#pragma once

#ifndef REAL_RUN
    //#define LOAD_FROM_FILE "sig17starterpack/sample.load"
    //#define LOAD_FROM_FILE "web.load"
    #define LOAD_FROM_FILE "small.load"
#endif

//#define PRINT_STATISTICS

using DictHash = unsigned int;
#define HASH_NOT_FOUND ((DictHash) -1)

#define NO_JOB ((ssize_t) -1)
#define THREAD_POOL_THREAD_COUNT (3)
