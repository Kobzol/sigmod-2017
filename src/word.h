#pragma once

#include <string>
#include <climits>

class Word
{
public:
    Word(std::string word, size_t from): word(word), from(from)
    {

    }

    inline bool is_active(size_t timestamp) const
    {
        return this->from <= timestamp && timestamp < this->to;
    }

    std::string word;
    size_t from;
    size_t to = UINT_MAX;
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
