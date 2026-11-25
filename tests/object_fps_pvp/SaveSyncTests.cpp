// The Match's save bookkeeping (match/SaveSync.hpp; gateway declaration D7): driven by hand, answers made up.
#include <doctest/doctest.h>

#include "object_fps_pvp/match/SaveSync.hpp"

#include <string>
#include <vector>

namespace M = object_fps_pvp::match;

namespace {

constexpr std::uint64_t kMatch = 9;

std::string ReadBody(const std::uint64_t version, const M::PlayerSave& save) {
    return "{\"userId\":1,\"version\":" + std::to_string(version) + ",\"body\":" + M::EncodePlayerSave(save) + "}";
}

std::string WriteBody(const std::uint64_t version, const M::PlayerSave& save) {
    return "{\"version\":" + std::to_string(version) + ",\"body\":" + M::EncodePlayerSave(save) + "}";
}

// One request due at this tick (fails the test otherwise).
M::SaveRequest One(M::SaveSync& sync, const std::uint64_t tick) {
    auto due = sync.Due(tick);
    REQUIRE_MESSAGE(due.size() == 1, "expected one request at tick " << tick << ", got " << due.size());
    return due.front();
}

// A player who joined and whose save (version 3, two matches, one death) was read.
M::SaveSync Loaded() {
    M::SaveSync sync(kMatch);
    sync.Joined(1);
    const auto read = One(sync, 0);
    CHECK_FALSE(read.write);
    sync.Answered(read.id, {200, ReadBody(3, {2, 1, 4, "match-4-user-1-leave-1"})}, 0);
    REQUIRE(sync.SaveOf(1).has_value());
    return sync;
}

} // namespace

TEST_CASE("save sync: a join reads the save; a leave writes it settled, with the version read and its own key") {
    M::SaveSync sync = Loaded();
    sync.Died(1);
    sync.Left(1);
    const auto write = One(sync, 1);
    CHECK(write.write);
    CHECK(write.key == "match-9-user-1-leave-1");
    CHECK(write.body == WriteBody(3, {3, 2, kMatch, "match-9-user-1-leave-1"}));
    CHECK(sync.Busy());
    sync.Answered(write.id, {200, "{\"version\":4}"}, 1);
    CHECK_FALSE(sync.Busy());
    CHECK(sync.Report().writes == 1);
    CHECK(sync.SaveOf(1) == M::PlayerSave{3, 2, kMatch, "match-9-user-1-leave-1"});
}

TEST_CASE("save sync: a leave and a join in one frame (a reconnect) are no leave") {
    M::SaveSync sync = Loaded();
    sync.Left(1);
    sync.Joined(1);
    CHECK_MESSAGE(sync.Due(1).empty(), "a reconnect in one frame was settled as a leave");
    CHECK_FALSE(sync.Busy());
}

TEST_CASE("save sync: a newer connection that replaces an older one (a join, then the older one's leave) is no leave") {
    M::SaveSync sync = Loaded();
    sync.Joined(1);
    sync.Left(1);
    CHECK_MESSAGE(sync.Due(1).empty(), "a replaced connection was settled as a leave");
    sync.Left(1);
    CHECK_MESSAGE(sync.Due(2).size() == 1, "the last connection's leave was not written");
}

TEST_CASE("save sync: a conflict reads again and settles the same leave on the newer save") {
    M::SaveSync sync = Loaded();
    sync.Left(1);
    const auto write = One(sync, 1);
    sync.Answered(write.id, {409, "{\"error\":\"version_conflict\",\"version\":5}"}, 1);
    const auto reread = One(sync, 1);
    CHECK_FALSE(reread.write);
    // Another Match wrote meanwhile: five matches, three deaths, at version 5.
    sync.Answered(reread.id, {200, ReadBody(5, {5, 3, 8, "match-8-user-1-leave-1"})}, 1);
    const auto again = One(sync, 1);
    CHECK(again.write);
    CHECK(again.key == write.key);
    CHECK_MESSAGE(again.body == WriteBody(5, {6, 3, kMatch, "match-9-user-1-leave-1"}),
                  "the leave was not settled on the newer save: " << again.body);
    sync.Answered(again.id, {200, "{\"version\":6}"}, 1);
    CHECK(sync.Report().failures == 0);
    CHECK(sync.SaveOf(1)->matches == 6);
}

