// Webthing-CPP
// SPDX-FileCopyrightText: 2024 Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/value.hpp>

using namespace bw::webthing;

TEST_CASE( "Values can be forwarded to external", "[value]" )
{
    GIVEN( "An external device" )
    {
        struct ExternalDevice
        {
            std::string option = "option-a";
        } external_device;

        AND_GIVEN( "Value is configured to forward to external" )
        {
            Value<std::string> option_value(std::nullopt, [&](auto option){
                external_device.option = option;
            });

            THEN( "It fowards the value" )
            {
                REQUIRE( option_value.get() == std::nullopt );
                REQUIRE( external_device.option == "option-a" );

                option_value.set("option-changed");

                REQUIRE( option_value.get() == "option-changed" );
                REQUIRE( external_device.option == "option-changed" );
            }
        }
    }
}

TEST_CASE( "Values only notify their observers when they have changed", "[value]" )
{
    // given an observer and a value
    struct ValueObserver
    {
        std::optional<std::string> last_value;
        int changed_counter = 0;
        void update(std::string value)
        {
            last_value = value;
            changed_counter++;
        }
    } value_observer;

    Value<std::string> value("val-x");
    value.add_observer([&](auto val){
        value_observer.update(val);
    });

    REQUIRE( value.get() == "val-x" );
    REQUIRE( !value_observer.last_value );
    REQUIRE( value_observer.changed_counter == 0 );

    // when changing the value, then
    value.set("val-a");

    REQUIRE( value.get() == "val-a" );
    REQUIRE( value_observer.last_value == "val-a" );
    REQUIRE( value_observer.changed_counter == 1 );

    // when changing the value again, then
    value.notify_of_external_update("val-b");

    REQUIRE( value.get() == "val-b" );
    REQUIRE( value_observer.last_value == "val-b" );
    REQUIRE( value_observer.changed_counter == 2 );

    // when updating the value without changing it, then
    value.notify_of_external_update("val-b");

    REQUIRE( value.get() == "val-b" );
    REQUIRE( value_observer.last_value == "val-b" );
    REQUIRE( value_observer.changed_counter == 2 );

    // when updating the value without changing it, then
    value.set("val-b");

    REQUIRE( value.get() == "val-b" );
    REQUIRE( value_observer.last_value == "val-b" );
    REQUIRE( value_observer.changed_counter == 2 );

    // when changing the value again, then
    value.set("val-c");

    REQUIRE( value.get() == "val-c" );
    REQUIRE( value_observer.last_value == "val-c" );
    REQUIRE( value_observer.changed_counter == 3 );
}