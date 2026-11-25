// The native side of LoopbackListener.hpp (OS headers are allowed in tests/common/os).
#include "gyo_test/LoopbackListener.hpp"

#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace GYO::Test {
namespace {

#if defined(_WIN32)
using NativeSocket = SOCKET;
constexpr NativeSocket kNoSocket = INVALID_SOCKET;
void CloseNative(const NativeSocket s) { closesocket(s); }
int Nfds(NativeSocket) { return 0; }
#else
using NativeSocket = int;
constexpr NativeSocket kNoSocket = -1;
void CloseNative(const NativeSocket s) { close(s); }
int Nfds(const NativeSocket s) { return s + 1; }
#endif

// Whether s becomes readable (a connection to accept, bytes, or the end) within the time.
bool Readable(const NativeSocket s, const std::chrono::milliseconds within) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(s, &readable);
    timeval wait{};
    wait.tv_sec = static_cast<decltype(wait.tv_sec)>(within.count() / 1000);
    wait.tv_usec = static_cast<decltype(wait.tv_usec)>((within.count() % 1000) * 1000);
    return select(Nfds(s), &readable, nullptr, nullptr, &wait) > 0;
}

} // namespace

struct LoopbackListener::Impl final {
    bool started = false;
    NativeSocket listening = kNoSocket;
    NativeSocket accepted = kNoSocket;
    std::uint16_t port = 0;
    std::string pending;
};

LoopbackListener::LoopbackListener() : impl_(std::make_unique<Impl>()) {
#if defined(_WIN32)
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return;
    impl_->started = true;
#endif
    impl_->listening = socket(AF_INET, SOCK_STREAM, 0);
    if (impl_->listening == kNoSocket) return;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t length = sizeof address;
    if (bind(impl_->listening, reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0 ||
        listen(impl_->listening, 4) != 0 ||
        getsockname(impl_->listening, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        CloseListening();
        return;
    }
    impl_->port = ntohs(address.sin_port);
}

LoopbackListener::~LoopbackListener() {
    CloseAccepted();
    CloseListening();
#if defined(_WIN32)
    if (impl_->started) WSACleanup();
#endif
}

bool LoopbackListener::Ok() const noexcept { return impl_->listening != kNoSocket; }

Engine::OS::DatagramAddress LoopbackListener::Address() const { return Engine::OS::DatagramAddress::Loopback(impl_->port); }

bool LoopbackListener::Accept(const std::chrono::milliseconds within) {
    CloseAccepted();
    if (!Ok() || !Readable(impl_->listening, within)) return false;
    impl_->accepted = accept(impl_->listening, nullptr, nullptr);
    return impl_->accepted != kNoSocket;
}

bool LoopbackListener::Send(const std::string_view text) {
    std::size_t sent = 0;
    while (sent < text.size()) {
        const auto wrote = send(impl_->accepted, text.data() + sent, static_cast<int>(text.size() - sent), 0);
        if (wrote <= 0) return false;
        sent += static_cast<std::size_t>(wrote);
    }
    return true;
}

std::optional<std::string> LoopbackListener::Receive(const std::size_t count) {
    char buffer[4096];
    while (impl_->pending.size() < count) {
        if (!Readable(impl_->accepted, std::chrono::seconds(10))) return std::nullopt;
        const auto got = recv(impl_->accepted, buffer, static_cast<int>(sizeof buffer), 0);
        if (got <= 0) return std::nullopt;
        impl_->pending.append(buffer, static_cast<std::size_t>(got));
    }
    std::string taken = impl_->pending.substr(0, count);
    impl_->pending.erase(0, count);
    return taken;
}

std::optional<std::string> LoopbackListener::ReceiveRequest() {
    char buffer[4096];
    std::size_t headEnd = std::string::npos;
    while ((headEnd = impl_->pending.find("\r\n\r\n")) == std::string::npos) {
        if (!Readable(impl_->accepted, std::chrono::seconds(10))) return std::nullopt;
        const auto got = recv(impl_->accepted, buffer, static_cast<int>(sizeof buffer), 0);
        if (got <= 0) return std::nullopt;
        impl_->pending.append(buffer, static_cast<std::size_t>(got));
    }
    headEnd += 4;
    std::size_t length = 0;
    for (const char* name : {"Content-Length: ", "content-length: "}) {
        const std::size_t at = impl_->pending.find(name);
        if (at != std::string::npos && at < headEnd) length = std::stoul(impl_->pending.substr(at + std::string(name).size()));
    }
    return Receive(headEnd + length);
}

void LoopbackListener::CloseAccepted() {
    if (impl_->accepted != kNoSocket) CloseNative(impl_->accepted);
    impl_->accepted = kNoSocket;
    impl_->pending.clear();
}

void LoopbackListener::CloseListening() {
    if (impl_->listening != kNoSocket) CloseNative(impl_->listening);
    impl_->listening = kNoSocket;
}

} // namespace GYO::Test
