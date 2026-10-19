#pragma once

#include <pch.h>
#include <U_Variables.h>

// ============================================================================
// Game Math Utilities (Math:: for Basics, Math::Extended:: for 2D/3D Game Math)
// ============================================================================
class Math {
private:
    // Internal random generator (hidden from autocomplete!)
    static inline std::mt19937 &RandomEngine() {
        thread_local std::mt19937 engine(std::random_device{}());
        return engine;
    }

public:

    // ========================================================================
    // Constants (Math::PI, Math::DegToRad, etc.)
    // ========================================================================

    // Mathematical constant PI (Half circle in radians = 3.14159265)
    static constexpr FLOAT PI = 3.14159265f;
    // Full circle in radians (2 * PI = 6.2831853)
    static constexpr FLOAT TwoPI = PI * 2.0f;
    // Quarter circle / 90 degrees in radians (PI / 2 = 1.5707963)
    static constexpr FLOAT HalfPI = PI / 2.0f;

    // Full circle / 360 degrees
    static constexpr FLOAT Degrees_Full = 360.0f;
    // Half circle / 180 degrees
    static constexpr FLOAT Degrees_Half = 180.0f;
    // Quarter circle / 90 degrees
    static constexpr FLOAT Degrees_Quarter = 90.0f;

    // Multiplier to convert Degrees -> Radians
    static constexpr FLOAT DegToRad = PI / Degrees_Half;
    // Multiplier to convert Radians -> Degrees
    static constexpr FLOAT RadToDeg = Degrees_Half / PI;

    // Positive Infinity representation
    static constexpr FLOAT Infinity = std::numeric_limits<FLOAT>::infinity();

    // ========================================================================
    // Math Basic Enums
    // ========================================================================

    // Rounding mode for Math::Round(...)
    enum RoundType : BYTE {
        Default = 0, // Round to nearest whole number (3.4 -> 3.0, 3.6 -> 4.0)

        Up = 1,     // Round UP to nearest whole number (3.1 -> 4.0)
        Ceil = 1,   // Alias for Up (3.1 -> 4.0)
		Roof = 1,   // Alias for Up (3.1 -> 4.0)

        Down = 2,   // Round DOWN to nearest whole number (3.9 -> 3.0)
        Floor = 2,  // Alias for Down (3.9 -> 3.0)
        Ground = 2  // Alias for Down (3.9 -> 3.0)
    };

    // ========================================================================
    // Basic Math, Waves & Rounding (Math::Abs, Math::Clamp, Math::Sin, etc.)
    // ========================================================================

    // Turns any negative number into a positive number (-5 becomes +5).
    // Example: FLOAT heightDiff = Math::Abs(myHeight - enemyHeight);
    template<typename T>
    static constexpr T Absolute(T value) { return (value < 0) ? -value : value; }

    // Rounds a decimal to the closest whole number (3.4 -> 3.0, 3.6 -> 4.0).
    // Example: FLOAT displayDamage = Math::Round(rawDamage);
    static inline FLOAT Round(FLOAT value, RoundType type) {
        switch (type) {
            case RoundType::Ceil:  return std::ceil(value);
            case RoundType::Floor: return std::floor(value);
            default:    return std::round(value);
        }
    }

    // Calculates the square root of a number (for example: SquareRoot(9) = 3, SquareRoot(16) = 4).
    // Example: FLOAT dist = Math::SquareRoot(dx * dx + dz * dz);
    static inline FLOAT SquareRoot(FLOAT value) { return std::sqrt(value); }

    // Keeps a value from dropping BELOW minVal (e.g. HP never goes below 0).
    // Example: hp = Math::Clamp_Min(hp, 0.0f);
    template<typename T>
    static constexpr T Clamp_Min(T value, T minVal) {
        return (value < minVal) ? minVal : value;
    }
    // Keeps a value from going ABOVE maxVal (e.g. Damage never goes above 9999).
    // Example: dmg = Math::Clamp_Max(dmg, 9999);
    template<typename T>
    static constexpr T Clamp_Max(T value, T maxVal) {
        return (value > maxVal) ? maxVal : value;
    }
    // Keeps a value trapped BETWEEN [minVal, maxVal].
    // Example: hp = Math::Clamp_MinMax(hp, 0.0f, maxHp);
    template<typename T>
    static constexpr T Clamp_MinMax(T value, T minVal, T maxVal) {
        return (value < minVal) ? minVal : ((value > maxVal) ? maxVal : value);
    }

