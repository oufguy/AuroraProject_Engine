#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>

// ============================================================================
// DirectX 11 Graphics Backend (D3D11 + DXGI SwapChain Pipeline)
// ============================================================================
// Implementation is fully isolated in R_DirectX.cpp to prevent Windows macro
// pollution (#define Escape, min, max) from infecting the rest of the engine.
// ============================================================================
class DirectXRenderer {
private:
    static inline BOOL   s_IsInitialized = false;
    static inline BOOL   s_VSync         = true;
    static inline COLOR  s_ClearColor    = COLOR(30, 40, 55, 255);
    static inline String s_GPUName       = "DirectX 11 Device";

public:
    // Pure runtime probe: checks whether d3d11.dll is present and functional
    static BOOL Is_Supported();

    // Initializes DirectX 11 Device, Immediate Context, and DXGI SwapChain
    static BOOL Init(SDL_Window *window);

    // Shuts down all D3D11 and DXGI resources cleanly
    static void Shutdown();

    // Handles live window resizing by recreating swapchain buffers
    static void Recreate();

    // Prepares the frame by clearing the render target view to s_ClearColor
    static BOOL Begin_Frame();

    // Presents the frame to screen, obeying s_VSync
    static void End_Frame();

    // VSync & Settings Accessors
    static inline void       Set_VSync(BOOL enabled)       { s_VSync = enabled; }
    static inline BOOL       Get_VSync()                   { return s_VSync; }
    static inline void       Set_ClearColor(const COLOR &c){ s_ClearColor = c; }
    static inline COLOR      Get_ClearColor()              { return s_ClearColor; }
    static inline StringView Get_GPUName()                 { return s_GPUName; }
    static inline BOOL       Is_Initialized()              { return s_IsInitialized; }
};

using R_DirectX = DirectXRenderer;
