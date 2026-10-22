# A program on the entry point, run by EntryTests.cpp.
add_executable(platform_sdl_entry_probe EntryProbe.cpp)
target_link_libraries(platform_sdl_entry_probe PRIVATE GYO::PlatformSDLMain GYO::PlatformSDLNative)
target_compile_features(platform_sdl_entry_probe PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(platform_sdl_entry_probe)

add_executable(platform_sdl_tests SdlPlatformTests.cpp EventTimeTests.cpp LogBridgeTests.cpp EntryTests.cpp WindowTests.cpp)
add_dependencies(platform_sdl_tests platform_sdl_entry_probe)
target_compile_definitions(platform_sdl_tests PRIVATE GYO_ENTRY_PROBE="$<TARGET_FILE:platform_sdl_entry_probe>")
target_link_libraries(platform_sdl_tests PRIVATE
    GYO::PlatformSDLNative
    gyo_test_support
    doctest::doctest
)
target_compile_features(platform_sdl_tests PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(platform_sdl_tests)
add_test(NAME platform_sdl_tests COMMAND platform_sdl_tests "-tce=*concurrent*,*two threads*")
set_tests_properties(platform_sdl_tests PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    TIMEOUT 15
)
# The log bridge's concurrency tests, declared with 60 s (ip3-declaration.md).
add_test(NAME platform_sdl_log_bridge_concurrency COMMAND platform_sdl_tests "-tc=*concurrent*,*two threads*")
set_tests_properties(platform_sdl_log_bridge_concurrency PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    TIMEOUT 60
)

# G1 and G2 of docs/architecture/platform-sdl.md (Design Invariant 1).
add_executable(platform_sdl_header_tests HeaderHygieneTests.cpp)
target_link_libraries(platform_sdl_header_tests PRIVATE GYO::PlatformSDL doctest::doctest)
if(TARGET GYO::InputBackendSDL)
    target_link_libraries(platform_sdl_header_tests PRIVATE GYO::InputBackendSDL)
    target_compile_definitions(platform_sdl_header_tests PRIVATE GYO_HYGIENE_SDL_INPUT)
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
