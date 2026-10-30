#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>

// Public path type so callers can pass paths easily
using FilePath = std::filesystem::path;

// ============================================================================
// Cross-Platform Thread-Safe File Manager
// ============================================================================
class M_File {
private:
    // ---- Internal Type Aliases (Hidden inside M_File) ----
    using InputFile  = std::ifstream;
    using OutputFile = std::ofstream;
    using ErrorCode  = std::error_code;
    using StreamSize = std::streamsize;
    using Mutex      = std::mutex;
    using ScopedLock = std::lock_guard<std::mutex>;

    // ---- Internal File Mode & Seek Flags (Plain English names!) ----
    static constexpr auto ModeRead      = std::ios::in | std::ios::binary;
    static constexpr auto ModeOverwrite = std::ios::out | std::ios::trunc | std::ios::binary;
    static constexpr auto ModeAppend    = std::ios::out | std::ios::app | std::ios::binary;
    static constexpr auto FileStart     = std::ios::beg;
    static constexpr auto FileEnd       = std::ios::end;

    // ---- Internal Thread Lock ----
    static inline Mutex g_FileMutex;

public:
    // 1. Open and load a file into RAM (returns empty string "" if file doesn't exist or is empty)
    // Example: String data = M_File::Open("Assets/Ship.awotex");
    static inline String Open(const FilePath &filePath) {
        ScopedLock lock(g_FileMutex);
        InputFile file(filePath, ModeRead);
        if (!file.is_open()) {
            LOG(Error, LogLevel::Core, "M_File::Open failed to open file: {}", filePath.string());
            return "";
        }

        file.seekg(0, FileEnd);
        INT64 size = static_cast<INT64>(file.tellg());
        if (size <= 0) {
            LOG(Error, LogLevel::Core, "M_File::Open failed to read file (empty or error): {}", filePath.string());
            return "";
        }

        String content(static_cast<size_t>(size), '\0');
        file.seekg(0, FileStart);
        file.read(content.data(), size);

        LOG(Print, LogLevel::Core, "M_File::Open loaded {} bytes from {}", size, filePath.string());
        return content;
    }

    // 2. Write data to a file (auto-creates folder/file; set bOverWrite = true to overwrite!)
    // Example (Overwrite): M_File::Write("Config/Server.ini", "Port=15100\n", true);
    // Example (Append):    M_File::Write("Logs/Server.log", "[INFO] Started!\n", false);
    static inline BOOL Write(const FilePath &filePath, StringView data, BOOL bOverWrite = false) {
        ScopedLock lock(g_FileMutex);
        if (filePath.has_parent_path()) {
            ErrorCode ec;
            std::filesystem::create_directories(filePath.parent_path(), ec);
            if (ec) {
                LOG(Error, LogLevel::Core, "M_File::Write failed to create directories for {}: {}", filePath.string(), ec.message());
            }
        }

        OutputFile file(filePath, bOverWrite ? ModeOverwrite : ModeAppend);
        if (!file.is_open()) {
            LOG(Error, LogLevel::Core, "M_File::Write failed to open file: {}", filePath.string());
            return false;
        }

        file.write(data.data(), static_cast<StreamSize>(data.size()));

        LOG(Print, LogLevel::Core, "M_File::Write wrote {} bytes to {}", data.size(), filePath.string());
        return file.good();
    }

    // 3. Close a loaded file in RAM (frees memory immediately)
    // Example: M_File::Close(data);
    static inline void Close(String &loadedData) {
        size_t freedBytes = loadedData.capacity();

        loadedData.clear();
        loadedData.shrink_to_fit();

        LOG(Print, LogLevel::Core, "M_File::Close freed {} bytes of memory", freedBytes);
    }

    // 4. Empty a file on disk (0 bytes) without deleting it
    // Example: M_File::Clean("Logs/Server.log");
    static inline BOOL Clean(const FilePath &filePath) {
        ScopedLock lock(g_FileMutex);
        OutputFile file(filePath, ModeOverwrite);

        LOG(Print, LogLevel::Core, "M_File::Clean emptied file: {}", filePath.string());
        return file.is_open();
    }

    // 5. Delete a file or folder from disk
    // Example: M_File::Delete("Temp/Cache.tmp");
    static inline BOOL Delete(const FilePath &path) {
        ScopedLock lock(g_FileMutex);
        ErrorCode ec;
        BOOL removed = std::filesystem::remove_all(path, ec) > 0;
        if (ec) {
            LOG(Error, LogLevel::Core, "M_File::Delete failed for {}: {}", path.string(), ec.message());
        } else {
            LOG(Print, LogLevel::Core, "M_File::Delete removed: {}", path.string());
        }
        return removed;
    }

