# Design Invariant 11 of docs/architecture/os.md: every Windows executable of
# the repository's own sources declares UTF-8 as its ANSI code page. engine/os
# owns the manifest and its embedding; the build layer decides which targets
# get it: every executable under engine, apps, tools, build/acceptance and
# tests, without naming a product (AGENTS.md sections 7 and 9). Third-party
# programs that FetchContent adds (protoc, for one) live outside those roots
# and keep their own manifests. tests/common/os checks the result.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/../../engine/os/cmake/GyoProcessCodePage.cmake")

# Records a directory the build layer adds, by its binary directory. A
# standalone tool wrapper reuses the tool's source directory, so looking the
# tool up by its source path finds the wrapper's top directory instead, which
# has no targets; its binary directory is unique.
function(gyo_record_code_page_directory binary_directory)
    set_property(GLOBAL APPEND PROPERTY GYO_CODE_PAGE_DIRECTORIES "${binary_directory}")
endfunction()

function(_gyo_code_page_walk directory)
    set(ancestors ${ARGN})
    if(directory IN_LIST ancestors)
        return()
    endif()
    list(APPEND ancestors "${directory}")
    get_property(source DIRECTORY "${directory}" PROPERTY SOURCE_DIR)
    file(RELATIVE_PATH relative "${GYO_REPOSITORY_ROOT}" "${source}")
    foreach(root IN ITEMS engine apps tools build/acceptance tests)
        if(relative STREQUAL root OR relative MATCHES "^${root}/")
            get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
            foreach(target IN LISTS targets)
                get_target_property(type ${target} TYPE)
                get_target_property(marked ${target} GYO_PROCESS_CODE_PAGE)
                get_target_property(exempt ${target} GYO_PROCESS_CODE_PAGE_EXEMPT)
                if(type STREQUAL "EXECUTABLE" AND NOT marked AND NOT exempt)
                    gyo_os_process_code_page(${target})
                endif()
            endforeach()
            break()
        endif()
    endforeach()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        _gyo_code_page_walk("${child}" ${ancestors})
    endforeach()
endfunction()

# Applies the manifest to every executable under the recorded directories and
# their subdirectories. Deferred to the end of the top directory, once every
# target exists. A target that must run without it sets
# GYO_PROCESS_CODE_PAGE_EXEMPT to the reason, which the check lists.
function(gyo_apply_process_code_page)
    get_property(directories GLOBAL PROPERTY GYO_CODE_PAGE_DIRECTORIES)
    foreach(directory IN LISTS directories)
        _gyo_code_page_walk("${directory}")
    endforeach()
endfunction()
