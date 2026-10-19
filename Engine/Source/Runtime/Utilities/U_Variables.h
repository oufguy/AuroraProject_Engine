#pragma once

#include <pch.h>

// ============================================================================
// Primitive Number Types (Clean, Explicit Sizing)
// ============================================================================

// Boolean (true / false). Example: BOOL bIsFlying = true;
using BOOL = bool;

// 1-Byte Unsigned (0 to 255) — Raw packet/file bytes, colors, level.
// Use int8 for signed 1-byte values (-128 to +127).
using BYTE = uint8_t;
// 2-Byte Signed (-32,768 to +32,767) — Small stats, elemental resistances, grid coords.
// Use uint16 for unsigned 2-byte values (0 to 65,535) like packet opcodes or Map IDs.
using SHORT = int16_t;

// 4-Byte Signed (-2.1B to +2.1B) — Default for HP, MP, damage, item counts, loops.
// Use DWORD for unsigned 4-byte values (0 to 4.2B) like bitflags.
using INT = int32_t;
// 4-Byte (0 to 4.2B). Example: DWORD bitFlags = 0xFF00;
using DWORD = uint32_t;
// 8-Byte Signed (up to 9.2 Quintillion) — Player Gold/SPI, Total EXP, database Account IDs.
// Use uint64 for unsigned 8-byte values (0 to 18.4 Quintillion) like file hashes.
using INT64 = int64_t;

// 4-Byte Decimal (~7 digits of precision) — World positions, movement speed, delta time.
// Use DOUBLE when you need high-precision timers that never lose accuracy over days.
using FLOAT = float;
// 8-Byte Decimal (~15-17 digits of precision) — High-precision server clock, long-running timers.
// Use FLOAT for normal 3D math, positions, and rendering.
using DOUBLE = double;

// 1 byte (-128 to 127).
// Use uint8 for unsigned 1-byte values (0 to 255) like raw packet bytes or colors.
using INT8 = int8_t;
// 2 bytes (0 to 65,535). Example: uint16 packetOpcode = 0x1001;
// Use int16 for signed 2-byte values (-32,768 to +32,767) like small stats or grid coords.
using UINT16 = uint16_t;
// 8-Byte (0 to 18 Quintillion). Example: uint64 fileHash = 0;
// Use int64 for signed 8-byte values (-9 Quintillion to +9 Quintillion) like player gold or total EXP.
using UINT64 = uint64_t;

// ============================================================================
// Text, Characters & Strings
// ============================================================================

// Standard 1-byte character. Example: Char inputKey = 'W'; // Keyboard key, delimiter
using Char = char;
// Wide 2-byte Unicode character. Example: WChar asianGlyph = L'仙'; // Asian fantasy character glyph
using WChar = wchar_t;

// Dynamic text string. Example: String playerName = "Li Wei"; // Character name, chat message
using String = std::string;
// Wide Unicode text string. Example: WString title = L"诛仙在线"; // Localized Asian fantasy text
using WString = std::wstring;
// Fast read-only string view (zero memory allocation). Example: void SendChat(StringView msg); // Function parameter
using StringView = std::string_view;
// Fast read-only wide string view (zero memory allocation). Example: void SetTitle(WStringView title);
using WStringView = std::wstring_view;

// Formats text with placeholders. Example: String msg = StringFormat("Player {} spawned at {}", name, pos);
template<typename... Args>
inline String StringFormat(std::format_string<Args...> fmt, Args&&... args) {
    return std::format(fmt, std::forward<Args>(args)...);
}

// ============================================================================
// Vectors & Colors
// ============================================================================

// 2D Vector (x, y). Example: Vector2D uvCoord = { 0.5f, 0.5f }; // Screen UI coords, texture UV
struct Vector2D {
    FLOAT x = 0.0f;
    FLOAT y = 0.0f;

    constexpr Vector2D operator+(const Vector2D &o) const { return { x + o.x, y + o.y }; }
    constexpr Vector2D operator-(const Vector2D &o) const { return { x - o.x, y - o.y }; }
    constexpr Vector2D operator*(FLOAT s) const { return { x * s, y * s }; }
};
// 3D Vector (x, y, z). Example: Vector3D playerPos = { 100.0f, 20.0f, -50.0f }; // World position, rotation, scale
struct Vector3D {
    FLOAT x = 0.0f;
    FLOAT y = 0.0f;
    FLOAT z = 0.0f;

    constexpr Vector3D operator+(const Vector3D &o) const { return { x + o.x, y + o.y, z + o.z }; }
    constexpr Vector3D operator-(const Vector3D &o) const { return { x - o.x, y - o.y, z - o.z }; }
    constexpr Vector3D operator*(FLOAT s) const { return { x * s, y * s, z * s }; }
};
// 4D Vector (x, y, z, w). Example: Vector4D tintColor = { 1.0f, 0.8f, 0.2f, 1.0f }; // RGBA colors, quaternions
struct Vector4D {
    FLOAT x = 0.0f;
    FLOAT y = 0.0f;
    FLOAT z = 0.0f;
    FLOAT w = 0.0f;
};

