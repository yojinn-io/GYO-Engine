// Entrance ① (logging declaration 5.3; Design Invariants LG-1 and LG-3): Forward turns a retained Message
// of a healthy World into records, with the World's name, and never fails or writes the World.
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "AssertTestSupport.hpp"
#include "WorldLoggingTestSupport.hpp"
#include "engine/world_logging/Forward.hpp"
#include "engine/world_time/Sampling.hpp"
#include "engine/world_time/TimeDiscardedLog.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

namespace WL = GYO::Engine::WorldLogging;
namespace W = GYO::Engine::WorldTime;
using namespace GYO::Engine::WorldLogging::Test;

namespace {

struct Ping final {
    std::int32_t value = 0;
    std::int32_t throwInDescribe = 0;
};

struct PingLog final {
    using Message = Ping;
    static constexpr std::string_view name = "Ping";
    static constexpr std::string_view category = "test.ping";
    static constexpr L::Level level = L::Level::Info;
    static void Describe(const Ping& message, L::RecordBuilder& out) {
        if (message.throwInDescribe != 0) throw std::runtime_error("describe failed");
        out.Text("ping " + std::to_string(message.value));
        out.Add("value", message.value);
    }
};

// What the product sends this frame, and what the game's own reader read.
struct Outbox final {
    std::int32_t first = 0;
    std::int32_t count = 0;
    std::int32_t throwAt = -1;
};
std::vector<std::int32_t>& GameRead() {
    static std::vector<std::int32_t> read;
    return read;
}

K::SystemResult SendPings(K::Res<Outbox> outbox, K::MessageWriter<Ping> pings) {
    for (std::int32_t i = 0; i < outbox->count; ++i) {
        pings.Send(Ping{outbox->first + i, outbox->throwAt == outbox->first + i ? 1 : 0});
    }
    return {};
}
K::SystemResult GameReadsPings(K::MessageReader<Ping> pings) {
    for (const Ping* ping : pings.Read()) GameRead().push_back(ping->value);
    return {};
}

void PingWorld(O::WorldSetup& s, const bool forward, const std::uint32_t capacity = 1024) {
    K::Registrar& r = s.Registrar();
    r.Resource<Outbox>({.name = "Outbox"});
    r.Message<Ping>({.name = "Ping", .capacity = capacity});
    r.AddSystem<&SendPings>(At("SendPings", "Main", "Update"));
    r.AddSystem<&GameReadsPings>(At("GameReadsPings", "Main", "PostUpdate"));
    if (forward) WL::Forward<PingLog>(s, {.schedule = "Main", .stage = "Last"});
}

O::ProductDescription PingProduct(const std::string& worldName, const bool forward) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = worldName;
    w.build = [forward](O::WorldSetup& s) { PingWorld(s, forward); };
    w.prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(Outbox{});
        return {};
    });
    d.worlds.push_back(std::move(w));
    return d;
}

void Send(O::Product& p, const std::string& world, std::int32_t first, std::int32_t count, std::int32_t throwAt = -1) {
    p.World(world).GetWorld().SetResource(Outbox{first, count, throwAt});
}

} // namespace

TEST_CASE("forward: each message becomes one record with the descriptor's level, category, text, fields and the World") {
    Logged logged;
    auto product = Make(PingProduct("Simulation", true));
    Send(*product, "Simulation", 10, 3);
    Frame(*product);
    const auto records = logged.Category("test.ping");
    REQUIRE(records.size() == 3);
    for (std::int32_t i = 0; i < 3; ++i) {
        const auto& record = records[static_cast<std::size_t>(i)];
        CHECK(record.level == L::Level::Info);
        CHECK(record.text == "ping " + std::to_string(10 + i));
        CHECK(record.world == "Simulation");
        CHECK_FALSE(record.worldInterrupted);
        REQUIRE(record.fields.size() == 1);
        CHECK(record.fields[0].key == "value");
        CHECK(record.fields[0].intValue == 10 + i);
    }
    // The next frame logs only that frame's messages.
    Send(*product, "Simulation", 20, 1);
    Frame(*product);
    CHECK(logged.Category("test.ping").size() == 4);
}

