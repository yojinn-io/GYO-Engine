add_executable(gyo_base_tests
    AssertTests.cpp
    ErrorTests.cpp
    Fnv1aTests.cpp
    IdIssuerTests.cpp
    PreprocessorTests.cpp
    ResultTests.cpp
)
# IdIssuerTests starts threads (the base library itself links nothing).
find_package(Threads REQUIRED)
target_link_libraries(gyo_base_tests PRIVATE GYO::Base gyo_test_support doctest::doctest Threads::Threads)
# Same warning level as Math so header code that would warn in a consumer
# fails here first.
if(MSVC)
    target_compile_options(gyo_base_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_base_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME gyo_base_tests COMMAND gyo_base_tests)

# The default handler must abort the process. A failing child process cannot
# be observed from doctest, so a probe executable runs under cmake -P and the
# script checks the exit status and stderr.
add_executable(gyo_assert_abort_probe AssertAbortProbe.cpp)
target_link_libraries(gyo_assert_abort_probe PRIVATE GYO::Base)
foreach(mode IN ITEMS default returning nested)
    add_test(NAME gyo_base.assert_abort.${mode}
        COMMAND ${CMAKE_COMMAND}
            -DPROBE=$<TARGET_FILE:gyo_assert_abort_probe>
            -DMODE=${mode}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/AssertAbortProbe.cmake)
    set_tests_properties(gyo_base.assert_abort.${mode} PROPERTIES TIMEOUT 30)
endforeach()

# Error levels D5 (docs/architecture/target/v2/error-levels-declaration.zh-Hant.md):
# discarding a [[nodiscard]] value unread is a compile error in GYO C++ code.
# Two checks, both at configure time (NO_CACHE, so every configure):
#   1. the options of build/cmake/GyoBuild.cmake reach this directory (a
#      directory's COMPILE_OPTIONS start from its parent's), so removing the
#      add_compile_options line is caught even though try_compile does not
#      inherit it;
#   2. one source built per variant with those options: the control compiles,
#      each discard variant fails and its output names nodiscard (the expected
#      failure; any other failure is not counted, AGENTS 14.2).
# Any mismatch stops the configure.
function(gyo_base_discard_is_error_samples)
    if(NOT GYO_DISCARD_IS_ERROR_OPTIONS)
        message(FATAL_ERROR "base.discard_is_error: GYO_DISCARD_IS_ERROR_OPTIONS is empty for "
            "${CMAKE_CXX_COMPILER_ID}; build/cmake/GyoBuild.cmake names no option for this compiler")
    endif()
    get_property(directory_options DIRECTORY PROPERTY COMPILE_OPTIONS)
    foreach(option IN LISTS GYO_DISCARD_IS_ERROR_OPTIONS)
        string(FIND "${directory_options}" "${option}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "base.discard_is_error: ${option} is not among the directory's compile "
                "options; build/cmake/GyoBuild.cmake no longer adds it")
        endif()
    endforeach()
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/DiscardSample.cpp")
    set(includes "${GYO_REPOSITORY_ROOT}/engine/base/include")
    set(flags ${GYO_DISCARD_IS_ERROR_OPTIONS})
    if(MSVC)
        list(APPEND flags /Zc:preprocessor /permissive-)
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}"
        "${GYO_REPOSITORY_ROOT}/engine/base/include/engine/base/Result.hpp")
    try_compile(accepted SOURCES "${source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
        COMPILE_DEFINITIONS -DGYO_DISCARD_SAMPLE_ACCEPTED ${flags}
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
        OUTPUT_VARIABLE accepted_output
        NO_CACHE)
    if(NOT accepted)
        message(FATAL_ERROR "base.discard_is_error: the control did not compile, so a sample "
            "cannot tell its own failure from a broken setup:\n${accepted_output}")
    endif()
    set(summary "control compiled")
    foreach(name IN ITEMS RESULT FUNCTION)
        try_compile(rejected SOURCES "${source}"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
            COMPILE_DEFINITIONS -DGYO_DISCARD_SAMPLE_${name} ${flags}
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
            OUTPUT_VARIABLE rejected_output
            NO_CACHE)
        if(rejected)
            message(FATAL_ERROR "base.discard_is_error: variant ${name} compiled; a discarded "
                "[[nodiscard]] value is not an error")
        endif()
        string(FIND "${rejected_output}" "nodiscard" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "base.discard_is_error: variant ${name} failed without naming nodiscard, "
                "so the failure is not the expected one:\n${rejected_output}")
        endif()
        string(APPEND summary "; ${name} rejected naming nodiscard")
    endforeach()
    message(STATUS "base.discard_is_error (try_compile, ${GYO_DISCARD_IS_ERROR_OPTIONS}): ${summary}")
endfunction()
gyo_base_discard_is_error_samples()
