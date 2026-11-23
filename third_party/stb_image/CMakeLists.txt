include(FetchContent)

# The PNG decoder behind the image Module (engine/modules/image). Only gyo_image
# links it, privately, and only its adapter src/adapters/stb_image/ includes the
# header: Design Invariant IM-1 of docs/architecture/target/v2/image.zh-Hant.md.
# One file, stb_image.h v2.34, fetched at a commit (not a branch) and checked
# by its SHA-256, so the fetched content cannot move. Dual licensed, MIT or
# public domain (the end of the file).
FetchContent_Declare(stb_image
    URL https://raw.githubusercontent.com/nothings/stb/1da9ade1e66195252114f5da497f244704a6c58d/stb_image.h
    URL_HASH SHA256=dfc2aadda6295f67f677587773f25e5bf8c8ad427ed0db7c518525c91e72e020
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(stb_image)

add_library(gyo_stb_image STATIC stb_image_implementation.c)
target_include_directories(gyo_stb_image SYSTEM PUBLIC ${stb_image_SOURCE_DIR})
set_target_properties(gyo_stb_image PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES)
# PUBLIC so the header sees the same configuration as the implementation
# (asset declaration 5.11): PNG only; no stdio (reading files is asset's); no
# HDR or linear float paths; no failure strings (a global or thread-local
# reason); the same largest side as PngLoader::kMaxDimension.
target_compile_definitions(gyo_stb_image PUBLIC
    STBI_ONLY_PNG
    STBI_NO_STDIO
    STBI_NO_HDR
    STBI_NO_LINEAR
    STBI_NO_FAILURE_STRINGS
    STBI_MAX_DIMENSIONS=16384
)
