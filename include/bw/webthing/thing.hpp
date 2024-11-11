// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <vector>
#include <bw/webthing/action.hpp>
#include <bw/webthing/constants.hpp>
#include <bw/webthing/event.hpp>
#include <bw/webthing/json.hpp>
#include <bw/webthing/property.hpp>
#include <bw/webthing/storage.hpp>

namespace bw::webthing {

class Thing
{
public:
    typedef std::function<std::shared_ptr<Action> (std::optional<json> input)> ActionSupplier;
    struct AvailableAction
    {
        json metadata;
        ActionSupplier class_supplier;
    };

    typedef std::function<void(const std::string& /*topic*/, const json& /*message*/)> MessageCallback; 

    Thing(std::string id, std::string title, std::vector<std::string> type, std::string description = "")
        : id(id), title(title), type(type), description(description)
    {
    }

    Thing(std::string id, std::string title, std::string type = "", std::string description = "")
        : Thing(id, title, std::vector<std::string>{type}, description)
    {
    }

    json as_thing_description() const
    {
        json thing({
            {"id", id},
            {"title", title},
            {"@context", context},
            {"@type", type},
            {"properties", get_property_descriptions() },
            {"actions", json::object()},
            {"events", json::object()},
            {"description", description},
            {"links", {
                {{"rel", "properties"}, {"href", href_prefix + "/properties"}},
                {{"rel", "actions"}, {"href", href_prefix + "/actions"}},
                {{"rel", "events"}, {"href", href_prefix + "/events"}}
            }}
        });

        if(ui_href)
        {
            thing["links"].push_back({
                {"rel", "alternate"}, {"mediaType", "text/html"}, {"href", *ui_href}
            });
        }

        for(auto& aa : available_actions)
        {
            std::string name = aa.first;
            thing["actions"][name] = aa.second.metadata;
            thing["actions"][name]["links"] = {{{"rel", "action"}, {"href", href_prefix + "/actions/" + name}}}; 
        }

        for(auto& ae : available_events)
        {
            std::string name = ae.first;
            thing["events"][name] = ae.second;
            thing["events"][name]["links"] = {{{"rel", "event"}, {"href", href_prefix + "/events/" + name}}};
        }
        return thing;
    }

    std::string get_href() const
    {
        if(href_prefix.size() > 0)
            return href_prefix;
        return "/";
    }

    std::optional<std::string> get_ui_href() const
    {
        return ui_href;
    }

    void set_ui_href(std::string href)
    {
        ui_href = href;
    }

    std::string get_id() const
    {
        return id;
    }

    std::string get_title() const
    {
        return title;
    }

    std::string get_description() const
    {
        return description;
    }

    std::vector<std::string> get_type() const
    {
        return type;
    }

    std::string get_context() const
    {
        return context;
    }

    void set_context(std::string context)
    {
        this->context = context;
    }

    json get_property_descriptions() const
    {
        auto pds = json::object();
        for(const auto& p : properties)
            pds[p.first] = p.second->as_property_description();
        return pds;
    }

    // Get the thing's actions a json array
    // action_name -- Optional action name to get description for
    json get_action_descriptions(std::optional<std::string> action_name = std::nullopt) const
    {
        json descriptions = json::array();

        for(const auto& action_entry : actions)
            for(const auto& action : action_entry.second)
                if(!action_name || action_name ==  action_entry.first)
                    descriptions.push_back(action->as_action_description());

        return descriptions;
    }

    // Get the thing's events as a json array.
    // event_name -- Optional event name to get description for
    json get_event_descriptions(const std::optional<std::string>& event_name = std::nullopt) const
    {
        json descriptions = json::array();

        for(const auto& evt : events)
            if(!event_name || event_name == evt->get_name())
                descriptions.push_back(evt->as_event_description());

        return descriptions;
    }

    void add_property(std::shared_ptr<PropertyBase> property)
    {
        property->set_href_prefix(href_prefix);
        properties[property->get_name()] = property;
    }

    void remove_property(const PropertyBase& property)
    {
        properties.erase(property.get_name());
    }

    // Find a property by name
    std::shared_ptr<PropertyBase> find_property(std::string property_name) const
    {
        if(properties.count(property_name) > 0)
            return properties.at(property_name);
        return nullptr;
    }

    template<class T>
    void set_property(std::string property_name, T value)
    {
        auto prop = find_property(property_name);
        if(prop)
            prop->set_value(value);
    }

    template<class T>
    std::optional<T> get_property(std::string property_name) const
    {
        auto property = find_property(property_name);
        if(property)
            return property->get_value<T>();
        return std::nullopt;
    }

    // Get a mapping of all properties and their values.
    json get_properties() const
    {
        auto json = json::object();

        for(const auto& pe : properties)
        {
            json[pe.first] = pe.second->get_property_value_object()[pe.first];
        }
        return json;
    }

    //Determine whether or not this thing has a given property.
    // property_name -- the property to look for
    bool has_property(std::string property_name) const
    {
       return properties.find(property_name) != properties.end();
    }

