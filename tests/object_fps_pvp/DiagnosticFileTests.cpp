#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "RetroFPS/Pvp/LogFile.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#include "RetroFPS/Pvp/NativeFiles.hpp"
#include "RetroFPS/Pvp/TracePaths.hpp"
#include "RetroFPS/Pvp/MatchIngressTrace.hpp"
#include "RetroFPS/Pvp/MatchShotTrace.hpp"

#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace fps::pvp;

namespace {
std::filesystem::path DiagnosticPath(const std::string& name) {
    const auto directory = std::filesystem::temp_directory_path() / "gyo_pvp_diagnostic_files";
    std::filesystem::create_directories(directory);
    const auto path = directory / (name + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::remove(path);
    return path;
}

// Text mode, as the writers write: the file has "\r\n" on Windows, before
// and after the move onto the trace sink.
std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string ReadBinary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<std::string> ReadLines(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);) lines.push_back(line);
    return lines;
}

// Three events covering every field, both bool values, every source but None
// and the double formats (17 significant digits, exponents, integers).
std::vector<MovementTraceEvent> GoldenEvents() {
    MovementTraceEvent a{};
    a.kind = MovementTraceKind::Resolved; a.timeNs = 123456789012345; a.playerId = 3; a.epoch = 7; a.sequence = 42;
    a.authorityTick = 9001; a.source = MovementInputSource::Held; a.queued = 2; a.pending = 5; a.seededNeutral = true;
    a.droppedSeconds = 0.1; a.frameSeconds = 1.0 / 3.0; a.count = 18446744073709551615ull; a.ageSeconds = 1e-9;
    a.resetReason = MovementResetReason::Starvation; a.startedNs = -5; a.lifeGeneration = 4;
    MovementTraceEvent b{};
    b.kind = MovementTraceKind::LifecycleCancelled; b.timeNs = 1; b.frameSeconds = 3.0; b.ageSeconds = 2.5e10;
    b.resetReason = MovementResetReason::LifeRespawn; b.source = MovementInputSource::Neutral;
    MovementTraceEvent c{};
    c.kind = MovementTraceKind::Sent; c.timeNs = 99; c.source = MovementInputSource::Actual; c.droppedSeconds = 0.016666666666666666;
    c.resetReason = MovementResetReason::SequenceExhausted;
    MovementTraceEvent d{};
    d.kind = MovementTraceKind::PhaseDecision; d.timeNs = 7; d.playerId = 2; d.epoch = 3; d.sequence = 77; d.count = 30;
    d.phasePath = MovementPhasePath::Window; d.phaseErrorSeconds = 0.0123; d.phaseShiftSeconds = 0.0123;
    d.phaseDeadbandSeconds = 0.002;
    return {a, b, c, d};
}

// Written by the movement trace writer before it moved onto the Engine trace
// sink (master 0fabb52, std::ofstream with setprecision(17)): the file format
// must not change with the transport. Schema 3 (v7.1 D152): the same bytes with
// the version number 3, plus the phase_decision kind, the only one with the
// four phase_* keys after started_ns.
const std::vector<std::string> GoldenLines{
    R"G({"schema_version":3,"kind":"resolved","time_ns":123456789012345,"player_id":3,"life_generation":4,"epoch":7,"sequence":42,"authority_tick":9001,"source":"held","queued":2,"pending":5,"seeded_neutral":true,"dropped_seconds":0.10000000000000001,"frame_seconds":0.33333333333333331,"count":18446744073709551615,"age_seconds":1.0000000000000001e-09,"reset_reason":"starvation","started_ns":-5})G",
    R"G({"schema_version":3,"kind":"lifecycle_cancelled","time_ns":1,"player_id":0,"life_generation":1,"epoch":1,"sequence":0,"authority_tick":0,"source":"neutral","queued":0,"pending":0,"seeded_neutral":false,"dropped_seconds":0,"frame_seconds":3,"count":0,"age_seconds":25000000000,"reset_reason":"life_respawn","started_ns":0})G",
    R"G({"schema_version":3,"kind":"sent","time_ns":99,"player_id":0,"life_generation":1,"epoch":1,"sequence":0,"authority_tick":0,"source":"actual","queued":0,"pending":0,"seeded_neutral":false,"dropped_seconds":0.016666666666666666,"frame_seconds":0,"count":0,"age_seconds":0,"reset_reason":"sequence_exhausted","started_ns":0})G",
    R"G({"schema_version":3,"kind":"phase_decision","time_ns":7,"player_id":2,"life_generation":1,"epoch":3,"sequence":77,"authority_tick":0,"source":"none","queued":0,"pending":0,"seeded_neutral":false,"dropped_seconds":0,"frame_seconds":0,"count":30,"age_seconds":0,"reset_reason":"none","started_ns":0,"phase_path":"window","phase_error_seconds":0.0123,"phase_shift_seconds":0.0123,"phase_deadband_seconds":0.002})G",
    R"G({"schema_version":3,"kind":"trace_end","events":4,"dropped":0})G",
};
} // namespace

