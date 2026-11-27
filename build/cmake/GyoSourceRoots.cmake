# Reads GyoSourceRoots.txt, the top-level directories of the repository's own
# code. Lines are roots; text after '#' and blank lines are ignored.
include_guard(GLOBAL)

function(gyo_source_roots output)
    set(list_file "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GyoSourceRoots.txt")
    if(NOT CMAKE_SCRIPT_MODE_FILE)
        # Editing the list reconfigures the project.
        set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${list_file}")
    endif()
    file(STRINGS "${list_file}" lines)
    set(roots "")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "#.*$" "" line "${line}")
        string(STRIP "${line}" line)
        if(NOT line STREQUAL "")
            list(APPEND roots "${line}")
        endif()
    endforeach()
    set(${output} "${roots}" PARENT_SCOPE)
endfunction()
