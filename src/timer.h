#pragma once

#include <chrono>

class Timer
{
public:
    void start()
    {
        this->point = std::chrono::steady_clock::now();
    }
    long stop()
    {
        auto elapsed = std::chrono::steady_clock::now() - this->point;
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    }
    long add()
    {
        this->total += this->stop();
        return this->total;
    }

    long total = 0;
    std::chrono::time_point<std::chrono::steady_clock> point;
};
