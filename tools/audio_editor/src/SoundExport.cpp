#include "gyo/audio_editor/SoundExport.hpp"

#include "gyo/audio_editor/Conversion.hpp"
#include "gyo/audio_editor/WavFile.hpp"

#include "gyo/framework/CatalogRegistration.hpp"

#include "engine/os/Files.hpp"

#include <array>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace Gyo::Tools::AudioEditor {
namespace {

namespace Files = Engine::Os::Files;

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
    auto reader = Files::FileReader::Open(path);
    if (!reader) {
        return Engine::Base::Err(FileError(AudioEditorErrorCode::ReadFailed, "cannot open", reader.error(), path));
    }
    std::vector<std::byte> bytes;
    std::array<std::byte, 64 * 1024> buffer{};
    while (true) {
        const auto read = reader->Read(buffer);
        if (!read) {
            return Engine::Base::Err(FileError(AudioEditorErrorCode::ReadFailed, "cannot read", read.error(), path));
        }
        if (*read == 0) return bytes;
        bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(*read));
    }
}

// Writes beside the target, then renames over it: a reader sees the old file
// or the new one.
Result<void, AudioEditorError> WriteReplacing(const std::string& path, const std::span<const std::byte> bytes) {
    const std::string temporary = path + ".gyo-tmp";
    const auto fail = [&](std::string message, const auto& cause) {
        (void)Files::RemoveFile(temporary);
        return Engine::Base::Err(FileError(AudioEditorErrorCode::WriteFailed, std::move(message), cause, path));
    };
    auto writer = Files::FileWriter::Open(temporary, Files::WriteMode::Truncate, Files::LineEndings::AsWritten);
    if (!writer) return fail("cannot create", writer.error());
    if (auto written = writer->Write(bytes); !written) return fail("cannot write", written.error());
    if (auto closed = writer->Close(); !closed) return fail("cannot close", closed.error());
    if (auto renamed = Files::RenameReplacing(temporary, path); !renamed) return fail("cannot replace", renamed.error());
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
    auto update = Framework::RegisterAsset(
        std::string_view(reinterpret_cast<const char*>(catalogText->data()), catalogText->size()),
        {request.registration.id, std::string(kSoundType), request.registration.path});
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

    const std::size_t slash = report.outputPath.find_last_of('/');
    if (slash != std::string::npos) {
        if (auto created = Files::CreateDirectories(report.outputPath.substr(0, slash)); !created) {
            return Engine::Base::Err(
                FileError(AudioEditorErrorCode::WriteFailed, "cannot create the directory", created.error(),
                          report.outputPath));
        }
    }
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
