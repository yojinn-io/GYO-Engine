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
