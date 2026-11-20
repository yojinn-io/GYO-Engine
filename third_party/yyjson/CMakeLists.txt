include(FetchContent)

# The JSON parser behind the engine's JSON Module (engine/json). Only gyo_json
# links it, privately: Design Invariant J-1 of
# docs/architecture/target/v2/json-module-declaration.zh-Hant.md.
# Pinned to the commit of the 0.13.0 release, not the tag, so the fetched
# content cannot move.
FetchContent_Declare(yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 6447536015f3d600f3d65323b10976103b337ca7 # 0.13.0
    GIT_SUBMODULES ""
    SOURCE_SUBDIR gyo-no-upstream-cmake
)
FetchContent_MakeAvailable(yyjson)

add_library(gyo_yyjson STATIC ${yyjson_SOURCE_DIR}/src/yyjson.c)
target_include_directories(gyo_yyjson SYSTEM PUBLIC ${yyjson_SOURCE_DIR}/src)
set_target_properties(gyo_yyjson PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
# PUBLIC so the header sees the same configuration as the library.
# The writer stays in (the JSON Module writes numbers with yyjson_write_number),
# and so does the fast floating-point conversion: without it yyjson formats
# numbers through the C library, which differs between platforms.
# Non-standard input and output (NaN, Infinity, comments) is compiled out.
target_compile_definitions(gyo_yyjson PUBLIC
    YYJSON_DISABLE_INCR_READER=1
    YYJSON_DISABLE_UTILS=1
    YYJSON_DISABLE_FILE=1
    YYJSON_DISABLE_NON_STANDARD=1
)
