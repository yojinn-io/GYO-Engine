cmake_minimum_required(VERSION 3.30)
# Set this before project() initializes the Apple toolchain. The application
# and offline Metal compiler must share one explicit deployment target.
if((CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR
    (CMAKE_HOST_APPLE AND NOT CMAKE_SYSTEM_NAME)) AND
   (NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL ""))
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS runtime version" FORCE)
endif()
project(GYO-Engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GYO_BUILD_UI_EDITOR "Build the standalone GYO JSON UI editor" ON)
option(BUILD_TESTING "Enable the optional quality-management build" OFF)
option(GYO_ENABLE_PACKAGING "Enable release manifests and acceptance contracts" OFF)

include(CTest)
include(cmake/GyoApps.cmake)
include(cmake/GyoAppPackaging.cmake)
include(cmake/GyoAppProject.cmake)
gyo_configure_apps()
gyo_resolve_components(${GYO_REQUESTED_COMPONENTS})

if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()

# Keep dependency sources private to each build tree. Disconnected updates still
# allow the first population, but do not mutate an already populated dependency.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL
    "Do not update already-populated FetchContent dependencies")

add_subdirectory(third_party)
add_subdirectory(engine)
add_subdirectory(model)
add_subdirectory(collision)
add_subdirectory(text)
if(GYO_ENABLE_SDL_PLATFORM)
    add_subdirectory(platform)
endif()
add_subdirectory(input)
add_subdirectory(render)
add_subdirectory(ui)
add_subdirectory(apps)

if(GYO_BUILD_UI_EDITOR)
    add_subdirectory(tools)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
    add_test(NAME cmake.render_options
        COMMAND ${CMAKE_COMMAND} -P "${PROJECT_SOURCE_DIR}/tests/cmake/RenderOptionsTests.cmake")
    add_test(NAME cmake.components
        COMMAND ${CMAKE_COMMAND} -P "${PROJECT_SOURCE_DIR}/tests/cmake/ComponentTests.cmake")
    add_test(NAME cmake.app_registry
        COMMAND ${CMAKE_COMMAND} "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/AppRegistryTests.cmake")
    add_test(NAME cmake.app_composition
        COMMAND ${CMAKE_COMMAND} "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/AppCompositionTests.cmake")
    add_test(NAME cmake.app_packaging
        COMMAND ${CMAKE_COMMAND} "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/AppPackagingTests.cmake")
    add_test(NAME cmake.app_project
        COMMAND ${CMAKE_COMMAND} "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/AppProjectTests.cmake")
    add_test(NAME cmake.app_tooling
        COMMAND ${CMAKE_COMMAND} "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/AppToolingTests.cmake")
    add_test(NAME cmake.msvc_runtime
        COMMAND ${CMAKE_COMMAND} "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/MsvcRuntimeTests.cmake")
endif()

gyo_finalize_app_installation()
gyo_finalize_app_packages()

# CI reads the configured graph, never guesses shader requirements from app names
# or the presence of restored cache directories.
set(_gyo_shader_bundles false)
set(_gyo_shader_host_tools false)
if(TARGET gyo_builtin_shaders)
    set(_gyo_shader_bundles true)
endif()
if(TARGET gyo_shader_host_tools)
    set(_gyo_shader_host_tools true)
endif()
file(WRITE "${PROJECT_BINARY_DIR}/gyo-build.json"
    "{\"shader_bundles\":${_gyo_shader_bundles},\"shader_host_tools\":${_gyo_shader_host_tools}}\n")

# Every test has an explicit execution class. Backends and shader helpers
# label their tests at registration; existing deterministic tests default CPU.
function(gyo_label_tests directory)
    get_property(_gyo_tests DIRECTORY "${directory}" PROPERTY TESTS)
    foreach(_gyo_test IN LISTS _gyo_tests)
        get_property(_gyo_labels TEST "${_gyo_test}" DIRECTORY "${directory}" PROPERTY LABELS)
        if(NOT _gyo_labels)
            set_property(TEST "${_gyo_test}" DIRECTORY "${directory}" PROPERTY LABELS cpu)
        endif()
    endforeach()
    get_property(_gyo_subdirs DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(_gyo_subdir IN LISTS _gyo_subdirs)
        gyo_label_tests("${_gyo_subdir}")
    endforeach()
endfunction()
if(BUILD_TESTING)
    gyo_label_tests("${CMAKE_CURRENT_SOURCE_DIR}")
endif()
