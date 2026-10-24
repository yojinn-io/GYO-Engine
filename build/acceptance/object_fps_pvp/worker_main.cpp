// Product-owned socket acceptance for ClientConnection's real background worker.
// The mock is external to production: HTTP joins and current-wire UDP movement and action transport.
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "client_v7.pb.h"
#include "acceptance_protocol.hpp"
#include "acceptance_capacity.hpp"
#include <asio.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <source_location>
#include <iostream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace fps::pvp;
using Clock=std::chrono::steady_clock;
using namespace std::chrono_literals;
namespace pb=object_fps_pvp::client::v7;
using asio::ip::udp;
using Json=nlohmann::json;
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void Require(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
struct Attempt { Clock::time_point at; pb::PlayerInput input; };
struct ActionAttempt { Clock::time_point at; pb::ActionBatch actions; };
struct PacketAttempt { Clock::time_point at; wire::Type type; std::size_t bytes; std::uint32_t sequence{}; };
// Opaque content digest of the mock arena (pv6 contract §2).
constexpr std::uint64_t WorkerArenaDigest=0x0123456789abcdefULL;
class MockGateway {
public:
    MockGateway():socket(io,udp::endpoint(asio::ip::address_v4::loopback(),0)) {
        socket.non_blocking(true);
        http.Get("/rooms",[](const auto&,auto& response){response.set_content(R"({"rooms":[]})","application/json");});
        http.Post("/rooms/1/join",[&](const auto& request,auto& response){
            const auto body=Json::parse(request.body);
            std::optional<std::pair<int,Json>> refused;
            {std::scoped_lock lock(refusalMutex);refused=refusal;}
            // A Client of another version is refused naming this version (pv7 contract §1).
            if(!refused && body.value("protocol_version",0u)!=AcceptanceProtocolVersion)
                refused={409,Json{{"error","protocol_version"},{"protocol_version",AcceptanceProtocolVersion}}};
            if(refused) {response.status=refused->first;response.set_content(refused->second.dump(),"application/json");return;}
            Json reply={{"session_id",1},{"session_token","worker-test"},{"player_id",1},{"protocol_version",protocolVersion.load()},
                {"arena_id","worker-test"},{"arena_version",1},{"arena_digest",joinArenaDigest.load()},{"udp_ip","127.0.0.1"},{"udp_port",socket.local_endpoint().port()}};
            if(replyWithoutVersion)reply.erase("protocol_version");
            response.set_content(reply.dump(),"application/json");
        });
        http.Post("/rooms/1/leave",[&](const auto&,auto& response){
            ++leaves;response.set_content(R"({"left":true})","application/json");
        });
        const auto port=http.bind_to_any_port("127.0.0.1");
        Require(port>0,"mock HTTP bind failed");
        address="127.0.0.1:"+std::to_string(port);
        server=std::thread([this]{http.listen_after_bind();});
    }
    ~MockGateway(){http.stop();if(server.joinable())server.join();}
    void Pump() {
        for(int count=0;count<64;++count) {
            std::array<std::uint8_t,2048> buffer{};udp::endpoint source;asio::error_code error;
            const auto size=socket.receive_from(asio::buffer(buffer),source,0,error);
            if(error==asio::error::would_block || error==asio::error::try_again)return;
            if(error)throw std::runtime_error(error.message());
            const auto packet=wire::Decode(std::span(buffer).first(size));
            Require(packet.has_value() && packet->session==1,"invalid worker UDP envelope");
            peer=source;packets.push_back({Clock::now(),packet->type,size,packet->sequence});
            if(silent)continue; // an outage in both directions: seen, never answered
            if(packet->type==wire::Type::Hello) {
                pb::Hello hello;Require(hello.ParseFromString(packet->payload) && hello.session_token()=="worker-test","invalid Hello");
                // A snapshot and a results frame before the first Welcome (CheckDroppedDatagrams).
                if(std::exchange(earlyBeforeWelcome,false)) {Snapshot(ack);SendResults(pb::ActionResults{});}
                pb::Welcome welcome;welcome.set_player_id(1);welcome.set_match_id(1);welcome.set_tick_rate(60);
                welcome.set_snapshot_rate(60);welcome.set_jump_height(.6F);welcome.set_gravity(18);welcome.set_arena_id("worker-test");welcome.set_arena_version(1);welcome.set_arena_digest(welcomeArenaDigest.load());
                if(includeRules) {
                    auto* rules=welcome.mutable_combat_rules();rules->set_maximum_hp(200);rules->set_shot_damage(37);
                    rules->set_cooldown_ticks(9);rules->set_shot_range(140);rules->set_maximum_reference_age_ms(333);
                    rules->set_magazine_capacity(7);rules->set_reload_ticks(88);rules->set_respawn_ticks(177);
                    rules->set_maximum_shot_damage(111); // pv7 §7: shot_damage <= maximum_shot_damage <= maximum_hp
                }
                // The time echo (pv7 §3): echo the Hello, stamp this Welcome. A bad
                // echo (receive after transmit) must change nothing for the Client.
                if(hello.has_clock())helloClocks.push_back(hello.clock());
                auto* clock=welcome.mutable_clock();welcomeTransmit+=1000;
                if(hello.has_clock()) {clock->set_origin_us(hello.clock().transmit_us());clock->set_receive_us(welcomeTransmit-(badWelcomeClock?0:1));}
                if(badWelcomeClock)clock->set_receive_us(welcomeTransmit+1);
                clock->set_transmit_us(welcomeTransmit);welcomeTransmits.push_back(welcomeTransmit);
                // Extreme diagnostics (the differential oracle of CheckOutage):
                // the liveness decisions must not see them.
                // Valid samples (the right origin, increasing transmits) with an
                // offset of about 2^52 us.
                if(extremeWelcomeClock && hello.has_clock()) {clock->set_receive_us(clock->receive_us()+ExtremeOffsetUs);
                    clock->set_transmit_us(welcomeTransmit+ExtremeOffsetUs);}
                Send(wire::Type::Welcome,welcome.SerializeAsString());
                if(snapshotOnHello)Snapshot(ack);
            } else if(packet->type==wire::Type::Input) {
                pb::PlayerInput input;Require(input.ParseFromString(packet->payload) && input.movement_epoch()>0 && input.movement_epoch()<=epoch &&
                    input.commands_size()>0 && input.commands_size()<=12,"invalid worker input window");
                attempts.push_back({Clock::now(),input});
                if(autoAck)Snapshot(input.commands(input.commands_size()-1).sequence());
            } else if(packet->type==wire::Type::Actions) {
                pb::ActionBatch message;
                Require(message.ParseFromString(packet->payload) && message.shots_size()<=MaxActionBatch &&
                    (message.shots_size() || message.acknowledged_through()),"invalid worker action batch");
                actionAttempts.push_back({Clock::now(),message});
            }
        }
    }
    // A timeout names the waiting line, so a failure points at the step that stalled.
    void Until(const std::function<bool()>& condition,std::chrono::milliseconds timeout=3000ms,
               std::source_location where=std::source_location::current()) {
        const auto deadline=Clock::now()+timeout;
        while(true) {
            Pump();if(condition())return;
            Require(Clock::now()<deadline,"mock worker wait timed out at worker_main.cpp:"+std::to_string(where.line()));
            std::this_thread::sleep_for(100us);
        }
    }
    void UntilTime(Clock::time_point deadline,std::source_location where=std::source_location::current()) {
        Until([&]{return Clock::now()>=deadline;},3000ms,where);
    }
    void Snapshot(std::uint64_t resolved) {
        ack=resolved;snapshotSentAt=Clock::now();pb::WorldSnapshot snapshot;snapshot.set_tick(++tick);
        auto* player=snapshot.add_players();player->set_player_id(1);player->set_movement_epoch(epoch);player->set_last_resolved_command(ack);player->set_life_generation(1);player->set_life_state(pb::LIFE_ALIVE);player->set_grounded(true);
        auto* combat=snapshot.add_combat();combat->set_player_id(1);combat->set_hp(74);combat->set_next_allowed_shot_tick(900);combat->set_life_generation(1);combat->set_magazine_ammo(7);
        combat->set_last_damage_tick(tick);combat->set_damage_count(3);combat->set_last_attacker_id(2);
        combat->set_last_hit_part(pb::HIT_PART_BODY); // pv7 §7 hit record: all four fields set together
        Send(wire::Type::Snapshot,snapshot.SerializeAsString());
    }
    // A snapshot of the given tick; the next Snapshot follows it.
    void SnapshotAt(std::uint64_t at,std::uint64_t resolved){tick=at-1;Snapshot(resolved);}
    // A snapshot of the given number of players (ids 1..count), player 1 first.
    void Crowded(unsigned count) {
        pb::WorldSnapshot snapshot;snapshot.set_tick(++tick);
        for(unsigned id=1;id<=count;++id) {
            auto* player=snapshot.add_players();player->set_player_id(id);player->set_movement_epoch(epoch);player->set_last_resolved_command(ack);player->set_life_generation(1);player->set_life_state(pb::LIFE_ALIVE);player->set_grounded(true);
            auto* combat=snapshot.add_combat();combat->set_player_id(id);combat->set_hp(100);combat->set_life_generation(1);combat->set_magazine_ammo(7);
        }
        Send(wire::Type::Snapshot,snapshot.SerializeAsString());
    }
    void Results(const std::vector<ActionId>& ids,ActionId retired=0) {
        pb::ActionResults results;results.set_retired_through(retired);
        for(const auto id:ids) {
            auto* d=results.add_decisions();d->set_action_id(id);d->set_resolved_tick(123);
            d->set_rejection(pb::REJECTION_COOLDOWN);d->set_kind(pb::ACTION_SHOT);d->set_life_generation(1);
        }
        SendResults(results);
    }
    void SendResults(const pb::ActionResults& results){Send(wire::Type::ActionResults,results.SerializeAsString());}
    void FailConnection() {
        pb::Error error;error.set_code("worker_test_failure");error.set_message("Intentional worker lifecycle failure");
        Send(wire::Type::Error,error.SerializeAsString());
    }
    std::string address;
    std::vector<Attempt> attempts;
    std::vector<ActionAttempt> actionAttempts;
    std::vector<PacketAttempt> packets;
    // The join route answers every join with this status and body until Accept().
    void Refuse(int status,Json body){std::scoped_lock lock(refusalMutex);refusal.emplace(status,std::move(body));}
    void Accept(){std::scoped_lock lock(refusalMutex);refusal.reset();}
    std::atomic<unsigned> protocolVersion{AcceptanceProtocolVersion};
    std::atomic<bool> replyWithoutVersion{};
    // The time echo seen on Hellos and the transmits of the Welcomes sent.
    std::vector<pb::ClockSample> helloClocks;
    std::vector<std::uint64_t> welcomeTransmits;
    std::uint64_t welcomeTransmit{};
    bool badWelcomeClock{},extremeWelcomeClock{};
    // CheckOutage: drop everything (silent), or answer a Hello with a Welcome only.
    bool silent{},snapshotOnHello{true};
    bool earlyBeforeWelcome{};
    void SendRaw(wire::Type type,const std::string& payload){Send(type,payload);}
    Clock::time_point snapshotSentAt{}; // when Snapshot last sent one
    static constexpr std::uint64_t ExtremeOffsetUs=std::uint64_t{1}<<52;
    std::atomic<std::uint64_t> joinArenaDigest{WorkerArenaDigest},welcomeArenaDigest{WorkerArenaDigest};
    bool includeRules{true};
    std::uint64_t epoch{1};
    bool autoAck{};
    std::atomic<std::uint64_t> leaves{};
private:
    void Send(wire::Type type,const std::string& payload){const auto bytes=wire::Encode({type,1,++sequence,payload});socket.send_to(asio::buffer(bytes),peer);}
    asio::io_context io;
    udp::socket socket;
    udp::endpoint peer;
    httplib::Server http;
    std::thread server;
    std::uint64_t ack{},tick{};
    std::uint32_t sequence{};
    std::mutex refusalMutex;
    std::optional<std::pair<int,Json>> refusal;
};
PlayerInput Input(std::uint64_t first,std::uint64_t last) {
    PlayerInput result;result.playerId=1;
    for(auto sequence=first;sequence<=last;++sequence)result.commands.push_back({sequence,1,0,0,0});
    return result;
}
// network-architecture §4 (v7.1 outage-resync client-interrupted): what one
// outage did, compared between a normal and an extreme time echo. Mock and
// Client share one process and one steady_clock, so each decision is timed
// against the moment the mock sent the last snapshot: liveness may read only
// arrival times (the differential oracle of §7). The booleans compare equal
// between the runs; the latencies, within a tolerance.
struct OutageOutcome {
    bool interruptedOnTime{},trackedLastSnapshot{},keptSession{},probedFaster{},resumed{},windowKept{},sequenceKept{},
        timedOutOnTime{};
    double interruptMs{},timeoutMs{};
    bool operator==(const OutageOutcome& other) const {
        return interruptedOnTime==other.interruptedOnTime && trackedLastSnapshot==other.trackedLastSnapshot &&
            keptSession==other.keptSession && probedFaster==other.probedFaster && resumed==other.resumed &&
            windowKept==other.windowKept && sequenceKept==other.sequenceKept && timedOutOnTime==other.timedOutOnTime;
    }
};
// A decision on time: no earlier than its threshold after the last snapshot was
// sent, and not later than a loopback arrival and a worker wake can explain.
constexpr double DecisionLateToleranceMs=100;
constexpr double RunsToleranceMs=50;
bool OnTime(double latencyMs,std::chrono::milliseconds threshold) {
    return latencyMs>=static_cast<double>(threshold.count())-1 && latencyMs<=static_cast<double>(threshold.count())+DecisionLateToleranceMs;
}
double MillisSince(Clock::time_point from,Clock::time_point to){return std::chrono::duration<double,std::milli>(to-from).count();}
OutageOutcome RunOutage(MockGateway& gateway,ClientConnection& connection,bool extremeClock) {
    OutageOutcome outcome;
    gateway.extremeWelcomeClock=extremeClock;
    connection.Join(gateway.address,"1");
    gateway.Until([&]{return connection.State().phase==ConnectionPhase::Playing;});
    const auto shot=connection.SubmitShot(connection.State().snapshot->tick,0,0);
    Require(shot.has_value(),"the outage scenario could not submit an action");
    gateway.Until([&]{return !gateway.actionAttempts.empty();});
    // An unacknowledged window in flight, as the Client has during an outage.
    connection.SendInput(Input(1,3));
    gateway.Until([&]{return !gateway.attempts.empty();});
    gateway.Snapshot(0);
    const auto lastSnapshotAt=gateway.snapshotSentAt;
    const auto packetsBefore=gateway.packets.size();
    gateway.silent=true;
    const auto silentAt=Clock::now();
    gateway.Until([&]{return connection.State().interrupted;},1500ms);
    outcome.interruptMs=MillisSince(lastSnapshotAt,Clock::now());
    // The first version's lower bound (from going silent, 5 ms early) is kept,
    // so the judgment passes nothing that version failed (AGENTS §14.4).
    outcome.interruptedOnTime=OnTime(outcome.interruptMs,ConnectionInterruptedAfter) &&
        MillisSince(silentAt,Clock::now())>=static_cast<double>(ConnectionInterruptedAfter.count())-5;
    outcome.trackedLastSnapshot=std::abs(MillisSince(lastSnapshotAt,connection.State().interruptedSince))<=20;
    if(!outcome.interruptedOnTime || !outcome.trackedLastSnapshot)return outcome;
    // Interrupted: the session, its sequence and its actions stay; Hello probes every 250 ms.
    const auto hellosFrom=gateway.packets.size();
    gateway.UntilTime(Clock::now()+1s);
    const auto hellos=std::count_if(gateway.packets.begin()+static_cast<std::ptrdiff_t>(hellosFrom),gateway.packets.end(),
        [](const PacketAttempt& packet){return packet.type==wire::Type::Hello;});
    outcome.probedFaster=hellos>=3 && hellos<=5;
    if(!outcome.probedFaster)return outcome;
    const auto during=connection.State();
    outcome.keptSession=during.phase==ConnectionPhase::Playing && during.error.empty() && during.interrupted &&
        during.actionTransport.retained==1 && during.actionTransport.allocatedThrough==*shot;
    // Each check names its own failure: a session that was not kept cannot resume.
    if(!outcome.keptSession)return outcome;
    gateway.silent=false;
    gateway.Snapshot(0);
    gateway.Until([&]{return !connection.State().interrupted;});
    outcome.resumed=connection.State().phase==ConnectionPhase::Playing;
    // The worker resends the same window after the resume (nothing acknowledged it).
    // A window that stopped being sent is not kept either: wait a bounded time.
    const auto resumedAt=Clock::now();
    for(const auto deadline=resumedAt+500ms;gateway.attempts.back().at<=resumedAt && Clock::now()<deadline;) {
        gateway.Pump();std::this_thread::sleep_for(100us);
    }
    if(gateway.attempts.back().at<=resumedAt)return outcome;
    const auto& resent=gateway.attempts.back().input;
    outcome.windowKept=resent.movement_epoch()==1 && resent.commands_size()==3;
    for(int index=0;outcome.windowKept && index<resent.commands_size();++index)
        outcome.windowKept=resent.commands(index).sequence()==static_cast<std::uint64_t>(index+1);
    outcome.sequenceKept=true;
    for(auto index=packetsBefore+1;index<gateway.packets.size();++index)
        outcome.sequenceKept=outcome.sequenceKept && gateway.packets[index].sequence==gateway.packets[index-1].sequence+1;
    // T_max counts from the last valid snapshot: Welcomes extend nothing.
    gateway.snapshotOnHello=false;
    gateway.Snapshot(0);
    const auto finalSnapshotAt=gateway.snapshotSentAt;
    const auto finalSentAfter=Clock::now();
    const auto welcomesFrom=gateway.welcomeTransmits.size();
    gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby;},7000ms);
    outcome.timeoutMs=MillisSince(finalSnapshotAt,Clock::now());
    outcome.timedOutOnTime=connection.State().error=="Gateway connection timed out" &&
        OnTime(outcome.timeoutMs,ConnectionTimeoutAfter) && gateway.welcomeTransmits.size()>welcomesFrom+10 &&
        MillisSince(finalSentAfter,Clock::now())>=static_cast<double>(ConnectionTimeoutAfter.count())-5;
    gateway.snapshotOnHello=true;gateway.extremeWelcomeClock=false;
    connection.Leave();gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby && connection.State().error.empty();});
    return outcome;
}
// Copies what the workers write to std::clog, still writing it through; the
// statistics lines are read back at the end (CheckDroppedDatagrams).
class ClogCapture final : public std::streambuf {
public:
    explicit ClogCapture(std::streambuf* through):through(through) {}
    std::string Text(){std::scoped_lock lock(mutex);return text;}
protected:
    int overflow(int c) override {
        if(c==traits_type::eof())return traits_type::not_eof(c);
        std::scoped_lock lock(mutex);text.push_back(static_cast<char>(c));
        return through->sputc(static_cast<char>(c));
    }
    std::streamsize xsputn(const char* data,std::streamsize size) override {
        std::scoped_lock lock(mutex);text.append(data,static_cast<std::size_t>(size));
        return through->sputn(data,size);
    }
    int sync() override {return through->pubsync();}
private:
    std::streambuf* through;
    std::mutex mutex;
    std::string text;
};
// v7.1 silent-branch-asserts (D180): datagrams of the session that decode but
// change nothing are counted on the worker statistics line. Sent early, so the
// first 10 s window (read at the end) holds them.
void CheckDroppedDatagrams(MockGateway& gateway,ClientConnection& connection) {
    gateway.earlyBeforeWelcome=true;
    connection.Join(gateway.address,"1");
    gateway.Until([&]{return connection.State().phase==ConnectionPhase::Playing;});
    const auto playing=connection.State().snapshot->tick;
    const std::string garbage("\x0a\x7f",2);
    gateway.SendRaw(wire::Type::Welcome,garbage);
    gateway.SendRaw(wire::Type::Snapshot,garbage);
    gateway.SendRaw(wire::Type::Error,garbage);
    pb::WorldSnapshot zeroTick;zeroTick.set_tick(0);
    gateway.SendRaw(wire::Type::Snapshot,zeroTick.SerializeAsString());
    pb::WorldSnapshot uneven;uneven.set_tick(playing+50);uneven.add_players()->set_player_id(1);
    gateway.SendRaw(wire::Type::Snapshot,uneven.SerializeAsString());
    gateway.SnapshotAt(playing,0); // not newer than the applied one
    // Windows for an authority the worker no longer has (D183): another
    // connection generation, and an epoch its newest snapshot does not carry.
    const auto attemptsBefore=gateway.attempts.size();
    connection.SendInput(Input(1,1),std::numeric_limits<std::uint64_t>::max());
    auto otherEpoch=Input(1,1);otherEpoch.movementEpoch=gateway.epoch+1;connection.SendInput(std::move(otherEpoch));
    for(const auto kind:{wire::Type::Hello,wire::Type::Input,wire::Type::Actions})gateway.SendRaw(kind,"");
    gateway.Snapshot(0);
    gateway.Until([&]{return connection.State().snapshot->tick>playing;});
    Require(connection.State().phase==ConnectionPhase::Playing && connection.State().error.empty(),
        "a dropped datagram changed the session");
    Require(gateway.attempts.size()==attemptsBefore,"a window for a replaced authority was sent");
    connection.Leave();gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby;});
}
// The first worker statistics line counts at least what CheckDroppedDatagrams sent.
void RequireDroppedCounts(const std::string& captured) {
    const auto at=captured.find("[ObjectFPS/PvP] worker statistics ");
    Require(at!=std::string::npos,"no worker statistics line was written");
    const auto line=captured.substr(at,captured.find('\n',at)-at);
    const auto count=[&](const std::string& key){
        const auto found=line.find(" "+key+"=");
        Require(found!=std::string::npos,"the worker statistics line lacks a dropped-datagram key");
        return std::stoull(line.substr(found+key.size()+2));
    };
    Require(count("unparsed_datagrams")>=3,"unparsed datagrams were not counted");
    Require(count("early_datagrams")>=2,"datagrams before the Welcome were not counted");
    Require(count("invalid_snapshot_datagrams")>=2,"invalid snapshots were not counted");
    Require(count("ignored_snapshot_datagrams")>=1,"snapshots that were not newer were not counted");
    Require(count("unexpected_kind_datagrams")>=3,"kinds the Gateway does not send were not counted");
    Require(count("stale_input_windows")>=2,"windows for a replaced authority were not counted");
}
bool Acknowledged(const ClientConnection& connection,std::uint64_t sequence) {
    const auto state=connection.State();
    Require(state.error.empty(),"worker connection failed");
    return state.snapshot && state.snapshot->players.at(0).lastResolvedCommand==sequence;
}
std::size_t CheckMaximumDatagrams() {
    const auto max64=std::numeric_limits<std::uint64_t>::max();
    const auto max32=std::numeric_limits<std::uint32_t>::max();
    std::size_t largest{};
    const auto check=[&](wire::Type type,const auto& message) {
        const auto bytes=wire::Encode({type,max64,max32,message.SerializeAsString()});
        Require(bytes.size()<=wire::MaxDatagram && wire::Decode(bytes).has_value(),"maximum-field v5 datagram exceeded envelope");
        largest=std::max(largest,bytes.size());
    };
    pb::ActionBatch batch;batch.set_acknowledged_through(max64-MaxActionWindow);
    pb::ActionResults results;results.set_retired_through(max64-MaxActionWindow);
    for(std::size_t n=0;n<MaxActionBatch;++n) {
        auto* shot=batch.add_shots();shot->set_action_id(max64-n);shot->set_observed_authority_tick(max64);
        shot->set_yaw(3.0f);shot->set_pitch(1.5f);shot->set_kind(pb::ACTION_SHOT);shot->set_life_generation(max64);
        shot->set_movement_epoch(max64);shot->set_command_sequence(max64);shot->set_view_tick_q16(max64); // pv7 §6, §7
        auto* d=results.add_decisions();d->set_action_id(max64-n);d->set_resolved_tick(max64);
        d->set_accepted(true);d->set_hit_kind(pb::HIT_PLAYER);d->set_target_id(max64);d->set_damage(max32);d->set_kind(pb::ACTION_SHOT);d->set_life_generation(max64);d->set_target_life_generation(max64);
        d->set_hit_part(pb::HIT_PART_HEAD);
    }
    check(wire::Type::Actions,batch);check(wire::Type::ActionResults,results);
    pb::PlayerInput movement;movement.set_movement_epoch(max64);movement.set_life_generation(max64);movement.set_observed_authority_tick(max64);
    for(std::size_t n=0;n<MaxPendingCommands;++n) {
        auto* c=movement.add_commands();c->set_sequence(max64-(MaxPendingCommands-1)+n);c->set_move_forward(1);c->set_move_right(1);
        c->set_yaw(3.0f);c->set_pitch(1.5f);c->set_jump_requested(true);
    }
    check(wire::Type::Input,movement);
    pb::WorldSnapshot snapshot;snapshot.set_tick(max64);
    snapshot.set_match_hold_us(10000000);snapshot.set_gateway_hold_us(10000000); // pv7 §5 holds at their clamp
    for(unsigned n=0;n<AcceptanceMaxPlayers;++n) {
        auto* p=snapshot.add_players();p->set_player_id(max64-n);
        p->set_x(std::numeric_limits<float>::max());p->set_y(std::numeric_limits<float>::max());p->set_z(std::numeric_limits<float>::max());
        p->set_yaw(3.0f);p->set_pitch(1.5f);p->set_last_resolved_command(max64);p->set_movement_epoch(max64);p->set_contiguous_pending_commands(MaxFutureCommands);p->set_vertical_velocity(4);p->set_grounded(true);
        p->set_life_generation(max64);p->set_life_state(pb::LIFE_ALIVE);p->set_life_state_tick(max64);p->set_respawn_tick(max64);
        p->set_movement_slack_sequence(max64);p->set_movement_slack_us(-MaxMovementSlackMicros);
        p->set_connection_quality_failures(ConnectionQualityFailedWindows-1);
        auto* c=snapshot.add_combat();c->set_player_id(max64-n);c->set_hp(max32);c->set_next_allowed_shot_tick(max64);c->set_life_generation(max64);c->set_magazine_ammo(max32);
        c->set_reload_action_id(max64);c->set_reload_start_tick(max64);c->set_reload_end_tick(max64);c->set_last_shot_action_id(max64);c->set_last_shot_tick(max64);
        c->set_last_damage_tick(max64);c->set_damage_count(std::numeric_limits<std::uint32_t>::max());c->set_last_attacker_id(max64);
        c->set_last_hit_part(pb::HIT_PART_HEAD);
    }
    check(wire::Type::Snapshot,snapshot);
    // A full room is the largest datagram (pv6 contract §7, 2026-10-07 revision).
    Require(largest==1073,"maximum-field full-room snapshot is not 1073 bytes"); // pv7 §8
    auto legacy=wire::Encode({wire::Type::Actions,1,1,batch.SerializeAsString()});
    // Every older version and the next one are refused.
    for(std::uint64_t version=1;version<=AcceptanceProtocolVersion+1;++version) {
        if(version==AcceptanceProtocolVersion) continue;
        wire::Write(std::span(legacy).subspan(4,2),version);
        Require(!wire::Decode(legacy),"UDP accepted another protocol version");
    }
    return largest;
}

