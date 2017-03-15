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
    NfaIterator(unsigned int state, ssize_t edgeIndex, const std::string& edge, unsigned int index)
            : state(state), edgeIndex(edgeIndex), edge(edge), index(index)
    {

    }

    unsigned state;
    ssize_t edgeIndex;
    std::string edge;
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
    Edge(const std::string& hash, int stateIndex) : stateIndex(stateIndex)
    {
        this->hashes.push_back(hash);
    }
    Edge(const std::vector<std::string>& hashes, int stateIndex) : hashes(hashes), stateIndex(stateIndex)
    {

    }

    std::vector<std::string> hashes;
    int stateIndex = NO_EDGE;
};

bool operator==(const Edge& e1, const Edge& e2);

class CombinedNfaState
{
public:
    CombinedNfaState()
    {

    }
    CombinedNfaState(const CombinedNfaState& state)
    {
        this->word = state.word;
        this->edges = state.edges;
    }

    Edge* get_edge(const std::string& hash)
    {
        auto it = this->edges.find(hash);
        if (it == this->edges.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    void add_edge(const std::string& hash, const std::vector<std::string>& hashes, int target)
    {
        this->edges.emplace(std::make_pair(hash, Edge(hashes, target)));
    }

    inline bool has_edges() const
    {
        return this->edges.size() > 0;
    }

    Word word;
    LOCK_INIT(flag);

//private:
    //std::vector<std::pair<DictHash, Edge>> edgesLinear;
    std::unordered_map<std::string, Edge> edges;
};

class Nfa
{
public:
    Nfa()
    {
        this->states.resize(NFA_STATES_INITIAL_SIZE);
        this->createState();    // add root state
    }

    void feedWord(NfaVisitor& visitor, const std::string& input, std::vector<std::pair<unsigned int, unsigned int>>& results, size_t timestamp)
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
                Edge* edge = state.get_edge(input);
                if (edge != nullptr)
                {
                    if (edge->hashes.size() == 1)
                    {
                        foundState = edge->stateIndex;
                        nextStates.emplace_back(edge->stateIndex, NO_EDGE, 0);
                    }
                    else
                    {
                        nextStates.emplace_back(iterator.state, 1, input, 1);
                    }
                }
            }
            else
            {
                Edge& edge = *state.get_edge(iterator.edge);
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

                    nextStates.emplace_back(iterator.state, iterator.edgeIndex, iterator.edge, iterator.index);
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
        LOCK(this->flag);
        if (__builtin_expect(this->states.size() <= this->stateIndex, false))
        {
            this->states.resize((size_t) (this->states.size() * 1.5));
        }

        size_t value = this->stateIndex++;
        UNLOCK(this->flag);
        return value;
    }

    size_t stateIndex = 0;
    std::vector<CombinedNfaState> states;

    LOCK_INIT(flag);
};
