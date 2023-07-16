// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/server.hpp>

using namespace bw::webthing;

TEST_CASE( "It can host a single thing" )
{
    Thing thing("uri:test:1", "single-thing");
    auto thing_container = SingleThing(&thing);

    auto server = WebThingServer::host(thing_container).port(57456).build();
    REQUIRE(server.get_name() == "single-thing");

    auto t = std::thread([&server]{
        std::this_thread::sleep_for(std::chrono::seconds(1));
        server.stop();
    });
    server.start();
    t.join();
}

TEST_CASE( "It can host a multiple things" )
{
    Thing thing_a("uri:test:a", "thing-a");
    Thing thing_b("uri:test:b", "thing-b");
    auto thing_container = MultipleThings({&thing_a, &thing_b}, "things-a-and-b");

    auto server = WebThingServer::host(thing_container).port(57123).build();
    REQUIRE(server.get_name() == "things-a-and-b");

    auto t = std::thread([&server]{
        std::this_thread::sleep_for(std::chrono::seconds(1));
        server.stop();
    });
    server.start();
    t.join();
}