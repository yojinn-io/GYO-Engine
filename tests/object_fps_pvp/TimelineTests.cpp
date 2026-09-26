#include <doctest/doctest.h>
#include "RetroFPS/Pvp/SnapshotTimeline.hpp"

#include <array>
#include <vector>

namespace {
using namespace fps::pvp;
SnapshotTimeline::Clock::time_point At(double seconds) {
    return SnapshotTimeline::Clock::time_point{} + std::chrono::duration_cast<SnapshotTimeline::Clock::duration>(
        std::chrono::duration<double>(seconds));
}
std::optional<SnapshotPresentation> Presented(SnapshotTimeline& history, PlayerId id, SnapshotTimeline::Clock::time_point now) {
    auto result = history.Sample(id, now);
    if (result) {
        result->holding = history.CommitPresented(now, &result->player, result->holding);
        result->holdCount = history.HoldCount();
        result->holdSeconds = history.HoldSeconds();
        result->totalHoldSeconds = history.TotalHoldSeconds();
    }
    return result;
}
WorldSnapshot State(std::uint64_t tick, std::uint64_t epoch = 1) {
    return {tick, {{1, {static_cast<float>(tick) * .05F, 0, 0}, 0, 0, tick, epoch},
                   {2, {0, 0, static_cast<float>(tick) * .05F}, 0, 0, tick, 1}}};
}
}

TEST_CASE("PvP remote history selects the same authority time at 30 60 and 144 FPS") {
    for (const int fps : {30, 60, 144}) {
        SnapshotTimeline timeline;
        std::uint64_t nextTick = 1;
        double previousCursor = 0;
        unsigned movingFrames = 0;
        float previousX = 0;
        for (int frame = 1; frame <= fps * 3; ++frame) {
            const double now = static_cast<double>(frame) / fps;
            while (nextTick * MovementTickSeconds <= now + 1e-9) {
                REQUIRE(timeline.Push(State(nextTick), At(nextTick * MovementTickSeconds)));
                ++nextTick;
            }
            const auto pose = Presented(timeline, 1, At(now));
            if (!pose) { CHECK(nextTick == 1); continue; }
            CHECK(pose->presentationTick + 1e-6 >= previousCursor);
            CHECK(pose->presentationTick <= nextTick - 1 + 1e-6);
            CHECK(pose->lowerTick <= pose->upperTick);
            CHECK(pose->historySize <= 64);
            if (frame > 2 && pose->player.position.x > previousX + .0001F) ++movingFrames;
            if (frame % fps == 0) CHECK(pose->player.position.x == doctest::Approx((now * 60 - 1) * .05).epsilon(1e-5));
            previousCursor = pose->presentationTick;
            previousX = pose->player.position.x;
        }
        CHECK(movingFrames >= static_cast<unsigned>(fps * 3 - std::ceil(2 * MovementTickSeconds * fps) - 1));
    }
}

TEST_CASE("PvP stalled presentation drains current history without replay or reanchoring") {
    for (const double pause : {.083, .25, 6.0}) {
        SnapshotTimeline timeline;
        for (std::uint64_t tick = 1; tick <= 30; ++tick)
            REQUIRE(timeline.Push(State(tick), At(tick * MovementTickSeconds)));
        REQUIRE(Presented(timeline, 1, At(.5)));
        const auto newest = static_cast<std::uint64_t>((.5 + pause) * 60);
        const auto firstRetained = std::max<std::uint64_t>(31, newest > 63 ? newest - 63 : 31);
        for (auto tick = firstRetained; tick <= newest; ++tick)
            REQUIRE(timeline.Push(State(tick), At(tick * MovementTickSeconds)));
        const auto pose = Presented(timeline, 1, At(.5 + pause));
        REQUIRE(pose);
        CHECK(pose->phaseReanchors == 0);
        CHECK(pose->presentationTick >= newest - 1 - 1e-5);
        CHECK(pose->presentationTick <= newest);
        CHECK(pose->historySize <= 64);
    }
}

