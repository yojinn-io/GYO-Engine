# Tests of sdl_gpu (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu"). They come and go with its row (Design
# Invariant SDL-2). None includes SDL3/: they drive the adapter through its interface (src/GPUDevice.hpp) and the
# Plugin through a Product.
#   sdl_gpu.startup, sdl_gpu.shaders, sdl_gpu.present (label cpu): the startup errors, the embedded shaders against
#     render's ABI and the present mode's fallback chain; no GPU, every CI row. sdl_gpu.contracts, sdl_gpu.unwritten
#     (label cpu): the checks the backend makes on a frame's data before it draws (RP-3), no GPU. sdl_gpu.framedraws
#     (label cpu): the adapter's account of a frame's drawings until it is handed to the GPU.
#   sdl_gpu.pass2d, sdl_gpu.pass3d, sdl_gpu.targets, sdl_gpu.phases, sdl_gpu.hdr, sdl_gpu.frames (label gpu): the 2D, 3D,
#     full-screen and depth-only passes (and render targets) drawn on a GPU and read back,
#     pixel by pixel, with SDL's GPU validation on; frames on a shown window (at least one must reach it), the present mode set from render's Presentation, World replacement. sdl_gpu.budget (label gpu) prints the frame
#     time of a frame of a common 2D game. L1 runs them on Linux on Lavapipe under Xvfb; elsewhere they run on a machine with a GPU.
#
# The tests' own full-screen shaders (shaders/, render-pipeline declaration section 14) are compiled by the same tool
# into a table of their own (TestShaders(), TestShaders.hpp) that the tests hand to GPUDevice::Create: nothing of them
# is in the Plugin.
set(gyo_test_shaders "${CMAKE_CURRENT_BINARY_DIR}/generated/TestShaders.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(GLOB gyo_test_shader_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.hlsl")
add_custom_command(OUTPUT "${gyo_test_shaders}"
    COMMAND "${gyo_shader_tool}" --output "${gyo_test_shaders}" --include "${gyo_render_shader_dir}" --function TestShaders
            --formats "${gyo_shader_format_list}" ${gyo_metal_args} --work "${CMAKE_CURRENT_BINARY_DIR}/shaders"
            --shader test_invert.frag fragment "${CMAKE_CURRENT_SOURCE_DIR}/shaders/test_invert.frag.hlsl"
            --shader test_shift.frag fragment "${CMAKE_CURRENT_SOURCE_DIR}/shaders/test_shift.frag.hlsl"
            --shader test_constant.frag fragment "${CMAKE_CURRENT_SOURCE_DIR}/shaders/test_constant.frag.hlsl"
    DEPENDS ${gyo_test_shader_sources} ${gyo_render_shader_sources} ${gyo_shader_tool_dependency} "${gyo_shader_tool}"
    COMMENT "Compiling sdl_gpu's test shaders (${gyo_shader_format_list})"
    VERBATIM)
add_executable(gyo_sdl_gpu_tests TestMain.cpp SDLGPUTests.cpp Pass2DTests.cpp Pass3DTests.cpp PhaseTests.cpp HdrTests.cpp "${gyo_test_shaders}")
target_include_directories(gyo_sdl_gpu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
# A failed SDL assertion ends the run, no dialog (plugins/sdl/tests/adapters/sdl/UnattendedAssertions.hpp).
target_link_libraries(gyo_sdl_gpu_tests PRIVATE gyo_sdl_unattended_assertions)
target_link_libraries(gyo_sdl_gpu_tests PRIVATE GYO::SDLGPU GYO::SDLWindow GYO::RenderWorld GYO::Render GYO::Input GYO::Window
                      GYO::Camera GYO::Transform
                      doctest::doctest)
if(MSVC)
    target_compile_options(gyo_sdl_gpu_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_sdl_gpu_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_sdl_gpu_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME sdl_gpu.startup COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu startup:*")
add_test(NAME sdl_gpu.shaders COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu shaders:*")
add_test(NAME sdl_gpu.present COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu present:*")
set_tests_properties(sdl_gpu.startup sdl_gpu.shaders sdl_gpu.present PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
add_test(NAME sdl_gpu.pass2d COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu pass2d:*")
add_test(NAME sdl_gpu.pass3d COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu pass3d:*")
add_test(NAME sdl_gpu.frames COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu frames:*")
# Render targets and texture formats (render-pipeline declaration section 13).
add_test(NAME sdl_gpu.targets COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu targets:*")
# Full-screen and depth-only passes (render-pipeline declaration section 14).
add_test(NAME sdl_gpu.phases COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu phases:*")
# A Hdr camera's picture and its tonemapping pass (render-pipeline declaration section 16).
add_test(NAME sdl_gpu.hdr COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu hdr:*")
# The checks the backend makes on a frame's data before it draws (RenderContracts.hpp; no GPU, label cpu): a frame
# that keeps them passes.
add_test(NAME sdl_gpu.contracts COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu contracts:*")
# The adapter's account of a frame's drawings until it is handed to the GPU (FrameDraws.hpp; no GPU, label cpu).
add_test(NAME sdl_gpu.framedraws COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu frame draws:*")
# RP-3: sampling a render target neither drawn on nor retained with a picture this frame is a Programmer Error the
# backend asserts before it draws (RenderContracts.hpp; no GPU, label cpu). The case is skipped in any other run (it
# ends the process); its own handler prints the tag and exits 0 when the assertion fails.
add_test(NAME sdl_gpu.unwritten COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu unwritten:*" --no-skip)
# Prints the declared frame-time budget's measurement (render.zh-Hant.md section 2); judges nothing.
add_test(NAME sdl_gpu.budget COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu budget:*" -s)
set_tests_properties(sdl_gpu.pass2d sdl_gpu.pass3d sdl_gpu.frames sdl_gpu.targets sdl_gpu.phases sdl_gpu.hdr sdl_gpu.budget PROPERTIES LABELS "gpu" TIMEOUT 120
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
set_tests_properties(sdl_gpu.contracts sdl_gpu.framedraws PROPERTIES LABELS "cpu" TIMEOUT 60 FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
set_tests_properties(sdl_gpu.unwritten PROPERTIES LABELS "cpu" TIMEOUT 60 PASS_REGULAR_EXPRESSION "\\[gt-unwritten-asserted\\]")
