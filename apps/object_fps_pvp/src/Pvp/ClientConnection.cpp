#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/NetworkStatistics.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "engine/math/scalar/Scalar.hpp"
#include "engine/threads/RoleThread.hpp"
#include "client_v6.pb.h"
#include <asio.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <map>
#include <limits>
#include <random>
#include <syncstream>

namespace fps::pvp {
namespace pb=object_fps_pvp::client::v6;
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
using asio::ip::udp;
namespace {
std::string Id(const Json& value) {return value.is_string()?value.get<std::string>():value.dump();}
std::string Hex64(std::uint64_t value) {
    char text[17];std::snprintf(text,sizeof text,"%016llx",static_cast<unsigned long long>(value));return text;
}
std::string BaseUrl(std::string address) {
    if(!address.starts_with("http://")) address="http://"+address;
    if(address.size()>256 || address.find_first_of("\r\n\t ")!=std::string::npos)
        throw std::invalid_argument("Invalid Gateway address");
    while(address.ends_with('/'))address.pop_back();
    return address;
}
Json Response(const httplib::Result& result) {
    if(!result)throw std::runtime_error("Gateway HTTP request failed: "+std::string(httplib::to_string(result.error())));
    auto body=Json::parse(result->body);
    if(result->status<200 || result->status>=300)
        throw std::runtime_error(body.value("error",std::string("Gateway rejected request")));
    return body;
}
std::unique_ptr<httplib::Client> Http(const std::string& base) {
    auto client=std::make_unique<httplib::Client>(base);
    client->set_connection_timeout(2,0);client->set_read_timeout(2,0);client->set_write_timeout(2,0);
    client->set_max_timeout(3000);return client;
}
std::optional<CombatRules> ReadRules(const pb::Welcome& welcome) {
    if(!welcome.has_combat_rules())return {};
    const auto& rules=welcome.combat_rules();
    if(!rules.maximum_hp() || !rules.shot_damage() || rules.shot_damage()>rules.maximum_hp() ||
       !rules.cooldown_ticks() || !std::isfinite(rules.shot_range()) || rules.shot_range()<=0 ||
       !rules.maximum_reference_age_ms() || !rules.magazine_capacity() ||
       !rules.reload_ticks() || !rules.respawn_ticks())return {};
    return CombatRules{rules.maximum_hp(),rules.shot_damage(),rules.cooldown_ticks(),rules.shot_range(),
        std::chrono::milliseconds{rules.maximum_reference_age_ms()},
        rules.magazine_capacity(),rules.reload_ticks(),rules.respawn_ticks()};
}
ShotRejection ReadRejection(pb::ShotRejection value) {
    switch(value) {
    case pb::REJECTION_NONE:return ShotRejection::None;
    case pb::REJECTION_INVALID_REFERENCE:return ShotRejection::InvalidReference;
    case pb::REJECTION_EXPIRED:return ShotRejection::Expired;
    case pb::REJECTION_COOLDOWN:return ShotRejection::Cooldown;
    case pb::REJECTION_STALE_LIFE:return ShotRejection::StaleLife;
    case pb::REJECTION_INVALID_LIFE:return ShotRejection::InvalidLife;
    case pb::REJECTION_DEAD:return ShotRejection::Dead;
    case pb::REJECTION_RELOADING:return ShotRejection::Reloading;
    case pb::REJECTION_EMPTY_MAGAZINE:return ShotRejection::EmptyMagazine;
    case pb::REJECTION_MAGAZINE_FULL:return ShotRejection::MagazineFull;
    default:throw std::invalid_argument("Invalid rejection");
    }
}
bool SameRules(const CombatRules& a,const CombatRules& b) {
    return a.maximumHp==b.maximumHp && a.shotDamage==b.shotDamage && a.cooldownTicks==b.cooldownTicks &&
        a.shotRange==b.shotRange && a.maximumReferenceAge==b.maximumReferenceAge &&
        a.magazineCapacity==b.magazineCapacity && a.reloadTicks==b.reloadTicks && a.respawnTicks==b.respawnTicks;
}
}
struct ClientConnection::Impl {
    enum class Action{Refresh,Create,Join,Leave};
    struct Request{Action action;std::string gateway,room;std::uint64_t generation;};
    mutable std::mutex mutex;
    ClientConnectionState state;
    std::deque<Request> requests;
    std::optional<PlayerInput> latestInput;
    std::vector<MovementCommand> submittedCommands;
    std::uint64_t submittedEpoch{1};
    struct ActionEntry {
        ShotRequest request;
        std::optional<ShotDecision> decision;
        bool delivered{};
        Clock::time_point receivedAt{}; // diagnostics: when the decision first arrived
    };
    std::map<ActionId,ActionEntry> actions; // Guarded by mutex, including allocation.
    ActionTransportState actionTransport;
    ActionId actionSendCursor{};
    Clock::time_point nextActionSendAt{}; // Worker-only deadline, never reset by Drain.
    std::deque<ReceivedSnapshot> receivedSnapshots;
    bool snapshotOverflow{};
    std::uint64_t snapshotOverflowCount{};
    // The simulation role's own copy of the history (DrainSimulation).
    std::deque<ReceivedSnapshot> simulationSnapshots;
    bool simulationConsumer{};
    std::uint64_t simulationOverflowCount{};
    std::vector<ArenaIdentity> installedArenas;
    std::atomic<std::uint64_t> generation{0};
    asio::io_context io;
    udp::socket socket{io};
    udp::endpoint endpoint;
    std::string base,room,token;
    std::uint64_t session{};
    std::uint64_t activeGeneration{};
    std::uint32_t sentSequence{},receivedSequence{};
    bool haveSequence{},welcomed{};
    bool haveSentInput{};
    // Input token bucket (InputSendBurst) and what the last input send carried.
    double inputTokens{InputSendBurst};
    Clock::time_point inputTokensAt{};
    std::uint64_t sentInputEpoch{},sentInputLife{},sentInputThrough{};
    bool roomRefreshFailed{}; // Guarded by mutex, like state.error.
    Clock::time_point helloAt{},connectedAt{},receivedAt{},lobbyPollAt{},nextInputSendAt{};
    // The worker is one asio io thread. Nothing polls: the socket becoming
    // readable, a post from another thread (a request, new input, a new action,
    // an ACK advance) and one timer at the earliest deadline wake it, and each
    // wake runs Service, the same request/network/lobby pass as before.
    asio::steady_timer deadlineTimer{io};
    std::optional<Clock::time_point> deadlineAt;
    std::atomic<bool> servicePosted{};
    bool readWaiting{};
    std::uint64_t transport{}; // bumped when the socket closes; stale waits compare it
    // Diagnostics: this worker's wakes (Service passes) and CPU per statistics window.
    Clock::time_point statisticsAt{};
    std::optional<double> cpuAt;
    std::uint64_t wakes{};
    Engine::Threads::RoleContext* role{}; // the worker's own context, for its precise Waiter
    std::optional<Engine::Threads::RoleThread> worker;
    Impl() {
        auto started=Engine::Threads::RoleThread::Start({"gyo-client-net",Engine::Threads::ThreadPriority::Interactive},
            [this](Engine::Threads::RoleContext& context){role=&context;Run();});
        if(!started)throw std::runtime_error(Engine::Base::Describe(started.error()));
        worker.emplace(std::move(*started));
    }
    ~Impl(){
        if(!worker)return;
        worker->RequestStop();io.stop();worker.reset();
    }
    // Any thread: one pending Service pass is enough. Never throws: a failed
    // post clears the flag, so a later post (or the timer) still wakes the worker.
    void PostService() noexcept {
        if(servicePosted.exchange(true))return;
        try{asio::post(io,[this]{servicePosted.store(false);Service();});}
        catch(...){servicePosted.store(false);}
    }

