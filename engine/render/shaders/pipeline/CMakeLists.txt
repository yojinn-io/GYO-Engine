cmake_minimum_required(VERSION 3.30)
project(GyoShaderTools LANGUAGES C CXX)

# This is a separate HOST build. Never add it to a cross-compiled target graph.
if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "GyoShaderTools must be built with a native host toolchain")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
endforeach()
include(cmake/Dependencies.cmake)

add_executable(gyo_shader_tool src/main.cpp)
target_include_directories(gyo_shader_tool PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../../base/include")
target_link_libraries(gyo_shader_tool PRIVATE
    gyo_host_shadercross spirv-cross-c nlohmann_json::nlohmann_json SDL3::SDL3-static)
target_compile_features(gyo_shader_tool PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(gyo_shader_tool PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(gyo_shader_tool PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_compile_definitions(gyo_shader_tool PRIVATE
    GYO_SHADERCROSS_REVISION="e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba"
    GYO_SPIRVCROSS_REVISION="1a6169566c73d3da552748fc372fe2bbb856e46e"
    GYO_DXC_REVISION="${GYO_DXC_REVISION}")
if(WIN32)
    add_custom_command(TARGET gyo_shader_tool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:gyo_host_dxcompiler>" "${GYO_DXIL_RUNTIME}"
            "$<TARGET_FILE_DIR:gyo_shader_tool>" VERBATIM)
elseif(UNIX)
    set_property(TARGET gyo_shader_tool PROPERTY BUILD_RPATH "$<TARGET_FILE_DIR:gyo_host_dxcompiler>")
endif()

include(CTest)
if(BUILD_TESTING)
    get_filename_component(GYO_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
    add_subdirectory("${GYO_REPOSITORY_ROOT}/tests/common/shader_pipeline" tests)
endif()
