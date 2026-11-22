# gyo_expect_links(<target> <link>...): the configure stops unless <target>'s links, private and interface alike
# (LINK_LIBRARIES and INTERFACE_LINK_LIBRARIES, $<LINK_ONLY:...> unwrapped), are exactly the listed ones. The one
# link-set check of the tests: a test directory that pins a unit's links includes this file and defers the call to
# the end of the configure, so a link added later from another directory is seen too:
#   include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
#   cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_links <target> <link>...)
# Each directory includes it itself, so no directory depends on the order tests/common adds them in
# (v8-arch-gyo2-naming-cleanup; it was defined in tests/common/time/CMakeLists.txt).
include_guard(GLOBAL)

function(gyo_expect_links target)
    get_target_property(own ${target} LINK_LIBRARIES)
    get_target_property(interface ${target} INTERFACE_LINK_LIBRARIES)
    set(actual)
    foreach(entry IN LISTS own interface)
        if(entry MATCHES "^\\$<LINK_ONLY:(.+)>$")
            set(entry "${CMAKE_MATCH_1}")
        endif()
        if(entry AND NOT entry MATCHES "-NOTFOUND$")
            list(APPEND actual "${entry}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES actual)
    list(SORT actual)
    set(expected ${ARGN})
    list(SORT expected)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "${target} links ${actual}; expected exactly ${expected}")
    endif()
endfunction()
