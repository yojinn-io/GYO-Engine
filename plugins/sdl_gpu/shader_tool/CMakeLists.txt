# sdl_gpu's shader tool (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu", the shader tool): compiles render's
# HLSL to the formats SDL_GPU takes (SPIR-V, DXIL, Metal libraries) and writes them into one C++ source that sdl_gpu
# embeds. A separate HOST build, configured by plugins/sdl_gpu/CMakeLists.txt through ExternalProject: never part of
# a cross-compiled target graph. Its one source is in sdl_gpu's src/adapters/sdl/ (Design Invariant SDL-1: it calls
# SDL and SDL_shadercross); it is runtime code for the os guard (R1 to R4: file and process work through SDL).
cmake_minimum_required(VERSION 3.30)
project(GyoSDLGPUShaderTool LANGUAGES C CXX)

if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "sdl_gpu's shader tool must be built with a native host toolchain")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No sources use C++ modules; scanning would make compiler-cache lookups uncacheable on GCC.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
endforeach()
include(cmake/Dependencies.cmake)

add_executable(gyo_sdl_gpu_shader_tool ../src/adapters/sdl/ShaderTool.cpp)
# Every Windows executable of the repository declares UTF-8 as its ANSI code page (docs/architecture/os.md Invariant
# 11); this project is configured apart from the root, whose build layer applies it to the others.
include("${CMAKE_CURRENT_LIST_DIR}/../../../engine/core/os/cmake/GyoProcessCodePage.cmake")
gyo_os_process_code_page(gyo_sdl_gpu_shader_tool)
target_link_libraries(gyo_sdl_gpu_shader_tool PRIVATE gyo_host_shadercross SDL3::SDL3-static)
target_compile_features(gyo_sdl_gpu_shader_tool PRIVATE cxx_std_20)
if(MSVC)
    # /Zc:preprocessor matches the rest of GYO (build/cmake/GyoBuild.cmake), which this project does not inherit.
    target_compile_options(gyo_sdl_gpu_shader_tool PRIVATE /W4 /utf-8 /permissive- /Zc:preprocessor)
else()
    target_compile_options(gyo_sdl_gpu_shader_tool PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(WIN32)
    add_custom_command(TARGET gyo_sdl_gpu_shader_tool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:gyo_host_dxcompiler>" "${GYO_DXIL_RUNTIME}"
            "$<TARGET_FILE_DIR:gyo_sdl_gpu_shader_tool>" VERBATIM)
elseif(UNIX)
    set_property(TARGET gyo_sdl_gpu_shader_tool PROPERTY BUILD_RPATH "$<TARGET_FILE_DIR:gyo_host_dxcompiler>")
endif()

# The tool's own tests (label shader; L1 runs them on Linux from this host build): it compiles render's 2D pass for
# this host's formats, and it refuses a shader that does not compile and an unknown format (exit 1, no output).
if(BUILD_TESTING AND GYO_RENDER_SHADER_DIR)
    enable_testing()
    if(WIN32)
        set(host_formats dxil,spirv)
    elseif(APPLE)
        set(host_formats metallib)
    else()
        set(host_formats spirv)
    endif()
    set(work "${CMAKE_BINARY_DIR}/tool-tests")
    add_test(NAME sdl_gpu.shader_tool.compiles
        COMMAND gyo_sdl_gpu_shader_tool --output "${work}/compiles.cpp" --include "${GYO_RENDER_SHADER_DIR}" --formats ${host_formats}
                --work "${work}/compiles" --shader pass2d.vert vertex "${GYO_RENDER_SHADER_DIR}/pass2d.vert.hlsl"
                --shader pass2d.frag fragment "${GYO_RENDER_SHADER_DIR}/pass2d.frag.hlsl")
    add_test(NAME sdl_gpu.shader_tool.rejects
        COMMAND gyo_sdl_gpu_shader_tool --output "${work}/rejects.cpp" --include "${CMAKE_CURRENT_SOURCE_DIR}/tests" --formats ${host_formats}
                --work "${work}/rejects" --shader broken.frag fragment "${CMAKE_CURRENT_SOURCE_DIR}/tests/broken.frag.hlsl")
    add_test(NAME sdl_gpu.shader_tool.unknown_format
        COMMAND gyo_sdl_gpu_shader_tool --output "${work}/unknown.cpp" --include "${GYO_RENDER_SHADER_DIR}" --formats glsl
                --shader pass2d.vert vertex "${GYO_RENDER_SHADER_DIR}/pass2d.vert.hlsl")
    set_tests_properties(sdl_gpu.shader_tool.compiles sdl_gpu.shader_tool.rejects sdl_gpu.shader_tool.unknown_format
        PROPERTIES LABELS "shader" TIMEOUT 300)
    set_tests_properties(sdl_gpu.shader_tool.rejects sdl_gpu.shader_tool.unknown_format PROPERTIES WILL_FAIL TRUE)
endif()
