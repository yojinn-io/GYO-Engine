set(GYO_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
cmake_path(NORMAL_PATH GYO_REPOSITORY_ROOT)
set(GYO_OUTPUT_ROOT "${GYO_REPOSITORY_ROOT}/build/target" CACHE PATH "Assembled local products")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No sources use C++ modules. CMake would otherwise scan every C++20 source
# and add GCC module flags that make compiler-cache lookups uncacheable.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
option(BUILD_TESTING "Build centralized engine and project tests" OFF)
option(GYO_BUILD_EXAMPLES "Build the programs under examples/" ${BUILD_TESTING})
option(GYO_ENABLE_PACKAGING "Generate product manifests and acceptance contracts" OFF)
set(GYO_PLUGINS "" CACHE STRING
    "Plugins to build besides those the configured products carry: config/plugins.csv names, or ALL")
include(CTest)
include("${CMAKE_CURRENT_LIST_DIR}/GyoGames.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/GyoTools.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/GyoGameProject.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/GyoPackaging.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/GyoUnitRegistry.cmake")
# Design Invariant 11 of docs/architecture/os.md: every Windows executable of
# the repository's own sources declares UTF-8 as its ANSI code page. The
# directories added below are recorded; the manifest is applied once every
# target exists, and tests/common/os checks it afterwards.
include("${CMAKE_CURRENT_LIST_DIR}/GyoCodePage.cmake")
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_apply_process_code_page)
gyo_configure_games()
gyo_configure_tools()
gyo_resolve_components(${GYO_REQUESTED_COMPONENTS})
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Do not update populated dependency sources")
add_subdirectory("${GYO_REPOSITORY_ROOT}/third_party" third_party)
# One floating-point contraction mode for GYO code on every platform: never fuse
# a multiply and add into FMA, so x64 Linux, Windows and macOS (and arm64 as a
# by-product) round identically apart from libm. MSVC's default /fp:precise
# already does not contract. Applies to the directories added below, including
# any dependency a product adds from its own directory.
add_compile_options("$<$<COMPILE_LANG_AND_ID:C,AppleClang,Clang,GNU>:-ffp-contract=off>"
                    "$<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang,GNU>:-ffp-contract=off>")
# One preprocessor for GYO code on every platform: MSVC uses its
# standard-conforming preprocessor, as Clang and GCC already do (clang-cl reports
# the Clang ID and needs nothing). GYO's variadic macros rely on it, and
# engine/base/Assert.hpp refuses to compile under the traditional one. Like the
# contraction mode, it applies to the directories added below, including any
# dependency a product adds from its own directory, but not to third_party.
add_compile_options("$<$<COMPILE_LANG_AND_ID:C,MSVC>:/Zc:preprocessor>"
                    "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:preprocessor>")
# The Module and Plugin registers name the engine's Modules (engine/modules/),
# the Plugins and their dependencies (Design Invariant O-6 of
# docs/architecture/target/v2/orchestration.zh-Hant.md); engine/CMakeLists.txt
# adds core, ecs and orchestration directly and the Modules in register order.
# The Plugins (plugins/) are added after every product, below: only those the
# products carry or GYO_PLUGINS names.
gyo_load_unit_registers(GYO_UNITS "${GYO_REPOSITORY_ROOT}/config/modules.csv"
                        "${GYO_REPOSITORY_ROOT}/config/plugins.csv")
add_subdirectory("${GYO_REPOSITORY_ROOT}/engine" engine)
gyo_record_code_page_directory("${CMAKE_CURRENT_BINARY_DIR}/engine")
foreach(GYO_CURRENT_GAME IN LISTS GYO_ACTIVE_GAMES)
    set(GYO_GAME_SOURCE_DIR "${GYO_REPOSITORY_ROOT}/games/${GYO_CURRENT_GAME}")
    set(GYO_GAME_BINARY_DIR "${PROJECT_BINARY_DIR}/games/${GYO_CURRENT_GAME}")
    add_subdirectory("${GYO_GAME_SOURCE_DIR}" "${GYO_GAME_BINARY_DIR}")
    gyo_record_code_page_directory("${GYO_GAME_BINARY_DIR}")
