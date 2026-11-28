# The operating-system layer: the only Engine code that includes OS headers or
# branches on the OS (docs/architecture/os.md). A leaf directly above
# GYO::Base: it links nothing else, no other Engine module, no SDL and no
# product, so every module above it may use it.
# Sources that do not depend on the OS live in src/common. Everything that does
# lives under src/sys (v8-arch-gyo2-os-src-platform): src/sys/<platform> holds
# each platform's own sources, selected here (the build layer may branch on the
# OS); src/sys/Files.cpp, Process.cpp, Machine.cpp and Console.cpp are the implementation
# macOS and Linux share, which calls the steps that differ through the abstract
# Detail::Platform (src/sys/Platform.hpp) that each platform implements. Windows
# implements the primitives itself; its Platform has only the step of
# src/sys/UserData.cpp, which every platform shares. src/sys/linux is also
# selected for any platform that is neither Windows nor Apple.
add_library(gyo_os STATIC
    src/common/Calendar.cpp
    src/common/SocketAddress.cpp
    src/common/OSError.cpp
    src/common/Paths.cpp
    src/common/Shutdown.cpp
    src/sys/UserData.cpp
)
if(WIN32)
    target_sources(gyo_os PRIVATE
        src/sys/windows/Console.cpp src/sys/windows/DatagramSocket.cpp src/sys/windows/Files.cpp
        src/sys/windows/Machine.cpp src/sys/windows/StreamSocket.cpp
        src/sys/windows/Platform.cpp
        src/sys/windows/Process.cpp src/sys/windows/Thread.cpp src/sys/windows/WakeSignal.cpp
        src/sys/windows/Wide.cpp)
    # CommandLineToArgvW (Console) and SHGetFolderPathW (the user data base,
    # v8-arch-gyo2-os-user-data): a Windows system library, as Design
    # Invariant 3 allows (D49).
    target_link_libraries(gyo_os PRIVATE shell32)
    # Winsock 2 (DatagramSocket, StreamSocket): a Windows system library, as Design Invariant 3
    # allows (net Module declaration D-1, v8-arch-gyo2-net-module).
    target_link_libraries(gyo_os PRIVATE ws2_32)
    # A Win32 name without its W or A suffix resolves to the wide entry point,
    # so the ANSI one is never reached by accident (Design Invariants 5 and 11;
    # R6 sees only names that end in A).
    target_compile_definitions(gyo_os PRIVATE UNICODE _UNICODE)
else()
    target_sources(gyo_os PRIVATE src/sys/Console.cpp src/sys/DatagramSocket.cpp src/sys/Files.cpp src/sys/Machine.cpp
        src/sys/Process.cpp src/sys/StreamSocket.cpp)
    if(APPLE)
        target_sources(gyo_os PRIVATE
            src/sys/macos/Platform.cpp src/sys/macos/Thread.cpp src/sys/macos/WakeSignal.cpp)
    else()
        target_sources(gyo_os PRIVATE
            src/sys/linux/Platform.cpp src/sys/linux/Thread.cpp src/sys/linux/WakeSignal.cpp)
    endif()
endif()
add_library(GYO::OS ALIAS gyo_os)

target_include_directories(gyo_os PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_os PUBLIC cxx_std_20)
# Engine-internal links are written here: core, ecs and orchestration are not
# Modules and are not registered (O-6 covers the Modules and Plugins only).
target_link_libraries(gyo_os PUBLIC GYO::Base)
if(NOT WIN32)
    # pthread_setname_np (Thread) and the wait fallback's mutex and condition
    # variable (src/sys/linux/WakeSignal.cpp): the system's thread library, as
    # Design Invariant 3 allows (D69, v8-arch-gyo2-os-wait).
    find_package(Threads REQUIRED)
    target_link_libraries(gyo_os PRIVATE Threads::Threads)
endif()
