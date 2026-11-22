#include "engine/asset/AssetStore.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "engine/asset/ContentPath.hpp"
#include "engine/asset/ReadWholeFile.hpp"
#include "engine/base/Assert.hpp"
#include "engine/base/Sha256.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/path/Uri.hpp"

namespace GYO::Engine::Asset {
namespace Detail {

// The queue of drops (5.5): a Handle's last core pushes (entry, life) here
// without the store's lock. Closed when the store shuts down; a push then goes
// nowhere.
class DropQueue final {
public:
    struct Drop {
        std::size_t entry;
        std::uint64_t life;
    };

    void Push(const std::size_t entry, const std::uint64_t life) {
        const std::lock_guard guard(mutex_);
        if (closed_) return;
        drops_.push_back(Drop{entry, life});
    }

    [[nodiscard]] std::vector<Drop> TakeAll() {
        const std::lock_guard guard(mutex_);
        return std::exchange(drops_, {});
    }

    void Close() {
        const std::lock_guard guard(mutex_);
        closed_ = true;
        drops_.clear();
    }

private:
    std::mutex mutex_;
    std::vector<Drop> drops_;
    bool closed_ = false;
};

HandleBody::HandleBody(AssetId id, const std::size_t entry, const std::uint64_t life,
                       std::shared_ptr<DropQueue> drops, std::optional<AssetLoadFailure> failure)
    : id(std::move(id)), entry(entry), life(life), drops(std::move(drops)), failure(std::move(failure)) {}

HandleBody::~HandleBody() {
    if (drops) drops->Push(entry, life);
}

namespace {

constexpr std::uint64_t kNoLife = 0;

// The store's reader-writer lock. It records the thread that holds it
// exclusively, so taking it again on that thread (shared or exclusive) is a
// GYO_ASSERT instead of a deadlock (5.8, E0). Shared holders are not recorded.
class StoreLock final {
public:
    void Lock() {
        CheckNotHeldHere();
        mutex_.lock();
        owner_.store(std::this_thread::get_id(), std::memory_order_relaxed);
    }
    void Unlock() {
        owner_.store(std::thread::id{}, std::memory_order_relaxed);
        mutex_.unlock();
    }
    void LockShared() const {
        CheckNotHeldHere();
        mutex_.lock_shared();
    }
    void UnlockShared() const { mutex_.unlock_shared(); }

private:
    void CheckNotHeldHere() const {
        GYO_ASSERT(owner_.load(std::memory_order_relaxed) != std::this_thread::get_id() &&
                   "the asset store's lock is taken again on the thread that holds it");
    }

