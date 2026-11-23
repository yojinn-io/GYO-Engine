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

#include "engine/entity_ids/RangeAllotter.hpp"
#include "engine/base/Trace.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/rollback/Runtime.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace GYO::Test::Rollback;

namespace {

constexpr W::Nanoseconds kStep = 15'625'000;

// The Command of each step (index = tick).
constexpr std::array<std::int32_t, 32> kScript{0, 1, 2, 3, 1, 1, 2, 5, 1, 0, 3, 2, 1, 4, 1, 1,
                                               2, 2, 3, 1, 0, 1, 2, 1, 1, 3, 2, 1, 1, 2, 1, 1};

// Every step run, the first time or again while resimulating.
int& StepsRun() {
    static int count = 0;
    return count;
}
K::SystemResult CountStep(K::Res<Command>) {
    ++StepsRun();
    return {};
}
K::SystemResult Advance(K::Query<K::Write<Health>> health, K::Res<Command> command) {
    const std::int32_t move = command->move;
    health.ForEach([move](K::EntityId, K::Mut<Health> hp) { hp->hp += move; });
    return {};
}
// ---- Spawns and despawns in the step (R-2 premise 6: the allotter rolls back), a side effect skipped while
// resimulating, and an OnAdd hook that reads "resimulating".
K::SystemResult Churn(K::Spawn<K::Bundle<Health>> spawn, K::Despawn despawn, K::Res<W::Tick> tick) {
    if (tick->value == 6) {
        auto id = spawn.New(Health{9});
        if (!id) return ::GYO::Engine::Base::Err(std::move(id).error());
    }
    if (tick->value == 8) despawn.Entity(2);
    return {};
}
int& SideEffects() {
    static int count = 0;
    return count;
}
K::SystemResult SideEffect(K::Res<Command>) {
    ++SideEffects();
    return {};
}
std::vector<int>& AddedFlags() {
    static std::vector<int> flags;
    return flags;
}
K::SystemResult HealthAdded(K::HookCtx, K::Res<W::Resimulation> resimulation) {
    AddedFlags().push_back(resimulation->active);
    return {};
}
bool SameAllotter(const GYO::Engine::EntityIds::RangeAllotter& a, const GYO::Engine::EntityIds::RangeAllotter& b) {
    return a.first == b.first && a.last == b.last && a.next == b.next && a.exhausted == b.exhausted;
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

// A receiver of the trace facade (no trace Module): keeps the open spans' categories and counts the runs of the
// step schedule FixedUpdate with and without a span.rollback around them (authority-rollback declaration 10.4).
struct SpanLog {
    std::vector<std::string> open;
    int firstRuns = 0;
    int resimulated = 0;
};
SpanLog& Spans() {
    static SpanLog log;
    return log;
}
::GYO::Engine::Base::TraceToken SpanBegin(std::string_view category, std::string_view name, std::string_view) noexcept {
    SpanLog& log = Spans();
    if (category == "kernel" && name == "FixedUpdate") {
        const bool under = std::find(log.open.begin(), log.open.end(), "rollback") != log.open.end();
        ++(under ? log.resimulated : log.firstRuns);
    }
    log.open.emplace_back(category);
    return {log.open.size(), 1, static_cast<std::uint32_t>(log.open.size())};
}
void SpanEnd(::GYO::Engine::Base::TraceToken, bool) noexcept { Spans().open.pop_back(); }
void SpanError(::GYO::Engine::Base::TraceToken, std::string_view, bool) noexcept {}
void SpanMark(std::string_view, bool) noexcept {}
std::size_t SpanPath(std::span<char>) noexcept { return 0; }
constexpr ::GYO::Engine::Base::TraceReceiver kSpanReceiver{&SpanBegin, &SpanEnd, &SpanError, &SpanMark, &SpanPath};

O::OptionBag WithStrategy(R::StateRollbackSettings settings = {}) {
    O::OptionBag options;
    options.Set(R::RollbackOptions{std::make_shared<R::StateRollbackStrategy>(settings)});
    return options;
}

// The input's entry runs after the rollback (Build) or before it (BuildFeedFirst: the frame's input is already
// written when the rollback writes the old ones back).
void BuildFeed(O::WorldSetup& s, bool feedFirst) {
    s.Registrar().Component<Health>({.name = "Health"});
    s.Registrar().Resource<Command>({.name = "Command"});
    R::RollbackEntry rollback(s);
    rollback.Component<Health>();
    rollback.Input<Command>();  // the strategy's depth: the depth-edge cases below write back that many
    rollback.StepSchedule("FixedUpdate", "Last");
    s.Registrar().AddSystem<&Advance>(In("Advance", "FixedUpdate", "Update"));
    s.Registrar().AddSystem<&CountStep>(In("CountStep", "FixedUpdate", "Update"));
    const R::RollbackOptions* options = s.Options<R::RollbackOptions>();
    const bool strategy = options != nullptr && options->strategy != nullptr;
    K::SystemOptions feed = In("Feed", "Main", "PreUpdate");
    if (strategy) {
        if (feedFirst) {
            feed.mounts[0].before = {"RunRollback"};
        } else {
            feed.mounts[0].after = {"RunRollback"};
        }
        s.Registrar().AddSystem<&KeepNotes>(In("KeepNotes", "Main", "Update"));
    }
    rollback.SkipDuringResimulation(feed);
    s.Registrar().AddSystem<&Feed>(feed);
}
void Build(O::WorldSetup& s) { BuildFeed(s, false); }

// R-2 premise 4: a message the step writes and reads within the step (a hit). A resimulation writes it again, and
// messages do not roll back (kernel DI-KS 10), so the sequence numbers and the cursor end further on than running
// straight; since summary version 4 they are not in the World summary (v8-arch-gyo2-kernel-summary-messages).
struct Hit {
    std::int32_t move;
};
void HashValue(K::Hasher& h, const Hit& v) { HashValue(h, v.move); }
K::SystemResult SendHit(K::MessageWriter<Hit> hits, K::Res<Command> command) {
    hits.Send(Hit{command->move});
    return {};
}
int& HitsRead() {
    static int count = 0;
    return count;
}
K::SystemResult TakeHits(K::MessageReader<Hit> hits) {
    HitsRead() += static_cast<int>(hits.Read().size());
    return {};
}
void BuildHit(O::WorldSetup& s) {
    Build(s);
    s.Registrar().Message<Hit>({.name = "Hit"});
    s.Registrar().AddSystem<&SendHit>(In("SendHit", "FixedUpdate", "Update"));
    s.Registrar().AddSystem<&TakeHits>(In("TakeHits", "FixedUpdate", "PostUpdate"));
}
void BuildFeedFirst(O::WorldSetup& s) { BuildFeed(s, true); }

// A System of the step takes Health off entity 2 at step 7 (the cell is gone, the entity stays).
K::SystemResult Strip(K::CommandsFor<K::Inserts<>, K::Removes<Health>> commands, K::Res<W::Tick> tick) {
    if (tick->value == 7) commands.Remove<Health>(2);
    return {};
}
// Score adds up Health every step (after Advance): a value that depends on its own history, and that takes no
// authoritative value.
K::SystemResult Accumulate(K::Query<K::Read<Health>, K::Write<Score>> rows) {
    rows.ForEach([](K::EntityId, const Health& hp, K::Mut<Score> score) { score->value += hp.hp; });
    return {};
}
void BuildScore(O::WorldSetup& s) {
    Build(s);
    s.Registrar().Component<Score>({.name = "Score"});
    R::RollbackEntry(s).Component<Score>();
    K::SystemOptions accumulate = In("Accumulate", "FixedUpdate", "Update");
    accumulate.mounts[0].after = {"Advance"};
    s.Registrar().AddSystem<&Accumulate>(accumulate);
}
void BuildStrip(O::WorldSetup& s) {
    Build(s);
    s.Registrar().AddSystem<&Strip>(In("Strip", "FixedUpdate", "Update"));
}

void BuildChurn(O::WorldSetup& s) {
    Build(s);
    s.Registrar().Resource<GYO::Engine::EntityIds::RangeAllotter>({.name = "Allotter"});
    s.Registrar().Allotter<GYO::Engine::EntityIds::RangeAllotter>();
    R::RollbackEntry rollback(s);
    rollback.Resource<GYO::Engine::EntityIds::RangeAllotter>({.equal = &SameAllotter});
    s.Registrar().AddSystem<&Churn>(In("Churn", "FixedUpdate", "Update"));
    K::SystemOptions side = In("SideEffect", "FixedUpdate", "PostUpdate");
    rollback.SkipDuringResimulation(side);
    s.Registrar().AddSystem<&SideEffect>(side);
    s.Registrar().Hook<Health, &HealthAdded>(K::HookTrigger::OnAdd, {.name = "HealthAdded"});
}

std::unique_ptr<O::Product> Make(O::OptionBag options, void (*build)(O::WorldSetup&) = &Build) {
    auto built = O::Product::Build(Game(build, std::move(options)), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Main").GetWorld();
    world.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
    world.SetResource(Command{});
    world.Insert(1, Health{100});
    world.Insert(2, Health{50});
    if (world.Types().ResourceIdOf<GYO::Engine::EntityIds::RangeAllotter>()) world.SetResource(GYO::Engine::EntityIds::RangeAllotter::Of(1000, 1999));
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
    CHECK_MESSAGE(TickOf(*straight) == TickOf(*rolled), "[rb-r2-equal] Tick " << TickOf(*rolled) << " vs " << TickOf(*straight));
    CHECK_MESSAGE(WorldOf(*rolled).Get<Health>(1)->hp == WorldOf(*straight).Get<Health>(1)->hp,
                  "[rb-r2-equal] Health of 1: " << WorldOf(*rolled).Get<Health>(1)->hp << " vs "
                                                << WorldOf(*straight).Get<Health>(1)->hp);
    CHECK_MESSAGE(SummaryText(*rolled) == SummaryText(*straight), "[rb-r2-equal] the World summaries differ");
    CHECK_MESSAGE(WorldOf(*rolled).Resource<W::Resimulation>()->active == 0, "[rb-resimulation-lowered]");
}

TEST_CASE("rollback strategy: R-2: a message written and read within the step leaves the summary of running straight") {
    auto straight = Make(WithStrategy(), &BuildHit);
    Frames(*straight, 12);
    HitsRead() = 0;
    auto rolled = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}), &BuildHit);
    Frames(*rolled, 9);
    Inbox(*rolled).ConfirmedThrough(4);
    Frames(*rolled, 3);
    // The resimulation ran steps again and their hits were written and read again (more than the 12 steps).
    REQUIRE_MESSAGE(HitsRead() > 12, "[rb-r2-message-in-step] hits read " << HitsRead());
    REQUIRE(WorldOf(*rolled).MessagePayloads<Hit>().empty());
    REQUIRE(WorldOf(*straight).MessagePayloads<Hit>().empty());
    CHECK_MESSAGE(WorldOf(*rolled).Get<Health>(1)->hp == WorldOf(*straight).Get<Health>(1)->hp, "[rb-r2-message-in-step] Health of 1");
    CHECK_MESSAGE(SummaryText(*rolled) == SummaryText(*straight), "[rb-r2-message-in-step] the World summaries differ");
}

