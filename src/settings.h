#pragma once

#ifndef REAL_RUN
    //#define LOAD_FROM_FILE "sig17starterpack/sample.load"
    //#define LOAD_FROM_FILE "web.load"
    #define LOAD_FROM_FILE "small.load"
#endif

//#define PRINT_STATISTICS

using DictHash = unsigned int;
#define HASH_NOT_FOUND ((DictHash) -1)

#define THREAD_COUNT (8)
#define THREAD_QUERY_COUNT (8)

#ifdef REAL_RUN
    #ifndef THREAD_COUNT
        #define THREAD_COUNT (40)
    #endif
    #ifndef THREAD_QUERY_COUNT
        #define THREAD_QUERY_COUNT (THREAD_COUNT)
    #endif
#endif

//#include "sparsepp.h"
//#include "hopscotch_map.h"
#include <unordered_map>

template <typename K, typename V>
using HashMap = std::unordered_map<K, V>; //spp::sparse_hash_map<K, V>;

#define MAX_LINEAR_MAP_SIZE (50)
#define DICTIONARY_HASH_MAP_SIZE (2 << 17)  // must be a power of two
#define DICTIONARY_HASH_MAP_PREALLOC (10)

// TODO: wordMap no substr
// TODO: overlay add with main thread
// TODO: remove Word::hashList