TEST_CASE("PvP remote missing snapshots hold without extrapolation and count episodes") {
    SnapshotTimeline timeline;
    REQUIRE(timeline.Push(State(10), At(1)));
    REQUIRE(timeline.Push(State(11), At(1 + MovementTickSeconds)));
    const auto first = Presented(timeline, 1, At(1.05));
    REQUIRE(first);
    CHECK_FALSE(first->holding);
    CHECK(first->holdCount == 0);
    CHECK(first->player.position.x == doctest::Approx(.55));
    const auto later = Presented(timeline, 1, At(1.2));
    REQUIRE(later);
    CHECK(later->holdCount == 1);
    CHECK(later->holdSeconds == doctest::Approx(.15));
    CHECK(later->player.position.x == first->player.position.x);
    REQUIRE(timeline.Push(State(23), At(1 + 13 * MovementTickSeconds)));
    const auto recovered = Presented(timeline, 1, At(1 + 13 * MovementTickSeconds));
    REQUIRE(recovered);
    CHECK_FALSE(recovered->holding);
    CHECK(recovered->gapCount == 11);
    CHECK(recovered->presentationTick == doctest::Approx(22));
    CHECK(recovered->totalHoldSeconds >= .15);
}

TEST_CASE("PvP remote epoch and membership changes never blend old player segments") {
    SnapshotTimeline timeline;
    REQUIRE(timeline.Push(State(1), At(1)));
    REQUIRE(timeline.Push(State(2), At(1 + MovementTickSeconds)));
    auto reset = State(3, 2);
    reset.players[0].position.x = 100;
    reset.players[0].lastResolvedCommand = 0;
    REQUIRE(timeline.Push(reset, At(1 + 2 * MovementTickSeconds)));
    const auto changed = Presented(timeline, 1, At(1 + 2 * MovementTickSeconds));
    REQUIRE(changed);
    CHECK(changed->player.movementEpoch == 2);
    CHECK(changed->player.position.x == 100);
    CHECK(changed->lowerTick == 3);
    const auto unchanged = Presented(timeline, 2, At(1 + 2 * MovementTickSeconds));
    REQUIRE(unchanged);
    CHECK(unchanged->player.position.z == doctest::Approx(.1));
    auto left = State(4, 2);
    left.players.erase(left.players.begin());
    REQUIRE(timeline.Push(left, At(1 + 3 * MovementTickSeconds)));
    CHECK_FALSE(Presented(timeline, 1, At(1.1)));
    auto joined = State(5);
    joined.players[0].playerId = 99;
    joined.players[0].position.x = -10;
    REQUIRE(timeline.Push(joined, At(1 + 4 * MovementTickSeconds)));
    const auto fresh = Presented(timeline, 99, At(1 + 4 * MovementTickSeconds));
    REQUIRE(fresh);
    CHECK(fresh->player.position.x == -10);
    timeline.Reset();
    CHECK(timeline.Size() == 0);
    CHECK_FALSE(Presented(timeline, 99, At(2)));
}

TEST_CASE("PvP remote orientation follows shortest angle through wrap") {
    SnapshotTimeline timeline;
    auto a = State(1), b = State(2);
    a.players[0].yaw = 179 * std::numbers::pi_v<float> / 180;
    b.players[0].yaw = -179 * std::numbers::pi_v<float> / 180;
    REQUIRE(timeline.Push(a, At(1)));
    REQUIRE(timeline.Push(b, At(1 + MovementTickSeconds)));
    const auto pose = Presented(timeline, 1, At(1 + 1.5 * MovementTickSeconds));
    REQUIRE(pose);
    CHECK(std::abs(pose->player.yaw) == doctest::Approx(std::numbers::pi_v<float>));
}

TEST_CASE("PvP phase estimator keeps a fixed slope and reanchors only sustained shifts") {
    SnapshotTimeline timeline;
    for (std::uint64_t tick = 1; tick <= 30; ++tick)
        REQUIRE(timeline.Push(State(tick), At(tick * MovementTickSeconds)));
    REQUIRE(Presented(timeline, 1, At(.5)));
    REQUIRE(timeline.Push(State(31), At(31 * MovementTickSeconds + .25)));
    const auto first = Presented(timeline, 1, At(31 * MovementTickSeconds + .25));
    REQUIRE(first);
    CHECK(first->phaseReanchors == 0);
    REQUIRE(timeline.Push(State(32), At(32 * MovementTickSeconds + .25)));
    REQUIRE(timeline.Push(State(33), At(33 * MovementTickSeconds + .25)));
    const auto anchored = Presented(timeline, 1, At(33 * MovementTickSeconds + .25));
    REQUIRE(anchored);
    CHECK(anchored->phaseReanchors == 1);
    CHECK(anchored->presentationTick == doctest::Approx(32));
    CHECK(anchored->presentationTick >= first->presentationTick);
    CHECK_FALSE(timeline.Push(State(32), At(2))); // reordered / duplicate ticks
    CHECK_FALSE(timeline.Push(State(33), At(2)));
}