TEST_CASE("PvP movement trace lines keep the bytes of the schema version 3 writer") {
    const auto events = GoldenEvents();
    REQUIRE(events.size() + 1 == GoldenLines.size());
    for (std::size_t k = 0; k < events.size(); ++k) CHECK(MovementTraceLine(events[k]) == GoldenLines[k]);
}

// client-interrupted (log-contract §1.4): a resume that restarts phase
// acquisition, count 0, the cancelled remainder as the shift.
TEST_CASE("PvP movement trace writes a resume reset as phase_path resume_reset") {
    MovementTraceEvent e{};
    e.kind = MovementTraceKind::PhaseDecision; e.timeNs = 8; e.playerId = 2; e.epoch = 3; e.sequence = 90;
    e.phasePath = MovementPhasePath::ResumeReset; e.phaseShiftSeconds = -0.0125;
    CHECK(MovementTraceLine(e) ==
          R"G({"schema_version":3,"kind":"phase_decision","time_ns":8,"player_id":2,"life_generation":1,"epoch":3,"sequence":90,"authority_tick":0,"source":"none","queued":0,"pending":0,"seeded_neutral":false,"dropped_seconds":0,"frame_seconds":0,"count":0,"age_seconds":0,"reset_reason":"none","started_ns":0,"phase_path":"resume_reset","phase_error_seconds":0,"phase_shift_seconds":-0.012500000000000001,"phase_deadband_seconds":0})G");
}

TEST_CASE("PvP movement trace file keeps its bytes and ends with trace_end") {
    const auto path = DiagnosticPath("movement");
    {
        MovementTraceWriter writer(path);
        for (const auto& event : GoldenEvents()) TraceMovement(event);
        writer.Finish();
        CHECK(writer.Good());
        // After Finish nothing reaches the file.
        TraceMovement(GoldenEvents().front());
    }
    std::string expected;
    for (const auto& line : GoldenLines) expected += line + "\n";
    CHECK(ReadFile(path) == expected);
}

TEST_CASE("PvP log file lines keep their timestamp format and order, appended") {
    const auto path = DiagnosticPath("log");
    { std::ofstream(path) << "earlier run\n"; }
    {
        // Declared first: the LogFile restores its buffer when destroyed.
        std::ostringstream console;
        LogFile log(path);
        log.Tee(console);
        log.Write("first");
        console << "second\nthird" << std::endl;
        log.Write("fourth");
        CHECK(log.FlushNow());
        // Written while the log is still open.
        CHECK(ReadLines(path).size() == 5);
        CHECK(console.str() == "second\nthird\n");
    }
    const auto lines = ReadLines(path);
    REQUIRE(lines.size() == 5);
    CHECK(lines[0] == "earlier run");
    const std::regex stamped(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}[+-]\d{4} mono_ns=\d+ (.*)$)");
    const std::vector<std::string> texts{"first", "second", "third", "fourth"};
    for (std::size_t k = 0; k < texts.size(); ++k) {
        std::smatch match;
        REQUIRE(std::regex_match(lines[k + 1], match, stamped));
        CHECK(match[1].str() == texts[k]);
    }
}

