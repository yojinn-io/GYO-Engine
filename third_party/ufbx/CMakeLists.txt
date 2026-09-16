include(FetchContent)

# Pinned importer version. ufbx is private to the optional adapter; the neutral
# model library and engine core configure without this dependency.
FetchContent_Declare(ufbx
    GIT_REPOSITORY https://github.com/ufbx/ufbx.git
    GIT_TAG v0.23.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR gyo-no-upstream-cmake
)
FetchContent_MakeAvailable(ufbx)

add_library(gyo_ufbx STATIC ${ufbx_SOURCE_DIR}/ufbx.c)
target_include_directories(gyo_ufbx PUBLIC ${ufbx_SOURCE_DIR})
set_target_properties(gyo_ufbx PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
# Never let importer code open material/cache paths outside the AssetManager.
target_compile_definitions(gyo_ufbx PRIVATE UFBX_NO_STDIO)
