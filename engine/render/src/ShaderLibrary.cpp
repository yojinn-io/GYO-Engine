#include "render/ShaderLibrary.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>

#include "engine/base/Sha256.hpp"

namespace Engine::Render {
namespace {
using Json = nlohmann::json;
RenderError Invalid(std::string message, std::string detail = {}) {
    return RenderError::Make(RenderErrorCode::InvalidArgument,
        "ShaderLibrary: " + std::move(message), std::move(detail));
}
std::string RequiredString(const Json& object, const char* key) {
    const auto value = object.at(key).get<std::string>();
    if (value.empty()) throw std::runtime_error(std::string("empty ") + key);
    return value;
}
std::uint32_t UnsignedInteger(const Json& value) {
    if (!value.is_number_integer() ||
        (value.is_number_integer() && !value.is_number_unsigned() && value.get<std::int64_t>() < 0) ||
        value.get<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("shader metadata requires a 32-bit unsigned integer");
    return value.get<std::uint32_t>();
}
std::string RelativePath(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    const std::filesystem::path path(value);
    if (value.empty() || value.find(':') != std::string::npos || path.is_absolute() ||
        path.has_root_name() || path.has_root_directory())
        throw std::runtime_error("artifact path must be relative: " + value);
    for (const auto& part : path)
        if (part == "..") throw std::runtime_error("artifact path escapes bundle: " + value);
    return path.lexically_normal().generic_string();
}
ShaderFormat ParseFormat(const std::string& value) {
    if (value == "dxil") return ShaderFormat::DXIL;
    if (value == "spirv") return ShaderFormat::SPIRV;
    if (value == "metallib") return ShaderFormat::Metallib;
    throw std::runtime_error("unsupported artifact format: " + value);
}
Asset::Loading::ByteBuffer Read(Asset::Loading::IAssetSource& source,
    const Asset::Resolver::AssetPathResolver& resolver, const std::string& path) {
    const auto resolved = resolver.Resolve(RelativePath(path));
    if (!resolved) throw std::runtime_error(resolved.error().message + ": " + path);
    auto bytes = source.ReadAll(resolved.value().Str());
    if (!bytes) throw std::runtime_error(bytes.error().message + ": " + resolved.value().Str());
    if (bytes.value().empty()) throw std::runtime_error("empty artifact: " + path);
    return std::move(bytes).value();
}
std::shared_ptr<const ShaderArtifact> LoadStage(const Json& stage,
    ShaderStage kind, ShaderFormat format, std::string_view interfaceName,
    Asset::Loading::IAssetSource& source, const Asset::Resolver::AssetPathResolver& resolver,
    const std::filesystem::path& manifestDirectory) {
    auto artifact = std::make_shared<ShaderArtifact>();
    artifact->format = format;
    artifact->stage = kind;
    artifact->entrypoint = RequiredString(stage, "entrypoint");
    artifact->resources.samplers = UnsignedInteger(stage.at("samplers"));
    if (!stage.at("uniform_sizes").is_array()) throw std::runtime_error("uniform_sizes must be an array");
    for (const auto& size : stage.at("uniform_sizes"))
        artifact->resources.uniformBufferSizes.push_back(UnsignedInteger(size));
    artifact->resources.storageBuffers = UnsignedInteger(stage.value("storage_buffers", Json(0)));
    artifact->resources.storageTextures = UnsignedInteger(stage.value("storage_textures", Json(0)));
    const bool vertex = kind == ShaderStage::Vertex;
    const std::vector<std::uint32_t> expected = interfaceName == "unlit"
        ? std::vector<std::uint32_t>{vertex ? 64U : 48U}
        : vertex ? std::vector<std::uint32_t>{} : std::vector<std::uint32_t>{16U};
    if (artifact->resources.samplers != (vertex ? 0U : 1U) ||
        artifact->resources.uniformBufferSizes != expected ||
        artifact->resources.storageBuffers || artifact->resources.storageTextures)
        throw std::runtime_error("artifact resource layout does not match " + std::string(interfaceName));
    const auto file = RelativePath(RequiredString(stage, "file"));
    artifact->code = Read(source, resolver, (manifestDirectory / file).generic_string());
    const auto hash = RequiredString(stage, "sha256");
    if (Base::Sha256(artifact->code) != hash)
        throw std::runtime_error("artifact content hash mismatch: " + file);
    return artifact;
}
} // namespace

Base::Result<void, RenderError> ShaderLibrary::AppendBundle(
    Asset::Loading::IAssetSource& source,
    const Asset::Resolver::AssetPathResolver& resolver, std::string_view manifestPath) {
    using Result = Base::Result<void, RenderError>;
    try {
        const auto path = RelativePath(std::string(manifestPath));
        const auto bytes = Read(source, resolver, path);
        const auto document = Json::parse(reinterpret_cast<const char*>(bytes.data()),
            reinterpret_cast<const char*>(bytes.data()) + bytes.size());
        if (UnsignedInteger(document.at("version")) != 1 ||
            RequiredString(document, "abi") != ShaderAbiVersion)
            return Result::Err(Invalid("unsupported bundle schema or shader ABI", path));
        if (!document.at("programs").is_array() || document.at("programs").empty())
            return Result::Err(Invalid("bundle requires programs", path));
        decltype(programs_) pending;
        for (const auto& program : document.at("programs")) {
            const auto id = RequiredString(program, "id");
            if ((!id.starts_with("builtin/") && !id.starts_with("game/")) ||
                id.find("..") != std::string::npos || id.find('\\') != std::string::npos)
                throw std::runtime_error("invalid logical shader ID: " + id);
            if (programs_.contains(id) || pending.contains(id))
                throw std::runtime_error("duplicate logical shader ID: " + id);
            const auto interfaceName = RequiredString(program, "interface");
            if (interfaceName != "unlit" && interfaceName != "scene_post")
                throw std::runtime_error("unsupported shader interface: " + interfaceName);
            if ((id == "builtin/unlit" && interfaceName != "unlit") ||
                (id == "builtin/scene_post" && interfaceName != "scene_post"))
                throw std::runtime_error("builtin shader interface mismatch: " + id);
            const auto& variants = program.at("variants");
            if (!variants.is_array() || variants.empty())
                throw std::runtime_error("shader has no artifact variants: " + id);
            auto& target = pending[id];
            for (const auto& variant : variants) {
                const auto format = ParseFormat(RequiredString(variant, "format"));
                if (target.contains(format)) throw std::runtime_error("duplicate shader format: " + id);
                const auto directory = std::filesystem::path(path).parent_path();
                ShaderProgram loaded{interfaceName,
                    LoadStage(variant.at("vertex"), ShaderStage::Vertex, format, interfaceName, source, resolver, directory),
                    LoadStage(variant.at("fragment"), ShaderStage::Fragment, format, interfaceName, source, resolver, directory)};
                target.emplace(format, std::move(loaded));
            }
        }
        // No observable mutation before the entire bundle (including bytes) is valid.
        auto newVersion = version_;
        if (!newVersion.empty()) newVersion += ";";
        newVersion += Base::Sha256(bytes).substr(0, 16);
        programs_.merge(pending);
        version_ = std::move(newVersion);
        return Result::Ok();
    } catch (const std::exception& error) {
        return Result::Err(Invalid(error.what(), std::string(manifestPath)));
    }
}

Base::Result<ShaderProgram, RenderError> ShaderLibrary::FindProgram(
    std::string_view id, ShaderFormat format) const {
    using Result = Base::Result<ShaderProgram, RenderError>;
    const auto found = programs_.find(id);
    if (found == programs_.end()) return Result::Err(Invalid("unknown shader ID", std::string(id)));
    const auto variant = found->second.find(format);
    if (variant == found->second.end())
        return Result::Err(Invalid("shader format is unavailable", std::string(id) + ": " + std::string(ShaderFormatName(format))));
    return Result::Ok(variant->second);
}

ShaderFormatMask ShaderLibrary::CompleteFormats() const noexcept {
    if (programs_.empty()) return 0;
    ShaderFormatMask available = AllShaderFormats;
    for (const auto& [id, variants] : programs_) {
        (void)id;
        ShaderFormatMask mask = 0;
        for (const auto& [format, program] : variants) { (void)program; mask |= FormatBit(format); }
        available &= mask;
    }
    return available;
}

std::vector<std::string> ShaderLibrary::ProgramIds() const {
    std::vector<std::string> ids;
    for (const auto& [id, variants] : programs_) { (void)variants; ids.push_back(id); }
    return ids;
}

} // namespace Engine::Render
