#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <spirv_cross_c.h>
#include <nlohmann/json.hpp>
#include "engine/base/Sha256.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using Bytes = std::vector<std::uint8_t>;
constexpr const char* kAbi = "gyo.raster.v1";

[[noreturn]] void Fail(const std::string& message) { throw std::runtime_error(message); }
void Check(bool condition, const std::string& message) { if (!condition) Fail(message); }
std::string Hash(std::span<const std::uint8_t> bytes) {
    return Engine::Base::Sha256(std::as_bytes(bytes));
}
std::string Hash(const std::string& text) {
    return Engine::Base::Sha256(std::as_bytes(std::span(text.data(), text.size())));
}
Bytes Read(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    Check(static_cast<bool>(stream), "cannot read " + path.string());
    const auto size = stream.tellg();
    Check(size >= 0 && size <= 64 * 1024 * 1024, "input is too large: " + path.string());
    Bytes bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Check(static_cast<bool>(stream), "incomplete read: " + path.string());
    return bytes;
}
std::string Text(const fs::path& path) {
    const auto bytes = Read(path);
    return {bytes.begin(), bytes.end()};
}
void Write(const fs::path& path, std::span<const std::uint8_t> bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    Check(static_cast<bool>(stream), "cannot write " + path.string());
}
void Write(const fs::path& path, const std::string& text) {
    Write(path, std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}
void AtomicWrite(const fs::path& path, const std::string& text) {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path temp = path.string() + ".tmp-" + std::to_string(nonce);
    Write(temp, text);
    if (!SDL_RenamePath(temp.string().c_str(), path.string().c_str())) {
        fs::remove(temp);
        Fail("cannot publish " + path.string() + ": " + SDL_GetError());
    }
}
void Run(const std::vector<std::string>& arguments) {
    std::vector<const char*> argv;
    for (const auto& argument : arguments) argv.push_back(argument.c_str());
    argv.push_back(nullptr);
    SDL_Process* process = SDL_CreateProcess(argv.data(), false);
    Check(process != nullptr, "cannot start " + arguments.front() + ": " + SDL_GetError());
    int exitCode = -1;
    const bool waited = SDL_WaitProcess(process, true, &exitCode);
    SDL_DestroyProcess(process);
    Check(waited && exitCode == 0, arguments.front() + " failed, exit " + std::to_string(exitCode));
}
std::string Capture(const std::vector<std::string>& arguments) {
    std::vector<const char*> argv;
    for (const auto& argument : arguments) argv.push_back(argument.c_str());
    argv.push_back(nullptr);
    const auto properties = SDL_CreateProperties();
    Check(properties != 0, "cannot create process properties");
    SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process* process = SDL_CreateProcessWithProperties(properties);
    SDL_DestroyProperties(properties);
    Check(process != nullptr, "cannot start " + arguments.front() + ": " + SDL_GetError());
    int exitCode = -1;
    std::size_t size{};
    void* output = SDL_ReadProcess(process, &size, &exitCode);
    const std::string result = output ? std::string(static_cast<const char*>(output), size) : std::string{};
    SDL_free(output);
    SDL_DestroyProcess(process);
    Check(exitCode == 0 && !result.empty(), arguments.front() + " did not return tool metadata: " + result);
    return result;
}

struct Context {
    spvc_context value{};
    Context() { Check(spvc_context_create(&value) == SPVC_SUCCESS, "cannot create SPIRV-Cross context"); }
    ~Context() { spvc_context_destroy(value); }
    Context(const Context&) = delete;
    void Require(spvc_result result) const {
        if (result != SPVC_SUCCESS) Fail(std::string("SPIRV-Cross: ") + spvc_context_get_last_error_string(value));
    }
    spvc_compiler Compiler(const Bytes& code, spvc_backend backend) const {
        Check(!code.empty() && code.size() % sizeof(SpvId) == 0, "invalid SPIR-V byte size");
        std::vector<SpvId> aligned(code.size() / sizeof(SpvId));
        std::memcpy(aligned.data(), code.data(), code.size());
        spvc_parsed_ir ir{};
        Require(spvc_context_parse_spirv(value, aligned.data(), aligned.size(), &ir));
        spvc_compiler compiler{};
        Require(spvc_context_create_compiler(value, backend, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler));
        return compiler;
    }
};
std::vector<spvc_reflected_resource> Resources(const Context& context, spvc_resources resources,
                                               spvc_resource_type kind) {
    const spvc_reflected_resource* result{};
    std::size_t count{};
    context.Require(spvc_resources_get_resource_list_for_type(resources, kind, &result, &count));
    if (count == 0) return {};
    return {result, result + count};
}
Json Type(spvc_type type) {
    return {{"bits", spvc_type_get_bit_width(type)},
            {"base", spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32 ? "float" : "other"},
            {"vectors", spvc_type_get_vector_size(type)}, {"columns", spvc_type_get_columns(type)},
            {"arrays", spvc_type_get_num_array_dimensions(type)}};
}
void RequireFloat(const Json& type, unsigned vectorSize, unsigned columns = 1) {
    Check(type.at("base") == "float" && type.at("bits") == 32 && type.at("vectors") == vectorSize &&
          type.at("columns") == columns && type.at("arrays") == 0, "shader ABI has an incompatible value type");
}

struct StageData {
    Bytes spirv;
    Json reflection;
    unsigned samplers{};
    std::vector<unsigned> uniformSizes;
    SDL_ShaderCross_ShaderStage stage{};
    std::string entrypoint{"main"};
};

Json ReflectAndValidate(StageData& stage, const std::string& interfaceName) {
    const bool vertex = stage.stage == SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
    Check(interfaceName == "unlit" || interfaceName == "scene_post", "unsupported shader interface " + interfaceName);
    Context context;
    auto compiler = context.Compiler(stage.spirv, SPVC_BACKEND_NONE);
    spvc_resources resources{};
    context.Require(spvc_compiler_create_shader_resources(compiler, &resources));
    const spvc_entry_point* entries{};
    std::size_t entryCount{};
    context.Require(spvc_compiler_get_entry_points(compiler, &entries, &entryCount));
    const auto expectedExecution = vertex ? SpvExecutionModelVertex : SpvExecutionModelFragment;
    Check(entryCount == 1 && entries[0].execution_model == expectedExecution &&
          stage.entrypoint == entries[0].name, "stage or entrypoint mismatch");
    for (const auto kind : {SPVC_RESOURCE_TYPE_STORAGE_BUFFER, SPVC_RESOURCE_TYPE_STORAGE_IMAGE,
                           SPVC_RESOURCE_TYPE_PUSH_CONSTANT, SPVC_RESOURCE_TYPE_SUBPASS_INPUT,
                           SPVC_RESOURCE_TYPE_ATOMIC_COUNTER, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE}) {
        Check(Resources(context, resources, kind).empty(), "gyo.raster.v1 does not support storage/push/combined resources");
    }
    const unsigned expectedSamplers = vertex ? 0U : 1U;
    Json bindingMetadata = Json::array();
    for (const auto kind : {SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS}) {
        const auto list = Resources(context, resources, kind);
        Check(list.size() == expectedSamplers, "shader ABI sampler/texture count mismatch");
        for (const auto& resource : list) {
            const auto set = spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationDescriptorSet);
            const auto binding = spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationBinding);
            const auto type = spvc_compiler_get_type_handle(compiler, resource.type_id);
            Check(set == 2 && binding == 0 && spvc_type_get_num_array_dimensions(type) == 0,
                  "fragment textures and samplers must use set 2 binding 0 without arrays");
            if (kind == SPVC_RESOURCE_TYPE_SEPARATE_IMAGE) {
                Check(spvc_type_get_image_dimension(type) == SpvDim2D && !spvc_type_get_image_arrayed(type) &&
                      !spvc_type_get_image_multisampled(type) && !spvc_type_get_image_is_depth(type),
                      "gyo.raster.v1 requires a non-array, non-MSAA 2D color texture");
            }
            bindingMetadata.push_back({{"kind", kind == SPVC_RESOURCE_TYPE_SEPARATE_IMAGE ? "texture" : "sampler"},
                                       {"set", set}, {"binding", binding}});
        }
    }
    stage.samplers = expectedSamplers;
    const bool hasUniform = !vertex || interfaceName == "unlit";
    const auto uniforms = Resources(context, resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER);
    Check(uniforms.size() == (hasUniform ? 1U : 0U), "shader ABI uniform count mismatch");
    Json uniformMetadata = Json::array();
    for (const auto& resource : uniforms) {
        const auto set = spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationDescriptorSet);
        const auto binding = spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationBinding);
        Check(set == (vertex ? 1U : 3U) && binding == 0, "uniform stage set/binding mismatch");
        Check(spvc_type_get_num_array_dimensions(spvc_compiler_get_type_handle(compiler, resource.type_id)) == 0,
              "uniform buffer arrays are outside gyo.raster.v1");
        const auto type = spvc_compiler_get_type_handle(compiler, resource.base_type_id);
        std::size_t size{};
        context.Require(spvc_compiler_get_declared_struct_size(compiler, type, &size));
        const unsigned expectedSize = vertex ? 64U : interfaceName == "unlit" ? 48U : 16U;
        Check(size == expectedSize, "uniform ABI size mismatch: expected " + std::to_string(expectedSize) +
              ", got " + std::to_string(size));
        const std::vector<std::string> names = vertex ? std::vector<std::string>{"worldViewProjection"} :
            interfaceName == "unlit" ? std::vector<std::string>{"tint", "uvScaleOffset", "alphaParams"} :
            std::vector<std::string>{"sceneColorParams"};
        Check(spvc_type_get_num_member_types(type) == names.size(), "uniform ABI member count mismatch");
        Json members = Json::array();
        for (unsigned index = 0; index < names.size(); ++index) {
            const std::string name = spvc_compiler_get_member_name(compiler, resource.base_type_id, index);
            unsigned offset{};
            context.Require(spvc_compiler_type_struct_member_offset(compiler, type, index, &offset));
            Check(name == names[index] && offset == index * 16U, "uniform ABI member name/offset mismatch: " + name);
            const auto memberType = Type(spvc_compiler_get_type_handle(compiler, spvc_type_get_member_type(type, index)));
            RequireFloat(memberType, 4, vertex ? 4U : 1U);
            Json member{{"name", name}, {"offset", offset}, {"type", memberType}};
            if (vertex) {
                unsigned stride{};
                context.Require(spvc_compiler_type_struct_member_matrix_stride(compiler, type, index, &stride));
                // DXC represents an HLSL row-major matrix as SPIR-V ColMajor.
                const bool columnMajor = spvc_compiler_has_member_decoration(compiler, resource.base_type_id,
                                                                            index, SpvDecorationColMajor);
                Check(stride == 16 && columnMajor, "matrix ABI packing mismatch");
                member["matrix_stride"] = stride;
                member["spirv_major"] = "column";
            }
            members.push_back(std::move(member));
        }
        stage.uniformSizes.push_back(expectedSize);
        uniformMetadata.push_back({{"set", set}, {"binding", binding}, {"size", size}, {"members", members}});
    }
    const auto io = [&](spvc_resource_type kind) {
        Json array = Json::array();
        for (const auto& resource : Resources(context, resources, kind)) {
            Check(spvc_compiler_has_decoration(compiler, resource.id, SpvDecorationLocation), "stage IO lacks location");
            array.push_back({{"location", spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationLocation)},
                             {"type", Type(spvc_compiler_get_type_handle(compiler, resource.type_id))}});
        }
        std::sort(array.begin(), array.end(), [](const auto& a, const auto& b) { return a.at("location") < b.at("location"); });
        return array;
    };
    auto inputs = io(SPVC_RESOURCE_TYPE_STAGE_INPUT);
    auto outputs = io(SPVC_RESOURCE_TYPE_STAGE_OUTPUT);
    const unsigned expectedInputs = vertex ? (interfaceName == "unlit" ? 2U : 0U) : 1U;
    Check(inputs.size() == expectedInputs && outputs.size() == 1, "stage IO count does not match raster ABI");
    for (unsigned index = 0; index < inputs.size(); ++index) {
        Check(inputs[index].at("location") == index, "stage IO location gap");
        RequireFloat(inputs[index].at("type"), vertex && index == 0 ? 3U : 2U);
    }
    Check(outputs[0].at("location") == 0, "stage output must use location 0");
    RequireFloat(outputs[0].at("type"), vertex ? 2U : 4U);
    return {{"uniforms", uniformMetadata}, {"bindings", bindingMetadata}, {"inputs", inputs}, {"outputs", outputs}};
}

