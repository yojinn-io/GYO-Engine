#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "engine/asset/AssetLoader.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/asset/Text.hpp"

// The text loader (declaration 5.16): type "text", formats "txt" and "json".
// It checks that the bytes are UTF-8 and hands them over unchanged; it does not
// read JSON (Design Invariant AS-2). It has no settings and declares no
// references.

namespace GYO::Engine::Asset {

enum class TextLoaderCode : std::uint8_t {
    // The bytes are not well-formed UTF-8.
    NotUtf8 = 1,
};

[[nodiscard]] constexpr const char* ToString(const TextLoaderCode code) noexcept {
    switch (code) {
    case TextLoaderCode::NotUtf8: return "NotUtf8";
    }
    return "Unknown";
}

struct TextSettings final {
    [[nodiscard]] friend bool operator==(const TextSettings&, const TextSettings&) = default;
};

class TextLoader final {
public:
    using Output = Text;
    using Settings = TextSettings;
    using Code = TextLoaderCode;

    static constexpr std::string_view kTypeName = "text";
    static constexpr std::array<std::string_view, 2> kFormats{"txt", "json"};
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{16} * 1024u * 1024u;

    // Any settings object is accepted and ignored.
    [[nodiscard]] Base::Result<Settings, LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;

    [[nodiscard]] Base::Result<Text, LoaderFailure<Code>> Load(std::span<const std::byte> bytes,
                                                              const Settings& settings,
                                                              LoadContext& context) const;
};

static_assert(AssetLoader<TextLoader>);

} // namespace GYO::Engine::Asset
