#pragma once

#include <string>
#include <vector>
#include "settings.h"

class Word
{
public:
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

    size_t from;
    size_t to;
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
