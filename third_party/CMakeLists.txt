# The JSON Module (engine/json) is an engine Module, so its external components
# are added whether or not tests are built.
add_subdirectory(yyjson)
add_subdirectory(fast_float)
# The image Module (engine/modules/image) is an engine Module too.
add_subdirectory(stb_image)
# The text Module (engine/modules/text) is an engine Module too: FreeType,
# HarfBuzz and libunibreak are added whether or not tests are built.
add_subdirectory(freetype)
add_subdirectory(harfbuzz)
add_subdirectory(libunibreak)

if(BUILD_TESTING)
    add_subdirectory(doctest)
endif()
