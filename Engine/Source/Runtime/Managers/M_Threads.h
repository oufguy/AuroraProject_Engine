#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>

// ============================================================================
// Cross-Platform Thread & Job Manager (Thread Pool & Parallel Simulation)
// ============================================================================
// Features:
//   - Background Asset Loading (Textures, Meshes, Disk I/O without frame drops)
//   - Massive Parallel Simulation (2,500+ missiles, 1,000 monsters across CPU cores)
//   - Dedicated Named Threads (Continuous network sockets, audio streaming)
//   - Pure C++23 standard library (Zero external dependencies, 100% cross-platform)
// ============================================================================
// Quick Usage Examples:
//
// 1. Fire-and-forget background job:
//    Threads::Run([]() {
//        File::Save("PlayerData.dat", buffer);
//    });
//
// 2. Parallel loop across all CPU cores (e.g. 2,500 Missiles / 1,000 Monsters):
//    Threads::Parallel_For(0, 2500, [](UINT i) {
//        missiles[i].Update();
//    });
//
// 3. Persistent named worker thread:
//    Threads::Create_NamedThread("NetworkIO", []() {
//        while (running) { network.Poll(); }
//    });
// ============================================================================
class ThreadManager {
private:
    static inline Array<std::thread>      s_Workers;
    static inline std::queue<Task>        s_TaskQueue;
    static inline std::mutex              s_QueueMutex;
    static inline std::condition_variable s_CV;
    static inline std::condition_variable s_WaitAllCV;
    static inline std::atomic<UINT>       s_ActiveTasks{ 0 };
    static inline std::atomic<BOOL>       s_Stop{ false };
    static inline BOOL                    s_IsInitialized = false;
    static inline UINT                    s_ThreadCount = 0;

    // Worker thread loop
    static inline void WorkerLoop() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(s_QueueMutex);
                s_CV.wait(lock, [] {
                    return s_Stop.load() || !s_TaskQueue.empty();
                });

                if (s_Stop.load() && s_TaskQueue.empty()) {
                    return;
                }

                task = std::move(s_TaskQueue.front());
                s_TaskQueue.pop();
                s_ActiveTasks++;
            }

            // Execute the assigned task safely
            if (task) {
                try {
                    task();
                } catch (const std::exception &e) {
                    LOG(Error, LogLevel::Core, "ThreadManager: Worker task threw an exception: {}", e.what());
                } catch (...) {
                    LOG(Error, LogLevel::Core, "ThreadManager: Worker task threw an unknown exception!");
                }
            }

            {
                std::lock_guard<std::mutex> lock(s_QueueMutex);
                s_ActiveTasks--;
            }
            s_WaitAllCV.notify_all();
        }
    }

