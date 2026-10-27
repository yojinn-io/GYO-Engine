cmake_minimum_required(VERSION 3.30)
# Design Invariant 11 of docs/architecture/os.md on a synthetic tree: the build
# layer (build/cmake/GyoCodePage.cmake) applies the UTF-8 code page manifest to
# every executable under the repository roots, also through a standalone tool
# wrapper that reuses the tool's source directory, and leaves other programs
# alone; the check (tests/common/os/ProcessCodePageCheck.cmake) refuses an
# executable the walk missed and lists an exemption.
get_filename_component(repository "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
string(RANDOM LENGTH 8 ALPHABET abcdef0123456789 run_id)
# WORK_ROOT: where to put the synthetic trees when run outside a build tree
# (the mutation runner starts it from the repository root).
if(NOT DEFINED WORK_ROOT)
    set(WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
endif()
set(work "${WORK_ROOT}/process-code-page-${run_id}")
set(source "${work}/source")
foreach(directory engine/core tools/editor tests/unit vendor/lib)
    file(MAKE_DIRECTORY "${source}/${directory}")
    file(WRITE "${source}/${directory}/main.cpp" "int main() { return 0; }\n")
endforeach()
file(WRITE "${source}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.30)
project(ProcessCodePageFixture LANGUAGES CXX)
set(GYO_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include("${GYO_TEST_REPOSITORY}/build/cmake/GyoCodePage.cmake")
include("${GYO_TEST_REPOSITORY}/tests/common/os/ProcessCodePageCheck.cmake")
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_apply_process_code_page)
set(GYO_CURRENT_TOOL editor)
foreach(directory engine/core tools/editor tests/unit vendor/lib)
    add_subdirectory(${directory})
    if(NOT (TEST_CASE STREQUAL "unrecorded" AND directory STREQUAL "tests/unit"))
        gyo_record_code_page_directory("${CMAKE_CURRENT_BINARY_DIR}/${directory}")
    endif()
endforeach()
if(TEST_CASE STREQUAL "exempt")
    set_property(TARGET unit_tests PROPERTY GYO_PROCESS_CODE_PAGE_EXEMPT "fixture reason")
endif()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_process_code_pages)
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_fixture_report)
# What the deferred calls left: each target's mark and how the manifest went in.
function(gyo_fixture_report)
    set(report "")
    foreach(target IN ITEMS core_tool editor unit_tests vendor_tool)
        get_target_property(mark ${target} GYO_PROCESS_CODE_PAGE)
        if(NOT mark)
            set(mark unmarked)
        endif()
        get_target_property(sources ${target} SOURCES)
        set(embedded none)
        if(sources MATCHES "utf8-codepage\\.manifest")
            set(embedded manifest)
        elseif(sources MATCHES "utf8-codepage\\.rc")
            set(embedded resource)
        endif()
        string(APPEND report "${target}=${mark} ${embedded}\n")
    endforeach()
    file(WRITE "${CMAKE_BINARY_DIR}/report.txt" "${report}")
endfunction()
]=])
file(WRITE "${source}/engine/core/CMakeLists.txt" "add_executable(core_tool main.cpp)\n")
file(WRITE "${source}/tests/unit/CMakeLists.txt" "add_executable(unit_tests main.cpp)\n")
# Outside the roots, as third-party programs are: never given the manifest.
file(WRITE "${source}/vendor/lib/CMakeLists.txt" "add_executable(vendor_tool main.cpp)\n")
# A standalone tool wrapper, as tools/ui_editor has: run as the top directory,
# it adds the repository, which adds this same source directory again.
file(WRITE "${source}/tools/editor/CMakeLists.txt" [=[
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT DEFINED GYO_CURRENT_TOOL)
    cmake_minimum_required(VERSION 3.30)
    project(EditorWrapper LANGUAGES NONE)
    get_filename_component(GYO_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    add_subdirectory("${GYO_REPOSITORY_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/repository")
    return()
endif()
add_executable(editor main.cpp)
]=])

set(configure_args "-DGYO_TEST_REPOSITORY=${repository}")
foreach(pair GENERATOR MAKE_PROGRAM PLATFORM CXX_COMPILER)
    if(${pair})
        if(pair STREQUAL "GENERATOR")
            list(APPEND configure_args -G "${GENERATOR}")
        elseif(pair STREQUAL "PLATFORM")
            list(APPEND configure_args -A "${PLATFORM}")
        elseif(pair STREQUAL "MAKE_PROGRAM")
            list(APPEND configure_args "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
        else()
            list(APPEND configure_args "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
        endif()
    endif()
endforeach()

function(configure label success fragment source_dir)
    set(binary "${work}/${label}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source_dir}" -B "${binary}" ${configure_args} ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 180)
    if(success AND NOT result EQUAL 0)
        message(FATAL_ERROR "${label}: configuration failed (${result})\n${stdout}\n${stderr}")
    elseif(NOT success AND result EQUAL 0)
        message(FATAL_ERROR "${label}: configuration unexpectedly succeeded\n${stdout}")
    endif()
    if(NOT fragment STREQUAL "")
        string(REGEX REPLACE "[ \t\r\n]+" " " text "${stdout} ${stderr}")
        string(FIND "${text}" "${fragment}" position)
        if(position LESS 0)
            message(FATAL_ERROR "${label}: expected '${fragment}'\n${stdout}\n${stderr}")
        endif()
    endif()
endfunction()

# The way a marked executable takes the manifest on this platform.
function(expect_report label report_file)
    file(READ "${report_file}" report)
    foreach(line IN ITEMS ${ARGN})
        string(FIND "${report}" "${line}\n" position)
        if(position LESS 0)
            message(FATAL_ERROR "${label}: expected '${line}' in\n${report}")
        endif()
    endforeach()
endfunction()

configure(plain TRUE "" "${source}")
file(STRINGS "${work}/plain/report.txt" lines)
string(REGEX MATCH "editor=UTF-8 ([a-z]+)" ignored "${lines}")
set(embedded "${CMAKE_MATCH_1}")
if(CMAKE_HOST_WIN32 AND embedded STREQUAL "none")
    message(FATAL_ERROR "plain: a Windows build embedded no manifest\n${lines}")
elseif(NOT CMAKE_HOST_WIN32 AND NOT embedded STREQUAL "none")
    message(FATAL_ERROR "plain: a non-Windows build embedded a manifest\n${lines}")
endif()
expect_report(plain "${work}/plain/report.txt"
    "core_tool=UTF-8 ${embedded}" "editor=UTF-8 ${embedded}" "unit_tests=UTF-8 ${embedded}"
    "vendor_tool=unmarked none")

# The wrapper's top directory has no languages; the tool added again below it
# still gets the manifest, embedded the same way as in the plain build.
configure(wrapper TRUE "" "${source}/tools/editor")
expect_report(wrapper "${work}/wrapper/report.txt"
    "core_tool=UTF-8 ${embedded}" "editor=UTF-8 ${embedded}" "unit_tests=UTF-8 ${embedded}"
    "vendor_tool=unmarked none")

configure(unrecorded FALSE "Process code page guard: unit_tests (tests/unit)" "${source}" -DTEST_CASE=unrecorded)
configure(exempt TRUE "runs without the UTF-8 manifest: unit_tests: fixture reason" "${source}" -DTEST_CASE=exempt)
expect_report(exempt "${work}/exempt/report.txt" "unit_tests=unmarked none" "editor=UTF-8 ${embedded}")
file(REMOVE_RECURSE "${work}")
