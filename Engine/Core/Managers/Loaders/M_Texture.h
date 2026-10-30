#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>
#include <M_Log.h>
#include <M_File.h>
#include <M_Threads.h>

// ============================================================================
// 2D RGBA Texture Data (RAM Pixel Buffer + Unique TextureID for GPU Caching)
// ============================================================================
struct TextureData {
    UINT         TextureID = 0; // 0 = Default 1x1 White Texture
    INT          Width     = 1;
    INT          Height    = 1;
    UINT         Revision  = 1;
    Array<COLOR> Pixels;        // Row-major RGBA8 pixels (Width * Height * 4 bytes)

    void Mark_Dirty() { ++Revision; }

    [[nodiscard]] BOOL Is_Valid() const {
        return Width > 0 && Height > 0 && Pixels.size() == static_cast<size_t>(Width) * static_cast<size_t>(Height);
    }
};

// ============================================================================
// Texture Manager (Procedural Generation, File Loading, Path Cache & Async Load)
// ============================================================================
// Quick Usage Example:
//   UINT checkerTex = M_Texture::Create_Checkerboard(64, 64, 8, COLOR_WHITE, COLOR_GRAY);
//   UINT brickTex   = M_Texture::Load("Assets/Textures/Brick.bmp");
//   cube.Set_Texture(checkerTex);
// ============================================================================
class M_Texture {
private:
    using Mutex      = std::mutex;
    using ScopedLock = std::lock_guard<std::mutex>;

    static inline Mutex                   s_Mutex;
    static inline uMap<UINT, TextureData> s_Textures;
    static inline uMap<String, UINT>      s_PathCache;
    static inline UINT                    s_NextTextureID    = 0;
    static inline UINT                    s_MissingTextureID = 0;
    static inline BOOL                    s_Initialized      = false;

    struct StreamedTextureJob {
        UINT                 TextureID = 0;
        Function<void(UINT)> Callback;
    };

    static inline std::mutex                s_StreamMutex;
    static inline Array<StreamedTextureJob> s_CompletedStreamQueue;
    static inline std::atomic<UINT>         s_PendingStreamCount{ 0 };
    static inline std::atomic<UINT>         s_CompletedStreamCount{ 0 };

    static inline void Ensure_Initialized() {
        if (s_Initialized) return;
        s_Initialized = true;

        // TextureID 0 = Built-in 1x1 Solid White Texture (Default for untextured meshes)
        TextureData white{};
        white.TextureID = 0;
        white.Width     = 1;
        white.Height    = 1;
        white.Revision  = 1;
        white.Pixels    = { COLOR_WHITE };
        s_Textures[0]   = std::move(white);
    }

    // Decodes a .bmp or .tga file from disk into a TextureData struct (runs on main or worker thread)
    static inline BOOL Decode_File(const FilePath &path, TextureData &outTex) {
        if (!M_File::Find(path)) return false;

        String pathStr = path.string();
        String ext     = path.extension().string();
        for (Char &c : ext) c = static_cast<Char>(std::tolower(static_cast<unsigned char>(c)));

        // 1. Try SDL3 built-in BMP loader (supports 8/16/24/32-bit BMPs and converts to RGBA32)
        if (ext == ".bmp") {
            SDL_Surface *surf = SDL_LoadBMP(pathStr.c_str());
            if (!surf) return false;

            SDL_Surface *rgba = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surf);
            if (!rgba) return false;

            outTex.Width  = rgba->w;
            outTex.Height = rgba->h;
            outTex.Pixels.resize(static_cast<size_t>(rgba->w) * static_cast<size_t>(rgba->h));

            const BYTE *srcBytes = static_cast<const BYTE *>(rgba->pixels);
            for (INT y = 0; y < rgba->h; ++y) {
                std::memcpy(
                    &outTex.Pixels[static_cast<size_t>(y) * static_cast<size_t>(rgba->w)],
                    srcBytes + static_cast<size_t>(y) * static_cast<size_t>(rgba->pitch),
                    static_cast<size_t>(rgba->w) * sizeof(COLOR));
            }

            SDL_DestroySurface(rgba);
            return outTex.Is_Valid();
        }

