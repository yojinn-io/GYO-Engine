# Tests of the authority Module (docs/architecture/target/v2/authority-rollback-declaration.zh-Hant.md, sections 4
# and 9.3). Synthetic data only; no product.
add_executable(gyo_authority_tests TestMain.cpp StrategySocketTests.cpp RegistrationsTests.cpp ServerChecksTests.cpp
    ConsistencyTests.cpp SourcesExportTests.cpp
    sources_fixture/SyntheticSimulation.cpp
    sources_fixture/AnotherSimulation.cpp)
target_include_directories(gyo_authority_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(gyo_authority_tests PRIVATE
    GYO_AUTHORITY_TEST_REPOSITORY_ROOT="${GYO_REPOSITORY_ROOT}"
    GYO_AUTHORITY_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(gyo_authority_tests PRIVATE GYO::Authority gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_authority_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_authority_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_authority_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS "authority socket" "authority registrations" "authority checks" "authority consistency"
        "authority sources")
    string(REPLACE " " "_" name "${group}")
    add_test(NAME authority.${name} COMMAND gyo_authority_tests "--test-case=${group}:*")
    set_tests_properties(authority.${name} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

# authority.authority_sources writes the G-5 export of the fixture; authority.sources_scan reads it with the
# product-side scanner (tests/common/authority_scan): the finding count must be the fixture's planted count.
set_tests_properties(authority.authority_sources PROPERTIES FIXTURES_SETUP authority_sources_export)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME authority.sources_scan
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/authority_sources_check.py"
        --root "${GYO_REPOSITORY_ROOT}" --list "${CMAKE_CURRENT_BINARY_DIR}/authority_sources.json")
set_tests_properties(authority.sources_scan PROPERTIES FIXTURES_REQUIRED authority_sources_export LABELS "cpu" TIMEOUT 60)
