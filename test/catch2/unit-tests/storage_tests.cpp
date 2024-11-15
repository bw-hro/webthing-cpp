// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/event.hpp>
#include <bw/webthing/storage.hpp>

using namespace bw::webthing;

using SimpleRingBuffer_EventPtr = SimpleRingBuffer<std::shared_ptr<Event>> ;
using FlexibleRingBuffer_EventPtr = SimpleRingBuffer<std::shared_ptr<Event>> ;

TEMPLATE_TEST_CASE( "RingBufferImpl compared to std::vector", "[.][benchmark][storage]",
    SimpleRingBuffer_EventPtr, FlexibleRingBuffer_EventPtr )
{
    int number_elements = GENERATE(10, 1000, 100000);
    std::string num_str = ", " + std::to_string(number_elements) + " stored elements";

    std::vector<std::shared_ptr<Event>> prepared_events;
    for(int i = 0; i < number_elements; i++)
        prepared_events.push_back(std::make_shared<Event>(nullptr, "test-event-" + std::to_string(i), "test-event-data"));

    BENCHMARK("RingBufferImpl with 2024 limit" + num_str)
    {
        TestType storage(2024); // limit
        for(int i = 0; i < number_elements; i++)
            storage.add(prepared_events[i]);
        return storage;
    };

    BENCHMARK("RingBufferImpl with 2024 limit, write protected" + num_str)
    {
        TestType storage(2024, true); // limit, write protected
        for(int i = 0; i < number_elements; i++)
            storage.add(prepared_events[i]);
        return storage;
    };

    BENCHMARK("RingBufferImpl without limit" + num_str)
    {
        TestType storage; // no limit
        for(int i = 0; i < number_elements; i++)
            storage.add(prepared_events[i]);
        return storage;
    };

    BENCHMARK("RingBufferImpl without limit, write protected" + num_str)
    {
        TestType storage(SIZE_MAX, true); // no limit, write protected
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

TEMPLATE_TEST_CASE( "RingBufferImpl does not limit number of stored elements by default", "[storage]",
    SimpleRingBuffer_EventPtr, FlexibleRingBuffer_EventPtr )
{
    TestType storage; // no limitation

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

TEMPLATE_TEST_CASE( "RingBufferImpl can limit number of stored elements", "[storage]",
    SimpleRingBuffer_EventPtr, FlexibleRingBuffer_EventPtr )
{
    TestType storage(3); // 3 elements limit

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

TEMPLATE_TEST_CASE( "RingBufferImpl gives access to stored elements as references", "[storage]",
    SimpleRingBuffer<std::string>, FlexibleRingBuffer<std::string> )
{
    TestType storage(3);
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

TEMPLATE_TEST_CASE( "RingBufferImpl can protect write access", "[storage]",
    SimpleRingBuffer<std::string>, FlexibleRingBuffer<std::string> )
{
    TestType storage(SIZE_MAX, true);

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

TEMPLATE_TEST_CASE( "RingBufferImpl offers iterator access", "[storage]",
    SimpleRingBuffer<std::string>, FlexibleRingBuffer<std::string> )
{
    TestType storage(3);
    storage.add("aaa");
    storage.add("bbb");
    storage.add("ccc");
    storage.add("ddd");
    storage.add("eee");
    
    for(int i = 0; i < storage.size(); i++)
        storage.get(i).at(0) = 'x';

    for(auto& element: storage)
        element.at(1) = 'y';

    REQUIRE( storage.size() == 3 );
    REQUIRE( storage.get(0) == "xyc" );
    REQUIRE( storage.get(1) == "xyd" );
    REQUIRE( storage.get(2) == "xye" );

    std::vector<std::string> es;
    for(const auto& element: storage)
        es.push_back(element);

    std::vector<std::string> expected = {"xyc", "xyd", "xye"};
    REQUIRE( es == expected );
}

TEMPLATE_TEST_CASE( "RingBufferImpl elements can be removed", "[storage]",
    FlexibleRingBuffer<std::string> )
{
    TestType storage(5);
    storage.add("a");
    storage.add("b");
    storage.add("c");
    storage.add("d");
    storage.add("e");
    storage.add("f");
    storage.add("g");

    REQUIRE( storage.size() == 5 );
    REQUIRE( storage.get(0) == "c" );
    REQUIRE( storage.get(4) == "g" );

    storage.remove_if([](auto s){
        return s == "d" || s == "f";
    });

    REQUIRE( storage.size() == 3 );
    REQUIRE( storage.get(0) == "c" );
    REQUIRE( storage.get(1) == "e" );
    REQUIRE( storage.get(2) == "g" );

    storage.add("h");

    REQUIRE( storage.size() == 4 );
    REQUIRE( storage.get(3) == "h" );
}