TEST_CASE("rollback strategy: each step run again has a span.rollback around it; the first run has none") {
    auto p = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}));
    Frames(*p, 9);
    Spans() = {};
    const ::GYO::Engine::Base::TraceReceiver* previous = ::GYO::Engine::Base::SetTraceReceiver(&kSpanReceiver);
    Inbox(*p).ConfirmedThrough(4);
    Frames(*p, 1); // steps 5 to 9 again, then step 10
    ::GYO::Engine::Base::SetTraceReceiver(previous);
    CHECK_MESSAGE(Spans().resimulated == 5, "[rb-trace-resimulate] runs of FixedUpdate under span.rollback: " << Spans().resimulated);
    CHECK_MESSAGE(Spans().firstRuns == 1, "[rb-trace-first-run] runs of FixedUpdate without span.rollback: " << Spans().firstRuns);
    CHECK_MESSAGE(Spans().open.empty(), "[rb-trace-resimulate] spans left open: " << Spans().open.size());
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
    StepsRun() = 0;
    Frames(*p, 1);
    CHECK_MESSAGE(StepsRun() == 1, "[rb-check-agrees] steps run in the frame: " << StepsRun());
    CHECK_MESSAGE(WorldOf(*p).Get<Health>(1)->hp == predicted + kScript[7], "[rb-check-agrees] " << WorldOf(*p).Get<Health>(1)->hp);
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

