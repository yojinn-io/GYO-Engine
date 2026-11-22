// The Modules' buffer of a World build (v8-arch-gyo2-authority-rollback step 3;
// declaration section 6, the user's decision (a)): the owner Creates, the others
// and the product's own build Find; Build writes, Finish and Check read; frozen
// after Build; one owner per type; kept with the build's prepare functions.
// Each CHECK carries a tag in brackets ("[or-buffer-...]"): the step's mutants
// expect it (docs/architecture/target/v2/scripts/orchestration_mutations.json).

#include "OrchestrationTestSupport.hpp"
#include "AssertTestSupport.hpp"

#include <string>
#include <vector>

using namespace GYO::Test::Orchestration;

namespace {

// A Module's registration table, as the rollback Module's will be.
struct Table {
    std::vector<std::string> entries;
};

std::string Joined(const std::vector<std::string>& lines) {
    std::string out;
    for (const std::string& line : lines) out += (out.empty() ? "" : " ") + line;
    return out;
}

O::ProductDescription Game(std::function<void(O::WorldSetup&)> build = {}) {
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main", [build](O::WorldSetup& s) {
        Basics(s.Registrar(), {FrameClock("Tick")});
        if (build) build(s);
    }, {"Tick"}));
    return d;
}

} // namespace

TEST_CASE("module-buffer: the owner creates, a later unit and the product's build write; Finish, Check and prepare read") {
    std::string finished;
    std::string checked;
    std::string prepared;
    Units units;
    units.Module("owner", {.build = [&prepared](O::WorldSetup& s) {
                     Table& table = s.Buffer().Create<Table>();
                     table.entries.push_back("owner");
                     // A prepare function reads what the build put in: the buffer is kept with it.
                     s.Prepare([&prepared, &table](K::World&) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
                         prepared = Joined(table.entries);
                         return {};
                     });
                 }});
    units.Module("user",
                 {.build = [](O::WorldSetup& s) {
                      Table* table = s.Buffer().Find<Table>();
                      REQUIRE_MESSAGE(table != nullptr, "[or-buffer-find] the owner's table is not there");
                      table->entries.push_back("user");
                  },
                  .finish = [&finished](const K::FrozenRegistry&, O::WorldFinish& c) {
                      const Table* table = c.Buffer().Find<Table>();
                      finished = table == nullptr ? "none" : Joined(table->entries);
                  },
                  .check = [&checked](const K::FrozenRegistry&, O::WorldCheck& c) {
                      const Table* table = c.Buffer().Find<Table>();
                      checked = table == nullptr ? "none" : Joined(table->entries);
                      CHECK_MESSAGE(c.Buffer().Frozen(), "[or-buffer-frozen] Check sees an open buffer");
                  }},
                 {"owner"});
    auto built = O::Product::Build(Game([](O::WorldSetup& s) {
                                       Table* table = s.Buffer().Find<Table>();
                                       REQUIRE(table != nullptr);
                                       table->entries.push_back("product");
                                   }),
                                   units.Take());
    ORCH_REQUIRE_BUILT(built);
    CHECK_MESSAGE(finished == "owner user product", "[or-buffer-finish] " << finished);
    CHECK_MESSAGE(checked == "owner user product", "[or-buffer-check] " << checked);
    CHECK_MESSAGE(prepared == "owner user product", "[or-buffer-prepare] " << prepared);
}

TEST_CASE("module-buffer: Find of a type nobody created is nullptr, and makes nothing") {
    bool absentInBuild = false;
    bool absentInCheck = false;
    Units units;
    units.Module("reader", {.build = [&absentInBuild](O::WorldSetup& s) { absentInBuild = s.Buffer().Find<Table>() == nullptr; },
                            .check = [&absentInCheck](const K::FrozenRegistry&, O::WorldCheck& c) {
                                absentInCheck = c.Buffer().Find<Table>() == nullptr;
                            }});
    auto built = O::Product::Build(Game(), units.Take());
    ORCH_REQUIRE_BUILT(built);
    CHECK_MESSAGE(absentInBuild, "[or-buffer-absent] Find made or found a Table in Build");
    CHECK_MESSAGE(absentInCheck, "[or-buffer-absent] Find made a Table: Check sees one");
}

TEST_CASE("module-buffer: two units creating one type is a Programmer Error") {
    Units units;
    units.Module("first", {.build = [](O::WorldSetup& s) { s.Buffer().Create<Table>(); }});
    units.Module("second", {.build = [](O::WorldSetup& s) { s.Buffer().Create<Table>(); }});
    GYO_CHECK_ASSERTS(O::Product::Build(Game(), units.Take()));
}

TEST_CASE("module-buffer: a Create or a writable Find after Build is a Programmer Error") {
    SUBCASE("Create from a prepare function") {
        Units units;
        units.Module("late", {.build = [](O::WorldSetup& s) {
                         O::ModuleBuffer* buffer = &s.Buffer();
                         s.Prepare([buffer](K::World&) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
                             buffer->Create<Table>();
                             return {};
                         });
                     }});
        GYO_CHECK_ASSERTS(O::Product::Build(Game(), units.Take()));
    }
    SUBCASE("a writable Find from a prepare function") {
        Units units;
        units.Module("late", {.build = [](O::WorldSetup& s) {
                         s.Buffer().Create<Table>();
                         O::ModuleBuffer* buffer = &s.Buffer();
                         s.Prepare([buffer](K::World&) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
                             static_cast<void>(buffer->Find<Table>());
                             return {};
                         });
                     }});
        GYO_CHECK_ASSERTS(O::Product::Build(Game(), units.Take()));
    }
}

TEST_CASE("module-buffer: each build of the World has a buffer of its own") {
    int created = 0;
    bool emptyAtStart = true;
    Units units;
    units.Module("owner", {.build = [&created, &emptyAtStart](O::WorldSetup& s) {
                     emptyAtStart = emptyAtStart && s.Buffer().Find<Table>() == nullptr;
                     s.Buffer().Create<Table>();
                     ++created;
                 }});
    auto built = O::Product::Build(Game(), units.Take());
    ORCH_REQUIRE_BUILT(built);
    auto replaced = (*built)->ReplaceWorld("Main", O::ReplaceRequest{});
    REQUIRE_MESSAGE(static_cast<bool>(replaced), (replaced ? std::string() : replaced.error().message));
    CHECK_MESSAGE(created == 2, "[or-buffer-per-build] " << created);
    CHECK_MESSAGE(emptyAtStart, "[or-buffer-per-build] the second build found the first build's Table");
}
