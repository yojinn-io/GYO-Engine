gyo_app_requirements(COMPONENTS SDL_RENDERER)
if(GYO_APP_DISCOVERY)
    return()
endif()

add_executable(Runtime WIN32
    main.cpp
)

target_link_libraries(Runtime PRIVATE
    GYO::Engine
    GYO::PlatformSDL
    GYO::RenderBackendSDL
)

gyo_register_app_package(TARGET Runtime STARTUP_ARGS --smoke-test
    STARTUP_ENVIRONMENT "SDL_VIDEODRIVER=dummy" "SDL_RENDER_DRIVER=software"
    STARTUP_TIMEOUT 15)

if(BUILD_TESTING)
    add_test(NAME runtime_smoke COMMAND Runtime --smoke-test)
    set_tests_properties(runtime_smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 15
    )
endif()
