#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "word.h"
#include "util.h"

#define NOT_FINAL_STATE ((ssize_t) -1)
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
#ifdef PRINT_STATISTICS
        this->size++;
#endif
    }

#ifdef PRINT_STATISTICS
    size_t get_size() const
    {
        return this->size;
    }

    size_t size = 0;
#endif

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
        return this->map.get_arc(input);
    }

    void add_arc_linear(const MapType& input, size_t index)
    {
        this->map.add_arc(input, index);

        if (this->map.get_size() > MAX_LINEAR_MAP_SIZE)
        {
            int size = (int) this->map.keys.size();
            for (int i = 0; i < size; i++)
            {
                this->hashMap.add_arc(this->map.keys[i].first, this->map.keys[i].second);
            }

            this->get_fn = &CombinedNfaState<MapType>::get_arc_hash;
            this->add_fn = &CombinedNfaState<MapType>::add_arc_hash;
        }
    }

    ssize_t get_arc_hash(const MapType& input) const
    {
        return this->hashMap.get_arc(input);
    }

    void add_arc_hash(const MapType& input, size_t index)
    {
        this->hashMap.add_arc(input, index);
    }

    size_t get_size() const
    {
        return this->map.get_size();
    }

    LinearNfaState<MapType> map;
    HashNfaState<MapType> hashMap;

    ssize_t (CombinedNfaState<MapType>::*get_fn)(const MapType& input) const;
    void (CombinedNfaState<MapType>::*add_fn)(const MapType& input, size_t index);

    Word word;
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

    /*void addWord(Word& word, size_t wordIndex)
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
            arc = (this->states[activeState].*(this->states[activeState].get_fn))(prefix);

            if (arc == NO_ARC)
            {
                size_t stateId = this->createState();
                CombinedNfaState<MapType>& state = this->states[activeState];
                (state.*(state.add_fn))(prefix, stateId);
                activeState = stateId;
            }
            else activeState = arc;
        }

        this->states[activeState].wordIndex = wordIndex;
    }*/

    void feedWord(NfaVisitor& visitor, MapType input, std::vector<std::pair<unsigned int, unsigned int>>& results, size_t timestamp)
    {
        size_t currentStateIndex = visitor.stateIndex;
        size_t nextStateIndex = 1 - currentStateIndex;

        visitor.states[nextStateIndex].clear();

        ssize_t arc = this->rootState.get_arc(input);
        if (arc != NO_ARC)
        {
            visitor.states[nextStateIndex].push_back(arc);
            CombinedNfaState<MapType>& nextState = this->states[arc];
            if (nextState.word.is_active(timestamp))
            {
                results.emplace_back(arc, nextState.word.length);
            }
        }

        for (ssize_t stateId : visitor.states[currentStateIndex])
        {
            CombinedNfaState<MapType>& state = this->states[stateId];
            ssize_t nextStateId = (state.*(state.get_fn))(input);
            if (nextStateId != NO_ARC)
            {
                visitor.states[nextStateIndex].push_back(nextStateId);

                CombinedNfaState<MapType>& nextState = this->states[nextStateId];
                if (nextState.word.is_active(timestamp))
                {
                    results.emplace_back(nextStateId, nextState.word.length);
                }
            }
        }

        visitor.stateIndex = nextStateIndex;
    }

    LinearMapNfaState<MapType> rootState;
    std::vector<CombinedNfaState<MapType>> states;

    size_t createState()
    {
        this->states.emplace_back();
        return this->states.size() - 1;
    }
};
