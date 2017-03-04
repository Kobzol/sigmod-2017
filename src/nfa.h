#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "word.h"
#include "util.h"
#include "simplemap.h"
#include "timer.h"

#define NO_ARC ((ssize_t) -1)

extern ssize_t* linearMap;
void initLinearMap();

template <typename MapType>
struct Mapping
{
    Mapping(MapType key, unsigned int value) : key(key), value(value)
    {

    }

    MapType key;
    unsigned int value;
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
        auto it = std::lower_bound(this->keys.begin(), this->keys.end(), input, [](const Mapping<MapType>& i1, const MapType& value) {
            return i1.key < value;
        });
        if (it == this->keys.end() || it->key != input) return NO_ARC;
        return it->value;
    }

    void add_arc(const MapType& input, size_t index)
    {
        auto it = std::lower_bound(this->keys.begin(), this->keys.end(), input, [](const Mapping<MapType>& i1, const MapType& value) {
            return i1.key < value;
        });
        this->keys.insert(it, Mapping<MapType>(input, index));
    }

    std::vector<Mapping<MapType>> keys;
    Word word;
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
        this->hashMap.insert({input, (unsigned int) index});
    }

    std::vector<std::pair<MapType, unsigned int>> linearMap;
    HashMap<MapType, size_t> hashMap;

    ssize_t (CombinedNfaState<MapType>::*get_fn)(const MapType& input) const;
    void (CombinedNfaState<MapType>::*add_fn)(const MapType& input, size_t index);

    Word word;
};

template <typename MapType>
using NfaStateType = LinearNfaState<MapType>;

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
                if (nextState.word.is_active(timestamp))
                {
                    results.emplace_back(arc, nextState.word.length);
                }
            }
        }

        for (ssize_t stateId : visitor.states[currentStateIndex])
        {
            NfaStateType<MapType>& state = this->states[stateId];
            ssize_t nextStateId = state.get_arc(input);
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
            if (nextState.word.is_active(timestamp))
            {
                results.emplace_back(arc, nextState.word.length);
            }
        }

        for (ssize_t stateId : visitor.states[currentStateIndex])
        {
            NfaStateType<MapType>& state = this->states[stateId];
            ssize_t nextStateId = state.get_arc(input);
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
