cmake_minimum_required(VERSION 3.1)
cmake_policy(SET CMP0091 NEW)
project(Webthing-CPP VERSION 1.0.3 DESCRIPTION "Webthing implementation for C++" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

IF(WIN32)
    ADD_DEFINITIONS(-DNOMINMAX)
ENDIF(WIN32)

IF(WIN32)
    ADD_DEFINITIONS(-DNOMINMAX)
ENDIF(WIN32)

option(WT_UNORDERED_JSON_OBJECT_LAYOUT "Enable unordered json object layout." OFF)
IF(WT_UNORDERED_JSON_OBJECT_LAYOUT)
    ADD_DEFINITIONS(-DWT_UNORDERED_JSON_OBJECT_LAYOUT)
ENDIF(WT_UNORDERED_JSON_OBJECT_LAYOUT)
message("WT_UNORDERED_JSON_OBJECT_LAYOUT: ${WT_UNORDERED_JSON_OBJECT_LAYOUT}")

option(WT_USE_JSON_SCHEMA_VALIDATION "Enable json schema validation of properties and actions." ON)
IF(WT_USE_JSON_SCHEMA_VALIDATION)
    ADD_DEFINITIONS(-DWT_USE_JSON_SCHEMA_VALIDATION)
ENDIF(WT_USE_JSON_SCHEMA_VALIDATION)
message("WT_USE_JSON_SCHEMA_VALIDATION: ${WT_USE_JSON_SCHEMA_VALIDATION}")

option(WT_WITH_SSL "Enable SSL support." ON)
IF(WT_WITH_SSL)
    ADD_DEFINITIONS(-DWT_WITH_SSL)
ENDIF(WT_WITH_SSL)
message("WT_WITH_SSL: ${WT_WITH_SSL}")

set(VCPKG_BUILD_TYPE ${CMAKE_BUILD_TYPE})
message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("VCPKG_BUILD_TYPE: ${VCPKG_BUILD_TYPE}")

find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")
message("µWebsockets include dir: ${UWEBSOCKETS_INCLUDE_DIRS}")
IF (WIN32)
    find_library(LIBUSOCKETS_STATIC uSockets.lib)
ELSE(WIN32)
    find_library(LIBUSOCKETS_STATIC libuSockets.a)
ENDIF(WIN32)
message(${LIBUSOCKETS_STATIC})

find_path(MDNS_INCLUDE_DIRS "mdns.h")
message("mdns include dir: ${MDNS_INCLUDE_DIRS}")

find_package(mdns REQUIRED)
find_package(nlohmann_json 3.11.2 REQUIRED)
find_package(nlohmann_json_schema_validator REQUIRED)
find_package(libuv REQUIRED NO_MODULE)
find_package(ZLIB REQUIRED)

IF(WT_WITH_SSL)
    find_package(OpenSSL REQUIRED)
ENDIF(WT_WITH_SSL)

add_subdirectory(examples)
add_subdirectory(test)
