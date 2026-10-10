#include <doctest/doctest.h>

#include "RetroFPS/Pvp/IngressStatistics.hpp"
#include "RetroFPS/Pvp/MatchIngressTrace.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace fps::pvp;

const std::filesystem::path Fixtures = PVP_INGRESS_FIXTURE_DIR;

std::vector<std::string> ReadLines(const std::filesystem::path& path) {
    std::ifstream input(path);
    REQUIRE(input);
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);) lines.push_back(line);
    return lines;
}

struct ParsedLine final {
    MatchIngressWindow window;
    std::vector<std::string> keys;
    bool valuesAreIntegers{true};
};

// Parses a golden Match line through the visitor: the fixed head, then every
// counter in visitor order. A key out of order leaves keys unequal.
ParsedLine ParseMatchLine(const std::string& line) {
    static const std::string prefix = "[ObjectFPS/PvP Match] ingress statistics ";
    static const std::regex digits("^[0-9]+$");
    REQUIRE(line.starts_with(prefix));
    std::istringstream words(line.substr(prefix.size()));
    std::vector<std::pair<std::string, std::string>> fields;
    ParsedLine parsed;
    for (std::string word; words >> word;) {
        const auto equals = word.find('=');
        REQUIRE(equals != std::string::npos);
        fields.emplace_back(word.substr(0, equals), word.substr(equals + 1));
        parsed.valuesAreIntegers = parsed.valuesAreIntegers && std::regex_match(fields.back().second, digits);
    }
    REQUIRE(parsed.valuesAreIntegers);
    REQUIRE(fields.size() >= 4);
    CHECK(fields[0] == std::pair<std::string, std::string>{"version", "1"});
    CHECK(fields[1].first == "player");
    CHECK(fields[2].first == "window_ms");
    CHECK(fields[3].first == "final");
    parsed.window.player = std::stoull(fields[1].second);
    parsed.window.windowMs = std::stoull(fields[2].second);
    parsed.window.final = fields[3].second == "1";
    std::size_t next = 4;
    ForEachMatchIngressField(parsed.window.counts, [&](const std::string&, std::uint64_t& value) {
        if (next < fields.size()) {
            parsed.keys.push_back(fields[next].first);
            value = std::stoull(fields[next].second);
        }
        ++next;
    });
    CHECK(next == fields.size());
    return parsed;
}

std::vector<std::string> MatchIngressKeys() {
    std::vector<std::string> keys;
    const MatchIngressCounts counts;
    ForEachMatchIngressField(counts, [&](const std::string& key, const std::uint64_t&) { keys.push_back(key); });
    return keys;
}

template <class Values, class Project>
std::uint64_t Sum(const Values& values, Project project) {
    return std::accumulate(values.begin(), values.end(), std::uint64_t{},
                           [&](std::uint64_t sum, const auto& value) { return sum + project(value); });
}

} // namespace

