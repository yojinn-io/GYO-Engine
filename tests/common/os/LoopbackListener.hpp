#pragma once

// A blocking loopback TCP listener with one accepted connection, for tests of engine/core/os StreamSocket and what is
// built on it (the http Module). engine/core/os has no listener (nothing but tests would use one, os.md Constraints
// 9); the OS knowledge stays here in tests/common/os, where OS headers are allowed.

#include <doctest/doctest.h>

#include "engine/os/DatagramSocket.hpp"

#include <cstddef>
#include <cstdint>
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
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace GYO::Test {

#if defined(_WIN32)
using NativeSocket = SOCKET;
inline constexpr NativeSocket kNoSocket = INVALID_SOCKET;
inline void CloseNative(const NativeSocket s) { closesocket(s); }
#else
using NativeSocket = int;
inline constexpr NativeSocket kNoSocket = -1;
inline void CloseNative(const NativeSocket s) { close(s); }
#endif

using GYO::Engine::OS::DatagramAddress;

// A blocking loopback listener with one accepted connection.
class LoopbackListener final {
public:
    LoopbackListener() {
#if defined(_WIN32)
        WSADATA data{};
        REQUIRE(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
        listening_ = socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(listening_ != kNoSocket);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        REQUIRE(bind(listening_, reinterpret_cast<const sockaddr*>(&address), sizeof address) == 0);
        REQUIRE(listen(listening_, 4) == 0);
        socklen_t length = sizeof address;
        REQUIRE(getsockname(listening_, reinterpret_cast<sockaddr*>(&address), &length) == 0);
        port_ = ntohs(address.sin_port);
    }
    ~LoopbackListener() {
        if (accepted_ != kNoSocket) CloseNative(accepted_);
        if (listening_ != kNoSocket) CloseNative(listening_);
#if defined(_WIN32)
        WSACleanup();
#endif
    }
    [[nodiscard]] DatagramAddress Address() const { return DatagramAddress::Loopback(port_); }
    void Accept() {
        accepted_ = accept(listening_, nullptr, nullptr);
        REQUIRE(accepted_ != kNoSocket);
    }
    void Send(const std::string& text) {
        REQUIRE(send(accepted_, text.data(), static_cast<int>(text.size()), 0) == static_cast<int>(text.size()));
    }
    [[nodiscard]] std::string Receive(const std::size_t count) {
        std::string text;
        char buffer[256];
        while (text.size() < count) {
            const auto got = recv(accepted_, buffer, static_cast<int>(sizeof buffer), 0);
            REQUIRE(got > 0);
            text.append(buffer, static_cast<std::size_t>(got));
        }
        return text;
    }
    // Reads until the end of an HTTP head (and nothing past what arrived with it).
    [[nodiscard]] std::string ReceiveHead() {
        std::string text;
        char buffer[256];
        while (text.find("\r\n\r\n") == std::string::npos) {
            const auto got = recv(accepted_, buffer, static_cast<int>(sizeof buffer), 0);
            REQUIRE(got > 0);
            text.append(buffer, static_cast<std::size_t>(got));
        }
        return text;
    }
    // Reads one HTTP request: its head and the Content-Length bytes of its body.
    [[nodiscard]] std::string ReceiveRequest() {
        std::string text = ReceiveHead();
        const std::size_t headEnd = text.find("\r\n\r\n") + 4;
        std::size_t length = 0;
        for (const char* name : {"Content-Length: ", "content-length: "}) {
            const std::size_t at = text.find(name);
            if (at != std::string::npos && at < headEnd) length = std::stoul(text.substr(at + std::string(name).size()));
        }
        if (text.size() < headEnd + length) text += Receive(headEnd + length - text.size());
        return text;
    }
    void CloseAccepted() {
        CloseNative(accepted_);
        accepted_ = kNoSocket;
    }
    void CloseListening() {
        CloseNative(listening_);
        listening_ = kNoSocket;
    }

private:
    NativeSocket listening_ = kNoSocket;
    NativeSocket accepted_ = kNoSocket;
    std::uint16_t port_ = 0;
};

} // namespace GYO::Test
