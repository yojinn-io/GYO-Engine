// Scenario test segment 3 (logging declaration docs/architecture/target/v2/logging-declaration.zh-Hant.md,
// section 9.2; judgements 2 to 5): "relay", a game of two Worlds, Lobby and Match, with the time Module.
// Match moves a racer each frame and sends a Lap message; the game's own reader keeps what it read.
//
//   S3-1          entrance 3 (F1, judgement 2): Match breaks by a System's Err; whoever decides what follows
//                 (the product's main; here, the test) writes the reason through Base::Log, and the logging
//                 Module makes it one record in a MemorySink and in the real JSON lines file. What the Engine
//                 returns and whether it writes is the error levels' business (their tests), not this segment's.
//   S3-2          the healthy path (F2): Forward<TimeDiscardedLog> gets the time Module's message with the
//                 World's name; TakeKernelDiagnostics gets the Lap buffer's dropped records.
//   S3-3          determinism (judgement 4): the same run with logging off (no Forward, no label) and on
//                 (Debug and up, two sinks) leaves the game's own Resources and what its reader read equal,
//                 byte for byte.
//   S3-4          one category in its own file (F3; judgements 3 and 5): 1,000 records of relay.movement go
//                 only to their file, everything else only to the console; every line reads back; the counts
//                 are conserved. (Spans belong to the trace Module since revision 3: user, 2026-11-21.)
//
// A failure is classified by contract first (orchestration skeleton declaration 8.4): each message names the
// step and the contract it relies on.

#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
// MSVC: doctest messages stream std::string_view; that needs the full ostream.
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/base/Log.hpp"
#include "engine/json/Document.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/logging/ConsoleSink.hpp"
#include "engine/logging/JsonLinesFileSink.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/logging/Write.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/world_logging/Forward.hpp"
#include "engine/world_logging/TakeKernelDiagnostics.hpp"
#include "engine/world_time/Sampling.hpp"
#include "engine/world_time/TimeDiscardedLog.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::WorldTime;
namespace L = GYO::Engine::Logging;
namespace WL = GYO::Engine::WorldLogging;
namespace Json = GYO::Engine::Json;

// ---- the game

struct Lap final {
    std::int32_t racer = 0;
    std::int32_t frame = 0;
};
struct LapLog final {
    using Message = Lap;
    static constexpr std::string_view name = "Lap";
    static constexpr std::string_view category = "relay";
    static constexpr L::Level level = L::Level::Debug;
    static void Describe(const Lap& lap, L::RecordBuilder& out) {
        out.Text("lap");
        out.Add("racer", lap.racer);
        out.Add("frame", lap.frame);
    }
};

// The game's own state: fixed size, no padding (compared byte for byte).
struct RacerState final {
    std::int64_t x = 0;
    std::int64_t frames = 0;
};
struct LapsRead final {
    std::array<std::int32_t, 64> frames{};
    std::int32_t count = 0;
    std::int32_t reserved = 0;
};
struct LobbyFrames final {
    std::int64_t value = 0;
};
// How and at which frame Match breaks (0: never).
struct Breaker final {
    std::int32_t frame = 0;
    std::int32_t how = 0; // 1: returns Err, 2: throws
};
// Laps sent per frame (over the buffer's capacity of 2 gives dropped records).
struct LapBurst final {
    std::int32_t count = 1;
    std::int32_t reserved = 0;
};

K::SystemResult MoveRacer(K::ResMut<RacerState> racer, K::Res<Breaker> breaker, K::Res<LapBurst> burst,
                          K::MessageWriter<Lap> laps) {
    racer->x += 3;
    racer->frames += 1;
    const auto frame = static_cast<std::int32_t>(racer->frames);
    for (std::int32_t i = 0; i < burst->count; ++i) laps.Send(Lap{i, frame});
    if (breaker->frame == frame && breaker->how == 1) {
        return ::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "MoveRacer: the racer fell"));
    }
    if (breaker->frame == frame && breaker->how == 2) throw std::runtime_error("MoveRacer threw");
    return {};
}
K::SystemResult ReadLaps(K::ResMut<LapsRead> read, K::MessageReader<Lap> laps) {
    for (const Lap* lap : laps.Read()) {
        if (read->count < static_cast<std::int32_t>(read->frames.size())) read->frames[static_cast<std::size_t>(read->count++)] = lap->frame;
    }
    return {};
}
K::SystemResult CountLobby(K::ResMut<LobbyFrames> frames) {
    frames->value += 1;
    return {};
}

K::SystemOptions At(std::string name, std::string stage) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = "Main";
    m.stage = std::move(stage);
    o.mounts.push_back(std::move(m));
    return o;
}

