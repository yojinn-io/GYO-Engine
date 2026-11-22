#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

namespace GYO::Engine::Asset {

// Why content.json or a catalog was refused (the (A) rules of declaration 5.3),
// or could not be read or written. E2: only this content is refused. Zero is
// not a valid code; the numeric values are not a data contract.
enum class CatalogFailureCode : std::uint8_t {
    // A file the content names (content.json, a listed catalog) cannot be read. Rule 1.
    Unreadable = 1,
    // The text is not JSON. Rules 1, 2.
    NotJson,
    // A required member is missing or of the wrong kind: the top level is not an
    // object, "catalogs" or "assets" is not an array, an entry is not an object,
    // "id", "type" or "path" is not a non-empty string. Rules 1, 2.
    BadShape,
    // "version" is missing or not an integer. Rules 1, 2.
    BadVersion,
    // "version" is an integer this version does not know. Rules 1, 2.
    UnknownVersion,
    // A "catalogs" entry is not a relative content path, or is content.json. Rule 1.
    BadCatalogPath,
    // content.json lists one catalog twice. Rule 1.
    DuplicateCatalog,
    // An id appears twice in the catalogs of one content. Rule 3.
    DuplicateId,
    // An id is not ASCII [a-z0-9._-] of 1 to 128 characters. Rule 4.
    BadId,
    // A "path" is not a relative content path, or is content.json. Rule 5.
    BadPath,
    // A "path" has no extension and the entry has no "format". Rule 6.
    NoFormat,
    // A "format" is not [a-z0-9]+. Rule 7.
    BadFormat,
    // A "settings" is not a JSON object. Rule 7.
    SettingsNotObject,
    // Writing failed: a string is not UTF-8, a preserved member is not JSON, or
    // memory ran out.
    WriteFailed,
};

[[nodiscard]] constexpr const char* ToString(const CatalogFailureCode code) noexcept {
    switch (code) {
    case CatalogFailureCode::Unreadable: return "Unreadable";
    case CatalogFailureCode::NotJson: return "NotJson";
    case CatalogFailureCode::BadShape: return "BadShape";
    case CatalogFailureCode::BadVersion: return "BadVersion";
    case CatalogFailureCode::UnknownVersion: return "UnknownVersion";
    case CatalogFailureCode::BadCatalogPath: return "BadCatalogPath";
    case CatalogFailureCode::DuplicateCatalog: return "DuplicateCatalog";
    case CatalogFailureCode::DuplicateId: return "DuplicateId";
    case CatalogFailureCode::BadId: return "BadId";
    case CatalogFailureCode::BadPath: return "BadPath";
    case CatalogFailureCode::NoFormat: return "NoFormat";
    case CatalogFailureCode::BadFormat: return "BadFormat";
    case CatalogFailureCode::SettingsNotObject: return "SettingsNotObject";
    case CatalogFailureCode::WriteFailed: return "WriteFailed";
    }
    return "Unknown";
}

using CatalogFailure = Base::Error<CatalogFailureCode>;
static_assert(Base::CodedError<CatalogFailure>);

template <class T>
using CatalogResult = Base::Result<T, CatalogFailure>;

} // namespace GYO::Engine::Asset
