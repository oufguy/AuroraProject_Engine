#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>

// Teach std::cout / std::cerr how to apply any COLOR(r, g, b, a) in the console!
// Uses universal ANSI color codes (\033[90m-\033[97m) so colors work on Windows, Linux, AND Wine!
inline std::ostream &operator<<(std::ostream &os, const COLOR &c) {
    if (c.r == 128 && c.g == 128 && c.b == 128) return os << "\033[90m"; // Gray
    INT code = 90 + ((c.r > 128) ? 1 : 0) + ((c.g > 128) ? 2 : 0) + ((c.b > 128) ? 4 : 0);
    return os << "\033[" << code << 'm';
}

// ============================================================================
// Log Categories & Strict Access Levels
// ============================================================================
enum LogType : BYTE {
    Print,  // Gray    [PRINT]
    Info,   // Green   [INFO]
    Warn,   // Yellow  [WARN]
    Error,  // Red     [ERROR]
    Fatal   // Magenta [FATAL]
};

enum LogLevel : BYTE {
    Core      = 0, // Engine Core logs (Default in Debug — shows Core, Developer, User)
    Developer = 1, // Game Dev logs    (Default in Development — shows Developer, User)
    User      = 2  // Player logs      (Default in Release — only shows User logs unless overridden)
};

// ============================================================================
// Log Manager (Thread-Safe Console Logger & Level Filter)
// ============================================================================
class LogManager {
private:
    using Mutex      = std::mutex;
    using ScopedLock = std::lock_guard<std::mutex>;

    static inline Mutex g_LogMutex;

#if defined(_RELEASE)
    static inline LogLevel s_CurrentLevel = User;      // Release: Only User logs by default
#elif defined(_DEVELOPMENT)
    static inline LogLevel s_CurrentLevel = Developer; // Development: Developer + User logs
#else
    static inline LogLevel s_CurrentLevel = Core;      // Debug: All logs including Engine Core
#endif

public:
    // Changes the active LogLevel filter at runtime (e.g. unlock Core logs in Release if needed!).
    // Example: Log::Set_LogLevel(Core);
    static inline void Set_LogLevel(LogLevel level) {
        s_CurrentLevel = level;
    }

    // Returns the currently active LogLevel filter.
    static inline LogLevel Get_LogLevel() {
        return s_CurrentLevel;
    }

    static inline void Output(LogType type, LogLevel level, StringView message) {
        // Filter out logs below the active configuration level!
        // (e.g. In Release [User=2], Core[0] and Developer[1] are skipped automatically)
        if (level < s_CurrentLevel) return;

        ScopedLock lock(g_LogMutex);

        switch (type) {
            case Info:
                std::cout << LOG_COLOR_INFO << "[INFO] " << message << '\n';
                break;
            case Warn:
                std::cout << LOG_COLOR_WARN << "[WARN] " << message << '\n';
                break;
            case Error:
                std::cerr << LOG_COLOR_ERROR << "[ERROR] " << message << '\n';
                break;
            case Fatal:
                std::cerr << LOG_COLOR_FATAL << "[FATAL] " << message << '\n';
                break;
            case Print:
            default:
                std::cout << LOG_COLOR_PRINT << "[PRINT] " << message << '\n';
                break;
        }
    }
};

using Log       = LogManager; // Short alias (Log::Set_LogLevel, Log::Get_LogLevel)
using M_Log     = LogManager; // Prefix alias (type M_ in autocomplete to see all Managers!)
using LogSystem = LogManager; // Backwards-compatible alias

// ============================================================================
// Global LOG(...) & PRINT(...) Functions
// ============================================================================

// 1. Log with (LogType, LogLevel, "message", ...)
// Example: LOG(Info, Developer, "Player {} spawned at {}", name, pos);
// Example: LOG(Error, Core, "Vulkan device lost!");
template<typename... Args>
inline void LOG(LogType type, LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    LogManager::Output(type, level, std::format(fmt, std::forward<Args>(args)...));
}

// 2. Quick Print: PRINT("message", ...) — Always User Level!
// Example: PRINT("Quick test value: {}", x);
template<typename... Args>
inline void PRINT(std::format_string<Args...> fmt, Args&&... args) {
    LogManager::Output(Print, User, std::format(fmt, std::forward<Args>(args)...));
}
