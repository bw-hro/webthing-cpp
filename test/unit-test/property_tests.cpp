// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/property.hpp>

using namespace bw::webthing;
using namespace Catch::Matchers;

struct MessageContains : Catch::Matchers::MatcherGenericBase
{
    MessageContains( const std::string& contains):
        contains{ contains }
    {}

    bool match(const std::exception& other) const {

        std::string what = other.what();

        return what.find(contains) != std::string::npos;

    }

    std::string describe() const override {
        return "Exception message contains: " + contains;
    }

private:
    const std::string& contains;
};

template<class T> std::shared_ptr<Value<T>> create_value(T initial_value)
{
    return std::make_shared<Value<T>>(initial_value, [](auto v){
        std::stringstream ss;
        ss << "Value changed to " << v;
        logger::info(ss.str());
    });
}

template<class T> std::shared_ptr<PropertyBase> create_proptery(std::string name, std::shared_ptr<Value<T>> value, json metadata = json::object())
{
    PropertyChangedCallback property_changed_callback = [](json prop){
        logger::info("Property changed: " + prop.dump());
    };
    return std::make_shared<Property<T>>(property_changed_callback, name, value, metadata);
}

template<class T> std::shared_ptr<PropertyBase> create_proptery(std::string name, T intial_value, json metadata = json::object())
{
    return create_proptery(name, create_value(intial_value), metadata);
}


TEST_CASE( "Properties can be updated", "[property]" )
{
    auto v1 = create_value<std::string>("benno");
    auto prop = create_proptery("test-prop-v1", v1);
    auto initial = *prop->get_value<std::string>();
    REQUIRE( initial == "benno" );

    prop->set_value(std::string("Бennö 森林"));
    auto actual = *prop->get_value<std::string>();
    REQUIRE( actual == "Бennö 森林" );
    REQUIRE( v1->get() == "Бennö 森林" );
}

TEST_CASE( "Properties value type can't be changed", "[property]" )
{
    auto val = create_value<std::string>("a_string_value");
    auto prop = create_proptery("test-prop", val);

    REQUIRE_THROWS_MATCHES(prop->set_value(123),
        PropertyError,
        Catch::Matchers::Message("Property value type not matching"));

    REQUIRE( *prop->get_value<std::string>() == "a_string_value" );
}

TEST_CASE( "Properties can only be changed from external when they are not read only properties", "[property]" )
{
    auto val = create_value(666);
    auto prop_ro = create_proptery("read-only-prop", val, {{"readOnly", true}});
    auto prop_w1 = create_proptery("writeable-prop-1", val, {{"readOnly", false}});
    auto prop_w2 = create_proptery("writeable-prop-2", val);


    REQUIRE_THROWS_MATCHES(prop_ro->set_value(123),
        PropertyError,
        Catch::Matchers::Message("Read-only property"));

    REQUIRE( *prop_ro->get_value<int>() == 666 );

    prop_w1->set_value(123);
    REQUIRE( *prop_w1->get_value<int>() == 123 );
    
    prop_w2->set_value(777);
    REQUIRE( *prop_w1->get_value<int>() == 777 );
}

TEST_CASE( "Properties can wrap json values", "[property]" )
{
    json initial_obj = {{"key","value"}};
    auto prop_obj = create_proptery("json-value-test-prop", initial_obj);
    auto initial = prop_obj->get_value<json>();
    REQUIRE( (*initial == initial_obj) );

    json new_obj = {{"name", "bw"}, {"age", 123}, {"colors", std::vector<std::string>({"r","g","b"})}};
    prop_obj->set_value(new_obj);
    auto changed = prop_obj->get_value<json>();
    REQUIRE( (*changed == new_obj) );
}

#ifdef WT_USE_JSON_SCHEMA_VALIDATION

TEST_CASE( "Properties can only be changed from external when provided values are valid", "[property]" )
{
    SECTION( "Integer" )
    {
        auto val = create_value(666);
        json property_description = {{"type", "integer"}, {"minimum", 555}};
        auto property = create_proptery("writeable-prop", val, property_description);

        REQUIRE_THROWS_MATCHES(property->set_value(123), PropertyError,
            MessageContains("Invalid property") &&
            MessageContains("below minimum of 555")
        );
        REQUIRE( *property->get_value<int>() == 666 );

        REQUIRE_THROWS_MATCHES(property->set_value(777.0), PropertyError,
            MessageContains("value type not matching")
        );
        REQUIRE( *property->get_value<int>() == 666 );

        property->set_value(1000);
        REQUIRE( *property->get_value<int>() == 1000 );
    }

    SECTION( "Number" )
    {
        auto val = create_value(666.0);
        json property_description = {{"type", "number"}, {"minimum", 555.0}};
        auto property = create_proptery("writeable-number-prop", val, property_description);

        REQUIRE_THROWS_MATCHES(property->set_value(123.0), PropertyError,
            MessageContains("Invalid property") &&
            MessageContains("below minimum of 555")
        );
        REQUIRE( *property->get_value<double>() == 666 );

        property->set_value(1000.123);
        REQUIRE( *property->get_value<double>() == 1000.123 );

        property->set_value(777);
        REQUIRE( *property->get_value<double>() == 777 );
    }

    SECTION( "String" )
    {
        auto val = create_value(std::string("first-string-value"));
        json property_description = {{"type", "string"}, {"pattern", "string-value"}};
        auto property = create_proptery("writeable-prop", val, property_description);

        REQUIRE_THROWS_MATCHES(property->set_value(std::string("something-invalid")),
            PropertyError,
            MessageContains("Invalid property") &&
            MessageContains("does not match regex pattern")
        );
        REQUIRE( *property->get_value<std::string>() == "first-string-value" );

        property->set_value(std::string("next-string-value"));
        REQUIRE( *property->get_value<std::string>() == "next-string-value" );
    }

    SECTION( "Object" )
    {
        json start_json_obj = {{"color", "red"}, {"amount", 42}};
        auto val = create_value(start_json_obj);
        json property_description = {{"type", "object"}, {"required", {"color"}}};
        auto property = create_proptery("writeable-prop", val, property_description);

        REQUIRE_THROWS_MATCHES(property->set_value("some-string-value"), PropertyError,
            MessageContains("value type not matching")
        );
        REQUIRE_THROWS_MATCHES(property->set_value(json::array({1,2,3})), PropertyError,
            MessageContains("Invalid property") &&
            MessageContains("unexpected instance type")
        );
        REQUIRE_THROWS_MATCHES(property->set_value(json::object({{"amount", 123}})), PropertyError,
            MessageContains("Invalid property") &&
            MessageContains("required property 'color' not found")
        );
        REQUIRE( *property->get_value<json>() == start_json_obj );

        json new_json_obj = {{"color", "black"}};
        property->set_value(new_json_obj);
        REQUIRE( *property->get_value<json>() == new_json_obj );
    }
}

#endif