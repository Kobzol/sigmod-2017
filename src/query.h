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
    Query(std::string document, size_t timestamp): timestamp(timestamp), document(document)
    {

    }

    size_t timestamp;
    std::string document;
    std::string result;
};
