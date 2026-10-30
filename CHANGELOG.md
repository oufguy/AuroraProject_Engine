# Changelog

All notable changes to the Aurora Engine are documented in this file.

---

## [v0.1a] - 2026-10-30

### Added
* **BaseClasses Architecture (`Engine/Core/Base` & `Engine/World/Base`):**
  * Added `B_Application` for 2D applications, tools, and headless processes.
  * Added `B_Game` for 3D games with automated lifecycle management.
  * Added `AURORA_APP` and `AURORA_GAME` macros to generate entry points and hide low-level engine bootstrap.
* **Gameplay Framework (`Engine/World/Framework`):**
  * Added `F_PlayerController` providing smooth flycam motion, shift speed boost, right-click mouse look, and reset hotkeys.
  * Added `Attach()` to link the controller to any camera or viewpoint reference point without multiplayer overhead.
* **Asset Manager (`M_Asset`):**
  * Added unified `M_Asset` manager for loading meshes (`Create_Cube`, `Create_Triangle`, `Create_Quad`, `Load_Mesh`), textures (`Create_Checkerboard`, `Load_Texture`), configs, and files.
* **Type-Safe Entity Spawning:**
  * Added templated `M_World::Spawn_Entity<TEntity>()` returning typed pointers without requiring manual entity ID management.
* **Crash-Proof Camera System:**
  * Added `M_World::Get_Camera()` with built-in fallback camera to prevent null-pointer dereferences.
  * `B_Game` now automatically boots a default camera and binds `F_PlayerController` on startup.

### Changed
* **Folder & Prefix Alignment:**
  * Enforced strict 1-to-1 relationship between prefixes and folder names:
    * `B_` &rarr; `Base/` (Base classes)
    * `F_` &rarr; `Framework/` (Framework classes)
    * `M_` &rarr; `Managers/` (Singletons & Managers)
    * `E_` &rarr; `Entities/` (Scene objects)
    * `I_` &rarr; `Input/` (Hardware polling)
    * `U_` &rarr; `Utilities/` (Math, colors, variables)
    * `R_` &rarr; `Internal/RHI/` (Render backends)
    * `S_` &rarr; `Internal/RHI/Shaders/` (Shader bytecode)
  * Relocated `M_World.h` to `Engine/World/Managers/M_World.h`.
  * Relocated `M_Network.h` to `Engine/Online/Managers/M_Network.h`.
* **Telemetry Display:**
  * Window title remains clean (`"Aurora Client"`). Diagnostic telemetry (FPS, VSync, Active API, visible/culled count) is now logged to the developer console at 1-second intervals.
* **Documentation:**
  * Updated `README.md` and `Engine/README.md` with new architectural diagrams, prefix tables, and clean Unreal-style game examples.

### Removed
* Deleted deprecated `AGameInstance.h` and legacy `Possess()` multiplayer character bindings.

---

## [v0.1] - 2026-10-30

### Initial Release
* Modular 3-pillar engine architecture (`Core`, `World`, `Online`).
* Dual RHI with live runtime hot-swapping between modern Vulkan and DirectX 11 (3,000+ FPS).
* Cross-platform windowing, timing, multi-threading, crash handling, and configuration management.
* 3D scene graph with 2D spatial hash grid, 6-plane frustum culling, and alpha blending sorting.
