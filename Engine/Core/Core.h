#pragma once

#include <pch.h>

// ============================================================================
// Utilities (Types, Colors, Math)
// ============================================================================
#include <U_Variables.h>
#include <U_Colors.h>
#include <U_Math.h>

// ============================================================================
// Input Devices (Keyboard & Mouse)
// ============================================================================
#include <I_Keyboard.h>
#include <I_Mouse.h>

// ============================================================================
// Core Foundation Managers (Logging, File I/O, Time, Crash, Window, Input, Threads)
// ============================================================================
#include <M_Log.h>
#include <M_File.h>
#include <M_Time.h>
#include <M_Crash.h>
#include <M_Window.h>
#include <M_Input.h>
#include <M_Threads.h>

// ============================================================================
// Resource & Data Loaders (Loaders depending on M_File & M_Threads)
// ============================================================================
#include <M_Config.h>
#include <M_Mesh.h>
#include <M_Texture.h>

// ============================================================================
// Render Hardware Interface (RHI)
// ============================================================================
#include <RHI_Manager.h>

// ============================================================================
// Process Types (Window Client vs Headless Server)
// ============================================================================
enum class ProcessType : BYTE {
    Window   = 0, // Full graphical client (Window + RHI Vulkan/DirectX + Input)
    Headless = 1  // Dedicated server / console tool (No Window, No GPU, pure logic & network)
};

// ============================================================================
// Master Engine Lifecycle & Process Runner (Core::BeginPlay / Tick / EndPlay)
// ============================================================================
// Quick Usage Example in main():
//   // Client:
//   Core::BeginPlay("Client", ProcessType::Window);
//   while (Core::Tick()) {
//       if (RHI_Manager::Begin_Frame()) { RHI_Manager::End_Frame(); }
//   }
//   Core::EndPlay();
//
//   // Server:
//   Core::BeginPlay("FieldServer", ProcessType::Headless);
//   while (Core::Tick()) { /* Server Ticks */ }
//   Core::EndPlay();
// ============================================================================
class Core {
private:
    static inline BOOL        s_IsRunning   = false;
    static inline BOOL        s_IsDebugMode = false;
    static inline ProcessType s_ProcessType = ProcessType::Window;

public:
    // Boots all automated Engine lifecycle systems based on ProcessType (Window vs Headless):
    // - Window:   Initializes Crash, SDL3, Time, Threads, Window, and RHI.
    // - Headless: Initializes Crash, SDL3 Events, Time, Threads (Zero Window/GPU overhead).
    // Example: Core::BeginPlay("Client", ProcessType::Window);
    // Example: Core::BeginPlay("FieldServer", ProcessType::Headless);
    static inline BOOL BeginPlay(
        StringView appName,
        ProcessType type = ProcessType::Window,
        StringView windowTitle = "")
    {
        s_IsRunning   = true;
        s_ProcessType = type;

        // 1. Crash Manager -> "{appName}Crash.log"
        M_Crash::Init(StringFormat("{}Crash.log", appName));

        // 2. SDL3 Base Events
        SDL_Init(SDL_INIT_EVENTS);

        // 3. Time System
        M_Time::Init();

        // 4. Worker Thread Pool (Auto-detects CPU cores, runs on Client & Server)
        M_Threads::Init();

        // 5. If Window process, create the OS Window & boot the RHI graphics backend!
        if (s_ProcessType == ProcessType::Window) {
            if (!M_Window::Create(windowTitle)) {
                LOG(Error, LogLevel::Core, "Core::BeginPlay failed to create Window!");
                Stop();
                return false;
            }

            // Initializes RHI graphics backend
            if (!RHI_Manager::Init()) {
                LOG(Error, LogLevel::Core, "Core::BeginPlay failed to initialize RHI!");
                Stop();
                return false;
            }
        }

        LOG(Info, LogLevel::Core, "Core::BeginPlay [{}] booted successfully ({})",
            appName,
            (s_ProcessType == ProcessType::Window) ? "Window" : "Headless"
        );
        return true;
    }

    // Backward-compatible alias for BeginPlay
    static inline BOOL Start_Process(StringView appName, ProcessType type = ProcessType::Window, StringView windowTitle = "") {
        return BeginPlay(appName, type, windowTitle);
    }

    // Updates all per-frame Engine systems. Returns TRUE while running.
    // Handles time updates, SDL input polling, World Tick (in _GAME), and minimized window CPU sleep.
    // Example: while (Core::Tick()) { ... }
    static inline BOOL Tick() {
        if (!s_IsRunning) return false;

        M_Time::Update();

        // Time-sliced mesh and texture streaming processing (1 asset per tick to prevent hitches)
        M_Mesh::Tick_Streaming(1);
        M_Texture::Tick_Streaming(1);

        if (s_ProcessType == ProcessType::Window) {
            if (!M_Input::Update()) {
                s_IsRunning = false;
                return false;
            }

            // Automatically throttle CPU loop when window is minimized to the taskbar
            if (M_Window::Is_Minimized()) {
                M_Time::Sleep(10);
            }
        }

        return true;
    }

    // Signals the engine to exit the while (Core::Tick()) loop cleanly.
    // Example: Core::Stop();
    static inline void Stop() {
        s_IsRunning = false;
    }

    // Backward-compatible alias for Stop
    static inline void Stop_Process() {
        Stop();
    }

    // Cleanly shuts down RHI, Window, Texture, Mesh, Threads and quits SDL subsystems.
    // Example: Core::EndPlay();
    static inline void EndPlay() {
        if (s_ProcessType == ProcessType::Window) {
            RHI_Manager::Shutdown();
            M_Window::Destroy();
        }

        M_Texture::Shutdown();
        M_Mesh::Shutdown();
        M_Threads::Shutdown();
        SDL_Quit();
        s_IsRunning = false;
        LOG(Info, LogLevel::Core, "Core::EndPlay finished.");
    }

    // Backward-compatible alias for EndPlay
    static inline void End_Process() {
        EndPlay();
    }

    static inline BOOL        Is_Running()       { return s_IsRunning; }
    static inline ProcessType Get_ProcessType()  { return s_ProcessType; }

    // ========================================================================
    // Debug Mode (Only available in Debug & Development builds — Locked OFF in Release!)
    // ========================================================================

    // Returns TRUE if this build is Debug or Development (FALSE in shipped Release builds)
    [[nodiscard]] static constexpr BOOL Is_DebugBuild() {
#if defined(_RELEASE)
        return false;
#else
        return true; // _DEBUG or _DEVELOPMENT
#endif
    }

    // Returns TRUE if Debug Mode (F1) is currently active (Always FALSE in Release builds)
    [[nodiscard]] static inline BOOL Is_DebugMode() {
#if defined(_RELEASE)
        return false;
#else
        return s_IsDebugMode;
#endif
    }

    // Sets Debug Mode ON/OFF (Ignored in Release builds so players can never enable it)
    static inline BOOL Set_DebugMode(BOOL enabled) {
#if defined(_RELEASE)
        (void)enabled;
        s_IsDebugMode = false;
        return false;
#else
        s_IsDebugMode = enabled;
        return s_IsDebugMode;
#endif
    }

    // Toggles Debug Mode ON/OFF in Debug & Development builds (No-op in Release builds)
    static inline BOOL Toggle_DebugMode() {
        return Set_DebugMode(!Is_DebugMode());
    }
};
