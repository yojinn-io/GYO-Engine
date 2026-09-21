add_executable(engine_tests
    test_main.cpp
    asset/ContentManifestTests.cpp
    asset/AssetContentContractTests.cpp
    asset/AssetIdTests.cpp
    asset/AssetTypeTests.cpp
    asset/CatalogParserTests.cpp
    asset/AssetPathResolverTests.cpp
    asset/AssetCatalogTests.cpp
    asset/AssetWatcherTests.cpp
    asset/AssetManagerTests.cpp
    asset/NativeFileAssetSourceTests.cpp
    runtime/RuntimeLoopTests.cpp
    runtime/RuntimePortTests.cpp
    io/BufferedStreamTests.cpp
    io/StreamReaderTests.cpp
)

target_link_libraries(engine_tests PRIVATE
    GYO::Engine
    doctest::doctest
    nlohmann_json::nlohmann_json
)
target_compile_definitions(engine_tests PRIVATE
    GYO_ASSET_CONTRACT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/asset_contract/cases.json")

add_test(NAME engine_tests COMMAND engine_tests)

if(TARGET GYO::AssetSdlImage)
    target_sources(engine_tests PRIVATE
        asset/SdlImageTextureLoaderTests.cpp
    )
    target_link_libraries(engine_tests PRIVATE GYO::AssetSdlImage)
    if(WIN32)
        add_custom_command(TARGET engine_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:engine_tests>
                $<TARGET_FILE_DIR:engine_tests>
            COMMAND_EXPAND_LISTS
        )
    endif()
endif()
