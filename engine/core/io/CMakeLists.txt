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
add_library(GYO::IO ALIAS gyo_io)

target_include_directories(gyo_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_io PUBLIC cxx_std_20)
# Engine-internal links are written here: core, ecs and orchestration are not
# Modules and are not registered (O-6 covers the Modules and Plugins only).
# Base public; Math (stream helpers use its scalars) and the OS layer (the
# native file system) private.
target_link_libraries(gyo_io PUBLIC GYO::Base PRIVATE GYO::Math GYO::OS)
