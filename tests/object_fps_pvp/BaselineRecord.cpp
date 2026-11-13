// v8-product-pvp-baseline: a report-only record of today's Match cost, the
// denominator of the kernel migration budget
// (docs/object_fps_pvp/plans/v8/v8-product-pvp-baseline-declaration.md).
// Deliberately not a CTest: the shared runner's microsecond timing is noise;
// it is run by hand and judges nothing. It includes the authority digest
// runner, so the replay scenarios are the digest's own, and checks every golden
// digest against the fixture, so a changed scenario cannot pass as the same one.
//
//   --replay --out FILE [--samples FILE]   per-step time of the golden scenarios
//                                          and of the four-player long scenario,
//                                          and the World bytes of the long one
//   --load --out FILE                      product arena load to the first Tick
#include <chrono>
#include <cstdint>
#include <vector>

namespace pvp_baseline {
// Null while nothing is recorded (warm-up, untimed ticks).
std::vector<std::int64_t>* recording = nullptr;
void Observe(std::chrono::steady_clock::duration took) {
    if (recording) recording->push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(took).count());
}
} // namespace pvp_baseline

// Only the authority step is inside the clock: the digest's snapshot encoding,
// SHA-256 and the scripted input stay outside it.
#define PVP_DIGEST_NO_MAIN 1
#define PVP_DIGEST_TIMED_STEP(...)                                                                    \
    do {                                                                                              \
        const auto pvpBaselineStarted = std::chrono::steady_clock::now();                             \
        __VA_ARGS__;                                                                                  \
        ::pvp_baseline::Observe(std::chrono::steady_clock::now() - pvpBaselineStarted);               \
    } while (false)
#include "AuthorityDigest.cpp"

#include <iomanip>

namespace fps::pvp {
// The friend accessor of PvpMatch (EntityModelTests.cpp defines its own in
// another executable): reads the columns' sizes, never writes.
struct PvpMatchTestAccess final {
    struct Bytes final {
        std::size_t entities{};
        std::size_t entityColumns{}; // bodies_, combats_, animations_: rows x element size
        std::size_t entityCapacity{}; // the same columns' reserved capacity x element size
        std::size_t inputDense{};    // actionLedgers_, connections_: rows x element size
        std::size_t inputNested{};   // their maps and deques (estimate, see NodeOverhead)
        std::size_t tickCaches{};    // tickShapes_ (with each entity's shapes), actionStartCombats_
    };
    // libc++ map node header (three pointers and the colour, padded); an
    // estimate written into the record's header, not a measured allocation.
    static constexpr std::size_t NodeOverhead = 32;
    // The animations_ element (id, value and the state it advanced from), not the snapshot's PlayerAnimation.
    static constexpr std::size_t AnimationTrackBytes = sizeof(PvpMatch::AnimationTrack);
    static Bytes Measure(const PvpMatch& match) {
        Bytes bytes;
        const auto dense = [](const auto& column) { return column.size() * sizeof(column.front()); };
        const auto reserved = [](const auto& column) { return column.capacity() * sizeof(column.front()); };
        bytes.entities = match.bodies_.size();
        bytes.entityColumns = dense(match.bodies_) + dense(match.combats_) + dense(match.animations_);
        bytes.entityCapacity = reserved(match.bodies_) + reserved(match.combats_) + reserved(match.animations_);
        bytes.inputDense = dense(match.actionLedgers_) + dense(match.connections_);
        for (const auto& ledger : match.actionLedgers_)
            bytes.inputNested += ledger.actions.size() * (sizeof(*ledger.actions.begin()) + NodeOverhead);
        for (const auto& connection : match.connections_) {
            bytes.inputNested += connection.commands.size() * (sizeof(*connection.commands.begin()) + NodeOverhead);
            bytes.inputNested += connection.backlogSamples.size() * sizeof(std::uint32_t);
            bytes.inputNested += connection.fallbackSamples.size() * sizeof(bool);
        }
        bytes.tickCaches = dense(match.tickShapes_) + dense(match.actionStartCombats_);
        for (const auto& entity : match.tickShapes_) bytes.tickCaches += dense(entity.shapes);
        return bytes;
    }
};
} // namespace fps::pvp

