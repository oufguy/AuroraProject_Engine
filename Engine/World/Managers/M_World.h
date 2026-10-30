#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>
#include <M_Threads.h>
#include <M_Mesh.h>
#include <M_Texture.h>
#include <E_Entity.h>
#include <E_StaticMesh.h>
#include <E_DynamicMesh.h>
#include <E_Camera.h>
#include <RHI_Manager.h>

// ============================================================================
// World Manager (Central Entity Spawning, O(1) ID Lookup, Spatial Grid & Frustum Culling)
// ============================================================================
// Architecture & Optimizations:
//   1. Engine Lifecycle (Owned by Core):
//      - Core::BeginPlay()   -> M_World::BeginPlay()
//      - Core::Tick()          -> M_World::Tick(dt)
//      - Core::EndPlay()       -> M_World::Shutdown()
//   2. Strictly Unique ID Lookup (O(1) Hash Map):
//      - Every Entity has a strictly Unique ID provided on Spawn.
//      - Find_Entity(id) and Destroy_Entity(id) use an O(1) hash map index
//        (s_EntityByID) — instant lookup even with 50,000 entities in the World!
//   3. Dynamic-Only Tick List (Zero StaticMesh Tick Overhead):
//      - M_World::Tick(dt) ONLY loops over s_DynamicEntities (Camera, DynamicMesh,
//        Character, Monster, Projectile). 10,000 StaticMesh trees/walls cost
//        0 CPU instructions during Tick!
//   4. 2D Spatial Hash Grid (32x32 Unit Cells):
//      - Partitions the world into 32x32 horizontal cells so Get_EntitiesInRadius()
//        only inspects nearby cells instead of scanning the entire map.
//   5. Full 6-Plane 3D Frustum Culling + Single-Pass Alpha Sorting:
//      - Culls entities outside the active E_Camera's Near, Far, Left, Right, Top,
//        and Bottom frustum planes using 3 dot products (zero per-entity trig).
//      - Draws Opaque entities immediately to fill the hardware Z-buffer, and sorts
//        Transparent entities back-to-front in a pre-reserved queue.
// ============================================================================
class M_World {
public:
    static constexpr FLOAT SPATIAL_CELL_SIZE     = 32.0f;
    static constexpr FLOAT INV_SPATIAL_CELL_SIZE = 1.0f / SPATIAL_CELL_SIZE;

private:
    struct TransparentDrawItem {
        const E_Entity *Entity = nullptr;
        FLOAT           DistSq = 0.0f;
    };

    static inline Array<UniquePtr<E_Entity>>   s_Entities;
    static inline Array<E_DynamicMesh*>        s_DynamicEntities;      // Only Dynamic entities for ultra-fast Tick()!
    static inline uMap<INT, E_Entity*>         s_EntityByID;           // O(1) instant lookup by Unique ID
    static inline uMap<INT64, Array<E_Entity*>> s_SpatialGrid;         // 2D Spatial Hash Grid: CellKey -> Entities in cell
    static inline uMap<INT, INT64>             s_EntityCellKey;        // Tracks each Entity's current Spatial Grid CellKey
    static inline Array<TransparentDrawItem>   s_TransparentQueue;     // Pre-reserved buffer for alpha sorting
    static inline String                       s_WorldName          = "None";
    static inline INT                          s_NextEntityID       = 0;
    static inline INT                          s_VisibleCount       = 0;
    static inline INT                          s_CulledCount        = 0;
    static inline BOOL                         s_IsLoaded           = false;
    static inline BOOL                         s_IsUnloading        = false;
    static inline BOOL                         s_HasPendingDestroys = false;

    // Packs 2D grid coordinates (cellX, cellZ) into a single 64-bit hash key
    [[nodiscard]] static constexpr INT64 Pack_CellKey(INT cellX, INT cellZ) {
        return (static_cast<INT64>(cellX) << 32) | static_cast<UINT32>(cellZ);
    }

    // Computes the 64-bit Spatial Grid cell key for any 3D world position
    [[nodiscard]] static inline INT64 Compute_CellKey(const Vector3D &pos) {
        INT cellX = static_cast<INT>(U_Math::Round(pos.x * INV_SPATIAL_CELL_SIZE, U_Math::Down));
        INT cellZ = static_cast<INT>(U_Math::Round(pos.z * INV_SPATIAL_CELL_SIZE, U_Math::Down));
        return Pack_CellKey(cellX, cellZ);
    }