TEST_CASE("rollback strategy: the step schedule and the frame schedule default to the time Module's; none is RB-E10") {
    // StepSchedule() takes the time Module's fixed-step schedule and its Last.
    auto defaults = O::Product::Build(Game([](O::WorldSetup& s) {
                                          s.Registrar().Component<Health>({.name = "Health"});
                                          R::RollbackEntry rollback(s);
                                          rollback.Component<Health>();
                                          rollback.StepSchedule();
                                      }, WithStrategy()),
                                      Units());
    REQUIRE_MESSAGE(static_cast<bool>(defaults), Reported(defaults));
    const R::RollbackRegistry* registry = WorldOf(**defaults).Resource<R::RollbackRegistry>();
    REQUIRE(registry != nullptr);
    CHECK_MESSAGE((registry->StepSchedule("FixedUpdate") != nullptr && registry->StepSchedule("FixedUpdate")->recordStage == "Last"),
                  "[rb-step-default]");
    // A time Module set to other names: the defaults follow it.
    O::OptionBag renamed = WithStrategy();
    renamed.Set(W::WorldTimeOptions{.frameSchedule = "Frame2", .fixedSchedule = "Physics"});
    auto followed = O::Product::Build(Game([](O::WorldSetup& s) {
                                          s.Registrar().Component<Health>({.name = "Health"});
                                          R::RollbackEntry rollback(s);
                                          rollback.Component<Health>();
                                          rollback.StepSchedule();
                                      }, std::move(renamed)),
                                      Units());
    const std::string text = Reported(followed);
    // (Neither schedule exists: the kernel names where each System was mounted.)
    CHECK_MESSAGE(text.find("'RollbackRecord:Physics' is mounted in schedule 'Physics'") != std::string::npos,
                  "[rb-step-default-follows] " << text);
    CHECK_MESSAGE(text.find("'RunRollback' is mounted in schedule 'Frame2'") != std::string::npos, "[rb-frame-default-follows] " << text);
    // The official strategy with no step schedule: RB-E10 (it would record nothing and take no authoritative value).
    auto none = O::Product::Build(Game([](O::WorldSetup& s) {
                                      s.Registrar().Component<Health>({.name = "Health"});
                                      R::RollbackEntry(s).Component<Health>();
                                  }, WithStrategy()),
                                  Units());
    CHECK_MESSAGE(Reported(none).find("RB-E10") != std::string::npos, "[rb-e10] " << Reported(none));
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

TEST_CASE("rollback strategy: R-5: a BuildNewFirst replacement that fails leaves the old World's rollback as it was") {
    // The registrations are frozen into each World (authority-rollback declaration 5.1): a failed replacement
    // touches nothing of the old World's, so its history and rollback go on.
    auto p = Make(WithStrategy());
    Frames(*p, 9);
    O::ReplaceRequest request;
    request.extra = [](K::World&) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        return ::GYO::Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "the save is broken"));
    };
    auto replaced = p->ReplaceWorld("Main", std::move(request));
    REQUIRE_FALSE(static_cast<bool>(replaced));
    REQUIRE_MESSAGE(WorldOf(*p).Resource<R::RollbackHistory>()->lastTick == std::optional<std::uint64_t>{9},
                    "[rb-r5-failed-replace-keeps] the old World's history");
    std::int32_t later = 0;
    for (std::uint64_t t = 6; t <= 10; ++t) later += kScript[t];
    Inbox(*p).Value<Health>(1, 5, Health{7});
    Inbox(*p).ConfirmedThrough(5);
    Frames(*p, 1);
    CHECK_MESSAGE(WorldOf(*p).Get<Health>(1)->hp == 7 + later, "[rb-r5-failed-replace-keeps] the rollback after the failure: "
                                                                    << WorldOf(*p).Get<Health>(1)->hp << " vs " << 7 + later);
}

TEST_CASE("rollback strategy: R-2 with spawns, a despawn and a side effect: the allotter, the alive set and the hooks go back") {
    SideEffects() = 0;
    auto straight = Make(WithStrategy(), &BuildChurn);
    Frames(*straight, 12);
    const int straightEffects = SideEffects();
    SideEffects() = 0;
    auto rolled = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}), &BuildChurn);
    Frames(*rolled, 10);
    REQUIRE((WorldOf(*rolled).Alive(1000) && !WorldOf(*rolled).Alive(2)));
    Inbox(*rolled).ConfirmedThrough(4);
    AddedFlags().clear();
    Frames(*rolled, 2);
    CHECK_MESSAGE(SummaryText(*rolled) == SummaryText(*straight), "[rb-r2-churn] the World summaries differ");
    CHECK_MESSAGE((WorldOf(*rolled).Alive(1000) && !WorldOf(*rolled).Alive(1001) && !WorldOf(*rolled).Alive(2)),
                  "[rb-r2-churn] the alive set");
    CHECK_MESSAGE(SideEffects() == straightEffects, "[rb-side-effect-skipped] " << SideEffects() << " vs " << straightEffects);
    // The restore puts 2 back (OnAdd) and the resimulation spawns 1000 again (OnAdd): both while resimulating.
    std::string flags;
    for (int flag : AddedFlags()) flags += std::to_string(flag);
    CHECK_MESSAGE(flags == "11", "[rb-resimulating-before-restore] OnAdd saw resimulating as " << flags);
}

// ---- R-1: an interface implemented from scratch (no code of the official strategy) goes through the same socket.

struct ScratchStore {
    std::map<std::uint64_t, std::vector<std::pair<K::EntityId, Health>>> health;
    std::map<std::uint64_t, std::vector<K::EntityId>> alive;
    std::map<std::uint64_t, Command> inputs;
    std::uint64_t decided = 0;
};
void HashValue(K::Hasher&, const ScratchStore&) {}

class ScratchStrategy final : public R::RollbackStrategy {
public:
    [[nodiscard]] std::string_view Name() const override { return "ScratchStrategy"; }
    void Check(R::CheckContext&) override {}
    ::GYO::Engine::Base::Result<void, K::KernelError> Record(R::RecordContext& c) override {
        R::Exclusive& world = c.World();
        const std::uint64_t tick = c.Tick();
        std::vector<std::pair<K::EntityId, Health>> cells;
        world.ReadColumn<Health>([&cells](K::EntityId e, K::ChangeStamp, K::ChangeStamp, const Health& h) { cells.emplace_back(e, h); });
        const std::vector<K::EntityId> alive = world.Entities();
        const Command input = **world.Read<Command>([](const Command& v) { return v; });
        static_cast<void>(world.Modify<ScratchStore>([&](ScratchStore& store) {
            store.health[tick] = cells;
            store.alive[tick] = alive;
            store.inputs[tick] = input;
        }));
        return {};
    }
    ::GYO::Engine::Base::Result<void, K::KernelError> Decide(R::DecideContext& c) override {
        R::Exclusive& world = c.World();
        const auto confirmed = **world.Read<R::RollbackInbox>([](const R::RollbackInbox& i) { return i.Confirmed(); });
        if (!confirmed || *confirmed > c.Tick()) return {};
        const std::uint64_t decided = **world.Read<ScratchStore>([](const ScratchStore& s) { return s.decided; });
        if (*confirmed <= decided) return {};
        static_cast<void>(world.Modify<ScratchStore>([&](ScratchStore& s) { s.decided = *confirmed; }));
        c.RollBackTo(*confirmed);
        static_cast<void>(world.Modify<W::Resimulation>([](W::Resimulation& r) { r.active = 1; }));
        return {};
    }
    ::GYO::Engine::Base::Result<void, K::KernelError> Restore(R::RestoreContext& c) override {
        R::Exclusive& world = c.World();
        const std::uint64_t target = c.Target();
        K::RestoreSet set;
        static_cast<void>(world.Read<ScratchStore>([&](const ScratchStore& s) {
            set.Alive(s.alive.at(target));
            set.Column<Health>(s.health.at(target));
        }));
        return world.Restore(std::move(set));
    }
    ::GYO::Engine::Base::Result<void, K::KernelError> Resimulate(R::ResimulateContext& c) override {
        R::Exclusive& world = c.World();
        W::RewindFixedSteps(world, c.Tick() - c.Target());
        for (std::uint64_t step = c.Target() + 1; step <= c.Tick(); ++step) {
            const Command input = **world.Read<ScratchStore>([step](const ScratchStore& s) { return s.inputs.at(step); });
            static_cast<void>(world.Modify<Command>([input](Command& v) { v = input; }));
            if (auto ran = W::RunOneFixedStep(world, c.StepSchedule()); !ran) return ran;
        }
        static_cast<void>(world.Modify<W::Resimulation>([](W::Resimulation& r) { r.active = 0; }));
        return {};
    }
    ::GYO::Engine::Base::Result<void, K::KernelError> Correct(R::CorrectContext&) override {
        ++CorrectCalls();
        return {};
    }
    static int& CorrectCalls() {
        static int calls = 0;
        return calls;
    }
};

