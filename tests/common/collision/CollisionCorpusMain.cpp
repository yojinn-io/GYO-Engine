// Collision corpus runner: writes the records for a same-host two-tree
// comparison, prints the capsule overload statistics, and runs every query on
// degenerate capsules (height == 2r, valid since FF-9).
#include "CollisionCorpus.hpp"

// Only the scoped throwing assertion handler is used; no doctest tests here.
#define DOCTEST_CONFIG_DISABLE
#include "AssertTestSupport.hpp"
#include "engine/collision/Collision.hpp"

#include <bit>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

using namespace Engine::Collision;
using Engine::Math::Ray;

const char* Outcome(bool asserted, bool hit) { return asserted ? "asserted" : hit ? "hit" : "miss"; }

// A degenerate capsule: height exactly two radii, a sphere. Valid since
// FF-9; both public overloads run the double algorithm, so every case runs.
void ReportDegenerate() {
    std::cout << "degenerate capsules (height == 2 * radius):\n";
    int shown = 0;
    for (float feetY : {1.0f, 100.0f, 1000.0f, 4096.0f, 65536.0f}) {
        for (float radius : {1.0e-3f, 1.0e-4f, 1.0e-5f, 1.0e-6f, 1.0e-7f}) {
            const VerticalCapsule capsule{{0.0f, feetY, 0.0f}, 2.0f * radius, radius};
            const Ray ray{{-1.0f, feetY + radius, 0.0f}, {1.0f, 0.0f, 0.0f}};
            bool verticalAsserted = false, verticalHit = false, generalAsserted = false, generalHit = false;
            {
                Engine::Test::ScopedAssertionHandler handler;
                try { verticalHit = RaycastCapsule(ray, 4.0f, capsule).has_value(); }
                catch (const Engine::Base::AssertionFailure&) { verticalAsserted = true; }
                try { generalHit = RaycastCapsule(ray, 4.0f, ToCapsule(capsule)).has_value(); }
                catch (const Engine::Base::AssertionFailure&) { generalAsserted = true; }
            }
            std::cout << "  feet.y=" << feetY << " radius=" << radius
                      << " valid=" << (IsValid(capsule) ? "true" : "false")
                      << " vertical=" << Outcome(verticalAsserted, verticalHit)
                      << " general=" << Outcome(generalAsserted, generalHit) << '\n';
            ++shown;
        }
    }
    std::cout << "  cases=" << shown << '\n';
}

void ReportStatistics() {
    for (const auto& [query, stats] : Engine::Test::CollisionCorpus::CompareCapsuleOverloads()) {
        std::cout << query << ": cases=" << stats.cases << " both_miss=" << stats.bothMiss
                  << " both_hit=" << stats.bothHit << " only_vertical_float_hits=" << stats.onlyFloatHits
                  << " only_general_double_hits=" << stats.onlyDoubleHits << " ulp{0,1,2-4,5-16,17-256,>256}={";
        for (std::size_t index = 0; index < stats.ulpBuckets.size(); ++index)
            std::cout << (index ? "," : "") << stats.ulpBuckets[index];
        std::cout << "} max_ulp=" << stats.maximumUlp << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    bool any = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--out" && index + 1 < argc) {
            std::ofstream file(argv[++index], std::ios::binary);
            for (const auto& record : Engine::Test::CollisionCorpus::Records()) file << record << '\n';
            if (!file) { std::cerr << "cannot write records\n"; return 2; }
        } else if (argument == "--stats") {
            ReportStatistics();
        } else if (argument == "--degenerate") {
            ReportDegenerate();
        } else {
            std::cerr << "usage: gyo_collision_corpus [--out FILE] [--stats] [--degenerate]\n";
            return 2;
        }
        any = true;
    }
    if (!any) { std::cerr << "usage: gyo_collision_corpus [--out FILE] [--stats] [--degenerate]\n"; return 2; }
    return 0;
}
