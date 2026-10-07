#include <doctest/doctest.h>

#include "gui_room.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
using fps::pvp::ClientConnectionState;
using fps::pvp::ConnectionPhase;
using fps::pvp::PlayerId;

ClientConnectionState Room(PlayerId self, std::initializer_list<PlayerId> ids) {
    ClientConnectionState state;
    state.phase = ConnectionPhase::Playing;
    state.playerId = self;
    state.snapshot.emplace();
    for (const auto id : ids) {
        state.snapshot->players.push_back({});
        state.snapshot->players.back().playerId = id;
        state.snapshot->combat.push_back({});
        state.snapshot->combat.back().playerId = id;
    }
    return state;
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("gyo-gui-room-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() { std::filesystem::create_directories(path); }
    ~TemporaryDirectory() { std::filesystem::remove_all(path); }
};
} // namespace

TEST_CASE("A two-GUI room is full exactly when the snapshot holds two players, with no files") {
    TemporaryDirectory directory;
    GuiRoom room(directory.path, "create", 2);
    for (const auto& state : {Room(1, {1}), Room(1, {1, 2}), Room(1, {2, 1}), Room(1, {1, 2, 3}), Room(1, {1, 2, 3, 4})}) {
        auto copy = state;
        CHECK_FALSE(room.Observe(copy));
        CHECK(room.Full(copy) == (copy.snapshot->players.size() == 2));
    }
    CHECK(room.Peer(Room(1, {1, 2})) == 2);
    CHECK(room.Peer(Room(2, {1, 2})) == 1);
    CHECK(std::filesystem::is_empty(directory.path));
}

TEST_CASE("A room with bots finds the other GUI by its published ID and waits for parked bots") {
    TemporaryDirectory directory;
    GuiRoom create(directory.path, "create", 4), join(directory.path, "join", 4);
    auto atCreate = Room(1, {1, 2, 7, 8}), atJoin = Room(2, {1, 2, 7, 8});
    CHECK_FALSE(create.Observe(atCreate));
    CHECK_FALSE(create.Full(atCreate));
    CHECK(join.Observe(atJoin) == 1);
    CHECK(create.Observe(atCreate) == 2);
    // Every player is present, but the bots have not parked yet.
    CHECK_FALSE(create.Full(atCreate));
    std::ofstream(directory.path / GuiRoom::BotsParkedFile) << "parked\n";
    CHECK(create.Observe(atCreate) == 2);
    CHECK(join.Observe(atJoin) == 1);
    CHECK(create.Full(atCreate));
    CHECK(join.Full(atJoin));
    CHECK_FALSE(create.Full(Room(1, {1, 2, 7})));
    // The peer leaves and rejoins under a new ID: it republishes and is found again.
    auto rejoined = Room(9, {1, 7, 8, 9});
    CHECK(join.Observe(rejoined) == 1);
    auto afterRejoin = Room(1, {1, 7, 8, 9});
    CHECK(create.Observe(afterRejoin) == 9);
    CHECK(create.Full(afterRejoin));
    CHECK(create.Peer(Room(1, {1, 7, 8})) == std::nullopt);
}

TEST_CASE("GUI participants are the two GUI entries in snapshot order") {
    const auto state = Room(1, {7, 2, 1, 8});
    const auto pair = GuiParticipants(state.snapshot->players, 1, 2);
    REQUIRE(pair);
    CHECK((*pair)[0].playerId == 2);
    CHECK((*pair)[1].playerId == 1);
    CHECK_FALSE(GuiParticipants(state.snapshot->combat, 1, 5));
    CHECK_FALSE(GuiParticipants(state.snapshot->combat, 1, 1));
    const auto two = Room(1, {1, 2});
    const auto same = GuiParticipants(two.snapshot->combat, 1, 2);
    REQUIRE(same);
    CHECK((*same)[0].playerId == 1);
    CHECK((*same)[1].playerId == 2);
}

TEST_CASE("The room size is checked") {
    CHECK_THROWS(GuiRoom(".", "create", 1));
    CHECK_THROWS(GuiRoom(".", "create", fps::pvp::MaxPlayers + 1));
    CHECK_THROWS(GuiRoom(".", "observer", 2));
}
