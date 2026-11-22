#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetLoadFailure.hpp"
#include "engine/asset/LoadContext.hpp"

// Every alive info of a store, copied under its read lock (declaration 5.6:
// what an apply point pulls into an AssetView; 5.7, 5.17). Immutable; values
// are shared, read-only pointers (Design Invariant AS-10). The dependency
// status ("with dependencies") is derived from the edges each time it is asked,
// so a dependency that failed and was reloaded later counts as loaded again.

namespace GYO::Engine::Asset {

enum class EdgeStatus : std::uint8_t {
    // The edge holds the referenced asset alive.
    Held = 1,
    // No loader and no preregistration for the referenced (type, format) when
    // the edge was added: not loaded, not a failure. Stays so for the parent's
    // life.
    Skipped,
    // The reference failed at the edge (failure has the code). Holds nothing.
    Failed,
    // In a strongly connected component (CyclicReference). Holds nothing; stays
    // so for the parent's life.
    Cyclic,
};

[[nodiscard]] constexpr const char* ToString(const EdgeStatus status) noexcept {
    switch (status) {
    case EdgeStatus::Held: return "Held";
    case EdgeStatus::Skipped: return "Skipped";
    case EdgeStatus::Failed: return "Failed";
    case EdgeStatus::Cyclic: return "Cyclic";
    }
    return "Unknown";
}

// One reference an asset's loader declared, in declaration order.
struct AssetEdge final {
    DeclaredReference reference;
    // The referenced id; empty when a path mapped to none.
    AssetId target;
    EdgeStatus status = EdgeStatus::Failed;
    // With Failed and Cyclic.
    std::optional<AssetLoadFailureCode> failure;

    [[nodiscard]] friend bool operator==(const AssetEdge&, const AssetEdge&) = default;
};

struct AssetRecord final {
    AssetId id;
    std::string type;
    // The life of this info, and the number of jobs queued for the id across
    // all its lives (the last one's number).
    std::uint64_t life = 0;
    std::uint64_t attempts = 0;
    AssetState state = AssetState::NotRequested;
    // With Loaded: the value and its C++ type.
    std::shared_ptr<const void> value;
    std::optional<std::type_index> valueType;
    // With Failed.
    std::optional<AssetLoadFailure> failure;
    // SHA-256 of the bytes read, when the store computes it and the file was
    // read; absent otherwise ("not computed").
    std::optional<std::string> sourceDigest;
    // With Loaded.
    std::vector<AssetEdge> edges;

    // The value as a T, or null when not loaded or of another C++ type.
    template <class T>
    [[nodiscard]] std::shared_ptr<const T> Value() const {
        if (!value || !valueType || *valueType != std::type_index(typeid(T))) return nullptr;
        return std::static_pointer_cast<const T>(value);
    }
};

enum class DependencyState : std::uint8_t {
    // The asset or one of its recursive dependencies has not finished.
    Incomplete,
    // The asset and every recursive dependency are loaded or skipped.
    Complete,
    // Everything finished and the asset or a recursive dependency failed.
    Failed,
};

[[nodiscard]] constexpr const char* ToString(const DependencyState state) noexcept {
    switch (state) {
    case DependencyState::Incomplete: return "Incomplete";
    case DependencyState::Complete: return "Complete";
    case DependencyState::Failed: return "Failed";
    }
    return "Unknown";
}

struct DependencyStatus final {
    DependencyState state = DependencyState::Incomplete;
    // Failures among the recursive dependencies (not the asset's own failure),
    // each with the parent whose edge it is.
    std::vector<DependencyFailure> failures;
    // Referenced ids skipped (no loader, no preregistration).
    std::vector<AssetId> skipped;
    // The asset and recursive dependencies that wait for a loader.
    std::vector<AssetId> waiting;
    // The asset and every recursive dependency reached through held edges, in
    // the order first reached.
    std::vector<AssetId> ids;
};

// A failed alive info, for a log to tell new failures from ones it has seen
// (5.7): the attempt number grows with every queued job, across lives.
struct FailureRecord final {
    AssetId id;
    std::uint64_t attempt = 0;
    AssetLoadFailureCode code;

    [[nodiscard]] friend bool operator==(const FailureRecord&, const FailureRecord&) = default;
};

class AssetSnapshot final {
public:
    AssetSnapshot() = default;
    AssetSnapshot(std::uint64_t version, std::vector<AssetRecord> records);

    // The store's version when this was copied; it changes whenever an info does.
    [[nodiscard]] std::uint64_t Version() const noexcept { return version_; }
    [[nodiscard]] std::span<const AssetRecord> Records() const noexcept { return records_; }
    [[nodiscard]] const AssetRecord* Find(const AssetId& id) const;
    // NotRequested when the id has no alive info.
    [[nodiscard]] AssetState State(const AssetId& id) const;
    // The status of the asset with all its recursive dependencies (5.17).
    [[nodiscard]] DependencyStatus WithDependencies(const AssetId& id) const;
    [[nodiscard]] std::vector<FailureRecord> Failures() const;
    [[nodiscard]] std::vector<AssetId> WaitingForLoader() const;

private:
    std::uint64_t version_ = 0;
    std::vector<AssetRecord> records_;
    std::unordered_map<AssetId, std::size_t> byId_;
};

} // namespace GYO::Engine::Asset
