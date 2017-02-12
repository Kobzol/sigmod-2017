#pragma once

#include <string>
#include <atomic>

class Query
{
public:
    Query(std::string document, size_t timestamp): timestamp(timestamp), document(document), jobFinished{0}
    {

    }

    Query(const Query& rhs)
    {
        this->timestamp = rhs.timestamp;
        this->document = rhs.document;
        this->result = result;
        this->jobFinished.store(rhs.jobFinished);
    }

    size_t timestamp;
    std::string document;
    std::string result;
    std::atomic<bool> jobFinished;
};
