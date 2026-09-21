#include "doctest/doctest.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>

#include "engine/asset/AssetError.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"

namespace fs = std::filesystem;
using Engine::Asset::AssetErrorCode;
using Engine::Asset::Loading::NativeFileAssetSource;

TEST_CASE("NativeFileAssetSource reads exact bytes from a resolved path") {
    const fs::path directory =
        fs::temp_directory_path() / "gyo_native_file_asset_source_tests";
    const fs::path path = directory / "payload.bin";
    fs::remove_all(directory);
    fs::create_directories(directory);

    constexpr std::array<unsigned char, 5> expected{0x00, 0x10, 0x7f, 0x80, 0xff};
    {
        std::ofstream output(path, std::ios::binary);
        REQUIRE(output.good());
        output.write(reinterpret_cast<const char*>(expected.data()),
                     static_cast<std::streamsize>(expected.size()));
        REQUIRE(output.good());
    }

    NativeFileAssetSource source;
    auto result = source.ReadAll(path.string());
    REQUIRE(result);
    REQUIRE(result.value().size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        CHECK(result.value()[index] == static_cast<std::byte>(expected[index]));
    }

    fs::remove_all(directory);
}

TEST_CASE("NativeFileAssetSource reports invalid and missing paths") {
    NativeFileAssetSource source;

    const auto empty = source.ReadAll({});
    REQUIRE_FALSE(empty);
    CHECK(empty.error().code == AssetErrorCode::InvalidPath);

    const fs::path missing =
        fs::temp_directory_path() / "gyo_missing_native_asset.bin";
    fs::remove(missing);
    const auto absent = source.ReadAll(missing.string());
    REQUIRE_FALSE(absent);
    CHECK(absent.error().code == AssetErrorCode::SourceReadFailed);
    CHECK(absent.error().detail == missing.string());
}
