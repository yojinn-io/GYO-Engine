// Spec J-17, kernel half (spec G-5; docs/architecture/zone-c-sources.zh-Hant.md
// gyo.zone-c-sources v1): the kernel exports the source files of zone C Systems
// from their registration sites. This case writes the export of the synthetic
// fixture for kernel.J-17_zone_scan (zone_c_export_check.py), which runs the
// product-side scanner tests/common/zone_scan/zone_scan.py on it.

#include "KernelTestSupport.hpp"

#include <fstream>

void RegisterSyntheticSimulation(GYO::Kernel::Registrar& registrar);

namespace {

using namespace KernelTest;

SystemResult ReadPos(Query<Read<TestPos>>) { return Ok(); }

constexpr const char* kFixtureRoot = "tests/common/kernel/zone_c_fixture";

} // namespace

TEST_CASE("J-17: the G-5 export lists the zone C fixture file as gyo.zone-c-sources v1") {
    Built built = Finish({{"synthetic", RegisterSyntheticSimulation}});
    KERNEL_REQUIRE_OK(built);
    ZoneCSourcesOptions options{GYO_KERNEL_TEST_REPOSITORY_ROOT, kFixtureRoot, {}, "0000000"};
    const auto exported = built.registry->Frozen().ExportZoneCSources(options);
    REQUIRE_MESSAGE(static_cast<bool>(exported), (exported ? std::string() : exported.error().detail));
    const std::string& text = *exported;
    CHECK(text.find("\"format\": \"gyo.zone-c-sources\"") != std::string::npos);
    CHECK(text.find("\"version\": 1") != std::string::npos);
    CHECK(text.find("\"generated_by\": \"gyo-kernel-report\"") != std::string::npos);
    CHECK(text.find("\"owner_root\": \"tests/common/kernel/zone_c_fixture\"") != std::string::npos);
    // Two zone C Systems, one file: listed once.
    const std::string file = "\"tests/common/kernel/zone_c_fixture/SyntheticSimulation.cpp\"";
    REQUIRE(text.find(file) != std::string::npos);
    CHECK(text.find(file, text.find(file) + 1) == std::string::npos);
    std::ofstream out(std::string(GYO_KERNEL_TEST_BINARY_DIR) + "/kernel_zone_c_sources.json", std::ios::binary);
    out << text;
    REQUIRE(out.good());
}

TEST_CASE("J-17: a zone C System registered outside the owner root is an export error, not a silent omission") {
    Built built = FinishOne([](Registrar& r) { r.AddSystem<&ReadPos>(Sys("ReadPos", Zone::C, {At("FixedUpdate", "Simulate")})); });
    KERNEL_REQUIRE_OK(built);
    ZoneCSourcesOptions options{GYO_KERNEL_TEST_REPOSITORY_ROOT, kFixtureRoot, {}, "0000000"};
    const auto exported = built.registry->Frozen().ExportZoneCSources(options);
    REQUIRE_FALSE(static_cast<bool>(exported));
    CHECK(exported.error().detail.find("ReadPos") != std::string::npos);
    // Zone B Systems are not listed.
    Built bOnly = FinishOne([](Registrar& r) { r.AddSystem<&ReadPos>(Sys("ReadPos", Zone::B, {At("Update", "First")})); });
    KERNEL_REQUIRE_OK(bOnly);
    const auto empty = bOnly.registry->Frozen().ExportZoneCSources(options);
    REQUIRE(static_cast<bool>(empty));
    CHECK(empty->find("\"files\": []") != std::string::npos);
}
