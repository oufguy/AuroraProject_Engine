#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>

// ============================================================================
// Cross-Platform Window Manager (SDL3 - Windows, Linux X11 & Wayland)
// ============================================================================
// Quick Usage Example:
//   M_Window::Set_Title("Aurora Client");
//   M_Window::Set_Size(1280, 720);
//   M_Window::Set_Mode(M_Window::Windowed);
//
//   while (Core::Tick()) {
//       if (I_Keyboard::Get_Key(Key::F11, M_Input::Pressed)) {
//           M_Window::Set_Mode(M_Window::Is_Fullscreen() ? M_Window::Windowed : M_Window::Borderless);
//       }
//   }
// ============================================================================
class M_Window {
    friend class M_Input; // M_Input::Update() polls SDL events and forwards window events here

public:
    // Display mode for Set_Mode(...)
    enum WindowMode : BYTE {
        Windowed   = 0, // Normal window with title bar (resizable)
        Borderless = 1, // Fullscreen at desktop resolution (instant Alt+Tab, recommended!)
        Fullscreen = 2  // Exclusive fullscreen (changes monitor resolution to Width x Height)
    };

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

private:
    static inline SDL_Window *s_Window      = nullptr;
    static inline BOOL        s_IsOpen      = false;
    static inline BOOL        s_IsFocused   = false;
    static inline BOOL        s_IsMinimized = false;
    static inline BOOL        s_WasResized  = false;
    static inline String      s_Title       = "Aurora";
    static inline WindowMode  s_Mode        = Windowed;
    static inline INT         s_Width       = 1280;
    static inline INT         s_Height      = 720;
    static inline INT         s_PixelWidth  = 1280;
    static inline INT         s_PixelHeight = 720;

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

    // Called by M_Input::Update() at the start of every frame
    static inline void BeginFrame() {
        s_WasResized = false;
    }

    // Called by M_Input::Update() for every SDL window event
    static inline void OnEvent(const SDL_Event &ev) {
        switch (ev.type) {
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (ev.window.data1 != s_PixelWidth || ev.window.data2 != s_PixelHeight) {
                    s_PixelWidth  = ev.window.data1;
                    s_PixelHeight = ev.window.data2;
                    s_WasResized  = true;
                    // Automatically remember new window size if the user dragged the window edges
                    if (s_IsOpen && s_Mode == Windowed && s_PixelWidth > 0 && s_PixelHeight > 0) {
                        SDL_GetWindowSize(s_Window, &s_Width, &s_Height);
                    }
                }
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED: s_IsFocused   = true;  break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:   s_IsFocused   = false; break;
            case SDL_EVENT_WINDOW_MINIMIZED:    s_IsMinimized = true;  break;
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_MAXIMIZED:    s_IsMinimized = false; break;
            default: break;
        }
    }

public:
    // ========================================================================
    // Create & Destroy (Called automatically by Core::BeginPlay / EndPlay)
    // ========================================================================

    // Opens the OS window using current Title, Width, Height, and Mode.
    // Returns FALSE if the OS refused to create a window (no display, e.g. headless Linux server).
    static inline BOOL Create(StringView title = "") {
        if (s_IsOpen) return true;
        if (!title.empty()) s_Title = String(title);

        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            LOG(Error, LogLevel::Core, "M_Window::Create failed to init SDL video: {}", SDL_GetError());
            return false;
        }

        // VULKAN = native Vulkan surface support, RESIZABLE = drag edges, HIGH_PIXEL_DENSITY = sharp on 4K
        SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

        s_Window = SDL_CreateWindow(s_Title.c_str(), s_Width, s_Height, flags);
        if (!s_Window) {
            LOG(Error, LogLevel::Core, "M_Window::Create failed: {}", SDL_GetError());
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return false;
        }

        s_IsOpen      = true;
        s_IsFocused   = true;
        s_IsMinimized = false;
        ApplyMode(s_Mode, s_Width, s_Height);
        SDL_GetWindowSizeInPixels(s_Window, &s_PixelWidth, &s_PixelHeight);

