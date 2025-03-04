// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/action.hpp>
#include <bw/webthing/event.hpp>
#include <bw/webthing/thing.hpp>

using namespace bw::webthing;

TEST_CASE( "Webthing thing context is configurable", "[context][thing]" )
{
    auto sut = std::make_shared<Thing>("uri::test.id", "my-test-thing");
    REQUIRE( sut->get_context() == WEBTHINGS_IO_CONTEXT ); // default

    sut->set_context("https://some.custom/context");
    REQUIRE( sut->get_context() == "https://some.custom/context" );
}

TEST_CASE( "Webthing thing validates description of available events", "[event][thing]" )
{
    auto types = std::vector<std::string>{"test-type"};
    auto sut = std::make_shared<Thing>("uri::test.id", "my-test-thing", types, "This is the description of my-test-thing");

    REQUIRE_NOTHROW(sut->add_available_event("test-event-a", {{"description", "Event A"}, {"type","string"}}));

    REQUIRE_THROWS_MATCHES(sut->add_available_event("test-event-b", {"\"JUST AN JSON STRING BUT NO OBJECT\""}),
    EventError, Catch::Matchers::Message("Event metadata must be encoded as json object."));
}

TEST_CASE( "Webthing thing stores all published events", "[event][thing]" )
{
    auto types = std::vector<std::string>{"test-type"};
    auto sut = std::make_shared<Thing>("uri::test.id", "my-test-thing", types, "This is the description of my-test-thing");

    sut->add_available_event("test-event-a", {{"description", "Event A"}, {"type","string"}});
    sut->add_available_event("test-event-b", {{"description", "Event B"}, {"type","string"}});
    sut->add_available_event("test-event-c", {{"description", "Event C"}, {"type","string"}});

    auto event1 = std::make_shared<Event>(sut.get(), "test-event-a", "test-data-1");
    auto event2 = std::make_shared<Event>(sut.get(), "test-event-b", "test-data-2");
    auto event3 = std::make_shared<Event>(sut.get(), "test-event-c", "test-data-3");
    auto event4 = std::make_shared<Event>(sut.get(), "test-event-a", "test-data-4");
    auto event5 = std::make_shared<Event>(sut.get(), "test-event-a", "test-data-5");

    sut->add_event(event1);
    sut->add_event(event2);
    sut->add_event(event3);
    sut->add_event(event4);
    sut->add_event(event5);

    REQUIRE( sut->get_event_descriptions().size() == 5 );
    REQUIRE( sut->get_event_descriptions("test-event-a").size() == 3 );
    REQUIRE( sut->get_event_descriptions("test-event-b").size() == 1 );
    REQUIRE( sut->get_event_descriptions("test-event-c").size() == 1 );
    REQUIRE( sut->get_event_descriptions("test-event-missing").size() == 0 );
}

TEST_CASE( "Webthing thing validates description of available actions", "[action][thing]" )
{
    auto types = std::vector<std::string>{"test-type"};
    auto sut = std::make_shared<Thing>("uri::test.id", "my-test-thing", types, "This is the description of my-test-thing");

    REQUIRE_NOTHROW(sut->add_available_action("test-action-a", {{"title", "Action A"}}, nullptr));

    REQUIRE_THROWS_MATCHES(sut->add_available_action("test-action-b", {"\"JUST AN JSON STRING BUT NO OBJECT\""}, nullptr),
    ActionError, Catch::Matchers::Message("Action metadata must be encoded as json object."));
}

