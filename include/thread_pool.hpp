#pragma once

// ─── Thread Pool ─────────────────────────────────────────────────────────────
// On Windows (old MinGW32 lacks std::thread), we use Win32 native primitives:
//   CreateThread / WaitForMultipleObjects / Semaphore / CRITICAL_SECTION
// On Linux/macOS, std::thread + std::mutex + std::condition_variable are used.
// ─────────────────────────────────────────────────────────────────────────────

#include <iostream>

#include <atomic>
#include <functional>
#include <queue>
#include <vector>

using namespace std;

#ifdef _WIN32
// ─── Win32 implementation ─────────────────────────────────────────────────────
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>

class ThreadPool {
public:
    explicit ThreadPool(int num_threads) : stop_(false) {
        InitializeCriticalSection(&cs_);
        // Semaphore counts available tasks; each enqueue releases 1.
        sem_ = CreateSemaphore(NULL, 0, 100000L, NULL);

        for (int i = 0; i < num_threads; ++i) {
            HANDLE h = CreateThread(NULL, 0, worker_proc, this, 0, NULL);
            if (h) workers_.push_back(h);
        }
    }

    ~ThreadPool() {
        stop_.store(true);
        // Wake every sleeping worker so they can check the stop flag and exit.
        if (!workers_.empty())
            ReleaseSemaphore(sem_, static_cast<LONG>(workers_.size()), NULL);

        if (!workers_.empty()) {
            WaitForMultipleObjects(static_cast<DWORD>(workers_.size()),
                                   workers_.data(), TRUE, INFINITE);
        }
        for (HANDLE h : workers_) CloseHandle(h);
        CloseHandle(sem_);
        DeleteCriticalSection(&cs_);
    }

    void enqueue(std::function<void()> task) {
        EnterCriticalSection(&cs_);
        tasks_.push(std::move(task));
        LeaveCriticalSection(&cs_);
        ReleaseSemaphore(sem_, 1, NULL);  // signal one task available
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    static DWORD WINAPI worker_proc(LPVOID param) {
        ThreadPool* pool = static_cast<ThreadPool*>(param);
        while (true) {
            WaitForSingleObject(pool->sem_, INFINITE);
            if (pool->stop_.load()) break;

            std::function<void()> task;
            EnterCriticalSection(&pool->cs_);
            if (!pool->tasks_.empty()) {
                task = std::move(pool->tasks_.front());
                pool->tasks_.pop();
            }
            LeaveCriticalSection(&pool->cs_);

            if (task) task();
        }
        return 0;
    }

    std::vector<HANDLE>               workers_;
    std::queue<std::function<void()>> tasks_;
    CRITICAL_SECTION                  cs_;
    HANDLE                            sem_;
    std::atomic<bool>                 stop_;
};

#else
// ─── POSIX implementation (Linux / macOS) ────────────────────────────────────
#  include <condition_variable>
#  include <mutex>
#  include <thread>

class ThreadPool {
public:
    explicit ThreadPool(int num_threads) : stop_(false) {
        for (int i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cv_.wait(lock, [this] {
                            return stop_.load() || !tasks_.empty();
                        });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        stop_.store(true);
        cv_.notify_all();
        for (auto& t : workers_)
            if (t.joinable()) t.join();
    }

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        mutex_;
    std::condition_variable           cv_;
    std::atomic<bool>                 stop_;
};

#endif  // _WIN32
