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

extern Timer insertHashTimer;
extern Timer nfaAddEdgeTimer;
extern Timer createStateTimer;
extern Timer addArcTimer;
extern Timer getArcTimer;


class Dictionary
{
public:
    Dictionary() : map(DICTIONARY_HASH_MAP_SIZE)
    {
        this->prefix.reserve(1000);
    }

    inline DictHash insert(const std::string& word, size_t wordHash)
    {
        return this->map.get_or_insert_hash(word, wordHash, this->map.size());
    }

    template <typename MapType>
    size_t createWordNfa(const std::string& word, size_t start, Nfa<MapType>& nfa, size_t timestamp, size_t& wordHash)
    {
        ssize_t activeState = 0;
        size_t size = word.size();
        size_t prefixHash;
        HASH_INIT(prefixHash);
        size_t i = start;

        for (; i < size; i++)
        {
            char c = word[i];
            HASH_UPDATE(wordHash, c);

            if (__builtin_expect(c == ' ', false))
            {
                DictHash hash = this->insert(this->prefix, prefixHash);
                this->nfaAddEdge(nfa, hash, activeState);
                this->prefix.clear();
                HASH_INIT(prefixHash);
                i++;
                break;
            }
            else
            {
                this->prefix += c;
                HASH_UPDATE(prefixHash, c);
            }
        }

        if (activeState == 0)
        {
            this->nfaAddEdge(nfa, this->insert(this->prefix, prefixHash), activeState);
            nfa.states[activeState].word.from = timestamp;
            nfa.states[activeState].word.to = UINT32_MAX;
            nfa.states[activeState].word.length = word.size() - start;
            this->prefix.clear();

            return (size_t) activeState;
        }

        for (; i < size; i++)
        {
            char c = word[i];
            HASH_UPDATE(wordHash, c);

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
    void nfaAddEdgeRoot(Nfa<MapType>& nfa, DictHash hash, ssize_t& activeState)
    {
        ssize_t arc = nfa.rootState.get_arc(hash);
        if (arc == NO_ARC)
        {
            size_t newStateId = nfa.createState();
            nfa.rootState.add_arc(hash, newStateId);
            activeState = newStateId;
        }
        else activeState = arc;
    }
    template <typename MapType>
    void nfaAddEdgeNoRoot(Nfa<MapType>& nfa, DictHash hash, ssize_t& activeState)
    {
        ssize_t arc = (nfa.states[activeState].*(nfa.states[activeState].get_fn))(hash);
        if (arc == NO_ARC)
        {
            size_t stateId = nfa.createState();
            NfaStateType<MapType>& state = nfa.states[activeState];
            state.add_arc(hash, stateId);
            activeState = stateId;
        }
        else activeState = arc;
    }

    template <typename MapType>
    void nfaAddEdge(Nfa<MapType>& nfa, DictHash hash, ssize_t& activeState)
    {
        ssize_t arc;
        if (activeState == 0)
        {

#ifdef PRINT_STATISTICS
            getArcTimer.start();
#endif
            arc = nfa.rootState.get_arc(hash);
#ifdef PRINT_STATISTICS
            getArcTimer.add();
#endif
            if (arc == NO_ARC)
            {
#ifdef PRINT_STATISTICS
                createStateTimer.start();
#endif
                size_t newStateId = nfa.createState();
#ifdef PRINT_STATISTICS
                createStateTimer.add();
                addArcTimer.start();
#endif
                nfa.rootState.add_arc(hash, newStateId);
#ifdef PRINT_STATISTICS
                addArcTimer.add();
#endif
                activeState = newStateId;
            }
            else activeState = arc;
        }
        else
        {
#ifdef PRINT_STATISTICS
            getArcTimer.start();
#endif
            auto& state = nfa.states[activeState];
            arc = state.get_arc(hash);
#ifdef PRINT_STATISTICS
            getArcTimer.add();
#endif
            if (arc == NO_ARC)
            {
#ifdef PRINT_STATISTICS
                createStateTimer.start();
#endif
                size_t stateId = nfa.createState();
#ifdef PRINT_STATISTICS
                createStateTimer.add();
#endif
                NfaStateType<MapType>& nextState = nfa.states[activeState];
#ifdef PRINT_STATISTICS
                addArcTimer.start();
#endif
                nextState.add_arc(hash, stateId);
#ifdef PRINT_STATISTICS
                addArcTimer.add();
#endif
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
