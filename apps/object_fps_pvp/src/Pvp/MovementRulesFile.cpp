#include "RetroFPS/Pvp/MovementRulesFile.hpp"

#include "RetroFPS/Pvp/NativeFiles.hpp"
#include "engine/os/Files.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace fps::pvp {

std::optional<StanceRules> LoadStanceRules(const std::filesystem::path& path, const Arena& arena, std::string& error) {
    error.clear();
    try {
        NativeReadFailure failure;
        const auto bytes = ReadNativeFile(path, failure, 16 * 1024);
        if (!bytes && failure.tooLarge) throw std::runtime_error("Movement rules exceed 16 KiB");
        if (!bytes) throw std::runtime_error("Cannot open movement rules: " + Engine::Os::Files::PathForMessage(path));
        const auto json = nlohmann::json::parse(bytes->begin(), bytes->end());
        constexpr std::array<std::string_view, 5> Keys{
            "version", "crouch_body_height", "crouch_eye_height", "crouch_speed_scale", "sprint_speed_scale"};
        if (!json.is_object() || json.size() != Keys.size())
            throw std::runtime_error("Movement rules must have exactly the version and the four stance values");
        for (const auto key : Keys)
            if (!json.contains(key)) throw std::runtime_error("Movement rules lack " + std::string(key));
        if (!json.at("version").is_number_integer() || json.at("version").get<std::int64_t>() != 1)
            throw std::runtime_error("Unsupported movement rules version");
        const auto value = [&](const char* key) {
            const auto& field = json.at(key);
            if (!field.is_number()) throw std::runtime_error(std::string("Movement rule ") + key + " is not a number");
            return field.get<float>();
        };
        const StanceRules rules{value("crouch_body_height"), value("crouch_eye_height"),
            value("crouch_speed_scale"), value("sprint_speed_scale")};
        if (!ValidStanceRules(arena, rules))
            throw std::runtime_error("Movement rules outside protocol-v8 section 3 for arena " + arena.id);
        return rules;
    } catch (const std::exception& failure) {
        error = failure.what();
        return std::nullopt;
    }
}

} // namespace fps::pvp
