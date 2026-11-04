#include "gyo/audio_editor/AudioEditorGui.hpp"
#include "gyo/audio_editor/ProjectFile.hpp"
#include "gyo/audio_editor/SoundExport.hpp"
#include "gyo/audio_editor/WavFile.hpp"

#include "gyo/framework/HeadlessShell.hpp"

#include "engine/base/Error.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"

#include <array>
#include <cstdio>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace Gyo::Tools::AudioEditor;

constexpr std::string_view kHelp =
    "Usage:\n"
    "  gyo_audio_editor edit [<project.json> | <source.wav>]\n"
    "  gyo_audio_editor info <source.wav>\n"
    "  gyo_audio_editor convert <source.wav> --asset-root <dir> --asset-catalog <catalog.json>\n"
    "                            --id <asset id> --path <path in the asset root>\n"
    "  gyo_audio_editor export <project.json>\n"
    "  gyo_audio_editor --help\n"
    "\n"
    "edit opens the editor's window: playback, waveform, trim, gain, the project\n"
    "file and export.\n"
    "convert writes the source as a clip of the engine's format into the asset root\n"
    "and adds or updates its \"sound\" entry in the catalog. The source is only read.\n"
    "export does the same for every region of a project: all are checked first,\n"
    "then the clips are written and the catalog once.\n"
    "Exit status: 0 done, 2 usage, 3 a file could not be read or written,\n"
    "4 an invalid WAV, catalog, registration or project.\n";

int ExitCode(const AudioEditorError& error) {
    switch (error.code) {
    case AudioEditorErrorCode::ReadFailed:
    case AudioEditorErrorCode::WriteFailed: return 3;
    default: return 4;
    }
}

int Usage(const std::string_view message) {
    std::cerr << "error: " << message << "\n\n" << kHelp;
    return 2;
}

int Info(const std::string& path) {
    Engine::IO::FS::NativeFileSystem files;
    auto uri = Engine::IO::FS::NativeFileUri(path);
    if (!uri) {
        std::cerr << "error: cannot open " << path << ": " << Engine::Base::Describe(uri.error()) << '\n';
        return 3;
    }
    // The same limit as conversion (src/FileAccess.hpp).
    Engine::IO::Helpers::ReadAllOptions read;
    read.maxBytes = 64U * 1024U * 1024U;
    const auto whole = Engine::IO::Helpers::ReadAllBytes(files, uri.value(), read);
    if (!whole) {
        std::cerr << "error: cannot read " << path << ": " << Engine::Base::Describe(whole.error()) << '\n';
        return 3;
    }
    const std::vector<std::byte>& bytes = whole.value();
    const auto decoded = DecodeWav(bytes);
    if (!decoded) {
        std::cerr << "error: " << path << ": " << Engine::Base::Describe(decoded.error()) << '\n';
        return ExitCode(decoded.error());
    }
    double peak = 0.0;
    for (const auto& plane : decoded->planes) {
        for (const double sample : plane) peak = std::max(peak, sample < 0 ? -sample : sample);
    }
    std::array<char, 64> seconds{};
    std::snprintf(seconds.data(), seconds.size(), "%.3f", static_cast<double>(decoded->Frames()) / decoded->sampleRate);
    std::array<char, 32> peakText{};
    std::snprintf(peakText.data(), peakText.size(), "%.3f", peak);
    std::cout << path << ": " << decoded->sampleRate << " Hz, " << decoded->bits << "-bit "
              << (decoded->formatTag == 3 ? "float" : "integer") << ", " << decoded->planes.size() << " ch, "
              << decoded->Frames() << " frames (" << seconds.data() << " s), peak " << peakText.data() << '\n';
    return 0;
}

void PrintExported(const std::string& source, const std::string& id, const std::string& catalog,
                   const ExportReport& exported) {
    std::cout << source << " -> " << exported.outputPath << ": " << exported.sourceRate << " Hz "
              << exported.sourceBits << "-bit " << exported.channels << " ch -> " << exported.outputRate
              << " Hz 16-bit, " << exported.frames << " frames, saturated " << exported.saturated << '\n';
    std::cout << (exported.added ? "added " : "updated ") << id << " in " << catalog << '\n';
    if (exported.previousPath) {
        std::cout << "warning: " << *exported.previousPath
                  << " is no longer registered; remove it if nothing else uses it\n";
    }
}

int Convert(const std::span<const std::string> arguments) {
    std::optional<std::string> source;
    std::map<std::string, std::string, std::less<>> options{
        {"--asset-root", {}}, {"--asset-catalog", {}}, {"--id", {}}, {"--path", {}}};
    for (std::size_t index = 2; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        const auto option = options.find(argument);
        if (option != options.end()) {
            if (index + 1 >= arguments.size() || !option->second.empty()) {
                return Usage(argument + " needs one value");
            }
            option->second = arguments[++index];
        } else if (!source && !argument.starts_with("--")) {
            source = argument;
        } else {
            return Usage("unexpected argument " + argument);
        }
    }
    if (!source) return Usage("convert needs a source WAV");
    for (const auto& [name, value] : options) {
        if (value.empty()) return Usage("convert needs " + name);
    }
    ExportRequest request;
    request.sourcePath = *source;
    request.assetRoot = options["--asset-root"];
    request.catalogPath = options["--asset-catalog"];
    request.registration = {options["--id"], options["--path"]};
    const auto exported = ExportSound(request);
    if (!exported) {
        std::cerr << "error: " << Engine::Base::Describe(exported.error()) << '\n';
        return ExitCode(exported.error());
    }
    PrintExported(request.sourcePath, request.registration.id, request.catalogPath, *exported);
    return 0;
}

int Export(const std::string& path) {
    const auto project = LoadProject(path);
    if (!project) {
        std::cerr << "error: " << Engine::Base::Describe(project.error()) << '\n';
        return ExitCode(project.error());
    }
    const ExportBatch& batch = project->batch;
    const auto exported = ExportSounds(batch);
    if (!exported) {
        std::cerr << "error: " << Engine::Base::Describe(exported.error()) << '\n';
        return ExitCode(exported.error());
    }
    for (std::size_t index = 0; index < exported->size(); ++index) {
        PrintExported(batch.sourcePath, batch.regions[index].registration.id, batch.catalogPath, (*exported)[index]);
    }
    return 0;
}

int Run(const std::span<const std::string> arguments) {
    if (arguments.size() < 2) return Usage("no command");
    const std::string_view command = arguments[1];
    if (command == "--help") {
        std::cout << kHelp;
        return 0;
    }
    if (command == "info") {
        if (arguments.size() != 3) return Usage("info needs one source WAV");
        return Info(arguments[2]);
    }
    if (command == "convert") return Convert(arguments);
    if (command == "export") {
        if (arguments.size() != 3 || arguments[2].starts_with("--")) return Usage("export needs one project file");
        return Export(arguments[2]);
    }
    if (command == "edit") {
        if (arguments.size() > 3) return Usage("edit takes at most one file");
        if (arguments.size() == 3 && arguments[2].starts_with("--")) return Usage("unexpected argument " + arguments[2]);
        return RunAudioEditorGui(arguments.size() == 3 ? arguments[2] : std::string());
    }
    return Usage("unknown command " + std::string(command));
}

} // namespace

int main(const int argc, char* argv[]) {
    // The shell turns the command line into UTF-8 (the narrow argv is the
    // ANSI code page on Windows) and shows UTF-8 console output as UTF-8.
    return Gyo::Tools::Framework::RunHeadlessShell(argc, argv, Run, Usage);
}