    mutable std::shared_mutex mutex_;
    std::atomic<std::thread::id> owner_{};
};

class WriteGuard final {
public:
    explicit WriteGuard(StoreLock& lock) : lock_(lock) { lock_.Lock(); }
    ~WriteGuard() { lock_.Unlock(); }
    WriteGuard(const WriteGuard&) = delete;
    WriteGuard& operator=(const WriteGuard&) = delete;

private:
    StoreLock& lock_;
};

class ReadGuard final {
public:
    explicit ReadGuard(const StoreLock& lock) : lock_(lock) { lock_.LockShared(); }
    ~ReadGuard() { lock_.UnlockShared(); }
    ReadGuard(const ReadGuard&) = delete;
    ReadGuard& operator=(const ReadGuard&) = delete;

private:
    const StoreLock& lock_;
};

// (type, format): what a loader is chosen by.
using Slot = std::pair<std::string, std::string>;

// How a request (or an edge) went (5.5's three branches; 5.8's counts).
enum class Branch : std::uint8_t {
    Hit,
    HitReloading,
    Revive,
    ReviveReloading,
    NewQueued,
    NewWaiting,
    NewNoLoader,
    NewBadSettings,
    NewNoExecutor,
};

// The kind an edge was counted as when it was added (5.17's counts).
enum class EdgeKind : std::uint8_t { Hit, Revive, NewQueued, NewWaiting, NewBadSettings, NewNoExecutor, Skipped, Failed };

struct Edge {
    AssetEdge view;
    std::size_t target = kNoEntry;
    EdgeKind kind = EdgeKind::Failed;
    // The hold of a Held edge.
    std::shared_ptr<const HandleBody> hold;
};

// Alive (5.5): an entry has an info only while the asset is alive.
struct Info {
    std::uint64_t life = kNoLife;
    std::weak_ptr<const HandleBody> core;
    std::uint32_t dropsToSkip = 0;
    AssetState state = AssetState::Queued;
    std::shared_ptr<const void> value;
    std::optional<std::type_index> valueType;
    std::optional<AssetLoadFailure> failure;
    std::optional<std::string> sourceDigest;
    std::vector<Edge> edges;
};

// A catalog entry: one per id, for the store's whole life.
struct Entry {
    CatalogEntry catalog;
    Slot slot;
    // The life of the info, or of the next one.
    std::uint64_t life = 1;
    // Jobs queued for this id, across lives.
    std::uint64_t attempts = 0;
    // What the registered loader made of "settings" (5.3 (B) 2).
    std::shared_ptr<const void> settings;
    std::optional<AssetLoadFailure> badSettings;
    std::unique_ptr<Info> info;
};

struct Job {
    std::size_t entry;
    std::uint64_t life;
};

struct Judgment {
    AssetState state;
    std::optional<AssetLoadFailure> failure;
};

// What a started job needs, copied under the lock.
struct Started {
    AssetId id;
    std::string path;
    std::size_t maxBytes = 0;
    std::shared_ptr<const ErasedLoader> loader;
    std::shared_ptr<const void> settings;
};

struct Outcome {
    std::shared_ptr<const void> value;
    std::optional<std::type_index> valueType;
    std::optional<AssetLoadFailure> failure;
    std::optional<std::string> sourceDigest;
    std::vector<DeclaredReference> references;
};

struct ParsedEntry {
    std::size_t entry;
    ParsedSettings result;
};

// What is released after the lock: removed infos (their values and holds),
// values of cancelled jobs, holds of edges marked cyclic.
struct Garbage {
    std::vector<std::unique_ptr<Info>> infos;
    std::vector<std::shared_ptr<const void>> values;
    std::vector<std::shared_ptr<const HandleBody>> holds;
};

AssetLoadFailure NoLoaderFailure(const Entry& entry) {
    return AssetLoadFailure::Make(AssetLoadFailureCode::NoLoader, "no loader for the entry's type and format",
                                  entry.catalog.id.Text() + " (" + entry.slot.first + ", " + entry.slot.second + ")");
}

AssetLoadFailureCode FromReference(const ReferenceFailureCode code) {
    switch (code) {
    case ReferenceFailureCode::BadReference: return AssetLoadFailureCode::BadReference;
    case ReferenceFailureCode::UnlistedReference: return AssetLoadFailureCode::UnlistedReference;
    case ReferenceFailureCode::AmbiguousReference: return AssetLoadFailureCode::AmbiguousReference;
    }
    GYO_UNREACHABLE();
}

AssetLoadFailureCode FromFileRead(const FileReadFailureCode code) {
    switch (code) {
    case FileReadFailureCode::NotFound: return AssetLoadFailureCode::NotFound;
    case FileReadFailureCode::ReadFailed: return AssetLoadFailureCode::ReadFailed;
    case FileReadFailureCode::TooLarge: return AssetLoadFailureCode::TooLarge;
    }
    GYO_UNREACHABLE();
}

// The content root made absolute against the current directory once, when the
// store is made, so jobs on any thread name the same files whatever the
// current directory is later. A root the OS cannot make absolute is kept as
// given (reading under it fails with the file's own error).
AssetStoreOptions WithAbsoluteRoot(AssetStoreOptions options) {
    if (options.contentRoot.empty()) return options;
    auto uri = IO::FS::NativeFileUri(options.contentRoot);
    if (!uri) return options;
    auto absolute = IO::Path::NativePathFromFileUri(*uri);
    if (absolute) options.contentRoot = std::move(*absolute);
    return options;
}

// Counts a started job down when it leaves Run, however it leaves.
class RunningJob final {
public:
    RunningJob(std::atomic<std::size_t>& running, std::mutex& mutex, std::condition_variable& done)
        : running_(running), mutex_(mutex), done_(done) {}
    ~RunningJob() {
        {
            const std::lock_guard guard(mutex_);
            running_.fetch_sub(1);
        }
        done_.notify_all();
    }
    RunningJob(const RunningJob&) = delete;
    RunningJob& operator=(const RunningJob&) = delete;

private:
    std::atomic<std::size_t>& running_;
    std::mutex& mutex_;
    std::condition_variable& done_;
};

} // namespace

class StoreState final : public ContentPathIndex, public std::enable_shared_from_this<StoreState> {
public:
    StoreState(const std::span<const CatalogEntry> entries, AssetStoreOptions options, AssetExecutor* executor)
        : options_(WithAbsoluteRoot(std::move(options))), executor_(executor), drops_(std::make_shared<DropQueue>()) {
        entries_.reserve(entries.size());
        for (const CatalogEntry& catalog : entries) {
            const bool added = byId_.emplace(catalog.id, entries_.size()).second;
            GYO_ASSERT(added && "the store's catalog entries have one id twice");
            byPath_[catalog.path].push_back(catalog.id);
            Entry entry;
            entry.catalog = catalog;
            entry.slot = Slot{catalog.type, EffectiveFormat(catalog)};
            entries_.push_back(std::move(entry));
        }
    }

    [[nodiscard]] std::span<const AssetId> IdsAt(const std::string_view path) const override {
        const auto found = byPath_.find(std::string(path));
        if (found == byPath_.end()) return {};
        return found->second;
    }

    // ---- Requests (5.5) ----

    std::shared_ptr<const HandleBody> Request(const AssetId& id, const std::string_view typeName) {
        const auto found = byId_.find(id);
        if (found == byId_.end()) {
            requestFailures_.fetch_add(1, std::memory_order_relaxed);
            return std::make_shared<const HandleBody>(
                id, kNoEntry, kNoLife, nullptr,
                AssetLoadFailure::Make(AssetLoadFailureCode::UnknownId, "the id is in no catalog", id.Text()));
        }
        const std::size_t index = found->second;
        const Entry& entry = entries_[index];
        if (entry.catalog.type != typeName) {
            requestFailures_.fetch_add(1, std::memory_order_relaxed);
            return FailedBody(index, AssetLoadFailure::Make(AssetLoadFailureCode::TypeMismatch,
                                                            "the requested type is not the entry's type",
                                                            id.Text() + ": requested " + std::string(typeName) +
                                                                ", the catalog has " + entry.catalog.type));
        }
        std::vector<Job> jobs;
        std::shared_ptr<const HandleBody> body;
        {
            WriteGuard guard(lock_);
            const auto [core, branch] = RequestLocked(index);
            body = core;
            CountRequest(branch);
            Dispatch(jobs);
            BeginSubmit(jobs);
        }
        SubmitJobs(jobs);
        return body;
    }

