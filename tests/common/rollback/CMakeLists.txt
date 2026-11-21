# Tests of the rollback Module (docs/architecture/target/v2/authority-rollback-declaration.zh-Hant.md, sections 5
# and 9.3). Synthetic data only; no product.
add_executable(gyo_rollback_tests TestMain.cpp RegistrationsTests.cpp)
target_include_directories(gyo_rollback_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gyo_rollback_tests PRIVATE GYO::Rollback gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_rollback_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_rollback_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_rollback_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS "rollback registrations")
    string(REPLACE " " "_" name "${group}")
    add_test(NAME rollback.${name} COMMAND gyo_rollback_tests "--test-case=${group}:*")
    set_tests_properties(rollback.${name} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()
