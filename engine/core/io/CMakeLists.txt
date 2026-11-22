# Engine file I/O: the file backend interface and its native backend, file URIs and paths, the stream a backend
# opens, and whole-file helpers (docs/architecture/io.md). Above GYO::Base; the OS layer is private: no
# engine/io public header names an engine/os type.
add_library(gyo_io STATIC
    # fs
    src/fs/NativeFileSystem.cpp
    # helpers
    src/helpers/UserFiles.cpp
    src/helpers/WriteFileAtomically.cpp
    # path
    src/path/PathUtils.cpp
    src/path/Uri.cpp
)
add_library(GYO::IO ALIAS gyo_io)

target_include_directories(gyo_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_io PUBLIC cxx_std_20)
# Engine-internal links are written here: core, ecs and orchestration are not
# Modules and are not registered (O-6 covers the Modules and Plugins only).
# Base public; the OS layer (the native file system) private.
target_link_libraries(gyo_io PUBLIC GYO::Base PRIVATE GYO::OS)
