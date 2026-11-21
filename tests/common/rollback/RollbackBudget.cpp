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
// (official strategy, mode Always):
//   rb_record_<P>    the strategy's Record of one step in which every cell
//                    changed (16 Systems each write one type of every entity);
//   rb_rollback_<P>  one rollback of depth steps with every cell changed:
//                    Restore plus Resimulate (Tick back, depth records, the
//                    raise and the lowering of Resimulation; the writing Systems
//                    are skipped while resimulating, so no author System runs).
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
    bool resimulating = false;
    double rollbackStart = 0;
};
Timings& Measured() {
    static Timings timings;
    return timings;
}

// The official strategy with a clock around Record, and around Restore + Resimulate (R-1: inheriting it and
// overriding some hooks is the same socket).
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
    ::Engine::Base::Result<void, K::KernelError> Restore(R::RestoreContext& context) override {
        Measured().resimulating = true;
        Measured().rollbackStart = NowNs();
        return R::StateRollbackStrategy::Restore(context);
    }
    ::Engine::Base::Result<void, K::KernelError> Resimulate(R::ResimulateContext& context) override {
        auto done = R::StateRollbackStrategy::Resimulate(context);
        Measured().rollback.push_back(NowNs() - Measured().rollbackStart);
        Measured().resimulating = false;
        return done;
    }
};

template <int I>
K::SystemResult Touch(K::Query<K::Write<Cell64<I>>> cells) {
    cells.ForEach([](K::EntityId, K::Mut<Cell64<I>> cell) { ++cell->bytes[0]; });
    return {};
}

template <int I>
void RegisterType(O::WorldSetup& s, R::RollbackEntry& rollback) {
    s.Registrar().Component<Cell64<I>>({.name = "Cell" + std::to_string(I)});
    rollback.Component<Cell64<I>>();
    K::SystemOptions touch;
    touch.name = "Touch" + std::to_string(I);
    touch.mounts.push_back(K::Mount{"FixedUpdate", "Update", {}, {}, {}, {}});
    rollback.SkipDuringResimulation(touch);
    s.Registrar().AddSystem<&Touch<I>>(touch);
}

template <int... I>
void RegisterTypes(O::WorldSetup& s, R::RollbackEntry& rollback, std::integer_sequence<int, I...>) {
    (RegisterType<I>(s, rollback), ...);
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
    world.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta, {}});
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
    Row(out, std::string("rb_rollback_") + size.name, Measured().rollback);
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
