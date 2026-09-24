#include <doctest/doctest.h>
#include "TestAssets.hpp"
#include "RetroFPS/App/EnemyPresentationDefinition.hpp"
#include "RetroFPS/App/CharacterPresentationDefinition.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Collision/CombatCollision.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "model/backend/ufbx/UfbxModelLoader.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>
#include <span>
#include <string_view>

using namespace fps;
using namespace fps::tests;

namespace {
// Keep malformed JSON in an isolated pipeline. All other bytes come from the
// deployed v2 catalog, and the shared production application is never mutated.
class EnemyDefinitionAssetSource final : public Engine::Asset::Loading::IAssetSource {
public:
    std::string overridePath;
    std::string overrideText;

    Engine::Base::Result<Engine::Asset::Loading::ByteBuffer, Engine::Asset::AssetError>
    ReadAll(std::string_view path) override {
        if (path == overridePath) {
            const auto bytes = std::as_bytes(std::span(overrideText.data(), overrideText.size()));
            return Engine::Base::Result<Engine::Asset::Loading::ByteBuffer, Engine::Asset::AssetError>::Ok(
                Engine::Asset::Loading::ByteBuffer(bytes.begin(), bytes.end()));
        }
        Engine::Asset::Loading::NativeFileAssetSource native;
        return native.ReadAll(path);
    }
};

struct EnemyDefinitionFixture final {
    Engine::Asset::AssetCatalog catalog = ProductionApplication().Catalog();
    Engine::Asset::Loading::LoaderRegistry registry;
    EnemyDefinitionAssetSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, registry};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy policy{{}};
    Engine::Asset::AssetManager assets{catalog, pipeline, storage, lifetime, policy};

    EnemyDefinitionFixture() {
        if (!registry.Register(std::make_unique<Engine::Asset::Loaders::TextLoader>()) ||
            !registry.Register(std::make_unique<Engine::Asset::Loaders::SdlImage::SdlImageTextureLoader>()) ||
            !registry.Register(std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>()))
            throw std::runtime_error("enemy definition fixture loaders failed to register");
    }

    void Override(const EnemyDefinition& enemy, std::string regions) {
        const auto* entry = catalog.Find(enemy.presentationAssetId);
        if (!entry) throw std::runtime_error("enemy definition fixture asset is absent");
        source.overridePath = entry->resolvedPath;
        source.overrideText = R"({"version":1,"character_asset_id":"object_fps_v2.enemy.melee.character",
            "hurt_regions":[)" + std::move(regions) + R"(],"attack":{"point":{"node":"hand_l"},
            "radius":0.12,"begin_seconds":0.1,"end_seconds":0.2}})";
        auto request = Engine::Asset::AssetRequest::WithTypeHint(Engine::Asset::AssetType::Text());
        request.mode = Engine::Asset::AssetRequest::Mode::ForceReload;
        const auto loaded = assets.Load(enemy.presentationAssetId, request);
        if (!loaded) throw std::runtime_error(loaded.error().message);
        assets.Release(loaded.value());
    }
};

std::string HeadRegion(std::string_view multiplier = {}) {
    std::string result = R"({"id":"head","start":{"node":"Head"},"end":{"node":"Head"},"radius":0.16)";
    if (!multiplier.empty()) result += ",\"damage_multiplier\":" + std::string(multiplier);
    return result + '}';
}
}

