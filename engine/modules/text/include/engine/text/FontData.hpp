#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "engine/asset/AssetTypeName.hpp"
#include "engine/text/TextFailure.hpp"

// The bytes of one font file (one face or a collection), shared and immutable:
// copies share the bytes, any thread may read and destroy one (cross-component
// rule CCR-9). FromBytes checks the whole "font" Data Contract, so a FontData
// that exists can be added to a TextContext; only memory can then fail.
namespace GYO::Engine::Text {

namespace Detail {
struct FontBytes;
} // namespace Detail

class FontData final {
public:
    // Takes the bytes and checks every face with HarfBuzz and FreeType
    // (Data Contract "font", docs/architecture/target/v2/text.zh-Hant.md).
    [[nodiscard]] static TextResult<FontData> FromBytes(std::vector<std::byte>&& bytes);

    // 1 for a single font, the number of faces for a collection.
    [[nodiscard]] std::uint32_t FaceCount() const noexcept;
    [[nodiscard]] std::span<const std::byte> Bytes() const noexcept;

private:
    explicit FontData(std::shared_ptr<const Detail::FontBytes> data) noexcept : data_(std::move(data)) {}

    std::shared_ptr<const Detail::FontBytes> data_;
};

} // namespace GYO::Engine::Text

template <>
struct GYO::Engine::Asset::AssetTypeName<GYO::Engine::Text::FontData> {
    static constexpr std::string_view value = "font";
};
