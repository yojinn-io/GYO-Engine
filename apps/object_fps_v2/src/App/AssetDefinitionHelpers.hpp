#pragma once

#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetRequest.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace fps::asset_definition_detail {

inline Engine::Asset::AssetId ReadAssetId(const nlohmann::json& json) {
    const auto name = json.get<std::string>();
    if (name.empty()) throw std::runtime_error("asset ID must not be empty");
    return Engine::Asset::AssetId::FromString(name);
}

inline void RequireVersionOne(const nlohmann::json& config) {
    const auto& version = config.at("version");
    if (!version.is_number_integer() || version != 1) {
        throw std::runtime_error("unsupported definition version; expected integer 1");
    }
}

template<class T>
std::shared_ptr<const T> LoadShared(
    Engine::Asset::AssetManager& assets,
    const Engine::Asset::AssetId& id,
    Engine::Asset::AssetType type) {
    const auto loaded = assets.Load(id, Engine::Asset::AssetRequest::WithTypeHint(type));
    if (!loaded) {
        throw std::runtime_error("asset '" + id.debugName + "': " +
                                 loaded.error().message + " " + loaded.error().detail);
    }
    auto payload = assets.GetSharedConst<T>(loaded.value());
    assets.Release(loaded.value());
    if (!payload) throw std::runtime_error("asset '" + id.debugName + "' has the wrong payload");
    return payload;
}

} // namespace fps::asset_definition_detail
