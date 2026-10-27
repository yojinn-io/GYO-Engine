# Design Invariant 11 (docs/architecture/os.md): every executable target under
# engine, apps, tools, build/acceptance and tests carries the UTF-8 code page
# manifest that the build layer applies (build/cmake/GyoCodePage.cmake), or an
# exemption with its reason. The build layer marks the targets on every
# platform, so a target the walk misses fails the configuration on any OS.
# This walk is written apart from the one that applies the manifest, and it
# also starts from the top directory by source path, so a directory the build
# layer forgot to record is seen. Exemptions are listed, not refused.
include_guard(GLOBAL)

function(_gyo_os_check_code_pages directory)
    set(ancestors ${ARGN})
    if(directory IN_LIST ancestors)
        return()
    endif()
    list(APPEND ancestors "${directory}")
    get_property(source DIRECTORY "${directory}" PROPERTY SOURCE_DIR)
    file(RELATIVE_PATH relative "${GYO_REPOSITORY_ROOT}" "${source}")
    if(relative MATCHES "^(engine|apps|tools|build/acceptance|tests)(/|$)")
        get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target IN LISTS targets)
            get_target_property(type ${target} TYPE)
            if(NOT type STREQUAL "EXECUTABLE")
                continue()
            endif()
            get_target_property(page ${target} GYO_PROCESS_CODE_PAGE)
            get_target_property(exempt ${target} GYO_PROCESS_CODE_PAGE_EXEMPT)
            if(exempt)
                set_property(GLOBAL APPEND PROPERTY GYO_OS_CODE_PAGE_EXEMPT "${target}: ${exempt}")
            elseif(NOT page STREQUAL "UTF-8")
                set_property(GLOBAL APPEND PROPERTY GYO_OS_CODE_PAGE_MISSING "${target} (${relative})")
            endif()
        endforeach()
    endif()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        _gyo_os_check_code_pages("${child}" ${ancestors})
    endforeach()
endfunction()

function(gyo_expect_process_code_pages)
    get_property(recorded GLOBAL PROPERTY GYO_CODE_PAGE_DIRECTORIES)
    foreach(directory IN ITEMS "${CMAKE_SOURCE_DIR}" ${recorded})
        _gyo_os_check_code_pages("${directory}")
    endforeach()
    get_property(exempt GLOBAL PROPERTY GYO_OS_CODE_PAGE_EXEMPT)
    list(REMOVE_DUPLICATES exempt)
    foreach(entry IN LISTS exempt)
        message(STATUS "Process code page: runs without the UTF-8 manifest: ${entry}")
    endforeach()
    get_property(missing GLOBAL PROPERTY GYO_OS_CODE_PAGE_MISSING)
    list(REMOVE_DUPLICATES missing)
    if(missing)
        list(JOIN missing ", " listed)
        message(FATAL_ERROR "Process code page guard: ${listed} carry no UTF-8 code page manifest; the build "
                            "layer applies it to every executable (Design Invariant 11, docs/architecture/os.md)")
    endif()
endfunction()
