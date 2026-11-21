# The operating-system layer: the only Engine code that includes OS headers or
# branches on the OS (docs/architecture/os.md). A leaf directly above
# GYO::Base: it links nothing else, no other Engine module, no SDL and no
# product, so every module above it may use it.
# Sources that do not depend on the OS live in src/common; one source per OS
# family lives under src/<os> and is selected here (the build layer may branch
# on the OS). The wait primitive has one source per platform: src/macos,
# src/windows, and src/linux, which is also the fallback for any other platform
# (v8-arch-gyo2-os-wait). The thread naming and priority calls moved from
# engine/threads keep one source with per-OS branches under src/thread, so the
# move reads against the old code (os-subsystem open decision 11 (c)).
add_library(gyo_os STATIC
    src/common/Calendar.cpp
    src/common/OsError.cpp
    src/common/Paths.cpp
    src/common/Shutdown.cpp
    src/thread/Thread.cpp
)
if(WIN32)
    target_sources(gyo_os PRIVATE
        src/windows/Console.cpp src/windows/Files.cpp src/windows/Process.cpp src/windows/WakeSignal.cpp
        src/windows/Wide.cpp)
    # CommandLineToArgvW (Console): a Windows system library, as Design
    # Invariant 3 allows (D49).
    target_link_libraries(gyo_os PRIVATE shell32)
    # A Win32 name without its W or A suffix resolves to the wide entry point,
    # so the ANSI one is never reached by accident (Design Invariants 5 and 11;
    # R6 sees only names that end in A).
    target_compile_definitions(gyo_os PRIVATE UNICODE _UNICODE)
else()
    target_sources(gyo_os PRIVATE src/posix/Console.cpp src/posix/Files.cpp src/posix/Process.cpp)
    if(APPLE)
        target_sources(gyo_os PRIVATE src/macos/WakeSignal.cpp)
    else()
        target_sources(gyo_os PRIVATE src/linux/WakeSignal.cpp)
    endif()
endif()
add_library(GYO::Os ALIAS gyo_os)

target_include_directories(gyo_os PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_os PUBLIC cxx_std_20)
# Engine-internal links are written here: core, ecs and orchestration are not
# Modules and are not registered (O-6 covers the Modules and Plugins only).
target_link_libraries(gyo_os PUBLIC GYO::Base)
if(NOT WIN32)
    # pthread_setname_np (Thread) and the wait fallback's mutex and condition
    # variable (src/linux/WakeSignal.cpp): the system's thread library, as
    # Design Invariant 3 allows (D69, v8-arch-gyo2-os-wait).
    find_package(Threads REQUIRED)
    target_link_libraries(gyo_os PRIVATE Threads::Threads)
endif()
