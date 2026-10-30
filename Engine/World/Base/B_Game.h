#pragma once

#include <B_Application.h>
#include <World.h>
#include <F_PlayerController.h>

// ============================================================================
// B_Game (3D Game Instance Base Class)
// ============================================================================
// Extends B_Application specifically for 3D games with scene graph, entities,
// frustum culling, and camera control.
//
// Usage:
//   class MyGame : public B_Game {
//   public:
//       void BeginPlay() override { ... }
//       void Tick(FLOAT dt) override { ... }
//       void EndPlay() override { ... }
//   };
//   AURORA_GAME(MyGame)
// ============================================================================
class B_Game : public B_Application {
public:
    virtual ~B_Game() = default;

    // Called once when the Engine and 3D World are initialized
    virtual void BeginPlay() {}

    // Called once per frame for game logic and entity updates
    virtual void Tick(FLOAT deltaTime) { (void)deltaTime; }

    // Called once when the game is shutting down
    virtual void EndPlay() {}

    // Built-in PlayerController ready for camera possession and movement
    F_PlayerController PlayerController;

    // ------------------------------------------------------------------------
    // Engine Launch Runner for B_Game
    // ------------------------------------------------------------------------
    template <typename TGame>
    static INT RunGame() {
        static_assert(std::is_base_of_v<B_Game, TGame>, "Class must inherit from B_Game!");

        TGame game;

        // 1. Load game configuration
        game.Settings.Load(game.Get_SettingsPath(), game.Get_DefaultSettings());

        // 2. Setup Window and RHI preferences
        M_Window::Set_Title("Aurora Client");
        M_Window::Set_Size(game.Settings.Get("Width"), game.Settings.Get("Height"));
        M_Window::Set_Mode(M_Window::ModeFromString(game.Settings.Get("Mode")));

        RHI_Manager::Swap_API(RHI_Manager::Parse_API(game.Settings.Get("API")));
        RHI_Manager::Set_VSync(game.Settings.Get("VSync"));

        // 3. Boot Core Lifecycle Subsystems
        if (!Core::BeginPlay("Client", ProcessType::Window)) {
            return 1;
        }

        // 4. Initialize 3D World and spawn default Camera
        M_World::BeginPlay("DefaultWorld");
        E_Camera *cam = M_World::Spawn_Entity<E_Camera>();
        cam->Reset();
        cam->Set_Active();
        game.PlayerController.Attach(cam);

        // 5. Invoke Game BeginPlay
        game.BeginPlay();

        PRINT("Game Initialized! Active Backend: {} ({}) | World: [{}] ({} Entities)",
            RHI_Manager::Get_APIName(RHI_Manager::Get_ActiveAPI()),
            RHI_Manager::Get_GPUName(),
            M_World::Get_Name(),
            M_World::Get_EntityCount());

        // 6. Main Engine Loop
        DOUBLE lastFpsLog = 0.0;
        while (Core::Tick()) {
            if (M_Window::Is_Minimized()) continue;

            FLOAT dt = M_Time::Get_DeltaTime();

            // Run user gameplay logic
            game.Tick(dt);

            // Step World transforms, physics, and animations
            M_World::Tick(dt);

            // Handle window resizing
            if (M_Window::Was_Resized()) {
                game.Settings.Set("Width",  M_Window::Get_Width());
                game.Settings.Set("Height", M_Window::Get_Height());

                Vector2D px = M_Window::Get_PixelSize();
                PRINT("Window resized -> {}x{} pixels", px.x, px.y);
            }

            // Engine hotkeys: F9 (API swap), F10 (VSync), F11 (Fullscreen)
            if (I_Keyboard::Get_Key(Key::F10, M_Input::Pressed)) {
                BOOL vsync = game.Settings.Set("VSync", !RHI_Manager::Get_VSync());
                RHI_Manager::Set_VSync(vsync);
            }
            if (I_Keyboard::Get_Key(Key::F11, M_Input::Pressed)) {
                auto mode = M_Window::Is_Fullscreen() ? M_Window::Windowed : M_Window::Fullscreen;
                M_Window::Set_Mode(mode);
                game.Settings.Set("Mode", M_Window::ModeToString(mode));
            }
            if (I_Keyboard::Get_Key(Key::F9, M_Input::Pressed)) {
                API_Type target = (RHI_Manager::Get_ActiveAPI() == API_Type::Vulkan) ? API_Type::DirectX11 : API_Type::Vulkan;
                if (RHI_Manager::Swap_API(target)) {
                    game.Settings.Set("API", (target == API_Type::Vulkan) ? "Vulkan" : "DirectX11");
                }
            }

            // Periodic telemetry log (clean, does NOT modify window title)
            if (M_Time::Check_Cooldown(lastFpsLog, 1.0)) {
                LOG(Info, LogLevel::Developer, "FPS: {} | VSync: {} | Backend: {} | Drawn: {} (Culled: {})",
                    M_Time::Get_FPS(),
                    RHI_Manager::Get_VSync() ? "ON" : "OFF",
                    RHI_Manager::Get_APIName(RHI_Manager::Get_ActiveAPI()),
                    M_World::Get_VisibleEntityCount(),
                    M_World::Get_CulledEntityCount());
            }

            // Render Frame with active RHI backend (Vulkan or DirectX 11)
            if (RHI_Manager::Begin_Frame()) {
                M_World::Render();
                RHI_Manager::End_Frame();
            }
        }

        // 7. Invoke Game EndPlay and clean up
        game.EndPlay();
        game.Settings.Save(game.Get_SettingsPath());
        M_World::Shutdown();
        Core::EndPlay();

        return 0;
    }
};

// Main Game Entry Macro: Hides int main() and runs B_Game
#define AURORA_GAME(GameClass) \
    int main(int argc, char** argv) { \
        (void)argc; (void)argv; \
        return B_Game::RunGame<GameClass>(); \
    }
