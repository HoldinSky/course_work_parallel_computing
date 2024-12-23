#ifndef CW_MAIN_HELPER_H
#define CW_MAIN_HELPER_H

#include <mutex>
#include <shared_mutex>

#include <cstdint>
#include <chrono>

using rwLock = std::shared_mutex;
using sharedLock = std::shared_lock<rwLock>;
using exclusiveLock = std::unique_lock<rwLock>;

static rwLock stdoutLock;
static rwLock commonMutex;

static std::condition_variable_any commonMonitor;

struct Terminal {
    const char *const red = "\033[0;31m";
    const char *const green = "\033[0;32m";
    const char *const yellow = "\033[0;33m";
    const char *const blue = "\033[0;34m";
    const char *const magenta = "\033[0;35m";
    const char *const cyan = "\033[0;36m";
    const char *const white = "\033[0;37m";
    const char *const reset = "\033[0m";
};

static constexpr Terminal terminal{};

template<typename FT>
std::chrono::duration<int64_t, std::milli>
measureExecutionTime(FT func) {
    const auto start = std::chrono::high_resolution_clock::now();
    func();
    const auto end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

#endif // CW_MAIN_HELPER_H