std::pair<std::string, std::string> MakeMsl(const StageData& stage) {
    Context context;
    const auto compiler = context.Compiler(stage.spirv, SPVC_BACKEND_MSL);
    const auto execution = stage.stage == SDL_SHADERCROSS_SHADERSTAGE_VERTEX ? SpvExecutionModelVertex : SpvExecutionModelFragment;
    context.Require(spvc_compiler_set_entry_point(compiler, stage.entrypoint.c_str(), execution));
    spvc_compiler_options options{};
    context.Require(spvc_compiler_create_compiler_options(compiler, &options));
    context.Require(spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_VERSION, 20000));
    context.Require(spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_PLATFORM, SPVC_MSL_PLATFORM_MACOS));
    context.Require(spvc_compiler_install_compiler_options(compiler, options));
    // Only the validated graphics resource subset is accepted above. No
    // argument buffers, storage buffers or implicit texture remapping.
    for (const auto& uniform : stage.reflection.at("uniforms")) {
        spvc_msl_resource_binding_2 binding;
        spvc_msl_resource_binding_init_2(&binding);
        binding.stage = execution;
        binding.desc_set = uniform.at("set").get<unsigned>();
        binding.binding = uniform.at("binding").get<unsigned>();
        binding.count = 1;
        binding.msl_buffer = binding.binding;
        context.Require(spvc_compiler_msl_add_resource_binding_2(compiler, &binding));
    }
    if (stage.samplers != 0) {
        spvc_msl_resource_binding_2 binding;
        spvc_msl_resource_binding_init_2(&binding);
        binding.stage = execution;
        binding.desc_set = 2;
        binding.binding = 0;
        binding.count = 1;
        binding.msl_texture = 0;
        binding.msl_sampler = 0;
        context.Require(spvc_compiler_msl_add_resource_binding_2(compiler, &binding));
    }
    const char* source{};
    context.Require(spvc_compiler_compile(compiler, &source));
    const char* entry = spvc_compiler_get_cleansed_entry_point_name(compiler, stage.entrypoint.c_str(), execution);
    Check(entry && *entry, "MSL entrypoint was not emitted");
    return {source, entry};
}

