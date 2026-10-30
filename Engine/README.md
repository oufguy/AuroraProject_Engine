# Aurora Engine (v0.1a)

A lightweight, high-performance, cross-platform C++23 game engine designed with an Unreal Engine-style developer experience without the bloat of an external editor.

---

## Architecture Pillars

```text
Engine/
├── Aurora.h                                  # Master umbrella header (#include <Aurora.h>)
│
├── Core/                                     # Platform, OS, Windowing & RHI
│   ├── Base/
│   │   └── B_Application.h                   # Base for 2D / Tool / Headless apps (AURORA_APP)
│   ├── Input/
│   │   ├── I_Keyboard.h                      # Hardware keyboard polling
│   │   └── I_Mouse.h                         # Hardware mouse & cursor polling
│   ├── Internal/
│   │   └── RHI/                              # Dual Vulkan & DirectX 11 Backends (3,000+ FPS)
│   ├── Managers/
│   │   ├── Loaders/                          # M_Config, M_Mesh, M_Texture
│   │   ├── M_Asset.h                         # Unified Asset Manager
│   │   ├── M_Crash.h, M_File.h, M_Input.h, M_Log.h, M_Threads.h, M_Time.h, M_Window.h
│   ├── Utilities/                            # U_Math, U_Colors, U_Variables
│   ├── Core.h, Core.cpp, pch.h
│
├── World/                                    # 3D Scene, Entities & Simulation
│   ├── Base/
│   │   └── B_Game.h                          # Base for 3D Games (AURORA_GAME)
│   ├── Framework/
│   │   └── F_PlayerController.h              # Flycam & Pawn Controller
│   ├── Entities/
│   │   ├── E_Camera.h, E_DynamicMesh.h, E_Entity.h, E_StaticMesh.h
│   ├── Managers/
│   │   └── M_World.h                         # Central 3D World Manager (O(1) Spatial Grid & Culling)
│   └── World.h
│
└── Online/                                   # Multiplayer & Replication
    ├── Base/                                 # Dedicated network base classes
    ├── Managers/
    │   └── M_Network.h                       # Socket & Replication Manager
    └── Online.h
```

---

## Strict Naming Conventions

Every folder and prefix has a strict 1-to-1 relationship:

| Prefix | Folder | Purpose | Examples |
| :--- | :--- | :--- | :--- |
| **`B_`** | **`Base/`** | Classes applications or games inherit from | `B_Application`, `B_Game` |
| **`F_`** | **`Framework/`** | Gameplay framework components | `F_PlayerController` |
| **`M_`** | **`Managers/`** | Central engine singletons & managers | `M_World`, `M_Asset`, `M_Window`, `M_Network` |
| **`E_`** | **`Entities/`** | 3D scene objects | `E_Camera`, `E_DynamicMesh`, `E_StaticMesh` |
| **`I_`** | **`Input/`** | Hardware input polling | `I_Keyboard`, `I_Mouse` |
| **`U_`** | **`Utilities/`** | Math, types, and colors | `U_Math`, `U_Colors`, `U_Variables` |
| **`R_`** | **`RHI/`** | Hardware rendering backends | `R_Vulkan`, `R_DirectX` |

---

## Quick Start Examples

### 1. 3D Game (`B_Game`)

```cpp
#include <Aurora.h>

class Client : public B_Game {
public:
    void BeginPlay() override {
        // 1. Camera is already spawned & attached to PlayerController by B_Game!
        m_Camera = M_World::Get_Camera();

        // 2. Spawn Meshes via M_Asset
        m_Cube = M_World::Spawn_Entity<E_DynamicMesh>();
        m_Cube->Mesh = M_Asset::Create_Cube(COLOR_CYAN);
        m_Cube->Set_Texture(M_Asset::Create_Checkerboard(64, 64, 8));
        m_Cube->RotationSpeed = { 45.0f, 90.0f, 0.0f };
    }

    void Tick(FLOAT deltaTime) override {
        // Flycam input & looking updated automatically!
        PlayerController.Update(deltaTime);
    }

    void EndPlay() override {
        PRINT("Game session ended.");
    }

private:
    E_Camera      *m_Camera = nullptr;
    E_DynamicMesh *m_Cube   = nullptr;
};

// Generates int main() and runs the engine automatically
AURORA_GAME(Client)
```

---

### 2. 2D Tool or Headless App (`B_Application`)

```cpp
#include <Aurora.h>

class ServerApp : public B_Application {
public:
    void OnInit() override {
        PRINT("Server Initialized.");
    }

    void OnUpdate(FLOAT deltaTime) override {
        // Network server tick
    }

    void OnShutdown() override {
        PRINT("Server Shutdown.");
    }
};

AURORA_APP(ServerApp)
```
