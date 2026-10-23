#include "R_DirectX.h"
#include <M_Log.h>
#include <M_Window.h>

// ============================================================================
// Pure Cross-Platform DirectX 11 Dynamic Driver (Zero Windows OS Headers)
// ============================================================================
// 1 Compile, 1 Build: Compiles identically on Windows, Linux, and macOS without
// any #ifdef _WIN32 or Windows-specific compilation guards.
// Uses SDL3's universal dynamic loader to interface with d3d11.dll / DXVK.
// ============================================================================

namespace {
    // Calling convention for DLL functions and COM vtables
#if defined(_MSC_VER) || defined(__MINGW32__)
    #define DX_CALL __stdcall
#else
    #define DX_CALL
#endif

    // Standard COM GUID definition (Pure C++, 16 bytes)
    struct DX_GUID {
        UINT32 Data1;
        UINT16 Data2;
        UINT16 Data3;
        BYTE   Data4[8];
    };

    // {6f15aaf2-d208-4e89-9ab4-489535d34f9c}
    static constexpr DX_GUID IID_Texture2D = {
        0x6f15aaf2, 0xd208, 0x4e89, { 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c }
    };

    // {54ec77fa-1377-44e6-8c32-88fd5f44c84c}
    static constexpr DX_GUID IID_DXGIDevice = {
        0x54ec77fa, 0x1377, 0x44e6, { 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c }
    };

    // Minimal DXGI SwapChain structures
    struct DX_RATIONAL {
        UINT Numerator;
        UINT Denominator;
    };

    struct DX_MODE_DESC {
        UINT        Width;
        UINT        Height;
        DX_RATIONAL RefreshRate;
        UINT        Format;
        UINT        ScanlineOrdering;
        UINT        Scaling;
    };

    struct DX_SAMPLE_DESC {
        UINT Count;
        UINT Quality;
    };

    struct DX_SWAP_CHAIN_DESC {
        DX_MODE_DESC   BufferDesc;
        DX_SAMPLE_DESC SampleDesc;
        UINT           BufferUsage;
        UINT           BufferCount;
        void          *OutputWindow;
        INT            Windowed;
        UINT           SwapEffect;
        UINT           Flags;
    };

    struct DX_VIEWPORT {
        FLOAT TopLeftX;
        FLOAT TopLeftY;
        FLOAT Width;
        FLOAT Height;
        FLOAT MinDepth;
        FLOAT MaxDepth;
    };

    struct DX_ADAPTER_DESC {
        wchar_t Description[128];
        UINT    VendorId;
        UINT    DeviceId;
        UINT    SubSysId;
        UINT    Revision;
        size_t  DedicatedVideoMemory;
        size_t  DedicatedSystemMemory;
        size_t  SharedSystemMemory;
        UINT64  AdapterLuid;
    };

    // Dynamic Function Pointer to D3D11CreateDeviceAndSwapChain
    using PFN_D3D11CreateDeviceAndSwapChain = int (DX_CALL *)(
        void       *pAdapter,
        int         DriverType,
        void       *Software,
        UINT        Flags,
        const int  *pFeatureLevels,
        UINT        FeatureLevels,
        UINT        SDKVersion,
        const void *pSwapChainDesc,
        void      **ppSwapChain,
        void      **ppDevice,
        int        *pFeatureLevel,
        void      **ppImmediateContext
    );

    // Active Handles & Context
    static SDL_SharedObject *g_D3D11Module      = nullptr;
    static void             *g_Device           = nullptr;
    static void             *g_DeviceContext    = nullptr;
    static void             *g_SwapChain        = nullptr;
    static void             *g_RenderTargetView = nullptr;
    static SDL_Window       *g_Window           = nullptr;
    static INT               g_Width            = 0;
    static INT               g_Height           = 0;

    // Release a COM object via standard IUnknown::Release (vtable index 2)
    inline void COM_Release(void *&ptr) {
        if (!ptr) return;
        using PFN_Release = unsigned long (DX_CALL *)(void *);
        void **vtable = *reinterpret_cast<void***>(ptr);
        reinterpret_cast<PFN_Release>(vtable[2])(ptr);
        ptr = nullptr;
    }
}