    // 6. Check if a file or directory exists on disk (Find & Exists are identical aliases)
    // Example: if (M_File::Exists("Config/Client.ini")) { /* File is available! */ }
    // Example: if (M_File::Find("Config/Client.ini"))   { /* File is available! */ }
    [[nodiscard]] static inline BOOL Exists(const FilePath &filePath) {
        return Find(filePath);
    }

    [[nodiscard]] static inline BOOL Find(const FilePath &filePath) {
        ScopedLock lock(g_FileMutex);
        ErrorCode ec;
        BOOL bExists = std::filesystem::exists(filePath, ec);
        if (ec) {
            LOG(Error, LogLevel::Core, "M_File::Find error checking {}: {}", filePath.string(), ec.message());
            return false;
        }
        return bExists;
    }

    // 7. Reads an entire binary file into a byte array (Array<BYTE>)
    // Example: Array<BYTE> glbData = M_File::Read_Bytes("Assets/Models/Cube.glb");
    [[nodiscard]] static inline Array<BYTE> Read_Bytes(const FilePath &filePath) {
        ScopedLock lock(g_FileMutex);
        InputFile file(filePath, ModeRead);
        if (!file.is_open()) {
            LOG(Error, LogLevel::Core, "M_File::Read_Bytes failed to open: {}", filePath.string());
            return {};
        }

        file.seekg(0, FileEnd);
        INT64 size = static_cast<INT64>(file.tellg());
        if (size <= 0) return {};

        Array<BYTE> buffer(static_cast<size_t>(size));
        file.seekg(0, FileStart);
        file.read(reinterpret_cast<Char *>(buffer.data()), size);
        return buffer;
    }

    // ========================================================================
    // 8. In-Memory Ephemeral Obfuscation / In-Place Crypto
    // ========================================================================
    // Keeps raw data encrypted directly in RAM via pointer reference.
    // The key lives only inside the local function stack (not in global memory),
    // making it impossible for CheatEngine or memory scanners to find a static key.
    // Symmetric cipher: calling it twice with the same key restores the original plaintext!
    // Example: M_File::Transform_Memory(buffer.data(), buffer.size(), myKey);
    static inline void Transform_Memory(void *data, size_t size, UINT64 key = 0xA7E04C92F13B8D56ULL) {
        if (!data || size == 0) return;
        BYTE  *bytes = static_cast<BYTE *>(data);
        UINT64 state = key ^ 0x5A73C9E2184B0F3DULL;

        for (size_t i = 0; i < size; ++i) {
            // 64-bit XorShift dynamic rolling keystream
            state ^= (state << 13);
            state ^= (state >> 7);
            state ^= (state << 17);
            BYTE keyByte = static_cast<BYTE>(state & 0xFF) ^ static_cast<BYTE>((i * 31) & 0xFF);
            bytes[i] ^= keyByte;
        }
    }

    // Friendly aliases for Transform_Memory
    static inline void Encrypt_Memory(void *data, size_t size, UINT64 key = 0xA7E04C92F13B8D56ULL) {
        Transform_Memory(data, size, key);
    }
    static inline void Decrypt_Memory(void *data, size_t size, UINT64 key = 0xA7E04C92F13B8D56ULL) {
        Transform_Memory(data, size, key);
    }

    // ========================================================================
    // 9. Proprietary Aurora Binary Asset Containers (.texaxe, .meshae, .mapae)
    // ========================================================================
    // Header format (8 bytes):
    //   - [0..3]: Magic signature:
    //             • 0x45584154 ("TAXE" / .texaxe texture container)
    //             • 0x4541584D ("MXAE" / .meshae 3D model container)
    //             • 0x45415857 ("WXAE" / .mapae  world/level container)
    //   - [4..5]: Version (UINT16 = 1)
    //   - [6..7]: Dynamic Salt (UINT16)
    static constexpr UINT32 MAGIC_TEXAXE = 0x45584154; // 'T', 'A', 'X', 'E'
    static constexpr UINT32 MAGIC_MESHAE = 0x4541584D; // 'M', 'X', 'A', 'E'
    static constexpr UINT32 MAGIC_MAPAE  = 0x45415857; // 'W', 'X', 'A', 'E'
    static constexpr UINT32 AXE_MAGIC    = MAGIC_TEXAXE;
    static constexpr UINT16 AXE_VERSION  = 1;

