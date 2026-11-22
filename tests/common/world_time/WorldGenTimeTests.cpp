// Time while a World generates (world-gen declaration docs/architecture/target/v2/world-gen-declaration.zh-Hant.md,
// section 5.6 and judgement 1 case C4; asset decision 9: what the decisions need is loaded while the World generates,
// and only then does the tick start). The time Module is not changed: the author picks one of two ways. A generation
// System holds until a World-outside flag (an asset that arrived) is set; "ManualDelta" sampling of one step per
// frame, so the frame counts are exact.
#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::WorldTime;

namespace {

constexpr W::Nanoseconds kStep = 15'625'000;  // 64 Hz

struct Arrived {
    std::int32_t on = 0;
};
inline void HashValue(K::Hasher& h, const Arrived& v) { HashValue(h, v.on); }

struct FrameRecord {
    std::uint64_t tick = 0;
    int notDriven = 0;
    int discarded = 0;
};
std::vector<FrameRecord>& Frames() {
    static std::vector<FrameRecord> frames;
    return frames;
}

// Gen: holds until the asset has arrived.
K::SystemResult WaitForAsset(K::Res<Arrived> arrived, K::ResMut<O::WorldGeneration> generation) {
    if (arrived->on == 0) generation->Hold();
    return {};
}

// Game: the first frame after generation turns the pause off (the pausing way, section 5.6).
K::SystemResult Unpause(K::ResMut<W::TimeControl> control) {
    control->paused = 0;
    return {};
}

// Main/Last, every frame: the tick and this frame's TimeDiscarded messages.
K::SystemResult RecordFrame(K::Res<W::Tick> tick, K::MessageReader<W::TimeDiscarded> discarded) {
    FrameRecord record;
    record.tick = tick->value;
    for (const W::TimeDiscarded* d : discarded.Read()) {
        ++record.discarded;
        if (d->cause == W::DiscardCause::NotDriven) ++record.notDriven;
    }
    Frames().push_back(record);
    return {};
}

K::SystemOptions At(std::string name, std::string stage, std::vector<std::string> sets = {}) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::string(O::DefaultSchedules::frame);
    m.stage = std::move(stage);
    m.sets = std::move(sets);
    o.mounts.push_back(std::move(m));
    return o;
}

O::RegisteredUnits TimeUnits() {
    O::RegisteredUnits units;
    O::RegisterRow row;
    row.name = "world_time";
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    units.rows.push_back(row);
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    return units;
}

// pausing: the description's prepare pauses time and the Game set unpauses it; otherwise (pausable false) the
// fixed step has the run condition AfterGenerating().
std::unique_ptr<O::Product> Make(bool pausing) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    W::WorldTimeOptions options;
    if (!pausing) {
        options.pausable = false;
        options.fixedStepConditions = {O::AfterGenerating()};
    }
    w.options.Set(std::move(options));
    w.build = [pausing](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.Resource<Arrived>({.name = "Arrived"});
        const std::string main(O::DefaultSchedules::frame);
        r.Set(K::SetOptions{.name = "Gen", .schedule = main}, {O::WhileGenerating()});
        r.Set(K::SetOptions{.name = "Game", .schedule = main}, {O::AfterGenerating()});
        r.AddSystem<&WaitForAsset>(At("WaitForAsset", std::string(O::DefaultSchedules::update), {"Gen"}));
        if (pausing) r.AddSystem<&Unpause>(At("Unpause", std::string(O::DefaultSchedules::update), {"Game"}));
        r.AddSystem<&RecordFrame>(At("RecordFrame", std::string(O::DefaultSchedules::last)));
    };
    w.prepare.push_back([pausing](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
        world.SetResource(Arrived{});
        if (pausing) world.SetResource(W::TimeControl{1.0, 1});
        return {};
    });
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), TimeUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

void Frame(O::Product& p) {
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}

// Runs k generating frames (the asset arrives before frame k) and two more.
void RunAround(O::Product& p, int k) {
    Frames().clear();
    for (int frame = 1; frame <= k + 2; ++frame) {
        if (frame == k) p.World("Main").GetWorld().SetResource(Arrived{1});
        Frame(p);
        if (frame < k) REQUIRE(p.Generating("Main"));
    }
    REQUIRE(!p.Generating("Main"));
    REQUIRE(Frames().size() == static_cast<std::size_t>(k + 2));
}

}  // namespace

TEST_CASE("generation: paused in the prepare function, no tick and no time discarded until generation has ended") {
    constexpr int k = 4;
    auto p = Make(true);
    RunAround(*p, k);
    // Frames 1..k generate; frame k+1 is the first after generation (Unpause runs after AdvanceTime); k+2 ticks.
    for (int i = 0; i < k + 1; ++i) {
        CHECK_MESSAGE(Frames()[i].tick == 0, "[wg-c4-paused-no-tick] frame " << (i + 1) << " ticked");
        CHECK_MESSAGE(Frames()[i].discarded == 0, "[wg-c4-paused-no-discard] frame " << (i + 1) << " discarded time");
    }
    CHECK(Frames()[k + 1].tick == 1);
    const W::RealTime* real = p->World("Main").GetWorld().Resource<W::RealTime>();
    REQUIRE(real != nullptr);
    CHECK(real->elapsed == (k + 2) * kStep);  // real time went on while the World generated
}

TEST_CASE("generation: a World that cannot pause keeps the fixed step off with AfterGenerating") {
    constexpr int k = 4;
    auto p = Make(false);
    RunAround(*p, k);
    // Frames 1..k generate; the fixed step runs from frame k+1. Each untaken frame is discarded by the next
    // AdvanceTime (NotDriven): k records, on frames 2..k+1.
    for (int i = 0; i < k; ++i) CHECK_MESSAGE(Frames()[i].tick == 0, "[wg-c4-cond-no-tick] frame " << (i + 1) << " ticked");
    CHECK(Frames()[0].notDriven == 0);
    int notDriven = 0;
    for (int i = 1; i <= k; ++i) {
        CHECK(Frames()[i].notDriven == 1);
        notDriven += Frames()[i].notDriven;
    }
    CHECK(notDriven == k);
    CHECK(Frames()[k].tick == 1);
    CHECK(Frames()[k + 1].tick == 2);
}
