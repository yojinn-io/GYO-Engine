#pragma once

// Support for the store's tests (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md,
// section 10; step 2 of section 9): content written to a fresh directory, executors the tests control
// (X-5), and the test loaders of section 10 (a slow one that can be told to fail, one in an engine
// format, one that declares references).

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <ostream>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "engine/asset/AssetExecutor.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/asset/Text.hpp"
#include "engine/asset/TextLoader.hpp"

namespace AssetTest {

namespace Asset = GYO::Engine::Asset;
namespace Base = GYO::Engine::Base;
using namespace std::chrono_literals;

// The bounded wait of the tests (section 10: 2 seconds; the normal path never waits for it).
inline constexpr auto kBound = 2s;

// ---- Content ----

// A content root under the test binary directory, emptied first, and the catalog entries a test adds.
class TestContent final {
public:
    explicit TestContent(const std::string& name) : root_(std::string(GYO_ASSET_TEST_BINARY_DIR) + "/store/" + name) {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    void WriteFile(const std::string& path, const std::string& bytes) const {
        const std::filesystem::path file = std::filesystem::path(root_) / path;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    // An entry; with bytes, its file too.
    TestContent& Add(const std::string& id, const std::string& type, const std::string& path,
                     std::optional<std::string> bytes = std::nullopt, std::optional<std::string> settings = std::nullopt,
                     std::optional<std::string> format = std::nullopt) {
        Asset::CatalogEntry entry;
        entry.id = Asset::AssetId(id);
        entry.type = type;
        entry.path = path;
        entry.format = std::move(format);
        entry.settings = std::move(settings);
        entries_.push_back(std::move(entry));
        if (bytes) WriteFile(path, *bytes);
        return *this;
    }

    [[nodiscard]] const std::string& Root() const noexcept { return root_; }
    [[nodiscard]] std::span<const Asset::CatalogEntry> Entries() const noexcept { return entries_; }

    [[nodiscard]] Asset::AssetStoreOptions Options() const {
        Asset::AssetStoreOptions options;
        options.contentRoot = root_;
        return options;
    }

private:
    std::string root_;
    std::vector<Asset::CatalogEntry> entries_;
};

// ---- Executors (X-5) ----

// Holds every job until the test runs it: on this thread (RunNext, RunAt, RunAll) or on a thread of its own
// (StartNext), so starting and finishing a job are controlled apart (with a gate in the loader). In
// RunNow mode a Submit runs the queue at once, queue-style: a Submit made while the queue is running is
// only queued.
class ControlledExecutor final : public Asset::AssetExecutor {
public:
    enum class Mode { Hold, RunNow };

    explicit ControlledExecutor(const Mode mode = Mode::Hold) : mode_(mode) {}

    void Submit(Job job) override {
        if (onSubmit) onSubmit();
        {
            const std::lock_guard guard(mutex_);
            if (stopped_) return;
            jobs_.push_back(std::move(job));
            ++submitted_;
        }
        if (mode_ == Mode::RunNow) RunAll();
    }

    [[nodiscard]] std::size_t Pending() const {
        const std::lock_guard guard(mutex_);
        return jobs_.size();
    }
    [[nodiscard]] std::size_t Submitted() const {
        const std::lock_guard guard(mutex_);
        return submitted_;
    }

    // Runs the job at this place in the queue; false when there is none.
    bool RunAt(const std::size_t place) {
        Job job;
        {
            const std::lock_guard guard(mutex_);
            if (place >= jobs_.size()) return false;
            job = std::move(jobs_[place]);
            jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(place));
        }
        job();
        return true;
    }
    bool RunNext() { return RunAt(0); }

    // Runs until the queue is empty, jobs submitted meanwhile included; a nested call returns at once.
    std::size_t RunAll() {
        if (running_.exchange(true)) return 0;
        std::size_t count = 0;
        while (RunNext()) ++count;
        running_ = false;
        return count;
    }

    // Starts the next job on a thread of its own.
    [[nodiscard]] std::thread StartNext() {
        Job job;
        {
            const std::lock_guard guard(mutex_);
            REQUIRE_FALSE(jobs_.empty());
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        return std::thread(std::move(job));
    }

    // Later Submits are dropped (an executor that has stopped).
    void Stop() {
        const std::lock_guard guard(mutex_);
        stopped_ = true;
        jobs_.clear();
    }

    std::function<void()> onSubmit;

private:
    const Mode mode_;
    mutable std::mutex mutex_;
    std::deque<Job> jobs_;
    std::size_t submitted_ = 0;
    bool stopped_ = false;
    std::atomic<bool> running_{false};
};

// Real threads: a few workers taking jobs in order.
class PoolExecutor final : public Asset::AssetExecutor {
public:
    explicit PoolExecutor(const std::size_t threads) {
        for (std::size_t index = 0; index < threads; ++index) workers_.emplace_back([this] { Work(); });
    }
    ~PoolExecutor() override { Stop(); }
    PoolExecutor(const PoolExecutor&) = delete;
    PoolExecutor& operator=(const PoolExecutor&) = delete;

    void Submit(Job job) override {
        {
            const std::lock_guard guard(mutex_);
            if (stopped_) return;
            jobs_.push_back(std::move(job));
        }
        wake_.notify_one();
    }

    // Drops what has not started and joins.
    void Stop() {
        {
            const std::lock_guard guard(mutex_);
            stopped_ = true;
            jobs_.clear();
        }
        wake_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

private:
    void Work() {
        while (true) {
            Job job;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock, [this] { return stopped_ || !jobs_.empty(); });
                if (stopped_) return;
                job = std::move(jobs_.front());
                jobs_.pop_front();
            }
            job();
        }
    }

    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> jobs_;
    bool stopped_ = false;
    std::vector<std::thread> workers_;
};

// ---- Hooks: a callback that calls the store (AS-11, "持鎖不做的事") ----

struct Hooks {
    std::atomic<Asset::AssetStore*> store{nullptr};
    std::atomic<bool> destructor{false};
    std::atomic<bool> settings{false};
    std::mutex labelMutex;
    std::string label;
};

inline Hooks& TheHooks() {
    static Hooks hooks;
    return hooks;
}

inline void SetHookLabel(std::string label) {
    const std::lock_guard guard(TheHooks().labelMutex);
    TheHooks().label = std::move(label);
}

// Calls an entry of the store that takes its lock. If the store holds its lock here, that is the
// re-entry GYO_ASSERT; the line before it names the hook and the phase, so each mutant has its own
// expected failure.
inline void CallStoreFromHook(const char* what) {
    Asset::AssetStore* store = TheHooks().store.load();
    if (store == nullptr) return;
    std::string label;
    {
        const std::lock_guard guard(TheHooks().labelMutex);
        label = TheHooks().label;
    }
    std::fprintf(stderr, "[hook] %s during %s\n", what, label.c_str());
    std::fflush(stderr);
    (void)store->Version();
}

// ---- The script of the test loaders ----

// What the test loaders do, set by a test: ids whose load waits at a gate, ids whose next loads fail, and
// what was called. Loaders stay pure functions of their input apart from this test control.
class Script final {
public:
    void Reset() {
        const std::lock_guard guard(mutex_);
        gated_.clear();
        failures_.clear();
        calls_.clear();
        current_ = 0;
        peak_ = 0;
    }
    void Gate(const std::string& id) {
        const std::lock_guard guard(mutex_);
        gated_.insert(id);
    }
    void Open(const std::string& id) {
        {
            const std::lock_guard guard(mutex_);
            gated_.erase(id);
        }
        changed_.notify_all();
    }
    void OpenAll() {
        {
            const std::lock_guard guard(mutex_);
            gated_.clear();
        }
        changed_.notify_all();
    }
    void FailNext(const std::string& id, const int times = 1) {
        const std::lock_guard guard(mutex_);
        failures_[id] = times;
    }
    // Waits until the loaders were called this many times.
    bool WaitForCalls(const std::size_t count, const std::chrono::milliseconds bound = kBound) {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, bound, [&] { return calls_.size() >= count; });
    }
    [[nodiscard]] std::vector<std::string> Calls() const {
        const std::lock_guard guard(mutex_);
        return calls_;
    }
    [[nodiscard]] std::size_t CallsOf(const std::string& id) const {
        const std::lock_guard guard(mutex_);
        return static_cast<std::size_t>(std::count(calls_.begin(), calls_.end(), id));
    }
    [[nodiscard]] int Peak() const {
        const std::lock_guard guard(mutex_);
        return peak_;
    }