// Runtime hardware/driver probe: checks if d3d11.dll / DXVK is dynamically loadable
BOOL DirectXRenderer::Is_Supported() {
    SDL_SharedObject *mod = SDL_LoadObject("d3d11.dll");
    if (!mod) return false;
    BOOL bHasFunc = (SDL_LoadFunction(mod, "D3D11CreateDeviceAndSwapChain") != nullptr);
    SDL_UnloadObject(mod);
    return bHasFunc;
}

BOOL DirectXRenderer::Init(SDL_Window *window) {
    if (s_IsInitialized) return true;
    if (!window) return false;

    // 1. Dynamically load d3d11.dll / DXVK (Zero compile-time OS link dependencies)
    g_D3D11Module = SDL_LoadObject("d3d11.dll");
    if (!g_D3D11Module) {
        LOG(Error, LogLevel::Core, "DirectX: d3d11.dll / DXVK dynamic library not found!");
        return false;
    }

    auto pfnCreate = reinterpret_cast<PFN_D3D11CreateDeviceAndSwapChain>(
        SDL_LoadFunction(g_D3D11Module, "D3D11CreateDeviceAndSwapChain"));

    if (!pfnCreate) {
        LOG(Error, LogLevel::Core, "DirectX: Failed to locate D3D11CreateDeviceAndSwapChain in d3d11.dll!");
        SDL_UnloadObject(g_D3D11Module);
        g_D3D11Module = nullptr;
        return false;
    }

    g_Window = window;

    // 2. Obtain native window handle (HWND) universally from SDL3 properties
    void *hwnd = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        nullptr);

    if (!hwnd) {
        LOG(Error, LogLevel::Core, "DirectX: Failed to obtain window handle from SDL3!");
        Shutdown();
        return false;
    }

    INT w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    if (w <= 0 || h <= 0) { w = 1280; h = 720; }
    g_Width  = w;
    g_Height = h;

    // 3. Setup DXGI SwapChain Description (Double Buffering)
    DX_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = static_cast<UINT>(w);
    sd.BufferDesc.Height                  = static_cast<UINT>(h);
    sd.BufferDesc.Format                  = 28; // DXGI_FORMAT_R8G8B8A8_UNORM
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage                        = 0x20; // DXGI_USAGE_RENDER_TARGET_OUTPUT
    sd.OutputWindow                       = hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = 1;
    sd.SwapEffect                         = 0; // DXGI_SWAP_EFFECT_DISCARD
    sd.Flags                              = 2; // DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH

    const int featureLevels[] = {
        0xb100, // D3D_FEATURE_LEVEL_11_1
        0xb000, // D3D_FEATURE_LEVEL_11_0
        0xa100, // D3D_FEATURE_LEVEL_10_1
        0xa000  // D3D_FEATURE_LEVEL_10_0
    };
    int chosenLevel = 0;

    int hr = pfnCreate(
        nullptr,         // Default adapter
        1,               // D3D_DRIVER_TYPE_HARDWARE
        nullptr,         // Software module
        0,               // Flags
        featureLevels,   // Feature level array
        4,               // Count
        7,               // D3D11_SDK_VERSION
        &sd,             // Swapchain descriptor
        &g_SwapChain,    // Out swapchain
        &g_Device,       // Out device
        &chosenLevel,    // Out feature level
        &g_DeviceContext // Out device context
    );

    if (hr < 0 || !g_Device || !g_SwapChain || !g_DeviceContext) {
        LOG(Error, LogLevel::Core, "DirectX: Failed to create D3D11 Device and SwapChain (Code: 0x{:08X})", static_cast<UINT32>(hr));
        Shutdown();
        return false;
    }

    // 4. Query GPU Name from DXGI Adapter (COM QueryInterface & GetDesc)
    void *dxgiDevice = nullptr;
    void **devVtbl = *reinterpret_cast<void***>(g_Device);
    using PFN_QI = int (DX_CALL *)(void *, const void *, void **);
    if (reinterpret_cast<PFN_QI>(devVtbl[0])(g_Device, &IID_DXGIDevice, &dxgiDevice) >= 0 && dxgiDevice) {
        void **dxgiVtbl = *reinterpret_cast<void***>(dxgiDevice);
        void *adapter = nullptr;
        using PFN_GetAdapter = int (DX_CALL *)(void *, void **);
        if (reinterpret_cast<PFN_GetAdapter>(dxgiVtbl[7])(dxgiDevice, &adapter) >= 0 && adapter) {
            void **adapterVtbl = *reinterpret_cast<void***>(adapter);
            DX_ADAPTER_DESC desc{};
            using PFN_GetDesc = int (DX_CALL *)(void *, void *);
            if (reinterpret_cast<PFN_GetDesc>(adapterVtbl[8])(adapter, &desc) >= 0) {
                String name;
                for (int i = 0; i < 128 && desc.Description[i] != 0; ++i) {
                    name += static_cast<char>(desc.Description[i]);
                }
                if (!name.empty()) {
                    s_GPUName = name;
                }
            }
            COM_Release(adapter);
        }
        COM_Release(dxgiDevice);
    }

    // 5. Create Render Target View from BackBuffer (SwapChain vtable[9] + Device vtable[9])
    void *pBackBuffer = nullptr;
    void **scVtbl = *reinterpret_cast<void***>(g_SwapChain);
    using PFN_GetBuffer = int (DX_CALL *)(void *, UINT, const void *, void **);
    hr = reinterpret_cast<PFN_GetBuffer>(scVtbl[9])(g_SwapChain, 0, &IID_Texture2D, &pBackBuffer);
    if (hr < 0 || !pBackBuffer) {
        LOG(Error, LogLevel::Core, "DirectX: Failed to obtain BackBuffer from SwapChain!");
        Shutdown();
        return false;
    }

    using PFN_CreateRTV = int (DX_CALL *)(void *, void *, const void *, void **);
    hr = reinterpret_cast<PFN_CreateRTV>(devVtbl[9])(g_Device, pBackBuffer, nullptr, &g_RenderTargetView);
    COM_Release(pBackBuffer);

    if (hr < 0 || !g_RenderTargetView) {
        LOG(Error, LogLevel::Core, "DirectX: Failed to create RenderTargetView!");
        Shutdown();
        return false;
    }

    // 6. Setup Initial Viewport (Context vtable[44])
    DX_VIEWPORT vp{};
    vp.Width    = static_cast<FLOAT>(w);
    vp.Height   = static_cast<FLOAT>(h);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    void **ctxVtbl = *reinterpret_cast<void***>(g_DeviceContext);
    using PFN_RSSetViewports = void (DX_CALL *)(void *, UINT, const void *);
    reinterpret_cast<PFN_RSSetViewports>(ctxVtbl[44])(g_DeviceContext, 1, &vp);

    s_IsInitialized = true;
    LOG(Info, LogLevel::Core, "DirectX 11: Backend initialized successfully on [{}] ({}x{})", s_GPUName, w, h);
    return true;
}

