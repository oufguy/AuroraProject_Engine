#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>

// Public path type so callers can pass paths easily
using FilePath = std::filesystem::path;

// ============================================================================
// Cross-Platform Thread-Safe File Manager
// ============================================================================
class FileManager {
private:
    // ---- Internal Type Aliases (Hidden inside FileManager) ----
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
    // Example: String data = File::Open("Assets/Ship.awotex");
    static inline String Open(const FilePath &filePath) {
        String Msg;

        ScopedLock lock(g_FileMutex);
        InputFile file(filePath, ModeRead);
        if (!file.is_open())
        {
            Msg = StringFormat("FileManager::Open failed to open file: {}", filePath.string());
            LOG(Error, LogLevel::Core, "{}", Msg);
            return "";
        }

        file.seekg(0, FileEnd);
        INT64 size = static_cast<INT64>(file.tellg());
        if (size <= 0)
        {
            Msg = StringFormat("FileManager::Open failed to read file (empty or error): {}", filePath.string());
            LOG(Error, LogLevel::Core, "{}", Msg);
            return "";
        }

        String content(static_cast<size_t>(size), '\0');
        file.seekg(0, FileStart);
        file.read(content.data(), size);

        Msg = StringFormat("FileManager::Open loaded {} bytes from {}", size, filePath.string());
        LOG(Print, LogLevel::Core, "{}", Msg);
        return content;
    }

    // 2. Write data to a file (auto-creates folder/file; set bOverWrite = true to overwrite!)
    // Example (Overwrite): File::Write("Config/Server.ini", "Port=15100\n", true);
    // Example (Append):    File::Write("Logs/Server.log", "[INFO] Started!\n", false);
    static inline BOOL Write(const FilePath &filePath, StringView data, BOOL bOverWrite = false) {
        String Msg;

        ScopedLock lock(g_FileMutex);
        if (filePath.has_parent_path()) {
            ErrorCode ec;
            std::filesystem::create_directories(filePath.parent_path(), ec);
            if (ec) {
                Msg = StringFormat("FileManager::Write failed to create directories for {}: {}", filePath.string(), ec.message());
                LOG(Error, LogLevel::Core, "{}", Msg);
            }
        }

        OutputFile file(filePath, bOverWrite ? ModeOverwrite : ModeAppend);
        if (!file.is_open())
        {
            Msg = StringFormat("FileManager::Write failed to open file: {}", filePath.string());
            LOG(Error, LogLevel::Core, "{}", Msg);
            return false;
        }

        file.write(data.data(), static_cast<StreamSize>(data.size()));

        Msg = StringFormat("FileManager::Write wrote {} bytes to {}", data.size(), filePath.string());
        LOG(Print, LogLevel::Core, "{}", Msg);
        return file.good();
    }

    // 3. Close a loaded file in RAM (frees memory immediately)
    // Example: File::Close(data);
    static inline void Close(String &loadedData) {
        String Msg;
        size_t freedBytes = loadedData.capacity();

        loadedData.clear();
        loadedData.shrink_to_fit();

        Msg = StringFormat("FileManager::Close freed {} bytes of memory", freedBytes);
        LOG(Print, LogLevel::Core, "{}", Msg);
    }

    // 4. Empty a file on disk (0 bytes) without deleting it
    // Example: File::Clean("Logs/Server.log");
    static inline BOOL Clean(const FilePath &filePath) {
        String Msg;

        ScopedLock lock(g_FileMutex);
        OutputFile file(filePath, ModeOverwrite);

        Msg = StringFormat("FileManager::Clean emptied file: {}", filePath.string());
        LOG(Print, LogLevel::Core, "{}", Msg);
        return file.is_open();
    }

    // 5. Delete a file or folder from disk
    // Example: File::Delete("Temp/Cache.tmp");
    static inline BOOL Delete(const FilePath &path) {
        String Msg;

        ScopedLock lock(g_FileMutex);
        ErrorCode ec;
        BOOL removed = std::filesystem::remove_all(path, ec) > 0;
        if (ec) {
            Msg = StringFormat("FileManager::Delete failed for {}: {}", path.string(), ec.message());
            LOG(Error, LogLevel::Core, "{}", Msg);
        } else {
            Msg = StringFormat("FileManager::Delete removed: {}", path.string());
            LOG(Print, LogLevel::Core, "{}", Msg);
        }
        return removed;
    }

    // 6. Check if a file or directory exists on disk
    // Example: if (File::Find("Config/Client.ini")) { /* File is available! */ }
    static inline BOOL Find(const FilePath &filePath) {
        ScopedLock lock(g_FileMutex);
        ErrorCode ec;
        BOOL bExists = std::filesystem::exists(filePath, ec);
        if (ec) {
            String Msg = StringFormat("FileManager::Find error checking {}: {}", filePath.string(), ec.message());
            LOG(Error, LogLevel::Core, "{}", Msg);
            return false;
        }
        return bExists;
    }
};

using File   = FileManager; // Short alias (File::Open, File::Write, File::Delete, etc.)
using M_File = FileManager; // Prefix alias (type M_ in autocomplete to see all Managers!)
