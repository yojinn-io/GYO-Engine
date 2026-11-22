#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>
#include <vector>

#include "engine/asset/AssetExecutor.hpp"
#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetLoadFailure.hpp"
#include "engine/asset/AssetLoader.hpp"
#include "engine/asset/AssetRef.hpp"
#include "engine/asset/AssetSnapshot.hpp"
#include "engine/asset/AssetTypeName.hpp"
#include "engine/asset/Catalog.hpp"
#include "engine/asset/Handle.hpp"
#include "engine/asset/LoadContext.hpp"

// The asset store: asset management outside the World, one per Product
// (declaration 5.4 to 5.8, 5.17; Design Invariants AS-1, AS-4, AS-5, AS-8,
// AS-10, AS-11, AS-13). It knows nothing of ECS; world_asset owns one, and a
// tool that builds no World uses it directly (Request, Pump, Read).
//
// The algorithm is Bevy's bevy_asset (5.5): one catalog entry per id, with an
// info only while the asset is alive; a strong core per Handle with a queue of
// drops; three branches of a request (no info, info whose core can be upgraded,
// info whose core went: a revival that skips one drop); maintenance that takes
// drops; lives so a late result or a late drop is told from the current one.
// Every decision is made under one reader-writer lock that also guards the
// loader table. Under the lock the store only works on its tables: it never
// reads a file, decodes, calls a loader's functions, submits to the executor
// or destroys a value (those happen after the lock is released). Taking the
// lock again on the thread that holds it exclusively is a GYO_ASSERT.

namespace GYO::Engine::Asset {

struct AssetByteLimit final {
    std::string type;
    std::string format;
    std::size_t maxBytes = 0;
};

struct AssetStoreOptions final {
    // The content root, a native path in UTF-8; an entry's file is
    // contentRoot + "/" + its path.
    std::string contentRoot;
    // SHA-256 of each file's bytes as read (5.5, decision 10).
    bool sourceDigest = true;
    // Jobs submitted to the executor and not finished at once; 0 is no limit.
    std::size_t maxInFlight = 0;
    // Overrides of loaders' default byte limits, by (type, format).
    std::vector<AssetByteLimit> byteLimits;
};

// Counts for the reconciliation of 5.8 (Design Invariant AS-8). Read with
// AssetStore::Counts.
struct AssetStoreCounts final {
    // Request calls: requests == hits + revives + newLives + requestFailures;
    // newLives == newLivesQueued + newLivesWaiting + newLivesNoLoader +
    // newLivesBadSettings + newLivesNoExecutor. hitsReloading and
    // revivesReloading are the hits and revivals that queued a job (a Failed
    // info judged again).
    std::uint64_t requests = 0;
    std::uint64_t hits = 0;
    std::uint64_t hitsReloading = 0;
    std::uint64_t revives = 0;
    std::uint64_t revivesReloading = 0;
    std::uint64_t newLives = 0;
    std::uint64_t newLivesQueued = 0;
    std::uint64_t newLivesWaiting = 0;
    std::uint64_t newLivesNoLoader = 0;
    std::uint64_t newLivesBadSettings = 0;
    std::uint64_t newLivesNoExecutor = 0;
    std::uint64_t requestFailures = 0;

    // Declared references, each counted once when its edge is added (5.17):
    // references == referenceHits + referenceRevives + referenceNewLives +
    // referenceSkipped + referenceFailures + referenceCyclic; referenceNewLives
    // == referenceNewLivesQueued + referenceNewLivesWaiting +
    // referenceNewLivesBadSettings + referenceNewLivesNoExecutor. An edge
    // marked cyclic later moves from its first kind to referenceCyclic.
    std::uint64_t references = 0;
    std::uint64_t referenceHits = 0;
    std::uint64_t referenceRevives = 0;
    std::uint64_t referenceNewLives = 0;
    std::uint64_t referenceNewLivesQueued = 0;
    std::uint64_t referenceNewLivesWaiting = 0;
    std::uint64_t referenceNewLivesBadSettings = 0;
    std::uint64_t referenceNewLivesNoExecutor = 0;
    std::uint64_t referenceSkipped = 0;
    std::uint64_t referenceFailures = 0;
    std::uint64_t referenceCyclic = 0;

    // Jobs (read and decode): jobsQueued == jobsLoaded + jobsFailed +
    // jobsCancelled + jobsDiscarded + jobsWaiting + jobsInFlight. With nothing
    // waiting or in flight it is 5.8's equation.
    std::uint64_t jobsQueued = 0;
    std::uint64_t jobsLoaded = 0;
    // Id-level failures after a job ran.
    std::uint64_t jobsFailed = 0;
    // The life changed before the job started or before it finished.
    std::uint64_t jobsCancelled = 0;
    // Not finished when the store shut down.
    std::uint64_t jobsDiscarded = 0;
    // Queued behind the in-flight limit.
    std::uint64_t jobsWaiting = 0;
    // Submitted to the executor and not finished.
    std::uint64_t jobsInFlight = 0;
    // Files a job started to read.
    std::uint64_t fileReads = 0;

