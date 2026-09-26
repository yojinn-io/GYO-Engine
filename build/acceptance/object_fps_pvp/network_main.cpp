#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include <httplib.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

using namespace fps::pvp;
using Clock=std::chrono::steady_clock;
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class Predicate> void Wait(Predicate predicate,const char* failure) {
    const auto deadline=Clock::now()+std::chrono::seconds(8);
    while(!predicate()) {if(Clock::now()>deadline)throw std::runtime_error(failure);std::this_thread::sleep_for(std::chrono::milliseconds(5));}
}
const PlayerState& Player(const WorldSnapshot& snapshot,PlayerId id) {
    for(const auto& player:snapshot.players)if(player.playerId==id)return player;
    throw std::runtime_error("Snapshot omitted expected player");
}
bool ExactPlayers(const ClientConnectionState& state,std::initializer_list<PlayerId> expected) {
    if(!state.snapshot || state.snapshot->players.size()!=expected.size())return false;
    for(const auto id:expected) {
        const auto count=std::count_if(state.snapshot->players.begin(),state.snapshot->players.end(),
            [id](const auto& player){return player.playerId==id;});
        if(count!=1)return false;
    }
    return true;
}
void WaitRoom(ClientConnection& client,unsigned players,const char* failure) {
    Wait([&]{const auto state=client.State();if(!state.error.empty())throw std::runtime_error(state.error);
        return state.phase==ConnectionPhase::Lobby && state.rooms.size()==1 && state.rooms.front().id=="1" &&
            state.rooms.front().players==players;},failure);
}
// Acceptance-only HTTP fault injection. UDP and all accepted HTTP requests
// still use the real Gateway/Match; production code has no test controls.
class LeaveFailureRelay final {
public:
    explicit LeaveFailureRelay(std::string gateway):upstream_(gateway.starts_with("http://")?gateway:"http://"+gateway) {
        server_.new_task_queue=[] {return new httplib::ThreadPool(2);};
        server_.Get("/rooms",[this](const auto& request,auto& response){Forward(request,response);});
        server_.Post(R"(/rooms.*)",[this](const auto& request,auto& response){Forward(request,response);});
        const auto port=server_.bind_to_any_port("127.0.0.1");
        Require(port>0,"cannot bind leave-failure acceptance relay");
        address_="127.0.0.1:"+std::to_string(port);
        worker_=std::jthread([this]{server_.listen_after_bind();});
        Wait([&]{return server_.is_running();},"leave-failure acceptance relay did not start");
    }
    ~LeaveFailureRelay(){server_.stop();if(worker_.joinable())worker_.join();}
    const std::string& Address()const{return address_;}
    bool VerifiedCleanupBeforeRejoin() {
        std::scoped_lock lock(mutex_);
        return leaveAttempts_>=2 && joinAttempts_==2 && retriedSameSession_ && cleanupBeforeRejoin_;
    }
private:
    void Forward(const httplib::Request& request,httplib::Response& response) {
        const bool leave=request.method=="POST" && request.path.ends_with("/leave");
        {
            std::scoped_lock lock(mutex_);
            if(leave && ++leaveAttempts_==1) {
                rejectedBody_=request.body;
                response.status=503;response.set_content(R"({"error":"acceptance_injected_leave_failure"})","application/json");
                return;
            }
            if(leave && leaveAttempts_==2)retriedSameSession_=request.body==rejectedBody_;
            if(request.method=="POST" && request.path.ends_with("/join")) {
                if(++joinAttempts_==2)cleanupBeforeRejoin_=cleanupConfirmed_;
            }
        }
        httplib::Client upstream(upstream_);
        upstream.set_connection_timeout(2,0);upstream.set_read_timeout(2,0);upstream.set_write_timeout(2,0);
        const auto result=request.method=="GET"?upstream.Get(request.path):
            upstream.Post(request.path,request.body,"application/json");
        if(!result) {
            response.status=502;response.set_content(R"({"error":"acceptance_upstream_failed"})","application/json");return;
        }
        if(leave && result->status>=200 && result->status<300) {
            std::scoped_lock lock(mutex_);cleanupConfirmed_=true;
        }
        response.status=result->status;response.set_content(result->body,"application/json");
    }
    httplib::Server server_;
    std::string upstream_,address_,rejectedBody_;
    std::mutex mutex_;
    unsigned leaveAttempts_{},joinAttempts_{};
    bool retriedSameSession_{},cleanupConfirmed_{},cleanupBeforeRejoin_{};
    std::jthread worker_;
};
int main(int argc,char** argv) {
    try {
        std::string gateway="127.0.0.1:8080";bool selfTest=false;
        std::filesystem::path disconnectReady, arenaPath;
        for(int index=1;index<argc;++index){
            const std::string arg=argv[index];
            if(arg=="--self-test")selfTest=true;
            else if(arg=="--gateway" && index+1<argc)gateway=argv[++index];
            else if(arg=="--arena" && index+1<argc)arenaPath=argv[++index];
            else if(arg=="--disconnect-ready" && index+1<argc)disconnectReady=argv[++index];
            else throw std::invalid_argument("Expected --gateway host:port or --self-test");
        }
        if(selfTest){
            auto bytes=wire::Encode({wire::Type::Input,0x0102030405060708ULL,0xffffffffu,"abc"});
            const auto packet=wire::Decode(bytes);
            Require(packet && packet->session==0x0102030405060708ULL && packet->payload=="abc","UDP roundtrip");
            Require(wire::Newer(0,0xffffffffu) && !wire::Newer(0xffffffffu,0),"sequence wrapping");
            auto oldVersion=bytes; oldVersion[5]=1;
            Require(!wire::Decode(oldVersion),"protocol v1 accepted");
            oldVersion[5]=2;
            Require(!wire::Decode(oldVersion),"protocol v2 accepted");
            bool oversizedRejected=false;
            try { static_cast<void>(wire::Encode({wire::Type::Input,1,1,std::string(wire::MaxDatagram, 'x')})); }
            catch(const std::length_error&) { oversizedRejected=true; }
            Require(oversizedRejected,"oversized UDP accepted");
            bytes[20]=1;Require(!wire::Decode(bytes),"invalid UDP length accepted");
            std::cout<<"wire self-test passed\n";return 0;
        }
        ClientConnection a,b;
        a.CreateAndJoin(gateway);
        Wait([&]{const auto s=a.State();if(!s.error.empty())throw std::runtime_error(s.error);return s.phase==ConnectionPhase::Playing;},"first client failed to join");
        b.Refresh(gateway);
        Wait([&]{const auto s=b.State();if(!s.error.empty())throw std::runtime_error(s.error);return s.phase==ConnectionPhase::Lobby && !s.rooms.empty();},"room list failed");
        b.Join(gateway,b.State().rooms.front().id);
        Wait([&]{const auto s=b.State();if(!s.error.empty())throw std::runtime_error(s.error);return s.phase==ConnectionPhase::Playing && a.State().snapshot->players.size()==2;},"second client failed to join");
        const auto idA=a.State().playerId,idB=b.State().playerId;
        if(!disconnectReady.empty()) {
            {std::ofstream signal(disconnectReady);signal<<"ready\n";Require(bool(signal),"cannot signal disconnect readiness");}
            Wait([&]{return a.State().phase==ConnectionPhase::Lobby && b.State().phase==ConnectionPhase::Lobby;},
                 "clients did not return to Lobby after Match failure");
            Require(!a.State().error.empty() && !b.State().error.empty(),"Match failure lacked client errors");
            Require(!a.State().snapshot && !b.State().snapshot,"failed Match retained stale world state");
            std::cout<<"failure acceptance passed: both clients returned to Lobby and cleared their world\n";
            return 0;
        }
        const auto startA=Player(*a.State().snapshot,idA).position;
        const auto startB=Player(*b.State().snapshot,idB).position;
        std::map<std::uint64_t,WorldSnapshot> historyA,historyB;
        std::string arenaError;
        const auto arena=Arena::Load(arenaPath,arenaError);
        if(!arena)throw std::runtime_error(arenaError);
        LocalPlayerPrediction predictionA(*arena),predictionB(*arena);
        auto previous=Clock::now();const auto started=previous;
        const auto end=previous+std::chrono::seconds(4);
        std::optional<fps::Float3> stoppedA,stoppedB;
        while(Clock::now()<end){
            const auto now=Clock::now();
            const auto beforeA=a.State(),beforeB=b.State();
            predictionA.Reconcile(Player(*beforeA.snapshot,idA),beforeA.snapshot->tick);
            predictionB.Reconcile(Player(*beforeB.snapshot,idB),beforeB.snapshot->tick);
            const double elapsed=std::chrono::duration<double>(now-previous).count();
            previous=now;
            const double age=std::chrono::duration<double>(now-started).count();
            const bool moving=age<2.0 || age>=3.0;
            if(predictionA.Advance(elapsed,0,moving?1.0F:0.0F,0,0))a.SendInput(predictionA.PendingInput());
            if(predictionB.Advance(elapsed,0,moving?-.5F:0.0F,0,0))b.SendInput(predictionB.PendingInput());
            Require(predictionA.Observation().pendingCommands<=MaxPendingCommands &&
                predictionB.Observation().pendingCommands<=MaxPendingCommands,"unbounded prediction window");
            const auto sa=a.State(),sb=b.State();
            Require(sa.error.empty() && sb.error.empty(),"connection failed while moving");
            if(age>=2.7 && age<3.0) {
                const auto poseA=Player(*sa.snapshot,idA).position,poseB=Player(*sb.snapshot,idB).position;
                if(!stoppedA){stoppedA=poseA;stoppedB=poseB;}
                Require(std::abs(poseA.x-stoppedA->x)<.0001F && std::abs(poseB.x-stoppedB->x)<.0001F,
                    "neutral command window did not stop both clients");
            }
            if(sa.snapshot)historyA[sa.snapshot->tick]=*sa.snapshot;
            if(sb.snapshot)historyB[sb.snapshot->tick]=*sb.snapshot;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        Require(stoppedA.has_value(),"stop phase not observed");
        const auto movedA=Player(*a.State().snapshot,idA).position;
        const auto movedB=Player(*b.State().snapshot,idB).position;
        Require(movedA.x-startA.x>4.5F,"A did not move authoritatively");
        Require(startB.x-movedB.x>2.0F,"B did not move authoritatively");
        std::size_t common{};
        for(const auto& [tick,sa]:historyA){
            Require(tick%SnapshotIntervalTicks==0,"snapshot cadence differs from product constant");
            const auto found=historyB.find(tick);if(found==historyB.end())continue;++common;
            for(const auto& p:sa.players){const auto& q=Player(found->second,p.playerId);Require(p.position.x==q.position.x && p.position.z==q.position.z,"clients disagree on same authority tick");}
        }
        Require(common>10,"insufficient common snapshots");
        // Drain queued steps, then fifteen missing authority ticks must stop both.
        std::this_thread::sleep_for(std::chrono::milliseconds(550));
        const auto stopped=Player(*a.State().snapshot,idA).position;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        Require(std::abs(Player(*a.State().snapshot,idA).position.x-stopped.x)<0.0001F,"input timeout did not stop movement");
        const auto idleBeforeA=a.State(),idleBeforeB=b.State();
        // Deliberately stop the application thread beyond the session timeout.
        // Only each ClientConnection's worker is active during this interval.
        std::this_thread::sleep_for(std::chrono::seconds(6));
        const auto idleA=a.State(),idleB=b.State();
        Require(idleA.phase==ConnectionPhase::Playing && idleB.phase==ConnectionPhase::Playing &&
            idleA.error.empty() && idleB.error.empty() && idleA.snapshot && idleB.snapshot,
            "render/input stall disconnected a healthy worker");
        Require(idleA.snapshot->tick>idleBeforeA.snapshot->tick && idleB.snapshot->tick>idleBeforeB.snapshot->tick,
            "worker stopped receiving snapshots during application stall");
        Require(std::abs(Player(*idleA.snapshot,idA).position.x-Player(*idleBeforeA.snapshot,idA).position.x)<.0001F &&
            std::abs(Player(*idleB.snapshot,idB).position.x-Player(*idleBeforeB.snapshot,idB).position.x)<.0001F,
            "session keepalive refreshed stale movement input");
        predictionA.Reconcile(Player(*idleA.snapshot,idA),idleA.snapshot->tick);
        predictionB.Reconcile(Player(*idleB.snapshot,idB),idleB.snapshot->tick);
        const auto lead=predictionA.PendingInput();
        Require(lead.commands.size()==InitialCommandLead &&
            lead.commands.front().sequence==Player(*idleA.snapshot,idA).lastResolvedCommand+1,
            "stall recovery did not rebuild command lead");
        for(const auto& command:lead.commands)
            Require(command.moveForward==0 && command.moveRight==0,"stall recovery lead was not neutral");
        previous=Clock::now();const auto resumedUntil=previous+std::chrono::seconds(1);
        while(Clock::now()<resumedUntil) {
            const auto now=Clock::now();const auto sa=a.State(),sb=b.State();
            Require(sa.phase==ConnectionPhase::Playing && sb.phase==ConnectionPhase::Playing &&
                sa.snapshot && sb.snapshot,"connection failed after application stall");
            predictionA.Reconcile(Player(*sa.snapshot,idA),sa.snapshot->tick);
            predictionB.Reconcile(Player(*sb.snapshot,idB),sb.snapshot->tick);
            const double elapsed=std::chrono::duration<double>(now-previous).count();previous=now;
            if(predictionA.Advance(elapsed,0,-1,0,0))a.SendInput(predictionA.PendingInput());
            if(predictionB.Advance(elapsed,0,1,0,0))b.SendInput(predictionB.PendingInput());
            Require(predictionA.Observation().pendingCommands<=MaxPendingCommands &&
                predictionB.Observation().pendingCommands<=MaxPendingCommands,"stall recovery window overflowed");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        Require(Player(*idleA.snapshot,idA).position.x-Player(*a.State().snapshot,idA).position.x>.5F &&
            Player(*b.State().snapshot,idB).position.x-Player(*idleB.snapshot,idB).position.x>.5F,
            "movement did not resume after rebuilding stalled command windows");
        ClientConnection observer;
        observer.Refresh(gateway);
        WaitRoom(observer,2,"lobby observer did not see two players");
        std::set<PlayerId> retiredIds;
        for(unsigned cycle=0;cycle<3;++cycle) {
            retiredIds.insert(a.State().playerId);
            a.Leave();
            Require(!a.State().snapshot && a.State().playerId==0,"leave retained local world identity");
            WaitRoom(a,1,"leave did not publish the refreshed one-player room");
            Wait([&]{return ExactPlayers(b.State(),{idB});},"leave retained a stale pawn remotely");
            WaitRoom(observer,1,"idle lobby did not observe departure");
            a.Join(gateway,"1");
            Require(a.State().rooms.empty(),"joining retained stale room counts");
            Wait([&]{const auto sa=a.State(),sb=b.State();if(!sa.error.empty())throw std::runtime_error(sa.error);
                return sa.phase==ConnectionPhase::Playing && ExactPlayers(sa,{sa.playerId,idB}) &&
                    ExactPlayers(sb,{sa.playerId,idB});},"rejoin produced the wrong player identities or count");
            Require(!retiredIds.contains(a.State().playerId),"rejoin reused a retired player identity");
            WaitRoom(observer,2,"idle lobby did not observe rejoin");
        }
        a.Leave();b.Leave();
        WaitRoom(observer,0,"both departures left occupied room slots");
        WaitRoom(a,0,"first lobby retained stale occupancy after both left");
        WaitRoom(b,0,"second lobby retained stale occupancy after both left");
        a.Join(gateway,"1");a.Leave();
        WaitRoom(a,0,"cancelled join retained a room reservation");
        Require(!a.State().snapshot && a.State().playerId==0,"cancelled join resurrected a world");
        WaitRoom(observer,0,"cancelled join leaked a player slot");
        {
            LeaveFailureRelay relay(gateway);
            {
                ClientConnection retry;
                retry.Join(relay.Address(),"1");
                Wait([&]{const auto state=retry.State();if(!state.error.empty())throw std::runtime_error(state.error);
                    return state.phase==ConnectionPhase::Playing && ExactPlayers(state,{state.playerId});},"fault-test player failed to join");
                const auto retired=retry.State().playerId;
                WaitRoom(observer,1,"observer did not see fault-test player");
                retry.Leave();
                Wait([&]{const auto state=retry.State();return state.phase==ConnectionPhase::Lobby && !state.error.empty();},
                    "failed HTTP leave was silently reported successful");
                const auto failed=retry.State();
                Require(!failed.snapshot && failed.playerId==0 && failed.rooms.empty(),"failed leave retained stale presentation state");
                retry.Join(relay.Address(),"1");
                Wait([&]{const auto state=retry.State();if(!state.error.empty())throw std::runtime_error(state.error);
                    return state.phase==ConnectionPhase::Playing && ExactPlayers(state,{state.playerId});},
                    "failed leave cleanup was not retried before rejoin");
                Require(retry.State().playerId!=retired && relay.VerifiedCleanupBeforeRejoin(),
                    "rejoin bypassed previous session cleanup");
                // Scope exit must release the remaining real reservation.
            }
            WaitRoom(observer,0,"client destruction retained an occupied room slot");
        }
        std::cout<<"network acceptance passed: two clients, movement, "<<common<<" identical authority snapshots, cadence, stop/resume, timeout, six-second application stall with worker keepalive and reseed, repeated leave/rejoin identities and lobby counts, cancelled join, failed-leave cleanup barrier, destructor cleanup\n";
        return 0;
    }catch(const std::exception& failure){std::cerr<<"network acceptance failed: "<<failure.what()<<'\n';return 1;}
}
