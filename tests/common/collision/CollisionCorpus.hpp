#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// A synthetic, deterministic corpus over every public Engine::Collision query.
// Each record is one text line naming the query, the case, its exact inputs
// and its exact result (IEEE bit patterns in hex), so two builds of the same
// host can be compared record by record. Inputs are generated from integers
// on a 1/128 grid (exact in float) and satisfy the current validation; boxes
// are strictly ordered and capsules are strictly taller than two radii.
namespace Engine::Test::CollisionCorpus {

[[nodiscard]] std::vector<std::string> Records();

// The two public capsule overloads (VerticalCapsule; and ToCapsule into
// Math::Capsule) over the same inputs. Since FF-9 both run the double algorithm.
struct OverloadComparison final {
    std::size_t cases{};
    std::size_t bothMiss{};
    std::size_t bothHit{};
    std::size_t onlyFloatHits{};
    std::size_t onlyDoubleHits{};
    // Distance between the two results in units in the last place, bucketed:
    // 0, 1, 2-4, 5-16, 17-256, more than 256.
    std::array<std::size_t, 6> ulpBuckets{};
    std::uint32_t maximumUlp{};
};

[[nodiscard]] std::map<std::string, OverloadComparison> CompareCapsuleOverloads();

} // namespace Engine::Test::CollisionCorpus
