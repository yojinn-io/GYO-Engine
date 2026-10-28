# Tests of the OS layer (docs/architecture/os.md). This is the one common test
# directory allowed to assert per OS; the boundary guard does not scan it.
add_executable(gyo_os_tests OsErrorTests.cpp FilesTests.cpp FileWriterTests.cpp FileChangeTests.cpp
    ProcessTests.cpp ConsoleTests.cpp ThreadTests.cpp)
# GYO::Threads: ThreadTests reads a role's priority back on the role's thread.
target_link_libraries(gyo_os_tests PRIVATE GYO::Os GYO::Threads gyo_test_support doctest::doctest)
add_test(NAME gyo_os_tests COMMAND gyo_os_tests)
set_tests_properties(gyo_os_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# Design Invariant 4 (Windows side): the public headers reach no Windows header
# and define no min or max. A separate executable, since the check must run
# before doctest's own windows.h.
add_executable(gyo_os_header_tests OsHeaderHygieneTests.cpp)
target_link_libraries(gyo_os_header_tests PRIVATE GYO::Os doctest::doctest)
target_compile_definitions(gyo_os_header_tests PRIVATE
    GYO_OS_INCLUDE_DIR="${GYO_REPOSITORY_ROOT}/engine/os/include"
    GYO_OS_HYGIENE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/OsHeaderHygieneTests.cpp")
add_test(NAME os.header_hygiene COMMAND gyo_os_header_tests)
set_tests_properties(os.header_hygiene PROPERTIES LABELS "cpu" TIMEOUT 60)

# The boundary guard scans the sources as text, so it gives the same result on
# every platform; its self-test runs it on synthetic trees.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME os.boundary_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/os_boundary_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME os.boundary_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*.py" -v)
set_tests_properties(os.boundary_guard os.boundary_guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)

# Console (command line, shutdown requests) needs a child process: the probe
# is started by console_check.py, which exits 77 for a check this runner cannot
# run (CTest shows it as skipped, so a gap stays visible).
add_executable(gyo_os_console_probe ConsoleProbe.cpp)
target_link_libraries(gyo_os_console_probe PRIVATE GYO::Os)
set(gyo_os_console_checks arguments shutdown)
if(WIN32)
    list(APPEND gyo_os_console_checks ctrl-c close)
endif()
foreach(check IN LISTS gyo_os_console_checks)
    string(REPLACE "-" "_" name "${check}")
    add_test(NAME os.console_${name}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/console_check.py"
            --probe "$<TARGET_FILE:gyo_os_console_probe>" --check ${check})
    set_tests_properties(os.console_${name} PROPERTIES LABELS "cpu" TIMEOUT 90 SKIP_RETURN_CODE 77)
endforeach()

# The OS layer is the lowest layer above GYO::Base: its links, private and
# interface alike, are exactly GYO::Base and OS system libraries (Design
# Invariant 3): on Windows shell32 for CommandLineToArgvW (D49), elsewhere the
# thread library for pthread_setname_np (D69).
# gyo_expect_links is defined with the time tests, which configure first.
# Checked once the whole project is configured, so an edge added later from
# another directory is seen too.
function(gyo_expect_os_links)
    if(WIN32)
        gyo_expect_links(gyo_os GYO::Base shell32)
    else()
        gyo_expect_links(gyo_os GYO::Base Threads::Threads)
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_os_links)

# Design Invariant 1: no target outside engine/os gets an OS-identifying compile
# definition from CMake (a name such as GYO_WINDOWS or WIN32_LEAN_AND_MEAN, or a
# $<PLATFORM_ID:...> condition), so no source can branch on the OS through one.
# The definition's value is not read: paths and strings may contain any word.
# Every target and directory under the scanned roots is checked, without naming
# a product; engine/os, this directory and third_party may define them.
# Definitions that name no OS but are set only on one OS (NOMINMAX) are not
# identification and are allowed.
function(gyo_os_definition_scope directory out relative_out)
    file(RELATIVE_PATH relative "${GYO_REPOSITORY_ROOT}" "${directory}")
    set(${relative_out} "${relative}" PARENT_SCOPE)
    set(in_scope FALSE)
    foreach(root IN LISTS GYO_OS_DEFINITION_ROOTS)
        if(relative STREQUAL root OR relative MATCHES "^${root}/")
            set(in_scope TRUE)
        endif()
    endforeach()
    foreach(exempt IN LISTS GYO_OS_DEFINITION_EXEMPT)
        if(relative STREQUAL exempt OR relative MATCHES "^${exempt}/")
            set(in_scope FALSE)
        endif()
    endforeach()
    set(${out} ${in_scope} PARENT_SCOPE)
endfunction()
function(gyo_os_check_definitions owner property entries)
    foreach(entry IN LISTS entries)
        set(name "${entry}")
        if(property MATCHES "OPTIONS$")
            # Only -D and /D options define macros.
            if(NOT entry MATCHES "^[-/]D(.+)$")
                continue()
            endif()
            set(name "${CMAKE_MATCH_1}")
        endif()
        string(REGEX REPLACE "=.*$" "" name "${name}")
        string(TOUPPER "${name}" upper)
        if(upper MATCHES "${GYO_OS_IDENTIFYING_NAME}" OR entry MATCHES "PLATFORM_ID")
            message(FATAL_ERROR "OS definition guard: ${owner} ${property} has '${entry}'; only engine/os may "
                                "identify the OS (Design Invariant 1, docs/architecture/os.md)")
        endif()
    endforeach()
endfunction()
function(gyo_os_walk_definitions directory)
    gyo_os_definition_scope("${directory}" in_scope relative)
    if(in_scope)
        get_property(definitions DIRECTORY "${directory}" PROPERTY COMPILE_DEFINITIONS)
        gyo_os_check_definitions("directory ${relative}" COMPILE_DEFINITIONS "${definitions}")
        get_property(options DIRECTORY "${directory}" PROPERTY COMPILE_OPTIONS)
        gyo_os_check_definitions("directory ${relative}" COMPILE_OPTIONS "${options}")
        get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target IN LISTS targets)
            foreach(property IN ITEMS COMPILE_DEFINITIONS INTERFACE_COMPILE_DEFINITIONS
                                      COMPILE_OPTIONS INTERFACE_COMPILE_OPTIONS)
                get_target_property(values ${target} ${property})
                if(values)
                    gyo_os_check_definitions("${target}" ${property} "${values}")
                endif()
            endforeach()
        endforeach()
    endif()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        gyo_os_walk_definitions("${child}")
    endforeach()