    // Called only while holding mutex at a connection-generation boundary.
    void ResetActions() {
        actions.clear();actionTransport={};actionSendCursor=0;
        state.combatRules.reset();state.movementRules.reset();state.actionTransport={};
    }
    ActionTransportState ActionState() const {
        auto result=actionTransport;result.retained=actions.size();
        for(const auto& [id,entry]:actions) {
            if(!entry.decision)++result.pending;
            else if(!entry.delivered)++result.unconsumed;
        }
        return result;
    }
    ClientConnectionState CopyState() const {
        auto result=state;result.actionTransport=ActionState();result.generation=generation.load();return result;
    }
    void CloseTransport() {
        asio::error_code error;socket.close(error);
        // Keep credentials until HTTP confirms the idempotent leave. Losing
        // them on a failed request can leave an old pawn occupying a room slot.
        welcomed=false;haveSequence=false;sentSequence=0;activeGeneration=0;
        haveSentInput=false;nextInputSendAt={};nextActionSendAt={};
        readWaiting=false;++transport;
        inputTokens=InputSendBurst;inputTokensAt={};
    }
    void Close() {
        CloseTransport();
        std::scoped_lock lock(mutex);latestInput.reset();submittedCommands.clear();submittedEpoch=1;
        receivedSnapshots.clear();snapshotOverflow=false;snapshotOverflowCount=0;
        simulationSnapshots.clear();simulationOverflowCount=0;
        ResetActions();
    }
    void Failure(std::string error,std::uint64_t failedGeneration) {
        CloseTransport();std::scoped_lock lock(mutex);
        if(failedGeneration!=generation.load())return;
        ++generation;
        // Publish the lifecycle transition and its generation-scoped history
        // reset together. Drain must not see an old generation's count reset.
        latestInput.reset();submittedCommands.clear();submittedEpoch=1;
        receivedSnapshots.clear();snapshotOverflow=false;snapshotOverflowCount=0;
        simulationSnapshots.clear();simulationOverflowCount=0;
        ResetActions();
        std::osyncstream(std::clog)<<"[ObjectFPS/PvP] connection failed player="<<state.playerId
            <<" reason="<<error<<'\n';
        state.phase=ConnectionPhase::Lobby;state.error=std::move(error);state.snapshot.reset();state.playerId=0;
        state.rooms.clear();state.arenaId.clear();roomRefreshFailed=false;
    }
    void Push(Action action,std::string gateway={},std::string roomId={}) {
        std::scoped_lock lock(mutex);
        // State publication and cancellation share this mutex. Checking an
        // atomic generation before locking cannot prevent a stale completion.
        const auto next=++generation;
        requests.clear();requests.push_back({action,std::move(gateway),std::move(roomId),next});
        state.error.clear();state.snapshot.reset();state.playerId=0;state.rooms.clear();state.arenaId.clear();roomRefreshFailed=false;
        state.phase=ConnectionPhase::Requesting;
        latestInput.reset();submittedCommands.clear();
        receivedSnapshots.clear();snapshotOverflow=false;snapshotOverflowCount=0;
        simulationSnapshots.clear();simulationOverflowCount=0;
        ResetActions();
        PostService();
    }
    // Both join paths compare identity first, then content (pv6 contract §2).
    // A missing, non-integer or zero digest is a content mismatch.
    // The Match's arena selects one installed arena by id and version.
    void CheckArena(const std::string& id,std::uint32_t version,std::optional<std::uint64_t> digest) {
        std::scoped_lock lock(mutex);
        if(installedArenas.empty())return;
        const auto installed=std::find_if(installedArenas.begin(),installedArenas.end(),
            [&](const ArenaIdentity& arena){return arena.id==id && arena.version==version;});
        if(installed==installedArenas.end()) {
            std::string list;
            for(const auto& arena:installedArenas)list+=(list.empty()?"":", ")+arena.id+" version "+std::to_string(arena.version);
            throw std::runtime_error("arena_identity_mismatch: Match arena "+id+" version "+std::to_string(version)+
                ", this Client has "+list);
        }
        if(!digest || *digest!=installed->digest)
            throw std::runtime_error("arena_content_mismatch: arena "+id+" content differs (Match "+
                (digest?Hex64(*digest):std::string("missing"))+", this Client "+Hex64(installed->digest)+")");
        state.arenaId=id;
    }
    void DisconnectRemote() {
        Close();
        if(session && !base.empty() && !room.empty()) {
            try {
                auto http=Http(base);
                const Json body={{"session_id",session},{"session_token",token}};
                const auto released=Response(http->Post("/rooms/"+room+"/leave",body.dump(),"application/json"));
                if(!released.is_object() || !released.value("left",false))
                    throw std::runtime_error("Gateway did not confirm departure");
            } catch(const std::exception& error) {
                throw std::runtime_error("Previous Match departure is unconfirmed; refresh to retry cleanup: "+std::string(error.what()));
            }
        }
        session=0;token.clear();
    }
    std::vector<LobbyRoom> FetchRooms() {
        auto http=Http(base);
        const auto body=Response(http->Get("/rooms"));
        std::vector<LobbyRoom> rooms;
        const auto& values=body.is_array()?body:body.at("rooms");
        for(const auto& r:values)rooms.push_back({Id(r.at("id")),r.value("players",0u),r.value("capacity",MaxPlayers)});
        return rooms;
    }
    void RefreshLobby(std::uint64_t requestGeneration) {
        try {
            auto rooms=base.empty()?std::vector<LobbyRoom>{}:FetchRooms();
            std::scoped_lock lock(mutex);
            if(requestGeneration!=generation.load())return;
            state.rooms=std::move(rooms);state.phase=ConnectionPhase::Lobby;
            state.snapshot.reset();state.playerId=0;state.arenaId.clear();
        } catch(const std::exception& error) {
            // Departure is already confirmed. This failure belongs only to
            // the room list and can be cleared by a later background refresh.
            std::scoped_lock lock(mutex);
            if(requestGeneration!=generation.load())return;
            state.rooms.clear();state.phase=ConnectionPhase::Lobby;
            state.error="Room list refresh failed: "+std::string(error.what());roomRefreshFailed=true;
        }
        lobbyPollAt=Clock::now();
    }
    void PollLobby() {
        // An unconfirmed departure is a barrier. Only an explicit action
        // retries it; background polling must not hide a failed cleanup.
        if(session || base.empty() || Clock::now()-lobbyPollAt<std::chrono::seconds(1))return;
        std::uint64_t polledGeneration{};
        {
            std::scoped_lock lock(mutex);
            if(state.phase!=ConnectionPhase::Lobby || !requests.empty())return;
            polledGeneration=generation.load();
        }
        try {
            auto rooms=FetchRooms();
            std::scoped_lock lock(mutex);
            if(polledGeneration!=generation.load() || state.phase!=ConnectionPhase::Lobby || !requests.empty())return;
            state.rooms=std::move(rooms);
            if(roomRefreshFailed){state.error.clear();roomRefreshFailed=false;}
        } catch(const std::exception& error) {
            std::scoped_lock lock(mutex);
            if(polledGeneration!=generation.load() || state.phase!=ConnectionPhase::Lobby || !requests.empty())return;
            state.rooms.clear();
            if(state.error.empty() || roomRefreshFailed) {
                state.error="Room list refresh failed: "+std::string(error.what());roomRefreshFailed=true;
            }
        }
        lobbyPollAt=Clock::now();
    }
    void Handle(const Request& request) {
        DisconnectRemote();
        if(request.generation!=generation.load())return;
        if(request.action==Action::Leave) {
            RefreshLobby(request.generation);return;
        }
        base=BaseUrl(request.gateway);
        auto http=Http(base);
        if(request.action==Action::Refresh) {
            RefreshLobby(request.generation);return;
        }
        room=request.room;
        if(request.action==Action::Create) {
            const auto created=Response(http->Post("/rooms","{}","application/json"));
            room=Id(created.at("id"));
        }
        if(request.generation!=generation.load())return;
        static std::atomic<std::uint64_t> nextRequest{};
        const auto requestId=std::to_string(Clock::now().time_since_epoch().count())+"-"+std::to_string(++nextRequest);
        const Json join={{"request_id",requestId},{"protocol_version",wire::ProtocolVersion}};
        const auto joined=Response(http->Post("/rooms/"+room+"/join",join.dump(),"application/json"));
        // Keep credentials even if cancelled so the next action releases its reservation.
        session=joined.at("session_id").get<std::uint64_t>();token=joined.at("session_token").get<std::string>();
        if(request.generation!=generation.load())return;
        // Compare the full JSON value: a non-integer or wider value must not truncate to the version.
        if(const auto& version=joined.at("protocol_version");!version.is_number_unsigned() ||
           version.get<std::uint64_t>()!=wire::ProtocolVersion)throw std::runtime_error("Client protocol mismatch");
        const auto digest=joined.find("arena_digest");
        CheckArena(joined.at("arena_id").get<std::string>(),joined.at("arena_version").get<std::uint32_t>(),
            digest!=joined.end() && digest->is_number_unsigned() && digest->get<std::uint64_t>()!=0 ?
                std::optional<std::uint64_t>{digest->get<std::uint64_t>()} : std::nullopt);
        udp::resolver resolver(io);
        const auto resolved=resolver.resolve(udp::v4(),joined.at("udp_ip").get<std::string>(),std::to_string(joined.at("udp_port").get<unsigned>()));
        endpoint=*resolved.begin();socket.open(endpoint.protocol());socket.bind(udp::endpoint(endpoint.protocol(),0));socket.non_blocking(true);
        connectedAt=receivedAt=Clock::now();helloAt={};
        std::scoped_lock lock(mutex);
        if(request.generation!=generation.load())return;
        activeGeneration=request.generation;
        state.playerId=joined.at("player_id").get<PlayerId>();state.phase=ConnectionPhase::Connecting;
    }
    bool Send(wire::Type type,const std::string& payload) {
        const auto bytes=wire::Encode({type,session,++sentSequence,payload});
        asio::error_code error;socket.send_to(asio::buffer(bytes),endpoint,0,error);
        if(error && error!=asio::error::would_block && error!=asio::error::try_again)throw std::runtime_error(error.message());
        return !error;
    }
    // Validate the complete batch before mutating either decisions or retirement.
    // Snapshot replacement and the game thread never own the only result copy.
    bool AcceptResults(const pb::ActionResults& message,Clock::time_point arrivedAt) {
        if(!state.combatRules || message.decisions_size()>static_cast<int>(MaxActionBatch) ||
           message.retired_through()>actionTransport.acknowledgedThrough)return false;
        std::vector<ShotDecision> decoded;decoded.reserve(message.decisions_size());
        for(const auto& value:message.decisions()) {
            if(!value.action_id() || value.action_id()>actionTransport.allocatedThrough || !value.resolved_tick() ||
               !value.life_generation() || (value.kind()!=pb::ACTION_SHOT && value.kind()!=pb::ACTION_RELOAD) ||
               !pb::ShotRejection_IsValid(value.rejection()) || !pb::ShotHitKind_IsValid(value.hit_kind()) ||
               (value.accepted()!=(value.rejection()==pb::REJECTION_NONE)) ||
               (!value.accepted() && (value.hit_kind()!=pb::HIT_MISS || value.target_id() || value.damage())) ||
               (value.hit_kind()==pb::HIT_PLAYER && (!value.target_id() || value.target_id()==state.playerId)) ||
               (value.hit_kind()!=pb::HIT_PLAYER && (value.target_id() || value.damage())) ||
               value.damage()>state.combatRules->shotDamage ||
               (value.hit_kind()==pb::HIT_PLAYER && !value.target_life_generation()) ||
               (value.hit_kind()!=pb::HIT_PLAYER && value.target_life_generation()) ||
               (value.kind()==pb::ACTION_RELOAD && (value.hit_kind()!=pb::HIT_MISS || value.target_id() || value.damage())))return false;
            ShotDecision decision{value.action_id(),value.resolved_tick(),value.accepted(),
                ReadRejection(value.rejection()),static_cast<ShotHitKind>(value.hit_kind()),
                value.target_id(),value.damage(),value.kind()==pb::ACTION_SHOT?ActionKind::Shot:ActionKind::Reload,
                value.life_generation(),value.target_life_generation()};
            if(std::any_of(decoded.begin(),decoded.end(),[&](const auto& d){return d.actionId==decision.actionId;}))return false;
            const auto found=actions.find(decision.actionId);
            if(decision.actionId>actionTransport.retiredThrough &&
               (found==actions.end() || found->second.request.kind!=decision.kind ||
                found->second.request.lifeGeneration!=decision.lifeGeneration || (found->second.decision && *found->second.decision!=decision)))return false;
            decoded.push_back(decision);
        }
        for(const auto& decision:decoded) {
            const auto found=actions.find(decision.actionId);
            if(found==actions.end())continue;
            if(!found->second.decision)found->second.receivedAt=arrivedAt;
            found->second.decision=decision;
        }
        actionTransport.retiredThrough=Engine::Math::Max(actionTransport.retiredThrough,message.retired_through());
        while(!actions.empty() && actions.begin()->first<=actionTransport.retiredThrough)actions.erase(actions.begin());
        return true;
    }
    void SendActions() {
        pb::ActionBatch message;
        {
            std::scoped_lock lock(mutex);
            if(activeGeneration!=generation.load() || !welcomed || Clock::now()<nextActionSendAt)return;
            auto cursor=actions.upper_bound(actionSendCursor);
            // Visit the whole 32-entry window, wrapping past delivered results.
            // A lost first batch must not starve behind the newest eight IDs.
            for(std::size_t visited=0;visited<actions.size() && message.shots_size()<static_cast<int>(MaxActionBatch);++visited) {
                if(cursor==actions.end())cursor=actions.begin();
                const auto& [id,entry]=*cursor++;
                if(entry.decision)continue;
                auto* shot=message.add_shots();shot->set_action_id(id);
                shot->set_observed_authority_tick(entry.request.observedAuthorityTick);
                shot->set_kind(entry.request.kind==ActionKind::Shot?pb::ACTION_SHOT:pb::ACTION_RELOAD);
                shot->set_life_generation(entry.request.lifeGeneration);
                if(entry.request.kind==ActionKind::Shot){shot->set_yaw(entry.request.yaw);shot->set_pitch(entry.request.pitch);}
                actionSendCursor=id;
            }
            if(message.shots().empty() && actionTransport.acknowledgedThrough<=actionTransport.retiredThrough)return;
            message.set_acknowledged_through(actionTransport.acknowledgedThrough);
        }
        const auto payload=message.SerializeAsString();
        const bool sent=Send(wire::Type::Actions,payload);
        // New requests, ACK-only packets and retransmissions share this deadline;
        // successful retirement never resets it and overdue slots are discarded.
        nextActionSendAt=Clock::now()+std::chrono::nanoseconds{(1'000'000'000+ActionSendRate-1)/ActionSendRate};
        if(sent) {
            std::scoped_lock lock(mutex);
            if(activeGeneration!=generation.load())return;
            ++actionTransport.sentBatches;actionTransport.sentShots+=message.shots_size();
            if(message.shots().empty())++actionTransport.sentAckOnlyBatches;
            actionTransport.maxPayloadBytes=Engine::Math::Max(actionTransport.maxPayloadBytes,payload.size());
            actionTransport.maxDatagramBytes=Engine::Math::Max(actionTransport.maxDatagramBytes,payload.size()+wire::HeaderSize);
            actionTransport.maxBatchShots=Engine::Math::Max(actionTransport.maxBatchShots,static_cast<std::size_t>(message.shots_size()));
        }
    }
    void Network() {
        if(!socket.is_open() || activeGeneration!=generation.load())return;
        const auto now=Clock::now();
        // Session liveness belongs to the I/O worker. A busy render thread must
        // not disconnect a healthy socket, and a keepalive must not renew any
        // movement input: active Hello only receives another Welcome.
        const auto helloInterval=welcomed?std::chrono::milliseconds(1000):std::chrono::milliseconds(250);
        if(now-helloAt>=helloInterval) {
            pb::Hello hello;hello.set_session_token(token);Send(wire::Type::Hello,hello.SerializeAsString());helloAt=now;
        }
        std::array<std::uint8_t,2048> bytes{};
        for(int count=0;count<64;++count) {
            udp::endpoint sender;asio::error_code error;
            const auto size=socket.receive_from(asio::buffer(bytes),sender,0,error);
            if(error==asio::error::would_block || error==asio::error::try_again)break;
            if(error)throw std::runtime_error(error.message());
            const auto arrivedAt=Clock::now();
            if(sender!=endpoint)continue;
            const auto packet=wire::Decode(std::span(bytes).first(size));
            if(!packet || packet->session!=session || (haveSequence && !wire::Newer(packet->sequence,receivedSequence)))continue;
            if(packet->type==wire::Type::Welcome) {
                pb::Welcome message;if(!message.ParseFromString(packet->payload))continue;
                CheckArena(message.arena_id(),message.arena_version(),
                    message.arena_digest()?std::optional<std::uint64_t>{message.arena_digest()}:std::nullopt);
                if(message.tick_rate()!=AuthorityTickRate || message.snapshot_rate()!=AuthorityTickRate/SnapshotIntervalTicks)
                    throw std::runtime_error("Unsupported Match cadence");
                const auto rules=ReadRules(message);
                if(!rules)throw std::runtime_error("Invalid Match combat rules");
                if(!std::isfinite(message.jump_height()) || message.jump_height()<=0 || message.jump_height()>10 ||
                   !std::isfinite(message.gravity()) || message.gravity()<=0 || message.gravity()>1000)
                    throw std::runtime_error("Invalid Match movement rules");
                std::scoped_lock lock(mutex);
                if(activeGeneration!=generation.load())return;
                if(message.player_id()!=state.playerId)throw std::runtime_error("Unexpected player identity");
                if(state.combatRules && !SameRules(*state.combatRules,*rules))
                    throw std::runtime_error("Match combat rules changed within a session");
                if(state.movementRules && (state.movementRules->jumpHeight!=message.jump_height() ||
                    state.movementRules->gravity!=message.gravity()))throw std::runtime_error("Match movement rules changed within a session");
                state.movementRules=MovementRules{message.jump_height(),message.gravity()};
                state.combatRules=rules;
                welcomed=true;
            } else if(packet->type==wire::Type::Snapshot) {
                if(!welcomed)continue;
                pb::WorldSnapshot message;if(!message.ParseFromString(packet->payload) ||
                    message.tick()==0 || message.players_size()>static_cast<int>(MaxPlayers) || message.combat_size()!=message.players_size())continue;
                WorldSnapshot snapshot;snapshot.tick=message.tick();bool self=false,valid=true;
                PlayerId own;std::uint32_t maximumHp{},magazineCapacity{};{
                    std::scoped_lock lock(mutex);
                    if(activeGeneration!=generation.load())return;
                    own=state.playerId;
                    if(!state.combatRules)continue;
                    maximumHp=state.combatRules->maximumHp;magazineCapacity=state.combatRules->magazineCapacity;
                }
                for(const auto& p:message.players()) {
                    if(p.player_id()==0 || std::any_of(snapshot.players.begin(),snapshot.players.end(),
                        [&](const PlayerState& existing){return existing.playerId==p.player_id();}) ||
                        !std::isfinite(p.x()) || !std::isfinite(p.y()) || !std::isfinite(p.z()) ||
                        !ValidMovementCommand({1,0,0,p.yaw(),p.pitch()}) || p.movement_epoch()==0 ||
                        p.contiguous_pending_commands()>MaxFutureCommands || !p.life_generation() ||
                        !std::isfinite(p.vertical_velocity()) ||
                        (p.life_state()!=pb::LIFE_ALIVE && p.life_state()!=pb::LIFE_DEAD) ||
                        p.life_state_tick()>message.tick() ||
                        (p.life_state()==pb::LIFE_ALIVE && p.respawn_tick()!=0) ||
                        (p.life_state()==pb::LIFE_DEAD && (!p.life_state_tick() || p.respawn_tick()<=p.life_state_tick())) ||
                        p.has_movement_slack_sequence()!=p.has_movement_slack_us() ||
                        (p.has_movement_slack_us() && (p.movement_slack_us()>MaxMovementSlackMicros || p.movement_slack_us()<-MaxMovementSlackMicros)) ||
                        (p.has_movement_slack_sequence() && (p.movement_slack_sequence()==0 || p.movement_slack_sequence()>p.last_resolved_command())) ||
                        p.connection_quality_failures()>=ConnectionQualityFailedWindows) {valid=false;break;}
                    snapshot.players.push_back({p.player_id(),{p.x(),p.y(),p.z()},p.yaw(),p.pitch(),p.last_resolved_command(),
                        p.movement_epoch(),p.contiguous_pending_commands(),p.vertical_velocity(),p.grounded(),p.life_generation(),
                        p.life_state()==pb::LIFE_ALIVE?LifeState::Alive:LifeState::Dead,p.life_state_tick(),p.respawn_tick(),
                        p.has_movement_slack_sequence()?std::optional<std::uint64_t>{p.movement_slack_sequence()}:std::nullopt,
                        p.has_movement_slack_us()?std::optional<std::int32_t>{p.movement_slack_us()}:std::nullopt,
                        p.connection_quality_failures()});
                    self|=p.player_id()==own;
                }
                for(const auto& combat:message.combat()) {
                    if(!combat.player_id() || combat.hp()>maximumHp || !combat.life_generation() || combat.magazine_ammo()>magazineCapacity ||
                       (combat.reload_action_id()==0 && (combat.reload_start_tick()!=0 || combat.reload_end_tick()!=0)) ||
                       (combat.reload_action_id()!=0 && (!combat.reload_start_tick() || combat.reload_start_tick()>message.tick() || combat.reload_end_tick()<=message.tick())) ||
                       (combat.last_shot_action_id()==0)!=(combat.last_shot_tick()==0) || combat.last_shot_tick()>message.tick() ||
                       std::none_of(snapshot.players.begin(),snapshot.players.end(),[&](const auto& p){return p.playerId==combat.player_id();}) ||
                       std::any_of(snapshot.combat.begin(),snapshot.combat.end(),[&](const auto& p){return p.playerId==combat.player_id();})) {
                        valid=false;break;
                    }
                    const auto& player=*std::find_if(snapshot.players.begin(),snapshot.players.end(),[&](const auto& p){return p.playerId==combat.player_id();});
                    if(combat.life_generation()!=player.lifeGeneration || (combat.hp()==0)!=(player.lifeState==LifeState::Dead) ||
                       (player.lifeState==LifeState::Dead && combat.reload_action_id()!=0)){valid=false;break;}
                    const CombatState record{combat.player_id(),combat.hp(),combat.next_allowed_shot_tick(),combat.life_generation(),
                        combat.magazine_ammo(),combat.reload_action_id(),combat.reload_start_tick(),combat.reload_end_tick(),
                        combat.last_shot_action_id(),combat.last_shot_tick(),combat.last_damage_tick(),combat.damage_count(),
                        combat.last_attacker_id()};
                    if(!ValidDamageRecord(record,player.lifeState==LifeState::Dead,player.lifeStateTick,message.tick(),maximumHp)){valid=false;break;}
                    snapshot.combat.push_back(record);
                }
                if(!valid)continue;
                std::scoped_lock lock(mutex);
                if(activeGeneration!=generation.load())return;
                if(!self && state.phase==ConnectionPhase::Playing &&
                    (!state.snapshot || snapshot.tick>state.snapshot->tick))
                    throw std::runtime_error("Local player left the Match");
                if(self && (!state.snapshot || snapshot.tick>state.snapshot->tick)) {
                    if(state.snapshot) {
                        for(const auto& player:snapshot.players)
                            for(const auto& previous:state.snapshot->players)
                                if(player.playerId==previous.playerId &&
                                    (player.lifeGeneration<previous.lifeGeneration || player.movementEpoch<previous.movementEpoch ||
                                     (player.lifeGeneration>previous.lifeGeneration && player.movementEpoch<=previous.movementEpoch) ||
                                     (player.movementEpoch==previous.movementEpoch && player.lastResolvedCommand<previous.lastResolvedCommand)))valid=false;
                    }
                    if(!valid)continue;
                    const auto& authority=*std::find_if(snapshot.players.begin(),snapshot.players.end(),
                        [&](const auto& player){return player.playerId==own;});
                    if(authority.movementEpoch!=submittedEpoch) {
                        latestInput.reset();submittedCommands.clear();submittedEpoch=authority.movementEpoch;
                        haveSentInput=false;nextInputSendAt={};
                    }
                    std::erase_if(submittedCommands,[&](const auto& command){return command.sequence<=authority.lastResolvedCommand;});
                    if(latestInput) {
                        std::erase_if(latestInput->commands,[&](const auto& command){return command.sequence<=authority.lastResolvedCommand;});
                        if(latestInput->commands.empty()) {
                            latestInput.reset();
                            // A fully acknowledged window has no retransmission
                            // deadline. The next available window must not wait
                            // behind a redundant send of the completed one.
                            haveSentInput=false;nextInputSendAt={};
                        }
                    }
                    if(receivedSnapshots.size()==MaxReceivedSnapshots) {
                        receivedSnapshots.pop_front();snapshotOverflow=true;++snapshotOverflowCount;
                    }
                    for(const auto& player:snapshot.players)
                        TraceMovement({.kind=MovementTraceKind::SnapshotReceived,
                            .timeNs=std::chrono::duration_cast<std::chrono::nanoseconds>(arrivedAt.time_since_epoch()).count(),.playerId=player.playerId,
                            .epoch=player.movementEpoch,.sequence=player.lastResolvedCommand,.authorityTick=snapshot.tick,
                            .queued=player.contiguousPendingCommands,.count=snapshotOverflowCount,.lifeGeneration=player.lifeGeneration});
                    state.snapshot=snapshot;state.phase=ConnectionPhase::Playing;
                    if(simulationConsumer) {
                        if(simulationSnapshots.size()==MaxReceivedSnapshots) {
                            simulationSnapshots.pop_front();++simulationOverflowCount;
                        }
                        simulationSnapshots.push_back({snapshot,arrivedAt});
                    }
                    receivedSnapshots.push_back({std::move(snapshot),arrivedAt});
                }
            } else if(packet->type==wire::Type::ActionResults) {
                if(!welcomed)continue;
                pb::ActionResults message;
                const bool parsed=message.ParseFromString(packet->payload);
                std::scoped_lock lock(mutex);
                if(activeGeneration!=generation.load())return;
                if(!parsed || !AcceptResults(message,arrivedAt)){++actionTransport.rejectedResultBatches;continue;}
            } else if(packet->type==wire::Type::Error) {
                pb::Error message;if(!message.ParseFromString(packet->payload))continue;
                throw std::runtime_error(message.message());
            } else continue;
            receivedSequence=packet->sequence;haveSequence=true;receivedAt=arrivedAt;
        }
        if(Clock::now()-(welcomed?receivedAt:connectedAt)>std::chrono::seconds(5))throw std::runtime_error("Gateway connection timed out");
        std::optional<PlayerInput> input;
        {std::scoped_lock lock(mutex);
            if(activeGeneration!=generation.load())return;
            if(welcomed && latestInput) {
                const auto now=Clock::now();
                const auto period=std::chrono::duration<double>(1.0/InputSendRate);
                if(inputTokensAt==Clock::time_point{})inputTokensAt=now;
                inputTokens=Engine::Math::Min(InputSendBurst,inputTokens+std::chrono::duration<double>(now-inputTokensAt)/period);
                inputTokensAt=now;
                // A window with a command never sent goes now; an unchanged one
                // waits for its deadline and leaves a token for the next command.
                const bool fresh=!haveSentInput || latestInput->movementEpoch!=sentInputEpoch ||
                    latestInput->lifeGeneration!=sentInputLife ||
                    (!latestInput->commands.empty() && latestInput->commands.back().sequence>sentInputThrough);
                if((fresh && inputTokens>=1) || (now>=nextInputSendAt && inputTokens>=InputSendBurst)) {
                    input=latestInput;inputTokens-=1;
                }
            }}
        if(input) {
            pb::PlayerInput message;message.set_movement_epoch(input->movementEpoch);message.set_life_generation(input->lifeGeneration);
            message.set_observed_authority_tick(input->observedAuthorityTick);
            for(const auto& command:input->commands) {
                auto* out=message.add_commands();out->set_sequence(command.sequence);
                out->set_move_forward(command.moveForward);out->set_move_right(command.moveRight);
                out->set_yaw(command.yaw);out->set_pitch(command.pitch);out->set_jump_requested(command.jumpRequested);
            }
            const auto payload=message.SerializeAsString();
            const auto sendStarted=MovementTraceNowNs();
            const bool sent=Send(wire::Type::Input,payload);
            const auto sendFinished=MovementTraceNowNs();
            if(sent)
                for(const auto& command:input->commands)
                    TraceMovement({.kind=MovementTraceKind::Sent,.timeNs=sendFinished,
                        .playerId=input->playerId,.epoch=input->movementEpoch,
                        .sequence=command.sequence,.startedNs=sendStarted,.lifeGeneration=input->lifeGeneration});
            const auto sentAt=Clock::now();
            const auto period=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0/InputSendRate));
            // A missed send is skipped, never repaid by a packet burst.
            nextInputSendAt=sentAt+period;
            haveSentInput=true;
            sentInputEpoch=input->movementEpoch;sentInputLife=input->lifeGeneration;
            sentInputThrough=input->commands.empty()?0:input->commands.back().sequence;
        }
        SendActions();
    }
    // One worker pass: a queued request (HTTP may block, as before), then the
    // network and lobby work, then the next wake-ups.
    void Service() {
        ++wakes;
        std::optional<Request> request;
        {std::scoped_lock lock(mutex);if(!requests.empty()){request=std::move(requests.front());requests.pop_front();}}
        const auto workGeneration=request?request->generation:activeGeneration;
        try {if(request)Handle(*request);Network();PollLobby();}
        catch(const std::exception& error){Failure(error.what(),workGeneration);}
        bool more=false;
        {std::scoped_lock lock(mutex);more=!requests.empty();}
        if(more)PostService();
        ReportStatistics();
        Arm();
    }
    // Waits for the socket to become readable and arms the timer at the
    // earliest deadline the pass above would act on. An early wake only runs a
    // pass that finds nothing due; it never sends sooner than before.
    void Arm() {
        const auto now=Clock::now();
        const bool live=socket.is_open() && activeGeneration==generation.load();
        if(live && !readWaiting) {
            readWaiting=true;
            socket.async_wait(udp::socket::wait_read,[this,current=transport](const asio::error_code& error) {
                if(current!=transport)return;
                readWaiting=false;
                if(error==asio::error::operation_aborted)return;
                Service();
            });
        }
        std::optional<Clock::time_point> next=statisticsAt+std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(NetworkStatisticsSeconds));
        const auto consider=[&](Clock::time_point at){if(!next || at<*next)next=at;};
        if(live) {
            consider(helloAt+(welcomed?std::chrono::milliseconds(1000):std::chrono::milliseconds(250)));
            consider((welcomed?receivedAt:connectedAt)+std::chrono::seconds(5)+std::chrono::milliseconds(1));
            std::scoped_lock lock(mutex);
            if(welcomed && latestInput) {
                const auto period=std::chrono::duration<double>(1.0/InputSendRate);
                // The same rule as Network: a window with a command never sent
                // waits for one token; an unchanged one for its deadline with a
                // full bucket.
                const bool fresh=!haveSentInput || latestInput->movementEpoch!=sentInputEpoch ||
                    latestInput->lifeGeneration!=sentInputLife ||
                    (!latestInput->commands.empty() && latestInput->commands.back().sequence>sentInputThrough);
                const auto refill=[&](double tokens){return inputTokensAt+std::chrono::duration_cast<Clock::duration>(
                    period*Engine::Math::Max(0.0,tokens-inputTokens));};
                consider(fresh?refill(1.0):std::max(nextInputSendAt,refill(InputSendBurst)));
            }
            if(welcomed && (std::any_of(actions.begin(),actions.end(),[](const auto& entry){return !entry.second.decision;}) ||
                            actionTransport.acknowledgedThrough>actionTransport.retiredThrough))
                consider(nextActionSendAt);
        } else if(!session && !base.empty()) {
            std::scoped_lock lock(mutex);
            if(state.phase==ConnectionPhase::Lobby)consider(lobbyPollAt+std::chrono::seconds(1));
        }
        if(!next)return;
        // asio's timer is subject to OS timer coalescing (on macOS even a short
        // wait wakes about half a millisecond late, which would slow the 60 Hz
        // resend). A far deadline is woken CoarseWakeLead early by the timer;
        // the last stretch waits on the role's Waiter (kqueue NOTE_CRITICAL),
        // blocking this thread at most CoarseWakeLead, like the old loop's last
        // short sleep. A stop request wakes that wait too.
        constexpr auto CoarseWakeLead=std::chrono::milliseconds(2);
        if(*next-now<=CoarseWakeLead) {
            if(deadlineAt && *deadlineAt<=*next)return; // an earlier wake is already pending
            deadlineAt=*next;
            asio::post(io,[this,at=*next]{
                if(role && !role->StopRequested())static_cast<void>(role->WaitUntil(at));
                deadlineAt.reset();
                Service();
            });
            return;
        }
        const auto at=*next-CoarseWakeLead;
        if(deadlineAt && *deadlineAt<=at && *deadlineAt>now)return; // an earlier wake is already armed
        deadlineAt=at;
        deadlineTimer.expires_at(at);
        deadlineTimer.async_wait([this](const asio::error_code& error) {
            if(error==asio::error::operation_aborted)return;
            deadlineAt.reset();
            Service();
        });
    }
    void ReportStatistics() {
        const auto now=Clock::now();
        if(std::chrono::duration<double>(now-statisticsAt).count()<NetworkStatisticsSeconds)return;
        const auto cpu=CurrentThreadCpuSeconds();
        PlayerId player{};{std::scoped_lock lock(mutex);player=state.playerId;}
        std::osyncstream(std::clog)<<WorkerStatisticsLine({.player=player,.windowSeconds=std::chrono::duration<double>(now-statisticsAt).count(),
            .wakes=wakes,.cpuSeconds=cpu && cpuAt ? std::optional<double>(*cpu-*cpuAt) : std::nullopt})<<'\n';
        statisticsAt=now;cpuAt=cpu;wakes=0;
    }
    void Run() {
        statisticsAt=Clock::now();
        cpuAt=CurrentThreadCpuSeconds();
        asio::post(io,[this]{Arm();});
        {
            // Between events nothing may be pending; the guard keeps run()
            // waiting. The destructor requests the stop, then stops the context.
            const auto work=asio::make_work_guard(io);
            // A handler's exception outside Service's own handling (an allocation
            // failure, say) ends the session visibly and the worker keeps running.
            for(;;) {
                try{io.run();break;}
                catch(const std::exception& error){
                    std::osyncstream(std::clog)<<"[ObjectFPS/PvP] network worker failed reason="<<error.what()<<'\n';
                    try{Failure(std::string("Network worker failed: ")+error.what(),activeGeneration);}catch(...){}
                }
            }
        }
        try{DisconnectRemote();}
        catch(const std::exception& error){std::osyncstream(std::clog)<<"[ObjectFPS/PvP] shutdown cleanup failed reason="<<error.what()<<'\n';}
    }
};
ClientConnection::ClientConnection():impl_(std::make_unique<Impl>()){}
ClientConnection::~ClientConnection()=default;
void ClientConnection::Refresh(std::string gateway){impl_->Push(Impl::Action::Refresh,std::move(gateway));}
void ClientConnection::CreateAndJoin(std::string gateway){impl_->Push(Impl::Action::Create,std::move(gateway));}
void ClientConnection::Join(std::string gateway,std::string room){impl_->Push(Impl::Action::Join,std::move(gateway),std::move(room));}
void ClientConnection::Leave(){impl_->Push(Impl::Action::Leave);}
void ClientConnection::SetArenaIdentity(std::string id,std::uint32_t version,std::uint64_t digest){
    SetArenaIdentities({{std::move(id),version,digest}});
}
void ClientConnection::SetArenaIdentities(std::vector<ArenaIdentity> installed){
    for(std::size_t i=0;i<installed.size();++i) {
        if(!installed[i].digest)throw std::invalid_argument("Arena content digest is zero");
        for(std::size_t j=0;j<i;++j)
            if(installed[j].id==installed[i].id)throw std::invalid_argument("Arena installed twice: "+installed[i].id);
    }
    std::scoped_lock lock(impl_->mutex);impl_->installedArenas=std::move(installed);
}
void ClientConnection::SendInput(PlayerInput input){
    SendInput(std::move(input),impl_->generation.load());
}
void ClientConnection::SendInput(PlayerInput input,std::uint64_t generation){
    // Generation changes happen under this mutex, so the check cannot race them.
    std::scoped_lock lock(impl_->mutex);
    if(generation!=impl_->generation.load())return;
    if((impl_->state.phase!=ConnectionPhase::Playing && impl_->state.phase!=ConnectionPhase::Connecting) ||
       input.playerId!=impl_->state.playerId || !PvpMatch::ValidInput(input))return;
    std::uint64_t resolved{},epoch{1},life{1};
    if(impl_->state.snapshot) {
        for(const auto& player:impl_->state.snapshot->players)
            if(player.playerId==input.playerId){resolved=player.lastResolvedCommand;epoch=player.movementEpoch;life=player.lifeGeneration;}
    }
    // Only authority can advance an epoch. Neither a stale application frame
    // nor a caller-provided future epoch can repopulate the worker's window.
    if(input.lifeGeneration!=life || input.movementEpoch!=epoch || input.movementEpoch!=impl_->submittedEpoch)return;
    std::erase_if(input.commands,[&](const auto& command){return command.sequence<=resolved;});
    std::erase_if(impl_->submittedCommands,[&](const auto& command){return command.sequence<=resolved;});
    if(input.commands.empty())return;
    // Replacement is safe only for a complete unacknowledged window. Retain
    // the sent window too, so a caller cannot mutate or omit an unacknowledged
    // command merely because the worker already consumed its mailbox.
    for(const auto& existing:impl_->submittedCommands) {
        const auto found=std::find_if(input.commands.begin(),input.commands.end(),
            [&](const auto& command){return command.sequence==existing.sequence;});
        if(found==input.commands.end() || *found!=existing)return;
    }
    impl_->submittedCommands=input.commands;
    impl_->latestInput=std::move(input);
    impl_->PostService();
}
std::optional<ActionId> ClientConnection::SubmitShot(std::uint64_t observedAuthorityTick,float yaw,float pitch){
    const auto state=State();
    if(!state.snapshot)return {};
    const auto player=std::find_if(state.snapshot->players.begin(),state.snapshot->players.end(),
        [&](const auto& p){return p.playerId==state.playerId;});
    if(player==state.snapshot->players.end())return {};
    return SubmitAction(ActionKind::Shot,player->lifeGeneration,observedAuthorityTick,yaw,pitch);
}
std::optional<ActionId> ClientConnection::SubmitAction(ActionKind kind,std::uint64_t lifeGeneration,
    std::uint64_t observedAuthorityTick,float yaw,float pitch){
    std::scoped_lock lock(impl_->mutex);
    if(impl_->state.phase!=ConnectionPhase::Playing || !impl_->state.combatRules || !lifeGeneration ||
       (kind!=ActionKind::Shot && kind!=ActionKind::Reload) ||
       !ValidMovementCommand({1,0,0,yaw,pitch}) || (kind==ActionKind::Reload && (yaw!=0 || pitch!=0)) ||
       impl_->actions.size()>=MaxActionWindow)return {};
    if(impl_->actionTransport.allocatedThrough==std::numeric_limits<ActionId>::max()) {
        impl_->state.error="Action IDs exhausted; leave and rejoin the Match";return {};
    }
    const auto id=impl_->actionTransport.allocatedThrough+1;
    if(id<=impl_->actionTransport.retiredThrough || id-impl_->actionTransport.retiredThrough>MaxActionWindow)return {};
    impl_->actions.emplace(id,Impl::ActionEntry{ShotRequest{id,observedAuthorityTick,yaw,pitch,kind,lifeGeneration},{},false});
    impl_->actionTransport.allocatedThrough=id;
    // D36: an eligible action leaves now, not at the worker's next pass.
    impl_->PostService();
    return id;
}
ClientConnectionState ClientConnection::State()const{std::scoped_lock lock(impl_->mutex);return impl_->CopyState();}
ClientConnectionDrain ClientConnection::Drain(){
    std::scoped_lock lock(impl_->mutex);
    ClientConnectionDrain result{impl_->CopyState(),{},impl_->generation.load(),impl_->snapshotOverflow,impl_->snapshotOverflowCount,{}};
    // Finish all potentially throwing allocations/copies before acknowledging
    // anything. Returning this owning batch is the game-consumption boundary.
    result.snapshots.reserve(impl_->receivedSnapshots.size());
    result.decisions.reserve(impl_->actions.size());
    result.decisionReceivedAt.reserve(impl_->actions.size());
    for(const auto& [id,entry]:impl_->actions)
        if(entry.decision && !entry.delivered){result.decisions.push_back(*entry.decision);result.decisionReceivedAt.push_back(entry.receivedAt);}
    for(auto& received:impl_->receivedSnapshots)result.snapshots.push_back(std::move(received));
    impl_->receivedSnapshots.clear();impl_->snapshotOverflow=false;
    for(const auto& decision:result.decisions)impl_->actions.at(decision.actionId).delivered=true;
    const auto acknowledged=impl_->actionTransport.acknowledgedThrough;
    while(impl_->actionTransport.acknowledgedThrough<impl_->actionTransport.allocatedThrough) {
        const auto next=impl_->actions.find(impl_->actionTransport.acknowledgedThrough+1);
        if(next==impl_->actions.end() || !next->second.delivered)break;
        ++impl_->actionTransport.acknowledgedThrough;
    }
    result.state.actionTransport=impl_->ActionState();
    if(impl_->actionTransport.acknowledgedThrough!=acknowledged)impl_->PostService();
    return result;
}
ClientSimulationDrain ClientConnection::DrainSimulation(){
    std::scoped_lock lock(impl_->mutex);
    impl_->simulationConsumer=true;
    ClientSimulationDrain result{{},impl_->generation.load(),impl_->state.playerId,impl_->state.arenaId,
        impl_->state.movementRules,impl_->simulationOverflowCount};
    result.snapshots.reserve(impl_->simulationSnapshots.size());
    for(auto& received:impl_->simulationSnapshots)result.snapshots.push_back(std::move(received));
    impl_->simulationSnapshots.clear();
    return result;
}
}