TEST_CASE("Match ingress lines equal the golden windows and keep the I2 classification exhaustive") {
    const auto lines = ReadLines(Fixtures / "match.txt");
    REQUIRE(lines.size() == 3); // zero, non-zero, final
    const auto keys = MatchIngressKeys();
    for (const auto& line : lines) {
        const auto parsed = ParseMatchLine(line);
        CHECK(parsed.keys == keys);
        const std::string ingressLine = MatchIngressStatisticsLine(parsed.window);
        const std::string& expectedIngressLine = line;
        CHECK(ingressLine == expectedIngressLine);

        const auto& counts = parsed.window.counts;
        const auto classifiedCommands = Sum(counts.classified, [](auto value) { return value; }) +
            Sum(counts.rejected, [](const auto& value) { return value.commands; });
        const auto receivedCommands = counts.received.commands;
        CHECK(classifiedCommands == receivedCommands);
        CHECK(counts.acceptedInputs + Sum(counts.rejected, [](const auto& value) { return value.inputs; }) ==
              counts.received.inputs);
        CHECK(counts.acceptedActions.batches + Sum(counts.rejectedActions, [](const auto& v) { return v.batches; }) ==
              counts.receivedActions.batches);
        CHECK(counts.acceptedActions.shots + Sum(counts.rejectedActions, [](const auto& v) { return v.shots; }) ==
              counts.receivedActions.shots);
        // J5a with every record closed: substituted = late_first + closed before any copy arrived.
        const auto goldenSubstituted = counts.substitutedCommands;
        CHECK(goldenSubstituted == counts.classified[IngressIndex(IngressCommandClass::LateFirst)] +
                                       Sum(counts.unarrived, [](auto value) { return value; }));
        // S1 and S2: every slack sample candidate ends merged, discarded or
        // published, and every published one overwritten, taken or unclaimed.
        const auto slackCandidates = counts.slackExecutedSamples + counts.slackLateSamples;
        CHECK(slackCandidates == counts.slackMergedSamples + counts.slackDiscardedSamples + counts.slackPublishedSamples);
        const auto slackPublished = counts.slackPublishedSamples;
        CHECK(slackPublished == counts.slackOverwrittenSamples + counts.slackTakenSamples + counts.slackUnclaimedSamples);
        CHECK(counts.slackPublishedNegativeSamples <= counts.slackPublishedSamples);
        CHECK(counts.slackCoalescedSamples <= counts.slackTakenSamples);
    }
    CHECK_FALSE(ParseMatchLine(lines[1]).window.final);
    CHECK(ParseMatchLine(lines[2]).window.final);

    MatchIngressWindow window{.player = 3, .windowMs = 10000};
    window.counts.rejected[IngressIndex(IngressInputRejection::EpochFuture)] = {.inputs = 2, .commands = 9};
    window.counts.rejectedActions[IngressIndex(IngressActionRejection::Conflict)] = {.batches = 1, .shots = 4};
    const auto line = MatchIngressStatisticsLine(window);
    CHECK(line.starts_with("[ObjectFPS/PvP Match] ingress statistics version=1 player=3 window_ms=10000 final=0 "
                           "received_inputs=0 received_commands=0 accepted_inputs=0 accepted_new_commands=0 "));
    CHECK(line.find(" epoch_old_inputs=0 epoch_old_commands=0 epoch_future_inputs=2 epoch_future_commands=9 ") !=
          std::string::npos);
    CHECK(line.find(" conflict_actions_batches=1 conflict_actions_shots=4 ") != std::string::npos);
}

TEST_CASE("Match ingress keys are unique, follow the naming rule and stay out of the frozen parsers") {
    const std::regex rule("^[a-z]+(_[a-z]+)*_(inputs|commands|batches|shots|acks|samples)$");
    std::set<std::string> seen{"version", "player", "window_ms", "final"};
    for (const auto& key : MatchIngressKeys()) {
        CAPTURE(key);
        CHECK(std::regex_match(key, rule));
        CHECK(seen.insert(key).second);
    }

    // build/acceptance/object_fps_pvp/network_statistics.py:17-19 and action_probe.py:594.
    const std::regex frozen[] = {
        std::regex(R"(player send statistics (.*)$)"),
        std::regex(R"(\[ObjectFPS/PvP Match\] network statistics (.*)$)"),
        std::regex(R"(\[ObjectFPS/PvP\] worker statistics (.*)$)"),
        std::regex(R"(rate_accepted_packets=(\d+) rate_limited_packets=(\d+) max_session_window_packets=(\d+))"),
    };
    MatchIngressWindow full{.player = 1, .windowMs = 10000, .final = true};
    std::uint64_t next = 1;
    ForEachMatchIngressField(full.counts, [&](const std::string&, std::uint64_t& value) { value = next++; });
    auto lines = ReadLines(Fixtures / "match.txt");
    lines.push_back(MatchIngressStatisticsLine(full));
    lines.push_back(MatchIngressStatisticsLine({}));
    for (const auto& line : lines)
        for (const auto& pattern : frozen) CHECK_FALSE(std::regex_search(line, pattern));
}

