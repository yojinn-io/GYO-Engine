#include "RetroFPS/App/ObjectFpsApplication.hpp"
#include "RetroFPS/App/ObjectFpsPresentation.hpp"
#include "RetroFPS/App/ObjectFpsRuntimeClient.hpp"
#include "RetroFPS/App/ObjectFpsUi.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Collision/CombatCollision.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "model/backend/ufbx/UfbxModelLoader.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/Renderer.hpp"
#include "ui/UiDocumentCodec.hpp"
#include "gyo/AppConfig.hpp"
#include "RigInspection.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
void Require(bool value,const std::string& error) { if(!value) throw std::runtime_error(error); }

void Inspect(const std::filesystem::path& root,const std::filesystem::path& output) {
    fps::ObjectFpsApplication application;
    std::string error;
    Require(application.InitializeContent(root,error),error);
    std::ofstream report;
    if(!output.empty()) {
        std::filesystem::create_directories(output);
        report.open(output/"rig-inspection.txt");
        Require(bool(report),"Cannot create rig inspection report");
    }
    auto& stream=output.empty()?std::cout:report;
    for(const auto& enemy:application.Content()->Data().enemies.GetDefinitions()) {
        stream<<"enemy="<<enemy.id<<"\n";
        InspectEnemyRig(*enemy.rig->model,stream);
    }
}

fps::ObjectFpsUi MakeUi(fps::ObjectFpsApplication& application) {
    auto& assets=application.Assets();
    auto loaded=assets.Load(Engine::Asset::AssetId::FromString("object_fps_v2.ui.screens"),
        Engine::Asset::AssetRequest::WithTypeHint(Engine::Asset::AssetType::Text()));
    Require(bool(loaded),loaded?"":loaded.error().message);
    auto text=assets.GetSharedConst<Engine::Asset::Loaders::TextAsset>(loaded.value());
    assets.Release(loaded.value());
    Require(bool(text),"Missing v2 UI text");
    auto parsed=Engine::Ui::UiDocumentCodec::Parse(text->text,"object_fps_v2.ui.screens");
    Require(bool(parsed),parsed?"":parsed.error().message);
    fps::ObjectFpsUi ui;std::string error;
    Require(ui.Initialize(std::make_shared<const Engine::Ui::UiDocument>(std::move(parsed).value()),error),error);
    return ui;
}

void Render(fps::ObjectFpsApplication& application,fps::ObjectFpsUi& ui,
    const fps::GameSessionSnapshot& snapshot,bool debug,const std::filesystem::path& capture) {
    fps::ObjectFpsDisplaySettings settings;settings.showCollisionVolumes=debug;
    std::string error;Engine::Ui::UiDrawList draw;
    Require(ui.Compose(snapshot,settings,{1280,720},draw,error),error);
    for(int attempt=0;attempt<180;++attempt) {
        Require(application.Platform().PumpEvents()!=Engine::Runtime::RuntimeControl::Stop,"Window closed before capture");
        if(!capture.empty()) application.Renderer().RequestSceneCapture();
        Require(application.Presentation().Present(snapshot,settings,draw,error),error);
        if(capture.empty()) return;
        auto pixels=application.Renderer().TakeSceneCapture();
        if(!pixels) {SDL_Delay(16);continue;}
        SDL_Surface* surface=SDL_CreateSurfaceFrom(static_cast<int>(pixels->width),static_cast<int>(pixels->height),
            SDL_PIXELFORMAT_RGBA32,pixels->rgba8.data(),static_cast<int>(pixels->width*4));
        Require(surface!=nullptr,SDL_GetError());
        const bool saved=SDL_SaveBMP(surface,capture.string().c_str());SDL_DestroySurface(surface);
        Require(saved,SDL_GetError());return;
    }
    throw std::runtime_error("No presented frame became available for capture");
}

