# Examples

Programs that show how a product uses the Engine. They are not part of the Engine and not tests:
each is its own executable that links the Engine the way a product does. They are built with
`-DGYO_BUILD_EXAMPLES=ON` (on by default when `BUILD_TESTING` is on, so CI keeps them compiling).

| Example | Target | Shows |
|---|---|---|
| [`orchestration/product_main.cpp`](orchestration/product_main.cpp) | `gyo_example_product_main` | A product's `main`: build a `Product`, run the default runner, and handle its exit (log the reason once, return the exit code). Run with `bad` for a broken frame. |
