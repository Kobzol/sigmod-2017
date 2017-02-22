#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include "word.h"
#include "settings.h"
#include "simplemap.h"


class Dictionary
{
public:
    Dictionary() : map(DICTIONARY_HASH_MAP_SIZE, DICTIONARY_HASH_MAP_PREALLOC)
    {

    }

    DictHash insert(const std::string& word)
    {
        DictHash hash = this->map.get(word);
        if (hash == HASH_NOT_FOUND)
        {
            hash = this->map.size();
            this->map.insert(word, hash);

            return hash;
        }

        return hash;
    }

    void createWord(const std::string& word, size_t start, std::vector<DictHash>& hashList)
    {
        size_t size = word.size();
        for (size_t i = start; i < size; i++)
        {
            char c = word[i];
            if (c == ' ')
            {
                hashList.push_back(this->insert(this->prefix));
                this->prefix.clear();
            }
            else this->prefix += c;
        }

        hashList.push_back(this->insert(this->prefix));
        this->prefix.clear();
    }

    size_t size()
    {
        return this->map.size();
    }

    std::string prefix;
    SimpleMap<std::string, DictHash> map;
};