fps::GameFrameInput AimAtEnemy(const fps::GameSessionSnapshot& snapshot,const fps::CampaignContent& content) {
    fps::GameFrameInput input;
    if(!snapshot.player||!snapshot.activeStage||snapshot.screen!=fps::GameScreen::Playing) return input;
    const auto& player=*snapshot.player;
    const fps::Float3 origin{player.position.x,player.feetY+player.eyeHeight,player.position.z};
    const auto& map=content.Stages()[snapshot.activeStage->ordinal].map;
    float nearest=std::numeric_limits<float>::max();fps::Float3 target{};bool found=false;
    for(const auto& enemy:snapshot.enemies) {
        if(enemy.state==fps::EnemyState::Dead) continue;
        for(const auto& region:enemy.hurtboxes) {
            if(region.region!="torso") continue;
            const auto& capsule=region.shape;
            const fps::Float3 point{(capsule.segmentStart.x+capsule.segmentEnd.x)*0.5F,
                (capsule.segmentStart.y+capsule.segmentEnd.y)*0.5F,(capsule.segmentStart.z+capsule.segmentEnd.z)*0.5F};
            const fps::Float3 delta{point.x-origin.x,point.y-origin.y,point.z-origin.z};
            const float distance=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
            if(distance>=nearest) continue;
            const auto blocking=fps::CombatCollision::Raycast(map,{},origin,delta,distance);
            if(blocking&&blocking->distance<distance-0.05F) continue;
            nearest=distance;target=point;found=true;
        }
    }
    if(found) {
        const float dx=target.x-origin.x,dz=target.z-origin.z;
        const float yaw=std::atan2(dx,dz),pitch=-std::atan2(target.y-origin.y,std::sqrt(dx*dx+dz*dz));
        input.lookEnabled=true;
        input.lookDeltaX=std::remainder(yaw-player.yawRadians,2*std::numbers::pi_v<float>)/0.0025F;
        // Published pitch contains the current weapon recoil. Compensate its
        // separately retained value to select the controller's aim pitch.
        const float aim=player.pitchRadians+snapshot.weapon.recoilDegrees*std::numbers::pi_v<float>/180.0F;
        input.lookDeltaY=(pitch-aim)/0.0025F;
        input.firePressed=true;input.fireHeld=true;
    }
    input.reloadPressed=snapshot.weapon.magazineAmmo==0&&!snapshot.weapon.reloading;
    return input;
}

void Wave(fps::ObjectFpsApplication& application,fps::ObjectFpsUi* ui,const std::filesystem::path& output) {
    fps::GameSession session;fps::GameSessionConfig config;
    config.fadeInSeconds=0.001F;config.fadeOutSeconds=0.001F;
    std::string error;Require(session.Initialize(application.Content(),config,error),error);
    const std::array<fps::GameSessionCommand,1> start{fps::StartCampaignCommand{}};
    Require(session.Advance(1.0F/60,{},start,error),error);
    std::size_t frames=0,seenPoses=0,shots=0;const auto begin=std::chrono::steady_clock::now();
    for(;frames<7200;++frames) {
        auto input=AimAtEnemy(session.Snapshot(),*application.Content());
        // Semiautomatic trigger presses alternate with a release, including
        // the initial campaign input latch and post-reload release gates.
        if(frames%2) {input.firePressed=false;input.fireHeld=false;}
        Require(session.Advance(1.0F/60,input,{},error),error);
        for(const auto& event:session.Events()) if(std::holds_alternative<fps::ShotEvent>(event.payload)) ++shots;
        const auto& snapshot=session.Snapshot();
        for(const auto& enemy:snapshot.enemies) {Require(!enemy.pose.globalTransforms.empty(),"Enemy pose is empty");++seenPoses;}
        if(ui&&frames%12==0) Render(application,*ui,snapshot,frames%120>=60,{});
        if(snapshot.campaignOutcome==fps::CampaignOutcome::PlayerDied) throw std::runtime_error(
            "Wave bot died before clearing first room: frame="+std::to_string(frames)+" shots="+std::to_string(shots)+
            " kills="+std::to_string(snapshot.campaignRooms.empty()?0:snapshot.campaignRooms.front().kills));
        if(!snapshot.activeStage) continue;
        const auto& level=application.Content()->Stages()[snapshot.activeStage->ordinal].definition;
        const std::size_t quota=static_cast<std::size_t>(level.meleeEnemyCount)+level.rangedEnemyCount;
        const auto room=std::find_if(snapshot.campaignRooms.begin(),snapshot.campaignRooms.end(),
            [&](const auto& stats){return stats.levelId==snapshot.activeStage->levelId;});
        // A door may unlock before the spawn quota is exhausted. Acceptance
        // waits for every scheduled enemy to die and its corpse to retire.
        if(room!=snapshot.campaignRooms.end()&&room->kills==quota&&snapshot.enemies.empty()) {
            Require(snapshot.activeStage->doorVisible,"Full enemy quota completed without unlocking the room");
            if(ui) {
                Render(application,*ui,snapshot,true,output.empty()?std::filesystem::path{}:output/"wave-cleared.bmp");
                const auto draws=application.Presentation().PreparedQueue().Meshes();
                Require(std::any_of(draws.begin(),draws.end(),[](const auto& draw) {
                    return draw.layer==Engine::Render::MeshLayer::ViewModel;
                }),"The equipped v2 weapon did not submit its ModelRenderer viewmodel");
                // This room uses textured world geometry; the mannequin's
                // untextured World meshes must all disappear after retirement.
                Require(std::none_of(draws.begin(),draws.end(),[](const auto& draw) {
                    return draw.layer==Engine::Render::MeshLayer::World&&!draw.material.texture.IsValid();
                }),"Retired enemy meshes remain in the prepared world queue");
            }
            const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
            std::cout<<"wave=cleared frames="<<frames+1<<" simulated_seconds="<<(frames+1)/60.0
                <<" kills="<<room->kills<<" quota="<<quota<<" remaining_enemy_instances="<<snapshot.enemies.size()
                <<" posed_enemies="<<seenPoses<<" shots="<<shots<<" elapsed_ms="<<milliseconds<<" health="<<snapshot.player->health<<"\n";
            return;
        }
    }
    throw std::runtime_error("Wave did not clear within 120 simulated seconds");
}

