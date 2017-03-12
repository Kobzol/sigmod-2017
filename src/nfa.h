#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <map>

#include "word.h"
#include "util.h"
#include "simplemap.h"
#include "timer.h"

#define NO_ARC ((ssize_t) -1)
#define NO_EDGE ((int) -1)

struct NfaIterator
{
    NfaIterator() : state(0), edgeIndex(NO_EDGE), index(0)
    {

    }
    NfaIterator(unsigned int state, ssize_t edgeIndex, unsigned int index) : state(state), edgeIndex(edgeIndex), index(index)
    {

    }

    unsigned state;
    ssize_t edgeIndex;
    unsigned int index;
};

class NfaVisitor
{
public:
    NfaVisitor()
    {
        for (int i = 0; i < 2; i++)
        {
            this->states[i].reserve(5);
        }
    }

    inline void reset()
    {
        this->states[0].clear();
        this->states[1].clear();
    }

    std::vector<NfaIterator> states[2];
    size_t stateIndex = 0;
};

struct Edge
{
    Edge()
    {

    }
    Edge(DictHash hash, int stateIndex) : stateIndex(stateIndex)
    {
        this->hashes.push_back(hash);
    }
    Edge(const std::vector<DictHash>& hashes, int stateIndex) : hashes(hashes), stateIndex(stateIndex)
    {

    }

    std::vector<DictHash> hashes;
    int stateIndex = NO_EDGE;
};

bool operator==(const Edge& e1, const Edge& e2);

class CombinedNfaState
{
public:
    std::unordered_map<DictHash, Edge> edges;
    Word word;
};

class Nfa
{
public:
    Nfa()
    {
        this->states.resize(NFA_STATES_INITIAL_SIZE);
        this->createState();    // add root state
    }

    void feedWord(NfaVisitor& visitor, DictHash input, std::vector<std::pair<unsigned int, unsigned int>>& results, size_t timestamp)
    {
        size_t currentStateIndex = visitor.stateIndex;
        size_t nextStateIndex = 1 - currentStateIndex;

        std::vector<NfaIterator>& nextStates = visitor.states[nextStateIndex];
        nextStates.clear();
        results.clear();

        visitor.states[currentStateIndex].emplace_back();

        for (NfaIterator& iterator : visitor.states[currentStateIndex])
        {
            CombinedNfaState& state = this->states[iterator.state];
            int foundState = -1;
            if (iterator.edgeIndex == NO_EDGE)
            {
                auto it = state.edges.find(input);
                if (it != state.edges.end())
                {
                    if (it->second.hashes.size() == 1)
                    {
                        foundState = it->second.stateIndex;
                        nextStates.emplace_back(it->second.stateIndex, NO_EDGE, 0);
                    }
                    else nextStates.emplace_back(iterator.state, input, 1);
                }
            }
            else
            {
                Edge& edge = state.edges[iterator.edgeIndex];
                if (edge.hashes[iterator.index] == input)
                {
                    iterator.index++;
                    if (iterator.index == edge.hashes.size())
                    {
                        iterator.state = edge.stateIndex;
                        iterator.edgeIndex = NO_EDGE;
                        iterator.index = 0;
                        foundState = edge.stateIndex;
                    }

                    nextStates.emplace_back(iterator.state, iterator.edgeIndex, iterator.index);
                }
            }

            if (foundState != -1)
            {
                CombinedNfaState& wordState = this->states[foundState];
                if (wordState.word.is_active(timestamp))
                {
                    results.emplace_back(foundState, wordState.word.length);
                }
            }
        }

        visitor.stateIndex = nextStateIndex;
    }

    size_t createState()
    {
        if (__builtin_expect(this->states.size() <= this->stateIndex, false))
        {
            this->states.resize((size_t) (this->states.size() * 1.5));
        }

        return this->stateIndex++;
    }

    size_t stateIndex = 0;
    std::vector<CombinedNfaState> states;
};
