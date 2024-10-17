// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <optional>

namespace bw::webthing {

template<class T>
class Value
{
public:
    typedef std::function<void (const T&)> ValueForwarder;
    typedef std::function<void (const T&)> ValueChangedCallback;

    Value(std::optional<T> initial_value = std::nullopt, ValueForwarder value_forwarder = nullptr)
        : last_value(initial_value)
        , value_forwarder(value_forwarder)
    {
    }

    void set(T value)
    {
        if (value_forwarder)
            value_forwarder(value);
        notify_of_external_update(value);
    }

    std::optional<T> get() const
    {
        return last_value;
    }

    void notify_of_external_update(T value)
    {
        if (last_value != value)
        {
            last_value = value;
            notify_observers(value);
        }
    }

    void add_observer(ValueChangedCallback observer)
    {
        observers.push_back(observer);
    }

private:

    void notify_observers(T value)
    {
        for (auto& observer : observers)
        {
            observer(value);
        }
    }

    std::optional<T> last_value;
    ValueForwarder value_forwarder;
    std::vector<ValueChangedCallback> observers;
};

} // bw::webthing
