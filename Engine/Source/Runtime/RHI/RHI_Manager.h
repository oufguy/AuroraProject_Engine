#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>
#include <M_Log.h>
#include <M_Config.h>
#include <M_Window.h>
#include <R_Vulkan.h>
#include <R_DirectX.h>

// ============================================================================
// Supported Graphics Backend APIs
// ============================================================================
enum class API_Type : BYTE {
    None      = 0,
    Vulkan    = 1, // Vulkan 1.3 / 1.4 Core (Default, Native Cross-Platform)
    DirectX11 = 2, // DirectX 11 (Windows DXGI / DXVK)
    DirectX9  = 3  // DirectX 9  (Windows D3D9 / DXVK)
};

// ============================================================================
// Device & Graphics Compatibility Information (Pure Runtime Detection)
// ============================================================================
struct DeviceCompatibility {
    String Platform;                 // "Windows", "Linux", "macOS", etc. (via SDL_GetPlatform)
    String GPU;                      // e.g. "NVIDIA GeForce RTX 4080", "AMD Radeon RX 7800 XT"
    BOOL   bIsWineProton     = false;// Running on Linux/Steam Deck via Wine/Proton
    BOOL   bHasDirectX11     = false;// d3d11.dll or DXVK is present and callable
    BOOL   bHasDirectX9      = false;// d3d9.dll or DXVK is present and callable
    BOOL   bHasVulkan        = true; // Vulkan is natively supported everywhere
    String CompatibilityNote;        // Pre-formatted notice for in-game UI / Settings HUD
};

// ============================================================================
// Master RHI Manager (PoE 2 Style Dynamic Graphics Backend Dispatcher)
// ============================================================================
// Features:
//   - Encapsulates and hides low-level Vulkan / DirectX swapchains and devices
//   - Hot-swaps backends on the live window via RHI_Manager::Swap_API(API_Type)
//   - Pure runtime hardware & platform compatibility detection (Zero #ifdef)
//   - Safe crash-free fallback and in-game UI warning notifications
//   - Headless awareness: zero window / graphics overhead on dedicated servers
// ============================================================================
class RHIManager {
private:
    static inline API_Type s_ActiveAPI      = API_Type::None;
    static inline BOOL     s_IsInitialized  = false;
    static inline String   s_ActiveWarning  = ""; // Message for in-game UI notification

    static inline BOOL InitBackend(API_Type api, SDL_Window *window) {
        switch (api) {
            case API_Type::Vulkan:    return R_Vulkan::Init(window);
            case API_Type::DirectX11: return R_DirectX::Init(window);
            default:                  return false;
        }
    }

    static inline void ShutdownBackend(API_Type api) {
        switch (api) {
            case API_Type::Vulkan:    R_Vulkan::Shutdown();  break;
            case API_Type::DirectX11: R_DirectX::Shutdown(); break;
            default: break;
        }
    }

public:
    // ========================================================================
    // Hardware & Platform Runtime Detection (Zero #ifdef, 100% Cross-Platform)
    // ========================================================================

    // Probes the running environment dynamically without crashing or compile-time OS locks
    static inline DeviceCompatibility Detect_Device() {
        DeviceCompatibility dev{};

        // 1. Query OS Platform from SDL3 runtime
        const Char *plat = SDL_GetPlatform();
        dev.Platform = plat ? plat : "Unknown";

        // 2. Query GPU Name from active backend or Vulkan
        dev.GPU = String(Get_GPUName());
        if (dev.GPU.empty() || dev.GPU == "None" || dev.GPU == "Unknown GPU") {
            StringView vulkanGPU = R_Vulkan::Get_GPUName();
            dev.GPU = vulkanGPU.empty() ? "Generic Graphics Adapter" : String(vulkanGPU);
        }

        // 3. Detect Wine / Proton on Linux/Steam Deck (Pure runtime SDL loader)
        SDL_SharedObject *ntdll = SDL_LoadObject("ntdll.dll");
        if (ntdll) {
            if (SDL_LoadFunction(ntdll, "wine_get_version") != nullptr) {
                dev.bIsWineProton = true;
            }
            SDL_UnloadObject(ntdll);
        }

        // 4. Detect DirectX 11 / DXVK availability (Pure runtime probe)
        dev.bHasDirectX11 = R_DirectX::Is_Supported();

        // 5. Detect DirectX 9 / DXVK availability (Pure runtime probe)
        SDL_SharedObject *d3d9 = SDL_LoadObject("d3d9.dll");
        if (d3d9) {
            dev.bHasDirectX9 = (SDL_LoadFunction(d3d9, "Direct3DCreate9") != nullptr);
            SDL_UnloadObject(d3d9);
        }

        dev.bHasVulkan = true;

        // 6. Pre-format in-game UI friendly message
        if (dev.bIsWineProton) {
            dev.CompatibilityNote = std::format(
                "Notice: Running via Proton/Wine on [{}]. Native Vulkan provides better performance.",
                dev.GPU);
        } else if (dev.Platform != "Windows" && !dev.bHasDirectX11) {
            dev.CompatibilityNote = std::format(
                "DirectX is unsupported on {} ({}). Native Vulkan is active.",
                dev.Platform, dev.GPU);
        } else if (!dev.bHasDirectX11) {
            dev.CompatibilityNote = std::format(
                "DirectX runtime was not found. Native Vulkan is active.");
        }

        return dev;
    }

