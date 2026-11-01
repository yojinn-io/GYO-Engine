#include "gyo/audio_editor/SoundExport.hpp"
#include "gyo/audio_editor/WavFile.hpp"

#include "engine/os/Console.hpp"
#include "engine/os/Files.hpp"

#include <array>
#include <cstdio>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace Gyo::Tools::AudioEditor;

constexpr std::string_view kHelp =
    "Usage:\n"
    "  gyo_audio_editor info <source.wav>\n"
    "  gyo_audio_editor convert <source.wav> --asset-root <dir> --asset-catalog <catalog.json>\n"
    "                            --id <asset id> --path <path in the asset root>\n"
    "  gyo_audio_editor --help\n"
    "\n"
    "convert writes the source as a clip of the engine's format into the asset root\n"
    "and adds or updates its \"sound\" entry in the catalog. The source is only read.\n"
    "Exit status: 0 done, 2 usage, 3 a file could not be read or written,\n"
    "4 an invalid WAV, catalog or registration.\n";

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
    auto reader = Engine::Os::Files::FileReader::Open(path);
    if (!reader) {
        std::cerr << "error: cannot open " << path << ": " << Engine::Base::Describe(reader.error()) << '\n';
        return 3;
    }
    std::vector<std::byte> bytes;
    std::array<std::byte, 64 * 1024> buffer{};
    while (true) {
        const auto read = reader->Read(buffer);
        if (!read) {
            std::cerr << "error: cannot read " << path << ": " << Engine::Base::Describe(read.error()) << '\n';
            return 3;
        }
        if (*read == 0) break;
        bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(*read));
    }
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

int Convert(const std::vector<std::string>& arguments) {
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
    std::cout << request.sourcePath << " -> " << exported->outputPath << ": " << exported->sourceRate << " Hz "
              << exported->sourceBits << "-bit " << exported->channels << " ch -> " << exported->outputRate
              << " Hz 16-bit, " << exported->frames << " frames, saturated " << exported->saturated << '\n';
    std::cout << (exported->added ? "added " : "updated ") << request.registration.id << " in "
              << request.catalogPath << '\n';
    if (exported->previousPath) {
        std::cout << "warning: " << *exported->previousPath
                  << " is no longer registered; remove it if nothing else uses it\n";
    }
    return 0;
}

} // namespace

int main(const int argc, char* argv[]) {
    // UTF-8 shows as UTF-8 on a Windows console until main returns.
    const auto utf8Console = Engine::Os::Console::Utf8ConsoleOutput::Enable();
    // UTF-8 arguments on every platform (the narrow argv is the ANSI code page on Windows).
    const auto commandLine = Engine::Os::Console::CommandLineUtf8(argc, argv);
    if (!commandLine) {
        return Usage(Engine::Base::Describe(commandLine.error()));
    }
    const std::vector<std::string>& arguments = *commandLine;
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
    return Usage("unknown command " + std::string(command));
}
