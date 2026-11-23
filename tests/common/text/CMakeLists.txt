# The text Module's tests (docs/architecture/target/v2/text-declaration.zh-Hant.md).
# The fonts they use are described in fonts/README.md.

# The Module's own tests see only its public include directory: a public header
# that included a FreeType, HarfBuzz or libunibreak header would not compile
# here (Design Invariant TX-1).
add_executable(gyo_text_tests
    test_main.cpp
    AtlasTests.cpp
    CaretTests.cpp
    FontTests.cpp
    GoldenTests.cpp
    LayoutTests.cpp
    ScenarioTests.cpp
    ShapeTests.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(gyo_text_tests PRIVATE GYO::Text gyo_test_support doctest::doctest Threads::Threads)
target_compile_definitions(gyo_text_tests PRIVATE GYO_TEXT_TEST_FONTS="${CMAKE_CURRENT_SOURCE_DIR}/fonts")
if(MSVC)
    target_compile_options(gyo_text_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(gyo_text_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME gyo_text_tests COMMAND gyo_text_tests)
set_tests_properties(gyo_text_tests PROPERTIES LABELS "cpu" TIMEOUT 120)

# Design Invariants TX-7 and TX-8: its own process, which sets variables that
# would change HarfBuzz's shaping and FreeType's hinting if they were read,
# before it uses either. The atlas digest is the one the case gets without the
# variables (computed 2026-11-23).
add_executable(gyo_text_environment_tests test_main.cpp EnvironmentTests.cpp)
target_link_libraries(gyo_text_environment_tests PRIVATE GYO::Text GYO::Base doctest::doctest)
target_compile_definitions(gyo_text_environment_tests PRIVATE
    GYO_TEXT_TEST_FONTS="${CMAKE_CURRENT_SOURCE_DIR}/fonts"
    GYO_TEXT_ENVIRONMENT_ATLAS="3d0e7ca59978c28fcddffd6f279b11d0ee5c9429205e6d9bd50cc2d60be91cec")
if(MSVC)
    target_compile_options(gyo_text_environment_tests PRIVATE /utf-8)
endif()
add_test(NAME gyo_text_environment COMMAND gyo_text_environment_tests)
set_tests_properties(gyo_text_environment PROPERTIES LABELS "cpu" TIMEOUT 60)

# The adapters' interfaces (engine/modules/text/src): Unicode's conformance
# tests through the libunibreak adapter, the two adapters' advances compared
# (Design Invariant TX-2) and the UTF-8 decoder. Only the adapters' own
# interfaces are used, never a component's header.
include(FetchContent)
FetchContent_GetProperties(libunibreak)
add_executable(gyo_text_adapter_tests test_main.cpp AdapterTests.cpp)
target_include_directories(gyo_text_adapter_tests PRIVATE ${GYO_REPOSITORY_ROOT}/engine/modules/text/src)
target_link_libraries(gyo_text_adapter_tests PRIVATE GYO::Text GYO::Base doctest::doctest)
# Unicode's LineBreakTest.txt and GraphemeBreakTest.txt (17.0.0), as shipped in
# libunibreak's release tarball (pinned by SHA-256; the test also checks each
# file's SHA-256 against the copy published by unicode.org).
target_compile_definitions(gyo_text_adapter_tests PRIVATE
    GYO_TEXT_TEST_FONTS="${CMAKE_CURRENT_SOURCE_DIR}/fonts"
    GYO_TEXT_UNICODE_TESTS="${libunibreak_SOURCE_DIR}/src")
if(MSVC)
    target_compile_options(gyo_text_adapter_tests PRIVATE /utf-8)
endif()
add_test(NAME gyo_text_adapter_tests COMMAND gyo_text_adapter_tests)
set_tests_properties(gyo_text_adapter_tests PROPERTIES LABELS "cpu" TIMEOUT 120)

# Criterion 7's measurement, built on demand only.
add_executable(gyo_text_measure_frame_cost EXCLUDE_FROM_ALL MeasureFrameCost.cpp)
target_link_libraries(gyo_text_measure_frame_cost PRIVATE GYO::Text)
target_compile_definitions(gyo_text_measure_frame_cost PRIVATE GYO_TEXT_TEST_FONTS="${CMAKE_CURRENT_SOURCE_DIR}/fonts")

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariants TX-1 and TX-9: the Module links Base, Asset and its three
# external components and nothing else, and hands no component to its users.
# Checked once the whole project is configured.
function(gyo_expect_text_links)
    gyo_expect_links(gyo_text GYO::Base GYO::Asset gyo_freetype gyo_harfbuzz gyo_libunibreak)
    get_target_property(interface gyo_text INTERFACE_LINK_LIBRARIES)
    foreach(component IN ITEMS gyo_freetype gyo_harfbuzz gyo_libunibreak)
        if(component IN_LIST interface)
            message(FATAL_ERROR "gyo_text exposes ${component} to its users; link it PRIVATE")
        endif()
    endforeach()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_text_links)

# Design Invariant TX-1: FreeType, HarfBuzz and libunibreak appear only in their
# adapter directories (engine/modules/text/src/adapters/<component>/), hb-ft
# nowhere; set up after the scenario tests passed (declaration 8.6, AGENTS §3 on
# when to set up guards).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME text.adapter_guard
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/text_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME text.adapter_guard_selftest
    COMMAND Python3::Interpreter -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_text_guard.py" -v)
set_tests_properties(text.adapter_guard text.adapter_guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)
