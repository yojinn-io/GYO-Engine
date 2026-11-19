# The JSON Module's tests (docs/architecture/target/v2/json-module-declaration.zh-Hant.md).
# gyo_json_component_tests names yyjson and fast_float directly: this is the
# adapter's own test directory, the one place outside engine/json/src that
# Design Invariant J-1 allows to.
add_executable(gyo_json_component_tests ComponentTests.cpp)
target_link_libraries(gyo_json_component_tests PRIVATE gyo_yyjson gyo_fast_float GYO::Base doctest::doctest)
add_test(NAME gyo_json_component_tests COMMAND gyo_json_component_tests)
set_tests_properties(gyo_json_component_tests PROPERTIES LABELS "cpu" TIMEOUT 60)
