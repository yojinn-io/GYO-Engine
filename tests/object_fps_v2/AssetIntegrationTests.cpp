#include <doctest/doctest.h>
#include "TestAssets.hpp"
#include "RetroFPS/App/EnemyPresentationDefinition.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Collision/CombatCollision.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace fps;
using namespace fps::tests;

TEST_CASE("v2 deployed campaign resolves two complete 1.6 metre skeletal enemies") {
    const auto& content=*ProductionApplication().Content();
    REQUIRE_FALSE(content.Stages().empty());
    const auto& enemies=content.Data().enemies.GetDefinitions();
    REQUIRE(enemies.size()==2);
    for(const auto& enemy:enemies) {
        REQUIRE(enemy.rig);
        const auto& rig=*enemy.rig;
        CHECK(enemy.hitboxHeight==doctest::Approx(1.6F));
        CHECK(rig.hurtRegions.size()>=11);
        CHECK(rig.model->clips[rig.clips[0]].name=="Armature|Idle_Loop");
        CHECK(rig.model->clips[rig.clips[1]].name=="Armature|Jog_Fwd_Loop");
        CHECK(rig.model->clips[rig.clips[2]].name==(enemy.kind==EnemyKind::Melee?
            "Armature|Punch_Jab":"Armature|Spell_Simple_Shoot"));
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

TEST_CASE("production attack clips connect the melee fist and release from the forward casting hand") {
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
            CHECK(rig.model->nodes[rig.attackPoint.node].name=="hand_l");
            Engine::Model::AnimationInstance playerAnimation(rig.model);
            REQUIRE(playerAnimation.Play(rig.clips[0],Engine::Model::PlaybackMode::Loop));
            REQUIRE(playerAnimation.Play(rig.clips[2],Engine::Model::PlaybackMode::Clamp,rig.transitionSeconds));
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
