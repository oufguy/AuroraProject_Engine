#pragma once

#include <Core.h>

// ============================================================================
// B_Application (Core Application Base Class)
// ============================================================================
// Base class for core applications (2D, tools, or headless processes).
// Automatically manages the window, RHI, configuration settings, and main loop.
//
// Usage:
//   class MyApp : public B_Application {
//   public:
//       void OnInit() override {}
//       void OnUpdate(FLOAT deltaTime) override {}
//       void OnShutdown() override {}
//   };
//   AURORA_APP(MyApp)
// ============================================================================
class B_Application {
public:
    virtual ~B_Application() = default;

    // Called once when the application and engine subsystems are initialized
    virtual void OnInit() {}

    // Called once per frame with the elapsed delta time in seconds
    virtual void OnUpdate(FLOAT deltaTime) { (void)deltaTime; }

    // Called once when the application is shutting down
    virtual void OnShutdown() {}

    // Configuration file path (can be overridden)
    virtual FilePath Get_SettingsPath() const { return "Settings.ini"; }

    // Default configuration content created on first run
    virtual String Get_DefaultSettings() const {
        return "# Graphics\n"
               "API = Vulkan                ; Options: Vulkan / DirectX11\n"
               "VSync = false               ; Options: true / false\n"
               "\n"
               "# Window\n"
               "Mode = Windowed             ; Options: Windowed / Borderless / Fullscreen\n"
               "Width = 1280\n"
               "Height = 720\n"
               "\n"
               "# Input\n"
               "Sensitivity = 1.0\n";
    }

    // In-memory configuration store accessible to the application
    M_Config Settings;

    // ------------------------------------------------------------------------
    // Engine Launch Runner for B_Application
    // ------------------------------------------------------------------------
    template <typename TApp>
    static INT RunApp() {
        static_assert(std::is_base_of_v<B_Application, TApp>, "Class must inherit from B_Application!");

        TApp app;

        // 1. Load application configuration
        app.Settings.Load(app.Get_SettingsPath(), app.Get_DefaultSettings());

        // 2. Setup Window and RHI preferences
        M_Window::Set_Title("Aurora Application");
        M_Window::Set_Size(app.Settings.Get("Width"), app.Settings.Get("Height"));
        M_Window::Set_Mode(M_Window::ModeFromString(app.Settings.Get("Mode")));

        RHI_Manager::Swap_API(RHI_Manager::Parse_API(app.Settings.Get("API")));
        RHI_Manager::Set_VSync(app.Settings.Get("VSync"));

        // 3. Boot Core Lifecycle Subsystems
        if (!Core::BeginPlay("App", ProcessType::Window)) {
            return 1;
        }

        app.OnInit();

        // 4. Main Event Loop
        DOUBLE lastFpsLog = 0.0;
        while (Core::Tick()) {
            if (M_Window::Is_Minimized()) continue;

            FLOAT dt = M_Time::Get_DeltaTime();
            app.OnUpdate(dt);

            // Handle window resizing
            if (M_Window::Was_Resized()) {
                app.Settings.Set("Width",  M_Window::Get_Width());
                app.Settings.Set("Height", M_Window::Get_Height());
            }

            // Engine hotkeys: F9 (API swap), F10 (VSync), F11 (Fullscreen)
            if (I_Keyboard::Get_Key(Key::F10, M_Input::Pressed)) {
                BOOL vsync = app.Settings.Set("VSync", !RHI_Manager::Get_VSync());
                RHI_Manager::Set_VSync(vsync);
            }
            if (I_Keyboard::Get_Key(Key::F11, M_Input::Pressed)) {
                auto mode = M_Window::Is_Fullscreen() ? M_Window::Windowed : M_Window::Fullscreen;
                M_Window::Set_Mode(mode);
                app.Settings.Set("Mode", M_Window::ModeToString(mode));
            }
            if (I_Keyboard::Get_Key(Key::F9, M_Input::Pressed)) {
                API_Type target = (RHI_Manager::Get_ActiveAPI() == API_Type::Vulkan) ? API_Type::DirectX11 : API_Type::Vulkan;
                if (RHI_Manager::Swap_API(target)) {
                    app.Settings.Set("API", (target == API_Type::Vulkan) ? "Vulkan" : "DirectX11");
                }
            }

            // Clean telemetry log (console only, does not alter window title)
            if (M_Time::Check_Cooldown(lastFpsLog, 1.0)) {
                LOG(Info, LogLevel::Developer, "FPS: {} | VSync: {} | Backend: {}",
                    M_Time::Get_FPS(),
                    RHI_Manager::Get_VSync() ? "ON" : "OFF",
                    RHI_Manager::Get_APIName(RHI_Manager::Get_ActiveAPI()));
            }

            // Render RHI frame
            if (RHI_Manager::Begin_Frame()) {
                RHI_Manager::End_Frame();
            }
        }

        app.OnShutdown();
        app.Settings.Save(app.Get_SettingsPath());
        Core::EndPlay();
        return 0;
    }
};

// Application entry macro for B_Application
#define AURORA_APP(AppClass) \
    int main(int argc, char** argv) { \
        (void)argc; (void)argv; \
        return B_Application::RunApp<AppClass>(); \
    }