    // Removes an Entity pointer from a specific Spatial Grid cell bucket
    static inline void Remove_FromSpatialCell(INT64 cellKey, E_Entity *entity) {
        auto it = s_SpatialGrid.find(cellKey);
        if (it != s_SpatialGrid.end()) {
            std::erase(it->second, entity);
            if (it->second.empty()) {
                s_SpatialGrid.erase(it);
            }
        }
    }

    // Updates an Entity's bucket in the 2D Spatial Hash Grid if it crossed a 32-unit cell boundary
    static inline void Sync_SpatialCell(E_Entity &entity) {
        const INT64 newKey = Compute_CellKey(entity.Position);
        auto it = s_EntityCellKey.find(entity.ID);
        if (it != s_EntityCellKey.end()) {
            if (it->second == newKey) return; // Still in the exact same 32x32 cell!
            Remove_FromSpatialCell(it->second, &entity);
            it->second = newKey;
        } else {
            s_EntityCellKey[entity.ID] = newKey;
        }
        s_SpatialGrid[newKey].push_back(&entity);
    }

public:
    // ========================================================================
    // 1. Engine Lifecycle (Called Automatically by Core)
    // ========================================================================

    // Initializes the default active World when the Engine boots
    static inline void BeginPlay(StringView defaultWorldName = "DefaultWorld") {
        if (s_IsLoaded) return;

        s_WorldName          = String(defaultWorldName);
        s_NextEntityID       = 0;
        s_VisibleCount       = 0;
        s_CulledCount        = 0;
        s_IsLoaded           = true;
        s_IsUnloading        = false;
        s_HasPendingDestroys = false;

        s_Entities.reserve(256);
        s_DynamicEntities.reserve(64);
        s_EntityByID.reserve(256);
        s_SpatialGrid.reserve(64);
        s_EntityCellKey.reserve(256);
        s_TransparentQueue.reserve(64);

        LOG(Info, LogLevel::Core, "M_World::BeginPlay -> World [{}] is ready.", s_WorldName);
    }

    // Updates ONLY active Dynamic Entities every frame (StaticMeshes are never touched!)
    // Automatically parallelizes across CPU cores via M_Threads::Parallel_For when entity count > 64!
    static inline void Tick(FLOAT dt) {
        if (!s_IsLoaded || s_IsUnloading) return;

        const size_t dynCount = s_DynamicEntities.size();
        if (dynCount == 0) return;

        // 1. Automatic Multi-Threaded Physics & Movement Dispatch
        if (dynCount > 64) {
            M_Threads::Parallel_For(0, static_cast<UINT>(dynCount), [dt](UINT i) {
                E_DynamicMesh *dyn = s_DynamicEntities[i];
                if (!dyn || !dyn->bIsActive || dyn->bPendingDestroy) return;

                if (dyn->Velocity.x != 0.0f || dyn->Velocity.y != 0.0f || dyn->Velocity.z != 0.0f) {
                    dyn->Move(dyn->Velocity * dt);
                }

                if (dyn->RotationSpeed.x != 0.0f || dyn->RotationSpeed.y != 0.0f || dyn->RotationSpeed.z != 0.0f) {
                    dyn->Add_Rotation(
                        dyn->RotationSpeed.x * dt,
                        dyn->RotationSpeed.y * dt,
                        dyn->RotationSpeed.z * dt);
                }
            });

            // Resync spatial grid cells after parallel simulation
            for (E_DynamicMesh *dyn : s_DynamicEntities) {
                if (dyn && dyn->bIsActive && !dyn->bPendingDestroy &&
                    (dyn->Velocity.x != 0.0f || dyn->Velocity.y != 0.0f || dyn->Velocity.z != 0.0f)) {
                    Sync_SpatialCell(*dyn);
                }
            }
        } else {
            // Fast sequential loop for small counts
            for (E_DynamicMesh *dyn : s_DynamicEntities) {
                if (!dyn || !dyn->bIsActive || dyn->bPendingDestroy) continue;

                if (dyn->Velocity.x != 0.0f || dyn->Velocity.y != 0.0f || dyn->Velocity.z != 0.0f) {
                    dyn->Move(dyn->Velocity * dt);
                    Sync_SpatialCell(*dyn);
                }

                if (dyn->RotationSpeed.x != 0.0f || dyn->RotationSpeed.y != 0.0f || dyn->RotationSpeed.z != 0.0f) {
                    dyn->Add_Rotation(
                        dyn->RotationSpeed.x * dt,
                        dyn->RotationSpeed.y * dt,
                        dyn->RotationSpeed.z * dt);
                }
            }
        }

        // 2. Only sweep destroyed entities on frames where Destroy_Entity(id) was actually called!
        if (s_HasPendingDestroys) {
            std::erase_if(s_DynamicEntities, [](const E_DynamicMesh *dyn) {
                return !dyn || dyn->bPendingDestroy;
            });
            std::erase_if(s_Entities, [](const UniquePtr<E_Entity> &ptr) {
                return !ptr || ptr->bPendingDestroy;
            });
            s_HasPendingDestroys = false;
        }
    }

