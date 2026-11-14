// Registrar::RequireExplicitOrder (batch v8-arch-gyo2-strict-order): the
// optional strict mode of the ambiguity box of the base System contract
// (docs/architecture/target/v2/diagrams/control-plane-revised.svg; concept row
// M3b). With it, a conflicting pair with no order path is a startup error
// (D-E8, rule M3b) instead of a tie by registration order; without it, nothing
// changes. The data here declares no zone (zone B), as a product that never
// uses determinism zones would. Every case is named "strict order: ..." so the
// kernel.strict_order ctest runs it; the mutation list of the batch names the
// case each mutant must fail.

#include "KernelTestSupport.hpp"

#include <string>
#include <tuple>
#include <vector>

namespace {

using namespace KernelTest;

struct Pos { std::int32_t x; };
struct Counter { std::int64_t value; };
struct Secret { std::int32_t value; };

SystemResult WriteFirst(Query<Write<Pos>>) { return Ok(); }
SystemResult WriteSecond(Query<Write<Pos>>) { return Ok(); }
SystemResult WriteThird(Query<Write<Pos>>) { return Ok(); }
SystemResult ReadFirst(Query<Read<Pos>>) { return Ok(); }
SystemResult ReadSecond(Query<Read<Pos>>) { return Ok(); }
SystemResult CountFirst(ResMut<Counter>) { return Ok(); }
SystemResult CountSecond(ResMut<Counter>) { return Ok(); }
SystemResult WriteSecretFirst(Query<Write<Secret>>) { return Ok(); }
SystemResult WriteSecretSecond(Query<Write<Secret>>) { return Ok(); }

ScheduleOptions Tick() {
    ScheduleOptions s;
    s.name = "Tick";
    s.domain = Domain::Frame;
    s.clock = true;
    s.stages = {"Simulate", "Present"};
    return s;
}

ScheduleOptions Step() {
    ScheduleOptions s;
    s.name = "Step";
    s.domain = Domain::Fixed;
    s.clock = true;
    s.stages = {"Simulate"};
    return s;
}

// A System with no zone (zone B by default), mounted once.
SystemOptions In(std::string name, std::string schedule, std::string stage, std::vector<std::string> before = {},
                 std::vector<std::string> after = {}, std::vector<std::string> sets = {}) {
    SystemOptions o;
    o.name = std::move(name);
    Mount m = At(std::move(schedule), std::move(stage), std::move(before), std::move(after));
    m.sets = std::move(sets);
    o.mounts.push_back(std::move(m));
    return o;
}

// Shared data (two writers are allowed) and the two schedules, no zones.
void Data(Registrar& r) {
    r.Component<Pos>({.name = "Pos", .writePolicy = WritePolicy::Shared});
    r.Resource<Counter>({.name = "Counter", .writePolicy = WritePolicy::Shared});
    r.Schedule(Tick());
    r.Schedule(Step());
}

// One plugin "game" with the data, the given Systems and, when strict, the call.
Built Game(bool strict, const std::function<void(Registrar&)>& systems) {
    return KernelTest::Finish({{"game", [strict, systems](Registrar& r) {
                                    if (strict) r.RequireExplicitOrder();
                                    Data(r);
                                    systems(r);
                                }}});
}

void TwoWriters(Registrar& r) {
    r.AddSystem<&WriteFirst>(In("WriteFirst", "Tick", "Simulate"));
    r.AddSystem<&WriteSecond>(In("WriteSecond", "Tick", "Simulate"));
}

bool HasText(const std::string& text, std::string_view part) {
    return text.find(part) != std::string::npos;
}

} // namespace

