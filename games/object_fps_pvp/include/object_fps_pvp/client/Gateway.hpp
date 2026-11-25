#pragma once

// The Client's side of the Gateway's external protocol v1 (docs/gateway-client-handoff.zh-Hant.md section 2; game-flow
// declaration 3.4): HTTP/1.1 and JSON, every call but the session's with the session's Bearer token, every change with
// an idempotency key of its own (the same key when a call is tried again).
//
//   - the messages: the requests (GatewayRequest) and what their answers say (ParseSession, ParseRooms, ParseRoom,
//     ParseSave, ErrorCodeOf). Pure: no socket, no clock.
//   - GatewayClient: one call at a time over GYO::Http, driven by the caller's Update once a frame (it never blocks);
//     it opens the session first, and opens it again once when an answer says it is gone (401). The Frame World keeps
//     it in a Resource carried from scene to scene (the session, and a call still going, go along).
//
// What an error code means for the player is the scene's (the codes only say what happened).

#include "engine/http/Http.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/os/SocketAddress.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace object_fps_pvp::client {

// The session the Gateway gave (POST /v1/session).
struct GatewaySession final {
    std::uint64_t userId = 0;
    std::string token;
    std::int64_t expiresAt = 0;  // Unix seconds
};

struct RoomMember final {
    std::uint64_t userId = 0;
    bool ready = false;
};

// Where a member of a started room connects (its own; 2 minutes).
struct RoomConnect final {
    std::string server;  // "A.B.C.D:P"
    std::uint64_t userId = 0;
    std::string token;   // hex
};

// A room as GET /v1/rooms lists it, and as GET /v1/rooms/{id} gives it (owner, members, connect: only there).
struct Room final {
    std::uint64_t id = 0;
    std::string mode;
    std::string map;
    std::uint32_t players = 0;
    std::uint32_t capacity = 0;
    std::string state;  // "open", "starting", "started"
    std::uint64_t owner = 0;
    std::vector<RoomMember> members;
    std::optional<RoomConnect> connect;
};

// A player's save as GET /v1/saves/me gives it: the body is the game's (match/PlayerSave.hpp), null when never written.
struct SaveAnswer final {
    std::uint64_t version = 0;
    std::string body;  // the body's JSON text ("null": never written)
};

// One call: what to send. key: the idempotency key (empty: a read, sent without one).
struct GatewayRequest final {
    std::string method = "GET";
    std::string target;
    std::string body;
    std::string key;
};

[[nodiscard]] GatewayRequest SessionRequest(const std::string& deviceId);
[[nodiscard]] GatewayRequest ListRoomsRequest();
[[nodiscard]] GatewayRequest CreateRoomRequest(const std::string& mode, const std::string& map, std::string key);
[[nodiscard]] GatewayRequest RoomRequest(std::uint64_t room);
[[nodiscard]] GatewayRequest JoinRequest(std::uint64_t room, std::string key);
[[nodiscard]] GatewayRequest ReadyRequest(std::uint64_t room, bool ready, std::string key);
[[nodiscard]] GatewayRequest StartRequest(std::uint64_t room, std::string key);
[[nodiscard]] GatewayRequest LeaveRequest(std::uint64_t room, std::string key);
[[nodiscard]] GatewayRequest SaveRequest();

// The HTTP request to the Gateway at host ("A.B.C.D:P"), with the session's token when one is given.
[[nodiscard]] GYO::Engine::Http::HttpRequest ToHttp(const GatewayRequest& request, const std::string& host,
                                                    const std::optional<GatewaySession>& session);

// What the answers say; nullopt with error naming what was wrong.
[[nodiscard]] std::optional<GatewaySession> ParseSession(const std::string& body, std::string& error);
[[nodiscard]] std::optional<std::vector<Room>> ParseRooms(const std::string& body, std::string& error);
[[nodiscard]] std::optional<Room> ParseRoom(const std::string& body, std::string& error);
[[nodiscard]] std::optional<SaveAnswer> ParseSave(const std::string& body, std::string& error);
// The error answer's code ({"error": "<code>"}); empty when the body has none.
[[nodiscard]] std::string ErrorCodeOf(const std::string& body);

