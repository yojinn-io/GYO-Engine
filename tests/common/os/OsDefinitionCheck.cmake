# Design Invariant 1 of docs/architecture/os.md: no target outside engine/os gets
# an OS-identifying compile definition from CMake (a name such as GYO_WINDOWS or
# WIN32_LEAN_AND_MEAN, or a $<PLATFORM_ID:...> condition), so no source can
# branch on the OS through one. The definition's value is not read: paths and
# strings may contain any word. Every target and directory under the scanned
# roots is checked, without naming a product; engine/os, tests/common/os and
# third_party may define them. Definitions that name no OS but are set only on
# one OS (NOMINMAX) are not identification and are allowed.
# A module, like ProcessCodePageCheck.cmake, so cmake.ProcessCodePage can run
# it on its synthetic tree (every root, plugins/ included, on any OS).
include_guard(GLOBAL)

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
            message(FATAL_ERROR "OS definition guard: ${owner} ${property} has '${entry}'; only engine/core/os sources "
                                "identify the OS (Design Invariant 1, docs/architecture/os.md)")
        endif()
    endforeach()
endfunction()
function(gyo_os_walk_definitions directory)
    # A standalone tool wrapper adds the repository, which reuses the tool's
    # source directory: follow each directory once along the current ancestry,
    # as the code page walks do.
    set(ancestors ${ARGN})
    if(directory IN_LIST ancestors)
        return()
    endif()
    list(APPEND ancestors "${directory}")
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
        gyo_os_walk_definitions("${child}" ${ancestors})
    endforeach()
endfunction()
function(gyo_expect_no_os_definitions)
    # Set here, not at file scope: the deferred call runs in the top directory's
    # scope, and the helpers above see their caller's variables.
    set(GYO_OS_DEFINITION_ROOTS engine plugins games tools build/acceptance tests)
    # engine/core/os identifies the OS in its sources (the compiler's own macros), never through a build
    # definition, so it is not exempt here; the entry for its old path engine/os was dead (it exempted nothing).
    set(GYO_OS_DEFINITION_EXEMPT tests/common/os)
    set(GYO_OS_IDENTIFYING_NAME
        "(^|[^A-Z0-9])(WIN32|WIN64|WINDOWS|WINNT|WINVER|APPLE|DARWIN|MACOS|MACOSX|OSX|IOS|LINUX|POSIX|UNIX|ANDROID|BSD|FREEBSD|OPENBSD|NETBSD|MINGW|MINGW32|MINGW64|CYGWIN|MSYS|EMSCRIPTEN)([^A-Z]|$)")
    gyo_os_walk_definitions("${CMAKE_SOURCE_DIR}")
endfunction()
