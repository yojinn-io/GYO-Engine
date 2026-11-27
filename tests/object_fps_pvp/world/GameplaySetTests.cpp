// The gameplay sets (world/GameplaySets.hpp; mode-config declaration 2.4, section 4), from outside, with a set of the
// test's own ("probe"): registered in every World whatever the mode; off, its Systems never run, none of its
// components or Resources is anywhere (so nothing of it is replicated or digested: every step's authority digest is
// the one of a World without the set); on, it runs, the Client receives what it replicates, and its settings and its
// arena block reach the World.
#include <doctest/doctest.h>

#include "WorldTestSupport.hpp"

#include "object_fps_pvp/world/GameplaySets.hpp"

#include "engine/authority/Consistency.hpp"
#include "engine/authority/Registrations.hpp"

#include <algorithm>
#include <sstream>

using namespace object_fps_pvp::test;

namespace {

namespace A = GYO::Engine::Authority;
namespace Json = GYO::Engine::Json;

// The probe's decision component (on each player, put by the Match while the set runs), its settings (put from the
// mode's block at every end: not replicated), its arena data (put from the arena block), a Resource only the Match
// writes (replicated: a Client has it only from the Match), and how often its Systems ran (the test's).
struct ProbeMark final {
    std::uint32_t steps = 0;
    friend bool operator==(const ProbeMark&, const ProbeMark&) = default;
};
void HashValue(K::Hasher& h, const ProbeMark& v) { HashValue(h, v.steps); }
struct ProbeState final {
    std::uint32_t start = 0;
};
void HashValue(K::Hasher& h, const ProbeState& v) { HashValue(h, v.start); }
struct ProbeTally final {
    std::uint32_t steps = 0;
};
void HashValue(K::Hasher& h, const ProbeTally& v) { HashValue(h, v.steps); }
struct ProbeArena final {
    std::uint32_t marker = 0;
};
void HashValue(K::Hasher&, const ProbeArena&) {}
struct ProbeRuns final {
    std::uint64_t fixed = 0;
    std::uint64_t frame = 0;
};
void HashValue(K::Hasher&, const ProbeRuns&) {}

K::SystemResult ProbeStep(K::Query<K::Read<PW::Owner>, K::Without<ProbeMark>> fresh, K::Query<K::Write<ProbeMark>> marked,
                          K::ResMut<ProbeRuns> runs, K::CommandsFor<K::Inserts<ProbeMark>, K::Removes<>> commands, K::Commands tally) {
    ++runs->fixed;
    tally.SetResource(ProbeTally{static_cast<std::uint32_t>(runs->fixed)});
    for (const K::EntityId id : fresh.Entities()) commands.Insert(id, ProbeMark{});
    marked.ForEach([](K::EntityId, K::Mut<ProbeMark> mark) { ++mark->steps; });
    return {};
}

K::SystemResult ProbeFrame(K::ResMut<ProbeRuns> runs) {
    ++runs->frame;
    return {};
}

std::uint32_t Read(const Json::ObjectView& block, const std::string_view key, std::string& error) {
    const auto value = block.Find(key);
    if (value) {
        if (const auto number = value->AsUint64()) return static_cast<std::uint32_t>(*number);
    }
    error = std::string(key) + " is not a whole number";
    return 0;
}

PW::GameplaySet ProbeSet() {
    PW::GameplaySet set;
    set.name = "probe";
    set.build = [](O::WorldSetup& setup) {
        K::Registrar& r = setup.Registrar();
        A::AuthorityEntry(setup).Component<ProbeMark>("ProbeMark");
        r.Resource<ProbeState>({.name = "ProbeState"});
        r.Resource<ProbeTally>({.name = "ProbeTally"});
        r.Resource<ProbeArena>({.name = "ProbeArena"});
        r.Resource<ProbeRuns>({.name = "ProbeRuns"});
        if (setup.Options<N::NetWorldOptions>() != nullptr) {
            N::NetEntry net(setup);
            net.Replicate<ProbeMark>("ProbeMark", {[](N::NetWriter& w, const ProbeMark& v) { w.Value(v.steps); },
                                                   [](N::NetReader& rd, ProbeMark& v) { return rd.Value(v.steps); }, 1});
            N::NetEntry::ResourceOptions<ProbeTally> tally;
            tally.codec = {[](N::NetWriter& w, const ProbeTally& v) { w.Value(v.steps); },
                           [](N::NetReader& rd, ProbeTally& v) { return rd.Value(v.steps); }, 1};
            net.ReplicateResource<ProbeTally>("ProbeTally", std::move(tally));
        }
        K::SystemOptions step;
        step.name = "ProbeStep";
        K::Mount fixed;
        fixed.schedule = std::string(O::DefaultSchedules::fixed);
        fixed.stage = std::string(O::DefaultSchedules::update);
        fixed.sets = {PW::GameplaySetFixed("probe")};
        if (setup.Options<N::NetWorldOptions>() != nullptr) fixed.conditions.push_back(N::OnServer());
        step.mounts.push_back(std::move(fixed));
        r.AddSystem<&ProbeStep>(std::move(step));
        K::SystemOptions frame;
        frame.name = "ProbeFrame";
        K::Mount main;
        main.schedule = std::string(O::DefaultSchedules::frame);
        main.stage = std::string(O::DefaultSchedules::update);
        main.sets = {PW::GameplaySetFrame("probe")};
        frame.mounts.push_back(std::move(main));
        r.AddSystem<&ProbeFrame>(std::move(frame));
        // The test's own count, whatever the mode.
        setup.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            w.SetResource(ProbeRuns{});
            return {};
        });
    };
    set.readMode = [](const Json::ObjectView& block, std::string& error) -> std::optional<PW::SetData> {
        const std::uint32_t start = Read(block, "start", error);
        if (!error.empty()) return std::nullopt;
        return PW::SetData{start, [start](K::World& w) { w.SetResource(ProbeState{start}); }};
    };
    set.readArena = [](const Json::ObjectView& block, std::string& error) -> std::optional<PW::SetData> {
        const std::uint32_t marker = Read(block, "marker", error);
        if (!error.empty()) return std::nullopt;
        return PW::SetData{marker, [marker](K::World& w) { w.SetResource(ProbeArena{marker}); }};
    };
    return set;
}

