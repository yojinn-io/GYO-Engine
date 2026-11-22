// The budget measurement of the rollback Module (authority-rollback declaration
// 5.4.5 and judgement 5; run by hand on the user's Linux host next to the
// research card's floor_bench, not a ctest).
//
// One process measures the three sizes of judgement 5 with the Module's
// configuration of 5.4.5: 16 component types of 64 bytes each, and
//   P1  32 entities (32 KB of state that rolls back), depth 20;
//   P2 128 entities (128 KB), depth 32;
//   P3   2 entities (2 KB), depth 20.
// For each size, on one World with the time Module and the rollback Module
// (official strategy, mode Always). The step schedule has one System only, the
// measuring System MarkAll: it marks every cell of every entity changed, in every
// step, resimulated steps included (5.4.5: "the step schedule has only a System
// that marks every cell changed"); no author System runs.
//   rb_record_<P>        the strategy's Record of one step (every cell changed);
//                        MarkAll is outside the clock;
//   rb_mark_<P>          MarkAll alone, one step (recorded, and subtracted below);
//   rb_rollback_raw_<P>  one rollback of depth steps, from Decide (the raise of
//                        Resimulation) to the end of Resimulate: Restore, Tick back,
//                        depth steps each with MarkAll and a record, the lowering;
//   rb_rollback_<P>      judgement 5 item 2's value per rollback: raw minus depth x
//                        the median of rb_mark_<P> in this process.
// Each case gets samples (default 1000) one-at-a-time samples, written as one
// CSV row in the research card's format (case,unit,n,batch,p50,p95,p99,max,mean;
// nanoseconds per operation), so analyze.py and judge_budget.py read it.
//
// Usage: gyo_rollback_budget <out.csv> [samples]

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/rollback/RollbackModule.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Rollback;
namespace W = GYO::Engine::WorldTime;

constexpr W::Nanoseconds kStep = 15'625'000;

template <int I>
struct Cell64 {
    std::array<std::uint8_t, 64> bytes{};
    friend bool operator==(const Cell64&, const Cell64&) = default;
};
template <int I>
void HashValue(K::Hasher& h, const Cell64<I>& v) {
    K::HashAsBytes(h, v.bytes);
}

double NowNs() {
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

// What the timed strategy measured (the System that calls it runs on this thread).
struct Timings {
    std::vector<double> record;
    std::vector<double> rollback;
    std::vector<double> mark;
    bool resimulating = false;
    double decideStart = 0;
    double rollbackStart = 0;
};
Timings& Measured() {
    static Timings timings;
    return timings;
}

// The official strategy with a clock around Record, and from Decide to the end of Resimulate (R-1: inheriting it
// and overriding some hooks is the same socket). Decide raises Resimulation when it rolls back, so the clock of a
// rollback starts at the Decide that led to the Restore.
class TimedStrategy final : public R::StateRollbackStrategy {
public:
    using R::StateRollbackStrategy::StateRollbackStrategy;
    ::Engine::Base::Result<void, K::KernelError> Record(R::RecordContext& context) override {
        if (Measured().resimulating) return R::StateRollbackStrategy::Record(context);
        const double start = NowNs();
        auto done = R::StateRollbackStrategy::Record(context);
        Measured().record.push_back(NowNs() - start);
        return done;
    }
    ::Engine::Base::Result<void, K::KernelError> Decide(R::DecideContext& context) override {
        Measured().decideStart = NowNs();
        return R::StateRollbackStrategy::Decide(context);
    }
    ::Engine::Base::Result<void, K::KernelError> Restore(R::RestoreContext& context) override {
        Measured().resimulating = true;
        Measured().rollbackStart = Measured().decideStart;
        return R::StateRollbackStrategy::Restore(context);
    }
    ::Engine::Base::Result<void, K::KernelError> Resimulate(R::ResimulateContext& context) override {
        auto done = R::StateRollbackStrategy::Resimulate(context);
        Measured().rollback.push_back(NowNs() - Measured().rollbackStart);
        Measured().resimulating = false;
        return done;
    }
};

// The measuring System: every cell of every entity changed, every step (not skipped while resimulating).
template <int... I>
K::SystemResult MarkAll(K::Query<K::Write<Cell64<I>>...> cells) {
    const double start = NowNs();
    cells.ForEach([](K::EntityId, K::Mut<Cell64<I>>... cell) { (++cell->bytes[0], ...); });
    Measured().mark.push_back(NowNs() - start);
    return {};
}
template <int... I>
constexpr auto MarkAllOf(std::integer_sequence<int, I...>) {
    return &MarkAll<I...>;
}

template <int I>
void RegisterType(O::WorldSetup& s, R::RollbackEntry& rollback) {
    s.Registrar().Component<Cell64<I>>({.name = "Cell" + std::to_string(I)});
    rollback.Component<Cell64<I>>();
}

template <int... I>
void RegisterTypes(O::WorldSetup& s, R::RollbackEntry& rollback, std::integer_sequence<int, I...>) {
    (RegisterType<I>(s, rollback), ...);
    K::SystemOptions mark;
    mark.name = "MarkAll";
    mark.mounts.push_back(K::Mount{"FixedUpdate", "Update", {}, {}, {}, {}});
    s.Registrar().AddSystem<MarkAllOf(std::integer_sequence<int, I...>{})>(mark);
}

template <int I>
void InsertCell(K::World& world, K::EntityId entity) {
    world.Insert(entity, Cell64<I>{});
}
template <int... I>
void InsertCells(K::World& world, K::EntityId entity, std::integer_sequence<int, I...>) {
    (InsertCell<I>(world, entity), ...);
}

O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    row.depends = std::move(depends);
    return row;
}

