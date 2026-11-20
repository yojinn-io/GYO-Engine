// Scenario test segment 3 (logging declaration docs/architecture/target/v2/logging-declaration.zh-Hant.md,
// section 9.2; judgements 2 to 5): "relay", a game of two Worlds, Lobby and Match, with the time Module.
// Match moves a racer each frame and sends a Lap message; the game's own reader keeps what it read.
//
//   S3-1 to S3-3  the fatal path (F1, judgement 2): Match breaks at frame 3 by (1) a System's Err, (2) a
//                 System's exception, (3) a DropOldFirst replacement that fails. The runner (WithFailureLog)
//                 or main writes the failure once and exits (user, 2026-11-20: "log it, then exit"); the
//                 record is in a MemorySink and in the real JSON lines file before the Logger is closed.
//   S3-4          the healthy path (F2): Forward<TimeDiscardedLog> gets the time Module's message with the
//                 World's name; TakeKernelDiagnostics gets the Lap buffer's dropped records.
//   S3-5          determinism (judgement 4): the same run with logging off (no Forward, no label) and on
//                 (Debug and up, two sinks) leaves the game's own Resources and what its reader read equal,
//                 byte for byte.
//   S3-6          trace (F3, F4; judgements 3 and 5): 1,000 trace records and 100 spans go only to the trace
//                 file, everything else only to the console; every line reads back; the counts are conserved.
//
// A failure is classified by contract first (orchestration skeleton declaration 8.4): each message names the
// step and the contract it relies on.

#include <doctest/doctest.h>

#include <array>
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

#include "engine/json/Document.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/logging/ConsoleSink.hpp"
#include "engine/logging/JsonLinesFileSink.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/logging/Span.hpp"
#include "engine/logging/Write.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/world_logging/Forward.hpp"
#include "engine/world_logging/TakeKernelDiagnostics.hpp"
#include "engine/world_logging/WriteFailure.hpp"
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
    o.zone = K::Zone::B;
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
        s.Registrar().Resource<LobbyFrames>({.name = "LobbyFrames", .zone = K::Zone::B});
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
        r.Resource<RacerState>({.name = "RacerState", .zone = K::Zone::B});
        r.Resource<LapsRead>({.name = "LapsRead", .zone = K::Zone::B});
        r.Resource<Breaker>({.name = "Breaker", .zone = K::Zone::B});
        r.Resource<LapBurst>({.name = "LapBurst", .zone = K::Zone::B});
        r.Message<Lap>({.name = "Lap", .zone = K::Zone::B, .capacity = 2});
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

// The runner of the scenario: frames until an entry point returns an error, then it returns (and
// WithFailureLog writes the failure and exits).
O::Runner FramesUntilBroken(int frames) {
    return [frames](O::Product& product) -> O::RunnerExit {
        for (int i = 0; i < frames; ++i) {
            if (!product.RunFrame()) return {O::RunnerExitCode::Failure, ""};
        }
        return {};
    };
}

// ---- the logging side

std::filesystem::path FilePath(const std::string& name) {
    const auto dir = std::filesystem::current_path() / "scenario-segment3-files";
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

    [[nodiscard]] std::vector<L::StoredRecord> Fatal() const {
        std::vector<L::StoredRecord> out;
        for (auto& record : memory->Records()) {
            if (record.fatal) out.push_back(std::move(record));
        }
        return out;
    }
};

// The fatal record against the WorldFailure, field by field (judgement 2).
void CheckFatalRecord(const L::StoredRecord& record, const O::WorldFailure& failure) {
    CHECK_MESSAGE(record.level == L::Level::Fatal, "S3: the record of a WorldFailure is Fatal (5.6)");
    CHECK(record.category == "world");
    CHECK(record.product == failure.product);
    CHECK(record.world == failure.world);
    CHECK(record.text == failure.Text());
    CHECK(record.Find("kind")->text == O::ToString(failure.kind));
    CHECK(record.Find("entry")->text == failure.entry);
    CHECK(record.Find("schedule")->text == failure.schedule);
    CHECK(record.Find("sync_epoch")->uintValue == failure.syncEpoch);
    CHECK(record.Find("where")->text == failure.where);
    CHECK(record.Find("last_schedule")->text == failure.lastSchedule);
}