std::string Mode(const std::string& id, const std::string& sets) { return ModeText(id, R"({"movement": "rules/movement_rules.json"})", sets); }

// The shipped content with the probe set and two modes: core_pvp (as shipped: no set) and probe_on (the probe, start 5),
// the first arena carrying a probe block (marker 9).
std::shared_ptr<const PW::GameContent> WithProbe() {
    static const std::shared_ptr<const PW::GameContent> content = [] {
        auto copy = std::make_shared<PW::GameContent>(*ShippedContent());
        copy->sets = {ProbeSet()};
        ModeFolder folder;
        folder.Modes({{"core.json", Mode("core_pvp", "{}")}, {"probe.json", Mode("probe_on", R"({"probe": {"start": 5}})")}});
        std::string error;
        auto modes = PW::LoadModes(folder.root.string(), copy->arenas, copy->combat, copy->sets, error);
        REQUIRE_MESSAGE(modes.has_value(), error);
        copy->modes = std::move(*modes);
        auto blocks = PW::ReadArenaSets(R"({"sets": {"probe": {"marker": 9}}})", copy->sets, error);
        REQUIRE_MESSAGE(blocks.has_value(), error);
        copy->arenaSets.assign(copy->arenas.size(), {});
        copy->arenaSets.front() = std::move(*blocks);
        return copy;
    }();
    return content;
}

bool AnyHas(K::World& world) {
    for (const K::EntityId id : world.Entities()) {
        if (world.Get<ProbeMark>(id) != nullptr) return true;
    }
    return false;
}

std::uint64_t DigestOf(K::World& world) {
    const auto digest = A::Digest(world);
    REQUIRE(static_cast<bool>(digest));
    return *digest;
}

} // namespace

