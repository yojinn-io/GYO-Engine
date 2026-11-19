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
    ReadTests.cpp
    NumberTests.cpp
    WriteTests.cpp
    WriteAllocationTests.cpp
    ScenarioTests.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(gyo_json_tests PRIVATE GYO::Json gyo_test_support doctest::doctest Threads::Threads)
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

# The adapter's internals (engine/json/src) and yyjson's allocator type: only
# this directory may name them (J-1).
add_executable(gyo_json_adapter_tests AdapterTests.cpp)
target_include_directories(gyo_json_adapter_tests PRIVATE ${GYO_REPOSITORY_ROOT}/engine/json/src)
target_link_libraries(gyo_json_adapter_tests PRIVATE GYO::Json gyo_yyjson doctest::doctest)
add_test(NAME gyo_json_adapter_tests COMMAND gyo_json_adapter_tests)
set_tests_properties(gyo_json_adapter_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# The survey's number vectors on every L1 platform (decision v8-arch-json-fast-float):
# regenerated into the build directory and checked to be the survey's set
# (SHA-256), then every number compared with its correctly rounded reference.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(gyo_json_vector_tests VectorTests.cpp)
target_link_libraries(gyo_json_vector_tests PRIVATE GYO::Json)
set(_gyo_json_vectors "${CMAKE_CURRENT_BINARY_DIR}/vectors")
add_test(NAME json.vectors.generate
    COMMAND Python3::Interpreter -I ${CMAKE_CURRENT_SOURCE_DIR}/vectors/gen_numbers.py ${_gyo_json_vectors})
set_tests_properties(json.vectors.generate PROPERTIES LABELS "cpu" TIMEOUT 300 FIXTURES_SETUP gyo_json_vectors)
add_test(NAME gyo_json_vector_tests COMMAND gyo_json_vector_tests ${_gyo_json_vectors})
set_tests_properties(gyo_json_vector_tests PROPERTIES LABELS "cpu" TIMEOUT 300 FIXTURES_REQUIRED gyo_json_vectors)

# Design Invariant J-1: yyjson and fast_float appear only in their adapter
# directories (engine/json/src/adapters/<component>/); set up after the
# scenario tests passed (declaration 8.5, AGENTS §3 on when to set up guards).
add_test(NAME json.adapter_guard
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/json_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME json.adapter_guard_selftest
    COMMAND Python3::Interpreter -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_json_guard.py" -v)
set_tests_properties(json.adapter_guard json.adapter_guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)
