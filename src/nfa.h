#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "word.h"
#include "util.h"

#define NOT_FINAL_STATE ((ssize_t) -1)
#define NO_ARC ((ssize_t) -1)

#define LINEAR_MAP_SIZE (1024 * 1024)
extern ssize_t* linearMap;
void initLinearMap();

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

    std::vector<ssize_t> states[2];
    size_t stateIndex = 0;
};

template <typename MapType>
class HashNfaState
{
public:
    ssize_t get_arc(const MapType& input) const
    {
        auto it = this->arcs.find(input);
        if (it == this->arcs.end())
        {
            return NO_ARC;
        }

        return it->second;
    }

    void add_arc(const MapType& input, size_t index)
    {
        this->arcs[input] = index;
    }

    size_t get_size() const
    {
        return this->arcs.size();
    }

private:
    HashMap<MapType, size_t> arcs;
};

template <typename MapType>
class LinearMapNfaState
{
public:
    ssize_t get_arc(const MapType& input) const
    {
        return linearMap[input];
    }

    void add_arc(const MapType& input, size_t index)
    {
        linearMap[input] = index;
    }

    ssize_t wordIndex = NOT_FINAL_STATE;
};

template <typename MapType>
class LinearNfaState
{
public:
    LinearNfaState()
    {

    }

    ssize_t get_arc(const MapType& input) const
    {
        int size = (int) this->keys.size();
        for (int i = 0; i < size; i++)
        {
            if (this->keys[i] == input)
            {
                return this->indices[i];
            }
        }

        return NO_ARC;
    }

    void add_arc(const MapType& input, size_t index)
    {
        this->keys.emplace_back(input);
        this->indices.emplace_back(index);
    }

    size_t get_size() const
    {
        return this->keys.size();
    }

    std::vector<MapType> keys;
    std::vector<size_t> indices;
};

template <typename MapType>
class CombinedNfaState
{
public:
    ssize_t get_arc(const MapType& input) const
    {
        return this->linearMap.get_arc(input);
    }

    void add_arc(const MapType& input, size_t index)
    {
        return this->linearMap.add_arc(input, index);
    }

    LinearNfaState<MapType> linearMap;
    //HashNfaState<MapType> hashMap;
    ssize_t wordIndex = NOT_FINAL_STATE;
};

template <typename MapType>
class Nfa
{
public:
    Nfa()
    {
        this->states.reserve(100000);
        this->createState();    // add root state
    }

    void addWord(Word& word, size_t wordIndex)
    {
        ssize_t activeState;
        ssize_t arc = this->rootState.get_arc(word.hashList[0]);
        if (arc == NO_ARC)
        {
            size_t newStateId = this->createState();
            this->rootState.add_arc(word.hashList[0], newStateId);
            activeState = newStateId;
        }
        else activeState = arc;

        int size = (int) word.hashList.size();
        for (int i = 1; i < size; i++)
        {
            DictHash prefix = word.hashList[i];
            arc = this->states[activeState].get_arc(prefix);

            if (arc == NO_ARC)
            {
                size_t stateId = this->createState();
                CombinedNfaState<MapType>& state = this->states[activeState];
                state.add_arc(prefix, stateId);
                activeState = stateId;
            }
            else activeState = arc;
        }

        this->states[activeState].wordIndex = wordIndex;
    }

    void feedWord(NfaVisitor& visitor, const MapType& input, std::vector<ssize_t>& results)
    {
        size_t currentStateIndex = visitor.stateIndex;
        size_t nextStateIndex = 1 - currentStateIndex;

        visitor.states[nextStateIndex].clear();

        ssize_t arc = this->rootState.get_arc(input);
        if (arc != NO_ARC)
        {
            visitor.states[nextStateIndex].push_back(arc);
            CombinedNfaState<MapType>& nextState = this->states[arc];
            if (nextState.wordIndex != NOT_FINAL_STATE)
            {
                results.push_back(nextState.wordIndex);
            }
        }

        for (ssize_t stateId : visitor.states[currentStateIndex])
        {
            CombinedNfaState<MapType>& state = this->states[stateId];
            ssize_t nextStateId = state.get_arc(input);
            if (nextStateId != NO_ARC)
            {
                visitor.states[nextStateIndex].push_back(nextStateId);

                CombinedNfaState<MapType>& nextState = this->states[nextStateId];
                if (nextState.wordIndex != NOT_FINAL_STATE)
                {
                    results.push_back(nextState.wordIndex);
                }
            }
        }

        visitor.stateIndex = nextStateIndex;
    }

private:
    LinearMapNfaState<MapType> rootState;
    std::vector<CombinedNfaState<MapType>> states;

    size_t createState()
    {
        this->states.emplace_back();
        return this->states.size() - 1;
    }
};