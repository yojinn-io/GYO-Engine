# A program on the entry point, run by EntryTests.cpp.
add_executable(platform_sdl_entry_probe EntryProbe.cpp)
target_link_libraries(platform_sdl_entry_probe PRIVATE GYO::PlatformSDLMain GYO::PlatformSDLNative)
target_compile_features(platform_sdl_entry_probe PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(platform_sdl_entry_probe)

add_executable(platform_sdl_tests SdlPlatformTests.cpp EventTimeTests.cpp LogBridgeTests.cpp EntryTests.cpp WindowTests.cpp
    EventWatchTests.cpp ConcurrencyWatchdog.cpp)
add_dependencies(platform_sdl_tests platform_sdl_entry_probe)
target_compile_definitions(platform_sdl_tests PRIVATE GYO_ENTRY_PROBE="$<TARGET_FILE:platform_sdl_entry_probe>")
target_link_libraries(platform_sdl_tests PRIVATE
    GYO::PlatformSDLNative
    gyo_test_support
    doctest::doctest
)
target_compile_features(platform_sdl_tests PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(platform_sdl_tests)
add_test(NAME platform_sdl_tests COMMAND platform_sdl_tests "-tce=*concurrent*,*two threads*,*waits for a callback*")
set_tests_properties(platform_sdl_tests PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    TIMEOUT 15
)
# Concurrency tests declared with 60 s: the log bridge (ip3-declaration.md) and
# event watch destruction (ip4-declaration.md).
add_test(NAME platform_sdl_concurrency COMMAND platform_sdl_tests "-tc=*concurrent*,*two threads*,*waits for a callback*")
set_tests_properties(platform_sdl_concurrency PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    TIMEOUT 60
)

# G1 and G2 of docs/architecture/platform-sdl.md (Design Invariant 1).
add_executable(platform_sdl_header_tests HeaderHygieneTests.cpp)
target_link_libraries(platform_sdl_header_tests PRIVATE GYO::PlatformSDL doctest::doctest)
# SDL's include path only: since the backends link SDL PRIVATE (ip4c), a public
# header that includes SDL would otherwise stop the compiler before G1 and G2
# could report it.
target_link_libraries(platform_sdl_header_tests PRIVATE SDL3::Headers)
if(TARGET GYO::InputBackendSDL)
    target_link_libraries(platform_sdl_header_tests PRIVATE GYO::InputBackendSDL)
    target_compile_definitions(platform_sdl_header_tests PRIVATE GYO_HYGIENE_SDL_INPUT)
endif()
if(TARGET GYO::AudioBackendSDL)
    target_link_libraries(platform_sdl_header_tests PRIVATE GYO::AudioBackendSDL)
    target_compile_definitions(platform_sdl_header_tests PRIVATE GYO_HYGIENE_SDL_AUDIO)
endif()
if(TARGET GYO::RenderBackendSDL)
    target_link_libraries(platform_sdl_header_tests PRIVATE GYO::RenderBackendSDL)
    target_compile_definitions(platform_sdl_header_tests PRIVATE GYO_HYGIENE_SDL_RENDERER)
endif()
if(TARGET GYO::RenderBackendSDLGPU)
    target_link_libraries(platform_sdl_header_tests PRIVATE GYO::RenderBackendSDLGPU)
    target_compile_definitions(platform_sdl_header_tests PRIVATE GYO_HYGIENE_SDL_GPU)
endif()
target_compile_definitions(platform_sdl_header_tests PRIVATE GYO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_features(platform_sdl_header_tests PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(platform_sdl_header_tests)
add_test(NAME platform_sdl_header_tests COMMAND platform_sdl_header_tests)
set_tests_properties(platform_sdl_header_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# G3 of docs/architecture/platform-sdl.md (Design Invariant 2): the SDL backends
# and the program entry pass SDL on only through the *Native targets. In their
# interface, SDL3 and the *Native targets appear only as $<LINK_ONLY:> (a
# PRIVATE link of a static or object library), and no native/include directory
# is public. Checked once the whole project is configured, so a link added later
# from another directory is seen too.
function(gyo_expect_private_sdl target)
    get_target_property(links ${target} INTERFACE_LINK_LIBRARIES)
    foreach(entry IN LISTS links)
        if(entry MATCHES "^\\$<LINK_ONLY:")
            continue()
        endif()
        if(entry MATCHES "^SDL3(::|-)" OR entry MATCHES "(Native|_native)$")
            message(FATAL_ERROR "G3: ${target} passes ${entry} on to its users; link it PRIVATE "
                                "(docs/architecture/platform-sdl.md, Design Invariant 2)")
        endif()
    endforeach()
    get_target_property(includes ${target} INTERFACE_INCLUDE_DIRECTORIES)
    foreach(entry IN LISTS includes)
        if(entry MATCHES "native/include")
            message(FATAL_ERROR "G3: ${target} makes ${entry} public; keep it PRIVATE "
                                "(docs/architecture/platform-sdl.md, Design Invariant 2)")
        endif()
    endforeach()
endfunction()
function(gyo_expect_private_sdl_backends)
    gyo_expect_private_sdl(gyo_platform_sdl)
    gyo_expect_private_sdl(gyo_platform_sdl_main)
    foreach(backend IN ITEMS gyo_input_sdl gyo_audio_sdl gyo_render_backend_sdl gyo_render_backend_sdl_gpu)
        if(TARGET ${backend})
            gyo_expect_private_sdl(${backend})
        endif()
    endforeach()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_private_sdl_backends)
