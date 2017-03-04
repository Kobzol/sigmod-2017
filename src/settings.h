#pragma once

#ifndef REAL_RUN
    //#define LOAD_FROM_FILE "sig17starterpack/sample.load"
    //#define LOAD_FROM_FILE "web.load"
    #define LOAD_FROM_FILE "50000_load/load"
#endif

#define PRINT_STATISTICS

using DictHash = unsigned int;
#define HASH_NOT_FOUND ((DictHash) -1)

#define THREAD_COUNT (12)
#define JOB_SPLIT_SIZE (5000UL)
#define WORDMAP_HASH_SIZE (2 << 25)
#define NFA_STATES_INITIAL_SIZE (2 << 26)
#define MAX_LINEAR_MAP_SIZE (50)
#define DICTIONARY_HASH_MAP_SIZE (2 << 24)  // must be a power of two
#define LINEAR_MAP_SIZE (1024 * 1024 * 10)

#ifdef REAL_RUN
    #ifndef THREAD_COUNT
        #define THREAD_COUNT (40)
    #endif
#endif

//#include "sparsepp.h"
//#include "hopscotch_map.h"
#include <unordered_map>

template <typename K, typename V>
using HashMap = std::unordered_map<K, V>; //spp::sparse_hash_map<K, V>;

// not scaling: simplemap linear probe (dict + wordmap), NFA linear map
