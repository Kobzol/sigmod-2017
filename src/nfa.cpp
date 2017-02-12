#include "nfa.h"

std::atomic<int> foundArcAt0{0};
std::atomic<int> notFoundArcAt0{0};

ssize_t* linearMap;

void init_linear_map()
{
    /*linearMap = new ssize_t[LINEAR_MAP_SIZE];
    for (size_t i = 0; i < LINEAR_MAP_SIZE; i++)
    {
        linearMap[i] = NO_ARC;
    }*/
}
