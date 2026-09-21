add_executable(gyo_ui_editor_core_tests EditorCoreTests.cpp)
target_compile_features(gyo_ui_editor_core_tests PRIVATE cxx_std_20)
target_link_libraries(gyo_ui_editor_core_tests PRIVATE gyo_ui_editor_core)
add_test(NAME gyo_ui_editor.core COMMAND gyo_ui_editor_core_tests)
set_tests_properties(gyo_ui_editor.core PROPERTIES LABELS "cpu;editor")

add_test(NAME gyo_ui_editor.validate
    COMMAND gyo_ui_editor --validate "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/minimal.ui.json")
set_tests_properties(gyo_ui_editor.validate PROPERTIES LABELS "cpu;editor")
