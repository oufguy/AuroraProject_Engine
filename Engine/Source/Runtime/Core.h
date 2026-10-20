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
// Engine Managers (Log, File, Time, Crash, Config, Input)
// ============================================================================
#include <M_Log.h>
#include <M_File.h>
#include <M_Time.h>
#include <M_Crash.h>
#include <M_Config.h>
#include <M_Input.h>

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
    // Pass your app name (e.g. "Client" or "FieldServer") to auto-name Config & Crash files!
    // Example: Core::BeginPlay("Client");
    static inline void BeginPlay(StringView appName = "Application") {
        s_IsRunning = true;

        // Initialize Crash Manager -> "Logs/ClientCrash_DD.MM.YY_HH.MM.log"
        Crash::Init(StringFormat("Logs/{}Crash.log", appName));

        // Load Config -> "Config/Client.ini"
        Config::Load(StringFormat("Config/{}.ini", appName));

        // Initialize SDL3 event system
        SDL_Init(SDL_INIT_EVENTS);

        // Prime the clock so the first frame has a clean DeltaTime
        Time::Init();

        LOG(Info, LogLevel::Core, "Core::BeginPlay initialized [{}]", appName);
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

    // 4. Saves Config/Keybinds and shuts down Engine subsystems cleanly before main() exits.
    // Example: Core::EndPlay();
    static inline void EndPlay() {
        Config::Save();
        SDL_Quit();
        LOG(Info, LogLevel::Core, "Core::EndPlay shut down cleanly.");
    }
};
