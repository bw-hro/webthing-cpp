// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <vector>

namespace bw::webthing {

template<class T>
class SimpleRingBuffer
{
public:
    SimpleRingBuffer(size_t capacity = SIZE_MAX)
        : max_size(capacity)
        , current_size(0)
        , start_pos(0)
    {
        if(max_size < SIZE_MAX)
            buffer.reserve(max_size);
    }

    T get(size_t index) const
    {
        if (index >= current_size)
            throw std::out_of_range("Index out of range");

        return buffer[(start_pos + index) % max_size];
    }

    void add(T element)
    {
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