    // A request-level failure: no core, so it holds nothing and pushes nothing.
    std::shared_ptr<const HandleBody> FailedBody(const std::size_t index, AssetLoadFailure failure) {
        return std::make_shared<const HandleBody>(entries_[index].catalog.id, index, kNoLife, nullptr,
                                                  std::move(failure));
    }

    // ---- Maintenance (5.5, 5.6) ----

    void Pump() {
        while (true) {
            std::vector<DropQueue::Drop> drops = drops_->TakeAll();
            if (drops.empty()) return;
            Garbage garbage;
            {
                WriteGuard guard(lock_);
                if (closed_) return;
                for (const DropQueue::Drop& drop : drops) TakeDrop(drop, garbage);
            }
            // Removed values (and the holds of their edges, which may push
            // more drops for the next round) go here, outside the lock.
        }
    }

    // ---- Jobs ----

    void Run(const Job job) {
        Started started;
        std::vector<Job> jobs;
        {
            WriteGuard guard(lock_);
            if (closed_) return;
            Entry& entry = entries_[job.entry];
            Info* info = entry.info.get();
            if (info == nullptr || info->life != job.life) {
                // Removed before it started: nothing is read; the slot goes on.
                ++counts_.jobsCancelled;
                --inFlight_;
                Dispatch(jobs);
                BeginSubmit(jobs);
            } else {
                info->state = AssetState::Loading;
                ++version_;
                started = Start(entry);
                // Started: Shutdown waits for it to finish (O-15: when world_asset's OnDestroy returns, no job of
                // the store is still running).
                running_.fetch_add(1);
            }
        }
        if (!started.loader) {
            SubmitJobs(jobs);
            NotifyProgress();
            return;
        }
        const RunningJob counted(running_, runningMutex_, runningDone_);
        Outcome outcome = Execute(started);
        Complete(job, std::move(outcome));
    }

    // ---- Loaders (5.4, 5.3 (B)) ----

    LoaderRegistrationResult RegisterLoader(std::shared_ptr<const ErasedLoader> loader) {
        std::vector<ParsedEntry> parsed = ParseSettingsFor(*loader);
        std::vector<Job> jobs;
        {
            WriteGuard guard(lock_);
            if (auto failure = CheckRegistration(*loader)) return Base::Err(std::move(*failure));
            InstallLoader(loader, parsed);
            Dispatch(jobs);
            BeginSubmit(jobs);
        }
        SubmitJobs(jobs);
        return {};
    }

    LoaderRegistrationResult PreregisterLoader(const std::type_index loaderType, const std::string_view typeName,
                                               const std::span<const std::string_view> formats) {
        WriteGuard guard(lock_);
        // The loader type already registered: it takes the formats it took then and no other, so a format it
        // does not take would wait for nothing.
        const ErasedLoader* registered = nullptr;
        for (const auto& [slot, loader] : loaders_) {
            if (loader->loaderType == loaderType) registered = loader.get();
        }
        for (const std::string_view format : formats) {
            const Slot slot{std::string(typeName), std::string(format)};
            if (registered != nullptr &&
                std::find(registered->formats.begin(), registered->formats.end(), format) == registered->formats.end()) {
                return Base::Err(LoaderRegistrationFailure::Make(
                    LoaderRegistrationFailureCode::FormatNotTaken,
                    "the loader is registered and does not take this format", slot.first + ", " + slot.second));
            }
            if (const auto loader = loaders_.find(slot); loader != loaders_.end() &&
                                                         loader->second->loaderType != loaderType) {
                return Base::Err(LoaderRegistrationFailure::Make(
                    LoaderRegistrationFailureCode::FormatTaken,
                    "another loader is registered for this type and format", slot.first + ", " + slot.second));
            }
            if (const auto other = preregistered_.find(slot); other != preregistered_.end() &&
                                                              other->second != loaderType) {
                return Base::Err(LoaderRegistrationFailure::Make(
                    LoaderRegistrationFailureCode::FormatPreregistered,
                    "another loader type is preregistered for this type and format", slot.first + ", " + slot.second));
            }
        }
        for (const std::string_view format : formats) {
            Slot slot{std::string(typeName), std::string(format)};
            if (!loaders_.contains(slot)) preregistered_.emplace(std::move(slot), loaderType);
        }
        return {};
    }

    // ---- Reads ----

    std::shared_ptr<const void> Read(const HandleBody* body, const std::type_index type) const {
        // No core, or the core of another store (its index means nothing here).
        if (body == nullptr || !body->drops || body->drops != drops_) return nullptr;
        ReadGuard guard(lock_);
        const Info* info = entries_[body->entry].info.get();
        if (info == nullptr || info->life != body->life || info->state != AssetState::Loaded) return nullptr;
        if (!info->valueType || *info->valueType != type) return nullptr;
        return info->value;
    }

    std::uint64_t Version() const {
        ReadGuard guard(lock_);
        return version_;
    }

    std::optional<AssetSnapshot> Pull(const std::optional<std::uint64_t> lastVersion) const {
        std::uint64_t version = 0;
        std::vector<AssetRecord> records;
        {
            ReadGuard guard(lock_);
            if (lastVersion && *lastVersion == version_) return std::nullopt;
            version = version_;
            for (const Entry& entry : entries_) {
                if (entry.info) records.push_back(Record(entry));
            }
        }
        return AssetSnapshot(version, std::move(records));
    }

