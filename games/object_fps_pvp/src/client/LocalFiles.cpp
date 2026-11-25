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

} // namespace object_fps_pvp::client
