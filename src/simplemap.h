#pragma once

#include <cstdlib>
#include <vector>
#include "settings.h"


template <typename Key, typename Value>
class SimpleMapNode
{
public:
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
    SimpleMap(size_t capacity, int preallocate = 0) : capacity(capacity), count(0)  // capacity has to be a power of two
    {
        this->nodes = new std::vector<SimpleMapNode<Key, Value>>[capacity];

        if (preallocate > 0)
        {
            for (size_t i = 0; i < this->capacity; i++)
            {
                this->nodes[i].reserve(preallocate);
            }
        }
    }
    ~SimpleMap()
    {
        //delete[] this->nodes;
    }

    SimpleMap(const SimpleMap&) = delete;
    SimpleMap& operator=(const SimpleMap&) = delete;

    void insert(const Key& key, Value value)
    {
        size_t hash = fnv(key) & (this->capacity - 1);

        this->nodes[hash].emplace_back(key, value);
        this->count++;
    }
    void insert_hash(const Key& key, Value value, size_t hash)
    {
        hash = hash & (this->capacity - 1);

        this->nodes[hash].emplace_back(key, value);
        this->count++;
    }
    Value get(const Key& key) const
    {
        size_t hash = fnv(key) & (this->capacity - 1);

        std::vector<SimpleMapNode<Key, Value>>& node = this->nodes[hash];
        SimpleMapNode<Key, Value>* start = node.data();
        SimpleMapNode<Key, Value>* end = start + node.size();

        while (start < end)
        {
            if (start->key == key) return start->value;
            start++;
        }

        return HASH_NOT_FOUND;
    }
    Value get_hash(const Key& key, size_t hash) const
    {
        hash = hash & (this->capacity - 1);

        std::vector<SimpleMapNode<Key, Value>>& node = this->nodes[hash];
        SimpleMapNode<Key, Value>* start = node.data();
        SimpleMapNode<Key, Value>* end = start + node.size();

        while (start < end)
        {
            if (start->key == key) return start->value;
            start++;
        }

        return HASH_NOT_FOUND;
    }

    size_t size() const
    {
        return this->count;
    }

    std::hash<std::string> hash_fn;
    std::vector<SimpleMapNode<Key, Value>>* nodes;
    size_t capacity;
    size_t count;
};
