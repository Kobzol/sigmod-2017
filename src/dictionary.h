#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include "word.h"
#include "settings.h"
#include "simplemap.h"
#include "nfa.h"


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
    template <typename MapType>
    void createWordNfa(const std::string& word, size_t start, std::vector<DictHash>& hashList, Nfa<MapType>& nfa, size_t wordIndex)
    {
        ssize_t activeState = 0;
        ssize_t arc;
        size_t size = word.size();
        for (size_t i = start; i < size; i++)
        {
            char c = word[i];
            if (c == ' ')
            {
                DictHash hash = this->insert(this->prefix);
                hashList.push_back(hash);
                this->nfaAddEdge(nfa, hash, activeState);
                this->prefix.clear();
            }
            else this->prefix += c;
        }

        DictHash hash = this->insert(this->prefix);
        hashList.push_back(hash);
        this->nfaAddEdge(nfa, hash, activeState);

        nfa.states[activeState].wordIndex = wordIndex;
        this->prefix.clear();
    }

    template <typename MapType>
    void nfaAddEdge(Nfa<MapType>& nfa, DictHash hash, ssize_t& activeState)
    {
        ssize_t arc;
        if (activeState == 0)
        {
            arc = nfa.rootState.get_arc(hash);
            if (arc == NO_ARC)
            {
                size_t newStateId = nfa.createState();
                nfa.rootState.add_arc(hash, newStateId);
                activeState = newStateId;
            }
            else activeState = arc;
        }
        else
        {
            arc = (nfa.states[activeState].*(nfa.states[activeState].get_fn))(hash);
            if (arc == NO_ARC)
            {
                size_t stateId = nfa.createState();
                CombinedNfaState<MapType>& state = nfa.states[activeState];
                (state.*(state.add_fn))(hash, stateId);
                activeState = stateId;
            }
            else activeState = arc;
        }
    }

    size_t size()
    {
        return this->map.size();
    }

    std::string prefix;
    SimpleMap<std::string, DictHash> map;
};