TEST_CASE( "Webthing things performes actions", "[action][thing]" )
{
    struct TestThing : public Thing
    {
        struct CustomAction : public Action
        {
            CustomAction(ActionBehavior action_behavior, std::optional<json> input) 
                : Action(generate_uuid(), action_behavior, "my-custom-action", input)
            {}

            void perform_action()
            {
                std::cout << "CustomAction perform action with input:" << get_input().value_or(json()) << std::endl; 
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        };

        TestThing()
            : Thing("uri::test.id", "my-test-thing", std::vector<std::string>({"test-type"}), "This is the description of my-test-thing")
        {
            auto supplier = [&](auto input){
                ActionBehavior ab;
                ab.notify_thing = [&](auto action_status){this->action_notify(action_status);};
                return std::make_shared<CustomAction>(std::move(ab), input);
            };

            json action_meta = {
                {"title", "CustomAction"},
                {"description", "A custom action for test purposes."},
                {"input", {{"type", "string"}}}
            };

            this->add_available_action("my-custom-action", action_meta, supplier);
        }
    };

    auto test_thing = std::make_shared<TestThing>();
    
    auto ttd = test_thing->as_thing_description();
    logger::info(ttd.dump(2));
    REQUIRE( ttd["actions"].size() == 1 );

    { 
        FIXED_UUID_SCOPED("abc-123");
        test_thing->perform_action("my-custom-action", "42-as-string");
    }
    { 
        FIXED_UUID_SCOPED("def-456");
        test_thing->perform_action("my-custom-action", "my-input-string");
    }

    auto ads = test_thing->get_action_descriptions();
    logger::info(ads.dump(2));
    REQUIRE( ads.size() == 2 );
    REQUIRE( test_thing->get_action("my-custom-action", "abc-123")->get_status() == "created" );
    REQUIRE( test_thing->get_action("my-custom-action", "def-456")->get_status() == "created" );

    test_thing->remove_action("my-custom-action", "abc-123");
    REQUIRE_FALSE( test_thing->get_action("my-custom-action", "abc-123") );

    // try to perform unavailable action
    { 
        FIXED_UUID_SCOPED("ghi-789");
        REQUIRE_FALSE( test_thing->perform_action("some-unavailable-action", "my-input-string") );
    }
}

#ifdef WT_USE_JSON_SCHEMA_VALIDATION

void register_action(std::shared_ptr<Thing> thing, json action_meta)
{
    std::string action_title = action_meta["title"];
    thing->add_available_action(action_title, std::move(action_meta), [action_title, thing](auto input){ 
        ActionBehavior ab;
        ab.notify_thing = [thing](auto status){ thing->action_notify(status); };
        ab.perform_action = [action_title]{ logger::info("perform " + action_title); };
        return std::make_shared<Action>(generate_uuid(), std::move(ab), action_title, input);
    });
}

TEST_CASE( "A thing validates the action input before execution", "[action][thing]" )
{
    auto types = std::vector<std::string>{"test-type"};
    auto thing = std::make_shared<Thing>("uri::test.id", "my-test-thing", types, "This is the description of my-test-thing");

    SECTION( "No input required" )
    {
        register_action(thing, {{"title", "action-without-input"}});
        REQUIRE(thing->perform_action("action-without-input"));
        REQUIRE(thing->perform_action("action-without-input", 42));
        REQUIRE(thing->perform_action("action-without-input", json({{"key", "value"}})));
    }
    
    SECTION( "Integer input required" )
    {
        register_action(thing, {{"title", "action-with-number-input"}, 
            {"input", {{"type", "integer"}, {"minimum", 42}, {"maximum", 666}}}});

        // no input
        REQUIRE_FALSE(thing->perform_action("action-with-number-input"));
        // wrong type
        REQUIRE_FALSE(thing->perform_action("action-with-number-input", json({{"key", "value"}})));
        // under minimum
        REQUIRE_FALSE(thing->perform_action("action-with-number-input", 1));
        // over maximum
        REQUIRE_FALSE(thing->perform_action("action-with-number-input", 1000));

        REQUIRE(thing->perform_action("action-with-number-input", 123));
    }
    
    SECTION( "Object input required" )
    {
        register_action(thing, {{"title", "action-with-object-input"},
            {"input", {{"type", "object"}, {"required", {"color", "amount"}}}}});
        
        // wrong type
        REQUIRE_FALSE(thing->perform_action("action-with-object-input", 1));
        // wrong type
        REQUIRE_FALSE(thing->perform_action("action-with-object-input", true));
        // wrong object layout
        REQUIRE_FALSE(thing->perform_action("action-with-object-input", json({{"key", "value"}})));

        REQUIRE(thing->perform_action("action-with-object-input", json({{"color", "green"}, {"amount", 42}})));
    }
}

#endif