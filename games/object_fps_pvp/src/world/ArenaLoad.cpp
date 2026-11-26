#include "object_fps_pvp/world/ArenaLoad.hpp"

#include "engine/asset/AssetLoadFailure.hpp"

namespace object_fps_pvp::world {

namespace A = GYO::Engine::Asset;
namespace VM = GYO::Engine::VoxelMap;
namespace WA = GYO::Engine::WorldAsset;

void HashValue(K::Hasher&, const MapLoad&) {}

namespace {

std::string Describe(const std::vector<WA::AssetReadyFailure>& failures) {
    std::string text;
    for (const WA::AssetReadyFailure& failure : failures) {
        if (!text.empty()) text += "; ";
        text += failure.id.Empty() ? failure.writtenPath : failure.id.Text();
        if (!failure.parent.Empty()) text += " (named by " + failure.parent.Text() + ")";
        text += ": ";
        text += A::ToString(failure.code);
    }
    return text;
}

} // namespace

MapReading ReadMap(const WA::AssetView& view, const A::Handle<VM::VoxelMapAsset>& map) {
    MapReading reading;
    const WA::AssetReadiness ready = view.Ready({map});
    if (!ready.failures.empty()) {
        reading.failure = Describe(ready.failures);
        return reading;
    }
    // The map's loaders are registered before any World is built (ConfigureGame): a wait for one never ends.
    if (!ready.waitingForLoader.empty() || !ready.skipped.empty()) {
        reading.failure = "no loader for the map (ConfigureGame registers them)";
        return reading;
    }
    const VM::VoxelMapAsset* asset = view.Get(map);
    if (asset == nullptr) return reading;
    reading.total = static_cast<std::uint32_t>(asset->regions.size());
    for (const auto& reference : asset->regions) {
        const VM::VoxelRegionDocument* region = view.Get(reference);
        if (region != nullptr) ++reading.done;
        reading.regions.push_back(region);
    }
    if (!ready.complete) {
        reading.regions.clear();
        return reading;
    }
    // A Loaded region has passed the rules that need no index; the rest (its sha, its chunks inside the map) here.
    if (auto checked = VM::CheckVoxelMap(asset->document, reading.regions); !checked) {
        const VM::VoxelMapError& error = checked.error();
        reading.failure = "the map: " + (error.detail.empty() ? error.message : error.message + " at " + error.detail);
        reading.regions.clear();
        return reading;
    }
    reading.complete = true;
    reading.index = &asset->document;
    return reading;
}

} // namespace object_fps_pvp::world