// The bytes of the timestamp and the default file name for given local fields.
// That the fields and the offset equal what localtime and strftime's %z gave
// before is checked per platform by the OS layer's tests (tests/common/os).
TEST_CASE("PvP log timestamps and file names keep their bytes for given local time fields") {
    using namespace std::chrono_literals;
    using Engine::Os::Process::LocalTime;
    const auto at = [](int year, int month, int day, int hour, int minute, int second, std::chrono::seconds offset) {
        return LocalTime{year, month, day, hour, minute, second, offset};
    };
    CHECK(FormatLogTimestamp(at(2026, 10, 16, 10, 40, 11, 9h), 7, 42) == "2026-10-16T10:40:11.007+0900 mono_ns=42");
    CHECK(FormatLogTimestamp(at(2026, 1, 15, 8, 30, 0, -(3h + 30min)), 999, 0) ==
          "2026-01-15T08:30:00.999-0330 mono_ns=0");
    CHECK(FormatLogTimestamp(at(2026, 1, 15, 17, 30, 59, 5h + 30min), 0, 9'000'000'000'000) ==
          "2026-01-15T17:30:59.000+0530 mono_ns=9000000000000");
    CHECK(FormatLogTimestamp(at(2026, 1, 15, 12, 0, 0, 0s), 0, 1) == "2026-01-15T12:00:00.000+0000 mono_ns=1");
    // An offset with seconds prints its whole minutes, as %z does.
    CHECK(FormatLogTimestamp(at(1890, 1, 1, 0, 0, 0, 9h + 18min + 59s), 0, 1) == "1890-01-01T00:00:00.000+0918 mono_ns=1");
    // The fallback fields print what a zero-initialized std::tm printed.
    CHECK(FormatLogTimestamp(at(1900, 1, 0, 0, 0, 0, 0s), 0, 0) == "1900-01-00T00:00:00.000+0000 mono_ns=0");
    CHECK(LogFileName("client", at(2026, 10, 16, 10, 40, 11, 9h)) == "client-20261016-104011.log");
    CHECK(LogFileName("client", at(2026, 1, 5, 8, 3, 9, 0s)) == "client-20260105-080309.log");
    // The current time converts, so no fallback appears in a log.
    CHECK(LocalWallClock(std::chrono::system_clock::now()).year >= 2026);
}

TEST_CASE("PvP movement trace counts an event a producer records after Finish") {
    const auto path = DiagnosticPath("movement-late");
    MovementTraceWriter writer(path);
    // A producer that took the sink just before Finish records just after it.
    std::shared_ptr<MovementTraceSink> held;
    {
        std::lock_guard lock(movementTraceSinkMutex);
        held = movementTraceSink;
    }
    REQUIRE(held);
    TraceMovement(GoldenEvents().front());
    writer.Finish();
    CHECK(writer.Good());
    held->Record(GoldenEvents().back());
    CHECK_FALSE(writer.Good());
    const auto lines = ReadLines(path);
    REQUIRE(lines.size() == 2);
    CHECK(lines.back() == R"({"schema_version":3,"kind":"trace_end","events":1,"dropped":0})");
}

TEST_CASE("PvP log file ends with the count of dropped lines") {
    const auto path = DiagnosticPath("log-dropped");
    std::size_t kept = 0;
    {
        LogFile log(path, 1);
        // A 64 MB line keeps the writer busy; with one waiting line allowed,
        // all but the first of the short lines after it are dropped.
        log.Write(std::string(std::size_t{64} << 20, 'd'));
        for (int i = 0; i < 10; ++i) log.Write("short " + std::to_string(i));
    }
    const auto lines = ReadLines(path);
    REQUIRE(!lines.empty());
    for (const auto& line : lines) kept += line.find(" short ") != std::string::npos;
    const std::regex dropped(R"(^.* mono_ns=\d+ \[log\] dropped_lines=(\d+)$)");
    std::smatch match;
    REQUIRE(std::regex_match(lines.back(), match, dropped));
    CHECK(std::stoul(match[1].str()) >= 8);
    CHECK(std::stoul(match[1].str()) + kept == 10);
}

