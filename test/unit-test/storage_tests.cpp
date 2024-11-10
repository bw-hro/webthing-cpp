// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/event.hpp>
#include <bw/webthing/storage.hpp>

using namespace bw::webthing;

TEST_CASE( "SimpleRingBuffer compared to std::vector", "[.][benchmark][storage]" )
{
    int number_elements = GENERATE(10, 1000, 100000, 1000000);
    std::string num_str = ", " + std::to_string(number_elements) + " stored elements";

    std::vector<std::shared_ptr<Event>> prepared_events;
    for(int i = 0; i < number_elements; i++)
        prepared_events.push_back(std::make_shared<Event>(nullptr, "test-event-" + std::to_string(i), "test-event-data"));

    BENCHMARK("SimpleRingBuffer with 2024 limit" + num_str)
    {
        SimpleRingBuffer<std::shared_ptr<Event>> storage(2024); // limit
        for(int i = 0; i < number_elements; i++)
            storage.add(prepared_events[i]);
        return storage;
    };

    BENCHMARK("SimpleRingBuffer with 2024 limit, write protected" + num_str)
    {
        SimpleRingBuffer<std::shared_ptr<Event>> storage(2024, true); // limit, write protected
        for(int i = 0; i < number_elements; i++)
            storage.add(prepared_events[i]);
        return storage;
    };

    BENCHMARK("SimpleRingBuffer without limit" + num_str)
    {
        SimpleRingBuffer<std::shared_ptr<Event>> storage; // no limit
        for(int i = 0; i < number_elements; i++)
            storage.add(prepared_events[i]);
        return storage;
    };

    BENCHMARK("SimpleRingBuffer without limit, write protected" + num_str)
    {
        SimpleRingBuffer<std::shared_ptr<Event>> storage(SIZE_MAX, true); // no limit, write protected
        for(int i = 0; i < number_elements; i++)
            storage.add(prepared_events[i]);
        return storage;
    };

    BENCHMARK("std::vector (unprotected)" + num_str)
    {
        std::vector<std::shared_ptr<Event>> storage; // no limit
        for(int i = 0; i < number_elements; i++)
            storage.push_back(prepared_events[i]);
        return storage;
    };
}

TEST_CASE( "SimpleRingBuffer does not limit number of stored elements by default", "[storage]" )
{
    SimpleRingBuffer<std::shared_ptr<Event>> storage; // no limitation

    REQUIRE( storage.size() ==  0 );
    REQUIRE_THROWS_AS( storage.get(0), std::out_of_range );

    int number_elements = 1000000;
    for(int i = 0; i < number_elements; i++)
        storage.add(std::make_shared<Event>(nullptr, "test-event-" + std::to_string(i), "test-event-data"));

    REQUIRE( storage.size() ==  number_elements );
    REQUIRE( storage.get(0)->get_name() == "test-event-0" );
    REQUIRE( storage.get(42)->get_name() == "test-event-42" );
    REQUIRE( storage.get(number_elements-1)->get_name() == "test-event-999999" );
    REQUIRE_THROWS_AS( storage.get(number_elements), std::out_of_range );

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
    REQUIRE_THROWS_AS( storage.get(0), std::out_of_range );

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
    REQUIRE_THROWS_AS( storage.get(4), std::out_of_range );

    std::vector<std::string> names;
    for(auto const& evt : storage)
    {
        names.push_back(evt->get_name());
    }
    
    std::vector<std::string> expected_names = {"test-event-4", "test-event-5", "test-event-6"};
    REQUIRE( names == expected_names );
}


TEST_CASE( "SimpleRingBuffer gives access to stored elements as references", "[storage]" )
{
    SimpleRingBuffer<std::string> storage(3);
    storage.add("first");
    storage.add("second");
    storage.add("third");
    REQUIRE( storage.get(0) == "first" );

    auto first_copy = storage.get(0);
    first_copy.at(0) = 'x';
    REQUIRE( storage.get(0) == "first" );

    auto& first_ref = storage.get(0);
    first_ref.at(0) = 'x';
    REQUIRE( storage.get(0) == "xirst" );
}

TEST_CASE( "SimpleRingBuffer can protect write access", "[storage]" )
{
    SimpleRingBuffer<std::string> storage(SIZE_MAX, true);

    int num_threads = 10;
    int num_elements_per_thread = 1000;
    auto ts = std::vector<std::thread>();

    for(int i = 0; i < num_threads; i++)
    {
        ts.push_back(std::thread([&]{
            for(int k = 0; k < num_elements_per_thread; k++)
                storage.add("t" + std::to_string(i) + "__" + std::to_string(k));
        }));
    }

    for(auto& t : ts)
        t.join();

    REQUIRE( storage.size() == num_threads * num_elements_per_thread );
}