    // Checks if the requested API is supported on the user's current environment
    static inline BOOL Is_APISupported(API_Type api) {
        switch (api) {
            case API_Type::Vulkan:
                return true; // Vulkan 1.3/1.4 core is native cross-platform
            case API_Type::DirectX11:
                return R_DirectX::Is_Supported();
            case API_Type::DirectX9: {
                SDL_SharedObject *d3d9 = SDL_LoadObject("d3d9.dll");
                if (!d3d9) return false;
                BOOL bHas = (SDL_LoadFunction(d3d9, "Direct3DCreate9") != nullptr);
                SDL_UnloadObject(d3d9);
                return bHas;
            }
            default:
                return false;
        }
    }

    // ========================================================================
    // In-Game UI / HUD Query Helpers
    // ========================================================================

    // Returns active warning for in-game UI overlay/dialog (empty if everything is normal)
    static inline StringView Get_ActiveWarning() {
        return s_ActiveWarning;
    }

    // Clears the active warning once dismissed by the player in the in-game GUI
    static inline void Clear_ActiveWarning() {
        s_ActiveWarning.clear();
    }

    // Helper for Settings Menu: provides readable status text for each API option
    static inline String Get_APIStatusText(API_Type api) {
        DeviceCompatibility dev = Detect_Device();
        switch (api) {
            case API_Type::Vulkan:
                return "Recommended (Native Cross-Platform)";
            case API_Type::DirectX11:
                if (!dev.bHasDirectX11) {
                    return std::format("Unsupported on {} ({})", dev.Platform, dev.GPU);
                }
                if (dev.bIsWineProton) {
                    return "Proton/Wine Detected (Vulkan is preferred)";
                }
                return "Compatible (Windows D3D11 / DXVK)";
            case API_Type::DirectX9:
                if (!dev.bHasDirectX9) {
                    return std::format("DirectX 9 not found on {} ({})", dev.Platform, dev.GPU);
                }
                return "Compatible (Legacy D3D9 / DXVK)";
            default:
                return "Unknown";
        }
    }

    // ========================================================================
    // Init & Shutdown
    // ========================================================================

