add_executable(gyo_base_tests
    AssertTests.cpp
    ErrorTests.cpp
    Fnv1aTests.cpp
    IdIssuerTests.cpp
    PreprocessorTests.cpp
    ResultTests.cpp
    TraceTests.cpp
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

# Base is the bottom of the engine and links nothing (v8-arch-gyo2-core-not-registered: core has no register
# row, so its links are written by hand and pinned here). An interface library: its links are its interface.
function(gyo_expect_base_links)
    get_target_property(interface gyo_base INTERFACE_LINK_LIBRARIES)
    if(interface)
        message(FATAL_ERROR "gyo_base links ${interface}; expected nothing")
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_base_links)
