#include "nfa.h"

bool operator==(const Edge& e1, const Edge& e2)
{
    return e1.hashes == e2.hashes && e1.stateIndex == e2.stateIndex;
}
