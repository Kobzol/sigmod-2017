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

int get_prefix(const std::string& word, size_t start, const std::string& needle);
int get_prefix(const std::string& word, size_t wordStart, const std::string& needle, size_t needleStart);

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
    Edge(const std::string& text, int stateIndex) : text(text), stateIndex(stateIndex)
    {

    }

    std::string text;
    int stateIndex = NO_EDGE;
};

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

    void add_edge(const std::string& text, int target)
    {
        this->edges.emplace_back(text, target);
    }

    Word word;
    LOCK_INIT(flag);

    std::vector<Edge> edges;
};

class Nfa
{
public:
    Nfa()
    {
        this->states.resize(NFA_STATES_INITIAL_SIZE);
        this->createState();    // add root state
    }

    void feedWord(NfaVisitor& visitor, const std::string& input,
                  std::vector<std::pair<unsigned int, unsigned int>>& results, size_t timestamp,
                  bool includeStartState)
    {
        size_t currentStateIndex = visitor.stateIndex;
        size_t nextStateIndex = 1 - currentStateIndex;

        std::vector<NfaIterator>& nextStates = visitor.states[nextStateIndex];
        nextStates.clear();
        results.clear();

        if (includeStartState)
        {
            visitor.states[currentStateIndex].emplace_back();
        }

        for (NfaIterator& iterator : visitor.states[currentStateIndex])
        {
            CombinedNfaState* state = &this->states[iterator.state];
            size_t charIndex = 0;
            size_t inputSize = input.size();

            while (charIndex < inputSize)
            {
                Edge* edge = nullptr;
                int prefix = 0;
                if (iterator.edgeIndex == NO_EDGE)
                {
                    int edgeCount = (int) state->edges.size();
                    for (int i = 0; i < edgeCount; i++)
                    {
                        edge = &state->edges[i];
                        prefix = get_prefix(input, charIndex, edge->text);
                        if (prefix > 0)
                        {
                            iterator.edgeIndex = i;
                            break;
                        }
                    }
                }
                else
                {
                    edge = &state->edges[iterator.edgeIndex];
                    prefix = get_prefix(edge->text, iterator.index, input, charIndex);
                }

                if (edge != nullptr && prefix > 0)
                {
                    if ((edge->text.size() - iterator.index) == prefix)   // move to next state
                    {
                        state = &this->states[edge->stateIndex];
                        iterator.state = edge->stateIndex;
                        iterator.edgeIndex = NO_EDGE;
                        iterator.index = 0;
                    }
                    else
                    {
                        iterator.index += prefix;
                    }

                    charIndex += prefix;
                }
                else break;
            }

            if (charIndex == inputSize)
            {
                nextStates.emplace_back(iterator.state, iterator.edgeIndex, iterator.index);
                if (iterator.edgeIndex == NO_EDGE && this->states[iterator.state].word.is_active(timestamp)) // found word
                {
                    results.emplace_back(iterator.state, this->states[iterator.state].word.length);
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
