find_package(Catch2 3 REQUIRED)

add_executable(tests
    "unit-test/action_tests.cpp"
    "unit-test/event_tests.cpp"
    "unit-test/json_validator_tests.cpp"
    "unit-test/property_tests.cpp"
    "unit-test/server_tests.cpp"
    "unit-test/thing_tests.cpp"
    "unit-test/utils_tests.cpp"
    "unit-test/webthing_tests.cpp"
)

set_property(TARGET tests PROPERTY
             MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(INCLUDES_FOR_TESTS ../include ${UWEBSOCKETS_INCLUDE_DIRS} ${MDNS_INCLUDE_DIRS})

target_include_directories(tests PRIVATE ${INCLUDES_FOR_TESTS})
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(tests PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(tests PRIVATE nlohmann_json_schema_validator)
target_link_libraries(tests PRIVATE ${LIBUSOCKETS_STATIC})
target_link_libraries(tests PRIVATE ZLIB::ZLIB)

IF(WIN32)
    target_link_libraries(tests PRIVATE uv_a)
ENDIF(WIN32)

IF(WT_WITH_SSL)
    target_link_libraries(tests PRIVATE OpenSSL::SSL OpenSSL::Crypto)
ENDIF(WT_WITH_SSL)

include(CTest)
include(Catch)
catch_discover_tests(tests)