# Engine file I/O: virtual and native file systems, mount table, paths, URIs,
# streams and whole-file helpers (docs/architecture/io.md). Above GYO::Base;
# Math and the OS layer are private: no engine/io public header names an
# engine/math or engine/os type.
add_library(gyo_io STATIC
    # fs
    src/fs/DirectoryIterator.cpp
    src/fs/MountTable.cpp
    src/fs/NativeFileSystem.cpp
    src/fs/Vfs.cpp
    # helpers
    src/helpers/ReadAllBytes.cpp
    src/helpers/ReadAllText.cpp
    src/helpers/WriteAllBytes.cpp
    src/helpers/WriteAllText.cpp
    src/helpers/UserFiles.cpp
    src/helpers/WriteFileAtomically.cpp
    # path
    src/path/PathUtils.cpp
    src/path/Uri.cpp
    # stream
    src/stream/MemoryStream.cpp
    src/stream/SpanStream.cpp
    src/stream/BufferedStream.cpp
    src/stream/StreamReader.cpp
    src/stream/StreamWriter.cpp
)
add_library(GYO::Io ALIAS gyo_io)

target_include_directories(gyo_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_io PUBLIC cxx_std_20)
target_link_libraries(gyo_io
    PUBLIC GYO::Base
    # The native file system calls the OS layer; stream helpers use Math scalars.
    PRIVATE GYO::Math GYO::Os
)
