#pragma once

#include <concepts>
#include <string_view>

// The type name of an asset type: the catalog's "type" for content that loads
// as T (declaration 5.4, "the type name is bound to the C++ type"). The Module
// that owns T specializes AssetTypeName in its library, whether or not a loader
// for T is registered:
//
//     template <>
//     struct GYO::Engine::Asset::AssetTypeName<Text> {
//         static constexpr std::string_view value = "text";
//     };
//
// A loader declares the same name, checked at compile time (AssetLoader).

namespace GYO::Engine::Asset {

template <class T>
struct AssetTypeName;

template <class T>
concept HasAssetTypeName = requires {
    { AssetTypeName<T>::value } -> std::convertible_to<std::string_view>;
};

} // namespace GYO::Engine::Asset