    // Cleanly unloads the World when the Engine shuts down
    static inline void Shutdown() {
        Unload();
        LOG(Info, LogLevel::Core, "M_World::Shutdown completed.");
    }

    // ========================================================================
    // 2. World State & Map Management (Crash Prevention)
    // ========================================================================

    // Returns TRUE if a World is currently loaded and safe to use
    [[nodiscard]] static inline BOOL Is_Loaded() {
        return s_IsLoaded && !s_IsUnloading;
    }

    // Returns the name of the currently active World
    [[nodiscard]] static inline const String &Get_Name() {
        return s_WorldName;
    }

    // Loads a new named World (cleanly unloads any previous World entities first)
    static inline BOOL Load(StringView worldName) {
        if (worldName.empty()) {
            LOG(Warn, LogLevel::Core, "M_World::Load called with empty worldName!");
            return false;
        }

        if (s_IsLoaded) {
            Unload();
        }

        s_WorldName          = String(worldName);
        s_NextEntityID       = 0;
        s_VisibleCount       = 0;
        s_CulledCount        = 0;
        s_IsLoaded           = true;
        s_IsUnloading        = false;
        s_HasPendingDestroys = false;

        s_Entities.reserve(256);
        s_DynamicEntities.reserve(64);
        s_EntityByID.reserve(256);
        s_SpatialGrid.reserve(64);
        s_EntityCellKey.reserve(256);
        s_TransparentQueue.reserve(64);

        LOG(Info, LogLevel::Core, "M_World::Load -> Loaded World [{}]", s_WorldName);
        return true;
    }

#pragma pack(push, 1)
    struct MapEntityData {
        INT   ID;
        BYTE  Type;
        FLOAT Pos[3];
        FLOAT Rot[3];
        FLOAT Size[3];
        BYTE  Color[4];
        FLOAT Transparency;
    };
#pragma pack(pop)

    // Exports active World entities into an encrypted proprietary Aurora map file (.mapae)
    // Prevents map datamining, secret room leaks, and level reverse engineering!
    // Example: M_World::Save_MapAe("Maps/Dungeon.mapae");
    static inline BOOL Save_MapAe(const FilePath &filePath, UINT64 key = 0xA7E04C92F13B8D56ULL) {
        if (!Is_Loaded()) return false;

        Array<MapEntityData> records;
        records.reserve(s_Entities.size());

        for (const auto &ptr : s_Entities) {
            if (!ptr || ptr->bPendingDestroy) continue;
            MapEntityData d{};
            d.ID           = ptr->ID;
            d.Type         = static_cast<BYTE>(ptr->Type);
            d.Pos[0]       = ptr->Position.x;
            d.Pos[1]       = ptr->Position.y;
            d.Pos[2]       = ptr->Position.z;
            d.Rot[0]       = ptr->Rotation.x;
            d.Rot[1]       = ptr->Rotation.y;
            d.Rot[2]       = ptr->Rotation.z;
            d.Size[0]      = ptr->Size.x;
            d.Size[1]      = ptr->Size.y;
            d.Size[2]      = ptr->Size.z;
            d.Color[0]     = ptr->Color.r;
            d.Color[1]     = ptr->Color.g;
            d.Color[2]     = ptr->Color.b;
            d.Color[3]     = ptr->Color.a;
            d.Transparency = ptr->Transparency;
            records.push_back(d);
        }

        const UINT32 count    = static_cast<UINT32>(records.size());
        const size_t byteSize = sizeof(UINT32) + count * sizeof(MapEntityData);
        Array<BYTE> buffer(byteSize);
        std::memcpy(&buffer[0], &count, sizeof(UINT32));
        if (count > 0) {
            std::memcpy(&buffer[sizeof(UINT32)], records.data(), count * sizeof(MapEntityData));
        }

        return M_File::Write_Encrypted(filePath, buffer.data(), buffer.size(), key, M_File::MAGIC_MAPAE);
    }