TEST_CASE("The match-ingress.jsonl sample uses the shared vocabulary and ends with its trace_end") {
    const auto file = Fixtures / "match-ingress.jsonl";
    // command_evidence.py and quad_evidence.py glob *commands.jsonl.
    const bool ingressNameEndsWithCommands = file.filename().string().ends_with("commands.jsonl");
    CHECK_FALSE(ingressNameEndsWithCommands);

    std::set<std::string> reasons, closes;
    for (std::size_t i = 0; i < IngressCount<IngressInputRejection>; ++i)
        reasons.emplace(IngressName(static_cast<IngressInputRejection>(i)));
    for (std::size_t i = 0; i < IngressCount<IngressSubstitutionClose>; ++i)
        closes.emplace(IngressName(static_cast<IngressSubstitutionClose>(i)));

    const auto lines = ReadLines(file);
    REQUIRE(lines.size() >= 2);
    std::set<std::string> kinds;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        CAPTURE(lines[i]);
        const auto record = nlohmann::json::parse(lines[i]);
        CHECK(record.at("schema") == "object_fps_pvp.match_ingress");
        CHECK(record.at("version") == 1);
        const auto kind = record.at("kind").get<std::string>();
        kinds.insert(kind);
        if (kind == "rejection") {
            CHECK(reasons.contains(record.at("reason").get<std::string>()));
            CHECK(record.at("commands").get<std::uint64_t>() ==
                  record.at("last_sequence").get<std::uint64_t>() - record.at("first_sequence").get<std::uint64_t>() + 1);
        } else if (kind == "substitution") {
            CHECK(closes.contains(record.at("close").get<std::string>()));
            if (!record.at("first_arrival_ns").is_null())
                CHECK(record.at("late_us").get<std::int64_t>() ==
                      (record.at("first_arrival_ns").get<std::int64_t>() - record.at("substituted_ns").get<std::int64_t>()) / 1000);
            else
                CHECK((record.at("late_us").is_null() && record.at("reference_age_us").is_null()));
        } else {
            REQUIRE(kind == "trace_end");
            CHECK(i + 1 == lines.size());
            CHECK(record.at("records").get<std::size_t>() == i);
        }
    }
    CHECK(kinds == std::set<std::string>{"rejection", "substitution", "trace_end"});
}

TEST_CASE("Match ingress windows always carry player 0 and the final window closes every player once") {
    MatchIngressStatisticsWriter writer;
    std::map<std::uint64_t, MatchIngressCounts> first;
    first[7].received = {.inputs = 2, .commands = 5};
    const auto opening = writer.Lines(first, 10000, false);
    REQUIRE(opening.size() == 2);
    CHECK(opening[0].starts_with("[ObjectFPS/PvP Match] ingress statistics version=1 player=0 window_ms=10000 final=0 "
                                 "received_inputs=0 received_commands=0 "));
    CHECK(opening[1].starts_with("[ObjectFPS/PvP Match] ingress statistics version=1 player=7 window_ms=10000 final=0 "
                                 "received_inputs=2 received_commands=5 "));
    // A quiet window still has its player=0 line; player 7 had nothing to hand over.
    const auto quiet = writer.Lines({}, 10002, false);
    REQUIRE(quiet.size() == 1);
    CHECK(quiet[0].starts_with("[ObjectFPS/PvP Match] ingress statistics version=1 player=0 window_ms=10002 final=0 "));

    std::map<std::uint64_t, MatchIngressCounts> last;
    last[8].received = {.inputs = 1, .commands = 1};
    const auto closing = writer.Lines(last, 4217, true);
    std::map<std::uint64_t, MatchIngressWindow> closed;
    for (const auto& line : closing) {
        const auto parsed = ParseMatchLine(line);
        CHECK(parsed.window.final);
        CHECK(parsed.window.windowMs == 4217);
        CHECK(closed.emplace(parsed.window.player, parsed.window).second);
    }
    // Player 7 was quiet in the last window and still gets its final=1 line.
    const bool finalLineWritten = closed.contains(7);
    CHECK(finalLineWritten);
    CHECK(closed.size() == 3);
    CHECK(closed.contains(0));
    REQUIRE(closed.contains(8));
    CHECK(closed.at(8).counts.received.inputs == 1);
}