    // In a loader: counts the call, waits at the gate (bounded, so a broken test does not hang), and says
    // whether this call is to fail.
    bool Enter(const std::string& id) {
        std::unique_lock lock(mutex_);
        calls_.push_back(id);
        ++current_;
        peak_ = std::max(peak_, current_);
        changed_.notify_all();
        changed_.wait_for(lock, 10s, [&] { return !gated_.contains(id); });
        --current_;
        const auto failure = failures_.find(id);
        if (failure == failures_.end() || failure->second == 0) return false;
        --failure->second;
        return true;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::set<std::string> gated_;
    std::map<std::string, int> failures_;
    std::vector<std::string> calls_;
    int current_ = 0;
    int peak_ = 0;
};

inline Script& TheScript() {
    static Script script;
    return script;
}

// ---- Test types ----

// Destructions of Probe values: how many, and on which thread the last one ran (AS-10 (c)).
struct Destructions {
    std::atomic<int> count{0};
    std::mutex mutex;
    std::thread::id lastThread;
};

inline Destructions& TheDestructions() {
    static Destructions destructions;
    return destructions;
}

// Content of type "probe": the file's bytes.
class Probe final {
public:
    explicit Probe(std::string bytes) : bytes_(std::move(bytes)) {}
    Probe(Probe&& other) noexcept : bytes_(std::move(other.bytes_)), moved_(true) { other.moved_ = true; }
    Probe(const Probe&) = delete;
    Probe& operator=(const Probe&) = delete;
    Probe& operator=(Probe&&) = delete;
    ~Probe() {
        // Only the value the store keeps (made by moving the loader's result) counts.
        if (!moved_ || bytes_.empty()) return;
        ++TheDestructions().count;
        {
            const std::lock_guard guard(TheDestructions().mutex);
            TheDestructions().lastThread = std::this_thread::get_id();
        }
        if (TheHooks().destructor.load()) CallStoreFromHook("value destructor");
    }

