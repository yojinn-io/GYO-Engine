#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetLoadFailure.hpp"
#include "engine/asset/AssetTypeName.hpp"

// A Handle holds an asset alive (declaration 5.5, following Bevy's
// Arc<StrongHandle>). Copies share one strong core: a copy is an atomic
// increment. When the last copy goes, the core pushes (id, life) to the store's
// queue of drops, without taking the store's lock; the store's maintenance
// (Pump) decides whether the asset goes. One life of an asset can have several
// cores one after another (a request after the last core went and before
// maintenance makes a new one: a revival).
//
// A request refused at the request level (UnknownId, TypeMismatch) gives a
// Handle with that failure and no core: dropping it pushes nothing and it holds
// nothing.
//
// Two Handles are equal when their ids are. A Handle that outlives its store
// stays safe: the push goes nowhere.
//
// A Handle tells nothing of the store's current state (Design Invariant AS-11):
// the life it was given is only for a view to tell that it is older than the
// Handle, and like the body it is reached through Detail::HandleAccess (the
// store, world_asset, tests), not by authors.

namespace GYO::Engine::Asset {

namespace Detail {

class DropQueue;

inline constexpr std::size_t kNoEntry = static_cast<std::size_t>(-1);

// The shared body of a Handle: the strong core, or a request-level failure.
class HandleBody final {
public:
    HandleBody(AssetId id, std::size_t entry, std::uint64_t life, std::shared_ptr<DropQueue> drops,
               std::optional<AssetLoadFailure> failure);
    ~HandleBody();
    HandleBody(const HandleBody&) = delete;
    HandleBody& operator=(const HandleBody&) = delete;

    AssetId id;
    // The store's index of the catalog entry; kNoEntry for UnknownId.
    std::size_t entry;
    // The life this core holds; 0 when there is no core.
    std::uint64_t life;
    // Null when there is no core.
    std::shared_ptr<DropQueue> drops;
    std::optional<AssetLoadFailure> failure;
};

// The store's and world_asset's access to a Handle's body and life (not for
// authors: a System reads states through AssetView, AS-11).
struct HandleAccess;

} // namespace Detail

template <class T>
    requires HasAssetTypeName<T>
class Handle final {
public:
    // Names nothing.
    Handle() = default;

    [[nodiscard]] const AssetId& Id() const noexcept { return body_ ? body_->id : EmptyId(); }

    // The request-level failure (UnknownId, TypeMismatch), or null.
    [[nodiscard]] const AssetLoadFailure* RequestFailure() const noexcept {
        return body_ && body_->failure ? &*body_->failure : nullptr;
    }

    [[nodiscard]] friend bool operator==(const Handle& left, const Handle& right) noexcept {
        return left.Id() == right.Id();
    }

    explicit Handle(std::shared_ptr<const Detail::HandleBody> body) : body_(std::move(body)) {}

private:
    friend struct Detail::HandleAccess;

    // The body (the strong core or the request-level failure), through Detail::HandleAccess only.
    [[nodiscard]] const std::shared_ptr<const Detail::HandleBody>& Body() const noexcept { return body_; }
    // The life of the asset this Handle holds; 0 without a core. Through Detail::HandleAccess only.
    [[nodiscard]] std::uint64_t Life() const noexcept { return body_ ? body_->life : 0; }

    static const AssetId& EmptyId() noexcept {
        static const AssetId empty;
        return empty;
    }

    std::shared_ptr<const Detail::HandleBody> body_;
};

namespace Detail {

struct HandleAccess final {
    template <class T>
    [[nodiscard]] static const std::shared_ptr<const HandleBody>& Body(const Handle<T>& handle) noexcept {
        return handle.Body();
    }
    template <class T>
    [[nodiscard]] static std::uint64_t Life(const Handle<T>& handle) noexcept {
        return handle.Life();
    }
};

} // namespace Detail

// The hash function of a Handle (declaration 5.9.1): its id string only, so a Handle hashes the same in every
// process and whatever order the catalog lists its entries in (Design Invariant AS-6).
template <class Sink, class T>
    requires Detail::HashesText<Sink>
void HashValue(Sink& sink, const Handle<T>& handle) {
    HashValue(sink, handle.Id());
}

} // namespace GYO::Engine::Asset

template <class T>
struct std::hash<GYO::Engine::Asset::Handle<T>> {
    [[nodiscard]] std::size_t operator()(const GYO::Engine::Asset::Handle<T>& handle) const noexcept {
        return std::hash<GYO::Engine::Asset::AssetId>{}(handle.Id());
    }
};
