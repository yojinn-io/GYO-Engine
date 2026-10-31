// Path dialogs of the SDL platform (docs/architecture/platform-sdl.md, Design
// Invariant 10), mostly through a fake launcher. CI never opens a real dialog:
// the real launcher runs only with SDL's dialog driver hint set, which makes
// every SDL backend answer Failed at once; the OS's threads and dialogs are
// outside these tests.

#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using Engine::Platform::Sdl::PathDialogAnswer;
using Engine::Platform::Sdl::PathDialogCallback;
using Engine::Platform::Sdl::PathDialogKind;
using Engine::Platform::Sdl::PathDialogLauncher;
using Engine::Platform::Sdl::PathDialogOutcome;
using Engine::Platform::Sdl::PathDialogRequest;
using Engine::Platform::Sdl::PlatformEvent;
using Engine::Platform::Sdl::SdlPlatform;

namespace {

std::unique_ptr<SdlPlatform> MakePlatform() {
    auto platform = SdlPlatform::Create();
    REQUIRE(platform);
    return std::move(platform.value());
}

// One request as the launcher received it; the test calls back when it likes.
struct LaunchedDialog final {
    PathDialogCallback callback{};
    void* userdata{};
    SDL_Window* window{};
    PathDialogKind kind{};
    const SDL_DialogFileFilter* filters{};
    int filterCount{};
    const char* defaultLocation{};

    void Answer(const char* const* files) const { callback(userdata, files, -1); }
    void Choose(const char* path) const {
        const char* files[] = {path, nullptr};
        Answer(files);
    }
};

std::vector<LaunchedDialog> launched;

void Keep(PathDialogCallback callback, void* userdata, SDL_Window* window, PathDialogKind kind,
          const SDL_DialogFileFilter* filters, int filterCount, const char* defaultLocation) noexcept {
    launched.push_back({callback, userdata, window, kind, filters, filterCount, defaultLocation});
}

// Answers before show returns, as SDL may.
void ChooseAtOnce(PathDialogCallback callback, void* userdata, SDL_Window*, PathDialogKind,
                  const SDL_DialogFileFilter*, int, const char*) noexcept {
    const char* files[] = {"at/once.json", nullptr};
    callback(userdata, files, 0);
}

constexpr PathDialogLauncher keeping{&Keep};
constexpr PathDialogLauncher choosingAtOnce{&ChooseAtOnce};

PathDialogRequest Request(std::uint64_t tag, PathDialogKind kind = PathDialogKind::OpenFile) {
    PathDialogRequest request;
    request.tag = tag;
    request.kind = kind;
    return request;
}

} // namespace

TEST_CASE("A path dialog answered before show returns is handed out with its tag") {
    auto platform = MakePlatform();
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(41), choosingAtOnce);
    const auto answers = platform->TakePathDialogAnswers();
    REQUIRE(answers.size() == 1);
    CHECK(answers[0].tag == 41);
    CHECK(answers[0].outcome == PathDialogOutcome::Chosen);
    CHECK(answers[0].path == "at/once.json");
    CHECK(answers[0].error.empty());
}

TEST_CASE("Path dialog answers are handed out once, in the order they arrive") {
    auto platform = MakePlatform();
    launched.clear();
    for (std::uint64_t tag : {1U, 2U, 3U}) Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(tag), keeping);
    REQUIRE(launched.size() == 3);
    CHECK(platform->TakePathDialogAnswers().empty());
    launched[2].Choose("c");
    launched[0].Choose("a");
    launched[1].Choose("b");
    const auto answers = platform->TakePathDialogAnswers();
    REQUIRE(answers.size() == 3);
    CHECK(answers[0].tag == 3);
    CHECK(answers[1].tag == 1);
    CHECK(answers[2].tag == 2);
    CHECK(answers[0].path == "c");
    CHECK(platform->TakePathDialogAnswers().empty());
}

TEST_CASE("A cancelled path dialog and one that failed are told apart") {
    auto platform = MakePlatform();
    launched.clear();
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(7), keeping);
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(8), keeping);
    REQUIRE(launched.size() == 2);
    const char* none[] = {nullptr};
    launched[0].Answer(none);
    SDL_SetError("no dialog portal");
    launched[1].Answer(nullptr);
    const auto answers = platform->TakePathDialogAnswers();
    REQUIRE(answers.size() == 2);
    CHECK(answers[0].tag == 7);
    CHECK(answers[0].outcome == PathDialogOutcome::Cancelled);
    CHECK(answers[0].path.empty());
    CHECK(answers[1].tag == 8);
    CHECK(answers[1].outcome == PathDialogOutcome::Failed);
    CHECK(answers[1].error == "no dialog portal");
}