    // Initializes the requested Graphics API (Default: Vulkan) on the live window.
    // Automatically detects API & VSync from ConfigManager if available, otherwise runs defaults.
    // Graceful fallback: If requested backend is missing, logs warning and defaults to Vulkan/None.
    // Headless processes (e.g. FieldServer) automatically bypass RHI with 0 graphics overhead.
    static inline BOOL Init(API_Type defaultAPI = API_Type::Vulkan) {
        if (s_IsInitialized) return true;

        SDL_Window *window = Window::Get_Handle();
        // Headless processes (Server / CLI) have no window and need zero graphics overhead
        if (!window) {
            LOG(Info, LogLevel::Core, "RHI_Manager: Headless process detected (Zero graphics overhead).");
            return true;
        }

        // 1. Detect API from Config file if available; otherwise use defaultAPI parameter
        String apiSetting = Config::Get("Graphics", "API", Get_APIName(defaultAPI));
        if (apiSetting == "DirectX11" || apiSetting == "DirectX") {
            s_ActiveAPI = API_Type::DirectX11;
        } else if (apiSetting == "DirectX9") {
            s_ActiveAPI = API_Type::DirectX9;
        } else {
            s_ActiveAPI = API_Type::Vulkan;
        }

        // 2. Detect VSync from Config file if available; otherwise use default (false)
        BOOL configVSync = Config::Get("Graphics", "VSync", Config::Get("Window", "VSync", false));

        // If requested API is missing, warn and default to Vulkan
        if (!Is_APISupported(s_ActiveAPI)) {
            s_ActiveWarning = std::format(
                "[{}] not found! Defaulting to [Vulkan].",
                Get_APIName(s_ActiveAPI));
            LOG(Warn, LogLevel::Core, "[RHI] {}", s_ActiveWarning);
            s_ActiveAPI = API_Type::Vulkan;
        }

        // Try initializing active backend
        if (!InitBackend(s_ActiveAPI, window)) {
            if (s_ActiveAPI != API_Type::Vulkan) {
                s_ActiveWarning = std::format(
                    "[{}] failed to initialize! Defaulting to [Vulkan].",
                    Get_APIName(s_ActiveAPI));
                LOG(Warn, LogLevel::Core, "[RHI] {}", s_ActiveWarning);

                s_ActiveAPI = API_Type::Vulkan;
                if (!InitBackend(s_ActiveAPI, window)) {
                    s_ActiveWarning = "Vulkan not found or failed! Defaulting to [None].";
                    LOG(Error, LogLevel::Core, "[RHI] {}", s_ActiveWarning);
                    s_ActiveAPI = API_Type::None;
                    return false;
                }
            } else {
                s_ActiveWarning = "Vulkan not found or failed! Defaulting to [None].";
                LOG(Error, LogLevel::Core, "[RHI] {}", s_ActiveWarning);
                s_ActiveAPI = API_Type::None;
                return false;
            }
        }

        // Apply VSync from Config to the active renderer without redundant re-saving
        Set_VSync(configVSync, false);

        s_IsInitialized = true;
        LOG(Info, LogLevel::Core, "RHI_Manager initialized with [{}] backend (VSync: {}) on [{}]",
            Get_APIName(s_ActiveAPI), configVSync ? "ON" : "OFF", Get_GPUName());
        return true;
    }

    // Shuts down the active graphics backend cleanly
    static inline void Shutdown() {
        if (!s_IsInitialized) return;

        ShutdownBackend(s_ActiveAPI);
        s_ActiveAPI     = API_Type::None;
        s_IsInitialized = false;
        LOG(Info, LogLevel::Core, "RHI_Manager shutdown cleanly.");
    }

    // Hot-swaps graphics backends on the fly (PoE 2 style, no game restart required!)
    // Safe pre-check: if target backend is missing, reverts changes and keeps previous backend!
    // Example: RHI_Manager::Swap_API(API_Type::DirectX11);
    static inline BOOL Swap_API(API_Type newAPI) {
        if (newAPI == s_ActiveAPI || newAPI == API_Type::None) return true;

        API_Type previousAPI = s_ActiveAPI;

        // 1. Compatibility Check: Does the system have the required files/libraries for newAPI?
        if (!Is_APISupported(newAPI)) {
            s_ActiveWarning = std::format(
                "[{}] not found! Reverting changes and keeping [{}].",
                Get_APIName(newAPI), Get_APIName(previousAPI));
            LOG(Warn, LogLevel::Core, "[RHI] {}", s_ActiveWarning);
            return false; // Safely revert, keep existing backend, NEVER crash
        }

        SDL_Window *window = Window::Get_Handle();
        if (!window) return false;

        LOG(Info, LogLevel::Core, "RHI_Manager: Swapping backend from [{}] to [{}]...",
            Get_APIName(previousAPI), Get_APIName(newAPI));

        // Preserve current user settings across the swap
        BOOL  previousVSync      = Get_VSync();
        COLOR previousClearColor = Get_ClearColor();

        // 2. Shutdown current backend
        ShutdownBackend(previousAPI);

        // 3. Initialize new backend on the same window
        s_ActiveAPI = newAPI;
        if (!InitBackend(s_ActiveAPI, window)) {
            s_ActiveWarning = std::format(
                "[{}] not found or failed to initialize! Reverting changes to [{}].",
                Get_APIName(newAPI), Get_APIName(previousAPI));
            LOG(Warn, LogLevel::Core, "[RHI] {}", s_ActiveWarning);

            // Revert back to what they had in the first place!
            s_ActiveAPI = previousAPI;
            if (!InitBackend(s_ActiveAPI, window)) {
                // If previous also somehow failed, default to Vulkan or None
                LOG(Warn, LogLevel::Core, "[RHI] Previous backend failed! Defaulting to [Vulkan].");
                s_ActiveAPI = API_Type::Vulkan;
                if (!InitBackend(s_ActiveAPI, window)) {
                    LOG(Error, LogLevel::Core, "[RHI] Vulkan not found! Defaulting to [None].");
                    s_ActiveAPI = API_Type::None;
                }
            }
            Set_VSync(previousVSync);
            Set_ClearColor(previousClearColor);
            return false;
        }

        // Apply preserved settings to the newly activated backend
        Set_VSync(previousVSync, false);
        Set_ClearColor(previousClearColor);

        // Immediately save the active API and VSync to the loaded config file
        Config::Set("Graphics", "API",   Get_APIName(s_ActiveAPI));
        Config::Set("Graphics", "VSync", previousVSync);
        Config::Save();

        LOG(Info, LogLevel::Core, "RHI_Manager: Successfully swapped backend to [{}]!", Get_APIName(s_ActiveAPI));
        return true;
    }

