#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstring>

#include "word.h"
#include "util.h"
#include "simplemap.h"
#include "timer.h"

#define NO_ARC ((int) -1)
#define NO_WORD ((int) -1)

struct Mapping
{
    Mapping(size_t state, unsigned int index): state(state), index(index)
    {

    }

    size_t state;
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

    std::vector<Mapping> states[2];
    size_t stateIndex = 0;
};

class CombinedNfaState
{
public:
    inline int get_arc(char c) const
    {
        for (auto& p : this->map)
        {
            if (p.first == c) return p.second;
        }
        return NO_ARC;
    }

    void add_arc(char c, unsigned int index)
    {
        this->map.emplace_back(c, index);
    }

    std::vector<std::pair<char, int>> map;
    int wordIndex = NO_WORD;
};


class Nfa
{
public:
    Nfa()
    {
        this->states.resize(NFA_STATES_INITIAL_SIZE);
        this->createState();    // add root state
    }

    void addWord(const std::string& word, int start, size_t index)
    {
        int size = (int) word.size();
        int state = 0;
        for (int i = start; i < size; i++)
        {
            char c = word[i];
            int arc = this->states[state].get_arc(c);
            if (arc == NO_ARC)
            {
                int newState = this->createState();
                this->states[state].add_arc(c, newState);
                state = newState;
            }
            else state = arc;
        }

        this->states[state].wordIndex = index;
    }

    void feedWord(NfaVisitor& visitor, char c)
    {
        size_t nextStateIndex = 1 - visitor.stateIndex;
        std::vector<Mapping>& nextStates = visitor.states[nextStateIndex];
        nextStates.clear();

        for (auto& mapping : visitor.states[visitor.stateIndex])
        {
            CombinedNfaState& state = this->states[mapping.state];
            int arc = state.get_arc(c);
            if (arc != NO_ARC)
            {
                nextStates.emplace_back(arc, 0);
            }
        }

        visitor.stateIndex = nextStateIndex;
    }

    size_t createState()
    {
        if (__builtin_expect(this->states.size() <= this->index, false))
        {
            this->states.resize(this->states.size() * 2);
        }
        return this->index++;
    }

    size_t index = 0;
    std::vector<CombinedNfaState> states;
};