// A new idempotency key (or device id): bytes random bytes as lower-case hex.
[[nodiscard]] std::string RandomHex(std::size_t bytes);

// The device id kept in the file at path (made, high entropy, when it is not there: the device id is the player's
// credential, gateway declaration L7). Empty with error when it could not be read or written.
[[nodiscard]] std::string LoadDeviceId(const std::string& path, std::string& error);

// One finished call.
struct GatewayResult final {
    std::string tag;          // the caller's name for the call
    int status = 0;           // the HTTP status (0: no answer)
    std::string body;
    // The Gateway's error code (an answer of 400 or more), or this side's: "unreachable", "timeout", "http" (an answer
    // that is not HTTP or not the protocol's), "no_device" (no device id: nothing is sent).
    std::string errorCode;
    std::string detail;       // when there was no answer: why
    [[nodiscard]] bool Ok() const noexcept { return status >= 200 && status < 300; }
};

// One HTTP exchange of a GatewayClient as it went, for the program's log (--verbose): what was sent, how it ended, how
// long its call waited behind the calls asked before it (from Ask to its turn; the exchanges of one call -- its
// session's, a retry -- share it) and how long the exchange took (nanoseconds).
struct GatewayCallRecord final {
    std::string tag;
    bool session = false;  // the session's own call, made for the call named tag
    std::string method;
    std::string target;
    int status = 0;  // 0: no answer (errorCode says why)
    std::string errorCode;
    std::int64_t waited = 0;
    std::int64_t took = 0;
};

class GatewayClient final {
public:
    // gateway: its address; deviceId: this player's (empty: none, unavailable says why; every call then ends at once
    // with "no_device"); timeout: how long a call may go without its answer.
    GatewayClient(GYO::Engine::OS::SocketAddress gateway, std::string deviceId, std::string unavailable = {},
                  std::int64_t timeout = kDefaultTimeout);

    // Asks for a call; calls go one at a time in the order asked, after the session.
    void Ask(std::string tag, GatewayRequest request);
    // Without a device id: every call ends at once with "no_device".
    [[nodiscard]] bool Offline() const noexcept { return deviceId_.empty(); }
    // Whether a call is waiting or going.
    [[nodiscard]] bool Busy() const noexcept { return current_.has_value() || !waiting_.empty(); }
    // One step at now (nanoseconds on any steady clock): starts the next call, polls the current one. A call with no
    // answer within the timeout ends with errorCode "timeout". The session's own call is not returned unless it failed
    // (then it is returned under the asked call's tag, and that call is dropped).
    [[nodiscard]] std::optional<GatewayResult> Update(std::int64_t now);
    [[nodiscard]] const std::optional<GatewaySession>& Session() const noexcept { return session_; }
    // The exchanges ended since the last take (the newest kKeptCalls at most).
    [[nodiscard]] std::vector<GatewayCallRecord> TakeCalls() { return std::exchange(calls_, {}); }
    static constexpr std::size_t kKeptCalls = 64;

    static constexpr std::int64_t kDefaultTimeout = 5'000'000'000;

private:
    struct Call {
        std::string tag;
        GatewayRequest request;
        bool retried = false;  // opened the session again once already
        std::optional<std::int64_t> asked;  // the first Update that saw it waiting
        std::int64_t waited = 0;            // from asked to its turn
    };
    struct Going {
        Call call;
        bool session = false;  // the session's own call, made for call
        std::optional<GYO::Engine::Http::HttpCall> http;
        std::int64_t started = 0;
        std::string failure;  // why the connection did not start
        std::string method;   // what was sent (the session's own request, or the call's)
        std::string target;
    };

    void Start(Call call, bool session, std::int64_t now);
    void Note(const Going& going, int status, std::string errorCode, std::int64_t now);
    void Keep(GatewayCallRecord record);

    GYO::Engine::OS::SocketAddress gateway_;
    std::string host_;
    std::string deviceId_;
    std::string unavailable_;
    std::int64_t timeout_;
    std::optional<GatewaySession> session_;
    std::deque<Call> waiting_;
    std::optional<Going> current_;
    std::vector<GatewayCallRecord> calls_;
};
inline void HashValue(GYO::Kernel::Hasher&, const GatewayClient&) {}

} // namespace object_fps_pvp::client
