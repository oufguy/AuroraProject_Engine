# AuroraProject Engine — Architecture & Directory Guide

A lightweight, cross-platform C++23 application & client-server engine built from scratch for high performance. Designed with a strict, developer-friendly API, zero bloat, and native **Windows** & **Linux** support.

---

## High-Level Dependency Rule

* **`Engine/`** is completely application-agnostic: It has zero knowledge of specific `Game`, `Client`, or `Server` rules.
* **`Game/`** modules (`Client`, `Server`, `Shared`) include `<Core.h>` and depend on `EngineSource`, never the other way around.
* **Headless Servers** only use core/network engine modules (`Utilities`, `Managers`, `Network`) and never require graphics or audio hardware.

---

## Design Principles

* **Simple first.** The engine runs on a **single main thread** by design (like classic 2000s MMO clients & per-zone servers). No thread pool, no data races, easy debugging.
  * Servers scale by running **more processes** (one per zone/role: `FieldServer`, `NPCServer`, ...), not more threads.
  * Slow *waiting* work (database queries) lives in its own server process. Assets are loaded up front / per-region behind a loading screen.
  * A job system can be added later *inside* the engine if ever needed, without changing game code.
* **Zero-cost when unused.** E.g. `Input::Update()` returns immediately when no window is open, so headless servers never pay for keyboard/mouse polling.
* **Cross-platform only.** No `<windows.h>` / DirectX in engine code. Everything goes through the C++23 standard library, SDL3, and Vulkan 1.3 Core.
* **Naming convention.** Folder prefix + two aliases per class: `U_` Utilities, `M_` Managers (`Log` / `M_Log`), `I_` Input devices (`Keyboard` / `I_Keyboard`), `R_` RHI (`Device` / `R_Device`), `C_` Classes (`Entity` / `C_Entity`).

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
│   │       ├── Core.h / Core.cpp    # Master include hub (<Core.h>) + lifecycle: Core::BeginPlay / Tick / Stop / EndPlay
│   │       │
│   │       ├── Classes/             # World object data structures (C_ prefix)
│   │       │   └── C_Entity.h       # Lightweight POD Entity struct (Transform, Type, Color, Transparency)
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
│   │       │   ├── M_Config.h       # ConfigManager (alias Config:: / M_Config::) — .ini / .cfg settings reader & writer with auto-defaults
│   │       │   ├── M_Input.h        # InputManager  (alias Input:: / M_Input::)   — SDL3 hardware event poller & remappable action keybinds
│   │       │   └── M_Window.h       # WindowManager (alias Window:: / M_Window::) — SDL3 OS window (Windowed / Borderless / Fullscreen)
│   │       │
│   │       ├── Input/               # Hardware input device state (I_ prefix)
│   │       │   ├── I_Keyboard.h     # Keyboard      (alias I_Keyboard::)          — Physical key state (Pressed, Held, Released)
│   │       │   └── I_Mouse.h        # Mouse         (alias I_Mouse::)             — Mouse buttons, cursor pos, raw look delta, scroll wheel
│   │       │
│   │       ├── RHI/                 # Render Hardware Interface (Vulkan 1.3 Core backend)
│   │       │   ├── R_Device.h       # RenderDevice  (alias Device:: / R_Device::) — Vulkan 1.3 instance, surface, physical & logical GPU
│   │       │   └── R_Swapchain.h    # RenderSwapchain (alias Swapchain:: / R_Swapchain::) — Dynamic rendering, triple-buffering, VSync
│   │       │
│   │       ├── Audio/               # Sound system abstraction
│   │       ├── Framework/           # Entity/Component model, application loop, world structure
│   │       ├── Network/             # Cross-platform TCP/UDP socket abstractions
│   │       └── Renderer/            # Scene manager, materials, mesh & UI rendering passes
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

// Check if a file is available on disk
if (File::Find("Config/Client.ini")) { /* File exists! */ }

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

### 5. Input System (`I_Keyboard.h`, `I_Mouse.h`, `M_Input.h`)
Powered by **SDL3** (statically linked with zero DLLs). Supports both direct device checks (`Keyboard::` / `Mouse::`) and remappable Action Keybinds (`Input::` / `M_Input::`) that automatically save to `Config/Client.ini` under `[Keybinds]`:

```cpp
// 1. Register remappable actions at startup (loads saved binds from Config or uses defaults):
Input::Bind("MoveForward", Key::W);
Input::Bind("Inventory",   Key::I);
Input::Bind("FirePrimary", MouseButton::MB_Left);

// 2. Input is polled automatically by Core::Tick() each frame:
while (Core::Tick()) {
    // Check remappable actions (Pressed once, Held down, or Released):
    if (Input::Get_Action("MoveForward", Input::Held))    { /* Fly forward */ }
    if (Input::Get_Action("Inventory",   Input::Pressed)) { /* Open Inventory once */ }
    if (Input::Get_Action("FirePrimary", Input::Held))    { /* Fire weapon */ }

    // Or query Mouse / Keyboard directly:
    Vector2D lookDelta = Mouse::Get_Delta();    // Raw mouse movement this frame
    FLOAT    zoomWheel = Mouse::Get_Scroll();   // Scroll wheel (+1 up, -1 down)
    if (Mouse::Get_Button(MouseButton::MB_Right, Input::Held)) { /* Rotate camera */ }
}
```

