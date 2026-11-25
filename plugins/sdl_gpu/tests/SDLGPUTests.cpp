// sdl_gpu without a GPU (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu"): the startup error without a window
// and the embedded shaders against render's shader ABI (RD-4). Tags "[sg-...]" and "[g3-...]" are the mutants'
// expectations.
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
#include "PresentModes.hpp"

// The whole file is in the Plugin's namespace (naming N17: a unit opens only its own root, aliases included).
namespace GYO::Plugins::SDLGPU {

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

TEST_CASE("sdl_gpu shaders: the 3D pass's shaders use what render's ABI says, in every embedded format") {
    const G::EmbeddedShader* vertex = G::FindEmbeddedShader("mesh3d.vert");
    const G::EmbeddedShader* fragment = G::FindEmbeddedShader("mesh3d.frag");
    REQUIRE_MESSAGE(vertex != nullptr, "[g3-abi-embedded] the 3D vertex shader is embedded");
    REQUIRE_MESSAGE(fragment != nullptr, "[g3-abi-embedded] the 3D fragment shader is embedded");
    CHECK_MESSAGE(vertex->stage == G::ShaderStage::Vertex, "[g3-abi-vertex-stage] the vertex shader is a vertex stage");
    CHECK_MESSAGE(fragment->stage == G::ShaderStage::Fragment, "[g3-abi-fragment-stage] the fragment shader is a fragment stage");
    CHECK_MESSAGE(vertex->resources.uniformBuffers == Abi::kMesh3DVertexUniformBuffers, "[g3-abi-vertex-uniforms] one vertex uniform buffer");
    CHECK_MESSAGE(vertex->resources.samplers == Abi::kMesh3DVertexSamplers, "[g3-abi-vertex-samplers] no vertex sampler");
    CHECK_MESSAGE(fragment->resources.samplers == Abi::kMesh3DFragmentSamplers, "[g3-abi-fragment-samplers] one fragment sampler");
    CHECK_MESSAGE(fragment->resources.uniformBuffers == Abi::kMesh3DFragmentUniformBuffers,
                  "[g3-abi-fragment-uniforms] one fragment uniform buffer (the colour)");
    for (const G::EmbeddedShader* shader : {vertex, fragment}) {
        CHECK_MESSAGE(shader->resources.storageTextures == 0, "[g3-abi-no-storage] no storage textures");
        CHECK_MESSAGE(shader->resources.storageBuffers == 0, "[g3-abi-no-storage] no storage buffers");
        REQUIRE_MESSAGE(!shader->compiled.empty(), "[g3-abi-compiled] at least one format is embedded");
        for (const G::CompiledShader& compiled : shader->compiled) {
            CHECK_MESSAGE(!compiled.code.empty(), "[g3-abi-compiled-code] each format has code");
            CHECK_MESSAGE(compiled.entryPoint == (compiled.format == G::ShaderFormat::Metallib ? "main0" : "main"),
                          "[g3-abi-entry] the entry point of the format (SPIRV-Cross renames main in Metal)");
        }
    }
}

TEST_CASE("sdl_gpu present: render's PresentMode falls back along Bevy's chain to what the window supports") {
    using PM = GYO::Engine::Render::PresentMode;
    using SM = G::SwapchainPresentMode;
    for (const bool immediate : {false, true}) {
        for (const bool mailbox : {false, true}) {
            CHECK_MESSAGE(G::ChoosePresentMode(PM::AutoVsync, immediate, mailbox) == SM::Vsync, "[sg-present-vsync] VSync on is VSYNC, always");
        }
    }
    CHECK_MESSAGE(G::ChoosePresentMode(PM::AutoNoVsync, true, true) == SM::Immediate, "[sg-present-immediate-first] VSync off: IMMEDIATE first");
    CHECK_MESSAGE(G::ChoosePresentMode(PM::AutoNoVsync, true, false) == SM::Immediate, "[sg-present-immediate] IMMEDIATE without MAILBOX");
    CHECK_MESSAGE(G::ChoosePresentMode(PM::AutoNoVsync, false, true) == SM::Mailbox, "[sg-present-mailbox] then MAILBOX");
    CHECK_MESSAGE(G::ChoosePresentMode(PM::AutoNoVsync, false, false) == SM::Vsync, "[sg-present-last-vsync] then VSYNC");
}

} // namespace GYO::Plugins::SDLGPU
