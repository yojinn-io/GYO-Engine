add_library(engine STATIC)
add_library(GYO::Engine ALIAS engine)
target_compile_features(engine PUBLIC cxx_std_20)

target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(engine
    PRIVATE
    # runtime
    src/runtime/RuntimeLoop.cpp

    # base
    # io/fs
    src/io/fs/DirectoryIterator.cpp
    src/io/fs/MountTable.cpp
    src/io/fs/Vfs.cpp
    # io/helpers
    src/io/helpers/ReadAllBytes.cpp
    src/io/helpers/ReadAllText.cpp
    src/io/helpers/WriteAllBytes.cpp
    src/io/helpers/WriteAllText.cpp
    # io/path
    src/io/path/PathUtils.cpp
    src/io/path/Uri.cpp
    # io/stream
    src/io/stream/MemoryStream.cpp
    src/io/stream/SpanStream.cpp
    src/io/stream/BufferedStream.cpp
    src/io/stream/StreamReader.cpp
    src/io/stream/StreamWriter.cpp

    # asset/catalog
    src/asset/catalog/CatalogParser.cpp
    # asset/loaders
    src/asset/loaders/BinaryLoader.cpp
    src/asset/loaders/FontLoader.cpp
    src/asset/loaders/SoundLoader.cpp
    src/asset/loaders/TextLoader.cpp
    src/asset/loaders/TextureLoader.cpp
    # asset/
    src/asset/AssetCatalog.cpp
    src/asset/AssetManager.cpp
    src/asset/AssetPathResolver.cpp
    src/asset/AssetPipeline.cpp
    src/asset/AssetWatcher.cpp
    src/asset/LoaderRegistry.cpp
    src/asset/NativeFileAssetSource.cpp
)
target_link_libraries(engine PRIVATE
    nlohmann_json::nlohmann_json
)

# SDL_image is an optional decoder implementation. GYO::Engine remains
# backend-neutral and continues to configure without SDL or SDL_image.
if(TARGET SDL3_image::SDL3_image)
    add_library(gyo_asset_sdl_image STATIC
        src/asset/loaders/sdl_image/SdlImageTextureLoader.cpp
    )
    add_library(GYO::AssetSdlImage ALIAS gyo_asset_sdl_image)

    target_link_libraries(gyo_asset_sdl_image
        PUBLIC GYO::Engine
        PRIVATE SDL3_image::SDL3_image SDL3::SDL3
    )
endif()
