# Tests of the save version Module (docs/architecture/target/v2/restore-entry-declaration.zh-Hant.md, section 19.5;
# Design Invariants SV-1 to SV-3 in docs/architecture/target/v2/save-version.zh-Hant.md).
add_executable(gyo_save_version_tests TestMain.cpp SaveVersionTests.cpp)
target_link_libraries(gyo_save_version_tests PRIVATE GYO::SaveVersion GYO::Kernel doctest::doctest)
if(MSVC)
    target_compile_options(gyo_save_version_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_save_version_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_save_version_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS check configure)
    add_test(NAME save_version.${group} COMMAND gyo_save_version_tests "--test-case=${group}:*")
    set_tests_properties(save_version.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

# V7: the table gyo_product_units generates from the real register files (no REGISTERS: config/), save_version not
# listed. The test links the Module only for its type (Policy), not to enable it.
add_executable(gyo_save_version_default_units_tests TestMain.cpp DefaultUnitsTests.cpp)
gyo_product_units(gyo_save_version_default_units_tests)
target_link_libraries(gyo_save_version_default_units_tests PRIVATE GYO::SaveVersion doctest::doctest)
if(MSVC)
    target_compile_options(gyo_save_version_default_units_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_save_version_default_units_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_save_version_default_units_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME save_version.default COMMAND gyo_save_version_default_units_tests "--test-case=default:*")
set_tests_properties(save_version.default PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
