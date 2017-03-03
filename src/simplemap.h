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
    Value get_or_insert_hash(const Key& key, size_t hash, Value value)
    {
        hash = hash & this->bitCapacity;

        while (true)
        {
            if (this->nodes[hash].key.size() == 0)
            {
                this->nodes[hash].key = key;
                this->nodes[hash].value = value;
                this->count++;
                return value;
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

template <typename Key, typename Value>
class SimpleMapChained
{
public:
    SimpleMapChained(size_t capacity, int preallocate = 0) : capacity(capacity), count(0)
    {
        this->nodes = new std::vector<SimpleMapNode<Key, Value>>[capacity];
        this->bitCapacity = capacity - 1;

        if (preallocate > 0)
        {
            for (size_t i = 0; i < this->capacity; i++)
            {
                this->nodes[i].reserve(preallocate);
            }
        }
    }
    ~SimpleMapChained()
    {
        //delete[] this->nodes;
    }

    SimpleMapChained(const SimpleMapChained&)
    {

    }
    SimpleMapChained& operator=(const SimpleMapChained&) = delete;

    void insert(const Key& key, Value value)
    {
        size_t hash = fnv(key) & this->bitCapacity;

        this->nodes[hash].emplace_back(key, value);
        this->count++;
    }
    void insert_hash(const Key& key, Value value, size_t hash)
    {
        hash = hash & this->bitCapacity;

        this->nodes[hash].emplace_back(key, value);
        this->count++;
    }
    Value get(const Key& key) const
    {
        size_t hash = fnv(key) & this->bitCapacity;

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
        hash = hash & this->bitCapacity;

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
    Value get_or_insert_hash(const Key& key, size_t hash, Value value)
    {
        hash = hash & this->bitCapacity;

        std::vector<SimpleMapNode<Key, Value>>& node = this->nodes[hash];
        SimpleMapNode<Key, Value>* start = node.data();
        SimpleMapNode<Key, Value>* end = start + node.size();

        while (start < end)
        {
            if (start->key == key) return start->value;
            start++;
        }

        node.emplace_back(key, value);
        this->count++;
        return value;
    }

    inline size_t size() const
    {
        return this->count;
    }

    std::vector<SimpleMapNode<Key, Value>>* nodes;
    size_t capacity;
    size_t bitCapacity;
    size_t count;
};
