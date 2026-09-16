#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "engine/asset/loading/IAssetSource.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/base/Result.hpp"
#include "render/RenderError.hpp"
#include "render/ShaderTypes.hpp"

namespace Engine::Render {

// CPU-only immutable artifacts. Append is atomic and eagerly reads bytes:
// source/resolver need not outlive this call; device handles are never cached.
class ShaderLibrary final {
public:
    [[nodiscard]] Base::Result<void, RenderError> AppendBundle(
        Asset::Loading::IAssetSource& source,
        const Asset::Resolver::AssetPathResolver& resolver,
        std::string_view manifestPath = "manifest.json");
    [[nodiscard]] Base::Result<ShaderProgram, RenderError> FindProgram(
        std::string_view id, ShaderFormat format) const;
    [[nodiscard]] ShaderFormatMask CompleteFormats() const noexcept;
    [[nodiscard]] std::vector<std::string> ProgramIds() const;
    [[nodiscard]] const std::string& Version() const noexcept { return version_; }
private:
    std::map<std::string, std::map<ShaderFormat, ShaderProgram>, std::less<>> programs_;
    std::string version_;
};

} // namespace Engine::Render
