// Product-owned real-socket action/movement acceptance. No GUI.
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#include "RetroFPS/Pvp/SnapshotTimeline.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace fps::pvp;
using Clock=std::chrono::steady_clock;
using Json=nlohmann::json;
namespace {
void Require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
// Probe-only bounded writer. The measured loop never formats JSON or writes files.
// Overflow/write failure invalidates the run, including an interrupted final flush.
class ActionEvidenceWriter {
public:
    explicit ActionEvidenceWriter(const std::filesystem::path& output)
        : events_(output/"actions.jsonl"),frames_(output/"action-frames.jsonl"),csv_(output/"frames.csv") {
        Require(bool(events_)&&bool(frames_)&&bool(csv_),"Cannot open action evidence");
        for(std::size_t i=0;i<columns_.size();++i)csv_<<(i?",":"")<<columns_[i];csv_<<'\n';
        worker_=std::thread([this]{Run();});
    }
    ~ActionEvidenceWriter(){Finish();}
    void Push(Json value,bool frame=false){
        std::lock_guard lock(mutex_);
        Require(!failed_ && !stopped_,"Action evidence writer failed");
        if(queue_.size()>=2048){failed_=true;++lost_;throw std::runtime_error("Action evidence queue overflow");}
        queue_.emplace_back(frame,std::move(value));ready_.notify_one();
    }
    void Finish(){
        {std::lock_guard lock(mutex_);stopped_=true;ready_.notify_one();}
        if(worker_.joinable())worker_.join();
    }
    bool Good(){std::lock_guard lock(mutex_);return !failed_;}
private:
    void Run(){
        try{
            for(;;){
                std::pair<bool,Json> item;
                {std::unique_lock lock(mutex_);ready_.wait(lock,[&]{return stopped_||!queue_.empty();});
                    if(queue_.empty())break;item=std::move(queue_.front());queue_.pop_front();}
                if(item.first){frames_<<item.second.dump()<<'\n';++frameCount_;
                    for(std::size_t i=0;i<columns_.size();++i)csv_<<(i?",":"")<<item.second.at(columns_[i]);csv_<<'\n';}
                else{events_<<item.second.dump()<<'\n';++eventCount_;}
                if(!events_||!frames_||!csv_)throw std::runtime_error("Action evidence write failed");
            }
            std::size_t lost;{std::lock_guard lock(mutex_);lost=lost_;}
            events_<<Json{{"kind","evidence_end"},{"records",eventCount_},{"lost",lost}}.dump()<<'\n';
            frames_<<Json{{"kind","evidence_end"},{"records",frameCount_},{"lost",lost}}.dump()<<'\n';
            events_.flush();frames_.flush();csv_.flush();
            if(!events_||!frames_||!csv_)throw std::runtime_error("Action evidence flush failed");
        }catch(...){std::lock_guard lock(mutex_);failed_=true;}
    }
    const std::vector<std::string> columns_{"time_ns","frame_seconds","pending_a","pending_b","queued_a","queued_b","epoch_a","epoch_b","remote_age_a","remote_age_b","resolved_a","resolved_b","authority_x_a","authority_z_a","authority_x_b","authority_z_b"};
    std::ofstream events_,frames_,csv_;
    std::mutex mutex_;std::condition_variable ready_;std::deque<std::pair<bool,Json>> queue_;
    std::thread worker_;bool failed_{},stopped_{};std::size_t frameCount_{},eventCount_{},lost_{};
};
const PlayerState& Player(const WorldSnapshot& snapshot,PlayerId id){
    for(const auto& p:snapshot.players)if(p.playerId==id)return p;
    throw std::runtime_error("Missing player in live snapshot");
}
template<class F> void Wait(F condition){
    const auto until=Clock::now()+std::chrono::seconds(8);
    while(!condition()){Require(Clock::now()<until,"Client startup/leave timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));}
}
Json Decision(const ShotDecision& d){return {{"action_id",d.actionId},{"resolved_tick",d.resolvedTick},
    {"accepted",d.accepted},{"rejection",static_cast<int>(d.rejection)},
    {"hit_kind",static_cast<int>(d.hitKind)},{"target_id",d.targetId},{"damage",d.damage},
    {"action_kind",static_cast<int>(d.kind)},{"life_generation",d.lifeGeneration},{"target_life_generation",d.targetLifeGeneration}};}
}
#include "gameplay_action.hpp"
int main(int argc,char** argv){
    std::filesystem::path output;
    try{
        std::string gateway;std::filesystem::path arenaPath;double duration=6,drainStallMs=0;int fps=60;bool legalShots=false,gameplay=false;
        for(int i=1;i<argc;++i){const std::string key=argv[i];Require(i+1<argc,"Missing option value");
            const std::string value=argv[++i];
            if(key=="--gateway")gateway=value;else if(key=="--arena")arenaPath=value;
            else if(key=="--output")output=value;else if(key=="--duration")duration=std::stod(value);
            else if(key=="--drain-stall-ms")drainStallMs=std::stod(value);
            else if(key=="--fps")fps=std::stoi(value);
            else if(key=="--legal-shots")legalShots=value=="true";
            else if(key=="--gameplay-v5")gameplay=value=="true";
            else throw std::runtime_error("Unknown option: "+key);
        }
        if(gameplay)return RunGameplay(gateway,arenaPath,output,fps,drainStallMs);
        Require(!output.empty() && std::isfinite(duration) && duration>=5 && duration<=(legalShots?1800:15) &&
            (fps==30||fps==60||fps==144) && drainStallMs>=0 && drainStallMs<=1500,"Invalid action-probe options");
        std::filesystem::create_directories(output);
        MovementTraceWriter trace(output/"clients-commands.jsonl");
        ActionEvidenceWriter evidence(output);
        std::string error;const auto arena=Arena::Load(arenaPath,error);Require(arena.has_value(),error);
        std::array<ClientConnection,2> clients;
        for(auto& c:clients)c.SetArenaIdentity(arena->id,arena->version);
        clients[0].CreateAndJoin(gateway);
        Wait([&]{const auto s=clients[0].State();Require(s.error.empty(),s.error);return s.phase==ConnectionPhase::Playing;});
        clients[1].Refresh(gateway);
        Wait([&]{const auto s=clients[1].State();Require(s.error.empty(),s.error);return !s.rooms.empty();});
        clients[1].Join(gateway,clients[1].State().rooms.front().id);
        Wait([&]{const auto a=clients[0].State(),b=clients[1].State();Require(b.error.empty(),b.error);
            return b.phase==ConnectionPhase::Playing && a.snapshot && a.snapshot->players.size()==2;});
        const std::array ids{clients[0].State().playerId,clients[1].State().playerId};
        for(const auto& c:clients)Require(c.State().combatRules.has_value(),"Welcome omitted authoritative CombatRules");
        std::array<LocalPlayerPrediction,2> prediction{LocalPlayerPrediction(*arena),LocalPlayerPrediction(*arena)};
        std::array<SnapshotTimeline,2> timelines;
        std::array<std::map<ActionId,Json>,2> submitted,decisions;
        std::array<std::size_t,2> maximumRetained{},maximumUnconsumed{};
        std::array<unsigned,2> blocked{};
        const auto started=Clock::now();auto previous=started,deadline=started,nextShot=started;
        const auto startNs=MovementTraceNowNs();
        const double warmup=legalShots?2:0,tail=legalShots?2:1;
        const double runDuration=duration+(legalShots?warmup+tail:0);
        if(legalShots)nextShot+=std::chrono::seconds(2);
        {std::ofstream ready(output/"ready.json");ready<<Json{{"start_ns",startNs},{"player_ids",ids}}.dump();}
        const auto framePeriod=std::chrono::nanoseconds(1'000'000'000/fps);
        const auto shotPeriod=std::chrono::nanoseconds(legalShots?400'000'000:1'000'000'000/30);
        while(std::chrono::duration<double>(Clock::now()-started).count()<runDuration){
            const auto now=Clock::now();const auto ns=MovementTraceNowNs();
            const double age=std::chrono::duration<double>(now-started).count();
            const double elapsed=std::chrono::duration<double>(now-previous).count();previous=now;
            const bool stall=drainStallMs>0 && age>=1.5 && age<1.5+drainStallMs/1000;
            const bool shoot=now>=nextShot && age<runDuration-tail;
            if(shoot){if(legalShots)nextShot=now+shotPeriod;
                else{nextShot+=shotPeriod;if(nextShot<now)nextShot=now+shotPeriod;}}
            Json frame={{"time_ns",ns},{"frame_seconds",elapsed},{"drain_stalled",stall}};
            for(std::size_t i=0;i<2;++i){
                ClientConnectionState state;
                if(stall)state=clients[i].State();
                else{
                    auto drain=clients[i].Drain();state=std::move(drain.state);
                    for(const auto& received:drain.snapshots)static_cast<void>(timelines[i].Push(received.snapshot,received.receivedAt));
                    for(const auto& d:drain.decisions){
                        const auto value=Decision(d);
                        Require(!decisions[i].contains(d.actionId),"Client delivered the same action twice");
                        Require(submitted[i].contains(d.actionId),"Decision lacks original client submission");
                        decisions[i][d.actionId]=value;
                        evidence.Push({{"kind","decision"},{"time_ns",ns},{"player_id",ids[i]},{"decision",value}});
                    }
                }
                Require(state.error.empty(),state.error);
                Require(state.phase==ConnectionPhase::Playing && state.snapshot.has_value(),"Live Session lost");
                const auto& authority=Player(*state.snapshot,ids[i]);
                prediction[i].Reconcile(authority,state.snapshot->tick);
                // Both players oscillate together on x, retaining a clear firing line.
                const float right=static_cast<long long>(age/.4)%2?-.5F:.5F;
                if(prediction[i].Advance(elapsed,0,right,0,0))clients[i].SendInput(prediction[i].PendingInput());
                const auto& p=prediction[i].Observation();
                Require(p.pendingCommands<=MaxPendingCommands && authority.contiguousPendingCommands<=MaxFutureCommands,"Movement window overflow");
                const auto remote=timelines[i].Sample(ids[1-i],now);
                const std::string suffix=i?"_b":"_a";
                frame["pending"+suffix]=p.pendingCommands;frame["queued"+suffix]=authority.contiguousPendingCommands;
                frame["epoch"+suffix]=authority.movementEpoch;frame["remote_age"+suffix]=remote?remote->latestReceiveAgeSeconds:1e9;
                frame["resolved"+suffix]=authority.lastResolvedCommand;frame["frozen"+suffix]=p.frozen;
                frame["authority_x"+suffix]=authority.position.x;frame["authority_z"+suffix]=authority.position.z;
                frame["retained"+suffix]=state.actionTransport.retained;frame["unconsumed"+suffix]=state.actionTransport.unconsumed;
                frame["pending_actions"+suffix]=state.actionTransport.pending;
                frame["ack"+suffix]=state.actionTransport.acknowledgedThrough;frame["retired"+suffix]=state.actionTransport.retiredThrough;
                frame["snapshot_tick"+suffix]=state.snapshot->tick;frame["combat"+suffix]=Json::array();
                for(const auto& combat:state.snapshot->combat)frame["combat"+suffix].push_back({{"player_id",combat.playerId},{"hp",combat.hp}});
                maximumRetained[i]=std::max(maximumRetained[i],state.actionTransport.retained);
                maximumUnconsumed[i]=std::max(maximumUnconsumed[i],state.actionTransport.unconsumed);
                Require(state.actionTransport.retained<=MaxActionWindow && state.actionTransport.unconsumed<=MaxActionWindow,"Action receipt window overflow");
                if(shoot){
                    const auto& target=Player(*state.snapshot,ids[1-i]);
                    const float yaw=std::atan2(target.position.x-authority.position.x,target.position.z-authority.position.z);
                    if(const auto id=clients[i].SubmitShot(state.snapshot->tick,yaw,0)){
                        Json request={{"action_id",*id},{"observed_tick",state.snapshot->tick},{"yaw",yaw},{"pitch",0}};
                        submitted[i][*id]=request;
                        evidence.Push({{"kind","submitted"},{"time_ns",ns},{"player_id",ids[i]},{"request",request}});
                    }else ++blocked[i];
                }
            }
            evidence.Push(std::move(frame),true);deadline+=framePeriod;
            if(deadline<Clock::now())deadline=Clock::now();std::this_thread::sleep_until(deadline);
        }
        Json result={{"passed",true},{"start_ns",startNs},{"end_ns",MovementTraceNowNs()},{"player_ids",ids},
            {"maximum_retained",maximumRetained},{"maximum_unconsumed",maximumUnconsumed},{"blocked_submissions",blocked},
            {"drain_stall_ms",drainStallMs},{"duration",duration},{"clients",Json::array()}};
        result["fps"]=fps;result["legal_shots"]=legalShots;result["shot_interval_ms"]=legalShots?400:1000.0/30;
        result["measurement_start_ns"]=startNs+static_cast<std::uint64_t>(warmup*1e9);
        result["measurement_end_ns"]=startNs+static_cast<std::uint64_t>((runDuration-tail)*1e9);
        for(std::size_t i=0;i<2;++i){
            const auto state=clients[i].State();Json hp=Json::array();
            for(const auto& combat:state.snapshot->combat)hp.push_back({{"player_id",combat.playerId},{"hp",combat.hp}});
            result["clients"].push_back({{"player_id",ids[i]},{"submitted",submitted[i].size()},{"decisions",decisions[i].size()},
                {"retired_through",state.actionTransport.retiredThrough},{"retained",state.actionTransport.retained},
                {"sent_batches",state.actionTransport.sentBatches},{"sent_shots",state.actionTransport.sentShots},
                {"sent_ack_only",state.actionTransport.sentAckOnlyBatches},{"maximum_datagram_bytes",state.actionTransport.maxDatagramBytes},
                {"maximum_batch_shots",state.actionTransport.maxBatchShots},{"maximum_hp",state.combatRules->maximumHp},
                {"shot_damage",state.combatRules->shotDamage},{"combat",hp}});
            if(submitted[i].size()!=decisions[i].size() || state.actionTransport.retained!=0)result["passed"]=false;
        }
        for(auto& c:clients)c.Leave();
        Wait([&]{return clients[0].State().phase==ConnectionPhase::Lobby && clients[1].State().phase==ConnectionPhase::Lobby;});
        trace.Finish();Require(trace.Good(),"Trace lost data");
        evidence.Finish();Require(evidence.Good(),"Action evidence lost data");
        std::ofstream summary(output/"action-client.json");summary<<result.dump(2)<<'\n';
        Require(bool(summary),"Evidence write failed");
        std::cout<<result.dump()<<'\n';return result["passed"].get<bool>()?0:1;
    }catch(const std::exception& e){
        if(!output.empty()){std::ofstream failure(output/"client-failure.json");failure<<Json{{"passed",false},{"error",e.what()}}.dump(2);}
        std::cerr<<e.what()<<'\n';return 1;
    }
}
