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
