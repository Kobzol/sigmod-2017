#pragma once

#include <mutex>
#include <vector>
#include "settings.h"

class JobQueue
{
public:
    void add_query(size_t queryId)
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        this->queries.push_back(queryId);
    }
    ssize_t get_job()
    {
        std::unique_lock<std::mutex> lock(this->mutex);

        if (this->queries.size() > 0)
        {
            size_t job = this->queries.at(this->queries.size() - 1);
            this->queries.resize(this->queries.size() - 1);

            return job;
        }
        else return NO_JOB;
    }
private:
    std::vector<size_t> queries;
    std::mutex mutex;
};
