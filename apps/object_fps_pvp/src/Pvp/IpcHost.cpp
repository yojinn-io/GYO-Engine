#include "RetroFPS/Pvp/IpcHost.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "engine/threads/RoleThread.hpp"
#include "runtime_v6.pb.h"
#include <asio.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <deque>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <syncstream>

namespace fps::pvp {
namespace pb = object_fps_pvp::runtime::v6;
using asio::ip::tcp;
namespace {
// Diagnostics only: lifecycle events and a statistics line every ten seconds
// on std::clog (match_main copies it into --log). Never affects the session.
class MatchEventLog final {
public:
    static void Line(const std::string& text) {
        std::osyncstream(std::clog) << "[ObjectFPS/PvP Match] " << text << '\n';
    }
    void Observe(const WorldSnapshot& snapshot, std::uint64_t coalescedSnapshots) {
        std::map<PlayerId, Life> current;
        for (const auto& player : snapshot.players) {
            const auto known = lives_.find(player.playerId);
            if (known != lives_.end() && player.lifeGeneration == known->second.generation &&
                known->second.state != LifeState::Dead && player.lifeState == LifeState::Dead) {
                PlayerId attacker{};
                for (const auto& combat : snapshot.combat)
                    if (combat.playerId == player.playerId) attacker = combat.lastAttackerId;
                std::ostringstream line;
                line << "death player=" << player.playerId << " tick=" << snapshot.tick << " attacker=" << attacker;
                Line(line.str());
            }
            if (known != lives_.end() && player.lifeGeneration > known->second.generation) {
                std::ostringstream line;
                line << "respawn player=" << player.playerId << " life=" << player.lifeGeneration << " tick=" << snapshot.tick
                     << " x=" << player.position.x << " z=" << player.position.z;
                Line(line.str());
            }
            current[player.playerId] = {player.lifeGeneration, player.lifeState};
        }
        lives_ = std::move(current);
        const auto now = std::chrono::steady_clock::now();
        if (now - lastStatistics_ >= std::chrono::seconds(10)) {
            lastStatistics_ = now;
            std::ostringstream line;
            line << "statistics tick=" << snapshot.tick << " players=" << snapshot.players.size()
                 << " coalesced_snapshots=" << coalescedSnapshots;
            Line(line.str());
        }
    }
private:
    struct Life { std::uint64_t generation{}; LifeState state{}; };
    std::map<PlayerId, Life> lives_;
    std::chrono::steady_clock::time_point lastStatistics_{};
};

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
    pb::RuntimeEnvelope message; message.set_protocol_version(wire::ProtocolVersion);
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
        state->set_last_shot_tick(p.lastShotTick);state->set_last_damage_tick(p.lastDamageTick);
        state->set_damage_count(p.damageCount);state->set_last_attacker_id(p.lastAttackerId);
    }
    return message;
}
// A lane writes only new content: decisions it has not sent and a retirement
// beyond the one it sent. TCP delivers each once, and a lane never outlives its
// connection (each accept starts a new Session), so nothing is resent. That
// relies on the Gateway dropping results only for a removed player, which
// never comes back under the same id.
// The host wakes Pump only on a step that publishes; with a snapshot every
// tick, a decision is seen on the tick that makes it.
static_assert(SnapshotIntervalTicks==1,"Action lanes rely on a publish every tick");
constexpr auto ActionInterval=std::chrono::nanoseconds((1'000'000'000+ActionSendRate-1)/ActionSendRate);
struct ActionLane {
    std::optional<std::chrono::steady_clock::time_point> lastSend;
    ActionId sentRetired{};
    // Sent decision ids above sentRetired; at most MaxActionWindow.
    std::set<ActionId> sent;
};
// The one test for new content: ScheduleWrite's deadline and Write's choice
// must agree, or a due lane with nothing to choose would wake writes forever.
bool LanePending(const ActionResults& results, const ActionLane& lane) {
    return results.retiredThrough>lane.sentRetired || std::any_of(results.decisions.begin(),results.decisions.end(),
        [&](const auto& decision){return !lane.sent.contains(decision.actionId);});
}
// At most one choice per ActionInterval; the first may go at once.
std::chrono::steady_clock::time_point LaneDue(const ActionLane& lane, std::chrono::steady_clock::time_point now) {
    return lane.lastSend?*lane.lastSend+ActionInterval:now;
}
// Chooses the retirement and up to MaxActionBatch unsent decisions in id order
// (decisions arrive sorted by id) and records them as sent at `now`.
pb::RuntimeEnvelope ActionMessage(const ActionResults& results, ActionLane& lane, std::chrono::steady_clock::time_point now) {
    pb::RuntimeEnvelope message;message.set_protocol_version(wire::ProtocolVersion);
    auto* out=message.mutable_action_results();out->set_player_id(results.playerId);
    out->set_retired_through(results.retiredThrough);
    lane.lastSend=now;
    lane.sentRetired=results.retiredThrough;
    lane.sent.erase(lane.sent.begin(),lane.sent.upper_bound(results.retiredThrough));
    for(const auto& decision:results.decisions) {
        if(static_cast<std::size_t>(out->decisions_size())==MaxActionBatch) break;
        if(lane.sent.contains(decision.actionId)) continue;
        lane.sent.insert(decision.actionId);
        auto* value=out->add_decisions();value->set_action_id(decision.actionId);
        value->set_resolved_tick(decision.resolvedTick);value->set_accepted(decision.accepted);
        value->set_rejection(RejectionForWire(decision.rejection));
        value->set_hit_kind(static_cast<pb::ShotHitKind>(decision.hitKind));
        value->set_target_id(decision.targetId);value->set_damage(decision.damage);
        value->set_kind(KindForWire(decision.kind));value->set_life_generation(decision.lifeGeneration);
        value->set_target_life_generation(decision.targetLifeGeneration);
    }
    return message;
}
}
// One asio io thread (gyo-match-ipc) serves the single Gateway connection.
// Nothing polls: the host's publish notification, the socket becoming
// readable or writable and the action lanes' deadline timer wake it. Phases:
// accepting -> resetting (before a session) -> connected -> closing (reset
// after it) -> accepting. A reset completes on the simulation role, whose
// publish notification wakes this thread to continue.
struct IpcHost::Impl {
    using Clock=std::chrono::steady_clock;
    // The host notifies from its simulation role; Stop detaches this gate
    // first, so no notification reaches a stopped or destroyed IpcHost.
    struct WakeGate {std::mutex mutex;Impl* impl{};};
    enum class Phase {Idle,Accepting,Resetting,Connected,Closing};
    // samples: the players whose movement slack sample the frame carries,
    // counted by the host when latest-wins replaces the frame unwritten.
    struct SnapshotFrame {std::vector<std::uint8_t> bytes;std::uint64_t tick;Clock::time_point queuedAt;std::vector<PlayerId> samples;};
    // One connection's state; replaced at each accept.
    struct Session {
        std::vector<std::uint8_t> input;
        std::deque<std::vector<std::uint8_t>> controls;
        std::optional<SnapshotFrame> latestSnapshot;
        std::vector<std::uint8_t> writing;
        std::size_t writeOffset{};
        bool writingSnapshot{};
        std::uint64_t writingTick{},coalescedSnapshots{};
        std::vector<PlayerId> writingSamples;
        Clock::time_point writingQueuedAt{};
        std::map<std::uint64_t,PlayerId> joins;
        MatchEventLog events;
        std::map<PlayerId,ActionLane> actionLanes;
    };

    MatchRuntimeHost& host;
    Arena arena;
    std::uint64_t arenaDigest{};
    asio::io_context io;
    tcp::acceptor listener{io};
    tcp::socket socket{io};
    asio::steady_timer laneTimer{io};
    std::optional<Clock::time_point> laneTimerAt;
    std::shared_ptr<WakeGate> gate=std::make_shared<WakeGate>();
    std::atomic<bool> pumpPosted{};
    Phase phase{Phase::Idle};
    std::future<void> reset;
    // Handlers of a closed connection still complete (aborted); they compare
    // their connection number with this one and return.
    std::uint64_t connection{};
    Session session;
    std::array<std::uint8_t,8192> buffer{};
    bool reading{},waitingWrite{};
    std::uint64_t requestSequence{};
    // Diagnostics only: io handler passes (accept, read, write, timer, pump).
    std::atomic<std::uint64_t> iterations{};
    std::optional<Engine::Threads::RoleThread> role;

    Impl(MatchRuntimeHost& value,const Arena& content):host(value),arena(content),arenaDigest(ArenaContentDigest(content)) {
        // Zero is reserved for a missing digest: such an arena cannot be served.
        if(!arenaDigest) throw std::runtime_error("Arena content digest is zero");
        if(std::string error; !ArenaHostsRoom(content,error)) throw std::runtime_error(error);
        gate->impl=this;
        host.SetPublishListener([gate=gate] {
            const std::lock_guard lock(gate->mutex);
            if(gate->impl) gate->impl->PostPump();
        });
    }
    void Detach() {
        const std::lock_guard lock(gate->mutex);
        gate->impl=nullptr;
    }
    // Any thread: one pending Pump at a time is enough.
    void PostPump() {
        if(!pumpPosted.exchange(true)) asio::post(io,[this]{pumpPosted.store(false);Pump();});
    }

    void StartAccept() {
        phase=Phase::Accepting;
        listener.async_accept([this](const asio::error_code& error,tcp::socket accepted) {
            iterations.fetch_add(1,std::memory_order_relaxed);
            if(error==asio::error::operation_aborted) return;
            if(error) {MatchEventLog::Line("ipc accept failed reason="+error.message()+"; no further connections");phase=Phase::Idle;return;}
            socket=std::move(accepted);
            ++connection;
            phase=Phase::Resetting;
            reset=host.RequestReset();
            CheckReset();
        });
    }
    // The reset before a session (Resetting) or after it (Closing) completed.
    void CheckReset() {
        if(!reset.valid() || reset.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
        reset.get();
        if(phase==Phase::Resetting) BeginSession();
        else if(phase==Phase::Closing) StartAccept();
    }
    void BeginSession() {
        asio::error_code error;
        socket.set_option(tcp::no_delay(true),error);
        if(!error) socket.non_blocking(true,error);
        session=Session{};
        phase=Phase::Connected;
        if(error) {Close("socket setup: "+error.message());return;}
        pb::RuntimeEnvelope ready; ready.set_protocol_version(wire::ProtocolVersion);
        auto* r=ready.mutable_ready(); r->set_arena_id(arena.id); r->set_arena_version(arena.version); r->set_arena_digest(arenaDigest);
        r->set_jump_height(arena.jumpHeight);r->set_gravity(arena.gravity);
        r->set_tick_rate(AuthorityTickRate); r->set_snapshot_interval_ticks(SnapshotIntervalTicks); r->set_max_players(MaxPlayers);
        auto* rules=r->mutable_combat_rules();rules->set_maximum_hp(PvpCombatRules.maximumHp);
        rules->set_shot_damage(PvpCombatRules.shotDamage);rules->set_cooldown_ticks(PvpCombatRules.cooldownTicks);
        rules->set_shot_range(PvpCombatRules.shotRange);
        rules->set_magazine_capacity(PvpCombatRules.magazineCapacity);rules->set_reload_ticks(PvpCombatRules.reloadTicks);
        rules->set_respawn_ticks(PvpCombatRules.respawnTicks);
        rules->set_maximum_reference_age_ms(static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(PvpCombatRules.maximumReferenceAge).count()));
        session.controls.push_back(wire::Frame(ready.SerializeAsString()));
        StartRead();
        Pump();
    }
    // The single path that ends a session: log why, close, reset the Match,
    // then accept the next connection once the reset completed.
    void Close(const std::string& reason) {
        if(phase!=Phase::Connected) return;
        MatchEventLog::Line("ipc closed reason="+reason);
        asio::error_code ignored;
        socket.close(ignored);
        laneTimer.cancel();
        laneTimerAt.reset();
        reading=waitingWrite=false;
        session=Session{};
        ++connection;
        phase=Phase::Closing;
        reset=host.RequestReset();
        CheckReset();
    }

    void StartRead() {
        reading=true;
        socket.async_read_some(asio::buffer(buffer),[this,current=connection](const asio::error_code& error,std::size_t received) {
            iterations.fetch_add(1,std::memory_order_relaxed);
            if(current!=connection) return;
            reading=false;
            if(error) {Close(error==asio::error::eof?"eof":"read: "+error.message());return;}
            try {
                if(const auto problem=Receive(received)) {Close(*problem);return;}
                Pump();
            } catch(const std::exception& failure) {Close(std::string("exception: ")+failure.what());return;}
            if(current==connection && phase==Phase::Connected) StartRead();
        });
    }
    // Parses complete frames; returns why the session must end, if it must.
    std::optional<std::string> Receive(std::size_t received) {
        auto& input=session.input;
        input.insert(input.end(),buffer.begin(),buffer.begin()+static_cast<std::ptrdiff_t>(received));
        if(input.size()>wire::MaxFrame+4+buffer.size()) return "input overflow";
        while(input.size()>=4) {
            const auto length=static_cast<std::size_t>(wire::Read(std::span(input).first(4)));
            if(!length || length>wire::MaxFrame) return "frame length";
            if(input.size()<length+4) break;
            pb::RuntimeEnvelope message;
            if(!message.ParseFromArray(input.data()+4,static_cast<int>(length)) || message.protocol_version()!=wire::ProtocolVersion)
                return "frame parse or protocol version";
            input.erase(input.begin(),input.begin()+static_cast<std::ptrdiff_t>(length+4));
            if(message.has_join()) {
                if(session.joins.size()>=64) return "pending joins over 64";
                const auto request=++requestSequence;
                if(!host.QueueJoin(request,message.join().player_id())) return "join refused by the Match queue";
                session.joins.emplace(request,message.join().player_id());
            } else if(message.has_leave()) {
                MatchEventLog::Line("leave player="+std::to_string(message.leave().player_id()));
                if(!host.QueueLeave(++requestSequence,message.leave().player_id())) return "leave refused by the Match queue";
                session.actionLanes.erase(message.leave().player_id());
            } else if(message.has_input()) {
                const auto& p=message.input();
                PlayerInput window{p.player_id(),{},p.movement_epoch(),p.life_generation(),p.observed_authority_tick()};
                for(const auto& command:p.commands())
                    window.commands.push_back({command.sequence(),command.move_forward(),command.move_right(),command.yaw(),command.pitch(),command.jump_requested()});
                // The host counts every outcome by reason: an empty or
                // oversized window as unknown_player (player 0) when the Match
                // does not hold the player, as resetting during a reset, and
                // as malformed otherwise. Nothing is answered here.
                static_cast<void>(host.SubmitInput(window));
            } else if(message.has_actions()) {
                const auto& value=message.actions();
                const auto shots=static_cast<std::size_t>(value.shots_size());
                if(shots>MaxActionBatch) {host.NoteWireRejection(value.player_id(),IngressActionRejection::OverBatch,shots);continue;}
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
                if(!valid) {host.NoteWireRejection(value.player_id(),IngressActionRejection::Malformed,shots);continue;}
                // Admission is distinct from a terminal shot decision. The
                // immutable request remains with the sender for retry. The
                // host counts the admission by reason.
                static_cast<void>(host.SubmitActionBatch(batch,value.acknowledged_through()));
            } else return "unknown message";
        }
        return std::nullopt;
    }

    // Takes what the host published and schedules the next write.
    void Pump() {
        iterations.fetch_add(1,std::memory_order_relaxed);
        if(phase==Phase::Resetting || phase==Phase::Closing) {CheckReset();return;}
        if(phase!=Phase::Connected) return;
        try {
            for(const auto& result:host.TakeControlResults()) {
                const auto found=session.joins.find(result.requestId);
                if(found==session.joins.end()) continue;
                pb::RuntimeEnvelope message; message.set_protocol_version(wire::ProtocolVersion);
                auto* joined=message.mutable_join_result(); joined->set_player_id(found->second);
                joined->set_accepted(result.accepted); joined->set_reason(result.error);
                MatchEventLog::Line("join player="+std::to_string(found->second)+(result.accepted?" accepted":" rejected reason="+result.error));
                if(result.accepted && host.GetActionResults(found->second)) session.actionLanes.try_emplace(found->second);
                session.controls.push_back(wire::Frame(message.SerializeAsString())); session.joins.erase(found);
                if(session.controls.size()>64) {Close("pending controls over 64");return;}
            }
            for(const auto& eviction:host.TakeEvictions()) {
                // The Match already removed the player; the Gateway clears its
                // reservation like a Leave and tells the Client why.
                pb::RuntimeEnvelope message; message.set_protocol_version(wire::ProtocolVersion);
                auto* evicted=message.mutable_evicted();evicted->set_player_id(eviction.playerId);
                MatchEventLog::Line("evicted player="+std::to_string(eviction.playerId)+" reference_age_ms="+
                    std::to_string(eviction.referenceAgeMillis)+" substituted_permille="+std::to_string(eviction.substitutedPermille)+
                    " movement_resets="+std::to_string(eviction.movementResets));
                evicted->set_reason(EvictionForWire(eviction.reason));
                evicted->set_reference_age_ms(eviction.referenceAgeMillis);
                evicted->set_substituted_permille(eviction.substitutedPermille);
                evicted->set_movement_resets(eviction.movementResets);
                session.controls.push_back(wire::Frame(message.SerializeAsString()));
                session.actionLanes.erase(eviction.playerId);
                if(session.controls.size()>64) {Close("pending controls over 64");return;}
            }
            if(auto snapshot=host.TakeSnapshot()) {
                session.events.Observe(*snapshot,session.coalescedSnapshots);
                if(session.latestSnapshot) {
                    ++session.coalescedSnapshots;
                    host.NoteCoalescedSlackSamples(session.latestSnapshot->samples);
                    TraceMovement({.kind=MovementTraceKind::Transport,.authorityTick=snapshot->tick,
                        .count=session.coalescedSnapshots,.ageSeconds=std::chrono::duration<double>(Clock::now()-session.latestSnapshot->queuedAt).count()});
                }
                std::vector<PlayerId> samples;
                for(const auto& player:snapshot->players) if(player.movementSlackSequence) samples.push_back(player.playerId);
                session.latestSnapshot=SnapshotFrame{wire::Frame(SnapshotMessage(*snapshot).SerializeAsString()),snapshot->tick,Clock::now(),std::move(samples)};
            }
            ScheduleWrite();
        } catch(const std::exception& failure) {Close(std::string("exception: ")+failure.what());}
    }

    // Waits for the socket to become writable when there is something to
    // write; otherwise arms the lane timer for the earliest lane with new
    // content. The frame is chosen only when the socket is writable.
    void ScheduleWrite() {
        if(phase!=Phase::Connected || waitingWrite) return;
        const auto now=Clock::now();
        std::optional<Clock::time_point> nextLane;
        for(auto it=session.actionLanes.begin();it!=session.actionLanes.end();) {
            const auto results=host.GetActionResults(it->first);
            if(!results) {it=session.actionLanes.erase(it);continue;}
            if(!LanePending(*results,it->second)) {++it;continue;}
            const auto due=LaneDue(it->second,now);
            if(!nextLane || due<*nextLane) nextLane=due;
            ++it;
        }
        const bool laneDue=nextLane && *nextLane<=now;
        if(!session.writing.empty() || !session.controls.empty() || session.latestSnapshot || laneDue) {
            waitingWrite=true;
            socket.async_wait(tcp::socket::wait_write,[this,current=connection](const asio::error_code& error) {
                iterations.fetch_add(1,std::memory_order_relaxed);
                if(current!=connection) return;
                waitingWrite=false;
                if(error) {Close("write wait: "+error.message());return;}
                try {Write();} catch(const std::exception& failure) {Close(std::string("exception: ")+failure.what());}
            });
            return;
        }
        if(nextLane && (!laneTimerAt || *nextLane<*laneTimerAt)) {
            laneTimerAt=*nextLane;
            laneTimer.expires_at(*nextLane);
            laneTimer.async_wait([this,current=connection](const asio::error_code& error) {
                iterations.fetch_add(1,std::memory_order_relaxed);
                if(error==asio::error::operation_aborted || current!=connection) return;
                laneTimerAt.reset();
                Pump();
            });
        }
    }
    // Chooses (if nothing is in progress) and writes one frame without blocking.
    void Write() {
        auto& s=session;
        // An unstarted snapshot is still replaceable. Once TCP accepted even
        // one byte, finish that frame before selecting anything else.
        if(s.writingSnapshot && !s.writing.empty() && s.writeOffset==0 && s.latestSnapshot) {
            ++s.coalescedSnapshots;
            host.NoteCoalescedSlackSamples(s.writingSamples);
            s.writing=std::move(s.latestSnapshot->bytes);s.writingTick=s.latestSnapshot->tick;
            s.writingQueuedAt=s.latestSnapshot->queuedAt;s.writingSamples=std::move(s.latestSnapshot->samples);s.latestSnapshot.reset();
            TraceMovement({.kind=MovementTraceKind::Transport,.authorityTick=s.writingTick,.count=s.coalescedSnapshots});
        }
        if(s.writing.empty()) {
            if(!s.controls.empty()) {
                s.writing=std::move(s.controls.front());s.controls.pop_front();s.writingSnapshot=false;s.writingTick=0;s.writingQueuedAt=Clock::now();s.writingSamples.clear();
            } else {
                // Results stay in Match until Client ACK. Only choose a batch
                // when a write slot is free; a blocked socket cannot accumulate
                // or overwrite a separate decision queue. A lane without new
                // content keeps its anchor: new content may go at once.
                const auto now=Clock::now();
                for(auto it=s.actionLanes.begin();it!=s.actionLanes.end();) {
                    auto& [playerId,lane]=*it;
                    if(now<LaneDue(lane,now)) {++it;continue;}
                    const auto results=host.GetActionResults(playerId);
                    if(!results) {it=s.actionLanes.erase(it);continue;}
                    ++it;
                    if(!LanePending(*results,lane)) continue;
                    s.writing=wire::Frame(ActionMessage(*results,lane,now).SerializeAsString());
                    s.writingSnapshot=false;s.writingTick=0;s.writingQueuedAt=now;s.writingSamples.clear();
                    break;
                }
            }
            if(s.writing.empty() && s.latestSnapshot) {
                s.writing=std::move(s.latestSnapshot->bytes);s.writingTick=s.latestSnapshot->tick;
                s.writingQueuedAt=s.latestSnapshot->queuedAt;s.writingSnapshot=true;s.writingSamples=std::move(s.latestSnapshot->samples);s.latestSnapshot.reset();
            }
            s.writeOffset=0;
        }
        if(!s.writing.empty()) {
            asio::error_code error;
            s.writeOffset+=socket.write_some(asio::buffer(s.writing.data()+s.writeOffset,s.writing.size()-s.writeOffset),error);
            if(error && !WouldBlock(error)) {Close("write: "+error.message());return;}
            if(s.writeOffset==s.writing.size()) s.writing.clear();
            else TraceMovement({.kind=MovementTraceKind::Transport,.authorityTick=s.writingTick,
                .count=s.coalescedSnapshots,.ageSeconds=std::chrono::duration<double>(Clock::now()-s.writingQueuedAt).count()});
        }
        // Take anything published meanwhile, then write again or arm the timer.
        Pump();
    }
};
IpcHost::IpcHost(MatchRuntimeHost& host,const Arena& arena):impl_(std::make_unique<Impl>(host,arena)){}
IpcHost::~IpcHost(){Stop();}
bool IpcHost::Start(const std::string& listenAddress,std::string& error) {
    try {
        if(impl_->role) throw std::logic_error("IpcHost is already started");
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
        impl_->listener.bind(endpoint); impl_->listener.listen(1);
        impl_->StartAccept();
        auto started=Engine::Threads::RoleThread::Start({"gyo-match-ipc",Engine::Threads::ThreadPriority::Normal},
            [impl=impl_.get()](Engine::Threads::RoleContext&) {
                // Between phases nothing may be pending (a reset completes on
                // the host's role); the work guard keeps run() waiting for its
                // notification. Stop requests the stop, then stops the context.
                // A handler's escaping exception ends the IPC thread visibly.
                const auto work=asio::make_work_guard(impl->io);
                try {impl->io.run();}
                catch(const std::exception& failure) {MatchEventLog::Line(std::string("ipc thread failed reason=")+failure.what());}
            });
        if(!started) throw std::runtime_error(Engine::Base::Describe(started.error()));
        impl_->role.emplace(std::move(*started));
        return true;
    } catch(const std::exception& failure) {error=failure.what();return false;}
}
void IpcHost::Stop() {
    impl_->Detach();
    if(!impl_->role) return;
    impl_->role->RequestStop();
    impl_->io.stop();
    impl_->role.reset();
    asio::error_code ignored;
    impl_->socket.close(ignored);
    impl_->listener.close(ignored);
}
std::uint64_t IpcHost::Iterations() const noexcept{return impl_->iterations.load(std::memory_order_relaxed);}
}
