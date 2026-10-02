// Included only by the product acceptance executable, never by the application.
#include "start_phase_record.hpp"
struct GameplayStep {
    double at{}; unsigned player{}; const char* name{}; ActionKind kind{ActionKind::Shot};
    std::uint64_t life{1}; ShotRejection expected{ShotRejection::None}; bool target{};
};
std::vector<GameplayStep> GameplayPlan() {
    std::vector<GameplayStep> plan;
    for(unsigned n=0;n<12;++n)plan.push_back({.5+n*.25,0,"magazine-shot"});
    plan.push_back({.5,1,"peer-prime"});
    plan.push_back({3.6,0,"empty",ActionKind::Shot,1,ShotRejection::EmptyMagazine});
    plan.push_back({3.8,0,"reload",ActionKind::Reload});
    plan.push_back({4.05,0,"repeat-reload",ActionKind::Reload,1,ShotRejection::Reloading});
    plan.push_back({4.2,0,"shot-during-reload",ActionKind::Shot,1,ShotRejection::Reloading});
    plan.push_back({4.4,0,"recovery-reload",ActionKind::Reload,1,ShotRejection::Reloading});
    plan.push_back({5.55,0,"full-reload",ActionKind::Reload,1,ShotRejection::MagazineFull});
    plan.push_back({5.75,1,"reload-before-death",ActionKind::Reload});
    for(unsigned n=0;n<4;++n)plan.push_back({6+n*.25,0,"lethal-series",ActionKind::Shot,1,ShotRejection::None,true});
    plan.push_back({7.1,1,"dead-shot",ActionKind::Shot,1,ShotRejection::Dead});
    plan.push_back({7.35,1,"dead-reload",ActionKind::Reload,1,ShotRejection::Dead});
    plan.push_back({8,0,"alive-during-peer-death"});
    plan.push_back({8.4,0,"alive-reload-during-peer-death",ActionKind::Reload});
    plan.push_back({10.25,1,"stale-life",ActionKind::Shot,1,ShotRejection::StaleLife});
    plan.push_back({10.5,1,"future-life",ActionKind::Shot,3,ShotRejection::InvalidLife});
    plan.push_back({10.75,1,"respawn-shot",ActionKind::Shot,2});
    plan.push_back({11.05,1,"respawn-reload",ActionKind::Reload,2});
    for(unsigned n=0;n<8;++n)plan.push_back({10.25+n*.25,0,"post-life-recovery-shot"});
    plan.push_back({12.5,0,"post-life-reload",ActionKind::Reload});
    plan.push_back({13.1,1,"post-reload-shot",ActionKind::Shot,2});
    plan.push_back({14.25,0,"final-shot"});
    std::stable_sort(plan.begin(),plan.end(),[](const auto& a,const auto& b){return a.at<b.at;});
    return plan;
}
Json GameplayCombat(const WorldSnapshot& snapshot) {
    Json value=Json::array();
    for(const auto& c:snapshot.combat)value.push_back({{"player_id",c.playerId},{"life_generation",c.lifeGeneration},
        {"hp",c.hp},{"ammo",c.magazineAmmo},{"reload_action_id",c.reloadActionId},{"reload_start_tick",c.reloadStartTick},
        {"reload_end_tick",c.reloadEndTick},{"next_shot_tick",c.nextAllowedShotTick},
        {"last_shot_id",c.lastShotActionId},{"last_shot_tick",c.lastShotTick}});
    return value;
}
Json GameplayPlayers(const WorldSnapshot& snapshot) {
    Json value=Json::array();
    for(const auto& p:snapshot.players)value.push_back({{"player_id",p.playerId},{"life_generation",p.lifeGeneration},
        {"life_state",static_cast<int>(p.lifeState)},{"life_state_tick",p.lifeStateTick},{"respawn_tick",p.respawnTick},
        {"epoch",p.movementEpoch},{"position",{p.position.x,p.position.y,p.position.z}},
        {"vertical_velocity",p.verticalVelocity},{"grounded",p.grounded},{"yaw",p.yaw},{"pitch",p.pitch}});
    return value;
}
int RunGameplay(const std::string& gateway,const std::filesystem::path& arenaPath,
                const std::filesystem::path& output,int fps,double drainStallMs) {
    Require(!output.empty()&&(fps==30||fps==60||fps==144)&&drainStallMs>=0&&drainStallMs<=1500,"Invalid gameplay probe options");
    std::filesystem::create_directories(output);
    const auto plan=GameplayPlan(); Json declared=Json::array();
    for(std::size_t n=0;n<plan.size();++n){const auto& s=plan[n];declared.push_back({{"ordinal",n},{"at_seconds",s.at},
        {"player_index",s.player},{"name",s.name},{"action_kind",static_cast<int>(s.kind)},
        {"life_generation",s.life},{"expected_rejection",static_cast<int>(s.expected)},{"target",s.target}});}
    {std::ofstream file(output/"gameplay-plan.json");file<<Json{{"protocol",5},{"duration_seconds",16},
        {"actions",declared},{"jump_edges_seconds",{.75,1.6,7.2,10.4}},
        {"fault_policy","Original actions remain denominator; only <=5.6s events may gain InvalidReference/Expired or ammo-dependent terminal outcome under injected faults; later life events retain exact verdict except operations generated inside a declared fault with original reference age >250ms may terminal Expired."}}.dump(2)<<'\n';}
    MovementTraceWriter trace(output/"clients-commands.jsonl");ActionEvidenceWriter evidence(output);
    std::string error;const auto arena=Arena::Load(arenaPath,error);Require(arena.has_value(),error);
    std::array<ClientConnection,2> clients;
    for(auto& c:clients)c.SetArenaIdentity(arena->id,arena->version);
    clients[0].CreateAndJoin(gateway);
    Wait([&]{const auto s=clients[0].State();Require(s.error.empty(),s.error);return s.phase==ConnectionPhase::Playing;});
    clients[1].Refresh(gateway);
    Wait([&]{const auto s=clients[1].State();Require(s.error.empty(),s.error);return !s.rooms.empty();});
    clients[1].Join(gateway,clients[1].State().rooms.front().id);
    Wait([&]{const auto a=clients[0].State(),b=clients[1].State();Require(b.error.empty(),b.error);
        return b.phase==ConnectionPhase::Playing&&a.snapshot&&a.snapshot->players.size()==2&&b.snapshot&&b.snapshot->players.size()==2;});
    const std::array ids{clients[0].State().playerId,clients[1].State().playerId};
    for(const auto& c:clients)Require(c.State().combatRules&&c.State().movementRules,"Missing v5 authoritative rules");
    std::array<LocalPlayerPrediction,2> prediction{LocalPlayerPrediction(*arena),LocalPlayerPrediction(*arena)};
    for(unsigned i=0;i<2;++i)prediction[i].SetMovementRules(*clients[i].State().movementRules);
    std::array<SnapshotTimeline,2> timelines;
    std::array<std::map<ActionId,Json>,2> submitted,decisions;
    std::array<std::size_t,2> maximumRetained{},maximumUnconsumed{};
    std::array<StartPhaseRecorder,2> startPhase;std::uint64_t frameIndex{};
    const auto started=Clock::now();auto previous=started,deadline=started;const auto startNs=MovementTraceNowNs();
    const auto period=std::chrono::nanoseconds(1'000'000'000/fps);
    {std::ofstream ready(output/"ready.json");ready<<Json{{"start_ns",startNs},{"player_ids",ids},{"protocol",5}}.dump();}
    std::size_t nextStep{};std::array<unsigned,2> nextJump{};
    constexpr std::array jumpAt{.75,1.6,7.2,10.4};
    while(std::chrono::duration<double>(Clock::now()-started).count()<16){
        const auto now=Clock::now();const auto ns=MovementTraceNowNs();
        const double age=std::chrono::duration<double>(now-started).count();
        const double elapsed=std::chrono::duration<double>(now-previous).count();previous=now;
        const bool stalled=drainStallMs>0&&age>=4&&age<4+drainStallMs/1000;
        Json frame={{"time_ns",ns},{"frame_seconds",elapsed},{"drain_stalled",stalled}};
        std::array<ClientConnectionState,2> states;
        for(unsigned i=0;i<2;++i){
            auto& state=states[i];
            if(stalled)state=clients[i].State();else{
                auto drain=clients[i].Drain();state=std::move(drain.state);
                Require(!drain.overflow || drainStallMs>0,"Unexpected gameplay snapshot receipt overflow");
                frame["snapshot_history_overflow_count"+std::string(i?"_b":"_a")]=drain.snapshotHistoryOverflowCount;
                for(const auto& received:drain.snapshots)static_cast<void>(timelines[i].Push(received.snapshot,received.receivedAt));
                for(const auto& d:drain.decisions){Require(!decisions[i].contains(d.actionId)&&submitted[i].contains(d.actionId),"Duplicate/unknown gameplay decision");
                    decisions[i][d.actionId]=Decision(d);evidence.Push({{"kind","decision"},{"time_ns",ns},{"player_id",ids[i]},{"decision",Decision(d)}});}
            }
            Require(state.error.empty(),state.error);Require(state.phase==ConnectionPhase::Playing&&state.snapshot,"Gameplay Session lost");
            const auto& authority=Player(*state.snapshot,ids[i]);prediction[i].Reconcile(authority,state.snapshot->tick);
            const bool jump=nextJump[i]<jumpAt.size()&&age>=jumpAt[nextJump[i]];
            if(jump){evidence.Push({{"kind","jump"},{"time_ns",ns},{"player_id",ids[i]},{"ordinal",nextJump[i]},
                {"life_generation",authority.lifeGeneration},{"life_state",static_cast<int>(authority.lifeState)}});++nextJump[i];}
            const float right=static_cast<long long>(age/.4)%2?-.5F:.5F;
            if(prediction[i].Advance(elapsed,0,right,0,0,jump))clients[i].SendInput(prediction[i].PendingInput());
            const auto& p=prediction[i].Observation();startPhase[i].Observe(frameIndex,ns,ids[i],&authority,p);
            const auto remote=timelines[i].Sample(ids[1-i],now);const std::string suffix=i?"_b":"_a";
            frame["pending"+suffix]=p.pendingCommands;frame["queued"+suffix]=authority.contiguousPendingCommands;
            frame["epoch"+suffix]=authority.movementEpoch;frame["remote_age"+suffix]=remote?remote->latestReceiveAgeSeconds:1e9;
            frame["resolved"+suffix]=authority.lastResolvedCommand;frame["frozen"+suffix]=p.frozen;
            frame["authority_x"+suffix]=authority.position.x;frame["authority_z"+suffix]=authority.position.z;
            frame["retained"+suffix]=state.actionTransport.retained;frame["unconsumed"+suffix]=state.actionTransport.unconsumed;
            frame["pending_actions"+suffix]=state.actionTransport.pending;frame["ack"+suffix]=state.actionTransport.acknowledgedThrough;
            frame["retired"+suffix]=state.actionTransport.retiredThrough;frame["snapshot_tick"+suffix]=state.snapshot->tick;
            frame["combat"+suffix]=GameplayCombat(*state.snapshot);frame["players"+suffix]=GameplayPlayers(*state.snapshot);
            maximumRetained[i]=std::max(maximumRetained[i],state.actionTransport.retained);
            maximumUnconsumed[i]=std::max(maximumUnconsumed[i],state.actionTransport.unconsumed);
            Require(p.pendingCommands<=12&&authority.contiguousPendingCommands<=32&&state.actionTransport.retained<=32&&state.actionTransport.unconsumed<=32,"Gameplay window overflow");
        }
        while(nextStep<plan.size()&&age>=plan[nextStep].at){const auto& s=plan[nextStep];auto& state=states[s.player];
            const auto& self=Player(*state.snapshot,ids[s.player]);const auto& target=Player(*state.snapshot,ids[1-s.player]);
            const float yaw=s.target?std::atan2(target.position.x-self.position.x,target.position.z-self.position.z):0;
            const float pitch=s.kind==ActionKind::Reload||s.target?0:1.3F;
            const auto id=clients[s.player].SubmitAction(s.kind,s.life,state.snapshot->tick,yaw,pitch);
            Require(id.has_value(),"Predeclared gameplay action could not allocate immutable ID");
            Json request={{"action_id",*id},{"observed_tick",state.snapshot->tick},{"yaw",yaw},{"pitch",pitch},
                {"action_kind",static_cast<int>(s.kind)},{"life_generation",s.life}};
            submitted[s.player][*id]=request;evidence.Push({{"kind","submitted"},{"time_ns",ns},{"player_id",ids[s.player]},
                {"request",request},{"plan_ordinal",nextStep}});++nextStep;
        }
        evidence.Push(std::move(frame),true);++frameIndex;deadline+=period;if(deadline<Clock::now())deadline=Clock::now();std::this_thread::sleep_until(deadline);
    }
    Json result={{"passed",nextStep==plan.size()},{"gameplay_v5",true},{"protocol",5},{"start_ns",startNs},{"end_ns",MovementTraceNowNs()},
        {"player_ids",ids},{"fps",fps},{"duration",16},{"planned_actions",plan.size()},{"maximum_retained",maximumRetained},
        {"maximum_unconsumed",maximumUnconsumed},{"drain_stall_ms",drainStallMs},{"clients",Json::array()}};
    for(unsigned i=0;i<2;++i){const auto state=clients[i].State();const auto& rules=*state.combatRules;
        result["clients"].push_back({{"player_id",ids[i]},{"submitted",submitted[i].size()},{"decisions",decisions[i].size()},
            {"retired_through",state.actionTransport.retiredThrough},{"retained",state.actionTransport.retained},
            {"maximum_datagram_bytes",state.actionTransport.maxDatagramBytes},{"maximum_batch_shots",state.actionTransport.maxBatchShots},
            {"maximum_hp",rules.maximumHp},{"shot_damage",rules.shotDamage},{"magazine_capacity",rules.magazineCapacity},
            {"cooldown_ticks",rules.cooldownTicks},{"reload_ticks",rules.reloadTicks},{"respawn_ticks",rules.respawnTicks},
            {"combat",GameplayCombat(*state.snapshot)},{"players",GameplayPlayers(*state.snapshot)},{"start_phase",startPhase[i].Json()}});
        if(submitted[i].size()!=decisions[i].size()||state.actionTransport.retained)result["passed"]=false;
    }
    for(auto& c:clients)c.Leave();Wait([&]{return clients[0].State().phase==ConnectionPhase::Lobby&&clients[1].State().phase==ConnectionPhase::Lobby;});
    trace.Finish();evidence.Finish();Require(trace.Good()&&evidence.Good(),"Gameplay trace flush failed");
    std::ofstream summary(output/"action-client.json");summary<<result.dump(2)<<'\n';Require(bool(summary),"Cannot write gameplay result");
    return result["passed"].get<bool>()?0:1;
}