TEST_CASE("PvP fixed-delay presentation remains bounded under jitter loss and reordered delivery") {
    struct Packet { double arrival; WorldSnapshot snapshot; };
    std::vector<Packet> packets;
    for (std::uint64_t tick = 1; tick <= 600; ++tick) {
        if (tick % 20 == 0 || tick == 301 || tick == 302) continue;
        const double jitter = static_cast<double>((tick * 17) % 21) / 1000 - .01;
        packets.push_back({tick * MovementTickSeconds + .02 + jitter, State(tick)});
        if (tick % 41 == 0) packets.push_back({tick * MovementTickSeconds + .06, State(tick)});
    }
    std::stable_sort(packets.begin(), packets.end(), [](const auto& a, const auto& b) { return a.arrival < b.arrival; });
    for (const int fps : {30, 60, 144}) {
        SnapshotTimeline timeline;
        std::size_t packet = 0;
        std::uint64_t newest{};
        double cursor = 0;
        for (int frame = 1; frame <= fps * 11; ++frame) {
            const double now = static_cast<double>(frame) / fps;
            while (packet < packets.size() && packets[packet].arrival <= now) {
                const auto& incoming = packets[packet++];
                if (timeline.Push(incoming.snapshot, At(incoming.arrival))) newest = incoming.snapshot.tick;
            }
            const auto pose = Presented(timeline, 1, At(now));
            if (!pose) continue;
            CHECK(pose->presentationTick + 1e-6 >= cursor);
            CHECK(pose->presentationTick <= newest + 1e-6);
            CHECK(pose->player.position.x <= newest * .05F + .0001F);
            CHECK(pose->historySize <= 64);
            CHECK(pose->phaseReanchors == 0);
            cursor = pose->presentationTick;
        }
        const auto end = Presented(timeline, 1, At(11));
        REQUIRE(end);
        CHECK(end->gapCount > 2);
        CHECK(end->holdCount > 0);
        CHECK(end->holding);
    }
}

TEST_CASE("PvP skipped presentation selections do not create hold episodes") {
    SnapshotTimeline history;
    REQUIRE(history.Push(State(1), At(1)));
    REQUIRE(history.Push(State(2), At(1 + MovementTickSeconds)));
    const auto skipped = history.Sample(1, At(2));
    REQUIRE(skipped);
    CHECK(skipped->holding);
    CHECK(history.HoldCount() == 0);
    CHECK(history.TotalHoldSeconds() == 0);
    const auto submitted = Presented(history, 1, At(2.1));
    REQUIRE(submitted);
    CHECK(submitted->holdCount == 0);
    CHECK(submitted->holdSeconds == 0);
    REQUIRE(history.Push(State(100), At(2.2)));
    REQUIRE(history.Sample(1, At(2.2))); // a fresh pose selected but not submitted
    CHECK(history.HoldCount() == 0);
    const auto next = Presented(history, 1, At(2.3));
    REQUIRE(next);
    CHECK(next->totalHoldSeconds == 0);
    const auto repeated = Presented(history, 1, At(2.4));
    REQUIRE(repeated);
    CHECK(repeated->holding);
    CHECK(repeated->holdCount == 1);
    CHECK(repeated->totalHoldSeconds == doctest::Approx(.1));
}

TEST_CASE("PvP stationary remote poses expose missing data without counting motion holds") {
    SnapshotTimeline history;
    auto first = State(1), second = State(2);
    second.players[0].position = first.players[0].position;
    REQUIRE(history.Push(first, At(1)));
    REQUIRE(history.Push(second, At(1 + MovementTickSeconds)));
    const auto stationary = Presented(history, 1, At(1.2));
    REQUIRE(stationary);
    CHECK(stationary->missingFutureSnapshot);
    CHECK_FALSE(stationary->holding);
    CHECK(stationary->holdCount == 0);
    const auto later = Presented(history, 1, At(1.5));
    REQUIRE(later);
    CHECK(later->totalHoldSeconds == 0);
    // The other player's changing position cannot mark the stationary one.
    REQUIRE(history.Sample(2, At(1.5)));
    CHECK(history.Sample(2, At(1.5))->holding);
}

