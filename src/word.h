#pragma once

#include <string>

class Word
{
public:
    Word(std::string word): word(word), active(true)
    {

    }

    std::string word;
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