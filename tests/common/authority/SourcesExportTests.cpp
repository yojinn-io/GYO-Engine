// G-5, the authority half (authority declaration 5.9; the former spec J-17,
// moved from the kernel, authority-rollback declaration I13): the authority
// Module exports the source files of what writes decision data. This case
// writes the export of the synthetic fixture for authority.sources_scan
// (authority_sources_check.py), which runs the product-side scanner
// tests/common/authority_scan/authority_scan.py on it.

#include "AuthorityTestSupport.hpp"

#include <fstream>

void RegisterSyntheticSimulation(GYO::Engine::Orchestration::WorldSetup& setup);
void RegisterAnotherSimulation(GYO::Engine::Orchestration::WorldSetup& setup);
void RegisterReaderOnly(GYO::Engine::Orchestration::WorldSetup& setup);

using namespace GYO::Test::Authority;

namespace {
constexpr const char* kFixtureRoot = "tests/common/authority/sources_fixture";
} // namespace

TEST_CASE("authority sources: the G-5 export lists the fixture files as gyo.authority-sources v1") {
    auto built = Build([](O::WorldSetup& s) {
        RegisterSyntheticSimulation(s);
        RegisterAnotherSimulation(s);
        RegisterReaderOnly(s);
    });
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    const auto exported = A::ExportAuthoritySources((*built)->World("Main").GetWorld(),
                                                    {GYO_AUTHORITY_TEST_REPOSITORY_ROOT, kFixtureRoot, {}, "0000000"});
    REQUIRE_MESSAGE(static_cast<bool>(exported), (exported ? std::string() : exported.error().message + " " + exported.error().detail));
    const std::string& text = *exported;
    CHECK_MESSAGE(text.find("\"version\": 1") != std::string::npos, "[au-g5-header]");
    CHECK_MESSAGE(text.find("\"generated_by\": \"gyo-authority-report\"") != std::string::npos, "[au-g5-header]");
    CHECK_MESSAGE(text.find("\"owner_root\": \"tests/common/authority/sources_fixture\"") != std::string::npos, "[au-g5-header]");
    // One writer in the first file: listed once; registered second, sorted first. A file with a reader only is not
    // listed.
    const std::string file = "\"tests/common/authority/sources_fixture/SyntheticSimulation.cpp\"";
    const std::string secondFile = "\"tests/common/authority/sources_fixture/AnotherSimulation.cpp\"";
    REQUIRE_MESSAGE((text.find(file) != std::string::npos && text.find(secondFile) != std::string::npos), "[au-g5-lists] " << text);
    CHECK_MESSAGE(text.find(file, text.find(file) + 1) == std::string::npos, "[au-g5-once]");
    CHECK_MESSAGE(text.find(secondFile) < text.find(file), "[au-g5-sorted]");
    CHECK_MESSAGE(text.find("ReaderOnly.cpp") == std::string::npos, "[au-g5-reader] " << text);
    std::ofstream out(std::string(GYO_AUTHORITY_TEST_BINARY_DIR) + "/authority_sources.json", std::ios::binary);
    out << text;
    REQUIRE(out.good());
}
