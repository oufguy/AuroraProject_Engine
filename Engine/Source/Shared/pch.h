#pragma once

// ============================================================================
// Console & Input/Output
// ============================================================================
#include <iostream>       // std::cout, std::cin, std::endl (Console printing & input)

// ============================================================================
// Numbers & Math
// ============================================================================
#include <cstdint>        // int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t
#include <cmath>          // std::sqrt, std::sin, std::cos, std::abs, std::pow (Math functions)
#include <limits>         // std::numeric_limits (Infinity, min/max values)
#include <random>         // std::mt19937, std::uniform_int_distribution, std::uniform_real_distribution
#include <algorithm>      // std::min, std::max, std::clamp, std::sort (Common algorithms)
#include <cstring>        // std::memcpy, std::memset, strcmp (Fast memory operations)

// ============================================================================
// Text & Formatting
// ============================================================================
#include <string>         // std::string, std::wstring (Dynamic text strings)
#include <string_view>    // std::string_view, std::wstring_view (Fast read-only text views)
#include <format>         // std::format, std::formatter (C++20 text formatting with "{}")

// ============================================================================
// Containers & Views
// ============================================================================
#include <vector>         // std::vector (Dynamic resizable array -> your Array<T>)
#include <array>          // std::array (Fixed-size stack array -> your fArray<T, N>)
#include <map>            // std::map (Sorted key-value table -> your Map<K, V>)
#include <unordered_map>  // std::unordered_map (Fast hash table -> your uMap<K, V>)
#include <set>            // std::set (Sorted unique list -> your Set<T>)
#include <unordered_set>  // std::unordered_set (Fast unique hash list -> your uSet<T>)
#include <span>           // std::span (Contiguous buffer view)
#include <optional>       // std::optional (Nullable values without pointers)

// ============================================================================
// Memory & Smart Pointers
// ============================================================================
#include <memory>         // std::shared_ptr, std::unique_ptr, std::weak_ptr, std::make_shared, std::make_unique

// ============================================================================
// Filesystem & File Streams
// ============================================================================
#include <filesystem>     // std::filesystem::path, exists, create_directories
#include <fstream>        // std::ifstream, std::ofstream (Reading & writing files)

// ============================================================================
// Time, Threading & Synchronization
// ============================================================================
#include <chrono>          // std::chrono::steady_clock, duration (High-precision DeltaTime & timers)
#include <thread>          // std::this_thread::sleep_for (Server tick rate / FPS limiter sleep)
#include <mutex>           // std::mutex, std::lock_guard (Thread safety locks)

// ============================================================================
// Diagnostics & Crash Handling
// ============================================================================
#include <csignal>         // std::signal, SIGSEGV, SIGFPE, SIGABRT (Hardware/memory crash catching)
#include <exception>       // std::set_terminate (Uncaught exception crash catching)
#include <source_location> // std::source_location (Auto-captures file, line, and function on crash)
#include <stacktrace>      // std::stacktrace (Auto-captures exact file, line & call stack on hardware crash!)

// ============================================================================
// Core Cross-Platform Hardware & Windowing (SDL3)
// ============================================================================
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// ============================================================================
// Modern Graphics API (Vulkan 1.3 / 1.4 Core)
// ============================================================================
#include <vulkan/vulkan.h>
