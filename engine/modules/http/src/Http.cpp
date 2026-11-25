#include "engine/http/Http.hpp"

#include <algorithm>
#include <array>
#include <charconv>

namespace GYO::Engine::Http {
namespace {

char Lower(const char c) noexcept { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

std::string LowerCase(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), Lower);
    return out;
}

bool EqualsNoCase(const std::string_view a, const std::string_view b) noexcept {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return Lower(x) == Lower(y); });
}

std::string_view Trim(std::string_view text) noexcept {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

HttpFailure Fail(const HttpFailureCode code, std::string message) { return HttpFailure::Make(code, std::move(message)); }

} // namespace

const char* ToString(const HttpFailureCode code) noexcept {
    switch (code) {
    case HttpFailureCode::ConnectFailed: return "ConnectFailed";
    case HttpFailureCode::TransferFailed: return "TransferFailed";
    case HttpFailureCode::MalformedResponse: return "MalformedResponse";
    case HttpFailureCode::Truncated: return "Truncated";
    case HttpFailureCode::TooLarge: return "TooLarge";
    }
    return "Unknown";
}

std::optional<std::string_view> HttpResponse::Find(const std::string_view name) const {
    for (const Header& header : headers) {
        if (EqualsNoCase(header.name, name)) return std::string_view(header.value);
    }
    return std::nullopt;
}

std::string EncodeRequest(const HttpRequest& request) {
    std::string out = request.method + " " + request.target + " HTTP/1.1\r\nHost: " + request.host + "\r\nConnection: close\r\n";
    const bool sendsBody = !request.body.empty() || request.method == "POST" || request.method == "PUT" || request.method == "PATCH";
    if (sendsBody) out += "Content-Length: " + std::to_string(request.body.size()) + "\r\n";
    for (const Header& header : request.headers) out += header.name + ": " + header.value + "\r\n";
    out += "\r\n";
    out += request.body;
    return out;
}

HttpResult<void> ResponseParser::Feed(const std::span<const std::byte> bytes) {
    if (stage_ == Stage::Done) return {};
    pending_.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return Step();
}

HttpResult<void> ResponseParser::Close() {
    if (stage_ == Stage::Done) return {};
    if (stage_ == Stage::UntilClose) {
        building_.body += pending_;
        pending_.clear();
        if (building_.body.size() > limits_.body) return Base::Err(Fail(HttpFailureCode::TooLarge, "the body is larger than the limit"));
        stage_ = Stage::Done;
        response_ = std::move(building_);
        return {};
    }
    return Base::Err(Fail(HttpFailureCode::Truncated, "the connection closed before the answer was complete"));
}

HttpResult<void> ResponseParser::ReadHead(const std::string_view head) {
    // The status line: HTTP/1.x SP 3DIGIT [SP reason].
    const std::size_t lineEnd = head.find("\r\n");
    const std::string_view statusLine = head.substr(0, lineEnd);
    if (statusLine.size() < 12 || (statusLine.substr(0, 9) != "HTTP/1.1 " && statusLine.substr(0, 9) != "HTTP/1.0 ")) {
        return Base::Err(Fail(HttpFailureCode::MalformedResponse, "the status line is not HTTP/1.x"));
    }
    int status = 0;
    const auto parsed = std::from_chars(statusLine.data() + 9, statusLine.data() + 12, status);
    if (parsed.ec != std::errc{} || parsed.ptr != statusLine.data() + 12 || status < 100 || status > 599 ||
        (statusLine.size() > 12 && statusLine[12] != ' ')) {
        return Base::Err(Fail(HttpFailureCode::MalformedResponse, "the status code is not three digits"));
    }
    building_ = HttpResponse{};
    building_.status = status;
    std::string_view rest = lineEnd == std::string_view::npos ? std::string_view{} : head.substr(lineEnd + 2);
    while (!rest.empty()) {
        const std::size_t end = rest.find("\r\n");
        const std::string_view line = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 2);
        if (line.empty()) continue;
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            return Base::Err(Fail(HttpFailureCode::MalformedResponse, "a header line has no name"));
        }
        building_.headers.push_back(Header{LowerCase(line.substr(0, colon)), std::string(Trim(line.substr(colon + 1)))});
    }
    // An interim answer (100 Continue): the real one follows.
    if (status < 200) {
        building_ = HttpResponse{};
        stage_ = Stage::Head;
        return {};
    }
    if (status == 204 || status == 304) {
        stage_ = Stage::Done;
        return {};
    }
    if (const auto encoding = building_.Find("transfer-encoding");
        encoding && LowerCase(*encoding).find("chunked") != std::string::npos) {
        stage_ = Stage::ChunkSize;
        return {};
    }
    if (const auto length = building_.Find("content-length")) {
        std::size_t size = 0;
        const auto read = std::from_chars(length->data(), length->data() + length->size(), size);
        if (read.ec != std::errc{} || read.ptr != length->data() + length->size()) {
            return Base::Err(Fail(HttpFailureCode::MalformedResponse, "Content-Length is not a number"));
        }
        if (size > limits_.body) return Base::Err(Fail(HttpFailureCode::TooLarge, "the body is larger than the limit"));
        remaining_ = size;
        stage_ = size == 0 ? Stage::Done : Stage::Sized;
        return {};
    }
    stage_ = Stage::UntilClose;
    return {};
}

