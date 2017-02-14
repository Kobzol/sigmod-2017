#pragma once

class Query
{
public:
    Query()
    {
        this->result.reserve(5000);
    }
    Query(size_t timestamp): timestamp(timestamp)
    {

    }

    size_t timestamp;
    std::string document;
    std::string result;
};
