#include <doctest/doctest.h>

#include "CollisionCorpus.hpp"

#include <set>
#include <string>

using namespace Engine::Test::CollisionCorpus;

TEST_CASE("the collision corpus covers every public query and is deterministic in process") {
    const auto first = Records();
    CHECK(first == Records());
    std::set<std::string> queries;
    for (const auto& record : first) queries.insert(record.substr(0, record.find(' ')));
    CHECK(queries == std::set<std::string>{
        "OverlapVerticalCapsuleAabb", "OverlapVerticalCapsules", "RaycastAabb", "RaycastCapsule/arbitrary",
        "RaycastCapsule/general", "RaycastCapsule/vertical", "SweepSphereAgainstCapsule/arbitrary",
        "SweepSphereAgainstCapsule/general", "SweepSphereAgainstCapsule/vertical",
        "SweepVerticalCapsuleAgainstAabb", "SweepVerticalCapsuleAgainstCapsule", "ToCapsule"});
    std::size_t hits = 0;
    for (const auto& record : first) hits += record.find("| miss") == std::string::npos ? 1 : 0;
    // Both outcomes occur, so the corpus exercises hits and misses alike.
    CHECK(hits > first.size() / 10);
    CHECK(hits < first.size());
}

TEST_CASE("capsule overload statistics account for every compared case") {
    for (const auto& [query, stats] : CompareCapsuleOverloads()) {
        CAPTURE(query);
        CHECK(stats.cases > 0);
        CHECK(stats.bothMiss + stats.bothHit + stats.onlyFloatHits + stats.onlyDoubleHits == stats.cases);
        std::size_t bucketed = 0;
        for (const auto count : stats.ulpBuckets) bucketed += count;
        CHECK(bucketed == stats.bothHit);
    }
}