endforeach()
unset(GYO_CURRENT_GAME)
foreach(GYO_CURRENT_TOOL IN LISTS GYO_ACTIVE_TOOLS)
    get_property(GYO_TOOL_VARIANT GLOBAL PROPERTY GYO_TOOL_${GYO_CURRENT_TOOL}_VARIANT)
    add_subdirectory("${GYO_REPOSITORY_ROOT}/tools/${GYO_CURRENT_TOOL}" "tools/${GYO_CURRENT_TOOL}")
    gyo_record_code_page_directory("${CMAKE_CURRENT_BINARY_DIR}/tools/${GYO_CURRENT_TOOL}")
endforeach()
unset(GYO_CURRENT_TOOL)
unset(GYO_TOOL_VARIANT)
if(BUILD_TESTING)
    add_subdirectory("${GYO_REPOSITORY_ROOT}/tests/common" tests/common)
    gyo_record_code_page_directory("${CMAKE_CURRENT_BINARY_DIR}/tests/common")
endif()
if(GYO_BUILD_EXAMPLES)
    add_subdirectory("${GYO_REPOSITORY_ROOT}/examples" examples)
    gyo_record_code_page_directory("${CMAKE_CURRENT_BINARY_DIR}/examples")
endif()
foreach(GYO_CURRENT_GAME IN LISTS GYO_ACTIVE_GAMES)
    set(GYO_GAME_SOURCE_DIR "${GYO_REPOSITORY_ROOT}/games/${GYO_CURRENT_GAME}")
    set(GYO_GAME_BINARY_DIR "${PROJECT_BINARY_DIR}/games/${GYO_CURRENT_GAME}")
    if(BUILD_TESTING AND EXISTS "${GYO_REPOSITORY_ROOT}/tests/${GYO_CURRENT_GAME}/CMakeLists.txt")
        add_subdirectory("${GYO_REPOSITORY_ROOT}/tests/${GYO_CURRENT_GAME}" "tests/${GYO_CURRENT_GAME}")
        gyo_record_code_page_directory("${CMAKE_CURRENT_BINARY_DIR}/tests/${GYO_CURRENT_GAME}")
    endif()
    if((BUILD_TESTING OR GYO_ENABLE_PACKAGING) AND EXISTS "${GYO_REPOSITORY_ROOT}/build/acceptance/${GYO_CURRENT_GAME}/CMakeLists.txt")
        add_subdirectory("${GYO_REPOSITORY_ROOT}/build/acceptance/${GYO_CURRENT_GAME}" "acceptance/${GYO_CURRENT_GAME}")
        gyo_record_code_page_directory("${CMAKE_CURRENT_BINARY_DIR}/acceptance/${GYO_CURRENT_GAME}")
    endif()
endforeach()
unset(GYO_CURRENT_GAME)
foreach(GYO_CURRENT_TOOL IN LISTS GYO_ACTIVE_TOOLS)
    get_property(GYO_TOOL_VARIANT GLOBAL PROPERTY GYO_TOOL_${GYO_CURRENT_TOOL}_VARIANT)
    if(BUILD_TESTING AND EXISTS "${GYO_REPOSITORY_ROOT}/tests/${GYO_CURRENT_TOOL}/CMakeLists.txt")
        add_subdirectory("${GYO_REPOSITORY_ROOT}/tests/${GYO_CURRENT_TOOL}" "tests/${GYO_CURRENT_TOOL}")
        gyo_record_code_page_directory("${CMAKE_CURRENT_BINARY_DIR}/tests/${GYO_CURRENT_TOOL}")
    endif()
    if((BUILD_TESTING OR GYO_ENABLE_PACKAGING) AND EXISTS "${GYO_REPOSITORY_ROOT}/build/acceptance/${GYO_CURRENT_TOOL}/CMakeLists.txt")
        add_subdirectory("${GYO_REPOSITORY_ROOT}/build/acceptance/${GYO_CURRENT_TOOL}" "acceptance/${GYO_CURRENT_TOOL}")
        gyo_record_code_page_directory("${CMAKE_CURRENT_BINARY_DIR}/acceptance/${GYO_CURRENT_TOOL}")
    endif()
