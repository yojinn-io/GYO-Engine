# Tests of save versions (docs/architecture/target/v2/restore-entry-declaration.zh-Hant.md, section 19.5;
# Design Invariants SV-1 to SV-3 in docs/architecture/target/v2/save-version.zh-Hant.md).
add_executable(gyo_save_version_tests TestMain.cpp SaveVersionTests.cpp)
target_link_libraries(gyo_save_version_tests PRIVATE GYO::SaveVersion GYO::Kernel doctest::doctest)
if(MSVC)
    target_compile_options(gyo_save_version_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_save_version_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_save_version_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME save_version.check COMMAND gyo_save_version_tests "--test-case=check:*")
set_tests_properties(save_version.check PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