    // Loads and spawns an entire encrypted proprietary Aurora map (.mapae) in RAM!
    // Example: M_World::Load_MapAe("Maps/Dungeon.mapae");
    static inline BOOL Load_MapAe(const FilePath &filePath, UINT64 key = 0xA7E04C92F13B8D56ULL) {
        Array<BYTE> raw = M_File::Open_Encrypted(filePath, key);
        if (raw.size() < sizeof(UINT32)) return false;

        const UINT32 count = *reinterpret_cast<const UINT32 *>(&raw[0]);
        if (raw.size() < sizeof(UINT32) + count * sizeof(MapEntityData)) return false;

        Unload();
        BeginPlay(filePath.stem().string());

        const MapEntityData *records = reinterpret_cast<const MapEntityData *>(&raw[sizeof(UINT32)]);
        for (UINT32 i = 0; i < count; ++i) {
            const MapEntityData &d = records[i];
            E_Entity &ent = Spawn_Entity(d.ID, static_cast<EntityType>(d.Type));
            ent.Position  = { d.Pos[0], d.Pos[1], d.Pos[2] };
            ent.Rotation  = { d.Rot[0], d.Rot[1], d.Rot[2] };
            ent.Size      = { d.Size[0], d.Size[1], d.Size[2] };
            ent.Color     = COLOR(d.Color[0], d.Color[1], d.Color[2], d.Color[3]);
            ent.Set_Transparency(d.Transparency);
        }

        LOG(Info, LogLevel::Core, "M_World::Load_MapAe -> Loaded [{}] ({} Entities spawned)", filePath.string(), count);
        return true;
    }

    // Unloads all entities in the current World safely
    static inline void Unload() {
        if (!s_IsLoaded && s_Entities.empty()) return;

        s_IsUnloading = true;
        s_TransparentQueue.clear();
        s_SpatialGrid.clear();
        s_EntityCellKey.clear();
        s_DynamicEntities.clear();
        s_EntityByID.clear();
        s_Entities.clear();

        s_NextEntityID       = 0;
        s_VisibleCount       = 0;
        s_CulledCount        = 0;
        s_IsLoaded           = false;
        s_IsUnloading        = false;
        s_HasPendingDestroys = false;
        s_WorldName          = "None";
    }

    // ========================================================================
    // 3. Central / Shared Entity Spawning, O(1) Lookup & Destruction (Strictly by Unique ID)
    // ========================================================================

    // Central shared spawn function with an explicit Unique ID provided on Spawn!
    // Allocates the exact lean subclass for the given EntityType (StaticMesh pays 0 Dynamic bytes).
    // Example: E_Entity &cube = M_World::Spawn_Entity(1, EntityType::DynamicMesh);
    static inline E_Entity &Spawn_Entity(INT id, EntityType type) {
        // Crash prevention: auto-load DefaultWorld if someone spawns before Load()
        if (!s_IsLoaded) {
            LOG(Warn, LogLevel::Core, "M_World::Spawn_Entity called before World loaded! Auto-starting DefaultWorld.");
            BeginPlay("DefaultWorld");
        }

        // Duplicate Unique ID guard: O(1) check via s_EntityByID
        if (Find_Entity(id) != nullptr) {
            INT fallbackID = ++s_NextEntityID;
            LOG(Warn, LogLevel::Core, "M_World::Spawn_Entity -> Unique ID [{}] already exists! Assigned [{}] instead.", id, fallbackID);
            id = fallbackID;
        } else if (id > s_NextEntityID) {
            s_NextEntityID = id;
        }

        // Allocate the exact memory layout required by EntityType (Zero bloated fields!)
        UniquePtr<E_Entity> ptr;
        switch (type) {
            case EntityType::Camera:
                ptr = MakeUnique<E_Camera>();
                break;
            case EntityType::StaticMesh:
                ptr = MakeUnique<E_StaticMesh>();
                break;
            case EntityType::DynamicMesh:
            case EntityType::Character:
            case EntityType::Monster:
            case EntityType::NPC:
            case EntityType::Projectile:
                ptr = MakeUnique<E_DynamicMesh>();
                break;
            default:
                ptr = MakeUnique<E_Entity>();
                break;
        }

        E_Entity &entity       = *ptr;
        entity.ID              = id;
        entity.Type            = type;
        entity.bIsActive       = true;
        entity.bPendingDestroy = false;

        // Enforce visibility & transparency rules based on EntityType
        switch (type) {
            case EntityType::Camera:
            case EntityType::Trigger:
                entity.bIsVisible   = false;
                entity.Transparency = 0.0f;
                break;
            default:
                entity.bIsVisible   = true;
                entity.Transparency = 1.0f;
                break;
        }

        // Register in O(1) ID lookup table, Spatial Hash Grid & Dynamic Tick list
        s_EntityByID[id] = &entity;
        Sync_SpatialCell(entity);

        switch (type) {
            case EntityType::Camera:
            case EntityType::DynamicMesh:
            case EntityType::Character:
            case EntityType::Monster:
            case EntityType::NPC:
            case EntityType::Projectile:
                s_DynamicEntities.push_back(static_cast<E_DynamicMesh*>(&entity));
                break;
            default:
                break;
        }

        s_Entities.push_back(std::move(ptr));
        return entity;
    }