void DirectXRenderer::Shutdown() {
    if (!s_IsInitialized && !g_D3D11Module) return;

    COM_Release(g_RenderTargetView);
    COM_Release(g_SwapChain);
    COM_Release(g_DeviceContext);
    COM_Release(g_Device);

    if (g_D3D11Module) {
        SDL_UnloadObject(g_D3D11Module);
        g_D3D11Module = nullptr;
    }

    g_Window        = nullptr;
    s_IsInitialized = false;
    LOG(Info, LogLevel::Core, "DirectX 11: Backend shutdown cleanly.");
}

void DirectXRenderer::Recreate() {
    if (!s_IsInitialized || !g_SwapChain || !g_Device || !g_DeviceContext) return;

    INT w = 0, h = 0;
    if (g_Window) {
        SDL_GetWindowSizeInPixels(g_Window, &w, &h);
    }
    if (w <= 0 || h <= 0) return;

    COM_Release(g_RenderTargetView);

    void **scVtbl = *reinterpret_cast<void***>(g_SwapChain);
    using PFN_ResizeBuffers = int (DX_CALL *)(void *, UINT, UINT, UINT, UINT, UINT);
    int hr = reinterpret_cast<PFN_ResizeBuffers>(scVtbl[13])(g_SwapChain, 0, static_cast<UINT>(w), static_cast<UINT>(h), 0, 0);
    if (hr < 0) {
        LOG(Error, LogLevel::Core, "DirectX: Failed to resize SwapChain buffers!");
        return;
    }

    void *pBackBuffer = nullptr;
    using PFN_GetBuffer = int (DX_CALL *)(void *, UINT, const void *, void **);
    if (reinterpret_cast<PFN_GetBuffer>(scVtbl[9])(g_SwapChain, 0, &IID_Texture2D, &pBackBuffer) >= 0 && pBackBuffer) {
        void **devVtbl = *reinterpret_cast<void***>(g_Device);
        using PFN_CreateRTV = int (DX_CALL *)(void *, void *, const void *, void **);
        reinterpret_cast<PFN_CreateRTV>(devVtbl[9])(g_Device, pBackBuffer, nullptr, &g_RenderTargetView);
        COM_Release(pBackBuffer);
    }

    DX_VIEWPORT vp{};
    vp.Width    = static_cast<FLOAT>(w);
    vp.Height   = static_cast<FLOAT>(h);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    void **ctxVtbl = *reinterpret_cast<void***>(g_DeviceContext);
    using PFN_RSSetViewports = void (DX_CALL *)(void *, UINT, const void *);
    reinterpret_cast<PFN_RSSetViewports>(ctxVtbl[44])(g_DeviceContext, 1, &vp);

    g_Width  = w;
    g_Height = h;
}

