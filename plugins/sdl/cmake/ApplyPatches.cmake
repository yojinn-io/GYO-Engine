# Applies GYO's patches (patches/*.patch, in name order) to the fetched SDL
# source. FetchContent may run the patch step again on a tree it already
# patched, so a patch that is already applied is skipped.
# Inputs: SOURCE_DIR, PATCH_DIR, GIT_EXECUTABLE.
#
# The source is an unpacked release archive, not a clone, and it lies inside
# the build tree, which may itself be inside a git work tree (build/target is
# ignored, not outside the repository). git apply in a repository takes the
# patch's paths from the repository's top, so repository discovery stops at
# the source directory: git apply then works as a plain patch program there.

get_filename_component(ceiling "${SOURCE_DIR}" DIRECTORY)
file(GLOB patches LIST_DIRECTORIES false "${PATCH_DIR}/*.patch")
list(SORT patches)
foreach(patch IN LISTS patches)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${ceiling}"
                "${GIT_EXECUTABLE}" apply --reverse --check "${patch}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE applied
        OUTPUT_QUIET ERROR_QUIET)
    if(applied EQUAL 0)
        continue()
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${ceiling}"
                "${GIT_EXECUTABLE}" apply "${patch}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "SDL patch ${patch} does not apply to ${SOURCE_DIR}:\n${error}")
    endif()
endforeach()