> **Note:** Mouse buttons use the `MB_` prefix (`MB_Left`, `MB_Middle`, `MB_Right`, `MB_Side1`, `MB_Side2`) so they never collide with `Key::Left` / `Key::Right`.
>
> Hardware polling only runs while a window is open (`Window::Is_Open()`). In console apps and headless servers `Input::Update()` costs ~nothing.

---

### 6. Window Manager (`M_Window.h` — `Window::` / `M_Window::`)
Opens a cross-platform OS window with SDL3 (Windows, Linux X11 & Wayland). Size and mode are read from `Config [Window]` with auto-defaults. Closing the window (**[X]** or **Alt+F4**) makes `Core::Tick()` return `false`, so the main loop exits normally:

```cpp
Window::Create("Aurora Client");   // Reads [Window] Width / Height / Mode (defaults: 1280 x 720, Windowed)

while (Core::Tick()) {
    if (Window::Is_Minimized()) { Time::Sleep(10); continue; }                 // Save CPU/GPU in the taskbar
    if (Window::Was_Resized())  { Vector2D px = Window::Get_PixelSize(); }    // Recreate swapchain later

    if (Keyboard::Get_Key(Key::F11, Input::Pressed))                           // Toggle fullscreen
        Window::Set_Mode(Window::Is_Fullscreen() ? Window::Windowed : Window::Borderless);

    Window::Set_MouseLocked(Mouse::Get_Button(MouseButton::MB_Right, Input::Held)); // MMO camera rotate
}
```

| Function | What it does |
| :--- | :--- |
| `Create(title)` / `Destroy()` | Open / close the window (the OS also cleans up automatically on exit) |
| `Set_Title`, `Set_Size`, `Set_Mode`, `Set_MouseLocked` | `Set_Size` / `Set_Mode` update `Config` in memory - call `Config::Save()` to keep them |
| `Get_Size()` / `Get_PixelSize()` | Screen units (UI, mouse) vs real pixels (render resolution on 4K / scaled displays) |
| `Is_Open`, `Is_Focused`, `Is_Minimized`, `Is_Fullscreen`, `Was_Resized` | Window state (`Was_Resized` is only `true` on the frame it happened) |
| `Get_Handle()` | Raw `SDL_Window*` for Vulkan RHI surface creation |

Window modes: `Window::Windowed`, `Window::Borderless` (desktop-resolution fullscreen, instant Alt+Tab — recommended), `Window::Fullscreen` (exclusive, changes monitor resolution).

---

### 7. Complete Application Loop Example (`Game/Client/Client.cpp`)
Instead of initializing and updating every Manager one by one, **`Core::BeginPlay`**, **`Core::Tick`**, and **`Core::EndPlay`** in [`Core.h`](Engine/Source/Runtime/Core.h) run all Engine systems automatically:

```cpp
#include <Core.h>

int main() {
    // 1. Starts CrashManager, loads Config/Client.ini, initializes SDL3 & starts the Time clock!
    Core::BeginPlay("Client");
    Window::Create("Aurora Client");
    R_Device::Init(Window::Get_Handle(), "Aurora Client");
    R_Swapchain::Init();

    // 2. Main Application Loop (Time::Update() + Input::Update() + window events every frame)
    DOUBLE lastLogTime = 0.0;
    while (Core::Tick()) {   // Returns false on window [X] / Alt+F4
        if (Window::Is_Minimized()) { Time::Sleep(10); continue; }

        // F10 = toggle VSync ON/OFF, F11 = toggle Fullscreen
        if (Keyboard::Get_Key(Key::F10, Input::Pressed)) R_Swapchain::Set_VSync(!R_Swapchain::Get_VSync());
        if (Keyboard::Get_Key(Key::F11, Input::Pressed)) Window::Set_Mode(Window::Is_Fullscreen() ? Window::Windowed : Window::Fullscreen);

        if (Time::Check_Cooldown(lastLogTime, 1.0)) {
            Window::Set_Title(StringFormat("Aurora Client | FPS: {} | VSync: {}", Time::Get_FPS(), R_Swapchain::Get_VSync() ? "ON" : "OFF"));
        }

        // Render Frame with modern Vulkan 1.3 Dynamic Rendering
        if (R_Swapchain::BeginFrame()) {
            R_Swapchain::EndFrame();
        }
    }

    // 3. Shuts down Swapchain, Vulkan Device, Window, and saves Config
    Core::EndPlay();
    return 0;
}
```

| Function | What it does |
| :--- | :--- |
| `Core::BeginPlay(appName)` | `Crash::Init("Logs/{app}Crash.log")`, `Config::Load("Config/{app}.ini")`, `SDL_Init`, `Time::Init()` |
| `Core::Tick()` | `Time::Update()` + `Input::Update()` (+ window events). Returns `false` when the window closes |
| `Core::EndPlay()` | `R_Swapchain::Shutdown()`, `R_Device::Shutdown()`, `Window::Destroy()`, `Config::Save()`, `SDL_Quit()` |

---

## Performance Notes

An empty `while (Core::Tick())` loop runs at **~12,000,000 iterations/sec (~83 ns) in `Debug`** with no window open (~1,100,000 with a window, since SDL then asks the OS for window messages every frame). Most of the no-window cost is the `Clock::now()` hardware timer read.

* Measure performance in **`Development`** or **`Release`** — `Debug` disables inlining and adds runtime checks.
* Above ~1,000 FPS, compare **frame time (ms)**, not FPS. A jump from 12M to 10M FPS is only ~17 ns and is usually just noise (code alignment, CPU boost, core scheduling).
* Input arrays (`Keyboard` / `Mouse`) are only copied on frames where a button actually changed (`s_IsDirty` flag).
