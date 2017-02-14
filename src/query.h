#pragma once

class Query
{
public:
    Query()
    {

    }
    Query(size_t timestamp): timestamp(timestamp)
    {

    }

    size_t timestamp;
    std::string document;
    std::string result;
};
