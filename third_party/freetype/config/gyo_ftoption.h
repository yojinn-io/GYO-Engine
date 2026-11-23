/*
 * FreeType's compile-time options for the text Module (FT_CONFIG_OPTIONS_H):
 * the upstream defaults, with the options below turned off.
 *
 * - The environment cannot change FreeType's behaviour: with
 *   FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES the variable FREETYPE_PROPERTIES
 *   could change the interpreter version and the hinting engine, so the same
 *   inputs would give different bitmaps on different machines (Design
 *   Invariants TX-7 and TX-8 of docs/architecture/target/v2/text.zh-Hant.md).
 * - No compressed or SVG fonts: the gzip, LZW, bzip2, PNG, Brotli and SVG code
 *   is not compiled (the "font" Data Contract takes sfnt only), and FreeType
 *   does not call HarfBuzz.
 */
#include <freetype/config/ftoption.h>

#undef FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES
#undef FT_CONFIG_OPTION_USE_ZLIB
#undef FT_CONFIG_OPTION_USE_LZW
#undef FT_CONFIG_OPTION_USE_BZIP2
#undef FT_CONFIG_OPTION_USE_PNG
#undef FT_CONFIG_OPTION_USE_BROTLI
#undef FT_CONFIG_OPTION_USE_HARFBUZZ
#undef FT_CONFIG_OPTION_SVG
