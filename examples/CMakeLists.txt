# Examples: programs that show how a product uses the Engine. They are not part of the Engine
# and not tests; each is its own executable linking the Engine like a product would.
# Built when GYO_BUILD_EXAMPLES is on (default: with BUILD_TESTING, so CI keeps them compiling).
function(gyo_add_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${ARGN})
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /permissive-)
        target_compile_definitions(${name} PRIVATE NOMINMAX)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# One directory per example, each with its own CMakeLists.txt, added here without naming it: removing a Plugin
# removes the examples that carry it (build/ci/common/plugin_removal.py, Design Invariant SDL-2).
file(GLOB _gyo_example_dirs LIST_DIRECTORIES true CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*")
list(SORT _gyo_example_dirs)
foreach(_gyo_example_dir IN LISTS _gyo_example_dirs)
    if(IS_DIRECTORY "${_gyo_example_dir}" AND EXISTS "${_gyo_example_dir}/CMakeLists.txt")
        add_subdirectory("${_gyo_example_dir}")
    endif()
endforeach()
