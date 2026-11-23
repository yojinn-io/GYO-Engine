include(FetchContent)

# FreeType, the glyph rasterizer behind the engine's text Module
# (engine/modules/text). Only gyo_text links it, privately: Design Invariant
# TX-1 of docs/architecture/target/v2/text.zh-Hant.md. Used under the FreeType
# License (docs/FTL.TXT; a product that links it credits The FreeType Project).
# The official release tarball, pinned by its SHA-256; the same tree as commit
# 37515cf56ba3349656049edbed89ffc6f6466f2d of the official git mirror.
FetchContent_Declare(freetype
    URL https://download.savannah.gnu.org/releases/freetype/freetype-2.15.0.tar.xz
        https://downloads.sourceforge.net/project/freetype/freetype2/2.15.0/freetype-2.15.0.tar.xz
    URL_HASH SHA256=93cd47eb719c325107f2d3cf087d06299e944c3b48a40b55537302304dea1dd5
    SOURCE_SUBDIR gyo-no-upstream-cmake
)
FetchContent_MakeAvailable(freetype)

set(_gyo_ft ${freetype_SOURCE_DIR}/src)
add_library(gyo_freetype STATIC
    ${_gyo_ft}/base/ftsystem.c
    ${_gyo_ft}/base/ftinit.c
    ${_gyo_ft}/base/ftdebug.c
    ${_gyo_ft}/base/ftbase.c
    ${_gyo_ft}/base/ftbbox.c
    ${_gyo_ft}/base/ftbitmap.c
    ${_gyo_ft}/base/ftmm.c
    ${_gyo_ft}/autofit/autofit.c
    ${_gyo_ft}/truetype/truetype.c
    ${_gyo_ft}/cff/cff.c
    ${_gyo_ft}/psaux/psaux.c
    ${_gyo_ft}/pshinter/pshinter.c
    ${_gyo_ft}/psnames/psnames.c
    ${_gyo_ft}/sfnt/sfnt.c
    ${_gyo_ft}/smooth/smooth.c
)
unset(_gyo_ft)
# The GYO configuration directory comes first, so FT_CONFIG_OPTIONS_H and
# FT_CONFIG_MODULES_H below resolve to it.
target_include_directories(gyo_freetype SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${freetype_SOURCE_DIR}/include
)
target_compile_definitions(gyo_freetype
    PUBLIC
        "FT_CONFIG_OPTIONS_H=<gyo_ftoption.h>"
        "FT_CONFIG_MODULES_H=<gyo_ftmodule.h>"
    PRIVATE
        FT2_BUILD_LIBRARY
)
set_target_properties(gyo_freetype PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_definitions(gyo_freetype PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
