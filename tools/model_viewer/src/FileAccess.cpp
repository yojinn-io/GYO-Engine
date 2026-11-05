#include "FileAccess.hpp"

#include "engine/base/Fnv1a.hpp"

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"

#include <utility>

namespace Gyo::Tools::ModelViewer {
namespace {

namespace Io = Engine::IO;

template <class E>
ModelViewerError FileError(const ModelViewerErrorCode code, std::string message, const E& cause,
                           const std::string_view path) {
    return ModelViewerError::Make(code, std::move(message), Engine::Base::CauseDetail(cause, path));
}

} // namespace

Result<std::vector<std::byte>, ModelViewerError> ReadAll(const std::string& path, const std::size_t maxBytes) {
    Io::FS::NativeFileSystem files;
    auto uri = Io::FS::NativeFileUri(path);
    if (!uri) return Engine::Base::Err(FileError(ModelViewerErrorCode::ReadFailed, "cannot open", uri.error(), path));
    Io::Helpers::ReadAllOptions read;
    read.maxBytes = maxBytes;
    auto bytes = Io::Helpers::ReadAllBytes(files, uri.value(), read);
    if (!bytes) return Engine::Base::Err(FileError(ModelViewerErrorCode::ReadFailed, "cannot read", bytes.error(), path));
    return std::move(bytes).value();
}

Result<Io::Helpers::FileStamp, ModelViewerError> Probe(const std::string& path) {
    Io::FS::NativeFileSystem files;
    auto uri = Io::FS::NativeFileUri(path);
    if (!uri) return Engine::Base::Err(FileError(ModelViewerErrorCode::ReadFailed, "cannot open", uri.error(), path));
    Io::Helpers::ReadAllOptions read;
    read.maxBytes = kMaxTextBytes;
    auto stamp = Io::Helpers::ProbeFileStamp(files, uri.value(), read);
    if (!stamp) return Engine::Base::Err(FileError(ModelViewerErrorCode::ReadFailed, "cannot read", stamp.error(), path));
    return stamp.value();
}

Result<void, ModelViewerError> WriteReplacing(const std::string& path, const std::span<const std::byte> bytes) {
    Io::FS::NativeFileSystem files;
    auto uri = Io::FS::NativeFileUri(path);
    if (!uri) return Engine::Base::Err(FileError(ModelViewerErrorCode::WriteFailed, "cannot write", uri.error(), path));
    auto written = Io::Helpers::WriteFileAtomically(files, uri.value(),
                                                    Engine::Base::ConstSpan<std::byte>(bytes.data(), bytes.size()),
                                                    {.createParentDirectories = true});
    if (!written) {
        return Engine::Base::Err(FileError(ModelViewerErrorCode::WriteFailed, "cannot write", written.error(), path));
    }
    return {};
}

Result<Engine::IO::Helpers::FileStamp, ModelViewerError> StampOfWritten(const std::string& path,
                                                                        const std::string& text) {
    auto stamp = Probe(path);
    if (!stamp) {
        auto error = std::move(stamp).error();
        error.detail += "; the file was written";
        return Engine::Base::Err(std::move(error));
    }
    if (!stamp->exists || stamp->contentHash != Engine::Base::Fnv1a64(text)) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::ChangedOnDisk,
                                                        "the file changed right after it was written", path));
    }
    return stamp.value();
}


} // namespace Gyo::Tools::ModelViewer