    AssetStoreCounts Counts() const {
        AssetStoreCounts counts;
        {
            ReadGuard guard(lock_);
            counts = counts_;
            counts.jobsWaiting = waiting_.size();
            counts.jobsInFlight = inFlight_;
        }
        counts.requestFailures = requestFailures_.load(std::memory_order_relaxed);
        counts.requests += counts.requestFailures;
        counts.fileReads = fileReads_.load(std::memory_order_relaxed);
        return counts;
    }

    AssetProbe Probe(const AssetId& id) const {
        const auto found = byId_.find(id);
        if (found == byId_.end()) return AssetProbe{};
        return ProbeEntry(found->second);
    }

    AssetProbe ProbeEntry(const std::size_t index) const {
        ReadGuard guard(lock_);
        const Entry& entry = entries_[index];
        AssetProbe probe;
        probe.life = entry.life;
        probe.attempts = entry.attempts;
        if (entry.info) {
            probe.state = entry.info->state;
            probe.dropsToSkip = entry.info->dropsToSkip;
        }
        return probe;
    }

    // ---- Shutdown (5.8, X-6) ----

    void Shutdown(const AssetShutdown mode) {
        if (mode == AssetShutdown::Drain) WaitUntilIdle();
        Garbage garbage;
        {
            WriteGuard guard(lock_);
            if (closed_) return;
            closed_ = true;
            drops_->Close();
            counts_.jobsDiscarded += waiting_.size() + inFlight_;
            waiting_.clear();
            inFlight_ = 0;
            for (Entry& entry : entries_) {
                if (!entry.info) continue;
                garbage.infos.push_back(std::move(entry.info));
                ++entry.life;
            }
            ++version_;
        }
        {
            // Jobs already in a loader finish (in both modes), find the store shut down and keep nothing; their
            // values are destroyed before they count down.
            std::unique_lock wait(runningMutex_);
            runningDone_.wait(wait, [this] { return running_.load() == 0; });
        }
        std::unique_lock wait(submitMutex_);
        submitDone_.wait(wait, [this] { return submitting_.load() == 0; });
    }

private:
    // ---- Under the lock ----

    // 5.5's judgment of an entry: BadSettings first; then a loader (and an
    // executor to run it), a preregistration, or NoLoader.
    Judgment Judge(const Entry& entry) const {
        if (entry.badSettings) return Judgment{AssetState::Failed, entry.badSettings};
        if (loaders_.contains(entry.slot)) {
            if (executor_ == nullptr) {
                return Judgment{AssetState::Failed,
                                AssetLoadFailure::Make(AssetLoadFailureCode::NoExecutor,
                                                       "the store has no executor to run the job",
                                                       entry.catalog.id.Text())};
            }
            return Judgment{AssetState::Queued, std::nullopt};
        }
        if (preregistered_.contains(entry.slot)) return Judgment{AssetState::WaitingForLoader, std::nullopt};
        return Judgment{AssetState::Failed, NoLoaderFailure(entry)};
    }

    // Sets an info that holds no value (new, Failed or waiting) to a judgment;
    // a Queued one gets a job and one more attempt. True when a job was queued.
    bool Apply(const std::size_t index, Entry& entry, Info& info, Judgment judgment) {
        info.state = judgment.state;
        info.failure = std::move(judgment.failure);
        info.sourceDigest.reset();
        if (info.state != AssetState::Queued) return false;
        ++entry.attempts;
        ++counts_.jobsQueued;
        waiting_.push_back(Job{index, info.life});
        return true;
    }

    // A Failed info is judged again on a request (5.5), as a new one would
    // be: BadSettings stays Failed (Judge gives it again), NoLoader becomes
    // Queued or WaitingForLoader once there is a loader or a
    // preregistration, the rest is queued. True when a job was queued.
    bool Rejudge(const std::size_t index, Entry& entry, Info& info) {
        if (info.state != AssetState::Failed) return false;
        Judgment judgment = Judge(entry);
        if (judgment.state == AssetState::Failed && judgment.failure->code == info.failure->code) return false;
        ++version_;
        return Apply(index, entry, info, std::move(judgment));
    }

    // Bevy's get_or_create_path_handle (server/info.rs:274-310).
    std::pair<std::shared_ptr<const HandleBody>, Branch> RequestLocked(const std::size_t index) {
        Entry& entry = entries_[index];
        if (!entry.info) {
            auto created = std::make_unique<Info>();
            created->life = entry.life;
            auto body = std::make_shared<const HandleBody>(entry.catalog.id, index, entry.life, drops_, std::nullopt);
            created->core = body;
            Info& info = *created;
            entry.info = std::move(created);
            Apply(index, entry, info, Judge(entry));
            ++version_;
            if (info.state == AssetState::Queued) return {body, Branch::NewQueued};
            if (info.state == AssetState::WaitingForLoader) return {body, Branch::NewWaiting};
            switch (info.failure->code) {
            case AssetLoadFailureCode::BadSettings: return {body, Branch::NewBadSettings};
            case AssetLoadFailureCode::NoExecutor: return {body, Branch::NewNoExecutor};
            default: return {body, Branch::NewNoLoader};
            }
        }
        Info& info = *entry.info;
        const bool reloading = Rejudge(index, entry, info);
        if (auto body = info.core.lock()) return {std::move(body), reloading ? Branch::HitReloading : Branch::Hit};
        auto body = std::make_shared<const HandleBody>(entry.catalog.id, index, info.life, drops_, std::nullopt);
        info.core = body;
        ++info.dropsToSkip;
        return {std::move(body), reloading ? Branch::ReviveReloading : Branch::Revive};
    }

