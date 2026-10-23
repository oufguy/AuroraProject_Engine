#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>
#include <M_Config.h>

// ============================================================================
// Cross-Platform Window Manager (SDL3 - Windows, Linux X11 & Wayland)
// ============================================================================
// Quick Usage Example:
//   Core::Start_Process("Client", ProcessType::Window, "Aurora Client");
//
//   while (Core::Tick()) {                     // Alt+F4 / [X] -> Core::Tick() returns false
//       if (Window::Is_Minimized()) { Time::Sleep(10); continue; } // Don't waste CPU/GPU when minimized
//       if (Window::Was_Resized())  { /* Recreate swapchain with Window::Get_PixelSize() */ }
//
//       if (Keyboard::Get_Key(Key::F11, Input::Pressed)) {
//           Window::Set_Mode(Window::Is_Fullscreen() ? Window::Windowed : Window::Borderless);
//       }
//   }
//
// Generated Settings.ini:
//   [Window]
//   Height = 720
//   Mode = Windowed        ; Options: Windowed / Borderless / Fullscreen
//   Width = 1280
// ============================================================================
class WindowManager {
    friend class InputManager; // Input::Update() polls SDL events and forwards window events here

public:
    // Display mode for Create(...) / Set_Mode(...)
    enum WindowMode : BYTE {
        Windowed   = 0, // Normal window with title bar (resizable)
        Borderless = 1, // Fullscreen at desktop resolution (instant Alt+Tab, recommended!)
        Fullscreen = 2  // Exclusive fullscreen (changes monitor resolution to Width x Height)
    };

private:
    static inline SDL_Window *s_Window      = nullptr;
    static inline BOOL        s_IsOpen      = false;
    static inline BOOL        s_IsFocused   = false;
    static inline BOOL        s_IsMinimized = false;
    static inline BOOL        s_WasResized  = false;
    static inline WindowMode  s_Mode        = Windowed;

    static inline WindowMode ModeFromString(StringView text) {
        if (text == "Borderless") return Borderless;
        if (text == "Fullscreen") return Fullscreen;
        return Windowed;
    }

    static inline const Char *ModeToString(WindowMode mode) {
        switch (mode) {
            case Borderless: return "Borderless";
            case Fullscreen: return "Fullscreen";
            default:         return "Windowed";
        }
    }

    // Applies Windowed / Borderless / Exclusive Fullscreen to the live SDL window
    static inline void ApplyMode(WindowMode mode, INT width, INT height) {
        s_Mode = mode;

        if (mode == Windowed) {
            SDL_SetWindowFullscreen(s_Window, false);
            SDL_SetWindowSize(s_Window, width, height);
            SDL_SetWindowPosition(s_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            return;
        }

        if (mode == Fullscreen) {
            SDL_DisplayID display = SDL_GetDisplayForWindow(s_Window);
            const SDL_DisplayMode *desktopMode = SDL_GetDesktopDisplayMode(display);
            if (desktopMode) {
                SDL_SetWindowFullscreenMode(s_Window, desktopMode);
            } else {
                SDL_SetWindowFullscreenMode(s_Window, nullptr);
            }
            SDL_SetWindowFullscreen(s_Window, true);
            return;
        } else {
            SDL_SetWindowFullscreenMode(s_Window, nullptr); // nullptr = borderless desktop fullscreen
        }
        SDL_SetWindowFullscreen(s_Window, true);
    }

    // Called by Input::Update() at the start of every frame
    static inline void BeginFrame() {
        s_WasResized = false;
    }

    // Called by Input::Update() for every SDL window event
    static inline void OnEvent(const SDL_Event &ev) {
        switch (ev.type) {
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: s_WasResized  = true;  break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:       s_IsFocused   = true;  break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:         s_IsFocused   = false; break;
            case SDL_EVENT_WINDOW_MINIMIZED:          s_IsMinimized = true;  break;
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_MAXIMIZED:          s_IsMinimized = false; break;
            default: break;
        }
    }

public:
    // ========================================================================
    // Create & Destroy
    // ========================================================================

    // Opens the OS window. Width / Height / Mode are read from Config [Window] (defaults auto-added).
    // Returns FALSE if the OS refused to create a window (no display, e.g. headless Linux server).
    // Example: Window::Create("Aurora Client");
    static inline BOOL Create(StringView title, INT defaultWidth = 1280, INT defaultHeight = 720) {
        if (s_IsOpen) return true;

        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            LOG(Error, LogLevel::Core, "Window::Create failed to init SDL video: {}", SDL_GetError());
            return false;
        }

        INT        width  = Config::Get("Window", "Width",  defaultWidth);
        INT        height = Config::Get("Window", "Height", defaultHeight);
        WindowMode mode   = ModeFromString(Config::Get("Window", "Mode", "Windowed"));

        // VULKAN = native Vulkan surface support, RESIZABLE = drag edges, HIGH_PIXEL_DENSITY = sharp on 4K
        SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

        s_Window = SDL_CreateWindow(String(title).c_str(), width, height, flags);
        if (!s_Window) {
            LOG(Error, LogLevel::Core, "Window::Create failed: {}", SDL_GetError());
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return false;
        }

        s_IsOpen      = true;
        s_IsFocused   = true;
        s_IsMinimized = false;
        ApplyMode(mode, width, height);

        LOG(Info, LogLevel::Core, "Window::Create opened '{}' ({}x{}, {})", title, width, height, ModeToString(s_Mode));
        return true;
    }

