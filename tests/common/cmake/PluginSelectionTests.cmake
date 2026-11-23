cmake_minimum_required(VERSION 3.30)
# The build layer adds only the Plugins that are selected (build/cmake/GyoBuild.cmake;
# declaration docs/architecture/target/v2/sdl-declaration.zh-Hant.md, section 9.1):
# the repository configured with no product and GYO_PLUGINS empty adds no Plugin
# directory, though config/plugins.csv has rows.
# Configure only. The sources the parent build fetched are reused and the child
# is disconnected, so the check needs no network; a Plugin the child added
# would have to download, and fails the configure instead.
# The selection itself (closure, ALL, refusals) is cmake.UnitRegistry; the build
# with the SDL Plugins removed is CI's core step (Design Invariant SDL-2).
get_filename_component(repository "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT DEFINED PARENT_BINARY)
    message(FATAL_ERROR "PluginSelectionTests.cmake needs -DPARENT_BINARY=<a configured build of the repository>")
endif()
if(NOT DEFINED WORK_ROOT)
    set(WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
endif()
string(RANDOM LENGTH 8 ALPHABET abcdef0123456789 run_id)
set(binary "${WORK_ROOT}/plugin-selection-${run_id}")

set(arguments "")
if(DEFINED GENERATOR AND NOT GENERATOR STREQUAL "")
    list(APPEND arguments -G "${GENERATOR}")
endif()
if(DEFINED MAKE_PROGRAM AND NOT MAKE_PROGRAM STREQUAL "")
    list(APPEND arguments "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
if(DEFINED PLATFORM AND NOT PLATFORM STREQUAL "")
    list(APPEND arguments -A "${PLATFORM}")
endif()
if(DEFINED C_COMPILER AND NOT C_COMPILER STREQUAL "")
    list(APPEND arguments "-DCMAKE_C_COMPILER=${C_COMPILER}")
endif()
if(DEFINED CXX_COMPILER AND NOT CXX_COMPILER STREQUAL "")
    list(APPEND arguments "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()
file(GLOB sources LIST_DIRECTORIES true "${PARENT_BINARY}/_deps/*-src")
foreach(source IN LISTS sources)
    get_filename_component(name "${source}" NAME)
    string(REGEX REPLACE "-src$" "" name "${name}")
    string(TOUPPER "${name}" name)
    list(APPEND arguments "-DFETCHCONTENT_SOURCE_DIR_${name}=${source}")
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" -S "${repository}" -B "${binary}" ${arguments}
        -DBUILD_TESTING=OFF -DGYO_BUILD_EXAMPLES=OFF -DGYO_ENABLE_PACKAGING=OFF
        "-DGYO_GAMES=" "-DGYO_TOOLS=" "-DGYO_PLUGINS="
        -DFETCHCONTENT_FULLY_DISCONNECTED=ON
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 300)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "The repository did not configure with no Plugin selected (${result}):\n${output}\n${error}")
endif()
file(READ "${binary}/gyo-build.json" record)
string(JSON plugins GET "${record}" plugins)
string(JSON count LENGTH "${plugins}")
if(NOT count EQUAL 0)
    message(FATAL_ERROR "Plugins added with none selected: ${plugins}")
endif()
# What a Plugin fetches is configured from its directory, so no Plugin
# directory means nothing of it fetched. This file names no Plugin: removing
# one (CI's core step, SDL-2) leaves it as it is.
file(GLOB added LIST_DIRECTORIES true "${binary}/plugins/*")
if(added)
    message(FATAL_ERROR "A Plugin directory was configured with no Plugin selected: ${added}")
endif()
file(REMOVE_RECURSE "${binary}")
message(STATUS "Plugin selection tests passed")
