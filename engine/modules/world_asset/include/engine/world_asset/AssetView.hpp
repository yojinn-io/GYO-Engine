#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetLoadFailure.hpp"
#include "engine/asset/AssetRef.hpp"
#include "engine/asset/AssetSnapshot.hpp"
#include "engine/asset/Handle.hpp"
#include "engine/kernel/Hash.hpp"

// What a World sees of the store (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md, 5.4, 5.6,
// 5.7, 5.17; Design Invariants AS-1, AS-4, AS-10 (a), AS-11): a Resource world_asset puts into every World it is
// built into. It holds every alive item of the store, as Bevy's Assets<A> holds them all, whoever requested them:
// a Handle carried over, extracted or held by a dependency reads the same way.
//
// It changes only at the World's apply point (world_asset's System, Main/First by default) and in world_asset's
// prepare function, so every System of one frame sees the same one. A value read here is shared and read-only;
// a pointer to it is valid only while the System that read it runs.
//
// An author does not write it: it has no mutating member, cannot be copied and cannot be made (its constructor
// is world_asset's). It can be moved, because the Kernel moves a Resource into place. Replacing or removing it
// whole is for review to catch; the apply System builds a new one when it is gone, and the version kept inside
// does not match a replaced one, so the next apply point pulls again.

namespace GYO::Engine::WorldAsset {

namespace Detail {
struct AssetAccess;
} // namespace Detail

// A Handle of any type, for the readiness of a group (Ready, Unfinished). It holds what the Handle holds.
class UntypedHandle final {
public:
    template <class T>
    UntypedHandle(const Asset::Handle<T>& handle) : body_(Asset::Detail::HandleAccess::Body(handle)) {} // NOLINT: converts implicitly

    [[nodiscard]] const Asset::AssetId& Id() const noexcept;

private:
    friend class AssetView;
    std::shared_ptr<const Asset::Detail::HandleBody> body_;
};

// One failure of a group (5.7): a Handle's own failure (request level or id level; parent empty), or a failure
// among its recursive dependencies (parent: the asset whose reference failed or whose dependency failed).
struct AssetReadyFailure final {
    // The Handle of the group the failure is under.
    Asset::AssetId handle;
    Asset::AssetId parent;
    // The asset that failed or was referenced; empty when a path mapped to no id (writtenPath has it).
    Asset::AssetId id;
    std::string writtenPath;
    Asset::AssetLoadFailureCode code{};

    [[nodiscard]] friend bool operator==(const AssetReadyFailure&, const AssetReadyFailure&) = default;
};

// The readiness of a group of Handles (5.7, decision 9). What to wait for, a loading screen or when to start
// ticking is the author's: this only tells.
struct AssetReadiness final {
    // Every Handle finished: Loaded or failed. WaitingForLoader and Queued are not finished.
    bool allFinished = true;
    // Every Handle Loaded, and with its dependencies Complete (5.17).
    bool complete = true;
    // Every Handle finished with its dependencies (failures included).
    bool dependenciesFinished = true;
    // Handles Loaded, Handles failed.
    std::size_t loaded = 0;
    std::size_t failed = 0;
    // The Handles' own failures and their dependencies' failures, in group order.
    std::vector<AssetReadyFailure> failures;
    // Dependencies skipped (no loader and no preregistration for their (type, format)).
    std::vector<Asset::AssetId> skipped;
    // The Handles and dependencies waiting for a loader.
    std::vector<Asset::AssetId> waitingForLoader;
    // Every id of the group with its dependencies, in the order first reached.
    std::vector<Asset::AssetId> ids;
};

class AssetView final {
public:
    AssetView(AssetView&&) = default;
    AssetView& operator=(AssetView&&) = default;
    AssetView(const AssetView&) = delete;
    AssetView& operator=(const AssetView&) = delete;
    ~AssetView() = default;

