// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <bw/webthing/json_validator.hpp>

using namespace bw::webthing;

json test_thing_scheme = R"(
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "title": "A Thing",
    "properties": {
        "name": {
            "description": "Name",
            "type": "string"
        }
    },
    "required": ["name"],
    "type": "object"
}

)"_json;

json valid_test_thing_json = {{"name", "a test thing"}};
json invalid_test_thing_json = {{"description", "This test thing is missing a name..."}};

#ifdef WT_USE_JSON_SCHEMA_VALIDATION

TEST_CASE( "When an invalid json value is validated an exception is thrown", "[json]")
{
    REQUIRE_THROWS_AS(validate_value_by_scheme(123, test_thing_scheme),
        InvalidJson);

    REQUIRE_THROWS_AS(validate_value_by_scheme(invalid_test_thing_json, test_thing_scheme),
        InvalidJson);
}

TEST_CASE( "When a valid json value is validated no exceptions will be thrown", "[json]")
{
    REQUIRE_NOTHROW(validate_value_by_scheme(valid_test_thing_json, test_thing_scheme));
}

#else // JSON VALIDATION IS DISABLED

TEST_CASE( "When JSON validation is disabled, all validation attemps will be successful", "[json]") 
{
    REQUIRE_NOTHROW(validate_value_by_scheme(123, test_thing_scheme));
    REQUIRE_NOTHROW(validate_value_by_scheme(valid_test_thing_json, test_thing_scheme));
    REQUIRE_NOTHROW(validate_value_by_scheme(invalid_test_thing_json, test_thing_scheme));
}

#endif
