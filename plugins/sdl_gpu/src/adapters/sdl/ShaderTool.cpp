// sdl_gpu's shader tool (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu", the shader tool): a host program
// built by plugins/sdl_gpu/shader_tool/ and run at build time. It compiles HLSL files to the asked formats through
// SDL_shadercross (DXC for HLSL to SPIR-V and SPIR-V to DXIL, SPIRV-Cross for SPIR-V to Metal source, Apple's metal
// and metallib for the Metal library), reads what each shader uses from its SPIR-V and writes one C++ source that
// defines EmbeddedShaders() (../../EmbeddedShaders.hpp). Files and processes go through SDL (os guard R4: no fstream,
// no file-system calls of std::filesystem in runtime code).
//
//   gyo_sdl_gpu_shader_tool --output <file.cpp> --include <dir> --formats spirv,dxil,metallib
//                           [--work <dir>] [--xcrun <path>] [--deployment-target <version>]
//                           --shader <name> <vertex|fragment> <file.hlsl> [--shader ...]
//
// It exits 0 and writes the output, or prints what failed and exits 1 (the output is left as it was).

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

[[noreturn]] void Fail(const std::string& message) { throw std::runtime_error(message); }

std::string SDLError() {
    const char* text = SDL_GetError();
    return text != nullptr && *text != '\0' ? std::string(text) : std::string("(SDL gave no text)");
}

Bytes Read(const std::string& path) {
    std::size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (data == nullptr) Fail("cannot read " + path + ": " + SDLError());
    const auto* first = static_cast<const std::uint8_t*>(data);
    Bytes bytes(first, first + size);
    SDL_free(data);
    return bytes;
}

void Write(const std::string& path, const void* data, const std::size_t size) {
    if (!SDL_SaveFile(path.c_str(), data, size)) Fail("cannot write " + path + ": " + SDLError());
}

// Writes next to the destination (its directory made first), then renames: a failed run leaves the old output whole.
void Publish(const std::string& path, const std::string& text) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos && slash > 0) {
        const std::string directory = path.substr(0, slash);
        if (!SDL_CreateDirectory(directory.c_str())) Fail("cannot make " + directory + ": " + SDLError());
    }
    const std::string temporary = path + ".tmp";
    Write(temporary, text.data(), text.size());
    if (!SDL_RenamePath(temporary.c_str(), path.c_str())) {
        static_cast<void>(SDL_RemovePath(temporary.c_str()));
        Fail("cannot publish " + path + ": " + SDLError());
    }
}

// Runs a program and waits; its output and errors go into the failure text.
void Run(const std::vector<std::string>& arguments) {
    std::vector<const char*> argv;
    for (const std::string& argument : arguments) argv.push_back(argument.c_str());
    argv.push_back(nullptr);
    const SDL_PropertiesID properties = SDL_CreateProperties();
    if (properties == 0) Fail("cannot make process properties: " + SDLError());
    SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process* process = SDL_CreateProcessWithProperties(properties);
    SDL_DestroyProperties(properties);
    if (process == nullptr) Fail("cannot start " + arguments.front() + ": " + SDLError());
    int exitCode = -1;
    std::size_t size = 0;
    void* output = SDL_ReadProcess(process, &size, &exitCode);
    const std::string text = output != nullptr ? std::string(static_cast<const char*>(output), size) : std::string();
    SDL_free(output);
    SDL_DestroyProcess(process);
    if (exitCode != 0) {
        std::string command;
        for (const std::string& argument : arguments) command += (command.empty() ? "" : " ") + argument;
        Fail(command + " failed (exit " + std::to_string(exitCode) + "):\n" + text);
    }
}

Bytes TakeCompilerOutput(void* memory, const std::size_t size, const std::string& what) {
    if (memory == nullptr || size == 0) Fail(what + ": " + SDLError());
    const auto* first = static_cast<const std::uint8_t*>(memory);
    Bytes bytes(first, first + size);
    SDL_free(memory);
    return bytes;
}

struct Shader {
    std::string name;
    SDL_ShaderCross_ShaderStage stage{};
    std::string path;
};

struct Options {
    std::string output;
    std::string include;
    std::string work;
    std::string xcrun = "xcrun";
    std::string deploymentTarget;
    std::vector<std::string> formats;
    std::vector<Shader> shaders;
};

std::vector<std::string> Split(const std::string& text, const char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find(separator, start);
        parts.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return parts;
}