    [[nodiscard]] const std::string& Bytes() const noexcept { return bytes_; }

private:
    std::string bytes_;
    bool moved_ = false;
};

// Content of type "linked": its bytes, and the references it declared. Lines: "id:<id>" (a linked),
// "probe:<id>" (a probe), "image:<id>" (an image), "text:<id>" (a text), "path:<path>" (a linked by
// path), "probe-path:<path>" (a probe by path); other lines are data.
struct Linked {
    std::string bytes;
    std::vector<Asset::AssetId> references;
};

// "image": for preregistration.
struct Image {
    std::string bytes;
};

// A C++ type that declares the type name "probe" too (5.3 (B) 4).
struct ProbeTwin {
    std::string bytes;
};

} // namespace AssetTest

template <>
struct GYO::Engine::Asset::AssetTypeName<AssetTest::Probe> {
    static constexpr std::string_view value = "probe";
};
template <>
struct GYO::Engine::Asset::AssetTypeName<AssetTest::Linked> {
    static constexpr std::string_view value = "linked";
};
template <>
struct GYO::Engine::Asset::AssetTypeName<AssetTest::Image> {
    static constexpr std::string_view value = "image";
};
template <>
struct GYO::Engine::Asset::AssetTypeName<AssetTest::ProbeTwin> {
    static constexpr std::string_view value = "probe";
};

namespace AssetTest {

enum class TestLoaderCode : std::uint8_t { Scripted = 1, BadSettings };

[[maybe_unused]] constexpr const char* ToString(const TestLoaderCode code) noexcept {
    return code == TestLoaderCode::Scripted ? "Scripted" : "BadSettings";
}

using TestFailure = Asset::LoaderFailure<TestLoaderCode>;

struct TestSettings {
    std::string json;
};

inline std::string BytesText(const std::span<const std::byte> bytes) {
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

// Settings with "bad" in them cannot be read.
inline Base::Result<TestSettings, TestFailure> ParseTestSettings(const std::optional<std::string_view> json) {
    if (TheHooks().settings.load()) CallStoreFromHook("settings parser");
    if (json && json->find("bad") != std::string_view::npos) {
        return Base::Err(TestFailure::Make(TestLoaderCode::BadSettings, "bad settings", std::string(*json)));
    }
    return TestSettings{json ? std::string(*json) : std::string()};
}

// The slow loader that can be told to fail (section 10): type "probe", format "probe". Bytes starting with
// "throw" make it throw.
struct ProbeLoader {
    using Output = Probe;
    using Settings = TestSettings;
    using Code = TestLoaderCode;
    static constexpr std::string_view kTypeName = "probe";
    static constexpr std::array<std::string_view, 1> kFormats{"probe"};
    static constexpr std::size_t kDefaultMaxBytes = 1024;

    Base::Result<Settings, TestFailure> ParseSettings(std::optional<std::string_view> json) const {
        return ParseTestSettings(json);
    }
    Base::Result<Probe, TestFailure> Load(std::span<const std::byte> bytes, const Settings&,
                                          Asset::LoadContext& context) const {
        const std::string text = BytesText(bytes);
        if (TheScript().Enter(context.Id().Text())) {
            return Base::Err(TestFailure::Make(TestLoaderCode::Scripted, "told to fail", context.Id().Text()));
        }
        if (text.starts_with("throw")) throw std::runtime_error("probe loader threw");
        return Probe(text);
    }
};

// The loader that declares references (section 10): type "linked", format "links".
struct LinkedLoader {
    using Output = Linked;
    using Settings = TestSettings;
    using Code = TestLoaderCode;
    static constexpr std::string_view kTypeName = "linked";
    static constexpr std::array<std::string_view, 1> kFormats{"links"};
    static constexpr std::size_t kDefaultMaxBytes = 1024;

    Base::Result<Settings, TestFailure> ParseSettings(std::optional<std::string_view> json) const {
        return ParseTestSettings(json);
    }
    Base::Result<Linked, TestFailure> Load(std::span<const std::byte> bytes, const Settings&,
                                           Asset::LoadContext& context) const {
        Linked value{BytesText(bytes), {}};
        if (TheScript().Enter(context.Id().Text())) {
            return Base::Err(TestFailure::Make(TestLoaderCode::Scripted, "told to fail", context.Id().Text()));
        }
        std::size_t start = 0;
        const std::string_view text = value.bytes;
        while (start < text.size()) {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos) end = text.size();
            const std::string_view line = text.substr(start, end - start);
            const auto after = [&](const std::string_view prefix) { return line.substr(prefix.size()); };
            if (line.starts_with("id:")) {
                value.references.push_back(context.Reference<Linked>(Asset::AssetId(after("id:"))).Id());
            } else if (line.starts_with("probe:")) {
                value.references.push_back(context.Reference<Probe>(Asset::AssetId(after("probe:"))).Id());
            } else if (line.starts_with("image:")) {
                value.references.push_back(context.Reference<Image>(Asset::AssetId(after("image:"))).Id());
            } else if (line.starts_with("text:")) {
                value.references.push_back(context.Reference<Asset::Text>(Asset::AssetId(after("text:"))).Id());
            } else if (line.starts_with("path:")) {
                value.references.push_back(context.ReferencePath<Linked>(after("path:")).Id());
            } else if (line.starts_with("probe-path:")) {
                value.references.push_back(context.ReferencePath<Probe>(after("probe-path:")).Id());
            }
            start = end + 1;
        }
        return value;
    }
};

// A loader in an engine format (section 10): type "text", format "gyotxt"; the file is "GYOTXT\n" and
// the text.
struct GyoTextLoader {
    using Output = Asset::Text;
    using Settings = TestSettings;
    using Code = TestLoaderCode;
    static constexpr std::string_view kTypeName = "text";
    static constexpr std::array<std::string_view, 1> kFormats{"gyotxt"};
    static constexpr std::size_t kDefaultMaxBytes = 1024;

