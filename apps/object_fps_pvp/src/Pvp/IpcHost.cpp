#include "RetroFPS/Pvp/IpcHost.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "runtime_v1.pb.h"
#include <asio.hpp>
#include <array>
#include <charconv>
#include <chrono>
#include <deque>
#include <future>
#include <map>
#include <string_view>
#include <thread>

namespace fps::pvp {
namespace pb = object_fps_pvp::runtime::v1;
using asio::ip::tcp;
namespace {
bool WouldBlock(const asio::error_code& error) {
    return error==asio::error::would_block || error==asio::error::try_again;
}
pb::RuntimeEnvelope SnapshotMessage(const WorldSnapshot& snapshot) {
    pb::RuntimeEnvelope message; message.set_protocol_version(1);
    auto* out=message.mutable_snapshot(); out->set_tick(snapshot.tick);
    for(const auto& p:snapshot.players) {
        auto* state=out->add_players(); state->set_player_id(p.playerId);
        state->set_x(p.position.x); state->set_y(p.position.y); state->set_z(p.position.z);
        state->set_yaw(p.yaw); state->set_pitch(p.pitch); state->set_last_input_sequence(p.lastInputSequence);
    }
    return message;
}
}
struct IpcHost::Impl {
    MatchRuntimeHost& host;
    Arena arena;
    asio::io_context io;
    tcp::acceptor listener{io};
    std::jthread worker;
    std::uint64_t requestSequence{};
    Impl(MatchRuntimeHost& value,const Arena& content):host(value),arena(content){}

    void Connection(tcp::socket& socket,std::stop_token stop) {
        socket.non_blocking(true);
        socket.set_option(tcp::no_delay(true));
        std::vector<std::uint8_t> input;
        std::deque<std::vector<std::uint8_t>> controls;
        std::optional<std::vector<std::uint8_t>> latestSnapshot;
        std::vector<std::uint8_t> writing;
        std::size_t writeOffset{};
        std::map<std::uint64_t,PlayerId> joins;
        pb::RuntimeEnvelope ready; ready.set_protocol_version(1);
        auto* r=ready.mutable_ready(); r->set_arena_id(arena.id); r->set_arena_version(arena.version);
        r->set_tick_rate(60); r->set_snapshot_interval_ticks(3); r->set_max_players(2);
        controls.push_back(wire::Frame(ready.SerializeAsString()));
        std::array<std::uint8_t,8192> buffer{};
        while(!stop.stop_requested()) {
            asio::error_code error;
            const auto received=socket.read_some(asio::buffer(buffer),error);
            if(error && !WouldBlock(error)) return;
            if(received) input.insert(input.end(),buffer.begin(),buffer.begin()+received);
            if(input.size()>wire::MaxFrame+4+buffer.size()) return;
            while(input.size()>=4) {
                const auto length=static_cast<std::size_t>(wire::Read(std::span(input).first(4)));
                if(!length || length>wire::MaxFrame) return;
                if(input.size()<length+4) break;
                pb::RuntimeEnvelope message;
                if(!message.ParseFromArray(input.data()+4,static_cast<int>(length)) || message.protocol_version()!=1) return;
                input.erase(input.begin(),input.begin()+static_cast<std::ptrdiff_t>(length+4));
                if(message.has_join()) {
                    if(joins.size()>=64) return;
                    const auto request=++requestSequence;
                    if(!host.QueueJoin(request,message.join().player_id())) return;
                    joins.emplace(request,message.join().player_id());
                } else if(message.has_leave()) {
                    if(!host.QueueLeave(++requestSequence,message.leave().player_id())) return;
                } else if(message.has_input()) {
                    const auto& p=message.input();
                    static_cast<void>(host.SubmitInput({p.player_id(),p.input_sequence(),p.client_tick(),
                        p.move_forward(),p.move_right(),p.yaw(),p.pitch()}));
                } else return;
            }
            for(const auto& result:host.TakeControlResults()) {
                const auto found=joins.find(result.requestId);
                if(found==joins.end()) continue;
                pb::RuntimeEnvelope message; message.set_protocol_version(1);
                auto* joined=message.mutable_join_result(); joined->set_player_id(found->second);
                joined->set_accepted(result.accepted); joined->set_reason(result.error);
                controls.push_back(wire::Frame(message.SerializeAsString())); joins.erase(found);
                if(controls.size()>64) return;
            }
            if(auto snapshot=host.TakeSnapshot()) latestSnapshot=wire::Frame(SnapshotMessage(*snapshot).SerializeAsString());
            if(writing.empty()) {
                if(!controls.empty()) { writing=std::move(controls.front()); controls.pop_front(); }
                else if(latestSnapshot) { writing=std::move(*latestSnapshot); latestSnapshot.reset(); }
                writeOffset=0;
            }
            if(!writing.empty()) {
                error.clear();
                writeOffset+=socket.write_some(asio::buffer(writing.data()+writeOffset,writing.size()-writeOffset),error);
                if(error && !WouldBlock(error)) return;
                if(writeOffset==writing.size()) writing.clear();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    void Run(std::stop_token stop) {
        while(!stop.stop_requested()) {
            tcp::socket socket(io); asio::error_code error;
            listener.accept(socket,error);
            if(error) {
                if(!WouldBlock(error)) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue;
            }
            auto reset=host.RequestReset();
            while(reset.wait_for(std::chrono::milliseconds(10))!=std::future_status::ready)
                if(stop.stop_requested()) return;
            try { Connection(socket,stop); } catch(const std::exception&) { /* Close the failed IPC session. */ }
            socket.close(error);
            auto cleared=host.RequestReset();
            while(cleared.wait_for(std::chrono::milliseconds(10))!=std::future_status::ready)
                if(stop.stop_requested()) return;
        }
    }
};
IpcHost::IpcHost(MatchRuntimeHost& host,const Arena& arena):impl_(std::make_unique<Impl>(host,arena)){}
IpcHost::~IpcHost(){Stop();}
bool IpcHost::Start(const std::string& listenAddress,std::string& error) {
    try {
        const auto separator=listenAddress.rfind(':');
        if(separator==std::string::npos) throw std::invalid_argument("Expected loopback-ip:port");
        const auto address=asio::ip::make_address(listenAddress.substr(0,separator));
        if(!address.is_loopback()) throw std::invalid_argument("IPC must listen on loopback");
        const auto portText=std::string_view(listenAddress).substr(separator+1);
        unsigned int port{};
        const auto parsed=std::from_chars(portText.data(),portText.data()+portText.size(),port);
        if(parsed.ec!=std::errc{} || parsed.ptr!=portText.data()+portText.size() || port==0 || port>65535)
            throw std::invalid_argument("IPC port must be a decimal integer in [1, 65535]");
        const tcp::endpoint endpoint(address,static_cast<unsigned short>(port));
        impl_->listener.open(endpoint.protocol());
        impl_->listener.set_option(tcp::acceptor::reuse_address(true));
        impl_->listener.bind(endpoint); impl_->listener.listen(1); impl_->listener.non_blocking(true);
        impl_->worker=std::jthread([this](std::stop_token stop){impl_->Run(stop);});
        return true;
    } catch(const std::exception& failure) {error=failure.what();return false;}
}
void IpcHost::Stop(){if(impl_->worker.joinable()){impl_->worker.request_stop();impl_->worker.join();}}
}