void BuildScratch(O::WorldSetup& s) {
    Build(s);
    s.Registrar().Resource<ScratchStore>({.name = "ScratchStore"});
    s.Prepare([](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(ScratchStore{});
        return {};
    });
}

TEST_CASE("rollback strategy: R-1: a strategy written from scratch through the same socket passes R-2") {
    auto straight = Make(WithStrategy());
    Frames(*straight, 12);
    O::OptionBag options;
    options.Set(R::RollbackOptions{std::make_shared<ScratchStrategy>()});
    auto scratch = Make(std::move(options), &BuildScratch);
    ScratchStrategy::CorrectCalls() = 0;
    Frames(*scratch, 9);
    Inbox(*scratch).ConfirmedThrough(4);
    Frames(*scratch, 3);
    CHECK_MESSAGE(WorldOf(*scratch).Get<Health>(1)->hp == WorldOf(*straight).Get<Health>(1)->hp,
                  "[rb-r1-scratch] " << WorldOf(*scratch).Get<Health>(1)->hp << " vs " << WorldOf(*straight).Get<Health>(1)->hp);
    CHECK_MESSAGE(WorldOf(*scratch).Get<Health>(2)->hp == WorldOf(*straight).Get<Health>(2)->hp, "[rb-r1-scratch] Health of 2");
    CHECK_MESSAGE(TickOf(*scratch) == TickOf(*straight), "[rb-r1-scratch] Tick " << TickOf(*scratch) << " vs " << TickOf(*straight));
    CHECK_MESSAGE(WorldOf(*scratch).Resource<W::Resimulation>()->active == 0, "[rb-r1-scratch] resimulating left raised");
    CHECK_MESSAGE(ScratchStrategy::CorrectCalls() == 12, "[rb-r1-correct-called] once a frame: " << ScratchStrategy::CorrectCalls());
}


// ---- Review of step 4: rollbacks that read the history earlier rollbacks rewrote, the window's edge, the
// entry's values before C, removals, the modes, and the inputs.

TEST_CASE("rollback strategy: rollbacks every frame at the depth's edge, across spawns and despawns, give the straight World") {
    constexpr std::uint32_t kDepth = 6;
    auto straight = Make(WithStrategy(), &BuildChurn);
    Frames(*straight, 26);
    auto rolled = Make(WithStrategy({.depth = kDepth, .mode = R::RollbackMode::Always}), &BuildChurn);
    Frames(*rolled, 8);
    Notes().clear();
    std::string steps;
    std::size_t kept = 0;
    for (int frame = 0; frame < 18; ++frame) {
        // The confirmed point is exactly depth steps behind: a rollback, not given up.
        Inbox(*rolled).ConfirmedThrough(TickOf(*rolled) - kDepth);
        StepsRun() = 0;
        Frames(*rolled, 1);
        if (StepsRun() != static_cast<int>(kDepth) + 1) steps += std::to_string(StepsRun()) + " ";
        kept = std::max(kept, WorldOf(*rolled).Resource<R::RollbackHistory>()->alive.size());
    }
    CHECK_MESSAGE(steps.empty(), "[rb-consecutive-steps] frames that did not resimulate depth steps: " << steps);
    CHECK_MESSAGE(Notes().empty(), "[rb-consecutive-steps] " << Notes().size() << " abandoned");
    CHECK_MESSAGE(SummaryText(*rolled) == SummaryText(*straight), "[rb-consecutive-equal] the World summaries differ");
    CHECK_MESSAGE(kept <= kDepth + 1, "[rb-history-pruned] alive sets kept: " << kept);
}

TEST_CASE("rollback strategy: an authoritative value before C counts as the value at C (the last at or before it)") {
    auto p = Make(WithStrategy());
    Frames(*p, 9);
    Inbox(*p).Value<Health>(1, 5, Health{7});
    Inbox(*p).ConfirmedThrough(7);
    Frames(*p, 1);
    std::int32_t later = 0;
    for (std::uint64_t t = 8; t <= 10; ++t) later += kScript[t];
    CHECK_MESSAGE(WorldOf(*p).Get<Health>(1)->hp == 7 + later, "[rb-authority-before-c] " << WorldOf(*p).Get<Health>(1)->hp
                                                                                          << " vs " << 7 + later);
}

TEST_CASE("rollback strategy: Removed takes the component off at that step; the entity stays") {
    auto p = Make(WithStrategy());
    Frames(*p, 9);
    Inbox(*p).Removed<Health>(1, 5);
    Inbox(*p).ConfirmedThrough(5);
    Frames(*p, 1);
    CHECK_MESSAGE((WorldOf(*p).Alive(1) && WorldOf(*p).Get<Health>(1) == nullptr), "[rb-removed-adopted]");
}

TEST_CASE("rollback strategy: a cell a step removed stays removed when the rollback goes back past the removal") {
    auto straight = Make(WithStrategy(), &BuildStrip);
    Frames(*straight, 12);
    auto rolled = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}), &BuildStrip);
    Frames(*rolled, 10);
    REQUIRE(WorldOf(*rolled).Get<Health>(2) == nullptr);
    Inbox(*rolled).ConfirmedThrough(8);
    Frames(*rolled, 2);
    CHECK_MESSAGE(WorldOf(*rolled).Get<Health>(2) == nullptr, "[rb-removed-cell-recorded] Health of 2 came back");
    CHECK_MESSAGE(SummaryText(*rolled) == SummaryText(*straight), "[rb-removed-cell-recorded] the World summaries differ");
}

TEST_CASE("rollback strategy: mode Disabled never goes back") {
    auto p = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Disabled}));
    Frames(*p, 9);
    Inbox(*p).Value<Health>(1, 4, Health{-1});
    Inbox(*p).ConfirmedThrough(4);
    StepsRun() = 0;
    Frames(*p, 1);
    CHECK_MESSAGE(StepsRun() == 1, "[rb-disabled] steps run in the frame: " << StepsRun());
}