    Base::Result<Settings, TestFailure> ParseSettings(std::optional<std::string_view> json) const {
        return ParseTestSettings(json);
    }
    Base::Result<Asset::Text, TestFailure> Load(std::span<const std::byte> bytes, const Settings&,
                                               Asset::LoadContext& context) const {
        const std::string text = BytesText(bytes);
        if (!text.starts_with("GYOTXT\n")) {
            return Base::Err(TestFailure::Make(TestLoaderCode::Scripted, "not gyotxt", context.Id().Text()));
        }
        return Asset::Text(text.substr(7));
    }
};

// Another loader for ("text", "txt"), to collide with the text loader.
struct OtherTextLoader {
    using Output = Asset::Text;
    using Settings = TestSettings;
    using Code = TestLoaderCode;
    static constexpr std::string_view kTypeName = "text";
    static constexpr std::array<std::string_view, 1> kFormats{"txt"};
    static constexpr std::size_t kDefaultMaxBytes = 1024;

    Base::Result<Settings, TestFailure> ParseSettings(std::optional<std::string_view> json) const {
        return ParseTestSettings(json);
    }
    Base::Result<Asset::Text, TestFailure> Load(std::span<const std::byte> bytes, const Settings&,
                                               Asset::LoadContext&) const {
        return Asset::Text(BytesText(bytes));
    }
};

// Image loaders for preregistration: "png" only, "jpg" only, and a second "png".
template <int Kind, std::size_t N>
struct ImageLoaderOf {
    using Output = Image;
    using Settings = TestSettings;
    using Code = TestLoaderCode;
    static constexpr std::string_view kTypeName = "image";
    static constexpr std::array<std::string_view, N> kFormats = Kind == 2 ? std::array<std::string_view, N>{"jpg"}
                                                                          : std::array<std::string_view, N>{"png"};
    static constexpr std::size_t kDefaultMaxBytes = 1024;

