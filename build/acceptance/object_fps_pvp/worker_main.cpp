// Product-owned socket acceptance for ClientConnection's real background worker.
// The mock is external to production: HTTP joins and wire-v3 UDP snapshots only.
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "client_v3.pb.h"
#include <asio.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace fps::pvp;
using Clock=std::chrono::steady_clock;
using namespace std::chrono_literals;
namespace pb=object_fps_pvp::client::v3;
using asio::ip::udp;
using Json=nlohmann::json;
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Attempt { Clock::time_point at; pb::PlayerInput input; };
class MockGateway {
public:
    MockGateway():socket(io,udp::endpoint(asio::ip::address_v4::loopback(),0)) {
        socket.non_blocking(true);
        http.Get("/rooms",[](const auto&,auto& response){response.set_content(R"({"rooms":[]})","application/json");});
        http.Post("/rooms/1/join",[&](const auto&,auto& response){
            const Json reply={{"session_id",1},{"session_token","worker-test"},{"player_id",1},{"protocol_version",3},
                {"arena_id","worker-test"},{"arena_version",1},{"udp_ip","127.0.0.1"},{"udp_port",socket.local_endpoint().port()}};
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
            peer=source;
            if(packet->type==wire::Type::Hello) {
                pb::Hello hello;Require(hello.ParseFromString(packet->payload) && hello.session_token()=="worker-test","invalid Hello");
                pb::Welcome welcome;welcome.set_player_id(1);welcome.set_match_id(1);welcome.set_tick_rate(60);
                welcome.set_snapshot_rate(60);welcome.set_arena_id("worker-test");welcome.set_arena_version(1);
                Send(wire::Type::Welcome,welcome.SerializeAsString());Snapshot(ack);
            } else if(packet->type==wire::Type::Input) {
                pb::PlayerInput input;Require(input.ParseFromString(packet->payload) && input.movement_epoch()==1 &&
                    input.commands_size()>0 && input.commands_size()<=12,"invalid worker input window");
                attempts.push_back({Clock::now(),input});
                if(autoAck)Snapshot(input.commands(input.commands_size()-1).sequence());
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
        auto* player=snapshot.add_players();player->set_player_id(1);player->set_movement_epoch(1);player->set_last_resolved_command(ack);
        Send(wire::Type::Snapshot,snapshot.SerializeAsString());
    }
    void FailConnection() {
        pb::Error error;error.set_code("worker_test_failure");error.set_message("Intentional worker lifecycle failure");
        Send(wire::Type::Error,error.SerializeAsString());
    }
    std::string address;
    std::vector<Attempt> attempts;
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
}
int main() {
    try {
        MockGateway gateway;ClientConnection connection;
        connection.SetArenaIdentity("worker-test",1);connection.Join(gateway.address,"1");
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

        // A partial acknowledgement leaves the existing 60 Hz deadline intact,
        // even when the render thread appends more commands during that period.
        connection.SendInput(Input(3,4));gateway.Snapshot(3);
        gateway.Until([&]{return Acknowledged(connection,3);});
        connection.SendInput(Input(4,5));
        gateway.Until([&]{return gateway.attempts.size()>=4;});
        const auto partialGapMs=std::chrono::duration<double,std::milli>(gateway.attempts.back().at-firstNewSend).count();
        Require(partialGapMs>=14,"partial acknowledgement bypassed the resend deadline");
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
        connection.Leave();gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby;});

        // Exercise real receipt overflow, then consume Drain concurrently with
        // worker failure publication. Counts may reset only with generation.
        constexpr unsigned failureCycles=16;
        std::uint64_t overflowObserved{};
        for(unsigned cycle=0;cycle<failureCycles;++cycle) {
            const auto leavesBefore=gateway.leaves.load();
            connection.Join(gateway.address,"1");
            gateway.Until([&]{return connection.State().phase==ConnectionPhase::Playing;});
            if(cycle)Require(gateway.leaves.load()>leavesBefore,"failed session cleanup was skipped before rejoin");
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
                    break;
                }
                Require(Clock::now()<deadline,"worker failure publication timed out");
                std::this_thread::yield();
            }
        }
        connection.Leave();gateway.Until([&]{return connection.State().phase==ConnectionPhase::Lobby && connection.State().error.empty();});
        std::cout<<Json{{"passed",true},{"restart_ms",restartMs},{"completed_window_gap_ms",completedWindowGapMs},
            {"partial_ack_gap_ms",partialGapMs},{"unacknowledged_resends_1s",resends},{"fully_acknowledged_60fps_attempts",sixtyAttempts},
            {"atomic_failure_cycles",failureCycles},{"snapshot_history_overflows_exercised",overflowObserved}}.dump()<<'\n';
        return 0;
    } catch(const std::exception& error){std::cerr<<"worker acceptance: "<<error.what()<<'\n';return 1;}
}
