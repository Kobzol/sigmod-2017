#pragma once

#include <string>
#include <vector>
#include "settings.h"

class Word
{
public:
    Word(std::vector<DictHash> hashList): hashList(hashList), active(true)
    {

    }

    bool operator==(const Word& other)
    {
        return other.hashList == this->hashList;
    }

    std::vector<DictHash> hashList;
    bool active;
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

class Query
{
public:
    Query(std::string document): document(document)
    {

    }

    std::string document;
    std::string result;

    inline bool is_completed()
    {
        return this->result != "";
    }
};