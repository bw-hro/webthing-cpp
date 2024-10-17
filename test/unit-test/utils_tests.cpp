// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/utils.hpp>

using namespace bw::webthing;

TEST_CASE( "Utils offer a helper for uuid generation", "[uuid]" )
{
    std::string uuid = bw::webthing::generate_uuid();
    logger::debug("uuid-test: " + uuid);
    REQUIRE( (uuid.find_first_not_of("-0123456789abcdef") == std::string::npos) );
    REQUIRE( uuid.size() == 32 + 4 );
    REQUIRE( uuid[ 8] == '-');
    REQUIRE( uuid[13] == '-');
    REQUIRE( uuid[18] == '-');
    REQUIRE( uuid[23] == '-');

    int samples = 100'000;
    logger::debug("uuid-test: start testing " + std::to_string(samples) + " samples");
    std::map<std::string, int> uuid_counter;
    for(int i = 0; i < samples; i++)
    {
        std::string uuid = bw::webthing::generate_uuid();
        uuid_counter[uuid] = uuid_counter[uuid]++;
    }
    logger::debug("uuid-test: finish sample tests");
    REQUIRE( uuid_counter.size() == samples );
}

TEST_CASE( "UUID generation can be (un)fixed", "[uuid]")
{
    {
        FIXED_UUID_SCOPED("my-fix-non-uuid");
        REQUIRE( generate_uuid() == "my-fix-non-uuid" );
        REQUIRE( generate_uuid() == "my-fix-non-uuid" );
        REQUIRE( generate_uuid() == "my-fix-non-uuid" );
        REQUIRE( generate_uuid() == "my-fix-non-uuid" );
        REQUIRE( generate_uuid() == "my-fix-non-uuid" );
    }

    // leaving the scope should unfix the uuid
    REQUIRE_FALSE( generate_uuid() == "my-fix-non-uuid" );
}

TEST_CASE( "Utils offer a simple customizable logging interface", "[log]" )
{
    SECTION( "Offers a default logging implementation" )
    {
        std::vector<std::thread> ts;

        for(int i = 0; i < 3 ; i++ )
        {
            ts.push_back(std::thread([i]{
                for(int count = 0; count < 10; count++)
                {
                    logger::warn("tid: " + std::to_string(i) +  " - Thing wants to warn you...");
                    logger::info("tid: " + std::to_string(i) +  " - some test content count: " + std::to_string( count ));
                }
            }));
        }

        for(auto& t : ts)
            t.join();
    }

    SECTION( "A custom log implementation can be registered" )
    {
        std::map<std::string, std::vector<std::string>> messages;
        auto log_impl_msg_collector = [&](auto level, auto msg)
        {
            std::string is_ok = level <= log_level::info ? "ok" : "warn";
            std::cout << "msg_collector: " << is_ok << " " << msg << std::endl;
            messages[is_ok].push_back(msg);
        };

        logger::error("error-1");
        logger::warn("warn-1");
        logger::info("info-1");
        logger::debug("debug-1");
        REQUIRE(messages.empty());

        logger::register_implementation(log_impl_msg_collector);
       
        logger::error("error-2");
        logger::warn("warn-2");
        logger::info("info-2");
        logger::debug("debug-2");

        std::vector<std::string> expected_ok {"info-2", "debug-2"};
        REQUIRE_THAT( messages["ok"], Catch::Matchers::Equals(expected_ok));

        std::vector<std::string> expected_warn {"error-2", "warn-2"};
        REQUIRE_THAT(messages["warn"], Catch::Matchers::Equals(expected_warn));
    }

    SECTION( "A custom log level can be specified" )
    {
        std::vector<std::string> messages;
        logger::register_implementation([&](auto l, auto m){messages.push_back(m);});

        logger::set_level(log_level::error);
                
        logger::error("error-3");
        logger::warn("warn-3");
        logger::info("info-3");
        logger::debug("debug-3");
        logger::trace("trace-3");

        REQUIRE_THAT(messages, Catch::Matchers::Equals(
            std::vector<std::string>{"error-3"}));

        logger::set_level(log_level::trace);

        logger::trace("error-4");
        logger::trace("trace-4");

        REQUIRE_THAT(messages, Catch::Matchers::Equals(
            std::vector<std::string>{"error-3", "error-4", "trace-4"}));

    }

    // reset logger to defaults, to avoid problems with global state of logger
    logger::set_level(log_level::debug);
    logger::register_implementation(nullptr);
}

TEST_CASE( "Utils offer a helper for timestamps", "[time]" )
{
    std::string ts_first = timestamp();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::string ts_second = timestamp();

    FIXED_TIME_SCOPED("1985-08-26T11:11:11.1111+00:02");
    std::string ts_fixed_first = timestamp();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::string ts_fixed_second = timestamp();

    REQUIRE_FALSE(ts_first == ts_second);
    REQUIRE_FALSE(ts_first == ts_fixed_first);
    REQUIRE_FALSE(ts_first == ts_fixed_second);

    REQUIRE_FALSE(ts_second == ts_fixed_first);
    REQUIRE_FALSE(ts_second == ts_fixed_second);

    REQUIRE(ts_fixed_first == ts_fixed_second);
    REQUIRE(ts_fixed_first == "1985-08-26T11:11:11.1111+00:02");
}