    // Horizontal wave between -1.0 and +1.0 (input in radians). Used with Sin for circular movement.
    // Example: FLOAT dirX = Math::Radians_Cos(yawRadians);
    static inline FLOAT Radians_Cos(FLOAT radians) { return std::cos(radians); }

    // Smooth wave between -1.0 and +1.0 (input in radians). Great for floating/bobbing animations!
    // Example: FLOAT hoverY = Math::Radians_Sin(gameTime * 2.0f) * 0.5f;
    static inline FLOAT Radians_Sin(FLOAT radians) { return std::sin(radians); }

    // Tangent ratio of an angle (input in radians). Used for 3D camera Field-of-View (FOV) slope.
    // Example: FLOAT fovSlope = Math::Radians_Tan(fovRadians * 0.5f);
    static inline FLOAT Radians_Tan(FLOAT radians) { return std::tan(radians); }

    // ========================================================================
    // Math::Extended:: (2D/3D Vectors, Angles, Distances, Random, Lerp)
    // ========================================================================
    class Extended {
    public:
        // ========================================================================
        // Math Extended Enums
        // ========================================================================

    // Conversion mode for Angle_Convert(...)
        enum AngleUnit : BYTE {
            ToDegrees = 0, // Converts Radians -> Degrees (0 to 360)
            ToRadians = 1, // Converts Degrees -> Radians
            Wrap180 = 2  // Keeps Degrees cleanly between -180 and +180
        };

        // Distance mode for 3D vectors in Get_Distance(...)
        enum DistanceType : BYTE {
            _2D = 0, // Ground distance only (ignores height Y)
            _3D = 1  // Exact 3D distance (includes height Y)
        };

        // Movement mode for Lerp(...)
        enum LerpType : BYTE {
            Smooth = 0, // Smoothly slows down as it approaches target
            Constant = 1  // Moves at a steady step speed (never overshoots!)
        };

        // Local ship/character direction for Vector_Axis(...)
        enum VectorAxis : BYTE {
            Forward = 0, // Where nose is pointing ('W' / shoot forward)
            Right = 1, // Where right wing is pointing ('D' strafe right)
            Up = 2  // Where roof is pointing ('Space' fly up)
        };

        // ---- Angle_... (Convert, Facing Check, LookAt for 2D & 3D) ----

        // Converts or wraps an angle based on AngleUnit (ToDegrees, ToRadians, Wrap180).
        // Example: FLOAT rad = Math::Extended::Angle_Convert(90.0f, Math::Extended::ToRadians);
        // Example: shipYaw   = Math::Extended::Angle_Convert(shipYaw, Math::Extended::Wrap180);
        static inline FLOAT Angle_Convert(FLOAT value, AngleUnit unit) {
            switch (unit) {
                case AngleUnit::ToDegrees: return value * RadToDeg;
                case AngleUnit::ToRadians: return value * DegToRad;
                case AngleUnit::Wrap180: {
                    FLOAT wrapped = std::fmod(value + Degrees_Half, Degrees_Full);
                    if (wrapped < 0.0f) wrapped += Degrees_Full;
                    return wrapped - Degrees_Half;
                }
                default: return value;
            }
        }

        // Compares two 2D directions: +1.0 = same way, 0.0 = 90-deg sideways, -1.0 = opposite!
        static constexpr FLOAT Angle_Facing(const Vector2D &dirA, const Vector2D &dirB) {
            return dirA.x * dirB.x + dirA.y * dirB.y;
        }

        // Compares two 3D directions: +1.0 = facing same way, 0.0 = 90-deg sideways, -1.0 = behind you!
        // Example: if (Math::Extended::Angle_Facing(shipForward, dirToEnemy) > 0.8f) { /* In radar cone! */ }
        static constexpr FLOAT Angle_Facing(const Vector3D &dirA, const Vector3D &dirB) {
            return dirA.x * dirB.x + dirA.y * dirB.y + dirA.z * dirB.z;
        }

        // Returns the 2D angle (in Degrees) needed at 'fromPos' to point at 'toPos'.
        static inline FLOAT Angle_LookAt(const Vector2D &fromPos, const Vector2D &toPos) {
            FLOAT dx = toPos.x - fromPos.x;
            FLOAT dy = toPos.y - fromPos.y;
            return Angle_Convert(std::atan2(dx, dy), ToDegrees);
        }

