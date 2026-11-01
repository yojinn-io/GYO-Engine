// v8-product-anim-bottlenecks (docs/object_fps_pvp/plans/v8/v8-product-anim-bottlenecks-declaration.md):
// a report-only measurement of what a Match-side pose would cost. It measures;
// the thresholds were declared and approved before any result
// (v8-product-anim-bottlenecks-declaration-approve), and it fails only when the
// load or its own self-check fails.
//
// B1 the sampling time of one full player pose, B2 the load of the player and
// its transferred clips (its peak memory is read around this program by
// anim_bottlenecks.py, outside C++: os_boundary_guard R3), B3 bit hashes of the transferred keys (H-load) and of
// the sampled global transforms (H-pose) for comparison across platforms.
#include "RetroFPS/Pvp/PlayerPresentation.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/model/Animation.hpp"
#include "engine/model/backend/ufbx/UfbxModelLoader.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "gyo/AppConfig.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fps::pvp;
using Clock = std::chrono::steady_clock;

constexpr int WarmupIterations = 100;
constexpr int TimedIterations = 2000;
constexpr int LoadRepeats = 3;
constexpr int WorstCaseSamplesPerTick = 12; // v8-product-anim-bottlenecks-worst-case (a)

// The build names the platform (CMAKE_SYSTEM_NAME and processor); the source
// branches on no OS (os_boundary_guard R2).
const char* Platform() { return GYO_ANIM_BOTTLENECKS_PLATFORM; }

std::filesystem::path AssetRoot() {
    const auto base = Engine::Platform::Sdl::BaseDirectory();
    if (!base) throw std::runtime_error("Cannot locate the PvP product directory");
    return base.value() / Gyo::AppConfig::Assets;
}

Engine::Asset::AssetCatalog LoadCatalog() {
    const auto root = AssetRoot();
    const auto manifest = Engine::Asset::ContentManifest::Load(root);
    if (!manifest) throw std::runtime_error(manifest.error().message + " " + manifest.error().detail);
    auto catalog = manifest.value().LoadCatalogs(root);
    if (!catalog) throw std::runtime_error(catalog.error().message + " " + catalog.error().detail);
    return std::move(catalog).value();
}

struct Assets final {
    Engine::Asset::AssetCatalog catalog{LoadCatalog()};
    Engine::Asset::Loading::LoaderRegistry loaders;
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, loaders};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy policy{{}};
    Engine::Asset::AssetManager assets{catalog, pipeline, storage, lifetime, policy};
    Assets() {
        loaders.Register(std::make_unique<Engine::Asset::Loaders::TextLoader>());
        loaders.Register(std::make_unique<Engine::Asset::Loaders::SdlImage::SdlImageTextureLoader>());
        loaders.Register(std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>());
    }
};

struct Loaded final {
    std::unique_ptr<Assets> fixture;
    std::shared_ptr<const PlayerPresentationDefinition> definition;
};

// The Client's load path: catalog, FBX import and the transfer of every clip.
Loaded Load() {
    Loaded loaded{std::make_unique<Assets>(), nullptr};
    std::string error;
    loaded.definition = LoadPlayerPresentationDefinition(loaded.fixture->assets, 1.8F, error);
    if (!loaded.definition) throw std::runtime_error(error);
    return loaded;
}

// FNV-1a over the exact bits of each value; Count is the number of values.
class BitHash final {
public:
    void Add(float value) { Bytes(&value, sizeof value); ++count_; }
    void Add(double value) { Bytes(&value, sizeof value); ++count_; }
    void Add(std::uint64_t value) { Bytes(&value, sizeof value); ++count_; }
    [[nodiscard]] std::uint64_t Value() const noexcept { return hash_; }
    [[nodiscard]] std::uint64_t Count() const noexcept { return count_; }
private:
    void Bytes(const void* data, std::size_t size) {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < size; ++i) { hash_ ^= bytes[i]; hash_ *= 1099511628211ULL; }
    }
    std::uint64_t hash_{14695981039346656037ULL};
    std::uint64_t count_{};
};

