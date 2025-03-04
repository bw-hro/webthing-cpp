find_package(Catch2 3 REQUIRED)
find_package(cpr CONFIG REQUIRED)

add_executable(tests
    "catch2/unit-tests/action_tests.cpp"
    "catch2/unit-tests/event_tests.cpp"
    "catch2/unit-tests/json_validator_tests.cpp"
    "catch2/unit-tests/property_tests.cpp"
    "catch2/unit-tests/server_tests.cpp"
    "catch2/unit-tests/storage_tests.cpp"
    "catch2/unit-tests/thing_tests.cpp"
    "catch2/unit-tests/utils_tests.cpp"
    "catch2/unit-tests/value_tests.cpp"
    "catch2/unit-tests/version_tests.cpp"
    "catch2/unit-tests/webthing_tests.cpp"
)

set_property(TARGET tests PROPERTY
             MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(INCLUDES_FOR_TESTS ../include)
target_compile_definitions(tests PRIVATE CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS)
target_include_directories(tests PRIVATE ${INCLUDES_FOR_TESTS})
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(tests PRIVATE cpr::cpr)
target_link_libraries(tests PRIVATE nlohmann_json_schema_validator::validator)
target_link_libraries(tests PRIVATE unofficial::uwebsockets::uwebsockets)

if(WT_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(tests PRIVATE --coverage)
        target_link_options(tests PRIVATE --coverage)
    else()
        message(FATAL_ERROR "Coverage only supported with GCC/Clang")
    endif()
endif()

include(CTest)
include(Catch)
catch_discover_tests(tests)