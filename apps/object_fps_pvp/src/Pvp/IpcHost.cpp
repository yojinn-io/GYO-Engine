#include "RetroFPS/Pvp/IpcHost.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "engine/math/scalar/Scalar.hpp"
#include "runtime_v5.pb.h"
#include <asio.hpp>
#include <array>
#include <charconv>
#include <chrono>
#include <deque>
#include <future>
#include <map>
#include <string_view>
#include <thread>

namespace fps::pvp {
namespace pb = object_fps_pvp::runtime::v5;
using asio::ip::tcp;
namespace {
pb::ShotRejection RejectionForWire(ShotRejection rejection) {
    switch(rejection) {
    case ShotRejection::None: return pb::REJECTION_NONE;
    case ShotRejection::InvalidReference: return pb::REJECTION_INVALID_REFERENCE;
    case ShotRejection::Expired: return pb::REJECTION_EXPIRED;
    case ShotRejection::Cooldown: return pb::REJECTION_COOLDOWN;
    case ShotRejection::Dead: return pb::REJECTION_DEAD;
    case ShotRejection::StaleLife: return pb::REJECTION_STALE_LIFE;
    case ShotRejection::InvalidLife: return pb::REJECTION_INVALID_LIFE;
    case ShotRejection::Reloading: return pb::REJECTION_RELOADING;
    case ShotRejection::EmptyMagazine: return pb::REJECTION_EMPTY_MAGAZINE;
    case ShotRejection::MagazineFull: return pb::REJECTION_MAGAZINE_FULL;
    }
    throw std::logic_error("Unmapped Match action rejection");
}
pb::ActionKind KindForWire(ActionKind kind) {
    switch(kind) {
    case ActionKind::Shot: return pb::ACTION_SHOT;
    case ActionKind::Reload: return pb::ACTION_RELOAD;
    }
    throw std::logic_error("Unmapped Match action kind");
}
pb::LifeState LifeForWire(LifeState state) {
    switch(state) {
    case LifeState::Alive: return pb::LIFE_ALIVE;
    case LifeState::Dead: return pb::LIFE_DEAD;
    }
    throw std::logic_error("Unmapped Match life state");
}
pb::EvictionReason EvictionForWire(EvictionReason reason) {
    switch(reason) {
    case EvictionReason::HighLatency: return pb::EVICTION_HIGH_LATENCY;
    case EvictionReason::UnstableInput: return pb::EVICTION_UNSTABLE_INPUT;
    }
    throw std::logic_error("Unmapped eviction reason");
}
bool WouldBlock(const asio::error_code& error) {
    return error==asio::error::would_block || error==asio::error::try_again;
}
pb::RuntimeEnvelope SnapshotMessage(const WorldSnapshot& snapshot) {
    pb::RuntimeEnvelope message; message.set_protocol_version(5);
    auto* out=message.mutable_snapshot(); out->set_tick(snapshot.tick);
    for(const auto& p:snapshot.players) {
        auto* state=out->add_players(); state->set_player_id(p.playerId);
        state->set_x(p.position.x); state->set_y(p.position.y); state->set_z(p.position.z);
        state->set_yaw(p.yaw); state->set_pitch(p.pitch); state->set_last_resolved_command(p.lastResolvedCommand);
        state->set_movement_epoch(p.movementEpoch);state->set_contiguous_pending_commands(p.contiguousPendingCommands);
        state->set_vertical_velocity(p.verticalVelocity);state->set_grounded(p.grounded);
        state->set_life_generation(p.lifeGeneration);state->set_life_state(LifeForWire(p.lifeState));
        state->set_life_state_tick(p.lifeStateTick);state->set_respawn_tick(p.respawnTick);
        if(p.movementSlackSequence && p.movementSlackMicros) {
            state->set_movement_slack_sequence(*p.movementSlackSequence);state->set_movement_slack_us(*p.movementSlackMicros);
        }
        state->set_connection_quality_failures(p.connectionQualityFailures);
    }
    for(const auto& p:snapshot.combat) {
        auto* state=out->add_combat();state->set_player_id(p.playerId);
        state->set_hp(p.hp);state->set_next_allowed_shot_tick(p.nextAllowedShotTick);
        state->set_life_generation(p.lifeGeneration);state->set_magazine_ammo(p.magazineAmmo);
        state->set_reload_action_id(p.reloadActionId);state->set_reload_start_tick(p.reloadStartTick);
        state->set_reload_end_tick(p.reloadEndTick);state->set_last_shot_action_id(p.lastShotActionId);
        state->set_last_shot_tick(p.lastShotTick);
    }
    return message;
}
pb::RuntimeEnvelope ActionMessage(const ActionResults& results, ActionId& cursor) {
    pb::RuntimeEnvelope message;message.set_protocol_version(5);
    auto* out=message.mutable_action_results();out->set_player_id(results.playerId);
    out->set_retired_through(results.retiredThrough);
    if(results.decisions.empty()) {cursor=results.retiredThrough;return message;}
    const auto first=std::find_if(results.decisions.begin(),results.decisions.end(),
        [&](const auto& decision){return decision.actionId>cursor;});
    const auto offset=first==results.decisions.end()?0:static_cast<std::size_t>(first-results.decisions.begin());
    for(std::size_t i=0;i<Engine::Math::Min(MaxActionBatch,results.decisions.size());++i) {
        const auto& decision=results.decisions[(offset+i)%results.decisions.size()];
        auto* value=out->add_decisions();value->set_action_id(decision.actionId);
        value->set_resolved_tick(decision.resolvedTick);value->set_accepted(decision.accepted);
        value->set_rejection(RejectionForWire(decision.rejection));
        value->set_hit_kind(static_cast<pb::ShotHitKind>(decision.hitKind));
        value->set_target_id(decision.targetId);value->set_damage(decision.damage);
        value->set_kind(KindForWire(decision.kind));value->set_life_generation(decision.lifeGeneration);
        value->set_target_life_generation(decision.targetLifeGeneration);
        cursor=decision.actionId;
    }
    return message;
}
}
struct IpcHost::Impl {
    MatchRuntimeHost& host;
    Arena arena;
    asio::io_context io;
    tcp::acceptor listener{io};
    std::jthread worker;
    std::uint64_t requestSequence{};
    Impl(MatchRuntimeHost& value,const Arena& content):host(value),arena(content){}

