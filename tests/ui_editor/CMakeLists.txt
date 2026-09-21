add_executable(gyo_ui_editor_core_tests EditorCoreTests.cpp)
target_compile_features(gyo_ui_editor_core_tests PRIVATE cxx_std_20)
target_link_libraries(gyo_ui_editor_core_tests PRIVATE gyo_ui_editor_core)
add_test(NAME gyo_ui_editor.core COMMAND gyo_ui_editor_core_tests)
set_tests_properties(gyo_ui_editor.core PROPERTIES LABELS "cpu;editor")

add_test(NAME gyo_ui_editor.validate
    COMMAND gyo_ui_editor --validate "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/minimal.ui.json")
set_tests_properties(gyo_ui_editor.validate PROPERTIES LABELS "cpu;editor")

add_test(NAME gyo_ui_editor.content_cli
    COMMAND ${CMAKE_COMMAND} "-DEDITOR=$<TARGET_FILE:gyo_ui_editor>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-fixtures"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/EditorCliTests.cmake")
set_tests_properties(gyo_ui_editor.content_cli PROPERTIES LABELS "cpu;editor" TIMEOUT 30)

if(TARGET gyo_ui_editor_preview)
    add_executable(gyo_ui_editor_preview_tests PreviewAssetTests.cpp)
    target_link_libraries(gyo_ui_editor_preview_tests PRIVATE
        gyo_ui_editor_preview imgui::sdl3_sdlrenderer3)
    if(MSVC)
        target_compile_options(gyo_ui_editor_preview_tests PRIVATE /utf-8)
        target_compile_definitions(gyo_ui_editor_preview_tests PRIVATE NOMINMAX)
    endif()
    gyo_copy_runtime_dlls(gyo_ui_editor_preview_tests)
    add_test(NAME gyo_ui_editor.preview COMMAND gyo_ui_editor_preview_tests)
    set_tests_properties(gyo_ui_editor.preview PROPERTIES LABELS "cpu;editor" TIMEOUT 30)
endif()
