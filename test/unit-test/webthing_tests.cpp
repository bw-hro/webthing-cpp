// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/webthing.hpp>

using namespace bw::webthing;

TEST_CASE( "It offers an easy api for 'Thing' creation.", "[api][thing]" )
{
    SECTION( "Generate id and title" )
    {
        FIXED_UUID_SCOPED("TEST_THING_UUID");

        auto thing = make_thing();

        REQUIRE( thing->get_id() == "uuid:TEST_THING_UUID" );
        REQUIRE( thing->get_id() == "uuid:TEST_THING_UUID" );
    }

    SECTION( "Only provide an id" )
    {
        auto thing = make_thing("my-test-id");

        REQUIRE( thing->get_id() == "my-test-id" );
        REQUIRE( thing->get_title() == "my-test-id" );
    }

    SECTION( "Only provice a single type for the Thing" )
    {
        auto thing = make_thing("uri:dev:ops:abc-123", "test-title", "MyTestType", "The description.");

        REQUIRE( thing->get_id() == "uri:dev:ops:abc-123" );
        REQUIRE( thing->get_title() == "test-title" );
        REQUIRE( thing->get_type().size() == 1);
        REQUIRE_THAT( thing->get_type(), Catch::Matchers::Contains("MyTestType"));
        REQUIRE( thing->get_description() == "The description.");
    }
}

TEST_CASE( "It offers an easy api for 'Value' creation.", "[api][value]" )
{
    SECTION( "pre initialized value" )
    {
        auto v = make_value(123);
        REQUIRE(v->get() == 123);

        v->set(42);
        REQUIRE(v->get() == 42);
    }

    SECTION( "pre initialized value with forwarder" )
    {
        std::string forwarded;

        auto v = make_value(std::string("first-string"), [&](auto v){ forwarded = v;});
        REQUIRE(v->get() == "first-string");
        REQUIRE( forwarded == "" );

        v->set("second-string");
        REQUIRE(v->get() == "second-string");
        REQUIRE( forwarded == "second-string" );
    }

    SECTION( "not initialized value" )
    {
        auto v = make_unknown_value<double>();
        REQUIRE(v->get() == std::nullopt);

        v->set(3.14);
        REQUIRE(v->get() == 3.14);
    }

    SECTION( "not initialized value with forwarder" )
    {
        std::string forwarded;

        auto v = make_unknown_value<bool>([&](auto v){ forwarded = v ? "true" : "false";});
        REQUIRE(v->get() == std::nullopt);
        REQUIRE( forwarded == "" );

        v->set(true);
        REQUIRE(v->get() == std::optional<bool>(true));
        REQUIRE( forwarded == "true" );
    }
}

TEST_CASE( "It offers an easy api for 'Property' creation and linkage.", "[api][property]" )
{
    auto thing = make_thing();
    
    SECTION( "Create property from initial value" )
    {
        link_property(thing, "int-prop", 42);
        REQUIRE(thing->get_property<int>("int-prop") == 42);

        link_property(thing, "float-prop", 3.14f, {{"title", "Float Property"}});
        REQUIRE_THAT(*thing->get_property<float>("float-prop"),
            Catch::Matchers::WithinRel(3.14f));
    }

    SECTION( "Create property by passing 'Value' object" )
    {
        auto value = make_unknown_value<std::string>();
        
        link_property(thing, "string-prop", value);
        REQUIRE(thing->get_property<std::string>("string-prop") == std::nullopt);
        
        thing->set_property("string-prop", std::string("first-value"));
        REQUIRE(thing->get_property<std::string>("string-prop") == "first-value");

        value->notify_of_external_update("second-value");
        REQUIRE(thing->get_property<std::string>("string-prop") == "second-value");
    }
}

TEST_CASE( "It offers an easy api for event linkage as well as emitting events.", "[api][event]" )
{
    auto thing = make_thing();

    SECTION( "Build-in events" )
    {
        link_event(thing, "event-name", R"({"title":"Event Title", "input":{"type": "number"}})"_json);
        emit_event(thing, "event-name", 42);
        json events = thing->get_event_descriptions("event-name");
        REQUIRE(events[0]["event-name"]["data"] == 42);
    }

    SECTION( "Custom events" )
    {
        struct CustomEvent : public Event
        {
            CustomEvent(std::shared_ptr<Thing> thing)
                : Event(thing.get(), "custom-event", "my-string-input")
            {}
        };

        link_event(thing, "custom-event");
        emit_event(thing, CustomEvent(thing));
        json events = thing->get_event_descriptions();
        REQUIRE(events[0]["custom-event"]["data"] == "my-string-input");
    }
}

