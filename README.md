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
│   │       │   ├── M_Log.h          # LogManager    (alias Log:: / M_Log::)       — Thread-safe logger with strict LogLevel & PRINT()
│   │       │   ├── M_File.h         # FileManager   (alias File:: / M_File::)     — Thread-safe file I/O (Open, Write, Close, Clean, Delete)
│   │       │   ├── M_Time.h         # TimeManager   (alias Time:: / M_Time::)     — High-precision DeltaTime, FPS, Uptime, Timestamps, Cooldowns
│   │       │   ├── M_Crash.h        # CrashManager  (alias Crash:: / M_Crash::)   — Cross-platform hardware crash catcher & stacktrace reporter
│   │       │   └── M_Config.h       # ConfigManager (alias Config:: / M_Config::) — .ini / .cfg settings reader & writer with auto-defaults
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

## Module Quick Reference & Examples

### 1. Config Manager (`M_Config.h` — `Config::` / `M_Config::`)
Loads and saves `.ini` / `.cfg` files. If a file or key does not exist yet, `Config::Get` automatically populates it with your default value so `Config::Save()` creates a complete `.ini` file automatically:

```cpp
Config::Load("Config/Client.ini");

INT    width      = Config::Get("Window",  "Width",       1920);
INT    height     = Config::Get("Window",  "Height",      1080);
BOOL   fullscreen = Config::Get("Window",  "Fullscreen",  false);
FLOAT  mouseSens  = Config::Get("Input",   "Sensitivity", 1.5f);
String serverIP   = Config::Get("Network", "ServerIP",    "127.0.0.1");

// Update a setting at runtime and save to disk:
Config::Set("Window", "Fullscreen", true);
Config::Save();
```

Generates `Config/Client.ini`:
```ini
[Input]
Sensitivity = 1.5000

[Network]
ServerIP = 127.0.0.1

[Window]
Fullscreen = true
Height = 1080
Width = 1920
```

---

### 2. Crash Manager (`M_Crash.h` — `Crash::` / `M_Crash::`)
Catches hardware crashes (Null Pointer, Divide-by-Zero, Abort), unhandled exceptions, and manual safety checks, writing a dated crash log (`ClientCrash_DD.MM.YY_HH.MM.log`) with the exact file name, line number, and function:

```cpp
Crash::Init("Logs/ClientCrash.log");

// Safety check (if false, halts and logs exact File, Line, and Function):
Crash::Check(playerPtr != nullptr, "Player pointer was NULL during spawn!");

// Manual fatal crash trigger:
Crash::Trigger("Vulkan GPU Device Lost!");
```

---

### 3. File Manager (`M_File.h` — `File::` / `M_File::`)
Thread-safe file reader and writer (auto-creates parent directories):

```cpp
// Write (bOverWrite = true overwrites file; false appends to the end)
File::Write("Logs/Client.log", "Client started!\n", true);
File::Write("Logs/Client.log", "Appended line!\n", false);

// Read file into RAM and free it when done
String data = File::Open("Config/Client.ini");
File::Close(data);
```

---

### 4. Log Manager (`M_Log.h` — `LOG(...)` & `PRINT(...)`)
Thread-safe console logger with strict `LogLevel` filtering (`Core`, `Developer`, `User`):

```cpp
PRINT("Quick print (always visible)!");
LOG(Info,  LogLevel::Developer, "Player spawned at {}", playerPos);
LOG(Warn,  LogLevel::Developer, "High ping detected: {}ms", ping);
LOG(Error, LogLevel::Core,      "Failed to load asset!");
```

---

### 5. Complete Application Loop Example (`Game/Client/Client.cpp`)

```cpp
#include <Core.h>

int main() {
    // 1. Initialize Crash Handler
    Crash::Init("Logs/ClientCrash.log");

    // 2. Load Config (auto-creates Config/Client.ini on Save if missing)
    Config::Load("Config/Client.ini");
    INT width  = Config::Get("Window", "Width",  1920);
    INT height = Config::Get("Window", "Height", 1080);
    Config::Save();

    PRINT("Client Initialized ({}x{})! PI = {}", width, height, Math::PI);

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
