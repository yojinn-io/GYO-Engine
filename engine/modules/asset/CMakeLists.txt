# The asset Module, library part (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md,
# 5.1; step 1 of section 9): AssetId, content paths, the catalog Data Contract (read, write, check),
# the loader interface (AssetLoader, LoadContext, AssetRef), reading a whole file and the text loader;
# step 2: the store (AssetStore, Handle, AssetLoaderRegistry, AssetSnapshot, the executor interface).
# It knows nothing of ECS: what goes into a World lives in engine/modules/world_asset, as engine/time
# and engine/world_time are split. No external component.
add_library(gyo_asset STATIC
    src/AssetId.cpp
    src/AssetSnapshot.cpp
    src/AssetStore.cpp
    src/Catalog.cpp
    src/ContentPath.cpp
    src/LoadContext.cpp
    src/ReadWholeFile.cpp
    src/TextLoader.cpp
)
add_library(GYO::Asset ALIAS gyo_asset)

target_include_directories(gyo_asset PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_asset PUBLIC cxx_std_20)
# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6).
# GYO::Base publicly; GYO::IO and GYO::Json (registered) privately, so no user of
# GYO::Asset sees their headers.
# Threads::Threads for the store's lock and std::thread::id; asset does not use engine/threads (X-1).
find_package(Threads REQUIRED)
target_link_libraries(gyo_asset PUBLIC GYO::Base PRIVATE GYO::IO Threads::Threads)
gyo_link_registered_dependencies(asset)
if(MSVC)
    target_compile_options(gyo_asset PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_asset PRIVATE -Wall -Wextra -Wpedantic)
endif()
