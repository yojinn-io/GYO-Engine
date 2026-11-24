# sdl_gpu (config/plugins.csv; docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu"): draws render's RenderCommands
# on the window through SDL_GPU. SDL3 is linked PRIVATE and only src/adapters/sdl/ and tests/adapters/sdl/ include
# SDL3/ (Design Invariant SDL-1).
#
# The shaders: render's HLSL (the GYO_RENDER_SHADER_DIR of GYO::Render) is compiled at build time by sdl_gpu's shader
# tool (shader_tool/, a separate host build) to this platform's formats and written into a generated source that the
# library embeds (src/EmbeddedShaders.hpp). The same host-build arrangement as master's GyoShaders.cmake.
include(ExternalProject)

set(GYO_SHADER_TOOL_EXECUTABLE "" CACHE FILEPATH "An existing native sdl_gpu shader tool (needed when cross compiling)")
set(GYO_SHADER_HOST_BUILD_DIR "${CMAKE_BINARY_DIR}/host-tools" CACHE PATH "The shader tool's own host build directory")
# AUTO: what SDL_GPU uses on the target system (Windows: Direct3D 12 or Vulkan; Linux: Vulkan; macOS: Metal).
set(GYO_SHADER_FORMATS "AUTO" CACHE STRING "Shader formats sdl_gpu embeds: AUTO or a list of spirv, dxil, metallib")

if(GYO_SHADER_TOOL_EXECUTABLE)
    if(NOT EXISTS "${GYO_SHADER_TOOL_EXECUTABLE}")
        message(FATAL_ERROR "GYO_SHADER_TOOL_EXECUTABLE does not exist: ${GYO_SHADER_TOOL_EXECUTABLE}")
    endif()
    set(gyo_shader_tool "${GYO_SHADER_TOOL_EXECUTABLE}")
    set(gyo_shader_tool_dependency "${GYO_SHADER_TOOL_EXECUTABLE}")
else()
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Cross compiling sdl_gpu needs GYO_SHADER_TOOL_EXECUTABLE built for the host")
    endif()
    if(CMAKE_HOST_WIN32)
        set(gyo_shader_tool "${GYO_SHADER_HOST_BUILD_DIR}/bin/gyo_sdl_gpu_shader_tool.exe")
    else()
        set(gyo_shader_tool "${GYO_SHADER_HOST_BUILD_DIR}/bin/gyo_sdl_gpu_shader_tool")
    endif()
    get_target_property(gyo_render_shader_dir GYO::Render GYO_RENDER_SHADER_DIR)
    set(host_args -DCMAKE_BUILD_TYPE=Release "-DBUILD_TESTING=${BUILD_TESTING}" "-DGYO_RENDER_SHADER_DIR=${gyo_render_shader_dir}")
    # IDE profiles often pick compilers and Ninja by absolute path without exporting CC/CXX: ExternalProject inherits
    # the generator, not these cache values. Native host tools only (cross builds stop above): no target toolchain
    # file, sysroot or compile flags are forwarded.
    foreach(host_tool CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_MAKE_PROGRAM)
        if(DEFINED ${host_tool} AND NOT "${${host_tool}}" STREQUAL "")
            list(APPEND host_args "-D${host_tool}:FILEPATH=${${host_tool}}")
        endif()
    endforeach()
    # An x86_64 target built on an arm64 Mac still builds and runs one native tool.
    if(CMAKE_HOST_APPLE)
        list(APPEND host_args "-DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_HOST_SYSTEM_PROCESSOR}")
    endif()
    # Source caches given explicitly are reused; target binaries are never shared with the host build.
    foreach(dependency SDL3 GYO_SPIRVCROSS GYO_SHADERCROSS GYO_DXC GYO_DXC_SOURCE)
        if(FETCHCONTENT_SOURCE_DIR_${dependency})
            list(APPEND host_args "-DFETCHCONTENT_SOURCE_DIR_${dependency}=${FETCHCONTENT_SOURCE_DIR_${dependency}}")
        endif()
    endforeach()
    # The name gyo_shader_host_tools is what the build layer reports in gyo-build.json (shader_host_tools) and CI
    # caches the host build for.
    ExternalProject_Add(gyo_shader_host_tools
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shader_tool"
        BINARY_DIR "${GYO_SHADER_HOST_BUILD_DIR}"
        CMAKE_ARGS ${host_args}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target gyo_sdl_gpu_shader_tool
        BUILD_BYPRODUCTS "${gyo_shader_tool}"
        BUILD_ALWAYS TRUE
        INSTALL_COMMAND ""
        USES_TERMINAL_CONFIGURE TRUE USES_TERMINAL_BUILD TRUE)
    set(gyo_shader_tool_dependency gyo_shader_host_tools)
endif()

set(gyo_shader_formats ${GYO_SHADER_FORMATS})
if(gyo_shader_formats STREQUAL "AUTO")
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(gyo_shader_formats dxil spirv)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(gyo_shader_formats spirv)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(gyo_shader_formats metallib)
    else()
        message(FATAL_ERROR "No AUTO shader formats for ${CMAKE_SYSTEM_NAME}")
    endif()
endif()
set(gyo_metal_args)
if("metallib" IN_LIST gyo_shader_formats)
    if(NOT CMAKE_HOST_APPLE)
        message(FATAL_ERROR "Metal libraries need a macOS host and Apple's Metal tools")
    endif()
    find_program(GYO_XCRUN_EXECUTABLE xcrun REQUIRED)
    list(APPEND gyo_metal_args --xcrun "${GYO_XCRUN_EXECUTABLE}" --work "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    # Without a deployment target the library is for the SDK's own macOS version.
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND gyo_metal_args --deployment-target "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
endif()
string(JOIN "," gyo_shader_format_list ${gyo_shader_formats})

get_target_property(gyo_render_shader_dir GYO::Render GYO_RENDER_SHADER_DIR)
file(GLOB gyo_render_shader_sources CONFIGURE_DEPENDS "${gyo_render_shader_dir}/*.hlsl" "${gyo_render_shader_dir}/*.hlsli")
set(gyo_embedded_shaders "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedShaders.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(OUTPUT "${gyo_embedded_shaders}"
    COMMAND "${gyo_shader_tool}" --output "${gyo_embedded_shaders}" --include "${gyo_render_shader_dir}"
            --formats "${gyo_shader_format_list}" ${gyo_metal_args}
            --shader pass2d.vert vertex "${gyo_render_shader_dir}/pass2d.vert.hlsl"
            --shader pass2d.frag fragment "${gyo_render_shader_dir}/pass2d.frag.hlsl"
    DEPENDS ${gyo_render_shader_sources} ${gyo_shader_tool_dependency} "${gyo_shader_tool}"
    COMMENT "Compiling render's shaders for sdl_gpu (${gyo_shader_format_list})"
    VERBATIM)

add_library(gyo_sdl_gpu STATIC
    src/SDLGPUPlugin.cpp
    src/adapters/sdl/GPUDevice.cpp
    "${gyo_embedded_shaders}"
)
add_library(GYO::SDLGPU ALIAS gyo_sdl_gpu)

target_include_directories(gyo_sdl_gpu
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# The Modules it reads and GYO::SDL come from config/plugins.csv (O-6).
target_link_libraries(gyo_sdl_gpu PUBLIC GYO::Base GYO::Kernel GYO::Orchestration GYO::Math
                      PRIVATE SDL3::SDL3-static)
gyo_link_registered_dependencies(sdl_gpu)
target_compile_features(gyo_sdl_gpu PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_sdl_gpu PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_sdl_gpu PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_sdl_gpu PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