TEST_CASE("rollback strategy: the inbox is pruned when C moves without a rollback (mode Check, agreeing values)") {
    auto p = Make(WithStrategy());
    Frames(*p, 6);
    std::int32_t hp = 100;
    for (std::uint64_t t = 1; t <= 3; ++t) {
        hp += kScript[t];
        Inbox(*p).Value<Health>(1, t, Health{hp});
    }
    Inbox(*p).ConfirmedThrough(3);
    StepsRun() = 0;
    Frames(*p, 1);
    REQUIRE(StepsRun() == 1);
    const R::InboxColumn<Health>* values = WorldOf(*p).Resource<R::RollbackInbox>()->Values<Health>();
    REQUIRE(values != nullptr);
    CHECK_MESSAGE(!values->At(1, 1).has_value(), "[rb-inbox-pruned] the value of step 1 is still kept");
    CHECK_MESSAGE((values->At(1, 3).has_value() && **values->At(1, 3) == Health{hp}), "[rb-inbox-pruned] the value at C is gone");
}

TEST_CASE("rollback strategy: a value for an entity this World does not have is no reason to go back") {
    auto p = Make(WithStrategy());
    Frames(*p, 6);
    Inbox(*p).Value<Health>(77, 3, Health{1});
    StepsRun() = 0;
    for (std::uint64_t c = 3; c <= 6; ++c) {
        Inbox(*p).ConfirmedThrough(c);
        Frames(*p, 1);
    }
    CHECK_MESSAGE(StepsRun() == 4, "[rb-not-alive-ignored] steps run in 4 frames: " << StepsRun());
}

TEST_CASE("rollback strategy: an input entry that runs before the rollback keeps this frame's input") {
    auto straight = Make(WithStrategy(), &BuildFeedFirst);
    Frames(*straight, 12);
    auto rolled = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}), &BuildFeedFirst);
    Frames(*rolled, 9);
    Inbox(*rolled).ConfirmedThrough(4);
    Frames(*rolled, 3);
    CHECK_MESSAGE(WorldOf(*rolled).Get<Health>(1)->hp == WorldOf(*straight).Get<Health>(1)->hp,
                  "[rb-input-put-back] " << WorldOf(*rolled).Get<Health>(1)->hp << " vs " << WorldOf(*straight).Get<Health>(1)->hp);
}

TEST_CASE("rollback strategy: a confirmed point ahead of this World waits until the World has run that step") {
    auto p = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}));
    Frames(*p, 6);
    Notes().clear();
    Inbox(*p).ConfirmedThrough(9);
    StepsRun() = 0;
    Frames(*p, 3);
    CHECK_MESSAGE(Notes().empty(), "[rb-ahead-waits] " << Notes().size() << " abandoned");
    CHECK_MESSAGE(StepsRun() == 3, "[rb-ahead-waits] steps run in 3 frames: " << StepsRun());
    // Step 9 has run: the next frame goes back to it (Always; no step to run again).
    REQUIRE(TickOf(*p) == 9);
    Frames(*p, 1);
    CHECK_MESSAGE(WorldOf(*p).Resource<R::RollbackHistory>()->decided == std::optional<std::uint64_t>(9), "[rb-ahead-waits] decided");
}

TEST_CASE("rollback strategy: a second rollback reads the history the first one rewrote") {
    // A: one rollback (an authoritative Health of 1 at step 5 changes the run from there). B: the same, then a
    // second rollback to step 8 whose authoritative Health is the one A's run has there; Score (no authoritative
    // value) is put back from the history the first rollback rewrote, so B ends as A.
    const auto run = [](bool second) {
        auto p = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}), &BuildScore);
        WorldOf(*p).Insert(1, Score{});
        WorldOf(*p).Insert(2, Score{});
        Frames(*p, 9);
        Inbox(*p).Value<Health>(1, 5, Health{7});
        Inbox(*p).ConfirmedThrough(5);
        Frames(*p, 1);
        if (second) {
            Inbox(*p).Value<Health>(1, 8, Health{7 + kScript[6] + kScript[7] + kScript[8]});
            Inbox(*p).ConfirmedThrough(8);
            StepsRun() = 0;
            Frames(*p, 1);
            CHECK_MESSAGE(StepsRun() == 3, "[rb-rewritten-history-read] steps run: " << StepsRun());
            Frames(*p, 1);
        } else {
            Frames(*p, 2);
        }
        return p;
    };
    auto a = run(false);
    auto b = run(true);
    CHECK_MESSAGE(WorldOf(*b).Get<Score>(1)->value == WorldOf(*a).Get<Score>(1)->value,
                  "[rb-rewritten-history-read] Score of 1: " << WorldOf(*b).Get<Score>(1)->value << " vs " << WorldOf(*a).Get<Score>(1)->value);
    CHECK_MESSAGE(SummaryText(*b) == SummaryText(*a), "[rb-rewritten-history-read] the World summaries differ");
}

// ---- R-8: the inbox's edges, the decision's edges and pruning (review of step 8: the entry boundaries).

namespace {
bool HealthNearOne(const Health& a, const Health& b) { return a.hp - b.hp <= 1 && b.hp - a.hp <= 1; }
} // namespace

TEST_CASE("rollback strategy: R-8: C earlier than this World's first record is given up, and told once") {
    auto p = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}));
    Frames(*p, 3);
    // A replacement that starts at tick 9 (as a load would): the new World's first record is step 10.
    O::ReplaceRequest request;
    request.extra = [](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::Tick{9});
        world.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
        world.SetResource(Command{});
        world.Insert(1, Health{100});
        return {};
    };
    auto replaced = p->ReplaceWorld("Main", std::move(request));
    REQUIRE_MESSAGE(static_cast<bool>(replaced), (replaced ? std::string() : replaced.error().message));
    Frames(*p, 2);
    REQUIRE(TickOf(*p) == 11);
    Notes().clear();
    Inbox(*p).ConfirmedThrough(5);
    auto ran = p->RunFrame();
    CHECK_MESSAGE(static_cast<bool>(ran), "[rb-before-first-record] " << (ran ? std::string() : ran.error().message));
    CHECK_MESSAGE((Notes().size() == 1 && Notes()[0].requested == 5), "[rb-before-first-record] notes: " << Notes().size());
    // The next frame does not tell it again.
    Frames(*p, 1);
    CHECK_MESSAGE(Notes().size() == 1, "[rb-abandoned-once] notes after one more frame: " << Notes().size());
}

