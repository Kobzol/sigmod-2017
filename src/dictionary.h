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
    size_t createWordNfaTwoStep(const std::string& word, int start, Nfa& nfa, size_t timestamp)
    {
        int size = (int) word.size();
        std::string prefix;
        std::vector<std::string> words;
        for (int i = start; i < size; i++)
        {
            char c = word[i];

            if (__builtin_expect(c == ' ', false))
            {
                words.push_back(prefix);
                prefix.clear();
            }
            else prefix += c;
        }

        words.push_back(prefix);

        size_t state = this->nfaAddEdges(nfa, words);
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

    size_t nfaAddEdges(Nfa& nfa, const std::vector<std::string>& words)
    {
        size_t wordIndex = 0;
        size_t size = words.size();
        NfaIterator iterator(0, NO_EDGE, 0);

        while (wordIndex < size)
        {
            CombinedNfaState& state = nfa.states[iterator.state];
            Edge* edge;
            const std::string& hash = words[wordIndex];

            LOCK(state.flag);
            Edge* foundEdge;
            if (state.has_edges() && ((foundEdge = state.get_edge(hash)) != nullptr))
            {
                edge = foundEdge;
            }
            else
            {
                size_t newState = nfa.createState();
                state.add_edge(hash, std::vector<std::string>(words.begin() + wordIndex, words.end()), newState);
                UNLOCK(state.flag);
                return newState;
            }

            int maxSize = std::min(edge->hashes.size(), words.size() - wordIndex);
            int i = 0;
            for (; i < maxSize; i++)
            {
                if (edge->hashes[i] != words[wordIndex + i])
                {
                    break;
                }
            }

            if (i < (int) edge->hashes.size())    // split substring
            {
                size_t newState = nfa.createState();
                std::vector<std::string> suffix(edge->hashes.begin() + i, edge->hashes.end());

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
            wordIndex += i;
        }

        return iterator.state;
    }
};
