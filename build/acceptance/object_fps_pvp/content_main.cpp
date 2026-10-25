// Content probe (v7.1 windows-long-path, P5): reads the installed content the
// way the Client does before it opens a window (PvpApplication::InitializeContent):
// the content manifest, its catalogs, every shader bundle with all of its
// artifacts (dxil, spirv and metallib, each checked against its hash), then
// the bytes of every catalog asset. No GPU and no window, so it runs on every
// L1 row; content_deep_path.py runs it from a package copied to a deep folder.
//
//   gyo_object_fps_pvp_content_probe --asset-root <bin/assets/object_fps_pvp>
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/os/Files.hpp"
#include "engine/render/ShaderLibrary.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

int Fail(const std::string& what) {
    std::fprintf(stderr, "content probe: %s\n", what.c_str());
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    namespace Asset = Engine::Asset;
    if (argc != 3 || std::string_view(argv[1]) != "--asset-root") return Fail("usage: --asset-root <directory>");
    auto root = Engine::Os::Files::PathFromUtf8(argv[2]);
    if (!root) return Fail("asset root is not UTF-8");
    const std::filesystem::path& assetRoot = root.value();

    auto manifest = Asset::ContentManifest::Load(assetRoot);
    if (!manifest) return Fail(manifest.error().message + ": " + manifest.error().detail);
    auto catalog = manifest.value().LoadCatalogs(assetRoot);
    if (!catalog) return Fail(catalog.error().message + ": " + catalog.error().detail);

    Asset::Loading::NativeFileAssetSource source;
    Engine::Render::ShaderLibrary shaders;
    for (const auto& bundle : manifest.value().shaderBundles) {
        auto bundleRoot = Engine::Os::Files::PathToUtf8(assetRoot / bundle.path);
        if (!bundleRoot) return Fail("shader bundle path has no UTF-8 form: " + bundle.path);
        Asset::Resolver::AssetPathResolver::Options options;
        options.assetsRoot = std::move(bundleRoot.value());
        options.allowAbsolutePath = false;
        options.allowEscapeAssetsRoot = false;
        const auto loaded = shaders.AppendBundle(source, Asset::Resolver::AssetPathResolver(std::move(options)));
        if (!loaded) return Fail(loaded.error().message + ": " + loaded.error().detail);
    }
    if (!shaders.CompleteFormats()) return Fail("shader bundles have no complete common format");

    std::size_t assets = 0;
    for (const auto* entry : catalog.value().Entries()) {
        const auto bytes = source.ReadAll(entry->resolvedPath);
        if (!bytes) return Fail(bytes.error().message + ": " + bytes.error().detail);
        ++assets;
    }
    std::printf("{\"bundles\":%zu,\"programs\":%zu,\"assets\":%zu}\n", manifest.value().shaderBundles.size(),
                shaders.ProgramIds().size(), assets);
    return 0;
}
