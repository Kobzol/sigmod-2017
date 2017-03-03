#pragma once

#include <mutex>
#include <thread>
#include <chrono>

#define USE_MUTEX

#ifdef USE_MUTEX
    #define LOCK(mut) mut.lock();
    #define UNLOCK(mut) mut.unlock();
    #define LOCK_INIT(name) std::mutex name;
#else
    #define LOCK(flag) while (flag.test_and_set(std::memory_order_acquire)) { std::this_thread::sleep_for(std::chrono::nanoseconds(10)); };
    #define UNLOCK(flag) flag.clear(std::memory_order_release);
    #define LOCK_INIT(name) std::atomic_flag name = ATOMIC_FLAG_INIT;
#endif