namespace {
std::chrono::steady_clock::time_point SteadyAt(std::int64_t nanoseconds) {
    return std::chrono::steady_clock::time_point{} + std::chrono::nanoseconds(nanoseconds);
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

// The records of the golden match-ingress.jsonl, as the host would hand them over.
std::vector<IngressRecord> GoldenRecords() {
    std::vector<IngressRecord> records;
    records.emplace_back(IngressRejectionRecord{.timeNs = 5000120000, .playerId = 7, .reason = IngressInputRejection::EpochOld,
        .epoch = 2, .life = 2, .firstSequence = 398, .lastSequence = 409, .commands = 12, .cursor = 411,
        .currentEpoch = 3, .currentLife = 2});
    records.emplace_back(IngressRejectionRecord{.timeNs = 5000250000, .playerId = 9,
        .reason = IngressInputRejection::UnknownPlayer, .epoch = 1, .life = 1, .firstSequence = 1, .lastSequence = 3,
        .commands = 3});
    records.emplace_back(IngressSubstitution{.playerId = 7, .epoch = 3, .life = 2, .sequence = 412,
        .substitutedAt = SteadyAt(5016000000), .substitutedTick = 301, .firstArrival = SteadyAt(5016041000),
        .copiesAfterFirst = 2, .referenceAgeMicros = 58000, .close = IngressSubstitutionClose::End});
    records.emplace_back(IngressSubstitution{.playerId = 7, .epoch = 3, .life = 2, .sequence = 413,
        .substitutedAt = SteadyAt(5032700000), .substitutedTick = 302, .close = IngressSubstitutionClose::Aged});
    records.emplace_back(IngressRejectionRecord{.timeNs = 5040000000, .playerId = 7,
        .reason = IngressInputRejection::ConflictQueued, .epoch = 3, .life = 2, .firstSequence = 414, .lastSequence = 416,
        .commands = 3, .cursor = 413, .currentEpoch = 3, .currentLife = 2});
    return records;
}

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream input(text);
    for (std::string line; std::getline(input, line);) lines.push_back(line);
    return lines;
}
} // namespace

TEST_CASE("The match-ingress.jsonl writer reproduces the golden sample across drains") {
    auto records = GoldenRecords();
    IngressRecords first{.records = std::vector<IngressRecord>(records.begin(), records.begin() + 2), .suppressed = 2};
    IngressRecords last{.records = std::vector<IngressRecord>(records.begin() + 2, records.end()), .suppressed = 1};
    std::ostringstream output;
    MatchIngressTraceWriter writer(output);
    writer.Write(first);
    writer.Finish(last);
    CHECK(writer.Good());
    const auto written = output.str();
    const auto golden = ReadText(Fixtures / "match-ingress.jsonl");
    CHECK(written == golden);
    // trace_end comes once, after every record, and nothing follows it.
    CHECK_THROWS_AS(writer.Write({}), std::logic_error);
}

TEST_CASE("The match-ingress.jsonl writer ends with trace_end after the last drain and counts what it was not given") {
    std::ostringstream output;
    MatchIngressTraceWriter writer(output);
    writer.Write({.records = {}, .suppressed = 4, .dropped = 1});
    IngressRecords last;
    last.records.emplace_back(IngressRejectionRecord{.timeNs = 1, .playerId = 2, .reason = IngressInputRejection::Malformed,
        .epoch = 1, .life = 1});
    last.records.emplace_back(IngressSubstitution{.playerId = 2, .epoch = 1, .life = 1, .sequence = 8,
        .substitutedAt = SteadyAt(1000), .close = IngressSubstitutionClose::Reset});
    last.dropped = 2;
    writer.Finish(last);
    const auto lines = SplitLines(output.str());
    REQUIRE(lines.size() == 3);
    const auto traceEnd = nlohmann::json::parse(lines.back());
    const bool traceEndIsLast = traceEnd.at("kind") == "trace_end";
    CHECK(traceEndIsLast);
    CHECK(traceEnd.at("records") == 2);
    const auto traceSuppressed = traceEnd.at("suppressed").get<std::uint64_t>();
    CHECK(traceSuppressed == 4);
    const auto traceDropped = traceEnd.at("dropped").get<std::uint64_t>();
    CHECK(traceDropped == 3);
    // An input without commands has no sequences; the substitution never arrived.
    const auto malformed = nlohmann::json::parse(lines[0]);
    CHECK(malformed.at("first_sequence").is_null());
    CHECK(malformed.at("last_sequence").is_null());
    CHECK(malformed.at("commands") == 0);
    const auto reset = nlohmann::json::parse(lines[1]);
    CHECK(reset.at("close") == "reset");
    CHECK(reset.at("late_us").is_null());
}

