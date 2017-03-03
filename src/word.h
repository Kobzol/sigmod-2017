#pragma once

#include <string>
#include <vector>
#include <atomic>
#include "settings.h"

class Word
{
public:
    Word() : to(0)
    {

    }
    Word(size_t from, int length): from(from), to(UINT32_MAX), length(length)
    {

    }

    inline void deactivate(size_t timestamp)
    {
        this->to = timestamp;
    }
    inline bool is_active(size_t timestamp) const
    {
        return this->from <= timestamp && timestamp < this->to;
    }

    std::atomic<size_t> from;   // TODO: check if neededs
    std::atomic<size_t> to;
    int length;
};

class Match
{
public:
    Match(unsigned int index, unsigned int length): index(index), length(length)
    {

    }

    unsigned int index;
    unsigned int length;
};
