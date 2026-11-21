// The official rollback strategy (v8-arch-gyo2-authority-rollback step 4;
// rollback declaration 5.5 and R-2, R-4, R-5; authority-rollback declaration
// 5.2, 5.3 and the entry shape of 5.5). A World on the default skeleton with the
// time Module at 64 Hz, one fixed step a frame: Advance adds the step's Command
// to every Health in FixedUpdate; Feed, the input's entry, writes the next
// step's Command in PreUpdate and is skipped while resimulating; the strategy
// records after each step and decides each frame in PreUpdate before Feed.
// Each CHECK carries a tag in brackets ("[rb-...]"): the step's mutants expect
// it (docs/architecture/target/v2/scripts/rollback_mutations.json).

#include "RollbackTestSupport.hpp"

#include "engine/rollback/Inbox.hpp"
#include "engine/rollback/Runtime.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"

#include <array>
#include <string>
#include <vector>

using namespace RollbackTest;

namespace {

constexpr W::Nanoseconds kStep = 15'625'000;

// The Command of each step (index = tick).
constexpr std::array<std::int32_t, 32> kScript{0, 1, 2, 3, 1, 1, 2, 5, 1, 0, 3, 2, 1, 4, 1, 1,
                                               2, 2, 3, 1, 0, 1, 2, 1, 1, 3, 2, 1, 1, 2, 1, 1};

K::SystemResult Advance(K::Query<K::Write<Health>> health, K::Res<Command> command) {
    const std::int32_t move = command->move;
    health.ForEach([move](K::EntityId, K::Mut<Health> hp) { hp->hp += move; });
    return {};
}
std::vector<R::RollbackAbandoned>& Notes() {
    static std::vector<R::RollbackAbandoned> notes;
    return notes;
}
// Keeps every RollbackAbandoned (a message nobody reads is trimmed when the Run ends).
K::SystemResult KeepNotes(K::MessageReader<R::RollbackAbandoned> notes) {
    for (const R::RollbackAbandoned* note : notes.Read()) Notes().push_back(*note);
    return {};
}
// The input's entry: the Command of the next step (Tick + 1).
K::SystemResult Feed(K::ResMut<Command> command, K::Res<W::Tick> tick) {
    command->move = kScript[(tick->value + 1) % kScript.size()];
    return {};
}

O::OptionBag WithStrategy(R::StateRollbackSettings settings = {}) {
    O::OptionBag options;
    options.Set(R::RollbackOptions{std::make_shared<R::StateRollbackStrategy>(settings)});
    return options;
}

void Build(O::WorldSetup& s) {
    s.Registrar().Component<Health>({.name = "Health"});
    s.Registrar().Resource<Command>({.name = "Command"});
    R::RollbackEntry rollback(s);
    rollback.Component<Health>();
    rollback.Input<Command>(64);
    rollback.StepSchedule("FixedUpdate", "Last");
    s.Registrar().AddSystem<&Advance>(In("Advance", "FixedUpdate", "Update"));
    const R::RollbackOptions* options = s.Options<R::RollbackOptions>();
    const bool strategy = options != nullptr && options->strategy != nullptr;
    K::SystemOptions feed = In("Feed", "Main", "PreUpdate");
    if (strategy) {
        feed.mounts[0].after = {"RunRollback"};
        s.Registrar().AddSystem<&KeepNotes>(In("KeepNotes", "Main", "Update"));
    }
    rollback.SkipDuringResimulation(feed);
    s.Registrar().AddSystem<&Feed>(feed);
}

std::unique_ptr<O::Product> Make(O::OptionBag options) {
    auto built = O::Product::Build(Game(&Build, std::move(options)), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Main").GetWorld();
    world.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta, {}});
    world.SetResource(Command{});
    world.Insert(1, Health{100});
    world.Insert(2, Health{50});
    return std::move(*built);
}

K::World& WorldOf(O::Product& p) { return p.World("Main").GetWorld(); }

void Frames(O::Product& p, int count) {
    for (int i = 0; i < count; ++i) {
        auto ran = p.RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
    }
}

std::uint64_t TickOf(O::Product& p) { return WorldOf(p).Resource<W::Tick>()->value; }

std::string SummaryText(O::Product& p) {
    auto summary = WorldOf(p).Summary();
    REQUIRE_MESSAGE(static_cast<bool>(summary), (summary ? std::string() : summary.error().Text()));
    return summary->Text();
}

R::RollbackInbox& Inbox(O::Product& p) { return *WorldOf(p).ResourceMut<R::RollbackInbox>(); }

} // namespace

