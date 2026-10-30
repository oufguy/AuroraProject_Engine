#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>
#include <M_Log.h>
#include <M_Window.h>
#include <M_Mesh.h>
#include <M_Texture.h>
#include <Shaders/S_Mesh.h>

// ============================================================================
// Pure Cross-Platform DirectX 11 Dynamic Driver (Zero Windows OS Headers)
// ============================================================================
// 1 Compile, 1 Build: Compiles identically on Windows, Linux, and macOS without
// any #ifdef _WIN32 or Windows-specific compilation guards.
// Uses SDL3's universal dynamic loader to interface with d3d11.dll / DXVK.
// ============================================================================
class R_DirectX {
public:
    static constexpr UINT MAX_VERTICES_PER_FRAME = 65536;
    static constexpr UINT MAX_INDICES_PER_FRAME  = 131072;
    static constexpr UINT STATIC_VERTEX_CAPACITY = 32768; // First half reserved for zero-copy cached static meshes
    static constexpr UINT STATIC_INDEX_CAPACITY  = 65536;

private:
    struct CachedMeshSlot {
        UINT  VertexOffset = 0;
        UINT  IndexOffset  = 0;
        UINT  VertexCount  = 0;
        UINT  IndexCount   = 0;
        UINT  Revision     = 0;
        DWORD FirstColor   = 0;
    };
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

    // Minimal DXGI & D3D11 structures
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

    struct DX_BUFFER_DESC {
        UINT ByteWidth;
        UINT Usage;
        UINT BindFlags;
        UINT CPUAccessFlags;
        UINT MiscFlags;
        UINT StructureByteStride;
    };

    struct DX_MAPPED_SUBRESOURCE {
        void *pData;
        UINT  RowPitch;
        UINT  DepthPitch;
    };

    struct DX_INPUT_ELEMENT_DESC {
        const char *SemanticName;
        UINT        SemanticIndex;
        UINT        Format;
        UINT        InputSlot;
        UINT        AlignedByteOffset;
        UINT        InputSlotClass;
        UINT        InstanceDataStepRate;
    };

    struct DX_RENDER_TARGET_BLEND_DESC {
        INT  BlendEnable;
        UINT SrcBlend;
        UINT DestBlend;
        UINT BlendOp;
        UINT SrcBlendAlpha;
        UINT DestBlendAlpha;
        UINT BlendOpAlpha;
        BYTE RenderTargetWriteMask;
    };

    struct DX_BLEND_DESC {
        INT                         AlphaToCoverageEnable;
        INT                         IndependentBlendEnable;
        DX_RENDER_TARGET_BLEND_DESC RenderTarget[8];
    };

    struct DX_RASTERIZER_DESC {
        UINT  FillMode;
        UINT  CullMode;
        INT   FrontCounterClockwise;
        INT   DepthBias;
        FLOAT DepthBiasClamp;
        FLOAT SlopeScaledDepthBias;
        INT   DepthClipEnable;
        INT   ScissorEnable;
        INT   MultisampleEnable;
        INT   AntialiasedLineEnable;
    };

    struct DX_TEXTURE2D_DESC {
        UINT           Width;
        UINT           Height;
        UINT           MipLevels;
        UINT           ArraySize;
        UINT           Format;
        DX_SAMPLE_DESC SampleDesc;
        UINT           Usage;
        UINT           BindFlags;
        UINT           CPUAccessFlags;
        UINT           MiscFlags;
    };

    struct DX_DEPTH_STENCILOP_DESC {
        UINT StencilFailOp;
        UINT StencilDepthFailOp;
        UINT StencilPassOp;
        UINT StencilFunc;
    };

    struct DX_DEPTH_STENCIL_DESC {
        INT                     DepthEnable;
        UINT                    DepthWriteMask;
        UINT                    DepthFunc;
        INT                     StencilEnable;
        BYTE                    StencilReadMask;
        BYTE                    StencilWriteMask;
        DX_DEPTH_STENCILOP_DESC FrontFace;
        DX_DEPTH_STENCILOP_DESC BackFace;
    };

    struct DX_SUBRESOURCE_DATA {
        const void *pSysMem;
        UINT        SysMemPitch;
        UINT        SysMemSlicePitch;
    };

    struct DX_SAMPLER_DESC {
        UINT  Filter;
        UINT  AddressU;
        UINT  AddressV;
        UINT  AddressW;
        FLOAT MipLODBias;
        UINT  MaxAnisotropy;
        UINT  ComparisonFunc;
        FLOAT BorderColor[4];
        FLOAT MinLOD;
        FLOAT MaxLOD;
    };

    struct GPUTextureSlot {
        void *Texture  = nullptr;
        void *SRV      = nullptr;
        UINT  Revision = 0;
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

    // Mesh Shader Bytecode is cleanly loaded from Engine::Shaders::Mesh (see Shaders/S_Mesh.h)

    // Active Handles & Context
    static inline SDL_SharedObject *s_D3D11Module        = nullptr;
    static inline void             *s_Device             = nullptr;
    static inline void             *s_DeviceContext      = nullptr;
    static inline void             *s_SwapChain          = nullptr;
    static inline void             *s_RenderTargetView   = nullptr;
    static inline void             *s_DepthStencilBuffer = nullptr;
    static inline void             *s_DepthStencilView   = nullptr;
    static inline SDL_Window       *s_Window             = nullptr;
    static inline INT               s_Width              = 0;
    static inline INT               s_Height             = 0;

    // Cached DeviceContext COM Function Pointers (Zero-overhead per-draw dispatch)
    using PFN_Map              = int  (DX_CALL *)(void *, void *, UINT, UINT, UINT, DX_MAPPED_SUBRESOURCE *);
    using PFN_Unmap            = void (DX_CALL *)(void *, void *, UINT);
    using PFN_DrawIndexed      = void (DX_CALL *)(void *, UINT, UINT, INT);
    using PFN_PSSetSRV         = void (DX_CALL *)(void *, UINT, UINT, void *const *);
    using PFN_OMSetRT          = void (DX_CALL *)(void *, UINT, void *const *, void *);
    using PFN_RSSetViewports   = void (DX_CALL *)(void *, UINT, const void *);
    using PFN_ClearRTV         = void (DX_CALL *)(void *, void *, const FLOAT *);
    using PFN_ClearDSV         = void (DX_CALL *)(void *, void *, UINT, FLOAT, BYTE);