    // Convenience overload that auto-generates the next Unique ID on Spawn
    static inline E_Entity &Spawn_Entity(EntityType type) {
        return Spawn_Entity(++s_NextEntityID, type);
    }

    // Type-safe templated spawn: auto-generates a unique ID and returns a typed pointer!
    // Example:
    //   E_Camera *cam      = M_World::Spawn_Entity<E_Camera>();
    //   E_DynamicMesh *dyn = M_World::Spawn_Entity<E_DynamicMesh>();
    //   E_StaticMesh *mesh = M_World::Spawn_Entity<E_StaticMesh>();
    template <typename TEntity = E_Entity>
    static inline TEntity *Spawn_Entity() {
        EntityType type = EntityType::StaticMesh;
        if constexpr (std::is_same_v<TEntity, E_Camera>) {
            type = EntityType::Camera;
        } else if constexpr (std::is_same_v<TEntity, E_DynamicMesh>) {
            type = EntityType::DynamicMesh;
        } else if constexpr (std::is_same_v<TEntity, E_StaticMesh>) {
            type = EntityType::StaticMesh;
        }
        return static_cast<TEntity*>(&Spawn_Entity(type));
    }

    // Spawns an Entity asynchronously:
    // Worker threads load & parse the 3D model (.obj) and texture (.texaxe/.bmp/.tga) concurrently.
    // The entity is smoothly integrated on the main thread during Core::Tick() with ZERO frame drops or hitches!
    // Example:
    //   M_World::Spawn_Entity_Async(10, EntityType::DynamicMesh, "Assets/Mob.obj", "Assets/Mob.texaxe", [](E_Entity &mob) {
    //       mob.Position = { 0.0f, 0.0f, 10.0f };
    //   });
    static inline E_Entity &Spawn_Entity_Async(
        INT id,
        EntityType type,
        const FilePath &meshPath,
        const FilePath &texturePath = "",
        Function<void(E_Entity&)> onReady = nullptr)
    {
        E_Entity &entity = Spawn_Entity(id, type);
        entity.bIsVisible = false; // Hidden until mesh & texture finish streaming

        struct StreamState {
            MeshData Mesh;
            UINT     TextureID = 0;
            BOOL     bMeshDone = false;
            BOOL     bTexDone  = false;
        };

        auto state = std::make_shared<StreamState>();
        state->bMeshDone = meshPath.empty();
        state->bTexDone  = texturePath.empty();

        auto CheckDone = [id, state, onReady]() {
            if (!state->bMeshDone || !state->bTexDone) return;
            E_Entity *ent = Find_Entity(id);
            if (!ent) return;

            if (state->Mesh.Is_Valid()) {
                ent->Mesh = std::move(state->Mesh);
            }
            if (state->TextureID != 0) {
                ent->TextureID = state->TextureID;
            }
            ent->bIsVisible = true;

            if (onReady) {
                onReady(*ent);
            }
        };

        if (!meshPath.empty()) {
            M_Mesh::Stream_OBJ(meshPath, [state, CheckDone](MeshData mesh) {
                state->Mesh      = std::move(mesh);
                state->bMeshDone = true;
                CheckDone();
            });
        }

        if (!texturePath.empty()) {
            M_Texture::Stream(texturePath, [state, CheckDone](UINT texID) {
                state->TextureID = texID;
                state->bTexDone  = true;
                CheckDone();
            });
        }

        return entity;
    }

    // Convenience overload for Spawn_Entity_Async with auto-generated Unique ID
    static inline E_Entity &Spawn_Entity_Async(
        EntityType type,
        const FilePath &meshPath,
        const FilePath &texturePath = "",
        Function<void(E_Entity&)> onReady = nullptr)
    {
        return Spawn_Entity_Async(++s_NextEntityID, type, meshPath, texturePath, std::move(onReady));
    }