TEST_CASE("A path dialog gets the window, kind, filters and location the request names") {
    auto platform = MakePlatform();
    launched.clear();
    {
        PathDialogRequest request = Request(5, PathDialogKind::SaveFile);
        request.filters = {{"UI layout", "json"}, {"Sound", "wav;wave"}};
        request.defaultLocation = "/tmp/out.json";
        Engine::Platform::Sdl::ShowPathDialogWith(*platform, std::move(request), keeping);
        // The request is gone here: SDL's pointers must not point into it.
    }
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(6, PathDialogKind::OpenFolder), keeping);
    REQUIRE(launched.size() == 2);
    const LaunchedDialog& save = launched[0];
    CHECK(save.window == Engine::Platform::Sdl::NativeWindow(*platform));
    CHECK(save.kind == PathDialogKind::SaveFile);
    REQUIRE(save.filterCount == 2);
    // Read when the dialog would answer, after the request was moved and destroyed.
    CHECK(std::string(save.filters[0].name) == "UI layout");
    CHECK(std::string(save.filters[0].pattern) == "json");
    CHECK(std::string(save.filters[1].name) == "Sound");
    CHECK(std::string(save.filters[1].pattern) == "wav;wave");
    REQUIRE(save.defaultLocation != nullptr);
    CHECK(std::string(save.defaultLocation) == "/tmp/out.json");
    const LaunchedDialog& folder = launched[1];
    CHECK(folder.kind == PathDialogKind::OpenFolder);
    CHECK(folder.filters == nullptr);
    CHECK(folder.filterCount == 0);
    CHECK(folder.defaultLocation == nullptr);
    save.Choose("x");
    folder.Choose("y");
    CHECK(platform->TakePathDialogAnswers().size() == 2);
}

TEST_CASE("A path dialog answer may come from another thread") {
    auto platform = MakePlatform();
    launched.clear();
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(9), keeping);
    REQUIRE(launched.size() == 1);
    const LaunchedDialog dialog = launched[0];
    std::thread([dialog] { dialog.Choose("from/another/thread"); }).join();
    const auto answers = platform->TakePathDialogAnswers();
    REQUIRE(answers.size() == 1);
    CHECK(answers[0].tag == 9);
    CHECK(answers[0].path == "from/another/thread");
}

TEST_CASE("A path dialog answer after the platform is destroyed is dropped safely") {
    launched.clear();
    {
        auto platform = MakePlatform();
        Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(10), keeping);
        Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(11), keeping);
        launched[1].Choose("arrived but never taken");
    }
    REQUIRE(launched.size() == 2);
    // The request and the mailbox outlive the platform until the callback.
    launched[0].Choose("late");
    auto next = MakePlatform();
    CHECK(next->TakePathDialogAnswers().empty());
}

TEST_CASE("Showing a path dialog fails a GYO_ASSERT inside an observer") {
    auto platform = MakePlatform();
    launched.clear();
    SDL_Event trigger{};
    trigger.type = SDL_EVENT_USER;
    trigger.user.code = 4343;
    REQUIRE(SDL_PushEvent(&trigger));
    bool ran = false;
    (void)platform->PumpEvents([&](const PlatformEvent& view) {
        const SDL_Event& event = Engine::Platform::Sdl::NativeEvent(view);
        if (event.type != SDL_EVENT_USER || event.user.code != 4343 || ran) return;
        ran = true;
        GYO_CHECK_ASSERTS(Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(12), keeping));
    });
    REQUIRE(ran);
    CHECK(launched.empty());
}

namespace {

// What the real launcher handed SDL, through fake SDL functions.
struct SdlCall final {
    std::string function;
    SDL_Window* window{};
    const SDL_DialogFileFilter* filters{};
    int filterCount{-2};
    const char* location{};
    bool allowMany{};
};
SdlCall lastSdlCall;

void SDLCALL FakeOpenFile(SDL_DialogFileCallback, void*, SDL_Window* window, const SDL_DialogFileFilter* filters,
                          int count, const char* location, bool many) {
    lastSdlCall = {"open", window, filters, count, location, many};
}
void SDLCALL FakeSaveFile(SDL_DialogFileCallback, void*, SDL_Window* window, const SDL_DialogFileFilter* filters,
                          int count, const char* location) {
    lastSdlCall = {"save", window, filters, count, location, false};
}
void SDLCALL FakeOpenFolder(SDL_DialogFileCallback, void*, SDL_Window* window, const char* location, bool many) {
    lastSdlCall = {"folder", window, nullptr, -1, location, many};
}

void SDLCALL NoCallback(void*, const char* const*, int) noexcept {}

} // namespace

