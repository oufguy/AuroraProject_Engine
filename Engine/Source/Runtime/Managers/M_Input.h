#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>
#include <M_Config.h>
#include <I_Keyboard.h>
#include <I_Mouse.h>
#include <M_Window.h>

// ============================================================================
// Cross-Platform Input & Keybind Manager (SDL3 Hardware Poller + Action Binds)
// ============================================================================
// Quick Usage Example:
//   // 1. Register remappable actions at startup (auto-loads/saves in [Keybinds] of Config!):
//   Input::Bind("MoveForward", Key::W);
//   Input::Bind("Inventory",   Key::I);
//   Input::Bind("FirePrimary", MouseButton::MB_Left);
//
//   // 2. Inside your main while (Input::Update()) loop:
//   if (Input::Get_Action("MoveForward", InputState::Held))    { /* Fly Forward */ }
//   if (Input::Get_Action("Inventory",   InputState::Pressed)) { /* Open Bag once */ }
//   if (Input::Get_Action("FirePrimary", InputState::Held))    { /* Shoot! */ }
// ============================================================================
class InputManager {
private:
    struct ActionBinding {
        BOOL        isMouse = false;
        Key         key     = Key::Unknown;
        MouseButton button  = MouseButton::MB_Left;
    };

    static inline uMap<String, ActionBinding> s_Bindings;

public:
    // Re-export InputState values so callers can write Input::Pressed, Input::Held, Input::Released!
    static constexpr InputState Pressed  = InputState::Pressed;
    static constexpr InputState Held     = InputState::Held;
    static constexpr InputState Released = InputState::Released;

    // ========================================================================
    // Core Frame Polling (Reads OS/SDL3 Hardware Events into Keyboard & Mouse)
    // ========================================================================

    // Call ONCE per frame in your main loop! Polls SDL3 Keyboard, Mouse, and Window events.
    // Returns FALSE when the user closes the window (clicks [X] or Alt+F4).
    // Example: while (Input::Update()) { ... }
    static inline BOOL Update() {
        // Fast-path: If no Window is open (Console / Headless Server), skip immediately in 1 CPU cycle!
        if (!Window::Is_Open()) [[likely]] return true;

        Keyboard::BeginFrame();
        Mouse::BeginFrame();
        Window::BeginFrame();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            // Forward all window events (resize, focus, minimize) to the Window Manager
            if (ev.type >= SDL_EVENT_WINDOW_FIRST && ev.type <= SDL_EVENT_WINDOW_LAST) {
                Window::OnEvent(ev);
            }

            switch (ev.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    return false;

                case SDL_EVENT_KEY_DOWN:
                    if (!ev.key.repeat) {
                        Keyboard::OnKeyEvent(static_cast<UINT16>(ev.key.scancode), true);
                    }
                    break;

                case SDL_EVENT_KEY_UP:
                    Keyboard::OnKeyEvent(static_cast<UINT16>(ev.key.scancode), false);
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    Mouse::OnButtonEvent(ev.button.button, true);
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    Mouse::OnButtonEvent(ev.button.button, false);
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    Mouse::OnMoveEvent(ev.motion.x, ev.motion.y, ev.motion.xrel, ev.motion.yrel);
                    break;

                case SDL_EVENT_MOUSE_WHEEL:
                    Mouse::OnScrollEvent(ev.wheel.y);
                    break;

                default:
                    break;
            }
        }
        return true;
    }

    // ========================================================================
    // Bind & Rebind Actions (Auto-synced with ConfigManager [Keybinds]!)
    // ========================================================================

    // Binds an Action Name to a Keyboard Key (reads saved key from Config [Keybinds] if present!).
    // Example: Input::Bind("MoveForward", Key::W);
    static inline void Bind(StringView actionName, Key defaultKey) {
        INT savedCode = ConfigManager::Get("Keybinds", actionName, static_cast<INT>(defaultKey));
        s_Bindings[String(actionName)] = { false, static_cast<Key>(savedCode), MouseButton::MB_Left };
    }

    // Binds an Action Name to a Mouse Button (reads saved button from Config [Keybinds] if present!).
    // Example: Input::Bind("FirePrimary", MouseButton::MB_Left);
    static inline void Bind(StringView actionName, MouseButton defaultButton) {
        // Negative numbers in Config [Keybinds] represent Mouse Buttons (-1 = Left, -3 = Right)
        INT savedCode = ConfigManager::Get("Keybinds", actionName, -static_cast<INT>(defaultButton));
        if (savedCode < 0) {
            s_Bindings[String(actionName)] = { true, Key::Unknown, static_cast<MouseButton>(-savedCode) };
        } else {
            s_Bindings[String(actionName)] = { false, static_cast<Key>(savedCode), MouseButton::MB_Left };
        }
    }

    // Changes an existing Action's keybind at runtime and updates Config [Keybinds] in memory!
    // Example: Input::Rebind("MoveForward", Key::Up);
    static inline void Rebind(StringView actionName, Key newKey) {
        s_Bindings[String(actionName)] = { false, newKey, MouseButton::MB_Left };
        ConfigManager::Set("Keybinds", actionName, static_cast<INT>(newKey));
    }

    // Changes an existing Action's bind to a Mouse Button at runtime and updates Config [Keybinds]!
    // Example: Input::Rebind("FirePrimary", MouseButton::MB_Right);
    static inline void Rebind(StringView actionName, MouseButton newButton) {
        s_Bindings[String(actionName)] = { true, Key::Unknown, newButton };
        ConfigManager::Set("Keybinds", actionName, -static_cast<INT>(newButton));
    }

    // ========================================================================
    // Check Action State (Works for both Keyboard & Mouse Binds!)
    // ========================================================================

    // Checks if a bound Action is Pressed, Held, or Released this frame.
    // Example: if (Input::Get_Action("MoveForward", Input::Held)) { ... }
    static inline BOOL Get_Action(StringView actionName, InputState state) {
        auto it = s_Bindings.find(String(actionName));
        if (it == s_Bindings.end()) return false;

        const ActionBinding &bind = it->second;
        if (bind.isMouse) {
            return Mouse::Get_Button(bind.button, state);
        }
        return Keyboard::Get_Key(bind.key, state);
    }
};

using Input   = InputManager; // Short alias (Input::Update, Input::Bind, Input::Get_Action)
using M_Input = InputManager; // Prefix alias (type M_ in autocomplete to see all Managers!)