TEST_CASE("PvP log file writes a failed assertion before the previous handler runs, then restores it") {
    const auto path = DiagnosticPath("log-assert");
    Engine::Test::ScopedAssertionHandler throwing;
    const auto terminate = std::get_terminate();
    {
        LogFile log(path);
        log.FlushOnCrash();
        CHECK(std::get_terminate() != terminate);
        // Keeps the writer busy, so the failure line is in the file only if
        // the handler waited for it.
        log.Write(std::string(std::size_t{64} << 20, 'w'));
        bool thrown = false;
        try {
            GYO_ASSERT(false);
        } catch (const Engine::Base::AssertionFailure&) {
            thrown = true;
        }
        CHECK(thrown);
        // In the file already: the handler flushed it before the previous one threw.
        const auto lines = ReadLines(path);
        REQUIRE(lines.size() == 2);
        CHECK(lines[1].find("GYO_ASSERT failed: false at ") != std::string::npos);
    }
    CHECK(std::get_terminate() == terminate);
    bool rethrown = false;
    try {
        GYO_ASSERT(false);
    } catch (const Engine::Base::AssertionFailure&) {
        rethrown = true;
    }
    CHECK(rethrown);
    CHECK(ReadLines(path).size() == 2);
}

// os-files-write moved these files from std::ofstream to engine/io: the same
// lines, with the platform's line endings ("\r\n" on Windows), as the
// text-mode stream wrote them, and the same failure messages.
TEST_CASE("PvP shot and ingress trace files keep the bytes the text-mode stream wrote") {
    const auto shots = DiagnosticPath("shots");
    {
        MatchShotTraceFile file(shots);
        file.Finish({});
    }
    const auto ingress = DiagnosticPath("ingress");
    {
        MatchIngressTraceFile file(ingress);
        file.Finish({});
    }
    const auto reference = DiagnosticPath("reference");
    { std::ofstream(reference) << MatchShotTraceEndLine(0, 0, 0) << '\n'; }
    CHECK(ReadBinary(shots) == ReadBinary(reference));
    { std::ofstream(reference) << MatchIngressTraceEndLine(0, 0, 0) << '\n'; }
    CHECK(ReadBinary(ingress) == ReadBinary(reference));
    const auto absent = DiagnosticPath("absent") / "x.jsonl";
    CHECK_THROWS_WITH_AS(MatchShotTraceFile{absent}, doctest::Contains("Cannot open shot trace: "), std::runtime_error);
    CHECK_THROWS_WITH_AS(MatchIngressTraceFile{absent}, doctest::Contains("Cannot open ingress trace: "),
                         std::runtime_error);
}

TEST_CASE("PvP native text files buffer past their buffer, flush when destroyed and report a failed write") {
    // More than the 16 KiB buffer, in pieces, through Close.
    const auto large = DiagnosticPath("large");
    std::string expected;
    {
        NativeTextFile file(large);
        REQUIRE(file);
        for (int line = 0; line < 2000; ++line) {
            const std::string text = "line " + std::to_string(line) + std::string(20, 'x');
            file.Stream() << text << '\n';
            expected += text + '\n';
        }
        CHECK(file.Close());
        CHECK(file); // still good after a good Close, as std::ofstream
        CHECK_FALSE(file.Close());
    }
    CHECK(ReadFile(large) == expected);
    // Destroyed without Close: what was written reaches the file.
    const auto unclosed = DiagnosticPath("unclosed");
    {
        NativeTextFile file(unclosed);
        file.Stream() << "kept\n";
    }
    CHECK(ReadFile(unclosed) == "kept\n");
    // A device that refuses every write (Linux): the trace file reports it.
    if (std::filesystem::exists("/dev/full")) {
        NativeTextFile full("/dev/full");
        REQUIRE(full);
        full.Stream() << "lost\n" << std::flush;
        CHECK_FALSE(full.Stream());
        CHECK_FALSE(full);
        MatchShotTraceFile shots("/dev/full");
        CHECK_THROWS_WITH_AS(shots.Finish({}), doctest::Contains("Shot trace failed to write"), std::runtime_error);
    }
}

