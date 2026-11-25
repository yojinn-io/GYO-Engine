// The http Module (docs/architecture/target/v2/http.zh-Hant.md): the request's bytes, the answer read as its bytes
// come in any pieces, and one call against the test library gyo_test_loopback_listener.
#include <doctest/doctest.h>

#include "engine/http/Http.hpp"

#include "gyo_test/LoopbackListener.hpp"

#include <chrono>
#include <string>
#include <thread>

namespace H = GYO::Engine::Http;

namespace {

std::span<const std::byte> AsBytes(const std::string& text) { return std::as_bytes(std::span(text.data(), text.size())); }

// Feeds the text one byte at a time (the worst split) and closes when asked.
H::HttpResult<void> FeedByByte(H::ResponseParser& parser, const std::string& text) {
    for (const char c : text) {
        const std::string one(1, c);
        if (auto fed = parser.Feed(AsBytes(one)); !fed) return fed;
    }
    return {};
}

} // namespace

TEST_CASE("http: a request names its target, host, length and closing") {
    H::HttpRequest request;
    request.method = "POST";
    request.target = "/v1/rooms";
    request.host = "127.0.0.1:7700";
    request.headers.push_back({"Authorization", "Bearer x"});
    request.body = "{\"mode\":\"coop\"}";
    CHECK(H::EncodeRequest(request) ==
          "POST /v1/rooms HTTP/1.1\r\nHost: 127.0.0.1:7700\r\nConnection: close\r\nContent-Length: 15\r\n"
          "Authorization: Bearer x\r\n\r\n{\"mode\":\"coop\"}");
    H::HttpRequest get;
    get.target = "/v1/rooms";
    get.host = "h";
    CHECK_MESSAGE(H::EncodeRequest(get).find("Content-Length") == std::string::npos, "a GET without a body has no length");
    H::HttpRequest emptyPost;
    emptyPost.method = "POST";
    emptyPost.host = "h";
    CHECK_MESSAGE(H::EncodeRequest(emptyPost).find("Content-Length: 0\r\n") != std::string::npos, "an empty POST says 0");
}

TEST_CASE("http: an answer with a length, in any pieces") {
    H::ResponseParser parser;
    REQUIRE(FeedByByte(parser, "HTTP/1.1 409 Conflict\r\nContent-Type: application/json\r\nContent-Length: 21\r\n\r\n"
                               "{\"error\":\"room_full\"}trailing").has_value());
    REQUIRE(parser.Response().has_value());
    CHECK(parser.Response()->status == 409);
    CHECK(parser.Response()->body == "{\"error\":\"room_full\"}");
    CHECK(parser.Response()->Find("CONTENT-TYPE").value_or("") == "application/json");
}

TEST_CASE("http: a chunked answer") {
    H::ResponseParser parser;
    REQUIRE(FeedByByte(parser, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                               "a;ext=1\r\n{\"a\":12345\r\n3\r\n67}\r\n0\r\nX-Trailer: t\r\n\r\n").has_value());
    REQUIRE_MESSAGE(parser.Response().has_value(), "a chunked answer did not complete");
    CHECK(parser.Response()->body == "{\"a\":1234567}");
}

TEST_CASE("http: an answer without a length ends with the connection") {
    H::ResponseParser parser;
    REQUIRE(FeedByByte(parser, "HTTP/1.0 200 OK\r\n\r\nall of it").has_value());
    CHECK_FALSE(parser.Response().has_value());
    REQUIRE(parser.Close().has_value());
    REQUIRE(parser.Response().has_value());
    CHECK(parser.Response()->body == "all of it");
}

TEST_CASE("http: an interim answer is followed by the real one; 204 has no body") {
    H::ResponseParser parser;
    REQUIRE(FeedByByte(parser, "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 204 No Content\r\n\r\n").has_value());
    REQUIRE(parser.Response().has_value());
    CHECK(parser.Response()->status == 204);
}

