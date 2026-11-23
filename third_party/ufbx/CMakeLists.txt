include(FetchContent)

# The FBX importer behind the model Module (engine/modules/model). Only
# gyo_model links it, privately, and only its adapter src/adapters/ufbx/
# includes the header: Design Invariant MD-1 of
# docs/architecture/target/v2/model.zh-Hant.md. Two files, ufbx.h and ufbx.c of
# v0.23.1, fetched at the release's commit (not a tag) and checked by their
# SHA-256, so the fetched content cannot move. Dual licensed, MIT or public
# domain (Unlicense).
set(GYO_UFBX_COMMIT 26a482ae66871d7de36eb722aa060bce95bce274)
FetchContent_Declare(ufbx_header
    URL https://raw.githubusercontent.com/ufbx/ufbx/${GYO_UFBX_COMMIT}/ufbx.h
    URL_HASH SHA256=f787be529af577efc04f3e3e735844a08556718dbb613d1211c2b313d72895dc
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_Declare(ufbx_source
    URL https://raw.githubusercontent.com/ufbx/ufbx/${GYO_UFBX_COMMIT}/ufbx.c
    URL_HASH SHA256=4a956c26a708e40d82ecb0aeaee54da709d5ba7c144c9642174970059bda7e1d
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(ufbx_header ufbx_source)

add_library(gyo_ufbx STATIC ${ufbx_source_SOURCE_DIR}/ufbx.c)
target_include_directories(gyo_ufbx SYSTEM PUBLIC ${ufbx_header_SOURCE_DIR})
set_target_properties(gyo_ufbx PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
# ufbx.c includes "ufbx.h" from its own directory first; the header lives in
# another fetched directory, so it is put on the path above.
# PUBLIC so the header sees the same configuration as the implementation: no
# stdio (reading files is asset's, and a loader opens no path written in a
# file).
target_compile_definitions(gyo_ufbx PUBLIC UFBX_NO_STDIO)
