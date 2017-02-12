#include "nfa.h"

ssize_t* linearMap;

void initLinearMap()
{
    linearMap = new ssize_t[LINEAR_MAP_SIZE];
    for (int i = 0; i < LINEAR_MAP_SIZE; i++)
    {
        linearMap[i] = NO_ARC;
    }
}
