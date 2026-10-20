# Aurora Engine & Game Roadmap (TODO)

## Early Stage (Core)
* ~~CMake + Static Library~~ (Cross-platform Windows/Linux, no DLL errors)
* ~~Custom Variables (`U_Variables`, `U_Colors`)~~ (`BYTE, DWORD, INT, Vector3D, COLOR`)
* ~~File Manager (`M_File` / `File::`)~~ (`Open, Write, Close, Clean, Delete`)
* ~~Log Manager (`M_Log` / `Log::`)~~ (`LOG(Type, Level, ...)`, `PRINT(...)`)
* ~~Math Module (`U_Math` / `Math::`)~~ (`PI`, `Clamp`, `Lerp`, `Math::Extended::`)
* ~~Time Manager (`M_Time` / `Time::`)~~ (`DeltaTime`, FPS, Timestamps, Cooldowns)
* ~~Crash Manager (`M_Crash` / `Crash::`)~~ (Catch hardware crashes, stacktrace file/line, dated `Crash.log`)
* ~~Config Manager (`M_Config` / `Config::` - Save/load Client & Server settings)~~
* ~~Input System (`I_Keyboard`, `I_Mouse`, `M_Input` - SDL3 Keyboard, Mouse & Remappable Keybinds)~~
* ~~Core Lifecycle (`Core::BeginPlay`, `Core::Tick`, `Core::Stop`, `Core::EndPlay`)~~
* ~~Core Loop Performance Pass~~ (Dirty-flag input arrays, skip polling without a window, leaner `Time::Update` -> ~12M FPS empty loop in Debug)
* ~~Threading Decision~~ (Single-threaded by design; servers scale by process. `M_Thread` job system only if ever truly needed)

## Later Stage (Engine & World)
* ~~Window Manager (`M_Window` / `Window::` - Cross-platform SDL3 window, modes, focus/minimize, mouse lock)~~
* ~~Render Handler (`RHI` - Vulkan 1.3 Core: `R_Device`, Dynamic Rendering `R_Swapchain`)~~
* Graphics Pipeline & Shaders (`R_Pipeline`, `R_Shader` - SPIR-V Shaders & Mesh Drawing) <- **NEXT**
* Asset Manager (`Asset::Get_Mesh("Name")` - Load all / per-region behind a loading screen, BC-compressed `.dds` textures)
* Culling & Spatial Grid (Frustum/occlusion culling + distance grid to boost FPS)
* Physics Module (Lightweight; separate Ground Box & Fly Hitbox like 2000s flying MMOs)
* GUI Module (Inventory, HUD, Buttons)
* Audio System (Sound etc.)

## End Stage (Multiplayer & Polish)
* Network Module (Client/Server TCP packets, non-blocking sockets polled in `Core::Tick`)
* Sharp Anti-Aliasing (Hardware MSAA + Anisotropic Filtering - crisp distance, no blurry TAA)

## Optional (Only If Needed)
* Job System (`M_Thread` - `Dispatch` / `ParallelFor`) for faster loading screens or huge entity counts