TEST_CASE( "It offers an easy api for action linkage as well as action execution.", "[api][action]" )
{
    SECTION( "Build-in actions" )
    {
        auto thing = make_thing();

        link_action(thing.get(), "action-name", R"({"title":"Action Title", "input":{"type": "number"}})"_json);
        auto action_1 = thing->perform_action("action-name", 42);        
        REQUIRE(action_1);
        REQUIRE(action_1->get_name() == "action-name");
        REQUIRE(action_1->get_input() == 42);

        link_action(thing.get(), "no-input-action");
        auto action_2 = thing->perform_action("no-input-action");
        REQUIRE(action_2);
        REQUIRE(action_2->get_name() == "no-input-action");
        REQUIRE_FALSE(action_2->get_input()); 
    }

    SECTION( "Custom action" )
    {
        struct CancelableTestAction : public Action
        {
            CancelableTestAction(Thing* thing, std::optional<json> input)
                : Action(generate_uuid(), thing, this, "cancelable-action", input)
            {

            }
            void perform_action()
            {
                logger::info("test-action performed");
            }

            void cancel_action()
            {
                logger::info("test-action canceled");
            }
        };
        auto thing = make_thing();
        link_action<CancelableTestAction>(thing, "cancelable-action");
        auto ca = thing->perform_action("cancelable-action");
        ca->start();
        ca->cancel();
        
        struct SimpleTestAction : public Action
        {
            SimpleTestAction(Thing* thing, std::optional<json> input)
                : Action(generate_uuid(), thing, this, "simple-test-action", input)
            {
            }
            
            void perform_action()
            {
                logger::info("test-action performed - " + get_input()->dump());
                get_thing<Thing>()->set_property<int>("prop-for-action", *get_input());
            }
        };

        link_property(thing, "prop-for-action", 123);
        link_action<SimpleTestAction>(thing, "simple-test-action", {{"title","Simple Test Action"}});

        auto pa = thing->perform_action("simple-test-action", 42);
        pa->start();
        pa->cancel();

        // REQUIRE(action_1->get_name() == "custom-action");
        // REQUIRE(action_1->get_input() == "my-custom-input");
    }

    SECTION( "Action within custom thing" )
    {
        struct CustomThing : public Thing
        {
            struct CustomAction : public Action
            {
                CustomAction(Thing* thing, std::optional<json> input)
                    : Action(generate_uuid(), thing, this, "custom-action-name", input)
                {
                }
                
                void perform_action()
                {
                    logger::info("CustomAction performed - " + get_input()->dump());
                    get_thing<CustomThing>()->custom_fun_for_action(*get_input());
                }
            };

            CustomThing()
                : Thing("id", "my-title")
            {
                link_property(this, "my-prop", std::string("initial_string"));
                link_action<CustomAction>(this, "custom-action-name");
            }

            void custom_fun_for_action(std::string input)
            {
                logger::warn("MY SUPER CUSTOM ACTION");
                set_property("my-prop", input);
            }
        };

        auto ct = std::make_shared<CustomThing>();
        auto ca = ct->perform_action("custom-action-name", "my-string-input");
        ca->start();
    }

    SECTION( "A varity of custom action constructors is supported")
    {
        auto thing = make_thing("thing-for-action-construtor-test");

        SECTION( "A custom action can ignore input" )
        {
            struct CustomActionIgnoreInput : public Action
            {   
                CustomActionIgnoreInput(Thing* thing)
                    : Action(generate_uuid(), thing, this, "no-action-input")
                {
                }

                void perform_action()
                {
                    logger::info("perform input ignoring action");
                }
            };

            link_action<CustomActionIgnoreInput>(thing, "no-action-input");
            auto action = thing->perform_action("no-action-input");
            REQUIRE(action);
            action->start();
        }

        SECTION( "A custom action can insist an on input" )
        {
            struct CustomActionInsistInput : public Action
            {   
                CustomActionInsistInput(Thing* thing, int input)
                    : Action(generate_uuid(), thing, this, "insist-on-action-input", input)
                {
                }

                void perform_action()
                {
                    logger::info("perform input insisting action with: " + get_input()->dump());
                }
            };

            link_action<CustomActionInsistInput>(thing, "insist-on-action-input");

            // input missing
            REQUIRE_FALSE(thing->perform_action("insist-on-action-input"));
            // wrong input type
            REQUIRE_FALSE(thing->perform_action("insist-on-action-input", "a-failing-string"));

            auto action = thing->perform_action("insist-on-action-input", 42);
            REQUIRE(action);
            REQUIRE(action->get_input() == 42);
            action->start();
        }

    }
}