    void CountRequest(const Branch branch) {
        ++counts_.requests;
        switch (branch) {
        case Branch::Hit: ++counts_.hits; break;
        case Branch::HitReloading: ++counts_.hits; ++counts_.hitsReloading; break;
        case Branch::Revive: ++counts_.revives; break;
        case Branch::ReviveReloading: ++counts_.revives; ++counts_.revivesReloading; break;
        case Branch::NewQueued: ++counts_.newLives; ++counts_.newLivesQueued; break;
        case Branch::NewWaiting: ++counts_.newLives; ++counts_.newLivesWaiting; break;
        case Branch::NewNoLoader: ++counts_.newLives; ++counts_.newLivesNoLoader; break;
        case Branch::NewBadSettings: ++counts_.newLives; ++counts_.newLivesBadSettings; break;
        case Branch::NewNoExecutor: ++counts_.newLives; ++counts_.newLivesNoExecutor; break;
        }
    }

    // Moves waiting jobs into jobs while the in-flight limit allows; after
    // shutdown, discards them.
    void Dispatch(std::vector<Job>& jobs) {
        if (closed_) {
            counts_.jobsDiscarded += waiting_.size();
            waiting_.clear();
            return;
        }
        while (!waiting_.empty() && (options_.maxInFlight == 0 || inFlight_ < options_.maxInFlight)) {
            jobs.push_back(waiting_.front());
            waiting_.pop_front();
            ++inFlight_;
        }
    }

    void BeginSubmit(const std::vector<Job>& jobs) {
        if (!jobs.empty()) submitting_.fetch_add(1);
    }

    void TakeDrop(const DropQueue::Drop& drop, Garbage& garbage) {
        ++counts_.dropsTaken;
        Entry& entry = entries_[drop.entry];
        Info* info = entry.info.get();
        if (info == nullptr || info->life != drop.life) {
            ++counts_.dropsStale;
            return;
        }
        if (info->dropsToSkip > 0) {
            --info->dropsToSkip;
            ++counts_.dropsSkipped;
            return;
        }
        garbage.infos.push_back(std::move(entry.info));
        ++entry.life;
        ++counts_.removals;
        ++version_;
    }

    Started Start(const Entry& entry) const {
        Started started;
        started.id = entry.catalog.id;
        started.path = entry.catalog.path;
        started.loader = loaders_.at(entry.slot);
        started.settings = entry.settings;
        started.maxBytes = started.loader->defaultMaxBytes;
        for (const AssetByteLimit& limit : options_.byteLimits) {
            if (limit.type == entry.slot.first && limit.format == entry.slot.second) started.maxBytes = limit.maxBytes;
        }
        return started;
    }

    // ---- Outside the lock ----

    Outcome Execute(const Started& started) {
        Outcome outcome;
        fileReads_.fetch_add(1, std::memory_order_relaxed);
        std::vector<std::byte> bytes;
        // Reading and hashing can throw (std::bad_alloc for a large file): the job still fails with a code and
        // Complete gives its in-flight slot back.
        try {
            auto read = ReadWholeFile(JoinContentRoot(options_.contentRoot, started.path), started.maxBytes);
            if (!read) {
                outcome.failure = AssetLoadFailure::Make(FromFileRead(read.error().code), read.error().message,
                                                         Base::CauseDetail(read.error(), started.id.Text()));
                return outcome;
            }
            bytes = std::move(*read);
            if (options_.sourceDigest) outcome.sourceDigest = Base::Sha256(bytes);
        } catch (const std::exception& error) {
            outcome.sourceDigest.reset();
            outcome.failure = AssetLoadFailure::Make(AssetLoadFailureCode::ReadFailed, "reading the file threw",
                                                     started.id.Text() + ": " + error.what());
            return outcome;
        } catch (...) {
            outcome.sourceDigest.reset();
            outcome.failure =
                AssetLoadFailure::Make(AssetLoadFailureCode::ReadFailed, "reading the file threw", started.id.Text());
            return outcome;
        }
        LoadContext context(started.id, started.path, started.maxBytes, this);
        try {
            LoadedValue loaded = started.loader->Load(bytes, started.settings.get(), context);
            if (loaded.failure) {
                outcome.failure = std::move(loaded.failure);
                return outcome;
            }
            outcome.value = std::move(loaded.value);
            outcome.valueType = started.loader->outputType;
            outcome.references = context.References();
        } catch (const std::exception& error) {
            outcome.failure =
                AssetLoadFailure::Make(AssetLoadFailureCode::LoaderThrew, "the loader threw",
                                       started.id.Text() + ": " + error.what());
            return outcome;
        } catch (...) {
            outcome.failure = AssetLoadFailure::Make(AssetLoadFailureCode::LoaderThrew, "the loader threw",
                                                     started.id.Text());
            return outcome;
        }
        return outcome;
    }

