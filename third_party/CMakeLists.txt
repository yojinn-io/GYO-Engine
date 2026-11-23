# The JSON Module (engine/json) is an engine Module, so its external components
# are added whether or not tests are built.
add_subdirectory(yyjson)
add_subdirectory(fast_float)
# The image Module (engine/image) is an engine Module too.
add_subdirectory(stb_image)

if(BUILD_TESTING)
    add_subdirectory(doctest)
endif()
