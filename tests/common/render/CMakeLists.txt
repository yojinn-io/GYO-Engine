# Tests of the render Module and render_world (docs/architecture/target/v2/render.zh-Hant.md, RD-1 to RD-4). Data and
# arithmetic only: no backend, no GPU (the 2D pass on a GPU is sdl_gpu's test, label gpu).
add_executable(gyo_render_tests TestMain.cpp RenderTests.cpp RenderWorldTests.cpp)
target_link_libraries(gyo_render_tests PRIVATE GYO::Render GYO::RenderWorld gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_render_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(gyo_render_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_render_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME render.data COMMAND gyo_render_tests "--test-case=render:*")
add_test(NAME render.world COMMAND gyo_render_tests "--test-case=render_world:*")
set_tests_properties(render.data render.world PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# RD-1: render links Base, the kernel (for HashValue) and Math only (no other Module, no Plugin, no SDL); render_world
# links render and the orchestration layer besides them.
function(gyo_expect_render_links)
    gyo_expect_links(gyo_render GYO::Base GYO::Kernel GYO::Math)
    gyo_expect_links(gyo_render_world GYO::Base GYO::Kernel GYO::Orchestration GYO::Render)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_render_links)