    // A job finished (5.5 "工作完成"): written under the lock when the life is
    // still the job's, cancelled otherwise.
    void Complete(const Job job, Outcome outcome) {
        Garbage garbage;
        std::vector<Job> jobs;
        {
            WriteGuard guard(lock_);
            if (closed_) {
                garbage.values.push_back(std::move(outcome.value));
                return;
            }
            --inFlight_;
            Entry& entry = entries_[job.entry];
            Info* info = entry.info.get();
            const bool current = info != nullptr && info->life == job.life;
            if (!current) {
                ++counts_.jobsCancelled;
                garbage.values.push_back(std::move(outcome.value));
            } else if (outcome.failure) {
                info->state = AssetState::Failed;
                info->failure = std::move(outcome.failure);
                info->sourceDigest = std::move(outcome.sourceDigest);
                ++counts_.jobsFailed;
                ++version_;
            } else {
                info->state = AssetState::Loaded;
                info->value = std::move(outcome.value);
                info->valueType = outcome.valueType;
                info->sourceDigest = std::move(outcome.sourceDigest);
                ++counts_.jobsLoaded;
                AddEdges(job.entry, std::move(outcome.references));
                MarkCycles(job.entry, garbage);
                ++version_;
            }
            Dispatch(jobs);
            BeginSubmit(jobs);
        }
        SubmitJobs(jobs);
        NotifyProgress();
    }

    // 5.17 "一條引用怎麼判": path, catalog, type name, loader or
    // preregistration (else skipped); then a request's three branches.
    void AddEdges(const std::size_t parentIndex, std::vector<DeclaredReference> references) {
        Info& parent = *entries_[parentIndex].info;
        for (DeclaredReference& reference : references) {
            Edge edge;
            edge.view.target = reference.id;
            edge.view.reference = std::move(reference);
            const DeclaredReference& declared = edge.view.reference;
            ++counts_.references;
            const auto failEdge = [&](const AssetLoadFailureCode code) {
                edge.view.status = EdgeStatus::Failed;
                edge.view.failure = code;
                edge.kind = EdgeKind::Failed;
                ++counts_.referenceFailures;
            };
            const auto found = byId_.find(declared.id);
            if (declared.failure) {
                failEdge(FromReference(*declared.failure));
            } else if (found == byId_.end()) {
                failEdge(AssetLoadFailureCode::UnknownId);
            } else if (entries_[found->second].catalog.type != declared.typeName) {
                failEdge(AssetLoadFailureCode::TypeMismatch);
            } else if (const Slot& slot = entries_[found->second].slot;
                       !loaders_.contains(slot) && !preregistered_.contains(slot)) {
                edge.view.status = EdgeStatus::Skipped;
                edge.kind = EdgeKind::Skipped;
                ++counts_.referenceSkipped;
            } else {
                edge.target = found->second;
                edge.view.status = EdgeStatus::Held;
                auto [body, branch] = RequestLocked(found->second);
                edge.hold = std::move(body);
                edge.kind = CountReference(branch);
            }
            parent.edges.push_back(std::move(edge));
        }
    }

    EdgeKind CountReference(const Branch branch) {
        switch (branch) {
        case Branch::Hit:
        case Branch::HitReloading: ++counts_.referenceHits; return EdgeKind::Hit;
        case Branch::Revive:
        case Branch::ReviveReloading: ++counts_.referenceRevives; return EdgeKind::Revive;
        case Branch::NewQueued:
            ++counts_.referenceNewLives;
            ++counts_.referenceNewLivesQueued;
            return EdgeKind::NewQueued;
        case Branch::NewWaiting:
            ++counts_.referenceNewLives;
            ++counts_.referenceNewLivesWaiting;
            return EdgeKind::NewWaiting;
        case Branch::NewBadSettings:
            ++counts_.referenceNewLives;
            ++counts_.referenceNewLivesBadSettings;
            return EdgeKind::NewBadSettings;
        case Branch::NewNoExecutor:
            ++counts_.referenceNewLives;
            ++counts_.referenceNewLivesNoExecutor;
            return EdgeKind::NewNoExecutor;
        case Branch::NewNoLoader: break;
        }
        // A held edge passed the loader-or-preregistration step.
        GYO_UNREACHABLE();
    }

    void UncountReference(const EdgeKind kind) {
        switch (kind) {
        case EdgeKind::Hit: --counts_.referenceHits; return;
        case EdgeKind::Revive: --counts_.referenceRevives; return;
        case EdgeKind::NewQueued:
            --counts_.referenceNewLives;
            --counts_.referenceNewLivesQueued;
            return;
        case EdgeKind::NewWaiting:
            --counts_.referenceNewLives;
            --counts_.referenceNewLivesWaiting;
            return;
        case EdgeKind::NewBadSettings:
            --counts_.referenceNewLives;
            --counts_.referenceNewLivesBadSettings;
            return;
        case EdgeKind::NewNoExecutor:
            --counts_.referenceNewLives;
            --counts_.referenceNewLivesNoExecutor;
            return;
        case EdgeKind::Skipped:
        case EdgeKind::Failed: break;
        }
        GYO_UNREACHABLE();
    }

