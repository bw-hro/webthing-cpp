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
    GIVEN("a custom action")
    {
        struct CustomAction : public Action
        {
            CustomAction() : Action("abc123", {
                    /*notify_thing*/[](json j){
                        std::cout << "THING GOT " << j << std::endl;
                    },
                    /*perform_action*/[&]{
                        std::cout << "CustomAction perform action with input:" << get_input().value_or(json()) << std::endl; 
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        work_done = true;
                    }
                }, "my-custom-action", json({"a", "b", "c"}))
            {}
            bool work_done = false;
        };

        CustomAction action;

        REQUIRE( action.get_id() == "abc123" );
        REQUIRE( action.get_status() == "created" );

        WHEN("action is performed successfully")
        {
            std::thread t([&](){
                action.start();
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            REQUIRE( action.get_status() == "pending" );
            
            if(t.joinable())
                t.join();

            THEN("action will be considered completed")
            {
                REQUIRE( action.get_status() == "completed" );
                REQUIRE( action.work_done );
            }
        }
    }

}