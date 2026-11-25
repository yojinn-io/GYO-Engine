#include "voxel_map_editor/Grid.hpp"

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"

#include <utility>

// Files through engine/io: the tool has no file system code of its own (the OS layer is engine/os's).
namespace voxel_map_editor {
namespace {

namespace Io = GYO::Engine::IO;
namespace Base = GYO::Engine::Base;

template <class E>
Error FileError(const ErrorCode code, std::string message, const E& cause, const std::string_view path) {
    return Error::Make(code, std::move(message), Base::CauseDetail(cause, path));
}

} // namespace

Result<std::vector<std::byte>> ReadFile(const std::string& path, const std::size_t maxBytes) {
    Io::FS::NativeFileSystem files;
    auto uri = Io::FS::NativeFileUri(path);
    if (!uri) return Base::Err(FileError(ErrorCode::ReadFailed, "cannot open", uri.error(), path));
    Io::Helpers::ReadAllOptions read;
    read.maxBytes = maxBytes;
    auto bytes = Io::Helpers::ReadAllBytes(files, uri.value(), read);
    if (!bytes) return Base::Err(FileError(ErrorCode::ReadFailed, "cannot read", bytes.error(), path));
    return std::move(bytes).value();
}

Result<void> WriteFileReplacing(const std::string& path, const std::string_view text) {
    Io::FS::NativeFileSystem files;
    auto uri = Io::FS::NativeFileUri(path);
    if (!uri) return Base::Err(FileError(ErrorCode::WriteFailed, "cannot write", uri.error(), path));
    auto written = Io::Helpers::WriteFileAtomically(files, uri.value(), Base::ConstSpan<std::byte>(AsBytes(text).data(), text.size()),
                                                    {.createParentDirectories = true});
    if (!written) return Base::Err(FileError(ErrorCode::WriteFailed, "cannot write", written.error(), path));
    return {};
}

Result<void> RemoveFileIfPresent(const std::string& path) {
    Io::FS::NativeFileSystem files;
    auto uri = Io::FS::NativeFileUri(path);
    if (!uri) return Base::Err(FileError(ErrorCode::WriteFailed, "cannot remove", uri.error(), path));
    auto removed = files.Remove(uri.value());
    if (!removed && removed.error().code != Io::IOErrorCode::NotFound)
        return Base::Err(FileError(ErrorCode::WriteFailed, "cannot remove", removed.error(), path));
    return {};
}

} // namespace voxel_map_editor
