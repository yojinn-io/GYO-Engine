add_executable(gyo_audio_editor_core_tests AudioEditorCoreTests.cpp AudioEditorEditTests.cpp
    AudioEditorRegionsTests.cpp)
target_compile_features(gyo_audio_editor_core_tests PRIVATE cxx_std_20)
# GYO::Engine and GYO::Audio for the guard of Design Invariant 2: the
# converted clip loads through SoundLoader and ClipBank. GYO::Os so the region
# tests ask the OS layer whether names that differ in case are one file.
target_link_libraries(gyo_audio_editor_core_tests PRIVATE
    gyo_audio_editor_core GYO::Engine GYO::Audio GYO::Os doctest::doctest nlohmann_json::nlohmann_json)
target_compile_definitions(gyo_audio_editor_core_tests PRIVATE
    GYO_AUDIO_EDITOR_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME gyo_audio_editor.core COMMAND gyo_audio_editor_core_tests)
set_tests_properties(gyo_audio_editor.core PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

add_test(NAME gyo_audio_editor.cli
    COMMAND ${CMAKE_COMMAND} "-DEDITOR=$<TARGET_FILE:gyo_audio_editor>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-work" "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/AudioEditorCliTests.cmake")
set_tests_properties(gyo_audio_editor.cli PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

# audio_editor Design Invariant 1 (docs/audio_editor/architecture.md): no game, and no SDL of its own.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME gyo_audio_editor.dependency_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/audio_editor_dependency_guard.py"
        --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME gyo_audio_editor.dependency_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}"
        -p "test_audio_editor_dependency_guard.py" -v)
set_tests_properties(gyo_audio_editor.dependency_guard gyo_audio_editor.dependency_guard_selftest
    PROPERTIES LABELS "cpu;editor" TIMEOUT 60)
