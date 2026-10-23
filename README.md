# AuroraProject Engine — Architecture & Directory Guide

A lightweight, cross-platform C++23 application & client-server engine built from scratch for high performance. Designed with a strict, developer-friendly API, zero bloat, and native **Windows** & **Linux** support.

---

## High-Level Dependency Rule

* **`Engine/`** is completely application-agnostic: It has zero knowledge of specific `Game`, `Client`, or `Server` rules.
* **`Game/`** modules (`Client`, `Server`, `Shared`) include `<Core.h>` and depend on `EngineSource`, never the other way around.
* **Headless Servers** only use core/network engine modules (`Utilities`, `Managers`, `Network`) and never require graphics or audio hardware.

---

## Design Principles

* **Simple first.** Core gameplay logic runs predictably on the **main thread** (like classic MMO architectures). Heavy background work (asset decoding, texture loading, or updating thousands of missiles/projectiles) is delegated to **`M_Threads`** via simple manual tasks (`Threads::Run`) or multi-core batch loops (`Threads::Parallel_For`) without stalling the main loop.
  * Servers scale by running **more processes** (one per zone), not complex entangled threads.
  * Slow *waiting* work (database queries) lives in its own server process. Assets are loaded up front / per-region behind a loading screen.
* **Zero-cost when unused.** E.g. `Input::Update()` returns immediately when no window is open, so headless servers never pay for keyboard/mouse polling. Headless servers completely skip RHI graphics backend initialization.
* **Cross-platform only.** Zero Windows OS headers (`<windows.h>` strictly banned). Graphics backends support both modern Vulkan 1.3/1.4 Core and pure dynamic DirectX 11 loaded via SDL3 dynamic symbol loading without link-time OS dependencies.
* **Naming convention.** Folder prefix + two aliases per class: `U_` Utilities, `M_` Managers (`Log` / `M_Log`), `I_` Input devices (`Keyboard` / `I_Keyboard`), `R_` RHI (`Device` / `R_Device`), `B_` BaseClasses (`Entity` / `B_Entity`).

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
│   │       ├── Core.h / Core.cpp    # Master include hub (<Core.h>) + lifecycle: Core::Start_Process / Tick / End_Process
│   │       │
│   │       ├── BaseClasses/         # Base world object data structures (B_ prefix)
│   │       │   └── B_Entity.h       # Lightweight C-style POD Entity struct (Type, Transform, Color, Transparency)
│   │       │
│   │       ├── Utilities/           # Passive data types, colors & math formulas (U_ prefix)
│   │       │   ├── U_Variables.h    # Explicit types (INT, FLOAT, BYTE, DWORD, String, Vector2D/3D/4D, COLOR, Array)
│   │       │   ├── U_Colors.h       # Color presets & console log palette
│   │       │   └── U_Math.h         # Base Math:: & 2D/3D Math::Extended:: (Angles, Vectors, Distance, Lerp, Random)
│   │       │
│   │       ├── Managers/            # Active engine systems & handlers (M_ prefix)
│   │       │   ├── Configs/
│   │       │   │   └── D_ConfigDefaults.h # Default fallback Settings.ini layout and options
│   │       │   ├── M_Log.h          # LogManager    (alias Log:: / M_Log::)       — Thread-safe logger with strict LogLevel & PRINT()
│   │       │   ├── M_File.h         # FileManager   (alias File:: / M_File::)     — Thread-safe file I/O (Open, Write, Close, Clean, Delete)
│   │       │   ├── M_Time.h         # TimeManager   (alias Time:: / M_Time::)     — High-precision DeltaTime, FPS, Uptime, Timestamps, Cooldowns
│   │       │   ├── M_Crash.h        # CrashManager  (alias Crash:: / M_Crash::)   — Cross-platform hardware crash catcher & stacktrace reporter
│   │       │   ├── M_Config.h       # ConfigManager (alias Config:: / M_Config::) — .ini / .cfg settings reader & writer with auto-defaults
│   │       │   ├── M_Threads.h      # ThreadManager (alias Threads:: / M_Threads::) — Multi-core worker pool (Run, Parallel_For)
│   │       │   ├── M_Input.h        # InputManager  (alias Input:: / M_Input::)   — SDL3 hardware event poller & remappable action keybinds
│   │       │   └── M_Window.h       # WindowManager (alias Window:: / M_Window::) — SDL3 OS window (Windowed / Borderless / Fullscreen)
│   │       │
│   │       ├── Input/               # Hardware input device state (I_ prefix)
│   │       │   ├── I_Keyboard.h     # Keyboard      (alias I_Keyboard::)          — Physical key state (Pressed, Held, Released)
│   │       │   └── I_Mouse.h        # Mouse         (alias I_Mouse::)             — Mouse buttons, cursor pos, raw look delta, scroll wheel
│   │       │
│   │       ├── RHI/                 # Render Hardware Interface (All graphics headers in 1 click)
│   │       │   ├── RHI_Manager.h    # RHIManager    (alias RHI:: / RHI_Manager::) — Master dynamic graphics dispatcher (Swap_API)
│   │       │   ├── R_Vulkan.h       # VulkanRenderer  (alias R_Vulkan::)          — Vulkan 1.3/1.4 Dynamic Rendering & Sync 2
│   │       │   ├── R_DirectX.h      # DirectXRenderer (alias R_DirectX::)         — DirectX 11 / DXGI pure dynamic runtime loader
│   │       │   └── R_DirectX.cpp    # DirectX 11 backend implementation (zero Windows OS headers / static libs)
│   │       │
│   │       ├── Audio/               # Sound system abstraction
│   │       ├── Framework/           # Entity/Component model, application loop, world structure
│   │       └── Network/             # Cross-platform TCP/UDP socket abstractions
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
Loads and saves clean `.ini` / `.cfg` files with human-readable formatting and option hints. If `Settings.ini` does not exist (or was deleted by a user), it auto-populates directly from `D_ConfigDefaults.h` (`ConfigDefaults::Settings`):