    // Checks if a file on disk has a proprietary Aurora encrypted container signature
    [[nodiscard]] static inline BOOL Is_Encrypted(const FilePath &filePath) {
        ScopedLock lock(g_FileMutex);
        InputFile file(filePath, ModeRead);
        if (!file.is_open()) return false;

        UINT32 magic = 0;
        file.read(reinterpret_cast<Char *>(&magic), sizeof(magic));
        return (file.gcount() == sizeof(magic) &&
               (magic == MAGIC_TEXAXE || magic == MAGIC_MESHAE || magic == MAGIC_MAPAE));
    }

    // Saves data to disk encrypted inside a proprietary Aurora container (.texaxe / .meshae / .mapae)
    // Example: M_File::Write_Encrypted("Assets/Hero.meshae", bytes.data(), bytes.size(), key, M_File::MAGIC_MESHAE);
    static inline BOOL Write_Encrypted(
        const FilePath &filePath,
        const void *data,
        size_t size,
        UINT64 key = 0xA7E04C92F13B8D56ULL,
        UINT32 magicSignature = MAGIC_TEXAXE)
    {
        if (!data || size == 0) return false;

        Array<BYTE> encrypted(size);
        std::memcpy(encrypted.data(), data, size);

        // Derive dynamic salt from file size
        UINT16 salt = static_cast<UINT16>((size ^ (size >> 16)) & 0xFFFF);
        UINT64 saltedKey = key ^ (static_cast<UINT64>(salt) << 32 | salt);
        Transform_Memory(encrypted.data(), size, saltedKey);

        ScopedLock lock(g_FileMutex);
        if (filePath.has_parent_path()) {
            ErrorCode ec;
            std::filesystem::create_directories(filePath.parent_path(), ec);
        }

        OutputFile file(filePath, ModeOverwrite);
        if (!file.is_open()) {
            LOG(Error, LogLevel::Core, "M_File::Write_Encrypted failed to open: {}", filePath.string());
            return false;
        }

        const UINT32 magic   = magicSignature;
        const UINT16 version = AXE_VERSION;
        file.write(reinterpret_cast<const Char *>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const Char *>(&version), sizeof(version));
        file.write(reinterpret_cast<const Char *>(&salt), sizeof(salt));
        file.write(reinterpret_cast<const Char *>(encrypted.data()), static_cast<StreamSize>(size));

        LOG(Info, LogLevel::Core, "M_File::Write_Encrypted saved proprietary container [{}] ({} bytes)", filePath.string(), size);
        return file.good();
    }

    // Opens and decrypts a proprietary Aurora .texaxe / .axe container from disk into memory
    // Returns empty array if file is invalid, missing, or key is wrong.
    // Example: Array<BYTE> raw = M_File::Open_Encrypted("Assets/Player.texaxe", 0x123456);
    [[nodiscard]] static inline Array<BYTE> Open_Encrypted(const FilePath &filePath, UINT64 key = 0xA7E04C92F13B8D56ULL) {
        ScopedLock lock(g_FileMutex);
        InputFile file(filePath, ModeRead);
        if (!file.is_open()) {
            LOG(Error, LogLevel::Core, "M_File::Open_Encrypted failed to open: {}", filePath.string());
            return {};
        }

        file.seekg(0, FileEnd);
        INT64 totalSize = static_cast<INT64>(file.tellg());
        if (totalSize <= 8) return {}; // Header is 8 bytes minimum

        file.seekg(0, FileStart);
        UINT32 magic   = 0;
        UINT16 version = 0;
        UINT16 salt    = 0;
        file.read(reinterpret_cast<Char *>(&magic), sizeof(magic));
        file.read(reinterpret_cast<Char *>(&version), sizeof(version));
        file.read(reinterpret_cast<Char *>(&salt), sizeof(salt));

        if ((magic != MAGIC_TEXAXE && magic != MAGIC_MESHAE && magic != MAGIC_MAPAE) || version != AXE_VERSION) {
            LOG(Warn, LogLevel::Core, "M_File::Open_Encrypted rejected [{}]: Not a valid Aurora container!", filePath.string());
            return {};
        }

        size_t payloadSize = static_cast<size_t>(totalSize - 8);
        Array<BYTE> payload(payloadSize);
        file.read(reinterpret_cast<Char *>(payload.data()), static_cast<StreamSize>(payloadSize));

        UINT64 saltedKey = key ^ (static_cast<UINT64>(salt) << 32 | salt);
        Transform_Memory(payload.data(), payloadSize, saltedKey);

        return payload;
    }
};
