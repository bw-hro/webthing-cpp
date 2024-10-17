// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <nlohmann/json.hpp>

namespace bw::webthing {

#ifdef WT_UNORDERED_JSON_OBJECT_LAYOUT
    typedef nlohmann::json json;
#else
    typedef nlohmann::ordered_json json;
#endif

} // bw::webthing