        LOG(Info, LogLevel::Core, "M_Window::Create opened '{}' ({}x{}, {})", s_Title, s_Width, s_Height, ModeToString(s_Mode));
        return true;
    }

    // Closes the window and shuts down SDL video (called automatically by Core::EndPlay).
    static inline void Destroy() {
        if (!s_IsOpen) return;

        SDL_DestroyWindow(s_Window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        s_Window = nullptr;
        s_IsOpen = false;
    }

    // ========================================================================
    // Set_... (Title, Size, Mode, Mouse Lock — Works before OR after Core::BeginPlay!)
    // ========================================================================

    // Changes the text in the window title bar.
    // Example: M_Window::Set_Title("Aurora Client");
    static inline void Set_Title(StringView title) {
        s_Title = String(title);
        if (s_IsOpen) SDL_SetWindowTitle(s_Window, s_Title.c_str());
    }

    // Sets the window size (Width, Height).
    // Example: M_Window::Set_Size(1920, 1080);
    static inline void Set_Size(INT width, INT height) {
        if (width > 0)  s_Width  = width;
        if (height > 0) s_Height = height;
        if (s_IsOpen) ApplyMode(s_Mode, s_Width, s_Height);
    }

    // Switches Windowed / Borderless / Fullscreen mode.
    // Example: M_Window::Set_Mode(M_Window::Borderless);
    static inline void Set_Mode(WindowMode mode) {
        s_Mode = mode;
        if (!s_IsOpen) return;
        ApplyMode(mode, s_Width, s_Height);
        s_WasResized = true;
    }

    // Locks & hides the cursor inside the window -> I_Mouse::Get_Delta() keeps working endlessly.
    // Perfect for rotating the camera while holding Right Mouse Button (MMO style!).
    // Example: M_Window::Set_MouseLocked(I_Mouse::Get_Button(MouseButton::MB_Right, M_Input::Held));
    static inline void Set_MouseLocked(BOOL locked) {
        if (s_IsOpen) SDL_SetWindowRelativeMouseMode(s_Window, locked);
    }

    // ========================================================================
    // Get_... / Is_... / Was_...
    // ========================================================================

    // Returns the window size in screen units (use for UI layout & mouse coordinates).
    // Example: Vector2D size = M_Window::Get_Size();
    static inline Vector2D Get_Size() {
        return { static_cast<FLOAT>(s_Width), static_cast<FLOAT>(s_Height) };
    }

    static inline INT Get_Width()  { return s_Width; }
    static inline INT Get_Height() { return s_Height; }

    // Returns the window size in real screen PIXELS (use for Vulkan swapchain / render resolution).
    // (On a 150% scaled monitor, a 1280x720 window is 1920x1080 pixels!)
    // Example: Vector2D pixels = M_Window::Get_PixelSize();
    static inline Vector2D Get_PixelSize() {
        return { static_cast<FLOAT>(s_PixelWidth), static_cast<FLOAT>(s_PixelHeight) };
    }

    // Returns the current mode (Windowed / Borderless / Fullscreen).
    static inline WindowMode Get_Mode()    { return s_Mode; }

    // Returns the raw SDL_Window* (used by RHI to create its graphics surface).
    static inline SDL_Window *Get_Handle() { return s_Window; }

    // TRUE while the window exists.
    static inline BOOL Is_Open()       { return s_IsOpen; }

    // TRUE while the window has keyboard focus (FALSE after Alt+Tab).
    static inline BOOL Is_Focused()    { return s_IsFocused; }

    // TRUE while minimized to the taskbar.
    static inline BOOL Is_Minimized()  { return s_IsMinimized; }

    // TRUE in Borderless or exclusive Fullscreen mode.
    static inline BOOL Is_Fullscreen() { return s_Mode != Windowed; }

    // TRUE only on the frame the window's pixel size changed (resize, fullscreen toggle, DPI change).
    static inline BOOL Was_Resized()   { return s_WasResized; }
};
