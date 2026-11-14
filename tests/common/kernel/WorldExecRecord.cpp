// BU-2 record executable (migration plan section 6, BU-2; world-exec
// declaration judgement 8): the kernel's own overhead on a synthetic Registry.
// Not a ctest: shared-runner timings are noise; run it by hand in Release and
// keep its output under build/evidence/kernel-world-exec/. The numbers are
// recorded, not judged (the declaration: the gate machine is a Linux container,
// U8; the budget is re-approved under U12).
//
// Shape (BU-2's "合成" column): 400 entities x 8 zone C columns x 20 (c) Systems
// with empty bodies (each fetches a two-column Query), four stages; one more
// (c) System spawns 4 and despawns 4 entities per tick (deferred, applied at the
// stage's sync point). Reported:
//   run       one Run of the whole schedule: p50, p95, max over the runs
//   dispatch  per System call: (run with 20 Systems - run with none) / 20
//   sync      per empty sync point: a schedule of 16 empty stages / 16
//   columns   the sync point that applies 4 spawns (8 components each) and 4
//             despawns, at 400 and at 4,000 entities: p95
//   verify    run p95 with verification on, at 400 and 4,000 entities, and the
//             slope per KiB of column bytes (spec A.6 layer 2 is linear)
//
// Usage: gyo_kernel_world_exec_record [runs]   (default 100000)

#include "engine/kernel/Kernel.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace GYO::Kernel;

template <int I>
struct Col {
    std::int32_t a;
    std::int32_t b;
};

SystemResult Ok() { return {}; }

template <int I>
SystemResult Empty(Query<Read<Col<I % 8>>, Read<Col<(I + 1) % 8>>>) {
    return Ok();
}

// Spawns 4 and despawns the 4 oldest ids (ids in [base, base + count) are alive).
struct Churn {
    std::uint64_t next;
    std::uint64_t oldest;
};

SystemResult ChurnSystem(SpawnWithId<Bundle<Col<0>, Col<1>, Col<2>, Col<3>, Col<4>, Col<5>, Col<6>, Col<7>>> spawn,
                         Despawn despawn, ResMut<Churn> churn) {
    for (int i = 0; i < 4; ++i) {
        const EntityId id = churn->next++;
        spawn.New(id, Col<0>{}, Col<1>{}, Col<2>{}, Col<3>{}, Col<4>{}, Col<5>{}, Col<6>{}, Col<7>{});
        if (auto r = despawn.Entity(churn->oldest++); !r) return r;
    }
    return Ok();
}

class FunctionPlugin final : public Plugin {
public:
    explicit FunctionPlugin(void (*build)(Registrar&, int), int arg) : build_(build), arg_(arg) {}
    [[nodiscard]] std::string_view Name() const override { return "record"; }
    void Build(Registrar& registrar) override { build_(registrar, arg_); }

private:
    void (*build_)(Registrar&, int);
    int arg_;
};

template <int I>
void RegisterColumn(Registrar& r) {
    r.Component<Col<I>>({.name = "Col" + std::to_string(I), .zone = Zone::C, .rollback = Rollback::Yes});
}

void Columns(Registrar& r) {
    RegisterColumn<0>(r);
    RegisterColumn<1>(r);
    RegisterColumn<2>(r);
    RegisterColumn<3>(r);
    RegisterColumn<4>(r);
    RegisterColumn<5>(r);
    RegisterColumn<6>(r);
    RegisterColumn<7>(r);
}

ScheduleOptions Schedule(int stages) {
    ScheduleOptions s;
    s.name = "Tick";
    s.domain = Domain::Fixed;
    s.clock = true;
    for (int i = 0; i < stages; ++i) s.stages.push_back("S" + std::to_string(i));
    return s;
}

SystemOptions Opt(std::string name, std::string stage) {
    SystemOptions o;
    o.name = std::move(name);
    o.zone = Zone::C;
    Mount m;
    m.schedule = "Tick";
    m.stage = std::move(stage);
    o.mounts.push_back(std::move(m));
    return o;
}

template <int I>
void AddEmpty(Registrar& r) {
    r.AddSystem<&Empty<I>>(Opt("E" + std::to_string(I), "S" + std::to_string(I % 4)));
}

template <int... I>
void AddEmpties(Registrar& r, std::integer_sequence<int, I...>) {
    (AddEmpty<I>(r), ...);
}

