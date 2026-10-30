#pragma once

// ============================================================================
// Aurora Engine — World Module (3D World, Entities, Camera, Physics)
// ============================================================================
// Include this header for 3D game projects:
//   #include <World.h>
//
// Automatically includes Core.h, so you have full access to Window, Input,
// Time, Threads, and RHI, plus:
//   - M_World: Spawning, O(1) ID lookup, spatial grid, 6-plane frustum culling
//   - E_Entity / E_StaticMesh / E_DynamicMesh / E_Camera
//   - M_Physics (3D Raycasting, AABB / Sphere collision)
// ============================================================================

#include <Core.h>
#include <E_Entity.h>
#include <E_StaticMesh.h>
#include <E_DynamicMesh.h>
#include <E_Camera.h>
#include <M_World.h>
