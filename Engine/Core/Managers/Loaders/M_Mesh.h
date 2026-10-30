#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>
#include <U_Math.h>
#include <M_Log.h>
#include <M_File.h>
#include <M_Threads.h>

// ============================================================================
// 3D / 2D Mesh Vertex & Mesh Data
// ============================================================================
struct Vertex {
    Vector3D Position = { 0.0f, 0.0f, 0.0f }; // 3D local position (X, Y, Z)
    Vector3D Normal   = { 0.0f, 1.0f, 0.0f }; // Surface direction for lighting
    Vector2D UV       = { 0.0f, 0.0f };       // Texture coordinates (0.0 to 1.0)
    COLOR    Color    = COLOR_WHITE;          // Vertex tint color (R, G, B, A)
};

struct MeshData {
    Array<Vertex> Vertices;
    Array<UINT>   Indices;
    UINT          MeshID   = 0; // Unique Mesh ID for automatic GPU vertex/index buffer caching
    UINT          Revision = 1; // Incremented by Mark_Dirty() when vertices change dynamically

    void Mark_Dirty() { ++Revision; }

    [[nodiscard]] UINT Get_VertexCount()   const { return static_cast<UINT>(Vertices.size()); }
    [[nodiscard]] UINT Get_IndexCount()    const { return static_cast<UINT>(Indices.size()); }
    [[nodiscard]] UINT Get_TriangleCount() const { return static_cast<UINT>(Indices.size() / 3); }
    [[nodiscard]] BOOL Is_Valid()          const { return !Vertices.empty() && !Indices.empty(); }
};

// Per-Draw GPU Constants (96 bytes: 4x4 MVP Matrix + RGBA Tint + Local Light)
struct DrawConstants {
    FLOAT MVP[16]  = {};
    FLOAT Tint[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
    FLOAT Light[4] = { 0.45f, 0.65f, -0.60f, 0.0f };
};

// ============================================================================
// Built-In Primitive Mesh Generator (Triangle, Quad, Cube)
// ============================================================================
// Quick Usage Example:
//   MeshData tri  = M_Mesh::Create_Triangle(COLOR_RED, COLOR_GREEN, COLOR_BLUE);
//   MeshData quad = M_Mesh::Create_Quad(COLOR_GREEN);
//   MeshData cube = M_Mesh::Create_Cube(COLOR_CYAN);
// ============================================================================
class M_Mesh {
private:
    // Cached per-frame camera basis & projection values (updated once per frame by RHI_Manager::Begin_Frame)
    static inline Vector3D s_CamPos   = { 0.0f, 0.0f, -3.5f };
    static inline Vector3D s_CamRight = { 1.0f, 0.0f,  0.0f };
    static inline Vector3D s_CamUp    = { 0.0f, 1.0f,  0.0f };
    static inline Vector3D s_CamFwd   = { 0.0f, 0.0f,  1.0f };
    static inline FLOAT    s_FocalLen = 1.7320508f; // 1.0 / tan(60 deg * 0.5)
    static inline FLOAT    s_NearClip = 0.1f;
    static inline FLOAT    s_FarClip  = 500.0f;
    static inline FLOAT    s_ProjQ    = 1.00020004f;  // s_FarClip / (s_FarClip - s_NearClip)
    static inline FLOAT    s_ProjZNearQ = 0.100020004f; // s_NearClip * s_ProjQ
    static inline UINT     s_NextMeshID = 0;
    static inline std::mutex             s_CacheMutex;
    static inline uMap<String, MeshData> s_MeshCache;

    struct StreamedMeshJob {
        String                   Key;
        MeshData                 Mesh;
        Function<void(MeshData)> Callback;
    };

    static inline std::mutex             s_StreamMutex;
    static inline Array<StreamedMeshJob> s_CompletedStreamQueue;
    static inline std::atomic<UINT>      s_PendingStreamCount{ 0 };
    static inline std::atomic<UINT>      s_CompletedStreamCount{ 0 };

public:
    // Assigns a new unique MeshID to a MeshData instance for GPU buffer caching
    static inline UINT Next_MeshID() {
        return ++s_NextMeshID;
    }
    enum PrimitiveType : BYTE {
        Triangle = 0, // 3 Vertices, 2 Triangles (Double-sided flat triangle)
        Quad     = 1, // 4 Vertices, 4 Triangles (Double-sided 1x1 flat square / plane)
        Cube     = 2  // 24 Vertices, 12 Triangles (1x1x1 3D box with normals & UVs)
    };

