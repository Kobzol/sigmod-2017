#pragma once

#include <chrono>

class Timer
{
public:
    void start()
    {
        this->point = std::chrono::steady_clock::now();
    }
    long get()
    {
        auto elapsed = std::chrono::steady_clock::now() - this->point;
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    }
    long add()
    {
        this->total += this->get();
        return this->total;
    }
    void reset()
    {
        this->total = 0;
        this->start();
    }

    long total = 0;
    std::chrono::time_point<std::chrono::steady_clock> point;
};