TEST_CASE("v2 deployed campaign resolves two complete 1.6 metre skeletal enemies") {
    const auto& content=*ProductionApplication().Content();
    REQUIRE_FALSE(content.Stages().empty());
    const auto& enemies=content.Data().enemies.GetDefinitions();
    REQUIRE(enemies.size()==2);
    for(const auto& enemy:enemies) {
        REQUIRE(enemy.rig);
        const auto& rig=*enemy.rig;
        CHECK(enemy.hitboxHeight==doctest::Approx(1.6F));
        REQUIRE(rig.hurtRegions.size()==11);
        for (const std::string_view id : {"head", "torso", "pelvis", "upper_arm_l", "forearm_l",
                "thigh_l", "calf_l", "upper_arm_r", "forearm_r", "thigh_r", "calf_r"}) {
            const auto region = std::find_if(rig.hurtRegions.begin(), rig.hurtRegions.end(),
                [&](const auto& candidate) { return candidate.id == id; });
            REQUIRE(region != rig.hurtRegions.end());
            const float expected = id == "head" ? 2.0F : id == "torso" || id == "pelvis" ? 1.0F : 0.75F;
            CHECK(region->damageMultiplier == doctest::Approx(expected));
        }
        CHECK(rig.model->clips[rig.clips[0]].name==(enemy.kind==EnemyKind::Melee?
            "Armature|Idle_Loop":"Armature|Pistol_Idle_Loop"));
        CHECK(rig.model->clips[rig.clips[1]].name=="Armature|Jog_Fwd_Loop");
        CHECK(rig.model->clips[rig.clips[2]].name==(enemy.kind==EnemyKind::Melee?
            "Armature|Punch_Jab":"Armature|Pistol_Shoot"));
        CHECK(rig.weapon.has_value()==(enemy.kind==EnemyKind::Ranged));
        CHECK(rig.model->clips[rig.clips[3]].name=="Armature|Death01");
        CHECK(rig.model->clips[rig.clips[2]].durationSeconds<=enemy.attackIntervalSeconds);
        CHECK(enemy.attackIntervalSeconds==doctest::Approx(enemy.kind==EnemyKind::Melee?0.9:1.25));
        Engine::Model::Pose reference;
        REQUIRE(Engine::Model::MakeDefaultPose(*rig.model,reference));
        std::vector<Engine::Model::SkinnedVertex> vertices;
        float minimum=std::numeric_limits<float>::max(),maximum=-minimum;
        for(std::size_t mesh=0;mesh<rig.model->meshes.size();++mesh) {
            REQUIRE(Engine::Model::SkinMesh(*rig.model,mesh,reference,vertices));
            for(const auto& vertex:vertices) {
                const float y=(vertex.position.y-rig.anchor.y)*rig.scale;
                minimum=std::min(minimum,y);maximum=std::max(maximum,y);
            }
        }
        CHECK(minimum==doctest::Approx(0).epsilon(1e-5));
        CHECK(maximum==doctest::Approx(1.6).epsilon(1e-5));
    }
}

TEST_CASE("enemy hurt region data defaults old multipliers and rejects ambiguous or invalid damage data") {
    const auto& definitions = ProductionApplication().Content()->Data().enemies.GetDefinitions();
    const auto found = std::find_if(definitions.begin(), definitions.end(),
        [](const auto& enemy) { return enemy.kind == EnemyKind::Melee; });
    REQUIRE(found != definitions.end());
    const auto& enemy = *found;
    EnemyDefinitionFixture fixture;
    std::string error;

    fixture.Override(enemy, HeadRegion());
    const auto legacy = LoadEnemyRig(fixture.assets, enemy, error);
    REQUIRE_MESSAGE(legacy, error);
    REQUIRE(legacy->hurtRegions.size() == 1);
    CHECK(legacy->hurtRegions.front().damageMultiplier == 1.0F);

    fixture.Override(enemy, HeadRegion("2.5"));
    const auto configured = LoadEnemyRig(fixture.assets, enemy, error);
    REQUIRE_MESSAGE(configured, error);
    CHECK(configured->hurtRegions.front().damageMultiplier == 2.5F);

    // Both exponents are valid JSON/double input, but cannot be stored as a
    // finite positive float. Exercise value validation, not JSON syntax errors.
    for (const std::string_view invalid : {"0", "-1", "1e100", "1e-100", "\"2\"", "true", "null", "[]", "{}"}) {
        CAPTURE(invalid);
        fixture.Override(enemy, HeadRegion(invalid));
        CHECK_FALSE(LoadEnemyRig(fixture.assets, enemy, error));
        CHECK(error.find(enemy.id) != std::string::npos);
        CHECK(error.find(enemy.presentationAssetId.debugName) != std::string::npos);
        CHECK(error.find("head") != std::string::npos);
        CHECK(error.find("damage_multiplier") != std::string::npos);
    }

    fixture.Override(enemy, HeadRegion("2") + ',' + HeadRegion("1"));
    CHECK_FALSE(LoadEnemyRig(fixture.assets, enemy, error));
    CHECK(error.find(enemy.id) != std::string::npos);
    CHECK(error.find("head") != std::string::npos);
    CHECK(error.find("duplicate") != std::string::npos);
    // The previous successful rig is immutable and unaffected by later loads.
    CHECK(configured->hurtRegions.front().damageMultiplier == 2.5F);
}

