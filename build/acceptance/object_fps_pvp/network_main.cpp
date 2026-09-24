#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
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
int main(int argc,char** argv) {
    try {
        std::string gateway="127.0.0.1:8080";bool selfTest=false;
        std::filesystem::path disconnectReady;
        for(int index=1;index<argc;++index){
            const std::string arg=argv[index];
            if(arg=="--self-test")selfTest=true;
            else if(arg=="--gateway" && index+1<argc)gateway=argv[++index];
            else if(arg=="--disconnect-ready" && index+1<argc)disconnectReady=argv[++index];
            else throw std::invalid_argument("Expected --gateway host:port or --self-test");
        }
        if(selfTest){
            auto bytes=wire::Encode({wire::Type::Input,0x0102030405060708ULL,0xffffffffu,"abc"});
            const auto packet=wire::Decode(bytes);
            Require(packet && packet->session==0x0102030405060708ULL && packet->payload=="abc","UDP roundtrip");
            Require(wire::Newer(0,0xffffffffu) && !wire::Newer(0xffffffffu,0),"sequence wrapping");
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
        Engine::Runtime::FixedTickRuntime ticks;
        auto previous=Clock::now();const auto end=previous+std::chrono::seconds(2);
        std::uint64_t sequence{};
        while(Clock::now()<end){
            const auto now=Clock::now();std::optional<PlayerInput> input;
            ticks.Advance(std::chrono::duration<double>(now-previous).count(),[&](const auto& tick){if(tick.tickId%2==0)input=PlayerInput{idA,++sequence,tick.tickId,0,1,0,0};});
            previous=now;
            if(input){a.SendInput(*input);input->playerId=idB;input->clientTick+=900000;input->moveRight=-0.5F;b.SendInput(*input);}
            const auto sa=a.State(),sb=b.State();
            Require(sa.error.empty() && sb.error.empty(),"connection failed while moving");
            if(sa.snapshot)historyA[sa.snapshot->tick]=*sa.snapshot;
            if(sb.snapshot)historyB[sb.snapshot->tick]=*sb.snapshot;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        const auto movedA=Player(*a.State().snapshot,idA).position;
        const auto movedB=Player(*b.State().snapshot,idB).position;
        Require(movedA.x-startA.x>4.5F,"A did not move authoritatively");
        Require(startB.x-movedB.x>2.0F,"B did not move authoritatively");
        std::size_t common{};
        for(const auto& [tick,sa]:historyA){
            Require(tick%3==0,"snapshot cadence is not authority /3");
            const auto found=historyB.find(tick);if(found==historyB.end())continue;++common;
            for(const auto& p:sa.players){const auto& q=Player(found->second,p.playerId);Require(p.position.x==q.position.x && p.position.z==q.position.z,"clients disagree on same authority tick");}
        }
        Require(common>10,"insufficient common snapshots");
        // No input updates: the fifteen-simulation-tick policy must stop both.
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        const auto stopped=Player(*a.State().snapshot,idA).position;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        Require(std::abs(Player(*a.State().snapshot,idA).position.x-stopped.x)<0.0001F,"input timeout did not stop movement");
        a.Leave();
        Wait([&]{const auto s=b.State();return s.snapshot && s.snapshot->players.size()==1;},"leave was not visible remotely");
        a.Join(gateway,"1");
        Wait([&]{const auto s=a.State();if(!s.error.empty())throw std::runtime_error(s.error);return s.phase==ConnectionPhase::Playing;},"rejoin failed");
        Require(a.State().playerId!=idA,"rejoin reused player identity");
        a.Leave();b.Leave();
        std::cout<<"network acceptance passed: two clients, movement, "<<common<<" identical authority snapshots, cadence, timeout, leave/rejoin\n";
        return 0;
    }catch(const std::exception& failure){std::cerr<<"network acceptance failed: "<<failure.what()<<'\n';return 1;}
}
