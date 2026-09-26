#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/MovementTrace.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "client_v3.pb.h"
#include <asio.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <mutex>
#include <random>
#include <syncstream>
#include <thread>

namespace fps::pvp {
namespace pb=object_fps_pvp::client::v3;
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
using asio::ip::udp;
namespace {
std::string Id(const Json& value) {return value.is_string()?value.get<std::string>():value.dump();}
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
    std::deque<ReceivedSnapshot> receivedSnapshots;
    bool snapshotOverflow{};
    std::uint64_t snapshotOverflowCount{};
    std::string expectedArena;
    std::uint32_t expectedArenaVersion{};
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
    bool roomRefreshFailed{}; // Guarded by mutex, like state.error.
    Clock::time_point helloAt{},connectedAt{},receivedAt{},lobbyPollAt{},nextInputSendAt{};
    std::jthread worker;
    Impl():worker([this](std::stop_token stop){Run(stop);}){}
    ~Impl(){worker.request_stop();worker.join();}

    void CloseTransport() {
        asio::error_code error;socket.close(error);
        // Keep credentials until HTTP confirms the idempotent leave. Losing
        // them on a failed request can leave an old pawn occupying a room slot.
        welcomed=false;haveSequence=false;sentSequence=0;activeGeneration=0;
        haveSentInput=false;nextInputSendAt={};
    }
    void Close() {
        CloseTransport();
        std::scoped_lock lock(mutex);latestInput.reset();submittedCommands.clear();submittedEpoch=1;
        receivedSnapshots.clear();snapshotOverflow=false;snapshotOverflowCount=0;
    }
    void Failure(std::string error,std::uint64_t failedGeneration) {
        CloseTransport();std::scoped_lock lock(mutex);
        if(failedGeneration!=generation.load())return;
        ++generation;
        // Publish the lifecycle transition and its generation-scoped history
        // reset together. Drain must not see an old generation's count reset.
        latestInput.reset();submittedCommands.clear();submittedEpoch=1;
        receivedSnapshots.clear();snapshotOverflow=false;snapshotOverflowCount=0;
        std::osyncstream(std::clog)<<"[ObjectFPS/PvP] connection failed player="<<state.playerId
            <<" reason="<<error<<'\n';
        state.phase=ConnectionPhase::Lobby;state.error=std::move(error);state.snapshot.reset();state.playerId=0;
        state.rooms.clear();roomRefreshFailed=false;
    }
    void Push(Action action,std::string gateway={},std::string roomId={}) {
        std::scoped_lock lock(mutex);
        // State publication and cancellation share this mutex. Checking an
        // atomic generation before locking cannot prevent a stale completion.
        const auto next=++generation;
        requests.clear();requests.push_back({action,std::move(gateway),std::move(roomId),next});
        state.error.clear();state.snapshot.reset();state.playerId=0;state.rooms.clear();roomRefreshFailed=false;
        state.phase=ConnectionPhase::Requesting;
        latestInput.reset();submittedCommands.clear();
        receivedSnapshots.clear();snapshotOverflow=false;snapshotOverflowCount=0;
    }
    void CheckArena(const std::string& id,std::uint32_t version) {
        std::scoped_lock lock(mutex);
        if(!expectedArena.empty() && (id!=expectedArena || version!=expectedArenaVersion))
            throw std::runtime_error("Arena content version mismatch");
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
        for(const auto& r:values)rooms.push_back({Id(r.at("id")),r.value("players",0u),r.value("capacity",2u)});
        return rooms;
    }
    void RefreshLobby(std::uint64_t requestGeneration) {
        try {
            auto rooms=base.empty()?std::vector<LobbyRoom>{}:FetchRooms();
            std::scoped_lock lock(mutex);
            if(requestGeneration!=generation.load())return;
            state.rooms=std::move(rooms);state.phase=ConnectionPhase::Lobby;
            state.snapshot.reset();state.playerId=0;
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
        const Json join={{"request_id",requestId},{"protocol_version",3}};
        const auto joined=Response(http->Post("/rooms/"+room+"/join",join.dump(),"application/json"));
        // Keep credentials even if cancelled so the next action releases its reservation.
        session=joined.at("session_id").get<std::uint64_t>();token=joined.at("session_token").get<std::string>();
        if(request.generation!=generation.load())return;
        if(joined.at("protocol_version").get<unsigned>()!=3)throw std::runtime_error("Client protocol mismatch");
        CheckArena(joined.at("arena_id").get<std::string>(),joined.at("arena_version").get<std::uint32_t>());
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
                CheckArena(message.arena_id(),message.arena_version());
                if(message.tick_rate()!=AuthorityTickRate || message.snapshot_rate()!=AuthorityTickRate/SnapshotIntervalTicks)
                    throw std::runtime_error("Unsupported Match cadence");
                std::scoped_lock lock(mutex);
                if(activeGeneration!=generation.load())return;
                if(message.player_id()!=state.playerId)throw std::runtime_error("Unexpected player identity");
                welcomed=true;
            } else if(packet->type==wire::Type::Snapshot) {
                if(!welcomed)continue;
                pb::WorldSnapshot message;if(!message.ParseFromString(packet->payload) ||
                    message.tick()==0 || message.players_size()>2)continue;
                WorldSnapshot snapshot;snapshot.tick=message.tick();bool self=false,valid=true;
                PlayerId own;{
                    std::scoped_lock lock(mutex);
                    if(activeGeneration!=generation.load())return;
                    own=state.playerId;
                }
                for(const auto& p:message.players()) {
                    if(p.player_id()==0 || std::any_of(snapshot.players.begin(),snapshot.players.end(),
                        [&](const PlayerState& existing){return existing.playerId==p.player_id();}) ||
                        !std::isfinite(p.x()) || !std::isfinite(p.y()) || !std::isfinite(p.z()) ||
                        !ValidMovementCommand({1,0,0,p.yaw(),p.pitch()}) || p.movement_epoch()==0 ||
                        p.contiguous_pending_commands()>MaxFutureCommands){valid=false;break;}
                    snapshot.players.push_back({p.player_id(),{p.x(),p.y(),p.z()},p.yaw(),p.pitch(),p.last_resolved_command(),
                        p.movement_epoch(),p.contiguous_pending_commands()});
                    self|=p.player_id()==own;
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
                                    (player.movementEpoch<previous.movementEpoch ||
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
                            .queued=player.contiguousPendingCommands,.count=snapshotOverflowCount});
                    state.snapshot=snapshot;state.phase=ConnectionPhase::Playing;
                    receivedSnapshots.push_back({std::move(snapshot),arrivedAt});
                }
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
            if(welcomed && (!haveSentInput || Clock::now()>=nextInputSendAt))input=latestInput;}
        if(input) {
            pb::PlayerInput message;message.set_movement_epoch(input->movementEpoch);
            for(const auto& command:input->commands) {
                auto* out=message.add_commands();out->set_sequence(command.sequence);
                out->set_move_forward(command.moveForward);out->set_move_right(command.moveRight);
                out->set_yaw(command.yaw);out->set_pitch(command.pitch);
            }
            const auto payload=message.SerializeAsString();
            const auto sendStarted=MovementTraceNowNs();
            const bool sent=Send(wire::Type::Input,payload);
            const auto sendFinished=MovementTraceNowNs();
            if(sent)
                for(const auto& command:input->commands)
                    TraceMovement({.kind=MovementTraceKind::Sent,.timeNs=sendFinished,
                        .playerId=input->playerId,.epoch=input->movementEpoch,
                        .sequence=command.sequence,.startedNs=sendStarted});
            const auto sentAt=Clock::now();
            const auto period=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0/InputSendRate));
            // A missed send is skipped, never repaid by a packet burst.
            nextInputSendAt=sentAt+period;
            haveSentInput=true;
        }
    }
    void Run(std::stop_token stop) {
        while(!stop.stop_requested()) {
            std::optional<Request> request;
            {std::scoped_lock lock(mutex);if(!requests.empty()){request=std::move(requests.front());requests.pop_front();}}
            const auto workGeneration=request?request->generation:activeGeneration;
            try {if(request)Handle(*request);Network();PollLobby();}
            catch(const std::exception& error){Failure(error.what(),workGeneration);}
            auto sleep=std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(2));
            const auto remaining=nextInputSendAt-Clock::now();
            if(haveSentInput && remaining>Clock::duration::zero())sleep=std::min(sleep,remaining);
            std::this_thread::sleep_for(sleep);
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
void ClientConnection::SetArenaIdentity(std::string id,std::uint32_t version){std::scoped_lock lock(impl_->mutex);impl_->expectedArena=std::move(id);impl_->expectedArenaVersion=version;}
void ClientConnection::SendInput(PlayerInput input){
    std::scoped_lock lock(impl_->mutex);
    if((impl_->state.phase!=ConnectionPhase::Playing && impl_->state.phase!=ConnectionPhase::Connecting) ||
       input.playerId!=impl_->state.playerId || !PvpMatch::ValidInput(input))return;
    std::uint64_t resolved{},epoch{1};
    if(impl_->state.snapshot) {
        for(const auto& player:impl_->state.snapshot->players)
            if(player.playerId==input.playerId){resolved=player.lastResolvedCommand;epoch=player.movementEpoch;}
    }
    // Only authority can advance an epoch. Neither a stale application frame
    // nor a caller-provided future epoch can repopulate the worker's window.
    if(input.movementEpoch!=epoch || input.movementEpoch!=impl_->submittedEpoch)return;
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
}
ClientConnectionState ClientConnection::State()const{std::scoped_lock lock(impl_->mutex);return impl_->state;}
ClientConnectionDrain ClientConnection::Drain(){
    std::scoped_lock lock(impl_->mutex);
    ClientConnectionDrain result{impl_->state,{},impl_->generation.load(),impl_->snapshotOverflow,impl_->snapshotOverflowCount};
    result.snapshots.reserve(impl_->receivedSnapshots.size());
    for(auto& received:impl_->receivedSnapshots)result.snapshots.push_back(std::move(received));
    impl_->receivedSnapshots.clear();impl_->snapshotOverflow=false;
    return result;
}
}