struct RelayOptions {
    bool logging = true;
    Breaker breaker{};
    std::int32_t burst = 1;
    W::Nanoseconds delta = 16'000'000;
    O::Runner runner;
};

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

O::ProductDescription Relay(const RelayOptions& options) {
    O::ProductDescription d;
    d.name = "relay";
    O::WorldDescription lobby;
    lobby.name = "Lobby";
    lobby.build = [](O::WorldSetup& s) {
        s.Registrar().Resource<LobbyFrames>({.name = "LobbyFrames"});
        s.Registrar().AddSystem<&CountLobby>(At("CountLobby", "Update"));
    };
    lobby.prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(LobbyFrames{});
        return {};
    });
    d.worlds.push_back(std::move(lobby));

    O::WorldDescription match;
    match.name = "Match";
    const bool logging = options.logging;
    match.build = [logging](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.Resource<RacerState>({.name = "RacerState"});
        r.Resource<LapsRead>({.name = "LapsRead"});
        r.Resource<Breaker>({.name = "Breaker"});
        r.Resource<LapBurst>({.name = "LapBurst"});
        r.Message<Lap>({.name = "Lap", .capacity = 2});
        r.AddSystem<&MoveRacer>(At("MoveRacer", "Update"));
        r.AddSystem<&ReadLaps>(At("ReadLaps", "PostUpdate"));
        if (logging) {
            WL::Forward<LapLog>(s, {.schedule = "Main", .stage = "Last"});
            WL::Forward<W::TimeDiscardedLog>(s, {.schedule = "Main", .stage = "Last"});
        }
    };
    const Breaker breaker = options.breaker;
    const std::int32_t burst = options.burst;
    const W::Nanoseconds delta = options.delta;
    match.prepare.push_back([breaker, burst, delta](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(RacerState{});
        world.SetResource(LapsRead{});
        world.SetResource(breaker);
        world.SetResource(LapBurst{burst, 0});
        world.SetResource(W::TimeSampling{delta, W::SamplingMode::ManualDelta, {}});
        return {};
    });
    d.worlds.push_back(std::move(match));
    if (options.runner) {
        d.configure = [runner = options.runner](O::ProductSetup& setup) { setup.InstallRunner("relay", runner); };
    }
    return d;
}