void Visuals(fps::ObjectFpsApplication& application,const std::filesystem::path& output) {
    auto ui=MakeUi(application);
    auto& client=application.Client();client.Submit(fps::StartCampaignCommand{});
    for(std::uint64_t frame=0;frame<5;++frame) {
        const Engine::Runtime::FrameContext context{frame,1.0/60};
        Require(client.ProcessEvents(context)!=Engine::Runtime::RuntimeControl::Stop,"Input stopped");
        Require(client.Update(context)!=Engine::Runtime::RuntimeControl::Stop,client.LastError());
    }
    Require(client.Query().activeStage.has_value(),"Runtime port did not enter gameplay");
    auto snapshot=client.Query();
    snapshot.player->position={3.5F,3.1F};snapshot.player->yawRadians=0;snapshot.player->pitchRadians=0.12F;
    snapshot.weaponPresentation.action=fps::WeaponAction::Holstered;
    snapshot.enemies.clear();
    const auto definitions=application.Content()->Data().enemies.GetDefinitions();
    const std::array states{fps::EnemyState::Idle,fps::EnemyState::Moving,fps::EnemyState::Attacking,fps::EnemyState::Dead};
    const std::array names{"idle","move","attack","death"};
    std::size_t captures=0;double skinMs=0;std::size_t vertexBytes=0;
    for(std::size_t state=0;state<states.size();++state)
        for(int sample=0;sample<(state==2?4:3);++sample) {
        snapshot.enemies.clear();
        for(std::size_t index=0;index<definitions.size();++index) {
            const auto& definition=definitions[index];const auto& rig=*definition.rig;
            fps::EnemySnapshot enemy;enemy.id=index+1;enemy.definitionId=definition.id;enemy.kind=definition.kind;
            enemy.state=states[state];enemy.position={2.5F+2*static_cast<float>(index),5.7F};enemy.yawRadians=std::numbers::pi_v<float>;
            enemy.health=enemy.maxHealth=definition.maxHealth;enemy.collisionRadius=definition.hitboxRadius;enemy.hitboxHeight=definition.hitboxHeight;
            const double duration=rig.model->clips[rig.clips[state]].durationSeconds;
            double time=duration*sample*0.5;
            if(state==2&&sample==1) time=definition.kind==fps::EnemyKind::Melee?
                (rig.attackBeginSeconds+rig.attackEndSeconds)*0.5:rig.releaseSeconds;
            if(state==2&&sample==2) time=definition.kind==fps::EnemyKind::Ranged?
                std::min(duration,rig.releaseSeconds+1.0/30.0):duration*0.5;
            if(state==2&&sample==3) time=duration;
            enemy.stateElapsedSeconds=static_cast<float>(time);
            const auto posed=Engine::Model::SamplePose(*rig.model,rig.clips[state],time,Engine::Model::PlaybackMode::Clamp,enemy.pose);
            Require(bool(posed),posed?"":posed.error());
            enemy.body={{enemy.position.x,0,enemy.position.z},enemy.hitboxHeight,enemy.collisionRadius};
            if(enemy.state!=fps::EnemyState::Dead) enemy.hurtboxes=fps::BuildEnemyHurtboxes(rig,enemy.pose,enemy.position,enemy.yawRadians);
            if(state==2&&definition.kind==fps::EnemyKind::Melee&&time>=rig.attackBeginSeconds&&time<=rig.attackEndSeconds) {
                const auto hand=fps::EnemyBoneWorldPoint(rig,enemy.pose,rig.attackPoint,enemy.position,enemy.yawRadians);
                enemy.attackShape=Engine::Collision::Capsule{{hand.x,hand.y,hand.z},{hand.x,hand.y,hand.z},rig.attackRadius*rig.scale};
            }
            std::vector<Engine::Model::SkinnedVertex> skinned;
            for(std::size_t mesh=0;mesh<rig.model->meshes.size();++mesh) {
                const auto begin=std::chrono::steady_clock::now();
                const auto result=Engine::Model::SkinMesh(*rig.model,mesh,enemy.pose,skinned);
                Require(bool(result),result?"":result.error());
                skinMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                vertexBytes+=skinned.size()*sizeof(Engine::Render::Vertex3D);
            }
            snapshot.enemies.push_back(std::move(enemy));
        }
        for(bool debug:{false,true}) {
            snapshot.screen=debug?fps::GameScreen::Paused:fps::GameScreen::Playing;
            const auto file=output.empty()?std::filesystem::path{}:
                output/(std::string(names[state])+"_"+std::to_string(sample)+(debug?"_collision.bmp":"_model.bmp"));
            Render(application,ui,snapshot,debug,file);++captures;
        }
    }
    // A profile view exposes the barrel/grip alignment hidden by a front view.
    snapshot.enemies.clear();
    for(const auto& definition:definitions) if(definition.rig->weapon) {
        const auto& rig=*definition.rig;
        fps::EnemySnapshot enemy;
        enemy.id=1;enemy.definitionId=definition.id;enemy.kind=definition.kind;
        enemy.state=fps::EnemyState::Attacking;enemy.position={3.5F,5.7F};
        enemy.yawRadians=-std::numbers::pi_v<float>*0.5F;
        const auto posed=Engine::Model::SamplePose(*rig.model,rig.clips[2],rig.releaseSeconds,
            Engine::Model::PlaybackMode::Clamp,enemy.pose);
        Require(bool(posed),posed?"":posed.error());
        snapshot.enemies.push_back(std::move(enemy));
    }
    if(!snapshot.enemies.empty()) {
        snapshot.screen=fps::GameScreen::Playing;
        Render(application,ui,snapshot,false,output.empty()?std::filesystem::path{}:output/"pistol_profile.bmp");
        ++captures;
    }
    std::cout<<"visual_frames="<<captures<<" cpu_skin_ms="<<skinMs<<" estimated_vertex_upload_bytes="<<vertexBytes<<"\n";
    Wave(application,&ui,output);
}
}