    // Finds an active Entity strictly by its Unique ID in O(1) constant time!
    // Returns nullptr if World is not loaded, ID does not exist, or Entity is pending destroy.
    [[nodiscard]] static inline E_Entity *Find_Entity(INT id) {
        if (!Is_Loaded()) return nullptr;

        auto it = s_EntityByID.find(id);
        if (it == s_EntityByID.end()) return nullptr;

        E_Entity *entity = it->second;
        if (!entity || entity->bPendingDestroy) return nullptr;
        return entity;
    }

    // Typed O(1) lookup with runtime EntityType safety check (Never crashes on wrong cast!)
    // Example: if (auto *cube = M_World::Find_Entity<E_DynamicMesh>(1)) { ... }
    template<typename T>
    [[nodiscard]] static inline T *Find_Entity(INT id) {
        E_Entity *entity = Find_Entity(id);
        if (!entity) return nullptr;

        if constexpr (std::is_same_v<T, E_DynamicMesh>) {
            switch (entity->Type) {
                case EntityType::Camera:
                case EntityType::DynamicMesh:
                case EntityType::Character:
                case EntityType::Monster:
                case EntityType::NPC:
                case EntityType::Projectile:
                    return static_cast<E_DynamicMesh*>(entity);
                default:
                    return nullptr;
            }
        } else if constexpr (std::is_same_v<T, E_StaticMesh>) {
            return (entity->Type == EntityType::StaticMesh) ? static_cast<E_StaticMesh*>(entity) : nullptr;
        } else if constexpr (std::is_same_v<T, E_Camera>) {
            return (entity->Type == EntityType::Camera) ? static_cast<E_Camera*>(entity) : nullptr;
        } else {
            return static_cast<T*>(entity);
        }
    }

    // Safely marks an Entity strictly by its Unique ID for destruction at the end of the frame
    static inline BOOL Destroy_Entity(INT id) {
        E_Entity *entity = Find_Entity(id);
        if (!entity) return false;

        auto cellIt = s_EntityCellKey.find(id);
        if (cellIt != s_EntityCellKey.end()) {
            Remove_FromSpatialCell(cellIt->second, entity);
            s_EntityCellKey.erase(cellIt);
        }

        entity->bPendingDestroy = true;
        entity->bIsActive       = false;
        entity->bIsVisible      = false;
        s_EntityByID.erase(id);
        s_HasPendingDestroys    = true;
        return true;
    }

    // Spatial Grid Accelerated Radius Query!
    // Only inspects the 32x32 grid cells overlapping (center +- radius) instead of scanning the entire map!
    static inline void Get_EntitiesInRadius(const Vector3D &center, FLOAT radius, Array<E_Entity*> &outEntities) {
        outEntities.clear();
        if (!Is_Loaded() || radius <= 0.0f) return;

        const INT minCellX = static_cast<INT>(U_Math::Round((center.x - radius) * INV_SPATIAL_CELL_SIZE, U_Math::Down));
        const INT maxCellX = static_cast<INT>(U_Math::Round((center.x + radius) * INV_SPATIAL_CELL_SIZE, U_Math::Down));
        const INT minCellZ = static_cast<INT>(U_Math::Round((center.z - radius) * INV_SPATIAL_CELL_SIZE, U_Math::Down));
        const INT maxCellZ = static_cast<INT>(U_Math::Round((center.z + radius) * INV_SPATIAL_CELL_SIZE, U_Math::Down));

        for (INT cz = minCellZ; cz <= maxCellZ; ++cz) {
            for (INT cx = minCellX; cx <= maxCellX; ++cx) {
                auto it = s_SpatialGrid.find(Pack_CellKey(cx, cz));
                if (it == s_SpatialGrid.end()) continue;

                for (E_Entity *entity : it->second) {
                    if (!entity || !entity->bIsActive || entity->bPendingDestroy) continue;
                    if (U_Math::Extended::Get_Distance(entity->Position, center, U_Math::Extended::_3D) <= radius) {
                        outEntities.push_back(entity);
                    }
                }
            }
        }
    }

    // Returns the number of active Entities currently in the World in O(1) time!
    [[nodiscard]] static inline INT Get_EntityCount() {
        return Is_Loaded() ? static_cast<INT>(s_EntityByID.size()) : 0;
    }

    // Returns the number of active Dynamic Entities currently ticked each frame
    [[nodiscard]] static inline INT Get_DynamicEntityCount() {
        return Is_Loaded() ? static_cast<INT>(s_DynamicEntities.size()) : 0;
    }