Bytes CopyCompilerOutput(void* memory, std::size_t size, const std::string& operation) {
    Check(memory != nullptr && size > 0, operation + ": " + SDL_GetError());
    const auto* first = static_cast<const std::uint8_t*>(memory);
    Bytes result(first, first + size);
    SDL_free(memory);
    return result;
}
StageData Compile(const fs::path& sourcePath, const fs::path& includeDirectory,
                  const std::string& interfaceName, SDL_ShaderCross_ShaderStage shaderStage,
                  const std::string& entrypoint = "main") {
    Check(std::regex_match(entrypoint, std::regex("[A-Za-z_][A-Za-z0-9_]*")), "invalid shader entrypoint");
    const auto source = Text(sourcePath);
    const auto includes = includeDirectory.string();
    // This private adapter owns SDL_GPU's resource spaces. Shared engine/game
    // HLSL uses logical ABI names and never selects a native graphics API.
    std::array<std::pair<std::string, std::string>, 7> locations{{
        {"GYO_VERTEX_UNIFORM_SLOT", "b0"}, {"GYO_VERTEX_UNIFORM_SPACE", "space1"},
        {"GYO_FRAGMENT_TEXTURE_SLOT", "t0"}, {"GYO_FRAGMENT_SAMPLER_SLOT", "s0"},
        {"GYO_FRAGMENT_IMAGE_SPACE", "space2"},
        {"GYO_FRAGMENT_UNIFORM_SLOT", "b0"}, {"GYO_FRAGMENT_UNIFORM_SPACE", "space3"}}};
    std::array<SDL_ShaderCross_HLSL_Define, locations.size() + 1> defines{};
    for (std::size_t index = 0; index < locations.size(); ++index)
        defines[index] = {locations[index].first.data(), locations[index].second.data()};
    SDL_ShaderCross_HLSL_Info info{};
    info.source = source.c_str();
    info.entrypoint = entrypoint.c_str();
    info.include_dir = includes.c_str();
    info.defines = defines.data();
    info.shader_stage = shaderStage;
    std::size_t size{};
    void* memory = SDL_ShaderCross_CompileSPIRVFromHLSL(&info, &size);
    StageData stage;
    stage.spirv = CopyCompilerOutput(memory, size, sourcePath.string());
    stage.stage = shaderStage;
    stage.entrypoint = entrypoint;
    stage.reflection = ReflectAndValidate(stage, interfaceName);
    return stage;
}

