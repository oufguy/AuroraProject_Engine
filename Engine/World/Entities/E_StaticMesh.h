#pragma once

#include <E_Entity.h>

// ============================================================================
// Static Mesh Entity (Fixed World Objects, Environment Props, Terrain, Buildings)
// ============================================================================
// Quick Usage Example:
//   E_StaticMesh wall;
//   wall.Mesh     = M_Mesh::Create_Cube(COLOR_GRAY);
//   wall.Position = { 0.0f, 0.0f, 5.0f };
//   RHI_Manager::Draw_Entity(wall);
// ============================================================================
class E_StaticMesh : public E_Entity {
public:
    E_StaticMesh() {
        Type         = EntityType::StaticMesh;
        bIsActive    = true;
        bIsVisible   = true;
        Transparency = 1.0f;
    }
};