// RGBA Color (0 to 255). Works with { R, G, B } or HEX (0xRRGGBB)!
// Example: Color red = { 255, 0, 0 };  or  Color green = 0x2ECC71;
struct COLOR {
    BYTE r = 255, g = 255, b = 255, a = 255;
    constexpr COLOR() = default;
    constexpr COLOR(BYTE R, BYTE G, BYTE B, BYTE A = 255) : r(R), g(G), b(B), a(A) {}
    constexpr COLOR(DWORD hex, BYTE A = 255) : r(BYTE(hex >> 16)), g(BYTE(hex >> 8)), b(BYTE(hex)), a(A) {}
    // Allows COLOR("#2ECC71") so editor Color Picker UIs can click & edit it!
    constexpr COLOR(StringView hex, BYTE A = 255) : a(A) {
        auto Hex = [](Char c) constexpr -> BYTE {
            return (c >= '0' && c <= '9') ? BYTE(c - '0') :
                (c >= 'a' && c <= 'f') ? BYTE(c - 'a' + 10) :
                (c >= 'A' && c <= 'F') ? BYTE(c - 'A' + 10) : 0;
        };
        if (!hex.empty() && hex[0] == '#') hex.remove_prefix(1);
        if (hex.size() >= 6) {
            r = BYTE((Hex(hex[0]) << 4) | Hex(hex[1]));
            g = BYTE((Hex(hex[2]) << 4) | Hex(hex[3]));
            b = BYTE((Hex(hex[4]) << 4) | Hex(hex[5]));
        }
    }
};

// ============================================================================
// C++20 std::formatter Specializations for DirectX Vectors
// Enables direct logging with {}: LOG_INFO("Position: {}", myVector3D);
// ============================================================================

template<>
struct std::formatter<Vector2D> : std::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const Vector2D &v, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "({:.2f}, {:.2f})", v.x, v.y);
    }
};
template<>
struct std::formatter<Vector3D> : std::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const Vector3D &v, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f})", v.x, v.y, v.z);
    }
};
template<>
struct std::formatter<Vector4D> : std::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const Vector4D &v, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f}, {:.2f})", v.x, v.y, v.z, v.w);
    }
};

// ============================================================================
// Game Utilities
// ============================================================================

// Dynamic heap array (std::vector). Example: Array&lt;uint32&gt; learnedSkillIds; // Resizable list of skills, monsters
template<typename T>
using Array = std::vector<T>;
// Fixed-size stack array (std::array). Example: fArray&lt;FLOAT, 4&gt; quat; // Bone rotations, fixed 4-element buffers
template<typename T, size_t N>
using fArray = std::array<T, N>;

// Sorted map by key (std::map, O(log N)). Example: Map&lt;uint32, String&gt; itemNames; // Leaderboards, sorted data
template<typename Key, typename Value>
using Map = std::map<Key, Value>;
// Fast hash map (std::unordered_map, O(1)). Example: uMap&lt;uint64, PlayerData&gt; activePlayers; // Fast ID lookup
template<typename Key, typename Value>
using uMap = std::unordered_map<Key, Value>;

// Sorted unique set (std::set). Example: Set&lt;uint32&gt; completedQuestIds; // Unique, ordered IDs
template<typename T>
using Set = std::set<T>;
// Fast hash set (std::unordered_set). Example: uSet&lt;uint64&gt; onlineAccountIds; // Fast membership check
template<typename T>
using uSet = std::unordered_set<T>;

// Shared reference-counting smart pointer. Example: SharedPtr&lt;StaticObject&gt; swordMesh; // Shared game asset
template<typename T>
using SharedPtr = std::shared_ptr<T>;
// Unique single-owner smart pointer. Example: UniquePtr&lt;Camera&gt; mainCamera; // Single exclusive owner
template<typename T>
using UniquePtr = std::unique_ptr<T>;
// Weak non-owning reference to SharedPtr. Example: WeakPtr&lt;StaticObject&gt; currentTarget; // Selected target without keeping alive
template<typename T>
using WeakPtr = std::weak_ptr<T>;

// Creates a new SharedPtr instance. Example: auto sword = MakeShared&lt;StaticObject&gt;("FlyingSword");
template<typename T, typename... Args>
inline SharedPtr<T> MakeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}
// Creates a new UniquePtr instance. Example: auto cam = MakeUnique&lt;Camera&gt;();
template<typename T, typename... Args>
inline UniquePtr<T> MakeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}