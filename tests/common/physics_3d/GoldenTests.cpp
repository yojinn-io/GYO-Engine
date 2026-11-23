// The cross-platform golden of the physics_3d Module (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md,
// sections 4.2 and 4.6; P3-5): a handful of queries in general position (rotated poses, irrational results) whose
// results must have the same binary32 bits on every platform. The literal bit patterns were generated on macOS x64
// (AppleClang, the core preset) on 2026-11-23; L1 runs this on linux-x64, windows-x64 and macos-arm64. A failure on
// one platform is the "rollback equivalence broken" stop of the rollback page: stop and report, do not work around it
// in physics.
#include <doctest/doctest.h>

#include <bit>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/physics_3d/Queries.hpp"
#include "engine/physics_3d/Shapes.hpp"

namespace M = GYO::Engine::Math;
namespace P3 = GYO::Engine::Physics3d;

namespace {

void Push(std::vector<std::uint32_t>& out, const float v) { out.push_back(std::bit_cast<std::uint32_t>(v)); }
void Push(std::vector<std::uint32_t>& out, const M::Vec3 v) {
    Push(out, v.x);
    Push(out, v.y);
    Push(out, v.z);
}

// A rotation that is not about one axis, written as float literals (no transcendental function).
constexpr M::Quaternion kTilt{0.18257419F, 0.36514837F, 0.54772256F, 0.73029674F};
constexpr M::Quaternion kTurnY{0.0F, 0.25881905F, 0.0F, 0.96592583F};

std::vector<std::uint32_t> Results() {
    std::vector<std::uint32_t> out;
    const auto ray = [&out](const std::optional<P3::RayHit>& hit) {
        REQUIRE(hit.has_value());
        Push(out, hit->t);
        Push(out, hit->normal);
    };
    const auto contact = [&out](const P3::Contact& c) {
        Push(out, c.point1);
        Push(out, c.point2);
        Push(out, c.normal1);
        Push(out, c.distance);
    };
    const auto cast = [&out](const P3::ShapeHit& h) {
        REQUIRE(h.status == P3::ShapeHitStatus::Converged);  // a sweep, not an initial overlap
        Push(out, h.time);
        Push(out, h.witness1);
        Push(out, h.witness2);
        Push(out, h.normal1);
    };
    const M::Ray slanted{{-7.3F, 2.1F, 0.37F}, {0.8F, -0.11F, 0.05F}};
    ray(P3::CastRay(P3::Sphere{1.3F}, P3::Pose{{1.1F, 0.9F, 0.7F}, kTilt}, slanted, 100.0F));
    ray(P3::CastRay(P3::Capsule{0.9F, 0.45F}, P3::Pose{{0.3F, 1.2F, 0.6F}, kTilt}, slanted, 100.0F));
    ray(P3::CastRay(P3::Cuboid{{0.7F, 1.9F, 0.4F}}, P3::Pose{{0.2F, 1.0F, 0.5F}, kTilt}, slanted, 100.0F));
    ray(P3::CastRay(slanted, 100.0F, M::Capsule{{0.1F, -0.6F, 0.2F}, {0.7F, 2.3F, 0.9F}, 0.35F}));
    auto boxes = P3::BoxSet::Build(std::vector<M::Aabb>{{{-1.0F, -1.0F, -1.0F}, {0.5F, 3.0F, 0.75F}},
                                                         {{0.25F, -2.0F, 0.1F}, {1.5F, 0.3F, 1.2F}},
                                                         {{-3.0F, 0.0F, -0.5F}, {-2.0F, 1.0F, 0.5F}}});
    REQUIRE(static_cast<bool>(boxes));
    const P3::Pose setPose{{0.4F, 0.3F, -0.2F}, kTilt};
    ray(P3::CastRay(*boxes, setPose, slanted, 100.0F));
    auto c1 = P3::FindContact(P3::Capsule{0.8F, 0.4F}, P3::Pose{{0.2F, 0.6F, 0.1F}, kTurnY}, P3::Capsule{0.6F, 0.3F},
                              P3::Pose{{0.7F, 0.9F, 0.3F}, kTilt}, 0.5F);
    REQUIRE((c1 && c1->has_value()));
    contact(**c1);
    auto c2 = P3::FindContact(P3::Capsule{0.8F, 0.4F}, P3::Pose{{0.9F, 0.55F, 0.3F}, kTurnY},
                              P3::Cuboid{{0.7F, 0.6F, 0.4F}}, P3::Pose{{0.3F, -0.2F, 0.1F}, kTilt}, 0.5F);
    REQUIRE((c2 && c2->has_value()));
    contact(**c2);
    auto c3 = P3::FindDeepestBoxContact(P3::Sphere{0.6F}, P3::Pose{{0.6F, 0.4F, 0.3F}, {}}, *boxes, setPose, 0.25F);
    REQUIRE((c3 && c3->has_value()));
    contact((*c3)->contact);
    out.push_back((*c3)->box);
    auto s1 = P3::CastShape(P3::Sphere{0.45F}, P3::Pose{{3.1F, 2.7F, 1.9F}, {}}, {-4.3F, -3.1F, -2.2F},
                            P3::Cuboid{{0.7F, 0.6F, 0.4F}}, P3::Pose{{0.3F, -0.2F, 0.1F}, kTilt});
    REQUIRE((s1 && s1->has_value()));
    cast(**s1);
    auto s2 = P3::CastShape(P3::Capsule{0.9F, 0.35F}, P3::Pose{{2.9F, 1.3F, 0.7F}, kTurnY}, {-3.7F, -0.9F, -0.4F},
                            P3::Cuboid{{0.7F, 0.6F, 0.4F}}, P3::Pose{{0.3F, -0.2F, 0.1F}, kTurnY});
    REQUIRE((s2 && s2->has_value()));
    cast(**s2);
    auto s3 = P3::CastShape(P3::Sphere{0.3F}, P3::Pose{{6.0F, 6.0F, 4.0F}, {}}, {-7.8F, -6.6F, -5.2F}, *boxes, setPose,
                            P3::ShapeCastOptions{1.0F, 0.05F, true});
    REQUIRE((s3 && s3->has_value()));
    cast(**s3);
    return out;
}

// Generated on macOS x64 (AppleClang), 2026-11-23. Do not regenerate to make a platform pass.
const std::vector<std::uint32_t> kGolden = {
    0x410E7AFEU, 0xBF7B4501U, 0x3E2DA4F4U, 0x3DB590B7U, 0x41099D1EU, 0xBF36963DU,
    0xBF0B88A8U, 0xBEE1A3D5U, 0x41053743U, 0xBF3BBBBCU, 0xBE08888AU, 0xBF2AAAABU,
    0x4115B7DAU, 0xBF3E62C0U, 0xBBF565B5U, 0x3F2B215EU, 0x40D4DD0AU, 0xBF3BBBBCU,
    0xBE08888AU, 0xBF2AAAABU, 0x3EF73729U, 0x3F799999U, 0x3EC403F6U, 0x3EACFD07U,
    0x3F799999U, 0x3E7393A8U, 0x3F3504F3U, 0x26014CAEU, 0x3F3504F3U, 0xBE51F225U,
    0x3F1B4E81U, 0xBE9B4E82U, 0x3D088889U, 0x3F471C72U, 0xBE8B60B6U, 0x3E416C19U,
    0xBF3BBBBCU, 0xBE08888AU, 0xBF2AAAABU, 0xBE6EEEF1U, 0x3E23D70BU, 0x3EA3D70AU,
    0xBDCCCCCEU, 0x3F49C8EEU, 0x3EDE5260U, 0x3EF1357AU, 0xBF3BBBBCU, 0xBE08888AU,
    0xBF2AAAABU, 0xBF5B4E82U, 0x00000000U, 0x3F112160U, 0x3EB2387DU, 0x3F2D0B7AU,
    0x3EF19E8DU, 0x3EB2387DU, 0x3F2D0B7AU, 0x3EF19E8DU, 0xBF32BB2AU, 0xBF17AB41U,
    0xBECDCB14U, 0x3EF955E3U, 0x3F6C5451U, 0x3ECCCCCEU, 0x3E4EF2B6U, 0x3F6C5451U,
    0x3ECCCCCEU, 0x3E4EF2B6U, 0xBF000000U, 0x00000000U, 0xBF5DB3D7U, 0x3F2DB6DFU,
    0x3F683A62U, 0x3FB5F152U, 0x3E8AF88AU, 0x3F70C2EBU, 0x3FB3CF30U, 0x3E73CEF2U,
    0x3F2AAAABU, 0xBEAAAAA9U, 0xBF2AAAABU
};

} // namespace

TEST_CASE("golden: the queries give the same binary32 bits on every platform") {
    const std::vector<std::uint32_t> actual = Results();
    std::string listing;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "0x%08XU,", static_cast<unsigned>(actual[i]));
        listing += buffer;
        listing += (i % 6 == 5) ? "\n" : " ";
    }
    REQUIRE_MESSAGE(actual.size() == kGolden.size(), "[p3-golden] count " << actual.size() << "\n" << listing);
    for (std::size_t i = 0; i < actual.size(); ++i) {
        CHECK_MESSAGE(actual[i] == kGolden[i], "[p3-golden] value " << i << ": " << actual[i] << " vs " << kGolden[i]);
    }
}
