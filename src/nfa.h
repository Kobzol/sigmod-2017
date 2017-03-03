#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "word.h"
#include "util.h"
#include "simplemap.h"
#include "timer.h"
#include "lock.h"

#define NO_ARC ((ssize_t) -1)

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
            if (this->keys[i].first == input)
            {
                return this->keys[i].second;
            }
        }

        return NO_ARC;
    }

    void add_arc(const MapType& input, size_t index)
    {
        this->keys.emplace_back(input, index);
    }

    size_t get_size() const
    {
        return this->keys.size();
    }

    std::vector<std::pair<MapType, unsigned int>> keys;
};

template <typename MapType>
class CombinedNfaState
{
public:
    CombinedNfaState(): get_fn(&CombinedNfaState<MapType>::get_arc_linear),
                        add_fn(&CombinedNfaState<MapType>::add_arc_linear)
    {

    }

    CombinedNfaState(const CombinedNfaState& other) // TODO
    {

    }

    ssize_t get_arc_linear(const MapType& input)
    {
        //this->lock();
        int size = (int) this->linearMap.size();
        for (int i = 0; i < size; i++)
        {
            if (this->linearMap[i].first == input)
            {
                //this->unlock();
                return this->linearMap[i].second;
            }
        }

        //this->unlock();
        return NO_ARC;
    }

    void add_arc_linear(const MapType& input, size_t index)
    {
        //this->lock();
        this->linearMap.emplace_back(input, index);

        if (__builtin_expect(this->linearMap.size() > MAX_LINEAR_MAP_SIZE, false))
        {
            int size = (int) this->linearMap.size();
            for (int i = 0; i < size; i++)
            {
                this->hashMap.insert({this->linearMap[i].first, this->linearMap[i].second});
            }

            this->get_fn = &CombinedNfaState<MapType>::get_arc_hash;
            this->add_fn = &CombinedNfaState<MapType>::add_arc_hash;
        }

        //this->unlock();
    }

    ssize_t get_arc_hash(const MapType& input)
    {
        //this->lock();
        auto it = this->hashMap.find(input);
        if (it == this->hashMap.end())
        {
            //this->unlock();
            return NO_ARC;
        }

        volatile ssize_t value = it->second;
        //this->unlock();

        return value;
    }

    void add_arc_hash(const MapType& input, size_t index)
    {
        //this->lock();
        this->hashMap.insert({input, (unsigned int) index});
        //this->unlock();
    }

    std::vector<std::pair<MapType, unsigned int>> linearMap;
    HashMap<MapType, size_t> hashMap;

    ssize_t (CombinedNfaState<MapType>::*get_fn)(const MapType& input);
    void (CombinedNfaState<MapType>::*add_fn)(const MapType& input, size_t index);

    Word word;
    LOCK_INIT(flag);
};

template <typename MapType>
using NfaStateType = CombinedNfaState<MapType>;

template <typename MapType>
class Nfa
{
public:
    Nfa()
    {
        this->states.reserve(NFA_STATES_INITIAL_SIZE);
        this->createState();    // add root state
    }

    void feedWord(NfaVisitor& visitor, MapType input, std::vector<std::pair<unsigned int, unsigned int>>& results,
                  size_t timestamp, bool includeStartState)
    {
        size_t currentStateIndex = visitor.stateIndex;
        size_t nextStateIndex = 1 - currentStateIndex;

        visitor.states[nextStateIndex].clear();

        if (includeStartState)
        {
            ssize_t arc = this->rootState.get_arc(input);
            if (arc != NO_ARC)
            {
                visitor.states[nextStateIndex].push_back(arc);
                NfaStateType<MapType>& nextState = this->states[arc];
                LOCK(nextState.flag);
                if (nextState.word.is_active(timestamp))
                {
                    results.emplace_back(arc, nextState.word.length);
                }
                UNLOCK(nextState.flag);
            }
        }

        for (ssize_t stateId : visitor.states[currentStateIndex])
        {
            NfaStateType<MapType>& state = this->states[stateId];
            LOCK(state.flag);
            ssize_t nextStateId = (state.*(state.get_fn))(input);
            UNLOCK(state.flag);

            if (nextStateId != NO_ARC)
            {
                visitor.states[nextStateIndex].push_back(nextStateId);

                NfaStateType<MapType>& nextState = this->states[nextStateId];
                LOCK(nextState.flag);
                if (nextState.word.is_active(timestamp))
                {
                    results.emplace_back(nextStateId, nextState.word.length);
                }
                UNLOCK(nextState.flag);
            }
        }

        visitor.stateIndex = nextStateIndex;
    }
    void feedWordWithInitial(NfaVisitor& visitor, MapType input,
                             std::vector<std::pair<unsigned int, unsigned int>>& results, size_t timestamp)
    {
        size_t currentStateIndex = visitor.stateIndex;
        size_t nextStateIndex = 1 - currentStateIndex;

        visitor.states[nextStateIndex].clear();

        ssize_t arc = this->rootState.get_arc(input);
        if (arc != NO_ARC)
        {
            visitor.states[nextStateIndex].push_back(arc);
            NfaStateType<MapType>& nextState = this->states[arc];
            LOCK(nextState.flag);
            if (nextState.word.is_active(timestamp))
            {
                results.emplace_back(arc, nextState.word.length);
            }
            UNLOCK(nextState.flag);
        }

        for (ssize_t stateId : visitor.states[currentStateIndex])
        {
            NfaStateType<MapType>& state = this->states[stateId];
            LOCK(state.flag);
            ssize_t nextStateId = (state.*(state.get_fn))(input);
            UNLOCK(state.flag)

            if (nextStateId != NO_ARC)
            {
                visitor.states[nextStateIndex].push_back(nextStateId);

                NfaStateType<MapType>& nextState = this->states[nextStateId];
                LOCK(nextState.flag);
                if (nextState.word.is_active(timestamp))
                {
                    results.emplace_back(nextStateId, nextState.word.length);
                }
                UNLOCK(nextState.flag);
            }
        }

        visitor.stateIndex = nextStateIndex;
    }

    LinearMapNfaState<MapType> rootState;
    std::vector<NfaStateType<MapType>> states;

    size_t createState()
    {
        LOCK(this->flag);
        this->states.emplace_back();
        size_t size = this->states.size() - 1;
        UNLOCK(this->flag);

        return size;
    }

    LOCK_INIT(flag);
};
