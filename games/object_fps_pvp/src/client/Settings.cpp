#include "object_fps_pvp/client/Settings.hpp"

#include "object_fps_pvp/client/LocalFiles.hpp"

#include "data/JsonFields.hpp"

#include "engine/json/Writer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::client {

namespace Base = GYO::Engine::Base;
namespace J = GYO::Engine::Json;
namespace Math = GYO::Engine::Math;

std::string EncodeSettings(const ClientSettings& settings) {
    std::string out;
    J::Writer writer(out);
    writer.BeginObject();
    writer.Key("version");
    writer.Uint(kSettingsVersion);
    writer.Key("resolution");
    writer.BeginArray();
    writer.Int(settings.resolution.x);
    writer.Int(settings.resolution.y);
    writer.EndArray();
    writer.Key("windowMode");
    writer.String(settings.window == GYO::Engine::Window::WindowMode::BorderlessFullscreen ? "borderless" : "windowed");
    writer.Key("vsync");
    writer.Bool(settings.vsync);
    writer.EndObject();
    (void)writer.Finish();
    return out;
}

std::optional<ClientSettings> DecodeSettings(const std::string& text, std::string& error) {
    try {
        const J::Document document = data::ParseDocument(text);
        const J::ObjectView root = data::AsObject(document.Root(), "the settings");
        if (data::Unsigned(data::Member(root, "version"), "version") != kSettingsVersion) throw std::runtime_error("another settings version");
        ClientSettings settings;
        const auto size = data::Member(root, "resolution").AsArray();
        if (!size || size->Size() != 2) throw std::runtime_error("resolution is not [width, height]");
        auto at = size->begin();
        settings.resolution.x = static_cast<std::int32_t>(data::Integer(*at, "the width"));
        ++at;
        settings.resolution.y = static_cast<std::int32_t>(data::Integer(*at, "the height"));
        if (std::find(kResolutions.begin(), kResolutions.end(), settings.resolution) == kResolutions.end()) {
            throw std::runtime_error("resolution is not one of the sizes offered");
        }
        const std::string mode = data::String(data::Member(root, "windowMode"), "windowMode");
        if (mode == "windowed") settings.window = GYO::Engine::Window::WindowMode::Windowed;
        else if (mode == "borderless") settings.window = GYO::Engine::Window::WindowMode::BorderlessFullscreen;
        else throw std::runtime_error("windowMode is neither windowed nor borderless");
        const auto vsync = data::Member(root, "vsync").AsBool();
        if (!vsync) throw std::runtime_error("vsync is not true or false");
        settings.vsync = *vsync;
        return settings;
    } catch (const std::runtime_error& failure) {
        error = failure.what();
        return std::nullopt;
    }
}

LoadedSettings LoadSettings(const std::string& path) {
    LoadedSettings loaded;
    if (path.empty()) return loaded;
    auto text = ReadLocalText(path);
    if (!text) {
        loaded.problem = path + ": " + Base::Describe(text.error());
        return loaded;
    }
    if (!*text) return loaded;
    std::string error;
    if (auto settings = DecodeSettings(**text, error)) {
        loaded.settings = *settings;
    } else {
        loaded.problem = path + ": " + error;
    }
    return loaded;
}

Base::Result<void, GYO::Engine::IO::IOError> SaveSettings(const std::string& path, const ClientSettings& settings) {
    return WriteLocalText(path, EncodeSettings(settings));
}

Math::Vec2i WindowSizeFor(const Math::Vec2i physical, const float scale) {
    const float by = scale > 0.0F ? scale : 1.0F;
    return {static_cast<int>(std::lround(static_cast<float>(physical.x) / by)), static_cast<int>(std::lround(static_cast<float>(physical.y) / by))};
}

Math::Vec2i NextResolution(const Math::Vec2i now) {
    const auto at = std::find(kResolutions.begin(), kResolutions.end(), now);
    if (at == kResolutions.end() || at + 1 == kResolutions.end()) return kResolutions[0];
    return *(at + 1);
}

std::string ResolutionText(const Math::Vec2i size) { return std::to_string(size.x) + " × " + std::to_string(size.y); }

} // namespace object_fps_pvp::client