TEST_CASE("production skeletal hurtboxes track sampled bones with the rendered instance transform") {
    const auto& enemy=ProductionApplication().Content()->Data().enemies.GetDefinitions().front();
    const auto& rig=*enemy.rig;
    Engine::Model::Pose idle,attack;
    REQUIRE(Engine::Model::SamplePose(*rig.model,rig.clips[0],0,Engine::Model::PlaybackMode::Loop,idle));
    REQUIRE(Engine::Model::SamplePose(*rig.model,rig.clips[2],0.25,Engine::Model::PlaybackMode::Clamp,attack));
    const auto before=BuildEnemyHurtboxes(rig,idle,{3,4},0.7F);
    const auto after=BuildEnemyHurtboxes(rig,attack,{3,4},0.7F);
    REQUIRE(after.size()==rig.hurtRegions.size());
    bool moved=false;
    for(std::size_t i=0;i<after.size();++i) {
        const auto& binding=rig.hurtRegions[i].start;
        const auto bone=Engine::Model::TransformPoint(attack.globalTransforms[binding.node],binding.offset);
        const auto world=Engine::Model::ToMatrix({{3,0,4},{0,std::sin(0.35F),0,std::cos(0.35F)},
            {rig.scale,rig.scale,rig.scale}});
        const auto expected=Engine::Model::TransformPoint(world,
            {bone.x-rig.anchor.x,bone.y-rig.anchor.y,bone.z-rig.anchor.z});
        CHECK(after[i].shape.segmentStart.x==doctest::Approx(expected.x));
        CHECK(after[i].shape.segmentStart.y==doctest::Approx(expected.y));
        CHECK(after[i].shape.segmentStart.z==doctest::Approx(expected.z));
        CHECK(after[i].shape.radius==doctest::Approx(rig.hurtRegions[i].radius*rig.scale));
        const auto a=before[i].shape.segmentStart,b=after[i].shape.segmentStart;
        moved=moved||std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>0.01F;
    }
    CHECK(moved);
}

TEST_CASE("human enemy animation assembly preserves character geometry and cached reference models") {
    auto& application=ProductionApplication();
    for(const auto& enemy:application.Content()->Data().enemies.GetDefinitions()) {
        const auto& rig=*enemy.rig;
        const std::string character=enemy.kind==EnemyKind::Melee?"superhero_male":"superhero_female";
        std::string error;
        const auto source=LoadCharacterPresentationDefinition(application.Assets(),
            Engine::Asset::AssetId::FromString("object_fps_v2.character."+character),error);
        REQUIRE_MESSAGE(source,error);
        CHECK(source->model!=rig.model);
        CHECK(source->model->clips.empty());
        REQUIRE(source->model->meshes.size()==rig.model->meshes.size());
        REQUIRE(source->model->nodes.size()==rig.model->nodes.size());
        REQUIRE(rig.model->materials.size()==3);
        CHECK(rig.model->materials.back().name.find(enemy.kind==EnemyKind::Melee?
            "Male":"Female")!=std::string::npos);
        Engine::Model::Pose sourcePose,reference;
        REQUIRE(Engine::Model::MakeDefaultPose(*source->model,sourcePose));
        REQUIRE(Engine::Model::MakeDefaultPose(*rig.model,reference));
        for(std::size_t node=0;node<reference.globalTransforms.size();++node)
            CHECK(reference.globalTransforms[node].values==sourcePose.globalTransforms[node].values);
        std::vector<Engine::Model::SkinnedVertex> before,after;
        for(std::size_t mesh=0;mesh<rig.model->meshes.size();++mesh) {
            REQUIRE(Engine::Model::SkinMesh(*source->model,mesh,sourcePose,before));
            REQUIRE(Engine::Model::SkinMesh(*rig.model,mesh,reference,after));
            REQUIRE(before.size()==after.size());
            for(std::size_t v=0;v<before.size();++v) {
                CHECK(before[v].position.x==after[v].position.x);
                CHECK(before[v].position.y==after[v].position.y);
                CHECK(before[v].position.z==after[v].position.z);
            }
        }
        for(auto clip:rig.clips) {
            for(double time:{0.0,rig.model->clips[clip].durationSeconds*0.5}) {
                Engine::Model::Pose pose;
                REQUIRE(Engine::Model::SamplePose(*rig.model,clip,time,Engine::Model::PlaybackMode::Clamp,pose));
                for(const auto& matrix:pose.globalTransforms)
                    for(float value:matrix.values) CHECK(std::isfinite(value));
            }
        }
    }
}