    void Connection(tcp::socket& socket,std::stop_token stop) {
        socket.non_blocking(true);
        socket.set_option(tcp::no_delay(true));
        std::vector<std::uint8_t> input;
        std::deque<std::vector<std::uint8_t>> controls;
        using Clock=std::chrono::steady_clock;
        struct SnapshotFrame {std::vector<std::uint8_t> bytes;std::uint64_t tick;Clock::time_point queuedAt;};
        std::optional<SnapshotFrame> latestSnapshot;
        std::vector<std::uint8_t> writing;
        std::size_t writeOffset{};
        bool writingSnapshot{};
        std::uint64_t writingTick{},coalescedSnapshots{};
        Clock::time_point writingQueuedAt{};
        std::map<std::uint64_t,PlayerId> joins;
        struct ActionLane {ActionId cursor{};Clock::time_point nextSend{};};
        std::map<PlayerId,ActionLane> actionLanes;
        pb::RuntimeEnvelope ready; ready.set_protocol_version(5);
        auto* r=ready.mutable_ready(); r->set_arena_id(arena.id); r->set_arena_version(arena.version);
        r->set_jump_height(arena.jumpHeight);r->set_gravity(arena.gravity);
        r->set_tick_rate(AuthorityTickRate); r->set_snapshot_interval_ticks(SnapshotIntervalTicks); r->set_max_players(2);
        auto* rules=r->mutable_combat_rules();rules->set_maximum_hp(PvpCombatRules.maximumHp);
        rules->set_shot_damage(PvpCombatRules.shotDamage);rules->set_cooldown_ticks(PvpCombatRules.cooldownTicks);
        rules->set_shot_range(PvpCombatRules.shotRange);
        rules->set_magazine_capacity(PvpCombatRules.magazineCapacity);rules->set_reload_ticks(PvpCombatRules.reloadTicks);
        rules->set_respawn_ticks(PvpCombatRules.respawnTicks);
        rules->set_maximum_reference_age_ms(static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(PvpCombatRules.maximumReferenceAge).count()));
        controls.push_back(wire::Frame(ready.SerializeAsString()));
        std::array<std::uint8_t,8192> buffer{};
        while(!stop.stop_requested()) {
            asio::error_code error;
            const auto received=socket.read_some(asio::buffer(buffer),error);
            if(error && !WouldBlock(error)) return;
            if(received) input.insert(input.end(),buffer.begin(),buffer.begin()+received);
            if(input.size()>wire::MaxFrame+4+buffer.size()) return;
            while(input.size()>=4) {
                const auto length=static_cast<std::size_t>(wire::Read(std::span(input).first(4)));
                if(!length || length>wire::MaxFrame) return;
                if(input.size()<length+4) break;
                pb::RuntimeEnvelope message;
                if(!message.ParseFromArray(input.data()+4,static_cast<int>(length)) || message.protocol_version()!=5) return;
                input.erase(input.begin(),input.begin()+static_cast<std::ptrdiff_t>(length+4));
                if(message.has_join()) {
                    if(joins.size()>=64) return;
                    const auto request=++requestSequence;
                    if(!host.QueueJoin(request,message.join().player_id())) return;
                    joins.emplace(request,message.join().player_id());
                } else if(message.has_leave()) {
                    if(!host.QueueLeave(++requestSequence,message.leave().player_id())) return;
                    actionLanes.erase(message.leave().player_id());
                } else if(message.has_input()) {
                    const auto& p=message.input();
                    if(p.commands_size()==0 || p.commands_size()>static_cast<int>(MaxPendingCommands)) continue;
                    PlayerInput window{p.player_id(),{},p.movement_epoch(),p.life_generation(),p.observed_authority_tick()};
                    for(const auto& command:p.commands())
                        window.commands.push_back({command.sequence(),command.move_forward(),command.move_right(),command.yaw(),command.pitch(),command.jump_requested()});
                    static_cast<void>(host.SubmitInput(window));
                } else if(message.has_actions()) {
                    const auto& value=message.actions();
                    if(value.shots_size()>static_cast<int>(MaxActionBatch)) continue;
                    ActionBatch batch{value.player_id(),{}};
                    bool valid=true;
                    for(const auto& shot:value.shots()) {
                        const bool shooting=shot.kind()==pb::ACTION_SHOT;
                        const bool reloading=shot.kind()==pb::ACTION_RELOAD;
                        if(!shot.life_generation() || (!shooting && !reloading) ||
                           (shooting && (!shot.has_yaw() || !shot.has_pitch())) ||
                           (reloading && (shot.has_yaw() || shot.has_pitch()))) {valid=false;break;}
                        batch.shots.push_back({shot.action_id(),shot.observed_authority_tick(),shot.yaw(),shot.pitch(),
                            shooting?ActionKind::Shot:ActionKind::Reload,shot.life_generation()});
                    }
                    if(!valid) continue;
                    // Admission is distinct from a terminal shot decision. The
                    // immutable request remains with the sender for retry.
                    static_cast<void>(host.SubmitActionBatch(batch,value.acknowledged_through()));
                } else return;
            }
            for(const auto& result:host.TakeControlResults()) {
                const auto found=joins.find(result.requestId);
                if(found==joins.end()) continue;
                pb::RuntimeEnvelope message; message.set_protocol_version(5);
                auto* joined=message.mutable_join_result(); joined->set_player_id(found->second);
                joined->set_accepted(result.accepted); joined->set_reason(result.error);
                if(result.accepted && host.GetActionResults(found->second)) actionLanes.try_emplace(found->second);
                controls.push_back(wire::Frame(message.SerializeAsString())); joins.erase(found);
                if(controls.size()>64) return;
            }
            for(const auto& eviction:host.TakeEvictions()) {
                // The Match already removed the player; the Gateway clears its
                // reservation like a Leave and tells the Client why.
                pb::RuntimeEnvelope message; message.set_protocol_version(5);
                auto* evicted=message.mutable_evicted();evicted->set_player_id(eviction.playerId);
                evicted->set_reason(EvictionForWire(eviction.reason));
                evicted->set_reference_age_ms(eviction.referenceAgeMillis);
                evicted->set_substituted_permille(eviction.substitutedPermille);
                evicted->set_movement_resets(eviction.movementResets);
                controls.push_back(wire::Frame(message.SerializeAsString()));
                actionLanes.erase(eviction.playerId);
                if(controls.size()>64) return;
            }
            if(auto snapshot=host.TakeSnapshot()) {
                if(latestSnapshot) {
                    ++coalescedSnapshots;
                    TraceMovement({.kind=MovementTraceKind::Transport,.authorityTick=snapshot->tick,
                        .count=coalescedSnapshots,.ageSeconds=std::chrono::duration<double>(Clock::now()-latestSnapshot->queuedAt).count()});
                }
                latestSnapshot=SnapshotFrame{wire::Frame(SnapshotMessage(*snapshot).SerializeAsString()),snapshot->tick,Clock::now()};
            }
            // An unstarted snapshot is still replaceable. Once TCP accepted
            // even one byte, finish that frame before selecting anything else.
            if(writingSnapshot && !writing.empty() && writeOffset==0 && latestSnapshot) {
                ++coalescedSnapshots;
                writing=std::move(latestSnapshot->bytes);writingTick=latestSnapshot->tick;
                writingQueuedAt=latestSnapshot->queuedAt;latestSnapshot.reset();
                TraceMovement({.kind=MovementTraceKind::Transport,.authorityTick=writingTick,.count=coalescedSnapshots});
            }
            if(writing.empty()) {
                if(!controls.empty()) {
                    writing=std::move(controls.front());controls.pop_front();writingSnapshot=false;writingTick=0;writingQueuedAt=Clock::now();
                } else {
                    // Results stay in Match until Client ACK. Only choose a
                    // batch when a write slot is free; a blocked socket cannot
                    // accumulate or overwrite a separate decision queue.
                    const auto now=Clock::now();
                    for(auto it=actionLanes.begin();it!=actionLanes.end();) {
                        auto& [playerId,lane]=*it;
                        if(now<lane.nextSend) {++it;continue;}
                        lane.nextSend=now+std::chrono::nanoseconds((1'000'000'000+ActionSendRate-1)/ActionSendRate);
                        const auto results=host.GetActionResults(playerId);
                        if(!results) {it=actionLanes.erase(it);continue;}
                        ++it;
                        if(results->decisions.empty() && results->retiredThrough==0) continue;
                        writing=wire::Frame(ActionMessage(*results,lane.cursor).SerializeAsString());
                        writingSnapshot=false;writingTick=0;writingQueuedAt=now;
                        break;
                    }
                }
                if(writing.empty() && latestSnapshot) {
                    writing=std::move(latestSnapshot->bytes);writingTick=latestSnapshot->tick;
                    writingQueuedAt=latestSnapshot->queuedAt;writingSnapshot=true;latestSnapshot.reset();
                }
                writeOffset=0;
            }
            if(!writing.empty()) {
                error.clear();
                writeOffset+=socket.write_some(asio::buffer(writing.data()+writeOffset,writing.size()-writeOffset),error);
                if(error && !WouldBlock(error)) return;
                if(writeOffset==writing.size()) writing.clear();
                else TraceMovement({.kind=MovementTraceKind::Transport,.authorityTick=writingTick,
                    .count=coalescedSnapshots,.ageSeconds=std::chrono::duration<double>(Clock::now()-writingQueuedAt).count()});
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    void Run(std::stop_token stop) {
        while(!stop.stop_requested()) {
            tcp::socket socket(io); asio::error_code error;
            listener.accept(socket,error);
            if(error) {
                if(!WouldBlock(error)) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue;
            }
            auto reset=host.RequestReset();
            while(reset.wait_for(std::chrono::milliseconds(10))!=std::future_status::ready)
                if(stop.stop_requested()) return;
            try { Connection(socket,stop); } catch(const std::exception&) { /* Close the failed IPC session. */ }
            socket.close(error);
            auto cleared=host.RequestReset();
            while(cleared.wait_for(std::chrono::milliseconds(10))!=std::future_status::ready)
                if(stop.stop_requested()) return;
        }
    }
};
IpcHost::IpcHost(MatchRuntimeHost& host,const Arena& arena):impl_(std::make_unique<Impl>(host,arena)){}
IpcHost::~IpcHost(){Stop();}
bool IpcHost::Start(const std::string& listenAddress,std::string& error) {
    try {
        const auto separator=listenAddress.rfind(':');
        if(separator==std::string::npos) throw std::invalid_argument("Expected loopback-ip:port");
        const auto address=asio::ip::make_address(listenAddress.substr(0,separator));
        if(!address.is_loopback()) throw std::invalid_argument("IPC must listen on loopback");
        const auto portText=std::string_view(listenAddress).substr(separator+1);
        unsigned int port{};
        const auto parsed=std::from_chars(portText.data(),portText.data()+portText.size(),port);
        if(parsed.ec!=std::errc{} || parsed.ptr!=portText.data()+portText.size() || port==0 || port>65535)
            throw std::invalid_argument("IPC port must be a decimal integer in [1, 65535]");
        const tcp::endpoint endpoint(address,static_cast<unsigned short>(port));
        impl_->listener.open(endpoint.protocol());
        impl_->listener.set_option(tcp::acceptor::reuse_address(true));
        impl_->listener.bind(endpoint); impl_->listener.listen(1); impl_->listener.non_blocking(true);
        impl_->worker=std::jthread([this](std::stop_token stop){impl_->Run(stop);});
        return true;
    } catch(const std::exception& failure) {error=failure.what();return false;}
}
void IpcHost::Stop(){if(impl_->worker.joinable()){impl_->worker.request_stop();impl_->worker.join();}}
}
