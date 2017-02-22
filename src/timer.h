#pragma once

#include <chrono>
#include <time.h>
#include <sys/time.h>

double get_wall_time()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        //  Handle error
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

using TimerClock = std::chrono::high_resolution_clock;

class Timer
{
public:
    void start()
    {
        //this->point = TimerClock::now();
        this->point = get_wall_time();
    }
    double get()
    {
        return get_wall_time() - this->point;
        //auto elapsed = TimerClock::now() - this->point;
        //return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    }
    double add()
    {
        this->total += this->get();
        return this->total;
    }
    void reset()
    {
        this->total = 0;
        this->start();
    }

    double total = 0;
    //std::chrono::time_point<TimerClock> point;
    double point;
};
