#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include "word.h"
#include "settings.h"


class Dictionary
{
public:
    DictHash get_hash(const std::string& word)
    {
        return this->dictionary[word];
    }
    DictHash get_hash_maybe(const std::string& word)
    {
        auto it = this->dictionary.find(word);
        if (it == this->dictionary.end())
        {
            return HASH_NOT_FOUND;
        }

        return it->second;
    }

    /*const std::string& get_string(DictHash hash)
    {
        if (!this->backMapping.count(hash))
        {
            std::cerr << "ERROR, NO HASH FOUND" << std::endl;
        }

        return this->backMapping.at(hash);
    }*/

    DictHash insert(const std::string& word)
    {
        if (!this->dictionary.count(word))
        {
            DictHash hash = this->dictionary.size();
            this->dictionary.insert({word, hash});
            //this->backMapping.insert({hash, word});

            return hash;
        }

        return this->get_hash(word);
    }

    void createWord(const std::string& word, size_t start, std::vector<DictHash>& hashList)
    {
        std::string prefix;
        for (size_t i = start; i < word.size(); i++)
        {
            char c = word[i];
            if (c == ' ')
            {
                hashList.push_back(this->insert(prefix));
                prefix.clear();
            }
            else
            {
                prefix += c;
            }
        }

        hashList.push_back(this->insert(prefix));
    }
    void createWordNoInsert(const std::string& word, std::vector<DictHash>& hashList)
    {
        std::string prefix;

        for (size_t i = 0; i < word.size(); i++)
        {
            char c = word[i];
            if (c == ' ')
            {
                hashList.push_back(this->get_hash_maybe(prefix));
                prefix = "";
            }
            else
            {
                prefix += c;
            }
        }

        hashList.push_back(this->get_hash_maybe(prefix));
    }

    /*std::string createString(const Word& word)
    {
        std::string result = this->get_string(word.hashList.at(0));

        size_t size = word.hashList.size();
        for (size_t i = 1; i < size; i++)
        {
            result += ' ';
            result += this->get_string(word.hashList.at(i));
        }
        return result;
    }*/

    size_t size()
    {
        return this->dictionary.size();
    }

private:
    HashMap<std::string, DictHash> dictionary;
    //std::unordered_map<DictHash, std::string> backMapping;
};
