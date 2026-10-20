#pragma once

#include <pch.h>
#include <U_Variables.h>

// ============================================================================
// Button / Key State (Strict Enum — Pressed, Held, or Released)
// ============================================================================
enum InputState : BYTE {
    Pressed  = 0, // Just pressed down on THIS exact frame (e.g. open Inventory 'I' once)
    Held     = 1, // Held down continuously across frames (e.g. hold 'W' to fly forward)
    Released = 2  // Just let go on THIS exact frame (e.g. release charged shot)
};

// ============================================================================
// Universal Keyboard Keys (Matches USB HID / SDL3 Scancodes directly!)
// ============================================================================
enum Key : UINT16 {
    Unknown     = 0,

    // Letters (A - Z)
    A = 4,  B = 5,  C = 6,  D = 7,  E = 8,  F = 9,  G = 10, H = 11, I = 12,
    J = 13, K = 14, L = 15, M = 16, N = 17, O = 18, P = 19, Q = 20, R = 21,
    S = 22, T = 23, U = 24, V = 25, W = 26, X = 27, Y = 28, Z = 29,

    // Top-Row Numbers (1 - 0)
    Num1 = 30, Num2 = 31, Num3 = 32, Num4 = 33, Num5 = 34,
    Num6 = 35, Num7 = 36, Num8 = 37, Num9 = 38, Num0 = 39,

    // Control & Action Keys
    Enter       = 40,
    Escape      = 41,
    Backspace   = 42,
    Tab         = 43,
    Space       = 44,

    // Function Keys (F1 - F12)
    F1  = 58, F2  = 59, F3  = 60, F4  = 61, F5  = 62, F6  = 63,
    F7  = 64, F8  = 65, F9  = 66, F10 = 67, F11 = 68, F12 = 69,

    // Navigation & Arrow Keys
    Insert      = 73,
    Home        = 74,
    PageUp      = 75,
    Delete      = 76,
    End         = 77,
    PageDown    = 78,
    Right       = 79,
    Left        = 80,
    Down        = 81,
    Up          = 82,

    // Modifiers (Shift, Ctrl, Alt)
    CtrlLeft    = 224,
    ShiftLeft   = 225,
    AltLeft     = 226,
    CtrlRight   = 228,
    ShiftRight  = 229,
    AltRight    = 230,

    MaxKeys     = 512
};

// ============================================================================
// Low-Level Keyboard Device State (Updated by M_Input / SDL3 each frame)
// ============================================================================
class Keyboard {
    friend class InputManager; // Allows M_Input to feed OS/SDL3 key events into Keyboard

private:
    static inline fArray<BOOL, Key::MaxKeys> s_CurrentKeys{};
    static inline fArray<BOOL, Key::MaxKeys> s_PreviousKeys{};
    static inline BOOL                       s_IsDirty = false;

    // Called at the start of Input::Update() — only copies array if a key actually changed!
    static inline void BeginFrame() {
        if (s_IsDirty) {
            s_PreviousKeys = s_CurrentKeys;
            s_IsDirty      = false;
        }
    }

    // Called by Input::Update() when SDL3 reports a key press or release
    static inline void OnKeyEvent(UINT16 scancode, BOOL isDown) {
        if (scancode < Key::MaxKeys && s_CurrentKeys[scancode] != isDown) {
            s_CurrentKeys[scancode] = isDown;
            s_IsDirty               = true;
        }
    }

public:
    // Checks if a specific Keyboard key is Pressed, Held, or Released this frame.
    // Example: if (Keyboard::Get_Key(Key::W, InputState::Held))     { /* Fly Forward */ }
    // Example: if (Keyboard::Get_Key(Key::I, InputState::Pressed))  { /* Toggle Inventory */ }
    static inline BOOL Get_Key(Key key, InputState state) {
        if (key >= Key::MaxKeys) return false;

        BOOL curr = s_CurrentKeys[key];
        BOOL prev = s_PreviousKeys[key];

        switch (state) {
            case InputState::Pressed:  return (curr && !prev);
            case InputState::Held:     return curr;
            case InputState::Released: return (!curr && prev);
            default:                   return false;
        }
    }
};

using I_Keyboard = Keyboard; // Prefix alias (type I_ in autocomplete to see Input devices!)
