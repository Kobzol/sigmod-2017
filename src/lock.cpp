#include "lock.h"

void waitsome(int iterations)
{
    volatile int sum = 0;
    for (int i = 0; i < iterations; i++)
    {
        sum += i * i;
    }
}