    // Maintenance: dropsTaken == dropsStale + dropsSkipped + removals.
    std::uint64_t dropsTaken = 0;
    std::uint64_t dropsStale = 0;
    std::uint64_t dropsSkipped = 0;
    std::uint64_t removals = 0;
};

// The store's own read of one id, for tests and tools (5.5: the only place
// NotRequested appears).
struct AssetProbe final {
    AssetState state = AssetState::NotRequested;
    // The life of the info, or of the next one when not alive.
    std::uint64_t life = 0;
    std::uint64_t attempts = 0;
    std::uint32_t dropsToSkip = 0;
};

namespace Detail {

class StoreState;

struct ParsedSettings final {
    std::shared_ptr<const void> settings;
    std::optional<AssetLoadFailure> failure;
};

struct LoadedValue final {
    std::shared_ptr<const void> value;
    std::optional<AssetLoadFailure> failure;
};

// A loader with its types erased; the store keeps these in its loader table.
class ErasedLoader {
public:
    ErasedLoader(std::type_index loaderType, std::type_index outputType, std::string typeName,
                 std::vector<std::string> formats, std::size_t defaultMaxBytes)
        : loaderType(loaderType), outputType(outputType), typeName(std::move(typeName)),
          formats(std::move(formats)), defaultMaxBytes(defaultMaxBytes) {}
    virtual ~ErasedLoader() = default;
    ErasedLoader(const ErasedLoader&) = delete;
    ErasedLoader& operator=(const ErasedLoader&) = delete;

    [[nodiscard]] virtual ParsedSettings ParseSettings(std::optional<std::string_view> json) const = 0;
    [[nodiscard]] virtual LoadedValue Load(std::span<const std::byte> bytes, const void* settings,
                                           LoadContext& context) const = 0;

    const std::type_index loaderType;
    const std::type_index outputType;
    const std::string typeName;
    const std::vector<std::string> formats;
    const std::size_t defaultMaxBytes;
};

template <AssetLoader L>
class LoaderModel final : public ErasedLoader {
public:
    explicit LoaderModel(L loader)
        : ErasedLoader(std::type_index(typeid(L)), std::type_index(typeid(typename L::Output)),
                       std::string(L::kTypeName), Formats(), L::kDefaultMaxBytes),
          loader_(std::move(loader)) {}

    [[nodiscard]] ParsedSettings ParseSettings(const std::optional<std::string_view> json) const override {
        auto parsed = loader_.ParseSettings(json);
        if (!parsed) {
            return ParsedSettings{nullptr, AssetLoadFailure::Make(AssetLoadFailureCode::BadSettings,
                                                                  "the loader cannot read the settings",
                                                                  Base::CauseDetail(parsed.error()))};
        }
        return ParsedSettings{std::make_shared<const typename L::Settings>(std::move(*parsed)), std::nullopt};
    }

    [[nodiscard]] LoadedValue Load(const std::span<const std::byte> bytes, const void* settings,
                                   LoadContext& context) const override {
        auto value = loader_.Load(bytes, *static_cast<const typename L::Settings*>(settings), context);
        if (!value) {
            return LoadedValue{nullptr, AssetLoadFailure::Make(AssetLoadFailureCode::LoaderFailed,
                                                               "the loader failed",
                                                               Base::CauseDetail(value.error(), context.Id().Text()))};
        }
        return LoadedValue{std::make_shared<const typename L::Output>(std::move(*value)), std::nullopt};
    }

private:
    static std::vector<std::string> Formats() {
        std::vector<std::string> formats;
        for (const auto& format : L::kFormats) formats.emplace_back(format);
        return formats;
    }

    L loader_;
};

[[nodiscard]] LoaderRegistrationResult RegisterLoader(StoreState& state, std::shared_ptr<const ErasedLoader> loader);
[[nodiscard]] LoaderRegistrationResult PreregisterLoader(StoreState& state, std::type_index loaderType,
                                                         std::string_view typeName,
                                                         std::span<const std::string_view> formats);
[[nodiscard]] std::shared_ptr<const HandleBody> Request(StoreState& state, const AssetId& id,
                                                        std::string_view typeName);
[[nodiscard]] std::shared_ptr<const void> Read(const StoreState& state, const HandleBody* body,
                                               std::type_index type);

} // namespace Detail

// Only the registration functions (5.4, O-A2): what a Module that owns a type
// is given to register its loader, at any time.
class AssetLoaderRegistry final {
public:
    // Registers L for (L::kTypeName, f) for each f of L::kFormats (5.3 (B)).
    // Registering the same loader type again succeeds and changes nothing.
    // Refused, with nothing registered, when another loader has one of the
    // pairs (FormatTaken), another loader type preregistered one
    // (FormatPreregistered), or another C++ type declared the type name
    // (TypeNameTaken). The settings of the catalog entries of those pairs are
    // read once, here, without the store's lock; an entry whose settings cannot
    // be read is Failed(BadSettings) when requested, and the loader is still
    // registered. Entries waiting for this loader are queued.
    template <AssetLoader L>
    LoaderRegistrationResult Register(L loader = L{}) const {
        return Detail::RegisterLoader(*state_, std::make_shared<const Detail::LoaderModel<L>>(std::move(loader)));
    }

