message(STATUS "Generate gui-html.hpp using gui.html content")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gui.html)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/gui.html" GUI_HTML_CONTENT)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/gui-html.hpp.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/gui-html.hpp"
)

set(LIBS_FOR_EXAMPLES 
    nlohmann_json_schema_validator::validator
    unofficial::uwebsockets::uwebsockets
)

set(INCLUDES_FOR_EXAMPLES ../include)

function(create_example_binary cpp_file)
    cmake_path(GET cpp_file STEM target)
    message("configure example '${target}'")
    add_executable("${target}" ${cpp_file})
    
    set_property(TARGET "${target}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    target_include_directories("${target}" PRIVATE ${INCLUDES_FOR_EXAMPLES})
    target_link_libraries("${target}" PRIVATE ${LIBS_FOR_EXAMPLES})
endfunction()

create_example_binary(single-thing.cpp)
create_example_binary(multiple-things.cpp)
create_example_binary(gui-thing.cpp)