TEST_CASE("strict order: an ambiguous pair is a startup error that names both Systems, the data, the schedule and the fix") {
    Built built = Game(true, TwoWriters);
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built.result), "RequireExplicitOrder had no effect");
    const auto& errors = built.result.error().errors;
    REQUIRE_MESSAGE(errors.size() == 1, Describe(built));
    const StartupError& error = errors.front();
    CHECK(error.Is(StartupCode::DE8));
    CHECK(error.rule == "M3b");
    REQUIRE(error.subjects.size() == 5);
    CHECK(error.subjects[0] == "WriteFirst");
    CHECK(error.subjects[1] == "WriteSecond");
    CHECK(error.subjects[2] == "component 'Pos'");
    CHECK(error.subjects[3] == "Tick");
    CHECK(error.subjects[4] == "game");
    CHECK(error.sources.size() == 2);
    CHECK_MESSAGE(HasText(error.message, "stage Simulate of schedule 'Tick'"), error.message);
    CHECK_MESSAGE(HasText(error.message, "conflict on component 'Pos'"), error.message);
    CHECK_MESSAGE(HasText(error.message, "declare 'WriteFirst' before or after 'WriteSecond'"), error.message);
    CHECK_MESSAGE(HasText(error.message, "RequireExplicitOrder, by 'game'"), error.message);
}

TEST_CASE("strict order: a declared before, after or set order passes and leaves no tie") {
    Built before = Game(true, [](Registrar& r) {
        r.AddSystem<&WriteFirst>(In("WriteFirst", "Tick", "Simulate"));
        r.AddSystem<&WriteSecond>(In("WriteSecond", "Tick", "Simulate", {"WriteFirst"}));
    });
    KERNEL_REQUIRE_OK(before);
    CHECK(before.registry->Frozen().TieBreaks().empty());
    CHECK(SystemSteps(*before.registry->Frozen().PlanOf("Tick")) == std::vector<std::string>{"WriteSecond", "WriteFirst"});

    Built after = Game(true, [](Registrar& r) {
        r.AddSystem<&WriteFirst>(In("WriteFirst", "Tick", "Simulate", {}, {"WriteSecond"}));
        r.AddSystem<&WriteSecond>(In("WriteSecond", "Tick", "Simulate"));
    });
    KERNEL_REQUIRE_OK(after);
    CHECK(after.registry->Frozen().TieBreaks().empty());
    CHECK(SystemSteps(*after.registry->Frozen().PlanOf("Tick")) == std::vector<std::string>{"WriteSecond", "WriteFirst"});

    Built set = Game(true, [](Registrar& r) {
        r.Set({.name = "Late", .schedule = "Tick", .after = {"WriteSecond"}});
        r.AddSystem<&WriteFirst>(In("WriteFirst", "Tick", "Simulate", {}, {}, {"Late"}));
        r.AddSystem<&WriteSecond>(In("WriteSecond", "Tick", "Simulate"));
    });
    KERNEL_REQUIRE_OK(set);
    CHECK(set.registry->Frozen().TieBreaks().empty());
    CHECK(SystemSteps(*set.registry->Frozen().PlanOf("Tick")) == std::vector<std::string>{"WriteSecond", "WriteFirst"});
}

TEST_CASE("strict order: a read-only pair is not ambiguous") {
    Built built = Game(true, [](Registrar& r) {
        r.AddSystem<&ReadFirst>(In("ReadFirst", "Tick", "Simulate"));
        r.AddSystem<&ReadSecond>(In("ReadSecond", "Tick", "Simulate"));
    });
    KERNEL_REQUIRE_OK(built);
    CHECK(built.registry->Frozen().TieBreaks().empty());
}

TEST_CASE("strict order: pairs ordered by a path or by the stage end pass") {
    // WriteFirst before ReadFirst before WriteSecond: the two writers have a path.
    KERNEL_REQUIRE_OK(Game(true, [](Registrar& r) {
        r.AddSystem<&WriteFirst>(In("WriteFirst", "Tick", "Simulate", {"ReadFirst"}));
        r.AddSystem<&ReadFirst>(In("ReadFirst", "Tick", "Simulate", {"WriteSecond"}));
        r.AddSystem<&WriteSecond>(In("WriteSecond", "Tick", "Simulate"));
    }));
    // Different stages are ordered by the stage end.
    KERNEL_REQUIRE_OK(Game(true, [](Registrar& r) {
        r.AddSystem<&WriteFirst>(In("WriteFirst", "Tick", "Simulate"));
        r.AddSystem<&WriteSecond>(In("WriteSecond", "Tick", "Present"));
    }));
}

