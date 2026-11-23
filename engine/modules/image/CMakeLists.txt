# The image Module: decoded images in one pixel layout (RGBA8, straight alpha,
# row 0 the top row) and the loader of the "image" type (PNG). It is the
# engine's only adapter for stb_image: its header and calls appear only under
# src/adapters/stb_image/ (Design Invariant IM-1 of
# docs/architecture/target/v2/image.zh-Hant.md). No SDL and no renderer, so a
# Match, a test or a tool loads images without a device. Not part of any World
# (library_only in config/modules.csv).
add_library(gyo_image STATIC
    src/ImageData.cpp
    src/PngLoader.cpp
    src/adapters/stb_image/StbPngDecode.cpp
)
add_library(GYO::Image ALIAS gyo_image)

target_include_directories(gyo_image
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(gyo_image PUBLIC cxx_std_20)
# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6).
# stb_image is linked here, privately, so no user of GYO::Image sees it.
target_link_libraries(gyo_image PUBLIC GYO::Base)
gyo_link_registered_dependencies(image)
target_link_libraries(gyo_image PRIVATE gyo_stb_image)
if(MSVC)
    target_compile_options(gyo_image PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_image PRIVATE -Wall -Wextra -Wpedantic)
endif()
