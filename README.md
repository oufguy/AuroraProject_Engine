# AuroraProject Engine — Architecture & Directory Guide

A lightweight, cross-platform C++23 application & client-server engine built from scratch for high performance. Designed with a strict, developer-friendly API, zero bloat, and native **Windows** & **Linux** support.

---

## High-Level Dependency Rule

* **`Engine/`** is completely application-agnostic: It has zero knowledge of specific `Game`, `Client`, or `Server` rules.
* **`Game/`** modules (`Client`, `Server`, `Shared`) include `<Core.h>` and depend on `EngineSource`, never the other way around.
* **Headless Servers** only use core/network engine modules (`Utilities`, `Managers`, `Network`) and never require graphics or audio hardware.

---

## Directory Breakdown (`Engine/` + Example `Game/` Structure)

```text
AuroraProject/
├── CMakeLists.txt                   # Root CMake build script (Static Engine + Client/Server targets)
├── CMakePresets.json                # Build configurations: Debug, Development, Release
│
├── Engine/                          # Reusable, standalone open-source engine framework
│   ├── Source/
│   │   ├── Shared/
│   │   │   └── pch.h                # Precompiled standard C++23 headers
│   │   │
│   │   └── Runtime/
│   │       ├── Core.h / Core.cpp    # Master engine include hub (<Core.h>)
│   │       │
│   │       ├── Utilities/           # Passive data types, colors & math formulas (U_ prefix)
│   │       │   ├── U_Variables.h    # Explicit types (INT, FLOAT, BYTE, DWORD, String, Vector2D/3D/4D, COLOR, Array)
│   │       │   ├── U_Colors.h       # Color presets & console log palette
│   │       │   └── U_Math.h         # Base Math:: & 2D/3D Math::Extended:: (Angles, Vectors, Distance, Lerp, Random)
│   │       │
│   │       ├── Managers/            # Active engine systems & handlers (M_ prefix)
│   │       │   ├── M_Log.h          # LogManager   (alias Log::)   — Thread-safe logger with strict LogLevel & PRINT()
│   │       │   ├── M_File.h         # FileManager  (alias File::)  — Thread-safe file I/O (Open, Write, Close, Clean, Delete)
│   │       │   ├── M_Time.h         # TimeManager  (alias Time::)  — High-precision DeltaTime, FPS, Uptime, Timestamps, Cooldowns
│   │       │   └── M_Crash.h        # CrashManager (alias Crash::) — Cross-platform hardware crash catcher & stacktrace reporter
│   │       │
│   │       ├── Audio/               # Sound system abstraction
│   │       ├── Framework/           # Entity/Component model, application loop, world structure
│   │       ├── Input/               # Keyboard & Mouse input handling
│   │       ├── Network/             # Cross-platform TCP/UDP socket abstractions
│   │       ├── Renderer/            # Scene manager, materials, mesh & UI rendering passes
│   │       └── RHI/                 # Render Hardware Interface (Vulkan backend)
│   │
│   ├── ThirdParty/                  # External header/static dependencies
│   └── Tools/                       # Asset packaging & pipeline tools
│
└── Game/                            # Example application / game workspace (kept separate from Engine)
    ├── Client/                      # Client executable target (AWO_Client)
    │   ├── Client.cpp               # Client entry point (includes <Core.h>)
    │   ├── Gameplay/                # Camera, movement, client-side logic
    │   └── Interface/               # UI screens, HUD, menus
    │
    ├── Server/                      # Dedicated headless server targets
    │   ├── Common/                  # Shared inter-server logic
    │   ├── FieldServer/             # Main world/zone server (Main.cpp)
    │   ├── IMServer/                # Chat & messaging server
    │   ├── NPCServer/               # AI & spawn controller server
    │   └── PreServer/               # Login & gateway routing server
    │
    └── Shared/                      # Shared definitions between Client and Servers
        ├── GameData/                # Shared data structures & constants
        └── Protocol/                # Network packet opcodes & layouts
```

---

## Build Configurations (`CMakePresets.json`)

| Preset | Macro Defined | Default `LogLevel` | Purpose |
| :--- | :--- | :--- | :--- |
| **`Debug`** | `_DEBUG` | `LogLevel::Core` | Shows **all** logs (`Core`, `Developer`, `User`) for low-level engine debugging. |
| **`Development`** | `_DEVELOPMENT` | `LogLevel::Developer` | Optimized with debug symbols; hides internal `Core` spam and shows `Developer` + `User` logs. |
| **`Release`** | `_RELEASE` | `LogLevel::User` | Maximum optimization; only outputs `User` logs unless unlocked via `Log::Set_LogLevel()`. |

---

## Quick Example (`Game/Client/Client.cpp`)

To create an application using the engine, create `Game/Client/Client.cpp` and include `<Core.h>`:

```cpp
#include <Core.h>

int main() {
    // 1. Initialize Crash Manager (auto-creates dated ClientCrash_DD.MM.YY_HH.MM.log on crash)
    Crash::Init("Logs/ClientCrash.log");

    // 2. Quick print
    PRINT("Client Initialized! PI = {}", Math::PI);

    // 3. Main Application Loop
    DOUBLE lastLogTime = 0.0;
    while (true) {
        Time::Update();

        // Print DeltaTime & FPS once every 1.0 second
        if (Time::Check_Cooldown(lastLogTime, 1.0)) {
            LOG(Info, LogLevel::Developer, "DeltaTime: {}s | FPS: {}", Time::Get_DeltaTime(), Time::Get_FPS());
        }
    }

    return 0;
}
```