# Examples

Programs that show how a product uses the Engine. They are not part of the Engine and not tests:
each is its own executable that links the Engine the way a product does. Each lives in its own directory with its
own `CMakeLists.txt`, which `examples/CMakeLists.txt` adds without naming it, so removing a Plugin also removes the
examples that carry it (`build/ci/common/plugin_removal.py`). They are built with
`-DGYO_BUILD_EXAMPLES=ON` (on by default when `BUILD_TESTING` is on, so CI keeps them compiling).

| Example | Target | Shows |
|---|---|---|
| [`orchestration/product_main.cpp`](orchestration/product_main.cpp) | `gyo_example_product_main` | A product's `main`: build a `Product`, run the default runner, and handle its exit (log the reason once, return the exit code). Run with `bad` for a broken frame. |
| [`sdl_window/main.cpp`](sdl_window/main.cpp) | `gyo_example_sdl_window` | A window through the `sdl_window` Plugin: the game reads `input`'s state, writes `window`'s requests (title, close) and closes on Escape; the Plugin's runner pumps the OS events and ends when the close was applied. `--frames N` closes after N frames (ctest `sdl_window.example` runs it on SDL's dummy driver). |