TEST_CASE("rollback strategy: R-2: going back and resimulating gives the World running straight would have") {
    // Straight: 12 frames, no rollback (the strategy records; nothing is confirmed).
    auto straight = Make(WithStrategy());
    Frames(*straight, 12);
    // Rolled back: the same 12 frames; after frame 9 the authority confirms step 4 with the values the World
    // predicted, and mode Always goes back to step 4 and resimulates 5 to 9 within frame 10.
    auto rolled = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}));
    Frames(*rolled, 9);
    REQUIRE(TickOf(*rolled) == 9);
    const auto hp1 = WorldOf(*rolled).Get<Health>(1);
    REQUIRE(hp1 != nullptr);
    Inbox(*rolled).ConfirmedThrough(4);
    Frames(*rolled, 3);
    REQUIRE(TickOf(*straight) == TickOf(*rolled));
    CHECK_MESSAGE(WorldOf(*rolled).Get<Health>(1)->hp == WorldOf(*straight).Get<Health>(1)->hp,
                  "[rb-r2-equal] Health of 1: " << WorldOf(*rolled).Get<Health>(1)->hp << " vs "
                                                << WorldOf(*straight).Get<Health>(1)->hp);
    CHECK_MESSAGE(SummaryText(*rolled) == SummaryText(*straight), "[rb-r2-equal] the World summaries differ");
    CHECK_MESSAGE(WorldOf(*rolled).Resource<W::Resimulation>()->active == 0, "[rb-resimulation-lowered]");
}

TEST_CASE("rollback strategy: an authoritative value that differs goes back and resimulates from it") {
    auto p = Make(WithStrategy());
    Frames(*p, 9);
    // The authority says Health of 1 was 7 at step 5; the World predicted otherwise.
    std::int32_t later = 0;
    for (std::uint64_t t = 6; t <= 10; ++t) later += kScript[t];
    Inbox(*p).Value<Health>(1, 5, Health{7});
    Inbox(*p).ConfirmedThrough(5);
    Frames(*p, 1);
    REQUIRE(TickOf(*p) == 10);
    CHECK_MESSAGE(WorldOf(*p).Get<Health>(1)->hp == 7 + later, "[rb-authority-adopted] " << WorldOf(*p).Get<Health>(1)->hp
                                                                                         << " vs " << 7 + later);
    // Entity 2 had no authoritative value: its prediction stands.
    std::int32_t all = 0;
    for (std::uint64_t t = 1; t <= 10; ++t) all += kScript[t];
    CHECK_MESSAGE(WorldOf(*p).Get<Health>(2)->hp == 50 + all, "[rb-others-kept] " << WorldOf(*p).Get<Health>(2)->hp);
}

TEST_CASE("rollback strategy: mode Check does not go back when the authoritative value is the predicted one") {
    auto p = Make(WithStrategy());
    Frames(*p, 6);
    const std::int32_t predicted = WorldOf(*p).Get<Health>(1)->hp;
    std::int32_t at3 = 100;
    for (std::uint64_t t = 1; t <= 3; ++t) at3 += kScript[t];
    Inbox(*p).Value<Health>(1, 3, Health{at3});
    Inbox(*p).ConfirmedThrough(3);
    const auto stampBefore = WorldOf(*p).Stamps<Health>(2);
    Frames(*p, 1);
    CHECK_MESSAGE(WorldOf(*p).Get<Health>(1)->hp == predicted + kScript[7], "[rb-check-agrees] " << WorldOf(*p).Get<Health>(1)->hp);
    // No restore: entity 2 changed in step 7 only, not before it by a restore (its added stamp is unchanged).
    CHECK_MESSAGE(WorldOf(*p).Stamps<Health>(2)->first == stampBefore->first, "[rb-check-agrees] restored anyway");
}

TEST_CASE("rollback strategy: a confirmed point further back than the depth is given up: RollbackAbandoned") {
    auto p = Make(WithStrategy({.depth = 4, .mode = R::RollbackMode::Always}));
    Frames(*p, 9);
    Notes().clear();
    Inbox(*p).ConfirmedThrough(2);
    Frames(*p, 1);
    const std::vector<R::RollbackAbandoned> notes = Notes();
    REQUIRE_MESSAGE(notes.size() == 1, "[rb-abandoned] " << notes.size() << " notes");
    CHECK_MESSAGE((notes[0].requested == 2 && notes[0].tick == 9 && notes[0].depth == 4), "[rb-abandoned] fields");
}

TEST_CASE("rollback strategy: R-4: a World without a strategy has no history and records nothing") {
    auto built = O::Product::Build(Game(&Build), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Main").GetWorld();
    CHECK_MESSAGE(!world.Types().ResourceIdOf<R::RollbackHistory>().has_value(), "[rb-r4-no-history] a history without a strategy");
    CHECK_MESSAGE(world.Types().SystemNamed("RollbackRecord:FixedUpdate") == nullptr, "[rb-r4-no-record] a record System");
}

TEST_CASE("rollback strategy: R-5: a replaced World starts with no history of the old one") {
    auto p = Make(WithStrategy());
    Frames(*p, 5);
    REQUIRE(WorldOf(*p).Resource<R::RollbackHistory>()->lastTick.has_value());
    auto replaced = p->ReplaceWorld("Main", O::ReplaceRequest{});
    REQUIRE_MESSAGE(static_cast<bool>(replaced), (replaced ? std::string() : replaced.error().message));
    CHECK_MESSAGE(!WorldOf(*p).Resource<R::RollbackHistory>()->lastTick.has_value(), "[rb-r5-fresh] the old history came along");
}