    // The state this frame sees (5.7): a request-level failure is Failed; a Handle with a core whose (id, life)
    // this view does not have is Queued (requested this frame, a new life against an older view, or a Handle
    // carried or extracted from a World that pulled later); a Handle that names nothing is NotRequested.
    template <class T>
    [[nodiscard]] Asset::AssetState State(const Asset::Handle<T>& handle) const {
        return StateOf(Asset::Detail::HandleAccess::Body(handle).get());
    }
    // The failure behind Failed: the Handle's request-level failure, or the item's id-level one; otherwise null.
    template <class T>
    [[nodiscard]] const Asset::AssetLoadFailure* Failure(const Asset::Handle<T>& handle) const {
        return FailureOf(Asset::Detail::HandleAccess::Body(handle).get());
    }
    // The value when the Handle's item is Loaded in this view and its value is a T; otherwise null.
    template <class T>
    [[nodiscard]] const T* Get(const Asset::Handle<T>& handle) const {
        const void* value = ValueOf(Matching(Asset::Detail::HandleAccess::Body(handle).get()), typeid(T));
        return static_cast<const T*>(value);
    }
    // The value of a referenced asset (5.17): always there in the view in which its parent is Complete with its
    // dependencies (unless the edge is cyclic or skipped); otherwise it may be null.
    template <class U>
    [[nodiscard]] const U* Get(const Asset::AssetRef<U>& reference) const {
        const void* value = ValueOf(snapshot_.Find(reference.Id()), typeid(U));
        return static_cast<const U*>(value);
    }
    // SHA-256 of the file's bytes as read (engine/base's Sha256 text), when the store computed it and the Handle's item is
    // in this view with a file read; null otherwise ("not computed", 5.5).
    template <class T>
    [[nodiscard]] const std::string* SourceDigest(const Asset::Handle<T>& handle) const {
        return DigestOf(Matching(Asset::Detail::HandleAccess::Body(handle).get()));
    }

    [[nodiscard]] AssetReadiness Ready(std::span<const UntypedHandle> handles) const;
    [[nodiscard]] AssetReadiness Ready(std::initializer_list<UntypedHandle> handles) const {
        return Ready(std::span<const UntypedHandle>(handles.begin(), handles.size()));
    }
    // How many of the Handles have not finished (Loaded or failed).
    [[nodiscard]] std::size_t Unfinished(std::span<const UntypedHandle> handles) const;
    [[nodiscard]] std::size_t Unfinished(std::initializer_list<UntypedHandle> handles) const {
        return Unfinished(std::span<const UntypedHandle>(handles.begin(), handles.size()));
    }
    // Every failed alive item, with its attempt number (5.7): a log keeps the (id, attempt, code) it has seen to
    // tell a new failure from one it reported.
    [[nodiscard]] std::vector<Asset::FailureRecord> Failures() const { return snapshot_.Failures(); }

private:
    friend struct Detail::AssetAccess;
    explicit AssetView(Asset::AssetSnapshot snapshot) : snapshot_(std::move(snapshot)) {}

    // The record of the Handle's (id, life), or null.
    [[nodiscard]] const Asset::AssetRecord* Matching(const Asset::Detail::HandleBody* body) const;
    [[nodiscard]] Asset::AssetState StateOf(const Asset::Detail::HandleBody* body) const;
    [[nodiscard]] const Asset::AssetLoadFailure* FailureOf(const Asset::Detail::HandleBody* body) const;
    [[nodiscard]] static const void* ValueOf(const Asset::AssetRecord* record, const std::type_info& type);
    [[nodiscard]] static const std::string* DigestOf(const Asset::AssetRecord* record);

    Asset::AssetSnapshot snapshot_;
};

// Not hashed (declaration 5.9.1): it changes with the order jobs finish in. It is told by polling, not by the
// Kernel's messages (since summary version 4 a message's sequence number is not in World::Summary either;
// v8-arch-gyo2-kernel-summary-messages).
inline void HashValue(Kernel::Hasher&, const AssetView&) {}

} // namespace GYO::Engine::WorldAsset
