#pragma once

// The Match's save bookkeeping (gateway declaration D7), as a pure state machine: no World, no socket, no clock. The
// caller tells it who joined, who left and who died, asks it what to send (Due, at a tick), sends that to the save
// service and hands back each answer (Answered). world/MatchSaves.hpp drives it from a System with HttpCall.
//
//   joined        -> read the save (once per stay; again after a failed read when the player joins again)
//   left          -> write the save settled with this stay's deaths, once the player has no connection left: joins
//                    and leaves are counted per connection, so a reconnect (a leave and a join in one frame) and a
//                    newer connection that replaces an older one (a join, then the older one's leave) are no leave
//   write's answer
//     200         -> the save is the written one
//     409         -> read again: a save that already carries this write's key is this write (the service forgot its
//                    record, it did not lose the write); otherwise the same leave is settled on the newer save and
//                    written again
//     5xx, none   -> sent again, the same request, after 16, 32, 64, 128 ticks (5 times in all), then a failure;
//                    an answer that has not come kAnswerTicks after its request is none (TimedOut)
//     other       -> a failure
//
// A leave's deaths and key are fixed when its write starts: a player who comes back and leaves again while it is in
// flight has a leave of their own, written after it.

#include "object_fps_pvp/match/PlayerSave.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace object_fps_pvp::match {

struct SaveRequest final {
    std::uint64_t id = 0;
    std::uint64_t user = 0;
    bool write = false;  // PUT with body and key; else GET
    std::string body;
    std::string key;
};

struct SaveAnswer final {
    // 0: no answer (the connection failed or broke).
    int status = 0;
    std::string body;
};

struct SaveSyncReport final {
    std::uint64_t reads = 0;
    std::uint64_t writes = 0;
    std::uint64_t failures = 0;
    std::string lastFailure;
};

class SaveSync final {
public:
    static constexpr int kAttempts = 5;
    static constexpr std::uint64_t kFirstBackoffTicks = 16;
    // How long a request waits for its answer (at 64 steps a second, five seconds).
    static constexpr std::uint64_t kAnswerTicks = 320;

    explicit SaveSync(std::uint64_t matchId) : matchId_(matchId) {}

    void Joined(std::uint64_t user);
    void Left(std::uint64_t user);
    void Died(std::uint64_t user);

    // What to send now.
    [[nodiscard]] std::vector<SaveRequest> Due(std::uint64_t tick);
    // The answer to request id (an id not in flight is ignored).
    void Answered(std::uint64_t id, const SaveAnswer& answer, std::uint64_t tick);
    // The requests whose answer did not come in time: each is taken as no answer; the caller drops their calls.
    [[nodiscard]] std::vector<std::uint64_t> TimedOut(std::uint64_t tick);

    // A read or write still to finish (a Match ending waits for it).
    [[nodiscard]] bool Busy() const;
    [[nodiscard]] const SaveSyncReport& Report() const noexcept { return report_; }
    // The save the Match last read or wrote for a player.
    [[nodiscard]] std::optional<PlayerSave> SaveOf(std::uint64_t user) const;

private:
    enum class Phase : std::uint8_t { Idle, Reading, Writing, Rereading };
    struct Player final {
        // Open connections of the player.
        std::uint32_t connections = 0;
        bool loaded = false;
        bool gaveUp = false;
        bool leaving = false;
        std::uint64_t version = 0;
        PlayerSave save;
        std::uint64_t deaths = 0;
        std::uint32_t leaves = 0;
        Phase phase = Phase::Idle;
        std::uint64_t inFlight = 0;  // 0: none
        std::uint64_t sentAt = 0;
        std::uint64_t retryAt = 0;
        int attempts = 0;
        // The leave being written.
        std::uint64_t leaveDeaths = 0;
        std::string leaveKey;
        PlayerSave leaveSave;
    };

    [[nodiscard]] SaveRequest Request(std::uint64_t user, const Player& player);
    void Retry(std::uint64_t user, Player& player, std::uint64_t tick, const std::string& why);
    void Fail(std::uint64_t user, Player& player, const std::string& why);
    void TakeRead(std::uint64_t user, Player& player, const SaveAnswer& answer, std::uint64_t tick);
    void TakeWrite(std::uint64_t user, Player& player, const SaveAnswer& answer, std::uint64_t tick);

    std::uint64_t matchId_;
    std::uint64_t nextId_ = 1;
    std::map<std::uint64_t, Player> players_;
    SaveSyncReport report_;
};

} // namespace object_fps_pvp::match