    // Says that L will come for (L::kTypeName, f), f in formats: requests of
    // those entries wait for it (WaitingForLoader) instead of failing with
    // NoLoader. When L registers, the waiters of formats L does not take become
    // NoLoader. Refused, with nothing recorded, when another loader is
    // registered for one of the pairs or another loader type preregistered one.
    // Preregistering again with the same loader type succeeds.
    template <AssetLoader L>
    LoaderRegistrationResult Preregister(std::span<const std::string_view> formats) const {
        return Detail::PreregisterLoader(*state_, std::type_index(typeid(L)), L::kTypeName, formats);
    }
    template <AssetLoader L>
    LoaderRegistrationResult Preregister(std::initializer_list<std::string_view> formats) const {
        return Preregister<L>(std::span<const std::string_view>(formats.begin(), formats.size()));
    }

    explicit AssetLoaderRegistry(Detail::StoreState& state) : state_(&state) {}

private:
    Detail::StoreState* state_;
};

// How Shutdown treats jobs not finished (5.8, X-6).
enum class AssetShutdown : std::uint8_t {
    // Wait until no job is waiting or in flight (the jobs of dependencies that
    // finishing jobs queue included), then stop. The executor must still be
    // running jobs.
    Drain = 1,
    // Stop at once; jobs not finished count as discarded.
    Discard,
};

class AssetStore final {
public:
    // entries: the catalog entries of the content, already checked against the
    // (A) rules of 5.3 (ids unique). executor: where jobs run, which must
    // outlive the store; null for none, and then every request that needs a
    // job is Failed(NoExecutor).
    AssetStore(std::span<const CatalogEntry> entries, AssetStoreOptions options, AssetExecutor* executor);
    // Shutdown(AssetShutdown::Discard).
    ~AssetStore();
    AssetStore(const AssetStore&) = delete;
    AssetStore& operator=(const AssetStore&) = delete;

    [[nodiscard]] AssetLoaderRegistry Loaders() const { return AssetLoaderRegistry(*state_); }

    // A request (5.5). Refused at the request level, without the lock and
    // without touching any info, when the id is in no catalog (UnknownId) or
    // T's type name is not the entry's type (TypeMismatch). Otherwise, under
    // the lock: no info makes a new life (Queued, WaitingForLoader,
    // Failed(NoLoader) or Failed(BadSettings)); an info whose core can be
    // upgraded shares it; an info whose core went gets a new core and skips
    // one drop. A Failed info is judged again first. Jobs are submitted after
    // the lock is released.
    template <class T>
        requires HasAssetTypeName<T>
    [[nodiscard]] Handle<T> Request(const AssetId& id) {
        return Handle<T>(Detail::Request(*state_, id, AssetTypeName<T>::value));
    }
    template <class T>
    [[nodiscard]] Handle<T> Request(const AssetRef<T>& reference) {
        return Request<T>(reference.Id());
    }

    // Maintenance (5.5, 5.6): takes the queue of drops until it is empty
    // (with what this round pushes). A drop of another life is stale; a drop
    // while the info skips drops uses one up; otherwise the info is removed
    // and the entry's life grows by one. Removed values are destroyed after
    // the lock is released.
    void Pump();

    // The value of the asset the Handle holds, under the read lock; null when
    // it is not loaded, the Handle has no core or the value is not a T.
    template <class T>
    [[nodiscard]] std::shared_ptr<const T> Read(const Handle<T>& handle) const {
        return std::static_pointer_cast<const T>(
            Detail::Read(*state_, handle.Body().get(), std::type_index(typeid(T))));
    }

    // Changes whenever an info does (created, a state, a value, an edge,
    // removed).
    [[nodiscard]] std::uint64_t Version() const;

    // Every alive info, copied under the read lock (5.6: an apply point's pull).
    [[nodiscard]] AssetSnapshot Snapshot() const;
    // The same, or nothing when the version is still lastVersion.
    [[nodiscard]] std::optional<AssetSnapshot> PullIfChanged(std::uint64_t lastVersion) const;

    [[nodiscard]] AssetStoreCounts Counts() const;
    // NotRequested for an id that is in no catalog.
    [[nodiscard]] AssetProbe Probe(const AssetId& id) const;

    // Stops (5.8, X-6): with Drain, first waits until no job is waiting or in
    // flight. Then no job is started or submitted, the queue of waiting jobs is
    // emptied, jobs not finished count as discarded, every info goes and
    // Handles dropped later push nothing. Waits for a job that is submitting
    // to return from Submit; does not wait for running jobs, which find the
    // store shut down and keep nothing. Only the first call acts. Not to be
    // called from a job.
    void Shutdown(AssetShutdown mode);

private:
    std::shared_ptr<Detail::StoreState> state_;
};

} // namespace GYO::Engine::Asset