void CollectInputs(const fs::path& source, const fs::path& includeDirectory, std::set<fs::path>& inputs) {
    const auto canonical = fs::canonical(source);
    if (!inputs.insert(canonical).second) return;
    const auto text = Text(canonical);
    static const std::regex includePattern(R"inc(^\s*#\s*include\s*[\"<]([^\">]+)[\">])inc", std::regex::multiline);
    for (auto it = std::sregex_iterator(text.begin(), text.end(), includePattern); it != std::sregex_iterator(); ++it) {
        const fs::path name = (*it)[1].str();
        fs::path include = canonical.parent_path() / name;
        if (!fs::exists(include)) include = includeDirectory / name;
        Check(fs::exists(include), "missing shader include: " + name.string());
        CollectInputs(include, includeDirectory, inputs);
    }
}

struct Options {
    fs::path spec;
    fs::path output;
    fs::path depfile;
    std::vector<std::string> formats;
    std::string xcrun{"xcrun"};
    std::string deploymentTarget;
};

Json Artifact(const StageData& stage, const std::string& format, const Options& options) {
    Bytes code;
    std::string entry = stage.entrypoint;
    if (format == "spirv") {
        code = stage.spirv;
    } else if (format == "dxil") {
        SDL_ShaderCross_SPIRV_Info info{};
        info.bytecode = stage.spirv.data();
        info.bytecode_size = stage.spirv.size();
        info.entrypoint = stage.entrypoint.c_str();
        info.shader_stage = stage.stage;
        std::size_t size{};
        void* memory = SDL_ShaderCross_CompileDXILFromSPIRV(&info, &size);
        code = CopyCompilerOutput(memory, size, "SPIR-V to DXIL");
    } else if (format == "metallib") {
        const auto [msl, metalEntry] = MakeMsl(stage);
        entry = metalEntry;
        const auto key = Hash(msl + options.deploymentTarget);
        const auto base = options.output / "intermediate" / key;
        const fs::path source = base.string() + ".metal";
        const fs::path air = base.string() + ".air";
        const fs::path library = base.string() + ".metallib";
        Write(source, msl);
        Run({options.xcrun, "--sdk", "macosx", "metal", "-std=macos-metal2.0",
             "-mmacosx-version-min=" + options.deploymentTarget, "-c", source.string(), "-o", air.string()});
        Run({options.xcrun, "--sdk", "macosx", "metallib", air.string(), "-o", library.string()});
        code = Read(library);
        Check(!code.empty(), "Metal produced an empty library");
    } else {
        Fail("unsupported shader format " + format);
    }
    const auto hash = Hash(code);
    const auto relative = fs::path("objects") / (hash + "." + format);
    const auto destination = options.output / relative;
    // Objects are immutable and content-addressed; only the final manifest
    // replacement publishes the new bundle. Old bundles stay valid on failure.
    if (!fs::exists(destination) || Hash(Read(destination)) != hash) Write(destination, code);
    return {{"file", relative.generic_string()}, {"entrypoint", entry},
            {"samplers", stage.samplers}, {"uniform_sizes", stage.uniformSizes},
            {"sha256", hash}, {"reflection", stage.reflection}};
}
std::string EscapeDepfile(const fs::path& path) {
    std::string result;
    for (char ch : path.generic_string()) {
        if (ch == ' ' || ch == '#' || ch == ':' || ch == '\\') result += '\\';
        if (ch == '$') result += '$';
        result += ch;
    }
    return result;
}
void Build(const Options& options) {
    Check(!options.formats.empty(), "at least one shader format is required");
    if (std::find(options.formats.begin(), options.formats.end(), "metallib") != options.formats.end())
        Check(!options.deploymentTarget.empty(), "metallib requires --deployment-target matching the application's minimum macOS version");
    const auto spec = Json::parse(Text(options.spec));
    Check(spec.at("version") == 1 && spec.at("programs").is_array() && !spec.at("programs").empty(),
          "shader spec must contain version 1 and programs");
    const auto specRoot = fs::absolute(options.spec).parent_path();
    const auto includeDirectory = fs::weakly_canonical(specRoot / spec.at("include_directory").get<std::string>());
    Json programs = Json::array();
    std::set<fs::path> inputs{fs::canonical(options.spec)};
    std::set<std::string> ids;
    for (const auto& program : spec.at("programs")) {
        const auto id = program.at("id").get<std::string>();
        Check((id.starts_with("builtin/") || id.starts_with("game/")) &&
              id.find("..") == std::string::npos && id.find('\\') == std::string::npos && ids.insert(id).second,
              "invalid or duplicate shader program id: " + id);
        const auto interfaceName = program.at("interface").get<std::string>();
        const auto vertexPath = fs::weakly_canonical(specRoot / program.at("vertex").get<std::string>());
        const auto fragmentPath = fs::weakly_canonical(specRoot / program.at("fragment").get<std::string>());
        CollectInputs(vertexPath, includeDirectory, inputs);
        CollectInputs(fragmentPath, includeDirectory, inputs);
        const auto vertex = Compile(vertexPath, includeDirectory, interfaceName, SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
                                    program.value("vertex_entrypoint", std::string{"main"}));
        const auto fragment = Compile(fragmentPath, includeDirectory, interfaceName, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT,
                                      program.value("fragment_entrypoint", std::string{"main"}));
        Check(vertex.reflection.at("outputs") == fragment.reflection.at("inputs"), "vertex/fragment interface mismatch");
        Json variants = Json::array();
        std::set<std::string> uniqueFormats;
        for (const auto& format : options.formats) {
            Check(uniqueFormats.insert(format).second, "duplicate output format");
            variants.push_back({{"format", format}, {"vertex", Artifact(vertex, format, options)},
                                {"fragment", Artifact(fragment, format, options)}});
        }
        programs.push_back({{"id", id}, {"interface", interfaceName}, {"variants", variants}});
    }
    Json sources = Json::object();
    for (const auto& input : inputs) sources[fs::relative(input, specRoot).generic_string()] = Hash(Read(input));
    Json toolchain{{"shadercross", GYO_SHADERCROSS_REVISION}, {"spirv_cross", GYO_SPIRVCROSS_REVISION},
                   {"dxc", GYO_DXC_REVISION}, {"profile", "gyo.raster.v1"}, {"sources", sources},
                   {"source_hash", Hash(sources.dump())}, {"msl_version", "2.0"},
                   {"macos_deployment_target", options.deploymentTarget}};
    if (std::find(options.formats.begin(), options.formats.end(), "metallib") != options.formats.end()) {
        toolchain["apple_sdk"] = Capture({options.xcrun, "--sdk", "macosx", "--show-sdk-version"});
        toolchain["apple_metal"] = Capture({options.xcrun, "--sdk", "macosx", "metal", "--version"});
    }
    const Json manifest{{"version", 1}, {"abi", kAbi}, {"toolchain", toolchain}, {"programs", programs}};
    fs::create_directories(options.output);
    if (!options.depfile.empty()) {
        std::string dependencies = EscapeDepfile(fs::absolute(options.output / "manifest.json")) + ":";
        for (const auto& input : inputs) dependencies += " " + EscapeDepfile(input);
        AtomicWrite(options.depfile, dependencies + "\n");
    }
    AtomicWrite(options.output / "manifest.json", manifest.dump(2) + "\n");
    std::cout << "shader bundle: " << options.output.string() << " (" << programs.size() << " programs)\n";
}

