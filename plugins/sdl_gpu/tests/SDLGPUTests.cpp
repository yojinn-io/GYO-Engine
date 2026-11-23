// sdl_gpu without a GPU (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu"): the startup error without a window
// and the embedded shaders against render's shader ABI (RD-4). Tags "[sg-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "engine/orchestration/Orchestration.hpp"
#include "engine/render/ShaderAbi.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"

#include "EmbeddedShaders.hpp"

namespace O = GYO::Engine::Orchestration;
namespace Abi = GYO::Engine::Render::ShaderAbi;
namespace G = GYO::Plugins::SDLGPU;

namespace {

O::RegisterRow Row(std::string name, O::UnitKind kind, bool library, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = kind;
    row.libraryOnly = library;
    row.defaultOn = kind == O::UnitKind::Module;
    row.depends = std::move(depends);
    return row;
}

} // namespace

TEST_CASE("sdl_gpu startup: without sdl_window there is no window to draw to (SDL-E2)") {
    O::RegisteredUnits units;
    units.rows.push_back(Row("render", O::UnitKind::Module, true));
    units.rows.push_back(Row("render_world", O::UnitKind::Module, false, {"render"}));
    units.modules.push_back({"render_world", std::make_unique<GYO::Engine::RenderWorld::RenderWorldModule>()});
    units.rows.push_back(Row("sdl", O::UnitKind::Plugin, true));
    units.rows.push_back(Row("sdl_gpu", O::UnitKind::Plugin, false, {"sdl", "render", "render_world"}));
    units.plugins.push_back({"sdl_gpu", std::make_unique<G::SDLGPUPlugin>()});
    O::ProductDescription d;
    d.name = "draws";
    O::WorldDescription w;
    w.name = "Render";
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), std::move(units));
    REQUIRE_MESSAGE(!built, "[sg-e2-refused] no Product without a window");
    bool reported = false;
    for (const auto& error : built.error().unitErrors) reported = reported || error.code == "SDL-E2";
    CHECK_MESSAGE(reported, "[sg-e2-code] sdl_gpu reports SDL-E2");
}

TEST_CASE("sdl_gpu shaders: the 2D pass's shaders use what render's ABI says, in every embedded format") {
    const G::EmbeddedShader* vertex = G::FindEmbeddedShader("pass2d.vert");
    const G::EmbeddedShader* fragment = G::FindEmbeddedShader("pass2d.frag");
    REQUIRE(vertex != nullptr);
    REQUIRE(fragment != nullptr);
    CHECK_MESSAGE(vertex->stage == G::ShaderStage::Vertex, "[sg-abi-vertex-stage] the vertex shader is a vertex stage");
    CHECK_MESSAGE(fragment->stage == G::ShaderStage::Fragment, "[sg-abi-fragment-stage] the fragment shader is a fragment stage");
    CHECK_MESSAGE(vertex->resources.uniformBuffers == Abi::kPass2DVertexUniformBuffers, "[sg-abi-vertex-uniforms] one vertex uniform buffer");
    CHECK_MESSAGE(vertex->resources.samplers == Abi::kPass2DVertexSamplers, "[sg-abi-vertex-samplers] no vertex sampler");
    CHECK_MESSAGE(fragment->resources.samplers == Abi::kPass2DFragmentSamplers, "[sg-abi-fragment-samplers] one fragment sampler");
    CHECK_MESSAGE(fragment->resources.uniformBuffers == Abi::kPass2DFragmentUniformBuffers, "[sg-abi-fragment-uniforms] no fragment uniform");
    for (const G::EmbeddedShader* shader : {vertex, fragment}) {
        CHECK_MESSAGE(shader->resources.storageTextures == 0, "[sg-abi-no-storage] no storage textures");
        CHECK_MESSAGE(shader->resources.storageBuffers == 0, "[sg-abi-no-storage] no storage buffers");
        REQUIRE_MESSAGE(!shader->compiled.empty(), "[sg-compiled] at least one format is embedded");
        for (const G::CompiledShader& compiled : shader->compiled) {
            CHECK_MESSAGE(!compiled.code.empty(), "[sg-compiled-code] each format has code");
            CHECK_MESSAGE(compiled.entryPoint == (compiled.format == G::ShaderFormat::Metallib ? "main0" : "main"),
                          "[sg-entry] the entry point of the format (SPIRV-Cross renames main in Metal)");
        }
    }
}
