# The JSON Module's tests (docs/architecture/target/v2/json-module-declaration.zh-Hant.md).
# gyo_json_component_tests names yyjson and fast_float directly: this is the
# adapter's own test directory, the one place outside engine/json/src that
# Design Invariant J-1 allows to.
add_executable(gyo_json_component_tests ComponentTests.cpp)
target_link_libraries(gyo_json_component_tests PRIVATE gyo_yyjson gyo_fast_float GYO::Base doctest::doctest)
add_test(NAME gyo_json_component_tests COMMAND gyo_json_component_tests)
set_tests_properties(gyo_json_component_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# The Module's own tests see only its public include directory: a public header
# that included a component header would not compile here (Design Invariant J-4).
add_executable(gyo_json_tests
    test_main.cpp
    JsonErrorTests.cpp
)
target_link_libraries(gyo_json_tests PRIVATE GYO::Json gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_json_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_json_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME gyo_json_tests COMMAND gyo_json_tests)
set_tests_properties(gyo_json_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# Design Invariants J-1 and J-6: the Module links Base and its two external
# components and nothing else, and hands neither component to its users.
# Checked once the whole project is configured, so an edge added later from
# another directory is seen too (gyo_expect_links comes from tests/common/time).
function(gyo_expect_json_links)
    gyo_expect_links(gyo_json GYO::Base gyo_yyjson gyo_fast_float)
    get_target_property(interface gyo_json INTERFACE_LINK_LIBRARIES)
    foreach(component IN ITEMS gyo_yyjson gyo_fast_float)
        if(component IN_LIST interface)
            message(FATAL_ERROR "gyo_json exposes ${component} to its users; link it PRIVATE")
        endif()
    endforeach()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_json_links)
