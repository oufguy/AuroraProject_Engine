#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>

// ============================================================================
// Network Manager (Non-blocking Cross-Platform UDP / TCP Layer)
// ============================================================================
// Optional module for multiplayer client/server communication.
// Single-player offline games never need to include or compile this.
// ============================================================================
class M_Network {
private:
    static inline BOOL s_IsInitialized = false;

public:
    static inline BOOL Init() {
        if (s_IsInitialized) return true;
        s_IsInitialized = true;
        LOG(Info, LogLevel::Core, "M_Network initialized.");
        return true;
    }

    static inline void Shutdown() {
        if (!s_IsInitialized) return;
        s_IsInitialized = false;
        LOG(Info, LogLevel::Core, "M_Network shutdown.");
    }

    static inline BOOL Is_Initialized() { return s_IsInitialized; }
};