TEST_CASE("rollback strategy: R-8: Differs compares at C: a later value waits, a value the prediction lacks differs, the registered comparison decides") {
    R::InboxColumn<Health> inbox;
    // History: entity 1 had 10 from step 3.
    R::ComponentHistory<Health> history(nullptr);
    inbox.Put(1, 3, Health{10});
    const std::vector<K::EntityId> alive{1, 2};
    history.Adopt(3, inbox, alive);
    // A value later than C is not compared yet.
    inbox.Put(1, 7, Health{99});
    CHECK_MESSAGE(!history.Differs(5, inbox, alive), "[rb-differs-later-waits]");
    // The authority has a value where the prediction had no component.
    R::InboxColumn<Health> lacking;
    lacking.Put(2, 3, Health{4});
    CHECK_MESSAGE(history.Differs(5, lacking, alive), "[rb-differs-prediction-lacks]");
    // The registered comparison: 11 is near 10.
    R::ComponentHistory<Health> near(&HealthNearOne);
    near.Adopt(3, inbox, alive);
    R::InboxColumn<Health> close;
    close.Put(1, 3, Health{11});
    CHECK_MESSAGE(!near.Differs(5, close, alive), "[rb-differs-registered-comparison]");
}

TEST_CASE("rollback strategy: R-8: pruning keeps per entity the newest value at or before C") {
    R::InboxColumn<Health> values;
    values.Put(1, 2, Health{2});
    values.Put(1, 4, Health{4});
    values.Put(1, 6, Health{6});
    values.Put(2, 1, Health{1});
    values.Prune(5);
    CHECK_MESSAGE(!values.At(1, 3).has_value(), "[rb-inbox-prune-older] the value of step 2 is kept");
    CHECK_MESSAGE((values.At(1, 5).has_value() && **values.At(1, 5) == Health{4}), "[rb-inbox-prune-keeps-c] the value at C is gone");
    CHECK_MESSAGE((values.At(1, 6).has_value() && **values.At(1, 6) == Health{6}), "[rb-inbox-prune-later] the later value is gone");
    // Another entity's entry never counts as "a later value of the same entity".
    R::InboxColumn<Health> two;
    two.Put(1, 4, Health{4});
    two.Put(2, 1, Health{1});
    two.Prune(5);
    CHECK_MESSAGE((two.At(1, 5).has_value() && **two.At(1, 5) == Health{4}), "[rb-inbox-prune-per-entity]");
}

TEST_CASE("rollback strategy: R-8: the inbox is pruned after a rollback and after giving up") {
    // After a rollback (mode Always).
    auto rolled = Make(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}));
    Frames(*rolled, 9);
    Inbox(*rolled).Value<Health>(1, 2, Health{1});
    Inbox(*rolled).Value<Health>(1, 4, Health{2});
    Inbox(*rolled).ConfirmedThrough(4);
    Frames(*rolled, 1);
    const R::InboxColumn<Health>* after = WorldOf(*rolled).Resource<R::RollbackInbox>()->Values<Health>();
    REQUIRE(after != nullptr);
    CHECK_MESSAGE(!after->At(1, 3).has_value(), "[rb-inbox-pruned-after-rollback]");
    // After giving up (further back than the depth).
    auto given = Make(WithStrategy({.depth = 4, .mode = R::RollbackMode::Always}));
    Frames(*given, 9);
    Inbox(*given).Value<Health>(1, 1, Health{1});
    Inbox(*given).Value<Health>(1, 2, Health{2});
    Inbox(*given).ConfirmedThrough(2);
    Frames(*given, 1);
    const R::InboxColumn<Health>* abandoned = WorldOf(*given).Resource<R::RollbackInbox>()->Values<Health>();
    REQUIRE(abandoned != nullptr);
    CHECK_MESSAGE(!abandoned->At(1, 1).has_value(), "[rb-inbox-pruned-after-abandon]");
}

// ---- R-2: the histories' edges (review of step 8).

namespace {
// An exclusive System that, by phase, records the Score resource and the Command input into histories of its own,
// restores Score from its history, or writes an input back.
struct Probe {
    int phase = 0;  // 1 record, 2 restore Score, 3 write Command back
    std::uint64_t tick = 0;
};
Probe& ProbeState() {
    static Probe probe;
    return probe;
}
std::unique_ptr<R::ResourceHistory<Score>>& ScoreHistoryBox() {
    static std::unique_ptr<R::ResourceHistory<Score>> history;
    return history;
}
std::unique_ptr<R::InputHistory<Command>>& CommandHistoryBox() {
    static std::unique_ptr<R::InputHistory<Command>> history;
    return history;
}
R::ResourceHistory<Score>& ScoreHistory() {
    if (!ScoreHistoryBox()) ScoreHistoryBox() = std::make_unique<R::ResourceHistory<Score>>(nullptr);
    return *ScoreHistoryBox();
}
R::InputHistory<Command>& CommandHistory() {
    if (!CommandHistoryBox()) CommandHistoryBox() = std::make_unique<R::InputHistory<Command>>(3);
    return *CommandHistoryBox();
}
K::SystemResult RunProbe(K::ExclusiveWorld<> world) {
    const Probe& probe = ProbeState();
    if (probe.phase == 1) {
        ScoreHistory().Record(world, 0, probe.tick);
        CommandHistory().Record(world, probe.tick);
    } else if (probe.phase == 2) {
        K::RestoreSet set;
        ScoreHistory().AddTo(set, probe.tick, {}, nullptr);
        return world.Restore(std::move(set));
    } else if (probe.phase == 3) {
        CommandHistory().WriteBack(world, probe.tick);
    }
    return {};
}
std::unique_ptr<O::Product> ProbeProduct() {
    auto built = O::Product::Build(Game([](O::WorldSetup& s) {
                                       s.Registrar().Resource<Score>({.name = "Score"});
                                       s.Registrar().Resource<Command>({.name = "Command"});
                                       s.Registrar().AddSystem<&RunProbe>(In("RunProbe", "Main", "Update"));
                                   }),
                                   Units(false));
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    (*built)->World("Main").GetWorld().SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
    return std::move(*built);
}
void ProbeAt(O::Product& p, int phase, std::uint64_t tick) {
    ProbeState() = {phase, tick};
    Frames(p, 1);
    ProbeState() = {};
}
} // namespace

TEST_CASE("rollback strategy: R-2: a resource's history drops what a resimulation replaces and records an absence") {
    ScoreHistoryBox().reset();
    CommandHistoryBox().reset();
    auto p = ProbeProduct();
    K::World& world = p->World("Main").GetWorld();
    for (std::int32_t t = 1; t <= 3; ++t) {
        world.SetResource(Score{t});
        ProbeAt(*p, 1, static_cast<std::uint64_t>(t));
    }
    // Step 2 run again with another value: the old 2 and 3 go.
    world.SetResource(Score{20});
    ProbeAt(*p, 1, 2);
    ProbeAt(*p, 2, 2);
    CHECK_MESSAGE((world.Resource<Score>() != nullptr && world.Resource<Score>()->value == 20),
                  "[rb-resource-history-replaced] " << (world.Resource<Score>() ? world.Resource<Score>()->value : -1));
    // The resource gone at step 3: restoring step 3 removes it.
    world.RemoveResource<Score>();
    ProbeAt(*p, 1, 3);
    world.SetResource(Score{99});
    ProbeAt(*p, 2, 3);
    CHECK_MESSAGE(world.Resource<Score>() == nullptr, "[rb-resource-history-absence] Score came back");
}

