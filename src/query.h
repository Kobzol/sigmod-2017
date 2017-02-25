#pragma once

class NgramHash
{
public:
    NgramHash(unsigned int index, DictHash hash) : index(index), hash(hash)
    {

    }

    unsigned int index;
    DictHash hash;
};

class QueryRegion
{
public:
    QueryRegion(int queryIndex, int from, int to) : queryIndex(queryIndex),
                                                    from(from),
                                                    to(to)
    {

    }

    int queryIndex;
    int from;
    int to;
    std::string result;
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

    void reset(size_t timestamp)
    {
        this->timestamp = timestamp;
        this->result.clear();
    }

    size_t timestamp;
    std::string document;
    std::string result;
};
