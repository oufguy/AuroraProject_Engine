#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Math.h>
#include <I_Keyboard.h>
#include <I_Mouse.h>
#include <M_Time.h>
#include <M_Window.h>
#include <E_Camera.h>

// ============================================================================
// F_PlayerController (Player & Camera Controller Framework Class)
// ============================================================================
// Encapsulates player input, view rotation, and smooth camera movement.
//
// Usage:
//   F_PlayerController controller;
//   controller.Possess(myCamera);
//   controller.Update(deltaTime);
// ============================================================================
class F_PlayerController {
public:
    E_Camera *Camera              = nullptr; // Target camera being controlled
    FLOAT     Sensitivity         = 1.0f;    // Mouse look sensitivity
    FLOAT     MoveSpeedMultiplier = 2.0f;    // Sprint / Shift speed multiplier
    BOOL      bEnableMovement     = true;    // Allows toggling controls on/off

    F_PlayerController() = default;
    explicit F_PlayerController(E_Camera *targetCamera) : Camera(targetCamera) {}
    virtual ~F_PlayerController() = default;

    // Attaches the controller to a camera entity or view reference point
    void Attach(E_Camera *targetCamera) {
        Camera = targetCamera;
    }

    // Call once per frame in your Game::Tick to process input and update camera
    virtual void Update(FLOAT deltaTime) {
        // Fall back to active World camera if no camera was explicitly attached (Safe & crash-proof!)
        E_Camera *cam = Camera ? Camera : &E_Camera::Get_Active();
        if (!cam || !bEnableMovement) return;

        FLOAT speed = I_Keyboard::Get_Key(Key::ShiftLeft, M_Input::Held)
            ? (cam->MoveSpeed * MoveSpeedMultiplier)
            : cam->MoveSpeed;

        Vector3D fwd     = cam->Get_Forward();
        Vector3D right   = cam->Get_Right();
        Vector3D wishDir = { 0.0f, 0.0f, 0.0f };

        // WASD horizontal movement
        if (I_Keyboard::Get_Key(Key::W, M_Input::Held)) wishDir = wishDir + fwd;
        if (I_Keyboard::Get_Key(Key::S, M_Input::Held)) wishDir = wishDir - fwd;
        if (I_Keyboard::Get_Key(Key::D, M_Input::Held)) wishDir = wishDir + right;
        if (I_Keyboard::Get_Key(Key::A, M_Input::Held)) wishDir = wishDir - right;

        // Space / Ctrl (or E / Q) vertical elevation
        if (I_Keyboard::Get_Key(Key::Space,    M_Input::Held) || I_Keyboard::Get_Key(Key::E, M_Input::Held)) wishDir.y += 1.0f;
        if (I_Keyboard::Get_Key(Key::CtrlLeft, M_Input::Held) || I_Keyboard::Get_Key(Key::Q, M_Input::Held)) wishDir.y -= 1.0f;

        wishDir = U_Math::Extended::Vector_Normalize(wishDir);

        // Smooth acceleration and deceleration (ticked by M_World::Tick)
        Vector3D targetVel = wishDir * speed;
        cam->Velocity      = U_Math::Extended::Lerp(cam->Velocity, targetVel, 12.0f, deltaTime, U_Math::Extended::Smooth);

        // Right Mouse Button: Lock cursor and rotate camera
        BOOL bRightHeld = I_Mouse::Get_Button(MouseButton::MB_Right, M_Input::Held);
        M_Window::Set_MouseLocked(bRightHeld);
        if (bRightHeld) {
            Vector2D delta = I_Mouse::Get_Delta();
            cam->Add_Rotation(delta.y * 0.15f * Sensitivity, delta.x * 0.15f * Sensitivity);
        }

        // Scroll Wheel: Forward impulse boost
        FLOAT scroll = I_Mouse::Get_Scroll();
        if (scroll != 0.0f) {
            cam->Velocity = cam->Velocity + (fwd * (scroll * 6.0f));
        }

        // 'R' Key: Reset camera to default position
        if (I_Keyboard::Get_Key(Key::R, M_Input::Pressed)) {
            cam->Reset();
        }
    }
};