endforeach()
unset(GYO_CURRENT_TOOL)
unset(GYO_TOOL_VARIANT)
# The Plugins the configured products carry (gyo_product_units records them)
# and those GYO_PLUGINS names, with the Plugins they depend on, in register
# order. A Plugin nobody selects is not configured, so nothing it fetches is
# downloaded or built (Design Invariant SDL-2 of
# docs/architecture/target/v2/sdl.zh-Hant.md). A product links a Plugin's
# target by name, which CMake resolves once every directory is configured.
get_property(_gyo_requested_plugins GLOBAL PROPERTY GYO_PLUGINS_REQUESTED)
gyo_plugin_selection(GYO_UNITS GYO_SELECTED_PLUGINS ${_gyo_requested_plugins} ${GYO_PLUGINS})
set_property(GLOBAL PROPERTY GYO_PLUGINS_ADDED TRUE)
foreach(_gyo_plugin IN LISTS GYO_SELECTED_PLUGINS)
    add_subdirectory("${GYO_REPOSITORY_ROOT}/plugins/${_gyo_plugin}" "plugins/${_gyo_plugin}")
    gyo_record_code_page_directory("${CMAKE_CURRENT_BINARY_DIR}/plugins/${_gyo_plugin}")
endforeach()
gyo_finalize_product_packages()
set(_gyo_shader_bundles false)
set(_gyo_shader_host_tools false)
get_property(_gyo_registered_shader_bundles GLOBAL PROPERTY GYO_SHADER_BUNDLE_TARGETS)
if(_gyo_registered_shader_bundles)
    set(_gyo_shader_bundles true)
endif()
if(TARGET gyo_shader_host_tools)
    set(_gyo_shader_host_tools true)
endif()
_gyo_registry_json_quote("${CMAKE_BUILD_TYPE}" _gyo_build_configuration)
gyo_go_build_record(_gyo_go_modules _gyo_go_services)
set(_gyo_plugins_json "[]")
foreach(_gyo_plugin IN LISTS GYO_SELECTED_PLUGINS)
    string(JSON _gyo_plugins_json SET "${_gyo_plugins_json}" 9999 "\"${_gyo_plugin}\"")
endforeach()
file(WRITE "${PROJECT_BINARY_DIR}/gyo-build.json" "{\"configuration\":${_gyo_build_configuration},\"plugins\":${_gyo_plugins_json},\"shader_bundles\":${_gyo_shader_bundles},\"shader_host_tools\":${_gyo_shader_host_tools},\"go_modules\":${_gyo_go_modules},\"go_services\":${_gyo_go_services}}\n")
function(gyo_label_tests directory)
    # A standalone tool wrapper adds the repository, which reuses the tool's
    # source directory. Source-based SUBDIRECTORIES lookup can revisit that
    # wrapper; follow each source once along the current ancestry.
    set(ancestors ${ARGN})
    if(directory IN_LIST ancestors)
        return()
    endif()
    list(APPEND ancestors "${directory}")
    get_property(tests DIRECTORY "${directory}" PROPERTY TESTS)
    foreach(test IN LISTS tests)
        get_property(labels TEST "${test}" DIRECTORY "${directory}" PROPERTY LABELS)
        if(NOT labels)
            set_property(TEST "${test}" DIRECTORY "${directory}" PROPERTY LABELS cpu)
        endif()
    endforeach()
    get_property(subdirs DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(subdir IN LISTS subdirs)
        gyo_label_tests("${subdir}" ${ancestors})
    endforeach()
endfunction()
if(BUILD_TESTING)
    gyo_label_tests("${GYO_REPOSITORY_ROOT}")
endif()
