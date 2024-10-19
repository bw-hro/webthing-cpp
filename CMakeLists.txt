cmake_minimum_required(VERSION 3.1)
cmake_policy(SET CMP0091 NEW)
project(Webthing-CPP VERSION 1.0.5 DESCRIPTION "Webthing implementation for C++" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

if(WIN32)
    add_definitions(-DNOMINMAX)
endif(WIN32)

option(WT_UNORDERED_JSON_OBJECT_LAYOUT "Enable unordered json object layout." OFF)
if(WT_UNORDERED_JSON_OBJECT_LAYOUT)
    add_definitions(-DWT_UNORDERED_JSON_OBJECT_LAYOUT)
endif(WT_UNORDERED_JSON_OBJECT_LAYOUT)
message("WT_UNORDERED_JSON_OBJECT_LAYOUT: ${WT_UNORDERED_JSON_OBJECT_LAYOUT}")

option(WT_USE_JSON_SCHEMA_VALIDATION "Enable json schema validation of properties and actions." ON)
if(WT_USE_JSON_SCHEMA_VALIDATION)
    add_definitions(-DWT_USE_JSON_SCHEMA_VALIDATION)
endif(WT_USE_JSON_SCHEMA_VALIDATION)
message("WT_USE_JSON_SCHEMA_VALIDATION: ${WT_USE_JSON_SCHEMA_VALIDATION}")

option(WT_WITH_SSL "Enable SSL support." ON)
if(WT_WITH_SSL)
    add_definitions(-DWT_WITH_SSL)
endif(WT_WITH_SSL)
message("WT_WITH_SSL: ${WT_WITH_SSL}")

set(VCPKG_BUILD_TYPE ${CMAKE_BUILD_TYPE})
message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("VCPKG_BUILD_TYPE: ${VCPKG_BUILD_TYPE}")

find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")
message("µWebsockets include dir: ${UWEBSOCKETS_INCLUDE_DIRS}")
if(WIN32)
    find_library(LIBUSOCKETS_STATIC uSockets.lib)
else(WIN32)
    find_library(LIBUSOCKETS_STATIC libuSockets.a)
endif(WIN32)
message(${LIBUSOCKETS_STATIC})

find_path(MDNS_INCLUDE_DIRS "mdns.h")
message("mdns include dir: ${MDNS_INCLUDE_DIRS}")

find_package(mdns REQUIRED)
find_package(nlohmann_json 3.11.2 REQUIRED)
find_package(nlohmann_json_schema_validator REQUIRED)
find_package(libuv REQUIRED NO_MODULE)
find_package(ZLIB REQUIRED)

if(WT_WITH_SSL)
    find_package(OpenSSL REQUIRED)
endif(WT_WITH_SSL)

option(WT_BUILD_EXAMPLES "build examples" ON)
message("WT_BUILD_EXAMPLES: ${WT_BUILD_EXAMPLES}")
if(WT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(WT_BUILD_TESTS "build tests" ON)
message("WT_BUILD_TESTS: ${WT_BUILD_TESTS}")
if(WT_BUILD_TESTS)
    add_subdirectory(test)
endif()

add_library(webthing-cpp INTERFACE)
target_include_directories(webthing-cpp INTERFACE
    $<BUILD_INTERFACE:"${CMAKE_CURRENT_SOURCE_DIR}/include}"> 
    $<INSTALL_INTERFACE:include>
)

install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_PREFIX}/include")
