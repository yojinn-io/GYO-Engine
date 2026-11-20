# Examples: programs that show how a product uses the Engine. They are not part of the Engine
# and not tests; each is its own executable linking the Engine like a product would.
# Built when GYO_BUILD_EXAMPLES is on (default: with BUILD_TESTING, so CI keeps them compiling).
function(gyo_add_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${ARGN})
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /permissive-)
        target_compile_definitions(${name} PRIVATE NOMINMAX)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

gyo_add_example(gyo_example_product_main orchestration/product_main.cpp GYO::Orchestration)
