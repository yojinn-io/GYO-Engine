#include "object_fps_pvp/client/LocalFiles.hpp"

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllText.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"

#include <cstddef>

namespace object_fps_pvp::client {

namespace Base = GYO::Engine::Base;
namespace FS = GYO::Engine::IO::FS;
namespace H = GYO::Engine::IO::Helpers;
namespace IO = GYO::Engine::IO;

Base::Result<std::optional<std::string>, IO::IOError> ReadLocalText(const std::string& path) {
    auto uri = FS::NativeFileUri(path);
    if (!uri) return Base::Err(std::move(uri).error());
    FS::NativeFileSystem fs;
    auto text = H::ReadAllTextUtf8(fs, *uri);
    if (!text) {
        if (text.error().code == IO::IOErrorCode::NotFound) return std::optional<std::string>{};
        return Base::Err(std::move(text).error());
    }
    return std::optional<std::string>(std::move(*text));
}

Base::Result<void, IO::IOError> WriteLocalText(const std::string& path, const std::string& text) {
    auto uri = FS::NativeFileUri(path);
    if (!uri) return Base::Err(std::move(uri).error());
    FS::NativeFileSystem fs;
    const auto* bytes = reinterpret_cast<const std::byte*>(text.data());
    H::WriteAtomicallyOptions options;
    options.createParentDirectories = true;
    return H::WriteFileAtomically(fs, *uri, Base::ConstSpan<std::byte>(bytes, text.size()), options);
}

namespace {

// The path up to its last separator (none: empty).
std::string_view Parent(const std::string_view path) {
    const auto at = path.find_last_of("/\\");
    return at == std::string_view::npos ? std::string_view{} : path.substr(0, at);
}

// The last component of a path.
std::string_view Name(const std::string_view path) {
    const auto at = path.find_last_of("/\\");
    return at == std::string_view::npos ? path : path.substr(at + 1);
}

} // namespace

std::string PortableDirectory(const std::string_view executablePath) {
    const std::string_view directory = Parent(executablePath);
    const std::string_view contents = Parent(directory);
    const std::string_view bundle = Parent(contents);
    if (Name(directory) == "MacOS" && Name(contents) == "Contents" && Name(bundle).ends_with(".app")) {
        return std::string(Parent(bundle));
    }
    return std::string(directory);
}

std::optional<DataPlace> ChooseDataPlace(const std::optional<std::string>& executablePath,
                                         const std::function<bool(const std::string&)>& exists,
                                         const std::optional<std::string>& userData) {
    if (executablePath) {
        std::string directory = PortableDirectory(*executablePath);
        if (!directory.empty() && exists(directory + "/" + std::string(kPortableMarker))) {
            return DataPlace{std::move(directory), true};
        }
    }
    if (userData) return DataPlace{*userData, false};
    return std::nullopt;
}

} // namespace object_fps_pvp::client