    // Alias for Swap_API
    static inline BOOL Swap(API_Type newAPI) {
        return Swap_API(newAPI);
    }

    // ========================================================================
    // Frame Rendering
    // ========================================================================

    // Prepares the screen for drawing. Clears screen using Dynamic Rendering.
    // Returns TRUE if frame is ready to render; FALSE if minimized or resizing.
    static inline BOOL Begin_Frame() {
        if (!s_IsInitialized) return false;

        switch (s_ActiveAPI) {
            case API_Type::Vulkan:    return R_Vulkan::Begin_Frame();
            case API_Type::DirectX11: return R_DirectX::Begin_Frame();
            default:                  return false;
        }
    }

    // Finishes rendering and presents the frame to screen
    static inline void End_Frame() {
        if (!s_IsInitialized) return;

        switch (s_ActiveAPI) {
            case API_Type::Vulkan:    R_Vulkan::End_Frame();  break;
            case API_Type::DirectX11: R_DirectX::End_Frame(); break;
            default: break;
        }
    }

    // ========================================================================
    // Settings & Queries
    // ========================================================================

    // Turns VSync ON or OFF dynamically across all backends.
    // If bSaveToConfig is TRUE (default), immediately saves the change to Config.
    static inline void Set_VSync(BOOL enabled, BOOL bSaveToConfig = true) {
        switch (s_ActiveAPI) {
            case API_Type::Vulkan:    R_Vulkan::Set_VSync(enabled);  break;
            case API_Type::DirectX11: R_DirectX::Set_VSync(enabled); break;
            default: break;
        }

        if (bSaveToConfig) {
            Config::Set("Graphics", "VSync", enabled);
            Config::Set("Window",   "VSync", enabled);
            Config::Save();
        }
    }

    static inline BOOL Get_VSync() {
        switch (s_ActiveAPI) {
            case API_Type::Vulkan:    return R_Vulkan::Get_VSync();
            case API_Type::DirectX11: return R_DirectX::Get_VSync();
            default:                  return false;
        }
    }

    static inline void Set_ClearColor(const COLOR &color) {
        switch (s_ActiveAPI) {
            case API_Type::Vulkan:    R_Vulkan::Set_ClearColor(color);  break;
            case API_Type::DirectX11: R_DirectX::Set_ClearColor(color); break;
            default: break;
        }
    }

    static inline COLOR Get_ClearColor() {
        switch (s_ActiveAPI) {
            case API_Type::Vulkan:    return R_Vulkan::Get_ClearColor();
            case API_Type::DirectX11: return R_DirectX::Get_ClearColor();
            default:                  return COLOR(0, 0, 0, 255);
        }
    }

    static inline StringView Get_GPUName() {
        switch (s_ActiveAPI) {
            case API_Type::Vulkan:    return R_Vulkan::Get_GPUName();
            case API_Type::DirectX11: return R_DirectX::Get_GPUName();
            default:                  return "None";
        }
    }

    static inline API_Type Get_ActiveAPI() {
        return s_ActiveAPI;
    }

    static inline const Char *Get_APIName(API_Type api) {
        switch (api) {
            case API_Type::Vulkan:    return "Vulkan";
            case API_Type::DirectX11: return "DirectX 11";
            case API_Type::DirectX9:  return "DirectX 9";
            default:                  return "None";
        }
    }

    static inline BOOL Is_Initialized() {
        return s_IsInitialized;
    }
};

using RHI         = RHIManager; // Short alias
using RHI_Manager = RHIManager; // Descriptive unique alias
using M_RHI       = RHIManager; // Manager alias
using R_RHI       = RHIManager; // RHI folder-prefix alias
