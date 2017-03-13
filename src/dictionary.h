#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include <cassert>
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

    }

    inline DictHash insert(const std::string& word, size_t wordHash)
    {
        return this->map.get_or_insert_hash(word, wordHash);
    }

    /*size_t createWordNfa(const std::string& word, int start, Nfa& nfa, size_t timestamp, size_t& wordHash)
    {
        NfaIterator iterator(0, NO_EDGE, 0);
        int size = (int) word.size();
        size_t prefixHash;
        HASH_INIT(prefixHash);
        std::string prefix;

        for (int i = start; i < size; i++)
        {
            char c = word[i];
            HASH_UPDATE(wordHash, c);

            if (__builtin_expect(c == ' ', false))
            {
                DictHash hash = this->insert(prefix, prefixHash);
                this->nfaAddEdge(nfa, hash, iterator);
                prefix.clear();
                HASH_INIT(prefixHash);
            }
            else
            {
                prefix += c;
                HASH_UPDATE(prefixHash, c);
            }
        }

        DictHash hash = this->insert(prefix, prefixHash);
        this->nfaAddEdge(nfa, hash, iterator);

        if (iterator.edgeIndex != NO_EDGE)
        {
            Edge& edge = nfa.states[iterator.state].edges[iterator.edgeIndex];
            if (iterator.index < edge.hashes.size())    // substring
            {
                size_t newStateId = nfa.createState();
                std::vector<DictHash> suffix(edge.hashes.cbegin() + iterator.index, edge.hashes.cend());
                nfa.states[newStateId].edges.insert({suffix[0], Edge(suffix, edge.stateIndex)});

                edge.stateIndex = newStateId;
                edge.hashes.resize(iterator.index);

                iterator.state = newStateId;
            }
            else iterator.state = edge.stateIndex;
        }

        nfa.states[iterator.state].word.from = timestamp;
        nfa.states[iterator.state].word.to = UINT32_MAX;
        nfa.states[iterator.state].word.length = (int) (word.size() - start);
        return (size_t) iterator.state;
    }*/

    size_t createWordNfaTwoStep(const std::string& word, int start, Nfa& nfa, size_t timestamp, size_t& wordHash)
    {
        int size = (int) word.size();
        size_t prefixHash;
        HASH_INIT(prefixHash);
        std::string prefix;

        std::vector<DictHash> hashes;
        for (int i = start; i < size; i++)
        {
            char c = word[i];
            HASH_UPDATE(wordHash, c);

            if (__builtin_expect(c == ' ', false))
            {
                hashes.push_back(this->insert(prefix, prefixHash));
                prefix.clear();
                HASH_INIT(prefixHash);
            }
            else
            {
                prefix += c;
                HASH_UPDATE(prefixHash, c);
            }
        }

        hashes.push_back(this->insert(prefix, prefixHash));

        size_t state = this->nfaAddEdges(nfa, hashes);
        nfa.states[state].word.from = timestamp;
        nfa.states[state].word.to = UINT32_MAX;
        nfa.states[state].word.length = (int) (word.size() - start);
        return (size_t) state;
    }

    /*void nfaAddEdge(Nfa& nfa, DictHash hash, NfaIterator& iterator)
    {
        while (true)
        {
            CombinedNfaState& state = nfa.states[iterator.state];
            if (iterator.edgeIndex == NO_EDGE)
            {
                auto it = state.edges.find(hash);
                if (it != state.edges.end())
                {
                    Edge& edge = it->second;
                    if (edge.hashes.size() > 1)
                    {
                        iterator.edgeIndex = hash; // edge found
                        iterator.index = 1;
                    }
                    else
                    {
                        iterator.edgeIndex = NO_EDGE;
                        iterator.index = 0;
                        iterator.state = edge.stateIndex;
                    }
                    return;
                }
                else
                {
                    // create edge
                    size_t newStateId = nfa.createState();
                    state.edges.insert({hash, Edge(hash, newStateId)});
                    iterator.edgeIndex = hash;
                    iterator.index = 1;
                }
            }
            else
            {
                Edge& edge = state.edges[iterator.edgeIndex];
                if (iterator.index == edge.hashes.size())   // advance edge
                {
                    edge.hashes.push_back(hash);
                    iterator.index++;
                }
                else
                {
                    if (edge.hashes[iterator.index] == hash)
                    {
                        iterator.index++;
                        if (iterator.index >= edge.hashes.size())   // move to next state
                        {
                            iterator.state = edge.stateIndex;
                            iterator.edgeIndex = NO_EDGE;
                            iterator.index = 0;
                        }
                    }
                    else
                    {
                        size_t newStateId = nfa.createState();
                        std::vector<DictHash> suffix(edge.hashes.cbegin() + iterator.index, edge.hashes.cend());

                        nfa.states[newStateId].edges.insert({suffix[0], Edge(suffix, edge.stateIndex)});

                        edge.stateIndex = newStateId;
                        edge.hashes.resize(iterator.index);

                        iterator.state = newStateId;
                        iterator.edgeIndex = NO_EDGE;
                        iterator.index = 0;
                        continue;
                    }
                }
            }

            return;
        }
    }*/

    size_t nfaAddEdges(Nfa& nfa, const std::vector<DictHash>& hashes)
    {
        size_t hashIndex = 0;
        size_t size = hashes.size();
        NfaIterator iterator(0, NO_EDGE, 0);

        while (hashIndex < size)
        {
            CombinedNfaState& state = nfa.states[iterator.state];
            Edge* edge;
            const DictHash hash = hashes[hashIndex];

            LOCK(state.flag);
            Edge* foundEdge;
            if (state.has_edges() && ((foundEdge = state.get_edge(hash)) != nullptr))
            {
                edge = foundEdge;
            }
            else
            {
                size_t newState = nfa.createState();
                state.add_edge(hash, std::vector<DictHash>(hashes.begin() + hashIndex, hashes.end()), newState);
                UNLOCK(state.flag);
                return newState;
            }

            int maxSize = std::min(edge->hashes.size(), hashes.size() - hashIndex);
            int i = 0;
            for (; i < maxSize; i++)
            {
                if (edge->hashes[i] != hashes[hashIndex + i])
                {
                    break;
                }
            }

            if (i < (int) edge->hashes.size())    // split substring
            {
                size_t newState = nfa.createState();
                std::vector<DictHash> suffix(edge->hashes.begin() + i, edge->hashes.end());

                nfa.states[newState].add_edge(suffix[0], suffix, edge->stateIndex);

                edge->hashes.resize(i);
                edge->stateIndex = newState;
                iterator.state = newState;
            }
            else  // move to next word
            {
                iterator.state = edge->stateIndex;
            }

            UNLOCK(state.flag);
            hashIndex += i;
        }

        return iterator.state;
    }


    size_t size()
    {
        return this->map.size();
    }

    SimpleMap<std::string, DictHash> map;
};
