#include "gyo/audio_editor/SoundExport.hpp"

#include "gyo/audio_editor/Conversion.hpp"
#include "gyo/audio_editor/WavFile.hpp"

#include "gyo/framework/json/CatalogText.hpp"

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"
#include "engine/os/Files.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace Gyo::Tools::AudioEditor {
namespace {

namespace Files = Engine::Os::Files;
namespace Io = Engine::IO;

// Larger sources or catalogs are refused rather than read whole: about four
// minutes of 48 kHz 24-bit stereo.
constexpr std::size_t kMaxReadBytes = 64U * 1024U * 1024U;

template <class E>
AudioEditorError FileError(const AudioEditorErrorCode code, std::string message, const E& cause,
                           const std::string_view path) {
    return AudioEditorError::Make(code, std::move(message), Engine::Base::CauseDetail(cause, path));
}

// The catalog type of every clip the editor registers.
constexpr std::string_view kSoundType = "sound";

// The framework's refusal as the editor's own code (exit status 4 either way).
AudioEditorError RegistrationFailed(Framework::CatalogError error) {
    const AudioEditorErrorCode code = error.code == Framework::CatalogErrorCode::InvalidCatalog
                                          ? AudioEditorErrorCode::InvalidCatalog
                                          : AudioEditorErrorCode::InvalidRegistration;
    return AudioEditorError::Make(code, std::move(error.message), std::move(error.detail));
}

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

// engine/io writes beside the target, then renames over it: a reader sees the
// old file or the new one. Missing folders above the target are created.
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

Result<std::string, AudioEditorError> Resolved(const std::string& path) {
    auto resolved = Files::ResolvedPath(path);
    if (!resolved) {
        return Engine::Base::Err(FileError(AudioEditorErrorCode::ReadFailed, "cannot resolve", resolved.error(), path));
    }
    return std::move(*resolved);
}

std::string Join(std::string root, const std::string_view relative) {
    if (!root.empty() && root.back() != '/' && root.back() != '\\') root += '/';
    root += relative;
    return root;
}

} // namespace

Result<ExportReport, AudioEditorError> ExportSound(const ExportRequest& request) {
    auto catalogText = ReadAll(request.catalogPath);
    if (!catalogText) return Engine::Base::Err(std::move(catalogText).error());
    // The default policy refuses an id of another type and a path another id has (exit status 4).
    auto update = Framework::RegisterAsset(
        std::string_view(reinterpret_cast<const char*>(catalogText->data()), catalogText->size()),
        {.id = request.registration.id, .type = std::string(kSoundType), .path = request.registration.path});
    if (!update) return Engine::Base::Err(RegistrationFailed(std::move(update).error()));

    ExportReport report;
    report.outputPath = Join(request.assetRoot, request.registration.path);
    auto output = Resolved(report.outputPath);
    if (!output) return Engine::Base::Err(std::move(output).error());
    for (const std::string* protectedPath : {&request.sourcePath, &request.catalogPath}) {
        auto resolved = Resolved(*protectedPath);
        if (!resolved) return Engine::Base::Err(std::move(resolved).error());
        if (*resolved == *output) {
            return Engine::Base::Err(AudioEditorError::Make(AudioEditorErrorCode::InvalidRegistration,
                                                            "the clip would overwrite its source or the catalog",
                                                            report.outputPath));
        }
    }

    auto sourceBytes = ReadAll(request.sourcePath);
    if (!sourceBytes) return Engine::Base::Err(std::move(sourceBytes).error());
    auto decoded = DecodeWav(*sourceBytes);
    if (!decoded) {
        auto error = std::move(decoded).error();
        error.detail = request.sourcePath;
        return Engine::Base::Err(std::move(error));
    }
    const ConvertedClip clip = ConvertToClip(*decoded, DitherSeed(request.registration.id));

    const std::vector<std::byte> wav = EncodePcm16Wav(clip.channels, clip.sampleRate, clip.samples);
    if (auto written = WriteReplacing(report.outputPath, wav); !written) {
        return Engine::Base::Err(std::move(written).error());
    }
    const std::string& text = update->text;
    if (auto written = WriteReplacing(request.catalogPath, std::as_bytes(std::span(text.data(), text.size())));
        !written) {
        return Engine::Base::Err(std::move(written).error());
    }

    report.sourceRate = decoded->sampleRate;
    report.sourceFormatTag = decoded->formatTag;
    report.sourceBits = decoded->bits;
    report.channels = clip.channels;
    report.outputRate = clip.sampleRate;
    report.frames = clip.Frames();
    report.saturated = clip.saturated;
    report.added = update->added;
    report.previousPath = std::move(update->previousPath);
    return report;
}

} // namespace Gyo::Tools::AudioEditor
