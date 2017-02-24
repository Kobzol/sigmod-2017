#pragma once

#include <atomic>

class NgramHash
{
public:
    NgramHash(unsigned int index, DictHash hash) : index(index), hash(hash)
    {

    }

    unsigned int index;
    DictHash hash;
};

class Query
{
public:
    Query()
    {
        this->result.reserve(1000);
    }
    Query(size_t timestamp): timestamp(timestamp)
    {

    }
    Query(const Query& other)
    {
        this->timestamp = other.timestamp;
        this->document = other.document;
        this->result = other.result;
        this->job_finished.store(false);
    }

    void reset(size_t timestamp)
    {
        this->timestamp = timestamp;
        this->result.clear();
        this->job_finished.store(false);
    }

    size_t timestamp;
    std::string document;
    std::string result;
    std::atomic<bool> job_finished{false};
};