    Base::Result<Settings, TestFailure> ParseSettings(std::optional<std::string_view> json) const {
        return ParseTestSettings(json);
    }
    Base::Result<Image, TestFailure> Load(std::span<const std::byte> bytes, const Settings&,
                                          Asset::LoadContext&) const {
        return Image{BytesText(bytes)};
    }
};
using PngLoader = ImageLoaderOf<1, 1>;
using JpgLoader = ImageLoaderOf<2, 1>;
using OtherPngLoader = ImageLoaderOf<3, 1>;

// Declares "probe" for ProbeTwin (5.3 (B) 4).
struct ProbeTwinLoader {
    using Output = ProbeTwin;
    using Settings = TestSettings;
    using Code = TestLoaderCode;
    static constexpr std::string_view kTypeName = "probe";
    static constexpr std::array<std::string_view, 1> kFormats{"twin"};
    static constexpr std::size_t kDefaultMaxBytes = 1024;

    Base::Result<Settings, TestFailure> ParseSettings(std::optional<std::string_view> json) const {
        return ParseTestSettings(json);
    }
    Base::Result<ProbeTwin, TestFailure> Load(std::span<const std::byte> bytes, const Settings&,
                                              Asset::LoadContext&) const {
        return ProbeTwin{BytesText(bytes)};
    }
};

// ---- Checks ----

// 5.8's equations (the job one with what waits or is in flight on the right).
inline void CheckEquations(const Asset::AssetStoreCounts& c) {
    CHECK(c.requests == c.hits + c.revives + c.newLives + c.requestFailures);
    CHECK(c.newLives ==
          c.newLivesQueued + c.newLivesWaiting + c.newLivesNoLoader + c.newLivesBadSettings + c.newLivesNoExecutor);
    CHECK(c.references == c.referenceHits + c.referenceRevives + c.referenceNewLives + c.referenceSkipped +
                              c.referenceFailures + c.referenceCyclic);
    CHECK(c.referenceNewLives == c.referenceNewLivesQueued + c.referenceNewLivesWaiting +
                                     c.referenceNewLivesBadSettings + c.referenceNewLivesNoExecutor);
    CHECK(c.jobsQueued == c.jobsLoaded + c.jobsFailed + c.jobsCancelled + c.jobsDiscarded + c.jobsWaiting +
                              c.jobsInFlight);
    CHECK(c.dropsTaken == c.dropsStale + c.dropsSkipped + c.removals);
}

inline bool Idle(const Asset::AssetStoreCounts& c) { return c.jobsWaiting == 0 && c.jobsInFlight == 0; }

// Waits (bounded) until no job waits or is in flight.
inline bool WaitIdle(const Asset::AssetStore& store, const std::chrono::milliseconds bound = 10s) {
    const auto until = std::chrono::steady_clock::now() + bound;
    while (std::chrono::steady_clock::now() < until) {
        if (Idle(store.Counts())) return true;
        std::this_thread::sleep_for(1ms);
    }
    return false;
}

inline Asset::AssetState StateOf(const Asset::AssetStore& store, const std::string& id) {
    return store.Probe(Asset::AssetId(id)).state;
}

inline std::optional<Asset::AssetLoadFailureCode> FailureOf(const Asset::AssetStore& store, const std::string& id) {
    const Asset::AssetSnapshot snapshot = store.Snapshot();
    const Asset::AssetRecord* record = snapshot.Find(Asset::AssetId(id));
    if (record == nullptr || !record->failure) return std::nullopt;
    return record->failure->code;
}

// Everything of a store's contents that does not depend on order: lives and attempts are left out
// (section 10, equivalence).
struct ContentLine {
    std::string id;
    std::string state;
    std::string failure;
    std::string digest;
    std::string value;
    std::string edges;
    std::string withDependencies;
    friend bool operator==(const ContentLine&, const ContentLine&) = default;
};

inline std::string ValueText(const Asset::AssetRecord& record) {
    if (auto probe = record.Value<Probe>()) return "probe:" + probe->Bytes();
    if (auto linked = record.Value<Linked>()) return "linked:" + linked->bytes;
    if (auto text = record.Value<Asset::Text>()) return "text:" + text->Bytes();
    return record.value ? "other" : "";
}

inline std::vector<ContentLine> Contents(const Asset::AssetSnapshot& snapshot) {
    std::vector<ContentLine> lines;
    for (const Asset::AssetRecord& record : snapshot.Records()) {
        ContentLine line;
        line.id = record.id.Text();
        line.state = Asset::ToString(record.state);
        line.failure = record.failure ? Asset::ToString(record.failure->code) : "";
        line.digest = record.sourceDigest.value_or("");
        line.value = ValueText(record);
        for (const Asset::AssetEdge& edge : record.edges) {
            line.edges += edge.target.Text() + "/" + Asset::ToString(edge.status) + "/" +
                          (edge.failure ? Asset::ToString(*edge.failure) : "") + ";";
        }
        const Asset::DependencyStatus status = snapshot.WithDependencies(record.id);
        line.withDependencies = Asset::ToString(status.state);
        for (const Asset::DependencyFailure& failure : status.failures) {
            line.withDependencies += " " + failure.parent.Text() + ">" + failure.reference.Text() + ":" +
                                     Asset::ToString(failure.code);
        }
        lines.push_back(std::move(line));
    }
    std::sort(lines.begin(), lines.end(), [](const ContentLine& a, const ContentLine& b) { return a.id < b.id; });
    return lines;
}

inline std::string Describe(const std::vector<ContentLine>& lines) {
    std::string text;
    for (const ContentLine& line : lines) {
        text += line.id + " " + line.state + " " + line.failure + " " + line.value + " [" + line.edges + "] " +
                line.withDependencies + "\n";
    }
    return text;
}

// Registers the loaders most tests use; each must succeed.
inline void RegisterTestLoaders(const Asset::AssetStore& store) {
    REQUIRE(store.Loaders().Register<ProbeLoader>());
    REQUIRE(store.Loaders().Register<LinkedLoader>());
    REQUIRE(store.Loaders().Register<Asset::TextLoader>());
}

// Resets the shared test state at the start and end of a test.
struct Fresh {
    Fresh() { Clear(); }
    ~Fresh() { Clear(); }
    static void Clear() {
        TheScript().Reset();
        TheHooks().store = nullptr;
        TheHooks().destructor = false;
        TheHooks().settings = false;
        SetHookLabel("");
    }
};

} // namespace AssetTest
