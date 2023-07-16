// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <bw/webthing/errors.hpp>
#include <bw/webthing/json.hpp>

#ifdef WT_USE_JSON_SCHEMA_VALIDATION

    #include <nlohmann/json-schema.hpp>

#endif

namespace bw::webthing {

template<class T>
void validate_value_by_scheme(const T& value, json schema)
{
#ifdef WT_USE_JSON_SCHEMA_VALIDATION

    nlohmann::json_schema::json_validator validator;
    validator.set_root_schema(schema);
    try
    {
        validator.validate(value);
    }
    catch(std::exception& ex)
    {
        throw InvalidJson(ex.what());
    }

#else
    // always succeed
#endif
}

} // bw::webthing