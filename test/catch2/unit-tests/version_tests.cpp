// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <regex>

#include <catch2/catch_all.hpp>
#include <bw/webthing/version.hpp>


TEST_CASE( "Webthing-CPP library has access to its version string", "[version]" )
{
    std::string pattern(R"(\d+\.\d+\.\d+)");
    std::regex pattern_regex(pattern);

    INFO("expect version: '" << bw::webthing::version << "' to be matched by: '" << pattern << "'");
    REQUIRE(std::regex_match(bw::webthing::version, pattern_regex));
}