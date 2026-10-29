# Windows processes run with UTF-8 as their ANSI code page, whatever the system
# setting (Design Invariant 11 of docs/architecture/os.md; os-utf8-codepage,
# D64). engine/os owns the manifest and how it is embedded in one executable.
# Which executables get it is the build layer's (build/cmake/GyoCodePage.cmake);
# a separately configured project, such as the host shader tool, calls
# gyo_os_process_code_page itself. The build layer may branch on the OS (Design
# Invariant 1).
include_guard(GLOBAL)

# How this build embeds the manifest, decided here, in the scope that enabled
# the compilers: the build layer applies it from a deferred call, which may run
# in a scope without them (a standalone tool wrapper's top directory has no
# languages). MSVC, clang-cl and Clang with the MSVC ABI merge a .manifest
# source into the manifest the linker embeds; MinGW embeds a resource.
if(WIN32)
    if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        set_property(GLOBAL PROPERTY GYO_OS_PROCESS_CODE_PAGE_EMBED manifest)
    else()
        enable_language(RC)
        set_property(GLOBAL PROPERTY GYO_OS_PROCESS_CODE_PAGE_EMBED resource)
    endif()
endif()

# Embeds the manifest in one executable and marks it. The mark is set on every
# platform, so the configure-time check in tests/common/os runs on every OS.
function(gyo_os_process_code_page target)
    set_property(TARGET ${target} PROPERTY GYO_PROCESS_CODE_PAGE UTF-8)
    get_property(embed GLOBAL PROPERTY GYO_OS_PROCESS_CODE_PAGE_EMBED)
    if(NOT embed)
        return()
    endif()
    set(manifest "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utf8-codepage.manifest")
    if(embed STREQUAL "manifest")
        target_sources(${target} PRIVATE "${manifest}")
    else()
        # The manifest a process takes at start is the RT_MANIFEST (24)
        # resource 1. Its language is explicit: MinGW-w64 links a default
        # manifest of language 0, which binutils drops when another one has a
        # language of its own (to be verified on a MinGW machine, os-subsystem
        # open decision 23).
        set(resource "${CMAKE_BINARY_DIR}/gyo_os/utf8-codepage.rc")
        file(CONFIGURE OUTPUT "${resource}" CONTENT "LANGUAGE 0x09, 0x01\n1 24 \"${manifest}\"\n")
        target_sources(${target} PRIVATE "${resource}")
        # The resource compiler records no dependency on the file it embeds.
        set_source_files_properties("${resource}" TARGET_DIRECTORY ${target} PROPERTIES OBJECT_DEPENDS "${manifest}")
    endif()
endfunction()
