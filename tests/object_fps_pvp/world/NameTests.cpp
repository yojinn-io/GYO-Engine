// The players' names (display-name declaration 5.1): the Match gives each player the name its room's roster has for
// its user, and every Client receives it with the player.
#include <doctest/doctest.h>

#include "WorldTestSupport.hpp"

#include <map>
#include <string>

using namespace object_fps_pvp::test;

namespace {

// The name a World has for a user's player; nullopt: no such player, or it has no PlayerName yet.
std::optional<std::string> NameOf(K::World& world, const std::uint64_t user) {
    const K::EntityId player = PlayerOf(world, user);
    if (player == 0) return std::nullopt;
    const PW::PlayerName* name = world.Get<PW::PlayerName>(player);
    if (name == nullptr) return std::nullopt;
    return name->text;
}

} // namespace

TEST_CASE("world: names: each player gets its user's name from the roster at the Match, and every Client receives it") {
    // Users 1 and 2: the roster names 1 only.
    Session session(2, 4, 0, ShippedContent(), {}, 60, std::nullopt, nullptr, 0, {{1, "甲 A"}, {5, "nobody's"}});
    const auto everywhere = [&] {
        for (std::size_t i = 0; i < 2; ++i) {
            if (NameOf(session.Client(i), 1) != std::optional<std::string>{"甲 A"}) return false;
            if (NameOf(session.Client(i), 2) != std::optional<std::string>{""}) return false;
        }
        return true;
    };
    for (int frame = 0; frame < 120 && !everywhere(); ++frame) session.Frame();
    CHECK_MESSAGE(NameOf(session.Match(), 1) == std::optional<std::string>{"甲 A"}, "[pvp-name-match]");
    CHECK_MESSAGE(NameOf(session.Match(), 2) == std::optional<std::string>{""}, "[pvp-name-unlisted]");
    for (std::size_t i = 0; i < 2; ++i) {
        INFO("client " << i + 1);
        CHECK_MESSAGE(NameOf(session.Client(i), 1) == std::optional<std::string>{"甲 A"}, "[pvp-name-replicated]");
        CHECK_MESSAGE(NameOf(session.Client(i), 2) == std::optional<std::string>{""}, "[pvp-name-replicated-empty]");
    }
    // Given once: a later frame keeps it.
    session.Frames(10);
    CHECK(NameOf(session.Client(1), 1) == std::optional<std::string>{"甲 A"});
}

TEST_CASE("world: names: a name longer than the wire takes is not received") {
    Session session(1, 4, 0, ShippedContent(), {}, 60, std::nullopt, nullptr, 0, {{1, std::string(129, 'x')}});
    for (int frame = 0; frame < 120 && PlayerOf(session.Client(0), 1) == 0; ++frame) session.Frame();
    session.Frames(30);
    CHECK(NameOf(session.Match(), 1) == std::optional<std::string>{std::string(129, 'x')});
    CHECK_MESSAGE(NameOf(session.Client(0), 1) != std::optional<std::string>{std::string(129, 'x')}, "[pvp-name-codec-limit]");
}