TEST_CASE("PvP a confirmed remote stop ends motion holds while missing data stays visible") {
    SnapshotTimeline history;
    REQUIRE(history.Push(State(1), At(1)));
    REQUIRE(history.Push(State(2), At(1 + MovementTickSeconds)));
    REQUIRE(Presented(history, 1, At(1.04)));
    const auto moving = Presented(history, 1, At(1.05));
    REQUIRE(moving);
    REQUIRE(moving->holding);
    auto stop = State(3);
    stop.players[0].position = State(2).players[0].position;
    REQUIRE(history.Push(stop, At(1 + 2 * MovementTickSeconds)));
    const auto stopped = Presented(history, 1, At(1.1));
    REQUIRE(stopped);
    CHECK(stopped->missingFutureSnapshot);
    CHECK_FALSE(stopped->holding);
    CHECK(stopped->holdCount == 1);
    CHECK(stopped->holdSeconds == 0);
    const auto later = Presented(history, 1, At(1.3));
    REQUIRE(later);
    CHECK(later->totalHoldSeconds == stopped->totalHoldSeconds);
}

TEST_CASE("PvP remote turn holds use current epoch motion and ignore equivalent yaw wrap") {
    for (const bool resetEpoch : {false, true}) {
        CAPTURE(resetEpoch);
        SnapshotTimeline history;
        auto first = State(1), second = State(2, resetEpoch ? 2 : 1);
        second.players[0].position = first.players[0].position;
        second.players[0].yaw = .2F;
        REQUIRE(history.Push(first, At(1)));
        REQUIRE(history.Push(second, At(1 + MovementTickSeconds)));
        REQUIRE(Presented(history, 1, At(1.1)));
        const auto pose = Presented(history, 1, At(1.2));
        REQUIRE(pose);
        CHECK(pose->missingFutureSnapshot);
        CHECK(pose->holding == !resetEpoch);
        CHECK(pose->holdCount == (resetEpoch ? 0 : 1));
    }
    SnapshotTimeline history;
    auto first = State(1), second = State(2);
    second.players[0].position = first.players[0].position;
    first.players[0].yaw = std::numbers::pi_v<float>;
    second.players[0].yaw = -std::numbers::pi_v<float>;
    REQUIRE(history.Push(first, At(1)));
    REQUIRE(history.Push(second, At(1 + MovementTickSeconds)));
    const auto pose = Presented(history, 1, At(1.2));
    REQUIRE(pose);
    CHECK_FALSE(pose->holding);
    CHECK(pose->holdCount == 0);
}

TEST_CASE("PvP fresh moving tail poses do not form a visible hold episode") {
    SnapshotTimeline history;
    REQUIRE(history.Push(State(1), At(1)));
    for (std::uint64_t tick = 2; tick <= 30; ++tick) {
        const double received = 1 + (tick - 1) * MovementTickSeconds;
        REQUIRE(history.Push(State(tick), At(received)));
        const auto pose = Presented(history, 1, At(received + .04));
        REQUIRE(pose);
        CHECK(pose->missingFutureSnapshot);
        CHECK_FALSE(pose->holding);
        CHECK(pose->holdCount == 0);
        CHECK(pose->totalHoldSeconds == 0);
    }
    const auto repeated = Presented(history, 1, At(1 + 29 * MovementTickSeconds + .06));
    REQUIRE(repeated);
    CHECK(repeated->holding);
    CHECK(repeated->holdCount == 1);
    CHECK(repeated->holdSeconds == doctest::Approx(.02));
    REQUIRE(history.Push(State(31), At(1.6)));
    const auto advancing = Presented(history, 1, At(1.65));
    REQUIRE(advancing);
    CHECK(advancing->missingFutureSnapshot);
    CHECK_FALSE(advancing->holding);
    CHECK(advancing->holdSeconds == 0);
    CHECK(advancing->totalHoldSeconds == doctest::Approx(.02));
}

TEST_CASE("PvP presented identity changes break repeated pose hold evidence") {
    SnapshotTimeline history;
    PlayerState pose{1, {1, 0, 1}, 0, 0};
    CHECK_FALSE(history.CommitPresented(At(1), &pose, true));
    CHECK(history.CommitPresented(At(1.02), &pose, true));
    ++pose.movementEpoch;
    CHECK_FALSE(history.CommitPresented(At(1.04), &pose, true));
    CHECK(history.HoldSeconds() == 0);
    CHECK(history.CommitPresented(At(1.06), &pose, true));
    ++pose.playerId;
    CHECK_FALSE(history.CommitPresented(At(1.08), &pose, true));
    CHECK(history.HoldSeconds() == 0);
    CHECK_FALSE(history.CommitPresented(At(1.1), nullptr, false));
    CHECK_FALSE(history.CommitPresented(At(1.12), &pose, true));
    CHECK(history.HoldCount() == 2);
    CHECK(history.TotalHoldSeconds() == doctest::Approx(.04));
}
