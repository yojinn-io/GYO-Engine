#pragma once

#include "RetroFPS/World/GridMap.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace fps {

struct MapLoadResult {
    std::optional<GridMap> map;
    std::string error;

    [[nodiscard]] bool Succeeded() const noexcept { return map.has_value(); }
    [[nodiscard]] explicit operator bool() const noexcept { return Succeeded(); }
};

class GridMapLoader final {
public:
    [[nodiscard]] static MapLoadResult Parse(std::string_view text);
};

} // namespace fps