// The fatal line in the real file, before the Logger is closed (the fatal output writes at once).
void CheckFatalInFile(const std::filesystem::path& file, const O::WorldFailure& failure) {
    int fatalLines = 0;
    for (const std::string& line : Lines(file)) {
        auto document = Json::Parse(line);
        REQUIRE_MESSAGE(static_cast<bool>(document), "S3: every line of the file reads back (LG-5)");
        const auto root = document.value().Root().AsObject().value();
        const auto level = root.Find("level");
        if (!level || level->AsString().value() != "fatal") continue;
        ++fatalLines;
        CHECK(root.Find("world").value().AsString().value() == failure.world);
        CHECK(root.Find("product").value().AsString().value() == failure.product);
        CHECK(root.Find("text").value().AsString().value() == failure.Text());
    }
    CHECK_MESSAGE(fatalLines == 1, "S3: the fatal line is in the file before Close, once (5.6 step 3)");
}

} // namespace

TEST_CASE("scenario segment 3: S3-1 a System's Err at frame 3: the runner logs the failure once and exits") {
    Logs logs("s3-1.jsonl");
    RelayOptions options;
    options.breaker = {3, 1};
    options.runner = WL::WithFailureLog(FramesUntilBroken(10));
    auto product = Make(options);
    const auto exit = product->Run();
    CHECK(exit.code == O::RunnerExitCode::Failure);
    REQUIRE(product->Failure() != nullptr);
    CHECK(product->Failure()->world == "Match");
    const auto fatal = logs.Fatal();
    REQUIRE_MESSAGE(fatal.size() == 1, "S3-1: one fatal record (5.6; log it, then exit)");
    CheckFatalRecord(fatal[0], *product->Failure());
    CHECK(fatal[0].Find("kernel_code")->text == "SystemFailed");
    CheckFatalInFile(logs.file, *product->Failure());
}

TEST_CASE("scenario segment 3: S3-2 a System's exception at frame 3: written from Product::Failure(), then exit") {
    Logs logs("s3-2.jsonl");
    RelayOptions options;
    options.breaker = {3, 2};
    options.runner = WL::WithFailureLog(FramesUntilBroken(10));
    auto product = Make(options);
    O::RunnerExit exit;
    CHECK_NOTHROW(exit = product->Run());
    CHECK(exit.code == O::RunnerExitCode::Failure);
    REQUIRE(product->Failure() != nullptr);
    const auto fatal = logs.Fatal();
    REQUIRE(fatal.size() == 1);
    CheckFatalRecord(fatal[0], *product->Failure());
    CheckFatalInFile(logs.file, *product->Failure());
}

TEST_CASE("scenario segment 3: S3-3 a DropOldFirst replacement that fails: the orchestration warning, then main logs the failure") {
    Logs logs("s3-3.jsonl");
    auto product = Make(RelayOptions{});
    for (int i = 0; i < 2; ++i) REQUIRE(product->RunFrame());
    O::ReplaceRequest request;
    request.order = O::ReplaceOrder::DropOldFirst;
    request.extra = [](K::World&) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        return ::Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "the save is broken",
                                                         {}, "save-broken"));
    };
    auto replaced = product->ReplaceWorld("Match", std::move(request));
    REQUIRE_FALSE(replaced);
    REQUIRE(replaced.error().failure.has_value());
    // main: log it, then exit.
    static_cast<void>(WL::WriteFailure(*replaced.error().failure));
    const auto records = logs.memory->Records();
    std::size_t warning = records.size();
    std::size_t fatal = records.size();
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (records[i].category == "orchestration" && records[i].level == L::Level::Warning && warning == records.size()) warning = i;
        if (records[i].fatal && fatal == records.size()) fatal = i;
    }
    REQUIRE_MESSAGE(warning < records.size(), "S3-3: the orchestration layer's Base::Log warning became a record (③)");
    REQUIRE(fatal < records.size());
    CHECK_MESSAGE(warning < fatal, "S3-3: the warning comes before the fatal record");
    CHECK(records[fatal].Find("kind")->text == "ReplacementFailed");
    CheckFatalRecord(records[fatal], *replaced.error().failure);
    CheckFatalInFile(logs.file, *replaced.error().failure);
}

TEST_CASE("scenario segment 3: S3-4 the healthy path: TimeDiscarded through Forward, dropped Laps through TakeKernelDiagnostics") {
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
        CHECK_MESSAGE(record.world == "Match", "S3-4: Forward's records carry the World (5.9)");
        CHECK(record.Find("cause")->text == "StepLimit");
    }
    CHECK_MESSAGE(time == 1, "S3-4: one TimeDiscarded record (①)");
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
    CHECK_MESSAGE(kernel == 1, "S3-4: one dropped-messages record (②)");
}

