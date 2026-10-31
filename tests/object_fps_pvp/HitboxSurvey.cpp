// v8-product-hitbox-survey (docs/object_fps_pvp/plans/v8/hitbox-plan.md): a
// report-only diagnostic of the production player against the 11 object_fps_v2
// hurt regions. It measures; it decides nothing. The Replacement Criteria
// thresholds were declared before any result (v8-product-hitbox-replacement-thresholds).
//
// Body coordinates follow the renderer: (global point - anchor) * scale, feet
// at the origin, +Y up and +Z forward at yaw 0 (ComputePlanarInput).
#include "RetroFPS/Pvp/PlayerPresentation.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/model/Animation.hpp"
#include "engine/model/backend/ufbx/UfbxModelLoader.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "gyo/AppConfig.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fps::pvp;
using Engine::Math::Vec3;

struct Region final {
    const char* id;
    const char* part; // head, body or limb (v8-product-hit-rules-v2-values)
    const char* start;
    const char* end;  // equal to start: a sphere at the region's vertex centroid
};

// object_fps_v2's hurt_regions (assets/object_fps_v2/characters/enemies/melee.enemy.json).
constexpr std::array<Region, 11> Regions{{
    {"head", "head", "Head", "Head"},
    {"torso", "body", "spine_01", "spine_03"},
    {"pelvis", "body", "pelvis", "spine_01"},
    {"upper_arm_l", "limb", "upperarm_l", "lowerarm_l"},
    {"forearm_l", "limb", "lowerarm_l", "hand_l"},
    {"thigh_l", "limb", "thigh_l", "calf_l"},
    {"calf_l", "limb", "calf_l", "foot_l"},
    {"upper_arm_r", "limb", "upperarm_r", "lowerarm_r"},
    {"forearm_r", "limb", "lowerarm_r", "hand_r"},
    {"thigh_r", "limb", "thigh_r", "calf_r"},
    {"calf_r", "limb", "calf_r", "foot_r"},
}};

// The nodes whose skinned vertices form each region: a vertex belongs to the
// region of the first of these nodes on its dominant joint's path to the root.
// Hands and feet (and the fingers and toes below them) belong to no region: v2's
// forearm ends at the wrist and its calf at the ankle, and their vertices beyond
// those endpoints would inflate the limb radii (survey run 1).
const std::map<std::string, std::string> RegionOfNode{
    {"Head", "head"}, {"neck_01", "head"},
    {"spine_02", "torso"}, {"spine_03", "torso"}, {"clavicle_l", "torso"}, {"clavicle_r", "torso"},
    {"pelvis", "pelvis"}, {"spine_01", "pelvis"},
    {"upperarm_l", "upper_arm_l"}, {"lowerarm_l", "forearm_l"}, {"hand_l", ""},
    {"thigh_l", "thigh_l"}, {"calf_l", "calf_l"}, {"foot_l", ""},
    {"upperarm_r", "upper_arm_r"}, {"lowerarm_r", "forearm_r"}, {"hand_r", ""},
    {"thigh_r", "thigh_r"}, {"calf_r", "calf_r"}, {"foot_r", ""},
};

