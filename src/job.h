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
        this->queries = new Job[JOBS_SIZE];
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
    Job* get_jobs(int& count)
    {
#ifdef USE_MUTEX
        if (this->get_available_job_count() == 0) return nullptr;

        LOCK(this->flag);

        if (this->get_available_job_count() == 0)
        {
            UNLOCK(this->flag);
            return nullptr;
        }
#else
        if (this->flag.test_and_set())
        {
            return nullptr;
        }
        if (this->get_available_job_count() == 0)
        {
            UNLOCK(this->flag);
            return nullptr;
        }
#endif

        count = this->get_available_job_count();
        Job* job = this->queries + this->workerIndex;
        this->workerIndex += count;

        UNLOCK(this->flag);

        return job;
    }

    inline int get_available_job_count() const
    {
        size_t freeCount = this->index - this->workerIndex;
        return std::min(JOB_BATCH_SIZE, (int) freeCount);
    }

    bool is_finished()
    {
        return this->workerCompleted == this->index;
    }

    std::atomic<size_t> workerIndex{0};
    std::atomic<size_t> workerCompleted{0};
    std::atomic<size_t> index{0};
    Job* queries;

    LOCK_INIT(flag);
};