TEST_CASE("scenario segment 3: S3-5 logging on or off leaves the game's own state and what its reader read the same") {
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
                  "S3-5: the game's Resource is the same with logging on (LG-3, judgement 4)");
    CHECK(std::memcmp(&outcomes[0].read, &outcomes[1].read, sizeof(LapsRead)) == 0);
    CHECK(std::memcmp(&outcomes[0].lobby, &outcomes[1].lobby, sizeof(LobbyFrames)) == 0);
    CHECK(outcomes[0].read.count == 12);
}

TEST_CASE("scenario segment 3: S3-6 trace records and spans only in the trace file, the rest only on the console") {
    const auto traceFile = FilePath("s3-6-trace.jsonl");
    std::FILE* console = std::tmpfile();
    REQUIRE(console != nullptr);
    {
        L::LoggerOptions options;
        options.sinks.push_back({.name = "console",
                                 .sink = std::make_shared<L::ConsoleSink>(console),
                                 .filter = {.exclude = {"trace."}, .minimum = L::Level::Info}});
        // Opened here and kept, so that its counts can be read after the Logger is closed.
        auto opened = L::JsonLinesFileSink::Open({.path = traceFile.string()});
        REQUIRE(opened);
        std::shared_ptr<L::JsonLinesFileSink> trace = opened.value();
        options.sinks.push_back({.name = "trace", .sink = trace, .filter = {.include = {"trace."}, .minimum = L::Level::Info}});
        options.takeAssertions = false;
        options.takeTerminate = false;
        auto handle = L::Install(std::move(options));
        REQUIRE(handle);
        const double specials[] = {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                                   -std::numeric_limits<double>::infinity(), -0.0, 1.25};
        for (int i = 0; i < 1000; ++i) {
            const std::string note = "player \"" + std::to_string(i % 8) + "\"\tqueued\n\\";
            if (i == 500) {
                L::Write(L::Level::Info, "trace.movement", "moved", {{"i", i}, {"i", i}});
            } else {
                L::Write(L::Level::Info, "trace.movement", "moved",
                         {{"i", i}, {"note", note}, {"value", specials[i % 5]}, {"schema_version", 1u}});
            }
        }
        for (int i = 0; i < 100; ++i) {
            L::Span span("trace.net", "send");
            span.End({{"i", i}});
        }
        for (int i = 0; i < 10; ++i) L::Write(L::Level::Info, "game", "not trace", {{"i", i}});
        handle.value().Close();
        const auto counts = trace->Counts();
        CHECK_MESSAGE(counts.written == 1099, "S3-6: 999 trace records and 100 spans reached the trace file");
        CHECK(counts.droppedFull == 0);
        CHECK(counts.droppedClosed == 0);
    }
    const auto lines = Lines(traceFile);
    REQUIRE(lines.size() == 1100);
    int records = 0;
    int spans = 0;
    for (std::size_t n = 0; n + 1 < lines.size(); ++n) {
        auto document = Json::Parse(lines[n]);
        REQUIRE_MESSAGE(static_cast<bool>(document), "S3-6: every trace line reads back (LG-5)");
        const auto root = document.value().Root().AsObject().value();
        const std::string category(root.Find("category").value().AsString().value());
        REQUIRE(category.starts_with("trace."));
        const auto fields = root.Find("fields").value().AsObject().value();
        const int i = static_cast<int>(fields.Find("i").value().AsInt64().value());
        if (category == "trace.net") {
            ++spans;
            CHECK(root.Find("kind").value().AsString().value() == "span");
            CHECK(root.Find("duration_ns").value().AsInt64().value() >= 0);
            continue;
        }
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
    CHECK(spans == 100);
    // The end line carries the counts (judgement 3).
    auto end = Json::Parse(lines.back());
    REQUIRE(end);
    const auto endRoot = end.value().Root().AsObject().value();
    CHECK(endRoot.Find("kind").value().AsString().value() == "end");
    CHECK(endRoot.Find("written").value().AsUint64().value() == 1099);

    std::rewind(console);
    std::string text;
    char buffer[4096];
    for (std::size_t got; (got = std::fread(buffer, 1, sizeof buffer, console)) > 0;) text.append(buffer, got);
    std::fclose(console);
    std::istringstream in(text);
    int game = 0;
    int logging = 0;
    for (std::string line; std::getline(in, line);) {
        CHECK_MESSAGE(line.find("[trace.") == std::string::npos, "S3-6: no trace record on the console (5.13)");
        if (line.starts_with("[game]")) ++game;
        if (line.starts_with("[logging] error:")) ++logging;
    }
    CHECK(game == 10);
    CHECK_MESSAGE(logging == 1, "S3-6: the record with a repeated key became one Error record of category logging (5.2)");
}
