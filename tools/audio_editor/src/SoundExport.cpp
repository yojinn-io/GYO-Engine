#include "gyo/audio_editor/SoundExport.hpp"

#include "gyo/audio_editor/Conversion.hpp"
#include "gyo/audio_editor/Edit.hpp"
#include "gyo/audio_editor/WavFile.hpp"

#include "gyo/framework/json/CatalogText.hpp"

#include "FileAccess.hpp"

#include "engine/os/Files.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace Gyo::Tools::AudioEditor {
namespace {

namespace Files = Engine::Os::Files;
using Detail::ReadAll;
using Detail::WriteReplacing;

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

Result<std::string, AudioEditorError> Resolved(const std::string& path) {
    auto resolved = Files::ResolvedPath(path);
    if (!resolved) {
        return Engine::Base::Err(FileError(AudioEditorErrorCode::ReadFailed, "cannot resolve", resolved.error(), path));
    }
    return std::move(*resolved);
}

// Whether two resolved paths name one file under the OS layer's comparison of
// names (ignoring case on Windows). ResolvedPath gives an existing prefix its
// real form but keeps a name that does not exist yet as written, so this
// compares those names too.
Result<bool, AudioEditorError> SameFile(const std::string_view left, const std::string_view right) {
    std::size_t a = 0;
    std::size_t b = 0;
    while (true) {
        const std::size_t aEnd = std::min(left.find('/', a), left.size());
        const std::size_t bEnd = std::min(right.find('/', b), right.size());
        auto same = Files::SamePathComponent(left.substr(a, aEnd - a), right.substr(b, bEnd - b));
        if (!same) {
            return Engine::Base::Err(FileError(AudioEditorErrorCode::ReadFailed, "cannot compare", same.error(), left));
        }
        if (!*same) return false;
        if (aEnd == left.size() || bEnd == right.size()) return aEnd == left.size() && bEnd == right.size();
        a = aEnd + 1;
        b = bEnd + 1;
    }
}

std::string Join(std::string root, const std::string_view relative) {
    if (!root.empty() && root.back() != '/' && root.back() != '\\') root += '/';
    root += relative;
    return root;
}

} // namespace

