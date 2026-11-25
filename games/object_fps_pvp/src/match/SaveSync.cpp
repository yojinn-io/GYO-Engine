#include "object_fps_pvp/match/SaveSync.hpp"

#include "data/JsonFields.hpp"

#include "engine/json/Writer.hpp"

#include <stdexcept>

namespace object_fps_pvp::match {

namespace J = GYO::Engine::Json;

namespace {

// The save service's read: its version and the save.
struct Read final {
    std::uint64_t version = 0;
    PlayerSave save;
};

std::optional<Read> ParseRead(const std::string& body, std::string& error) {
    try {
        const J::Document document = data::ParseDocument(body);
        const J::ObjectView object = data::AsObject(document.Root(), "the save service's read");
        Read read;
        read.version = data::Unsigned(data::Member(object, "version"), "version");
        std::string text;
        J::Writer writer(text);
        J::WriteValue(writer, data::Member(object, "body"));
        (void)writer.Finish();
        auto save = DecodePlayerSave(text, error);
        if (!save) return std::nullopt;
        read.save = std::move(*save);
        return read;
    } catch (const std::runtime_error& failure) {
        error = failure.what();
        return std::nullopt;
    }
}

std::optional<std::uint64_t> ParseWritten(const std::string& body) {
    try {
        const J::Document document = data::ParseDocument(body);
        return data::Unsigned(data::Member(data::AsObject(document.Root(), "the write's answer"), "version"), "version");
    } catch (const std::runtime_error&) {
        return std::nullopt;
    }
}

bool Retryable(const int status) { return status == 0 || status >= 500; }

} // namespace

void SaveSync::Joined(const std::uint64_t user) {
    Player& player = players_[user];
    ++player.connections;
    player.leaving = false;
    // A failed read is tried again when the player comes back.
    if (!player.loaded && player.phase == Phase::Idle) player.gaveUp = false;
}

void SaveSync::Left(const std::uint64_t user) {
    const auto found = players_.find(user);
    if (found == players_.end() || found->second.connections == 0) return;
    if (--found->second.connections == 0) found->second.leaving = true;
}

void SaveSync::Died(const std::uint64_t user) {
    const auto found = players_.find(user);
    if (found != players_.end()) ++found->second.deaths;
}

SaveRequest SaveSync::Request(const std::uint64_t user, const Player& player) {
    SaveRequest request;
    request.id = nextId_++;
    request.user = user;
    request.write = player.phase == Phase::Writing;
    if (request.write) {
        request.key = player.leaveKey;
        request.body = "{\"version\":" + std::to_string(player.version) + ",\"body\":" + EncodePlayerSave(player.leaveSave) + "}";
    }
    return request;
}

std::vector<SaveRequest> SaveSync::Due(const std::uint64_t tick) {
    std::vector<SaveRequest> due;
    for (auto& [user, player] : players_) {
        if (player.inFlight != 0 || player.gaveUp) continue;
        if (player.phase == Phase::Idle) {
            if (!player.loaded && player.connections > 0) {
                player.phase = Phase::Reading;
                player.attempts = 0;
                player.retryAt = tick;
            } else if (player.loaded && player.leaving) {
                // The leave is fixed now: its deaths, its key, the save it makes.
                player.leaving = false;
                player.leaveDeaths = player.deaths;
                player.deaths = 0;
                ++player.leaves;
                player.leaveKey = "match-" + std::to_string(matchId_) + "-user-" + std::to_string(user) + "-leave-" +
                                  std::to_string(player.leaves);
                player.leaveSave = Settled(player.save, player.leaveDeaths, matchId_, player.leaveKey);
                player.phase = Phase::Writing;
                player.attempts = 0;
                player.retryAt = tick;
            } else {
                continue;
            }
        }
        if (tick < player.retryAt) continue;
        ++player.attempts;
        SaveRequest request = Request(user, player);
        player.inFlight = request.id;
        player.sentAt = tick;
        due.push_back(std::move(request));
    }
    return due;
}

void SaveSync::Retry(const std::uint64_t user, Player& player, const std::uint64_t tick, const std::string& why) {
    if (player.attempts >= kAttempts) return Fail(user, player, why);
    player.retryAt = tick + (kFirstBackoffTicks << (player.attempts - 1));
}

void SaveSync::Fail(const std::uint64_t user, Player& player, const std::string& why) {
    const bool writing = player.phase != Phase::Reading;
    ++report_.failures;
    report_.lastFailure = "user " + std::to_string(user) + (writing ? " write: " : " read: ") + why;
    player.phase = Phase::Idle;
    player.inFlight = 0;
    // A failed read is tried again when the player joins again; a failed write's leave is lost (counted above).
    if (!writing) player.gaveUp = true;
}

void SaveSync::TakeRead(const std::uint64_t user, Player& player, const SaveAnswer& answer, const std::uint64_t tick) {
    if (Retryable(answer.status)) return Retry(user, player, tick, "the save service gave no answer (" + std::to_string(answer.status) + ")");
    if (answer.status != 200) return Fail(user, player, "the save service answered " + std::to_string(answer.status) + ": " + answer.body);
    std::string error;
    auto read = ParseRead(answer.body, error);
    if (!read) return Fail(user, player, error);
    if (player.phase == Phase::Rereading) {
        if (read->save.lastWrite == player.leaveKey) {
            // This write is already in the save (the service forgot its record of it).
            player.version = read->version;
            player.save = std::move(read->save);
            player.phase = Phase::Idle;
            ++report_.writes;
            return;
        }
        // Settle the same leave on the newer save and write it again.
        player.version = read->version;
        player.save = read->save;
        player.leaveSave = Settled(std::move(read->save), player.leaveDeaths, matchId_, player.leaveKey);
        player.phase = Phase::Writing;
        player.retryAt = tick;
        return;
    }
    player.version = read->version;
    player.save = std::move(read->save);
    player.loaded = true;
    player.phase = Phase::Idle;
    ++report_.reads;
}

void SaveSync::TakeWrite(const std::uint64_t user, Player& player, const SaveAnswer& answer, const std::uint64_t tick) {
    if (Retryable(answer.status)) return Retry(user, player, tick, "the save service gave no answer (" + std::to_string(answer.status) + ")");
    if (answer.status == 409) {
        if (player.attempts >= kAttempts) return Fail(user, player, "the save kept changing (409)");
        player.phase = Phase::Rereading;
        player.retryAt = tick;
        return;
    }
    if (answer.status != 200) return Fail(user, player, "the save service answered " + std::to_string(answer.status) + ": " + answer.body);
    const auto version = ParseWritten(answer.body);
    if (!version) return Fail(user, player, "the save service's answer has no version");
    player.version = *version;
    player.save = player.leaveSave;
    player.phase = Phase::Idle;
    ++report_.writes;
}

void SaveSync::Answered(const std::uint64_t id, const SaveAnswer& answer, const std::uint64_t tick) {
    for (auto& [user, player] : players_) {
        if (player.inFlight != id) continue;
        player.inFlight = 0;
        if (player.phase == Phase::Writing) TakeWrite(user, player, answer, tick);
        else TakeRead(user, player, answer, tick);
        return;
    }
}

std::vector<std::uint64_t> SaveSync::TimedOut(const std::uint64_t tick) {
    std::vector<std::uint64_t> late;
    for (const auto& [user, player] : players_) {
        if (player.inFlight != 0 && tick - player.sentAt >= kAnswerTicks) late.push_back(player.inFlight);
    }
    for (const std::uint64_t id : late) Answered(id, SaveAnswer{}, tick);
    return late;
}

bool SaveSync::Busy() const {
    for (const auto& [user, player] : players_) {
        if (player.phase != Phase::Idle || (player.loaded && player.leaving)) return true;
    }
    return false;
}

std::optional<PlayerSave> SaveSync::SaveOf(const std::uint64_t user) const {
    const auto found = players_.find(user);
    if (found == players_.end() || !found->second.loaded) return std::nullopt;
    return found->second.save;
}

} // namespace object_fps_pvp::match