struct Size {
    const char* name;
    int entities;
    std::uint32_t depth;
};

std::vector<double> Sorted(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    return samples;
}
double Percentile(const std::vector<double>& sorted, double q) {
    auto rank = static_cast<std::size_t>(std::ceil(q * static_cast<double>(sorted.size())));
    if (rank == 0) rank = 1;
    return sorted[rank - 1];
}
void Row(std::FILE* out, const std::string& name, const std::vector<double>& samples) {
    const std::vector<double> s = Sorted(samples);
    double sum = 0;
    for (double v : s) sum += v;
    std::fprintf(out, "%s,ns_per_op,%zu,1,%.1f,%.1f,%.1f,%.1f,%.1f\n", name.c_str(), s.size(), Percentile(s, 0.50),
                 Percentile(s, 0.95), Percentile(s, 0.99), s.back(), sum / static_cast<double>(s.size()));
    std::fflush(out);
}

bool Measure(const Size& size, std::size_t samples, std::FILE* out) {
    O::RegisteredUnits units;
    units.rows.push_back(ModuleRow("world_time"));
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    units.rows.push_back(ModuleRow("rollback", {"world_time"}));
    units.modules.push_back({"rollback", std::make_unique<R::RollbackModule>()});
    O::ProductDescription d;
    d.name = "budget";
    O::WorldDescription w;
    w.name = "Main";
    w.options.Set(R::RollbackOptions{std::make_shared<TimedStrategy>(R::StateRollbackSettings{size.depth, R::RollbackMode::Always})});
    w.build = [](O::WorldSetup& s) {
        R::RollbackEntry rollback(s);
        RegisterTypes(s, rollback, std::make_integer_sequence<int, 16>{});
        rollback.StepSchedule("FixedUpdate", "Last");
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), std::move(units));
    if (!built) {
        std::fprintf(stderr, "build failed: %s\n", built.error().detail.c_str());
        return false;
    }
    O::Product& product = **built;
    K::World& world = product.World("Main").GetWorld();
    world.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
    for (int e = 1; e <= size.entities; ++e) InsertCells(world, static_cast<K::EntityId>(e), std::make_integer_sequence<int, 16>{});
    const auto frame = [&product]() {
        auto ran = product.RunFrame();
        if (!ran) std::fprintf(stderr, "frame failed: %s\n", ran.error().message.c_str());
        return static_cast<bool>(ran);
    };
    // Warm up past the depth, then record samples (one step a frame; every cell changes in each).
    for (std::uint32_t i = 0; i < size.depth + 10; ++i) {
        if (!frame()) return false;
    }
    Measured() = Timings{};
    while (Measured().record.size() < samples) {
        if (!frame()) return false;
    }
    Row(out, std::string("rb_record_") + size.name, Measured().record);
    // One rollback of depth steps per frame: the confirmed point is always depth steps behind.
    Measured() = Timings{};
    while (Measured().rollback.size() < samples) {
        const std::uint64_t tick = world.Resource<W::Tick>()->value;
        world.ResourceMut<R::RollbackInbox>()->ConfirmedThrough(tick - size.depth);
        if (!frame()) return false;
    }
    // Judgement 5 item 2: each rollback minus depth x the median of MarkAll alone in this process.
    const std::vector<double> marks = Sorted(Measured().mark);
    const double markMedian = Percentile(marks, 0.50);
    std::vector<double> net;
    net.reserve(Measured().rollback.size());
    for (double raw : Measured().rollback) net.push_back(raw - static_cast<double>(size.depth) * markMedian);
    Row(out, std::string("rb_mark_") + size.name, Measured().mark);
    Row(out, std::string("rb_rollback_raw_") + size.name, Measured().rollback);
    Row(out, std::string("rb_rollback_") + size.name, net);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <out.csv> [samples]\n", argv[0]);
        return 2;
    }
    const std::size_t samples = argc > 2 ? static_cast<std::size_t>(std::strtoul(argv[2], nullptr, 10)) : 1000;
    std::FILE* out = std::fopen(argv[1], "w");
    if (out == nullptr) {
        std::perror(argv[1]);
        return 2;
    }
    std::fprintf(out, "case,unit,n,batch,p50,p95,p99,max,mean\n");
    const Size sizes[] = {{"P1", 32, 20}, {"P2", 128, 32}, {"P3", 2, 20}};
    bool ok = true;
    for (const Size& size : sizes) ok = Measure(size, samples, out) && ok;
    std::fclose(out);
    return ok ? 0 : 1;
}