TEST_CASE("world: a mode's gameplay sets read with their settings; a set the game does not have, or bad settings, do not") {
    std::vector<PW::GameplaySet> sets{ProbeSet()};
    const auto load = [&](const std::string& probe, std::string& error) {
        ModeFolder folder;
        folder.Modes({{"a.json", Mode("a", probe)}});
        const auto& content = *ShippedContent();
        return PW::LoadModes(folder.root.string(), content.arenas, content.combat, sets, error);
    };
    std::string error;
    const auto on = load(R"({"probe": {"start": 3}})", error);
    REQUIRE_MESSAGE(on.has_value(), error);
    CHECK(on->front().on.Has(0));
    const auto other = load(R"({"probe": {"start": 4}})", error);
    REQUIRE_MESSAGE(other.has_value(), error);
    CHECK_MESSAGE(other->front().rulesDigest != on->front().rulesDigest, "the set's settings are in the rules digest");
    const auto off = load("{}", error);
    REQUIRE_MESSAGE(off.has_value(), error);
    CHECK_FALSE(off->front().on.Has(0));
    CHECK(off->front().rulesDigest != on->front().rulesDigest);
    CHECK_FALSE(load(R"({"doors": {}})", error).has_value());
    CHECK_MESSAGE(error.find("no gameplay set 'doors'") != std::string::npos, error);
    CHECK_FALSE(load(R"({"probe": {"start": "three"}})", error).has_value());
    CHECK_MESSAGE(error.find("start is not a whole number") != std::string::npos, error);
    // An arena block for a set the game does not have.
    CHECK_FALSE(PW::ReadArenaSets(R"({"sets": {"doors": {}}})", sets, error).has_value());
    CHECK_MESSAGE(error.find("no gameplay set 'doors'") != std::string::npos, error);
    // No block at all: none.
    const auto none = PW::ReadArenaSets(R"({"id": "x"})", sets, error);
    REQUIRE_MESSAGE(none.has_value(), error);
    CHECK(none->empty());
}

TEST_CASE("world: a gameplay set the mode leaves off never runs, is on no entity, is not sent, and leaves every digest as without it") {
    // The same steps with the probe off (core_pvp) and without the probe at all.
    Session off(1, 4, 0, WithProbe(), {}, 60, std::nullopt, WithProbe());
    Session without(1, 4);
    for (int frame = 0; frame < 150; ++frame) {
        const std::vector<std::optional<PW::PlayerInput>> walk{PW::PlayerInput{.forward = 1.0F, .present = 1}};
        off.Frame(frame > 60 ? walk : std::vector<std::optional<PW::PlayerInput>>{});
        without.Frame(frame > 60 ? walk : std::vector<std::optional<PW::PlayerInput>>{});
        CAPTURE(frame);
        REQUIRE_MESSAGE(DigestOf(off.Match()) == DigestOf(without.Match()), "[pvp-set-off-digest] the Match's digest");
        REQUIRE_MESSAGE(DigestOf(off.Client(0)) == DigestOf(without.Client(0)), "[pvp-set-off-digest] the Client's digest");
    }
    REQUIRE(PlayerOf(off.Match(), 1) != 0);
    CHECK_MESSAGE(off.Match().Resource<ProbeRuns>()->fixed == 0, "[pvp-set-off-runs] its fixed System never ran");
    CHECK_MESSAGE(off.Match().Resource<ProbeRuns>()->frame == 0, "[pvp-set-off-runs] its frame System never ran");
    CHECK_MESSAGE(off.Client(0).Resource<ProbeRuns>()->frame == 0, "[pvp-set-off-runs] nor at the Client");
    CHECK_MESSAGE(!AnyHas(off.Match()), "[pvp-set-off-component] no entity has its component");
    CHECK(!AnyHas(off.Client(0)));
    CHECK_MESSAGE(off.Match().Resource<ProbeState>() == nullptr, "[pvp-set-off-resource] its settings are not put");
    CHECK(off.Client(0).Resource<ProbeState>() == nullptr);
    CHECK(off.Match().Resource<ProbeTally>() == nullptr);
    CHECK_MESSAGE(off.Client(0).Resource<ProbeTally>() == nullptr, "[pvp-set-off-sent] a Resource that is not there is not sent");
    CHECK(off.Match().Resource<ProbeArena>() == nullptr);
    CHECK(off.Client(0).Resource<PW::ContentMismatch>()->reason.empty());
    // Registered all the same: the protocol has it (both ends connected), so it is not the shipped protocol.
    const auto* withSet = off.Match().Resource<N::NetWorldLink>();
    const auto* shipped = without.Match().Resource<N::NetWorldLink>();
    REQUIRE((withSet != nullptr && shipped != nullptr));
    CHECK(withSet->registry->Hash() != shipped->registry->Hash());
}