std::unique_ptr<O::Product> Make(const RelayOptions& options) {
    auto built = O::Product::Build(Relay(options), TimeUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

// ---- the logging side

std::filesystem::path FilePath(const std::string& name) {
    // In the build directory, never the working directory (a run from the repository root left files there).
    const auto dir = std::filesystem::path(GYO_SCENARIO_TEST_BINARY_DIR) / "scenario-segment3-files";
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::filesystem::remove(path);
    return path;
}

std::vector<std::string> Lines(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    return lines;
}

struct Logs {
    std::shared_ptr<L::MemorySink> memory = std::make_shared<L::MemorySink>();
    std::filesystem::path file;
    std::optional<L::LoggerHandle> handle;

    explicit Logs(const std::string& fileName) : file(FilePath(fileName)) {
        L::LoggerOptions options;
        options.sinks.push_back({.name = "memory", .sink = memory, .filter = {.minimum = L::Level::Debug}});
        options.sinks.push_back({.name = "file",
                                 .sink = L::JsonLinesFileOptions{.path = file.string()},
                                 .filter = {.minimum = L::Level::Debug}});
        options.takeAssertions = false;
        options.takeTerminate = false;
        auto installed = L::Install(std::move(options));
        REQUIRE_MESSAGE(static_cast<bool>(installed), "logging: Install of the scenario's sinks (5.2)");
        handle.emplace(std::move(installed.value()));
    }

    // Records at Warning and above: what the Engine (and the product's main) said went wrong.
    [[nodiscard]] std::vector<L::StoredRecord> Problems() const {
        std::vector<L::StoredRecord> out;
        for (auto& record : memory->Records()) {
            if (record.level >= L::Level::Warning) out.push_back(std::move(record));
        }
        return out;
    }
};

// The line in the real file once the queue is written: one line at Warning and above, with this text.
void CheckOneProblemInFile(const std::filesystem::path& file, const std::string& text) {
    REQUIRE_MESSAGE(L::Flush(std::chrono::seconds{5}), "S3: the file sink wrote what it took (LG-6)");
    int problems = 0;
    for (const std::string& line : Lines(file)) {
        auto document = Json::Parse(line);
        REQUIRE_MESSAGE(static_cast<bool>(document), "S3: every line of the file reads back (LG-5)");
        const auto root = document.value().Root().AsObject().value();
        const auto level = root.Find("level");
        if (!level) continue;
        const auto name = level->AsString().value();
        if (name != "warning" && name != "error" && name != "fatal") continue;
        ++problems;
        CHECK(root.Find("text").value().AsString().value() == text);
    }
    CHECK_MESSAGE(problems == 1, "S3: the file holds the one line, once");
}

} // namespace

TEST_CASE("scenario segment 3: S3-1 a broken World's reason written through Base::Log becomes one record in both sinks") {
    Logs logs("s3-1.jsonl");
    RelayOptions options;
    options.breaker = {1, 1};
    auto product = Make(options);
    const O::RunnerExit exit = product->Run();
    REQUIRE(exit.code == O::RunnerExitCode::Failure);
    REQUIRE_FALSE(exit.reason.empty());
    const std::size_t before = logs.Problems().size();
    // Whoever decides what follows writes the reason (the category is its choice).
    ::Engine::Base::Log(::Engine::Base::LogLevel::Error, "relay", exit.reason);
    const auto problems = logs.Problems();
    REQUIRE_MESSAGE(problems.size() == before + 1, "S3-1: one Base::Log line became one record (entrance 3)");
    const L::StoredRecord& line = problems.back();
    CHECK(line.level == L::Level::Error);
    CHECK_FALSE(line.fatal);
    CHECK(line.category == "relay");
    CHECK(line.text == exit.reason);
    CheckOneProblemInFile(logs.file, exit.reason);
}

TEST_CASE("scenario segment 3: S3-2 the healthy path: TimeDiscarded through Forward, dropped Laps through TakeKernelDiagnostics") {
    Logs logs("s3-4.jsonl");
    RelayOptions options;
    options.delta = 1'000'000'000; // one real second in one frame: time is discarded
    options.burst = 5;             // five Laps into a buffer of two
    auto product = Make(options);
    REQUIRE(product->RunFrame());
    std::size_t time = 0;
    for (const auto& record : logs.memory->Records()) {
        if (record.category != "world_time") continue;
        ++time;
        CHECK_MESSAGE(record.world == "Match", "S3-2: Forward's records carry the World (5.9)");
        CHECK(record.Find("cause")->text == "StepLimit");
    }
    CHECK_MESSAGE(time == 1, "S3-2: one TimeDiscarded record (①)");
    CHECK(WL::TakeKernelDiagnostics(product->World("Match").GetWorld()) == 1);
    CHECK(WL::TakeKernelDiagnostics(product->World("Lobby").GetWorld()) == 0);
    std::size_t kernel = 0;
    for (const auto& record : logs.memory->Records()) {
        if (record.category != "kernel") continue;
        ++kernel;
        CHECK(record.world == "Match");
        CHECK(record.Find("message")->text == "Lap");
        CHECK(record.Find("count")->uintValue == 3);
    }
    CHECK_MESSAGE(kernel == 1, "S3-2: one dropped-messages record (②)");
}

TEST_CASE("scenario segment 3: S3-3 logging on or off leaves the game's own state and what its reader read the same") {
    struct Outcome {
        RacerState racer;
        LapsRead read;
        LobbyFrames lobby;
    };
    std::vector<Outcome> outcomes;
    for (const bool logging : {false, true}) {
        std::optional<Logs> logs;
        if (logging) logs.emplace("s3-5.jsonl");
        RelayOptions options;
        options.logging = logging;
        options.burst = 2;
        auto product = Make(options);
        for (int i = 0; i < 6; ++i) REQUIRE(product->RunFrame());
        K::World& match = product->World("Match").GetWorld();
        outcomes.push_back(Outcome{*match.Resource<RacerState>(), *match.Resource<LapsRead>(),
                                   *product->World("Lobby").GetWorld().Resource<LobbyFrames>()});
        if (logging) CHECK(logs->memory->Records().size() >= 12);
    }
    CHECK_MESSAGE(std::memcmp(&outcomes[0].racer, &outcomes[1].racer, sizeof(RacerState)) == 0,
                  "S3-3: the game's Resource is the same with logging on (LG-3, judgement 4)");
    CHECK(std::memcmp(&outcomes[0].read, &outcomes[1].read, sizeof(LapsRead)) == 0);
    CHECK(std::memcmp(&outcomes[0].lobby, &outcomes[1].lobby, sizeof(LobbyFrames)) == 0);
    CHECK(outcomes[0].read.count == 12);
}

TEST_CASE("scenario segment 3: S3-4 one category's records only in their own file, the rest only on the console") {
    const auto traceFile = FilePath("s3-4-movement.jsonl");
    std::FILE* console = std::tmpfile();
    REQUIRE(console != nullptr);
    {
        L::LoggerOptions options;
        options.sinks.push_back({.name = "console",
                                 .sink = std::make_shared<L::ConsoleSink>(console),
                                 .filter = {.exclude = {"relay.movement"}, .minimum = L::Level::Info}});
        // Opened here and kept, so that its counts can be read after the Logger is closed.
        auto opened = L::JsonLinesFileSink::Open({.path = traceFile.string()});
        REQUIRE(opened);
        std::shared_ptr<L::JsonLinesFileSink> trace = opened.value();
        options.sinks.push_back({.name = "movement", .sink = trace, .filter = {.include = {"relay.movement"}, .minimum = L::Level::Info}});
        options.takeAssertions = false;
        options.takeTerminate = false;
        auto handle = L::Install(std::move(options));
        REQUIRE(handle);
        const double specials[] = {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                                   -std::numeric_limits<double>::infinity(), -0.0, 1.25};
        for (int i = 0; i < 1000; ++i) {
            const std::string note = "player \"" + std::to_string(i % 8) + "\"\tqueued\n\\";
            if (i == 500) {
                L::Write(L::Level::Info, "relay.movement", "moved", {{"i", i}, {"i", i}});
            } else {
                L::Write(L::Level::Info, "relay.movement", "moved",
                         {{"i", i}, {"note", note}, {"value", specials[i % 5]}, {"schema_version", 1u}});
            }
        }
        for (int i = 0; i < 10; ++i) L::Write(L::Level::Info, "game", "not trace", {{"i", i}});
        handle.value().Close();
        const auto counts = trace->Counts();
        CHECK_MESSAGE(counts.written == 1000, "S3-4: 999 records and the Error record that replaced the one with a repeated key reached the file");
        CHECK(counts.droppedFull == 0);
        CHECK(counts.droppedClosed == 0);
    }
    const auto lines = Lines(traceFile);
    REQUIRE(lines.size() == 1001);
    int records = 0;
    int replaced = 0;
    for (std::size_t n = 0; n + 1 < lines.size(); ++n) {
        auto document = Json::Parse(lines[n]);
        REQUIRE_MESSAGE(static_cast<bool>(document), "S3-4: every line of the file reads back (LG-5)");
        const auto root = document.value().Root().AsObject().value();
        const std::string category(root.Find("category").value().AsString().value());
        if (category == "logging") {
            ++replaced;
            CHECK(root.Find("level").value().AsString().value() == "error");
            continue;
        }
        REQUIRE(category == "relay.movement");
        const auto fields = root.Find("fields").value().AsObject().value();
        const int i = static_cast<int>(fields.Find("i").value().AsInt64().value());
        ++records;
        CHECK(i != 500);
        CHECK(fields.Find("note").value().AsString().value() ==
              "player \"" + std::to_string(i % 8) + "\"\tqueued\n\\");
        const auto value = fields.Find("value").value();
        switch (i % 5) {
        case 0: CHECK(value.AsString().value() == "NaN"); break;
        case 1: CHECK(value.AsString().value() == "Infinity"); break;
        case 2: CHECK(value.AsString().value() == "-Infinity"); break;
        case 3: CHECK(std::signbit(value.AsDouble().value())); break;
        default: CHECK(value.AsDouble().value() == 1.25); break;
        }
    }
    CHECK(records == 999);
    CHECK_MESSAGE(replaced == 1, "S3-4: the sink that takes relay.movement got the replacement (5.2)");
    // The end line carries the counts (judgement 3).
    auto end = Json::Parse(lines.back());
    REQUIRE(end);
    const auto endRoot = end.value().Root().AsObject().value();
    CHECK(endRoot.Find("kind").value().AsString().value() == "end");
    CHECK(endRoot.Find("written").value().AsUint64().value() == 1000);

    std::rewind(console);
    std::string text;
    char buffer[4096];
    for (std::size_t got; (got = std::fread(buffer, 1, sizeof buffer, console)) > 0;) text.append(buffer, got);
    std::fclose(console);
    std::istringstream in(text);
    int game = 0;
    int logging = 0;
    for (std::string line; std::getline(in, line);) {
        CHECK_MESSAGE(line.find("[relay.movement]") == std::string::npos, "S3-4: no relay.movement record on the console (5.8)");
        if (line.starts_with("[game]")) ++game;
        if (line.starts_with("[logging] error:")) ++logging;
    }
    CHECK(game == 10);
    CHECK_MESSAGE(logging == 1, "S3-4: the record with a repeated key became one Error record of category logging (5.2)");
}
