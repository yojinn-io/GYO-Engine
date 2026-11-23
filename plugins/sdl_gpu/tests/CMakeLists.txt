# Tests of sdl_gpu (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu"). They come and go with its row (Design
# Invariant SDL-2). None includes SDL3/: they drive the adapter through its interface (src/GPUDevice.hpp) and the
# Plugin through a Product.
#   sdl_gpu.startup, sdl_gpu.shaders (label cpu): the startup errors and the embedded shaders against render's ABI;
#     no GPU, every CI row.
#   sdl_gpu.pass2d, sdl_gpu.frames (label gpu): the 2D pass drawn on a GPU and read back, pixel by pixel; frames on a
#     hidden window. L1 runs them on Linux on Lavapipe under Xvfb; elsewhere they run on a machine with a GPU.
add_executable(gyo_sdl_gpu_tests TestMain.cpp SDLGPUTests.cpp Pass2DTests.cpp)
target_include_directories(gyo_sdl_gpu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(gyo_sdl_gpu_tests PRIVATE GYO::SDLGPU GYO::SDLWindow GYO::RenderWorld GYO::Render GYO::Input GYO::Window
                      doctest::doctest)
if(MSVC)
    target_compile_options(gyo_sdl_gpu_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_sdl_gpu_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_sdl_gpu_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME sdl_gpu.startup COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu startup:*")
add_test(NAME sdl_gpu.shaders COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu shaders:*")
set_tests_properties(sdl_gpu.startup sdl_gpu.shaders PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
add_test(NAME sdl_gpu.pass2d COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu pass2d:*")
add_test(NAME sdl_gpu.frames COMMAND gyo_sdl_gpu_tests "--test-case=sdl_gpu frames:*")
set_tests_properties(sdl_gpu.pass2d sdl_gpu.frames PROPERTIES LABELS "gpu" TIMEOUT 120
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
