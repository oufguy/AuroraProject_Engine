# Aurora Engine & Game Roadmap (TODO)

## Early Stage (Core)
* ~~CMake + Static Library~~ (Cross-platform Windows/Linux, no DLL errors)
* ~~Custom Variables (`U_Variables`, `U_Colors`)~~ (`BYTE, DWORD, INT, Vector3D, COLOR`)
* ~~File Manager (`U_FileManager`)~~ (`Open, Write, Close, Clean, Delete`)
* ~~Log Module (`U_Log`)~~ (`LOG(Info/Warn/Error/Print)`)
* ~~Math Module (`U_Math`)~~ (`PI`, `Clamp`, `Lerp`, `Camera::View`)
* ~~Extend Math Module~~ (`Math::Extended::`)
* ~~Time Module (`U_TimeManager` - `DeltaTime`, FPS, Timers)~~
* ~~Crash Handler (`U_CrashHandler` - Catch crashes, write `Crash.log`)~~
* Config Manager (`U_Config` - Save/load Client & Server settings)
* Input Module (`I_Input` - Keyboard + Mouse)

## Later Stage (Engine & World)
* Window Manager (`R_Window` - Create OS window)
* Render Handler (`RHI` - Vulkan backend)
* Culling & Spatial Grid (Frustum/occlusion culling + distance grid to boost FPS)
* Physics Module (Lightweight; separate Ground Box & Fly Hitbox like 2000s flying MMOs)
* GUI Module (Inventory, HUD, Buttons)
* Audio System (Sound etc.)

## End Stage (Multiplayer & Polish)
* Network Module (Client/Server TCP packets)
* Sharp Anti-Aliasing (Hardware MSAA + Anisotropic Filtering — crisp distance, no blurry TAA)
