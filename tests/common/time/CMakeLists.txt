# GYO::Os: the backend test compares the Waiter with the engine/os mechanism.
# Threads::Threads: the tests start threads of their own.
find_package(Threads REQUIRED)
add_executable(gyo_time_tests WaiterTests.cpp)
target_link_libraries(gyo_time_tests PRIVATE GYO::Time GYO::Os Threads::Threads gyo_test_support
    doctest::doctest)
add_test(NAME gyo_time_tests COMMAND gyo_time_tests)
set_tests_properties(gyo_time_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# TT-1 L2 (record only, run by hand under a declaration): wake precision of the
# engine Waiter against condition_variable and sleep_until, in a process without
# a window. Deliberately not a CTest.
add_executable(gyo_time_wake_record WakeRecord.cpp)
target_link_libraries(gyo_time_wake_record PRIVATE GYO::Time Threads::Threads)

# The time base and role threads stay leaves above GYO::Base: their links,
# private and interface alike, are exactly these, so no SDL or engine library
# edge appears beyond engine/os, which they reach privately. Checked once the whole project is configured, so an edge added
# later from another directory is seen too.
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
function(gyo_expect_time_and_thread_links)
    # The time base reaches the platform wait mechanism through engine/os
    # (v8-arch-gyo2-os-wait), privately: what links GYO::Time gets no engine/os
    # headers from it (time.md Invariant 10).
    gyo_expect_links(gyo_time GYO::Base GYO::Os)
    get_target_property(time_interface gyo_time INTERFACE_LINK_LIBRARIES)
    if("GYO::Os" IN_LIST time_interface)
        message(FATAL_ERROR "gyo_time exposes GYO::Os to its users; link it PRIVATE")
    endif()
    # The role threads name and prioritize themselves through engine/os
    # (os-threads-backend, D69), privately: what links GYO::Threads gets no
    # engine/os headers from it.
    gyo_expect_links(gyo_threads GYO::Time Threads::Threads GYO::Os)
    get_target_property(interface gyo_threads INTERFACE_LINK_LIBRARIES)
    if("GYO::Os" IN_LIST interface)
        message(FATAL_ERROR "gyo_threads exposes GYO::Os to its users; link it PRIVATE")
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_time_and_thread_links)
