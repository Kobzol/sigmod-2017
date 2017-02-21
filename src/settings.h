#pragma once

#ifndef REAL_RUN
    //#define LOAD_FROM_FILE "sig17starterpack/sample.load"
    //#define LOAD_FROM_FILE "web.load"
    #define LOAD_FROM_FILE "large.load"
#endif

//#define PRINT_STATISTICS

using DictHash = unsigned int;
#define HASH_NOT_FOUND ((DictHash) -1)

#define THREAD_COUNT (8)

#ifdef REAL_RUN
    #ifndef THREAD_COUNT
        #define THREAD_COUNT (40)
    #endif
#endif

//#include "sparsepp.h"
#include <unordered_map>

template <typename K, typename V>
using HashMap = std::unordered_map<K, V>; //spp::sparse_hash_map<K, V>;

#define MAX_LINEAR_MAP_SIZE (50)