    // Closes the window and shuts down SDL video (the OS also does this automatically on exit).
    // Example: Window::Destroy();
    static inline void Destroy() {
        if (!s_IsOpen) return;

        SDL_DestroyWindow(s_Window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        s_Window = nullptr;
        s_IsOpen = false;
    }

    // ========================================================================
    // Set_... (Title, Size, Mode, Mouse Lock)
    // ========================================================================

    // Changes the text in the window title bar.
    // Example: Window::Set_Title(StringFormat("Aurora | FPS: {}", Time::Get_FPS()));
    static inline void Set_Title(StringView title) {
        if (s_IsOpen) SDL_SetWindowTitle(s_Window, String(title).c_str());
    }

    // Resizes the window and immediately saves to Config [Window].
    // Example: Window::Set_Size(1920, 1080);
    static inline void Set_Size(INT width, INT height) {
        Config::Set("Window", "Width",  width);
        Config::Set("Window", "Height", height);
        Config::Save();
        if (s_IsOpen) ApplyMode(s_Mode, width, height);
    }

    // Switches Windowed / Borderless / Fullscreen and immediately saves to Config [Window].
    // Example: Window::Set_Mode(Window::Borderless);
    static inline void Set_Mode(WindowMode mode) {
        if (!s_IsOpen) return;
        ApplyMode(mode, Config::Get("Window", "Width", 1280), Config::Get("Window", "Height", 720));
        Config::Set("Window", "Mode", ModeToString(s_Mode));
        Config::Save();
        s_WasResized = true;
    }

    // Locks & hides the cursor inside the window -> Mouse::Get_Delta() keeps working endlessly.
    // Perfect for rotating the camera while holding Right Mouse Button (MMO style!).
    // Example: Window::Set_MouseLocked(Mouse::Get_Button(MouseButton::MB_Right, Input::Held));
    static inline void Set_MouseLocked(BOOL locked) {
        if (s_IsOpen) SDL_SetWindowRelativeMouseMode(s_Window, locked);
    }

    // ========================================================================
    // Get_... / Is_... / Was_...
    // ========================================================================

    // Returns the window size in screen units (use for UI layout & mouse coordinates).
    // Example: Vector2D size = Window::Get_Size();
    static inline Vector2D Get_Size() {
        INT w = 0, h = 0;
        if (s_IsOpen) SDL_GetWindowSize(s_Window, &w, &h);
        return { static_cast<FLOAT>(w), static_cast<FLOAT>(h) };
    }

    // Returns the window size in real screen PIXELS (use for Vulkan swapchain / render resolution).
    // (On a 150% scaled monitor, a 1280x720 window is 1920x1080 pixels!)
    // Example: Vector2D pixels = Window::Get_PixelSize();
    static inline Vector2D Get_PixelSize() {
        INT w = 0, h = 0;
        if (s_IsOpen) SDL_GetWindowSizeInPixels(s_Window, &w, &h);
        return { static_cast<FLOAT>(w), static_cast<FLOAT>(h) };
    }

    // Returns the current mode (Windowed / Borderless / Fullscreen).
    static inline WindowMode Get_Mode()   { return s_Mode; }

    // Returns the raw SDL_Window* (needed later by the Vulkan RHI to create its surface).
    static inline SDL_Window *Get_Handle() { return s_Window; }

    // TRUE while the window exists.
    static inline BOOL Is_Open()       { return s_IsOpen; }

    // TRUE while the window has keyboard focus (FALSE after Alt+Tab).
    static inline BOOL Is_Focused()    { return s_IsFocused; }

    // TRUE while minimized to the taskbar (skip rendering & sleep to save CPU/GPU!).
    static inline BOOL Is_Minimized()  { return s_IsMinimized; }

    // TRUE in Borderless or exclusive Fullscreen mode.
    static inline BOOL Is_Fullscreen() { return s_Mode != Windowed; }

    // TRUE only on the frame the window's pixel size changed (resize, fullscreen toggle, DPI change).
    static inline BOOL Was_Resized()   { return s_WasResized; }
};

using Window   = WindowManager; // Short alias (Window::Create, Window::Set_Mode, Window::Is_Minimized)
using M_Window = WindowManager; // Prefix alias (type M_ in autocomplete to see all Managers!)
