#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "word.h"
#include "util.h"
#include "simplemap.h"

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

    ssize_t get_arc_linear(const MapType& input) const
    {
        int size = (int) this->linearMap.size();
        for (int i = 0; i < size; i++)
        {
            if (this->linearMap[i].first == input)
            {
                return this->linearMap[i].second;
            }
        }

        return NO_ARC;
    }

    void add_arc_linear(const MapType& input, size_t index)
    {
        this->linearMap.emplace_back(input, index);

        if (this->linearMap.size() > MAX_LINEAR_MAP_SIZE)
        {
            int size = (int) this->linearMap.size();
            for (int i = 0; i < size; i++)
            {
                this->hashMap[this->linearMap[i].first] = this->linearMap[i].second;
            }

            this->get_fn = &CombinedNfaState<MapType>::get_arc_hash;
            this->add_fn = &CombinedNfaState<MapType>::add_arc_hash;
        }
    }

    ssize_t get_arc_hash(const MapType& input) const
    {
        auto it = this->hashMap.find(input);
        if (it == this->hashMap.end())
        {
            return NO_ARC;
        }

        return it->second;
    }

    void add_arc_hash(const MapType& input, size_t index)
    {
        this->hashMap[input] = index;
    }

    std::vector<std::pair<MapType, unsigned int>> linearMap;
    HashMap<MapType, size_t> hashMap;

    ssize_t (CombinedNfaState<MapType>::*get_fn)(const MapType& input) const;
    void (CombinedNfaState<MapType>::*add_fn)(const MapType& input, size_t index);

    Word word;
};

template <typename MapType>
using NfaStateType = CombinedNfaState<MapType>;

template <typename MapType>
class Nfa
{
public:
    Nfa()
    {
        this->states.reserve(100000);
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
                if (nextState.word.is_active(timestamp))
                {
                    results.emplace_back(arc, nextState.word.length);
                }
            }
        }

        for (ssize_t stateId : visitor.states[currentStateIndex])
        {
            NfaStateType<MapType>& state = this->states[stateId];
            ssize_t nextStateId = (state.*(state.get_fn))(input);
            if (nextStateId != NO_ARC)
            {
                visitor.states[nextStateIndex].push_back(nextStateId);

                NfaStateType<MapType>& nextState = this->states[nextStateId];
                if (nextState.word.is_active(timestamp))
                {
                    results.emplace_back(nextStateId, nextState.word.length);
                }
            }
        }

        visitor.stateIndex = nextStateIndex;
    }

    LinearMapNfaState<MapType> rootState;
    std::vector<NfaStateType<MapType>> states;

    size_t createState()
    {
        this->states.emplace_back();
        return this->states.size() - 1;
    }
};
