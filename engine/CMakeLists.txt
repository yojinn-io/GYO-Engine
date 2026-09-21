add_library(engine STATIC)
add_library(GYO::Engine ALIAS engine)
target_compile_features(engine PUBLIC cxx_std_20)

target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/base/include
    ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/asset/include
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
)

target_sources(engine
    PRIVATE
    # runtime
    runtime/src/RuntimeLoop.cpp

    # base
    # io/fs
    io/src/fs/DirectoryIterator.cpp
    io/src/fs/MountTable.cpp
    io/src/fs/Vfs.cpp
    # io/helpers
    io/src/helpers/ReadAllBytes.cpp
    io/src/helpers/ReadAllText.cpp
    io/src/helpers/WriteAllBytes.cpp
    io/src/helpers/WriteAllText.cpp
    # io/path
    io/src/path/PathUtils.cpp
    io/src/path/Uri.cpp
    # io/stream
    io/src/stream/MemoryStream.cpp
    io/src/stream/SpanStream.cpp
    io/src/stream/BufferedStream.cpp
    io/src/stream/StreamReader.cpp
    io/src/stream/StreamWriter.cpp

    # asset/catalog
    asset/src/catalog/CatalogParser.cpp
    # asset/loaders
    asset/src/loaders/BinaryLoader.cpp
    asset/src/loaders/FontLoader.cpp
    asset/src/loaders/SoundLoader.cpp
    asset/src/loaders/TextLoader.cpp
    asset/src/loaders/TextureLoader.cpp
    # asset/
    asset/src/ContentManifest.cpp
    asset/src/AssetCatalog.cpp
    asset/src/AssetManager.cpp
    asset/src/AssetPathResolver.cpp
    asset/src/AssetPipeline.cpp
    asset/src/AssetWatcher.cpp
    asset/src/LoaderRegistry.cpp
    asset/src/NativeFileAssetSource.cpp
)
target_link_libraries(engine PRIVATE
    nlohmann_json::nlohmann_json
)

# SDL_image is an optional decoder implementation. GYO::Engine remains
# backend-neutral and continues to configure without SDL or SDL_image.
if(TARGET SDL3_image::SDL3_image)
    add_library(gyo_asset_sdl_image STATIC
        asset/src/loaders/sdl_image/SdlImageTextureLoader.cpp
    )
    add_library(GYO::AssetSdlImage ALIAS gyo_asset_sdl_image)

    target_link_libraries(gyo_asset_sdl_image
        PUBLIC GYO::Engine
        PRIVATE SDL3_image::SDL3_image SDL3::SDL3
    )
endif()

add_subdirectory(model)
add_subdirectory(collision)
add_subdirectory(text)
if(GYO_ENABLE_SDL_PLATFORM)
    add_subdirectory(platform)
endif()
add_subdirectory(input)
add_subdirectory(render)
add_subdirectory(ui)
