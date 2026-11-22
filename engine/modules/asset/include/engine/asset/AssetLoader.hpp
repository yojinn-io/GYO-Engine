#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "engine/asset/AssetTypeName.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

// The loader interface (declaration 5.16; Design Invariants AS-2, AS-3, AS-9).
// A loader turns the bytes of one catalog entry into a value of its Output
// type. The Module that owns the type writes the loader; asset knows no content
// format. A loader declares:
//
//     struct TextureLoader {
//         using Output = Texture;              // AssetTypeName<Texture>::value is "texture"
//         using Settings = TextureSettings;
//         using Code = TextureLoaderCode;      // an enum starting at 1, with ToString
//         static constexpr std::string_view kTypeName = "texture";   // must equal Output's
//         static constexpr std::array<std::string_view, 2> kFormats{"png", "jpg"};
//         static constexpr std::size_t kDefaultMaxBytes = 64u * 1024u * 1024u;
//         Base::Result<Settings, LoaderFailure<Code>> ParseSettings(std::optional<std::string_view> json) const;
//         Base::Result<Texture, LoaderFailure<Code>> Load(std::span<const std::byte> bytes,
//                                                         const Settings& settings, LoadContext& context) const;
//     };
//
// ParseSettings reads the entry's "settings" (a JSON object as compact text,
// or nothing when the entry has none). The store calls it once per entry when
// the loader is registered. Load has no World, no clock and no global mutable
// state: the same bytes and settings give the same value and the same declared
// references. A loader is chosen by (type, format); kFormats are the formats it
// takes. The Output must be safe to read from many threads at once and to
// destroy on any thread (AS-10 (c)).

namespace GYO::Engine::Asset {

// A loader's failure: E2, this entry did not load. Code is the loader's enum.
template <class Code>
using LoaderFailure = Base::Error<Code>;

template <class L>
concept AssetLoader = requires(const L& loader, std::span<const std::byte> bytes,
                               const typename L::Settings& settings, LoadContext& context,
                               std::optional<std::string_view> settingsJson) {
    typename L::Output;
    typename L::Settings;
    typename L::Code;
    requires std::is_enum_v<typename L::Code>;
    requires Base::CodedError<LoaderFailure<typename L::Code>>;
    requires HasAssetTypeName<typename L::Output>;
    { L::kTypeName } -> std::convertible_to<std::string_view>;
    // The type name the loader declares is the one bound to its Output type.
    requires std::string_view(L::kTypeName) == std::string_view(AssetTypeName<typename L::Output>::value);
    { std::size(L::kFormats) } -> std::convertible_to<std::size_t>;
    requires std::size(L::kFormats) > 0;
    { *std::begin(L::kFormats) } -> std::convertible_to<std::string_view>;
    { L::kDefaultMaxBytes } -> std::convertible_to<std::size_t>;
    { loader.ParseSettings(settingsJson) }
        -> std::same_as<Base::Result<typename L::Settings, LoaderFailure<typename L::Code>>>;
    { loader.Load(bytes, settings, context) }
        -> std::same_as<Base::Result<typename L::Output, LoaderFailure<typename L::Code>>>;
};

} // namespace GYO::Engine::Asset
