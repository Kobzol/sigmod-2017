#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <shared_mutex>
#include <tbb/concurrent_unordered_map.h>

#include "word.h"
#include "util.h"

#define NOT_FINAL_STATE ((ssize_t) -1)
#define NO_ARC ((ssize_t) -1)

#define LINEAR_MAP_SIZE (1024 * 1000)
extern ssize_t* linearMap;

void init_linear_map();

class NfaVisitor
{
public:
    std::unordered_set<ssize_t> states[2];
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
        if (this->arcs.count(input))
        {
            return this->arcs.at(input);
        }

        return NO_ARC;
    }

    void add_arc(const MapType& input, size_t index) override
    {
        this->arcs.insert({input, index});
    }

//private:
    tbb::concurrent_unordered_map<MapType, size_t> arcs{2000};
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
        this->states.push_back(new HashNfaState<MapType>());    // add root state
        this->states.reserve(1000 * 1000);
    }

    void addWord(Word& word, size_t wordIndex)
    {
        ssize_t activeState = 0;
        size_t index = 0;

        while (index < word.word.size())
        {
            std::string prefix = find_prefix_from(word.word, index);

            ssize_t arc = this->states.at(activeState)->get_arc(prefix);

            if (arc == NO_ARC)
            {
                //std::unique_lock<std::shared_timed_mutex> lock(this->mutex);
                size_t state = this->createState();
                this->states.at(activeState)->add_arc(prefix, state);
                activeState = state;
            }
            else activeState = arc;

            index += prefix.size() + 1;
        }

        this->states.at(activeState)->wordIndex = wordIndex;
    }

    void feedWord(NfaVisitor& visitor, const MapType& input, std::vector<ssize_t>& results)
    {
        size_t currentStateIndex = visitor.stateIndex;
        size_t nextStateIndex = 1 - currentStateIndex;

        visitor.states[nextStateIndex].clear();
        visitor.states[currentStateIndex].insert(0);

        //std::shared_lock<std::shared_timed_mutex> lock(this->mutex);
        for (ssize_t stateId : visitor.states[currentStateIndex])
        {
            NfaState<MapType>* state = this->states.at(stateId);
            ssize_t nextStateId;

            nextStateId = state->get_arc(input);
            if (nextStateId != NO_ARC)
            {
                visitor.states[nextStateIndex].insert(nextStateId);

                NfaState<MapType>* nextState = this->states.at(nextStateId);
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
    std::shared_timed_mutex mutex;
    std::atomic<bool> inserting{0};
    std::atomic<int> reading{0};
    std::condition_variable waitVar;

    size_t createState()
    {
        this->states.push_back(new LinearNfaState<MapType>());
        return this->states.size() - 1;
    }
};