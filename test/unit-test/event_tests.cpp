// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/event.hpp>

using namespace bw::webthing;

TEST_CASE( "Webthing events have an json representation", "[event][json]" )
{
    FIXED_TIME_SCOPED("2023-02-17T01:23:45.000+00:00");

    auto event = std::make_shared<Event>(nullptr, "test-event-name", "test-event-data");
    auto expected_json = json::parse(R"(
        {"test-event-name":{
            "timestamp":"2023-02-17T01:23:45.000+00:00",
            "data":"test-event-data"}
        }
    )");

    REQUIRE( event->as_event_description() == expected_json );
}