std::string Hex(std::uint64_t value) {
    std::ostringstream out;
    out << std::hex << value;
    return out.str();
}

std::vector<std::size_t> UsedClips(const PlayerPresentationDefinition& d) {
    return {d.idleClip, d.walkClip, d.jogClip, d.shootClip, d.reloadClip, d.jumpStartClip,
            d.jumpLoopClip, d.jumpLandClip, d.deathClip, d.aimUpClip, d.aimNeutralClip,
            d.aimDownClip, d.hitClip, d.crouchIdleClip, d.crouchMoveClip};
}

BitHash HashClips(const PlayerPresentationDefinition& definition, std::uint64_t& keyBytes) {
    const auto& model = *definition.character->model;
    BitHash hash;
    keyBytes = 0;
    for (const auto index : UsedClips(definition)) {
        const auto& clip = model.clips.at(index);
        hash.Add(clip.durationSeconds);
        for (const auto& track : clip.tracks) {
            hash.Add(static_cast<std::uint64_t>(track.nodeIndex));
            for (const auto& key : track.translations) {
                hash.Add(key.timeSeconds); hash.Add(key.value.x); hash.Add(key.value.y); hash.Add(key.value.z);
            }
            for (const auto& key : track.rotations) {
                hash.Add(key.timeSeconds); hash.Add(key.value.x); hash.Add(key.value.y);
                hash.Add(key.value.z); hash.Add(key.value.w);
            }
            for (const auto& key : track.scales) {
                hash.Add(key.timeSeconds); hash.Add(key.value.x); hash.Add(key.value.y); hash.Add(key.value.z);
            }
            keyBytes += track.translations.size() * sizeof(track.translations[0]) +
                        track.rotations.size() * sizeof(track.rotations[0]) +
                        track.scales.size() * sizeof(track.scales[0]);
        }
    }
    return hash;
}

struct PoseCase final {
    std::string id;
    PlayerLocomotionState state;
    PlayerActionPose actions;
};

// The 24 fixed poses of the declaration, identical on every platform.
std::vector<PoseCase> Cases(const PlayerPresentationDefinition& definition) {
    const auto& clips = definition.character->model->clips;
    const auto at = [&](std::size_t clip, double fraction) { return clips.at(clip).durationSeconds * fraction; };
    std::vector<PoseCase> cases;
    const auto add = [&](std::string id, auto&& configure) {
        PoseCase c{std::move(id), {}, {}};
        c.state.initialized = true;
        c.state.phaseCycles = 0.3;
        c.state.idleSeconds = 0.7;
        configure(c.state, c.actions);
        cases.push_back(std::move(c));
    };
    for (const float move : {0.0F, 0.5F, 1.0F})
        for (const bool shoot : {false, true})
            add(std::string("move") + std::to_string(static_cast<int>(move * 10)) + (shoot ? "_shoot" : "_hold"),
                [&](PlayerLocomotionState& s, PlayerActionPose& a) {
                    s.moveWeight = move; s.jogWeight = 0.5;
                    if (shoot) { a.upper = PlayerUpperAction::Shoot; a.upperClipSeconds = at(definition.shootClip, 0.3); }
                });
    for (const float move : {0.0F, 0.5F, 1.0F})
        add("crouch" + std::to_string(static_cast<int>(move * 10)), [&](PlayerLocomotionState& s, PlayerActionPose&) {
            s.crouched = true; s.moveWeight = move;
        });
    const std::array<std::pair<PlayerLowerAction, std::size_t>, 3> jumps{{
        {PlayerLowerAction::JumpStart, definition.jumpStartClip},
        {PlayerLowerAction::JumpLoop, definition.jumpLoopClip},
        {PlayerLowerAction::JumpLand, definition.jumpLandClip}}};
    for (std::size_t j = 0; j < jumps.size(); ++j)
        for (const double fraction : {0.25, 0.75})
            add("jump" + std::to_string(j) + "_" + std::to_string(static_cast<int>(fraction * 100)),
                [&](PlayerLocomotionState&, PlayerActionPose& a) {
                    a.lower = jumps[j].first; a.lowerClipSeconds = at(jumps[j].second, fraction);
                });
    for (const double fraction : {0.25, 0.75})
        add("death_" + std::to_string(static_cast<int>(fraction * 100)), [&](PlayerLocomotionState& s, PlayerActionPose& a) {
            s.dead = true; a.lower = PlayerLowerAction::Death; a.lowerClipSeconds = at(definition.deathClip, fraction);
        });
    for (const float aim : {-1.0F, 0.0F, 1.0F})
        add("aim" + std::to_string(static_cast<int>(aim)), [&](PlayerLocomotionState&, PlayerActionPose& a) {
            a.aimWeight = aim;
        });
    const std::array<std::pair<PlayerUpperAction, std::size_t>, 2> uppers{{
        {PlayerUpperAction::Reload, definition.reloadClip}, {PlayerUpperAction::Hit, definition.hitClip}}};
    for (std::size_t u = 0; u < uppers.size(); ++u)
        for (const double fraction : {0.25, 0.75})
            add(std::string(u == 0 ? "reload_" : "hit_") + std::to_string(static_cast<int>(fraction * 100)),
                [&](PlayerLocomotionState&, PlayerActionPose& a) {
                    a.upper = uppers[u].first; a.upperClipSeconds = at(uppers[u].second, fraction);
                });
    return cases;
}