TEST_CASE("head hurtbox covers the visible upper skull rather than just the neck joint") {
    for(const auto& enemy:ProductionApplication().Content()->Data().enemies.GetDefinitions()) {
        const auto& rig=*enemy.rig;
        const auto head=rig.model->FindNode("Head");REQUIRE(head);
        Engine::Model::Pose reference;REQUIRE(Engine::Model::MakeDefaultPose(*rig.model,reference));
        float top=-std::numeric_limits<float>::max(),minimumZ=std::numeric_limits<float>::max(),maximumZ=-minimumZ;
        std::size_t headVertices=0;
        for(std::size_t mesh=0;mesh<rig.model->meshes.size();++mesh) {
            std::vector<Engine::Model::SkinnedVertex> vertices;
            REQUIRE(Engine::Model::SkinMesh(*rig.model,mesh,reference,vertices));
            const auto& source=rig.model->meshes[mesh];
            for(std::size_t v=0;v<source.vertices.size();++v) {
                float headWeight=0;
                for(std::size_t w=0;w<4;++w) if(source.vertices[v].weights[w]>0&&
                    source.joints[source.vertices[v].joints[w]].nodeIndex==*head)
                    headWeight+=source.vertices[v].weights[w];
                if(headWeight<0.5F) continue;
                ++headVertices;top=std::max(top,vertices[v].position.y);
                minimumZ=std::min(minimumZ,vertices[v].position.z);maximumZ=std::max(maximumZ,vertices[v].position.z);
            }
        }
        REQUIRE(headVertices>0);
        const auto volumes=BuildEnemyHurtboxes(rig,reference,{},0);
        const auto shape=std::find_if(volumes.begin(),volumes.end(),[](const auto& box){return box.region=="head";});
        REQUIRE(shape!=volumes.end());
        const float skullY=(top-0.015F-rig.anchor.y)*rig.scale;
        const float skullZ=((minimumZ+maximumZ)*0.5F-rig.anchor.z)*rig.scale;
        const auto hit=Engine::Collision::RaycastCapsule({-1,skullY,skullZ},{1,0,0},2,shape->shape);
        REQUIRE(hit);
        CHECK(*hit<1.1F);
    }
}

TEST_CASE("invalid enemy presentation references and incompatible attack timing fail explicitly") {
    auto& application=ProductionApplication();
    auto enemy=application.Content()->Data().enemies.GetDefinitions().front();
    std::string error;
    enemy.presentationAssetId=Engine::Asset::AssetId::FromString("object_fps_v2.missing.enemy");
    CHECK_FALSE(LoadEnemyRig(application.Assets(),enemy,error));
    CHECK(error.find("object_fps_v2.missing.enemy")!=std::string::npos);
    enemy=application.Content()->Data().enemies.GetDefinitions().front();
    enemy.attackIntervalSeconds=0.01F;
    CHECK_FALSE(LoadEnemyRig(application.Assets(),enemy,error));
    CHECK(error.find("cooldown")!=std::string::npos);
}