    // Returns how many Entities passed Frustum Culling and were drawn this frame
    [[nodiscard]] static inline INT Get_VisibleEntityCount() {
        return s_VisibleCount;
    }

    // Returns how many Entities were skipped by 3D Frustum Culling this frame
    [[nodiscard]] static inline INT Get_CulledEntityCount() {
        return s_CulledCount;
    }

    // Returns the active 3D Camera in the World (Never null — built-in fallback prevents crashes!)
    static inline E_Camera *Get_Camera() {
        return &E_Camera::Get_Active();
    }

    // Read-only access to the World's Entity list
    [[nodiscard]] static inline const Array<UniquePtr<E_Entity>> &Get_Entities() {
        return s_Entities;
    }

    // ========================================================================
    // 4. Single-Pass World Rendering (Full 6-Plane 3D Frustum Culling + Alpha Sort)
    // ========================================================================

    // Draws an E_Entity (or E_StaticMesh / E_DynamicMesh) using its Mesh, Position, Rotation, Size, Color, Transparency, and TextureID.
    static inline void Draw_Entity(const E_Entity &entity) {
        if (!entity.bIsActive || !entity.bIsVisible || !entity.Mesh.Is_Valid()) return;
        RHI_Manager::Draw_Mesh(entity.Mesh, entity.Position, entity.Rotation, entity.Size, entity.Color, entity.Transparency, entity.TextureID);
    }

