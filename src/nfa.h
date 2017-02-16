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

    std::vector<ssize_t> states[2];
    size_t stateIndex = 0;
};

template <typename MapType>
class NfaState
{
public:
    virtual ssize_t get_arc(const MapType& input) = 0;
    virtual void add_arc(const MapType& input, size_t index) = 0;
    ssize_t wordIndex = NOT_FINAL_STATE;
};

template <typename MapType>
class HashNfaState : public NfaState<MapType>
{
public:
    ssize_t get_arc(const MapType& input) override
    {
        auto it = this->arcs.find(input);
        if (it == this->arcs.end())
        {
            return NO_ARC;
        }

        return it->second;
    }

    void add_arc(const MapType& input, size_t index) override
    {
        this->arcs[input] = index;
    }

private:
    std::unordered_map<MapType, size_t> arcs;
};

template <typename MapType>
class LinearMapNfaState : public NfaState<MapType>
{
public:
    ssize_t get_arc(const MapType& input) override
    {
        return linearMap[input];
    }

    void add_arc(const MapType& input, size_t index) override
    {
        linearMap[input] = index;
    }
};

template <typename MapType>
class LinearNfaState : public NfaState<MapType>
{
public:
    LinearNfaState()
    {
        this->arcs.reserve(5);
    }

    ssize_t get_arc(const MapType& input) override
    {
        for (Mapping& mapping : this->arcs)
        {
            if (mapping.key == input)
            {
                return mapping.index;
            }
        }

        return NO_ARC;
    }

    void add_arc(const MapType& input, size_t index) override
    {
        this->arcs.emplace_back(input, index);
    }

private:
    class Mapping
    {
    public:
        Mapping(const MapType& key, size_t index): key(key), index(index)
        {

        }

        MapType key;
        size_t index;
    };

    std::vector<Mapping> arcs;
};

template <typename MapType>
class Nfa
{
public:
    Nfa()
    {
        this->states.push_back(new LinearMapNfaState<MapType>());    // add root state
    }

    void addWord(Word& word, size_t wordIndex)
    {
        ssize_t activeState = 0;

        for (DictHash prefix : word.hashList)
        {
            ssize_t arc = this->states[activeState]->get_arc(prefix);

            if (arc == NO_ARC)
            {
                size_t state = this->createState();
                this->states[activeState]->add_arc(prefix, state);
                activeState = state;
            }
            else activeState = arc;
        }

        this->states[activeState]->wordIndex = wordIndex;
    }

    void feedWord(NfaVisitor& visitor, const MapType& input, std::vector<ssize_t>& results)
    {
        size_t currentStateIndex = visitor.stateIndex;
        size_t nextStateIndex = 1 - currentStateIndex;

        visitor.states[nextStateIndex].clear();
        visitor.states[currentStateIndex].push_back(0);

        for (ssize_t stateId : visitor.states[currentStateIndex])
        {
            NfaState<MapType>* state = this->states[stateId];
            ssize_t nextStateId = state->get_arc(input);
            if (nextStateId != NO_ARC)
            {
                visitor.states[nextStateIndex].push_back(nextStateId);

                NfaState<MapType>* nextState = this->states[nextStateId];
                if (nextState->wordIndex != NOT_FINAL_STATE)
                {
                    results.push_back(nextState->wordIndex);
                }
            }
        }

        visitor.stateIndex = nextStateIndex;
    }

private:
    std::vector<NfaState<MapType>*> states;

    size_t createState()
    {
        this->states.push_back(new LinearNfaState<MapType>());
        return this->states.size() - 1;
    }
};