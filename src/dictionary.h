#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include "word.h"
#include "settings.h"
#include "simplemap.h"
#include "nfa.h"
#include "hash.h"


class Dictionary
{
public:
    Dictionary() : map(DICTIONARY_HASH_MAP_SIZE)
    {
        this->prefix.reserve(1000);
    }

    DictHash insert(const std::string& word, size_t wordHash)
    {
        DictHash hash = this->map.get_hash(word, wordHash);
        if (hash == HASH_NOT_FOUND)
        {
            hash = this->map.size();
            this->map.insert_hash(word, hash, wordHash);
            return hash;
        }

        return hash;
    }

    template <typename MapType>
    size_t createWordNfa(const std::string& word, size_t start, Nfa<MapType>& nfa, size_t timestamp, size_t& wordHash)
    {
        ssize_t activeState = 0;
        size_t size = word.size();
        size_t prefixHash;
        HASH_INIT(prefixHash);

        for (size_t i = start; i < size; i++)
        {
            char c = word[i];
            if (__builtin_expect(c == ' ', false))
            {
                DictHash hash = this->insert(this->prefix, prefixHash);
                this->nfaAddEdge(nfa, hash, activeState);
                this->prefix.clear();
                HASH_INIT(prefixHash);
            }
            else
            {
                this->prefix += c;
                HASH_UPDATE(prefixHash, c);
            }
            HASH_UPDATE(wordHash, c);
        }

        DictHash hash = this->insert(this->prefix, prefixHash);
        this->nfaAddEdge(nfa, hash, activeState);

        nfa.states[activeState].word.from = timestamp;
        nfa.states[activeState].word.to = UINT32_MAX;
        nfa.states[activeState].word.length = word.size() - start;
        this->prefix.clear();

        return (size_t) activeState;
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
                NfaStateType<MapType>& state = nfa.states[activeState];
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
