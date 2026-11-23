include(FetchContent)

# HarfBuzz, the text shaper behind the engine's text Module
# (engine/modules/text). Only gyo_text links it, privately: Design Invariant
# TX-1 of docs/architecture/target/v2/text.zh-Hant.md. Used under its "Old MIT"
# licence (COPYING). The official release tarball, pinned by its SHA-256
# (the digest GitHub publishes for the release asset).
FetchContent_Declare(harfbuzz
    URL https://github.com/harfbuzz/harfbuzz/releases/download/15.2.0/harfbuzz-15.2.0.tar.xz
    URL_HASH SHA256=e2ec49090744383cdbe7df808c1cedaafba06a97be8dc4211fe546ba585f12cb
    SOURCE_SUBDIR gyo-no-upstream-cmake
)
FetchContent_MakeAvailable(harfbuzz)

# The single-file build. No HAVE_* is defined, so hb-ft (the FreeType glue),
# CoreText, DirectWrite, GDI, Uniscribe, glib, ICU, graphite2 and wasm are
# compiled out; Unicode comes from HarfBuzz's own tables. HB_NO_GETENV turns
# off every environment variable HarfBuzz reads (HB_OPTIONS, HB_SHAPER_LIST,
# HB_FONT_FUNCS, HB_FACE_LOADER), so the environment cannot change shaping
# (Design Invariants TX-7 and TX-8).
add_library(gyo_harfbuzz STATIC ${harfbuzz_SOURCE_DIR}/src/harfbuzz.cc)
target_include_directories(gyo_harfbuzz SYSTEM PUBLIC ${harfbuzz_SOURCE_DIR}/src)
target_compile_definitions(gyo_harfbuzz PUBLIC HB_NO_GETENV)
target_compile_features(gyo_harfbuzz PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(gyo_harfbuzz PRIVATE /bigobj)
endif()
