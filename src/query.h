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

class Query
{
public:
    Query()
    {
        this->result.reserve(1000);
        //this->wordHashes.reserve(100);
    }
    Query(size_t timestamp): timestamp(timestamp)
    {

    }

    void reset(size_t timestamp)
    {
        this->timestamp = timestamp;
        this->result.clear();
        //this->wordHashes.clear();
    }

    size_t timestamp;
    std::string document;
    //std::vector<NgramHash> wordHashes;
    std::string result;
};
