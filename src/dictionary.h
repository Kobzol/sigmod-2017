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
        return this->map.get_or_insert_hash(word, wordHash, this->map.size());
    }

    size_t createWordNfa(const std::string& word, int start, Nfa& nfa, size_t timestamp, size_t& wordHash)
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
                nfa.states[newStateId].edges.emplace_back(suffix, edge.stateIndex);

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
    }

    void nfaAddEdge(Nfa& nfa, DictHash hash, NfaIterator& iterator)
    {
        while (true)
        {
            CombinedNfaState& state = nfa.states[iterator.state];
            if (iterator.edgeIndex == NO_EDGE)
            {
                auto it = std::lower_bound(state.edges.begin(), state.edges.end(), hash, [](Edge& edge, DictHash value)
                {
                    return edge.hashes[0] < value;
                });
                if (it != state.edges.end() && it->hashes[0] == hash)
                {
                    Edge& edge = *it;
                    if (edge.hashes.size() > 1)
                    {
                        iterator.edgeIndex = (it - state.edges.begin()); // edge found
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
                    int index = (it - state.edges.begin());
                    state.edges.insert(it, Edge(hash, newStateId));
                    iterator.edgeIndex = index;
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

                        nfa.states[newStateId].edges.emplace_back(suffix, edge.stateIndex);

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
    }

    size_t size()
    {
        return this->map.size();
    }

    SimpleMap<std::string, DictHash> map;
};
