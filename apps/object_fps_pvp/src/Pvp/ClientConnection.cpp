#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "client_v1.pb.h"
#include <asio.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <mutex>
#include <random>
#include <thread>

namespace fps::pvp {
namespace pb=object_fps_pvp::client::v1;
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
    std::uint64_t inputHighWater{};
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
    Clock::time_point helloAt{},connectedAt{},receivedAt{};
    std::jthread worker;
    Impl():worker([this](std::stop_token stop){Run(stop);}){}
    ~Impl(){worker.request_stop();worker.join();}

    void Close() {
        asio::error_code error;socket.close(error);
        session=0;token.clear();welcomed=false;haveSequence=false;sentSequence=0;activeGeneration=0;
        std::scoped_lock lock(mutex);latestInput.reset();inputHighWater=0;
    }
    void Failure(std::string error,std::uint64_t failedGeneration) {
        Close();std::scoped_lock lock(mutex);
        if(failedGeneration!=generation.load())return;
        state.phase=ConnectionPhase::Lobby;state.error=std::move(error);state.snapshot.reset();state.playerId=0;
    }
    void Push(Action action,std::string gateway={},std::string roomId={}) {
        std::scoped_lock lock(mutex);
        // State publication and cancellation share this mutex. Checking an
        // atomic generation before locking cannot prevent a stale completion.
        const auto next=++generation;
        requests.clear();requests.push_back({action,std::move(gateway),std::move(roomId),next});
        state.error.clear();state.snapshot.reset();state.playerId=0;
        state.phase=action==Action::Leave?ConnectionPhase::Lobby:ConnectionPhase::Requesting;
        latestInput.reset();inputHighWater=0;
    }
    void CheckArena(const std::string& id,std::uint32_t version) {
        std::scoped_lock lock(mutex);
        if(!expectedArena.empty() && (id!=expectedArena || version!=expectedArenaVersion))
            throw std::runtime_error("Arena content version mismatch");
    }
    void DisconnectRemote() {
        if(session && !base.empty() && !room.empty()) {
            auto http=Http(base);
            const Json body={{"session_id",session},{"session_token",token}};
            static_cast<void>(http->Post("/rooms/"+room+"/leave",body.dump(),"application/json"));
        }
        Close();
    }
    void Handle(const Request& request) {
        DisconnectRemote();
        if(request.generation!=generation.load())return;
        if(request.action==Action::Leave) {
            std::scoped_lock lock(mutex);
            if(request.generation!=generation.load())return;
            state.phase=ConnectionPhase::Lobby;state.snapshot.reset();state.playerId=0;
            return;
        }
        base=BaseUrl(request.gateway);
        auto http=Http(base);
        if(request.action==Action::Refresh) {
            const auto body=Response(http->Get("/rooms"));
            std::vector<LobbyRoom> rooms;
            const auto& values=body.is_array()?body:body.at("rooms");
            for(const auto& r:values)rooms.push_back({Id(r.at("id")),r.value("players",0u),r.value("capacity",2u)});
            std::scoped_lock lock(mutex);
            if(request.generation!=generation.load())return;
            state.rooms=std::move(rooms);state.phase=ConnectionPhase::Lobby;return;
        }
        room=request.room;
        if(request.action==Action::Create) {
            const auto created=Response(http->Post("/rooms","{}","application/json"));
            room=Id(created.at("id"));
        }
        if(request.generation!=generation.load())return;
        static std::atomic<std::uint64_t> nextRequest{};
        const auto requestId=std::to_string(Clock::now().time_since_epoch().count())+"-"+std::to_string(++nextRequest);
        const Json join={{"request_id",requestId},{"protocol_version",1}};
        const auto joined=Response(http->Post("/rooms/"+room+"/join",join.dump(),"application/json"));
        // Keep credentials even if cancelled so the next action releases its reservation.
        session=joined.at("session_id").get<std::uint64_t>();token=joined.at("session_token").get<std::string>();
        if(request.generation!=generation.load())return;
        if(joined.at("protocol_version").get<unsigned>()!=1)throw std::runtime_error("Client protocol mismatch");
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
    void Send(wire::Type type,const std::string& payload) {
        const auto bytes=wire::Encode({type,session,++sentSequence,payload});
        asio::error_code error;socket.send_to(asio::buffer(bytes),endpoint,0,error);
        if(error && error!=asio::error::would_block && error!=asio::error::try_again)throw std::runtime_error(error.message());
    }
    void Network() {
        if(!socket.is_open() || activeGeneration!=generation.load())return;
        const auto now=Clock::now();
        if(!welcomed && now-helloAt>=std::chrono::milliseconds(250)) {
            pb::Hello hello;hello.set_session_token(token);Send(wire::Type::Hello,hello.SerializeAsString());helloAt=now;
        }
        std::array<std::uint8_t,2048> bytes{};
        for(int count=0;count<64;++count) {
            udp::endpoint sender;asio::error_code error;
            const auto size=socket.receive_from(asio::buffer(bytes),sender,0,error);
            if(error==asio::error::would_block || error==asio::error::try_again)break;
            if(error)throw std::runtime_error(error.message());
            if(sender!=endpoint)continue;
            const auto packet=wire::Decode(std::span(bytes).first(size));
            if(!packet || packet->session!=session || (haveSequence && !wire::Newer(packet->sequence,receivedSequence)))continue;
            if(packet->type==wire::Type::Welcome) {
                pb::Welcome message;if(!message.ParseFromString(packet->payload))continue;
                CheckArena(message.arena_id(),message.arena_version());
                if(message.tick_rate()!=60 || message.snapshot_rate()!=20)throw std::runtime_error("Unsupported Match cadence");
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
                        !std::isfinite(p.yaw()) || !std::isfinite(p.pitch())){valid=false;break;}
                    snapshot.players.push_back({p.player_id(),{p.x(),p.y(),p.z()},p.yaw(),p.pitch(),p.last_input_sequence()});
                    self|=p.player_id()==own;
                }
                if(!valid)continue;
                std::scoped_lock lock(mutex);
                if(activeGeneration!=generation.load())return;
                if(self && (!state.snapshot || snapshot.tick>state.snapshot->tick)) {state.snapshot=std::move(snapshot);state.phase=ConnectionPhase::Playing;}
            } else if(packet->type==wire::Type::Error) {
                pb::Error message;if(!message.ParseFromString(packet->payload))continue;
                throw std::runtime_error(message.message());
            } else continue;
            receivedSequence=packet->sequence;haveSequence=true;receivedAt=now;
        }
        if(now-(welcomed?receivedAt:connectedAt)>std::chrono::seconds(5))throw std::runtime_error("Gateway connection timed out");
        std::optional<PlayerInput> input;
        {std::scoped_lock lock(mutex);
            if(activeGeneration!=generation.load())return;
            if(welcomed){input=latestInput;latestInput.reset();}}
        if(input) {
            pb::PlayerInput message;message.set_input_sequence(input->sequence);message.set_client_tick(input->clientTick);
            message.set_move_forward(input->moveForward);message.set_move_right(input->moveRight);
            message.set_yaw(input->yaw);message.set_pitch(input->pitch);Send(wire::Type::Input,message.SerializeAsString());
        }
    }
    void Run(std::stop_token stop) {
        while(!stop.stop_requested()) {
            std::optional<Request> request;
            {std::scoped_lock lock(mutex);if(!requests.empty()){request=std::move(requests.front());requests.pop_front();}}
            const auto workGeneration=request?request->generation:activeGeneration;
            try {if(request)Handle(*request);Network();}
            catch(const std::exception& error){Failure(error.what(),workGeneration);}
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        try{DisconnectRemote();}catch(...){}
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
       input.playerId!=impl_->state.playerId || input.sequence==0 || input.sequence<=impl_->inputHighWater)return;
    impl_->inputHighWater=input.sequence;
    impl_->latestInput=input;
}
ClientConnectionState ClientConnection::State()const{std::scoped_lock lock(impl_->mutex);return impl_->state;}
}