HttpResult<void> ResponseParser::Step() {
    for (;;) {
        switch (stage_) {
        case Stage::Head: {
            const std::size_t end = pending_.find("\r\n\r\n");
            if (end == std::string::npos) {
                if (pending_.size() > limits_.head) return Base::Err(Fail(HttpFailureCode::TooLarge, "the head is larger than the limit"));
                return {};
            }
            if (end + 4 > limits_.head) return Base::Err(Fail(HttpFailureCode::TooLarge, "the head is larger than the limit"));
            const std::string head = pending_.substr(0, end + 2);
            pending_.erase(0, end + 4);
            if (auto read = ReadHead(head); !read) return read;
            break;
        }
        case Stage::Sized: {
            const std::size_t take = std::min(remaining_, pending_.size());
            building_.body.append(pending_, 0, take);
            pending_.erase(0, take);
            remaining_ -= take;
            if (remaining_ > 0) return {};
            stage_ = Stage::Done;
            break;
        }
        case Stage::ChunkSize: {
            const std::size_t end = pending_.find("\r\n");
            if (end == std::string::npos) {
                if (pending_.size() > 1024) return Base::Err(Fail(HttpFailureCode::MalformedResponse, "a chunk size line is too long"));
                return {};
            }
            std::string_view line(pending_.data(), end);
            line = line.substr(0, line.find(';'));
            line = Trim(line);
            std::size_t size = 0;
            const auto read = std::from_chars(line.data(), line.data() + line.size(), size, 16);
            if (line.empty() || read.ec != std::errc{} || read.ptr != line.data() + line.size()) {
                return Base::Err(Fail(HttpFailureCode::MalformedResponse, "a chunk size is not hexadecimal"));
            }
            pending_.erase(0, end + 2);
            if (size > limits_.body || building_.body.size() + size > limits_.body) {
                return Base::Err(Fail(HttpFailureCode::TooLarge, "the body is larger than the limit"));
            }
            remaining_ = size;
            stage_ = size == 0 ? Stage::Trailer : Stage::ChunkData;
            break;
        }
        case Stage::ChunkData: {
            const std::size_t take = std::min(remaining_, pending_.size());
            building_.body.append(pending_, 0, take);
            pending_.erase(0, take);
            remaining_ -= take;
            if (remaining_ > 0) return {};
            stage_ = Stage::ChunkEnd;
            break;
        }
        case Stage::ChunkEnd: {
            if (pending_.size() < 2) return {};
            if (pending_[0] != '\r' || pending_[1] != '\n') {
                return Base::Err(Fail(HttpFailureCode::MalformedResponse, "a chunk does not end with CRLF"));
            }
            pending_.erase(0, 2);
            stage_ = Stage::ChunkSize;
            break;
        }
        case Stage::Trailer: {
            const std::size_t end = pending_.find("\r\n");
            if (end == std::string::npos) {
                if (pending_.size() > limits_.head) return Base::Err(Fail(HttpFailureCode::TooLarge, "the trailer is larger than the limit"));
                return {};
            }
            const bool last = end == 0;
            pending_.erase(0, end + 2);
            if (last) stage_ = Stage::Done;
            break;
        }
        case Stage::UntilClose: {
            building_.body += pending_;
            pending_.clear();
            if (building_.body.size() > limits_.body) return Base::Err(Fail(HttpFailureCode::TooLarge, "the body is larger than the limit"));
            return {};
        }
        case Stage::Done:
            if (!response_) response_ = std::move(building_);
            return {};
        }
    }
}

HttpCall::HttpCall(OS::StreamSocket socket, std::string request, const ResponseLimits limits)
    : socket_(std::move(socket)), request_(std::move(request)), parser_(limits) {}

HttpResult<HttpCall> HttpCall::Start(const OS::DatagramAddress& to, const HttpRequest& request, const ResponseLimits limits) {
    auto socket = OS::StreamSocket::Connect(to);
    if (!socket) {
        return Base::Err(Fail(HttpFailureCode::ConnectFailed, "the connection did not start: " + socket.error().message));
    }
    return HttpCall(std::move(socket).value(), EncodeRequest(request), limits);
}

HttpResult<std::optional<HttpResponse>> HttpCall::Poll() {
    if (failed_) return Base::Err(*failed_);
    if (parser_.Response()) return std::optional<HttpResponse>(*parser_.Response());
    const auto fail = [this](HttpFailure failure) -> HttpResult<std::optional<HttpResponse>> {
        failed_ = failure;
        return Base::Err(std::move(failure));
    };
    auto state = socket_.State();
    if (!state) return fail(Fail(HttpFailureCode::ConnectFailed, "the connection did not open: " + state.error().message));
    if (*state == OS::StreamState::Connecting) return std::optional<HttpResponse>();
    while (sent_ < request_.size()) {
        auto sent = socket_.Send(std::as_bytes(std::span(request_.data() + sent_, request_.size() - sent_)));
        if (!sent) return fail(Fail(HttpFailureCode::TransferFailed, "the request did not go out: " + sent.error().message));
        if (*sent == 0) break;
        sent_ += *sent;
    }
    std::array<std::byte, 4096> buffer{};
    for (;;) {
        auto received = socket_.Receive(buffer);
        if (!received) return fail(Fail(HttpFailureCode::TransferFailed, "the answer did not come in: " + received.error().message));
        if (!*received) break;
        if (**received == 0) {
            if (auto closed = parser_.Close(); !closed) return fail(closed.error());
            break;
        }
        if (auto fed = parser_.Feed(std::span(buffer).first(**received)); !fed) return fail(fed.error());
        if (parser_.Response()) break;
    }
    if (parser_.Response()) return std::optional<HttpResponse>(*parser_.Response());
    return std::optional<HttpResponse>();
}

} // namespace GYO::Engine::Http
