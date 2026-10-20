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
// Engine Managers (Log, File, Time, Crash, Config, Window, Input)
// ============================================================================
#include <M_Log.h>
#include <M_File.h>
#include <M_Time.h>
#include <M_Crash.h>
#include <M_Config.h>
#include <M_Window.h>
#include <M_Input.h>

// ============================================================================
// Render Hardware Interface (Vulkan 1.3 Core)
// ============================================================================
#include <R_Device.h>
#include <R_Swapchain.h>

// ============================================================================
// Engine World Classes (Entity)
// ============================================================================
#include <C_Entity.h>

// ============================================================================
// Master Engine Lifecycle (Core::BeginPlay, Core::Tick, Core::EndPlay)
// ============================================================================
// Quick Usage Example in main():
//   Core::BeginPlay("Client");
//   while (Core::Tick()) {
//       // Your game / server logic here!
//   }
//   Core::EndPlay();
// ============================================================================
class Core {
private:
    static inline BOOL s_IsRunning = false;

public:
    // 1. Starts all Engine Managers at once (Crash, Config, SDL3 Events, Time)!
    // Pass your app name (e.g. "Settings", "Client", or "FieldServer") to auto-name Config & Crash files!
    // Example: Core::BeginPlay("Settings");
    static inline void BeginPlay(StringView appName = "Settings") {
        s_IsRunning = true;

        // Initialize Crash Manager -> "{appName}Crash.log"
        Crash::Init(StringFormat("{}Crash.log", appName));

        // Load Config -> "{appName}.ini"
        Config::Load(StringFormat("{}.ini", appName));

        // Initialize SDL3 event system
        SDL_Init(SDL_INIT_EVENTS);

        // Prime the clock so the first frame has a clean DeltaTime
        Time::Init();

        LOG(Info, LogLevel::Core, "Core::BeginPlay initialized");
    }

    // 2. Updates all per-frame Engine systems (Time + Input + Window events)!
    // Returns TRUE while running, or FALSE when the window closes or Core::Stop() is called.
    // Example: while (Core::Tick()) { ... }
    static inline BOOL Tick() {
        if (!s_IsRunning) return false;

        Time::Update();

        if (!Input::Update()) {
            s_IsRunning = false;
            return false;
        }

        return true;
    }

    // 3. Signals the Engine to exit the while (Core::Tick()) loop cleanly at the end of the frame.
    // Example: if (Keyboard::Get_Key(Key::Escape, Input::Pressed)) { Core::Stop(); }
    static inline void Stop() {
        s_IsRunning = false;
    }

    // 4. Closes the Window, saves Config/Keybinds and shuts down Engine subsystems cleanly.
    // Example: Core::EndPlay();
    static inline void EndPlay() {
        R_Swapchain::Shutdown();
        R_Device::Shutdown();
        Window::Destroy();
        Config::Save();
        SDL_Quit();
        LOG(Info, LogLevel::Core, "Core::EndPlay shut down cleanly.");
    }
};
