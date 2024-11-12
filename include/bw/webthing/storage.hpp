// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace bw::webthing {

struct StorageConfig
{
    size_t max_size = SIZE_MAX;
    bool write_protected = true;
};

// A simple ring buffer that overwrites oldest elements when max_size is reached.
// This ring buffer can not remove a subset of its elements. 
template<class T>
class SimpleRingBuffer
{
public:
    SimpleRingBuffer(size_t max_size = SIZE_MAX, bool write_protected = false)
        : max_size(max_size)
        , current_size(0)
        , start_pos(0)
    {
        if(max_size < SIZE_MAX)
            buffer.reserve(max_size);

        if(write_protected)
            mutex = std::make_unique<std::mutex>();
    }

    SimpleRingBuffer(const StorageConfig& config)
        : SimpleRingBuffer(config.max_size, config.write_protected)
    {}

    T& get(size_t index)
    {
        return buffer[resolve_index(index)];
    }

    const T& get(size_t index) const
    {
        return buffer[resolve_index(index)];
    }

    void add(T element)
    {
        auto lock = conditional_lock();

        if (current_size < max_size)
        {
            buffer.push_back(element);
            ++current_size;
        }
        else
        {
            buffer[start_pos] = element;
            start_pos = (start_pos + 1) % max_size;
        }
    }

    size_t size() const
    {
        return current_size;
    }

    auto begin() { return Iterator(this, 0); }
    auto end() { return Iterator(this, current_size); }
    auto begin() const { return ConstIterator(this, 0); }
    auto end() const { return ConstIterator(this, current_size); }

private:
    std::vector<T> buffer;
    size_t max_size;
    size_t current_size;
    size_t start_pos;
    std::unique_ptr<std::mutex> mutex;

    const size_t resolve_index(size_t index) const
    {
        if (index >= current_size)
            throw std::out_of_range("Index out of range");

        return (start_pos + index) % max_size;
    }

    std::unique_ptr<std::scoped_lock<std::mutex>> conditional_lock()
    {
        std::unique_ptr<std::scoped_lock<std::mutex>> lock;
        if(mutex)
            lock = std::make_unique<std::scoped_lock<std::mutex>>(*mutex);
        return lock;
    }

    struct Iterator
    {
        Iterator(SimpleRingBuffer* buffer, size_t position)
            : buffer(buffer)
            , position(position)
        {}

        bool operator!=(const Iterator& other) const
        {
            return position != other.position;
        }

        Iterator& operator++()
        {
            ++position;
            return *this;
        }

        T& operator*()
        {
            return buffer->get(position);
        }

    private:
        SimpleRingBuffer* buffer;
        size_t position;
    };

    struct ConstIterator
    {
        ConstIterator(const SimpleRingBuffer* buffer, size_t position)
            : buffer(buffer)
            , position(position)
        {}

        bool operator!=(const ConstIterator& other) const
        {
            return position != other.position;
        }

        ConstIterator& operator++()
        {
            ++position;
            return *this;
        }

        const T& operator*() const
        {
            return buffer->get(position);
        }

    private:
        const SimpleRingBuffer* buffer;
        size_t position;
    };
};

// A more feature rich ring buffer that overwrites oldest elements when max_size is reached.
// This ring buffer is able to remove a subset of its elements while maintaining the insertion order.
template<class T>
class FlexibleRingBuffer
{
public:
    FlexibleRingBuffer(size_t max_size = SIZE_MAX, bool write_protected = false)
        : max_size(max_size)
    {
        if(write_protected)
            mutex = std::make_unique<std::mutex>();
    }

    FlexibleRingBuffer(const StorageConfig& config)
        : FlexibleRingBuffer(config.max_size, config.write_protected)
    {}

    T& get(size_t index)
    {
        return buffer.at(index);
    }

    const T& get(size_t index) const
    {
        return buffer.at(index);
    }

    void add(T element)
    {
        auto lock = conditional_lock();
        buffer.push_back(element);
        if (size() > max_size)
            buffer.pop_front();
    }

    void remove_if(std::function<bool (const T& element)> predicate)
    {
        auto lock = conditional_lock();
        buffer.erase(std::remove_if(buffer.begin(), buffer.end(), predicate), buffer.end());
    }

    size_t size() const
    {
        return buffer.size();
    }
    auto begin() { return buffer.begin(); }
    auto end() { return buffer.end(); }
    auto begin() const { return buffer.begin(); }
    auto end() const { return buffer.end(); }

private:
    std::deque<T> buffer;
    size_t max_size;
    std::unique_ptr<std::mutex> mutex;

    std::unique_ptr<std::scoped_lock<std::mutex>> conditional_lock()
    {
        std::unique_ptr<std::scoped_lock<std::mutex>> lock;
        if(mutex)
            lock = std::make_unique<std::scoped_lock<std::mutex>>(*mutex);
        return lock;
    }
};

} // bw::webthing
