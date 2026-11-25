# The window's mode, its Windowed size and VSync on a letterboxed picture (window-mode-vsync declaration; the
# machine checks of docs/architecture/target/v2/window-mode-vsync-declaration.zh-Hant.md section 5).
gyo_add_example(gyo_example_window_mode main.cpp GYO::Input GYO::Window GYO::Camera GYO::Render GYO::RenderWorld)
gyo_product_units(gyo_example_window_mode MODULES render_world PLUGINS sdl_window sdl_gpu)
if(BUILD_TESTING)
    # A shown window on a GPU (label gpu: L1 runs it on Linux on Lavapipe under Xvfb): the switches go by themselves,
    # at least one frame is shown, no frame, request or present mode fails.
    add_test(NAME window_mode.example COMMAND gyo_example_window_mode --frames 340)
    set_tests_properties(window_mode.example PROPERTIES LABELS "gpu" TIMEOUT 120)
endif()
