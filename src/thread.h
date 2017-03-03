#pragma once

#include <thread>
#include <iostream>
#include <vector>
#include <mutex>
#include <chrono>

#include "settings.h"
#include "query.h"
#include "job.h"

extern JobQueue jobQueue;

void delete_ngram(std::string& line, size_t timestamp);
void add_ngram(const std::string& line, size_t timestamp);

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
            int count = 0;
            Job* jobs = jobQueue.get_jobs(count);
            if (jobs != nullptr)
            {
                for (int i = 0; i < count; i++)
                {
                    Job* job = jobs + i;
                    //if (job->type == JOBTYPE_ADD) add_ngram(job->data, job->timestamp);
                    //else delete_ngram(job->data, job->timestamp);
                    add_ngram(job->data, job->timestamp);
                }

                jobQueue.workerCompleted += count;
            }
            else std::this_thread::sleep_for(std::chrono::nanoseconds(10));
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
        for (size_t i = 0; i < THREADPOOL_COUNT; i++)
        {
            this->threads[i].id = i + 1;
        }
    }

private:
    Thread threads[THREADPOOL_COUNT];
};