Result<std::vector<ExportReport>, AudioEditorError> ExportSounds(const ExportBatch& batch) {
    if (batch.regions.empty()) {
        return Engine::Base::Err(AudioEditorError::Make(AudioEditorErrorCode::InvalidRegistration,
                                                        "there is no region to export", batch.sourcePath));
    }
    for (std::size_t index = 0; index < batch.regions.size(); ++index) {
        for (std::size_t other = 0; other < index; ++other) {
            if (batch.regions[other].registration.id == batch.regions[index].registration.id) {
                return Engine::Base::Err(AudioEditorError::Make(AudioEditorErrorCode::InvalidRegistration,
                                                                "two regions have the same id",
                                                                batch.regions[index].registration.id));
            }
        }
    }

    auto catalogText = ReadAll(batch.catalogPath);
    if (!catalogText) return Engine::Base::Err(std::move(catalogText).error());
    std::string text(reinterpret_cast<const char*>(catalogText->data()), catalogText->size());
    std::vector<ExportReport> reports(batch.regions.size());
    for (std::size_t index = 0; index < batch.regions.size(); ++index) {
        const SoundRegistration& registration = batch.regions[index].registration;
        // The default policy refuses an id of another type and a path another id has (exit status 4).
        auto update = Framework::RegisterAsset(
            text, {.id = registration.id, .type = std::string(kSoundType), .path = registration.path});
        if (!update) return Engine::Base::Err(RegistrationFailed(std::move(update).error()));
        text = std::move(update->text);
        reports[index].added = update->added;
        reports[index].previousPath = std::move(update->previousPath);
    }
    // A path one region gives up and another registers in the same export is
    // still registered, so it is not reported as left behind.
    for (ExportReport& report : reports) {
        if (!report.previousPath.has_value()) continue;
        for (const SoundRegion& region : batch.regions) {
            if (region.registration.path == *report.previousPath) {
                report.previousPath.reset();
                break;
            }
        }
    }

    std::vector<std::string> outputs;
    outputs.reserve(batch.regions.size());
    auto source = Resolved(batch.sourcePath);
    if (!source) return Engine::Base::Err(std::move(source).error());
    auto catalog = Resolved(batch.catalogPath);
    if (!catalog) return Engine::Base::Err(std::move(catalog).error());
    for (std::size_t index = 0; index < batch.regions.size(); ++index) {
        reports[index].outputPath = Join(batch.assetRoot, batch.regions[index].registration.path);
        auto output = Resolved(reports[index].outputPath);
        if (!output) return Engine::Base::Err(std::move(output).error());
        for (const std::string* protectedPath : {&*source, &*catalog}) {
            auto same = SameFile(*output, *protectedPath);
            if (!same) return Engine::Base::Err(std::move(same).error());
            if (*same) {
                return Engine::Base::Err(AudioEditorError::Make(AudioEditorErrorCode::InvalidRegistration,
                                                                "the clip would overwrite its source or the catalog",
                                                                reports[index].outputPath));
            }
        }
        for (const std::string& earlier : outputs) {
            auto same = SameFile(*output, earlier);
            if (!same) return Engine::Base::Err(std::move(same).error());
            if (*same) {
                return Engine::Base::Err(AudioEditorError::Make(AudioEditorErrorCode::InvalidRegistration,
                                                                "two regions write the same file",
                                                                reports[index].outputPath));
            }
        }
        outputs.push_back(std::move(*output));
    }

    auto sourceBytes = ReadAll(batch.sourcePath);
    if (!sourceBytes) return Engine::Base::Err(std::move(sourceBytes).error());
    auto decoded = DecodeWav(*sourceBytes);
    if (!decoded) {
        auto error = std::move(decoded).error();
        error.detail = batch.sourcePath;
        return Engine::Base::Err(std::move(error));
    }
    std::vector<std::vector<std::byte>> wavs;
    wavs.reserve(batch.regions.size());
    for (std::size_t index = 0; index < batch.regions.size(); ++index) {
        const SoundRegion& region = batch.regions[index];
        auto rendered = RenderClip(*decoded, region.settings, DitherSeed(region.registration.id));
        if (!rendered) {
            auto error = std::move(rendered).error();
            error.detail = region.registration.id + ": " + error.detail;
            return Engine::Base::Err(std::move(error));
        }
        const ConvertedClip& clip = *rendered;
        wavs.push_back(EncodePcm16Wav(clip.channels, clip.sampleRate, clip.samples));
        ExportReport& report = reports[index];
        report.sourceRate = decoded->sampleRate;
        report.sourceFormatTag = decoded->formatTag;
        report.sourceBits = decoded->bits;
        report.channels = clip.channels;
        report.outputRate = clip.sampleRate;
        report.frames = clip.Frames();
        report.saturated = clip.saturated;
    }

    for (std::size_t index = 0; index < wavs.size(); ++index) {
        if (auto written = WriteReplacing(reports[index].outputPath, wavs[index]); !written) {
            // The clips already written stay in the asset root, unregistered: say which.
            auto error = std::move(written).error();
            for (std::size_t done = 0; done < index; ++done) {
                error.detail += (done == 0 ? "; written but not registered: " : ", ") + reports[done].outputPath;
            }
            return Engine::Base::Err(std::move(error));
        }
    }
    if (auto written = WriteReplacing(batch.catalogPath, std::as_bytes(std::span(text.data(), text.size())));
        !written) {
        auto error = std::move(written).error();
        for (std::size_t done = 0; done < reports.size(); ++done) {
            error.detail += (done == 0 ? "; written but not registered: " : ", ") + reports[done].outputPath;
        }
        return Engine::Base::Err(std::move(error));
    }
    return reports;
}

Result<ExportReport, AudioEditorError> ExportSound(const ExportRequest& request) {
    auto reports = ExportSounds({.sourcePath = request.sourcePath,
                                 .assetRoot = request.assetRoot,
                                 .catalogPath = request.catalogPath,
                                 .regions = {{request.registration, request.settings}}});
    if (!reports) return Engine::Base::Err(std::move(reports).error());
    return std::move(reports->front());
}

} // namespace Gyo::Tools::AudioEditor
