#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>
#include <U_Math.h>
#include <M_Mesh.h>

// ============================================================================
// Entity Categories (Identifies what kind of object it is — Used instead of 'virtual'!)
// ============================================================================
enum class EntityType : BYTE {
    None        = 0,
    Camera      = 1, // 3D Camera (Always invisible & transparent)
    StaticMesh  = 2, // Fixed environment props, terrain, rocks, buildings
    DynamicMesh = 3, // Moving objects, characters, monsters, interactive props
    Character   = 4, // Playable player characters
    Monster     = 5, // Enemies, mobs, bosses
    NPC         = 6, // Quest givers, merchants, friendly characters
    Projectile  = 7, // Missiles, bullets, spell effects
    ItemDrop    = 8, // Loot, gold, pickup items on the ground
    Trigger     = 9  // Invisible zones, teleports, event volumes
};

// ============================================================================
// Base Entity Class (Zero 'virtual' Overhead — Shared Functions Use EntityType!)
// ============================================================================
// How C++ Non-Virtual Inheritance Works Here:
//   - Every function written below in E_Entity is compiled ONCE in memory.
//   - Under the hood, calling `cube.Set_Transparency(0.5f)` is secretly compiled as:
//     `E_Entity::Set_Transparency(&cube, 0.5f)` (passing the reference automatically!).
//   - Because there is NO `virtual` keyword, there is 0 bytes of vtable bloat,
//     and shared functions simply check `switch (Type)` when behavior differs!
// ============================================================================
class E_Entity {
public:
    // Unique ID in the World (Assigned on Spawn via M_World::Spawn_Entity)
    INT        ID              = 0;

    // What kind of Entity this is (Used for fast switch-case instead of virtual!)
    EntityType Type            = EntityType::None;

    // If FALSE, simulation and rendering skip it immediately
    BOOL       bIsActive       = true;

    // If FALSE, rendering skips drawing it (e.g. E_Camera or Trigger volumes)
    BOOL       bIsVisible      = true;

    // Crash prevention flag: set TRUE by M_World::Destroy_Entity, safely erased at end of Tick
    BOOL       bPendingDestroy = false;

    // Visual 3D/2D Mesh shape (Triangle, Quad, Cube, or loaded .OBJ mesh)
    MeshData   Mesh;

    // 2D Surface Texture ID (0 = Default White Texture, or from M_Texture::Load / Create_Checkerboard)
    UINT       TextureID       = 0;

    // Visibility: 1.0 = Fully Visible, 0.0 = Fully Invisible (Transparent)
    FLOAT      Transparency    = 1.0f;

    // RGBA Tint multiplier (Default: White = original vertex colors)
    COLOR      Color           = COLOR_WHITE;

    // World Transform (X, Y, Z)
    Vector3D   Position        = Vector3D(0.0f, 0.0f, 0.0f);
    Vector3D   Rotation        = Vector3D(0.0f, 0.0f, 0.0f); // x = Pitch, y = Yaw, z = Roll (Degrees)
    Vector3D   Size            = Vector3D(1.0f, 1.0f, 1.0f);

    // ========================================================================
    // Shared Set_... / Get_... Functions (Written ONCE, Shared by All Subclasses!)
    // ========================================================================

    // Sets transparency (0.0 to 1.0)
    void Set_Transparency(FLOAT value) {
        Transparency = U_Math::Clamp_MinMax(value, 0.0f, 1.0f);
    }

    // Sets visibility
    void Set_Visible(BOOL visible) {
        bIsVisible = visible;
    }

    // Sets the 2D Texture ID applied to this Entity's surface
    void Set_Texture(UINT textureID) {
        TextureID = textureID;
    }

    // Sets 3D world position
    void Set_Position(const Vector3D &pos) {
        Position = pos;
    }

    // Sets 3D rotation in degrees
    void Set_Rotation(const Vector3D &rotDeg) {
        Rotation = rotDeg;
    }

    // Rotates the Entity by (deltaPitch, deltaYaw, deltaRoll) in degrees
    void Add_Rotation(FLOAT deltaPitch, FLOAT deltaYaw, FLOAT deltaRoll = 0.0f) {
        Rotation.x += deltaPitch;
        Rotation.y += deltaYaw;
        Rotation.z += deltaRoll;
    }

    // Final Transparency = Texture Alpha * Transparency of Entity
    [[nodiscard]] constexpr FLOAT Get_FinalTransparency(FLOAT textureAlpha = 1.0f) const {
        return bIsVisible ? (textureAlpha * Transparency) : 0.0f;
    }

    // ========================================================================
    // Shared 3D Direction Vectors & Movement Helpers
    // ========================================================================

    // Unit vector pointing in the direction this Entity is facing (+Z when Rotation is 0)
    [[nodiscard]] Vector3D Get_Forward() const {
        FLOAT pitch = Rotation.x * U_Math::DegToRad;
        FLOAT yaw   = Rotation.y * U_Math::DegToRad;
        FLOAT cp    = U_Math::Radians_Cos(pitch);
        return {
            U_Math::Radians_Sin(yaw) * cp,
            -U_Math::Radians_Sin(pitch),
            U_Math::Radians_Cos(yaw) * cp
        };
    }

    // Unit vector pointing to this Entity's right (+X when Rotation is 0)
    [[nodiscard]] Vector3D Get_Right() const {
        FLOAT yaw = Rotation.y * U_Math::DegToRad;
        return {
            U_Math::Radians_Cos(yaw),
            0.0f,
            -U_Math::Radians_Sin(yaw)
        };
    }

    // Unit vector pointing up relative to this Entity's tilt (+Y when Rotation is 0)
    [[nodiscard]] Vector3D Get_Up() const {
        FLOAT pitch = Rotation.x * U_Math::DegToRad;
        FLOAT yaw   = Rotation.y * U_Math::DegToRad;
        FLOAT sp    = U_Math::Radians_Sin(pitch);
        FLOAT cp    = U_Math::Radians_Cos(pitch);
        return {
            U_Math::Radians_Sin(yaw) * sp,
            cp,
            U_Math::Radians_Cos(yaw) * sp
        };
    }

    // Moves the Entity by a world-space offset vector
    void Move(const Vector3D &worldOffset) {
        Position.x += worldOffset.x;
        Position.y += worldOffset.y;
        Position.z += worldOffset.z;
    }

    // Moves the Entity forward (+) or backward (-) along its facing direction
    void Move_Forward(FLOAT distance) {
        Vector3D fwd = Get_Forward();
        Position.x += fwd.x * distance;
        Position.y += fwd.y * distance;
        Position.z += fwd.z * distance;
    }

    // Moves the Entity right (+) or left (-) relative to its facing direction
    void Move_Right(FLOAT distance) {
        Vector3D right = Get_Right();
        Position.x += right.x * distance;
        Position.y += right.y * distance;
        Position.z += right.z * distance;
    }

    // Moves the Entity straight up (+) or down (-) along the world Y axis
    void Move_Up(FLOAT distance) {
        Position.y += distance;
    }
};
