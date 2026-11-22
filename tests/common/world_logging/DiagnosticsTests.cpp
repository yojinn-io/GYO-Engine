// Entrance ② (logging declaration 5.4): the runner takes the Kernel's own diagnostics records between two
// Runs; each becomes one Warning record of category "kernel" with the World's name, and an interrupted
// World's records are taken too, marked world_interrupted (question 2 (c), approved).
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <cstdint>
#include <string>

#include "WorldLoggingTestSupport.hpp"
#include "engine/world_logging/TakeKernelDiagnostics.hpp"

namespace WL = GYO::Engine::WorldLogging;
using namespace GYO::Engine::WorldLogging::Test;

namespace {

struct Note final {
    std::int32_t value = 0;
};
struct Body final {
    std::int32_t m = 0;
};
struct FailNow final {
    std::int32_t on = 0;
};

K::SystemResult SendFiveNotes(K::MessageWriter<Note> notes) {
    for (std::int32_t i = 0; i < 5; ++i) notes.Send(Note{i});
    return {};
}
K::SystemResult ReadNotes(K::MessageReader<Note> notes) {
    static_cast<void>(notes.Read());
    return {};
}
K::SystemResult FailIfAsked(K::Res<FailNow> fail) {
    if (fail->on != 0) return ::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "asked to fail"));
    return {};
}
K::SystemResult SpawnSevenTwice(K::SpawnWithId<K::Bundle<Body>> spawn) {
    spawn.New(7, Body{71});
    spawn.New(7, Body{72});
    return {};
}

O::ProductDescription NotesProduct(const bool spawnTwice) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Simulation";
    w.build = [spawnTwice](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.Resource<FailNow>({.name = "FailNow"});
        r.Message<Note>({.name = "Note", .capacity = 2});
        r.AddSystem<&SendFiveNotes>(At("SendFiveNotes", "Main", "Update"));
        r.AddSystem<&ReadNotes>(At("ReadNotes", "Main", "PostUpdate"));
        r.AddSystem<&FailIfAsked>(At("FailIfAsked", "Main", "Last"));
        if (spawnTwice) {
            r.Component<Body>({.name = "Body"});
            r.AddSystem<&SpawnSevenTwice>(At("SpawnSevenTwice", "Main", "Update"));
        }
    };
    w.prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(FailNow{});
        return {};
    });
    d.worlds.push_back(std::move(w));
    return d;
}

} // namespace

TEST_CASE("diagnostics: dropped messages become one Warning record of category kernel; taking empties them") {
    Logged logged;
    auto product = Make(NotesProduct(false));
    Frame(*product);
    K::World& world = product->World("Simulation").GetWorld();
    CHECK(WL::TakeKernelDiagnostics(world) == 1);
    const auto records = logged.Category("kernel");
    REQUIRE(records.size() == 1);
    const auto& record = records[0];
    CHECK(record.level == L::Level::Warning);
    CHECK(record.world == "Simulation");
    CHECK_FALSE(record.worldInterrupted);
    REQUIRE(record.Find("message") != nullptr);
    CHECK(record.Find("message")->text == "Note");
    REQUIRE(record.Find("count") != nullptr);
    CHECK(record.Find("count")->uintValue == 3);
    REQUIRE(record.Find("first_seq") != nullptr);
    REQUIRE(record.Find("last_seq") != nullptr);
    CHECK(record.Find("last_seq")->uintValue - record.Find("first_seq")->uintValue == 2);
    CHECK(record.text.find("Note") != std::string::npos);
    // ReadNotes had never run when the first frame dropped (a reader that never ran holds nothing).
    REQUIRE(record.Find("readers") != nullptr);
    CHECK(record.Find("readers")->text.empty());
    CHECK(WL::TakeKernelDiagnostics(world) == 0);
    CHECK(logged.Category("kernel").size() == 1);
    // The second frame drops records ReadNotes, which has run now, had not read.
    Frame(*product);
    CHECK(WL::TakeKernelDiagnostics(world) == 1);
    const auto second = logged.Category("kernel");
    REQUIRE(second.size() == 2);
    CHECK(second[1].Find("readers")->text == "ReadNotes@Main");
}

TEST_CASE("diagnostics: a rejected spawn becomes a Warning record with its reason, entity and emitter") {
    Logged logged;
    auto product = Make(NotesProduct(true));
    Frame(*product);
    K::World& world = product->World("Simulation").GetWorld();
    CHECK(WL::TakeKernelDiagnostics(world) >= 2);
    bool found = false;
    for (const auto& record : logged.Category("kernel")) {
        const auto* reason = record.Find("reason");
        if (reason == nullptr) continue;
        found = true;
        CHECK(reason->text == "IdSpawnedTwice");
        CHECK(record.Find("entity")->uintValue == 7);
        CHECK(record.Find("emitter")->text == "SpawnSevenTwice");
        CHECK(record.Find("sync_epoch") != nullptr);
        CHECK(record.world == "Simulation");
    }
    CHECK(found);
}

TEST_CASE("diagnostics: an interrupted World's records are taken too, marked world_interrupted") {
    Logged logged;
    auto product = Make(NotesProduct(false));
    product->World("Simulation").GetWorld().SetResource(FailNow{1});
    auto ran = product->RunFrame();
    REQUIRE_FALSE(ran);
    K::World& world = product->World("Simulation").GetWorld();
    REQUIRE(world.State() == K::WorldState::Interrupted);
    CHECK(WL::TakeKernelDiagnostics(world) == 1);
    const auto records = logged.Category("kernel");
    REQUIRE(records.size() == 1);
    CHECK(records[0].worldInterrupted);
    CHECK(records[0].world == "Simulation");
}
