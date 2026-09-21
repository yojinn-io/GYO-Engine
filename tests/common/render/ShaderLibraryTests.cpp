#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <cstring>
#include <map>
#include <string>

#include "engine/base/Sha256.hpp"
#include "render/ShaderLibrary.hpp"

namespace {
using namespace Engine;
using namespace Engine::Render;
using Json = nlohmann::json;
using Bytes = Asset::Loading::ByteBuffer;
Bytes ToBytes(std::string_view value) {
    Bytes result(value.size());
    std::memcpy(result.data(), value.data(), value.size());
    return result;
}
struct MemorySource final : Asset::Loading::IAssetSource {
    std::map<std::string, Bytes, std::less<>> files;
    Base::Result<Bytes, Asset::Loading::AssetError> ReadAll(std::string_view path) override {
        using Result = Base::Result<Bytes, Asset::Loading::AssetError>;
        const auto found = files.find(path);
        if (found == files.end()) return Result::Err(Asset::Loading::AssetError::Make(
            static_cast<Asset::AssetErrorCode>(1), "missing file", std::string(path)));
        return Result::Ok(found->second);
    }
};
struct Fixture {
    MemorySource source;
    Asset::Resolver::AssetPathResolver resolver{
        Asset::Resolver::AssetPathResolver::Options{"bundle"}};
    Json manifest;
    Fixture(std::string id = "builtin/unlit", std::string format = "spirv") {
        const Bytes vertex = ToBytes("test vertex bytes"), fragment = ToBytes("test fragment bytes");
        source.files["bundle/vertex.bin"] = vertex;
        source.files["bundle/fragment.bin"] = fragment;
        manifest = {{"version", 1}, {"abi", "gyo.raster.v1"},
            {"programs", Json::array({{{"id", id}, {"interface", "unlit"},
                {"variants", Json::array({{{"format", format},
                    {"vertex", {{"file", "vertex.bin"}, {"entrypoint", "main"},
                        {"samplers", 0}, {"uniform_sizes", {64}}, {"sha256", Base::Sha256(vertex)}}},
                    {"fragment", {{"file", "fragment.bin"}, {"entrypoint", "main0"},
                        {"samplers", 1}, {"uniform_sizes", {48}}, {"sha256", Base::Sha256(fragment)}}}}})}}})}};
        Save();
    }
    void Save() { source.files["bundle/manifest.json"] = ToBytes(manifest.dump()); }
};

TEST_CASE("SHA256 fingerprints match published short and multi-block vectors") {
    CHECK(Base::Sha256({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(Base::Sha256(ToBytes("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Base::Sha256(ToBytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(Base::Sha256(ToBytes(std::string(1000000, 'a'))) ==
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("ShaderLibrary owns immutable bytes and keeps artifact-specific entrypoints") {
    Fixture fixture;
    ShaderLibrary library;
    REQUIRE(library.AppendBundle(fixture.source, fixture.resolver));
    CHECK(library.CompleteFormats() == FormatBit(ShaderFormat::SPIRV));
    auto program = library.FindProgram("builtin/unlit", ShaderFormat::SPIRV);
    REQUIRE(program);
    CHECK(program.value().vertex->resources.uniformBufferSizes == std::vector<std::uint32_t>{64});
    CHECK(program.value().fragment->entrypoint == "main0");
    fixture.source.files.clear();
    CHECK(program.value().vertex->code == ToBytes("test vertex bytes"));
    CHECK_FALSE(library.FindProgram("game/missing", ShaderFormat::SPIRV));
    CHECK_FALSE(library.FindProgram("builtin/unlit", ShaderFormat::DXIL));
}

TEST_CASE("ShaderLibrary appends atomically and intersects complete program formats") {
    Fixture builtin;
    ShaderLibrary library;
    REQUIRE(library.AppendBundle(builtin.source, builtin.resolver));
    const auto version = library.Version();
    CHECK_FALSE(library.AppendBundle(builtin.source, builtin.resolver));
    CHECK(library.Version() == version);
    Fixture game("game/test", "dxil");
    REQUIRE(library.AppendBundle(game.source, game.resolver));
    CHECK(library.CompleteFormats() == 0);
    CHECK(library.ProgramIds().size() == 2);
    Fixture failed("game/failed");
    failed.manifest["programs"].push_back(failed.manifest["programs"][0]);
    failed.manifest["programs"][1]["id"] = "game/missing_bytes";
    failed.manifest["programs"][1]["variants"][0]["fragment"]["file"] = "missing.bin";
    failed.Save();
    CHECK_FALSE(library.AppendBundle(failed.source, failed.resolver));
    CHECK(library.ProgramIds().size() == 2);
    CHECK_FALSE(library.FindProgram("game/failed", ShaderFormat::SPIRV));
}

TEST_CASE("ShaderLibrary rejects incompatible metadata, corruption, and path escapes") {
    Fixture fixture;
    auto& variant = fixture.manifest["programs"][0]["variants"][0];
    SUBCASE("schema") { fixture.manifest["version"] = 2; }
    SUBCASE("ABI") { fixture.manifest["abi"] = "other.v1"; }
    SUBCASE("uniform size") { variant["fragment"]["uniform_sizes"] = {64}; }
    SUBCASE("resource count") { variant["vertex"]["samplers"] = 1; }
    SUBCASE("fractional resource count") { variant["fragment"]["samplers"] = 1.5; }
    SUBCASE("overflow resource count") { variant["fragment"]["samplers"] = 4294967297ULL; }
    SUBCASE("negative resource count") { variant["vertex"]["samplers"] = -4294967296LL; }
    SUBCASE("overflow uniform size") { variant["vertex"]["uniform_sizes"] = {4294967360ULL}; }
    SUBCASE("storage resources") { variant["fragment"]["storage_buffers"] = 1; }
    SUBCASE("unknown format") { variant["format"] = "dxbc"; }
    SUBCASE("path escape") { variant["fragment"]["file"] = "../fragment.bin"; }
    SUBCASE("absolute Windows path") { variant["fragment"]["file"] = "C:/fragment.bin"; }
    SUBCASE("absolute POSIX path") { variant["fragment"]["file"] = "/fragment.bin"; }
    SUBCASE("corrupt artifact") { fixture.source.files["bundle/fragment.bin"][0] = std::byte{0}; }
    SUBCASE("empty entrypoint") { variant["fragment"]["entrypoint"] = ""; }
    fixture.Save();
    ShaderLibrary library;
    CHECK_FALSE(library.AppendBundle(fixture.source, fixture.resolver));
    CHECK(library.ProgramIds().empty());
    CHECK(library.CompleteFormats() == 0);
}
} // namespace