    // 5.17 "循環引用": only the edges of `added` are new, so a new strongly
    // connected component contains it: the nodes it reaches that also reach it.
    // Every held edge inside becomes Cyclic and lets go; an edge marked so
    // stays so (it is still an edge of the graph) until its parent's life ends.
    void MarkCycles(const std::size_t added, Garbage& garbage) {
        const auto graphEdge = [](const Edge& edge) {
            return edge.target != kNoEntry && (edge.view.status == EdgeStatus::Held ||
                                               edge.view.status == EdgeStatus::Cyclic);
        };
        // A new component contains one of added's edges: none, no new cycle (and no walk over every info).
        const std::vector<Edge>& addedEdges = entries_[added].info->edges;
        if (std::none_of(addedEdges.begin(), addedEdges.end(), graphEdge)) return;
        std::unordered_set<std::size_t> forward{added};
        std::vector<std::size_t> stack{added};
        while (!stack.empty()) {
            const std::size_t node = stack.back();
            stack.pop_back();
            const Info* info = entries_[node].info.get();
            if (info == nullptr) continue;
            for (const Edge& edge : info->edges) {
                if (graphEdge(edge) && forward.insert(edge.target).second) stack.push_back(edge.target);
            }
        }
        std::unordered_map<std::size_t, std::vector<std::size_t>> reverse;
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            const Info* info = entries_[index].info.get();
            if (info == nullptr) continue;
            for (const Edge& edge : info->edges) {
                if (graphEdge(edge)) reverse[edge.target].push_back(index);
            }
        }
        std::unordered_set<std::size_t> component{added};
        stack.assign(1, added);
        while (!stack.empty()) {
            const std::size_t node = stack.back();
            stack.pop_back();
            const auto sources = reverse.find(node);
            if (sources == reverse.end()) continue;
            for (const std::size_t source : sources->second) {
                if (forward.contains(source) && component.insert(source).second) stack.push_back(source);
            }
        }
        for (const std::size_t member : component) {
            Info* info = entries_[member].info.get();
            if (info == nullptr) continue;
            for (Edge& edge : info->edges) {
                if (edge.view.status != EdgeStatus::Held || !component.contains(edge.target)) continue;
                edge.view.status = EdgeStatus::Cyclic;
                edge.view.failure = AssetLoadFailureCode::CyclicReference;
                UncountReference(edge.kind);
                ++counts_.referenceCyclic;
                garbage.holds.push_back(std::move(edge.hold));
            }
        }
    }

    std::optional<LoaderRegistrationFailure> CheckRegistration(const ErasedLoader& loader) const {
        if (const auto owner = typeNameOwners_.find(loader.typeName);
            owner != typeNameOwners_.end() && owner->second != loader.outputType) {
            return LoaderRegistrationFailure::Make(LoaderRegistrationFailureCode::TypeNameTaken,
                                                   "another C++ type declared this type name", loader.typeName);
        }
        for (const std::string& format : loader.formats) {
            const Slot slot{loader.typeName, format};
            if (const auto other = loaders_.find(slot);
                other != loaders_.end() && other->second->loaderType != loader.loaderType) {
                return LoaderRegistrationFailure::Make(LoaderRegistrationFailureCode::FormatTaken,
                                                       "another loader is registered for this type and format",
                                                       slot.first + ", " + slot.second);
            }
            if (const auto other = preregistered_.find(slot);
                other != preregistered_.end() && other->second != loader.loaderType) {
                return LoaderRegistrationFailure::Make(
                    LoaderRegistrationFailureCode::FormatPreregistered,
                    "another loader type is preregistered for this type and format", slot.first + ", " + slot.second);
            }
        }
        return std::nullopt;
    }

    // Puts the loader in the table, writes the settings it read for its new
    // pairs, ends its preregistrations and judges the infos that waited.
    void InstallLoader(const std::shared_ptr<const ErasedLoader>& loader, std::vector<ParsedEntry>& parsed) {
        typeNameOwners_.emplace(loader->typeName, loader->outputType);
        std::set<Slot> changed;
        for (const std::string& format : loader->formats) {
            Slot slot{loader->typeName, format};
            if (loaders_.emplace(slot, loader).second) changed.insert(std::move(slot));
        }
        for (ParsedEntry& entry : parsed) {
            Entry& target = entries_[entry.entry];
            if (!changed.contains(target.slot)) continue;
            target.settings = std::move(entry.result.settings);
            target.badSettings = std::move(entry.result.failure);
        }
        // Its preregistrations end; the waiters of formats it does not take
        // become NoLoader (5.4).
        for (auto preregistration = preregistered_.begin(); preregistration != preregistered_.end();) {
            if (preregistration->second != loader->loaderType) {
                ++preregistration;
                continue;
            }
            changed.insert(preregistration->first);
            preregistration = preregistered_.erase(preregistration);
        }
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            Entry& entry = entries_[index];
            if (!entry.info || entry.info->state != AssetState::WaitingForLoader) continue;
            if (!changed.contains(entry.slot)) continue;
            Apply(index, entry, *entry.info, Judge(entry));
            ++version_;
        }
    }

    // ---- Outside the lock ----

