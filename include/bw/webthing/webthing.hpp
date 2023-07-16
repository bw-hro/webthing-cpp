// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <bw/webthing/action.hpp>
#include <bw/webthing/errors.hpp>
#include <bw/webthing/event.hpp>
#include <bw/webthing/json.hpp>
#include <bw/webthing/json_validator.hpp>
#include <bw/webthing/mdns.hpp>
#include <bw/webthing/property.hpp>
#include <bw/webthing/server.hpp>
#include <bw/webthing/thing.hpp>
#include <bw/webthing/utils.hpp>
#include <bw/webthing/value.hpp>

namespace bw::webthing
{

std::shared_ptr<Thing> make_thing(std::string id, std::string title, std::vector<std::string> type, std::string description)
{
    if(id == "")
        id = "uuid:" + generate_uuid();
    if(title == "")
        title = id;
    return std::make_shared<Thing>(id, title, type, description);
}

std::shared_ptr<Thing> make_thing(std::string id = "", std::string title = "", std::string type = "", std::string description = "")
{
    std::vector<std::string> types;
    if(type != "")
        types.push_back(type);
    return make_thing(id, title, types, description);
}

template<class T> std::shared_ptr<Value<T>> make_value(T initial_value,  typename Value<T>::ValueForwarder value_forwarder = nullptr)
{
    return std::make_shared<Value<T>>(initial_value, std::move(value_forwarder));
}

template<class T> std::shared_ptr<Value<T>> make_unknown_value(typename Value<T>::ValueForwarder value_forwarder = nullptr)
{
    return std::make_shared<Value<T>>(std::nullopt, std::move(value_forwarder));
}

template<class T> std::shared_ptr<Property<T>> link_property(Thing* thing, std::string name, std::shared_ptr<Value<T>> value, json metadata = json::object())
{
    PropertyChangedCallback property_changed_callback = [thing](json property_status){
        thing->property_notify(property_status);
    };
    auto property = std::make_shared<Property<T>>(std::move(property_changed_callback), name, value, metadata);
    thing->add_property(property);
    return property;
}

template<class T> std::shared_ptr<Property<T>> link_property(Thing* thing, std::string name, T intial_value, json metadata = json::object())
{
    return link_property(thing, name, make_value(intial_value), metadata);
}

template<class T> std::shared_ptr<Property<T>> link_property(std::shared_ptr<Thing> thing, std::string name, std::shared_ptr<Value<T>> value, json metadata = json::object())
{
   return link_property(thing.get(), name, value, metadata);   
}

template<class T> std::shared_ptr<Property<T>> link_property(std::shared_ptr<Thing> thing, std::string name, T intial_value, json metadata = json::object())
{
    return link_property(thing, name, make_value(intial_value), metadata);
}

void link_event(Thing* thing, std::string name, json metadata = json::object())
{
    thing->add_available_event(name, metadata);
}

void link_event(std::shared_ptr<Thing> thing, std::string name, json metadata = json::object())
{
    link_event(thing.get(), name, metadata);
}

std::shared_ptr<Event> emit_event(Thing* thing, std::string name, std::optional<json> data = std::nullopt)
{
    auto event = std::make_shared<Event>(thing, name, data);
    thing->add_event(event);
    return event;
}

std::shared_ptr<Event> emit_event(std::shared_ptr<Thing> thing, std::string name, std::optional<json> data = std::nullopt)
{
    return emit_event(thing.get(), name, data);
}

std::shared_ptr<Event> emit_event(Thing* thing, Event&& event)
{
    auto event_ptr = std::make_shared<Event>(event);
    thing->add_event(event_ptr);
    return event_ptr;
}

std::shared_ptr<Event> emit_event(std::shared_ptr<Thing> thing, Event&& event)
{
    auto event_ptr = std::make_shared<Event>(event);
    thing->add_event(event_ptr);
    return event_ptr;
}

void link_action(Thing* thing, std::string action_name, json metadata,
    std::function<void()> perform_action = nullptr, std::function<void()> cancel_action = nullptr)
{
    Thing::ActionSupplier action_supplier = [thing, action_name, perform_action, cancel_action](auto input){
        return std::make_shared<Action>(generate_uuid(), 
            make_action_behavior(thing, perform_action, cancel_action),
            action_name, input);
    };

    thing->add_available_action(action_name, metadata, std::move(action_supplier));
}

void link_action(std::shared_ptr<Thing> thing, std::string action_name, json metadata,
    std::function<void()> perform_action = nullptr, std::function<void()> cancel_action = nullptr)
{
    link_action(thing.get(), action_name, metadata, perform_action, cancel_action);
}

void link_action(Thing* thing, std::string action_name, 
    std::function<void()> perform_action = nullptr, std::function<void()> cancel_action = nullptr)
{
    link_action(thing, action_name, json::object(), perform_action, cancel_action);
}

void link_action(std::shared_ptr<Thing> thing, std::string action_name, 
    std::function<void()> perform_action = nullptr, std::function<void()> cancel_action = nullptr)
{
    link_action(thing.get(), action_name, perform_action, cancel_action);
}

template<class ActionImpl> void link_action(Thing* thing, std::string action_name, json metadata = json::object())
{
    static_assert(has_perform_action<ActionImpl>::value, "ActionImpl does not have a perform_action method");

    Thing::ActionSupplier action_supplier;
    if constexpr(std::is_constructible_v<ActionImpl, Thing*, std::optional<json>>)
    {
        action_supplier = [thing](auto input){
            return std::make_shared<ActionImpl>(thing, input);
        };
    }
    else if constexpr(std::is_constructible_v<ActionImpl, Thing*, json>)
    {
        action_supplier = [thing](auto input){
            if(!input)
                throw ActionError("Input must not be empty for this Action type");
            return std::make_shared<ActionImpl>(thing, input.value_or(json()));
        };
    }
    else if constexpr(std::is_constructible_v<ActionImpl, Thing*>)
    {
        action_supplier = [thing](auto input){
            return std::make_shared<ActionImpl>(thing);
        };
    }
    else
    {
        throw std::runtime_error("ActionImpl does not provide a suitable constructor");
    }

    thing->add_available_action(action_name, metadata, std::move(action_supplier));
}

template<class ActionImpl> void link_action(std::shared_ptr<Thing> thing, std::string action_name, json metadata = json::object())
{
    link_action<ActionImpl>(thing.get(), action_name, metadata);
}

} // bw::webthing