TEST_CASE("strict order: every ambiguous pair of every schedule is reported at once") {
    Built built = Game(true, [](Registrar& r) {
        r.AddSystem<&WriteFirst>(In("WriteFirst", "Tick", "Simulate"));
        r.AddSystem<&WriteSecond>(In("WriteSecond", "Tick", "Simulate"));
        r.AddSystem<&WriteThird>(In("WriteThird", "Tick", "Simulate"));
        r.AddSystem<&CountFirst>(In("CountFirst", "Step", "Simulate"));
        r.AddSystem<&CountSecond>(In("CountSecond", "Step", "Simulate"));
    });
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built.result), "RequireExplicitOrder had no effect");
    std::vector<std::tuple<std::string, std::string, std::string, std::string>> pairs;
    for (const StartupError& error : built.result.error().errors) {
        CHECK(error.Is(StartupCode::DE8));
        CHECK(error.rule == "M3b");
        REQUIRE(error.subjects.size() >= 4);
        pairs.emplace_back(error.subjects[0], error.subjects[1], error.subjects[2], error.subjects[3]);
    }
    // In schedule order, then registration order: no pair is lost to an earlier one.
    const std::vector<std::tuple<std::string, std::string, std::string, std::string>> expected{
        {"WriteFirst", "WriteSecond", "component 'Pos'", "Tick"},
        {"WriteFirst", "WriteThird", "component 'Pos'", "Tick"},
        {"WriteSecond", "WriteThird", "component 'Pos'", "Tick"},
        {"CountFirst", "CountSecond", "resource 'Counter'", "Step"},
    };
    CHECK_MESSAGE(pairs == expected, Describe(built));
}

TEST_CASE("strict order: without RequireExplicitOrder the same pair is tied by registration and listed") {
    Built built = Game(false, TwoWriters);
    KERNEL_REQUIRE_OK(built);
    const auto ties = built.registry->Frozen().TieBreaks();
    REQUIRE(ties.size() == 1);
    CHECK(ties[0].schedule == "Tick");
    CHECK(ties[0].first == "WriteFirst");
    CHECK(ties[0].second == "WriteSecond");
    CHECK(SystemSteps(*built.registry->Frozen().PlanOf("Tick")) == std::vector<std::string>{"WriteFirst", "WriteSecond"});
}

TEST_CASE("strict order: any plugin turns it on for the whole Registry, and calling it twice is calling it once") {
    Built built = KernelTest::Finish({
        {"game", [](Registrar& r) {
             Data(r);
             TwoWriters(r);
         }},
        {"rules", [](Registrar& r) {
             r.RequireExplicitOrder();
             r.RequireExplicitOrder();
         }},
        {"replay", [](Registrar& r) { r.RequireExplicitOrder(); }},
    });
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built.result), "RequireExplicitOrder from another plugin had no effect");
    const auto& errors = built.result.error().errors;
    REQUIRE_MESSAGE(errors.size() == 1, Describe(built));
    const std::vector<std::string> expected{"WriteFirst", "WriteSecond", "component 'Pos'", "Tick", "rules", "replay"};
    CHECK(errors.front().subjects == expected);
    CHECK_MESSAGE(HasText(errors.front().message, "RequireExplicitOrder, by 'rules', 'replay'"), errors.front().message);
}

TEST_CASE("strict order: a pair with a zone C side stays one D-3 error") {
    Built built = KernelTest::Finish({{"game", [](Registrar& r) {
                                           r.RequireExplicitOrder();
                                           r.Component<Secret>({.name = "Secret", .zone = Zone::C,
                                                                .writePolicy = WritePolicy::Shared});
                                           r.Schedule(Tick());
                                           SystemOptions first = In("WriteSecretFirst", "Tick", "Simulate");
                                           first.zone = Zone::C;
                                           SystemOptions second = In("WriteSecretSecond", "Tick", "Simulate");
                                           second.zone = Zone::C;
                                           r.AddSystem<&WriteSecretFirst>(first);
                                           r.AddSystem<&WriteSecretSecond>(second);
                                       }}});
    REQUIRE_FALSE(static_cast<bool>(built.result));
    const auto& errors = built.result.error().errors;
    REQUIRE_MESSAGE(errors.size() == 1, Describe(built));
    CHECK(errors.front().Is(StartupCode::DE8));
    CHECK(errors.front().rule == "D-3");
}