```cpp
Config::Load("Settings.ini");

// Section, Key, FallbackValue (if key is missing, uses default with zero crashes!)
String apiMode    = Config::Get("Graphics", "API",         "Vulkan");
BOOL   vsync      = Config::Get("Graphics", "VSync",       false);
String windowMode = Config::Get("Window",   "Mode",        "Windowed");
INT    width      = Config::Get("Window",   "Width",       1280);
INT    height     = Config::Get("Window",   "Height",      720);
FLOAT  mouseSens  = Config::Get("Input",    "Sensitivity", 1.0f);

// Update a setting in memory and save to disk:
Config::Set("Graphics", "VSync", true);
Config::Save();
```

Produces clean, Notepad-friendly `Settings.ini` with inline option comments:
```ini
[Graphics]
API = Vulkan                 ; Options: Vulkan / DirectX11
VSync = false                ; Options: true / false

[Window]
Mode = Windowed              ; Options: Windowed / Borderless / Fullscreen
Width = 1280
Height = 720

[Input]
Sensitivity = 1.0000
```

---

### 2. Thread Manager (`M_Threads.h` — `Threads::` / `M_Threads::`)
Cross-platform worker thread pool powered by C++23 standard threads. Allows manual offloading of heavy background work (texture streaming, file writes) or multi-core parallel for-loops (e.g. updating 2,500 missiles or entity physics batches) without stalling the main loop:

```cpp
// 1. Fire-and-forget background task:
Threads::Run([]() {
    // Heavy background decoding or texture decompression
});

// 2. High-performance multi-threaded Parallel_For (splits range across CPU cores):
Threads::Parallel_For(0, 2500, [](size_t i) {
    // Update projectile / missile[i] physics in parallel
});
```

---

### 3. Crash Manager (`M_Crash.h` — `Crash::` / `M_Crash::`)
Catches hardware crashes (Null Pointer, Divide-by-Zero, Abort), unhandled exceptions, and manual safety checks, writing a dated crash log (`ClientCrash_DD.MM.YY_HH.MM.log`) with the exact file name, line number, and function:

```cpp
Crash::Init("Logs/ClientCrash.log");

// Safety check (if false, halts and logs exact File, Line, and Function):
Crash::Check(playerPtr != nullptr, "Player pointer was NULL during spawn!");

// Manual fatal crash trigger:
Crash::Trigger("Vulkan GPU Device Lost!");
```

---

### 4. File Manager (`M_File.h` — `File::` / `M_File::`)
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

### 5. Log Manager (`M_Log.h` — `LOG(...)` & `PRINT(...)`)
Thread-safe console logger with strict `LogLevel` filtering (`Core`, `Developer`, `User`):

