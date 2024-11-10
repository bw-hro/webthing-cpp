// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace bw::webthing {

template<class T>
class SimpleRingBuffer
{
public:
    SimpleRingBuffer(size_t capacity = SIZE_MAX, bool write_protected = false)
        : max_size(capacity)
        , current_size(0)
        , start_pos(0)
    {
        if(max_size < SIZE_MAX)
            buffer.reserve(max_size);

        if(write_protected)
            mutex = std::make_unique<std::mutex>();
    }

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
        std::unique_ptr<std::scoped_lock<std::mutex>> lock;
        if(mutex)
            lock = std::make_unique<std::scoped_lock<std::mutex>>(*mutex);

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

    auto begin() const { return Iterator(this, 0); }
    auto end() const { return Iterator(this, current_size); }

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

    struct Iterator
    {
        Iterator(const SimpleRingBuffer* buffer, size_t position)
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

        T operator*() const
        {
            return buffer->get(position);
        }

    private:
        const SimpleRingBuffer* buffer;
        size_t position;
    };
};

} // bw::webthing