TEST_CASE("forward: with no sink taking the descriptor's level the cursor still moves; later records are new ones only") {
    Logged logged(L::Level::Warning);
    auto product = Make(PingProduct("Simulation", true));
    Send(*product, "Simulation", 1, 2);
    Frame(*product);
    CHECK(logged.Category("test.ping").empty());
    L::SetLevel("memory", "", L::Level::Info);
    Send(*product, "Simulation", 5, 1);
    Frame(*product);
    const auto records = logged.Category("test.ping");
    REQUIRE(records.size() == 1);
    CHECK(records[0].text == "ping 5");
}

TEST_CASE("forward: a descriptor that throws gives one Error record of category logging; the frame and the World go on") {
    Logged logged;
    const std::uint64_t failedBefore = WL::FailedDescriptions();
    auto product = Make(PingProduct("Simulation", true));
    Send(*product, "Simulation", 1, 3, 2);
    Frame(*product);
    CHECK(product->World("Simulation").GetWorld().State() == K::WorldState::Ready);
    const auto pings = logged.Category("test.ping");
    REQUIRE(pings.size() == 2);
    CHECK(pings[0].text == "ping 1");
    CHECK(pings[1].text == "ping 3");
    const auto errors = logged.Category("logging");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].level == L::Level::Error);
    CHECK(errors[0].world == "Simulation");
    CHECK(errors[0].text.find("Ping") != std::string::npos);
    CHECK(WL::FailedDescriptions() == failedBefore + 1);
}

TEST_CASE("forward: Forward of one descriptor twice in a World is a startup error") {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Simulation";
    w.build = [](O::WorldSetup& s) {
        PingWorld(s, true);
        WL::Forward<PingLog>(s, {.schedule = "Main", .stage = "Last"});
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), {});
    CHECK_FALSE(built);
}

TEST_CASE("forward: a World name over 63 bytes, or an empty mount, is a Programmer Error") {
    const std::string longName(64, 'w');
    GYO_CHECK_ASSERTS(O::Product::Build(PingProduct(longName, true), {}));
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Simulation";
    w.build = [](O::WorldSetup& s) {
        PingWorld(s, false);
        WL::Forward<PingLog>(s, {.schedule = "Main", .stage = ""});
    };
    d.worlds.push_back(std::move(w));
    GYO_CHECK_ASSERTS(O::Product::Build(std::move(d), {}));
}

TEST_CASE("forward: the game's own reader reads the same messages with or without Forward (declaration 5.11)") {
    std::vector<std::vector<std::int32_t>> runs;
    for (const bool forward : {false, true}) {
        Logged logged;
        GameRead().clear();
        auto product = Make(PingProduct("Simulation", forward));
        for (std::int32_t frame = 0; frame < 4; ++frame) {
            Send(*product, "Simulation", frame * 10, frame + 1);
            Frame(*product);
        }
        runs.push_back(GameRead());
        CHECK(logged.Category("test.ping").size() == (forward ? 10u : 0u));
    }
    CHECK(runs[0] == runs[1]);
}

TEST_CASE("forward: TimeDiscardedLog: the time Module's message is logged with its amount, real delta and cause") {
    Logged logged;
    O::RegisteredUnits units;
    O::RegisterRow row;
    row.name = "world_time";
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    units.rows.push_back(row);
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Simulation";
    w.build = [](O::WorldSetup& s) { WL::Forward<W::TimeDiscardedLog>(s, {.schedule = "Main", .stage = "Last"}); };
    // One real second in one frame: more fixed steps than a frame may run, so time is discarded.
    w.prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::TimeSampling{1'000'000'000, W::SamplingMode::ManualDelta, {}});
        return {};
    });
    d.worlds.push_back(std::move(w));
    auto product = Make(std::move(d), std::move(units));
    Frame(*product);
    const auto records = logged.Category("world_time");
    REQUIRE(records.size() == 1);
    CHECK(records[0].level == L::Level::Debug);
    CHECK(records[0].world == "Simulation");
    const auto* amount = records[0].Find("amount_ns");
    const auto* real = records[0].Find("real_delta_ns");
    const auto* cause = records[0].Find("cause");
    REQUIRE(amount != nullptr);
    REQUIRE(real != nullptr);
    REQUIRE(cause != nullptr);
    CHECK(amount->intValue > 0);
    CHECK(real->intValue == 1'000'000'000);
    CHECK(cause->text == "StepLimit");
    CHECK(records[0].text.find("StepLimit") != std::string::npos);
}
