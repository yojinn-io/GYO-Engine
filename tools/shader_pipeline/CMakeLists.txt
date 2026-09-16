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
    "${CMAKE_CURRENT_LIST_DIR}/../../engine/include")
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
    add_test(NAME shader.host_pipeline COMMAND gyo_shader_tool --self-test
        "${CMAKE_CURRENT_LIST_DIR}/../.." "${CMAKE_BINARY_DIR}/test-output")
    set_tests_properties(shader.host_pipeline PROPERTIES LABELS "shader;host" TIMEOUT 120)
    add_test(NAME shader.cmake_dependencies COMMAND ${CMAKE_COMMAND}
        "-DROOT=${CMAKE_CURRENT_LIST_DIR}/../.."
        "-DWORK=${CMAKE_BINARY_DIR}/cmake-test-output"
        "-DTOOL=$<TARGET_FILE:gyo_shader_tool>"
        "-DGENERATOR=${CMAKE_GENERATOR}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        -P "${CMAKE_CURRENT_LIST_DIR}/tests/CMakeIntegration.cmake")
    set_tests_properties(shader.cmake_dependencies PROPERTIES LABELS "shader;host;cmake" TIMEOUT 180)
    find_program(GYO_SHADER_TEST_NINJA ninja)
    if(GYO_SHADER_TEST_NINJA)
        add_test(NAME shader.host_toolchain COMMAND ${CMAKE_COMMAND}
            "-DROOT=${CMAKE_CURRENT_LIST_DIR}/../.."
            "-DWORK=${CMAKE_BINARY_DIR}/host-toolchain-test-output"
            "-DC_COMPILER=${CMAKE_C_COMPILER}" "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DNINJA=${GYO_SHADER_TEST_NINJA}" "-DUSES_MSVC=${MSVC}"
            -P "${CMAKE_CURRENT_LIST_DIR}/tests/HostToolchain.cmake")
        set_tests_properties(shader.host_toolchain PROPERTIES LABELS "shader;host;cmake" TIMEOUT 120
            SKIP_REGULAR_EXPRESSION "SKIP: native Ninja compiler probe requires")
    endif()
endif()
