// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <optional>
#include <bw/webthing/json.hpp>
#include <bw/webthing/utils.hpp>

namespace bw::webthing {

class Action;

json action_status_message(const Action& action);
json action_status_message(std::shared_ptr<Action> action);

struct ActionBehavior
{
    std::function<void (json)> notify_thing;
    std::function<void ()> perform_action;
    std::function<void ()> cancel_action;
    std::function<void* ()> get_thing;
};


template<class T> ActionBehavior make_action_behavior(T* thing,
    std::function<void ()> perform_action = nullptr,
    std::function<void ()> cancel_action = nullptr)
{
    return { 
        [thing](auto action_status){ thing->action_notify(action_status); },
        std::move(perform_action),
        std::move(cancel_action),
        [thing]{ return thing; }
    };
}

template <typename T, typename = void>
struct has_perform_action : std::false_type {};

template <typename T>
struct has_perform_action<T, std::void_t<decltype(&T::perform_action)>> : std::true_type {};

template <typename T, typename = void>
struct has_cancel_action : std::false_type {};

template <typename T>
struct has_cancel_action<T, std::void_t<decltype(&T::cancel_action)>> : std::true_type {};

template <typename T>
typename std::enable_if<has_cancel_action<T>::value>::type
execute_cancel_action(T& action_impl)
{
    (action_impl.*&T::cancel_action)();
}

template <typename T>
typename std::enable_if<!has_cancel_action<T>::value>::type
execute_cancel_action(T& action_impl)
{
    // action_impl has no cancel_action that could be executed
}

template<class T, class A> ActionBehavior make_action_behavior(T* thing, A* action_impl)
{
    return { 
        [thing](auto action_status){ thing->action_notify(action_status); },
        [action_impl]{ action_impl->perform_action(); },
        [action_impl]{ execute_cancel_action(*action_impl); },
        [thing]{ return thing; }
    };
}

//An Action represents an individual action on a thing.
class Action : public std::enable_shared_from_this<Action> 
{
public:
    Action(std::string id, ActionBehavior action_behavior, std::string name, std::optional<json> input = std::nullopt)
        : id(id)
        , action_behavior(action_behavior)
        , name(name)
        , input(input)
        , href("/actions/" + name + "/" + id)
        , status("created")
        , time_requested(bw::webthing::timestamp())
    {
    }

    template<class T, class A> Action(std::string id, T* thing, A* action_impl, std::string name, std::optional<json> input = std::nullopt)
        : Action(id, make_action_behavior(thing, action_impl), name, input)
    {
    }

    // Get the action description of the action as a json object.
    json as_action_description() const
    {
        json description;
        description[name]["href"] = href_prefix + href;
        description[name]["timeRequested"] = time_requested;
        description[name]["status"] = status;

        if(input)
            description[name]["input"] = *input;

        if(time_completed)
            description[name]["timeCompleted"] = *time_completed;

        return description;
    }

    // Set the prefix of any hrefs associated with this action.
    void set_href_prefix(const std::string& prefix)
    {
        href_prefix = prefix;
    }

    std::string get_id() const
    {
        return id;
    }

    std::string get_name() const
    {
        return name;
    }

    std::string get_href() const
    {
        return href_prefix + href;
    }

    std::string get_status() const
    {  
        return status;
    }

    std::string get_time_requested() const
    {
        return time_requested;
    }   

    std::optional<std::string> get_time_completed() const
    {
        return time_completed;
    }

    std::optional<json> get_input() const
    {
        return input;
    }

    template<class T> T* get_thing()
    {
        if(action_behavior.get_thing)
            return static_cast<T*>(action_behavior.get_thing());
        return nullptr;
    }

    // Start performing the action.
    void start()
    {
        status = "pending";
        notify_thing();
        perform_action();
        finish();
    }

    // Finish performing the action.
    void finish()
    {
        status = "completed";
        time_completed = timestamp();
        notify_thing();
    }

    void perform_action()
    {
        if(action_behavior.perform_action)
            action_behavior.perform_action();
    }

    void cancel()
    {
        if(action_behavior.cancel_action)
            action_behavior.cancel_action();
    }

private:
    void notify_thing()
    {
        if(action_behavior.notify_thing)
        {
            try
            {
                // Pass this as shared_ptr to ensure Action remains alive during callback execution
                // this might be necessary, as an Action is interacted with from different threads.
                action_behavior.notify_thing(action_status_message(shared_from_this()));
            }
            catch(std::bad_weak_ptr&)
            {
                action_behavior.notify_thing(action_status_message(*this));
            }
        }
    }

    std::string id;
    ActionBehavior action_behavior;
    std::string name;
    std::optional<json> input;
    std::string href_prefix;
    std::string href;
    std::string status;
    std::string time_requested;
    std::optional<std::string> time_completed;
};

inline json action_status_message(const Action& action)
{
    json description = action.as_action_description();
    return json({
        {"messageType", "actionStatus"}, 
        {"data", description}
    });
}

inline json action_status_message(std::shared_ptr<Action> action)
{
    return action_status_message(*action);
}

} // bw::webthing