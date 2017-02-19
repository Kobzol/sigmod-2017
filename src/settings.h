#pragma once

#ifndef REAL_RUN
    //#define LOAD_FROM_FILE "sig17starterpack/sample.load"
    //#define LOAD_FROM_FILE "web.load"
    #define LOAD_FROM_FILE "large.load"
#endif

#define PRINT_STATISTICS

using DictHash = unsigned int;
#define HASH_NOT_FOUND ((DictHash) -1)

#define THREAD_COUNT (4)

#ifdef REAL_RUN
    #ifndef THREAD_COUNT
        #define THREAD_COUNT (40)
    #endif
#endif
