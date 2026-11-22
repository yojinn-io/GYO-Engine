// The hash functions of what asset puts into a World (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md,
// 5.9.1; Design Invariant AS-6; section 10 "Summary"): a Handle hashes its id string only, and AssetRequests and
// AssetView hash nothing, so World::Summary does not depend on the store's internal indices or on how a World came
// to hold its Handles.
#include "WorldAssetSupport.hpp"

namespace WorldAssetTest {
namespace {

Content WrittenContent(const std::string& name, const bool reversed) {
    Content content(name);
    content.Add("a.one", "text", "a/one.txt", "one")
        .Add("a.two", "text", "a/two.txt", "two")
        .Add("a.three", "text", "a/three.txt", "three")
        .Write(reversed);
    return content;
}

std::unique_ptr<O::Product> Make(const Content& content, AssetTest::ControlledExecutor& executor) {
    O::ProductDescription description;
    description.name = "summary";
    WA::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    description.options.Set(options);
    description.worlds.push_back(AuthorWorld("S"));
    return Build(std::move(description), Units());
}

void Hold(O::Product& p, const std::vector<std::string>& ids) {
    for (const std::string& id : ids) HeldOf(p, "S").handles.push_back(RequestsOf(p, "S").Load<Text>(Asset::AssetId(id)));
}

K::Fingerprint SummaryOf(O::Product& p) {
    auto summary = std::as_const(WorldOf(p, "S")).Summary();
    REQUIRE_MESSAGE(static_cast<bool>(summary), (summary ? std::string() : summary.error().message));
    return *summary;
}

}  // namespace

TEST_CASE("summary: two catalogs listing the entries in different orders give the same Summary for the same Handles") {
    Fresh fresh;
    const Content forward = WrittenContent("summary-forward", false);
    const Content backward = WrittenContent("summary-backward", true);
    AssetTest::ControlledExecutor executor(AssetTest::ControlledExecutor::Mode::RunNow);
    auto first = Make(forward, executor);
    auto second = Make(backward, executor);
    Hold(*first, {"a.one", "a.two", "a.three"});
    Hold(*second, {"a.one", "a.two", "a.three"});
    Frame(*first);
    Frame(*second);
    // The stores' internal indices differ: the catalogs list the entries in reverse orders.
    REQUIRE(Asset::Detail::HandleAccess::Body(HeldOf(*first, "S").handles.front())->entry !=
            Asset::Detail::HandleAccess::Body(HeldOf(*second, "S").handles.front())->entry);
    CHECK_MESSAGE(SummaryOf(*first) == SummaryOf(*second),
                  "[aw-summary-catalog-order] the Summary depends on the order the catalog lists its entries in");
    // Control: other Handles give another Summary (the Handles are hashed at all).
    auto third = Make(forward, executor);
    Hold(*third, {"a.one", "a.two"});
    Frame(*third);
    CHECK(SummaryOf(*third) != SummaryOf(*first));
}

TEST_CASE("summary: a new game and a save loaded into the same level give the same Summary") {
    Fresh fresh;
    const Content content = WrittenContent("summary-save", false);
    AssetTest::ControlledExecutor executor(AssetTest::ControlledExecutor::Mode::RunNow);
    auto product = Make(content, executor);
    O::Product& p = *product;
    Hold(p, {"a.one", "a.two"});  // a new game
    Frame(p);
    const K::Fingerprint fresh_start = SummaryOf(p);
    TheSave().ids = {"a.one", "a.two"};
    REQUIRE(static_cast<bool>(p.ReplaceWorld("S", O::ReplaceRequest{{}, {"LoadSave"}, O::ReplaceOrder::BuildNewFirst})));
    Frame(p);
    REQUIRE(TheSave().seen.size() == 2);
    CHECK(TheSave().seen[0].state == Asset::AssetState::Loaded);  // the save's request hit
    CHECK_MESSAGE(SummaryOf(p) == fresh_start, "[aw-summary-save] the loaded save's Summary differs from the new game's");
}

}  // namespace WorldAssetTest
