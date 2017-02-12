#pragma once

#include <thread>
#include <iostream>
#include <vector>
#include <mutex>
#include <chrono>

#include "settings.h"
#include "query.h"
#include "job.h"

JobQueue jobQueue;
extern std::vector<Query> queries;
void find_in_document(Query& query);


class Thread
{
public:
    Thread() : id(0), terminated(false), handle(std::thread(&Thread::thread_fn, this))
    {
        this->handle.detach();
    }

    void thread_fn()
    {
        while (!this->terminated)
        {
            ssize_t jobId = jobQueue.get_job();
            if (jobId != NO_JOB)
            {
                find_in_document(queries.at(jobId));
                queries.at(jobId).jobFinished = true;
            }
            else std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    size_t id;

private:
    bool terminated;
    std::thread handle;
};

class ThreadPool
{
public:
    ThreadPool()
    {
        for (size_t i = 0; i < THREAD_POOL_THREAD_COUNT; i++)
        {
            this->threads[i].id = i + 1;
        }
    }

private:
    Thread threads[THREAD_POOL_THREAD_COUNT];
};