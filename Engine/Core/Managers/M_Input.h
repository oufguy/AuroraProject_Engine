#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>
#include <I_Keyboard.h>
#include <I_Mouse.h>
#include <M_Window.h>

// ============================================================================
// Cross-Platform Input & Keybind Manager (SDL3 Hardware Poller + Action Binds)
// ============================================================================
// Quick Usage Example:
//   // 1. Register remappable actions:
//   M_Input::Bind("MoveForward", Key::W);
//   M_Input::Bind("Inventory",   Key::I);
//   M_Input::Bind("FirePrimary", MouseButton::MB_Left);
//
//   // 2. Inside your main while (M_Input::Update()) loop:
//   if (M_Input::Get_Action("MoveForward", InputState::Held))    { /* Fly Forward */ }
//   if (M_Input::Get_Action("Inventory",   InputState::Pressed)) { /* Open Bag once */ }
//   if (M_Input::Get_Action("FirePrimary", InputState::Held))    { /* Shoot! */ }
// ============================================================================
class M_Input {
private:
    struct ActionBinding {
        BOOL        isMouse = false;
        Key         key     = Key::Unknown;
        MouseButton button  = MouseButton::MB_Left;
    };

    // Transparent hash & equality so Get_Action(StringView) never allocates a heap String!
    struct StringHash {
        using is_transparent = void;
        size_t operator()(StringView sv) const noexcept { return std::hash<StringView>{}(sv); }
    };

    static inline std::unordered_map<String, ActionBinding, StringHash, std::equal_to<>> s_Bindings;

public:
    // Re-export InputState values so callers can write M_Input::Pressed, M_Input::Held, M_Input::Released!
    static constexpr InputState Pressed  = InputState::Pressed;
    static constexpr InputState Held     = InputState::Held;
    static constexpr InputState Released = InputState::Released;

    // ========================================================================
    // Core Frame Polling (Reads OS/SDL3 Hardware Events into Keyboard & Mouse)
    // ========================================================================

    // Call ONCE per frame in your main loop! Polls SDL3 Keyboard, Mouse, and Window events.
    // Returns FALSE when the user closes the window (clicks [X] or Alt+F4).
    // Example: while (M_Input::Update()) { ... }
    static inline BOOL Update() {
        // Fast-path: If no Window is open (Console / Headless Server), skip immediately in 1 CPU cycle!
        if (!M_Window::Is_Open()) [[likely]] return true;

        I_Keyboard::BeginFrame();
        I_Mouse::BeginFrame();
        M_Window::BeginFrame();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            // Forward all window events (resize, focus, minimize) to the Window Manager
            if (ev.type >= SDL_EVENT_WINDOW_FIRST && ev.type <= SDL_EVENT_WINDOW_LAST) {
                M_Window::OnEvent(ev);
            }

            switch (ev.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    return false;

                case SDL_EVENT_KEY_DOWN:
                    if (!ev.key.repeat) {
                        I_Keyboard::OnKeyEvent(static_cast<UINT16>(ev.key.scancode), true);
                    }
                    break;

                case SDL_EVENT_KEY_UP:
                    I_Keyboard::OnKeyEvent(static_cast<UINT16>(ev.key.scancode), false);
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    I_Mouse::OnButtonEvent(ev.button.button, true);
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    I_Mouse::OnButtonEvent(ev.button.button, false);
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    I_Mouse::OnMoveEvent(ev.motion.x, ev.motion.y, ev.motion.xrel, ev.motion.yrel);
                    break;

                case SDL_EVENT_MOUSE_WHEEL:
                    I_Mouse::OnScrollEvent(ev.wheel.y);
                    break;

                default:
                    break;
            }
        }
        return true;
    }

    // ========================================================================
    // Bind & Rebind Actions
    // ========================================================================

    // Binds an Action Name to a Keyboard Key.
    // Example: M_Input::Bind("MoveForward", Key::W);
    static inline void Bind(StringView actionName, Key defaultKey) {
        s_Bindings[String(actionName)] = { false, defaultKey, MouseButton::MB_Left };
    }

    // Binds an Action Name to a Mouse Button.
    // Example: M_Input::Bind("FirePrimary", MouseButton::MB_Left);
    static inline void Bind(StringView actionName, MouseButton defaultButton) {
        s_Bindings[String(actionName)] = { true, Key::Unknown, defaultButton };
    }

    // Changes an existing Action's keybind at runtime.
    // Example: M_Input::Rebind("MoveForward", Key::Up);
    static inline void Rebind(StringView actionName, Key newKey) {
        s_Bindings[String(actionName)] = { false, newKey, MouseButton::MB_Left };
    }

    // Changes an existing Action's bind to a Mouse Button at runtime.
    // Example: M_Input::Rebind("FirePrimary", MouseButton::MB_Right);
    static inline void Rebind(StringView actionName, MouseButton newButton) {
        s_Bindings[String(actionName)] = { true, Key::Unknown, newButton };
    }

    // ========================================================================
    // Check Action State (Works for both Keyboard & Mouse Binds!)
    // ========================================================================

    // Checks if a bound Action is Pressed, Held, or Released this frame.
    // Example: if (M_Input::Get_Action("MoveForward", M_Input::Held)) { ... }
    static inline BOOL Get_Action(StringView actionName, InputState state) {
        auto it = s_Bindings.find(actionName);
        if (it == s_Bindings.end()) return false;

        const ActionBinding &bind = it->second;
        if (bind.isMouse) {
            return I_Mouse::Get_Button(bind.button, state);
        }
        return I_Keyboard::Get_Key(bind.key, state);
    }
};