TEST_CASE("rollback strategy: R-2: an input history of capacity n keeps the last n steps") {
    ScoreHistoryBox().reset();
    CommandHistoryBox().reset();
    auto p = ProbeProduct();
    K::World& world = p->World("Main").GetWorld();
    world.SetResource(Score{0});
    for (std::int32_t t = 1; t <= 4; ++t) {
        world.SetResource(Command{t});
        ProbeAt(*p, 1, static_cast<std::uint64_t>(t));
    }
    world.SetResource(Command{0});
    ProbeAt(*p, 3, 2);
    CHECK_MESSAGE(world.Resource<Command>()->move == 2, "[rb-input-capacity-edge] step 2 of the last 3: " << world.Resource<Command>()->move);
}

TEST_CASE("rollback strategy: R-2: the column history is pruned to the depth, and keeps an unchanged base") {
    auto p = Make(WithStrategy({.depth = 4}));
    Frames(*p, 12);
    const R::RollbackHistory* history = WorldOf(*p).Resource<R::RollbackHistory>();
    REQUIRE((history != nullptr && !history->columns.empty()));
    const auto* health = dynamic_cast<const R::ComponentHistory<Health>*>(history->columns.front().get());
    REQUIRE(health != nullptr);
    CHECK_MESSAGE(!health->At(1, 2).has_value(), "[rb-columns-pruned] step 2 is kept at depth 4, tick 12");
    CHECK_MESSAGE(health->At(1, 12).has_value(), "[rb-columns-pruned] the last step is gone");
    // A cell that has not changed since before the window keeps its one entry (the window's base).
    R::ComponentHistory<Health> still(nullptr);
    R::InboxColumn<Health> once;
    once.Put(1, 2, Health{5});
    const std::vector<K::EntityId> alive{1};
    still.Adopt(2, once, alive);
    still.Prune(10);
    CHECK_MESSAGE((still.At(1, 12).has_value() && still.At(1, 12)->hp == 5), "[rb-prune-keeps-unchanged-base]");
}

TEST_CASE("rollback strategy: R-8: the authoritative value is laid over the history at C") {
    auto p = Make(WithStrategy());
    Frames(*p, 9);
    Inbox(*p).Value<Health>(1, 5, Health{7});
    Inbox(*p).ConfirmedThrough(5);
    Frames(*p, 1);
    const R::RollbackHistory* history = WorldOf(*p).Resource<R::RollbackHistory>();
    REQUIRE((history != nullptr && !history->columns.empty()));
    const auto* health = dynamic_cast<const R::ComponentHistory<Health>*>(history->columns.front().get());
    REQUIRE(health != nullptr);
    CHECK_MESSAGE((health->At(1, 5).has_value() && health->At(1, 5)->hp == 7),
                  "[rb-history-adopted] step 5 in the history: " << (health->At(1, 5) ? health->At(1, 5)->hp : -1));
}

// ---- R-2 × InitialValue: a restore puts back what was recorded and derives nothing (review of step 8, A3;
// user 2026-11-22 (a2): notifications run, InitialValue does not).

namespace {
struct Req {
    std::int32_t v = 0;
    friend bool operator==(const Req&, const Req&) = default;
};
void HashValue(K::Hasher& h, const Req& r) { HashValue(h, r.v); }
struct Tgt {
    std::int32_t v = 0;
    friend bool operator==(const Tgt&, const Tgt&) = default;
};
void HashValue(K::Hasher& h, const Tgt& r) { HashValue(h, r.v); }
Tgt TgtFromReq(const Req& r) { return Tgt{r.v}; }

// Step 2 inserts Req (the kernel fills Tgt), step 4 removes Tgt; step 7 removes Req (R-2) or, in the death case,
// a Health at 0 despawns the entity.
K::SystemResult ReqScript(K::CommandsFor<K::Inserts<Req>, K::Removes<Tgt, Req>> c, K::Res<W::Tick> tick) {
    if (tick->value == 2) c.Insert<Req>(1, Req{5});
    if (tick->value == 4) c.Remove<Tgt>(1);
    return {};
}
K::SystemResult DropReqAtSeven(K::CommandsFor<K::Inserts<>, K::Removes<Req>> c, K::Res<W::Tick> tick) {
    if (tick->value == 7) c.Remove<Req>(1);
    return {};
}
K::SystemResult Wear(K::Query<K::Write<Health>> health) {
    health.ForEach([](K::EntityId, K::Mut<Health> hp) { hp->hp -= 1; });
    return {};
}
K::SystemResult DieAtZero(K::Despawn despawn, K::Query<K::Read<Health>> health) {
    health.ForEach([&despawn](K::EntityId entity, const Health& hp) {
        if (hp.hp <= 0) despawn.Entity(entity);
    });
    return {};
}
std::vector<std::string>& TgtAdds() {
    static std::vector<std::string> adds;
    return adds;
}
K::SystemResult TgtAdded(K::HookCtx ctx, K::Res<W::Resimulation> resimulation) {
    TgtAdds().push_back(std::to_string(ctx.Entity()) + (resimulation->active != 0 ? " resimulating" : ""));
    return {};
}
void RegisterReq(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    r.Component<Health>({.name = "Health"});
    r.Component<Req>({.name = "Req"});
    r.Component<Tgt>({.name = "Tgt"});
    r.InitialValue<Tgt, &TgtFromReq>();
    R::RollbackEntry rollback(s);
    rollback.Component<Health>();
    rollback.Component<Req>();
    rollback.Component<Tgt>();
    rollback.StepSchedule();
    r.AddSystem<&ReqScript>(In("ReqScript", "FixedUpdate", "Update"));
}
void BuildReqDrop(O::WorldSetup& s) {
    RegisterReq(s);
    s.Registrar().AddSystem<&DropReqAtSeven>(In("DropReqAtSeven", "FixedUpdate", "Update"));
}
void BuildReqDeath(O::WorldSetup& s) {
    RegisterReq(s);
    s.Registrar().AddSystem<&Wear>(In("Wear", "FixedUpdate", "PreUpdate"));
    s.Registrar().AddSystem<&DieAtZero>(In("DieAtZero", "FixedUpdate", "PostUpdate"));
    s.Registrar().Hook<Tgt, &TgtAdded>(K::HookTrigger::OnAdd, {.name = "TgtAdded"});
}
std::unique_ptr<O::Product> MakeReq(O::OptionBag options, void (*build)(O::WorldSetup&), std::int32_t hp) {
    auto built = O::Product::Build(Game(build, std::move(options)), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Main").GetWorld();
    world.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
    world.Insert(1, Health{hp});
    return std::move(*built);
}
} // namespace

TEST_CASE("rollback strategy: R-2: a restore puts back a request without filling its InitialValue target") {
    auto straight = MakeReq(WithStrategy(), &BuildReqDrop, 100);
    Frames(*straight, 10);
    auto rolled = MakeReq(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}), &BuildReqDrop, 100);
    Frames(*rolled, 9);
    REQUIRE((WorldOf(*rolled).Get<Req>(1) == nullptr && WorldOf(*rolled).Get<Tgt>(1) == nullptr));
    // Step 5 had Req and no Tgt.
    Inbox(*rolled).ConfirmedThrough(5);
    Frames(*rolled, 1);
    CHECK_MESSAGE(WorldOf(*rolled).Get<Tgt>(1) == nullptr, "[rb-restore-no-initial-value] the restore filled Tgt");
    CHECK_MESSAGE(SummaryText(*rolled) == SummaryText(*straight), "[rb-restore-no-initial-value] the World summaries differ");
}

