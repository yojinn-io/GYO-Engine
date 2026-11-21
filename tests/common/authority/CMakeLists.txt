# Tests of the authority Module (docs/architecture/target/v2/authority-rollback-declaration.zh-Hant.md, sections 4
# and 9.3). Synthetic data only; no product.
add_executable(gyo_authority_tests TestMain.cpp StrategySocketTests.cpp RegistrationsTests.cpp ServerChecksTests.cpp
    ConsistencyTests.cpp)
target_include_directories(gyo_authority_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gyo_authority_tests PRIVATE GYO::Authority gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_authority_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_authority_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_authority_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS "authority socket" "authority registrations" "authority checks" "authority consistency")
    string(REPLACE " " "_" name "${group}")
    add_test(NAME authority.${name} COMMAND gyo_authority_tests "--test-case=${group}:*")
    set_tests_properties(authority.${name} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()
