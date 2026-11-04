#include "FileAccess.hpp"

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"

#include <string_view>
#include <utility>

namespace Gyo::Tools::AudioEditor::Detail {
namespace {

namespace Io = Engine::IO;

template <class E>
AudioEditorError FileError(const AudioEditorErrorCode code, std::string message, const E& cause,
                           const std::string_view path) {
    return AudioEditorError::Make(code, std::move(message), Engine::Base::CauseDetail(cause, path));
}

} // namespace

Result<std::vector<std::byte>, AudioEditorError> ReadAll(const std::string& path) {
    Io::FS::NativeFileSystem files;
    auto uri = Io::FS::NativeFileUri(path);
    if (!uri) return Engine::Base::Err(FileError(AudioEditorErrorCode::ReadFailed, "cannot open", uri.error(), path));
    Io::Helpers::ReadAllOptions read;
    read.maxBytes = kMaxReadBytes;
    auto bytes = Io::Helpers::ReadAllBytes(files, uri.value(), read);
    if (!bytes) return Engine::Base::Err(FileError(AudioEditorErrorCode::ReadFailed, "cannot read", bytes.error(), path));
    return std::move(bytes).value();
}

Result<void, AudioEditorError> WriteReplacing(const std::string& path, const std::span<const std::byte> bytes) {
    Io::FS::NativeFileSystem files;
    auto uri = Io::FS::NativeFileUri(path);
    if (!uri) return Engine::Base::Err(FileError(AudioEditorErrorCode::WriteFailed, "cannot write", uri.error(), path));
    auto written = Io::Helpers::WriteFileAtomically(files, uri.value(),
                                                    Engine::Base::ConstSpan<std::byte>(bytes.data(), bytes.size()),
                                                    {.createParentDirectories = true});
    if (!written) {
        return Engine::Base::Err(FileError(AudioEditorErrorCode::WriteFailed, "cannot write", written.error(), path));
    }
    return {};
}

} // namespace Gyo::Tools::AudioEditor::Detail
