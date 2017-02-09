#pragma once

#include <string>
#include <vector>
#include "settings.h"

class Word
{
public:
    Word(std::vector<DictHash> hashList, size_t from): hashList(hashList),
                                                       from(from),
                                                       to(UINT32_MAX)
    {

    }

    inline bool operator==(const Word& other)
    {
        return other.hashList == this->hashList;
    }

    inline void deactivate(size_t timestamp)
    {
        this->to = timestamp;
    }
    inline bool is_active(size_t timestamp) const
    {
        return this->from <= timestamp && timestamp < this->to;
    }

    std::vector<DictHash> hashList;
    size_t from;
    size_t to;
};

class Match
{
public:
    Match(int index, std::string word): index(index), word(word)
    {

    }

    int index;
    std::string word;
};