void SelfTest(const fs::path& root, const fs::path& output) {
    Options valid;
    valid.spec = root / "render/shaders/builtin/bundle.json";
    valid.output = output / "valid";
    valid.formats = {"spirv", "dxil"};
    Build(valid);
    const auto manifest = Json::parse(Text(valid.output / "manifest.json"));
    Check(manifest.at("programs").size() == 2, "builtin program count");
    for (const auto& program : manifest.at("programs")) for (const auto& variant : program.at("variants")) {
        for (const auto* stage : {"vertex", "fragment"}) {
            const auto& artifact = variant.at(stage);
            Check(Hash(Read(valid.output / artifact.at("file").get<std::string>())) == artifact.at("sha256").get<std::string>(), "artifact hash");
        }
    }
    const auto unlit = Compile(root / "render/shaders/builtin/unlit.vert.hlsl", root / "render/shaders/common",
                               "unlit", SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
    const auto [msl, entry] = MakeMsl(unlit);
    Check(!entry.empty() && entry != "main" && msl.find(entry + "(") != std::string::npos, "MSL actual entrypoint metadata");
    const auto fragment = Compile(root / "render/shaders/builtin/unlit.frag.hlsl", root / "render/shaders/common",
                                  "unlit", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);
    const auto [fragmentMsl, fragmentEntry] = MakeMsl(fragment);
    Check(fragmentMsl.find("[[buffer(0)]]") != std::string::npos &&
          fragmentMsl.find("[[texture(0)]]") != std::string::npos &&
          fragmentMsl.find("[[sampler(0)]]") != std::string::npos && !fragmentEntry.empty(), "MSL resource remap");
    Options game = valid;
    game.spec = root / "apps/object_fps/shaders/bundle.json";
    game.output = output / "game";
    Build(game);

    auto namedSpec = Json::parse(Text(valid.spec));
    namedSpec["include_directory"] = fs::absolute(root / "render/shaders/common").generic_string();
    namedSpec["programs"].erase(namedSpec["programs"].begin() + 1, namedSpec["programs"].end());
    namedSpec["programs"][0]["id"] = "game/named_entries";
    for (const auto* kind : {"vertex", "fragment"}) {
        auto& program = namedSpec["programs"][0];
        auto code = Text(root / "render/shaders/builtin" / program.at(kind).get<std::string>());
        const std::string entrypoint = std::string{"GyoNamed"} + (std::string{kind} == "vertex" ? "Vertex" : "Fragment");
        const auto name = code.find("main(");
        Check(name != std::string::npos, "named entrypoint fixture requires main");
        code.replace(name, 4, entrypoint);
        const auto path = output / (std::string{kind} + "-named.hlsl");
        Write(path, code);
        program[kind] = fs::absolute(path).generic_string();
        program[std::string{kind} + "_entrypoint"] = entrypoint;
    }
    const auto namedPath = output / "named.json";
    Write(namedPath, namedSpec.dump());
    Options named = valid;
    named.spec = namedPath;
    named.output = output / "named";
    Build(named);
    const auto namedManifest = Json::parse(Text(named.output / "manifest.json"));
    for (const auto& variant : namedManifest.at("programs")[0].at("variants")) {
        Check(variant.at("vertex").at("entrypoint") == "GyoNamedVertex" &&
              variant.at("fragment").at("entrypoint") == "GyoNamedFragment", "non-main stage entrypoints were lost");
    }
    const auto namedVertex = Compile(output / "vertex-named.hlsl", root / "render/shaders/common", "unlit",
                                    SDL_SHADERCROSS_SHADERSTAGE_VERTEX, "GyoNamedVertex");
    const auto [namedMsl, namedMetalEntry] = MakeMsl(namedVertex);
    Check(namedMetalEntry == "GyoNamedVertex" && namedMsl.find(namedMetalEntry + "(") != std::string::npos,
          "named MSL entrypoint was lost");

    const auto originalManifest = Text(valid.output / "manifest.json");
    const auto expectRejected = [&](const std::string& name, const std::string& code) {
        const auto fixture = output / (name + ".frag.hlsl");
        Write(fixture, code);
        auto spec = Json::parse(Text(valid.spec));
        spec["include_directory"] = fs::absolute(root / "render/shaders/common").generic_string();
        spec["programs"] = Json::array({{{"id", "game/bad_test"}, {"interface", "unlit"},
            {"vertex", fs::absolute(root / "render/shaders/builtin/unlit.vert.hlsl").generic_string()},
            {"fragment", fs::absolute(fixture).generic_string()}}});
        const auto specPath = output / (name + ".json");
        Write(specPath, spec.dump());
        auto invalid = valid;
        invalid.spec = specPath;
        bool rejected = false;
        try { Build(invalid); } catch (const std::exception&) { rejected = true; }
        Check(rejected, "invalid shader accepted: " + name);
        Check(Text(valid.output / "manifest.json") == originalManifest, "failed build replaced a valid manifest");
    };
    expectRejected("compile_error", "this is not hlsl");
    expectRejected("wrong_layout", R"(
Texture2D<float4> t : register(t0, space2); SamplerState s : register(s0, space2);
cbuffer FragmentUniforms : register(b0, space3) { float4 tint; float4 uvScaleOffset; float4 alphaParams; float4 extra; };
float4 main(float2 uv:TEXCOORD0):SV_TARGET { return t.Sample(s,uv)*tint+uvScaleOffset+alphaParams+extra; }
)");
    expectRejected("wrong_binding", R"(
Texture2D<float4> t : register(t1, space2); SamplerState s : register(s1, space2);
cbuffer FragmentUniforms : register(b0, space3) { float4 tint; float4 uvScaleOffset; float4 alphaParams; };
float4 main(float2 uv:TEXCOORD0):SV_TARGET { return t.Sample(s,uv)*tint+uvScaleOffset+alphaParams; }
)");
    expectRejected("wrong_io", R"(
#define GYO_FRAGMENT_UNLIT
#include "RasterAbi.hlsli"
float4 main(float3 uv:TEXCOORD0):SV_TARGET { return GyoSampleUnlit(uv.xy)+uv.z; }
)");
    std::cout << "shader host tests passed: hashes, layouts, bindings, stage IO, MSL names, atomic publication\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        Check(SDL_ShaderCross_Init(), std::string("shadercross initialization: ") + SDL_GetError());
        struct Shutdown { ~Shutdown() { SDL_ShaderCross_Quit(); } } shutdown;
        if (argc == 4 && std::string(argv[1]) == "--self-test") {
            SelfTest(fs::absolute(argv[2]), fs::absolute(argv[3]));
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--version") {
            std::cout << "gyo_shader_tool 1; shadercross " << GYO_SHADERCROSS_REVISION << "; dxc " << GYO_DXC_REVISION << '\n';
            return 0;
        }
        Options options;
        for (int index = 1; index < argc; index += 2) {
            Check(index + 1 < argc, "missing argument value");
            const std::string key = argv[index];
            const std::string value = argv[index + 1];
            if (key == "--spec") options.spec = fs::absolute(value);
            else if (key == "--output") options.output = fs::absolute(value);
            else if (key == "--depfile") options.depfile = fs::absolute(value);
            else if (key == "--xcrun") options.xcrun = value;
            else if (key == "--deployment-target") options.deploymentTarget = value;
            else if (key == "--formats") {
                std::size_t start = 0;
                while (start < value.size()) {
                    const auto end = value.find_first_of(";,", start);
                    options.formats.push_back(value.substr(start, end - start));
                    if (end == std::string::npos) break;
                    start = end + 1;
                }
            } else Fail("unknown option " + key);
        }
        Check(!options.spec.empty() && !options.output.empty(),
              "usage: gyo_shader_tool --spec bundle.json --output directory --formats spirv,dxil[,metallib]");
        Build(options);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "gyo_shader_tool: " << error.what() << '\n';
        return 1;
    }
}
