#pragma once

#include <utility>

#include "engine/asset/AssetId.hpp"

// A reference to another asset that does not hold it (declaration 5.17): only
// the id and the type it loads as. A loader puts it into the value it makes;
// what keeps the referenced asset alive is the store's dependency edge, not the
// reference. An author who wants to hold the asset requests a Handle with it.
//
// A reference declared by path (LoadContext::ReferencePath) has an empty id
// until the store maps the path to an id through the catalog.

namespace GYO::Engine::Asset {

template <class U>
class AssetRef final {
public:
    AssetRef() = default;
    explicit AssetRef(AssetId id) : id_(std::move(id)) {}

    [[nodiscard]] const AssetId& Id() const noexcept { return id_; }

    [[nodiscard]] friend bool operator==(const AssetRef&, const AssetRef&) = default;

private:
    AssetId id_;
};

} // namespace GYO::Engine::Asset
