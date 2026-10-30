#pragma once

#include "RetroFPS/Pvp/Arena.hpp"
#include "RetroFPS/Pvp/Movement.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace fps::pvp {

// The Match's movement rules data (protocol-v8 section 3), a product Data
// Contract that can be written by hand: version 1 and the four stance values,
// every key required and no other key. The values must satisfy
// ValidStanceRules for the hosted arena; anything else is an error, never a
// fallback to DefaultStanceRules. Only the Match reads it; Clients take the
// values from Welcome.
[[nodiscard]] std::optional<StanceRules> LoadStanceRules(
    const std::filesystem::path& path, const Arena& arena, std::string& error);

} // namespace fps::pvp
