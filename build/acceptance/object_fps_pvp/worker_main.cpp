// Product-owned socket acceptance for ClientConnection's real background worker.
// The mock is external to production: HTTP joins and current-wire UDP movement and action transport.
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "client_v6.pb.h"
#include "acceptance_protocol.hpp"
#include <asio.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace fps::pvp;
using Clock=std::chrono::steady_clock;
using namespace std::chrono_literals;
namespace pb=object_fps_pvp::client::v6;
using asio::ip::udp;
using Json=nlohmann::json;
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Attempt { Clock::time_point at; pb::PlayerInput input; };
struct ActionAttempt { Clock::time_point at; pb::ActionBatch actions; };
struct PacketAttempt { Clock::time_point at; wire::Type type; std::size_t bytes; };
// Opaque content digest of the mock arena (pv6 contract §2).
constexpr std::uint64_t WorkerArenaDigest=0x0123456789abcdefULL;
class MockGateway {
public:
    MockGateway():socket(io,udp::endpoint(asio::ip::address_v4::loopback(),0)) {
        socket.non_blocking(true);
        http.Get("/rooms",[](const auto&,auto& response){response.set_content(R"({"rooms":[]})","application/json");});
        http.Post("/rooms/1/join",[&](const auto& request,auto& response){
            const auto body=Json::parse(request.body);
            if(body.value("protocol_version",0u)!=AcceptanceProtocolVersion) {
                response.status=400;response.set_content(R"({"error":"protocol_version"})","application/json");return;
            }
            const Json reply={{"session_id",1},{"session_token","worker-test"},{"player_id",1},{"protocol_version",protocolVersion.load()},
                {"arena_id","worker-test"},{"arena_version",1},{"arena_digest",joinArenaDigest.load()},{"udp_ip","127.0.0.1"},{"udp_port",socket.local_endpoint().port()}};
            response.set_content(reply.dump(),"application/json");
        });
        http.Post("/rooms/1/leave",[&](const auto&,auto& response){
            ++leaves;response.set_content(R"({"left":true})","application/json");
        });
        const auto port=http.bind_to_any_port("127.0.0.1");
        Require(port>0,"mock HTTP bind failed");
        address="127.0.0.1:"+std::to_string(port);
        server=std::jthread([this]{http.listen_after_bind();});
    }
    ~MockGateway(){http.stop();}
    void Pump() {
        for(int count=0;count<64;++count) {
            std::array<std::uint8_t,2048> buffer{};udp::endpoint source;asio::error_code error;
            const auto size=socket.receive_from(asio::buffer(buffer),source,0,error);
            if(error==asio::error::would_block || error==asio::error::try_again)return;
            if(error)throw std::runtime_error(error.message());
            const auto packet=wire::Decode(std::span(buffer).first(size));
            Require(packet.has_value() && packet->session==1,"invalid worker UDP envelope");
            peer=source;packets.push_back({Clock::now(),packet->type,size});
            if(packet->type==wire::Type::Hello) {
                pb::Hello hello;Require(hello.ParseFromString(packet->payload) && hello.session_token()=="worker-test","invalid Hello");
                pb::Welcome welcome;welcome.set_player_id(1);welcome.set_match_id(1);welcome.set_tick_rate(60);
                welcome.set_snapshot_rate(60);welcome.set_jump_height(.6F);welcome.set_gravity(18);welcome.set_arena_id("worker-test");welcome.set_arena_version(1);welcome.set_arena_digest(welcomeArenaDigest.load());
                if(includeRules) {
                    auto* rules=welcome.mutable_combat_rules();rules->set_maximum_hp(200);rules->set_shot_damage(37);
                    rules->set_cooldown_ticks(9);rules->set_shot_range(140);rules->set_maximum_reference_age_ms(333);
                    rules->set_magazine_capacity(7);rules->set_reload_ticks(88);rules->set_respawn_ticks(177);
                }
                Send(wire::Type::Welcome,welcome.SerializeAsString());Snapshot(ack);
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
    void Until(const std::function<bool()>& condition,std::chrono::milliseconds timeout=3000ms) {
        const auto deadline=Clock::now()+timeout;
        while(true) {
            Pump();if(condition())return;
            Require(Clock::now()<deadline,"mock worker wait timed out");
            std::this_thread::sleep_for(100us);
        }
    }
    void UntilTime(Clock::time_point deadline){Until([&]{return Clock::now()>=deadline;});}
    void Snapshot(std::uint64_t resolved) {
        ack=resolved;pb::WorldSnapshot snapshot;snapshot.set_tick(++tick);
        auto* player=snapshot.add_players();player->set_player_id(1);player->set_movement_epoch(epoch);player->set_last_resolved_command(ack);player->set_life_generation(1);player->set_life_state(pb::LIFE_ALIVE);player->set_grounded(true);
        auto* combat=snapshot.add_combat();combat->set_player_id(1);combat->set_hp(74);combat->set_next_allowed_shot_tick(900);combat->set_life_generation(1);combat->set_magazine_ammo(7);
        combat->set_last_damage_tick(tick);combat->set_damage_count(3);combat->set_last_attacker_id(2); // pv6 hit record
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
    std::atomic<unsigned> protocolVersion{AcceptanceProtocolVersion};
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
    std::jthread server;
    std::uint64_t ack{},tick{};
    std::uint32_t sequence{};
};
PlayerInput Input(std::uint64_t first,std::uint64_t last) {
    PlayerInput result;result.playerId=1;
    for(auto sequence=first;sequence<=last;++sequence)result.commands.push_back({sequence,1,0,0,0});
    return result;
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
        auto* d=results.add_decisions();d->set_action_id(max64-n);d->set_resolved_tick(max64);
        d->set_accepted(true);d->set_hit_kind(pb::HIT_PLAYER);d->set_target_id(max64);d->set_damage(max32);d->set_kind(pb::ACTION_SHOT);d->set_life_generation(max64);d->set_target_life_generation(max64);
    }
    check(wire::Type::Actions,batch);check(wire::Type::ActionResults,results);
    pb::PlayerInput movement;movement.set_movement_epoch(max64);movement.set_life_generation(max64);movement.set_observed_authority_tick(max64);
    for(std::size_t n=0;n<MaxPendingCommands;++n) {
        auto* c=movement.add_commands();c->set_sequence(max64-(MaxPendingCommands-1)+n);c->set_move_forward(1);c->set_move_right(1);
        c->set_yaw(3.0f);c->set_pitch(1.5f);c->set_jump_requested(true);
    }
    check(wire::Type::Input,movement);
    pb::WorldSnapshot snapshot;snapshot.set_tick(max64);
    for(unsigned n=0;n<2;++n) {
        auto* p=snapshot.add_players();p->set_player_id(max64-n);
        p->set_x(std::numeric_limits<float>::max());p->set_y(std::numeric_limits<float>::max());p->set_z(std::numeric_limits<float>::max());
        p->set_yaw(3.0f);p->set_pitch(1.5f);p->set_last_resolved_command(max64);p->set_movement_epoch(max64);p->set_contiguous_pending_commands(MaxFutureCommands);p->set_vertical_velocity(4);p->set_grounded(true);
        p->set_life_generation(max64);p->set_life_state(pb::LIFE_ALIVE);p->set_life_state_tick(max64);p->set_respawn_tick(max64);
        p->set_movement_slack_sequence(max64);p->set_movement_slack_us(-MaxMovementSlackMicros);
        p->set_connection_quality_failures(ConnectionQualityFailedWindows-1);
        auto* c=snapshot.add_combat();c->set_player_id(max64-n);c->set_hp(max32);c->set_next_allowed_shot_tick(max64);c->set_life_generation(max64);c->set_magazine_ammo(max32);
        c->set_reload_action_id(max64);c->set_reload_start_tick(max64);c->set_reload_end_tick(max64);c->set_last_shot_action_id(max64);c->set_last_shot_tick(max64);
        c->set_last_damage_tick(max64);c->set_damage_count(std::numeric_limits<std::uint32_t>::max());c->set_last_attacker_id(max64);
    }
    check(wire::Type::Snapshot,snapshot);
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
        rules->shotRange==140 && rules->maximumReferenceAge==333ms && rules->magazineCapacity==7 && rules->reloadTicks==88 && rules->respawnTicks==177,"client duplicated authoritative combat defaults");
    Require(connection.State().snapshot->combat.at(0).hp==74 &&
        connection.State().snapshot->combat.at(0).nextAllowedShotTick==900,"snapshot lost independent combat state");
    Require(connection.State().snapshot->combat.at(0).lastDamageTick==connection.State().snapshot->tick &&
        connection.State().snapshot->combat.at(0).damageCount==3 && connection.State().snapshot->combat.at(0).lastAttackerId==2,
        "snapshot lost the hit record");
    Require(!connection.SubmitShot(0,std::numeric_limits<float>::infinity(),0),"invalid aim allocated an action");
    std::mutex allocatedMutex;std::vector<ActionId> allocated;
    std::vector<std::jthread> producers;
    for(unsigned n=0;n<8;++n)producers.emplace_back([&]{
        for(unsigned shot=0;shot<4;++shot) {
            const auto id=connection.SubmitShot(connection.State().snapshot->tick,0,0);
            if(id){std::scoped_lock lock(allocatedMutex);allocated.push_back(*id);}
        }
    });
    producers.clear();std::sort(allocated.begin(),allocated.end());
    Require(allocated.size()==MaxActionWindow,"concurrent submission lost capacity");
    for(std::size_t n=0;n<allocated.size();++n)Require(allocated[n]==n+1,"action allocation duplicated or skipped an ID");
    Require(!connection.SubmitShot(1,0,0),"full action window accepted another request");
    const auto firstActions=gateway.actionAttempts.size();
    gateway.autoAck=false;connection.SendInput(Input(67,67));
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
    Require(maxPacketsInSecond<=120,"60+30+Hello exceeded the actual 120-packet session budget");
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
    gateway.Results({1});gateway.Until([&]{return connection.State().actionTransport.unconsumed==1;});
    const auto last=connection.Drain();
    Require(last.decisions.size()==1 && last.decisions[0].actionId==1 &&
        last.state.actionTransport.acknowledgedThrough==32,"contiguous consumed results failed to ACK through 32");
    const auto ackStart=gateway.actionAttempts.size();
    // Drop two ACK-only transmissions: no authority retirement is returned.
    gateway.Until([&]{return gateway.actionAttempts.size()>=ackStart+3;});
    for(std::size_t n=ackStart;n<gateway.actionAttempts.size();++n)
        Require(gateway.actionAttempts[n].actions.shots().empty() && gateway.actionAttempts[n].actions.acknowledged_through()==32,
            "ACK loss stopped acknowledgements or replayed decided shots");
    Require(connection.State().actionTransport.retained==32,"unconfirmed ACK discarded bounded results");
    gateway.Results({},16);gateway.Until([&]{return connection.State().actionTransport.retiredThrough==16;});
    gateway.Results({},0);gateway.Results({},32);
    gateway.Until([&]{return connection.State().actionTransport.retained==0;});
    const auto resumed=connection.SubmitShot(connection.State().snapshot->tick,0,0);
    Require(resumed && *resumed==33,"retirement or failed submission skipped an allocated ID");
    gateway.Until([&]{return !gateway.actionAttempts.back().actions.shots().empty() && gateway.actionAttempts.back().actions.shots(0).action_id()==33;});
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
    try {
        const auto maximumDatagram=CheckMaximumDatagrams();
        MockGateway gateway;ClientConnection connection;
        bool zeroDigestRejected=false;
        try{connection.SetArenaIdentity("worker-test",1,0);}catch(const std::invalid_argument&){zeroDigestRejected=true;}
        Require(zeroDigestRejected,"zero arena digest accepted as identity");
        connection.SetArenaIdentity("worker-test",1,WorkerArenaDigest);connection.Join(gateway.address,"1");
        gateway.Until([&]{return connection.State().phase==ConnectionPhase::Playing;});
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
        // at most the bucket: two at once, then 60 per second, never a burst.
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
        const auto actionEvidence=CheckActions(gateway,connection);
        connection.Leave();gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby;});
        Require(!connection.State().combatRules && connection.State().actionTransport.retained==0 &&
            connection.State().actionTransport.allocatedThrough==0 && connection.Drain().decisions.empty(),"Leave retained action lifecycle state");
        gateway.epoch=1;

        // The previous version must be refused as a legacy join.
        gateway.protocolVersion=AcceptanceProtocolVersion-1;connection.Join(gateway.address,"1");
        gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby && !connection.State().error.empty();});
        Require(connection.State().error.find("protocol mismatch")!=std::string::npos,"HTTP accepted the previous protocol version");
        gateway.protocolVersion=AcceptanceProtocolVersion;gateway.includeRules=false;connection.Join(gateway.address,"1");
        gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby && !connection.State().error.empty();});
        Require(connection.State().error.find("combat rules")!=std::string::npos,"Welcome accepted missing rules");
        gateway.includeRules=true;

        // Same arena id and version with different content is refused on both
        // join paths with the content code; the reason starts with the code.
        const auto refusedWith=[&](const char* code){
            connection.Join(gateway.address,"1");
            gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby && !connection.State().error.empty();});
            return connection.State().error.starts_with(code);
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
        std::cout<<Json{{"passed",true},{"restart_ms",restartMs},{"completed_window_gap_ms",completedWindowGapMs},
            {"partial_ack_gap_ms",partialGapMs},{"unacknowledged_resends_1s",resends},{"fully_acknowledged_60fps_attempts",sixtyAttempts},
            {"atomic_failure_cycles",failureCycles},{"snapshot_history_overflows_exercised",overflowObserved},
            {"largest_maximum_field_datagram_bytes",maximumDatagram},{"actions",actionEvidence}}.dump()<<'\n';
        return 0;
    } catch(const std::exception& error){std::cerr<<"worker acceptance: "<<error.what()<<'\n';return 1;}
}
