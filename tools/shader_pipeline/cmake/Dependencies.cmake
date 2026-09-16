include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# All downloaded sources and tools remain inside the host build directory.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.4.0)
FetchContent_MakeAvailable(SDL3)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.12.0)
FetchContent_MakeAvailable(nlohmann_json)

set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(gyo_spirvcross
    URL https://codeload.github.com/KhronosGroup/SPIRV-Cross/tar.gz/1a6169566c73d3da552748fc372fe2bbb856e46e
    URL_HASH SHA256=0f295b214b164e42a1d21537c8da7b44569806c16220dda9798558edfaacd11e)
FetchContent_MakeAvailable(gyo_spirvcross)

FetchContent_Declare(gyo_shadercross
    URL https://codeload.github.com/libsdl-org/SDL_shadercross/tar.gz/e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba
    URL_HASH SHA256=342bb6a8e734745eb5951f25c87fa7aad62f46b3736def8681d9fa7ad046887f
    SOURCE_SUBDIR gyo-no-upstream-cmake)
FetchContent_MakeAvailable(gyo_shadercross)

if(WIN32)
    FetchContent_Declare(gyo_dxc
        URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/dxc_2026_02_20.zip
        URL_HASH SHA256=a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e
        SOURCE_SUBDIR gyo-no-upstream-cmake)
    FetchContent_MakeAvailable(gyo_dxc)
    if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
        set(dxc_arch arm64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(dxc_arch x64)
    else()
        message(FATAL_ERROR "Shader host tools require a 64-bit Windows toolchain")
    endif()
    add_library(gyo_host_dxcompiler SHARED IMPORTED GLOBAL)
    set_target_properties(gyo_host_dxcompiler PROPERTIES
        IMPORTED_LOCATION "${gyo_dxc_SOURCE_DIR}/bin/${dxc_arch}/dxcompiler.dll"
        IMPORTED_IMPLIB "${gyo_dxc_SOURCE_DIR}/lib/${dxc_arch}/dxcompiler.lib")
    set(GYO_DXIL_RUNTIME "${gyo_dxc_SOURCE_DIR}/bin/${dxc_arch}/dxil.dll")
    set(GYO_DXC_REVISION "v1.9.2602")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    FetchContent_Declare(gyo_dxc
        URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/linux_dxc_2026_02_20.x86_64.tar.gz
        URL_HASH SHA256=a1d3e3b5e1c5685b3eb27d5e8890e41d87df45def05112a2d6f1a63a931f7d60
        SOURCE_SUBDIR gyo-no-upstream-cmake)
    FetchContent_MakeAvailable(gyo_dxc)
    add_library(gyo_host_dxcompiler SHARED IMPORTED GLOBAL)
    set_target_properties(gyo_host_dxcompiler PROPERTIES
        IMPORTED_LOCATION "${gyo_dxc_SOURCE_DIR}/lib/libdxcompiler.so")
    set(GYO_DXC_REVISION "v1.9.2602")
else()
    # Apple (and other native hosts) build DXC including its SPIR-V generator.
    # A pinned git checkout also retains DXC's own dependency gitlinks.
    FetchContent_Declare(gyo_dxc_source
        GIT_REPOSITORY https://github.com/libsdl-org/DirectXShaderCompiler.git
        GIT_TAG 2c84a1c5ab7091608c97df6ba5ccf46e71c322eb
        GIT_SUBMODULES_RECURSE TRUE SOURCE_SUBDIR gyo-no-upstream-cmake)
    FetchContent_MakeAvailable(gyo_dxc_source)
    set(BUILD_SHARED_LIBS OFF)
    set(DXC_COVERAGE OFF CACHE BOOL "" FORCE)
    include("${gyo_dxc_source_SOURCE_DIR}/cmake/caches/PredefinedParams.cmake")
    # DXC declares an older CMake policy scope. Cache these overrides after
    # its preset so option() cannot resurrect its large upstream test suite.
    set(HLSL_ENABLE_DEBUG_ITERATORS ON CACHE BOOL "" FORCE)
    set(HLSL_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
    set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
    set(HLSL_DISABLE_SOURCE_GENERATION TRUE CACHE BOOL "" FORCE)
    set(SPIRV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${gyo_dxc_source_SOURCE_DIR}" "${gyo_dxc_source_BINARY_DIR}" EXCLUDE_FROM_ALL)
    add_library(gyo_host_dxcompiler ALIAS dxcompiler)
    add_dependencies(dxcompiler dxildll)
    set(GYO_DXC_REVISION "2c84a1c5ab7091608c97df6ba5ccf46e71c322eb")
endif()

# Upstream's runtime installer fetches unrelated floating dependencies. This
# private wrapper builds its unmodified library source without that installer.
add_library(gyo_host_shadercross STATIC "${gyo_shadercross_SOURCE_DIR}/src/SDL_shadercross.c")
target_include_directories(gyo_host_shadercross PUBLIC "${gyo_shadercross_SOURCE_DIR}/include")
target_compile_definitions(gyo_host_shadercross PRIVATE SDL_SHADERCROSS_DXC)
target_link_libraries(gyo_host_shadercross PUBLIC SDL3::SDL3-static spirv-cross-c gyo_host_dxcompiler)
target_compile_features(gyo_host_shadercross PRIVATE c_std_99)
