// Owner-local real-clock network/prediction acceptance. Faults exist only here.
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#include "RetroFPS/Pvp/SnapshotTimeline.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>
using namespace fps::pvp;
using Clock = std::chrono::steady_clock;
namespace {
void Require(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
const PlayerState& Player(const WorldSnapshot& snapshot, PlayerId id) {
    const auto it = std::find_if(snapshot.players.begin(), snapshot.players.end(),
        [id](const auto& p) { return p.playerId == id; });
    Require(it != snapshot.players.end(), "Missing live player"); return *it;
}
template<class Test> void Wait(Test test) {
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (!test()) { Require(Clock::now() < deadline, "Session startup timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
}
struct Frame { std::int64_t timeNs; double seconds; std::array<std::size_t,2> pending;
    std::array<std::uint32_t,2> queued; std::array<std::uint64_t,2> epoch;
    std::array<double,2> remoteAge; std::array<std::uint64_t,2> resolved;
    std::array<fps::Float3,2> authority; };
}
int main(int argc, char** argv) {
    try {
        std::string gateway="127.0.0.1:8080"; std::filesystem::path arenaPath, output;
        double fps=60, duration=30, stallAt=-1, stallMs=0;
        for (int i=1; i<argc; ++i) {
            const std::string key=argv[i]; Require(i+1<argc,"Missing option value");
            const std::string value=argv[++i];
            if(key=="--gateway")gateway=value; else if(key=="--arena")arenaPath=value;
            else if(key=="--output")output=value; else if(key=="--fps")fps=std::stod(value);
            else if(key=="--duration")duration=std::stod(value);
            else if(key=="--stall-at")stallAt=std::stod(value);
            else if(key=="--stall-ms")stallMs=std::stod(value);
            else throw std::runtime_error("Unknown option: "+key);
        }
        Require((fps==30 || fps==60 || fps==144) && duration>=5 && duration<=3600,
            "Expected FPS 30/60/144 and duration 5..3600");
        Require(!output.empty() && stallMs>=0 && stallMs<=6000,"Invalid output/fault options");
        std::filesystem::create_directories(output);
        MovementTraceWriter trace(output/"clients-commands.jsonl");
        std::string error; const auto arena=Arena::Load(arenaPath,error); Require(arena.has_value(),error);
        std::array<ClientConnection,2> clients;
        for(auto& c:clients)c.SetArenaIdentity(arena->id,arena->version);
        clients[0].CreateAndJoin(gateway);
        Wait([&]{const auto s=clients[0].State();Require(s.error.empty(),s.error);return s.phase==ConnectionPhase::Playing;});
        clients[1].Refresh(gateway);
        Wait([&]{const auto s=clients[1].State();Require(s.error.empty(),s.error);return !s.rooms.empty();});
        clients[1].Join(gateway,clients[1].State().rooms.front().id);
        Wait([&]{const auto a=clients[0].State(),b=clients[1].State();Require(b.error.empty(),b.error);
            return b.phase==ConnectionPhase::Playing && a.snapshot && a.snapshot->players.size()==2;});
        std::array<LocalPlayerPrediction,2> prediction{LocalPlayerPrediction(*arena),LocalPlayerPrediction(*arena)};
        std::array<SnapshotTimeline,2> timelines;
        const std::array ids{clients[0].State().playerId,clients[1].State().playerId};
        std::array<std::uint64_t,2> previousEpoch{},overflowCounts{};
        const auto started=Clock::now(), measurement=started+std::chrono::seconds(2);
        const auto end=measurement+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(duration));
        auto previous=started, deadline=started;
        const auto period=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1/fps));
        std::vector<Frame> frames; frames.reserve(static_cast<std::size_t>((duration+5)*fps)+1000);
        std::size_t resets{}, gaps{}, frozen{}; bool injected{}; std::int64_t releaseNs{};
        std::optional<Clock::time_point> stableSince,recoveredAt;
        std::array<std::optional<fps::Float3>,2> releasePositions;
        while(Clock::now()<end+std::chrono::seconds(2)) {
            auto now=Clock::now();
            if(!injected && stallAt>=0 && std::chrono::duration<double>(now-measurement).count()>=stallAt) {
                std::this_thread::sleep_for(std::chrono::duration<double,std::milli>(stallMs));
                injected=true; now=Clock::now(); releaseNs=MovementTraceNowNs();
            }
            const double elapsed=std::chrono::duration<double>(now-previous).count(); previous=now;
            const bool measured=now>=measurement && now<end;
            if(measured && elapsed>=.1)++gaps;
            Frame frame{MovementTraceNowNs(),elapsed}; bool recovered=true;
            for(std::size_t i=0;i<2;++i) {
                auto drain=clients[i].Drain();
                Require(drain.state.error.empty(),drain.state.error);
                Require(drain.state.phase==ConnectionPhase::Playing && drain.state.snapshot.has_value(),"Live session lost");
                for(const auto& received:drain.snapshots)static_cast<void>(timelines[i].Push(received.snapshot,received.receivedAt));
                const auto& authority=Player(*drain.state.snapshot,ids[i]);
                if(measured && previousEpoch[i] && previousEpoch[i]!=authority.movementEpoch)++resets;
                previousEpoch[i]=authority.movementEpoch;overflowCounts[i]=drain.snapshotHistoryOverflowCount;
                prediction[i].Reconcile(authority,drain.state.snapshot->tick);
                const double age=std::chrono::duration<double>(now-measurement).count();
                // Alternate 300 ms movement and 300 ms stop; no net drift into a wall.
                const auto leg=static_cast<long long>(std::floor(std::max(0.0,age)/.6));
                const float axis=injected && MovementTraceNowNs()-releaseNs<1500000000LL ? 1.0F :
                    (measured && std::fmod(age,.6)<.3 ? (leg%2?-1.0F:1.0F):0.0F);
                if(prediction[i].Advance(elapsed,axis,0,0,0))clients[i].SendInput(prediction[i].PendingInput());
                const auto& p=prediction[i].Observation();
                Require(p.pendingCommands<=MaxPendingCommands,"Prediction window exceeded bound");
                Require(authority.contiguousPendingCommands<=MaxFutureCommands,"Server window exceeded bound");
                if(measured && p.frozen)++frozen;
                const auto remote=timelines[i].Sample(ids[1-i],now);
                frame.pending[i]=p.pendingCommands;frame.queued[i]=authority.contiguousPendingCommands;
                frame.epoch[i]=authority.movementEpoch;frame.remoteAge[i]=remote?remote->latestReceiveAgeSeconds:1e9;
                frame.resolved[i]=authority.lastResolvedCommand;frame.authority[i]=authority.position;
                if(injected && !releasePositions[i])releasePositions[i]=authority.position;
                const bool responded=!injected || (releasePositions[i] &&
                    std::hypot(authority.position.x-releasePositions[i]->x,authority.position.z-releasePositions[i]->z)>.05);
                recovered=recovered && responded && !p.frozen && p.pendingCommands<MaxPendingCommands &&
                    authority.contiguousPendingCommands<=3 && remote && remote->latestReceiveAgeSeconds<.1;
            }
            if(injected && !recoveredAt) {
                if(recovered) { if(!stableSince)stableSince=now;
                    if(now-*stableSince>=std::chrono::milliseconds(250))recoveredAt=now; }
                else stableSince.reset();
            }
            if(measured)frames.push_back(frame);
            deadline+=period;
            if(deadline<Clock::now())deadline=Clock::now(); // Never replay missed wakeups.
            std::this_thread::sleep_until(deadline);
        }
        const bool recoveryOK=!injected || (recoveredAt && std::chrono::duration<double>(stableSince->time_since_epoch()).count()-releaseNs/1e9<=1.5);
        for(auto& c:clients)c.Leave();
        Wait([&]{return clients[0].State().phase==ConnectionPhase::Lobby && clients[1].State().phase==ConnectionPhase::Lobby;});
        trace.Finish();Require(trace.Good(),"Diagnostic trace lost/corrupted data");
        std::ofstream csv(output/"frames.csv");csv<<"time_ns,frame_seconds,pending_a,pending_b,queued_a,queued_b,epoch_a,epoch_b,remote_age_a,remote_age_b,resolved_a,resolved_b,authority_x_a,authority_z_a,authority_x_b,authority_z_b\n"<<std::setprecision(17);
        for(const auto& f:frames)csv<<f.timeNs<<','<<f.seconds<<','<<f.pending[0]<<','<<f.pending[1]<<','<<f.queued[0]<<','<<f.queued[1]<<','<<f.epoch[0]<<','<<f.epoch[1]<<','<<f.remoteAge[0]<<','<<f.remoteAge[1]<<','<<f.resolved[0]<<','<<f.resolved[1]<<','<<f.authority[0].x<<','<<f.authority[0].z<<','<<f.authority[1].x<<','<<f.authority[1].z<<'\n';
        Require(bool(csv),"Frame evidence write failed");
        std::ofstream summary(output/"timing.json");
        summary<<std::setprecision(17)<<"{\"fps\":"<<fps<<",\"duration\":"<<duration
            <<",\"start_ns\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(measurement.time_since_epoch()).count()
            <<",\"end_ns\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(end.time_since_epoch()).count()
            <<",\"player_ids\":["<<ids[0]<<','<<ids[1]<<']'
            <<",\"frames\":"<<frames.size()<<",\"epoch_changes\":"<<resets<<",\"gaps_100ms\":"<<gaps
            <<",\"frozen_frames\":"<<frozen<<",\"history_overflows\":"<<overflowCounts[0]+overflowCounts[1]
            <<",\"injected\":"<<(injected?"true":"false")<<",\"release_ns\":"<<releaseNs
            <<",\"recovery_client_passed\":"<<(recoveryOK?"true":"false")<<"}\n";
        Require(bool(summary),"Summary write failed");Require(recoveryOK,"Client recovery exceeded 1.5 seconds");
        std::cout<<"timing probe completed: "<<frames.size()<<" frames, "<<resets<<" epoch changes, "<<gaps<<" large gaps\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
