# Applies GYO's patches (patches/*.patch, in name order) to the fetched SDL
# source. FetchContent may run the patch step again on a tree it already
# patched, so a patch that is already applied is skipped.
# Inputs: SOURCE_DIR, PATCH_DIR, GIT_EXECUTABLE.

file(GLOB patches LIST_DIRECTORIES false "${PATCH_DIR}/*.patch")
list(SORT patches)
foreach(patch IN LISTS patches)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${patch}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE applied
        OUTPUT_QUIET ERROR_QUIET)
    if(applied EQUAL 0)
        continue()
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply "${patch}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SDL patch ${patch} does not apply to ${SOURCE_DIR}:\n${error}")
    endif()
endforeach()