    std::vector<ParsedEntry> ParseSettingsFor(const ErasedLoader& loader) const {
        std::vector<ParsedEntry> parsed;
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            const Entry& entry = entries_[index];
            if (entry.slot.first != loader.typeName) continue;
            if (std::find(loader.formats.begin(), loader.formats.end(), entry.slot.second) == loader.formats.end()) {
                continue;
            }
            const std::optional<std::string_view> json =
                entry.catalog.settings ? std::optional<std::string_view>(*entry.catalog.settings) : std::nullopt;
            ParsedSettings result;
            try {
                result = loader.ParseSettings(json);
            } catch (const std::exception& error) {
                result.failure = AssetLoadFailure::Make(AssetLoadFailureCode::BadSettings,
                                                        "the loader threw reading the settings",
                                                        entry.catalog.id.Text() + ": " + error.what());
            } catch (...) {
                result.failure = AssetLoadFailure::Make(AssetLoadFailureCode::BadSettings,
                                                        "the loader threw reading the settings",
                                                        entry.catalog.id.Text());
            }
            if (result.failure) result.failure->detail = entry.catalog.id.Text() + "; " + result.failure->detail;
            parsed.push_back(ParsedEntry{index, std::move(result)});
        }
        return parsed;
    }

    void SubmitJobs(const std::vector<Job>& jobs) {
        if (jobs.empty()) return;
        for (const Job& job : jobs) {
            executor_->Submit([state = shared_from_this(), job] { state->Run(job); });
        }
        {
            const std::lock_guard guard(submitMutex_);
            submitting_.fetch_sub(1);
        }
        submitDone_.notify_all();
    }

    void NotifyProgress() {
        {
            const std::lock_guard guard(progressMutex_);
            ++progress_;
        }
        progressChanged_.notify_all();
    }

    void WaitUntilIdle() {
        while (true) {
            std::uint64_t seen = 0;
            {
                const std::lock_guard guard(progressMutex_);
                seen = progress_;
            }
            {
                ReadGuard guard(lock_);
                if (closed_ || (waiting_.empty() && inFlight_ == 0)) return;
            }
            std::unique_lock wait(progressMutex_);
            progressChanged_.wait(wait, [&] { return progress_ != seen; });
        }
    }

    AssetRecord Record(const Entry& entry) const {
        const Info& info = *entry.info;
        AssetRecord record;
        record.id = entry.catalog.id;
        record.type = entry.catalog.type;
        record.life = info.life;
        record.attempts = entry.attempts;
        record.state = info.state;
        record.value = info.value;
        record.valueType = info.valueType;
        record.failure = info.failure;
        record.sourceDigest = info.sourceDigest;
        record.edges.reserve(info.edges.size());
        for (const Edge& edge : info.edges) record.edges.push_back(edge.view);
        return record;
    }

    // Fixed at construction.
    const AssetStoreOptions options_;
    AssetExecutor* const executor_;
    std::vector<Entry> entries_;
    std::unordered_map<AssetId, std::size_t> byId_;
    std::unordered_map<std::string, std::vector<AssetId>> byPath_;
    const std::shared_ptr<DropQueue> drops_;

    // Guarded by lock_ (the parts of entries_ that change, too).
    StoreLock lock_;
    std::map<Slot, std::shared_ptr<const ErasedLoader>> loaders_;
    std::map<Slot, std::type_index> preregistered_;
    std::map<std::string, std::type_index> typeNameOwners_;
    std::deque<Job> waiting_;
    std::size_t inFlight_ = 0;
    bool closed_ = false;
    std::uint64_t version_ = 1;
    AssetStoreCounts counts_;

    // Not guarded by lock_.
    std::atomic<std::uint64_t> requestFailures_{0};
    std::atomic<std::uint64_t> fileReads_{0};
    // Rounds of SubmitJobs under way: incremented under lock_ (so none starts
    // after shutdown), decremented under submitMutex_.
    std::atomic<std::size_t> submitting_{0};
    std::mutex submitMutex_;
    std::condition_variable submitDone_;
    // Jobs started (past the life check, into the file and the loader) and not yet through Complete:
    // incremented under lock_ (so none starts after shutdown), decremented under runningMutex_.
    std::atomic<std::size_t> running_{0};
    std::mutex runningMutex_;
    std::condition_variable runningDone_;
    std::uint64_t progress_ = 0;
    std::mutex progressMutex_;
    std::condition_variable progressChanged_;
};

LoaderRegistrationResult RegisterLoader(StoreState& state, std::shared_ptr<const ErasedLoader> loader) {
    return state.RegisterLoader(std::move(loader));
}

LoaderRegistrationResult PreregisterLoader(StoreState& state, const std::type_index loaderType,
                                           const std::string_view typeName,
                                           const std::span<const std::string_view> formats) {
    return state.PreregisterLoader(loaderType, typeName, formats);
}

std::shared_ptr<const HandleBody> Request(StoreState& state, const AssetId& id, const std::string_view typeName) {
    return state.Request(id, typeName);
}

std::shared_ptr<const void> Read(const StoreState& state, const HandleBody* body, const std::type_index type) {
    return state.Read(body, type);
}

AssetStoreCounts Counts(const StoreState& state) { return state.Counts(); }

AssetProbe Probe(const StoreState& state, const AssetId& id) { return state.Probe(id); }

} // namespace Detail

AssetStore::AssetStore(const std::span<const CatalogEntry> entries, AssetStoreOptions options,
                       AssetExecutor* executor)
    : state_(std::make_shared<Detail::StoreState>(entries, std::move(options), executor)) {}

AssetStore::~AssetStore() { Shutdown(AssetShutdown::Discard); }

void AssetStore::Pump() { state_->Pump(); }

std::uint64_t AssetStore::Version() const { return state_->Version(); }

AssetSnapshot AssetStore::Snapshot() const { return *state_->Pull(std::nullopt); }

std::optional<AssetSnapshot> AssetStore::PullIfChanged(const std::uint64_t lastVersion) const {
    return state_->Pull(lastVersion);
}

AssetStoreCounts AssetStore::Counts() const { return state_->Counts(); }

AssetProbe AssetStore::Probe(const AssetId& id) const { return state_->Probe(id); }

void AssetStore::Shutdown(const AssetShutdown mode) { state_->Shutdown(mode); }

} // namespace GYO::Engine::Asset