    // Cached camera query accessors (zero recalculation across World & RHI)
    [[nodiscard]] static inline const Vector3D &Get_CameraPos()     { return s_CamPos; }
    [[nodiscard]] static inline const Vector3D &Get_CameraRight()   { return s_CamRight; }
    [[nodiscard]] static inline const Vector3D &Get_CameraUp()      { return s_CamUp; }
    [[nodiscard]] static inline const Vector3D &Get_CameraForward() { return s_CamFwd; }
    [[nodiscard]] static inline FLOAT Get_FocalLength()             { return s_FocalLen; }
    [[nodiscard]] static inline FLOAT Get_NearClip()                { return s_NearClip; }
    [[nodiscard]] static inline FLOAT Get_FarClip()                 { return s_FarClip; }
    [[nodiscard]] static inline FLOAT Get_ProjQ()                   { return s_ProjQ; }
    [[nodiscard]] static inline FLOAT Get_ProjZNearQ()              { return s_ProjZNearQ; }

    // Updates cached camera view vectors once per frame (called automatically by RHI_Manager::Begin_Frame)
    static inline void Set_CameraView(
        const Vector3D &pos,
        const Vector3D &right,
        const Vector3D &up,
        const Vector3D &forward,
        FLOAT           fovDegrees,
        FLOAT           nearClip,
        FLOAT           farClip)
    {
        s_CamPos   = pos;
        s_CamRight = right;
        s_CamUp    = up;
        s_CamFwd   = forward;

        FLOAT halfFov = U_Math::Clamp_MinMax(fovDegrees, 20.0f, 140.0f) * 0.5f * U_Math::DegToRad;
        s_FocalLen    = U_Math::Radians_Cos(halfFov) / U_Math::Radians_Sin(halfFov);
        s_NearClip    = U_Math::Clamp_Min(nearClip, 0.01f);
        s_FarClip     = U_Math::Clamp_Min(farClip, s_NearClip + 1.0f);
        s_ProjQ       = s_FarClip / (s_FarClip - s_NearClip);
        s_ProjZNearQ  = s_NearClip * s_ProjQ;
    }

    // Creates a 1x1 centered Triangle with a single solid color.
    // Example: MeshData tri = M_Mesh::Create_Triangle(COLOR_RED);
    static inline MeshData Create_Triangle(const COLOR &color = COLOR_WHITE) {
        return Create_Triangle(color, color, color);
    }