TEST_CASE("The real path dialog launcher hands each kind to its SDL function with the request's arguments") {
    using Engine::Platform::Sdl::ShowWithSdlFunctions;
    const Engine::Platform::Sdl::SdlDialogFunctions fakes{&FakeOpenFile, &FakeSaveFile, &FakeOpenFolder};
    auto* const window = reinterpret_cast<SDL_Window*>(std::uintptr_t{0x1000});
    const SDL_DialogFileFilter filters[] = {{"Layout", "json"}};
    const char* const location = "/tmp/start";

    ShowWithSdlFunctions(fakes, &NoCallback, nullptr, window, PathDialogKind::OpenFile, filters, 1, location);
    CHECK(lastSdlCall.function == "open");
    CHECK(lastSdlCall.window == window);
    CHECK(lastSdlCall.filters == filters);
    CHECK(lastSdlCall.filterCount == 1);
    CHECK(lastSdlCall.location == location);
    CHECK_FALSE(lastSdlCall.allowMany);

    ShowWithSdlFunctions(fakes, &NoCallback, nullptr, window, PathDialogKind::SaveFile, filters, 1, location);
    CHECK(lastSdlCall.function == "save");
    CHECK(lastSdlCall.window == window);
    CHECK(lastSdlCall.filters == filters);
    CHECK(lastSdlCall.filterCount == 1);
    CHECK(lastSdlCall.location == location);

    ShowWithSdlFunctions(fakes, &NoCallback, nullptr, window, PathDialogKind::OpenFolder, filters, 1, location);
    CHECK(lastSdlCall.function == "folder");
    CHECK(lastSdlCall.window == window);
    CHECK(lastSdlCall.location == location);
    CHECK_FALSE(lastSdlCall.allowMany);
}

TEST_CASE("A failed path dialog may be followed by one more answer, and no more") {
    // SDL 3.4 on Windows answers Failed from the modern dialog, then again from the legacy one.
    auto platform = MakePlatform();
    launched.clear();
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(13), keeping);
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(17), keeping);
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(18), keeping);
    REQUIRE(launched.size() == 3);
    SDL_SetError("modern dialog failed");
    launched[0].Answer(nullptr);
    launched[0].Choose("legacy/choice.json");
    launched[0].Choose("a third call");
    launched[1].Choose("first");
    launched[1].Choose("second");
    launched[2].Answer(nullptr);
    launched[2].Answer(nullptr);
    launched[2].Choose("after two failures");
    const auto answers = platform->TakePathDialogAnswers();
    REQUIRE(answers.size() == 5);
    CHECK(answers[3].tag == 18);
    CHECK(answers[3].outcome == PathDialogOutcome::Failed);
    CHECK(answers[4].tag == 18);
    CHECK(answers[4].outcome == PathDialogOutcome::Failed);
    CHECK(answers[0].tag == 13);
    CHECK(answers[0].outcome == PathDialogOutcome::Failed);
    CHECK(answers[1].tag == 13);
    CHECK(answers[1].outcome == PathDialogOutcome::Chosen);
    CHECK(answers[1].path == "legacy/choice.json");
    CHECK(answers[2].tag == 17);
    CHECK(answers[2].path == "first");
    CHECK(platform->TakePathDialogAnswers().empty());
}

TEST_CASE("Equal path dialog requests are all shown and all answered") {
    // The platform never refuses, merges or deduplicates a request (Design Invariant 10).
    auto platform = MakePlatform();
    launched.clear();
    for (int i = 0; i < 5; ++i) {
        PathDialogRequest request = Request(30);
        request.defaultLocation = "/same/place";
        Engine::Platform::Sdl::ShowPathDialogWith(*platform, std::move(request), keeping);
    }
    REQUIRE(launched.size() == 5);
    for (const LaunchedDialog& dialog : launched) dialog.Choose("same");
    const auto answers = platform->TakePathDialogAnswers();
    REQUIRE(answers.size() == 5);
    for (const PathDialogAnswer& answer : answers) CHECK(answer.tag == 30);
}

