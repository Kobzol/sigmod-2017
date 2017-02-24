#pragma once

#define HASH_INIT(hash) hash = 0x811c9dc5;
#define HASH_UPDATE(hash, c) hash = hash ^ c; \
                              hash = hash * 16777619;
