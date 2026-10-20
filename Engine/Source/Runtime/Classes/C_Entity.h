#pragma once

#include <Core.h>

// ============================================================================
// Entity Categories (Identifies what kind of object it is)
// ============================================================================
enum class EntityType : BYTE {
    None = 0,
    StaticMesh,     // Fixed environment props, terrain, rocks, buildings
    DynamicMesh,    // Moving doors, elevators, interactive levers
    Character,      // Playable player characters
    Monster,        // Enemies, mobs, bosses
    NPC,            // Quest givers, merchants, friendly characters
    Projectile,     // Missiles, bullets, spell effects
    ItemDrop,       // Loot, gold, pickup items on the ground
    Trigger         // Invisible zones, teleports, event volumes
};

// ============================================================================
// Lightweight C-Style Entity Struct (Pure Data)
// ============================================================================
struct Entity {
    // Unique ID in the Workspace / World
    INT ID = 0;

    // What kind of Entity this is
    EntityType Type = EntityType::None;

    // If FALSE, simulation and rendering skip it immediately
    BOOL bIsActive = true;

    // Visibility: 1.0 = Fully Visible, 0.0 = Fully Invisible
    FLOAT Transparency = 1.0f;

    // RGBA Tint multiplier
    Vector4D Color = Vector4D(1.0f, 1.0f, 1.0f, 1.0f);
    // Texture ID to pass onto Render
    INT TextureID = 0;

    // World Transform
    Vector3D Position = Vector3D(0.0f, 0.0f, 0.0f);
    Vector3D Rotation = Vector3D(0.0f, 0.0f, 0.0f);
    Vector3D Size     = Vector3D(1.0f, 1.0f, 1.0f);
};
