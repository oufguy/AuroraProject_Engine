#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>
#include <M_File.h>

// ============================================================================
// Cross-Platform Config Manager (.ini / .cfg Reader & Writer)
// ============================================================================
// Quick Usage Example:
//   Config::Load("Config/Client.ini");
//
//   INT    width      = Config::Get("Window",  "Width",       1920);
//   INT    height     = Config::Get("Window",  "Height",      1080);
//   BOOL   fullscreen = Config::Get("Window",  "Fullscreen",  false);
//   FLOAT  mouseSens  = Config::Get("Input",   "Sensitivity", 1.5f);
//   String serverIP   = Config::Get("Network", "ServerIP",    "127.0.0.1");
//
//   Config::Save(); // Auto-creates Config/Client.ini with defaults if missing!
// ============================================================================
class ConfigManager {
private:
    // Stores settings as: s_Data["Section"]["Key"] = "Value"
    // Uses sorted Map so saved .ini files are always neatly ordered!
    using KeyValueMap = Map<String, String>;
    using SectionMap  = Map<String, KeyValueMap>;

    static inline SectionMap s_Data;
    static inline FilePath   s_CurrentFile = "Config/Settings.ini";

    // Trims spaces, tabs, and '\r' from the start and end of a line
    static inline String Trim(StringView text) {
        size_t start = text.find_first_not_of(" \t\r\n");
        if (start == StringView::npos) return "";
        size_t end = text.find_last_not_of(" \t\r\n");
        return String(text.substr(start, end - start + 1));
    }

public:
    // ========================================================================
    // Load & Save (.ini / .cfg files)
    // ========================================================================

    // Loads settings from an .ini/.cfg file into memory.
    // If the file doesn't exist yet, it remembers the path so Save() will create it!
    // Example: Config::Load("Config/Client.ini");
    static inline BOOL Load(const FilePath &filePath) {
        s_CurrentFile = filePath;
        s_Data.clear();

        if (!std::filesystem::exists(filePath)) {
            LOG(Warn, LogLevel::Core, "ConfigManager::Load '{}' not found — will create with defaults on Save().", filePath.string());
            return false;
        }

        String rawText = FileManager::Open(filePath);
        if (rawText.empty()) return false;

        String currentSection = "General";
        size_t pos = 0;

        while (pos < rawText.size()) {
            size_t nextLine = rawText.find('\n', pos);
            if (nextLine == String::npos) nextLine = rawText.size();

            String line = Trim(StringView(rawText).substr(pos, nextLine - pos));
            pos = nextLine + 1;

            // Skip empty lines and comments (# or ;)
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;

            // Check for [Section] header
            if (line.front() == '[' && line.back() == ']') {
                currentSection = Trim(StringView(line).substr(1, line.size() - 2));
                continue;
            }

            // Parse Key = Value
            size_t eqPos = line.find('=');
            if (eqPos != String::npos) {
                String key = Trim(StringView(line).substr(0, eqPos));
                String val = Trim(StringView(line).substr(eqPos + 1));
                if (!key.empty()) {
                    s_Data[currentSection][key] = val;
                }
            }
        }

        FileManager::Close(rawText);
        LOG(Info, LogLevel::Core, "ConfigManager::Load loaded settings from {}", filePath.string());
        return true;
    }

    // Saves all current settings in memory to disk as a clean, human-readable .ini file!
    // Example: Config::Save();
    static inline BOOL Save(const FilePath &filePath = "") {
        if (!filePath.empty()) {
            s_CurrentFile = filePath;
        }

        String output;
        for (const auto &[sectionName, keys] : s_Data) {
            output += StringFormat("[{}]\n", sectionName);
            for (const auto &[key, val] : keys) {
                output += StringFormat("{} = {}\n", key, val);
            }
            output += "\n";
        }

        BOOL ok = FileManager::Write(s_CurrentFile, output, true);
        if (ok) {
            LOG(Info, LogLevel::Core, "ConfigManager::Save saved settings to {}", s_CurrentFile.string());
        }
        return ok;
    }

    // ========================================================================
    // Set (Update BOOL, INT, FLOAT, or String in memory)
    // ========================================================================

    // Sets a String setting under [section].
    // Example: Config::Set("Network", "ServerIP", "127.0.0.1");
    static inline void Set(StringView section, StringView key, StringView value) {
        s_Data[String(section)][String(key)] = String(value);
    }

    // Sets a const Char* string setting under [section].
    static inline void Set(StringView section, StringView key, const Char *value) {
        s_Data[String(section)][String(key)] = String(value);
    }

    // Sets a BOOL setting ("true" / "false") under [section].
    // Example: Config::Set("Window", "Fullscreen", true);
    static inline void Set(StringView section, StringView key, BOOL value) {
        s_Data[String(section)][String(key)] = value ? "true" : "false";
    }

    // Sets an INT setting under [section].
    // Example: Config::Set("Window", "Width", 1920);
    static inline void Set(StringView section, StringView key, INT value) {
        s_Data[String(section)][String(key)] = StringFormat("{}", value);
    }

    // Sets a FLOAT setting under [section].
    // Example: Config::Set("Input", "Sensitivity", 1.5f);
    static inline void Set(StringView section, StringView key, FLOAT value) {
        s_Data[String(section)][String(key)] = StringFormat("{:.4f}", value);
    }

    // ========================================================================
    // Get (Reads setting; if missing, auto-fills with defaultValue!)
    // ========================================================================

    // Gets a String setting. If missing from the file, sets and returns 'defaultValue'!
    // Example: String ip = Config::Get("Network", "ServerIP", "127.0.0.1");
    static inline String Get(StringView section, StringView key, StringView defaultValue) {
        String secStr(section);
        String keyStr(key);
        auto &secMap = s_Data[secStr];
        auto it = secMap.find(keyStr);
        if (it == secMap.end()) {
            secMap[keyStr] = String(defaultValue);
            return String(defaultValue);
        }
        return it->second;
    }

    // Overload so passing a literal "string" doesn't get converted to BOOL!
    static inline String Get(StringView section, StringView key, const Char *defaultValue) {
        return Get(section, key, StringView(defaultValue));
    }

    // Gets a BOOL setting (true / false / 1 / 0). If missing, sets and returns 'defaultValue'!
    // Example: BOOL bFullscreen = Config::Get("Window", "Fullscreen", false);
    static inline BOOL Get(StringView section, StringView key, BOOL defaultValue) {
        String raw = Get(section, key, StringView(defaultValue ? "true" : "false"));
        return (raw == "true" || raw == "True" || raw == "TRUE" || raw == "1");
    }

    // Gets an INT setting. If missing, sets and returns 'defaultValue'!
    // Example: INT width = Config::Get("Window", "Width", 1280);
    static inline INT Get(StringView section, StringView key, INT defaultValue) {
        String raw = Get(section, key, StringView(StringFormat("{}", defaultValue)));
        try {
            return std::stoi(raw);
        } catch (...) {
            return defaultValue;
        }
    }

    // Gets a FLOAT setting. If missing, sets and returns 'defaultValue'!
    // Example: FLOAT sens = Config::Get("Input", "Sensitivity", 1.0f);
    static inline FLOAT Get(StringView section, StringView key, FLOAT defaultValue) {
        String raw = Get(section, key, StringView(StringFormat("{:.4f}", defaultValue)));
        try {
            return std::stof(raw);
        } catch (...) {
            return defaultValue;
        }
    }
};

using Config   = ConfigManager; // Short alias (Config::Load, Config::Get, Config::Set, Config::Save)
using M_Config = ConfigManager; // Prefix alias (type M_ in autocomplete to see all Managers!)
