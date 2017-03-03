#pragma once

#include <mutex>

#define USE_MUTEX

#ifdef USE_MUTEX
    #define LOCK(mut) mut.lock();
    #define UNLOCK(mut) mut.unlock();
    #define LOCK_INIT(name) std::mutex name;
#else
    #define LOCK(flag) while (flag.test_and_set(std::memory_order_acquire));
    #define UNLOCK(flag) flag.clear(std::memory_order_release);
    #define LOCK_INIT(name) std::atomic_flag name = ATOMIC_FLAG_INIT;
#endif


using LockType = std::mutex;
