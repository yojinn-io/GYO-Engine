#include "gyo/model_viewer/EventDocument.hpp"
#include "gyo/model_viewer/EventFiles.hpp"
#include "gyo/model_viewer/ModelContent.hpp"
#include "gyo/model_viewer/ModelSummary.hpp"

#include "ViewerWindow.hpp"

#include "gyo/framework/HeadlessShell.hpp"

#include "engine/base/Error.hpp"

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace {

using namespace Gyo::Tools::ModelViewer;

constexpr std::string_view kHelp =
    "Usage:\n"
    "  gyo_model_viewer models    --asset-root <dir> [--asset-catalog <catalog.json>]\n"
    "  gyo_model_viewer info      --asset-root <dir> [--asset-catalog <catalog.json>] --model <asset id>\n"
    "  gyo_model_viewer roundtrip --asset-root <dir> [--asset-catalog <catalog.json>] --events <asset id>\n"
    "  gyo_model_viewer window    [--asset-root <dir> [--asset-catalog <catalog.json>]] [--gpu-driver <driver>]\n"
    "  gyo_model_viewer --help\n"
    "\n"
    "Without --asset-catalog the asset root is a content root (content.json).\n"
    "models lists the model assets; info describes a model and its gyo.anim_events\n"
    "files; roundtrip checks that the viewer would save an events file as Engine\n"
    "reads it. Nothing is written. window opens the viewer's window (gui variant):\n"
    "the models, a model's 3D preview and its clips; --gpu-driver is auto, d3d12,\n"
    "vulkan or metal.\n"
    "Exit status: 0 done, 2 usage, 3 a file could not be read, 4 content that\n"
    "cannot be mounted or that Engine or the catalog refuses.\n";

int ExitCode(const ModelViewerError& error) {
    switch (error.code) {
    case ModelViewerErrorCode::ReadFailed:
    case ModelViewerErrorCode::WriteFailed: return 3;
    default: return 4;
    }
}

int Fail(const ModelViewerError& error) {
    std::cerr << "error: " << Engine::Base::Describe(error) << '\n';
    return ExitCode(error);
}

int Usage(const std::string_view message) {
    std::cerr << "error: " << message << "\n\n" << kHelp;
    return 2;
}

std::filesystem::path PathOf(const std::string& utf8) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

// The options after the command; each takes one value and appears once.
std::optional<std::map<std::string, std::string, std::less<>>> Options(const std::span<const std::string> arguments,
                                                                       std::initializer_list<std::string_view> allowed,
                                                                       std::string& problem) {
    std::map<std::string, std::string, std::less<>> options;
    for (std::size_t index = 2; index < arguments.size(); ++index) {
        const std::string& name = arguments[index];
        if (std::find(allowed.begin(), allowed.end(), name) == allowed.end()) {
            problem = "unexpected argument " + name;
            return std::nullopt;
        }
        if (index + 1 >= arguments.size() || options.contains(name)) {
            problem = name + " needs one value";
            return std::nullopt;
        }
        options[name] = arguments[++index];
    }
    return options;
}

Result<ModelContent, ModelViewerError> MountFrom(const std::map<std::string, std::string, std::less<>>& options) {
    std::optional<std::filesystem::path> catalog;
    if (const auto found = options.find("--asset-catalog"); found != options.end()) catalog = PathOf(found->second);
    return ModelContent::Mount(PathOf(options.at("--asset-root")), catalog);
}

int Models(const ModelContent& content) {
    for (const auto* model : content.Models()) std::cout << model->id << '\t' << model->sourcePath << '\n';
    return 0;
}

int Info(const ModelContent& content, const std::string& id) {
    auto loaded = content.LoadModel(id);
    if (!loaded) return Fail(loaded.error());
    std::cout << id << " (" << loaded->path << ")\n" << DescribeModel(*loaded->model);
    auto candidates = content.EventsFor(id);
    if (!candidates) return Fail(candidates.error());
    std::cout << "gyo.anim_events: " << candidates->size() << '\n';
    for (const auto& candidate : *candidates) {
        std::cout << "  " << candidate.assetId << ": ";
        if (!candidate.events) {
            std::cout << "refused by Engine: " << candidate.refusal << '\n';
            continue;
        }
        // Parsed is not yet valid: binding to this model is Engine's to decide too.
        if (auto bound = Engine::Model::BindAnimationEvents(*candidate.events, *loaded->model); !bound) {
            std::cout << "refused by Engine: " << Engine::Base::Describe(bound.error()) << '\n';
            continue;
        }
        std::size_t count = 0;
        for (const auto& track : candidate.events->tracks) count += track.events.size();
        std::cout << count << " events in " << candidate.events->tracks.size() << " clips\n";
    }
    return 0;
}

int Roundtrip(const ModelContent& content, const std::string& id) {
    auto text = content.ReadText(id);
    if (!text) return Fail(text.error());
    auto parsed = Engine::Model::ParseAnimationEvents(*text);
    if (!parsed) {
        return Fail(ModelViewerError::Make(ModelViewerErrorCode::InvalidEvents, "Engine refuses the events",
                                           Engine::Base::CauseDetail(parsed.error(), id)));
    }
    auto loaded = content.LoadModel(parsed->modelAssetId);
    if (!loaded) return Fail(loaded.error());
    const auto* asset = content.Find(id);
    auto opened = OpenEvents(Utf8Path(asset->resolvedPath), loaded->assetId, *loaded->model);
    if (!opened) return Fail(opened.error());
    auto written = CheckedText(opened->document.Set(), *loaded->model);
    if (!written) return Fail(written.error());
    std::cout << id << ": reads back the same; the text is "
              << (*written == *text ? "byte for byte the same" : "laid out differently") << '\n';
    return 0;
}

int Run(const std::span<const std::string> arguments) {
    if (arguments.size() < 2) return Usage("no command");
    const std::string_view command = arguments[1];
    if (command == "--help") {
        std::cout << kHelp;
        return 0;
    }
    std::string problem;
    std::optional<std::map<std::string, std::string, std::less<>>> options;
    std::string_view required;
    if (command == "models") {
        options = Options(arguments, {"--asset-root", "--asset-catalog"}, problem);
    } else if (command == "info") {
        options = Options(arguments, {"--asset-root", "--asset-catalog", "--model"}, problem);
        required = "--model";
    } else if (command == "window") {
        options = Options(arguments, {"--asset-root", "--asset-catalog", "--gpu-driver"}, problem);
        if (!options) return Usage(problem);
        if (options->contains("--asset-catalog") && !options->contains("--asset-root")) {
            return Usage("--asset-catalog needs --asset-root");
        }
        WindowOptions window;
        if (const auto found = options->find("--asset-root"); found != options->end()) window.assetRoot = found->second;
        if (const auto found = options->find("--asset-catalog"); found != options->end()) window.assetCatalog = found->second;
        if (const auto found = options->find("--gpu-driver"); found != options->end()) window.gpuDriver = found->second;
        return RunViewerWindow(window);
    } else if (command == "roundtrip") {
        options = Options(arguments, {"--asset-root", "--asset-catalog", "--events"}, problem);
        required = "--events";
    } else {
        return Usage("unknown command " + std::string(command));
    }
    if (!options) return Usage(problem);
    if (!options->contains("--asset-root")) return Usage(std::string(command) + " needs --asset-root");
    if (!required.empty() && !options->contains(required)) {
        return Usage(std::string(command) + " needs " + std::string(required));
    }
    auto content = MountFrom(*options);
    if (!content) return Fail(content.error());
    if (command == "models") return Models(*content);
    if (command == "info") return Info(*content, options->find("--model")->second);
    return Roundtrip(*content, options->find("--events")->second);
}

} // namespace

int main(const int argc, char* argv[]) {
    // The shell turns the command line into UTF-8 (the narrow argv is the
    // ANSI code page on Windows) and shows UTF-8 console output as UTF-8.
    return Gyo::Tools::Framework::RunHeadlessShell(argc, argv, Run, Usage);
}