    // Creates a 1x1 centered Triangle with custom colors on each corner (great for RGB testing!).
    // Double-sided (front + back indices) so it remains visible when flying behind it in 3D.
    // Example: MeshData rgbTri = M_Mesh::Create_Triangle(COLOR_RED, COLOR_GREEN, COLOR_BLUE);
    static inline MeshData Create_Triangle(const COLOR &topColor, const COLOR &rightColor, const COLOR &leftColor) {
        MeshData mesh;
        mesh.Vertices = {
            { {  0.0f,  0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.5f, 0.0f }, topColor   }, // Top
            { {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, rightColor }, // Bottom Right
            { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, leftColor  }  // Bottom Left
        };
        mesh.Indices = {
            0, 1, 2, // Front face
            0, 2, 1  // Back face
        };
        mesh.MeshID = Next_MeshID();
        return mesh;
    }

    // Creates a 1x1 centered Quad / Plane (4 vertices, double-sided so visible from both sides).
    // Example: MeshData quad = M_Mesh::Create_Quad(COLOR_GREEN);
    static inline MeshData Create_Quad(const COLOR &color = COLOR_WHITE) {
        MeshData mesh;
        mesh.Vertices = {
            { { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, color }, // Top Left
            { {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, color }, // Top Right
            { {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, color }, // Bottom Right
            { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, color }  // Bottom Left
        };
        mesh.Indices = {
            0, 1, 2, 2, 3, 0, // Front face
            0, 2, 1, 2, 0, 3  // Back face
        };
        mesh.MeshID = Next_MeshID();
        return mesh;
    }

    // Creates a 1x1x1 centered 3D Cube (24 vertices, 36 indices / 12 triangles).
    // Example: MeshData cube = M_Mesh::Create_Cube(COLOR_CYAN);
    static inline MeshData Create_Cube(const COLOR &color = COLOR_WHITE) {
        MeshData mesh;
        mesh.Vertices = {
            // Front Face (-Z)
            { { -0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f }, color },
            { {  0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f }, color },
            { {  0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f }, color },
            { { -0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f }, color },

            // Back Face (+Z)
            { {  0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f }, color },
            { { -0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f }, color },
            { { -0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f }, color },
            { {  0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f }, color },

            // Left Face (-X)
            { { -0.5f,  0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, color },
            { { -0.5f,  0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }, color },
            { { -0.5f, -0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, color },
            { { -0.5f, -0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }, color },

            // Right Face (+X)
            { {  0.5f,  0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, color },
            { {  0.5f,  0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }, color },
            { {  0.5f, -0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, color },
            { {  0.5f, -0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }, color },

            // Top Face (+Y)
            { { -0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f }, color },
            { {  0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f }, color },
            { {  0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f }, color },
            { { -0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f }, color },

            // Bottom Face (-Y)
            { { -0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f }, color },
            { {  0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f }, color },
            { {  0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f }, color },
            { { -0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f }, color }
        };

        mesh.Indices.reserve(36);
        for (UINT face = 0; face < 6; ++face) {
            UINT base = face * 4;
            mesh.Indices.push_back(base + 0);
            mesh.Indices.push_back(base + 1);
            mesh.Indices.push_back(base + 2);
            mesh.Indices.push_back(base + 2);
            mesh.Indices.push_back(base + 3);
            mesh.Indices.push_back(base + 0);
        }
        mesh.MeshID = Next_MeshID();
        return mesh;
    }

    // Creates a primitive mesh by enum type (Triangle, Quad, or Cube).
    // Example: MeshData mesh = M_Mesh::Create(M_Mesh::Cube, COLOR_YELLOW);
    static inline MeshData Create(PrimitiveType type, const COLOR &color = COLOR_WHITE) {
        switch (type) {
            case Triangle: return Create_Triangle(color);
            case Quad:     return Create_Quad(color);
            case Cube:
            default:       return Create_Cube(color);
        }
    }

    // Loads a 3D Wavefront .OBJ model from disk (supports v, vt, vn, and triangulated/quad faces f).
    // Automatically caches the parsed MeshData by file path so loading the same .obj 100x is instant O(1)!
    // If the file is missing or invalid, logs a warning and falls back to a Cube so the game never crashes.
    static inline MeshData Load_OBJ(const FilePath &path, const COLOR &defaultColor = COLOR_WHITE) {
        String key = path.string();
        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            auto it = s_MeshCache.find(key);
            if (it != s_MeshCache.end()) {
                return it->second;
            }
        }

        if (!M_File::Find(path)) {
            LOG(Warn, LogLevel::Developer, "M_Mesh::Load_OBJ -> File [{}] not found! Falling back to Cube.", key);
            return Create_Cube(defaultColor);
        }

        String text = M_File::Open(path);
        if (text.empty()) {
            return Create_Cube(defaultColor);
        }

        Array<Vector3D> positions;
        Array<Vector3D> normals;
        Array<Vector2D> uvs;
        MeshData        mesh;

        positions.reserve(256);
        normals.reserve(256);
        uvs.reserve(256);

        std::istringstream stream(text);
        String line;
        while (std::getline(stream, line)) {
            if (line.empty() || line[0] == '#') continue;

            std::istringstream ls(line);
            String prefix;
            ls >> prefix;

            if (prefix == "v") {
                Vector3D p{};
                ls >> p.x >> p.y >> p.z;
                positions.push_back(p);
            } else if (prefix == "vt") {
                Vector2D uv{};
                ls >> uv.x >> uv.y;
                uv.y = 1.0f - uv.y; // Convert OpenGL bottom-left V to top-left UV
                uvs.push_back(uv);
            } else if (prefix == "vn") {
                Vector3D n{};
                ls >> n.x >> n.y >> n.z;
                normals.push_back(n);
            } else if (prefix == "f") {
                Array<UINT> faceIndices;
                String vertToken;
                while (ls >> vertToken) {
                    INT vIdx = 0, vtIdx = 0, vnIdx = 0;
                    size_t s1 = vertToken.find('/');
                    if (s1 == String::npos) {
                        vIdx = std::atoi(vertToken.c_str());
                    } else {
                        vIdx = std::atoi(vertToken.substr(0, s1).c_str());
                        size_t s2 = vertToken.find('/', s1 + 1);
                        if (s2 == String::npos) {
                            vtIdx = std::atoi(vertToken.substr(s1 + 1).c_str());
                        } else {
                            if (s2 > s1 + 1) vtIdx = std::atoi(vertToken.substr(s1 + 1, s2 - s1 - 1).c_str());
                            vnIdx = std::atoi(vertToken.substr(s2 + 1).c_str());
                        }
                    }

                    // Handle OBJ 1-based and relative negative indices (used by Blender exports)
                    if (vIdx < 0)  vIdx  = static_cast<INT>(positions.size()) + vIdx + 1;
                    if (vtIdx < 0) vtIdx = static_cast<INT>(uvs.size()) + vtIdx + 1;
                    if (vnIdx < 0) vnIdx = static_cast<INT>(normals.size()) + vnIdx + 1;

                    Vertex vert{};
                    vert.Color = defaultColor;
                    if (vIdx > 0 && static_cast<size_t>(vIdx) <= positions.size()) vert.Position = positions[vIdx - 1];
                    if (vtIdx > 0 && static_cast<size_t>(vtIdx) <= uvs.size())     vert.UV       = uvs[vtIdx - 1];
                    if (vnIdx > 0 && static_cast<size_t>(vnIdx) <= normals.size()) vert.Normal   = normals[vnIdx - 1];

                    faceIndices.push_back(static_cast<UINT>(mesh.Vertices.size()));
                    mesh.Vertices.push_back(vert);
                }

                // Fan-triangulate N-gon face (works for triangles, quads, etc.)
                for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                    mesh.Indices.push_back(faceIndices[0]);
                    mesh.Indices.push_back(faceIndices[i]);
                    mesh.Indices.push_back(faceIndices[i + 1]);
                }
            }
        }

        if (!mesh.Is_Valid()) {
            LOG(Warn, LogLevel::Developer, "M_Mesh::Load_OBJ -> [{}] had no valid geometry! Falling back to Cube.", key);
            return Create_Cube(defaultColor);
        }

        mesh.MeshID = Next_MeshID();
        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            s_MeshCache[key] = mesh;
        }

        LOG(Info, LogLevel::Core, "M_Mesh::Load_OBJ -> Loaded [{}] ({} Vertices, {} Triangles)",
            key, mesh.Get_VertexCount(), mesh.Get_TriangleCount());
        return mesh;
    }

    // Exports any MeshData into a proprietary encrypted Aurora .meshae container!
    // Example: M_Mesh::Save_MeshAe("Assets/Hero.meshae", heroMesh);
    static inline BOOL Save_MeshAe(const FilePath &path, const MeshData &mesh, UINT64 key = 0xA7E04C92F13B8D56ULL) {
        if (!mesh.Is_Valid()) return false;
        const UINT32 vCount = mesh.Get_VertexCount();
        const UINT32 iCount = mesh.Get_IndexCount();
        const size_t vBytes = static_cast<size_t>(vCount) * sizeof(Vertex);
        const size_t iBytes = static_cast<size_t>(iCount) * sizeof(UINT);

        Array<BYTE> buffer(8 + vBytes + iBytes);
        std::memcpy(&buffer[0], &vCount, sizeof(UINT32));
        std::memcpy(&buffer[4], &iCount, sizeof(UINT32));
        std::memcpy(&buffer[8], mesh.Vertices.data(), vBytes);
        std::memcpy(&buffer[8 + vBytes], mesh.Indices.data(), iBytes);

        return M_File::Write_Encrypted(path, buffer.data(), buffer.size(), key, M_File::MAGIC_MESHAE);
    }

    // Packs a raw unencrypted .glb or .obj binary file directly inside an encrypted .meshae container!
    // Dataminers and asset rippers cannot extract it, but the engine reads it on the fly in RAM.
    static inline BOOL Pack_Model_To_MeshAe(const FilePath &meshaePath, const FilePath &sourceModelPath, UINT64 key = 0xA7E04C92F13B8D56ULL) {
        Array<BYTE> modelBytes = M_File::Read_Bytes(sourceModelPath);
        if (modelBytes.empty()) return false;
        return M_File::Write_Encrypted(meshaePath, modelBytes.data(), modelBytes.size(), key, M_File::MAGIC_MESHAE);
    }

    // Loads a proprietary encrypted Aurora 3D model (.meshae)
    static inline MeshData Load_MeshAe(const FilePath &path, UINT64 key = 0xA7E04C92F13B8D56ULL) {
        Array<BYTE> raw = M_File::Open_Encrypted(path, key);
        if (raw.size() < 8) return Create_Cube();

        const UINT32 vCount = *reinterpret_cast<const UINT32 *>(&raw[0]);
        const UINT32 iCount = *reinterpret_cast<const UINT32 *>(&raw[4]);
        const size_t vBytes = static_cast<size_t>(vCount) * sizeof(Vertex);
        const size_t iBytes = static_cast<size_t>(iCount) * sizeof(UINT);

        if (vCount == 0 || iCount == 0 || raw.size() < 8 + vBytes + iBytes) {
            return Create_Cube();
        }

        MeshData mesh;
        mesh.Vertices.resize(vCount);
        mesh.Indices.resize(iCount);
        std::memcpy(mesh.Vertices.data(), &raw[8], vBytes);
        std::memcpy(mesh.Indices.data(), &raw[8 + vBytes], iBytes);
        mesh.MeshID = Next_MeshID();
        return mesh;
    }

    // Universal 3D Model Loader (.meshae encrypted or .obj raw) with O(1) path caching
    // Example: MeshData mesh = M_Mesh::Load("Assets/Sword.meshae");
    static inline MeshData Load(const FilePath &path, const COLOR &defaultColor = COLOR_WHITE) {
        String key = path.string();
        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            auto it = s_MeshCache.find(key);
            if (it != s_MeshCache.end()) {
                return it->second;
            }
        }

        String ext = path.extension().string();
        for (Char &c : ext) c = static_cast<Char>(std::tolower(static_cast<unsigned char>(c)));

        MeshData mesh;
        if (ext == ".meshae") {
            mesh = Load_MeshAe(path);
        } else {
            mesh = Load_OBJ(path, defaultColor);
        }

        if (mesh.Is_Valid()) {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            s_MeshCache[key] = mesh;
        }
        return mesh;
    }

    // Multi-Threaded Asynchronous 3D Model Streaming Queue using M_Threads!
    // Parses .meshae or .obj on a worker thread and dispatches piece-by-piece on Core::Tick() with zero lag!
    static inline void Stream(
        const FilePath &path,
        Function<void(MeshData)> onLoaded,
        const COLOR &defaultColor = COLOR_WHITE)
    {
        String key = path.string();

        // 1. Instant O(1) cache lookup if already parsed & cached
        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            auto it = s_MeshCache.find(key);
            if (it != s_MeshCache.end()) {
                MeshData cached = it->second;
                {
                    std::lock_guard<std::mutex> sLock(s_StreamMutex);
                    s_CompletedStreamQueue.push_back({ key, std::move(cached), std::move(onLoaded) });
                }
                ++s_CompletedStreamCount;
                return;
            }
        }

        // 2. Schedule background worker thread to read & parse file without stalling the main thread
        ++s_PendingStreamCount;
        M_Threads::Run([path, key, onLoaded, defaultColor]() {
            MeshData loaded = Load(path, defaultColor);
            {
                std::lock_guard<std::mutex> lock(s_StreamMutex);
                s_CompletedStreamQueue.push_back({ key, std::move(loaded), std::move(onLoaded) });
            }
            ++s_CompletedStreamCount;
            if (s_PendingStreamCount > 0) {
                --s_PendingStreamCount;
            }
        });
    }

    // Asynchronous loader (routes through the unified time-sliced streaming queue)
    static inline void Stream_OBJ(
        const FilePath &path,
        Function<void(MeshData)> onLoaded,
        const COLOR &defaultColor = COLOR_WHITE)
    {
        Stream(path, std::move(onLoaded), defaultColor);
    }

    // Asynchronous loader (automatically routes through the time-sliced streaming queue)
    static inline void Load_OBJ_Async(
        const FilePath &path,
        Function<void(MeshData)> onLoaded,
        const COLOR &defaultColor = COLOR_WHITE)
    {
        Stream_OBJ(path, std::move(onLoaded), defaultColor);
    }

    // Processes completed mesh streaming jobs piece-by-piece on the main thread during Core::Tick().
    // Limits dispatching to `budgetPerTick` meshes per frame to eliminate frame-time stutters.
    static inline void Tick_Streaming(UINT budgetPerTick = 1) {
        if (budgetPerTick == 0) return;
        // Fast-path: Skip mutex acquisition when no background jobs are completed (saves thousands of locks/sec)
        if (s_CompletedStreamCount.load(std::memory_order_relaxed) == 0) return;

        Array<StreamedMeshJob> jobsToDispatch;
        {
            std::lock_guard<std::mutex> lock(s_StreamMutex);
            if (s_CompletedStreamQueue.empty()) return;

            size_t count = std::min(static_cast<size_t>(budgetPerTick), s_CompletedStreamQueue.size());
            jobsToDispatch.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                jobsToDispatch.push_back(std::move(s_CompletedStreamQueue[i]));
            }
            s_CompletedStreamQueue.erase(s_CompletedStreamQueue.begin(), s_CompletedStreamQueue.begin() + count);
            s_CompletedStreamCount.store(static_cast<UINT>(s_CompletedStreamQueue.size()), std::memory_order_relaxed);
        }

        for (auto &job : jobsToDispatch) {
            if (job.Callback) {
                job.Callback(std::move(job.Mesh));
            }
        }
    }

    // Returns the number of meshes currently loading/parsing in the background
    [[nodiscard]] static inline UINT Get_PendingStreamCount() {
        return s_PendingStreamCount.load();
    }

    // Cleans up all mesh cache and pending streaming queues cleanly
    static inline void Shutdown() {
        {
            std::lock_guard<std::mutex> lock(s_StreamMutex);
            s_CompletedStreamQueue.clear();
            s_PendingStreamCount = 0;
        }
        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            s_MeshCache.clear();
        }
    }

    // Builds the 96-byte GPU constant block (Model-View-Projection matrix, RGBA tint, and 3D lighting)
    static inline DrawConstants Build_DrawConstants(
        const Vector3D &pos,
        const Vector3D &rotDeg,
        const Vector3D &size,
        const COLOR    &color,
        FLOAT           transparency,
        FLOAT           aspectRatio,
        BOOL            bFlipClipY,
        BOOL            bEnable3DShading)
    {
        DrawConstants dc{};

        FLOAT mv00, mv10, mv20;
        FLOAT mv01, mv11, mv21;
        FLOAT mv02, mv12, mv22;
        FLOAT r00 = 1.0f, r10 = 0.0f, r20 = 0.0f;
        FLOAT r01 = 0.0f, r11 = 1.0f, r21 = 0.0f;
        FLOAT r02 = 0.0f, r12 = 0.0f, r22 = 1.0f;

        if (rotDeg.x == 0.0f && rotDeg.y == 0.0f && rotDeg.z == 0.0f) {
            // Fast Path: Unrotated identity matrix (skips 6 trig calls & 36 multiplications)
            mv00 = s_CamRight.x * size.x; mv10 = s_CamUp.x * size.x; mv20 = s_CamFwd.x * size.x;
            mv01 = s_CamRight.y * size.y; mv11 = s_CamUp.y * size.y; mv21 = s_CamFwd.y * size.y;
            mv02 = s_CamRight.z * size.z; mv12 = s_CamUp.z * size.z; mv22 = s_CamFwd.z * size.z;
        } else {
            // 1. Rotation angles (Pitch = X, Yaw = Y, Roll = Z) in radians
            FLOAT rx = rotDeg.x * U_Math::DegToRad;
            FLOAT ry = rotDeg.y * U_Math::DegToRad;
            FLOAT rz = rotDeg.z * U_Math::DegToRad;

            FLOAT cx = U_Math::Radians_Cos(rx), sx = U_Math::Radians_Sin(rx);
            FLOAT cy = U_Math::Radians_Cos(ry), sy = U_Math::Radians_Sin(ry);
            FLOAT cz = U_Math::Radians_Cos(rz), sz = U_Math::Radians_Sin(rz);

            // Rotation matrix R = Ry * Rx * Rz (3x3 basis columns)
            r00 = cy * cz + sy * sx * sz;
            r10 = cx * sz;
            r20 = -sy * cz + cy * sx * sz;

            r01 = -cy * sz + sy * sx * cz;
            r11 = cx * cz;
            r21 = sy * sz + cy * sx * cz;

            r02 = sy * cx;
            r12 = -sx;
            r22 = cy * cx;

            // 2. World Model columns (M = R * Scale) & Camera View Transform (from active E_Camera)
            FLOAT m00 = r00 * size.x, m10 = r10 * size.x, m20 = r20 * size.x;
            FLOAT m01 = r01 * size.y, m11 = r11 * size.y, m21 = r21 * size.y;
            FLOAT m02 = r02 * size.z, m12 = r12 * size.z, m22 = r22 * size.z;

            // Model-View = View * Model (project world columns onto cached Camera Right, Up, Forward)
            mv00 = s_CamRight.x * m00 + s_CamRight.y * m10 + s_CamRight.z * m20;
            mv10 = s_CamUp.x    * m00 + s_CamUp.y    * m10 + s_CamUp.z    * m20;
            mv20 = s_CamFwd.x   * m00 + s_CamFwd.y   * m10 + s_CamFwd.z   * m20;

            mv01 = s_CamRight.x * m01 + s_CamRight.y * m11 + s_CamRight.z * m21;
            mv11 = s_CamUp.x    * m01 + s_CamUp.y    * m11 + s_CamUp.z    * m21;
            mv21 = s_CamFwd.x   * m01 + s_CamFwd.y   * m11 + s_CamFwd.z   * m21;

            mv02 = s_CamRight.x * m02 + s_CamRight.y * m12 + s_CamRight.z * m22;
            mv12 = s_CamUp.x    * m02 + s_CamUp.y    * m12 + s_CamUp.z    * m22;
            mv22 = s_CamFwd.x   * m02 + s_CamFwd.y   * m12 + s_CamFwd.z   * m22;
        }

        FLOAT relX = pos.x - s_CamPos.x;
        FLOAT relY = pos.y - s_CamPos.y;
        FLOAT relZ = pos.z - s_CamPos.z;

        FLOAT mv03 = s_CamRight.x * relX + s_CamRight.y * relY + s_CamRight.z * relZ;
        FLOAT mv13 = s_CamUp.x    * relX + s_CamUp.y    * relY + s_CamUp.z    * relZ;
        FLOAT mv23 = s_CamFwd.x   * relX + s_CamFwd.y   * relY + s_CamFwd.z   * relZ;

        // 3. Perspective Projection (from precomputed camera projection parameters, [0, 1] depth)
        if (aspectRatio <= 0.001f) aspectRatio = 16.0f / 9.0f;
        FLOAT xScale = s_FocalLen / aspectRatio;
        FLOAT yScale = bFlipClipY ? -s_FocalLen : s_FocalLen;
        FLOAT q      = s_ProjQ;
        FLOAT znq    = s_ProjZNearQ;

        // Column-major 4x4 MVP = Proj * ModelView
        dc.MVP[0]  = mv00 * xScale;
        dc.MVP[1]  = mv10 * yScale;
        dc.MVP[2]  = mv20 * q;
        dc.MVP[3]  = mv20;

        dc.MVP[4]  = mv01 * xScale;
        dc.MVP[5]  = mv11 * yScale;
        dc.MVP[6]  = mv21 * q;
        dc.MVP[7]  = mv21;

        dc.MVP[8]  = mv02 * xScale;
        dc.MVP[9]  = mv12 * yScale;
        dc.MVP[10] = mv22 * q;
        dc.MVP[11] = mv22;

        dc.MVP[12] = mv03 * xScale;
        dc.MVP[13] = mv13 * yScale;
        dc.MVP[14] = mv23 * q - znq;
        dc.MVP[15] = mv23;

        // 4. Tint & Alpha Transparency
        FLOAT clampedAlpha = U_Math::Clamp_MinMax(transparency, 0.0f, 1.0f);
        dc.Tint[0] = static_cast<FLOAT>(color.r) / 255.0f;
        dc.Tint[1] = static_cast<FLOAT>(color.g) / 255.0f;
        dc.Tint[2] = static_cast<FLOAT>(color.b) / 255.0f;
        dc.Tint[3] = (static_cast<FLOAT>(color.a) / 255.0f) * clampedAlpha;

        // 5. Transform directional light into local model space (R^T * L_world)
        constexpr FLOAT lx = 0.45f, ly = 0.65f, lz = -0.60f;
        dc.Light[0] = r00 * lx + r10 * ly + r20 * lz;
        dc.Light[1] = r01 * lx + r11 * ly + r21 * lz;
        dc.Light[2] = r02 * lx + r12 * ly + r22 * lz;
        dc.Light[3] = bEnable3DShading ? 1.0f : 0.0f;

        return dc;
    }
};
