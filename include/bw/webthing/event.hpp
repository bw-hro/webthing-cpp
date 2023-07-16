// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <optional>
#include <bw/webthing/json.hpp>
#include <bw/webthing/utils.hpp>

namespace bw::webthing {

//#include <bw/webthing/thing.hpp>
class Thing;

// An Event represents an individual event from a thing.
class Event
{
public:
    Event(Thing* thing, std::string name, std::optional<json> data = std::nullopt)
        : thing(thing)
        , name(name)
        , data(data)
        , time(bw::webthing::timestamp())
    {}

    // Get the event description of the event as a json object.
    json as_event_description() const
    {
        json description;
        description[name]["timestamp"] = time;

        if(data)
            description[name]["data"] = *data;

        return description;
    }

    Thing* get_thing() const
    {
        return thing;
    }

    std::string get_name() const
    {
        return name;
    }

    std::optional<json> get_data()
    {
        return data;
    }

    std::string get_time()
    {
        return time;
    }

private:
    Thing* thing;
    std::string name;
    std::optional<json> data;
    std::string time;
};

inline json event_message(const Event& event)
{
    json description = event.as_event_description();
    return json({
        {"messageType", "event"}, 
        {"data", description}
    });
}


} // bw::webthing