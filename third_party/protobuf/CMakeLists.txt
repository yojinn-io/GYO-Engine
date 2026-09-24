include(FetchContent)
# Compiler and C++ runtime deliberately come from the same pinned release.
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
set(protobuf_FORCE_FETCH_DEPENDENCIES ON CACHE BOOL "" FORCE)
FetchContent_Declare(protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG v36.2 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(protobuf)