// mode 0: 20 empty Systems + churn; 1: no System (4 stages); 2: 16 empty stages; 3: churn only.
void Build(Registrar& r, int mode) {
    Columns(r);
    r.Resource<Churn>({.name = "Churn", .zone = Zone::C, .rollback = Rollback::Yes});
    if (mode == 2) {
        r.Schedule(Schedule(16));
        return;
    }
    r.Schedule(Schedule(4));
    if (mode == 0) AddEmpties(r, std::make_integer_sequence<int, 20>{});
    if (mode == 0 || mode == 3) r.AddSystem<&ChurnSystem>(Opt("Churn", "S3"));
}

struct Stats {
    double p50 = 0;
    double p95 = 0;
    double max = 0;
};

Stats Measure(int mode, std::uint64_t entities, int runs, bool verification) {
    FunctionPlugin plugin(&Build, mode);
    Registry registry({&plugin});
    if (!registry.Finish()) {
        std::fprintf(stderr, "Finish failed (mode %d)\n", mode);
        std::exit(2);
    }
    Product product(registry, "record");
    World& world = product.GetWorld();
    world.SetResource(KernelSettings{verification, false});
    for (std::uint64_t id = 0; id < entities; ++id) {
        world.Insert(id, Col<0>{});
        world.Insert(id, Col<1>{});
        world.Insert(id, Col<2>{});
        world.Insert(id, Col<3>{});
        world.Insert(id, Col<4>{});
        world.Insert(id, Col<5>{});
        world.Insert(id, Col<6>{});
        world.Insert(id, Col<7>{});
    }
    world.SetResource(Churn{entities, 0});
    std::vector<double> micros;
    micros.reserve(static_cast<std::size_t>(runs));
    const int warmup = std::min(runs / 10, 1000);
    for (int i = 0; i < runs + warmup; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const auto result = product.Run("Tick");
        const auto end = std::chrono::steady_clock::now();
        if (!result) {
            std::fprintf(stderr, "Run failed: %s\n", result.error().Text().c_str());
            std::exit(2);
        }
        if (i >= warmup) micros.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    }
    std::sort(micros.begin(), micros.end());
    Stats stats;
    stats.p50 = micros[micros.size() / 2];
    stats.p95 = micros[std::min(micros.size() - 1, micros.size() * 95 / 100)];
    stats.max = micros.back();
    return stats;
}

void Print(const char* label, const Stats& s) {
    std::printf("%-44s p50 %10.2f us  p95 %10.2f us  max %10.2f us\n", label, s.p50, s.p95, s.max);
}

} // namespace

int main(int argc, char** argv) {
    const int runs = argc > 1 ? std::atoi(argv[1]) : 100000;
    const int columnRuns = std::max(1, runs / 10);
#if defined(NDEBUG)
    std::printf("BU-2 record (NDEBUG defined), runs %d, column runs %d\n", runs, columnRuns);
#else
    std::printf("BU-2 record (NDEBUG not defined: not a Release build), runs %d, column runs %d\n", runs, columnRuns);
#endif
    const Stats full = Measure(0, 400, runs, false);
    const Stats none = Measure(1, 400, runs, false);
    const Stats syncs = Measure(2, 400, runs, false);
    Print("run: 400 x 8 cols, 20 (c) Systems + churn", full);
    Print("run: same columns, no System (4 stages)", none);
    std::printf("dispatch per System (p50 difference / 21)   %10.3f us\n", (full.p50 - none.p50) / 21.0);
    std::printf("empty sync point (16 stages, p50 / 16)      %10.3f us\n", syncs.p50 / 16.0);
    const Stats churn400 = Measure(3, 400, columnRuns, false);
    const Stats churn4000 = Measure(3, 4000, columnRuns, false);
    Print("columns: churn run at 400 entities", churn400);
    Print("columns: churn run at 4000 entities", churn4000);
    // Layer 2 copies and compares every column at every System: far slower, so fewer runs.
    const int verifyRuns = std::max(1, runs / 500);
    std::printf("verify runs %d\n", verifyRuns);
    const Stats verify400 = Measure(0, 400, verifyRuns, true);
    const Stats verify4000 = Measure(0, 4000, verifyRuns, true);
    Print("verify on: 400 entities", verify400);
    Print("verify on: 4000 entities", verify4000);
    // Column bytes: 8 columns x cells of (id, added, changed, 8-byte value) = 32 bytes.
    const double kib400 = 400.0 * 8 * 32 / 1024.0;
    const double kib4000 = 4000.0 * 8 * 32 / 1024.0;
    std::printf("verify slope (p50, per KiB of column bytes)  %10.3f us/KiB\n",
                (verify4000.p50 - verify400.p50) / (kib4000 - kib400));
    return 0;
}
