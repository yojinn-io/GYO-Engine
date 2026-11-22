# The World end of asset (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md, 5.1; step 3 of
# section 9): the registration unit WorldAssetModule that owns the Product's AssetStore, the two Resources every
# World gets (AssetRequests, AssetView), the apply System and the prepare function. engine/modules/asset stays the
# library that knows nothing of ECS (tools use it without a World), as engine/time and engine/world_time are split.
add_library(gyo_world_asset STATIC
    src/AssetView.cpp
    src/WorldAssetModule.cpp
)
add_library(GYO::WorldAsset ALIAS gyo_world_asset)

target_include_directories(gyo_world_asset PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6). No link to engine/threads: the executor is
# the product's, given through AssetOptions (5.4).
target_link_libraries(gyo_world_asset PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(world_asset)
target_compile_features(gyo_world_asset PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_world_asset PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_world_asset PRIVATE -Wall -Wextra -Wpedantic)
endif()
