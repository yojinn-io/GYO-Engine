add_executable(gyo_render_tests
    test_main.cpp
    ColorTransformTests.cpp
    PrimitiveMeshTests.cpp
    RenderQueueTests.cpp
    RendererTests.cpp
    ShaderLibraryTests.cpp
)

target_link_libraries(gyo_render_tests PRIVATE
    GYO::Render
    doctest::doctest
    nlohmann_json::nlohmann_json
)

add_test(NAME gyo_render_tests COMMAND gyo_render_tests)
