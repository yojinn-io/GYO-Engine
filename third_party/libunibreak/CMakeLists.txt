include(FetchContent)

# libunibreak, the line breaking (UAX #14) and grapheme breaking (UAX #29)
# behind the engine's text Module (engine/modules/text; decision
# v8-arch-gyo2-sdl-t, the user's choice (丁)). Only gyo_text links it,
# privately: Design Invariant TX-1 of docs/architecture/target/v2/text.zh-Hant.md.
# Used under the zlib licence (LICENCE). Unicode 17.0. The official release
# tarball, pinned by its SHA-256.
FetchContent_Declare(libunibreak
    URL https://github.com/adah1972/libunibreak/releases/download/libunibreak_8_0_1/libunibreak-8.0.1.tar.gz
    URL_HASH SHA256=ac0dcc96c395bc86ff7f86f6b91d8b45bc8c39db775f3f1e73f947a11520737b
    SOURCE_SUBDIR gyo-no-upstream-cmake
)
FetchContent_MakeAvailable(libunibreak)

# The library's own source list (src/Makefile.am); the data files are included
# by these. Word breaking is not used yet and is left out.
set(_gyo_ub ${libunibreak_SOURCE_DIR}/src)
add_library(gyo_libunibreak STATIC
    ${_gyo_ub}/unibreakbase.c
    ${_gyo_ub}/unibreakdef.c
    ${_gyo_ub}/linebreak.c
    ${_gyo_ub}/linebreakdata.c
    ${_gyo_ub}/linebreakdef.c
    ${_gyo_ub}/eastasianwidthdef.c
    ${_gyo_ub}/emojidef.c
    ${_gyo_ub}/graphemebreak.c
)
unset(_gyo_ub)
target_include_directories(gyo_libunibreak SYSTEM PUBLIC ${libunibreak_SOURCE_DIR}/src)
set_target_properties(gyo_libunibreak PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
