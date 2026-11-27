# A window, its input and its runner through sdl_window (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_window").
# The product carries the default Modules and the sdl_window Plugin; the build adds the Plugin because of this line.
gyo_add_example(gyo_example_sdl_window main.cpp GYO::Input GYO::Window)
gyo_product_units(gyo_example_sdl_window PLUGINS sdl_window)
if(BUILD_TESTING)
    # The example runs headless on SDL's dummy video driver: the runner pumps, frames run, the game closes the window
    # after 30 frames and the runner ends with Success.
    add_test(NAME sdl_window.example COMMAND gyo_example_sdl_window --frames 30)
    set_tests_properties(sdl_window.example PROPERTIES LABELS "cpu" TIMEOUT 60 ENVIRONMENT "SDL_VIDEODRIVER=dummy")
    # An SDL assertion ends the run instead of showing SDL's dialog (tests/common/support/SdlAssertTestSupport.hpp).
    set_property(TEST sdl_window.example APPEND PROPERTY ENVIRONMENT "SDL_ASSERT=abort")
endif()
