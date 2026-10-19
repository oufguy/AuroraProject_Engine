
# Project Architecture & Directory Guide

This repository follows a modular, early-2000s inspired engine design (similar to Unreal Engine 2/3 and id Tech architectures). The primary goal is complete separation between the reusable **Engine** platform and the project-specific **Game** implementation.

---

## High-Level Dependency Rule

* **Engine** is completely game-agnostic: It has zero knowledge of `Game`, `Client`, `Server`, or game gameplay rules.
* **Game** modules (`Client`, `Server`, `Shared`) depend on `Engine` modules, never the other way around.
* **Headless Servers** must only link against headless engine modules (`Core`, `Network`) and must never link against graphics/audio pipelines (`RHI`, `Renderer`, `Audio`, `Input`).

---

## Directory Breakdown
```
MyGameProject/
├── Engine/                      # Reusable, standalone engine framework
│   ├── Source/Runtime/
│   │   ├── Audio/               # Sound system abstraction (DirectSound, OpenAL, FMOD)
│   │   ├── Utilities/           # Math, memory management, containers, logging, filesystem
│   │   ├── Framework/           # Generic Entity/Component model, game loop, world structure
│   │   ├── Input/               # Low-level input polling (DirectInput, RawInput, Win32)
│   │   ├── Network/             # Socket wrappers, platform I/O (IOCP / epoll), TCP/UDP abstractions
│   │   ├── Renderer/            # Scene manager, material system, mesh rendering passes
│   │   └── RHI/                 # Render Hardware Interface (Direct3D 9 / OpenGL abstraction)
│   ├── ThirdParty/              # Core-level external dependencies (e.g., stb, zlib)
│   └── Tools/                   # Asset packaging, pipeline tools, protocol compilers
│
├── Game/                        # Project-specific logic and binaries
│   ├── Client/                  # Game client executable (links Renderer, Audio, Input, Network)
│   │   ├── Gameplay/            # Camera handling, client-side prediction, player animation logic
│   │   └── Interface/           # UI screens, HUD, in-game menus
│   │
│   ├── Server/                  # Dedicated server executables (headless, links Core + Network only)
│   │   ├── Common/              # Inter-server communication and shared server base logic
│   │   ├── FieldServer/         # World maps, spatial partitioning, zone game loops
│   │   ├── IMServer/            # Instant messaging, whisper, guild/chat channels
│   │   ├── NPCServer/           # Monster AI logic, pathfinding, spawn management
│   │   └── PreServer/           # Authentication, gateway routing, account login
│   │
│   └── Shared/                  # Code shared exclusively across Client and Servers
│       ├── GameData/            # Shared balance stats, database definitions, item formulas
│       └── Protocol/            # Binary packet layouts, opcode enums, serialization code
```