int main(int argc,char** argv) {
    try {
        fps::ObjectFpsApplicationOptions options;options.title="Object FPS v2 enemy acceptance";
        options.session.fadeInSeconds=0.001F;options.session.fadeOutSeconds=0.001F;
        std::string mode;std::filesystem::path output;
        for(int i=1;i<argc;++i) {
            const std::string argument=argv[i];
            if(argument=="--help") {
                std::cout<<"v2 acceptance: --startup-smoke-test | --headless-smoke-test | --inspect-rig | --smoke-test\n"
                    <<"Options: --gpu-driver auto|d3d12|vulkan|metal --output DIRECTORY\n";
                return 0;
            }
            if(argument=="--gpu-driver"&&i+1<argc) options.gpuDriver=argv[++i];
            else if(argument=="--output"&&i+1<argc) output=argv[++i];
            else if(argument=="--inspect-rig"||argument=="--startup-smoke-test"||argument=="--headless-smoke-test"||argument=="--smoke-test") {
                Require(mode.empty(),"Select one acceptance mode");mode=argument;
            } else throw std::runtime_error("Unknown/incomplete option: "+argument);
        }
        Require(!mode.empty(),"Select --inspect-rig, --startup-smoke-test, --headless-smoke-test or --smoke-test");
        const char* base=SDL_GetBasePath();Require(base!=nullptr,"Cannot locate executable directory");
        const auto root=std::filesystem::path(base)/Gyo::AppConfig::Assets;
        if(!output.empty()) std::filesystem::create_directories(output);
        if(mode=="--inspect-rig") {Inspect(root,output);return 0;}
        fps::ObjectFpsApplication application;std::string error;
        Require(application.InitializeContent(root,error),error);
        if(mode=="--startup-smoke-test") {std::cout<<"v2 content ready: stages="<<application.Content()->Stages().size()<<"\n";return 0;}
        if(mode=="--headless-smoke-test") {Wave(application,nullptr,output);return 0;}
        Require(application.InitializeGraphics(options,error),error);
        Visuals(application,output);return 0;
    } catch(const std::exception& error) {std::cerr<<"v2 acceptance: "<<error.what()<<"\n";return 1;}
}
