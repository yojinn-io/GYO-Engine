#include "RetroFPS/Pvp/IpcHost.hpp"
#include "RetroFPS/Pvp/ClockSync.hpp"
#include "RetroFPS/Pvp/ClogLine.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "engine/threads/RoleThread.hpp"
#include "runtime_v7.pb.h"
#include "engine/base/Assert.hpp"
#include <google/protobuf/unknown_field_set.h>
#include <asio.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <deque>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

namespace fps::pvp {
namespace pb = object_fps_pvp::runtime::v7;
using asio::ip::tcp;
namespace {
// Diagnostics only: lifecycle events and a statistics line every ten seconds
// on std::clog (match_main copies it into --log). Never affects the session.
class MatchEventLog final {
public:
    static void Line(const std::string& text) {
        ClogLine() << "[ObjectFPS/PvP Match] " << text << '\n';
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
        state->set_last_hit_part(static_cast<pb::HitPart>(p.lastHitPart));
    }
    return message;
}
// pv7 §2a: a lane resends every unacknowledged decision, at most once per
// ActionInterval, until the Client's ACK retires it; the Gateway or a lost
// datagram may drop any frame, and results are idempotent. A frame carries the
// retirement and up to MaxActionBatch decisions: the ResendFirstSlots smallest
// ids first (the Client acknowledges only contiguously, so the smallest must
// never starve), the other slots cycling through the rest. The retirement goes
// once when it advances, and once more when asked (an ACK-only batch
// acknowledging no more than it: the Client missed it) or when a batch is
// refused (D-6).
// The host wakes Pump only on a step that publishes; with a snapshot every
// tick, a decision is seen on the tick that makes it.
static_assert(SnapshotIntervalTicks==1,"Action lanes rely on a publish every tick");
constexpr auto ActionInterval=std::chrono::nanoseconds((1'000'000'000+ActionSendRate-1)/ActionSendRate);
constexpr std::size_t ResendFirstSlots=4;
static_assert(ResendFirstSlots<MaxActionBatch,"Some slots must cycle, or ids above the first ones starve");
struct ActionLane {
    std::optional<std::chrono::steady_clock::time_point> lastSend;
    ActionId sentRetired{};
    // The last id a cycling slot carried; the next cycle starts above it.
    ActionId cursor{};
    // One send even with nothing unacknowledged and no new retirement.
    bool asked{};
    // Diagnostics only (pv7 §5, D-6): whole batches refused since the lane
    // began (the player's join on this link) and the latest reason.
    std::uint64_t refused{};
    pb::ActionRefusal lastRefusal{pb::ACTION_REFUSAL_UNSPECIFIED};
    // Diagnostics only (log-contract, E1-7): when this I/O thread first saw the
    // lane with something to send since its last send. A send is late only
    // beyond both this and the lane's due time (an idle lane may send at once).
    std::optional<std::chrono::steady_clock::time_point> pendingSince;
};
// The hit part's wire value is the domain's (pv7 §7).
static_assert(static_cast<int>(HitPart::Unspecified)==pb::HIT_PART_UNSPECIFIED && static_cast<int>(HitPart::Body)==pb::HIT_PART_BODY &&
              static_cast<int>(HitPart::Head)==pb::HIT_PART_HEAD && pb::HitPart_ARRAYSIZE==3,"HitPart differs from the wire");
// The wire name of each refusal: ActionRefusal is IngressActionRejection + 1,
// value for value (pv7 §5: the names are the host's).
constexpr pb::ActionRefusal RefusalForWire(IngressActionRejection reason) {
    return static_cast<pb::ActionRefusal>(IngressIndex(reason)+1);
}
static_assert(RefusalForWire(IngressActionRejection::OverBatch)==pb::ACTION_REFUSAL_OVER_BATCH);
static_assert(RefusalForWire(IngressActionRejection::Malformed)==pb::ACTION_REFUSAL_MALFORMED);
static_assert(RefusalForWire(IngressActionRejection::Resetting)==pb::ACTION_REFUSAL_RESETTING);
static_assert(RefusalForWire(IngressActionRejection::InvalidPlayer)==pb::ACTION_REFUSAL_INVALID_PLAYER);
static_assert(RefusalForWire(IngressActionRejection::InvalidBatch)==pb::ACTION_REFUSAL_INVALID_BATCH);
static_assert(RefusalForWire(IngressActionRejection::Conflict)==pb::ACTION_REFUSAL_CONFLICT);
static_assert(RefusalForWire(IngressActionRejection::OutsideWindow)==pb::ACTION_REFUSAL_OUTSIDE_WINDOW);
static_assert(RefusalForWire(IngressActionRejection::Full)==pb::ACTION_REFUSAL_FULL);
static_assert(IngressIndex(IngressActionRejection::Count)==8 && pb::ActionRefusal_ARRAYSIZE==9,"a refusal without a wire name");
// The one test for something to send: ScheduleWrite's deadline and Write's
// choice must agree, or a due lane with nothing to choose would wake writes
// forever.
bool LanePending(const ActionResults& results, const ActionLane& lane) {
    return !results.decisions.empty() || results.retiredThrough>lane.sentRetired || lane.asked;
}
// At most one choice per ActionInterval; the first may go at once.
std::chrono::steady_clock::time_point LaneDue(const ActionLane& lane, std::chrono::steady_clock::time_point now) {
    return lane.lastSend?*lane.lastSend+ActionInterval:now;
}
// Chooses the retirement and up to MaxActionBatch unacknowledged decisions
// (they arrive sorted by id): the smallest first, then the cycling slots above
// the cursor, wrapping to the lowest of the rest. Written in id order.
pb::RuntimeEnvelope ActionMessage(const ActionResults& results, ActionLane& lane, std::chrono::steady_clock::time_point now) {
    pb::RuntimeEnvelope message;message.set_protocol_version(wire::ProtocolVersion);
    auto* out=message.mutable_action_results();out->set_player_id(results.playerId);
    out->set_retired_through(results.retiredThrough);
    out->set_refused_batches(lane.refused);out->set_last_refusal(lane.lastRefusal);
    lane.lastSend=now;
    lane.sentRetired=results.retiredThrough;
    lane.asked=false;
    const auto& decisions=results.decisions;
    const auto first=std::min(decisions.size(),ResendFirstSlots);
    std::vector<std::size_t> chosen(first);
    std::iota(chosen.begin(),chosen.end(),std::size_t{0});
    const auto rest=decisions.size()-first;
    const auto cycling=std::min(rest,MaxActionBatch-first);
    const auto above=std::upper_bound(decisions.begin()+static_cast<std::ptrdiff_t>(first),decisions.end(),lane.cursor,
        [](ActionId cursor,const auto& decision){return cursor<decision.actionId;});
    const auto start=static_cast<std::size_t>(above-decisions.begin())-first;
    for(std::size_t k=0;k<cycling;++k) {
        const auto index=first+(start+k)%rest;
        chosen.push_back(index);
        lane.cursor=decisions[index].actionId;
    }
    std::sort(chosen.begin(),chosen.end());
    for(const auto index:chosen) {
        const auto& decision=decisions[index];
        auto* value=out->add_decisions();value->set_action_id(decision.actionId);
        value->set_resolved_tick(decision.resolvedTick);value->set_accepted(decision.accepted);
        value->set_rejection(RejectionForWire(decision.rejection));
        value->set_hit_kind(static_cast<pb::ShotHitKind>(decision.hitKind));
        value->set_target_id(decision.targetId);value->set_damage(decision.damage);
        value->set_kind(KindForWire(decision.kind));value->set_life_generation(decision.lifeGeneration);
        value->set_target_life_generation(decision.targetLifeGeneration);
        value->set_hit_part(static_cast<pb::HitPart>(decision.hitPart));
    }
    return message;
}
}
// Diagnostics only (pv7 §4): the Gateway's drop reports as the Match reads
// them, per stream. A violation is logged, counted and ignored (D-5, no veto:
// the link stays open); every report is logged as read, the same body the
// Gateway logs as written, so L2 can compare them by report_sequence. Nothing
// here reaches the host (D32).
class DropReports {
public:
    void Read(const pb::GatewayDropReport& report) {
        auto& stream=streams_[report.player_id()];
        std::string body="player="+std::to_string(report.player_id())+" report_sequence="+std::to_string(report.report_sequence())+
            " final="+(report.final()?"1":"0")+" samples="+std::to_string(report.samples_size())+
            " samples_omitted="+std::to_string(report.samples_omitted())+" merged_streams="+std::to_string(report.merged_streams());
        std::vector<std::string> problems;
        if(stream.final) problems.push_back("after_final");
        if(report.report_sequence()!=stream.sequence+1) problems.push_back("sequence");
        if(report.merged_streams()<stream.merged || (report.player_id()!=0 && report.merged_streams()!=0)) problems.push_back("merged_streams");
        if(report.samples_omitted()<stream.omitted) problems.push_back("samples_omitted");
        std::map<int,std::uint64_t> totals;
        int previous=0;
        for(const auto& count:report.counts()) {
            const int reason=count.reason();
            body+=" "+Key(reason)+"="+std::to_string(count.total());
            const bool inScope=report.player_id()==0?(reason>=40 && reason<=45):(reason>=1 && reason<=34);
            if(!pb::DropReason_IsValid(reason) || !inScope) problems.push_back("reason_range");
            if(reason<=previous || count.total()==0) problems.push_back("counts_order");
            previous=reason;
            totals[reason]=count.total();
        }
        for(const auto& [reason,total]:stream.totals)
            if(const auto found=totals.find(reason);found==totals.end() || found->second<total) {problems.push_back("counts_regress");break;}
        if(report.samples_size()>4) problems.push_back("samples_over_4");
        for(const auto& sample:report.samples())
            if(!totals.contains(sample.reason())) {problems.push_back("sample_reason");break;}
        for(const auto& sample:report.samples())
            if(sample.dwell_us()>10'000'000) {problems.push_back("dwell_range");break;} // pv7 §2 clamps it
        MatchEventLog::Line("gw_drop report "+body);
        for(const auto& problem:problems) {
            MatchEventLog::Line("gw_drop violation player="+std::to_string(report.player_id())+" report_sequence="+
                std::to_string(report.report_sequence())+" problem="+problem);
        }
        // The stream goes on from what was read, trusted or not.
        stream.sequence=report.report_sequence();stream.totals=std::move(totals);stream.final=stream.final || report.final();
        stream.omitted=std::max(stream.omitted,report.samples_omitted());
        if(report.player_id()==0) stream.merged=std::max(stream.merged,report.merged_streams()); // only the Gateway stream merges
    }
private:
    // The ingress key of a reason: its proto name in lower case (pv7 §4).
    static std::string Key(int reason) {
        std::string name=pb::DropReason_IsValid(reason)?pb::DropReason_Name(static_cast<pb::DropReason>(reason)):"drop_reason_"+std::to_string(reason);
        for(auto& c:name) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return name;
    }
    struct Stream {std::uint64_t sequence{},omitted{},merged{};std::map<int,std::uint64_t> totals;bool final{};};
    std::map<std::uint64_t,Stream> streams_;
};

// Diagnostics only (pv7 §2, §5; D32): per player, the Client's GYOP sequence
// and the Gateway dwell the input and action messages carry, logged every ten
// seconds and at the session end as gw_transport lines. A gap is the
// sequences skipped between two messages (Hello, the Gateway's reported drops
// and network loss together; reconciled offline with the Gateway's counts); a
// late one is not above the largest seen. A missing or out-of-range value is
// counted and ignored (D-5). Nothing here reaches the host.
class CauseRecords {
public:
    void Observe(PlayerId player,const std::optional<std::uint32_t>& sequence,const std::optional<std::uint32_t>& dwell) {
        auto& p=players_[player];
        ++p.messages;
        if(!sequence || !*sequence) ++p.sequenceMissing;
        else if(*sequence<=p.largest) ++p.sequenceLate;
        else {
            if(p.largest) p.sequenceGaps+=*sequence-p.largest-1;
            p.largest=*sequence;
        }
        if(!dwell) ++p.dwellMissing;
        else if(*dwell>10'000'000) ++p.dwellRange; // pv7 §2 clamps it
        else {++p.dwellSamples;p.dwellSum+=*dwell;p.dwellMax=std::max<std::uint64_t>(p.dwellMax,*dwell);}
    }
    // Logs and starts a new window once ten seconds passed, or at the end.
    void Roll(std::chrono::steady_clock::time_point now,bool final=false) {
        if(!started_) started_=now;
        if(!final && now-*started_<std::chrono::seconds(10)) return;
        const auto window=std::chrono::duration_cast<std::chrono::milliseconds>(now-*started_).count();
        for(auto& [player,p]:players_) {
            std::ostringstream line;
            line<<"gw_transport player="<<player<<" window_ms="<<window<<" final="<<(final?1:0)<<" messages="<<p.messages
                <<" sequence_missing="<<p.sequenceMissing<<" sequence_gaps="<<p.sequenceGaps<<" sequence_late="<<p.sequenceLate
                <<" largest_sequence="<<p.largest<<" dwell_samples="<<p.dwellSamples<<" dwell_missing="<<p.dwellMissing
                <<" dwell_range="<<p.dwellRange<<" dwell_max_us="<<p.dwellMax
                <<" dwell_mean_us="<<(p.dwellSamples?p.dwellSum/p.dwellSamples:0);
            MatchEventLog::Line(line.str());
            p=Player{.largest=p.largest}; // the sequence goes on across windows
        }
        started_=now;
    }
private:
    struct Player {
        std::uint64_t messages{},sequenceMissing{},sequenceGaps{},sequenceLate{},largest{};
        std::uint64_t dwellSamples{},dwellMissing{},dwellRange{},dwellSum{},dwellMax{};
    };
    std::map<PlayerId,Player> players_;
    std::optional<std::chrono::steady_clock::time_point> started_;
};

// The wire name of a Leave reason in the log; unspecified for 0 and any value
// this version does not know (pv7 §5).
std::string_view LeaveReasonName(int reason) {
    switch(reason) {
    case pb::LEAVE_REASON_CLIENT_REQUEST: return "client_request";
    case pb::LEAVE_REASON_SESSION_TIMEOUT: return "session_timeout";
    case pb::LEAVE_REASON_JOIN_TIMEOUT: return "join_timeout";
    default: return "unspecified";
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
    // The M clock domain of pv7 §2: steady_clock, as the Match trace's time_ns.
    static std::uint64_t NowMicros() {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count());
    }
    // The host notifies from its simulation role; Stop detaches this gate
    // first, so no notification reaches a stopped or destroyed IpcHost.
    struct WakeGate {std::mutex mutex;Impl* impl{};};
    enum class Phase {Idle,Accepting,Resetting,Connected,Closing};
    // samples: the players whose movement slack sample the frame carries,
    // counted by the host when latest-wins replaces the frame unwritten.
    // publishedAt (the host's clock) starts the match_hold_us the frame gets
    // when it is chosen for writing (pv7 §5, D-4); a replaced one never does.
    struct SnapshotFrame {pb::RuntimeEnvelope message;std::uint64_t tick;Clock::time_point queuedAt;std::vector<PlayerId> samples;
        std::optional<Clock::time_point> publishedAt;};
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
        std::uint64_t frames{}; // frames of this connection that passed the version check
        // Diagnostics only (pv7 §2, §3): the clock sampling with the Gateway
        // and the one reply to its latest heartbeat (newest wins), written
        // after controls, action lanes and snapshots; never in controls.
        ClockSync clock;
        std::optional<pb::ClockSample> clockReply;
        DropReports drops; // the Gateway's drop reports on this connection
        CauseRecords transport; // the Client sequences and dwell the Gateway forwarded
        std::uint64_t leaveReasonViolations{};
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
    // Diagnostics only (E1-7): written on the io thread, taken by match_main
    // at its statistics window; the mutex guards only these counts.
    std::mutex laneMutex;
    IpcLaneStatistics lanes;
    void SnapshotChosen(Clock::time_point queuedAt, Clock::time_point now) {
        std::scoped_lock lock(laneMutex);
        ++lanes.snapshotWrites;
        lanes.snapshotWait.Record(now > queuedAt ? now - queuedAt : Clock::duration::zero());
    }
    void ActionChosen(Clock::time_point due, Clock::time_point now) {
        std::scoped_lock lock(laneMutex);
        ++lanes.actionWrites;
        lanes.actionLate.Record(now > due ? now - due : Clock::duration::zero());
    }
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
        session.clock=ClockSync(NowMicros());
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
        rules->set_maximum_shot_damage(PvpCombatRules.maximumShotDamage);
        rules->set_maximum_reference_age_ms(static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(PvpCombatRules.maximumReferenceAge).count()));
        session.controls.push_back(wire::Frame(ready.SerializeAsString()));
        StartRead();
        Pump();
    }
    // The log every session end writes once: its last clock window and why
    // it ended (Close, and Stop for a session still connected).
    void LogEnd(const std::string& reason) {
        if(const auto window=session.clock.Roll(NowMicros(),ClockWindowEnd::Final)) MatchEventLog::Line(ClockWindowLine(*window));
        session.transport.Roll(Clock::now(),true);
        MatchEventLog::Line("ipc closed reason="+reason);
    }
    // The single path that ends a session while the Match runs: log why,
    // close, reset the Match, then accept the next connection once the reset
    // completed.
    void Close(const std::string& reason) {
        if(phase!=Phase::Connected) return;
        LogEnd(reason);
        // A Gateway of another version refuses Ready and closes without a
        // frame; a pv7 Gateway sends its version first (pv7 contract §1). An
        // idle Gateway of this version also sends nothing before it stops.
        if(reason=="eof" && session.frames==0)
            MatchEventLog::Line("ipc hint: the Gateway closed before sending any frame; it was idle or speaks another protocol version");
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
    void AskLane(PlayerId player) {
        if(const auto lane=session.actionLanes.find(player);lane!=session.actionLanes.end()) lane->second.asked=true;
    }
    // A refused batch: counted for the Client's ActionResults (D-6) and
    // answered with one more send of what the lane holds.
    void Refused(PlayerId player,IngressActionRejection reason) {
        if(const auto lane=session.actionLanes.find(player);lane!=session.actionLanes.end()) {
            ++lane->second.refused;lane->second.lastRefusal=RefusalForWire(reason);lane->second.asked=true;
        }
    }
    // The frame of a snapshot chosen for writing now: its Match hold, from the
    // host's publication to now on the host's clock (pv7 §5, D-4).
    std::vector<std::uint8_t> SnapshotBytes(SnapshotFrame& frame) {
        if(frame.publishedAt) {
            const auto hold=std::chrono::duration_cast<std::chrono::microseconds>(host.Now()-*frame.publishedAt).count();
            frame.message.mutable_snapshot()->set_match_hold_us(static_cast<std::uint32_t>(std::clamp<std::int64_t>(hold,0,10'000'000)));
        }
        return wire::Frame(frame.message.SerializeAsString());
    }
    // Names a message this side does not take: a known one sent the wrong way,
    // field numbers this version does not know, or an envelope with none.
    static std::string UnknownMessage(const pb::RuntimeEnvelope& message) {
        if(message.message_case()!=pb::RuntimeEnvelope::MESSAGE_NOT_SET)
            return "unknown message field="+std::to_string(static_cast<int>(message.message_case()));
        const auto& unknown=message.GetReflection()->GetUnknownFields(message);
        if(unknown.field_count()==0) return "unknown message: empty";
        std::string fields;
        for(int i=0;i<unknown.field_count();++i) fields+=(i?",":"")+std::to_string(unknown.field(i).number());
        return "unknown message fields="+fields;
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
            if(!message.ParseFromArray(input.data()+4,static_cast<int>(length))) return "frame parse";
            // The version is checked before the content (pv7 contract §1).
            if(message.protocol_version()!=wire::ProtocolVersion)
                return "protocol version mismatch: expected "+std::to_string(wire::ProtocolVersion)+", received "+
                    std::to_string(message.protocol_version());
            ++session.frames;
            input.erase(input.begin(),input.begin()+static_cast<std::ptrdiff_t>(length+4));
            if(message.has_join()) {
                if(session.joins.size()>=64) return "pending joins over 64";
                const auto request=++requestSequence;
                if(!host.QueueJoin(request,message.join().player_id())) return "join refused by the Match queue";
                session.joins.emplace(request,message.join().player_id());
            } else if(message.has_leave()) {
                // The reason is diagnostics only: every Leave removes the
                // player the same way; a missing or unknown one is counted.
                const auto reason=static_cast<int>(message.leave().reason());
                const auto name=LeaveReasonName(reason);
                MatchEventLog::Line("leave player="+std::to_string(message.leave().player_id())+" reason="+std::string(name));
                if(name=="unspecified")
                    MatchEventLog::Line("leave violation player="+std::to_string(message.leave().player_id())+" value="+
                        std::to_string(reason)+" count="+std::to_string(++session.leaveReasonViolations));
                if(!host.QueueLeave(++requestSequence,message.leave().player_id())) return "leave refused by the Match queue";
                session.actionLanes.erase(message.leave().player_id());
            } else if(message.has_input()) {
                const auto& p=message.input();
                session.transport.Observe(p.player_id(),p.has_client_transport_sequence()?std::optional(p.client_transport_sequence()):std::nullopt,
                    p.has_gateway_dwell_us()?std::optional(p.gateway_dwell_us()):std::nullopt);
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
                session.transport.Observe(value.player_id(),value.has_client_transport_sequence()?std::optional(value.client_transport_sequence()):std::nullopt,
                    value.has_gateway_dwell_us()?std::optional(value.gateway_dwell_us()):std::nullopt);
                const auto shots=static_cast<std::size_t>(value.shots_size());
                if(shots>MaxActionBatch) {
                    host.NoteWireRejection(value.player_id(),IngressActionRejection::OverBatch,shots);
                    Refused(value.player_id(),IngressActionRejection::OverBatch);
                    continue;
                }
                ActionBatch batch{value.player_id(),{}};
                bool valid=true;
                for(const auto& shot:value.shots()) {
                    const bool shooting=shot.kind()==pb::ACTION_SHOT;
                    const bool reloading=shot.kind()==pb::ACTION_RELOAD;
                    if(!shot.life_generation() || (!shooting && !reloading) ||
                       (shooting && (!shot.has_yaw() || !shot.has_pitch())) ||
                       (reloading && (shot.has_yaw() || shot.has_pitch()))) {valid=false;break;}
                    batch.shots.push_back({shot.action_id(),shot.observed_authority_tick(),shot.yaw(),shot.pitch(),
                        shooting?ActionKind::Shot:ActionKind::Reload,shot.life_generation(),
                        shot.movement_epoch(),shot.command_sequence(),shot.view_tick_q16()});
                }
                if(!valid) {
                    host.NoteWireRejection(value.player_id(),IngressActionRejection::Malformed,shots);
                    Refused(value.player_id(),IngressActionRejection::Malformed);
                    continue;
                }
                // Admission is distinct from a terminal shot decision. The
                // immutable request remains with the sender for retry. The
                // host counts the admission by reason.
                // An ACK-only batch acknowledging no more than the retirement
                // shows the Client missed it; a refusal answers with what the
                // lane holds (D-6). Either asks the lane for one more send.
                const auto acknowledged=value.acknowledged_through();
                const bool missed=!shots && acknowledged>0 && [&] {
                    const auto retired=host.GetActionResults(value.player_id());
                    return retired && acknowledged<=retired->retiredThrough;
                }();
                auto refusal=IngressActionRejection::Count;
                if(host.SubmitActionBatch(batch,value.acknowledged_through(),&refusal)!=ActionAdmission::Accepted)
                    Refused(value.player_id(),refusal);
                else if(missed) AskLane(value.player_id());
            } else if(message.has_drop_report()) {
                session.drops.Read(message.drop_report()); // diagnostics only: logged, checked, never answered
            } else if(message.has_clock()) {
                // Diagnostics only: measured here and answered once; nothing
                // reaches the host (D32). A semantic violation is counted and
                // ignored (pv7 §2).
                const auto& sample=message.clock();
                ClockStamp peer{.transmitUs=sample.transmit_us()};
                if(sample.has_origin_us()) peer.originUs=sample.origin_us();
                if(sample.has_receive_us()) peer.receiveUs=sample.receive_us();
                if(const auto echo=session.clock.Observe(peer,NowMicros())) {
                    if(session.clockReply) session.clock.NoteReplaced();
                    session.clockReply.emplace();
                    session.clockReply->set_origin_us(echo->first);
                    session.clockReply->set_receive_us(echo->second);
                }
            } else return UnknownMessage(message);
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
                if(found==session.joins.end()) {
                    // A Leave's result is not relayed. A Join's request was
                    // recorded on this thread before the host could answer it.
                    GYO_ASSERT(result.kind==ControlKind::Leave);
                    continue;
                }
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
            auto published=Clock::time_point::min();
            if(auto snapshot=host.TakeSnapshot(&published)) {
                session.events.Observe(*snapshot,session.coalescedSnapshots);
                if(session.latestSnapshot) {
                    ++session.coalescedSnapshots;
                    host.NoteCoalescedSlackSamples(session.latestSnapshot->samples);
                    TraceMovement({.kind=MovementTraceKind::Transport,.authorityTick=snapshot->tick,
                        .count=session.coalescedSnapshots,.ageSeconds=std::chrono::duration<double>(Clock::now()-session.latestSnapshot->queuedAt).count()});
                }
                std::vector<PlayerId> samples;
                for(const auto& player:snapshot->players) if(player.movementSlackSequence) samples.push_back(player.playerId);
                session.latestSnapshot=SnapshotFrame{SnapshotMessage(*snapshot),snapshot->tick,Clock::now(),std::move(samples),
                    published==Clock::time_point::min()?std::nullopt:std::optional(published)};
            }
            if(const auto window=session.clock.Roll(NowMicros())) MatchEventLog::Line(ClockWindowLine(*window));
            session.transport.Roll(Clock::now());
            ScheduleWrite();
        } catch(const std::exception& failure) {Close(std::string("exception: ")+failure.what());}
    }