    static inline PFN_Map              s_pfnMap              = nullptr;
    static inline PFN_Unmap            s_pfnUnmap            = nullptr;
    static inline PFN_DrawIndexed      s_pfnDrawIndexed      = nullptr;
    static inline PFN_PSSetSRV         s_pfnPSSetSRV         = nullptr;
    static inline PFN_OMSetRT          s_pfnOMSetRT          = nullptr;
    static inline PFN_RSSetViewports   s_pfnRSSetViewports   = nullptr;
    static inline PFN_ClearRTV         s_pfnClearRTV         = nullptr;
    static inline PFN_ClearDSV         s_pfnClearDSV         = nullptr;

    // Mesh Pipeline Handles
    static inline void             *s_VertexShader       = nullptr;
    static inline void             *s_PixelShader        = nullptr;
    static inline void             *s_InputLayout        = nullptr;
    static inline void             *s_ConstantBuffer     = nullptr;
    static inline void             *s_VertexBuffer       = nullptr;
    static inline void             *s_IndexBuffer        = nullptr;
    static inline void             *s_BlendState         = nullptr;
    static inline void             *s_DepthStencilState  = nullptr;
    static inline void             *s_RasterizerState    = nullptr;
    static inline void             *s_SamplerState       = nullptr;
    static inline uMap<UINT, GPUTextureSlot> s_TextureCache;
    static inline UINT              s_LastBoundTextureID = 0xFFFFFFFFu;
    static inline uMap<UINT, CachedMeshSlot> s_MeshCache;
    static inline UINT              s_StaticVertexCursor = 0;
    static inline UINT              s_StaticIndexCursor  = 0;
    static inline UINT              s_FrameVertexOffset  = STATIC_VERTEX_CAPACITY;
    static inline UINT              s_FrameIndexOffset   = STATIC_INDEX_CAPACITY;
    static inline BOOL              s_BufferNeedsDiscard = true;

    // Renderer State
    static inline BOOL   s_IsInitialized = false;
    static inline BOOL   s_IsFrameActive = false;
    static inline BOOL   s_VSync         = true;
    static inline BOOL   s_AllowTearing  = false;
    static inline COLOR  s_ClearColor    = COLOR(30, 40, 55, 255);
    static inline String s_GPUName       = "DirectX 11 Device";

    // Release a COM object via standard IUnknown::Release (vtable index 2)
    static inline void COM_Release(void *&ptr) {
        if (!ptr) return;
        using PFN_Release = unsigned long (DX_CALL *)(void *);
        void **vtable = *reinterpret_cast<void***>(ptr);
        reinterpret_cast<PFN_Release>(vtable[2])(ptr);
        ptr = nullptr;
    }

