set(LIBS_FOR_EXAMPLES 
    ${LIBUSOCKETS_STATIC}
    nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator
    ZLIB::ZLIB
    $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
)

set(INCLUDES_FOR_EXAMPLES ../include ${UWEBSOCKETS_INCLUDE_DIRS} ${MDNS_INCLUDE_DIRS})


function(create_example_binary cpp_file)    
    cmake_path(GET cpp_file STEM target)
    message("configure example '${target}'")
    add_executable("${target}" ${cpp_file})
    
    set_property(TARGET "${target}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    target_include_directories("${target}" PRIVATE ${INCLUDES_FOR_EXAMPLES})
    target_link_libraries("${target}" PRIVATE ${LIBS_FOR_EXAMPLES})
    
    IF(WT_WITH_SSL)
        target_link_libraries("${target}" PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    ENDIF(WT_WITH_SSL)

endfunction()

create_example_binary(single-thing.cpp)
create_example_binary(multiple-things.cpp)
create_example_binary(gui-thing.cpp)