        // Returns the horizontal 3D angle (in Degrees) needed at 'fromPos' to look at 'toPos'.
        // Example: FLOAT turretYaw = Math::Extended::Angle_LookAt(turretPos, playerPos);
        static inline FLOAT Angle_LookAt(const Vector3D &fromPos, const Vector3D &toPos) {
            FLOAT dx = toPos.x - fromPos.x;
            FLOAT dz = toPos.z - fromPos.z;
            return Angle_Convert(std::atan2(dx, dz), ToDegrees);
        }

        // ---- Get_... (Distances & Random Numbers) ----

        // Returns the distance between two single numbers (INT or FLOAT).
        // Example: FLOAT heightDiff = Math::Extended::Get_Distance(myY, groundY);
        template<typename T>
        static constexpr T Get_Distance(T a, T b) {
            return Absolute(a - b);
        }

        // Returns 2D distance between two Vector2D points (UI, screen, radar).
        static inline FLOAT Get_Distance(const Vector2D &a, const Vector2D &b) {
            FLOAT dx = b.x - a.x;
            FLOAT dy = b.y - a.y;
            return SquareRoot(dx * dx + dy * dy);
        }

        // Returns 3D distance between two Vector3D positions (Flat2D ignores height Y, Full3D includes Y).
        // Example: FLOAT dist = Math::Extended::Get_Distance(playerPos, monsterPos, Math::Extended::Full3D);
        static inline FLOAT Get_Distance(const Vector3D &a, const Vector3D &b, DistanceType type) {
            FLOAT dx = b.x - a.x;
            FLOAT dz = b.z - a.z;
            if (type == DistanceType::_2D) return SquareRoot(dx * dx + dz * dz);
            FLOAT dy = b.y - a.y;
            return SquareRoot(dx * dx + dy * dy + dz * dz);
        }

        // Returns a random whole number between [minVal, maxVal].
        // Example: INT dmg = Math::Extended::Get_Random(10, 25);
        static inline INT Get_Random(INT minVal, INT maxVal) {
            if (minVal > maxVal) std::swap(minVal, maxVal);
            std::uniform_int_distribution<INT> dist(minVal, maxVal);
            return dist(RandomEngine());
        }

        // Returns a random decimal number between [minVal, maxVal].
        // Example: FLOAT spread = Math::Extended::Get_Random(-1.5f, 1.5f);
        static inline FLOAT Get_Random(FLOAT minVal, FLOAT maxVal) {
            if (minVal > maxVal) std::swap(minVal, maxVal);
            std::uniform_real_distribution<FLOAT> dist(minVal, maxVal);
            return dist(RandomEngine());
        }

        // ---- Vector_... (Axis, Direction, Length, Normalize for 2D & 3D) ----

        // Returns the local 3D direction arrow (Forward, Right, or Up) from Yaw & Pitch degrees.
        // Example: Vector3D forward = Math::Extended::Vector_Axis(Math::Extended::Forward, shipYaw, shipPitch);
        // Example: Vector3D right   = Math::Extended::Vector_Axis(Math::Extended::Right, shipYaw);
        static inline Vector3D Vector_Axis(VectorAxis axis, FLOAT yawDeg, FLOAT pitchDeg = 0.0f) {
            FLOAT y = Angle_Convert(yawDeg, AngleUnit::ToRadians);
            if (axis == VectorAxis::Right) {
                return { Radians_Cos(y), 0.0f, -Radians_Sin(y) };
            }
            FLOAT p = Angle_Convert(pitchDeg, AngleUnit::ToRadians);
            FLOAT cosP = Radians_Cos(p);
            FLOAT sinP = Radians_Sin(p);
            if (axis == VectorAxis::Up) {
                return { Radians_Sin(y) * sinP, cosP, Radians_Cos(y) * sinP };
            }
            // Forward
            return { Radians_Sin(y) * cosP, -sinP, Radians_Cos(y) * cosP };
        }

        // Returns the length (magnitude/speed) of a 2D vector.
        static inline FLOAT Vector_Length(const Vector2D &v) {
            return SquareRoot(v.x * v.x + v.y * v.y);
        }

        // Returns the length (magnitude/speed) of a 3D vector.
        // Example: FLOAT currentSpeed = Math::Extended::Vector_Length(playerVelocity);
        static inline FLOAT Vector_Length(const Vector3D &v) {
            return SquareRoot(v.x * v.x + v.y * v.y + v.z * v.z);
        }

        // Shrinks or stretches a Vector2D arrow so its length is 1.0.
        static inline Vector2D Vector_Normalize(const Vector2D &v) {
            FLOAT len = Vector_Length(v);
            if (len <= 0.000001f) return { 0.0f, 0.0f };
            FLOAT invLen = 1.0f / len;
            return { v.x * invLen, v.y * invLen };
        }