TEST_CASE("production attack clips connect the melee fist and release from the attached pistol muzzle") {
    const auto parsed=GridMapLoader::Parse(
        "############\n#P........R#\n#..........#\n#..........#\n#..........#\n"
        "#..........#\n#..........#\n#..........#\n#M........D#\n############\n");
    REQUIRE_MESSAGE(parsed,parsed.error);
    const auto& map=*parsed.map;
    const auto definitions=ProductionApplication().Content()->Data().enemies.GetDefinitions();
    for(const auto& definition:definitions) {
        const bool melee=definition.kind==EnemyKind::Melee;
        const Float2 player{4.5F,melee?5.06F:7.5F},position{4.5F,4.5F};
        EnemySystem system;std::string error;
        REQUIRE_MESSAGE(system.Initialize(map,player,0.25F,1,{},error),error);
        const auto spawned=system.Spawn(map,player,0.25F,position,definition,error);
        REQUIRE_MESSAGE(spawned.Spawned(),error);
        std::vector<EnemyAttackEvent> events;
        for(int frame=0;frame<60;++frame) {
            system.Update(map,{player,0.25F,1.8F,0},1.0F/120);
            for(const auto& attack:system.GetAttackEvents()) events.push_back(attack);
        }
        REQUIRE(events.size()==1);
        CHECK(events.front().damage==definition.damage);
        CHECK(events.front().origin.z>position.z+0.1F);
        if(!melee) {
            const auto& rig=*definition.rig;
            REQUIRE(rig.weapon);
            CHECK(rig.model->nodes[rig.attackPoint.node].name=="hand_r");
            Engine::Model::AnimationInstance playerAnimation(rig.model);
            REQUIRE(playerAnimation.Play(rig.clips[0],Engine::Model::PlaybackMode::Loop));
            REQUIRE(playerAnimation.Play(rig.clips[2],Engine::Model::PlaybackMode::Clamp,rig.attackTransitionSeconds));
            REQUIRE(playerAnimation.Advance(rig.releaseSeconds));
            const auto expected=EnemyBoneWorldPoint(rig,playerAnimation.CurrentPose(),rig.attackPoint,position,0);
            CHECK(events.front().origin.x==doctest::Approx(expected.x).epsilon(1e-4));
            CHECK(events.front().origin.y==doctest::Approx(expected.y).epsilon(1e-4));
            CHECK(events.front().origin.z==doctest::Approx(expected.z).epsilon(1e-4));
        }
        REQUIRE(system.Kill(spawned.enemyId));
        REQUIRE(system.GetSnapshots().size()==1);
        CHECK(system.GetSnapshots().front().hurtboxes.empty());
        CHECK_FALSE(system.GetSnapshots().front().attackShape);
        const auto deathSeconds=static_cast<float>(definition.rig->model->clips[definition.rig->clips[3]].durationSeconds);
        system.Update(map,{player,0.25F,1.8F,0},deathSeconds+0.01F);
        CHECK(system.GetAttackEvents().empty());
        CHECK(system.RetireExpiredDead()==1);
        CHECK(system.GetSnapshots().empty());
    }
}

TEST_CASE("production campaign snapshots carry active geometry and pause preserves authoritative poses") {
    auto session=StartedSession();
    const auto initial=session.Snapshot();
    REQUIRE(initial.screen==GameScreen::Playing);
    REQUIRE(initial.activeStage);REQUIRE(initial.player);
    REQUIRE_FALSE(initial.enemies.empty());
    REQUIRE_FALSE(initial.worldCollisionBoxes.empty());
    for(const auto& enemy:initial.enemies) {
        REQUIRE_FALSE(enemy.pose.globalTransforms.empty());
        REQUIRE_FALSE(enemy.hurtboxes.empty());
        CHECK(enemy.body.radius==enemy.collisionRadius);
    }
    std::string error;
    const std::array<GameSessionCommand,1> pause{PauseCommand{}};
    REQUIRE_MESSAGE(session.Advance(0,{},pause,error),error);
    const auto paused=session.Snapshot();
    REQUIRE(paused.screen==GameScreen::Paused);
    REQUIRE_MESSAGE(session.Advance(10,{},{},error),error);
    const auto& unchanged=session.Snapshot();
    REQUIRE(unchanged.enemies.size()==paused.enemies.size());
    for(std::size_t i=0;i<paused.enemies.size();++i) {
        CHECK(unchanged.enemies[i].position.x==paused.enemies[i].position.x);
        CHECK(unchanged.enemies[i].position.z==paused.enemies[i].position.z);
        CHECK(unchanged.enemies[i].stateElapsedSeconds==paused.enemies[i].stateElapsedSeconds);
        for(std::size_t node=0;node<paused.enemies[i].pose.globalTransforms.size();++node)
            CHECK(unchanged.enemies[i].pose.globalTransforms[node].values==paused.enemies[i].pose.globalTransforms[node].values);
    }
    const std::array<GameSessionCommand,1> menu{ReturnToMainMenuCommand{}};
    REQUIRE_MESSAGE(session.Advance(0,{},menu,error),error);
    for(int frame=0;frame<4;++frame) REQUIRE_MESSAGE(session.Advance(0.016F,{},{},error),error);
    CHECK(session.Snapshot().screen==GameScreen::MainMenu);
    CHECK(session.Snapshot().enemies.empty());
    CHECK_FALSE(session.Snapshot().activeStage);
}
