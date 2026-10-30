add_executable(gyo_ui_tests
    test_main.cpp
    UiDocumentCodecTests.cpp
    UiRendererTests.cpp
    UiRuntimeTests.cpp
)

target_link_libraries(gyo_ui_tests PRIVATE
    GYO::Ui
    GYO::UiRenderer
    doctest::doctest
)

add_test(NAME gyo_ui_tests COMMAND gyo_ui_tests)
target_include_directories(gyo_ui_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests/common/support")

# GYO::Ui Design Invariant 2 (docs/ui_toolchain.md): no dependency on GYO::Input.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME ui.dependency_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ui_dependency_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME ui.dependency_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_ui_dependency_guard.py" -v)
set_tests_properties(ui.dependency_guard ui.dependency_guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)
