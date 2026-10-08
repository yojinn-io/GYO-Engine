// Product-owned room-capacity acceptance: up to AcceptanceMaxPlayers headless
// Clients in one room, a refused extra join and bots that fight. No GUI.
// The two-Client probes and analyzers are unchanged; quad_evidence.py reads
// this probe's output.
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/ClientSimulation.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#include "acceptance_capacity.hpp"
#include "acceptance_protocol.hpp"
#include "timer_baseline.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace fps::pvp;
using Clock=std::chrono::steady_clock;
using Json=nlohmann::json;
namespace {
void Require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
template<class F> void Wait(F condition,double seconds,const std::string& why){
    const auto until=Clock::now()+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
    while(!condition()){Require(Clock::now()<until,why);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
}
struct Options {
    std::string gateway;std::filesystem::path arena,output;
    int fps{60};double duration{16};unsigned clients{AcceptanceMaxPlayers};unsigned expectPlayers{};
    bool create{true},fifth{};
    // Passive bots fill a GUI probe's room: they never act, walk to the -X wall
    // (off the line between the GUI spawns), then mark themselves parked and
    // stand until the stop file appears.
    bool passive{};std::filesystem::path trace,parkedFile,stopFile;
};
Options Parse(int argc,char** argv){
    Options o;bool expectSet{};
    for(int i=1;i<argc;++i){const std::string key=argv[i];Require(i+1<argc,"Missing option value");const std::string value=argv[++i];
        if(key=="--gateway")o.gateway=value;else if(key=="--arena")o.arena=value;else if(key=="--output")o.output=value;
        else if(key=="--fps")o.fps=std::stoi(value);else if(key=="--duration")o.duration=std::stod(value);
        else if(key=="--clients")o.clients=static_cast<unsigned>(std::stoul(value));
        else if(key=="--expect-players"){o.expectPlayers=static_cast<unsigned>(std::stoul(value));expectSet=true;}
        else if(key=="--create")o.create=value=="true";else if(key=="--fifth")o.fifth=value=="true";
        else if(key=="--passive")o.passive=value=="true";else if(key=="--trace")o.trace=value;
        else if(key=="--parked-file")o.parkedFile=value;else if(key=="--stop-file")o.stopFile=value;
        else throw std::runtime_error("Unknown option: "+key);}
    if(!expectSet)o.expectPlayers=o.create?o.clients:0;
    Require(!o.gateway.empty()&&!o.arena.empty()&&!o.output.empty(),"--gateway, --arena and --output are required");
    Require(o.fps==30||o.fps==60||o.fps==144,"--fps must be 30, 60 or 144");
    Require(std::isfinite(o.duration)&&o.duration>=4&&o.duration<=1800,"--duration must be 4..1800 seconds");
    Require(o.clients>=1&&o.clients<=AcceptanceMaxPlayers,"--clients must be 1..room capacity");
    Require(o.expectPlayers==0||(o.expectPlayers>=o.clients&&o.expectPlayers<=AcceptanceMaxPlayers),"--expect-players must be 0 or clients..capacity");
    Require(!o.fifth||(o.create&&o.clients==AcceptanceMaxPlayers),"--fifth needs a full room this probe created");
    Require(!o.passive||(!o.create&&!o.fifth&&!o.parkedFile.empty()&&!o.stopFile.empty()),
        "--passive bots join an existing room and need --parked-file and --stop-file");
    if(o.trace.empty())o.trace=o.output/"clients-commands.jsonl";
    return o;
}
const PlayerState* Find(const WorldSnapshot& snapshot,PlayerId id){
    for(const auto& p:snapshot.players)if(p.playerId==id)return &p;
    return nullptr;
}
const CombatState* FindCombat(const WorldSnapshot& snapshot,PlayerId id){
    for(const auto& c:snapshot.combat)if(c.playerId==id)return &c;
    return nullptr;
}
Json SnapshotJson(const WorldSnapshot& s){
    Json players=Json::array(),combat=Json::array();
    for(const auto& p:s.players)players.push_back({{"player_id",p.playerId},{"life_generation",p.lifeGeneration},
        {"life_state",static_cast<int>(p.lifeState)},{"life_state_tick",p.lifeStateTick},{"respawn_tick",p.respawnTick},
        {"epoch",p.movementEpoch},{"position",{p.position.x,p.position.y,p.position.z}},{"yaw",p.yaw}});
    for(const auto& c:s.combat)combat.push_back({{"player_id",c.playerId},{"life_generation",c.lifeGeneration},{"hp",c.hp},
        {"ammo",c.magazineAmmo},{"reload_end_tick",c.reloadEndTick},{"damage_count",c.damageCount},
        {"last_attacker_id",c.lastAttackerId},{"last_damage_tick",c.lastDamageTick}});
    return {{"tick",s.tick},{"players",players},{"combat",combat}};
}
Json DecisionJson(const ShotDecision& d){return {{"action_id",d.actionId},{"resolved_tick",d.resolvedTick},
    {"accepted",d.accepted},{"rejection",static_cast<int>(d.rejection)},{"hit_kind",static_cast<int>(d.hitKind)},
    {"target_id",d.targetId},{"damage",d.damage},{"action_kind",static_cast<int>(d.kind)},
    {"life_generation",d.lifeGeneration},{"target_life_generation",d.targetLifeGeneration}};}

// A bot fires at the nearest living opponent every BotActionSeconds and reloads
// once its own count of shots reaches the magazine; a new life refills it.
constexpr double BotActionSeconds=.25;
// From any spawn of a 20 m arena at 3 m/s, 2.5 s reaches the -X wall.
constexpr double PassiveParkSeconds=2.5;
struct Bot {
    ClientConnection connection;
    ClientSimulation simulation;
    PlayerId id{};
    double nextAction{};
    std::uint64_t life{},shotsThisMagazine{};
    std::optional<ActionId> reload;
    std::map<ActionId,Json> submitted,decisions;
    std::vector<WorldSnapshot> received;
    std::size_t maximumPlayersSeen{};
    explicit Bot(const Arena& arena):simulation(arena){}
};
}
int main(int argc,char** argv){
    std::filesystem::path output;
    try{
        const auto options=Parse(argc,argv);output=options.output;
        std::filesystem::create_directories(output);
        const auto timer=TimerBaseline::Measure("std::this_thread::sleep_until",TimerBaseline::Schedule::Absolute,[](auto deadline){std::this_thread::sleep_until(deadline);});
        std::string error;const auto arena=Arena::Load(options.arena,error);Require(arena.has_value(),error);
        MovementTraceWriter trace(options.trace);
        std::vector<std::unique_ptr<Bot>> bots;
        for(unsigned i=0;i<options.clients;++i){bots.push_back(std::make_unique<Bot>(*arena));
            bots.back()->connection.SetArenaIdentity(arena->id,arena->version,ArenaContentDigest(*arena));}
        const auto playing=[](const ClientConnection& c){const auto s=c.State();Require(s.error.empty(),s.error);return s.phase==ConnectionPhase::Playing;};
        // Joins one at a time, the first creating the room when asked.
        for(unsigned i=0;i<options.clients;++i){auto& c=bots[i]->connection;
            if(i==0&&options.create)c.CreateAndJoin(options.gateway);
            else{auto refreshed=Clock::now()-std::chrono::seconds(1);
                Wait([&]{if(Clock::now()-refreshed>=std::chrono::milliseconds(250)){c.Refresh(options.gateway);refreshed=Clock::now();}
                    const auto s=c.State();return s.phase==ConnectionPhase::Lobby&&!s.rooms.empty();},15,"No room to join");
                c.Join(options.gateway,c.State().rooms.front().id);}
            Wait([&]{return playing(c);},8,"Client could not join the room");
            bots[i]->id=c.State().playerId;}
        const auto full=[&]{for(const auto& b:bots){const auto s=b->connection.State();Require(s.error.empty(),s.error);
            if(!s.snapshot||s.snapshot->players.size()<options.expectPlayers)return false;}return true;};
        if(options.expectPlayers)Wait(full,15,"Room did not reach the expected players");
        for(auto& b:bots){const auto s=b->connection.State();Require(s.combatRules&&s.movementRules,"Missing authoritative rules");
}
        Json fifth=nullptr;
        if(options.fifth){
            // The room is full: an extra Client sees it in the list and is refused.
            ClientConnection extra;extra.SetArenaIdentity(arena->id,arena->version,ArenaContentDigest(*arena));
            extra.Refresh(options.gateway);
            Wait([&]{const auto s=extra.State();Require(s.error.empty(),s.error);return !s.rooms.empty();},8,"Extra Client saw no room");
            const auto room=extra.State().rooms.front();
            extra.Join(options.gateway,room.id);
            Wait([&]{const auto s=extra.State();return !s.error.empty()||s.phase==ConnectionPhase::Playing;},8,"Extra join neither failed nor joined");
            const auto s=extra.State();
            fifth={{"listed_players",room.players},{"listed_capacity",room.capacity},{"error",s.error},
                {"joined",s.phase==ConnectionPhase::Playing}};
            if(s.phase==ConnectionPhase::Playing)extra.Leave();
        }
        // Snapshots received while the room filled up are not part of the run.
        for(auto& b:bots){const auto drain=b->connection.Drain();Require(drain.state.error.empty(),drain.state.error);
            Require(drain.decisions.empty(),"Decision before any action");}
        Json ids=Json::array();for(const auto& b:bots)ids.push_back(b->id);
        const auto started=Clock::now();auto previous=started,deadline=started;const auto startNs=MovementTraceNowNs();
        {std::ofstream ready(output/"ready.json");ready<<Json{{"start_ns",startNs},{"player_ids",ids},{"protocol",AcceptanceProtocolVersion}}.dump();}
        const auto period=std::chrono::nanoseconds(1'000'000'000/options.fps);
        std::vector<double> frameSeconds;std::vector<std::uint64_t> frameTimes;
        frameSeconds.reserve(static_cast<std::size_t>(options.duration*options.fps)+16);frameTimes.reserve(frameSeconds.capacity());
        for(std::size_t i=0;i<bots.size();++i)bots[i]->nextAction=.5+i*BotActionSeconds/bots.size();
        bool parked=false;
        while(std::chrono::duration<double>(Clock::now()-started).count()<options.duration){
            const auto now=Clock::now();const double age=std::chrono::duration<double>(now-started).count();
            const double elapsed=std::chrono::duration<double>(now-previous).count();previous=now;frameSeconds.push_back(elapsed);frameTimes.push_back(MovementTraceNowNs());
            for(std::size_t i=0;i<bots.size();++i){auto& b=*bots[i];
                auto drain=b.connection.Drain();const auto& state=drain.state;
                Require(state.error.empty(),state.error);Require(state.phase==ConnectionPhase::Playing&&state.snapshot,"Session lost");
                Require(!drain.overflow,"Snapshot receipt overflow");
                for(const auto& r:drain.snapshots){b.maximumPlayersSeen=std::max(b.maximumPlayersSeen,r.snapshot.players.size());
                    b.received.push_back(r.snapshot);}
                for(const auto& d:drain.decisions){Require(!b.decisions.contains(d.actionId)&&b.submitted.contains(d.actionId),"Duplicate or unknown decision");
                    b.decisions[d.actionId]=DecisionJson(d);
                    if(b.reload&&*b.reload==d.actionId&&!d.accepted)b.reload.reset();
                    // A refused shot spent no round of this life's magazine.
                    if(d.kind==ActionKind::Shot&&!d.accepted&&d.lifeGeneration==b.life&&b.shotsThisMagazine)--b.shotsThisMagazine;}
                const auto& snapshot=*state.snapshot;const auto* self=Find(snapshot,b.id);const auto* own=FindCombat(snapshot,b.id);
                Require(self&&own,"Own player missing from snapshot");
                b.simulation.Observe(*self,snapshot.tick,state.movementRules);
                // Each bot strafes on its own phase so the four never move as one block;
                // a passive bot walks to the -X wall and stays against it.
                const float right=options.passive?-1.F:static_cast<long long>(age/.4+i*.5)%2?-.5F:.5F;
                if(auto window=b.simulation.Frame(now,{0,right,0,0}))b.connection.SendInput(std::move(*window));
                if(self->lifeGeneration!=b.life){b.life=self->lifeGeneration;b.shotsThisMagazine=0;b.reload.reset();}
                // An accepted reload refills the magazine once the authority reaches its end tick.
                if(b.reload&&b.decisions.contains(*b.reload)&&own->reloadActionId==*b.reload&&snapshot.tick>=own->reloadEndTick){
                    b.reload.reset();b.shotsThisMagazine=0;}
                if(options.passive||age<b.nextAction||age>=options.duration-1)continue;
                b.nextAction+=BotActionSeconds;
                if(self->lifeState!=LifeState::Alive||b.reload)continue;
                const auto& rules=*state.combatRules;
                if(b.shotsThisMagazine>=rules.magazineCapacity){
                    const auto id=b.connection.SubmitAction(ActionKind::Reload,b.life,snapshot.tick);
                    Require(id.has_value(),"Reload could not allocate an action ID");
                    b.reload=*id;
                    b.submitted[*id]={{"action_id",*id},{"observed_tick",snapshot.tick},{"action_kind",static_cast<int>(ActionKind::Reload)},{"life_generation",b.life},{"time_ns",MovementTraceNowNs()}};
                    continue;}
                const PlayerState* target=nullptr;float nearest=std::numeric_limits<float>::max();
                for(const auto& p:snapshot.players){if(p.playerId==b.id||p.lifeState!=LifeState::Alive)continue;
                    const float dx=p.position.x-self->position.x,dz=p.position.z-self->position.z,d=dx*dx+dz*dz;
                    if(d<nearest){nearest=d;target=&p;}}
                if(!target)continue;
                const float yaw=std::atan2(target->position.x-self->position.x,target->position.z-self->position.z);
                const auto id=b.connection.SubmitAction(ActionKind::Shot,b.life,snapshot.tick,yaw,0);
                Require(id.has_value(),"Shot could not allocate an action ID");
                ++b.shotsThisMagazine;
                b.submitted[*id]={{"action_id",*id},{"observed_tick",snapshot.tick},{"action_kind",static_cast<int>(ActionKind::Shot)},
                    {"life_generation",b.life},{"yaw",yaw},{"target_id",target->playerId},{"time_ns",MovementTraceNowNs()}};
            }
            if(options.passive){
                const double elapsed=std::chrono::duration<double>(Clock::now()-started).count();
                if(!parked&&elapsed>=PassiveParkSeconds){std::ofstream file(options.parkedFile);file<<"parked\n";Require(bool(file),"Cannot mark the bots parked");parked=true;}
                if(parked&&std::filesystem::exists(options.stopFile))break;
            }
            deadline+=period;if(deadline<Clock::now())deadline=Clock::now();std::this_thread::sleep_until(deadline);
        }
        Require(!options.passive||parked,"Passive bots ended before parking");
        const auto endNs=MovementTraceNowNs();
        // Outstanding decisions arrive after the last action second; wait for them before leaving.
        Wait([&]{bool done=true;for(auto& b:bots){auto drain=b->connection.Drain();Require(drain.state.error.empty(),drain.state.error);
                for(const auto& d:drain.decisions){Require(!b->decisions.contains(d.actionId)&&b->submitted.contains(d.actionId),"Duplicate or unknown decision");
                    b->decisions[d.actionId]=DecisionJson(d);}
                done=done&&b->decisions.size()==b->submitted.size()&&drain.state.actionTransport.retained==0;}
            return done;},5,"Decisions or retained actions remained after the run");
        Json result={{"protocol",AcceptanceProtocolVersion},{"capacity",AcceptanceMaxPlayers},{"start_ns",startNs},{"end_ns",endNs},
            {"player_ids",ids},{"fps",options.fps},{"duration",options.duration},{"created_room",options.create},
            {"expect_players",options.expectPlayers},{"fifth",fifth},{"bot_action_seconds",BotActionSeconds},
            {"frame_seconds",frameSeconds},{"frame_time_ns",frameTimes},{"timer",timer.Json()},{"clients",Json::array()}};
        const auto spawns=[&]{Json v=Json::array();for(const auto& s:arena->spawns)v.push_back({s.position.x,s.position.y,s.position.z});return v;}();
        result["arena"]={{"id",arena->id},{"radius",arena->radius},{"spawns",spawns}};
        for(const auto& b:bots){const auto s=b->connection.State();
            Json submitted=Json::array(),decided=Json::array();
            for(const auto& [id,value]:b->submitted)submitted.push_back(value);
            for(const auto& [id,value]:b->decisions)decided.push_back(value);
            result["clients"].push_back({{"player_id",b->id},{"submitted",submitted},{"decisions",decided},
                {"retained",s.actionTransport.retained},{"maximum_players_seen",b->maximumPlayersSeen},
                {"maximum_datagram_bytes",s.actionTransport.maxDatagramBytes},{"magazine_capacity",s.combatRules->magazineCapacity}});}
        {std::ofstream snapshots(output/"quad-snapshots.jsonl");
            for(std::size_t i=0;i<bots.size();++i)for(const auto& r:bots[i]->received){auto line=SnapshotJson(r);line["client"]=i;snapshots<<line.dump()<<'\n';}
            Require(bool(snapshots),"Cannot write received snapshots");}
        for(auto& b:bots)b->connection.Leave();
        Wait([&]{for(const auto& b:bots)if(b->connection.State().phase!=ConnectionPhase::Lobby)return false;return true;},8,"Clients did not leave");
        trace.Finish();Require(trace.Good(),"Client trace lost data");
        std::ofstream summary(output/"quad-client.json");summary<<result.dump(2)<<'\n';Require(bool(summary),"Cannot write quad result");
        std::cout<<"quad probe: "<<bots.size()<<" clients completed\n";return 0;
    }catch(const std::exception& e){
        if(!output.empty()){std::ofstream failure(output/"client-failure.json");failure<<Json{{"passed",false},{"error",e.what()}}.dump(2);}
        std::cerr<<"quad probe: "<<e.what()<<'\n';return 1;
    }
}