TEST_CASE("PvP log directories that cannot be made name the directory and the cause") {
    const auto file = DiagnosticPath("not-a-directory");
    { std::ofstream(file) << "x"; }
    CHECK_THROWS_WITH_AS(CreateNativeDirectories(file / "logs"), doctest::Contains("Cannot create directory: "),
                         std::runtime_error);
    try {
        CreateNativeDirectories(file / "logs");
    } catch (const std::runtime_error& error) {
        CHECK(std::string(error.what()).find("(InvalidPath") != std::string::npos);
    }
}

// Trace names with characters the Windows ANSI code page cannot hold keep them
// (os-path-utf8-sweep, D63): the names are worked on in UTF-8, where MSVC's
// path::string() threw for them.
TEST_CASE("PvP trace names outside the ANSI code page keep their characters") {
    const std::filesystem::path movement(u8"\u8a18\u9332/\u8a66\u5408-commands.jsonl");
    CHECK(NumberedTracePath(movement, 2) == std::filesystem::path(u8"\u8a18\u9332/\u8a66\u5408.2-commands.jsonl"));
    CHECK(NumberedTracePath(std::filesystem::path(u8"\u8a18\u9332/\uad81\uc801.jsonl"), 3) ==
          std::filesystem::path(u8"\u8a18\u9332/\uad81\uc801.3.jsonl"));
    CHECK(MatchShotTracePath(movement) == std::filesystem::path(u8"\u8a18\u9332/\u8a66\u5408-shots.jsonl"));
    std::string error;
    CHECK(MatchIngressTracePath(movement, {}, error) == std::filesystem::path(u8"\u8a18\u9332/\u8a66\u5408-ingress.jsonl"));
    CHECK(error.empty());
    // A refused name is named in UTF-8 in the message.
    const std::filesystem::path refused(u8"\u8a18\u9332/\u60aa-commands.jsonl");
    CHECK(MatchIngressTracePath(movement, refused, error).empty());
    const std::u8string refusedText = refused.u8string();
    CHECK(error == "The ingress trace name must not end in commands.jsonl: " +
                       std::string(refusedText.begin(), refusedText.end()));
}

// v7.1 trace-no-overwrite (D191): a restart takes the next free name, and the
// files named after a movement trace must be free too.
TEST_CASE("PvP a restarted trace takes the next free numbered name") {
    CHECK(NumberedTracePath("logs/match-commands.jsonl", 1) == std::filesystem::path("logs/match-commands.jsonl"));
    CHECK(NumberedTracePath("logs/match-commands.jsonl", 2) == std::filesystem::path("logs/match.2-commands.jsonl"));
    CHECK(NumberedTracePath("logs/trace.jsonl", 3) == std::filesystem::path("logs/trace.3.jsonl"));
    CHECK(NumberedTracePath("logs/trace", 2) == std::filesystem::path("logs/trace.2"));
    CHECK(FreshTracePath({}).empty());

    const auto directory = std::filesystem::temp_directory_path() / "gyo_pvp_fresh_trace_test";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto movement = directory / "match-commands.jsonl";
    const auto shots = [](const std::filesystem::path& candidate) {
        return std::vector<std::filesystem::path>{MatchShotTracePath(candidate)};
    };
    CHECK(FreshTracePath(movement, shots) == movement);
    std::ofstream(movement) << "first run\n";
    CHECK(FreshTracePath(movement, shots) == directory / "match.2-commands.jsonl");
    // Only a derived file of the second name exists: the third name is the free one.
    std::ofstream(directory / "match.2-shots.jsonl") << "kept\n";
    CHECK(FreshTracePath(movement, shots) == directory / "match.3-commands.jsonl");
    // The earlier files are never truncated by the choice. The stream closes
    // before the directory goes (Windows cannot remove an open file).
    std::string text;
    {
        std::ifstream first(movement);
        std::getline(first, text);
    }
    CHECK(text == "first run");
    std::filesystem::remove_all(directory);
}