endfunction()
function(gyo_expect_no_os_definitions)
    # Set here, not at file scope: the deferred call runs in the top directory's
    # scope, and the helpers above see their caller's variables.
    set(GYO_OS_DEFINITION_ROOTS engine apps tools build/acceptance tests)
    set(GYO_OS_DEFINITION_EXEMPT engine/os tests/common/os)
    set(GYO_OS_IDENTIFYING_NAME
        "(^|[^A-Z0-9])(WIN32|WIN64|WINDOWS|WINNT|WINVER|APPLE|DARWIN|MACOS|MACOSX|OSX|IOS|LINUX|POSIX|UNIX|ANDROID|BSD|FREEBSD|OPENBSD|NETBSD|MINGW|MINGW32|MINGW64|CYGWIN|MSYS|EMSCRIPTEN)([^A-Z]|$)")
    gyo_os_walk_definitions("${CMAKE_SOURCE_DIR}")
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_no_os_definitions)

# Design Invariant 11: every executable target under the scanned roots carries
# the UTF-8 code page manifest, checked once the project is configured (the
# check is a module so cmake.ProcessCodePage can run it on synthetic trees).
include("${CMAKE_CURRENT_SOURCE_DIR}/ProcessCodePageCheck.cmake")
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_process_code_pages)
