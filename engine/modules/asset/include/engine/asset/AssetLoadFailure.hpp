#pragma once

#include <cstdint>
#include <string>

#include "engine/asset/AssetId.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

// What the store says about an asset (declaration 5.7, 5.8; Design Invariant
// AS-8). Every failure here is E2: it is reported, never logged or retried by
// the store, and the World goes on.

namespace GYO::Engine::Asset {

// The state of an id in the store (5.7). NotRequested is what the store's own
// reads give for an id that has no info (it is not alive).
enum class AssetState : std::uint8_t {
    NotRequested,
    // No loader for the entry's (type, format) yet, but one was preregistered.
    WaitingForLoader,
    // A job is queued (behind the in-flight limit or in the executor).
    Queued,
    // The job has started: the file is being read or decoded.
    Loading,
    Loaded,
    Failed,
};

[[nodiscard]] constexpr const char* ToString(const AssetState state) noexcept {
    switch (state) {
    case AssetState::NotRequested: return "NotRequested";
    case AssetState::WaitingForLoader: return "WaitingForLoader";
    case AssetState::Queued: return "Queued";
    case AssetState::Loading: return "Loading";
    case AssetState::Loaded: return "Loaded";
    case AssetState::Failed: return "Failed";
    }
    return "Unknown";
}

// Zero is not a valid code; the numeric values are not a data contract.
enum class AssetLoadFailureCode : std::uint8_t {
    // Request level (the Handle carries it, no info is touched) and edge level:
    // the id is in no catalog.
    UnknownId = 1,
    // Request level and edge level: the requested type's name is not the
    // entry's "type".
    TypeMismatch,
    // Id level: no loader for the entry's (type, format) and none preregistered.
    NoLoader,
    // Id level: the registered loader could not read the entry's "settings".
    BadSettings,
    // Id level: a job is needed and the store was given no executor. The
    // store does not run the job on the caller's thread instead.
    NoExecutor,
    // Id level: reading the file (FileReadFailureCode).
    NotFound,
    ReadFailed,
    TooLarge,
    // Id level: the loader returned a failure; the detail has its code.
    LoaderFailed,
    // Id level: the loader threw; the detail has what() when there is one.
    LoaderThrew,
    // Edge level (ReferenceFailureCode).
    BadReference,
    UnlistedReference,
    AmbiguousReference,
    // Edge level: the edge is in a strongly connected component of the
    // dependency graph (a self reference included).
    CyclicReference,
};

[[nodiscard]] constexpr const char* ToString(const AssetLoadFailureCode code) noexcept {
    switch (code) {
    case AssetLoadFailureCode::UnknownId: return "UnknownId";
    case AssetLoadFailureCode::TypeMismatch: return "TypeMismatch";
    case AssetLoadFailureCode::NoLoader: return "NoLoader";
    case AssetLoadFailureCode::BadSettings: return "BadSettings";
    case AssetLoadFailureCode::NoExecutor: return "NoExecutor";
    case AssetLoadFailureCode::NotFound: return "NotFound";
    case AssetLoadFailureCode::ReadFailed: return "ReadFailed";
    case AssetLoadFailureCode::TooLarge: return "TooLarge";
    case AssetLoadFailureCode::LoaderFailed: return "LoaderFailed";
    case AssetLoadFailureCode::LoaderThrew: return "LoaderThrew";
    case AssetLoadFailureCode::BadReference: return "BadReference";
    case AssetLoadFailureCode::UnlistedReference: return "UnlistedReference";
    case AssetLoadFailureCode::AmbiguousReference: return "AmbiguousReference";
    case AssetLoadFailureCode::CyclicReference: return "CyclicReference";
    }
    return "Unknown";
}

using AssetLoadFailure = Base::Error<AssetLoadFailureCode>;
static_assert(Base::CodedError<AssetLoadFailure>);

// A failure among an asset's recursive dependencies (5.17): parent's reference
// failed at the edge (BadReference, UnlistedReference, AmbiguousReference,
// UnknownId, TypeMismatch, CyclicReference), or the referenced asset itself
// failed (code is then that asset's failure code). reference is the id
// referenced, empty when a path did not map to one (writtenPath has it then).
struct DependencyFailure final {
    AssetId parent;
    AssetId reference;
    std::string writtenPath;
    AssetLoadFailureCode code;

    [[nodiscard]] friend bool operator==(const DependencyFailure&, const DependencyFailure&) = default;
};

// Why registering or preregistering a loader was refused (5.3 (B) 1, 4, 5).
// The loader is not registered (or preregistered); nothing else changes.
enum class LoaderRegistrationFailureCode : std::uint8_t {
    // A different loader is registered for one of the (type, format) pairs.
    FormatTaken = 1,
    // A different loader type is preregistered for one of the pairs.
    FormatPreregistered,
    // A different C++ type already declared this type name.
    TypeNameTaken,
};

[[nodiscard]] constexpr const char* ToString(const LoaderRegistrationFailureCode code) noexcept {
    switch (code) {
    case LoaderRegistrationFailureCode::FormatTaken: return "FormatTaken";
    case LoaderRegistrationFailureCode::FormatPreregistered: return "FormatPreregistered";
    case LoaderRegistrationFailureCode::TypeNameTaken: return "TypeNameTaken";
    }
    return "Unknown";
}

using LoaderRegistrationFailure = Base::Error<LoaderRegistrationFailureCode>;
static_assert(Base::CodedError<LoaderRegistrationFailure>);

using LoaderRegistrationResult = Base::Result<void, LoaderRegistrationFailure>;

} // namespace GYO::Engine::Asset
