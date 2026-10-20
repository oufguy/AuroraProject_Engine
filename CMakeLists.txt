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
# Auto-grab C++ files in Engine/Source and every subfolder inside it
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "Engine/Source/*.cpp"
    "Engine/Source/*.h"
)

add_library(EngineSource STATIC)

target_sources(EngineSource
  PRIVATE
    ${ENGINE_SOURCES}
)

# Auto-grab Engine/Source and every subfolder inside it (Utilities, Managers, RHI, etc.)
file(GLOB_RECURSE ENGINE_ALL_PATHS CONFIGURE_DEPENDS LIST_DIRECTORIES true "Engine/Source/*")
set(ENGINE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source")
foreach(item ${ENGINE_ALL_PATHS})
    if(IS_DIRECTORY "${item}")
        list(APPEND ENGINE_INCLUDE_DIRS "${item}")
    endif()
endforeach()
target_include_directories(EngineSource
    PUBLIC
        ${ENGINE_INCLUDE_DIRS}
)

target_precompile_headers(EngineSource
  PUBLIC
    Engine/Source/Shared/pch.h
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

# Pass configuration macros (_DEBUG, _DEVELOPMENT, _RELEASE) to Engine, Client, and Server
target_compile_definitions(EngineSource
  PUBLIC
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:_DEVELOPMENT>
    $<$<CONFIG:Release>:_RELEASE>
)

# ---------- Client ----------
file(GLOB_RECURSE CLIENT_FILES CONFIGURE_DEPENDS
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
file(GLOB_RECURSE FIELDSERVER_FILES CONFIGURE_DEPENDS
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