    void property_notify(json property_status_message)
    {
        logger::debug("thing::property_notify : " + property_status_message.dump());
        for(auto& observer : observers)
            observer( id + "/properties", property_status_message);
    }

    // Perform an action on the thing.
    // name -- name of the action
    // input -- any action inputs 
    std::shared_ptr<Action> perform_action(std::string name, std::optional<json> input = std::nullopt)
    {
        if(available_actions.count(name) == 0)
            return nullptr;

        auto& action_type = available_actions[name];

        if(action_type.metadata.contains("input"))
        {
            try
            {
                validate_value_by_scheme(input.value_or(json()), action_type.metadata["input"]);
            }
            catch(std::exception& ex)
            {
                logger::debug("action: '" + name + "' invalid input: " + 
                    input.value_or(json()).dump() +" error: " + ex.what());
                return nullptr;
            }
        }

        try
        {
            auto action = action_type.class_supplier( std::move(input) );
            action->set_href_prefix(href_prefix);
            action_notify(action_status_message(action));
            actions[name].add(action);
            return action;
        }
        catch(std::exception& ex)
        {
            logger::debug("Construction of action '" + name + "' failed with error: " + ex.what());
            return nullptr;
        }
    }

    // Add an available action.
    // name -- name of the action
    // metadata -- action metadata, i.e. type, description, etc. as a json object
    // class_supplier -- function to instantiate this action
    void add_available_action(std::string name, json metadata, ActionSupplier class_supplier)
    {
        if(!metadata.is_object())
            throw ActionError("Action metadata must be encoded as json object.");

        available_actions[name] = { metadata, class_supplier };
        actions[name] = {action_storage_config};
    }

    void action_notify(json action_status_message)
    {
        logger::debug("thing::action_notify : " + action_status_message.dump());
        for(auto& observer : observers)
            observer( id + "/actions", action_status_message);
    }

    // Get an action by its name and id
    // return the action when found, std::nullopt otherwise
    std::shared_ptr<Action> get_action(std::string action_name, std::string action_id) const
    {
        if(actions.count(action_name) == 0)
            return nullptr;

        const auto& actions_for_name = actions.at(action_name);
        for(const auto& action : actions_for_name)
            if(action->get_id() == action_id)
                return action;

        return nullptr;
    }

    // Remove an existing action identified by its name and id
    // Returns bool indicating the presence of the action 
    bool remove_action(std::string action_name, std::string action_id)
    {
        auto action = get_action(action_name, action_id);
        if(!action)
            return false;
        
        action->cancel();
        auto& as = actions[action_name];
        as.remove_if([&action_id](auto a){return a->get_id() == action_id;});
        return true;
    }

    // Add a new event and notify subscribers
     void add_event(std::shared_ptr<Event> event)
     {
        events.add(event);
        event_notify(*event);
     }

    // Add an available event.
    // name -- name of the event
    // metadata -- event metadata, i.e. type, description, etc., as a json object
    void add_available_event(std::string name, json metadata = json::object())
    {
        if(!metadata.is_object())
            throw EventError("Event metadata must be encoded as json object.");

        available_events[name] = metadata;
    }

    void event_notify(const Event& event)
    {
        if(available_events.count(event.get_name()) == 0)
            return;

        json message = event_message(event);
        logger::debug("thing::event_notify : " + message.dump());

        for(auto& observer : observers)
            observer( id + "/events/" + event.get_name(), message);
    }

    // Set the prefix of any hrefs associated with this thing.
    void set_href_prefix(std::string prefix)
    {
        href_prefix = prefix;

        for(const auto& property : properties )
            property.second->set_href_prefix(prefix);

        for(auto& action_entry : actions)
            for(auto& action : action_entry.second)
                action->set_href_prefix(prefix);
    }

    void add_message_observer(MessageCallback observer)
    {
        observers.push_back(observer);
    }

    // configures the storage of events, should be set in initialization phase
    void configure_event_storage(const StorageConfig& config)
    {
        event_storage_config = config;
        events = {event_storage_config};
    }

    // configures the storage of actions, should be set in initialization phase
    // before actions are linked to the thing
    void configure_action_storage(const StorageConfig& config)
    {
        action_storage_config = config;
        for (auto& [action_name, actions] : actions)
        {
            actions = {action_storage_config};
        }
    }

protected:
    std::string id;
    std::string context = WEBTHINGS_IO_CONTEXT;
    std::string title;
    std::vector<std::string> type;
    std::string description;
    std::map<std::string, std::shared_ptr<PropertyBase>> properties;
    std::map<std::string, AvailableAction> available_actions;
    std::map<std::string, json> available_events;
    StorageConfig action_storage_config = {10000, true};
    std::map<std::string, FlexibleRingBuffer<std::shared_ptr<Action>>> actions;
    StorageConfig event_storage_config = {100000};
    SimpleRingBuffer<std::shared_ptr<Event>> events = {event_storage_config};
    std::string href_prefix;
    std::optional<std::string> ui_href;
    std::vector<MessageCallback> observers;
};

} // bw::webthing