    // Renders all active, visible Entities in a SINGLE loop over s_Entities:
    //   - Full 6-Plane 3D Sphere-Frustum Culling (Near, Far, Left, Right, Top, Bottom)
    //   - Draws Opaque entities immediately to populate the hardware depth buffer
    //   - Queues Transparent entities, sorts them back-to-front by distance, and draws them second
    static inline void Render() {
        if (!Is_Loaded()) return;

        // Sync active 3D camera view matrix before frustum culling and drawing
        E_Camera &activeCam = E_Camera::Get_Active();
        activeCam.Update_CameraView();

        const E_Camera &cam      = activeCam;
        const Vector3D  camPos   = cam.Position;
        const Vector3D  camRight = M_Mesh::Get_CameraRight();
        const Vector3D  camUp    = M_Mesh::Get_CameraUp();
        const Vector3D  camFwd   = M_Mesh::Get_CameraForward();
        const FLOAT     nearClip = cam.NearClip;
        const FLOAT     farClip  = cam.FarClip;

        // Precompute Frustum FOV cone slopes once per frame (Zero per-entity trig!)
        const Vector2D px     = M_Window::Get_PixelSize();
        const FLOAT    aspect = (px.y > 0.0f) ? (px.x / px.y) : (16.0f / 9.0f);
        const FLOAT    tanHalfFovY = (M_Mesh::Get_FocalLength() > 0.001f) ? (1.0f / M_Mesh::Get_FocalLength()) : 0.57735f;
        const FLOAT    tanHalfFovX = tanHalfFovY * aspect;
        const FLOAT    secHalfFovY = U_Math::SquareRoot(1.0f + tanHalfFovY * tanHalfFovY);
        const FLOAT    secHalfFovX = U_Math::SquareRoot(1.0f + tanHalfFovX * tanHalfFovX);

        if (s_TransparentQueue.capacity() < 256) s_TransparentQueue.reserve(256);
        s_TransparentQueue.clear();
        s_VisibleCount = 0;
        s_CulledCount  = 0;

        const size_t totalEntities = s_Entities.size();

        // Automatic Multi-Threaded Frustum Culling Dispatch:
        // When scene has > 128 entities, parallelizes projection & sphere culling across all CPU cores!
        if (totalEntities > 128) {
            static Array<BYTE>  s_VisFlags;
            static Array<FLOAT> s_DistSqArray;
            if (s_VisFlags.size() < totalEntities) {
                s_VisFlags.resize(totalEntities);
                s_DistSqArray.resize(totalEntities);
            }

            std::atomic<INT> parallelVis{ 0 };
            std::atomic<INT> parallelCulled{ 0 };

            M_Threads::Parallel_For(0, static_cast<UINT>(totalEntities), [&](UINT i) {
                const auto &ptr = s_Entities[i];
                if (!ptr || !ptr->bIsActive || !ptr->bIsVisible || ptr->bPendingDestroy ||
                    ptr->Transparency <= 0.001f || !ptr->Mesh.Is_Valid()) {
                    s_VisFlags[i] = 0;
                    return;
                }

                const E_Entity &entity = *ptr;
                const FLOAT dx = entity.Position.x - camPos.x;
                const FLOAT dy = entity.Position.y - camPos.y;
                const FLOAT dz = entity.Position.z - camPos.z;

                const FLOAT maxScale    = U_Math::Clamp_Min(U_Math::Clamp_Min(entity.Size.x, entity.Size.y), entity.Size.z);
                const FLOAT boundRadius = maxScale * 0.9f;

                const FLOAT viewZ = dx * camFwd.x + dy * camFwd.y + dz * camFwd.z;
                if (viewZ + boundRadius < nearClip || viewZ - boundRadius > farClip) {
                    s_VisFlags[i] = 0;
                    ++parallelCulled;
                    return;
                }

                const FLOAT viewX = dx * camRight.x + dy * camRight.y + dz * camRight.z;
                if (U_Math::Absolute(viewX) > viewZ * tanHalfFovX + boundRadius * secHalfFovX) {
                    s_VisFlags[i] = 0;
                    ++parallelCulled;
                    return;
                }

                const FLOAT viewY = dx * camUp.x + dy * camUp.y + dz * camUp.z;
                if (U_Math::Absolute(viewY) > viewZ * tanHalfFovY + boundRadius * secHalfFovY) {
                    s_VisFlags[i] = 0;
                    ++parallelCulled;
                    return;
                }

                s_VisFlags[i]    = (entity.Transparency >= 0.999f) ? 1 : 2; // 1 = Opaque, 2 = Transparent
                s_DistSqArray[i] = dx * dx + dy * dy + dz * dz;
                ++parallelVis;
            });

            s_VisibleCount = parallelVis.load();
            s_CulledCount  = parallelCulled.load();

            for (size_t i = 0; i < totalEntities; ++i) {
                if (s_VisFlags[i] == 1) {
                    Draw_Entity(*s_Entities[i]);
                } else if (s_VisFlags[i] == 2) {
                    s_TransparentQueue.push_back({ s_Entities[i].get(), s_DistSqArray[i] });
                }
            }
        } else {
            // Fast single-thread pass for smaller scenes (<= 128 entities)
            for (const auto &ptr : s_Entities) {
                if (!ptr) continue;
                E_Entity &entity = *ptr;
                if (!entity.bIsActive || !entity.bIsVisible || entity.bPendingDestroy) continue;
                if (entity.Transparency <= 0.001f || !entity.Mesh.Is_Valid()) continue;

                const FLOAT dx = entity.Position.x - camPos.x;
                const FLOAT dy = entity.Position.y - camPos.y;
                const FLOAT dz = entity.Position.z - camPos.z;

                const FLOAT maxScale    = U_Math::Clamp_Min(U_Math::Clamp_Min(entity.Size.x, entity.Size.y), entity.Size.z);
                const FLOAT boundRadius = maxScale * 0.9f;

                const FLOAT viewZ = dx * camFwd.x + dy * camFwd.y + dz * camFwd.z;
                if (viewZ + boundRadius < nearClip || viewZ - boundRadius > farClip) {
                    ++s_CulledCount;
                    continue;
                }

                const FLOAT viewX = dx * camRight.x + dy * camRight.y + dz * camRight.z;
                if (U_Math::Absolute(viewX) > viewZ * tanHalfFovX + boundRadius * secHalfFovX) {
                    ++s_CulledCount;
                    continue;
                }

                const FLOAT viewY = dx * camUp.x + dy * camUp.y + dz * camUp.z;
                if (U_Math::Absolute(viewY) > viewZ * tanHalfFovY + boundRadius * secHalfFovY) {
                    ++s_CulledCount;
                    continue;
                }

                ++s_VisibleCount;

                if (entity.Transparency >= 0.999f) {
                    Draw_Entity(entity);
                } else {
                    const FLOAT distSq = dx * dx + dy * dy + dz * dz;
                    s_TransparentQueue.push_back({ &entity, distSq });
                }
            }
        }

        // Draw Transparent entities sorted farthest-to-closest (Painter's order over Z-buffer)
        if (!s_TransparentQueue.empty()) {
            if (s_TransparentQueue.size() > 1) {
                std::sort(s_TransparentQueue.begin(), s_TransparentQueue.end(),
                    [](const TransparentDrawItem &a, const TransparentDrawItem &b) {
                        return a.DistSq > b.DistSq;
                    });
            }

            for (const TransparentDrawItem &item : s_TransparentQueue) {
                Draw_Entity(*item.Entity);
            }
        }
    }
};