TEST_CASE("save sync: a conflict whose save already carries this write's key is this write") {
    M::SaveSync sync = Loaded();
    sync.Left(1);
    const auto write = One(sync, 1);
    sync.Answered(write.id, {409, "{\"error\":\"version_conflict\",\"version\":4}"}, 1);
    const auto reread = One(sync, 1);
    sync.Answered(reread.id, {200, ReadBody(4, {3, 1, kMatch, write.key})}, 1);
    CHECK_MESSAGE(sync.Due(1).empty(), "a write already in the save was written again");
    CHECK(sync.Report().writes == 1);
    CHECK(sync.Report().failures == 0);
}

TEST_CASE("save sync: no answer sends the same request again after growing waits, then is a failure") {
    M::SaveSync sync = Loaded();
    sync.Left(1);
    std::uint64_t tick = 1;
    auto write = One(sync, tick);
    const std::string key = write.key;
    const std::vector<std::uint64_t> waits{16, 32, 64, 128};
    for (const std::uint64_t wait : waits) {
        sync.Answered(write.id, {503, ""}, tick);
        CHECK_MESSAGE(sync.Due(tick + wait - 1).empty(), "sent again before its wait of " << wait << " ticks");
        tick += wait;
        write = One(sync, tick);
        CHECK(write.key == key);
    }
    sync.Answered(write.id, {0, ""}, tick);
    CHECK(sync.Report().failures == 1);
    CHECK_MESSAGE(sync.Report().lastFailure.rfind("user 1 write:", 0) == 0, sync.Report().lastFailure);
    CHECK_FALSE(sync.Busy());
}

TEST_CASE("save sync: an answer that never comes is taken as none, and the request goes out again") {
    M::SaveSync sync = Loaded();
    sync.Left(1);
    const auto write = One(sync, 10);
    CHECK(sync.TimedOut(10 + M::SaveSync::kAnswerTicks - 1).empty());
    const auto late = sync.TimedOut(10 + M::SaveSync::kAnswerTicks);
    REQUIRE_MESSAGE(late.size() == 1, "a request with no answer never timed out");
    CHECK(late.front() == write.id);
    const auto again = One(sync, 10 + M::SaveSync::kAnswerTicks + M::SaveSync::kFirstBackoffTicks);
    CHECK(again.key == write.key);
    sync.Answered(write.id, {200, "{\"version\":4}"}, 400);
    CHECK_MESSAGE(sync.Report().writes == 0, "the timed-out request's late answer was taken");
}

TEST_CASE("save sync: a player who comes back and leaves again while a write is in flight has a leave of their own") {
    M::SaveSync sync = Loaded();
    sync.Left(1);
    const auto first = One(sync, 1);
    sync.Joined(1);
    sync.Died(1);
    sync.Left(1);
    CHECK(sync.Due(2).empty());
    sync.Answered(first.id, {200, "{\"version\":4}"}, 2);
    const auto second = One(sync, 3);
    CHECK(second.key == "match-9-user-1-leave-2");
    CHECK_MESSAGE(second.body == WriteBody(4, {3, 2, kMatch, "match-9-user-1-leave-2"}),
                  "the second leave lost its death or counted the Match twice: " << second.body);
}

TEST_CASE("save sync: a refused read gives up for this stay and is tried again when the player comes back") {
    M::SaveSync sync(kMatch);
    sync.Joined(1);
    auto read = One(sync, 0);
    sync.Answered(read.id, {400, "{\"error\":\"invalid_user\"}"}, 0);
    CHECK(sync.Report().failures == 1);
    CHECK(sync.Due(1).empty());
    sync.Left(1);
    CHECK_MESSAGE(sync.Due(2).empty(), "a leave whose save was never read was written");
    sync.Joined(1);
    read = One(sync, 3);
    CHECK_FALSE(read.write);
}
