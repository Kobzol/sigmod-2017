#pragma once

#include <atomic>

class Query
{
public:
    Query(std::string document, size_t timestamp): timestamp(timestamp), document(document)
    {

    }

    Query(const Query& query)
    {
        this->timestamp = query.timestamp;
        this->document = query.document;
        this->result = query.result;
    }
    Query& operator=(const Query& rhs) = delete;

    size_t timestamp;
    std::string document;
    std::string result;
    std::atomic<bool> jobFinished;
};