public:
    // ========================================================================
    // Lifecycle (Init & Shutdown)
    // ========================================================================

    // Initializes worker thread pool matching CPU core count.
    // If threadCount == 0, defaults to (Logical CPU Cores - 1), minimum 1.
    static inline BOOL Init(UINT threadCount = 0) {
        if (s_IsInitialized) return true;

        UINT hwCores = std::thread::hardware_concurrency();
        if (threadCount == 0) {
            threadCount = (hwCores > 1) ? (hwCores - 1) : 1;
        }

        s_Stop          = false;
        s_ThreadCount   = threadCount;
        s_ActiveTasks   = 0;

        s_Workers.reserve(threadCount);
        for (UINT i = 0; i < threadCount; ++i) {
            s_Workers.emplace_back(WorkerLoop);
        }

        s_IsInitialized = true;
        LOG(Info, LogLevel::Core, "ThreadManager: Initialized with [{}] worker threads on [{}] CPU cores.",
            s_ThreadCount, hwCores);
        return true;
    }

    // Gracefully finishes all tasks and joins all worker threads
    static inline void Shutdown() {
        if (!s_IsInitialized) return;

        s_Stop = true;
        s_CV.notify_all();

        for (auto &worker : s_Workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        s_Workers.clear();

        // Clear any remaining tasks in queue
        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            while (!s_TaskQueue.empty()) {
                s_TaskQueue.pop();
            }
        }

        s_IsInitialized = false;
        s_ThreadCount   = 0;
        LOG(Info, LogLevel::Core, "ThreadManager: Worker threads shutdown cleanly.");
    }

    // ========================================================================
    // Task Execution
    // ========================================================================

    // Runs a task asynchronously on a background worker thread (Fire & Forget)
    // Example: Threads::Run([]() { Texture::Load("Dragon.png"); });
    static inline void Run(Task task) {
        if (!s_IsInitialized) {
            if (task) {
                try {
                    task();
                } catch (const std::exception &e) {
                    LOG(Error, LogLevel::Core, "ThreadManager::Run (Fallback): Exception thrown: {}", e.what());
                } catch (...) {
                    LOG(Error, LogLevel::Core, "ThreadManager::Run (Fallback): Unknown exception thrown!");
                }
            }
            return;
        }

        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            s_TaskQueue.push(std::move(task));
        }
        s_CV.notify_one();
    }

    // Divides a range [start, end) across all CPU worker threads simultaneously.
    // Blocks the caller until all chunks have finished.
    // Example: Threads::Parallel_For(0, 2500, [](UINT i) { missiles[i].Update(); });
    static inline void Parallel_For(UINT start, UINT end, const Function<void(UINT)> &loopBody) {
        if (start >= end || !loopBody) return;
        UINT totalItems = end - start;

        // If thread pool is not active or work is tiny, run directly on current thread
        if (!s_IsInitialized || s_Workers.empty() || totalItems <= 1) {
            for (UINT i = start; i < end; ++i) {
                loopBody(i);
            }
            return;
        }

        UINT numWorkers = static_cast<UINT>(s_Workers.size());
        UINT chunkSize  = (totalItems + numWorkers - 1) / numWorkers;
        std::atomic<UINT> remainingChunks{ 0 };

        for (UINT i = start; i < end; i += chunkSize) {
            UINT chunkEnd = std::min(i + chunkSize, end);
            remainingChunks++;

            Run([i, chunkEnd, &loopBody, &remainingChunks]() {
                for (UINT j = i; j < chunkEnd; ++j) {
                    loopBody(j);
                }
                remainingChunks--;
            });
        }

        // Wait until all parallel chunks complete
        while (remainingChunks.load() > 0) {
            std::this_thread::yield();
        }
    }

    // Spawns a dedicated long-running thread with a descriptive name
    // Example: Threads::Create_NamedThread("NetworkIO", []() { while(running) { ... } });
    static inline void Create_NamedThread(StringView name, Task task) {
        std::thread t([nameStr = String(name), task = std::move(task)]() {
            LOG(Info, LogLevel::Core, "Thread [{}] started.", nameStr);
            if (task) {
                try {
                    task();
                } catch (const std::exception &e) {
                    LOG(Error, LogLevel::Core, "Thread [{}]: Exception thrown: {}", nameStr, e.what());
                } catch (...) {
                    LOG(Error, LogLevel::Core, "Thread [{}]: Unknown exception thrown!", nameStr);
                }
            }
            LOG(Info, LogLevel::Core, "Thread [{}] terminated.", nameStr);
        });
        t.detach();
    }

    // Waits until all queued tasks and active worker tasks have finished
    static inline void Wait_All() {
        if (!s_IsInitialized) return;
        std::unique_lock<std::mutex> lock(s_QueueMutex);
        s_WaitAllCV.wait(lock, [] {
            return s_TaskQueue.empty() && s_ActiveTasks.load() == 0;
        });
    }

    // ========================================================================
    // Status & Queries
    // ========================================================================

    static inline UINT Get_ThreadCount() {
        return s_ThreadCount;
    }

    static inline UINT Get_HardwareCores() {
        return std::thread::hardware_concurrency();
    }

    static inline UINT Get_ActiveTaskCount() {
        return s_ActiveTasks.load();
    }

    static inline UINT Get_PendingTaskCount() {
        std::lock_guard<std::mutex> lock(s_QueueMutex);
        return static_cast<UINT>(s_TaskQueue.size());
    }

    static inline BOOL Is_Initialized() {
        return s_IsInitialized;
    }
};

using Threads        = ThreadManager; // Standard short alias
using M_Threads      = ThreadManager; // Manager prefix alias
using Thread_Manager = ThreadManager; // Descriptive alias