Json CheckActions(MockGateway& gateway,ClientConnection& connection) {
    const auto rules=connection.State().combatRules;
    Require(rules && rules->maximumHp==200 && rules->shotDamage==37 && rules->cooldownTicks==9 &&
        rules->shotRange==140 && rules->maximumReferenceAge==333ms && rules->magazineCapacity==7 && rules->reloadTicks==88 && rules->respawnTicks==177 && rules->maximumShotDamage==111,"client duplicated authoritative combat defaults");
    Require(connection.State().snapshot->combat.at(0).hp==74 &&
        connection.State().snapshot->combat.at(0).nextAllowedShotTick==900,"snapshot lost independent combat state");
    Require(connection.State().snapshot->combat.at(0).lastDamageTick==connection.State().snapshot->tick &&
        connection.State().snapshot->combat.at(0).damageCount==3 && connection.State().snapshot->combat.at(0).lastAttackerId==2 &&
        connection.State().snapshot->combat.at(0).lastHitPart==HitPart::Body,
        "snapshot lost the hit record");
    Require(!connection.SubmitShot(0,std::numeric_limits<float>::infinity(),0),"invalid aim allocated an action");
    std::mutex allocatedMutex;std::vector<ActionId> allocated;
    std::vector<std::thread> producers;
    for(unsigned n=0;n<8;++n)producers.emplace_back([&]{
        for(unsigned shot=0;shot<4;++shot) {
            const auto id=connection.SubmitShot(connection.State().snapshot->tick,0,0);
            if(id){std::scoped_lock lock(allocatedMutex);allocated.push_back(*id);}
        }
    });
    for(auto& producer:producers)producer.join();
    producers.clear();std::sort(allocated.begin(),allocated.end());
    Require(allocated.size()==MaxActionWindow,"concurrent submission lost capacity");
    for(std::size_t n=0;n<allocated.size();++n)Require(allocated[n]==n+1,"action allocation duplicated or skipped an ID");
    Require(!connection.SubmitShot(1,0,0),"full action window accepted another request");
    // An eligible action leaves at SubmitAction (v7 D36), so a batch can go out
    // while the producers still allocate; let it arrive before the stalled
    // window's first counted batch.
    gateway.UntilTime(Clock::now()+50ms);
    const auto firstActions=gateway.actionAttempts.size();
    // An unacknowledged input after the last acknowledged command (99 in the
    // current epoch), so movement resends share the budget window below.
    gateway.autoAck=false;
    auto pendingInput=Input(100,100);pendingInput.movementEpoch=gateway.epoch;connection.SendInput(std::move(pendingInput));
    const auto inputsBefore=gateway.attempts.size();
    const auto coexistenceStart=Clock::now();
    // No Drain: the main-thread consumer is stalled for a full second while
    // movement, Hello and the independent circular action worker continue.
    gateway.UntilTime(coexistenceStart+1100ms);
    std::map<ActionId,std::size_t> deliveries;
    std::map<ActionId,std::string> immutableRequests;
    for(std::size_t n=firstActions;n<gateway.actionAttempts.size();++n) {
        const auto& attempt=gateway.actionAttempts[n];
        Require(attempt.actions.shots_size()==MaxActionBatch && !attempt.actions.acknowledged_through(),"unexpected stalled action batch");
        if(n>firstActions)Require(attempt.at-gateway.actionAttempts[n-1].at>=30ms,"action worker emitted overdue catch-up packets");
        for(const auto& shot:attempt.actions.shots()) {
            ++deliveries[shot.action_id()];
            const auto [entry,inserted]=immutableRequests.emplace(shot.action_id(),shot.SerializeAsString());
            Require(inserted || entry->second==shot.SerializeAsString(),"worker mutated an allocated shot on retry");
        }
    }
    for(const auto id:allocated)Require(deliveries[id]>=2,"circular resend starved part of the 32-entry window");
    std::size_t maxPacketsInSecond{};
    // Every actual receive counts, including Hello and duplicate movement/action
    // retransmissions. Check all sliding windows, stronger than one fixed phase.
    for(std::size_t first=0,last=0;first<gateway.packets.size();++first) {
        while(last<gateway.packets.size() && gateway.packets[last].at-gateway.packets[first].at<1s)++last;
        maxPacketsInSecond=std::max(maxPacketsInSecond,last-first);
    }
    Require(gateway.attempts.size()-inputsBefore>=55,"the budget window carried no input resends");
    Require(maxPacketsInSecond<=120,"60+30+Hello exceeded the actual 120-packet session budget");
    // The window above carries resends of one window but no waiting new
    // commands, so the drained input rate is bounded here instead: draining
    // sends, a full burst, actions with one resend, and Hello.
    static_assert(InputSendDrainRate+InputSendBurst+(ActionSendRate+1)+2<=120,
        "input drain rate exceeds the 120-packet session budget");
    // Reordered results leave an ID-1 hole. Invalid/conflicting batches cannot
    // partially publish ID 1 or retire data which the game has never consumed.
    for(ActionId first=2;first<=32;first+=8) {
        std::vector<ActionId> ids;
        for(ActionId id=first;id<=32 && id<first+8;++id)ids.push_back(id);
        gateway.Results(ids);
    }
    gateway.Until([&]{return connection.State().actionTransport.unconsumed==31;});
    pb::ActionResults conflict;
    auto* fresh=conflict.add_decisions();fresh->set_action_id(1);fresh->set_resolved_tick(123);fresh->set_rejection(pb::REJECTION_COOLDOWN);fresh->set_kind(pb::ACTION_SHOT);fresh->set_life_generation(1);
    auto* changed=conflict.add_decisions();changed->set_action_id(2);changed->set_resolved_tick(124);changed->set_rejection(pb::REJECTION_COOLDOWN);changed->set_kind(pb::ACTION_SHOT);changed->set_life_generation(1);
    gateway.SendResults(conflict);gateway.Results({},32);
    gateway.Until([&]{return connection.State().actionTransport.rejectedResultBatches>=2;});
    Require(connection.State().actionTransport.pending==1 && connection.State().actionTransport.retiredThrough==0,
        "malformed result batch partially changed transport state");
    const auto beforeEpoch=connection.State().actionTransport;
    ++gateway.epoch;gateway.Snapshot(0);
    gateway.Until([&]{return connection.State().snapshot->players.at(0).movementEpoch==gateway.epoch;});
    {   // pv7 §2a (D-12): a newer tick whose movement epoch goes back is never applied, not
        // even until the next snapshot (the mock answers each Hello with one).
        const auto keptTick=connection.State().snapshot->tick;
        --gateway.epoch;gateway.SnapshotAt(keptTick+1,0);++gateway.epoch;
        bool applied=false;
        const auto until=Clock::now()+60ms;
        gateway.Until([&]{applied=applied || connection.State().snapshot->players.at(0).movementEpoch<gateway.epoch;
            return Clock::now()>=until;});
        Require(!applied,"the Client applied an old, repeated or regressed snapshot");
    }
    Require(connection.State().actionTransport.retained==beforeEpoch.retained &&
        connection.State().actionTransport.unconsumed==31,"movement epoch erased action results");
    for(unsigned n=0;n<MaxReceivedSnapshots+16;++n)gateway.Snapshot(0);
    gateway.UntilTime(Clock::now()+50ms);
    const auto drained=connection.Drain();
    Require(drained.overflow && drained.snapshots.size()==MaxReceivedSnapshots && drained.decisions.size()==31 &&
        drained.state.actionTransport.acknowledgedThrough==0,"snapshot overflow lost decisions or ACK crossed an ID hole");
    for(const auto& d:drained.decisions)Require(!d.accepted && d.rejection==ShotRejection::Cooldown,"client fabricated a successful result");
    gateway.Results({2,3,4,5,6,7,8,9});gateway.UntilTime(Clock::now()+10ms);
    Require(connection.Drain().decisions.empty(),"duplicate results were delivered twice");
    Require(!connection.SubmitShot(1,0,0),"consumed but unretired results freed the distance window");
    const auto firstResultAt=Clock::now();
    gateway.Results({1});gateway.Until([&]{return connection.State().actionTransport.unconsumed==1;});
    // A repeated result does not move the receipt time (v7 batch 08).
    gateway.UntilTime(Clock::now()+10ms);
    const auto repeatAt=Clock::now();
    gateway.Results({1});
    // Let the deadline armed while ID 1 was pending pass, so only the ACK's own
    // wake-up (or the next Hello, up to a second away) can send the ACK.
    gateway.UntilTime(Clock::now()+50ms);
    const auto drainedAt=Clock::now();
    const auto last=connection.Drain();
    Require(last.decisions.size()==1 && last.decisions[0].actionId==1 &&
        last.state.actionTransport.acknowledgedThrough==32,"contiguous consumed results failed to ACK through 32");
    Require(last.decisionReceivedAt.size()==1 && last.decisionReceivedAt[0]>=firstResultAt && last.decisionReceivedAt[0]<repeatAt,
        "decision receipt time is not the first arrival");
    const auto ackStart=gateway.actionAttempts.size();
    // Drop two ACK-only transmissions: no authority retirement is returned.
    gateway.Until([&]{return gateway.actionAttempts.size()>=ackStart+3;});
    // The ACK advanced by Drain wakes the worker (v7 batch 08), not its next Hello.
    Require(gateway.actionAttempts[ackStart].at-drainedAt<40ms,"an advanced ACK waited for an unrelated wake");
    for(std::size_t n=ackStart;n<gateway.actionAttempts.size();++n)
        Require(gateway.actionAttempts[n].actions.shots().empty() && gateway.actionAttempts[n].actions.acknowledged_through()==32,
            "ACK loss stopped acknowledgements or replayed decided shots");
    Require(connection.State().actionTransport.retained==32,"unconfirmed ACK discarded bounded results");
    gateway.Results({},16);gateway.Until([&]{return connection.State().actionTransport.retiredThrough==16;});
    gateway.Results({},0);gateway.Results({},32);
    gateway.Until([&]{return connection.State().actionTransport.retained==0;});
    // Idle first: the last ACK-only batch's deadline passes, so the new action is
    // eligible at once and nothing but SubmitAction's wake-up is pending.
    gateway.UntilTime(Clock::now()+50ms);
    const auto submittedAt=Clock::now();
    const auto resumed=connection.SubmitShot(connection.State().snapshot->tick,0,0);
    Require(resumed && *resumed==33,"retirement or failed submission skipped an allocated ID");
    gateway.Until([&]{return !gateway.actionAttempts.back().actions.shots().empty() && gateway.actionAttempts.back().actions.shots(0).action_id()==33;});
    // D36: a lone action leaves at SubmitAction once eligible (at most one
    // ActionSendRate interval after the previous batch), not at a later wake.
    Require(gateway.actionAttempts.back().at-submittedAt<40ms,"an eligible action waited for an unrelated wake");
    gateway.Results({33},32);gateway.Until([&]{return connection.State().actionTransport.unconsumed==1;});
    Require(connection.Drain().decisions.size()==1,"new action failed after clearing loss");
    gateway.Results({},33);gateway.Until([&]{return connection.State().actionTransport.retained==0;});
    const auto state=connection.State().actionTransport;
    Require(state.maxDatagramBytes<=wire::MaxDatagram && state.maxBatchShots==8,"worker diagnostics exceeded packet limits");
    return Json{{"concurrent_actions",allocated.size()},{"max_packets_in_any_1s",maxPacketsInSecond},
        {"action_batches",state.sentBatches},{"ack_only_batches",state.sentAckOnlyBatches},
        {"invalid_result_batches",state.rejectedResultBatches},{"max_action_datagram_bytes",state.maxDatagramBytes},
        {"main_stall_ms",1100},{"restored_action_id",*resumed}};
}
}
int main() {
    ClogCapture capture(std::clog.rdbuf());
    struct Restore{std::streambuf* previous;~Restore(){std::clog.rdbuf(previous);}} restore{std::clog.rdbuf(&capture)};
    try {
        const auto maximumDatagram=CheckMaximumDatagrams();
        MockGateway gateway;ClientConnection connection;
        bool zeroDigestRejected=false;
        try{connection.SetArenaIdentity("worker-test",1,0);}catch(const std::invalid_argument&){zeroDigestRejected=true;}
        Require(zeroDigestRejected,"zero arena digest accepted as identity");
        connection.SetArenaIdentity("worker-test",1,WorkerArenaDigest);
        CheckDroppedDatagrams(gateway,connection);
        // Early too, so a mutant of the outage path fails on the check that guards it:
        // the mock sends snapshots only on demand, so the later sections pass
        // through interruptions too.
        // An outage in both directions, then T_max with Welcomes only; the same
        // with an extreme time echo must decide the same (differential oracle).
        const auto outage=RunOutage(gateway,connection,false);
        Require(outage.interruptedOnTime,"the connection was not interrupted at T_int after the last snapshot");
        Require(outage.trackedLastSnapshot,"the interruption did not count from the last snapshot's arrival");
        Require(outage.keptSession,"an interruption did not keep the session, its window and its actions");
        Require(outage.probedFaster,"an interrupted connection did not probe with Hello every 250 ms");
        Require(outage.resumed,"a valid snapshot did not resume the connection");
        Require(outage.windowKept,"the worker's window changed across an interruption");
        Require(outage.sequenceKept,"an interruption restarted the transport sequence");
        Require(outage.timedOutOnTime,"a connection without snapshots did not time out at T_max (a Welcome kept it?)");
        const auto extreme=RunOutage(gateway,connection,true);
        Require(extreme==outage && std::abs(extreme.interruptMs-outage.interruptMs)<=RunsToleranceMs &&
            std::abs(extreme.timeoutMs-outage.timeoutMs)<=RunsToleranceMs,"an extreme time echo changed a liveness decision");
        // The later sections read the mock's records from their own session on.
        gateway.attempts.clear();gateway.actionAttempts.clear();gateway.packets.clear();
        gateway.helloClocks.clear();gateway.welcomeTransmits.clear();
        gateway.badWelcomeClock=true; // a bad echo on the first Welcome changes nothing
        connection.Join(gateway.address,"1");
        gateway.Until([&]{return connection.State().phase==ConnectionPhase::Playing;});
        gateway.badWelcomeClock=false;
        // Each Hello carries the echo of the latest Welcome (pv7 §3): origin is
        // that Welcome's transmit, and its own transmit increases.
        gateway.Until([&]{return gateway.helloClocks.size()>=3;},std::chrono::milliseconds(4000));
        for(std::size_t i=1;i<gateway.helloClocks.size();++i) {
            const auto& hello=gateway.helloClocks[i];
            Require(hello.has_origin_us() && hello.has_receive_us() && hello.origin_us()==gateway.welcomeTransmits[i-1] &&
                hello.transmit_us()>gateway.helloClocks[i-1].transmit_us() && hello.transmit_us()>=hello.receive_us(),
                "a Hello did not echo the latest Welcome");
        }
        Require(!gateway.helloClocks.front().has_origin_us(),"the first Hello echoed a Welcome it never read");
        connection.SendInput(Input(1,2));
        gateway.Until([&]{return gateway.attempts.size()>=2;});
        const auto lastOldSend=gateway.attempts.back().at;
        gateway.Snapshot(2);
        gateway.Until([&]{return Acknowledged(connection,2);});
        const auto restartedAt=Clock::now();
        connection.SendInput(Input(3,3));
        gateway.Until([&]{return gateway.attempts.size()>=3;});
        const auto firstNewSend=gateway.attempts.back().at;
        const auto restartMs=std::chrono::duration<double,std::milli>(firstNewSend-restartedAt).count();
        const auto completedWindowGapMs=std::chrono::duration<double,std::milli>(firstNewSend-lastOldSend).count();
        Require(restartMs<10 && completedWindowGapMs<14,"new window waited behind a fully acknowledged window's deadline");

        // A partial acknowledgement leaves the unchanged window on its 60 Hz
        // resend deadline. A command never sent goes at the next poll while the
        // input token bucket (60/s, two tokens) has one; a resend always leaves one.
        connection.SendInput(Input(3,4));gateway.Snapshot(3);
        gateway.Until([&]{return Acknowledged(connection,3);});
        const auto onlyFour=[&](std::size_t index){
            const auto& input=gateway.attempts[index].input;
            return input.commands_size()==1 && input.commands(0).sequence()==4;
        };
        gateway.Until([&]{
            const auto size=gateway.attempts.size();
            return size>=2 && onlyFour(size-1) && onlyFour(size-2);
        });
        const auto resentAt=gateway.attempts.back().at;
        const auto partialGapMs=std::chrono::duration<double,std::milli>(resentAt-gateway.attempts[gateway.attempts.size()-2].at).count();
        Require(partialGapMs>=14,"partial acknowledgement bypassed the resend deadline");
        const auto beforeFresh=gateway.attempts.size();
        connection.SendInput(Input(4,5));
        gateway.Until([&]{return gateway.attempts.size()>beforeFresh;});
        const auto& fresh=gateway.attempts.back().input;
        Require(fresh.commands(fresh.commands_size()-1).sequence()==5,"the new command was not sent next");
        const auto freshMs=std::chrono::duration<double,std::milli>(gateway.attempts.back().at-resentAt).count();
        Require(freshMs<10,"a new command waited behind the resend deadline");
        connection.SendInput(Input(4,6));
        const auto resendBegin=gateway.attempts.size();
        gateway.UntilTime(Clock::now()+1s);
        const auto resends=gateway.attempts.size()-resendBegin;
        Require(resends>=55 && resends<=65,"unacknowledged window was not resent at approximately 60 Hz");
        for(auto index=resendBegin+1;index<gateway.attempts.size();++index)
            Require(gateway.attempts[index].at-gateway.attempts[index-1].at>=14ms,"worker emitted a resend catch-up burst");

        gateway.Snapshot(6);gateway.Until([&]{return Acknowledged(connection,6);});
        // Room capacity (pv6 contract §1): a full room's snapshot is accepted;
        // one more player makes it invalid, dropped without changing the state.
        gateway.Crowded(AcceptanceMaxPlayers);
        gateway.Until([&]{const auto state=connection.State();
            return state.snapshot && state.snapshot->players.size()==AcceptanceMaxPlayers;});
        const auto fullTick=connection.State().snapshot->tick;
        gateway.Crowded(AcceptanceMaxPlayers+1);
        gateway.UntilTime(Clock::now()+60ms);
        Require(connection.State().snapshot->tick==fullTick,"a snapshot above the room capacity was accepted");
        gateway.Snapshot(6);
        gateway.Until([&]{const auto state=connection.State();
            return state.snapshot && state.snapshot->tick==fullTick+2 && state.snapshot->players.size()==1;});
        // pv7 §2a (D-12): the Gateway no longer drops an old or regressed
        // snapshot; the Client keeps only a newer tick with no cursor going back.
        const auto keptTick=connection.State().snapshot->tick;
        gateway.SnapshotAt(keptTick,6);     // the same tick again
        gateway.SnapshotAt(keptTick-1,6);   // an older tick
        gateway.SnapshotAt(keptTick+1,5);   // newer, but the cursor goes back
        gateway.UntilTime(Clock::now()+60ms);
        Require(connection.State().snapshot->tick==keptTick && Acknowledged(connection,6),
            "the Client applied an old, repeated or regressed snapshot");
        gateway.SnapshotAt(keptTick+2,6);
        gateway.Until([&]{const auto state=connection.State();return state.snapshot && state.snapshot->tick==keptTick+2;});
        gateway.autoAck=true;
        const auto sixtyBegin=gateway.attempts.size();
        const auto frameStart=Clock::now();
        for(std::uint64_t sequence=7;sequence<67;++sequence) {
            connection.SendInput(Input(sequence,sequence));
            gateway.UntilTime(frameStart+std::chrono::nanoseconds((sequence-6)*1'000'000'000/60));
        }
        gateway.Until([&]{return Acknowledged(connection,66);});
        const auto sixtyAttempts=gateway.attempts.size()-sixtyBegin;
        Require(sixtyAttempts>=60 && sixtyAttempts<=70,"fully acknowledged 60 FPS publication caused excessive sends");
        // Publishing a new command every 4 ms without acknowledgement still sends
        // at most the bucket: two at once, then sends that waited for a token at
        // most InputSendDrainRate (80) per second, never a burst.
        gateway.autoAck=false;
        const auto fastBegin=gateway.attempts.size();
        const auto fastStart=Clock::now();
        for(std::uint64_t sequence=67;sequence<78;++sequence) {
            connection.SendInput(Input(67,sequence));
            gateway.UntilTime(fastStart+std::chrono::milliseconds((sequence-66)*4));
        }
        gateway.UntilTime(fastStart+100ms);
        const auto fastAttempts=gateway.attempts.size()-fastBegin;
        Require(fastAttempts<=2+7,"fresh publications exceeded the input token bucket");
        for(auto index=fastBegin+2;index<gateway.attempts.size();++index)
            Require(gateway.attempts[index].at-gateway.attempts[index-2].at>=14ms,"worker emitted an input burst beyond two packets");
        gateway.Snapshot(77);gateway.Until([&]{return Acknowledged(connection,77);});
        // A token debt drains (v7 batch 09a). With a full bucket three new
        // commands go out at once, so the third waits about a step for its
        // token; then one is published each step, as production does. A send
        // that waited is charged less in proportion to its wait
        // (InputSendDrainRate 80), so each later command waits a quarter step
        // (4.17 ms) less than the one before, and none waits after four.
        gateway.UntilTime(Clock::now()+100ms);
        constexpr double stepMs=1000.0/InputSendRate;
        // Timing tolerances (v7 batch 09a-impl; logs pvp-v7-09ab-impl-20261013/09a/derive):
        // midway, to the nearest 0.25 ms, between the largest deviation in 29
        // runs of this probe (30 launched, one wrote no output; 0.13, 0.37,
        // 0.34 ms) and the smallest one a wrong drain rate or a stale wait
        // produces (drain 70/s: 1.79 ms a step; 75/s: a fourth wait of 3.1 ms;
        // a stale wait: a quarter step, 4.17 ms). Those three are model values,
        // not measured mutants. On the final build 30 probe runs
        // (09a/probe30) deviated at most 0.233, 0.405 and 0.480 ms; the
        // midpoints stay the same.
        constexpr double drainToleranceMs=1.0;     // per-step drain against a quarter step
        constexpr double settledWaitMs=1.75;       // a wait after the fourth command
        constexpr double staleGapToleranceMs=2.25; // two steps after the second send
        std::vector<double> drainWaits,drainIntervals;
        Clock::time_point previousPublish{};
        const auto publishWait=[&](std::uint64_t last){
            const auto before=gateway.attempts.size();
            const auto publishedAt=Clock::now();
            connection.SendInput(Input(78,last));
            const auto sentLast=[&]{const auto& input=gateway.attempts.back().input;
                return input.commands(input.commands_size()-1).sequence()==last;};
            gateway.Until([&]{return gateway.attempts.size()>before && sentLast();});
            if(previousPublish!=Clock::time_point{})
                drainIntervals.push_back(std::chrono::duration<double,std::milli>(publishedAt-previousPublish).count());
            previousPublish=publishedAt;
            return std::chrono::duration<double,std::milli>(gateway.attempts.back().at-publishedAt).count();
        };
        const auto drainBegin=gateway.attempts.size();
        publishWait(78);publishWait(79);previousPublish={};drainIntervals.clear();
        const auto drainStart=Clock::now();
        for(std::uint64_t k=0;k<8;++k) {
            gateway.UntilTime(drainStart+std::chrono::nanoseconds(k*1'000'000'000/InputSendRate));
            drainWaits.push_back(publishWait(80+k));
        }
        Require(drainWaits[0]>=12,"the third new command did not wait for a token");
        for(std::size_t k=1;k<=3;++k) {
            // Each step's wait falls by a quarter step, plus any lateness of this publication.
            const double drained=drainWaits[k-1]-drainWaits[k]-(drainIntervals[k-1]-stepMs);
            Require(std::abs(drained-stepMs/4)<=drainToleranceMs,"the waiting sends did not drain the token debt");
        }
        for(std::size_t k=4;k<drainWaits.size();++k)
            Require(drainWaits[k]<=settledWaitMs+std::max(0.0,stepMs-drainIntervals[k-1]),"a token wait outlived four commands");
        // Publishes a window and returns when its newest command was sent.
        const auto sendWindow=[&](PlayerInput input){
            const auto last=input.commands.back().sequence;
            const auto before=gateway.attempts.size();
            connection.SendInput(std::move(input));
            gateway.Until([&]{
                if(gateway.attempts.size()<=before)return false;
                const auto& sent=gateway.attempts.back().input;
                return sent.commands(sent.commands_size()-1).sequence()==last;});
            return gateway.attempts.back().at;
        };
        // Right after the last step's send, two more new commands one after
        // another (the window then holds MaxPendingCommands), three in a row
        // with the last step's. A discounted send leaves at most one token, so
        // no three sends since the drain began span less than a step, and the
        // third waits for its token. A wait that stayed recorded after its send
        // would credit every later send and let the bucket refill above one token.
        Clock::time_point thirdPublished{};
        for(std::uint64_t last=88;last<=89;++last){thirdPublished=Clock::now();sendWindow(Input(78,last));}
        const double burstThirdWaitMs=std::chrono::duration<double,std::milli>(gateway.attempts.back().at-thirdPublished).count();
        double discountedMinSpanMs=std::numeric_limits<double>::infinity();
        for(auto index=drainBegin+2;index<gateway.attempts.size();++index) {
            const auto span=gateway.attempts[index].at-gateway.attempts[index-2].at;
            discountedMinSpanMs=std::min(discountedMinSpanMs,std::chrono::duration<double,std::milli>(span).count());
            Require(span>=14ms,"a discounted send left more than one token");
        }
        Require(burstThirdWaitMs>=10,"the third back-to-back command did not wait for its token");
        gateway.Snapshot(89);gateway.Until([&]{return Acknowledged(connection,89);});
        // A wait that ends without its send (the command acknowledged, or
        // dropped by a new movement epoch) leaves no credit behind. From a full
        // bucket two commands go at once, and a third is still waiting when the
        // acknowledgement or epoch arrives. A step after the second send a new
        // command goes at once and pays a whole token, so the next one goes two
        // steps after the second send; a stale wait would credit the first and
        // let the next go a quarter step early.
        std::vector<double> staleWaitGapsMs;
        const auto staleWait=[&](std::uint64_t first,bool newEpoch,const char* message){
            gateway.UntilTime(Clock::now()+100ms);
            const auto window=[&](std::uint64_t from,std::uint64_t last){
                auto input=Input(from,last);input.movementEpoch=gateway.epoch;return input;};
            sendWindow(window(first,first));
            const auto secondAt=sendWindow(window(first,first+1));
            const auto waiting=gateway.attempts.size();
            connection.SendInput(window(first,first+2));
            gateway.UntilTime(Clock::now()+2ms);
            if(newEpoch)++gateway.epoch;
            gateway.Snapshot(first+2);
            gateway.Until([&]{return Acknowledged(connection,first+2) &&
                connection.State().snapshot->players.at(0).movementEpoch==gateway.epoch;});
            Require(gateway.attempts.size()==waiting,"the waiting command was sent before its wait ended");
            gateway.UntilTime(secondAt+std::chrono::nanoseconds(1'000'000'000/InputSendRate)+3ms);
            sendWindow(window(first+3,first+3));
            const auto nextAt=sendWindow(window(first+3,first+4));
            staleWaitGapsMs.push_back(std::chrono::duration<double,std::milli>(nextAt-secondAt).count());
            Require(staleWaitGapsMs.back()>=2*stepMs-staleGapToleranceMs,message);
            gateway.Snapshot(first+4);gateway.Until([&]{return Acknowledged(connection,first+4);});
        };
        staleWait(90,false,"a wait ended by a full acknowledgement credited a later send");
        staleWait(95,true,"a wait ended by a new movement epoch credited a later send");
        const auto actionEvidence=CheckActions(gateway,connection);
        connection.Leave();gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby;});
        Require(!connection.State().combatRules && connection.State().actionTransport.retained==0 &&
            connection.State().actionTransport.allocatedThrough==0 && connection.Drain().decisions.empty(),"Leave retained action lifecycle state");
        gateway.epoch=1;

        // Version refusals name both versions (pv7 contract §1); the error code
        // decides, so a full room is not reported as a version mismatch.
        const auto joinError=[&]{
            connection.Join(gateway.address,"1");
            gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby && !connection.State().error.empty();});
            return connection.State().error;
        };
        const auto mismatch=[](const std::string& gatewayVersion){
            return "protocol_version_mismatch: Client "+std::to_string(AcceptanceProtocolVersion)+", Gateway "+gatewayVersion;
        };
        const auto newer=std::to_string(AcceptanceProtocolVersion+1);
        gateway.protocolVersion=AcceptanceProtocolVersion-1;
        Require(joinError()==mismatch(std::to_string(AcceptanceProtocolVersion-1)),
            "HTTP accepted the previous protocol version in the join reply");
        gateway.protocolVersion=AcceptanceProtocolVersion;gateway.replyWithoutVersion=true;
        Require(joinError()==mismatch("unknown"),"a join reply without a version was not a mismatch");
        gateway.replyWithoutVersion=false;
        gateway.Refuse(409,{{"error","protocol_version"},{"protocol_version",AcceptanceProtocolVersion+1}});
        Require(joinError()==mismatch(newer),"a newer Gateway's refusal did not name both versions");
        gateway.Refuse(409,{{"error","room_full"}});
        Require(joinError()=="room_full","a full room was reported as something else");
        gateway.Refuse(400,{{"error","protocol_version"},{"protocol_version",AcceptanceProtocolVersion+1}});
        Require(joinError()==mismatch(newer),"a version refusal of status 400 was not a version mismatch");
        // Only a JSON unsigned integer is a version.
        for(const auto& unnamed:{Json(),Json(-1),Json("8"),Json(7.5)}) {
            Json refusal={{"error","protocol_version"}};
            if(!unnamed.is_null())refusal["protocol_version"]=unnamed;
            gateway.Refuse(409,refusal);
            Require(joinError()==mismatch("unknown"),"a refusal without an unsigned version did not say unknown");
        }
        gateway.Accept();
        gateway.protocolVersion=AcceptanceProtocolVersion;gateway.includeRules=false;connection.Join(gateway.address,"1");
        gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby && !connection.State().error.empty();});
        Require(connection.State().error.find("combat rules")!=std::string::npos,"Welcome accepted missing rules");
        gateway.includeRules=true;

        // Same arena id and version with different content is refused on both
        // join paths with the content code; the reason starts with the code.
        const auto refusedWith=[&](const char* code){
            connection.Join(gateway.address,"1");
            // Stop on acceptance too, so an accepted join fails with its own message.
            gateway.Until([&]{const auto s=connection.State();
                return (s.phase==ConnectionPhase::Lobby && !s.error.empty()) || s.phase==ConnectionPhase::Playing;});
            const auto refused=connection.State().error.starts_with(code);
            if(!refused)connection.Leave();
            return refused;
        };
        gateway.joinArenaDigest=WorkerArenaDigest^1;
        Require(refusedWith("arena_content_mismatch:"),"HTTP join accepted different arena content");
        gateway.joinArenaDigest=0;
        Require(refusedWith("arena_content_mismatch:"),"HTTP join accepted a missing arena digest");
        gateway.joinArenaDigest=WorkerArenaDigest;gateway.welcomeArenaDigest=WorkerArenaDigest^1;
        Require(refusedWith("arena_content_mismatch:"),"Welcome accepted different arena content");
        gateway.welcomeArenaDigest=0;
        Require(refusedWith("arena_content_mismatch:"),"Welcome accepted a missing arena digest");
        gateway.welcomeArenaDigest=WorkerArenaDigest;
        connection.SetArenaIdentity("other-arena",1,WorkerArenaDigest);
        Require(refusedWith("arena_identity_mismatch:"),"join accepted a different arena identity");
        // Several installed arenas: the Match's id selects one, whose content is then compared.
        connection.SetArenaIdentities({{"other-arena",1,WorkerArenaDigest^1},{"worker-test",2,WorkerArenaDigest},{"third-arena",1,7}});
        Require(refusedWith("arena_identity_mismatch:"),"join accepted an arena installed only in another version");
        Require(connection.State().error.find("other-arena version 1, worker-test version 2, third-arena version 1")!=std::string::npos,
            "identity mismatch did not list the installed arenas");
        connection.SetArenaIdentities({{"other-arena",1,WorkerArenaDigest^1},{"worker-test",1,WorkerArenaDigest^1}});
        Require(refusedWith("arena_content_mismatch:"),"join accepted the selected arena with different content");
        connection.SetArenaIdentities({{"other-arena",1,WorkerArenaDigest^1},{"worker-test",1,WorkerArenaDigest}});
        connection.Join(gateway.address,"1");
        gateway.Until([&]{const auto s=connection.State();
            return (s.phase==ConnectionPhase::Lobby && !s.error.empty()) || s.phase==ConnectionPhase::Playing;});
        Require(connection.State().phase==ConnectionPhase::Playing && connection.State().arenaId=="worker-test",
            "join did not select the installed Match arena");
        connection.Leave();
        gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby;});
        Require(connection.State().arenaId.empty(),"leaving kept the selected arena");
        bool duplicateRejected=false;
        try{connection.SetArenaIdentities({{"a",1,1},{"a",2,2}});}catch(const std::invalid_argument&){duplicateRejected=true;}
        Require(duplicateRejected,"an arena installed twice was accepted");
        connection.SetArenaIdentity("worker-test",1,WorkerArenaDigest);

        // Exercise real receipt overflow, then consume Drain concurrently with
        // worker failure publication. Counts may reset only with generation.
        constexpr unsigned failureCycles=16;
        std::uint64_t overflowObserved{};
        for(unsigned cycle=0;cycle<failureCycles;++cycle) {
            const auto leavesBefore=gateway.leaves.load();
            connection.Join(gateway.address,"1");
            gateway.Until([&]{return connection.State().phase==ConnectionPhase::Playing;});
            if(cycle)Require(gateway.leaves.load()>leavesBefore,"failed session cleanup was skipped before rejoin");
            const auto freshAction=connection.SubmitShot(connection.State().snapshot->tick,0,0);
            Require(freshAction && *freshAction==1,"new player lifecycle failed to restart action IDs");
            const auto initial=connection.Drain();
            const auto expectedTick=initial.state.snapshot->tick+MaxReceivedSnapshots+16;
            for(std::size_t index=0;index<MaxReceivedSnapshots+16;++index)gateway.Snapshot(66);
            gateway.Until([&]{
                const auto state=connection.State();
                Require(state.error.empty(),"worker failed while filling snapshot history");
                return state.snapshot && state.snapshot->tick>=expectedTick;
            });
            const auto full=connection.Drain();
            Require(full.overflow && full.snapshots.size()==MaxReceivedSnapshots && full.snapshotHistoryOverflowCount>=16,
                "worker snapshot history did not exercise its overflow boundary");
            overflowObserved+=full.snapshotHistoryOverflowCount;
            gateway.FailConnection();
            const auto deadline=Clock::now()+3s;
            while(true) {
                const auto drained=connection.Drain();
                if(drained.generation==full.generation) {
                    Require(drained.snapshotHistoryOverflowCount>=full.snapshotHistoryOverflowCount,
                        "failure cleared receipt history before publishing its new generation");
                } else {
                    Require(drained.generation==full.generation+1 && drained.state.phase==ConnectionPhase::Lobby &&
                        !drained.state.snapshot && drained.state.playerId==0 && !drained.state.error.empty() &&
                        drained.snapshots.empty() && !drained.overflow && drained.snapshotHistoryOverflowCount==0,
                        "failure generation did not atomically publish empty Lobby state and history");
                    Require(drained.decisions.empty() && !drained.state.combatRules &&
                        drained.state.actionTransport.retained==0 && drained.state.actionTransport.allocatedThrough==0,
                        "failure generation retained stale action state");
                    break;
                }
                Require(Clock::now()<deadline,"worker failure publication timed out");
                std::this_thread::yield();
            }
        }
        connection.Leave();gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby && connection.State().error.empty();});
        RequireDroppedCounts(capture.Text());
        std::cout<<Json{{"passed",true},{"restart_ms",restartMs},{"completed_window_gap_ms",completedWindowGapMs},
            {"partial_ack_gap_ms",partialGapMs},{"unacknowledged_resends_1s",resends},{"fully_acknowledged_60fps_attempts",sixtyAttempts},
            {"token_debt_waits_ms",drainWaits},{"token_debt_publish_intervals_ms",drainIntervals},
            {"burst_third_wait_ms",burstThirdWaitMs},{"discounted_min_span_ms",discountedMinSpanMs},{"stale_wait_next_gaps_ms",staleWaitGapsMs},
            {"atomic_failure_cycles",failureCycles},{"snapshot_history_overflows_exercised",overflowObserved},
            {"largest_maximum_field_datagram_bytes",maximumDatagram},{"actions",actionEvidence},
            {"outage_interrupt_ms",{outage.interruptMs,extreme.interruptMs}},{"outage_timeout_ms",{outage.timeoutMs,extreme.timeoutMs}}}.dump()<<'\n';
        return 0;
    } catch(const std::exception& error){std::cerr<<"worker acceptance: "<<error.what()<<'\n';return 1;}
}
