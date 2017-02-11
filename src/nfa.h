#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "word.h"
#include "util.h"

#define NOT_FINAL_STATE ((ssize_t) -1)

class NfaVisitor
{
public:
    std::unordered_set<size_t> states[2];
    size_t stateIndex = 0;
};

template <typename MapType>
class NfaState
{
public:
    std::unordered_map<MapType, size_t> arcs;
    ssize_t wordIndex = NOT_FINAL_STATE;
};

template <typename MapType>
class Nfa
{
public:
    Nfa()
    {
        this->states.emplace_back();    // add root state
    }

    void addWord(Word& word, size_t wordIndex)
    {
        size_t activeState = 0;
        size_t index = 0;

        while (index < word.word.size())
        {
            std::string prefix = find_prefix_from(word.word, index);

            if (!this->states.at(activeState).arcs.count(prefix))
            {
                this->states.at(activeState).arcs.insert({prefix, this->createState()});
            }

            activeState = this->states.at(activeState).arcs.at(prefix);

            index += prefix.size() + 1;
        }

        this->states.at(activeState).wordIndex = wordIndex;
    }

    std::vector<ssize_t> feedWord(NfaVisitor& visitor, const MapType& input)
    {
        std::vector<ssize_t> results;

        size_t currentStateIndex = visitor.stateIndex;
        size_t nextStateIndex = 1 - currentStateIndex;

        visitor.states[nextStateIndex].clear();
        visitor.states[currentStateIndex].insert(0);

        for (size_t stateId : visitor.states[currentStateIndex])
        {
            NfaState<MapType>& state = this->states.at(stateId);
            if (state.arcs.count(input))
            {
                size_t nextStateId = state.arcs.at(input);
                visitor.states[nextStateIndex].insert(nextStateId);

                NfaState<MapType>& nextState = this->states.at(nextStateId);
                if (nextState.wordIndex != NOT_FINAL_STATE)
                {
                    results.push_back(nextState.wordIndex);
                }
            }
        }

        visitor.stateIndex = nextStateIndex;

        return results;
    }

private:
    std::vector<NfaState<MapType>> states;

    size_t createState()
    {
        this->states.emplace_back();
        return this->states.size() - 1;
    }
};