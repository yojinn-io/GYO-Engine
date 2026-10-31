#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/framework/CatalogRegistration.hpp"
#include "gyo/framework/PathDialog.hpp"

#include <string>
#include <string_view>
#include <thread>

using namespace Gyo::Tools::Framework;

namespace {

constexpr const char* kCatalog = R"({
  "version": 1,
  "assets": [
    { "id": "game.text.settings", "type": "text", "path": "settings.json" },
    { "id": "game.audio.shot",
      "type": "sound", "path": "audio/shot.wav" },

    { "id": "game.font.ui", "type": "font", "path": "fonts/ui.ttf" }
  ]
}
)";

} // namespace

TEST_CASE("Registering a new asset appends one line and keeps every other byte") {
    const auto update = RegisterAsset(kCatalog, {"game.audio.reload", "sound", "audio/reload.wav"});
    REQUIRE(update.has_value());
    CHECK(update->added);
    const std::string catalog = kCatalog;
    const std::size_t last = catalog.find("\"fonts/ui.ttf\" }") + std::string_view("\"fonts/ui.ttf\" }").size();
    CHECK(update->text == catalog.substr(0, last) +
                              ",\n    { \"id\": \"game.audio.reload\", \"type\": \"sound\", \"path\": \"audio/reload.wav\" }" +
                              catalog.substr(last));
}

TEST_CASE("The asset type is the caller's (Design Invariant 3)") {
    const auto update = RegisterAsset(kCatalog, {"game.text.credits", "text", "credits.json"});
    REQUIRE(update.has_value());
    CHECK(update->text.find(R"({ "id": "game.text.credits", "type": "text", "path": "credits.json" })") !=
          std::string::npos);
}

TEST_CASE("Registering an existing asset rewrites only its entry") {
    const auto update = RegisterAsset(kCatalog, {"game.audio.shot", "sound", "audio/shot_v2.wav"});
    REQUIRE(update.has_value());
    CHECK_FALSE(update->added);
    REQUIRE(update->previousPath.has_value());
    CHECK(*update->previousPath == "audio/shot.wav");
    std::string expected = kCatalog;
    const std::size_t begin = expected.find("{ \"id\": \"game.audio.shot\"");
    const std::size_t end = expected.find('}', begin) + 1;
    expected.replace(begin, end - begin, R"({ "id": "game.audio.shot", "type": "sound", "path": "audio/shot_v2.wav" })");
    CHECK(update->text == expected);

    const auto same = RegisterAsset(kCatalog, {"game.audio.shot", "sound", "audio/shot.wav"});
    REQUIRE(same.has_value());
    CHECK_FALSE(same->previousPath.has_value());
}

TEST_CASE("Registering into an empty assets array") {
    const auto update = RegisterAsset("{\n  \"version\": 1,\n  \"assets\": []\n}\n", {"a.b", "sound", "b.wav"});
    REQUIRE(update.has_value());
    CHECK(update->text ==
          "{\n  \"version\": 1,\n  \"assets\": [\n    { \"id\": \"a.b\", \"type\": \"sound\", \"path\": \"b.wav\" }\n  ]\n}\n");
}

TEST_CASE("Registration refuses what the content contract or another entry forbids") {
    const auto code = [](const std::string_view text, const AssetRegistration& registration) {
        const auto result = RegisterAsset(text, registration);
        REQUIRE_FALSE(result.has_value());
        return result.error().code;
    };
    CHECK(code(kCatalog, {"game.text.settings", "sound", "x.wav"}) == CatalogErrorCode::InvalidRegistration); // another type
    CHECK(code(kCatalog, {"game.audio.shot", "text", "audio/shot.wav"}) == CatalogErrorCode::InvalidRegistration);
    CHECK(code(kCatalog, {"game.audio.new", "sound", "fonts/ui.ttf"}) == CatalogErrorCode::InvalidRegistration); // taken path
    CHECK(code(kCatalog, {"", "sound", "x.wav"}) == CatalogErrorCode::InvalidRegistration);
    CHECK(code(kCatalog, {"game.audio.new", "", "x.wav"}) == CatalogErrorCode::InvalidRegistration);
    for (const char* path : {"", "/abs.wav", "a/../b.wav", "./a.wav", "a//b.wav", "a\\b.wav", "C:/a.wav", "content.json"}) {
        CAPTURE(path);
        CHECK(code(kCatalog, {"game.audio.new", "sound", path}) == CatalogErrorCode::InvalidRegistration);
    }
    CHECK(code(R"({"version": 2, "assets": []})", {"a", "sound", "a.wav"}) == CatalogErrorCode::InvalidCatalog);
    CHECK(code(R"({"version": 1})", {"a", "sound", "a.wav"}) == CatalogErrorCode::InvalidCatalog);
    CHECK(code(R"({"version": 1, "assets": [{"id": "a", "type": "text", "path": "a"}, {"id": "a", "type": "text", "path": "b"}]})",
               {"c", "sound", "c.wav"}) == CatalogErrorCode::InvalidCatalog);
    CHECK(code("not json", {"a", "sound", "a.wav"}) == CatalogErrorCode::InvalidCatalog);
}

// An answer posted from another thread (as SDL may call back) is taken once,
// in posting order, by the tool's thread.
TEST_CASE("The path dialog inbox hands every answer over once, in order") {
    PathDialogInbox inbox;
    std::thread poster([&inbox] {
        inbox.Post({.tag = 3, .outcome = PathDialogOutcome::Chosen, .path = "a.json"});
        inbox.Post({.tag = 7, .outcome = PathDialogOutcome::Cancelled});
    });
    poster.join();
    const auto taken = inbox.Take();
    REQUIRE(taken.size() == 2);
    CHECK(taken[0].tag == 3);
    CHECK(taken[0].path == "a.json");
    CHECK(taken[1].tag == 7);
    CHECK(taken[1].outcome == PathDialogOutcome::Cancelled);
    CHECK(inbox.Take().empty());
}