BOOL DirectXRenderer::Begin_Frame() {
    if (!s_IsInitialized || !g_DeviceContext || !g_RenderTargetView) return false;

    // Check if live window was resized
    INT currentW = 0, currentH = 0;
    if (g_Window) {
        SDL_GetWindowSizeInPixels(g_Window, &currentW, &currentH);
    }
    if (currentW > 0 && currentH > 0 && (currentW != g_Width || currentH != g_Height)) {
        Recreate();
    }

    FLOAT clearColor[4] = {
        static_cast<FLOAT>(s_ClearColor.r) / 255.0f,
        static_cast<FLOAT>(s_ClearColor.g) / 255.0f,
        static_cast<FLOAT>(s_ClearColor.b) / 255.0f,
        static_cast<FLOAT>(s_ClearColor.a) / 255.0f
    };

    void **ctxVtbl = *reinterpret_cast<void***>(g_DeviceContext);

    // Context::OMSetRenderTargets (vtable index 33)
    using PFN_OMSetRT = void (DX_CALL *)(void *, UINT, void *const *, void *);
    reinterpret_cast<PFN_OMSetRT>(ctxVtbl[33])(g_DeviceContext, 1, &g_RenderTargetView, nullptr);

    // Context::ClearRenderTargetView (vtable index 50)
    using PFN_ClearRTV = void (DX_CALL *)(void *, void *, const FLOAT *);
    reinterpret_cast<PFN_ClearRTV>(ctxVtbl[50])(g_DeviceContext, g_RenderTargetView, clearColor);

    return true;
}

void DirectXRenderer::End_Frame() {
    if (!s_IsInitialized || !g_SwapChain) return;

    UINT syncInterval = s_VSync ? 1 : 0;
    void **scVtbl = *reinterpret_cast<void***>(g_SwapChain);

    // SwapChain::Present (vtable index 8)
    using PFN_Present = int (DX_CALL *)(void *, UINT, UINT);
    reinterpret_cast<PFN_Present>(scVtbl[8])(g_SwapChain, syncInterval, 0);
}
