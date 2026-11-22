#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "engine/asset/AssetTypeName.hpp"

// Text content: UTF-8 bytes exactly as the file holds them (declaration 5.16).
// Its type name is "text". Immutable once made; read it from any thread.

namespace GYO::Engine::Asset {

class Text final {
public:
    explicit Text(std::string bytes) : bytes_(std::move(bytes)) {}

    [[nodiscard]] const std::string& Bytes() const noexcept { return bytes_; }

    [[nodiscard]] friend bool operator==(const Text&, const Text&) = default;

private:
    std::string bytes_;
};

template <>
struct AssetTypeName<Text> {
    static constexpr std::string_view value = "text";
};

} // namespace GYO::Engine::Asset