TEST_CASE("A path dialog answer carries the whole tag, the first path and no stray text") {
    auto platform = MakePlatform();
    launched.clear();
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(0xFFFFFFFF00000001ULL), keeping);
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(31), keeping);
    REQUIRE(launched.size() == 2);
    const char* several[] = {"first.json", "second.json", nullptr};
    launched[0].Answer(several);
    const char* none[] = {nullptr};
    launched[1].Answer(none);
    const auto answers = platform->TakePathDialogAnswers();
    REQUIRE(answers.size() == 2);
    CHECK(answers[0].tag == 0xFFFFFFFF00000001ULL);
    CHECK(answers[0].path == "first.json");
    CHECK(answers[0].error.empty());
    CHECK(answers[1].outcome == PathDialogOutcome::Cancelled);
    CHECK(answers[1].path.empty());
    CHECK(answers[1].error.empty());
}

namespace {

// A launcher whose requests another thread answers while the main thread keeps showing and taking.
std::mutex racingMutex;
std::vector<LaunchedDialog> racingQueue;

void KeepForAnotherThread(PathDialogCallback callback, void* userdata, SDL_Window* window, PathDialogKind kind,
                          const SDL_DialogFileFilter* filters, int filterCount, const char* location) noexcept {
    const std::lock_guard lock(racingMutex);
    racingQueue.push_back({callback, userdata, window, kind, filters, filterCount, location});
}

} // namespace

TEST_CASE("Path dialog answers from another thread race safely with showing and taking") {
    auto platform = MakePlatform();
    constexpr std::uint64_t count = 500;
    std::atomic<bool> showing{true};
    std::thread answering([&] {
        std::uint64_t answered = 0;
        while (answered < count) {
            std::vector<LaunchedDialog> batch;
            {
                const std::lock_guard lock(racingMutex);
                batch.swap(racingQueue);
            }
            for (const LaunchedDialog& dialog : batch) dialog.Choose("raced");
            answered += batch.size();
            if (batch.empty() && !showing) std::this_thread::yield();
        }
    });
    std::vector<std::uint64_t> tags;
    for (std::uint64_t tag = 1; tag <= count; ++tag) {
        Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(tag), {&KeepForAnotherThread});
        for (const PathDialogAnswer& answer : platform->TakePathDialogAnswers()) tags.push_back(answer.tag);
    }
    showing = false;
    answering.join();
    for (const PathDialogAnswer& answer : platform->TakePathDialogAnswers()) tags.push_back(answer.tag);
    REQUIRE(tags.size() == count);
    std::sort(tags.begin(), tags.end());
    for (std::uint64_t i = 0; i < count; ++i) CHECK(tags[i] == i + 1);
}

TEST_CASE("An empty first path from a path dialog counts as cancelled") {
    // zenity reports a cancelled dialog as one empty path.
    auto platform = MakePlatform();
    launched.clear();
    Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(14), keeping);
    REQUIRE(launched.size() == 1);
    launched[0].Choose("");
    const auto answers = platform->TakePathDialogAnswers();
    REQUIRE(answers.size() == 1);
    CHECK(answers[0].outcome == PathDialogOutcome::Cancelled);
}

TEST_CASE("A path dialog request with no valid kind or an empty pattern fails a GYO_ASSERT") {
    auto platform = MakePlatform();
    launched.clear();
    GYO_CHECK_ASSERTS(Engine::Platform::Sdl::ShowPathDialogWith(*platform, Request(15, PathDialogKind{}), keeping));
    PathDialogRequest request = Request(16);
    request.filters = {{"Anything", ""}};
    GYO_CHECK_ASSERTS(Engine::Platform::Sdl::ShowPathDialogWith(*platform, std::move(request), keeping));
    CHECK(launched.empty());
}

TEST_CASE("The real path dialog launcher calls back once per request when SDL has no dialog driver") {
    auto platform = MakePlatform();
    // Every SDL dialog backend fails at once, before showing anything, when this hint names no driver it has.
    // It stays set: on Linux, resetting it makes SDL probe D-Bus and zenity again.
    REQUIRE(SDL_SetHint(SDL_HINT_FILE_DIALOG_DRIVER, "none"));
    platform->ShowPathDialog(Request(21, PathDialogKind::OpenFile));
    PathDialogRequest save = Request(22, PathDialogKind::SaveFile);
    save.filters = {{"UI layout", "json"}};
    platform->ShowPathDialog(std::move(save));
    platform->ShowPathDialog(Request(23, PathDialogKind::OpenFolder));
    const auto answers = platform->TakePathDialogAnswers();
    REQUIRE(answers.size() == 3);
    for (std::size_t i = 0; i < answers.size(); ++i) {
        CHECK(answers[i].tag == 21 + i);
        CHECK(answers[i].outcome == PathDialogOutcome::Failed);
        CHECK_FALSE(answers[i].error.empty());
    }
}
