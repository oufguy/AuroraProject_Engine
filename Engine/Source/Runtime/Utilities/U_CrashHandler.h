#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Log.h>
#include <U_FileManager.h>
#include <U_TimeManager.h>

// ============================================================================
// Cross-Platform Crash & Assert Handler (100% Pure C++23 — Windows & Linux)
// ============================================================================
class CrashHandler {
private:
    using SourceLoc  = std::source_location;
    using StackTrace = std::stacktrace;

    static inline FilePath s_CrashLogPath = "Logs/Crash.log";

    // Automatically turns "ClientCrash.log" into "ClientCrash_19.10.26_23.15.log"
    // (Note: Windows forbids ':' inside filenames, so it uses DD.MM.YY_HH.MM!)
    static inline FilePath BuildDatedLogPath() {
        FilePath folder = s_CrashLogPath.parent_path();
        String stem     = s_CrashLogPath.stem().string();
        String ext      = s_CrashLogPath.extension().string();
        if (ext.empty()) ext = ".log";

        String datedName = StringFormat("{}_{}{}", stem, TimeManager::Get_TimeStamp(true), ext);
        return folder.empty() ? FilePath(datedName) : (folder / datedName);
    }

    // Cleans function name (turns "AWO_Client!main+0x12" into just "main")
    static inline String CleanFunctionName(String func) {
        size_t bangPos = func.find('!');
        if (bangPos != String::npos) func = func.substr(bangPos + 1);
        size_t plusPos = func.find('+');
        if (plusPos != String::npos) func = func.substr(0, plusPos);
        return func;
    }

    // Finds the exact short File Name, Line, and Function where the crash happened (NO PC paths!)
    static inline String GetCrashLocation() {
        StackTrace trace = StackTrace::current();

        for (const auto &entry : trace) {
            String fullPath = entry.source_file();
            UINT64 line     = entry.source_line();
            if (fullPath.empty() || line == 0) continue;

            // Only grab just the file name (e.g. "Client.cpp") so PC folder paths are never shown!
            String shortFile = FilePath(fullPath).filename().string();

            // Skip internal CrashHandler and OS/CRT internal files (only match our Engine or Game files!)
            if (shortFile == "U_CrashHandler.h") continue;
            if (fullPath.find("Engine") == String::npos && fullPath.find("Game") == String::npos) continue;

            String func = CleanFunctionName(entry.description());
            return StringFormat(
                "File     : {} (Line {})\n"
                "Function : {}\n",
                shortFile, line, func
            );
        }

        return "Function : Unknown (Run with Debug symbols enabled to see File & Line)\n";
    }

    // Internal OS Signal callback (catches Null Pointer, Divide-by-Zero, Abort, etc.)
    static inline void SignalHandler(INT signalCode) {
        String crashType = "Unknown Hardware Crash";
        String plainWhat = "An unknown hardware fault stopped the program.";

        switch (signalCode) {
            case SIGSEGV:
                crashType = "Memory Access Crash (Null / Invalid Pointer)";
                plainWhat = "Code tried to read or write to an empty (nullptr) or deleted variable/pointer!";
                break;
            case SIGFPE:
                crashType = "Math Crash (Divide by Zero)";
                plainWhat = "Code tried to divide a number by 0 (or produced an invalid math result)!";
                break;
            case SIGILL:
                crashType = "CPU Instruction Crash";
                plainWhat = "CPU hit a corrupted instruction or bad function pointer!";
                break;
            case SIGABRT:
                crashType = "Emergency Abort Crash";
                plainWhat = "A fatal memory or library check forced the program to stop immediately!";
                break;
        }

        String report = StringFormat(
            "============================================================\n"
            "[FATAL CRASH REPORT]\n"
            "Type     : {}\n"
            "Why      : {}\n"
            "{}"
            "============================================================\n",
            crashType,
            plainWhat,
            GetCrashLocation()
        );

        LOG(Fatal, LogLevel::Core, "\n{}", report);
        FileManager::Write(BuildDatedLogPath(), report, true);

        std::_Exit(signalCode);
    }

    // Internal C++ Terminate callback (catches unhandled throw/exceptions)
    static inline void TerminateHandler() {
        String report = StringFormat(
            "============================================================\n"
            "[FATAL CRASH REPORT]\n"
            "Type     : Unhandled C++ Exception\n"
            "Why      : A C++ error was thrown (e.g. out-of-bounds or bad format) and never caught!\n"
            "{}"
            "============================================================\n",
            GetCrashLocation()
        );

        LOG(Fatal, LogLevel::Core, "\n{}", report);
        FileManager::Write(BuildDatedLogPath(), report, true);

        std::_Exit(1);
    }

public:
    // ========================================================================
    // Setup & Configuration
    // ========================================================================

    // Call ONCE at the very start of main() to catch crashes and write them to Crash.log!
    // Example (Client): Crash::Init("Logs/Crash.log");
    // Example (Server): Crash::Init("Logs/ServerCrash.log");
    static inline void Init(const FilePath &crashLogPath = "Logs/Crash.log") {
        s_CrashLogPath = crashLogPath;

        std::signal(SIGSEGV, SignalHandler);
        std::signal(SIGFPE,  SignalHandler);
        std::signal(SIGILL,  SignalHandler);
        std::signal(SIGABRT, SignalHandler);
        std::set_terminate(TerminateHandler);

        LOG(Info, LogLevel::Core, "Crash::Init registered crash handlers -> {}", s_CrashLogPath.string());
    }

    // ========================================================================
    // Manual Crash & Strict Safety Checks (Auto-captures File, Line, Function!)
    // ========================================================================

    // Immediately halts the engine, logs [FATAL], and writes the full report to Crash.log.
    // Automatically records the exact File, Line, and Function where Trigger() was called!
    // Example: Crash::Trigger("Vulkan GPU Device Lost!");
    static inline void Trigger(StringView reason, const SourceLoc loc = SourceLoc::current()) {
        String shortFile = FilePath(loc.file_name()).filename().string();

        String report = StringFormat(
            "============================================================\n"
            "[FATAL CRASH REPORT]\n"
            "Type     : Manual Engine Crash\n"
            "Why      : {}\n"
            "File     : {} (Line {})\n"
            "Function : {}\n"
            "============================================================\n",
            reason,
            shortFile,
            loc.line(),
            loc.function_name()
        );

        LOG(Fatal, LogLevel::Core, "\n{}", report);
        FileManager::Write(BuildDatedLogPath(), report, false);

        std::_Exit(1);
    }

    // Verifies that 'condition' is true. If FALSE, immediately triggers a full Crash report
    // with the exact File, Line, and Function where the check failed!
    // Example: Crash::Check(playerPtr != nullptr, "Player pointer was NULL during spawn!");
    static inline void Check(BOOL condition, StringView reason, const SourceLoc loc = SourceLoc::current()) {
        if (!condition) {
            Trigger(reason, loc);
        }
    }
};