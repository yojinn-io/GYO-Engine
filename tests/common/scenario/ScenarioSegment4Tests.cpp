// Scenario test segment 4 (trace declaration docs/architecture/target/v2/trace-declaration.zh-Hant.md, section 9;
// judgements 2 and 4): a game of two Worlds, Lobby and Match, with the default schedule skeleton (Main). Match
// moves a racer each frame; at frame 3 its System breaks by (1) an Err, (2) an exception, (3) a failed
// GYO_ASSERT. The trace Module writes spans and paths as logging records into a MemorySink.
//
//   S4-1  an Err: the spans RunFrame > Main@Match > MoveRacer@Match are linked by parent, MoveRacer carries the
//         error, and one path record names the whole path (D4 (i)).
//   S4-2  an exception: the spans it unwinds carry exception; one path record, from the deepest span.
//   S4-3  a failed GYO_ASSERT in the System: the logging Module's one Fatal record carries trace_path (D4 (a)).
//   S4-4  trace on or off leaves the game's own state the same, byte for byte (TR-2, judgement 4).

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
// MSVC: doctest messages stream std::string_view; that needs the full ostream.
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "AssertTestSupport.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/trace/Trace.hpp"

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace L = GYO::Engine::Logging;
namespace T = GYO::Engine::Trace;

// ---- the game

struct RacerState final {
    std::int64_t x = 0;
    std::int64_t frames = 0;
};
struct LobbyFrames final {
    std::int64_t value = 0;
};
// How and at which frame Match breaks (0: never).
struct Breaker final {
    std::int32_t frame = 0;
    std::int32_t how = 0; // 1: Err, 2: throws, 3: a failed GYO_ASSERT
};

K::SystemResult MoveRacer(K::ResMut<RacerState> racer, K::Res<Breaker> breaker) {
    racer->x += 3;
    racer->frames += 1;
    const auto frame = static_cast<std::int32_t>(racer->frames);
    if (breaker->frame == frame && breaker->how == 1) {
        return ::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "MoveRacer: the racer fell"));
    }
    if (breaker->frame == frame && breaker->how == 2) throw std::runtime_error("MoveRacer threw");
    if (breaker->frame == frame && breaker->how == 3) GYO_ASSERT(racer->x < 0 && "MoveRacer: the track ended");
    return {};
}
K::SystemResult CountLobby(K::ResMut<LobbyFrames> frames) {
    frames->value += 1;
    return {};
}

K::SystemOptions Update(std::string name) {
    K::SystemOptions o;
    o.name = std::move(name);
    o.zone = K::Zone::B;
    K::Mount m;
    m.schedule = "Main";
    m.stage = "Update";
    o.mounts.push_back(std::move(m));
    return o;
}

std::unique_ptr<O::Product> Make(Breaker breaker) {
    O::ProductDescription d;
    d.name = "relay";
    O::WorldDescription lobby;
    lobby.name = "Lobby";
    lobby.build = [](O::WorldSetup& s) {
        s.Registrar().Resource<LobbyFrames>({.name = "LobbyFrames", .zone = K::Zone::B});
        s.Registrar().AddSystem<&CountLobby>(Update("CountLobby"));
    };
    lobby.prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(LobbyFrames{});
        return {};
    });
    d.worlds.push_back(std::move(lobby));
    O::WorldDescription match;
    match.name = "Match";
    match.build = [](O::WorldSetup& s) {
        s.Registrar().Resource<RacerState>({.name = "RacerState", .zone = K::Zone::B});
        s.Registrar().Resource<Breaker>({.name = "Breaker", .zone = K::Zone::B});
        s.Registrar().AddSystem<&MoveRacer>(Update("MoveRacer"));
    };
    match.prepare.push_back([breaker](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(RacerState{});
        world.SetResource(breaker);
        return {};
    });
    d.worlds.push_back(std::move(match));
    auto built = O::Product::Build(std::move(d), O::RegisteredUnits{});
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

// ---- the trace side

// A Logger with a memory sink taking every span. and the assertion handler (when asked), and Trace.
struct Traced {
    std::shared_ptr<L::MemorySink> memory = std::make_shared<L::MemorySink>();
    std::optional<L::LoggerHandle> logger;
    std::optional<T::TraceHandle> trace;

    explicit Traced(bool takeAssertions = false) {
        L::LoggerOptions options;
        options.sinks.push_back({.name = "memory", .sink = memory, .filter = {.include = {"span.", "assert"}, .minimum = L::Level::Trace}});
        options.takeAssertions = takeAssertions;
        options.takeTerminate = false;
        auto installed = L::Install(std::move(options));
        REQUIRE_MESSAGE(static_cast<bool>(installed), "S4: Install of the Logger");
        logger.emplace(std::move(installed.value()));
        trace.emplace(T::Install({}));
    }

    [[nodiscard]] std::vector<L::StoredRecord> Category(std::string_view category) const {
        std::vector<L::StoredRecord> out;
        for (auto& record : memory->Records()) {
            if (record.category == category) out.push_back(std::move(record));
        }
        return out;
    }
    // The last span record of this name in this World.
    [[nodiscard]] std::optional<L::StoredRecord> Span(std::string_view category, std::string_view name, std::string_view world) const {
        std::optional<L::StoredRecord> found;
        for (auto& record : memory->Records()) {
            if (record.category == category && record.text == name && record.world == world) found = record;
        }
        return found;
    }
};

