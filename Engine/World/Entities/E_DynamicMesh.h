#pragma once

#include <E_Entity.h>

// ============================================================================
// Dynamic Mesh Entity (Moving Objects, Characters, Monsters, Interactive Props)
// ============================================================================
// Quick Usage Example:
//   E_DynamicMesh cube;
//   cube.Mesh     = M_Mesh::Create_Cube(COLOR_CYAN);
//   cube.Position = { -1.1f, 0.0f, 0.0f };
//   cube.Rotation.y += 90.0f * dt;
//   RHI_Manager::Draw_Entity(cube);
// ============================================================================
class E_DynamicMesh : public E_Entity {
public:
    // Dynamic Movement & Rotation Properties (Ticked automatically by M_World::Tick!)
    // Only Dynamic entities pay the 28 bytes of memory for these — StaticMesh & Camera pay 0 bytes!
    Vector3D Velocity      = Vector3D(0.0f, 0.0f, 0.0f); // World movement velocity (Units/sec)
    Vector3D RotationSpeed = Vector3D(0.0f, 0.0f, 0.0f); // Continuous rotation speed (Degrees/sec)
    FLOAT    MoveSpeed     = 5.0f;                       // Base movement speed (Units/sec)

    E_DynamicMesh() {
        Type         = EntityType::DynamicMesh;
        bIsActive    = true;
        bIsVisible   = true;
        Transparency = 1.0f;
    }
};