    static inline BOOL CreateMeshPipelineAndBuffers() {
        void **devVtbl = *reinterpret_cast<void***>(s_Device);
        int hr = 0;

        // 1. Create Vertex Shader (Device vtable[12])
        using PFN_CreateVS = int (DX_CALL *)(void *, const void *, size_t, void *, void **);
        hr = reinterpret_cast<PFN_CreateVS>(devVtbl[12])(s_Device, Engine::Shaders::Mesh::DirectX_Vertex_DXBC, sizeof(Engine::Shaders::Mesh::DirectX_Vertex_DXBC), nullptr, &s_VertexShader);
        if (hr < 0 || !s_VertexShader) {
            LOG(Error, LogLevel::Core, "DirectX: CreateVertexShader failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        // 2. Create Pixel Shader (Device vtable[15])
        using PFN_CreatePS = int (DX_CALL *)(void *, const void *, size_t, void *, void **);
        hr = reinterpret_cast<PFN_CreatePS>(devVtbl[15])(s_Device, Engine::Shaders::Mesh::DirectX_Pixel_DXBC, sizeof(Engine::Shaders::Mesh::DirectX_Pixel_DXBC), nullptr, &s_PixelShader);
        if (hr < 0 || !s_PixelShader) {
            LOG(Error, LogLevel::Core, "DirectX: CreatePixelShader failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        // 3. Create Input Layout (Device vtable[11])
        // DXGI_FORMAT_R32G32B32_FLOAT = 6, DXGI_FORMAT_R32G32_FLOAT = 16, DXGI_FORMAT_R8G8B8A8_UNORM = 28
        DX_INPUT_ELEMENT_DESC layoutDesc[4] = {
            { "POSITION", 0,  6, 0, offsetof(Vertex, Position), 0, 0 },
            { "NORMAL",   0,  6, 0, offsetof(Vertex, Normal),   0, 0 },
            { "TEXCOORD", 0, 16, 0, offsetof(Vertex, UV),       0, 0 },
            { "COLOR",    0, 28, 0, offsetof(Vertex, Color),    0, 0 }
        };
        using PFN_CreateIL = int (DX_CALL *)(void *, const void *, UINT, const void *, size_t, void **);
        hr = reinterpret_cast<PFN_CreateIL>(devVtbl[11])(s_Device, layoutDesc, 4, Engine::Shaders::Mesh::DirectX_Vertex_DXBC, sizeof(Engine::Shaders::Mesh::DirectX_Vertex_DXBC), &s_InputLayout);
        if (hr < 0 || !s_InputLayout) {
            LOG(Error, LogLevel::Core, "DirectX: CreateInputLayout failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        // 4. Create Dynamic Buffers (Device vtable[3])
        // Usage: D3D11_USAGE_DYNAMIC = 2, CPUAccess: D3D11_CPU_ACCESS_WRITE = 0x10000
        // BindFlags: VERTEX_BUFFER = 1, INDEX_BUFFER = 2, CONSTANT_BUFFER = 4
        using PFN_CreateBuffer = int (DX_CALL *)(void *, const void *, const void *, void **);

        DX_BUFFER_DESC vbDesc{ MAX_VERTICES_PER_FRAME * sizeof(Vertex), 2, 1, 0x10000, 0, 0 };
        hr = reinterpret_cast<PFN_CreateBuffer>(devVtbl[3])(s_Device, &vbDesc, nullptr, &s_VertexBuffer);
        if (hr < 0 || !s_VertexBuffer) {
            LOG(Error, LogLevel::Core, "DirectX: CreateBuffer(VB) failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        DX_BUFFER_DESC ibDesc{ MAX_INDICES_PER_FRAME * sizeof(UINT), 2, 2, 0x10000, 0, 0 };
        hr = reinterpret_cast<PFN_CreateBuffer>(devVtbl[3])(s_Device, &ibDesc, nullptr, &s_IndexBuffer);
        if (hr < 0 || !s_IndexBuffer) {
            LOG(Error, LogLevel::Core, "DirectX: CreateBuffer(IB) failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        DX_BUFFER_DESC cbDesc{ sizeof(DrawConstants), 2, 4, 0x10000, 0, 0 };
        hr = reinterpret_cast<PFN_CreateBuffer>(devVtbl[3])(s_Device, &cbDesc, nullptr, &s_ConstantBuffer);
        if (hr < 0 || !s_ConstantBuffer) {
            LOG(Error, LogLevel::Core, "DirectX: CreateBuffer(CB) failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        // 5. Create Alpha Blend State (Device vtable[20])
        // Note: D3D11 validates all 8 RenderTarget blend descriptors (0 is an invalid D3D11_BLEND_OP!)
        DX_BLEND_DESC blendDesc{};
        for (int i = 0; i < 8; ++i) {
            blendDesc.RenderTarget[i].BlendEnable           = (i == 0) ? 1 : 0;
            blendDesc.RenderTarget[i].SrcBlend              = 5;    // D3D11_BLEND_SRC_ALPHA
            blendDesc.RenderTarget[i].DestBlend             = 6;    // D3D11_BLEND_INV_SRC_ALPHA
            blendDesc.RenderTarget[i].BlendOp               = 1;    // D3D11_BLEND_OP_ADD
            blendDesc.RenderTarget[i].SrcBlendAlpha         = 2;    // D3D11_BLEND_ONE
            blendDesc.RenderTarget[i].DestBlendAlpha        = 1;    // D3D11_BLEND_ZERO
            blendDesc.RenderTarget[i].BlendOpAlpha          = 1;    // D3D11_BLEND_OP_ADD
            blendDesc.RenderTarget[i].RenderTargetWriteMask = 0x0F; // D3D11_COLOR_WRITE_ENABLE_ALL
        }

        using PFN_CreateBlend = int (DX_CALL *)(void *, const void *, void **);
        hr = reinterpret_cast<PFN_CreateBlend>(devVtbl[20])(s_Device, &blendDesc, &s_BlendState);
        if (hr < 0 || !s_BlendState) {
            LOG(Error, LogLevel::Core, "DirectX: CreateBlendState failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        // 6. Create Depth Stencil State (Device vtable[21]: Z-Test + Z-Write + LESS_EQUAL)
        DX_DEPTH_STENCIL_DESC depthDesc{};
        depthDesc.DepthEnable      = 1;
        depthDesc.DepthWriteMask   = 1; // D3D11_DEPTH_WRITE_MASK_ALL
        depthDesc.DepthFunc        = 4; // D3D11_COMPARISON_LESS_EQUAL
        depthDesc.StencilEnable    = 0;
        depthDesc.StencilReadMask  = 0xFF;
        depthDesc.StencilWriteMask = 0xFF;
        depthDesc.FrontFace        = { 1, 1, 1, 8 }; // KEEP, KEEP, KEEP, ALWAYS
        depthDesc.BackFace         = { 1, 1, 1, 8 };

        using PFN_CreateDepthState = int (DX_CALL *)(void *, const void *, void **);
        hr = reinterpret_cast<PFN_CreateDepthState>(devVtbl[21])(s_Device, &depthDesc, &s_DepthStencilState);
        if (hr < 0 || !s_DepthStencilState) {
            LOG(Error, LogLevel::Core, "DirectX: CreateDepthStencilState failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        // 7. Create Rasterizer State (Device vtable[22]: Solid + Backface Culling + Clockwise Front)
        DX_RASTERIZER_DESC rastDesc{};
        rastDesc.FillMode              = 3; // D3D11_FILL_SOLID
        rastDesc.CullMode              = 3; // D3D11_CULL_BACK
        rastDesc.FrontCounterClockwise = 0; // Clockwise = Front
        rastDesc.DepthClipEnable       = 1;

        using PFN_CreateRast = int (DX_CALL *)(void *, const void *, void **);
        hr = reinterpret_cast<PFN_CreateRast>(devVtbl[22])(s_Device, &rastDesc, &s_RasterizerState);
        if (hr < 0 || !s_RasterizerState) {
            LOG(Error, LogLevel::Core, "DirectX: CreateRasterizerState failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        // 8. Create Texture Sampler State (Device vtable[23]: Linear Filtering + Wrap UV)
        DX_SAMPLER_DESC sampDesc{};
        sampDesc.Filter         = 0x15; // D3D11_FILTER_MIN_MAG_MIP_LINEAR
        sampDesc.AddressU       = 1;    // D3D11_TEXTURE_ADDRESS_WRAP
        sampDesc.AddressV       = 1;    // D3D11_TEXTURE_ADDRESS_WRAP
        sampDesc.AddressW       = 1;    // D3D11_TEXTURE_ADDRESS_WRAP
        sampDesc.MipLODBias     = 0.0f;
        sampDesc.MaxAnisotropy  = 1;
        sampDesc.ComparisonFunc = 8;    // D3D11_COMPARISON_ALWAYS
        sampDesc.MinLOD         = 0.0f;
        sampDesc.MaxLOD         = 3.402823466e+38f;

        using PFN_CreateSampler = int (DX_CALL *)(void *, const void *, void **);
        hr = reinterpret_cast<PFN_CreateSampler>(devVtbl[23])(s_Device, &sampDesc, &s_SamplerState);
        if (hr < 0 || !s_SamplerState) {
            LOG(Error, LogLevel::Core, "DirectX: CreateSamplerState failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        return true;
    }

    static inline void *GetOrUploadTextureSRV(UINT textureID) {
        if (!s_Device) return nullptr;

        // Fast-path: Check GPU texture cache first with zero mutex locks
        auto it = s_TextureCache.find(textureID);
        if (it != s_TextureCache.end() && it->second.SRV != nullptr) {
            return it->second.SRV;
        }

        const TextureData &tex = M_Texture::Get(textureID);
        if (!tex.Is_Valid()) return nullptr;

        if (it != s_TextureCache.end()) {
            COM_Release(it->second.SRV);
            COM_Release(it->second.Texture);
        }

        DX_TEXTURE2D_DESC texDesc{};
        texDesc.Width              = tex.Width;
        texDesc.Height             = tex.Height;
        texDesc.MipLevels          = 1;
        texDesc.ArraySize          = 1;
        texDesc.Format             = 28;   // DXGI_FORMAT_R8G8B8A8_UNORM
        texDesc.SampleDesc.Count   = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage              = 0;    // D3D11_USAGE_DEFAULT
        texDesc.BindFlags          = 0x08; // D3D11_BIND_SHADER_RESOURCE

        DX_SUBRESOURCE_DATA subData{};
        subData.pSysMem          = tex.Pixels.data();
        subData.SysMemPitch      = tex.Width * sizeof(COLOR);
        subData.SysMemSlicePitch = 0;

        void **devVtbl = *reinterpret_cast<void***>(s_Device);
        using PFN_CreateTex2D = int (DX_CALL *)(void *, const void *, const void *, void **);
        using PFN_CreateSRV   = int (DX_CALL *)(void *, void *, const void *, void **);

        void *gpuTex = nullptr;
        if (reinterpret_cast<PFN_CreateTex2D>(devVtbl[5])(s_Device, &texDesc, &subData, &gpuTex) < 0 || !gpuTex) {
            return nullptr;
        }

        void *gpuSRV = nullptr;
        if (reinterpret_cast<PFN_CreateSRV>(devVtbl[7])(s_Device, gpuTex, nullptr, &gpuSRV) < 0 || !gpuSRV) {
            COM_Release(gpuTex);
            return nullptr;
        }

        s_TextureCache[tex.TextureID] = { gpuTex, gpuSRV, tex.Revision };
        return gpuSRV;
    }

    static inline BOOL CreateDepthBuffer(INT w, INT h) {
        if (!s_Device || w <= 0 || h <= 0) return false;

        COM_Release(s_DepthStencilView);
        COM_Release(s_DepthStencilBuffer);

        DX_TEXTURE2D_DESC depthTexDesc{};
        depthTexDesc.Width              = static_cast<UINT>(w);
        depthTexDesc.Height             = static_cast<UINT>(h);
        depthTexDesc.MipLevels          = 1;
        depthTexDesc.ArraySize          = 1;
        depthTexDesc.Format             = 40; // DXGI_FORMAT_D32_FLOAT
        depthTexDesc.SampleDesc.Count   = 1;
        depthTexDesc.SampleDesc.Quality = 0;
        depthTexDesc.Usage              = 0;    // D3D11_USAGE_DEFAULT
        depthTexDesc.BindFlags          = 0x40; // D3D11_BIND_DEPTH_STENCIL

        void **devVtbl = *reinterpret_cast<void***>(s_Device);
        using PFN_CreateTex2D = int (DX_CALL *)(void *, const void *, const void *, void **);
        int hr = reinterpret_cast<PFN_CreateTex2D>(devVtbl[5])(s_Device, &depthTexDesc, nullptr, &s_DepthStencilBuffer);
        if (hr < 0 || !s_DepthStencilBuffer) {
            LOG(Error, LogLevel::Core, "DirectX: CreateTexture2D(Depth) failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        using PFN_CreateDSV = int (DX_CALL *)(void *, void *, const void *, void **);
        hr = reinterpret_cast<PFN_CreateDSV>(devVtbl[10])(s_Device, s_DepthStencilBuffer, nullptr, &s_DepthStencilView);
        if (hr < 0 || !s_DepthStencilView) {
            LOG(Error, LogLevel::Core, "DirectX: CreateDepthStencilView failed (0x{:08X})", static_cast<UINT>(hr));
            return false;
        }

        return true;
    }

    static inline void DestroyMeshPipelineAndBuffers() {
        for (auto &kv : s_TextureCache) {
            COM_Release(kv.second.SRV);
            COM_Release(kv.second.Texture);
        }
        s_TextureCache.clear();
        s_LastBoundTextureID = 0xFFFFFFFFu;

        s_MeshCache.clear();
        s_StaticVertexCursor = 0;
        s_StaticIndexCursor  = 0;
        s_FrameVertexOffset  = STATIC_VERTEX_CAPACITY;
        s_FrameIndexOffset   = STATIC_INDEX_CAPACITY;
        s_BufferNeedsDiscard = true;

        COM_Release(s_SamplerState);
        COM_Release(s_RasterizerState);
        COM_Release(s_DepthStencilState);
        COM_Release(s_BlendState);
        COM_Release(s_ConstantBuffer);
        COM_Release(s_IndexBuffer);
        COM_Release(s_VertexBuffer);
        COM_Release(s_InputLayout);
        COM_Release(s_PixelShader);
        COM_Release(s_VertexShader);
    }

public:
    // Pure runtime probe: checks whether d3d11.dll is present and functional
    static inline BOOL Is_Supported() {
        SDL_SharedObject *mod = SDL_LoadObject("d3d11.dll");
        if (!mod) return false;
        BOOL bHasFunc = (SDL_LoadFunction(mod, "D3D11CreateDeviceAndSwapChain") != nullptr);
        SDL_UnloadObject(mod);
        return bHasFunc;
    }

    // Initializes DirectX 11 Device, Immediate Context, SwapChain, and Mesh Pipeline
    static inline BOOL Init(SDL_Window *window) {
        if (s_IsInitialized) return true;
        if (!window) return false;

        // 1. Dynamically load d3d11.dll / DXVK (Zero compile-time OS link dependencies)
        s_D3D11Module = SDL_LoadObject("d3d11.dll");
        if (!s_D3D11Module) {
            LOG(Error, LogLevel::Core, "DirectX: d3d11.dll / DXVK dynamic library not found!");
            return false;
        }

        auto pfnCreate = reinterpret_cast<PFN_D3D11CreateDeviceAndSwapChain>(
            SDL_LoadFunction(s_D3D11Module, "D3D11CreateDeviceAndSwapChain"));

        if (!pfnCreate) {
            LOG(Error, LogLevel::Core, "DirectX: Failed to locate D3D11CreateDeviceAndSwapChain in d3d11.dll!");
            SDL_UnloadObject(s_D3D11Module);
            s_D3D11Module = nullptr;
            return false;
        }

        s_Window = window;

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
        s_Width  = w;
        s_Height = h;

        // 3. Setup DXGI SwapChain Description (Modern Flip-Model + Allow Tearing for Uncapped FPS)
        // DXGI_SWAP_EFFECT_FLIP_DISCARD = 4, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING = 2048
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
        sd.SwapEffect                         = 4;    // DXGI_SWAP_EFFECT_FLIP_DISCARD
        sd.Flags                              = 2048; // DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
        s_AllowTearing                        = true;

        const int featureLevels[] = {
            0xb100, // D3D_FEATURE_LEVEL_11_1
            0xb000, // D3D_FEATURE_LEVEL_11_0
            0xa100, // D3D_FEATURE_LEVEL_10_1
            0xa000  // D3D_FEATURE_LEVEL_10_0
        };
        int chosenLevel = 0;

        int hr = pfnCreate(
            nullptr,            // Default adapter
            1,                  // D3D_DRIVER_TYPE_HARDWARE
            nullptr,            // Software module
            0,                  // Flags
            featureLevels,      // Feature level array
            4,                  // Count
            7,                  // D3D11_SDK_VERSION
            &sd,                // Swapchain descriptor
            &s_SwapChain,       // Out swapchain
            &s_Device,          // Out device
            &chosenLevel,       // Out feature level
            &s_DeviceContext    // Out device context
        );

        // Fallback 1: Flip-Model without tearing flag
        if (hr < 0) {
            s_AllowTearing = false;
            sd.Flags       = 0;
            hr = pfnCreate(nullptr, 1, nullptr, 0, featureLevels, 4, 7, &sd, &s_SwapChain, &s_Device, &chosenLevel, &s_DeviceContext);
        }

        // Fallback 2: Legacy BitBlt Discard (0) for older OS / Wine
        if (hr < 0) {
            sd.SwapEffect = 0;
            sd.Flags      = 2;
            hr = pfnCreate(nullptr, 1, nullptr, 0, featureLevels, 4, 7, &sd, &s_SwapChain, &s_Device, &chosenLevel, &s_DeviceContext);
        }

        if (hr < 0 || !s_Device || !s_SwapChain || !s_DeviceContext) {
            LOG(Error, LogLevel::Core, "DirectX: Failed to create D3D11 Device and SwapChain (Code: 0x{:08X})", static_cast<UINT32>(hr));
            Shutdown();
            return false;
        }

        // Cache Device Context function pointers for zero-overhead per-draw dispatch
        void **ctxVtbl       = *reinterpret_cast<void***>(s_DeviceContext);
        s_pfnMap             = reinterpret_cast<PFN_Map>(ctxVtbl[14]);
        s_pfnUnmap           = reinterpret_cast<PFN_Unmap>(ctxVtbl[15]);
        s_pfnDrawIndexed     = reinterpret_cast<PFN_DrawIndexed>(ctxVtbl[12]);
        s_pfnPSSetSRV        = reinterpret_cast<PFN_PSSetSRV>(ctxVtbl[8]);
        s_pfnOMSetRT         = reinterpret_cast<PFN_OMSetRT>(ctxVtbl[33]);
        s_pfnRSSetViewports  = reinterpret_cast<PFN_RSSetViewports>(ctxVtbl[44]);
        s_pfnClearRTV        = reinterpret_cast<PFN_ClearRTV>(ctxVtbl[50]);
        s_pfnClearDSV        = reinterpret_cast<PFN_ClearDSV>(ctxVtbl[53]);

        // 4. Query GPU Name from DXGI Adapter (COM QueryInterface & GetDesc)
        void *dxgiDevice = nullptr;
        void **devVtbl = *reinterpret_cast<void***>(s_Device);
        using PFN_QI = int (DX_CALL *)(void *, const void *, void **);
        if (reinterpret_cast<PFN_QI>(devVtbl[0])(s_Device, &IID_DXGIDevice, &dxgiDevice) >= 0 && dxgiDevice) {
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
        void **scVtbl = *reinterpret_cast<void***>(s_SwapChain);
        using PFN_GetBuffer = int (DX_CALL *)(void *, UINT, const void *, void **);
        hr = reinterpret_cast<PFN_GetBuffer>(scVtbl[9])(s_SwapChain, 0, &IID_Texture2D, &pBackBuffer);
        if (hr < 0 || !pBackBuffer) {
            LOG(Error, LogLevel::Core, "DirectX: Failed to obtain BackBuffer from SwapChain!");
            Shutdown();
            return false;
        }

        using PFN_CreateRTV = int (DX_CALL *)(void *, void *, const void *, void **);
        hr = reinterpret_cast<PFN_CreateRTV>(devVtbl[9])(s_Device, pBackBuffer, nullptr, &s_RenderTargetView);
        COM_Release(pBackBuffer);

        if (hr < 0 || !s_RenderTargetView) {
            LOG(Error, LogLevel::Core, "DirectX: Failed to create RenderTargetView!");
            Shutdown();
            return false;
        }

        if (!CreateDepthBuffer(w, h)) {
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
        using PFN_RSSetViewports = void (DX_CALL *)(void *, UINT, const void *);
        reinterpret_cast<PFN_RSSetViewports>(ctxVtbl[44])(s_DeviceContext, 1, &vp);

        // 7. Create Mesh Pipeline & Dynamic Buffers
        if (!CreateMeshPipelineAndBuffers()) {
            LOG(Error, LogLevel::Core, "DirectX: Failed to create Mesh Pipeline!");
            Shutdown();
            return false;
        }

        // Bind pipeline states once to the device context (DirectX 11 sticky state = zero per-frame overhead!)
        BindMeshPipelineStates();

        s_IsInitialized = true;
        LOG(Info, LogLevel::Core, "DirectX 11: Backend initialized successfully on [{}] ({}x{})", s_GPUName, w, h);
        return true;
    }

    // Shuts down all D3D11 and DXGI resources cleanly
    static inline void Shutdown() {
        if (!s_IsInitialized && !s_D3D11Module) return;

        DestroyMeshPipelineAndBuffers();

        COM_Release(s_DepthStencilView);
        COM_Release(s_DepthStencilBuffer);
        COM_Release(s_RenderTargetView);
        COM_Release(s_SwapChain);
        COM_Release(s_DeviceContext);
        COM_Release(s_Device);

        if (s_D3D11Module) {
            SDL_UnloadObject(s_D3D11Module);
            s_D3D11Module = nullptr;
        }

        s_Window        = nullptr;
        s_IsFrameActive = false;
        s_IsInitialized = false;
        LOG(Info, LogLevel::Core, "DirectX 11: Backend shutdown cleanly.");
    }

    // Handles live window resizing by recreating swapchain buffers
    static inline void Recreate() {
        if (!s_IsInitialized || !s_SwapChain || !s_Device || !s_DeviceContext) return;

        INT w = 0, h = 0;
        if (s_Window) {
            SDL_GetWindowSizeInPixels(s_Window, &w, &h);
        }
        if (w <= 0 || h <= 0) return;

        COM_Release(s_DepthStencilView);
        COM_Release(s_DepthStencilBuffer);
        COM_Release(s_RenderTargetView);

        void **scVtbl = *reinterpret_cast<void***>(s_SwapChain);
        using PFN_ResizeBuffers = int (DX_CALL *)(void *, UINT, UINT, UINT, UINT, UINT);
        UINT resizeFlags = s_AllowTearing ? 2048u : 0u;
        int hr = reinterpret_cast<PFN_ResizeBuffers>(scVtbl[13])(s_SwapChain, 0, static_cast<UINT>(w), static_cast<UINT>(h), 0, resizeFlags);
        if (hr < 0) {
            LOG(Error, LogLevel::Core, "DirectX: Failed to resize SwapChain buffers!");
            return;
        }

        void *pBackBuffer = nullptr;
        using PFN_GetBuffer = int (DX_CALL *)(void *, UINT, const void *, void **);
        if (reinterpret_cast<PFN_GetBuffer>(scVtbl[9])(s_SwapChain, 0, &IID_Texture2D, &pBackBuffer) >= 0 && pBackBuffer) {
            void **devVtbl = *reinterpret_cast<void***>(s_Device);
            using PFN_CreateRTV = int (DX_CALL *)(void *, void *, const void *, void **);
            reinterpret_cast<PFN_CreateRTV>(devVtbl[9])(s_Device, pBackBuffer, nullptr, &s_RenderTargetView);
            COM_Release(pBackBuffer);
        }

        CreateDepthBuffer(w, h);

        DX_VIEWPORT vp{};
        vp.Width    = static_cast<FLOAT>(w);
        vp.Height   = static_cast<FLOAT>(h);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        void **ctxVtbl = *reinterpret_cast<void***>(s_DeviceContext);
        using PFN_RSSetViewports = void (DX_CALL *)(void *, UINT, const void *);
        reinterpret_cast<PFN_RSSetViewports>(ctxVtbl[44])(s_DeviceContext, 1, &vp);

        s_Width  = w;
        s_Height = h;

        // Re-bind pipeline states after resize
        BindMeshPipelineStates();
    }

    // Binds all static pipeline states to D3D11 DeviceContext once (Zero per-frame virtual calls!)
    static inline void BindMeshPipelineStates() {
        if (!s_DeviceContext) return;
        void **ctxVtbl = *reinterpret_cast<void***>(s_DeviceContext);
        using PFN_IASetIL   = void (DX_CALL *)(void *, void *);
        using PFN_IASetVB   = void (DX_CALL *)(void *, UINT, UINT, void *const *, const UINT *, const UINT *);
        using PFN_IASetIB   = void (DX_CALL *)(void *, void *, UINT, UINT);
        using PFN_IASetTopo = void (DX_CALL *)(void *, UINT);
        using PFN_VSSetS    = void (DX_CALL *)(void *, void *, void *const *, UINT);
        using PFN_VSSetCB   = void (DX_CALL *)(void *, UINT, UINT, void *const *);
        using PFN_PSSetS    = void (DX_CALL *)(void *, void *, void *const *, UINT);
        using PFN_PSSetSamp = void (DX_CALL *)(void *, UINT, UINT, void *const *);
        using PFN_RSSetSt   = void (DX_CALL *)(void *, void *);
        using PFN_OMSetBS   = void (DX_CALL *)(void *, void *, const FLOAT *, UINT);
        using PFN_OMSetDSS  = void (DX_CALL *)(void *, void *, UINT);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        reinterpret_cast<PFN_IASetIL>(ctxVtbl[17])(s_DeviceContext, s_InputLayout);
        reinterpret_cast<PFN_IASetVB>(ctxVtbl[18])(s_DeviceContext, 0, 1, &s_VertexBuffer, &stride, &offset);
        reinterpret_cast<PFN_IASetIB>(ctxVtbl[19])(s_DeviceContext, s_IndexBuffer, 42 /* DXGI_FORMAT_R32_UINT */, 0);
        reinterpret_cast<PFN_IASetTopo>(ctxVtbl[24])(s_DeviceContext, 4 /* D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST */);
        reinterpret_cast<PFN_VSSetS>(ctxVtbl[11])(s_DeviceContext, s_VertexShader, nullptr, 0);
        reinterpret_cast<PFN_VSSetCB>(ctxVtbl[7])(s_DeviceContext, 0, 1, &s_ConstantBuffer);
        reinterpret_cast<PFN_PSSetS>(ctxVtbl[9])(s_DeviceContext, s_PixelShader, nullptr, 0);
        reinterpret_cast<PFN_PSSetSamp>(ctxVtbl[10])(s_DeviceContext, 0, 1, &s_SamplerState);
        reinterpret_cast<PFN_RSSetSt>(ctxVtbl[43])(s_DeviceContext, s_RasterizerState);
        reinterpret_cast<PFN_OMSetBS>(ctxVtbl[35])(s_DeviceContext, s_BlendState, nullptr, 0xFFFFFFFF);
        reinterpret_cast<PFN_OMSetDSS>(ctxVtbl[36])(s_DeviceContext, s_DepthStencilState, 0);
    }

    // Prepares the frame by clearing the render target view to s_ClearColor
    static inline BOOL Begin_Frame() {
        if (!s_IsInitialized || !s_DeviceContext || M_Window::Is_Minimized()) return false;

        // Check if live window was resized (1-byte boolean check, zero OS call overhead!)
        if (M_Window::Was_Resized() || !s_RenderTargetView) {
            Recreate();
            if (!s_RenderTargetView) return false;
        }

        FLOAT clearColor[4] = {
            static_cast<FLOAT>(s_ClearColor.r) / 255.0f,
            static_cast<FLOAT>(s_ClearColor.g) / 255.0f,
            static_cast<FLOAT>(s_ClearColor.b) / 255.0f,
            static_cast<FLOAT>(s_ClearColor.a) / 255.0f
        };

        // Context::OMSetRenderTargets (cached COM pointer)
        s_pfnOMSetRT(s_DeviceContext, 1, &s_RenderTargetView, s_DepthStencilView);

        // Context::RSSetViewports (cached COM pointer)
        DX_VIEWPORT vp{ 0.0f, 0.0f, static_cast<FLOAT>(s_Width), static_cast<FLOAT>(s_Height), 0.0f, 1.0f };
        s_pfnRSSetViewports(s_DeviceContext, 1, &vp);

        // Context::ClearRenderTargetView & ClearDepthStencilView (cached COM pointers)
        s_pfnClearRTV(s_DeviceContext, s_RenderTargetView, clearColor);
        if (s_DepthStencilView) {
            s_pfnClearDSV(s_DeviceContext, s_DepthStencilView, 1 /* D3D11_CLEAR_DEPTH */, 1.0f, 0);
        }

        s_LastBoundTextureID = 0xFFFFFFFFu;
        s_IsFrameActive = true;
        return true;
    }

    // Draws a 3D/2D Mesh with transform, color tint, alpha transparency, and optional TextureID.
    // Automatically caches static meshes & textures in GPU memory so unchanged assets are NEVER re-uploaded!
    static inline void Draw_Mesh(
        const MeshData &mesh,
        const Vector3D &pos,
        const Vector3D &rot,
        const Vector3D &size,
        const COLOR    &color,
        FLOAT           transparency,
        UINT            textureID = 0)
    {
        if (!s_IsFrameActive || !s_DeviceContext || !mesh.Is_Valid() || transparency <= 0.001f) return;

        const UINT vCount = mesh.Get_VertexCount();
        const UINT iCount = mesh.Get_IndexCount();

        // Bind Texture SRV only when the active TextureID changes
        if (textureID != s_LastBoundTextureID) {
            void *srv = GetOrUploadTextureSRV(textureID);
            if (srv) {
                s_pfnPSSetSRV(s_DeviceContext, 0, 1, &srv);
                s_LastBoundTextureID = textureID;
            }
        }

        // One-time initial buffer discard after creation so subsequent NO_OVERWRITE maps are always valid
        if (s_BufferNeedsDiscard) {
            DX_MAPPED_SUBRESOURCE dummy{};
            if (s_pfnMap(s_DeviceContext, s_VertexBuffer, 0, 4 /* WRITE_DISCARD */, 0, &dummy) >= 0) {
                s_pfnUnmap(s_DeviceContext, s_VertexBuffer, 0);
            }
            if (s_pfnMap(s_DeviceContext, s_IndexBuffer, 0, 4 /* WRITE_DISCARD */, 0, &dummy) >= 0) {
                s_pfnUnmap(s_DeviceContext, s_IndexBuffer, 0);
            }
            s_BufferNeedsDiscard = false;
        }

        UINT drawVertexOffset = 0;
        UINT drawIndexOffset  = 0;
        BOOL bUsedStaticCache = false;

        if (mesh.MeshID != 0) {
            const COLOR &c0 = mesh.Vertices[0].Color;
            const DWORD firstColor = (DWORD(c0.r) << 24) | (DWORD(c0.g) << 16) | (DWORD(c0.b) << 8) | DWORD(c0.a);

            auto it = s_MeshCache.find(mesh.MeshID);
            if (it != s_MeshCache.end()) {
                const CachedMeshSlot &slot = it->second;
                if (slot.Revision    == mesh.Revision &&
                    slot.FirstColor  == firstColor    &&
                    slot.VertexCount == vCount        &&
                    slot.IndexCount  == iCount)
                {
                    // Zero-copy GPU Static Cache Hit! 0 Map/Unmap calls on Vertex or Index Buffer!
                    drawVertexOffset = slot.VertexOffset;
                    drawIndexOffset  = slot.IndexOffset;
                    bUsedStaticCache = true;
                }
            } else if (s_StaticVertexCursor + vCount <= STATIC_VERTEX_CAPACITY &&
                       s_StaticIndexCursor  + iCount <= STATIC_INDEX_CAPACITY)
            {
                // First time seeing this MeshID -> upload ONCE into the Static Cache region with NO_OVERWRITE (5)
                drawVertexOffset = s_StaticVertexCursor;
                drawIndexOffset  = s_StaticIndexCursor;

                DX_MAPPED_SUBRESOURCE mappedVB{};
                if (s_pfnMap(s_DeviceContext, s_VertexBuffer, 0, 5 /* WRITE_NO_OVERWRITE */, 0, &mappedVB) >= 0 && mappedVB.pData) {
                    std::memcpy(static_cast<Vertex *>(mappedVB.pData) + drawVertexOffset, mesh.Vertices.data(), vCount * sizeof(Vertex));
                    s_pfnUnmap(s_DeviceContext, s_VertexBuffer, 0);
                }

                DX_MAPPED_SUBRESOURCE mappedIB{};
                if (s_pfnMap(s_DeviceContext, s_IndexBuffer, 0, 5 /* WRITE_NO_OVERWRITE */, 0, &mappedIB) >= 0 && mappedIB.pData) {
                    std::memcpy(static_cast<UINT *>(mappedIB.pData) + drawIndexOffset, mesh.Indices.data(), iCount * sizeof(UINT));
                    s_pfnUnmap(s_DeviceContext, s_IndexBuffer, 0);
                }

                s_StaticVertexCursor += vCount;
                s_StaticIndexCursor  += iCount;

                s_MeshCache[mesh.MeshID] = { drawVertexOffset, drawIndexOffset, vCount, iCount, mesh.Revision, firstColor };
                bUsedStaticCache         = true;
            }
        }

        // Dynamic Ring Buffer (for meshes whose vertices mutate every frame, e.g. animated vertex colors)
        if (!bUsedStaticCache) {
            if (s_FrameVertexOffset + vCount > MAX_VERTICES_PER_FRAME ||
                s_FrameIndexOffset  + iCount > MAX_INDICES_PER_FRAME)
            {
                // Wrap dynamic ring buffer back to start of dynamic region (never touches static cache!)
                s_FrameVertexOffset = STATIC_VERTEX_CAPACITY;
                s_FrameIndexOffset  = STATIC_INDEX_CAPACITY;
            }

            drawVertexOffset = s_FrameVertexOffset;
            drawIndexOffset  = s_FrameIndexOffset;

            DX_MAPPED_SUBRESOURCE mappedVB{};
            if (s_pfnMap(s_DeviceContext, s_VertexBuffer, 0, 5 /* WRITE_NO_OVERWRITE */, 0, &mappedVB) >= 0 && mappedVB.pData) {
                std::memcpy(static_cast<Vertex *>(mappedVB.pData) + drawVertexOffset, mesh.Vertices.data(), vCount * sizeof(Vertex));
                s_pfnUnmap(s_DeviceContext, s_VertexBuffer, 0);
            }

            DX_MAPPED_SUBRESOURCE mappedIB{};
            if (s_pfnMap(s_DeviceContext, s_IndexBuffer, 0, 5 /* WRITE_NO_OVERWRITE */, 0, &mappedIB) >= 0 && mappedIB.pData) {
                std::memcpy(static_cast<UINT *>(mappedIB.pData) + drawIndexOffset, mesh.Indices.data(), iCount * sizeof(UINT));
                s_pfnUnmap(s_DeviceContext, s_IndexBuffer, 0);
            }

            s_FrameVertexOffset += vCount;
            s_FrameIndexOffset  += iCount;
        }

        FLOAT aspect = (s_Height > 0)
            ? (static_cast<FLOAT>(s_Width) / static_cast<FLOAT>(s_Height))
            : (16.0f / 9.0f);

        BOOL bShade3D = (vCount > 4);
        DrawConstants dc = M_Mesh::Build_DrawConstants(pos, rot, size, color, transparency, aspect, false, bShade3D);

        DX_MAPPED_SUBRESOURCE mappedCB{};
        if (s_pfnMap(s_DeviceContext, s_ConstantBuffer, 0, 4 /* WRITE_DISCARD */, 0, &mappedCB) >= 0 && mappedCB.pData) {
            std::memcpy(mappedCB.pData, &dc, sizeof(DrawConstants));
            s_pfnUnmap(s_DeviceContext, s_ConstantBuffer, 0);
        }

        // Context::DrawIndexed (zero vtable dereference overhead)
        s_pfnDrawIndexed(s_DeviceContext, iCount, drawIndexOffset, static_cast<INT>(drawVertexOffset));
    }

    // Presents the frame to screen, obeying s_VSync
    static inline void End_Frame() {
        if (!s_IsInitialized || !s_SwapChain || !s_IsFrameActive) return;
        s_IsFrameActive = false;

        UINT syncInterval = s_VSync ? 1 : 0;
        // DXGI_PRESENT_ALLOW_TEARING (0x200 = 512) unlocks uncapped FPS in Flip-Model when VSync is OFF
        UINT presentFlags = (!s_VSync && s_AllowTearing) ? 512u : 0u;
        void **scVtbl = *reinterpret_cast<void***>(s_SwapChain);

        // SwapChain::Present (vtable index 8)
        using PFN_Present = int (DX_CALL *)(void *, UINT, UINT);
        reinterpret_cast<PFN_Present>(scVtbl[8])(s_SwapChain, syncInterval, presentFlags);
    }

    // VSync & Settings Accessors
    static inline void       Set_VSync(BOOL enabled)       { s_VSync = enabled; }
    static inline BOOL       Get_VSync()                   { return s_VSync; }
    static inline void       Set_ClearColor(const COLOR &c){ s_ClearColor = c; }
    static inline COLOR      Get_ClearColor()              { return s_ClearColor; }
    static inline StringView Get_GPUName()                 { return s_GPUName; }
    static inline BOOL       Is_Initialized()              { return s_IsInitialized; }
};

#undef DX_CALL