Options Parse(const int argc, char** argv) {
    Options options;
    const auto value = [&](int& i) -> std::string {
        if (i + 1 >= argc) Fail(std::string("missing value after ") + argv[i]);
        return argv[++i];
    };
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--output") {
            options.output = value(i);
        } else if (argument == "--include") {
            options.include = value(i);
        } else if (argument == "--work") {
            options.work = value(i);
        } else if (argument == "--xcrun") {
            options.xcrun = value(i);
        } else if (argument == "--deployment-target") {
            options.deploymentTarget = value(i);
        } else if (argument == "--formats") {
            options.formats = Split(value(i), ',');
        } else if (argument == "--shader") {
            Shader shader;
            shader.name = value(i);
            const std::string stage = value(i);
            if (stage == "vertex") {
                shader.stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
            } else if (stage == "fragment") {
                shader.stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
            } else {
                Fail("unknown stage " + stage + " (vertex or fragment)");
            }
            shader.path = value(i);
            options.shaders.push_back(std::move(shader));
        } else {
            Fail("unknown argument " + std::string(argument));
        }
    }
    if (options.output.empty() || options.include.empty() || options.formats.empty() || options.shaders.empty()) {
        Fail("--output, --include, --formats and at least one --shader are needed");
    }
    for (const std::string& format : options.formats) {
        if (format != "spirv" && format != "dxil" && format != "metallib") Fail("unknown format " + format);
        if (format == "metallib" && options.work.empty()) Fail("metallib needs --work");
    }
    return options;
}

struct Compiled {
    std::string format;
    Bytes code;
    std::string entryPoint;
};

struct Result {
    const Shader* shader = nullptr;
    SDL_ShaderCross_GraphicsShaderResourceInfo resources{};
    std::vector<Compiled> compiled;
};

Bytes CompileSpirv(const Shader& shader, const Options& options) {
    const Bytes source = Read(shader.path);
    const std::string text(source.begin(), source.end());
    SDL_ShaderCross_HLSL_Info info{};
    info.source = text.c_str();
    info.entrypoint = "main";
    info.include_dir = options.include.c_str();
    info.defines = nullptr;
    info.shader_stage = shader.stage;
    info.props = 0;
    std::size_t size = 0;
    void* memory = SDL_ShaderCross_CompileSPIRVFromHLSL(&info, &size);
    return TakeCompilerOutput(memory, size, shader.path + " to SPIR-V");
}

SDL_ShaderCross_SPIRV_Info SpirvInfo(const Bytes& spirv, const Shader& shader) {
    SDL_ShaderCross_SPIRV_Info info{};
    info.bytecode = spirv.data();
    info.bytecode_size = spirv.size();
    info.entrypoint = "main";
    info.shader_stage = shader.stage;
    info.props = 0;
    return info;
}

Compiled Metallib(const Bytes& spirv, const Shader& shader, const Options& options) {
    SDL_ShaderCross_SPIRV_Info info = SpirvInfo(spirv, shader);
    const SDL_PropertiesID properties = SDL_CreateProperties();
    if (properties == 0) Fail("cannot make properties: " + SDLError());
    SDL_SetStringProperty(properties, SDL_SHADERCROSS_PROP_SPIRV_MSL_VERSION_STRING, "2.0.0");
    info.props = properties;
    void* memory = SDL_ShaderCross_TranspileMSLFromSPIRV(&info);
    SDL_DestroyProperties(properties);
    if (memory == nullptr) Fail(shader.path + " to Metal source: " + SDLError());
    const std::string msl(static_cast<const char*>(memory));
    SDL_free(memory);
    // SPIRV-Cross renames the entry point "main", a reserved name in Metal.
    if (msl.find("main0(") == std::string::npos) Fail(shader.path + ": the Metal source has no entry point main0");
    if (!SDL_CreateDirectory(options.work.c_str())) Fail("cannot make " + options.work + ": " + SDLError());
    const std::string base = options.work + "/" + shader.name;
    const std::string metal = base + ".metal";
    const std::string air = base + ".air";
    const std::string library = base + ".metallib";
    Write(metal, msl.data(), msl.size());
    std::vector<std::string> compile{options.xcrun, "--sdk", "macosx", "metal", "-std=macos-metal2.0"};
    if (!options.deploymentTarget.empty()) compile.push_back("-mmacosx-version-min=" + options.deploymentTarget);
    compile.insert(compile.end(), {"-c", metal, "-o", air});
    Run(compile);
    Run({options.xcrun, "--sdk", "macosx", "metallib", air, "-o", library});
    Bytes code = Read(library);
    if (code.empty()) Fail(library + " is empty");
    return Compiled{"metallib", std::move(code), "main0"};
}

