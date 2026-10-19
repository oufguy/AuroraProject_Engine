#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Math.h>

// ============================================================================
// Game & Server Time (DeltaTime, FPS, Uptime, Cooldowns, Sleep)
// ============================================================================
class TimeManager {
private:
    // Internal high-precision clock (steady_clock never jumps backward if OS clock changes!)
    using Clock     = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    static inline TimePoint s_StartTime     = Clock::now();
    static inline TimePoint s_LastFrameTime = Clock::now();

    static inline FLOAT  s_DeltaTime  = 0.0f;
    static inline DOUBLE s_TotalTime  = 0.0;
    static inline FLOAT  s_FPSTimer   = 0.0f;
    static inline INT    s_FrameCount = 0;
    static inline INT    s_CurrentFPS = 0;

public:
    // Time unit for Get_TotalTime(...)
    enum TimeUnit : BYTE {
        Seconds      = 0, // Total time in Seconds (e.g. 12.50s)
        Milliseconds = 1  // Total time in Milliseconds (e.g. 12500.0ms)
    };

    // ========================================================================
    // Core Frame / Tick Loop
    // ========================================================================

    // Call this ONCE at the very top of your Client or Server while(running) loop!
    // Automatically updates DeltaTime, TotalTime, and calculates FPS every 1 second.
    // Example: TimeHandler::Update();
    static inline void Update() {
        TimePoint now = Clock::now();

        // Calculate time passed since the previous frame in Seconds
        std::chrono::duration<FLOAT> frameDiff = now - s_LastFrameTime;
        s_LastFrameTime = now;

        // Cap DeltaTime at 0.1s (10 FPS minimum) so dragging the window never teleports players through walls!
        s_DeltaTime = Math::Clamp_Max(frameDiff.count(), 0.1f);

        // Calculate total uptime since engine start (uses DOUBLE so it never loses accuracy over days!)
        std::chrono::duration<DOUBLE> totalDiff = now - s_StartTime;
        s_TotalTime = totalDiff.count();

        // Count frames and update FPS once every 1.0 second
        s_FrameCount++;
        s_FPSTimer += frameDiff.count();
        if (s_FPSTimer >= 1.0f) {
            s_CurrentFPS = s_FrameCount;
            s_FrameCount = 0;
            s_FPSTimer  -= 1.0f;
        }
    }

    // Pauses the current thread for 'milliseconds' (Saves CPU in Server loops or caps FPS!).
    // Example: TimeHandler::Sleep(10);
    static inline void Sleep(DWORD milliseconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

    // ========================================================================
    // Get_... (DeltaTime, FPS, Total Uptime)
    // ========================================================================

    // Returns the time in Seconds between the last frame and this frame (e.g. 0.016f at 60 FPS).
    // Multiply all movement/speeds by this so 60 FPS and 240 FPS move at the exact same speed!
    // Example: FLOAT dt = TimeHandler::Get_DeltaTime();
    static inline FLOAT Get_DeltaTime() {
        return s_DeltaTime;
    }

    // Returns how many frames (Client) or ticks (Server) ran in the last second.
    // Example: INT fps = TimeHandler::Get_FPS();
    static inline INT Get_FPS() {
        return s_CurrentFPS;
    }

    // Returns total time elapsed since the Engine/Server started (Seconds or Milliseconds).
    // Example: DOUBLE timeSec = Time::Get_TotalTime(Time::Seconds);
    // Example: DOUBLE timeMS  = Time::Get_TotalTime(Time::Milliseconds);
    static inline DOUBLE Get_TotalTime(TimeUnit unit) {
        if (unit == Milliseconds) {
            return s_TotalTime * 1000.0;
        }
        return s_TotalTime;
    }

    // Returns current local Date & Time as "DD.MM.YY_HH.MM" (safe for filenames) or "DD.MM.YY HH:MM".
    // (Note: Windows forbids ':' inside filenames, so file mode uses '.' or '_' instead!)
    // Example: String stamp = Time::Get_TimeStamp(true); // "19.10.26_23.15"
    static inline String Get_TimeStamp(BOOL forFileName = true) {
        auto now = std::chrono::system_clock::now();
        std::time_t rawTime = std::chrono::system_clock::to_time_t(now);
        std::tm localTm{};
#ifdef _WIN32
        localtime_s(&localTm, &rawTime);
#else
        localtime_r(&rawTime, &localTm);
#endif
        INT day    = localTm.tm_mday;
        INT month  = localTm.tm_mon + 1;
        INT year   = (localTm.tm_year + 1900) % 100;
        INT hour   = localTm.tm_hour;
        INT minute = localTm.tm_min;

        if (forFileName) {
            return StringFormat("{:02}.{:02}.{:02}_{:02}.{:02}", day, month, year, hour, minute);
        }
        return StringFormat("{:02}.{:02}.{:02} {:02}:{:02}", day, month, year, hour, minute);
    }

    // ========================================================================
    // Cooldown / Timer Helper (Skills, Fire Rate, Monster Spawns)
    // ========================================================================

    // Checks if 'cooldownSeconds' has passed since 'lastUsedTime'.
    // If ready: automatically updates 'lastUsedTime' to NOW and returns true!
    // Example: if (Time::Check_Cooldown(lastMissileShotTime, 0.5)) { ShootMissile(); }
    static inline BOOL Check_Cooldown(DOUBLE &lastUsedTime, DOUBLE cooldownSeconds) {
        if ((s_TotalTime - lastUsedTime) >= cooldownSeconds) {
            lastUsedTime = s_TotalTime;
            return true;
        }
        return false;
    }
};