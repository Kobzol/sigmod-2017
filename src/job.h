#pragma once

#include <mutex>
#include <vector>
#include <queue>
#include "settings.h"

class JobQueue
{
public:
    void add_query(size_t queryId)
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        this->queries.push(queryId);
    }
    ssize_t get_job()
    {
        if (this->queries.size() < 1)
        {
            return NO_JOB;
        }

        std::unique_lock<std::mutex> lock(this->mutex);

        if (this->queries.size() > 0)
        {
            size_t job = this->queries.front();
            this->queries.pop();

            return job;
        }
        else return NO_JOB;
    }
private:
    std::queue<size_t> queries;
    std::mutex mutex;
};
