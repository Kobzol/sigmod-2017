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
        size_t state = this->nfaAddEdges(nfa, word, start);
        nfa.states[state].word.from = timestamp;
        nfa.states[state].word.to = UINT32_MAX;
        nfa.states[state].word.length = (int) (word.size() - start);
        return (size_t) state;
    }
    size_t nfaAddEdges(Nfa& nfa, const std::string& text, int start)
    {
        size_t charIndex = (size_t) start;
        size_t size = text.size();
        NfaIterator iterator(0, NO_EDGE, 0);

        while (charIndex < size)
        {
            CombinedNfaState& state = nfa.states[iterator.state];
            LOCK(state.flag);

            // find edge
            int edgeCount = (int) state.edges.size();
            int i = 0;
            int prefix = 0;
            for (; i < edgeCount; i++)
            {
                prefix = get_prefix(text, charIndex, state.edges[i].text);
                if (prefix > 0)
                {
                    break;
                }
            }

            if (i == edgeCount) // no edge found
            {
                size_t newState = nfa.createState();
                state.add_edge(text.substr(charIndex), newState);
                UNLOCK(state.flag);
                return newState;
            }
            else if (prefix == state.edges[i].text.size())  // move to next state
            {
                charIndex += prefix;
                iterator.state = state.edges[i].stateIndex;
                UNLOCK(state.flag);
            }
            else // split
            {
                size_t newState = nfa.createState();
                nfa.states[newState].add_edge(state.edges[i].text.substr(prefix), state.edges[i].stateIndex);

                state.edges[i].text.resize(prefix);
                state.edges[i].stateIndex = newState;
                iterator.state = newState;
                UNLOCK(state.flag);
                charIndex += prefix;
            }
        }

        return iterator.state;
    }
};