        // 2. Built-in 24-bit / 32-bit Uncompressed TGA Loader (Zero external DLLs!)
        if (ext == ".tga") {
            String rawData = M_File::Open(path);
            if (rawData.size() < 18) return false;
            const BYTE *bytes = reinterpret_cast<const BYTE *>(rawData.data());

            const BYTE idLen     = bytes[0];
            const BYTE imageType = bytes[2];
            const INT  width     = static_cast<INT>(bytes[12] | (bytes[13] << 8));
            const INT  height    = static_cast<INT>(bytes[14] | (bytes[15] << 8));
            const BYTE bpp       = bytes[16];
            const BYTE desc      = bytes[17];

            if (imageType != 2 || width <= 0 || height <= 0 || (bpp != 24 && bpp != 32)) {
                return false;
            }

            const size_t bytesPerPixel = bpp / 8;
            const size_t dataOffset    = 18 + idLen;
            const size_t totalPixels   = static_cast<size_t>(width) * static_cast<size_t>(height);
            if (rawData.size() < dataOffset + totalPixels * bytesPerPixel) return false;

            outTex.Width  = width;
            outTex.Height = height;
            outTex.Pixels.resize(totalPixels);

            const BOOL flipY = ((desc & 0x20) == 0); // TGA default origin is bottom-left unless bit 5 is set
            for (INT y = 0; y < height; ++y) {
                const INT dstY = flipY ? (height - 1 - y) : y;
                for (INT x = 0; x < width; ++x) {
                    const size_t srcIdx = dataOffset + (static_cast<size_t>(y) * width + x) * bytesPerPixel;
                    const BYTE   b      = bytes[srcIdx + 0];
                    const BYTE   g      = bytes[srcIdx + 1];
                    const BYTE   r      = bytes[srcIdx + 2];
                    const BYTE   a      = (bytesPerPixel == 4) ? bytes[srcIdx + 3] : 255;
                    outTex.Pixels[static_cast<size_t>(dstY) * width + x] = COLOR(r, g, b, a);
                }
            }
            return outTex.Is_Valid();
        }

        // 3. Proprietary Encrypted Aurora Texture (.texaxe)
        if (ext == ".texaxe") {
            Array<BYTE> raw = M_File::Open_Encrypted(path);
            if (raw.size() < 8) return false;

            INT width  = *reinterpret_cast<const INT *>(&raw[0]);
            INT height = *reinterpret_cast<const INT *>(&raw[4]);
            size_t pixelBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(COLOR);
            if (width <= 0 || height <= 0 || raw.size() < 8 + pixelBytes) return false;

            outTex.Width  = width;
            outTex.Height = height;
            outTex.Pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
            std::memcpy(outTex.Pixels.data(), &raw[8], pixelBytes);
            return outTex.Is_Valid();
        }

        return false;
    }

