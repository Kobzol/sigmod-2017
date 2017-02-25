#pragma once

#include <cstdlib>
#include <vector>
#include "settings.h"


template <typename Key, typename Value>
class SimpleMapNode
{
public:
    SimpleMapNode() : key(Key()), value(Value()) { }
    SimpleMapNode(Key key, Value value) : key(key), value(value)
    {

    }

    Key key;
    Value value;
};

inline size_t fnv(const std::string& string)
{
    const char* str = string.c_str();
    int size = (int) string.size();

    char c;
    size_t hash = 0x811c9dc5;

    while (size-- && (c = *str++))
    {
        hash = hash ^ c;
        hash = hash * 16777619;
    }

    return hash;
}

inline size_t hashfn(const std::string& string)
{
    size_t hash = 5381;
    int c;
    const char* str = string.c_str();
    int size = (int) string.size();

    while (size-- && (c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

template <typename Key, typename Value>
class SimpleMap
{
public:
    SimpleMap(size_t capacity, int preallocate = 0) : capacity(capacity)  // capacity has to be a power of two
    {
        this->nodes = new SimpleMapNode<Key, Value>[capacity];
        this->bitCapacity = capacity - 1;
    }
    ~SimpleMap()
    {
        //delete[] this->nodes;
    }

    SimpleMap(const SimpleMap&) = delete;
    SimpleMap& operator=(const SimpleMap&) = delete;

    void insert(const Key& key, Value value)
    {
        size_t hash = fnv(key) & this->bitCapacity;

        while (true)
        {
            if (this->nodes[hash].key.size() == 0)
            {
                this->nodes[hash].key = key;
                this->nodes[hash].value = value;
                this->count++;
                return;
            }
            hash = (hash + 1) & this->bitCapacity;
        }
    }
    void insert_hash(const Key& key, Value value, size_t hash)
    {
        hash = hash & this->bitCapacity;

        while (true)
        {
            if (this->nodes[hash].key.size() == 0)
            {
                this->nodes[hash].key = key;
                this->nodes[hash].value = value;
                this->count++;
                return;
            }
            hash = (hash + 1) & this->bitCapacity;
        }
    }
    Value get(const Key& key) const
    {
        size_t hash = fnv(key) & this->bitCapacity;

        while (true)
        {
            if (this->nodes[hash].key.size() == 0)
            {
                return HASH_NOT_FOUND;
            }
            else if (this->nodes[hash].key == key)
            {
                return this->nodes[hash].value;
            }
            hash = (hash + 1) & this->bitCapacity;
        }
    }
    Value get_hash(const Key& key, size_t hash) const
    {
        hash = hash & this->bitCapacity;

        while (true)
        {
            if (this->nodes[hash].key.size() == 0)
            {
                return HASH_NOT_FOUND;
            }
            else if (this->nodes[hash].key == key)
            {
                return this->nodes[hash].value;
            }
            hash = (hash + 1) & this->bitCapacity;
        }
    }

    size_t size() const
    {
        return this->count;
    }

    SimpleMapNode<Key, Value>* nodes;
    size_t capacity;
    size_t bitCapacity;
    size_t count;
};
