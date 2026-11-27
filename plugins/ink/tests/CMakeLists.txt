# Tests of the ink Plugin (docs/architecture/target/v2/ink.zh-Hant.md). They come
# and go with the Plugin's row. Only adapters/inkcpp/ includes inkcpp: its
# compiler turns the committed .ink.json files into story bytes in memory, so
# the tests need neither inklecate nor .NET. The Plugin is library_only, so
# the tests link GYO::Ink themselves; the scene's Product carries no unit.
add_executable(gyo_ink_tests
    TestMain.cpp
    DialogueTests.cpp
    SceneTests.cpp
    adapters/inkcpp/CompileStory.cpp)
target_include_directories(gyo_ink_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gyo_ink_tests PRIVATE GYO::Ink GYO::Orchestration GYO::OS gyo_inkcpp_compiler doctest::doctest)
target_compile_definitions(gyo_ink_tests PRIVATE GYO_INK_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
if(MSVC)
    target_compile_options(gyo_ink_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_ink_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_ink_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME ink.dialogue COMMAND gyo_ink_tests)
set_tests_properties(ink.dialogue PROPERTIES
    LABELS "cpu" TIMEOUT 60 FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
