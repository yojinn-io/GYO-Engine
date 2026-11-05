#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"

#include <utility>

namespace Gyo::Tools::VoxelMapEditor {
namespace {

namespace Io = Engine::IO;

template <class E>
VoxelMapError FileError(const VoxelMapErrorCode code, std::string message, const E& cause, const std::string_view path) {
    return VoxelMapError::Make(code, std::move(message), Engine::Base::CauseDetail(cause, path));
}

} // namespace

Result<std::vector<std::byte>> ReadFile(const std::string& path, const std::size_t maxBytes) {
    Io::FS::NativeFileSystem files;
    auto uri = Io::FS::NativeFileUri(path);
    if (!uri) return Engine::Base::Err(FileError(VoxelMapErrorCode::ReadFailed, "cannot open", uri.error(), path));
    Io::Helpers::ReadAllOptions read;
    read.maxBytes = maxBytes;
    auto bytes = Io::Helpers::ReadAllBytes(files, uri.value(), read);
    if (!bytes) return Engine::Base::Err(FileError(VoxelMapErrorCode::ReadFailed, "cannot read", bytes.error(), path));
    return std::move(bytes).value();
}

// engine/io writes beside the target, then renames over it: a reader sees the
// old file or the new one.
Result<void> WriteFileReplacing(const std::string& path, const std::string_view text) {
    Io::FS::NativeFileSystem files;
    auto uri = Io::FS::NativeFileUri(path);
    if (!uri) return Engine::Base::Err(FileError(VoxelMapErrorCode::WriteFailed, "cannot write", uri.error(), path));
    const auto* data = reinterpret_cast<const std::byte*>(text.data());
    auto written = Io::Helpers::WriteFileAtomically(files, uri.value(), Engine::Base::ConstSpan<std::byte>(data, text.size()),
                                                    {.createParentDirectories = true});
    if (!written) return Engine::Base::Err(FileError(VoxelMapErrorCode::WriteFailed, "cannot write", written.error(), path));
    return {};
}

} // namespace Gyo::Tools::VoxelMapEditor