TEST_CASE("The match-ingress.jsonl writer reports a failed write when it ends") {
    std::ostringstream output;
    MatchIngressTraceWriter writer(output);
    output.setstate(std::ios::badbit);
    writer.Write({});
    CHECK_FALSE(writer.Good());
    output.clear(); // even a stream that recovers does not hide the lost lines
    const bool failureRemembered = !writer.Good();
    CHECK(failureRemembered);
    bool ingressFailureThrown = false;
    try {
        writer.Finish({});
    } catch (const std::runtime_error&) {
        ingressFailureThrown = true;
    }
    CHECK(ingressFailureThrown);

    std::ostringstream healthy;
    MatchIngressTraceWriter fine(healthy);
    CHECK_NOTHROW(fine.Finish({.records = {}, .suppressed = 9, .dropped = 9}));
}

TEST_CASE("The ingress detail file sits next to the movement trace and never ends in commands.jsonl") {
    std::string error;
    const auto derived = MatchIngressTracePath("runs/a/match-commands.jsonl", {}, error);
    CHECK(error.empty());
    const auto derivedName = derived.filename().string();
    CHECK(derivedName == "match-ingress.jsonl");
    CHECK(derived.parent_path() == std::filesystem::path("runs/a"));
    const bool ingressNameEndsWithCommands = derivedName.ends_with("commands.jsonl");
    CHECK_FALSE(ingressNameEndsWithCommands);
    CHECK(MatchIngressTracePath("trace.jsonl", {}, error) == std::filesystem::path("trace.jsonl.ingress.jsonl"));
    CHECK(MatchIngressTracePath("commands.jsonl", {}, error) == std::filesystem::path("commands.jsonl.ingress.jsonl"));
    CHECK(error.empty());
    CHECK(MatchIngressTracePath("runs/match-commands.jsonl", "other/detail.jsonl", error) ==
          std::filesystem::path("other/detail.jsonl"));
    CHECK(error.empty());

    // No movement trace: no detail file, and naming one is a usage error.
    const auto withoutMovementTrace = MatchIngressTracePath({}, {}, error);
    CHECK(withoutMovementTrace.empty());
    CHECK(error.empty());
    CHECK(MatchIngressTracePath({}, "detail.jsonl", error).empty());
    const bool ingressWithoutMovementRefused = !error.empty();
    CHECK(ingressWithoutMovementRefused);

    error.clear();
    CHECK(MatchIngressTracePath("runs/match-commands.jsonl", "runs/ingress-commands.jsonl", error).empty());
    const bool commandsSuffixRefused = error.find("commands.jsonl") != std::string::npos;
    CHECK(commandsSuffixRefused);
    error.clear();
    CHECK(MatchIngressTracePath("runs/trace.jsonl", "runs/./trace.jsonl", error).empty());
    CHECK_FALSE(error.empty());
}

TEST_CASE("The ingress record buffer keeps the first rejections of each player and reason per window") {
    MatchIngressRecordBuffer buffer(5, 2);
    const auto rejection = [&](PlayerId bucket, IngressInputRejection reason) {
        if (buffer.AdmitRejection(bucket, reason))
            buffer.Add(IngressRejectionRecord{.playerId = bucket, .reason = reason});
    };
    for (int i = 0; i < 3; ++i) rejection(1, IngressInputRejection::EpochOld);
    rejection(1, IngressInputRejection::EpochFuture);
    rejection(0, IngressInputRejection::EpochOld);
    auto drained = buffer.Drain();
    const auto keptRejections = drained.records.size();
    CHECK(keptRejections == 4);
    const auto suppressedRejections = drained.suppressed;
    CHECK(suppressedRejections == 1);
    CHECK(drained.dropped == 0);
    // Draining is not a new window; a new window is.
    rejection(1, IngressInputRejection::EpochOld);
    CHECK(buffer.Drain().suppressed == 1);
    buffer.NewWindow();
    rejection(1, IngressInputRejection::EpochOld);
    rejection(1, IngressInputRejection::EpochOld);
    drained = buffer.Drain();
    const auto keptInNewWindow = drained.records.size();
    CHECK(keptInNewWindow == 2);
    CHECK(drained.suppressed == 0);

    // A full buffer drops; suppression is judged first.
    for (std::uint64_t sequence = 1; sequence <= 6; ++sequence)
        buffer.Add(IngressSubstitution{.playerId = 1, .sequence = sequence});
    rejection(2, IngressInputRejection::Malformed);
    rejection(1, IngressInputRejection::EpochOld);
    drained = buffer.Drain();
    CHECK(drained.records.size() == 5);
    const auto droppedRecords = drained.dropped;
    CHECK(droppedRecords == 2);
    CHECK(drained.suppressed == 1);
}
