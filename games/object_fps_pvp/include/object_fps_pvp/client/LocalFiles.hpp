#pragma once

// The Client's own files in the user's data directory (the single player's save, the device id; later the settings):
// small UTF-8 texts read and written whole, through the IO helpers (a write replaces the file in one step).

#include "engine/base/Result.hpp"
#include "engine/io/IOError.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::client {

// The file's text; nullopt when it is not there.
[[nodiscard]] GYO::Engine::Base::Result<std::optional<std::string>, GYO::Engine::IO::IOError> ReadLocalText(const std::string& path);

// Writes text as the whole file, making the directories above it.
[[nodiscard]] GYO::Engine::Base::Result<void, GYO::Engine::IO::IOError> WriteLocalText(const std::string& path, const std::string& text);

// Where this Client keeps its files (the settings, and a directory per profile): the user's data directory, or, in
// portable mode, beside the program (Godot's self-contained mode, editor/file_system/editor_paths.cpp:144-160).
struct DataPlace final {
    std::string directory;
    bool portable = false;
};

// The empty file whose presence turns portable mode on, looked for in PortableDirectory.
inline constexpr std::string_view kPortableMarker = "portable";

// The directory beside the program: the executable's own (either separator), or, for an executable inside a macOS app
// bundle (<dir>/X.app/Contents/MacOS/<exe>: the bundle is read-only), the directory that holds X.app.
[[nodiscard]] std::string PortableDirectory(std::string_view executablePath);

// The place: PortableDirectory when the marker is there in it (exists(<dir>/portable)), else the user's data directory.
// executablePath none (it could not be asked): no portable mode. nullopt: neither (nothing is saved).
[[nodiscard]] std::optional<DataPlace> ChooseDataPlace(const std::optional<std::string>& executablePath,
                                                       const std::function<bool(const std::string&)>& exists,
                                                       const std::optional<std::string>& userData);

} // namespace object_fps_pvp::client
