#include "object_fps_pvp/client/LocalFiles.hpp"

#include "engine/base/Result.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllText.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"

#include <cstddef>

namespace object_fps_pvp::client {

namespace Base = GYO::Engine::Base;
namespace FS = GYO::Engine::IO::FS;
namespace H = GYO::Engine::IO::Helpers;

std::optional<std::string> ReadLocalText(const std::string& path, std::string& error) {
    error.clear();
    auto uri = FS::NativeFileUri(path);
    if (!uri) {
        error = path + ": " + Base::Describe(uri.error());
        return std::nullopt;
    }
    FS::NativeFileSystem fs;
    auto text = H::ReadAllTextUtf8(fs, *uri);
    if (!text) {
        if (text.error().code != GYO::Engine::IO::IOErrorCode::NotFound) error = path + ": " + Base::Describe(text.error());
        return std::nullopt;
    }
    return std::move(*text);
}

std::string WriteLocalText(const std::string& path, const std::string& text) {
    auto uri = FS::NativeFileUri(path);
    if (!uri) return path + ": " + Base::Describe(uri.error());
    FS::NativeFileSystem fs;
    const auto* bytes = reinterpret_cast<const std::byte*>(text.data());
    H::WriteAtomicallyOptions options;
    options.createParentDirectories = true;
    auto written = H::WriteFileAtomically(fs, *uri, GYO::Engine::Base::ConstSpan<std::byte>(bytes, text.size()), options);
    if (!written) return path + ": " + Base::Describe(written.error());
    return {};
}

} // namespace object_fps_pvp::client
