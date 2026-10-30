#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <I_Keyboard.h>

// ============================================================================
// Universal Mouse Buttons (Matches SDL3 Button IDs directly!)
// ============================================================================
enum MouseButton : BYTE {
    MB_Left       = 1, // Primary click / Fire weapon
    MB_Middle     = 2, // Scroll wheel click
    MB_Right      = 3, // Secondary click / Aim / Camera rotate
    MB_Side1      = 4, // Thumb button 1 (Back)
    MB_Side2      = 5, // Thumb button 2 (Forward)
    MB_MaxButtons = 8
};

// ============================================================================
// Low-Level Mouse Device State (Updated by M_Input / SDL3 each frame)
// ============================================================================
class I_Mouse {
    friend class M_Input; // Allows M_Input to feed OS/SDL3 mouse events into Mouse

private:
    static inline fArray<BOOL, MouseButton::MB_MaxButtons> s_CurrentButtons{};
    static inline fArray<BOOL, MouseButton::MB_MaxButtons> s_PreviousButtons{};
    static inline BOOL                                     s_IsDirty = false;

    static inline Vector2D s_Position    = { 0.0f, 0.0f };
    static inline Vector2D s_Delta       = { 0.0f, 0.0f };
    static inline FLOAT    s_ScrollWheel = 0.0f;

    // Resets per-frame movement delta & scroll wheel at the start of M_Input::Update()
    static inline void BeginFrame() {
        if (s_IsDirty) {
            s_PreviousButtons = s_CurrentButtons;
            s_IsDirty         = false;
        }
        s_Delta       = { 0.0f, 0.0f };
        s_ScrollWheel = 0.0f;
    }

    static inline void OnButtonEvent(BYTE button, BOOL isDown) {
        if (button < MouseButton::MB_MaxButtons) {
            s_CurrentButtons[button] = isDown;
            s_IsDirty                = true;
        }
    }

    static inline void OnMoveEvent(FLOAT x, FLOAT y, FLOAT dx, FLOAT dy) {
        s_Position.x  = x;
        s_Position.y  = y;
        s_Delta.x    += dx;
        s_Delta.y    += dy;
    }

    static inline void OnScrollEvent(FLOAT scrollY) {
        s_ScrollWheel += scrollY;
    }

public:
    // Checks if a Mouse Button (Left, Right, Middle, Side1, Side2) is Pressed, Held, or Released.
    // Example: if (I_Mouse::Get_Button(MouseButton::MB_Left, InputState::Held)) { /* Fire Machine Gun! */ }
    static inline BOOL Get_Button(MouseButton button, InputState state) {
        if (button >= MouseButton::MB_MaxButtons) return false;

        BOOL curr = s_CurrentButtons[button];
        BOOL prev = s_PreviousButtons[button];

        switch (state) {
            case InputState::Pressed:  return (curr && !prev);
            case InputState::Held:     return curr;
            case InputState::Released: return (!curr && prev);
            default:                   return false;
        }
    }

    // Returns the current Mouse cursor position (x, y) inside the window (for clicking UI buttons).
    // Example: Vector2D cursor = I_Mouse::Get_Position();
    static inline Vector2D Get_Position() {
        return s_Position;
    }

    // Returns how far the Mouse moved (dx, dy) on THIS frame (for rotating 3D Camera / Ship!).
    // Example: Vector2D lookDelta = I_Mouse::Get_Delta();
    static inline Vector2D Get_Delta() {
        return s_Delta;
    }

    // Returns Mouse Scroll Wheel step on THIS frame (+1.0 = scrolled up, -1.0 = scrolled down, 0.0 = none).
    // Example: FLOAT zoomStep = I_Mouse::Get_Scroll();
    static inline FLOAT Get_Scroll() {
        return s_ScrollWheel;
    }
};

