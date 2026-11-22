#pragma once

#include <compare>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

// The id of an asset: the string key of a catalog entry, and the persistent key
// that authors, content, saves and decision data record (declaration
// docs/architecture/target/v2/asset-declaration.zh-Hant.md, 5.2). Two ids are
// equal when their strings are equal; there is no hash-only equality.
//
// An AssetId holds any string; IsValidAssetId says whether it is in the form a
// catalog accepts (rule (A) 4 of 5.3). An id that is not in any catalog is
// reported where it is looked up, not here.

namespace GYO::Engine::Asset {

inline constexpr std::size_t kMaxAssetIdLength = 128;

// ASCII [a-z0-9._-], 1 to kMaxAssetIdLength characters.
[[nodiscard]] bool IsValidAssetId(std::string_view text) noexcept;

class AssetId final {
public:
    AssetId() = default;
    explicit AssetId(std::string text) : text_(std::move(text)) {}
    explicit AssetId(std::string_view text) : text_(text) {}
    explicit AssetId(const char* text) : text_(text) {}

    [[nodiscard]] const std::string& Text() const noexcept { return text_; }
    [[nodiscard]] bool Empty() const noexcept { return text_.empty(); }

    [[nodiscard]] friend bool operator==(const AssetId&, const AssetId&) = default;
    [[nodiscard]] friend std::strong_ordering operator<=>(const AssetId& left, const AssetId& right) {
        return left.text_.compare(right.text_) <=> 0;
    }

private:
    std::string text_;
};

} // namespace GYO::Engine::Asset

template <>
struct std::hash<GYO::Engine::Asset::AssetId> {
    [[nodiscard]] std::size_t operator()(const GYO::Engine::Asset::AssetId& id) const noexcept {
        return std::hash<std::string>{}(id.Text());
    }
};
