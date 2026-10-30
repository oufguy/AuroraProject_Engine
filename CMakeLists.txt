cmake_minimum_required(VERSION 3.25)

project(AuroraProject LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Bake C++ runtime DLLs (vcruntime140 / msvcp140) directly into .exe as Static (/MT and /MTd)
# So Debug, Development, and Release .exe files run on any PC or Linux Wine with zero missing DLLs!
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---------- ThirdParty: SDL3 (Static — Zero DLLs!) ----------
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.18
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# ---------- Engine ----------
# Auto-export compile commands for IDE IntelliSense (Visual Studio, VS Code, Clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Engine Folders (Explicit, non-recursive — the ONE place to register a new folder)
# Each folder is added as an include path AND its .h/.cpp files are compiled.
# So `#include <M_Mesh.h>` works without writing the folder path.
set(ENGINE_FOLDERS
    "Engine"                              # Aurora.h (Master umbrella header)

    # Core Module (Platform, RHI, OS abstractions)
    "Engine/Core"                         # Core.h (BeginPlay / Tick / EndPlay) + pch.h
    "Engine/Core/Base"                    # B_Application (Base application lifecycle)
    "Engine/Core/Utilities"               # U_*  Types, Math, Colors
    "Engine/Core/Input"                   # I_*  Keyboard, Mouse
    "Engine/Core/Managers"                # M_*  Log, File, Time, Window, Threads, Crash, Input, Asset
    "Engine/Core/Managers/Loaders"        # M_*  Config, Mesh, Texture (depend on M_File)
    "Engine/Core/Internal/RHI"            # RHI_Manager, R_* Vulkan / DirectX backends
    "Engine/Core/Internal/RHI/Shaders"    # S_*  Precompiled shader bytecode

    # World Module (3D Scene, Entities, Camera)
    "Engine/World"                        # World.h (Module header)
    "Engine/World/Base"                   # B_Game (Base 3D game lifecycle)
    "Engine/World/Framework"              # F_PlayerController
    "Engine/World/Managers"               # M_World (3D World, culling, ticking)
    "Engine/World/Entities"               # E_*  Entities (Camera, StaticMesh, DynamicMesh)

    # Online Module (Multiplayer, Sockets, Replication)
    "Engine/Online"                       # Online.h (Module header)
    "Engine/Online/Base"                  # Base network classes
    "Engine/Online/Managers"              # M_Network (Multiplayer sockets, replication)
)

set(ENGINE_SOURCES "")
set(ENGINE_INCLUDE_DIRS "")
foreach(folder ${ENGINE_FOLDERS})
    file(GLOB folder_files CONFIGURE_DEPENDS "${folder}/*.h" "${folder}/*.cpp")
    list(APPEND ENGINE_SOURCES ${folder_files})
    list(APPEND ENGINE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/${folder}")
endforeach()

add_library(EngineSource STATIC)

target_sources(EngineSource
  PRIVATE
    ${ENGINE_SOURCES}
)

target_include_directories(EngineSource
    PUBLIC
        ${ENGINE_INCLUDE_DIRS}
)

target_precompile_headers(EngineSource
  PUBLIC
    Engine/Core/pch.h
)

# Required on Linux (GCC/Clang) for std::thread and std::mutex (automatic no-op on Windows)
find_package(Threads REQUIRED)

# ---------- ThirdParty: Vulkan SDK ----------
find_package(Vulkan REQUIRED)

target_link_libraries(EngineSource
  PUBLIC
    Threads::Threads
    SDL3::SDL3-static
    Vulkan::Vulkan
)

# Pass configuration macros (_DEBUG, _DEVELOPMENT, _RELEASE)
target_compile_definitions(EngineSource
  PUBLIC
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:_DEVELOPMENT>
    $<$<CONFIG:Release>:_RELEASE>
)

# High-Performance Compiler Optimizations (/O2 MaxSpeed, /Ob2 InlineAny, /Oi Intrinsics, /Ot FastCode, /fp:fast SIMD FMA, /GS- ZeroCookie)
if(MSVC)
    target_compile_options(EngineSource
      PUBLIC
        $<$<NOT:$<CONFIG:Debug>>:/O2 /Ob2 /Oi /Ot /fp:fast /GS->
    )
endif()

# ---------- Client ----------
file(GLOB CLIENT_FILES CONFIGURE_DEPENDS
    "Game/Client/*.cpp"
    "Game/Client/*.h"
)

add_executable(AWO_Client)

target_sources(AWO_Client
  PRIVATE
    ${CLIENT_FILES}
)

target_include_directories(AWO_Client
  PRIVATE
    Game/Client
)

target_link_libraries(AWO_Client
  PRIVATE
    EngineSource
)

set_target_properties(AWO_Client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/Client"
)

# ---------- FieldServer ----------
file(GLOB FIELDSERVER_FILES CONFIGURE_DEPENDS
    "Game/Server/FieldServer/*.cpp"
    "Game/Server/FieldServer/*.h"
)
add_executable(FieldServer)

target_sources(FieldServer
  PRIVATE
    ${FIELDSERVER_FILES}
)

target_link_libraries(FieldServer
  PRIVATE
    EngineSource
)

set_target_properties(FieldServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/Server"
)