void Sample(const PlayerPresentationDefinition& definition, const PoseCase& c, PlayerPresentationPose& pose) {
    std::string error;
    if (!SamplePlayerPresentationPose(definition, c.state, c.actions, pose, error))
        throw std::runtime_error("pose " + c.id + ": " + error);
}

BitHash HashPoses(const PlayerPresentationDefinition& definition, const std::vector<PoseCase>& cases) {
    BitHash hash;
    PlayerPresentationPose pose;
    for (const auto& c : cases) {
        Sample(definition, c, pose);
        for (const auto& matrix : pose.body.globalTransforms)
            for (const float value : matrix.values) hash.Add(value);
    }
    return hash;
}

// The hash must see every value: one flipped bit in one value changes it.
bool HashDetectsFlip() {
    const std::array<float, 3> values{1.0F, -2.5F, 0.125F};
    BitHash a, b;
    for (const float v : values) a.Add(v);
    auto flipped = values;
    std::uint32_t bits{};
    std::memcpy(&bits, &flipped[1], sizeof bits);
    bits ^= 1U;
    std::memcpy(&flipped[1], &bits, sizeof bits);
    for (const float v : flipped) b.Add(v);
    return a.Value() != b.Value() && a.Count() == b.Count();
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string out;
        std::string mode; // empty, --baseline (catalog only) or --load-only (one full load)
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--out" && i + 1 < argc) out = argv[++i];
            else if ((arg == "--baseline" || arg == "--load-only") && mode.empty()) mode = arg;
            else throw std::invalid_argument("usage: [--baseline | --load-only] [--out report.json]");
        }
        if (mode == "--baseline") { (void)LoadCatalog(); return 0; }
        if (mode == "--load-only") { (void)Load(); return 0; }
        if (!HashDetectsFlip()) { std::cerr << "anim bottlenecks: self-check failed: bit flip undetected\n"; return 1; }

        // B2
        std::vector<double> loadSeconds;
        Loaded loaded;
        for (int i = 0; i < LoadRepeats; ++i) {
            loaded = {};
            const auto start = Clock::now();
            loaded = Load();
            loadSeconds.push_back(std::chrono::duration<double>(Clock::now() - start).count());
        }
        const auto& definition = *loaded.definition;
        const auto root = AssetRoot();
        const auto fileBytes = [&](const char* path) {
            return static_cast<std::uint64_t>(std::filesystem::file_size(root / path));
        };

        // B3
        std::uint64_t keyBytes{}, keyBytesAgain{};
        const auto load1 = HashClips(definition, keyBytes), load2 = HashClips(definition, keyBytesAgain);
        const auto cases = Cases(definition);
        const auto pose1 = HashPoses(definition, cases), pose2 = HashPoses(definition, cases);
        const std::uint64_t expectedPoseValues =
            static_cast<std::uint64_t>(definition.character->model->nodes.size()) * 16 * cases.size();
        const bool repeatEqual = load1.Value() == load2.Value() && pose1.Value() == pose2.Value();
        const bool coverage = cases.size() == 24 && pose1.Count() == expectedPoseValues && load1.Count() > 0;

        // B1
        nlohmann::json perCase = nlohmann::json::object();
        double worstP99 = 0;
        std::string worstCase;
        std::uint64_t timedSamples = 0;
        PlayerPresentationPose pose;
        for (const auto& c : cases) {
            for (int i = 0; i < WarmupIterations; ++i) Sample(definition, c, pose);
            std::vector<double> micros;
            micros.reserve(TimedIterations);
            for (int i = 0; i < TimedIterations; ++i) {
                const auto start = Clock::now();
                Sample(definition, c, pose);
                micros.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count());
            }
            timedSamples += micros.size();
            std::sort(micros.begin(), micros.end());
            const double median = micros[micros.size() / 2];
            const double p99 = micros[static_cast<std::size_t>(micros.size() * 0.99)];
            perCase[c.id] = {{"median_us", median}, {"p99_us", p99}, {"max_us", micros.back()}};
            if (p99 > worstP99) { worstP99 = p99; worstCase = c.id; }
        }
        const bool complete = timedSamples == cases.size() * TimedIterations &&
                              loadSeconds.size() == LoadRepeats;

        const double loadMax = *std::max_element(loadSeconds.begin(), loadSeconds.end());
        const double tickMs = WorstCaseSamplesPerTick * worstP99 / 1000.0;
        std::ostringstream summary;
        summary.precision(4);
        summary << "anim bottlenecks: platform=" << Platform() << " complete=" << complete
                << " selfcheck=" << coverage << " repeat_equal=" << repeatEqual
                << " b1_worst=" << worstCase << " b1_p99_us=" << worstP99 << " b1_12x_ms=" << tickMs
                << " b2_load_max_s=" << loadMax
                << " h_load=" << Hex(load1.Value()) << "/" << load1.Count()
                << " h_pose=" << Hex(pose1.Value()) << "/" << pose1.Count()
                << " animation_digest=" << Hex(AnimationContentDigest(definition));
        std::cout << summary.str() << '\n';
        std::ostringstream p99s;
        p99s.precision(4);
        p99s << "anim bottlenecks p99_us:";
        for (const auto& [id, value] : perCase.items()) p99s << ' ' << id << '=' << value["p99_us"].get<double>();
        std::cout << p99s.str() << '\n';

        const nlohmann::json report{
            {"version", 1}, {"platform", Platform()}, {"complete", complete},
            {"b1", {{"warmup", WarmupIterations}, {"iterations", TimedIterations}, {"cases", perCase},
                    {"worst_case", worstCase}, {"worst_p99_us", worstP99},
                    {"samples_per_tick", WorstCaseSamplesPerTick}, {"worst_tick_ms", tickMs}}},
            {"b2", {{"load_seconds", loadSeconds},
                    {"animation_fbx_bytes", fileBytes("animations/ual_mannequin/UAL1_Standard.fbx")},
                    {"player_fbx_bytes", fileBytes("characters/player/Player_V20.fbx")},
                    {"used_clip_key_bytes", keyBytes}, {"used_clips", UsedClips(definition).size()}}},
            {"b3", {{"h_load", Hex(load1.Value())}, {"h_load_values", load1.Count()},
                    {"h_pose", Hex(pose1.Value())}, {"h_pose_values", pose1.Count()},
                    {"h_pose_expected_values", expectedPoseValues}, {"repeat_equal", repeatEqual},
                    {"animation_digest", Hex(AnimationContentDigest(definition))}}},
        };
        if (!out.empty()) std::ofstream(out) << report.dump(1) << '\n';
        if (!coverage) { std::cerr << "anim bottlenecks: self-check failed: hash coverage\n"; return 1; }
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "anim bottlenecks: " << failure.what() << '\n';
        return 2;
    }
}
