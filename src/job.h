#pragma once

#include <mutex>
#include <vector>
#include <queue>
#include <atomic>
#include "settings.h"

#define JOBTYPE_ADD (1)
#define JOBTYPE_DELETE (2)

class Job
{
public:
    Job()
    {

    }
    Job(size_t timestamp, int type, std::string data) : timestamp(timestamp),
                                                        type(type),
                                                        data(data)
    {

    }

    size_t timestamp;
    int type;
    std::string data;
};

class JobQueue
{
public:
    JobQueue()
    {
        this->queries = new Job[1024 * 1024 * 10];
    }

    void start_batch()
    {
        this->index = 0;
        this->workerIndex = 0;
        this->workerCompleted = 0;
    }

    void add_query(size_t timestamp, int type, const std::string& data)
    {
        this->queries[this->index].timestamp = timestamp;
        this->queries[this->index].type = type;
        this->queries[this->index].data = data;
        this->index++;
    }
    Job* get_job()
    {
        if (this->workerIndex >= this->index) return nullptr;

        Job* job = this->queries + this->workerIndex;
        this->workerIndex++;

        return job;
    }
    bool is_finished()
    {
        return this->workerCompleted == this->index;
    }

    std::atomic<size_t> workerIndex{0};
    std::atomic<size_t> workerCompleted{0};
    std::atomic<size_t> index{0};
    Job* queries;
};
