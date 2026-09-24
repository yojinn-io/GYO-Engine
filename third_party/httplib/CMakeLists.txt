include(FetchContent)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_INSTALL OFF CACHE BOOL "" FORCE)

# Some MinGW-w64 headers provide GetAddrInfoExW but omit its cancellation API.
# cpp-httplib's optional async DNS path needs both. Keep its supported blocking
# resolver fallback local to this dependency when that API cannot compile/link.
if(MINGW)
    include(CMakePushCheckState)
    include(CheckCXXSymbolExists)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_DEFINITIONS -D_WIN32_WINNT=0x0A00)
    set(CMAKE_REQUIRED_LIBRARIES ws2_32)
    check_cxx_symbol_exists(GetAddrInfoExCancel "winsock2.h;ws2tcpip.h"
        GYO_HTTPLIB_HAS_GETADDRINFOEXCANCEL)
    cmake_pop_check_state()
    if(NOT GYO_HTTPLIB_HAS_GETADDRINFOEXCANCEL)
        set(HTTPLIB_USE_NON_BLOCKING_GETADDRINFO OFF CACHE BOOL
            "Enable non-blocking DNS when the toolchain supplies its required APIs" FORCE)
        message(STATUS "cpp-httplib: using synchronous DNS; MinGW lacks GetAddrInfoExCancel")
    endif()
endif()

FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.56.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(httplib)