    // Waits for the socket to become writable when there is something to
    // write; otherwise arms the lane timer for the earliest lane with
    // something to send. The frame is chosen only when the socket is writable.
    void ScheduleWrite() {
        if(phase!=Phase::Connected || waitingWrite) return;
        const auto now=Clock::now();
        std::optional<Clock::time_point> nextLane;
        for(auto it=session.actionLanes.begin();it!=session.actionLanes.end();) {
            const auto results=host.GetActionResults(it->first);
            if(!results) {it=session.actionLanes.erase(it);continue;}
            if(!LanePending(*results,it->second)) {++it;continue;}
            if(!it->second.pendingSince) it->second.pendingSince=now;
            const auto due=LaneDue(it->second,now);
            if(!nextLane || due<*nextLane) nextLane=due;
            ++it;
        }
        const bool laneDue=nextLane && *nextLane<=now;
        if(!session.writing.empty() || !session.controls.empty() || session.latestSnapshot || laneDue || session.clockReply) {
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
            s.writing=SnapshotBytes(*s.latestSnapshot);s.writingTick=s.latestSnapshot->tick;
            s.writingQueuedAt=s.latestSnapshot->queuedAt;s.writingSamples=std::move(s.latestSnapshot->samples);s.latestSnapshot.reset();
            SnapshotChosen(s.writingQueuedAt,Clock::now());
            TraceMovement({.kind=MovementTraceKind::Transport,.authorityTick=s.writingTick,.count=s.coalescedSnapshots});
        }
        if(s.writing.empty()) {
            if(!s.controls.empty()) {
                s.writing=std::move(s.controls.front());s.controls.pop_front();s.writingSnapshot=false;s.writingTick=0;s.writingQueuedAt=Clock::now();s.writingSamples.clear();
            } else {
                // Results stay in Match until Client ACK. Only choose a batch
                // when a write slot is free; a blocked socket cannot accumulate
                // or overwrite a separate decision queue. A lane with nothing
                // to send keeps its anchor: the next send may go at once.
                const auto now=Clock::now();
                for(auto it=s.actionLanes.begin();it!=s.actionLanes.end();) {
                    auto& [playerId,lane]=*it;
                    if(now<LaneDue(lane,now)) {++it;continue;}
                    const auto results=host.GetActionResults(playerId);
                    if(!results) {it=s.actionLanes.erase(it);continue;}
                    ++it;
                    if(!LanePending(*results,lane)) continue;
                    ActionChosen(std::max(LaneDue(lane,now),lane.pendingSince.value_or(now)),now);
                    lane.pendingSince.reset();
                    s.writing=wire::Frame(ActionMessage(*results,lane,now).SerializeAsString());
                    s.writingSnapshot=false;s.writingTick=0;s.writingQueuedAt=now;s.writingSamples.clear();
                    break;
                }
            }
            if(s.writing.empty() && s.latestSnapshot) {
                s.writing=SnapshotBytes(*s.latestSnapshot);s.writingTick=s.latestSnapshot->tick;
                s.writingQueuedAt=s.latestSnapshot->queuedAt;s.writingSnapshot=true;s.writingSamples=std::move(s.latestSnapshot->samples);s.latestSnapshot.reset();
                SnapshotChosen(s.writingQueuedAt,Clock::now());
            }
            if(s.writing.empty() && s.clockReply) {
                // Last in priority; transmit is stamped now, when the frame is chosen.
                pb::RuntimeEnvelope message;message.set_protocol_version(wire::ProtocolVersion);
                *message.mutable_clock()=*s.clockReply;s.clockReply.reset();
                message.mutable_clock()->set_transmit_us(s.clock.Stamp(NowMicros()));
                s.writing=wire::Frame(message.SerializeAsString());
                s.writingSnapshot=false;s.writingTick=0;s.writingQueuedAt=Clock::now();s.writingSamples.clear();
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
        MatchEventLog::Line(RoleStartedLine(*impl_->role));
        return true;
    } catch(const std::exception& failure) {error=failure.what();return false;}
}
void IpcHost::Stop() {
    impl_->Detach();
    if(!impl_->role) return;
    // A session still connected ends with the log every end writes, on the
    // IPC thread that owns it; no reset (the Match is going away). Waited for
    // a bounded time: an IPC thread that already failed runs nothing more.
    auto ended=std::make_shared<std::promise<void>>();
    auto done=ended->get_future();
    asio::post(impl_->io,[impl=impl_.get(),ended] {
        if(impl->phase==Impl::Phase::Connected) {
            impl->LogEnd("stopped");
            impl->phase=Impl::Phase::Idle;
        }
        ended->set_value();
    });
    if(done.wait_for(std::chrono::seconds(2))!=std::future_status::ready)
        MatchEventLog::Line("ipc stop: the IPC thread did not end the session within 2 s");
    impl_->role->RequestStop();
    impl_->io.stop();
    impl_->role.reset();
    asio::error_code ignored;
    impl_->socket.close(ignored);
    impl_->listener.close(ignored);
}
std::uint64_t IpcHost::Iterations() const noexcept{return impl_->iterations.load(std::memory_order_relaxed);}
IpcLaneStatistics IpcHost::TakeLaneStatistics(){std::scoped_lock lock(impl_->laneMutex);return std::exchange(impl_->lanes,{});}
}