TEST_CASE("http: what the parser refuses") {
    {
        H::ResponseParser parser;
        auto fed = parser.Feed(AsBytes(std::string("SMTP ready\r\n\r\n")));
        REQUIRE_FALSE(fed.has_value());
        CHECK(fed.error().code == H::HttpFailureCode::MalformedResponse);
    }
    {
        H::ResponseParser parser;
        REQUIRE(parser.Feed(AsBytes(std::string("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabc"))).has_value());
        auto closed = parser.Close();
        CHECK_MESSAGE((!closed && closed.error().code == H::HttpFailureCode::Truncated), "a body cut short is not Truncated");
    }
    {
        H::ResponseParser parser(H::ResponseLimits{64, 8});
        auto fed = parser.Feed(AsBytes(std::string("HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\n")));
        CHECK_MESSAGE((!fed && fed.error().code == H::HttpFailureCode::TooLarge), "a body past the limit is not TooLarge");
    }
    {
        H::ResponseParser parser(H::ResponseLimits{16, 8});
        auto fed = parser.Feed(AsBytes(std::string("HTTP/1.1 200 OK\r\nX-Long: 0123456789\r\n")));
        CHECK_MESSAGE((!fed && fed.error().code == H::HttpFailureCode::TooLarge), "a head past the limit is not TooLarge");
    }
    {
        H::ResponseParser parser;
        auto fed = parser.Feed(AsBytes(std::string("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n")));
        REQUIRE_FALSE(fed.has_value());
        CHECK(fed.error().code == H::HttpFailureCode::MalformedResponse);
    }
}

TEST_CASE("http: a call sends its request and reads the answer over loopback") {
    GYO::Test::LoopbackListener listener;
    REQUIRE(listener.Ok());
    std::optional<std::string> request;
    bool answered = false;
    std::thread server([&] {
        // No REQUIRE on this thread: it reports what happened and ends.
        if (!listener.Accept()) return;
        request = listener.ReceiveRequest();
        answered = listener.Send("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
        listener.CloseAccepted();
    });
    H::HttpRequest sent;
    sent.target = "/v1/rooms";
    sent.host = "127.0.0.1";
    std::optional<H::HttpResponse> answer;
    std::string failure;
    if (auto call = H::HttpCall::Start(listener.Address(), sent)) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!answer && failure.empty() && std::chrono::steady_clock::now() < until) {
            auto polled = call->Poll();
            if (!polled) failure = polled.error().message;
            else if (*polled) answer = *polled;
            else std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (answer) {
            auto again = call->Poll();
            CHECK_MESSAGE((again && again->has_value()), "a finished call does not give its answer again");
        }
    } else {
        failure = call.error().message;
    }
    server.join();
    REQUIRE_MESSAGE(failure.empty(), "the call failed: " << failure);
    REQUIRE_MESSAGE(answer.has_value(), "no answer within five seconds");
    CHECK(answered);
    CHECK(answer->status == 200);
    CHECK(answer->body == "ok");
    CHECK(request.value_or("").rfind("GET /v1/rooms HTTP/1.1\r\n", 0) == 0);
}

TEST_CASE("http: a call to a port nobody listens on is Unreachable, with the OS's error in the detail") {
    GYO::Engine::OS::DatagramAddress closed{};
    {
        GYO::Test::LoopbackListener listener;
        REQUIRE(listener.Ok());
        closed = listener.Address();
        listener.CloseListening();
    }
    auto call = H::HttpCall::Start(closed, H::HttpRequest{});
    REQUIRE(call.has_value());
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        auto polled = call->Poll();
        if (!polled) {
            CHECK(polled.error().code == H::HttpFailureCode::Unreachable);
            CHECK_MESSAGE(polled.error().detail.rfind("Unreachable", 0) == 0, "the detail lost the OS's code: " << polled.error().detail);
            break;
        }
        REQUIRE_MESSAGE(std::chrono::steady_clock::now() < until, "the failed connect was never reported");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

TEST_CASE("http: after a failure the parser keeps it") {
    H::ResponseParser parser;
    REQUIRE_FALSE(parser.Feed(AsBytes(std::string("SMTP ready\r\n\r\n"))).has_value());
    auto again = parser.Feed(AsBytes(std::string("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n")));
    CHECK_MESSAGE((!again && again.error().code == H::HttpFailureCode::MalformedResponse), "a failed parser took more bytes");
    CHECK_FALSE(parser.Response().has_value());
}
