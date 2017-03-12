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
#define NO_EDGE ((int) -1)

struct Edge
{
    Edge()
    {

    }
    Edge(const std::string& text, int stateIndex) : text(text), stateIndex(stateIndex)
    {

    }
    std::string text;
    int stateIndex;
};
struct EdgeInfo
{
    EdgeInfo()
    {

    }
    EdgeInfo(int edgeIndex, int prefixLength) : edgeIndex(edgeIndex), prefixLength(prefixLength)
    {

    }
    int edgeIndex;
    int prefixLength;
};

static int get_prefix(const std::string& s1, const std::string& s2, int from)
{
    int size = (int) std::min(s1.size() - from, s2.size());
    int i = 0;
    for (; i < size; i++)
    {
        if (s1[i + from] != s2[i]) return i;
    }

    return i;
}


struct Mapping
{
    Mapping() : state(0), edge(NO_EDGE), index(0)
    {

    }
    Mapping(size_t state, int edge, unsigned int index): state(state), edge(edge), index(index)
    {

    }

    size_t state;
    int edge;
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
    // edge, prefix length
    EdgeInfo getPrefix(const std::string& input, int from)
    {
        int size = (int) this->map.size();
        for (int i = 0; i < size; i++)
        {
            int prefix = get_prefix(input, this->map[i].text, from);
            if (prefix > 0)
            {
                return { i, prefix };
            }
        }

        return { NO_ARC, NO_ARC };
    }

    std::vector<Edge> map;
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

    void addWord(const std::string& word, int start, int index)
    {
        int size = (int) word.size();
        int state = 0;
        while (start < size)
        {
            EdgeInfo edgeInfo = this->states[state].getPrefix(word, start);
            if (edgeInfo.edgeIndex == NO_ARC)
            {
                int newState = this->createState();
                this->states[state].map.emplace_back(word.substr(start), newState);
                state = newState;
                break;
            }
            else
            {
                Edge& edge = this->states[state].map[edgeInfo.edgeIndex];
                if ((int) edge.text.size() == edgeInfo.prefixLength)  // pass to next state, TODO: check if same string
                {
                    state = edge.stateIndex;
                }
                else    // split edge
                {
                    int edgeState = edge.stateIndex;
                    int newState = this->createState();

                    std::string remainder = edge.text.substr(edgeInfo.prefixLength);
                    this->states[newState].map.emplace_back(remainder, edgeState);

                    edge.stateIndex = newState;
                    edge.text.resize(edgeInfo.prefixLength);

                    state = newState;
                }

                start += edgeInfo.prefixLength;
            }
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
            if (mapping.edge == NO_EDGE)
            {
                for (size_t i = 0; i < state.map.size(); i++)
                {
                    Edge& edge = state.map[i];
                    if (edge.text[0] == c)
                    {
                        if (edge.text.size() > 1)
                        {
                            nextStates.emplace_back(mapping.state, i, 1);
                        }
                        else nextStates.emplace_back(edge.stateIndex, NO_EDGE, 0);
                        break;
                    }
                }
            }
            else
            {
                Edge& edge = state.map[mapping.edge];
                if (edge.text[mapping.index] == c)
                {
                    if (mapping.index + 1 < edge.text.size())
                    {
                        nextStates.emplace_back(mapping.state, mapping.edge, mapping.index + 1);
                    }
                    else
                    {
                        nextStates.emplace_back(edge.stateIndex, NO_EDGE, 0);
                    }
                }
            }
        }

        visitor.stateIndex = nextStateIndex;
    }

    unsigned int createState()
    {
        if (__builtin_expect(this->states.size() <= this->index, false))
        {
            this->states.resize(this->states.size() * 2);
        }
        return this->index++;
    }

    unsigned int index = 0;
    std::vector<CombinedNfaState> states;
};