```cpp
PRINT("Quick print (always visible)!");
LOG(Info,  LogLevel::Developer, "Player spawned at {}", playerPos);
LOG(Warn,  LogLevel::Developer, "High ping detected: {}ms", ping);
LOG(Error, LogLevel::Core,      "Failed to load asset!");
```

---

### 6. Input System (`I_Keyboard.h`, `I_Mouse.h`, `M_Input.h`)
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

### 7. Window Manager (`M_Window.h` — `Window::` / `M_Window::`)
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

### 8. Complete Application Loop Example (`Game/Client/Client.cpp`)
Instead of initializing and updating every Manager one by one, **`Core::Start_Process`**, **`Core::Tick`**, and **`Core::End_Process`** in [`Core.h`](Engine/Source/Runtime/Core.h) run all Engine systems automatically:

```cpp
#include <Core.h>

int main() {
    // 1. Starts Crash, Config, SDL3, Time, Threads, Window & RHI (Vulkan default) in 1 line!
    Core::Start_Process("Client", ProcessType::Window, "Aurora Client");

    // 2. Main Application Loop (Time + Input + Window events polled automatically)
    DOUBLE lastLogTime = 0.0;
    while (Core::Tick()) {   // Returns false on window [X] / Alt+F4
        if (Window::Is_Minimized()) { Time::Sleep(10); continue; }

        // F10 = toggle VSync ON/OFF, F11 = toggle Fullscreen
        if (Keyboard::Get_Key(Key::F10, Input::Pressed)) RHI_Manager::Set_VSync(!RHI_Manager::Get_VSync());
        if (Keyboard::Get_Key(Key::F11, Input::Pressed)) Window::Set_Mode(Window::Is_Fullscreen() ? Window::Windowed : Window::Fullscreen);

        // F8 = Hot-swap graphics backend at runtime (PoE 2 style, no game restart needed!)
        if (Keyboard::Get_Key(Key::F8, Input::Pressed)) {
            RHI_Manager::Swap_API(RHI_Manager::Get_ActiveAPI() == API_Type::Vulkan ? API_Type::DirectX11 : API_Type::Vulkan);
        }

        if (Time::Check_Cooldown(lastLogTime, 1.0)) {
            Window::Set_Title(StringFormat("Aurora Client | FPS: {} | VSync: {} | Backend: {}",
                Time::Get_FPS(),
                RHI_Manager::Get_VSync() ? "ON" : "OFF",
                RHI_Manager::Get_APIName(RHI_Manager::Get_ActiveAPI())));
        }

        // Render Frame with active backend (Vulkan / DirectX)
        if (RHI_Manager::Begin_Frame()) {
            RHI_Manager::End_Frame();
        }
    }

    // 3. Shuts down RHI, Window, Threads, saves Config and quits SDL cleanly
    Core::End_Process();
    return 0;
}
```

| Function | What it does |
| :--- | :--- |
| `Core::Start_Process(app, type)` | Initializes Crash, Config, Time, SDL3, Threads pool. If `ProcessType::Window`, creates Window & boots RHI |
| `Core::Tick()` | `Time::Update()` + `Input::Update()` (+ window events). Returns `false` when window closes |
| `Core::End_Process()` | `RHI_Manager::Shutdown()`, `Window::Destroy()`, `Threads::Shutdown()`, `Config::Save()`, `SDL_Quit()` |
| `RHI_Manager::Swap_API(newAPI)` | Hot-swaps graphics backends (`Vulkan` $\leftrightarrow$ `DirectX11`) on the live window |
| `Threads::Run(task)` / `Parallel_For` | Multi-core thread pool for background offloading & parallel loops |

---

## Performance Notes

An empty `while (Core::Tick())` loop runs at **~12,000,000 iterations/sec (~83 ns) in `Debug`** with no window open (~1,100,000 with a window, since SDL then asks the OS for window messages every frame). Most of the no-window cost is the `Clock::now()` hardware timer read.

* Measure performance in **`Development`** or **`Release`** — `Debug` disables inlining and adds runtime checks.
* Above ~1,000 FPS, compare **frame time (ms)**, not FPS. A jump from 12M to 10M FPS is only ~17 ns and is usually just noise (code alignment, CPU boost, core scheduling).
* Input arrays (`Keyboard` / `Mouse`) are only copied on frames where a button actually changed (`s_IsDirty` flag).
