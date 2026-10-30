#pragma once

// ============================================================================
// Aurora Engine Master Umbrella Header
// ============================================================================
// Includes all engine subsystems, unified asset manager, and BaseClasses.
// Include this single header in your game code to access everything.
// ============================================================================

// 1. Core Platform & Systems
#include <Core.h>
#include <M_Asset.h>
#include <B_Application.h>

// 2. 3D World & Gameplay
#include <World.h>
#include <F_PlayerController.h>
#include <B_Game.h>

// 3. Networking & Multiplayer
#include <Online.h>