namespace {

constexpr unsigned LongPlayers = MaxPlayers; // the contract's room capacity (Movement.hpp)
constexpr unsigned LongWarmupTicks = 600;
constexpr unsigned LongMeasuredTicks = 3600;

struct Summary final {
    std::size_t ticks{};
    double p50{}, p95{}, p99{}, max{}, mean{}; // microseconds
};

// Nearest rank: the smallest sample with at least q of all samples at or below it.
Summary Summarize(std::vector<std::int64_t> samples) {
    Summary s;
    s.ticks = samples.size();
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    const auto rank = [&](double q) {
        auto index = static_cast<std::size_t>(std::ceil(q * static_cast<double>(samples.size())));
        index = std::clamp<std::size_t>(index, 1, samples.size());
        return static_cast<double>(samples[index - 1]) / 1000.0;
    };
    s.p50 = rank(0.50);
    s.p95 = rank(0.95);
    s.p99 = rank(0.99);
    s.max = static_cast<double>(samples.back()) / 1000.0;
    double total = 0;
    for (const auto value : samples) total += static_cast<double>(value);
    s.mean = total / static_cast<double>(samples.size()) / 1000.0;
    return s;
}

std::map<std::string, std::string> ReadGolden(const std::string& path) {
    std::ifstream file{path};
    Require(bool(file), "Cannot read golden file " + path);
    std::map<std::string, std::string> expected;
    for (std::string line; std::getline(file, line);) {
        if (line.empty() || line.front() == '#') continue;
        std::istringstream fields(line);
        std::string name, value;
        Require(bool(fields >> name >> value), "Malformed golden line: " + line);
        expected[name] = value;
    }
    return expected;
}

void Header(std::ostream& out, std::string_view kind) {
    out << "# format=gyo.kernel-baseline, version=1\n"
        << "# kind=" << kind << " tool=gyo_object_fps_pvp_baseline_record build_type=" << PVP_BASELINE_BUILD_TYPE
        << " platform=" << PVP_BASELINE_PLATFORM << " digest_version=" << DigestVersion << '\n';
}

// The four-player long scenario: every player moves, jumps and crouches on
// its own phase and fires aimed shots at the next player in bursts, so lives
// end and restart. Not part of the digest's golden set (aimed shots use
// non-zero angles); its digest is recorded so a later replay host can show it
// ran the same scenario.
struct LongResult final {
    std::vector<std::int64_t> samples;
    std::string digest;
    std::uint64_t records{};
    fps::pvp::PvpMatchTestAccess::Bytes atStart, peak, atEnd;
    std::uint64_t deaths{}, shots{};
};

LongResult RunLong() {
    LongResult result;
    Digest digest(false, false, nullptr);
    std::vector<PlayerId> ids;
    for (PlayerId id = 1; id <= LongPlayers; ++id) ids.push_back(id);
    Duel duel(NamedArena("product"), digest, ids);
    const auto total = LongWarmupTicks + LongMeasuredTicks;
    for (unsigned t = 0; t < total; ++t) {
        for (const auto id : ids) {
            const unsigned u = t + 97 * id;
            Intent intent = Axial(u, 1 + id % 2);
            intent.jump = u % 120 == 7;
            intent.crouch = (u / 300) % 4 == 3;
            duel.Move(id, intent);
            const PlayerId target = id % LongPlayers + 1;
            if (u % 15 == 0 && (u / 240) % 2 == 0) {
                const auto [yaw, pitch] = duel.Aim(id, target);
                duel.Act(id, ActionKind::Shot, yaw, pitch);
                ++result.shots;
            }
            if (u % 400 == 399) duel.Act(id, ActionKind::Reload);
        }
        pvp_baseline::recording = t >= LongWarmupTicks ? &result.samples : nullptr;
        duel.Tick();
        pvp_baseline::recording = nullptr;
        for (const auto id : ids)
            if (duel.State(id).lifeState == LifeState::Dead && duel.State(id).lifeStateTick == duel.Match().TickCount())
                ++result.deaths;
        const auto bytes = fps::pvp::PvpMatchTestAccess::Measure(duel.Match());
        if (t + 1 == LongWarmupTicks) result.atStart = bytes;
        const auto sum = [](const auto& b) { return b.entityColumns + b.inputDense + b.inputNested + b.tickCaches; };
        if (sum(bytes) > sum(result.peak)) result.peak = bytes;
        result.atEnd = bytes;
    }
    result.digest = digest.Value();
    result.records = digest.Count();
    return result;
}

// RunLink's tick schedule (AuthorityDigest.cpp), the same arithmetic.
unsigned ExpectedLinkTicks(unsigned scale) {
    double nextTick = 1000.0 / 60.0;
    unsigned ticks{};
    for (int millisecond = 0; millisecond <= 6000 * static_cast<int>(scale); ++millisecond) {
        if (millisecond + 0.000001 >= nextTick) {
            nextTick += 1000.0 / 60.0;
            ++ticks;
        }
    }
    return ticks;
}

void Row(std::ostream& out, std::string_view section, std::string_view name, std::string_view group, const Summary& s,
         std::string_view digest) {
    out << section << ',' << name << ',' << group << ',' << s.ticks << ',' << std::fixed << std::setprecision(3) << s.p50
        << ',' << s.p95 << ',' << s.p99 << ',' << s.max << ',' << s.mean << ',' << digest << '\n';
}

int RunReplay(const std::string& outPath, const std::optional<std::string>& samplesPath) {
    const auto golden = ReadGolden(PVP_BASELINE_GOLDEN);
    const auto scenarios = Scenarios();
    // Warm-up: one untimed pass over the golden set (code, allocator, caches).
    for (const auto& s : scenarios) {
        if (!s.golden) continue;
        Digest digest(true, false, nullptr);
        s.run(1, digest);
    }
    std::ofstream out{outPath};
    std::optional<std::ofstream> raw;
    if (samplesPath) raw.emplace(*samplesPath);
    Header(out, "replay-tick");
    out << "# step=PvpMatch::Tick (group match) or MatchRuntimeHost::Advance (group host); warm-up: one untimed pass "
           "of the golden set, and the first "
        << LongWarmupTicks << " ticks of long4\n"
        << "section,name,group,ticks,p50_us,p95_us,p99_us,max_us,mean_us,digest\n";
    if (raw) *raw << "name,index,ns\n";
    std::vector<std::int64_t> match, host, all;
    unsigned checked{};
    for (const auto& s : scenarios) {
        if (!s.golden) continue;
        std::vector<std::int64_t> samples;
        Digest digest(true, false, nullptr);
        pvp_baseline::recording = &samples;
        s.run(1, digest);
        pvp_baseline::recording = nullptr;
        const auto expected = golden.find(s.name);
        Require(expected != golden.end() && expected->second == digest.Value(), "golden mismatch: " + s.name);
        // Count reconciliation: one timed step per digest record, except in the
        // prediction scenarios, whose frames are records too: there the Match
        // ticks on RunLink's own schedule, recounted here.
        const bool isLink = s.name.rfind("prediction-", 0) == 0;
        Require(samples.size() == (isLink ? ExpectedLinkTicks(1) : digest.Count()),
                "timed steps differ from the scenario's ticks: " + s.name);
        ++checked;
        const bool isHost = s.name.rfind("host-", 0) == 0;
        Row(out, "scenario", s.name, isHost ? "host" : "match", Summarize(samples), digest.Value());
        if (raw)
            for (std::size_t i = 0; i < samples.size(); ++i) *raw << s.name << ',' << i << ',' << samples[i] << '\n';
        (isHost ? host : match).insert((isHost ? host : match).end(), samples.begin(), samples.end());
        all.insert(all.end(), samples.begin(), samples.end());
    }
    Require(checked == golden.size(), "the golden file names scenarios outside the golden set");
    const auto longRun = RunLong();
    Require(longRun.samples.size() == LongMeasuredTicks, "long4 timed steps differ from its measured ticks");
    Require(longRun.records == LongWarmupTicks + LongMeasuredTicks, "long4 records differ from its ticks");
    if (raw)
        for (std::size_t i = 0; i < longRun.samples.size(); ++i) *raw << "long4," << i << ',' << longRun.samples[i] << '\n';
    Row(out, "scenario", "long4/product", "match", Summarize(longRun.samples), longRun.digest);
    Row(out, "group", "golden-match", "match", Summarize(match), "");
    Row(out, "group", "golden-host", "host", Summarize(host), "");
    Row(out, "group", "golden-all", "all", Summarize(all), "");
    out << "# golden_scenarios=" << checked << " long4_players=" << LongPlayers << " long4_shots=" << longRun.shots
        << " long4_deaths=" << longRun.deaths << '\n';
    out << "# world_bytes: map node overhead assumed " << fps::pvp::PvpMatchTestAccess::NodeOverhead
        << " B (estimate); entity_columns = bodies_ + combats_ + animations_ (rows x element size)\n"
        << "world,point,entities,entity_columns,entity_capacity,input_dense,input_nested_estimate,tick_caches,total\n";
    for (const auto& [point, b] : {std::pair{"long4-start", longRun.atStart}, std::pair{"long4-peak", longRun.peak},
                                   std::pair{"long4-end", longRun.atEnd}})
        out << "world," << point << ',' << b.entities << ',' << b.entityColumns << ',' << b.entityCapacity << ','
            << b.inputDense << ',' << b.inputNested << ',' << b.tickCaches << ','
            << b.entityColumns + b.inputDense + b.inputNested + b.tickCaches << '\n';
    out << "# element_bytes PlayerState=" << sizeof(PlayerState) << " CombatState=" << sizeof(CombatState)
        << " AnimationTrack=" << fps::pvp::PvpMatchTestAccess::AnimationTrackBytes << '\n';
    return out && (!raw || *raw) ? 0 : 1;
}

// From the first statement of this mode: the product arena file read, the
// Match built with the room's players, and its first Tick done. Process
// start-up before main is outside it.
int RunLoad(const std::string& outPath) {
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    auto arena = Load(PVP_DIGEST_PRODUCT_ARENA);
    const auto loaded = Clock::now();
    PvpMatch match(std::move(arena));
    std::string error;
    for (PlayerId id = 1; id <= LongPlayers; ++id) Require(match.Join(id, error), "join: " + error);
    const auto built = Clock::now();
    match.Tick({1, TickSeconds});
    const auto ticked = Clock::now();
    Require(match.TickCount() == 1, "the first Tick did not run");
    const auto micros = [](Clock::duration d) {
        return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(d).count()) / 1000.0;
    };
    std::ofstream out{outPath};
    Header(out, "replay-load");
    out << "load_us,build_join_us,first_tick_us,total_us,players\n"
        << std::fixed << std::setprecision(3) << micros(loaded - started) << ',' << micros(built - loaded) << ','
        << micros(ticked - built) << ',' << micros(ticked - started) << ',' << LongPlayers << '\n';
    return out ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string_view> args(argv + 1, argv + argc);
        const auto option = [&](std::string_view name) -> std::optional<std::string> {
            for (std::size_t i = 0; i + 1 < args.size(); ++i) if (args[i] == name) return std::string(args[i + 1]);
            return std::nullopt;
        };
        const auto flag = [&](std::string_view name) { return std::find(args.begin(), args.end(), name) != args.end(); };
        const auto out = option("--out");
        if (flag("--replay") && out) return RunReplay(*out, option("--samples"));
        if (flag("--load") && out) return RunLoad(*out);
        std::cout << "gyo_object_fps_pvp_baseline_record --replay --out FILE [--samples FILE] | --load --out FILE\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "baseline record: " << error.what() << '\n';
        return 1;
    }
}
