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
// Engine Managers (Log, File, Time, Crash, Config, Window, Input, Threads, RHI)
// ============================================================================
#include <M_Log.h>
#include <M_File.h>
#include <M_Time.h>
#include <M_Crash.h>
#include <M_Config.h>
#include <M_Window.h>
#include <M_Input.h>
#include <M_Threads.h>

// ============================================================================
// Render Hardware Interface (RHI)
// ============================================================================
#include <RHI_Manager.h>

// ============================================================================
// Base Classes (Entity)
// ============================================================================
#include <B_Entity.h>

// ============================================================================
// Process Types (Window Client vs Headless Server)
// ============================================================================
enum class ProcessType : BYTE {
    Window   = 0, // Full graphical client (Window + RHI Vulkan/DirectX + Input)
    Headless = 1  // Dedicated server / console tool (No Window, No GPU, pure logic & network)
};

// ============================================================================
// Master Engine Lifecycle & Process Runner (Core::Start_Process / Tick / End_Process)
// ============================================================================
// Quick Usage Example in main():
//   // Client:
//   Core::Start_Process("Client", ProcessType::Window);
//   while (Core::Tick()) {
//       if (RHI_Manager::Begin_Frame()) { RHI_Manager::End_Frame(); }
//   }
//   Core::End_Process();
//
//   // Server:
//   Core::Start_Process("FieldServer", ProcessType::Headless);
//   while (Core::Tick()) { /* Server Ticks */ }
//   Core::End_Process();
// ============================================================================
class Core {
private:
    static inline BOOL        s_IsRunning   = false;
    static inline ProcessType s_ProcessType = ProcessType::Window;

public:
    // Starts the engine process based on ProcessType (Window vs Headless)
    // - Window:   Initializes Crash, Config, SDL3, Time, Window, and RHI (Vulkan default).
    // - Headless: Initializes Crash, Config, SDL3 Events, Time (Zero Window/GPU overhead).
    // Example: Core::Start_Process("Client", ProcessType::Window);
    // Example: Core::Start_Process("FieldServer", ProcessType::Headless);
    static inline BOOL Start_Process(StringView appName, ProcessType type = ProcessType::Window, StringView windowTitle = "") {
        s_IsRunning   = true;
        s_ProcessType = type;

        // 1. Crash Manager -> "{appName}Crash.log"
        Crash::Init(StringFormat("{}Crash.log", appName));

        // 2. Config Manager -> "Settings.ini"
        Config::Load("Settings.ini");

        // 3. SDL3 Base Events
        SDL_Init(SDL_INIT_EVENTS);

        // 4. Time System
        Time::Init();

        // 5. Worker Thread Pool (Auto-detects CPU cores, runs on Client & Server)
        Threads::Init();

        // 6. If Window process, create the OS Window & boot the RHI graphics backend!
        if (s_ProcessType == ProcessType::Window) {
            String title = windowTitle.empty() ? String(appName) : String(windowTitle);
            if (!Window::Create(title)) {
                LOG(Error, LogLevel::Core, "Core::Start_Process failed to create Window!");
                Stop_Process();
                return false;
            }

            // Initializes RHI graphics backend (auto-reads API & VSync from ConfigManager!)
            if (!RHI_Manager::Init()) {
                LOG(Error, LogLevel::Core, "Core::Start_Process failed to initialize RHI!");
                Stop_Process();
                return false;
            }
        }

        LOG(Info, LogLevel::Core, "Core::Start_Process [{}] booted successfully ({})",
            appName, (s_ProcessType == ProcessType::Window) ? "Window" : "Headless");
        return true;
    }

    // Updates all per-frame Engine systems. Returns TRUE while running.
    // Handles window minimizing, time updates, and SDL input polling automatically.
    // Example: while (Core::Tick()) { ... }
    static inline BOOL Tick() {
        if (!s_IsRunning) return false;

        Time::Update();

        if (s_ProcessType == ProcessType::Window) {
            if (!Input::Update()) {
                s_IsRunning = false;
                return false;
            }
        }

        return true;
    }

    // Signals the engine to exit the while (Core::Tick()) loop cleanly.
    // Example: Core::Stop_Process();
    static inline void Stop_Process() {
        s_IsRunning = false;
    }

    // Cleanly shuts down RHI, Window, saves Config and quits SDL subsystems.
    // Example: Core::End_Process();
    static inline void End_Process() {
        if (s_ProcessType == ProcessType::Window) {
            RHI_Manager::Shutdown();
            Window::Destroy();
        }

        Threads::Shutdown();
        Config::Save();
        SDL_Quit();
        s_IsRunning = false;
        LOG(Info, LogLevel::Core, "Core::End_Process finished.");
    }

    // ------------------------------------------------------------------------
    // Convenience Aliases (Both StartProcess and Start_Process work!)
    // ------------------------------------------------------------------------
    static inline BOOL StartProcess(StringView appName, ProcessType type = ProcessType::Window, StringView windowTitle = "") {
        return Start_Process(appName, type, windowTitle);
    }
    static inline void StopProcess() { Stop_Process(); }
    static inline void EndProcess()  { End_Process(); }
    static inline void BeginPlay(StringView appName = "Settings") { Start_Process(appName, ProcessType::Window); }
    static inline void EndPlay()     { End_Process(); }

    static inline BOOL        Is_Running()       { return s_IsRunning; }
    static inline ProcessType Get_ProcessType()  { return s_ProcessType; }
};
