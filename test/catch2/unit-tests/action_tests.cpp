// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <thread>
#include <catch2/catch_all.hpp>
#include <bw/webthing/action.hpp>

using namespace bw::webthing;

TEST_CASE( "Webthing actions have an json representation", "[action][json]" )
{
    FIXED_TIME_SCOPED("2023-02-17T01:23:45.000+00:00");

    Action action("test-action-123", ActionBehavior(), "test-action-name", "test-action-input");
    action.set_href_prefix("test-base");

    auto expected_json = json::parse(R"(
        {"test-action-name": {
            "href": "test-base/actions/test-action-name/test-action-123",
            "timeRequested": "2023-02-17T01:23:45.000+00:00",
            "status": "created",
            "input": "test-action-input"
        }}
    )");

    REQUIRE( action.as_action_description() == expected_json );
}

TEST_CASE( "Webthing specifies action status message fromat", "[action][json]" )
{
    FIXED_TIME_SCOPED("2023-02-17T02:34:56.000+00:00");

    Action action("#456", ActionBehavior(), "test-action-name", "test-action-input");
    action.set_href_prefix("test-base");

    auto expected_json = json::parse(R"(
        {
            "messageType": "actionStatus",
            "data": {
                "test-action-name": {
                    "href": "test-base/actions/test-action-name/#456",
                    "timeRequested": "2023-02-17T02:34:56.000+00:00",
                    "status": "created",
                    "input": "test-action-input"
                }
            }
        }
    )");

    REQUIRE( action_status_message(action) == expected_json );
}

SCENARIO( "actions have a stateful lifecycle", "[action]" )
{
    FIXED_TIME_SCOPED("2025-02-17T02:34:56.000+00:00");


    GIVEN("a custom action")
    {
        struct CustomAction : public Action
        {
            CustomAction(void* thing_void_ptr)
                : Action("abc123", {
                    /*notify_thing*/[](json j){
                        logger::info("THING GOT " + j.dump());
                    },
                    /*perform_action*/[&]{
                        logger::info("CustomAction perform action with input:" + get_input().value_or(json()).dump());

                        auto start_time = std::chrono::steady_clock::now();
                        auto max_duration = std::chrono::milliseconds(300);

                        while (!cancel_work && std::chrono::steady_clock::now() - start_time < max_duration)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                        work_done = !cancel_work;
                    },
                    /*canel_action*/[&]{
                        logger::info("CustomAction cancel action" + get_input().value_or(json()).dump());
                        cancel_work = true;
                    },
                    /*get_thing*/[&]{
                        return thing_ptr;
                    }
                }, "my-custom-action", json({"a", "b", "c"}))
                , thing_ptr(thing_void_ptr)
            {}
            void* thing_ptr = nullptr;
            bool cancel_work = false;
            bool work_done = false;
        };
    
        struct TestThing
        {
        } thing;
        CustomAction action(&thing);

        REQUIRE( action.get_id() == "abc123" );
        REQUIRE( action.get_name() == "my-custom-action" );
        REQUIRE( action.get_status() == "created" );
        REQUIRE( action.get_href() == "/actions/my-custom-action/abc123" );
        REQUIRE( action.get_thing<TestThing>() == &thing );
        REQUIRE( action.get_time_requested() == "2025-02-17T02:34:56.000+00:00" );
        REQUIRE_FALSE( action.get_time_completed() );

        WHEN("action is performed successfully")
        {
            std::thread t([&](){
                action.start();
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            REQUIRE( action.get_status() == "pending" );
            
            if(t.joinable())
                t.join();

            THEN("action will be considered completed and work to be done")
            {
                REQUIRE( action.get_status() == "completed" );
                REQUIRE( action.work_done );
                REQUIRE( action.get_time_completed() == "2025-02-17T02:34:56.000+00:00" );
            }
        }

        WHEN("action was canceled")
        {
            std::thread t([&](){
                action.start();
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            REQUIRE( action.get_status() == "pending" );
            
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            action.cancel();

            if(t.joinable())
                t.join();

            THEN("action will be considered completed but work to be unfinsihed")
            {
                REQUIRE( action.get_status() == "completed" );
                REQUIRE_FALSE( action.work_done );
                REQUIRE( action.get_time_completed() == "2025-02-17T02:34:56.000+00:00" );
            }
        }
    }

}