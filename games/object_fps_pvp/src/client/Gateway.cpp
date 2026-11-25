#include "object_fps_pvp/client/Gateway.hpp"

#include "object_fps_pvp/client/LocalFiles.hpp"

#include "data/JsonFields.hpp"

#include "engine/json/Writer.hpp"

#include <random>
#include <stdexcept>
#include <utility>

namespace object_fps_pvp::client {

namespace Base = GYO::Engine::Base;
namespace H = GYO::Engine::Http;
namespace J = GYO::Engine::Json;

namespace {

std::string RoomTarget(const std::uint64_t room, const std::string_view action = {}) {
    std::string target = "/v1/rooms/" + std::to_string(room);
    if (!action.empty()) target += "/" + std::string(action);
    return target;
}

Room RoomOf(const J::ObjectView& object) {
    Room room;
    room.id = data::Unsigned(data::Member(object, "id"), "id");
    room.mode = data::String(data::Member(object, "mode"), "mode");
    room.map = data::String(data::Member(object, "map"), "map");
    room.players = static_cast<std::uint32_t>(data::Unsigned(data::Member(object, "players"), "players"));
    room.capacity = static_cast<std::uint32_t>(data::Unsigned(data::Member(object, "capacity"), "capacity"));
    room.state = data::String(data::Member(object, "state"), "state");
    if (data::HasMember(object, "owner")) room.owner = data::Unsigned(data::Member(object, "owner"), "owner");
    if (data::HasMember(object, "members")) {
        const auto members = data::Member(object, "members").AsArray();
        if (!members) throw std::runtime_error("members is not an array");
        for (const J::Value member : *members) {
            const J::ObjectView entry = data::AsObject(member, "a member");
            const auto ready = data::Member(entry, "ready").AsBool();
            if (!ready) throw std::runtime_error("a member's ready is not true or false");
            room.members.push_back(RoomMember{data::Unsigned(data::Member(entry, "userId"), "userId"), *ready});
        }
    }
    if (data::HasMember(object, "connect")) {
        const J::ObjectView connect = data::AsObject(data::Member(object, "connect"), "connect");
        room.connect = RoomConnect{data::String(data::Member(connect, "server"), "server"),
                                   data::Unsigned(data::Member(connect, "userId"), "userId"),
                                   data::String(data::Member(connect, "token"), "token")};
    }
    return room;
}

// The body's contract read by read; error and nullopt when it threw.
template <class T, class Read>
std::optional<T> Parsed(const std::string& body, std::string& error, Read read) {
    try {
        const J::Document document = data::ParseDocument(body);
        return read(data::AsObject(document.Root(), "the answer"));
    } catch (const std::runtime_error& failure) {
        error = failure.what();
        return std::nullopt;
    }
}

} // namespace

GatewayRequest SessionRequest(const std::string& deviceId) {
    std::string body;
    J::Writer writer(body);
    writer.BeginObject();
    writer.Key("deviceId");
    writer.String(deviceId);
    writer.EndObject();
    (void)writer.Finish();
    return {"POST", "/v1/session", std::move(body), {}};
}

GatewayRequest ListRoomsRequest() { return {"GET", "/v1/rooms", {}, {}}; }

GatewayRequest CreateRoomRequest(const std::string& mode, const std::string& map, std::string key) {
    std::string body;
    J::Writer writer(body);
    writer.BeginObject();
    writer.Key("mode");
    writer.String(mode);
    writer.Key("map");
    writer.String(map);
    writer.EndObject();
    (void)writer.Finish();
    return {"POST", "/v1/rooms", std::move(body), std::move(key)};
}

GatewayRequest RoomRequest(const std::uint64_t room) { return {"GET", RoomTarget(room), {}, {}}; }
GatewayRequest JoinRequest(const std::uint64_t room, std::string key) { return {"POST", RoomTarget(room, "join"), {}, std::move(key)}; }

GatewayRequest ReadyRequest(const std::uint64_t room, const bool ready, std::string key) {
    std::string body;
    J::Writer writer(body);
    writer.BeginObject();
    writer.Key("ready");
    writer.Bool(ready);
    writer.EndObject();
    (void)writer.Finish();
    return {"POST", RoomTarget(room, "ready"), std::move(body), std::move(key)};
}

GatewayRequest StartRequest(const std::uint64_t room, std::string key) { return {"POST", RoomTarget(room, "start"), {}, std::move(key)}; }
GatewayRequest LeaveRequest(const std::uint64_t room, std::string key) { return {"POST", RoomTarget(room, "leave"), {}, std::move(key)}; }
GatewayRequest SaveRequest() { return {"GET", "/v1/saves/me", {}, {}}; }

H::HttpRequest ToHttp(const GatewayRequest& request, const std::string& host, const std::optional<GatewaySession>& session) {
    H::HttpRequest http;
    http.method = request.method;
    http.target = request.target;
    http.host = host;
    if (session) http.headers.push_back({"Authorization", "Bearer " + session->token});
    if (!request.key.empty()) http.headers.push_back({"Idempotency-Key", request.key});
    if (!request.body.empty()) http.headers.push_back({"Content-Type", "application/json"});
    http.body = request.body;
    return http;
}

std::optional<GatewaySession> ParseSession(const std::string& body, std::string& error) {
    return Parsed<GatewaySession>(body, error, [](const J::ObjectView& root) {
        return GatewaySession{data::Unsigned(data::Member(root, "userId"), "userId"), data::String(data::Member(root, "sessionToken"), "sessionToken"),
                              data::Integer(data::Member(root, "expiresAt"), "expiresAt")};
    });
}

std::optional<std::vector<Room>> ParseRooms(const std::string& body, std::string& error) {
    return Parsed<std::vector<Room>>(body, error, [](const J::ObjectView& root) {
        const auto rooms = data::Member(root, "rooms").AsArray();
        if (!rooms) throw std::runtime_error("rooms is not an array");
        std::vector<Room> out;
        for (const J::Value room : *rooms) out.push_back(RoomOf(data::AsObject(room, "a room")));
        return out;
    });
}

std::optional<Room> ParseRoom(const std::string& body, std::string& error) {
    return Parsed<Room>(body, error, [](const J::ObjectView& root) { return RoomOf(root); });
}

std::optional<SaveAnswer> ParseSave(const std::string& body, std::string& error) {
    return Parsed<SaveAnswer>(body, error, [](const J::ObjectView& root) {
        SaveAnswer answer;
        answer.version = data::Unsigned(data::Member(root, "version"), "version");
        // The body as its own JSON text (the save's reader parses it again: match/PlayerSave.hpp).
        J::Writer writer(answer.body);
        J::WriteValue(writer, data::Member(root, "body"));
        (void)writer.Finish();
        return answer;
    });
}

std::string ErrorCodeOf(const std::string& body) {
    std::string error;
    const auto code = Parsed<std::string>(body, error, [](const J::ObjectView& root) { return data::String(data::Member(root, "error"), "error"); });
    return code.value_or(std::string());
}

std::string RandomHex(const std::size_t bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::random_device device;
    std::string out;
    out.reserve(bytes * 2);
    for (std::size_t i = 0; i < bytes; ++i) {
        const auto byte = static_cast<unsigned>(device() & 0xFFU);
        out.push_back(kDigits[byte >> 4U]);
        out.push_back(kDigits[byte & 0xFU]);
    }
    return out;
}

std::string LoadDeviceId(const std::string& path, std::string& error) {
    error.clear();
    if (path.empty()) {
        error = "no directory for the device id";
        return {};
    }
    auto kept = ReadLocalText(path);
    if (!kept) {
        error = Base::Describe(kept.error());
        return {};
    }
    if (*kept && !(*kept)->empty()) return **kept;
    std::string made = RandomHex(32);
    if (auto written = WriteLocalText(path, made); !written) {
        error = Base::Describe(written.error());
        return {};
    }
    return made;
}

GatewayClient::GatewayClient(GYO::Engine::OS::SocketAddress gateway, std::string deviceId, std::string unavailable,
                             const std::int64_t timeout)
    : gateway_(gateway), host_(gateway.ToString()), deviceId_(std::move(deviceId)), unavailable_(std::move(unavailable)), timeout_(timeout) {}

void GatewayClient::Ask(std::string tag, GatewayRequest request) { waiting_.push_back(Call{std::move(tag), std::move(request), false}); }

void GatewayClient::Start(Call call, const bool session, const std::int64_t now) {
    const GatewayRequest sent = session ? SessionRequest(deviceId_) : call.request;
    auto started = H::HttpCall::Start(gateway_, ToHttp(sent, host_, session ? std::nullopt : session_));
    current_ = Going{std::move(call), session, std::nullopt, now, {}};
    if (started) current_->http.emplace(std::move(started).value());
    else current_->failure = Base::Describe(started.error());
}

void GatewayClient::Note(const Going& going, const int status, std::string errorCode, const std::int64_t now) {
    if (calls_.size() == kKeptCalls) calls_.erase(calls_.begin());
    const GatewayRequest& request = going.session ? SessionRequest(deviceId_) : going.call.request;
    calls_.push_back(GatewayCallRecord{going.call.tag, going.session, request.method, request.target, status, std::move(errorCode),
                                       going.started - going.call.asked.value_or(going.started), now - going.started});
}

std::optional<GatewayResult> GatewayClient::Update(const std::int64_t now) {
    for (Call& waiting : waiting_) {
        if (!waiting.asked) waiting.asked = now;
    }
    if (!current_) {
        if (waiting_.empty()) return std::nullopt;
        Call next = std::move(waiting_.front());
        waiting_.pop_front();
        if (deviceId_.empty()) return GatewayResult{next.tag, 0, {}, "no_device", unavailable_};
        Start(std::move(next), !session_.has_value(), now);
    }
    Going& going = *current_;
    const auto finish = [this](GatewayResult result) {
        current_.reset();
        return std::optional<GatewayResult>(std::move(result));
    };
    GatewayResult result;
    result.tag = going.call.tag;
    if (!going.http) {
        Note(going, 0, "unreachable", now);
        return finish({result.tag, 0, {}, "unreachable", going.failure});
    }
    auto polled = going.http->Poll();
    if (!polled) {
        const bool unreachable = polled.error().code == H::HttpFailureCode::Unreachable || polled.error().code == H::HttpFailureCode::ConnectFailed;
        Note(going, 0, unreachable ? "unreachable" : "http", now);
        return finish({result.tag, 0, {}, unreachable ? "unreachable" : "http", Base::Describe(polled.error())});
    }
    if (!polled->has_value()) {
        if (now - going.started > timeout_) {
            Note(going, 0, "timeout", now);
            return finish({result.tag, 0, {}, "timeout", "no answer within " + std::to_string(timeout_ / 1'000'000'000) + " s"});
        }
        return std::nullopt;
    }
    const H::HttpResponse& response = **polled;
    result.status = response.status;
    result.body = response.body;
    if (!result.Ok()) result.errorCode = ErrorCodeOf(response.body);
    Note(going, result.status, result.errorCode, now);
    if (going.session) {
        if (!result.Ok()) return finish(std::move(result));
        std::string error;
        auto opened = ParseSession(response.body, error);
        if (!opened) return finish({result.tag, result.status, response.body, "http", "the session's answer: " + error});
        session_ = std::move(*opened);
        Call call = std::move(going.call);
        current_.reset();
        Start(std::move(call), false, now);
        return std::nullopt;
    }
    // The session is gone (expired, or the Gateway forgot it): open it again, once, and try the call again.
    if (result.status == 401 && !going.call.retried) {
        session_.reset();
        Call call = std::move(going.call);
        call.retried = true;
        current_.reset();
        Start(std::move(call), true, now);
        return std::nullopt;
    }
    return finish(std::move(result));
}

} // namespace object_fps_pvp::client
