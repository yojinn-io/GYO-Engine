# Tests of the render Module and render_world (docs/architecture/target/v2/render.zh-Hant.md, RD-1 to RD-4). Data and
# arithmetic only: no backend, no GPU (the 2D pass on a GPU is sdl_gpu's test, label gpu).
add_executable(gyo_render_tests TestMain.cpp RenderTests.cpp RenderWorldTests.cpp Pipeline2dTests.cpp Pipeline3dTests.cpp PipelineWorldTests.cpp VisibilityTests.cpp)
target_link_libraries(gyo_render_tests PRIVATE GYO::Render GYO::RenderWorld GYO::WorldRender GYO::CameraRender GYO::SpriteRender GYO::MeshRender
    GYO::Visibility GYO::EntityIds gyo_test_support doctest::doctest)
# The asset path of sprite_render reads a real PNG through world_asset (the store's test executor and content).
target_link_libraries(gyo_render_tests PRIVATE GYO::WorldAsset GYO::Image GYO::Model)
target_include_directories(gyo_render_tests PRIVATE "${GYO_REPOSITORY_ROOT}/tests/common/asset" "${GYO_REPOSITORY_ROOT}/tests/common/world_asset")
target_compile_definitions(gyo_render_tests PRIVATE GYO_ASSET_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    GYO_RENDER_TEST_PNG="${GYO_REPOSITORY_ROOT}/tests/common/image/contract/gradient_rgba_16x8.png"
    GYO_RENDER_TEST_FBX="${GYO_REPOSITORY_ROOT}/tests/common/model/contract/skinned_bar.fbx")
if(MSVC)
    target_compile_options(gyo_render_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(gyo_render_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_render_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME render.data COMMAND gyo_render_tests "--test-case=render:*")
add_test(NAME render.world COMMAND gyo_render_tests "--test-case=render_world:*")
add_test(NAME render.pipeline2d COMMAND gyo_render_tests "--test-case=pipeline2d:*")
add_test(NAME render.extract COMMAND gyo_render_tests "--test-case=render_extract:*")
add_test(NAME render.pipeline3d COMMAND gyo_render_tests "--test-case=pipeline3d:*,compose:*")
add_test(NAME render.visibility COMMAND gyo_render_tests "--test-case=visibility:*")
set_tests_properties(render.data render.world render.pipeline2d render.extract render.pipeline3d render.visibility PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# RD-1: render links Base, the kernel (for HashValue), Math and, for their types only, the Modules whose data the
# pipelines read (asset, image, world_asset, transform, camera); no Plugin, no SDL. render_world links render and the
# orchestration layer besides them; the render World's units link render_world, and sprite_render camera_render.
function(gyo_expect_render_links)
    gyo_expect_links(gyo_render GYO::Asset GYO::Base GYO::Camera GYO::Image GYO::Kernel GYO::Math GYO::Model GYO::Transform GYO::WorldAsset)
    gyo_expect_links(gyo_render_world GYO::Base GYO::Kernel GYO::Orchestration GYO::Render)
    gyo_expect_links(gyo_world_render GYO::Base GYO::Camera GYO::Kernel GYO::Orchestration GYO::Render)
    gyo_expect_links(gyo_camera_render GYO::Base GYO::Kernel GYO::Orchestration GYO::Render GYO::RenderWorld)
    gyo_expect_links(gyo_sprite_render GYO::Base GYO::CameraRender GYO::Kernel GYO::Orchestration GYO::Render GYO::RenderWorld)
    gyo_expect_links(gyo_mesh_render GYO::Base GYO::CameraRender GYO::Kernel GYO::Orchestration GYO::Render GYO::RenderWorld)
    # big-map declaration K4: the visibility Module links render, transform and camera (types and set names) and the
    # core; no render World unit.
    gyo_expect_links(gyo_visibility GYO::Base GYO::Camera GYO::Kernel GYO::Math GYO::Orchestration GYO::Render GYO::Transform)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_render_links)