std::uint64_t Id(const L::StoredRecord& record, std::string_view key) {
    const auto* field = record.Find(key);
    return field != nullptr ? field->uintValue : ~0ull;
}
std::string Text(const L::StoredRecord& record, std::string_view key) {
    const auto* field = record.Find(key);
    return field != nullptr ? field->text : std::string("<none>");
}
bool Flag(const L::StoredRecord& record, std::string_view key) {
    const auto* field = record.Find(key);
    return field != nullptr && field->boolValue;
}

void Frames(O::Product& product, int frames) {
    for (int i = 0; i < frames; ++i) REQUIRE(static_cast<bool>(product.RunFrame()));
}

} // namespace

TEST_CASE("scenario segment 4: S4-1 an Err at frame 3: the path RunFrame > Main@Match > MoveRacer@Match, the error on MoveRacer") {
    auto product = Make({3, 1});
    Traced traced;
    Frames(*product, 2);
    REQUIRE_FALSE(static_cast<bool>(product->RunFrame()));
    const auto system = traced.Span("span.kernel", "MoveRacer", "Match");
    const auto schedule = traced.Span("span.kernel", "Main", "Match");
    const auto frame = traced.Span("span.orchestration", "RunFrame", "");
    REQUIRE(system.has_value());
    REQUIRE(schedule.has_value());
    REQUIRE(frame.has_value());
    CHECK_MESSAGE(Id(*system, "parent_id") == Id(*schedule, "span_id"), "S4-1: MoveRacer's parent is Main (TR-4)");
    CHECK_MESSAGE(Id(*schedule, "parent_id") == Id(*frame, "span_id"), "S4-1: Main's parent is RunFrame (TR-4)");
    CHECK_MESSAGE(Text(*system, "error").find("MoveRacer: the racer fell") != std::string::npos, "S4-1: the error is on MoveRacer (D4)");
    const auto paths = traced.Category("span.path");
    REQUIRE_MESSAGE(paths.size() == 1, "S4-1: one path record (D4 (i))");
    CHECK(Text(paths[0], "path") == "orchestration:RunFrame > kernel:Main@Match > kernel:MoveRacer@Match");
    CHECK(Id(paths[0], "span_id") == Id(*system, "span_id"));
}

TEST_CASE("scenario segment 4: S4-2 an exception at frame 3: the unwound spans say so; one path from the deepest") {
    auto product = Make({3, 2});
    Traced traced;
    Frames(*product, 2);
    CHECK_THROWS_AS(static_cast<void>(product->RunFrame()), std::runtime_error);
    const auto system = traced.Span("span.kernel", "MoveRacer", "Match");
    const auto frame = traced.Span("span.orchestration", "RunFrame", "");
    REQUIRE(system.has_value());
    REQUIRE(frame.has_value());
    CHECK_MESSAGE(Flag(*system, "exception"), "S4-2: MoveRacer was unwound by the exception");
    CHECK_MESSAGE(Flag(*frame, "exception"), "S4-2: RunFrame was unwound by the exception");
    const auto paths = traced.Category("span.path");
    REQUIRE_MESSAGE(paths.size() == 1, "S4-2: one path record, from the first span the exception unwound (D4 (i))");
    CHECK(Text(paths[0], "path") == "orchestration:RunFrame > kernel:Main@Match > kernel:MoveRacer@Match");
}

TEST_CASE("scenario segment 4: S4-3 a failed GYO_ASSERT in the System: the one Fatal record carries trace_path") {
    auto product = Make({3, 3});
    // The test's handler throws AssertionFailure; the Logger goes on to it after writing the Fatal record.
    ::Engine::Test::ScopedAssertionHandler throwing;
    Traced traced(true);
    Frames(*product, 2);
    bool asserted = false;
    try {
        static_cast<void>(product->RunFrame());
    } catch (const ::Engine::Base::AssertionFailure&) {
        asserted = true;
    }
    REQUIRE(asserted);
    std::vector<L::StoredRecord> fatal;
    for (auto& record : traced.memory->Records()) {
        if (record.level == L::Level::Fatal) fatal.push_back(std::move(record));
    }
    REQUIRE_MESSAGE(fatal.size() == 1, "S4-3: exactly one Fatal record (logging LG-2)");
    CHECK(fatal[0].category == "assert");
    CHECK_MESSAGE(Text(fatal[0], "trace_path") == "orchestration:RunFrame > kernel:Main@Match > kernel:MoveRacer@Match",
                  "S4-3: the Fatal record carries the path at the moment of the assertion (D4 (a))");
}

TEST_CASE("scenario segment 4: S4-4 trace on or off leaves the game's own state the same") {
    auto run = [](bool trace) {
        auto product = Make({});
        std::optional<Traced> traced;
        if (trace) traced.emplace();
        Frames(*product, 5);
        RacerState racer = *product->World("Match").GetWorld().Resource<RacerState>();
        LobbyFrames lobby = *product->World("Lobby").GetWorld().Resource<LobbyFrames>();
        if (trace) CHECK_MESSAGE(!traced->Category("span.kernel").empty(), "S4-4: the traced run wrote spans");
        return std::pair{racer, lobby};
    };
    const auto off = run(false);
    const auto on = run(true);
    CHECK_MESSAGE(std::memcmp(&off.first, &on.first, sizeof(RacerState)) == 0, "S4-4: RacerState is the same (TR-2)");
    CHECK_MESSAGE(std::memcmp(&off.second, &on.second, sizeof(LobbyFrames)) == 0, "S4-4: LobbyFrames is the same (TR-2)");
}