Result CompileShader(const Shader& shader, const Options& options) {
    Result result;
    result.shader = &shader;
    const Bytes spirv = CompileSpirv(shader, options);
    SDL_ShaderCross_GraphicsShaderMetadata* metadata = SDL_ShaderCross_ReflectGraphicsSPIRV(spirv.data(), spirv.size(), 0);
    if (metadata == nullptr) Fail(shader.path + ": cannot read what the shader uses: " + SDLError());
    result.resources = metadata->resource_info;
    SDL_free(metadata);
    for (const std::string& format : options.formats) {
        if (format == "spirv") {
            result.compiled.push_back(Compiled{"spirv", spirv, "main"});
        } else if (format == "dxil") {
            const SDL_ShaderCross_SPIRV_Info info = SpirvInfo(spirv, shader);
            std::size_t size = 0;
            void* memory = SDL_ShaderCross_CompileDXILFromSPIRV(&info, &size);
            result.compiled.push_back(Compiled{"dxil", TakeCompilerOutput(memory, size, shader.path + " to DXIL"), "main"});
        } else {
            result.compiled.push_back(Metallib(spirv, shader, options));
        }
    }
    return result;
}

std::string Identifier(const std::string& name) {
    std::string id;
    for (const char c : name) id += (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? c : '_';
    return id;
}

const char* FormatName(const std::string& format) {
    if (format == "spirv") return "ShaderFormat::Spirv";
    if (format == "dxil") return "ShaderFormat::Dxil";
    return "ShaderFormat::Metallib";
}

std::string Source(const std::vector<Result>& results) {
    std::string out;
    out += "// Generated at build time by sdl_gpu's shader tool (plugins/sdl_gpu/src/adapters/sdl/ShaderTool.cpp) from\n";
    out += "// render's HLSL. Do not edit; do not commit.\n";
    out += "#include \"EmbeddedShaders.hpp\"\n\nnamespace GYO::Plugins::SDLGPU {\n\nnamespace {\n\n";
    for (const Result& result : results) {
        const std::string id = Identifier(result.shader->name);
        for (const Compiled& compiled : result.compiled) {
            out += "alignas(4) const std::uint8_t k_" + id + "_" + compiled.format + "[] = {";
            for (std::size_t i = 0; i < compiled.code.size(); ++i) {
                if (i % 24 == 0) out += "\n   ";
                out += " " + std::to_string(compiled.code[i]) + ",";
            }
            out += "\n};\n";
        }
        out += "const CompiledShader k_" + id + "_compiled[] = {\n";
        for (const Compiled& compiled : result.compiled) {
            out += std::string("    {") + FormatName(compiled.format) + ", {k_" + id + "_" + compiled.format + ", " +
                   std::to_string(compiled.code.size()) + "}, \"" + compiled.entryPoint + "\"},\n";
        }
        out += "};\n\n";
    }
    out += "const EmbeddedShader kShaders[] = {\n";
    for (const Result& result : results) {
        const std::string id = Identifier(result.shader->name);
        const auto& r = result.resources;
        out += "    {\"" + result.shader->name + "\", " +
               (result.shader->stage == SDL_SHADERCROSS_SHADERSTAGE_VERTEX ? "ShaderStage::Vertex" : "ShaderStage::Fragment") +
               ", {" + std::to_string(r.num_samplers) + ", " + std::to_string(r.num_storage_textures) + ", " +
               std::to_string(r.num_storage_buffers) + ", " + std::to_string(r.num_uniform_buffers) + "}, k_" + id + "_compiled},\n";
    }
    out += "};\n\n} // namespace\n\nstd::span<const EmbeddedShader> EmbeddedShaders() noexcept { return kShaders; }\n\n";
    out += "} // namespace GYO::Plugins::SDLGPU\n";
    return out;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = Parse(argc, argv);
        if (!SDL_ShaderCross_Init()) Fail("SDL_ShaderCross_Init: " + SDLError());
        std::vector<Result> results;
        for (const Shader& shader : options.shaders) results.push_back(CompileShader(shader, options));
        SDL_ShaderCross_Quit();
        Publish(options.output, Source(results));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "gyo_sdl_gpu_shader_tool: %s\n", error.what());
        return 1;
    }
}
