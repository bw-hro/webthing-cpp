// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <stdexcept>

namespace bw::webthing {

class InvalidJson : public std::runtime_error
{
public:
    InvalidJson()
        : std::runtime_error("General json validation error")
    {}

    InvalidJson(std::string message)
        : std::runtime_error(message)
    {}  
};

class PropertyError : public std::runtime_error
{
public:
    PropertyError()
        : std::runtime_error("General property error")
    {}

    PropertyError(std::string message)
        : std::runtime_error(message)
    {}  
};

class ActionError : public std::runtime_error
{
public:
    ActionError()
        : std::runtime_error("General action error")
    {}

    ActionError(std::string message)
        : std::runtime_error(message)
    {}  
};

class EventError : public std::runtime_error
{
public:
    EventError()
        : std::runtime_error("General event error")
    {}

    EventError(std::string message)
        : std::runtime_error(message)
    {}  
};

} // bw::webthing