Engine::Asset::AssetCatalog LoadCatalog() {
    const auto base = Engine::Platform::Sdl::BaseDirectory();
    if (!base) throw std::runtime_error("Cannot locate the PvP product directory");
    const auto root = base.value() / Gyo::AppConfig::Assets;
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

double Span(Vec3 a, Vec3 b) {
    const double x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
    return std::sqrt(x * x + y * y + z * z);
}

double DistanceToSegment(Vec3 p, Vec3 a, Vec3 b) {
    const double abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
    const double length2 = abx * abx + aby * aby + abz * abz;
    double t = length2 > 0 ? ((p.x - a.x) * abx + (p.y - a.y) * aby + (p.z - a.z) * abz) / length2 : 0;
    t = std::clamp(t, 0.0, 1.0);
    return Span(p, {static_cast<float>(a.x + abx * t), static_cast<float>(a.y + aby * t),
                        static_cast<float>(a.z + abz * t)});
}

class Survey final {
public:
    explicit Survey(const PlayerPresentationDefinition& definition)
        : definition_(definition), model_(*definition.character->model) {}

    // 1. Each node the regions need is present exactly once.
    nlohmann::json Nodes() const {
        nlohmann::json out = nlohmann::json::object();
        for (const auto& region : Regions)
            for (const char* name : {region.start, region.end}) out[name] = Count(name);
        for (const auto& [name, region] : RegionOfNode) out[name] = Count(name.c_str());
        return out;
    }
    bool NodesUnique() const {
        for (const auto& [name, count] : Nodes().items())
            if (count.get<int>() != 1) return false;
        return true;
    }

    struct Shape final { Vec3 a, b; double radius{}, radiusMax{}; std::size_t vertices{}; };

    // 2. The fixed pose of one stance: endpoints from the joints, a head sphere
    // at its vertex centroid, radii from the skinned vertices (95th percentile).
    std::map<std::string, Shape> Measure(bool crouched) const {
        const auto pose = Pose(Base(crouched), {});
        std::map<std::string, std::vector<Vec3>> points;
        for (std::size_t mesh = 0; mesh < model_.meshes.size(); ++mesh) {
            std::vector<Engine::Model::SkinnedVertex> skinned;
            if (!Engine::Model::SkinMesh(model_, mesh, pose.body, skinned))
                throw std::runtime_error("SkinMesh failed");
            const auto& part = model_.meshes[mesh];
            for (std::size_t i = 0; i < part.vertices.size(); ++i) {
                const auto& vertex = part.vertices[i];
                const auto dominant = std::max_element(vertex.weights.begin(), vertex.weights.end()) - vertex.weights.begin();
                if (vertex.weights[dominant] <= 0 || vertex.joints[dominant] >= part.joints.size()) continue;
                const auto region = RegionOf(part.joints[vertex.joints[dominant]].nodeIndex);
                if (region) points[*region].push_back(Body(skinned[i].position));
            }
        }
        std::map<std::string, Shape> shapes;
        for (const auto& region : Regions) {
            Shape shape;
            const auto& own = points[region.id];
            if (std::string(region.start) == region.end) {
                Vec3 sum{};
                for (const auto& p : own) sum = {sum.x + p.x, sum.y + p.y, sum.z + p.z};
                const auto n = static_cast<float>(std::max<std::size_t>(own.size(), 1));
                shape.a = shape.b = {sum.x / n, sum.y / n, sum.z / n};
            } else {
                shape.a = Joint(pose, region.start);
                shape.b = Joint(pose, region.end);
            }
            std::vector<double> distances;
            for (const auto& p : own) distances.push_back(DistanceToSegment(p, shape.a, shape.b));
            std::sort(distances.begin(), distances.end());
            shape.vertices = distances.size();
            if (!distances.empty()) {
                shape.radius = distances[std::min(distances.size() - 1, distances.size() * 95 / 100)];
                shape.radiusMax = distances.back();
            }
            shapes[region.id] = shape;
        }
        return shapes;
    }

    // 3. How far each region's endpoints move from the fixed pose over a clip.
    struct Variant final { std::string name; bool crouched{}; PlayerLocomotionState state; PlayerActionPose actions; };
    std::vector<Variant> Variants() const {
        std::vector<Variant> out;
        const auto sampleCount = 60;
        const auto clipSeconds = [&](std::size_t clip) { return model_.clips.at(clip).durationSeconds; };
        for (int i = 0; i < sampleCount; ++i) {
            const double t = static_cast<double>(i) / sampleCount;
            for (const auto& [name, jog, crouched] : {std::tuple{"walk", 0.0, false}, std::tuple{"jog", 1.0, false},
                                                      std::tuple{"crouch_move", 0.0, true}}) {
                auto state = Base(crouched);
                state.moveWeight = 1;
                state.jogWeight = jog;
                state.phaseCycles = t;
                out.push_back({name, crouched, state, {}});
            }
            for (const auto& [name, lower, clip] : {std::tuple{"jump_start", PlayerLowerAction::JumpStart, definition_.jumpStartClip},
                                                    std::tuple{"jump_loop", PlayerLowerAction::JumpLoop, definition_.jumpLoopClip},
                                                    std::tuple{"jump_land", PlayerLowerAction::JumpLand, definition_.jumpLandClip}}) {
                PlayerActionPose actions;
                actions.lower = lower;
                actions.lowerClipSeconds = t * clipSeconds(clip);
                out.push_back({name, false, Base(false), actions});
            }
            for (const auto& [name, upper, clip] : {std::tuple{"shoot", PlayerUpperAction::Shoot, definition_.shootClip},
                                                    std::tuple{"reload", PlayerUpperAction::Reload, definition_.reloadClip},
                                                    std::tuple{"hit", PlayerUpperAction::Hit, definition_.hitClip}}) {
                for (const bool crouched : {false, true}) {
                    PlayerActionPose actions;
                    actions.upper = upper;
                    actions.upperClipSeconds = t * clipSeconds(clip);
                    out.push_back({std::string(name) + (crouched ? "_crouched" : ""), crouched, Base(crouched), actions});
                }
            }
        }
        for (const auto& [name, weight] : {std::pair{"aim_up", -1.0F}, std::pair{"aim_down", 1.0F}})
            for (const bool crouched : {false, true}) {
                PlayerActionPose actions;
                actions.aimWeight = weight;
                out.push_back({std::string(name) + (crouched ? "_crouched" : ""), crouched, Base(crouched), actions});
            }
        return out;
    }
    nlohmann::json Deviations(const std::map<std::string, Shape>& standing,
                              const std::map<std::string, Shape>& crouching) const {
        std::map<std::string, std::map<std::string, double>> worst;
        for (const auto& variant : Variants()) {
            const auto pose = Pose(variant.state, variant.actions);
            const auto& fixed = variant.crouched ? crouching : standing;
            for (const auto& region : Regions) {
                if (std::string(region.start) == region.end) continue; // the head sphere is a vertex centroid
                const auto a = Joint(pose, region.start), b = Joint(pose, region.end);
                const auto& reference = fixed.at(region.id);
                auto& value = worst[variant.name][region.id];
                value = std::max({value, Span(a, reference.a), Span(b, reference.b)});
            }
        }
        nlohmann::json out = nlohmann::json::object();
        for (const auto& [variant, regions] : worst)
            for (const auto& [region, value] : regions) out[variant][region] = value;
        return out;
    }

    // 4. Frontal projection (the x-y plane): the area of the 11 regions' union
    // over the area of the current single capsule (radius 0.25, height 1.8).
    nlohmann::json Coverage(const std::map<std::string, Shape>& standing) const {
        constexpr double Cell = 0.0025, Radius = 0.25, Height = 1.8;
        const auto inStadium = [](double x, double y, double ax, double ay, double bx, double by, double r) {
            const double dx = bx - ax, dy = by - ay, length2 = dx * dx + dy * dy;
            double t = length2 > 0 ? ((x - ax) * dx + (y - ay) * dy) / length2 : 0;
            t = std::clamp(t, 0.0, 1.0);
            const double px = ax + dx * t - x, py = ay + dy * t - y;
            return px * px + py * py <= r * r;
        };
        std::size_t capsule{}, regions{}, both{};
        for (double x = -1.0; x <= 1.0; x += Cell)
            for (double y = -0.2; y <= 2.2; y += Cell) {
                const bool inCapsule = inStadium(x, y, 0, Radius, 0, Height - Radius, Radius);
                bool inRegions = false;
                for (const auto& [id, shape] : standing)
                    inRegions = inRegions || inStadium(x, y, shape.a.x, shape.a.y, shape.b.x, shape.b.y, shape.radius);
                capsule += inCapsule;
                regions += inRegions;
                both += inCapsule && inRegions;
            }
        return {{"capsule_area_m2", capsule * Cell * Cell}, {"union_area_m2", regions * Cell * Cell},
                {"coverage", static_cast<double>(regions) / static_cast<double>(capsule)},
                {"union_inside_capsule_over_capsule", static_cast<double>(both) / static_cast<double>(capsule)},
                {"cell_m", Cell}};
    }

    // 5. Three parts (v8-product-hit-rules-three-parts, measured as declared in
    // v8-product-hitbox-three-part-survey-method): the head sphere carried by the
    // Head bone, the legs' moving envelope, and the body's idle and moving shapes.
    nlohmann::json ThreeParts(const std::map<std::string, Shape>& standing,
                              const std::map<std::string, Shape>& crouching) const {
        const auto isMoving = [](const std::string& name) { return name == "walk" || name == "jog" || name == "crouch_move"; };
        const auto isJump = [](const std::string& name) { return name.rfind("jump_", 0) == 0; };
        const std::array<const char*, 4> legs{"thigh_l", "calf_l", "thigh_r", "calf_r"};
        const std::array<const char*, 2> body{"torso", "pelvis"};
        // The head centre in the Head bone's frame, per stance.
        std::map<bool, Vec3> headLocal;
        for (const bool crouched : {false, true}) {
            const auto& center = (crouched ? crouching : standing).at("head").a;
            const auto pose = Pose(Base(crouched), {});
            const auto inverse = Engine::Math::Inverse(pose.body.globalTransforms.at(Node("Head")));
            if (!inverse) throw std::runtime_error("Head transform is not invertible");
            headLocal[crouched] = Engine::Math::TransformPoint(*inverse, Model(center));
        }
        struct Sample final { std::string name; bool crouched; std::map<std::string, std::pair<Vec3, Vec3>> segments; Vec3 head; };
        std::vector<Sample> samples;
        for (const auto& variant : Variants()) {
            const auto pose = Pose(variant.state, variant.actions);
            Sample sample{variant.name, variant.crouched, {}, {}};
            for (const auto& region : Regions)
                if (std::string(region.start) != region.end)
                    sample.segments[region.id] = {Joint(pose, region.start), Joint(pose, region.end)};
            sample.head = Body(Engine::Math::TransformPoint(pose.body.globalTransforms.at(Node("Head")), headLocal[variant.crouched]));
            samples.push_back(std::move(sample));
        }
        nlohmann::json out = nlohmann::json::object();
        // (a) Head drift per variant.
        std::map<std::string, double> headDrift;
        for (const auto& sample : samples) {
            const auto& fixed = (sample.crouched ? crouching : standing).at("head").a;
            auto& value = headDrift[sample.name];
            value = std::max(value, Span(sample.head, fixed));
        }
        double headWorst{};
        for (const auto& [name, value] : headDrift) { out["head_drift_m"][name] = value; headWorst = std::max(headWorst, value); }
        out["head_radius_m"] = standing.at("head").radius;
        out["head_drift_max_m"] = headWorst;
        // (b) Legs: the moving envelope (an axis-aligned box in body coordinates)
        // against the area the four leg segments actually sweep, frontal projection.
        constexpr double Cell = 0.0025;
        for (const bool crouched : {false, true}) {
            const auto& fixed = crouched ? crouching : standing;
            Vec3 low{1e9F, 1e9F, 1e9F}, high{-1e9F, -1e9F, -1e9F};
            std::vector<const Sample*> moving;
            for (const auto& sample : samples)
                if (sample.crouched == crouched && isMoving(sample.name)) moving.push_back(&sample);
            for (const auto* sample : moving)
                for (const char* leg : legs) {
                    const auto r = static_cast<float>(fixed.at(leg).radius);
                    for (const auto& p : {sample->segments.at(leg).first, sample->segments.at(leg).second}) {
                        low = {std::min(low.x, p.x - r), std::min(low.y, p.y - r), std::min(low.z, p.z - r)};
                        high = {std::max(high.x, p.x + r), std::max(high.y, p.y + r), std::max(high.z, p.z + r)};
                    }
                }
            std::size_t swept{};
            for (double x = low.x; x <= high.x; x += Cell)
                for (double y = low.y; y <= high.y; y += Cell) {
                    bool in = false;
                    for (const auto* sample : moving) {
                        for (const char* leg : legs) {
                            const auto& [a, b] = sample->segments.at(leg);
                            const double r = fixed.at(leg).radius;
                            const double dx = b.x - a.x, dy = b.y - a.y, length2 = dx * dx + dy * dy;
                            double t = length2 > 0 ? ((x - a.x) * dx + (y - a.y) * dy) / length2 : 0;
                            t = std::clamp(t, 0.0, 1.0);
                            const double px = a.x + dx * t - x, py = a.y + dy * t - y;
                            if (px * px + py * py <= r * r) { in = true; break; }
                        }
                        if (in) break;
                    }
                    swept += in;
                }
            const double envelope = (high.x - low.x) * (high.y - low.y), sweptArea = swept * Cell * Cell;
            const auto key = crouched ? "legs_crouched" : "legs_standing";
            out[key] = {{"envelope_min", {low.x, low.y, low.z}}, {"envelope_max", {high.x, high.y, high.z}},
                {"envelope_frontal_m2", envelope}, {"swept_frontal_m2", sweptArea},
                {"envelope_over_swept", envelope / sweptArea}, {"moving_samples", moving.size()}};
            if (!crouched) {
                // Jumps: how many jump-sample leg endpoints fall outside the standing envelope.
                std::size_t total{}, outside{};
                for (const auto& sample : samples) {
                    if (sample.crouched || !isJump(sample.name)) continue;
                    for (const char* leg : legs)
                        for (const auto& p : {sample.segments.at(leg).first, sample.segments.at(leg).second}) {
                            ++total;
                            outside += p.x < low.x || p.x > high.x || p.y < low.y || p.y > high.y || p.z < low.z || p.z > high.z;
                        }
                }
                out["jump_leg_endpoints_outside_standing_envelope"] = {{"outside", outside}, {"total", total}};
            }
        }
        // (c) Body: idle shapes (the fixed pose) and moving shapes (the mean of the
        // moving samples' endpoints); the residual deviation, reported only.
        for (const bool crouched : {false, true}) {
            const auto& fixed = crouched ? crouching : standing;
            std::map<std::string, std::pair<Vec3, Vec3>> mean;
            std::size_t count{};
            for (const auto& sample : samples) {
                if (sample.crouched != crouched || !isMoving(sample.name)) continue;
                ++count;
                for (const char* part : body) {
                    auto& [a, b] = mean[part];
                    const auto& [sa, sb] = sample.segments.at(part);
                    a = {a.x + sa.x, a.y + sa.y, a.z + sa.z};
                    b = {b.x + sb.x, b.y + sb.y, b.z + sb.z};
                }
            }
            for (auto& [part, ends] : mean) {
                const auto n = static_cast<float>(count);
                ends.first = {ends.first.x / n, ends.first.y / n, ends.first.z / n};
                ends.second = {ends.second.x / n, ends.second.y / n, ends.second.z / n};
                out[crouched ? "body_moving_crouched" : "body_moving_standing"][part] = {
                    {"a", {ends.first.x, ends.first.y, ends.first.z}}, {"b", {ends.second.x, ends.second.y, ends.second.z}},
                    {"radius_p95", fixed.at(part).radius}};
            }
            std::map<std::string, std::map<std::string, double>> residual;
            for (const auto& sample : samples) {
                if (sample.crouched != crouched) continue;
                for (const char* part : body) {
                    const auto reference = isMoving(sample.name) ? mean.at(part)
                        : std::pair{fixed.at(part).a, fixed.at(part).b};
                    const auto& [a, b] = sample.segments.at(part);
                    auto& value = residual[sample.name][part];
                    value = std::max({value, Span(a, reference.first), Span(b, reference.second)});
                }
            }
            for (const auto& [name, parts] : residual)
                for (const auto& [part, value] : parts) out["body_residual_m"][name][part] = value;
        }
        return out;
    }

private:
    Vec3 Model(Vec3 body) const {
        const auto s = definition_.scale;
        return {body.x / s + definition_.anchor.x, body.y / s + definition_.anchor.y, body.z / s + definition_.anchor.z};
    }
    int Count(const char* name) const {
        return static_cast<int>(std::count_if(model_.nodes.begin(), model_.nodes.end(),
            [&](const auto& node) { return node.name == name; }));
    }
    std::size_t Node(const char* name) const {
        const auto found = std::find_if(model_.nodes.begin(), model_.nodes.end(), [&](const auto& node) { return node.name == name; });
        if (found == model_.nodes.end()) throw std::runtime_error(std::string("missing node ") + name);
        return static_cast<std::size_t>(found - model_.nodes.begin());
    }
    std::optional<std::string> RegionOf(std::size_t node) const {
        for (std::optional<std::size_t> at = node; at; at = model_.nodes[*at].parentIndex) {
            const auto found = RegionOfNode.find(model_.nodes[*at].name);
            if (found != RegionOfNode.end())
                return found->second.empty() ? std::nullopt : std::optional<std::string>(found->second);
        }
        return std::nullopt;
    }
    Vec3 Body(Vec3 model) const {
        const auto s = definition_.scale;
        return {(model.x - definition_.anchor.x) * s, (model.y - definition_.anchor.y) * s, (model.z - definition_.anchor.z) * s};
    }
    Vec3 Joint(const PlayerPresentationPose& pose, const char* name) const {
        return Body(Engine::Math::TransformPoint(pose.body.globalTransforms.at(Node(name)), Vec3{}));
    }
    static PlayerLocomotionState Base(bool crouched) {
        PlayerLocomotionState state;
        state.initialized = true;
        state.crouched = crouched;
        return state;
    }
    PlayerPresentationPose Pose(const PlayerLocomotionState& state, const PlayerActionPose& actions) const {
        PlayerPresentationPose pose;
        std::string error;
        if (!SamplePlayerPresentationPose(definition_, state, actions, pose, error)) throw std::runtime_error(error);
        return pose;
    }

    const PlayerPresentationDefinition& definition_;
    const Engine::Model::ModelAsset& model_;
};

nlohmann::json ShapesJson(const std::map<std::string, Survey::Shape>& shapes) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& region : Regions) {
        const auto& shape = shapes.at(region.id);
        out[region.id] = {{"part", region.part}, {"a", {shape.a.x, shape.a.y, shape.a.z}}, {"b", {shape.b.x, shape.b.y, shape.b.z}},
            {"radius_p95", shape.radius}, {"radius_max", shape.radiusMax}, {"vertices", shape.vertices},
            {"top_y", std::max(shape.a.y, shape.b.y) + shape.radius}};
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string out;
        bool smoke = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--smoke") smoke = true;
            else if (arg == "--out" && i + 1 < argc) out = argv[++i];
            else throw std::invalid_argument("usage: [--smoke] [--out report.json]");
        }
        Assets fixture;
        std::string error;
        const auto definition = LoadPlayerPresentationDefinition(fixture.assets, 1.8F, error);
        if (!definition) throw std::runtime_error(error);
        Survey survey(*definition);
        // The smoke check (CTest) only proves the regions can be measured on the
        // production player: every node present once. It judges nothing else.
        if (!survey.NodesUnique()) {
            std::cerr << "hitbox survey: a region node is missing or not unique: " << survey.Nodes().dump() << '\n';
            return 1;
        }
        if (smoke) { std::cout << "hitbox survey: region nodes unique\n"; return 0; }
        const auto standing = survey.Measure(false), crouching = survey.Measure(true);
        const nlohmann::json report{
            {"version", 1},
            {"body_height", definition->bodyHeight}, {"scale", definition->scale},
            {"nodes", survey.Nodes()},
            {"standing", ShapesJson(standing)}, {"crouched", ShapesJson(crouching)},
            {"deviation_m", survey.Deviations(standing, crouching)},
            {"coverage_standing_frontal", survey.Coverage(standing)},
            {"three_parts", survey.ThreeParts(standing, crouching)},
        };
        if (out.empty()) std::cout << report.dump(1) << '\n';
        else std::ofstream(out) << report.dump(1) << '\n';
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "hitbox survey: " << failure.what() << '\n';
        return 2;
    }
}
