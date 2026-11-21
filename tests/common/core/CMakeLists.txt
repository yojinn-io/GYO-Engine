add_executable(engine_tests
    test_main.cpp
    io/BufferedStreamTests.cpp
    io/NativeFileSystemTests.cpp
    io/StreamReaderTests.cpp
    io/UriTests.cpp
    io/UserFilesTests.cpp
    io/VfsTests.cpp
)

target_link_libraries(engine_tests PRIVATE
    GYO::Io
    # io/UserFilesTests.cpp checks engine/io against engine/os's own path rules.
    GYO::Os
    gyo_test_support
    doctest::doctest
)

add_test(NAME engine_tests COMMAND engine_tests)

# Windows UNC paths through engine/io (io-native-paths, D54): a temporary tree
# named through the drive's administrative share, at a short and a 300+ character
# path. A machine where the share is not reachable reports Skipped (exit 77).
if(WIN32)
    add_executable(gyo_io_unc_probe io/UncProbe.cpp)
    target_link_libraries(gyo_io_unc_probe PRIVATE GYO::Io)
    foreach(host localhost 127.0.0.1)
        add_test(NAME io.unc_${host} COMMAND gyo_io_unc_probe ${host})
        set_tests_properties(io.unc_${host} PROPERTIES LABELS "cpu" TIMEOUT 60 SKIP_RETURN_CODE 77)
    endforeach()
endif()

# io links Base publicly and Math and the OS layer privately (docs/architecture/io.md;
# v8-arch-gyo2-core-not-registered: core's links are written by hand and pinned here).
function(gyo_expect_io_links)
    gyo_expect_links(gyo_io GYO::Base GYO::Math GYO::Os)
    get_target_property(interface gyo_io INTERFACE_LINK_LIBRARIES)
    foreach(private IN ITEMS GYO::Math GYO::Os)
        if(private IN_LIST interface)
            message(FATAL_ERROR "gyo_io exposes ${private} to its users; link it PRIVATE")
        endif()
    endforeach()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_io_links)