        // Shrinks or stretches a Vector3D arrow so its length is 1.0 (stops diagonal speed boost!).
        // Example: Vector3D moveDir = Math::Extended::Vector_Normalize(inputVector);
        static inline Vector3D Vector_Normalize(const Vector3D &v) {
            FLOAT len = Vector_Length(v);
            if (len <= 0.000001f) return { 0.0f, 0.0f, 0.0f };
            FLOAT invLen = 1.0f / len;
            return { v.x * invLen, v.y * invLen, v.z * invLen };
        }

        // Returns a length-1.0 2D direction arrow pointing from 'fromPos' to 'toPos'.
        static inline Vector2D Vector_Direction(const Vector2D &fromPos, const Vector2D &toPos) {
            return Vector_Normalize(toPos - fromPos);
        }

        // Returns a length-1.0 3D direction arrow pointing from 'fromPos' straight at 'toPos'.
        // Example: Vector3D bulletDir = Math::Extended::Vector_Direction(gunPos, enemyPos);
        static inline Vector3D Vector_Direction(const Vector3D &fromPos, const Vector3D &toPos) {
            return Vector_Normalize(toPos - fromPos);
        }

        // ---- Lerp_... (Smooth Blending & Constant-Speed Movement — Built-in DeltaTime!) ----

        // Moves or blends a number (INT or FLOAT) toward 'end' using Smooth or Constant speed.
        // Example: curSpeed = Math::Extended::Lerp(curSpeed, targetSpeed, accel, dt, Math::Extended::Constant);
        template<typename T>
        static constexpr T Lerp(T start, T end, FLOAT speed, FLOAT deltaTime, LerpType type) {
            if (type == LerpType::Constant) {
                T maxStep = static_cast<T>(speed * deltaTime);
                if (Absolute(end - start) <= maxStep) return end;
                return start + ((end > start) ? maxStep : -maxStep);
            }
            FLOAT alpha = Clamp_MinMax(speed * deltaTime, 0.0f, 1.0f);
            return static_cast<T>(start + (end - start) * alpha);
        }

        // Moves or blends a 2D position toward 'end' using Smooth or Constant speed.
        static inline Vector2D Lerp(const Vector2D &start, const Vector2D &end, FLOAT speed, FLOAT deltaTime, LerpType type) {
            if (type == LerpType::Constant) {
                FLOAT maxStep = speed * deltaTime;
                Vector2D diff = end - start;
                FLOAT dist = Vector_Length(diff);
                if (dist <= maxStep || dist <= 0.000001f) return end;
                return start + (diff * (maxStep / dist));
            }
            FLOAT alpha = Clamp_MinMax(speed * deltaTime, 0.0f, 1.0f);
            return start + (end - start) * alpha;
        }

        // Moves or blends a 3D position toward 'end' using Smooth or Constant speed.
        // Example: playerPos  = Math::Extended::Lerp(playerPos, serverPos, 10.0f, dt, Math::Extended::Smooth);
        // Example: missilePos = Math::Extended::Lerp(missilePos, enemyPos, 50.0f, dt, Math::Extended::Constant);
        static inline Vector3D Lerp(const Vector3D &start, const Vector3D &end, FLOAT speed, FLOAT deltaTime, LerpType type) {
            if (type == LerpType::Constant) {
                FLOAT maxStep = speed * deltaTime;
                Vector3D diff = end - start;
                FLOAT dist = Vector_Length(diff);
                if (dist <= maxStep || dist <= 0.000001f) return end;
                return start + (diff * (maxStep / dist));
            }
            FLOAT alpha = Clamp_MinMax(speed * deltaTime, 0.0f, 1.0f);
            return start + (end - start) * alpha;
        }

        // Smoothly turns an angle toward targetDeg using the shortest turn direction (left vs right).
        // Example: currentYaw = Math::Extended::Lerp_Angle(currentYaw, targetYaw, 8.0f, deltaTime);
        static inline FLOAT Lerp_Angle(FLOAT startDeg, FLOAT targetDeg, FLOAT speed, FLOAT deltaTime) {
            FLOAT alpha = Clamp_MinMax(speed * deltaTime, 0.0f, 1.0f);
            FLOAT diff = Angle_Convert(targetDeg - startDeg, Wrap180);
            return Angle_Convert(startDeg + diff * alpha, Wrap180);
        }
    };
};

using MathEx = Math::Extended; // Alias for easier access to Math::Extended functions