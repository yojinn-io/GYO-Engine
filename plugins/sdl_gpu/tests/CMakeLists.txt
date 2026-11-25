# Tests of sdl_gpu (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu"). They come and go with its row (Design
# Invariant SDL-2). None includes SDL3/: they drive the adapter through its interface (src/GPUDevice.hpp) and the
# Plugin through a Product.
#   sdl_gpu.startup, sdl_gpu.shaders, sdl_gpu.present (label cpu): the startup errors, the embedded shaders against
#     render's ABI and the present mode's fallback chain; no GPU, every CI row.
#   sdl_gpu.pass2d, sdl_gpu.pass3d, sdl_gpu.frames (label gpu): the 2D and 3D passes drawn on a GPU and read back,
#     pixel by pixel, with SDL's GPU validation on; frames on a shown window (at least one must reach it), the present mode set from render's Presentation, World replacement. sdl_gpu.budget (label gpu) prints the frame
#     time of a frame of a common 2D game. L1 runs them on Linux on Lavapipe under Xvfb; elsewhere they run on a machine with a GPU.
add_executable(gyo_sdl_gpu_tests TestMain.cpp SDLGPUTests.cpp Pass2DTests.cpp Pass3DTests.cpp)
target_include_directories(gyo_sdl_gpu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
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
# Prints the declared frame-time budget's measurement (render.zh-Hant.md section 2); judges nothing.
add_test(NAME sdl_gpu.budget COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu budget:*" -s)
set_tests_properties(sdl_gpu.pass2d sdl_gpu.pass3d sdl_gpu.frames sdl_gpu.budget PROPERTIES LABELS "gpu" TIMEOUT 120
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
