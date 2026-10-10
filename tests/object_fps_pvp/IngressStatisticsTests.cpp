#include <doctest/doctest.h>

#include "RetroFPS/Pvp/IngressStatistics.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <regex>
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
    const std::regex rule("^[a-z]+(_[a-z]+)*_(inputs|commands|batches|shots)$");
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