TEST_CASE("world: a gameplay set the mode turns on runs at its ends, the Client receives what it replicates, its settings and arena block are put") {
    const auto content = WithProbe();
    Session on(1, 4, 0, content, {}, 60, std::nullopt, content, 1);
    on.Frames(150);
    const K::EntityId player = PlayerOf(on.Match(), 1);
    REQUIRE(player != 0);
    CHECK_MESSAGE(on.Match().Resource<ProbeRuns>()->fixed > 0, "[pvp-set-on-runs] its fixed System ran");
    CHECK_MESSAGE(on.Match().Resource<ProbeRuns>()->frame > 0, "[pvp-set-on-runs] its frame System ran");
    CHECK(on.Client(0).Resource<ProbeRuns>()->frame > 0);
    REQUIRE_MESSAGE(on.Match().Get<ProbeMark>(player) != nullptr, "[pvp-set-on-component] the Match put its component");
    CHECK(on.Match().Get<ProbeMark>(player)->steps > 0);
    const ProbeMark* sent = on.Client(0).Get<ProbeMark>(player);
    REQUIRE_MESSAGE(sent != nullptr, "[pvp-set-on-sent] the Client received the component");
    CHECK(sent->steps > 0);
    // The settings, put at each end from its own mode file.
    for (K::World* world : {&on.Match(), &on.Client(0)}) {
        REQUIRE_MESSAGE(world->Resource<ProbeState>() != nullptr, "[pvp-set-on-settings] ProbeState: the mode's settings are put");
        CHECK(world->Resource<ProbeState>()->start == 5);
    }
    // The Resource only the Match writes reaches the Client by replication (its own ProbeStep does not run).
    REQUIRE(on.Match().Resource<ProbeTally>() != nullptr);
    REQUIRE_MESSAGE(on.Client(0).Resource<ProbeTally>() != nullptr, "[pvp-set-on-sent] the Client received the Resource");
    CHECK(on.Client(0).Resource<ProbeTally>()->steps > 0);
    REQUIRE(on.Match().Resource<ProbeArena>() != nullptr);
    CHECK_MESSAGE(on.Match().Resource<ProbeArena>()->marker == 9, "[pvp-set-on-arena] the arena's block is put");
    CHECK(on.Client(0).Resource<PW::ContentMismatch>()->reason.empty());
    CHECK(on.Match().Resource<PW::MatchInfo>()->modeId == "probe_on");
}

TEST_CASE("world: a gameplay set is registered after the core: the core's decision types keep their authority numbers") {
    // The core's decision types keep their authority registration numbers: the dataset report lists them first, in the
    // core's order, then the set's.
    Session off(1, 4, 0, WithProbe(), {}, 60, std::nullopt, WithProbe());
    Session without(1, 4);
    const auto withSet = A::DatasetReport(off.Match());
    const auto shipped = A::DatasetReport(without.Match());
    REQUIRE((withSet && shipped));
    const auto types = [](const std::string& report) {
        std::vector<std::string> lines;
        std::istringstream in(report);
        for (std::string line; std::getline(in, line);) {
            if (line.rfind("type ", 0) == 0) lines.push_back(line);
        }
        return lines;
    };
    const auto mine = types(*withSet);
    const auto core = types(*shipped);
    REQUIRE(mine.size() == core.size() + 1);
    CHECK_MESSAGE(std::equal(core.begin(), core.end(), mine.begin()), "[pvp-set-after-core] the core's types keep their numbers");
    CHECK(mine.back().find("ProbeMark") != std::string::npos);
}

TEST_CASE("world: a Client whose arena carries another block for a set the mode turns on is a content mismatch (rules)") {
    // The Match's first arena carries marker 9 for the probe; the Client's, marker 8. Same mode, same arena, same walls.
    auto other = std::make_shared<PW::GameContent>(*WithProbe());
    std::string error;
    auto blocks = PW::ReadArenaSets(R"({"sets": {"probe": {"marker": 8}}})", other->sets, error);
    REQUIRE_MESSAGE(blocks.has_value(), error);
    other->arenaSets.front() = std::move(*blocks);
    CHECK(other->MatchRulesDigest(1, 0) != WithProbe()->MatchRulesDigest(1, 0));
    // The mode leaves the set off: the arena's block is not played, the digest is the mode's.
    CHECK(other->MatchRulesDigest(0, 0) == WithProbe()->modes.front().rulesDigest);
    Session session(1, 4, 0, other, {}, 60, std::nullopt, WithProbe(), 1);
    session.Frames(90);
    const PW::ContentMismatch& mismatch = *session.Client(0).Resource<PW::ContentMismatch>();
    CHECK_MESSAGE(mismatch.kind == PW::MismatchKind::Rules, "[pvp-set-arena-digest] " << mismatch.reason);
}
