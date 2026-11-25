#pragma once

// A small HTTP/1.1 client (the PvP Gateway port's declaration D8, games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md):
// a game talks to its backend services. A library (config/modules.csv library_only=yes): a product that wants it
// links GYO::Http; nothing is registered.
//
// Two parts:
//   - the message: EncodeRequest writes a request's bytes; ResponseParser reads an answer from bytes as they come
//     (Content-Length, chunked, or until the connection closes). Pure: no socket, no clock.
//   - HttpCall: one request on one connection (engine/core/os StreamSocket, "Connection: close"), driven by the
//     caller's Poll, one step a frame; it never blocks. When to give up is the caller's: dropping the call closes it.
//     A World keeps the call in a Resource and a System polls it.
//
// Not: DNS (an IPv4 literal address, as the datagram socket takes), TLS, redirects, cookies, keep-alive, HTTP/2.

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/os/StreamSocket.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace GYO::Engine::Http {

// Zero is not a valid code; the numeric values are not a data contract.
enum class HttpFailureCode : std::uint16_t {
    // The connection did not open: the OS refused it or the peer cannot be reached (the detail has the OS's error).
    ConnectFailed = 1,
    // The connection broke while the request went out or the answer came in.
    TransferFailed,
    // The answer is not HTTP/1.1 this client reads (the detail says where it stopped).
    MalformedResponse,
    // The connection closed before the answer was complete.
    Truncated,
    // The answer's head or body is larger than the limit.
    TooLarge,
};

[[nodiscard]] const char* ToString(HttpFailureCode code) noexcept;

using HttpFailure = Base::Error<HttpFailureCode>;
static_assert(Base::CodedError<HttpFailure>);

template <class T>
using HttpResult = Base::Result<T, HttpFailure>;

struct Header final {
    std::string name;
    std::string value;
};

struct HttpRequest final {
    std::string method = "GET";
    // The origin-form target: "/v1/rooms".
    std::string target = "/";
    // The Host header's value ("127.0.0.1:7700").
    std::string host;
    // More headers (not Host, Content-Length or Connection, which EncodeRequest writes).
    std::vector<Header> headers;
    std::string body;
};

struct HttpResponse final {
    int status = 0;
    // Names in lower case, in the order they came.
    std::vector<Header> headers;
    std::string body;

    // The first header of that name (any case), or nullopt.
    [[nodiscard]] std::optional<std::string_view> Find(std::string_view name) const;
};

// The request's bytes: the request line, Host, Connection: close, Content-Length when there is a body (or the method
// is POST, PUT or PATCH), the other headers, the body.
[[nodiscard]] std::string EncodeRequest(const HttpRequest& request);

// The largest head and body ResponseParser takes.
struct ResponseLimits final {
    std::size_t head = 16 * 1024;
    std::size_t body = 1024 * 1024;
};

// Reads one answer from bytes as they arrive. Feed the bytes, then Close when the connection ended; Result says
// whether the answer is complete.
class ResponseParser final {
public:
    explicit ResponseParser(ResponseLimits limits = {}) : limits_(limits) {}

    // Takes more bytes. Once complete, bytes after the answer are ignored.
    [[nodiscard]] HttpResult<void> Feed(std::span<const std::byte> bytes);
    // The connection closed: an answer read until the close is complete now; any other unfinished one is Truncated.
    [[nodiscard]] HttpResult<void> Close();
    // The answer, once complete.
    [[nodiscard]] const std::optional<HttpResponse>& Response() const noexcept { return response_; }

private:
    enum class Stage : std::uint8_t { Head, Sized, ChunkSize, ChunkData, ChunkEnd, Trailer, UntilClose, Done };
    [[nodiscard]] HttpResult<void> Step();
    [[nodiscard]] HttpResult<void> ReadHead(std::string_view head);

    ResponseLimits limits_;
    Stage stage_ = Stage::Head;
    std::string pending_;
    HttpResponse building_;
    std::size_t remaining_ = 0;
    std::optional<HttpResponse> response_;
};

// One request on its own connection.
class HttpCall final {
public:
    // Starts connecting to `to` with the request's bytes ready. ConnectFailed when the OS refuses to start.
    [[nodiscard]] static HttpResult<HttpCall> Start(const OS::DatagramAddress& to, const HttpRequest& request,
                                                    ResponseLimits limits = {});

    // One step: the connect, the bytes the OS takes, the bytes waiting. The answer once it is complete (the call is
    // then finished; later polls give it again), nullopt while it is not.
    [[nodiscard]] HttpResult<std::optional<HttpResponse>> Poll();

private:
    HttpCall(OS::StreamSocket socket, std::string request, ResponseLimits limits);

    OS::StreamSocket socket_;
    std::string request_;
    std::size_t sent_ = 0;
    ResponseParser parser_;
    std::optional<HttpFailure> failed_;
};

} // namespace GYO::Engine::Http
