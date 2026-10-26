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
    runtime/FixedTickRuntimeTests.cpp
    runtime/RuntimePortTests.cpp
    io/BufferedStreamTests.cpp
    io/NativeFileSystemTests.cpp
    io/StreamReaderTests.cpp
    io/UriTests.cpp
    io/VfsTests.cpp
)

target_link_libraries(engine_tests PRIVATE
    GYO::Engine
    gyo_test_support
    doctest::doctest
    nlohmann_json::nlohmann_json
)
target_compile_definitions(engine_tests PRIVATE
    GYO_ASSET_CONTRACT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/asset_contract/cases.json")

add_test(NAME engine_tests COMMAND engine_tests)

# Windows UNC paths through engine/io (io-native-paths, D54): a temporary tree
# named through the drive's administrative share, at a short and a 300+ character
# path. A machine where the share is not reachable reports Skipped (exit 77).
if(WIN32)
    add_executable(gyo_io_unc_probe io/UncProbe.cpp)
    target_link_libraries(gyo_io_unc_probe PRIVATE GYO::Engine)
    foreach(host localhost 127.0.0.1)
        add_test(NAME io.unc_${host} COMMAND gyo_io_unc_probe ${host})
        set_tests_properties(io.unc_${host} PROPERTIES LABELS "cpu" TIMEOUT 60 SKIP_RETURN_CODE 77)
    endforeach()
endif()

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