public:
    // Returns the default 1x1 White TextureID (0)
    [[nodiscard]] static constexpr UINT Get_White() {
        return 0;
    }

    // Returns a 64x64 Magenta/Black missing-texture fallback checkerboard (never crashes on missing files!)
    [[nodiscard]] static inline UINT Get_MissingTexture() {
        if (s_MissingTextureID != 0) return s_MissingTextureID;
        s_MissingTextureID = Create_Checkerboard(64, 64, 8, COLOR_MAGENTA, COLOR(0, 0, 0, 255));
        return s_MissingTextureID;
    }

    // Creates a 1x1 solid color texture and returns its Unique TextureID
    static inline UINT Create_Solid(const COLOR &color) {
        ScopedLock lock(s_Mutex);
        Ensure_Initialized();

        TextureData tex{};
        tex.TextureID = ++s_NextTextureID;
        tex.Width     = 1;
        tex.Height    = 1;
        tex.Revision  = 1;
        tex.Pixels    = { color };

        UINT id        = tex.TextureID;
        s_Textures[id] = std::move(tex);
        return id;
    }

    // Creates a procedural 2D checkerboard texture (great for UV testing, floors, cubes & debug grids!)
    // Example: UINT checker = M_Texture::Create_Checkerboard(64, 64, 8, COLOR_WHITE, COLOR_GRAY);
    static inline UINT Create_Checkerboard(
        INT          width    = 64,
        INT          height   = 64,
        INT          cellSize = 8,
        const COLOR &colorA   = COLOR_WHITE,
        const COLOR &colorB   = COLOR(50, 65, 85, 255))
    {
        if (width <= 0)    width    = 64;
        if (height <= 0)   height   = 64;
        if (cellSize <= 0) cellSize = 8;

        TextureData tex{};
        tex.Width    = width;
        tex.Height   = height;
        tex.Revision = 1;
        tex.Pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

        for (INT y = 0; y < height; ++y) {
            for (INT x = 0; x < width; ++x) {
                const BOOL isEven = (((x / cellSize) + (y / cellSize)) % 2) == 0;
                tex.Pixels[static_cast<size_t>(y) * width + x] = isEven ? colorA : colorB;
            }
        }

        ScopedLock lock(s_Mutex);
        Ensure_Initialized();
        tex.TextureID  = ++s_NextTextureID;
        UINT id        = tex.TextureID;
        s_Textures[id] = std::move(tex);
        return id;
    }

    // Creates a custom texture directly from an RGBA8 pixel array
    static inline UINT Create_FromPixels(INT width, INT height, const Array<COLOR> &pixels) {
        if (width <= 0 || height <= 0 || pixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
            return Get_MissingTexture();
        }

        ScopedLock lock(s_Mutex);
        Ensure_Initialized();

        TextureData tex{};
        tex.TextureID = ++s_NextTextureID;
        tex.Width     = width;
        tex.Height    = height;
        tex.Revision  = 1;
        tex.Pixels    = pixels;

        UINT id        = tex.TextureID;
        s_Textures[id] = std::move(tex);
        return id;
    }

    // Loads a .bmp or .tga texture from disk (cached by path so duplicate loads are instant O(1)!).
    // If the file is missing or invalid, logs a warning and returns Get_MissingTexture().
    static inline UINT Load(const FilePath &path) {
        String key = path.string();
        {
            ScopedLock lock(s_Mutex);
            Ensure_Initialized();
            auto it = s_PathCache.find(key);
            if (it != s_PathCache.end()) {
                return it->second;
            }
        }

        TextureData decoded{};
        if (!Decode_File(path, decoded)) {
            LOG(Warn, LogLevel::Developer, "M_Texture::Load failed to load [{}]! Using fallback checkerboard.", key);
            return Get_MissingTexture();
        }

        ScopedLock lock(s_Mutex);
        decoded.TextureID = ++s_NextTextureID;
        decoded.Revision  = 1;
        UINT id           = decoded.TextureID;
        s_Textures[id]    = std::move(decoded);
        s_PathCache[key]  = id;

        LOG(Info, LogLevel::Core, "M_Texture::Load -> Loaded [{}] (ID: {}, {}x{})",
            key, id, s_Textures[id].Width, s_Textures[id].Height);
        return id;
    }

    // Multi-Threaded Asynchronous Texture Streaming Queue using M_Threads!
    // Decodes the image on a background worker thread, adds it to the streaming array,
    // and dispatches it piece-by-piece on the main thread during Core::Tick() so frame rates remain rock solid!
    static inline void Stream(const FilePath &path, Function<void(UINT)> onLoaded) {
        String key = path.string();
        {
            ScopedLock lock(s_Mutex);
            Ensure_Initialized();
            auto it = s_PathCache.find(key);
            if (it != s_PathCache.end()) {
                UINT id = it->second;
                {
                    std::lock_guard<std::mutex> sLock(s_StreamMutex);
                    s_CompletedStreamQueue.push_back({ id, std::move(onLoaded) });
                }
                ++s_CompletedStreamCount;
                return;
            }
        }

        ++s_PendingStreamCount;
        M_Threads::Run([path, onLoaded]() {
            UINT texID = Load(path);
            {
                std::lock_guard<std::mutex> lock(s_StreamMutex);
                s_CompletedStreamQueue.push_back({ texID, std::move(onLoaded) });
            }
            ++s_CompletedStreamCount;
            if (s_PendingStreamCount > 0) {
                --s_PendingStreamCount;
            }
        });
    }

    // Multi-Threaded Asynchronous Texture Loader (routes through the time-sliced streaming queue)
    static inline void Load_Async(const FilePath &path, Function<void(UINT)> onLoaded) {
        Stream(path, std::move(onLoaded));
    }

    // Processes completed texture streaming jobs piece-by-piece on the main thread during Core::Tick().
    // Limits dispatching to `budgetPerTick` textures per frame to eliminate frame-time stutters.
    static inline void Tick_Streaming(UINT budgetPerTick = 1) {
        if (budgetPerTick == 0) return;
        // Fast-path: Skip mutex acquisition when no background jobs are completed (saves thousands of locks/sec)
        if (s_CompletedStreamCount.load(std::memory_order_relaxed) == 0) return;

        Array<StreamedTextureJob> jobsToDispatch;
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
                job.Callback(job.TextureID);
            }
        }
    }

    // Returns the number of textures currently decoding in the background
    [[nodiscard]] static inline UINT Get_PendingStreamCount() {
        return s_PendingStreamCount.load();
    }

    // Thread-safe lookup of TextureData by TextureID (falls back to 1x1 White TextureID 0 if not found)
    [[nodiscard]] static inline const TextureData &Get(UINT textureID) {
        ScopedLock lock(s_Mutex);
        Ensure_Initialized();

        auto it = s_Textures.find(textureID);
        if (it != s_Textures.end() && it->second.Is_Valid()) {
            return it->second;
        }
        return s_Textures[0];
    }

    // Exports any loaded or procedural texture into a proprietary encrypted Aurora .texaxe file!
    // Example: M_Texture::Save_TexAxe("Assets/Ground.texaxe", groundTexID);
    static inline BOOL Save_TexAxe(const FilePath &path, UINT textureID, UINT64 key = 0xA7E04C92F13B8D56ULL) {
        const TextureData &tex = Get(textureID);
        if (!tex.Is_Valid()) return false;

        size_t pixelBytes = static_cast<size_t>(tex.Width) * static_cast<size_t>(tex.Height) * sizeof(COLOR);
        Array<BYTE> buffer(8 + pixelBytes);
        std::memcpy(&buffer[0], &tex.Width, sizeof(INT));
        std::memcpy(&buffer[4], &tex.Height, sizeof(INT));
        std::memcpy(&buffer[8], tex.Pixels.data(), pixelBytes);

        return M_File::Write_Encrypted(path, buffer.data(), buffer.size(), key);
    }

    // Unloads a texture from RAM and frees its pixel memory
    // (Default White TextureID 0 cannot be unloaded)
    static inline void Unload(UINT textureID) {
        if (textureID == 0) return;
        ScopedLock lock(s_Mutex);
        auto it = s_Textures.find(textureID);
        if (it != s_Textures.end()) {
            for (auto pathIt = s_PathCache.begin(); pathIt != s_PathCache.end(); ++pathIt) {
                if (pathIt->second == textureID) {
                    s_PathCache.erase(pathIt);
                    break;
                }
            }
            s_Textures.erase(it);
        }
    }

    // Frees all cached CPU texture pixel data and clears the streaming queue cleanly on engine shutdown
    static inline void Shutdown() {
        {
            std::lock_guard<std::mutex> lock(s_StreamMutex);
            s_CompletedStreamQueue.clear();
            s_PendingStreamCount = 0;
        }
        ScopedLock lock(s_Mutex);
        s_Textures.clear();
        s_PathCache.clear();
        s_NextTextureID    = 0;
        s_MissingTextureID = 0;
        s_Initialized      = false;
    }
};
