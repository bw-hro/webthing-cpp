// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/event.hpp>
#include <bw/webthing/storage.hpp>

using namespace bw::webthing;

TEST_CASE( "SimpleRingBuffer must not have too much overhead compared to std::vector", "[.][benchmark][storage]" )
{
    int number_elements = 10000;
    
    BENCHMARK("SimpleRingBuffer with 256 limit")
    {
            SimpleRingBuffer<std::shared_ptr<Event>> storage(256); // limit
            for(int i = 0; i < number_elements; i++)
                storage.add(std::make_shared<Event>(nullptr, "test-event-" + std::to_string(i), "test-event-data"));
            return storage;
    };

    BENCHMARK("SimpleRingBuffer without limit"){
        SimpleRingBuffer<std::shared_ptr<Event>> storage; // no limit
        for(int i = 0; i < number_elements; i++)
            storage.add(std::make_shared<Event>(nullptr, "test-event-" + std::to_string(i), "test-event-data"));
        return storage;
    };

    BENCHMARK("std::vector"){
        std::vector<std::shared_ptr<Event>> storage; // no limitation
        for(int i = 0; i < number_elements; i++)
            storage.push_back(std::make_shared<Event>(nullptr, "test-event-" + std::to_string(i), "test-event-data"));
        return storage;
    };
}

TEST_CASE( "SimpleRingBuffer does not limit number of stored elements by default", "[storage]" )
{
    SimpleRingBuffer<std::shared_ptr<Event>> storage; // no limitation

    REQUIRE( storage.size() ==  0 );
    REQUIRE_THROWS( storage.get(0) );

    int number_elements = 1000000;
    for(int i = 0; i < number_elements; i++)
        storage.add(std::make_shared<Event>(nullptr, "test-event-" + std::to_string(i), "test-event-data"));

    REQUIRE( storage.size() ==  number_elements );
    REQUIRE( storage.get(0)->get_name() == "test-event-0" );
    REQUIRE( storage.get(42)->get_name() == "test-event-42" );
    REQUIRE( storage.get(number_elements-1)->get_name() == "test-event-999999" );
    REQUIRE_THROWS( storage.get(number_elements) );

    std::vector<std::string> names;
    for(auto const& evt : storage)
    {
        names.push_back(evt->get_name());
    }

    REQUIRE( names[0] == storage.get(0)->get_name() );
    REQUIRE( names[42] == storage.get(42)->get_name() );
    REQUIRE( names[number_elements-1] == storage.get(number_elements-1)->get_name() );   
}

TEST_CASE( "SimpleRingBuffer can limit number of stored elements", "[storage]" )
{
    SimpleRingBuffer<std::shared_ptr<Event>> storage(3); // 3 elements limit

    REQUIRE( storage.size() ==  0 );
    REQUIRE_THROWS( storage.get(0) );

    storage.add(std::make_shared<Event>(nullptr, "test-event-1", "test-event-data"));
    storage.add(std::make_shared<Event>(nullptr, "test-event-2", "test-event-data"));
    storage.add(std::make_shared<Event>(nullptr, "test-event-3", "test-event-data"));
    storage.add(std::make_shared<Event>(nullptr, "test-event-4", "test-event-data"));
    storage.add(std::make_shared<Event>(nullptr, "test-event-5", "test-event-data"));
    storage.add(std::make_shared<Event>(nullptr, "test-event-6", "test-event-data"));

    REQUIRE( storage.size() ==  3 );
    REQUIRE( storage.get(0)->get_name() == "test-event-4" );
    REQUIRE( storage.get(1)->get_name() == "test-event-5" );
    REQUIRE( storage.get(2)->get_name() == "test-event-6" );
    REQUIRE_THROWS( storage.get(4) );

    std::vector<std::string> names;
    for(auto const& evt : storage)
    {
        names.push_back(evt->get_name());
    }
    
    std::vector<std::string> expected_names = {"test-event-4", "test-event-5", "test-event-6"};
    REQUIRE( names == expected_names );
}