# The tools' shared layer (docs/framework/architecture.md); added with it, when
# a tool is selected.
add_executable(gyo_framework_tests FrameworkCoreTests.cpp)
target_compile_features(gyo_framework_tests PRIVATE cxx_std_20)
target_link_libraries(gyo_framework_tests PRIVATE GYO::ToolFramework doctest::doctest)
add_test(NAME gyo_framework.core COMMAND gyo_framework_tests)
set_tests_properties(gyo_framework.core PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

# Design Invariants 1-3: not a tool, one dependency direction, neutral.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME gyo_framework.dependency_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/framework_dependency_guard.py"
        --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME gyo_framework.dependency_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}"
        -p "test_framework_dependency_guard.py" -v)
set_tests_properties(gyo_framework.dependency_guard gyo_framework.dependency_guard_selftest
    PROPERTIES LABELS "cpu;editor" TIMEOUT 60)
