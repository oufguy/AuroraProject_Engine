# AuroraProject Engine — Architecture & Directory Guide

A lightweight, cross-platform C++23 application & client-server engine built from scratch for high performance. Designed with a strict, developer-friendly API, zero bloat, and native **Windows** & **Linux** support.

---

## High-Level Dependency Rule

* **`Engine/`** is completely application-agnostic: It has zero knowledge of specific `Game`, `Client`, or `Server` rules.
* **`Game/`** modules (`Client`, `Server`, `Shared`) include `<Aurora.h>` and depend on `EngineSource`, never the other way around.
* **Headless Servers** only use core/network engine modules (`Utilities`, `Managers`, `Network`) and never require graphics or audio hardware.

---

## Design Principles

* **Simple first.** Core gameplay logic runs predictably on the **main thread** (like classic MMO architectures). Heavy background work (asset decoding, texture loading, or updating thousands of missiles/projectiles) is delegated to **`M_Threads`** via simple manual tasks (`Threads::Run`) or multi-core batch loops (`Threads::Parallel_For`) without stalling the main loop.
  * Servers scale by running **more processes** (one per zone), not complex entangled threads.
  * Slow *waiting* work (database queries) lives in its own server process. Assets are loaded up front / per-region behind a loading screen.
* **Zero-cost when unused.** E.g. `Input::Update()` returns immediately when no window is open, so headless servers never pay for keyboard/mouse polling. Headless servers completely skip RHI graphics backend initialization.
* **Cross-platform only.** Zero Windows OS headers (`<windows.h>` strictly banned). Graphics backends support both modern Vulkan 1.3/1.4 Core and pure dynamic DirectX 11 loaded via SDL3 dynamic symbol loading without link-time OS dependencies.
* **Naming convention.** One class per header, prefix = folder: `B_` Base, `F_` Framework, `U_` Utilities, `I_` Input, `M_` Managers, `E_` Entities, `R_` RHI backends, `S_` Shaders.

---

## Directory Breakdown

> See [`Engine/README.md`](Engine/README.md) for module overview.

```text
AuroraProject/
├── CMakeLists.txt                   # Root CMake build script
├── CMakePresets.json                # Build configurations (Debug, Development, Release)
│
├── Engine/
│   ├── Aurora.h                     # Master umbrella header (#include <Aurora.h>)
│   ├── README.md                    # Engine modular architecture overview
│   ├── Core/                        # Foundation: 2D, Window, Input, Threads, Loaders, RHI
│   │   ├── Base/                    # B_Application (Base for apps / tools)
│   │   ├── Input/                   # I_Keyboard, I_Mouse
│   │   ├── Managers/                # M_Asset, M_Crash, M_File, M_Log, M_Time, M_Window...
│   │   ├── Utilities/               # U_Math, U_Colors, U_Variables
│   │   └── Internal/RHI/            # R_Vulkan, R_DirectX
│   ├── World/                       # 3D World: Entities, Camera, 3D Meshes, Physics
│   │   ├── Base/                    # B_Game (Base for 3D games)
│   │   ├── Framework/               # F_PlayerController
│   │   ├── Entities/                # E_Camera, E_DynamicMesh, E_StaticMesh, E_Entity
│   │   └── Managers/                # M_World (O(1) Spatial Hash Grid & Frustum Culling)
│   └── Online/                      # Multiplayer: UDP/TCP, Packets, Replication
│       ├── Base/                    # Network base classes
│       └── Managers/                # M_Network
│
└── Game/
    ├── Client/                      # Client executable (AWO_Client)
    ├── Server/FieldServer/          # Headless server executable (FieldServer)
    └── Content/                     # Game assets (.texaxe / .meshae / .mapae)
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

### 8. Complete Application Example (`Game/Client/Client.cpp`)
Games inherit from **`B_Game`** and tools inherit from **`B_Application`**. All low-level engine initialization, window pumping, RHI begin/end frame dispatch, and camera controls are handled automatically:

```cpp
#include <Aurora.h>

class Client : public B_Game {
public:
    void BeginPlay() override {
        // 1. Camera is already spawned & attached to PlayerController by B_Game!
        m_Camera = M_World::Get_Camera();

        // 2. Setup Rotating Textured Cube via M_Asset
        m_Cube = M_World::Spawn_Entity<E_DynamicMesh>();
        m_Cube->Mesh          = M_Asset::Create_Cube(COLOR_CYAN);
        m_Cube->Set_Texture(M_Asset::Create_Checkerboard(64, 64, 8));
        m_Cube->RotationSpeed = { 45.0f, 90.0f, 0.0f };

        // 3. Setup RGB Wave Triangle via M_Asset
        m_Triangle = M_World::Spawn_Entity<E_StaticMesh>();
        m_Triangle->Mesh = M_Asset::Create_Triangle(COLOR_RED, COLOR_GREEN, COLOR_BLUE);
    }

    void Tick(FLOAT deltaTime) override {
        // Flycam movement & mouse look handled by PlayerController
        PlayerController.Update(deltaTime);
    }

    void EndPlay() override {
        PRINT("Client game session closed gracefully.");
    }

private:
    E_Camera      *m_Camera   = nullptr;
    E_DynamicMesh *m_Cube     = nullptr;
    E_StaticMesh  *m_Triangle = nullptr;
};

// Generates int main() and runs the engine automatically
AURORA_GAME(Client)
```

| Function | What it does |
| :--- | :--- |
| `B_Game::BeginPlay()` | Called once when the Engine and 3D World are booted |
| `B_Game::Tick(dt)` | Called every frame with the elapsed delta time |
| `B_Game::EndPlay()` | Called right before the engine cleanly terminates |
| `AURORA_GAME(GameClass)` | Generates `int main()` and runs the game instance |
| `AURORA_APP(AppClass)` | Generates `int main()` and runs a 2D or headless application |
| `M_World::Spawn_Entity<T>()` | Type-safe entity allocation returning typed pointers without raw IDs |
| `M_Asset` | Unified resource manager for meshes, textures, configs, and files |
| `F_PlayerController` | Flycam movement, mouse look, speed multiplier, and camera orientation reset |

---

## Performance Notes

An empty `while (Core::Tick())` loop runs at **~12,000,000 iterations/sec (~83 ns) in `Debug`** with no window open (~1,100,000 with a window, since SDL then asks the OS for window messages every frame). Most of the no-window cost is the `Clock::now()` hardware timer read.

* Measure performance in **`Development`** or **`Release`** — `Debug` disables inlining and adds runtime checks.
* Above ~1,000 FPS, compare **frame time (ms)**, not FPS. A jump from 12M to 10M FPS is only ~17 ns and is usually just noise (code alignment, CPU boost, core scheduling).
* Input arrays (`Keyboard` / `Mouse`) are only copied on frames where a button actually changed (`s_IsDirty` flag).
