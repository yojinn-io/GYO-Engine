# The text Module: fonts, shaping, line breaking, layout and glyph
# rasterization into an atlas behind the engine's own interface
# (docs/architecture/target/v2/text-declaration.zh-Hant.md). Its external
# components are FreeType, HarfBuzz and libunibreak; their headers, types and
# calls appear only under src/adapters/<component>/ (Design Invariant TX-1 of
# docs/architecture/target/v2/text.zh-Hant.md). Not part of any World
# (library_only in config/modules.csv).
add_library(gyo_text STATIC
    src/Caret.cpp
    src/FontData.cpp
    src/FontLoader.cpp
    src/GlyphAtlas.cpp
    src/TextContext.cpp
    src/Utf8.cpp
    src/adapters/freetype/FreeTypeRaster.cpp
    src/adapters/harfbuzz/HarfBuzzShaping.cpp
    src/adapters/libunibreak/Segmentation.cpp
    src/unicode/DefaultIgnorable.cpp
)
add_library(GYO::Text ALIAS gyo_text)

target_include_directories(gyo_text
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(gyo_text PUBLIC cxx_std_20)
# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6). The external components are linked here,
# privately, so no user of GYO::Text sees them.
target_link_libraries(gyo_text PUBLIC GYO::Base)
gyo_link_registered_dependencies(text)
target_link_libraries(gyo_text PRIVATE gyo_freetype gyo_harfbuzz gyo_libunibreak)
if(MSVC)
    target_compile_options(gyo_text PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_text PRIVATE -Wall -Wextra -Wpedantic)
endif()
