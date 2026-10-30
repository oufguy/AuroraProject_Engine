#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>
#include <M_File.h>

// ============================================================================
// Auto-Converting Config Value (Implicit cast to int, bool, float, String!)
// ============================================================================
struct ConfigValue {
    String Raw;

    operator String() const { return Raw; }
    operator StringView() const { return Raw; }
    operator const char*() const { return Raw.c_str(); }
    operator bool() const { return (Raw == "true" || Raw == "True" || Raw == "TRUE" || Raw == "1"); }
    operator int() const { try { return std::stoi(Raw); } catch (...) { return 0; } }
    operator unsigned int() const { try { return static_cast<unsigned int>(std::stoul(Raw)); } catch (...) { return 0; } }
    operator float() const { try { return std::stof(Raw); } catch (...) { return 0.0f; } }
    operator double() const { try { return std::stod(Raw); } catch (...) { return 0.0; } }
};

// ============================================================================
// Lightweight In-Memory Config (.ini / .cfg Key-Value Store)
// ============================================================================
// Knows NOTHING about your game. It only reads, stores and writes "Key = Value" lines.
// Your game decides which keys exist and what their defaults are.
//
// Example:
//   M_Config Settings;
//   Settings.Load("Settings.ini", "Width = 1280\nHeight = 720\n"); // 2nd arg = file written if missing
//   INT  width = Settings.Get("Width");
//   INT  newW  = Settings.Set("Width", 1920);
//   Settings.Save("Settings.ini");                                  // Keeps your # comments!
// ============================================================================
struct M_Config {
    Map<String, String> Values;

private:
    static inline String Trim(StringView text) {
        size_t start = text.find_first_not_of(" \t\r\n");
        if (start == StringView::npos) return "";
        size_t end = text.find_last_not_of(" \t\r\n");
        return String(text.substr(start, end - start + 1));
    }

    // Splits "Key = Value ; comment" -> key, value. Returns FALSE for blank / comment / [Section] lines.
    static inline BOOL ParseLine(StringView rawLine, String &key, String &value) {
        String line = Trim(rawLine);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') return false;

        size_t commentPos = line.find_first_of(";#");
        if (commentPos != String::npos) line = Trim(StringView(line).substr(0, commentPos));

        size_t eqPos = line.find('=');
        if (eqPos == String::npos) return false;

        key   = Trim(StringView(line).substr(0, eqPos));
        value = Trim(StringView(line).substr(eqPos + 1));
        return !key.empty();
    }

public:
    // Loads "Key = Value" lines from disk into RAM.
    // If the file is missing and 'defaultText' is given, the file is created from it first.
    BOOL Load(const FilePath &path, StringView defaultText = "") {
        if (!M_File::Find(path)) {
            if (defaultText.empty()) {
                LOG(Warn, LogLevel::Core, "M_Config::Load '{}' not found.", path.string());
                return false;
            }
            LOG(Warn, LogLevel::Core, "M_Config::Load '{}' not found - creating it from defaults.", path.string());
            M_File::Write(path, defaultText, true);
        }

        String rawText = M_File::Open(path);
        if (rawText.empty()) return false;

        Values.clear();
        size_t pos = 0;
        while (pos < rawText.size()) {
            size_t nextLine = rawText.find('\n', pos);
            if (nextLine == String::npos) nextLine = rawText.size();

            String key, value;
            if (ParseLine(StringView(rawText).substr(pos, nextLine - pos), key, value)) {
                Values[key] = value;
            }
            pos = nextLine + 1;
        }
        LOG(Info, LogLevel::Core, "M_Config::Load loaded {} settings from {}", Values.size(), path.string());
        return true;
    }

    // Saves RAM values to disk. Existing lines (and their comments) are kept in place,
    // only the values are updated. New keys are added at the bottom.
    BOOL Save(const FilePath &path) const {
        String oldText = M_File::Find(path) ? M_File::Open(path) : "";
        String output;
        ::Set<String> written; // '::' = the global Set<T> container, not M_Config::Set()

        size_t pos = 0;
        while (pos < oldText.size()) {
            size_t nextLine = oldText.find('\n', pos);
            if (nextLine == String::npos) nextLine = oldText.size();
            StringView line = StringView(oldText).substr(pos, nextLine - pos);
            pos = nextLine + 1;

            String key, oldValue;
            auto it = ParseLine(line, key, oldValue) ? Values.find(key) : Values.end();
            if (it == Values.end()) {
                output += String(line) + "\n"; // Comment, blank line or unknown key -> keep as-is
                continue;
            }

            // Replace only the value part, keep the inline comment if present
            size_t commentPos = line.find_first_of(";#");
            if (commentPos != StringView::npos) {
                output += StringFormat("{} = {}\t{}\n", key, it->second, line.substr(commentPos));
            } else {
                output += StringFormat("{} = {}\n", key, it->second);
            }
            written.insert(key);
        }

        for (const auto &[k, v] : Values) {
            if (!written.contains(k)) output += StringFormat("{} = {}\n", k, v);
        }
        return M_File::Write(path, output, true);
    }

    // Read-only: returns an auto-converting value (never changes the map)
    ConfigValue Get(StringView key) const {
        auto it = Values.find(String(key));
        if (it != Values.end()) return { it->second };
        return { "" };
    }

    // Writes to RAM and returns the newly set value
    template<typename T> requires (!std::is_convertible_v<T, StringView>)
    T Set(StringView key, const T &val) {
        if constexpr (std::is_same_v<T, bool>) {
            Values[String(key)] = val ? "true" : "false";
        } else {
            Values[String(key)] = StringFormat("{}", val);
        }
        return val;
    }

    // Text overload so Settings.Set("API", "Vulkan") works with plain quotes
    StringView Set(StringView key, StringView val) {
        Values[String(key)] = String(val);
        return val;
    }
};