TEST_CASE("rollback strategy: R-2: an entity a predicted death took comes back as recorded: no InitialValue, its hooks see resimulating") {
    // Health 7, one less each step: the prediction kills entity 1 at step 7. The authority says 100 at step 5.
    auto p = MakeReq(WithStrategy(), &BuildReqDeath, 7);
    Frames(*p, 9);
    REQUIRE(!WorldOf(*p).Alive(1));
    TgtAdds().clear();
    Inbox(*p).Value<Health>(1, 5, Health{100});
    Inbox(*p).ConfirmedThrough(5);
    Frames(*p, 1);
    REQUIRE_MESSAGE(WorldOf(*p).Alive(1), "[rb-revive-no-initial-value] entity 1 did not come back");
    CHECK_MESSAGE(WorldOf(*p).Get<Req>(1) != nullptr, "[rb-revive-no-initial-value] Req did not come back");
    CHECK_MESSAGE(WorldOf(*p).Get<Tgt>(1) == nullptr, "[rb-revive-no-initial-value] the revival filled Tgt");
    CHECK_MESSAGE(TgtAdds().empty(), "[rb-revive-no-initial-value] OnAdd of a Tgt nobody recorded: " << TgtAdds().size());
}

namespace {

// ---- A component input (net Module declaration D-11, section 5): Steer is on each entity, the entry writes each
// entity's next step, the step adds each entity's own Steer to its Health.
struct Steer {
    std::int32_t move = 0;
    friend bool operator==(const Steer&, const Steer&) = default;
};
inline void HashValue(K::Hasher& h, const Steer& v) { HashValue(h, v.move); }

K::SystemResult AdvanceSteered(K::Query<K::Read<Steer>, K::Write<Health>> rows) {
    rows.ForEach([](K::EntityId, const Steer& steer, K::Mut<Health> hp) { hp->hp += steer.move; });
    return {};
}
// The entry: each entity's Steer of the next step (Tick + 1), different per entity.
K::SystemResult FeedSteer(K::Query<K::Write<Steer>> steers, K::Res<W::Tick> tick) {
    steers.ForEach([&tick](K::EntityId id, K::Mut<Steer> steer) {
        steer->move = kScript[(tick->value + 1 + id) % kScript.size()] * static_cast<std::int32_t>(id);
    });
    return {};
}

void BuildSteerFeed(O::WorldSetup& s, bool feedFirst) {
    s.Registrar().Component<Health>({.name = "Health"});
    s.Registrar().Component<Steer>({.name = "Steer"});
    R::RollbackEntry rollback(s);
    rollback.Component<Health>();
    rollback.Input<Steer>(R::InputHistoryLength::StrategyDepth, R::DataKind::Component);
    rollback.StepSchedule("FixedUpdate", "Last");
    s.Registrar().AddSystem<&AdvanceSteered>(In("AdvanceSteered", "FixedUpdate", "Update"));
    K::SystemOptions feed = In("FeedSteer", "Main", "PreUpdate");
    const R::RollbackOptions* options = s.Options<R::RollbackOptions>();
    if (options != nullptr && options->strategy != nullptr) {
        if (feedFirst) {
            feed.mounts[0].before = {"RunRollback"};
        } else {
            feed.mounts[0].after = {"RunRollback"};
        }
    }
    rollback.SkipDuringResimulation(feed);
    s.Registrar().AddSystem<&FeedSteer>(feed);
}
void BuildSteer(O::WorldSetup& s) { BuildSteerFeed(s, false); }
void BuildSteerFirst(O::WorldSetup& s) { BuildSteerFeed(s, true); }

std::unique_ptr<O::Product> MakeSteer(O::OptionBag options, void (*build)(O::WorldSetup&)) {
    auto built = O::Product::Build(Game(build, std::move(options)), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Main").GetWorld();
    world.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
    world.Insert(1, Health{100});
    world.Insert(2, Health{50});
    world.Insert(1, Steer{});
    world.Insert(2, Steer{});
    return std::move(*built);
}

} // namespace

TEST_CASE("rollback strategy: R-2 with a component input: each entity's input is written back for each step run again") {
    auto straight = MakeSteer(WithStrategy(), &BuildSteer);
    Frames(*straight, 12);
    auto rolled = MakeSteer(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}), &BuildSteer);
    Frames(*rolled, 9);
    Inbox(*rolled).ConfirmedThrough(4);
    Frames(*rolled, 3);
    for (const K::EntityId id : {K::EntityId{1}, K::EntityId{2}}) {
        CAPTURE(id);
        CHECK_MESSAGE(WorldOf(*rolled).Get<Health>(id)->hp == WorldOf(*straight).Get<Health>(id)->hp,
                      "[rb-component-input-r2] " << WorldOf(*rolled).Get<Health>(id)->hp << " vs "
                                                 << WorldOf(*straight).Get<Health>(id)->hp);
    }
    CHECK_MESSAGE(SummaryText(*rolled) == SummaryText(*straight), "[rb-component-input-r2] the World summaries differ");
    const auto& inputs = WorldOf(*rolled).Resource<R::RollbackRegistry>()->inputs;
    CHECK_MESSAGE((inputs.size() == 1 && inputs[0].kind == R::DataKind::Component), "[rb-component-input-kind]");
}

TEST_CASE("rollback strategy: a component input's entry that runs before the rollback keeps this frame's input") {
    auto straight = MakeSteer(WithStrategy(), &BuildSteerFirst);
    Frames(*straight, 12);
    auto rolled = MakeSteer(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}), &BuildSteerFirst);
    Frames(*rolled, 9);
    Inbox(*rolled).ConfirmedThrough(4);
    Frames(*rolled, 3);
    for (const K::EntityId id : {K::EntityId{1}, K::EntityId{2}}) {
        CAPTURE(id);
        CHECK_MESSAGE(WorldOf(*rolled).Get<Health>(id)->hp == WorldOf(*straight).Get<Health>(id)->hp,
                      "[rb-component-input-put-back] " << WorldOf(*rolled).Get<Health>(id)->hp << " vs "
                                                       << WorldOf(*straight).Get<Health>(id)->hp);
    }
}
