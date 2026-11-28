// sdl_gpu's adapter (Design Invariant SDL-1: with ShaderTool.cpp, the only place of sdl_gpu that includes SDL3/). It
// implements GPUDevice (../../GPUDevice.hpp): the device, the window's swapchain, the textures (render targets among
// them) and meshes keyed by render's ids, the passes' pipelines and the frame (uploads in one copy pass, then one
// render pass per pass of RenderCommands, in their order, each on its target). Shapes follow SDL's own GPU examples and Dear ImGui's SDL_GPU backend
// (imgui_impl_sdlgpu3.cpp): one transfer buffer cycled each frame, vertex, index and instance buffers grown to the
// frame's need, the scissor per draw; the 3D pass's depth texture as SDL's examples make one (a depth target cleared by its pass's
// load op, made again when the target's size changes). Each pass is recorded in a debug group named by PassLabel, its
// recording timed on the CPU, and each frame handed to the GPU with a fence, looked at later without waiting, for the
// whole frame's GPU time (render-pipeline declaration 15).
#include "../../GPUDevice.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "engine/base/Assert.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/render/ShaderAbi.hpp"

#include "../../EmbeddedShaders.hpp"
#include "../../FrameDraws.hpp"
#include "../../PassLabels.hpp"
#include "../../PresentModes.hpp"

namespace GYO::Plugins::SDLGPU {

namespace R = Engine::Render;
namespace Abi = Engine::Render::ShaderAbi;

namespace {

void RequireMainThread() { GYO_ASSERT(SDL_IsMainThread() && "sdl_gpu's adapter is called on the main thread only (SDL-4)"); }

std::string SDLError(const char* step) {
    const char* text = SDL_GetError();
    return std::string(step) + ": " + (text != nullptr && *text != '\0' ? text : "(SDL gave no text)");
}

SDL_GPUShaderFormat SDLFormat(const ShaderFormat format) noexcept {
    switch (format) {
    case ShaderFormat::Spirv: return SDL_GPU_SHADERFORMAT_SPIRV;
    case ShaderFormat::Dxil: return SDL_GPU_SHADERFORMAT_DXIL;
    case ShaderFormat::Metallib: return SDL_GPU_SHADERFORMAT_METALLIB;
    }
    return SDL_GPU_SHADERFORMAT_INVALID;
}

SDL_GPUShaderFormat FormatsOf(const EmbeddedShader& shader) noexcept {
    SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_INVALID;
    for (const CompiledShader& compiled : shader.compiled) formats |= SDLFormat(compiled.format);
    return formats;
}

// Offsets inside the transfer buffer: D3D12 places texture copies on 512 bytes. Sums are 64-bit; a frame's uploads
// past what one transfer buffer can hold (4 GiB) fail the frame.
constexpr std::uint64_t kPlacement = 512;
constexpr std::uint64_t kMaxFrameBytes = 0xFFFFFFFFULL;
std::uint64_t AlignUp(const std::uint64_t value) noexcept { return (value + kPlacement - 1) / kPlacement * kPlacement; }

// A buffer size for need (at most kMaxFrameBytes): powers of two from 4 KiB, so a buffer grows seldom.
std::uint32_t GrowTo(const std::uint64_t need) noexcept {
    std::uint64_t size = 4096;
    while (size < need) size *= 2;
    return static_cast<std::uint32_t>(std::min(size, kMaxFrameBytes));
}

// SDL's format of a render format (SDL_gpu.h:767, :824).
SDL_GPUTextureFormat SDLTextureFormat(const R::TextureFormat format) noexcept {
    switch (format) {
    case R::TextureFormat::Rgba8Unorm: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case R::TextureFormat::Rgba8UnormSrgb: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    }
    return SDL_GPU_TEXTUREFORMAT_INVALID;
}

// SDL's format of a render target's format: a Depth target's is the device's depth format.
SDL_GPUTextureFormat SDLTargetFormat(const R::RenderTargetFormat format, const SDL_GPUTextureFormat depthFormat) noexcept {
    switch (format) {
    case R::RenderTargetFormat::Rgba8Unorm: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case R::RenderTargetFormat::Depth: return depthFormat;
    }
    return SDL_GPU_TEXTUREFORMAT_INVALID;
}

template <class... F>
struct Overloaded : F... {
    using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>;

// A 3D pass's depth texture for one colour target, made again when the target's size changes.
struct Depth {
    SDL_GPUTexture* texture = nullptr;
    R::TargetSize size{};
};

// A texture of render's asks made from pixels.
struct GPUTexture {
    SDL_GPUTexture* texture = nullptr;
    R::TextureFilter filter = R::TextureFilter::Linear;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// A render target of render's asks (render-pipeline declaration sections 13 and 14.11): its texture made, and made
// again, at the size each TextureTargetSize ask gives (render decides the sizes).
struct GPUTarget {
    SDL_GPUTexture* texture = nullptr;
    R::TextureFilter filter = R::TextureFilter::Linear;
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    // The texture's size (or the size SDL refused: no texture until the next size ask).
    R::TargetSize size{};
    // Its id (render's).
    std::uint32_t id = 0;
    // Drawn on by a frame handed to the GPU since it was made or made again (as RenderTextures's picture). Without
    // one, its first pass clears it (to transparent black when it has no clear; a Depth target to 1), and a draw
    // sampling it in a frame that did not draw it is skipped and counted.
    bool picture = false;
    // A Depth target (the device's depth format): drawn on by depth-only passes and as a 3D pass's depth.
    bool isDepth = false;
    // A colour target's own depth for its 3D passes (none for a Depth target).
    Depth depth;
};

// A sampled texture: a texture's, a target's or the white one.
struct Sampled {
    SDL_GPUTexture* texture = nullptr;
    R::TextureFilter filter = R::TextureFilter::Linear;
};

struct Pipeline {
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    R::BlendMode blend = R::BlendMode::Alpha;
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
};

// The colour a mesh pipeline puts on its target: the target's format and the draw's blend. A depth-only pass has none.
struct MeshColour {
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    R::BlendMode blend = R::BlendMode::Opaque;

    friend bool operator==(const MeshColour&, const MeshColour&) = default;
};

// A mesh pipeline (a 3D pass's, or a depth-only pass's: no colour) and its key. depthWrite is false whenever depthTest
// is (a draw that does not test writes no depth).
struct Pipeline3D {
    std::optional<MeshColour> colour;
    bool depthTest = true;
    bool depthWrite = true;
    R::CullMode cull = R::CullMode::None;
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
};

// A full-screen pass's pipeline and its key: its fragment shader and the target's format.
struct PipelineFullscreen {
    const EmbeddedShader* shader = nullptr;
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
};

// A mesh of render's asks: its vertices and indices in buffers of their own.
struct GPUMesh {
    SDL_GPUBuffer* vertices = nullptr;
    SDL_GPUBuffer* indices = nullptr;
    std::uint32_t indexCount = 0;
};

// The depth formats of 3D passes and Depth targets, the first the device supports as a depth target that is also sampled
// taken (SDL guarantees D16 for both, SDL_gpu.h:703,747; Bevy's is Depth32Float).
constexpr SDL_GPUTextureFormat kDepthFormats[] = {SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                                                  SDL_GPU_TEXTUREFORMAT_D16_UNORM};

// The bytes of a pass's own vertices and indices (Pass2D, Pass3D, PassDepth), and of its instances (Pass3D, PassDepth);
// a full-screen pass has none.
struct PassGeometry {
    const std::uint8_t* vertices = nullptr;
    std::uint64_t vertexBytes = 0;
    const std::uint8_t* indices = nullptr;
    std::uint64_t indexBytes = 0;
    const std::uint8_t* instances = nullptr;
    std::uint64_t instanceBytes = 0;
};

PassGeometry BytesOf(const R::MeshGeometry& g) {
    return PassGeometry{reinterpret_cast<const std::uint8_t*>(g.vertices.data()), g.vertices.size() * sizeof(R::Vertex3D),
                        reinterpret_cast<const std::uint8_t*>(g.indices.data()), g.indices.size() * sizeof(std::uint32_t),
                        reinterpret_cast<const std::uint8_t*>(g.instances.data()), g.instances.size() * sizeof(R::Instance3D)};
}

PassGeometry GeometryOf(const R::RenderPass& pass) {
    return std::visit(Overloaded{
                          [](const R::Pass2D& p) {
                              return PassGeometry{reinterpret_cast<const std::uint8_t*>(p.vertices.data()), p.vertices.size() * sizeof(R::Vertex2D),
                                                  reinterpret_cast<const std::uint8_t*>(p.indices.data()), p.indices.size() * sizeof(std::uint32_t)};
                          },
                          [](const R::Pass3D& p) { return BytesOf(p); },
                          [](const R::PassDepth& p) { return BytesOf(p); },
                          // One triangle of no vertex input.
                          [](const R::PassFullscreen&) { return PassGeometry{}; },
                      },
                      pass);
}

// One chunk of the frame's uploads, at its offset in the transfer buffer.
struct Upload {
    std::uint64_t offset = 0;
    const std::uint8_t* bytes = nullptr;
    std::uint64_t size = 0;
};

SDL_GPUShader* MakeShader(SDL_GPUDevice* device, const EmbeddedShader& shader, std::string& error) {
    const SDL_GPUShaderFormat available = SDL_GetGPUShaderFormats(device);
    for (const CompiledShader& compiled : shader.compiled) {
        if ((available & SDLFormat(compiled.format)) == 0) continue;
        const std::string entry(compiled.entryPoint);
        SDL_GPUShaderCreateInfo info{};
        info.code = compiled.code.data();
        info.code_size = compiled.code.size();
        info.entrypoint = entry.c_str();
        info.format = SDLFormat(compiled.format);
        info.stage = shader.stage == ShaderStage::Vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
        info.num_samplers = shader.resources.samplers;
        info.num_storage_textures = shader.resources.storageTextures;
        info.num_storage_buffers = shader.resources.storageBuffers;
        info.num_uniform_buffers = shader.resources.uniformBuffers;
        SDL_GPUShader* made = SDL_CreateGPUShader(device, &info);
        if (made == nullptr) error = SDLError("SDL_CreateGPUShader");
        return made;
    }
    error = std::string("no compiled form of ") + std::string(shader.name) + " for this device's driver";
    return nullptr;
}

SDL_GPUSampler* MakeSampler(SDL_GPUDevice* device, const SDL_GPUFilter filter) {
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = filter;
    info.mag_filter = filter;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    return SDL_CreateGPUSampler(device, &info);
}

// A pass's debug group on the command buffer (a capture tool shows its name), around the pass's render passes and
// outside them (Metal scopes a group pushed inside a pass to that pass, SDL_gpu.h:2977-2980); counted in the frame's
// timing.
class DebugGroup final {
public:
    DebugGroup(SDL_GPUCommandBuffer* cmd, const std::string& name, std::uint64_t& count) : cmd_(cmd) {
        SDL_PushGPUDebugGroup(cmd_, name.c_str());
        ++count;
    }
    ~DebugGroup() { SDL_PopGPUDebugGroup(cmd_); }
    DebugGroup(const DebugGroup&) = delete;
    DebugGroup& operator=(const DebugGroup&) = delete;

private:
    SDL_GPUCommandBuffer* cmd_;
};

} // namespace

struct GPUDevice::State {
    SDL_GPUDevice* device = nullptr;
    SDL_Window* window = nullptr;
    bool videoStarted = false;
    std::string driver;
    // SDL claims the window with VSYNC (SDL_ClaimWindowForGPUDevice).
    SwapchainPresentMode presentMode = SwapchainPresentMode::Vsync;
    SDL_GPUShader* vertex = nullptr;
    SDL_GPUShader* fragment = nullptr;
    SDL_GPUShader* meshVertex = nullptr;
    SDL_GPUShader* meshFragment = nullptr;
    SDL_GPUShader* depthVertex = nullptr;
    SDL_GPUShader* depthFragment = nullptr;
    SDL_GPUShader* fullscreenVertex = nullptr;
    // The shaders full-screen passes may name besides the embedded ones (GPUDevice::Create's more; a generated table
    // that outlives the device).
    std::span<const EmbeddedShader> more;
    // Full-screen passes' fragment shaders, made the first time a pass names them.
    std::unordered_map<const EmbeddedShader*, SDL_GPUShader*> fragments;
    std::vector<PipelineFullscreen> pipelinesFullscreen;
    SDL_GPUSampler* linear = nullptr;
    SDL_GPUSampler* nearest = nullptr;
    GPUTexture white;
    std::unordered_map<std::uint32_t, GPUTexture> textures;
    std::unordered_map<std::uint32_t, GPUTarget> targets;
    // The targets the frame being recorded drew on (until it is handed to the GPU).
    FrameDraws frameDraws;
    std::vector<Pipeline> pipelines;
    std::unordered_map<std::uint32_t, GPUMesh> meshes;
    std::vector<Pipeline3D> pipelines3d;
    // The 3D passes' depth format, and the depth texture of the main target (each render target has its own).
    SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    Depth depth;
    SDL_GPUTransferBuffer* transfer = nullptr;
    std::uint32_t transferSize = 0;
    SDL_GPUBuffer* vertices = nullptr;
    std::uint32_t verticesSize = 0;
    SDL_GPUBuffer* indices = nullptr;
    std::uint32_t indicesSize = 0;
    // The 3D passes' instances of the frame (Instance3D, read per instance).
    SDL_GPUBuffer* instances = nullptr;
    std::uint32_t instancesSize = 0;
    // The frames recorded (Submit and Capture), and the last one's timing (render-pipeline declaration 15.3).
    std::uint64_t framesRecorded = 0;
    FrameTiming timing;
    // A frame handed to the GPU whose fence was not seen signaled yet, and when it was handed.
    struct InFlight {
        std::uint64_t frame = 0;
        SDL_GPUFence* fence = nullptr;
        std::chrono::steady_clock::time_point submitted;
    };
    std::vector<InFlight> inFlight;
    // The latest frame seen done on the GPU.
    std::optional<GPUFrameTime> gpuTime;

    ~State() {
        if (device != nullptr) {
            for (const InFlight& f : inFlight) SDL_ReleaseGPUFence(device, f.fence);
            for (auto& [id, t] : textures) SDL_ReleaseGPUTexture(device, t.texture);
            for (auto& [id, t] : targets) ReleaseTarget(t);
            if (white.texture != nullptr) SDL_ReleaseGPUTexture(device, white.texture);
            for (Pipeline& p : pipelines) SDL_ReleaseGPUGraphicsPipeline(device, p.pipeline);
            for (Pipeline3D& p : pipelines3d) SDL_ReleaseGPUGraphicsPipeline(device, p.pipeline);
            for (PipelineFullscreen& p : pipelinesFullscreen) SDL_ReleaseGPUGraphicsPipeline(device, p.pipeline);
            for (auto& [shader, made] : fragments) SDL_ReleaseGPUShader(device, made);
            ReleaseMeshes();
            if (depth.texture != nullptr) SDL_ReleaseGPUTexture(device, depth.texture);
            if (linear != nullptr) SDL_ReleaseGPUSampler(device, linear);
            if (nearest != nullptr) SDL_ReleaseGPUSampler(device, nearest);
            if (vertex != nullptr) SDL_ReleaseGPUShader(device, vertex);
            if (fragment != nullptr) SDL_ReleaseGPUShader(device, fragment);
            if (meshVertex != nullptr) SDL_ReleaseGPUShader(device, meshVertex);
            if (meshFragment != nullptr) SDL_ReleaseGPUShader(device, meshFragment);
            for (SDL_GPUShader* shader : {depthVertex, depthFragment, fullscreenVertex}) {
                if (shader != nullptr) SDL_ReleaseGPUShader(device, shader);
            }
            if (transfer != nullptr) SDL_ReleaseGPUTransferBuffer(device, transfer);
            if (vertices != nullptr) SDL_ReleaseGPUBuffer(device, vertices);
            if (indices != nullptr) SDL_ReleaseGPUBuffer(device, indices);
            if (instances != nullptr) SDL_ReleaseGPUBuffer(device, instances);
            if (window != nullptr) SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyGPUDevice(device);
        }
        if (videoStarted) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    // A target's texture and depth (either may be none).
    void ReleaseTarget(GPUTarget& t) {
        if (t.texture != nullptr) SDL_ReleaseGPUTexture(device, t.texture);
        if (t.depth.texture != nullptr) SDL_ReleaseGPUTexture(device, t.depth.texture);
        t.texture = nullptr;
        t.depth = Depth{};
    }

    // What a draw naming id samples: the white texture for TextureId{}, a texture's, a target's; nullopt when it has no
    // GPU texture, or (a target) no picture here: neither drawn by a frame handed to the GPU nor by this frame (a frame
    // the GPU did not get: render-pipeline declaration 14.14).
    std::optional<Sampled> SampledOf(const R::TextureId id) const {
        if (id.value == 0) return Sampled{white.texture, R::TextureFilter::Linear};
        if (const auto it = textures.find(id.value); it != textures.end()) return Sampled{it->second.texture, it->second.filter};
        if (const auto it = targets.find(id.value);
            it != targets.end() && it->second.texture != nullptr && (it->second.picture || frameDraws.Drew(id.value))) {
            // A Depth target between its texels: the nearest (RenderTargetFormat::Depth).
            return Sampled{it->second.texture, it->second.isDepth ? R::TextureFilter::Nearest : it->second.filter};
        }
        return std::nullopt;
    }

    void ReleaseMeshes() {
        for (auto& [id, m] : meshes) {
            SDL_ReleaseGPUBuffer(device, m.vertices);
            SDL_ReleaseGPUBuffer(device, m.indices);
        }
        meshes.clear();
    }

    // The colour target of a pipeline: the format, and the blend of a BlendMode (render.zh-Hant.md section 2).
    static SDL_GPUColorTargetDescription ColorTarget(const SDL_GPUTextureFormat format, const R::BlendMode blend) {
        SDL_GPUColorTargetDescription target{};
        target.format = format;
        if (blend == R::BlendMode::Alpha) {
            target.blend_state.enable_blend = true;
            target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        }
        return target;
    }

    // The pipeline for a target format and a blend mode, made the first time it is asked for; nullptr when SDL refuses.
    SDL_GPUGraphicsPipeline* PipelineFor(const SDL_GPUTextureFormat format, const R::BlendMode blend, std::string& error) {
        for (const Pipeline& p : pipelines) {
            if (p.format == format && p.blend == blend) return p.pipeline;
        }
        const SDL_GPUColorTargetDescription target = ColorTarget(format, blend);
        SDL_GPUVertexBufferDescription buffer{};
        buffer.slot = 0;
        buffer.pitch = Abi::kPass2DVertexStride;
        buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        SDL_GPUVertexAttribute attributes[3]{};
        attributes[0] = {Abi::kPass2DPositionLocation, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, Abi::kPass2DPositionOffset};
        attributes[1] = {Abi::kPass2DUvLocation, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, Abi::kPass2DUvOffset};
        attributes[2] = {Abi::kPass2DColorLocation, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, Abi::kPass2DColorOffset};
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vertex;
        info.fragment_shader = fragment;
        info.vertex_input_state.vertex_buffer_descriptions = &buffer;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = attributes;
        info.vertex_input_state.num_vertex_attributes = 3;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        SDL_GPUGraphicsPipeline* made = SDL_CreateGPUGraphicsPipeline(device, &info);
        if (made == nullptr) {
            error = SDLError("SDL_CreateGPUGraphicsPipeline");
            return nullptr;
        }
        pipelines.push_back(Pipeline{format, blend, made});
        return made;
    }

    // The 3D pipeline of a target format and a draw's blend, depth and cull, made the first time it is asked for;
    // nullptr when SDL refuses. Depth: LESS_OR_EQUAL when the draw tests, ALWAYS and no write when it does not. Cull
    // Back leaves out the triangles that are clockwise on the screen (counter-clockwise is the front, as Bevy's).
    // colour: none for a depth-only pass's (its shaders, no colour target).
    SDL_GPUGraphicsPipeline* Pipeline3DFor(const std::optional<MeshColour>& colour, const R::MeshDraw& draw, std::string& error) {
        const bool writes = draw.depthTest && draw.depthWrite;
        for (const Pipeline3D& p : pipelines3d) {
            if (p.colour == colour && p.depthTest == draw.depthTest && p.depthWrite == writes && p.cull == draw.cull) return p.pipeline;
        }
        const bool depthOnly = !colour;
        const SDL_GPUColorTargetDescription target = depthOnly ? SDL_GPUColorTargetDescription{} : ColorTarget(colour->format, colour->blend);
        // Slot 0 per vertex, slot 1 per instance (ShaderAbi.hpp).
        SDL_GPUVertexBufferDescription buffers[2]{};
        buffers[0].slot = Abi::kMesh3DVertexSlot;
        buffers[0].pitch = Abi::kMesh3DVertexStride;
        buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        buffers[1].slot = Abi::kMesh3DInstanceSlot;
        buffers[1].pitch = Abi::kMesh3DInstanceStride;
        buffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        constexpr std::uint32_t kAttributes = 3 + Abi::kMesh3DWorldColumns;
        SDL_GPUVertexAttribute attributes[kAttributes]{};
        attributes[0] = {Abi::kMesh3DPositionLocation, Abi::kMesh3DVertexSlot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, Abi::kMesh3DPositionOffset};
        attributes[1] = {Abi::kMesh3DUvLocation, Abi::kMesh3DVertexSlot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, Abi::kMesh3DUvOffset};
        for (std::uint32_t column = 0; column < Abi::kMesh3DWorldColumns; ++column) {
            attributes[2 + column] = {Abi::kMesh3DWorldLocation + column, Abi::kMesh3DInstanceSlot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                                   Abi::kMesh3DWorldOffset + column * 4 * static_cast<std::uint32_t>(sizeof(float))};
        }
        attributes[2 + Abi::kMesh3DWorldColumns] = {Abi::kMesh3DColorLocation, Abi::kMesh3DInstanceSlot,
                                                 SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, Abi::kMesh3DColorOffset};
        SDL_GPUGraphicsPipelineCreateInfo info{};
        // A depth-only pass's vertex stage reads the position and the world alone; the other inputs stay bound.
        info.vertex_shader = depthOnly ? depthVertex : meshVertex;
        info.fragment_shader = depthOnly ? depthFragment : meshFragment;
        info.vertex_input_state.vertex_buffer_descriptions = buffers;
        info.vertex_input_state.num_vertex_buffers = 2;
        info.vertex_input_state.vertex_attributes = attributes;
        info.vertex_input_state.num_vertex_attributes = kAttributes;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = draw.cull == R::CullMode::Back ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
        info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.compare_op = draw.depthTest ? SDL_GPU_COMPAREOP_LESS_OR_EQUAL : SDL_GPU_COMPAREOP_ALWAYS;
        info.depth_stencil_state.enable_depth_write = writes;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.target_info.color_target_descriptions = depthOnly ? nullptr : &target;
        info.target_info.num_color_targets = depthOnly ? 0 : 1;
        info.target_info.depth_stencil_format = depthFormat;
        info.target_info.has_depth_stencil_target = true;
        SDL_GPUGraphicsPipeline* made = SDL_CreateGPUGraphicsPipeline(device, &info);
        if (made == nullptr) {
            error = SDLError("SDL_CreateGPUGraphicsPipeline");
            return nullptr;
        }
        pipelines3d.push_back(Pipeline3D{colour, draw.depthTest, writes, draw.cull, made});
        return made;
    }

    // The shader a full-screen pass names: an embedded one, or one of more; nullptr when the device carries none of
    // that name.
    const EmbeddedShader* FindShader(const std::string& name) const {
        if (const EmbeddedShader* embedded = FindEmbeddedShader(name)) return embedded;
        for (const EmbeddedShader& shader : more) {
            if (shader.name == name) return &shader;
        }
        return nullptr;
    }

    // The full-screen pipeline of a fragment shader and a target format, made the first time it is asked for (the
    // shader too); nullptr when SDL refuses. One triangle of no vertex input, no depth, the colour replaced.
    SDL_GPUGraphicsPipeline* PipelineFullscreenFor(const EmbeddedShader& shader, const SDL_GPUTextureFormat format, std::string& error) {
        for (const PipelineFullscreen& p : pipelinesFullscreen) {
            if (p.shader == &shader && p.format == format) return p.pipeline;
        }
        auto made = fragments.find(&shader);
        if (made == fragments.end()) {
            SDL_GPUShader* fragment = MakeShader(device, shader, error);
            if (fragment == nullptr) return nullptr;
            made = fragments.emplace(&shader, fragment).first;
        }
        const SDL_GPUColorTargetDescription target = ColorTarget(format, R::BlendMode::Opaque);
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = fullscreenVertex;
        info.fragment_shader = made->second;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
        if (pipeline == nullptr) {
            error = SDLError("SDL_CreateGPUGraphicsPipeline");
            return nullptr;
        }
        pipelinesFullscreen.push_back(PipelineFullscreen{&shader, format, pipeline});
        return pipeline;
    }

    // The depth texture of a colour target of this size (made again when the size changes). false: SDL refused it.
    bool EnsureDepth(Depth& depth, const R::TargetSize size, std::string& error) {
        if (depth.texture != nullptr && depth.size == size) return true;
        if (depth.texture != nullptr) SDL_ReleaseGPUTexture(device, depth.texture);
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = depthFormat;
        info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        info.width = size.width;
        info.height = size.height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        depth.texture = SDL_CreateGPUTexture(device, &info);
        depth.size = depth.texture != nullptr ? size : R::TargetSize{};
        if (depth.texture == nullptr) error = SDLError("SDL_CreateGPUTexture (depth)");
        return depth.texture != nullptr;
    }

    bool EnsureBuffer(SDL_GPUBuffer*& buffer, std::uint32_t& size, const std::uint64_t need, const SDL_GPUBufferUsageFlags usage,
                      std::string& error) {
        if (need <= size && buffer != nullptr) return true;
        if (buffer != nullptr) SDL_ReleaseGPUBuffer(device, buffer);
        SDL_GPUBufferCreateInfo info{};
        info.usage = usage;
        info.size = GrowTo(need);
        buffer = SDL_CreateGPUBuffer(device, &info);
        size = buffer != nullptr ? info.size : 0;
        if (buffer == nullptr) error = SDLError("SDL_CreateGPUBuffer");
        return buffer != nullptr;
    }

    bool EnsureTransfer(const std::uint64_t need, std::string& error) {
        if (need <= transferSize && transfer != nullptr) return true;
        if (transfer != nullptr) SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = GrowTo(need);
        transfer = SDL_CreateGPUTransferBuffer(device, &info);
        transferSize = transfer != nullptr ? info.size : 0;
        if (transfer == nullptr) error = SDLError("SDL_CreateGPUTransferBuffer");
        return transfer != nullptr;
    }

    // A 2D texture; nullptr when the device does not take the format for usage or SDL refuses (error says which).
    SDL_GPUTexture* MakeTexture(const std::uint32_t width, const std::uint32_t height, const SDL_GPUTextureUsageFlags usage,
                                const SDL_GPUTextureFormat format, std::string& error) const {
        // SDL_gpu.h:4478: a format is not taken for every usage on every driver (Lavapipe included); asked, not assumed.
        if (!SDL_GPUTextureSupportsFormat(device, format, SDL_GPU_TEXTURETYPE_2D, usage)) {
            error = "the device does not support this texture format for this usage";
            return nullptr;
        }
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = format;
        info.usage = usage;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        SDL_GPUTexture* made = SDL_CreateGPUTexture(device, &info);
        if (made == nullptr) error = SDLError("SDL_CreateGPUTexture");
        return made;
    }

    // The usage of a render target: drawn on, then sampled (a Depth target drawn on as depth).
    static constexpr SDL_GPUTextureUsageFlags kTargetUsage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    static constexpr SDL_GPUTextureUsageFlags kDepthTargetUsage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

    // One frame's work on a target: the asks, the uploads, the passes whose target is 0. Records what it could not do.
    struct Frame {
        std::vector<Upload> uploads;
        struct TextureCopy {
            std::uint64_t offset;
            SDL_GPUTexture* texture;
            R::TextureRegion region;
        };
        std::vector<TextureCopy> textureCopies;
        struct BufferCopy {
            std::uint64_t offset;
            SDL_GPUBuffer* buffer;
            std::uint64_t size;
        };
        std::vector<BufferCopy> bufferCopies;
        // Made this frame (let go again when the frame's uploads fail: they would have no pixels).
        std::vector<std::uint32_t> made;
        std::vector<std::uint32_t> madeMeshes;
        // Let go this frame: released once the copy pass that may still name them is recorded (SDL: a released
        // texture is not referenced afterwards).
        std::vector<SDL_GPUTexture*> released;
        std::vector<SDL_GPUBuffer*> releasedBuffers;
        std::uint64_t vertexOffset = 0;
        std::uint64_t vertexBytes = 0;
        std::uint64_t indexOffset = 0;
        std::uint64_t indexBytes = 0;
        std::uint64_t instanceOffset = 0;
        std::uint64_t instanceBytes = 0;
        std::uint64_t total = 0;
    };

    // Applies the asks to the texture table (makes now, lets go after the copy pass; the pixels go in the copy pass).
    void ApplyAsks(const std::span<const R::TextureOp> asks, Frame& frame, FrameTally& tally, R::RenderFrameReceipt& receipt) {
        const auto place = [&](const std::vector<std::uint8_t>& pixels) {
            frame.total = AlignUp(frame.total);
            const std::uint64_t offset = frame.total;
            frame.uploads.push_back(Upload{offset, pixels.data(), pixels.size()});
            frame.total += pixels.size();
            return offset;
        };
        for (const R::TextureOp& ask : asks) {
            if (const auto* make = std::get_if<R::TextureCreate>(&ask)) {
                const SDL_GPUTextureFormat format = SDLTextureFormat(make->desc.format);
                SDL_GPUTexture* texture =
                    MakeTexture(make->desc.width, make->desc.height, SDL_GPU_TEXTUREUSAGE_SAMPLER, format, tally.lastRefusal);
                if (texture == nullptr) {
                    ++tally.refusedAsks;
                    continue;
                }
                textures[make->id.value] = GPUTexture{texture, make->desc.filter, make->desc.width, make->desc.height};
                frame.made.push_back(make->id.value);
                frame.textureCopies.push_back({place(make->pixels), texture, R::TextureRegion{0, 0, make->desc.width, make->desc.height}});
            } else if (const auto* update = std::get_if<R::TextureUpdate>(&ask)) {
                const auto it = textures.find(update->id.value);
                if (it == textures.end()) {
                    ++tally.refusedAsks;
                    continue;
                }
                frame.textureCopies.push_back({place(update->pixels), it->second.texture, update->region});
            } else if (const auto* release = std::get_if<R::TextureRelease>(&ask)) {
                if (const auto it = textures.find(release->id.value); it != textures.end()) {
                    frame.released.push_back(it->second.texture);
                    textures.erase(it);
                } else if (const auto at = targets.find(release->id.value); at != targets.end()) {
                    if (at->second.texture != nullptr) frame.released.push_back(at->second.texture);
                    if (at->second.depth.texture != nullptr) frame.released.push_back(at->second.depth.texture);
                    targets.erase(at);
                }
            } else if (const auto* sized = std::get_if<R::TextureTargetSize>(&ask)) {
                // Made (again) at its size, with no picture; SDL's refusal leaves it without a texture until its next size.
                const auto it = targets.find(sized->id.value);
                if (it == targets.end()) {
                    ++tally.refusedAsks;
                    continue;
                }
                GPUTarget& t = it->second;
                ReleaseTarget(t);
                t.size = sized->size;
                t.texture = MakeTexture(t.size.width, t.size.height, t.isDepth ? kDepthTargetUsage : kTargetUsage, t.format, tally.lastRefusal);
                t.picture = false;
                if (t.texture == nullptr) {
                    ++tally.refusedAsks;
                } else {
                    receipt.RecordApplied(sized->id, t.size);
                }
            } else if (const auto* target = std::get_if<R::TextureTargetCreate>(&ask)) {
                // Its texture is made by its first TextureTargetSize. Asked again for one it has (render asks for a
                // target whose size was not applied again whole): nothing to do, ids are never given twice.
                if (targets.contains(target->id.value)) continue;
                GPUTarget made;
                made.id = target->id.value;
                made.filter = target->desc.filter;
                made.isDepth = target->desc.format == R::RenderTargetFormat::Depth;
                made.format = SDLTargetFormat(target->desc.format, depthFormat);
                targets[target->id.value] = made;
            }
        }
    }

    // Applies the mesh asks to the mesh table (buffers made now, let go after the copy pass; the bytes go in the
    // copy pass), as ApplyAsks does for textures.
    void ApplyMeshAsks(const std::span<const R::MeshOp> asks, Frame& frame, FrameTally& tally) {
        const auto place = [&](const void* bytes, const std::uint64_t size) {
            frame.total = AlignUp(frame.total);
            const std::uint64_t offset = frame.total;
            frame.uploads.push_back(Upload{offset, static_cast<const std::uint8_t*>(bytes), size});
            frame.total += size;
            return offset;
        };
        const auto makeBuffer = [&](const SDL_GPUBufferUsageFlags usage, const std::uint64_t size) {
            SDL_GPUBufferCreateInfo info{};
            info.usage = usage;
            info.size = static_cast<std::uint32_t>(std::min(size, kMaxFrameBytes));
            return SDL_CreateGPUBuffer(device, &info);
        };
        for (const R::MeshOp& ask : asks) {
            if (const auto* make = std::get_if<R::MeshCreate>(&ask)) {
                const std::uint64_t vertexBytes = make->vertices.size() * sizeof(R::Vertex3D);
                const std::uint64_t indexBytes = make->indices.size() * sizeof(std::uint32_t);
                GPUMesh mesh{makeBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, vertexBytes), makeBuffer(SDL_GPU_BUFFERUSAGE_INDEX, indexBytes),
                             static_cast<std::uint32_t>(make->indices.size())};
                if (mesh.vertices == nullptr || mesh.indices == nullptr) {
                    tally.lastRefusal = SDLError("SDL_CreateGPUBuffer (mesh)");
                    if (mesh.vertices != nullptr) SDL_ReleaseGPUBuffer(device, mesh.vertices);
                    if (mesh.indices != nullptr) SDL_ReleaseGPUBuffer(device, mesh.indices);
                    ++tally.refusedMeshAsks;
                    continue;
                }
                meshes[make->id.value] = mesh;
                frame.madeMeshes.push_back(make->id.value);
                frame.bufferCopies.push_back({place(make->vertices.data(), vertexBytes), mesh.vertices, vertexBytes});
                frame.bufferCopies.push_back({place(make->indices.data(), indexBytes), mesh.indices, indexBytes});
            } else if (const auto* release = std::get_if<R::MeshRelease>(&ask)) {
                const auto it = meshes.find(release->id.value);
                if (it == meshes.end()) continue;
                frame.releasedBuffers.push_back(it->second.vertices);
                frame.releasedBuffers.push_back(it->second.indices);
                meshes.erase(it);
            }
        }
    }

    // The picture a pass draws on: the main target's (target: none) or a render target's.
    struct Target {
        SDL_GPUTexture* texture = nullptr;
        SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
        R::TargetSize size{};
        Depth* depth = nullptr;
        GPUTarget* target = nullptr;
    };

    // This frame's target of each pass, resolved once after the asks (render-pipeline declaration section 13): the
    // main target when there is a main picture (none in a skipped frame: not drawn, not counted), or a render target
    // with a texture; a pass on any other id (never made, let go, refused) is counted as unmapped here, once, and has
    // none. PlaceGeometry and RecordPasses both go by this table.
    std::vector<std::optional<Target>> ResolveTargets(const R::RenderCommands& commands, const std::optional<Target>& main, FrameTally& tally) {
        std::vector<std::optional<Target>> resolved;
        resolved.reserve(commands.passes.size());
        for (const R::RenderPass& pass : commands.passes) {
            const std::optional<R::TextureId> id = R::TargetOf(pass);
            if (!id) {
                resolved.push_back(main);
                continue;
            }
            const auto it = targets.find(id->value);
            if (it == targets.end() || it->second.texture == nullptr) {
                ++tally.unmappedPasses;
                resolved.emplace_back();
                continue;
            }
            GPUTarget& t = it->second;
            resolved.push_back(Target{t.texture, t.format, t.size, &t.depth, &t});
        }
        return resolved;
    }

    // Places the vertices, indices and instances of the passes that have a target this frame (2D and 3D, in their
    // order) after the asks' bytes.
    void PlaceGeometry(const R::RenderCommands& commands, const std::vector<std::optional<Target>>& resolved, Frame& frame,
                       std::vector<Upload>& geometry) {
        std::uint64_t vertexBytes = 0;
        std::uint64_t indexBytes = 0;
        std::uint64_t instanceBytes = 0;
        for (std::size_t p = 0; p < commands.passes.size(); ++p) {
            if (!resolved[p]) continue;
            const PassGeometry g = GeometryOf(commands.passes[p]);
            vertexBytes += g.vertexBytes;
            indexBytes += g.indexBytes;
            instanceBytes += g.instanceBytes;
        }
        frame.total = AlignUp(frame.total);
        frame.vertexOffset = frame.total;
        frame.vertexBytes = vertexBytes;
        frame.total += vertexBytes;
        frame.total = AlignUp(frame.total);
        frame.indexOffset = frame.total;
        frame.indexBytes = indexBytes;
        frame.total += indexBytes;
        frame.total = AlignUp(frame.total);
        frame.instanceOffset = frame.total;
        frame.instanceBytes = instanceBytes;
        frame.total += instanceBytes;
        std::uint64_t v = frame.vertexOffset;
        std::uint64_t i = frame.indexOffset;
        std::uint64_t n = frame.instanceOffset;
        for (std::size_t p = 0; p < commands.passes.size(); ++p) {
            if (!resolved[p]) continue;
            const PassGeometry g = GeometryOf(commands.passes[p]);
            geometry.push_back(Upload{v, g.vertices, g.vertexBytes});
            geometry.push_back(Upload{i, g.indices, g.indexBytes});
            geometry.push_back(Upload{n, g.instances, g.instanceBytes});
            v += g.vertexBytes;
            i += g.indexBytes;
            n += g.instanceBytes;
        }
    }

    void ReleaseLater(Frame& frame) {
        for (SDL_GPUTexture* texture : frame.released) SDL_ReleaseGPUTexture(device, texture);
        frame.released.clear();
        for (SDL_GPUBuffer* buffer : frame.releasedBuffers) SDL_ReleaseGPUBuffer(device, buffer);
        frame.releasedBuffers.clear();
    }

    // The frame's uploads failed: the textures and meshes made this frame have no contents, so they go again (their
    // asks counted as refused; draws naming them are skipped).
    void Abandon(Frame& frame, FrameTally& tally) {
        for (const std::uint32_t id : frame.madeMeshes) {
            const auto it = meshes.find(id);
            if (it == meshes.end()) continue;
            SDL_ReleaseGPUBuffer(device, it->second.vertices);
            SDL_ReleaseGPUBuffer(device, it->second.indices);
            meshes.erase(it);
            ++tally.refusedMeshAsks;
        }
        for (const std::uint32_t id : frame.made) {
            const auto it = textures.find(id);
            if (it == textures.end()) continue;
            SDL_ReleaseGPUTexture(device, it->second.texture);
            textures.erase(it);
            ++tally.refusedAsks;
        }
        ReleaseLater(frame);
    }

    // Records the copy pass of the frame (the asks already applied to frame; the geometry of the passes resolved has).
    // false: SDL refused a step, or the uploads are too large (error says which).
    bool RecordUploads(SDL_GPUCommandBuffer* cmd, Frame& frame, const R::RenderCommands& commands,
                       const std::vector<std::optional<Target>>& resolved, FrameTally& tally, std::string& error) {
        std::vector<Upload> geometry;
        PlaceGeometry(commands, resolved, frame, geometry);
        if (frame.total == 0) {
            ReleaseLater(frame);
            return true;
        }
        const auto fail = [&](std::string text) {
            error = std::move(text);
            Abandon(frame, tally);
            return false;
        };
        if (frame.total > kMaxFrameBytes) return fail("this frame's uploads exceed what one transfer buffer holds (4 GiB)");
        if (!EnsureTransfer(frame.total, error)) return fail(error);
        if (frame.vertexBytes > 0 && !EnsureBuffer(vertices, verticesSize, frame.vertexBytes, SDL_GPU_BUFFERUSAGE_VERTEX, error)) return fail(error);
        if (frame.indexBytes > 0 && !EnsureBuffer(indices, indicesSize, frame.indexBytes, SDL_GPU_BUFFERUSAGE_INDEX, error)) return fail(error);
        if (frame.instanceBytes > 0 && !EnsureBuffer(instances, instancesSize, frame.instanceBytes, SDL_GPU_BUFFERUSAGE_VERTEX, error)) {
            return fail(error);
        }
        auto* mapped = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device, transfer, true));
        if (mapped == nullptr) return fail(SDLError("SDL_MapGPUTransferBuffer"));
        for (const Upload& u : frame.uploads) std::memcpy(mapped + u.offset, u.bytes, u.size);
        for (const Upload& u : geometry) {
            if (u.size > 0) std::memcpy(mapped + u.offset, u.bytes, u.size);
        }
        SDL_UnmapGPUTransferBuffer(device, transfer);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
        for (const Frame::TextureCopy& c : frame.textureCopies) {
            SDL_GPUTextureTransferInfo source{};
            source.transfer_buffer = transfer;
            source.offset = static_cast<std::uint32_t>(c.offset);
            source.pixels_per_row = c.region.width;
            source.rows_per_layer = c.region.height;
            SDL_GPUTextureRegion destination{};
            destination.texture = c.texture;
            destination.x = c.region.x;
            destination.y = c.region.y;
            destination.w = c.region.width;
            destination.h = c.region.height;
            destination.d = 1;
            SDL_UploadToGPUTexture(copy, &source, &destination, false);
        }
        for (const Frame::BufferCopy& c : frame.bufferCopies) {
            SDL_GPUTransferBufferLocation source{transfer, static_cast<std::uint32_t>(c.offset)};
            SDL_GPUBufferRegion destination{c.buffer, 0, static_cast<std::uint32_t>(c.size)};
            SDL_UploadToGPUBuffer(copy, &source, &destination, false);
        }
        if (frame.vertexBytes > 0) {
            SDL_GPUTransferBufferLocation source{transfer, static_cast<std::uint32_t>(frame.vertexOffset)};
            SDL_GPUBufferRegion destination{vertices, 0, static_cast<std::uint32_t>(frame.vertexBytes)};
            SDL_UploadToGPUBuffer(copy, &source, &destination, true);
        }
        if (frame.indexBytes > 0) {
            SDL_GPUTransferBufferLocation source{transfer, static_cast<std::uint32_t>(frame.indexOffset)};
            SDL_GPUBufferRegion destination{indices, 0, static_cast<std::uint32_t>(frame.indexBytes)};
            SDL_UploadToGPUBuffer(copy, &source, &destination, true);
        }
        if (frame.instanceBytes > 0) {
            SDL_GPUTransferBufferLocation source{transfer, static_cast<std::uint32_t>(frame.instanceOffset)};
            SDL_GPUBufferRegion destination{instances, 0, static_cast<std::uint32_t>(frame.instanceBytes)};
            SDL_UploadToGPUBuffer(copy, &source, &destination, true);
        }
        SDL_EndGPUCopyPass(copy);
        ReleaseLater(frame);
        return true;
    }

    // The frame just recorded was handed to the GPU at submitted with this fence: its GPU time is known once the fence
    // is seen signaled (LookAtFences).
    void Track(SDL_GPUFence* fence, const std::chrono::steady_clock::time_point submitted) {
        inFlight.push_back(InFlight{timing.frame, fence, submitted});
    }

    // Looks at the fences of the frames handed to the GPU, without waiting: each one signaled gives its frame's GPU time
    // (from its submission to now: an upper bound, the fence has no time of its own) and is let go. The latest frame
    // seen done is kept.
    void LookAtFences() {
        const auto now = std::chrono::steady_clock::now();
        std::erase_if(inFlight, [&](const InFlight& f) {
            if (!SDL_QueryGPUFence(device, f.fence)) return false;
            if (!gpuTime || gpuTime->frame < f.frame) {
                gpuTime = GPUFrameTime{f.frame, std::chrono::duration_cast<std::chrono::nanoseconds>(now - f.submitted)};
            }
            SDL_ReleaseGPUFence(device, f.fence);
            return true;
        });
    }

    // A pass begins drawing on a render target: true when its picture is undefined (no picture, not drawn on yet this
    // frame), so the pass clears it instead of keeping it (colour: its clear or transparent black; depth: 1).
    bool BeginWrite(const GPUTarget& target) { return frameDraws.Begin(target.id, target.picture); }

    // The frame was handed to the GPU: the targets its passes drew on have a picture from now on, and go in the receipt
    // (render-pipeline declaration 14.13, 14.14). A frame not handed (cancelled, refused) leaves them as they were.
    void Commit(R::RenderFrameReceipt& receipt) {
        for (const std::uint32_t id : frameDraws.Commit(receipt)) {
            if (const auto it = targets.find(id); it != targets.end()) it->second.picture = true;
        }
    }

    // How a pass's colour starts on its target: cleared to its clear colour (or transparent black) when it gives one or
    // the target's picture is undefined (the main target's first pass of the frame, a render target made or made again
    // since it was last drawn on); else as the target is (Bevy's LoadOp::Load, texture_attachment.rs:52). Marks the
    // target begun.
    SDL_GPUColorTargetInfo StartColour(const Target& on, const std::optional<R::Color8>& clear, bool& mainBegun) {
        bool undefined = false;
        if (on.target != nullptr) {
            undefined = BeginWrite(*on.target);
        } else {
            undefined = !mainBegun;
            mainBegun = true;
        }
        SDL_GPUColorTargetInfo colour{};
        colour.texture = on.texture;
        colour.store_op = SDL_GPU_STOREOP_STORE;
        const R::Color8 c = clear.value_or(R::Color8{0, 0, 0, 0});
        colour.clear_color = SDL_FColor{c.r / 255.0F, c.g / 255.0F, c.b / 255.0F, c.a / 255.0F};
        colour.load_op = (undefined || clear) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        return colour;
    }

    // Records one render pass per pass that has a target this frame (resolved, ResolveTargets), in their order (RP-3:
    // the GPU's order is RenderCommands'; SDL_GPU keeps what one pass writes and a later one reads in that order). The
    // main target (main; none in a skipped frame) with no pass gets one pass that clears it. Each pass recorded is in its
    // debug group and its recording timed (timing, by the pass's place in commands).
    void RecordPasses(SDL_GPUCommandBuffer* cmd, const R::RenderCommands& commands, const std::vector<std::optional<Target>>& resolved,
                      const std::optional<Target>& main, FrameTally& tally) {
        bool mainBegun = false;
        std::uint32_t vertexBase = 0;
        std::uint32_t indexBase = 0;
        std::uint32_t instanceBase = 0;
        for (std::size_t p = 0; p < commands.passes.size(); ++p) {
            if (!resolved[p]) continue;
            const R::RenderPass& item = commands.passes[p];
            const Target& on = *resolved[p];
            const R::PassWrites writes = R::WritesOf(item);
            const bool onDepth = on.target != nullptr && on.target->isDepth;
            const auto requireColour = [&](const bool colour) {
                GYO_ASSERT(onDepth != colour && "a depth-only pass on a Depth target, a pass that draws colour on a colour target");
            };
            PassTiming& passTiming = timing.passes[p];
            const auto began = std::chrono::steady_clock::now();
            const DebugGroup group(cmd, passTiming.name, timing.debugGroups);
            std::visit(Overloaded{
                           [&](const R::PassDepth& pass) {
                               requireColour(false);
                               RecordPassDepth(cmd, on, writes, pass, vertexBase, indexBase, instanceBase, tally);
                           },
                           [&](const R::Pass3D& pass) {
                               requireColour(true);
                               const SDL_GPUColorTargetInfo colour = StartColour(on, std::nullopt, mainBegun);
                               RecordPass3D(cmd, on, writes, colour, pass, vertexBase, indexBase, instanceBase, tally);
                           },
                           [&](const R::PassFullscreen& pass) {
                               requireColour(true);
                               const SDL_GPUColorTargetInfo colour = StartColour(on, std::nullopt, mainBegun);
                               RecordPassFullscreen(cmd, on, writes, colour, pass, tally);
                           },
                           [&](const R::Pass2D& pass) {
                               requireColour(true);
                               const SDL_GPUColorTargetInfo colour = StartColour(on, pass.clear, mainBegun);
                               RecordPass2D(cmd, on, writes, colour, pass, vertexBase, indexBase, tally);
                           },
                       },
                       item);
            passTiming.recorded = true;
            passTiming.cpu = std::chrono::steady_clock::now() - began;
            const PassGeometry g = GeometryOf(item);
            vertexBase += static_cast<std::uint32_t>(g.vertexBytes);
            indexBase += static_cast<std::uint32_t>(g.indexBytes);
            instanceBase += static_cast<std::uint32_t>(g.instanceBytes);
        }
        if (main && !mainBegun) {
            const SDL_GPUColorTargetInfo clear = StartColour(*main, std::nullopt, mainBegun);
            SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &clear, 1, nullptr));
        }
    }

    // One frame on a command buffer: the asks applied, the render targets sized for main's size (main: the main picture,
    // none in a skipped frame: the render targets are still drawn), the targets resolved, the copy pass, the passes.
    // false: the uploads failed (error says why); the main picture then gets one pass that clears it, and no pass of
    // commands is recorded. The frame is counted and its timing starts here (one per pass of commands, none recorded);
    // the fences of the frames before it are looked at first.
    bool DrawFrame(SDL_GPUCommandBuffer* cmd, const std::span<const R::TextureOp> asks, const std::span<const R::MeshOp> meshAsks,
                   const R::RenderCommands& commands, const std::optional<Target>& main, FrameTally& tally, std::string& error,
                   R::RenderFrameReceipt& receipt) {
        LookAtFences();
        timing = TimingOf(++framesRecorded, commands);
        Frame frame;
        frameDraws.Clear();
        ApplyAsks(asks, frame, tally, receipt);
        ApplyMeshAsks(meshAsks, frame, tally);
        const std::vector<std::optional<Target>> resolved = ResolveTargets(commands, main, tally);
        if (!RecordUploads(cmd, frame, commands, resolved, tally, error)) {
            RecordPasses(cmd, R::RenderCommands{}, {}, main, tally);
            return false;
        }
        RecordPasses(cmd, commands, resolved, main, tally);
        return true;
    }

    // A draw that samples the target its pass draws on reads and writes one texture at once (undefined on the GPU):
    // skipped and counted.
    // A draw that samples what its pass draws on (its colour target or its depth target) reads and writes one texture
    // at once.
    static bool SamplesOwnTarget(const R::PassWrites& writes, const R::TextureId texture) noexcept { return writes.Writes(texture); }


    // Records one Pass2D on its target (colour: how it starts). vertexBase, indexBase: where its vertices and indices
    // are in the frame's buffers.
    void RecordPass2D(SDL_GPUCommandBuffer* cmd, const Target& on, const R::PassWrites& writes, const SDL_GPUColorTargetInfo& colour,
                      const R::Pass2D& pass,
                      const std::uint32_t vertexBase, const std::uint32_t indexBase, FrameTally& tally) {
        const SDL_GPUTextureFormat format = on.format;
        const R::TargetSize size = on.size;
        SDL_GPURenderPass* render = SDL_BeginGPURenderPass(cmd, &colour, 1, nullptr);
        // The clear above covers the whole target; the draws land in the pass's viewport.
        const R::PixelRect area = R::ViewportOf(pass, size);
        const bool drawable = area.width != 0 && area.height != 0;
        const SDL_GPUViewport viewport{static_cast<float>(area.x), static_cast<float>(area.y), static_cast<float>(area.width),
                                       static_cast<float>(area.height), 0.0F, 1.0F};
        if (drawable) SDL_SetGPUViewport(render, &viewport);
        const Engine::Math::Rect view = drawable ? R::ViewOf(pass, size) : Engine::Math::Rect{};
        if (drawable) {
            const Abi::Pass2DViewUniform uniform = Abi::ToUniform(R::ViewToClip(view));
            SDL_PushGPUVertexUniformData(cmd, Abi::kPass2DViewUniformSlot, &uniform, sizeof(uniform));
        }
        // A pass with no vertices or no indices has nothing a draw could name.
        const bool bound = drawable && !pass.vertices.empty() && !pass.indices.empty() && vertices != nullptr && indices != nullptr;
        if (bound) {
            const SDL_GPUBufferBinding vb{vertices, vertexBase};
            SDL_BindGPUVertexBuffers(render, 0, &vb, 1);
            const SDL_GPUBufferBinding ib{indices, indexBase};
            SDL_BindGPUIndexBuffer(render, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        }
        // Binds only what changes from one draw to the next (a pass of many small draws, such as ImGui's).
        SDL_GPUGraphicsPipeline* boundPipeline = nullptr;
        SDL_GPUTextureSamplerBinding boundTexture{};
        for (const R::Draw2D& draw : pass.draws) {
            GYO_ASSERT(draw.indexOffset <= pass.indices.size() && draw.indexCount <= pass.indices.size() - draw.indexOffset &&
                       "a draw's indices outside its pass");
            if (draw.indexCount == 0 || !bound) continue;
            const std::optional<Sampled> t = SampledOf(draw.texture);
            if (!t || SamplesOwnTarget(writes, draw.texture)) {
                ++tally.skippedDraws;
                continue;
            }
            const R::PixelRect scissor = R::ClipToPixels(draw.clip, view, area);
            if (scissor.width == 0 || scissor.height == 0) continue;
            SDL_GPUGraphicsPipeline* pipeline = PipelineFor(format, draw.blend, tally.lastRefusal);
            if (pipeline == nullptr) {
                ++tally.skippedDraws;
                continue;
            }
            if (pipeline != boundPipeline) {
                SDL_BindGPUGraphicsPipeline(render, pipeline);
                boundPipeline = pipeline;
            }
            const SDL_Rect rect{static_cast<int>(scissor.x), static_cast<int>(scissor.y), static_cast<int>(scissor.width),
                                static_cast<int>(scissor.height)};
            SDL_SetGPUScissor(render, &rect);
            const SDL_GPUTextureSamplerBinding binding{t->texture, t->filter == R::TextureFilter::Nearest ? nearest : linear};
            if (binding.texture != boundTexture.texture || binding.sampler != boundTexture.sampler) {
                SDL_BindGPUFragmentSamplers(render, Abi::kPass2DTextureSlot, &binding, 1);
                boundTexture = binding;
            }
            SDL_DrawGPUIndexedPrimitives(render, draw.indexCount, 1, draw.indexOffset, static_cast<std::int32_t>(draw.vertexOffset), 0);
        }
        SDL_EndGPURenderPass(render);
    }

    // Records one Pass3D on its target: its depth (Pass3D::depth, or the target's own) cleared to 1 unless the pass
    // keeps it (or a Depth target has no picture yet), its colour kept as the target is (colour: cleared to
    // transparent black when it is the target's first pass of the frame, as a Pass2D without a clear). vertexBase,
    // indexBase, instanceBase: where the pass's own vertices and indices and its instances are in the frame's buffers.
    void RecordPass3D(SDL_GPUCommandBuffer* cmd, const Target& on, const R::PassWrites& writes, const SDL_GPUColorTargetInfo& colour,
                      const R::Pass3D& pass, const std::uint32_t vertexBase, const std::uint32_t indexBase, const std::uint32_t instanceBase,
                      FrameTally& tally) {
        SDL_GPUTexture* depth = nullptr;
        bool undefined = false;
        if (pass.depth) {
            const auto it = targets.find(pass.depth->value);
            if (it == targets.end() || it->second.texture == nullptr) {
                // No depth to draw with (let go, never made, refused): as below.
                tally.skippedDraws += pass.draws.size();
                SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &colour, 1, nullptr));
                return;
            }
            GPUTarget& t = it->second;
            GYO_ASSERT(t.isDepth && "a 3D pass's depth is a Depth target");
            if (t.size != on.size) {
                // On the data side the two are one size (the backend's caller asserts it, FirstDepthOfAnotherSize);
                // here the main target is the window's picture, whose size may differ from the data side's for a
                // frame (a window being dragged): the draws are skipped and counted, the colour as the rule says.
                tally.skippedDraws += pass.draws.size();
                SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &colour, 1, nullptr));
                return;
            }
            depth = t.texture;
            undefined = BeginWrite(t);
        } else if (EnsureDepth(*on.depth, on.size, tally.lastRefusal)) {
            depth = on.depth->texture;
        } else {
            // No depth to draw with: the pass's draws are skipped; the colour is still what the rule above says.
            tally.skippedDraws += pass.draws.size();
            SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &colour, 1, nullptr));
            return;
        }
        RecordMeshes(cmd, on, writes, &colour, depth, pass.clearDepth || undefined, pass, std::span<const R::Draw3D>(pass.draws), vertexBase,
                     indexBase, instanceBase, tally);
    }

    // Records one PassDepth on its Depth target (on): cleared to 1 unless the pass keeps it (or it has no picture
    // yet), no colour.
    void RecordPassDepth(SDL_GPUCommandBuffer* cmd, const Target& on, const R::PassWrites& writes, const R::PassDepth& pass,
                         const std::uint32_t vertexBase, const std::uint32_t indexBase, const std::uint32_t instanceBase, FrameTally& tally) {
        const bool undefined = BeginWrite(*on.target);
        RecordMeshes(cmd, on, writes, nullptr, on.texture, pass.clearDepth || undefined, pass, std::span<const R::MeshDraw>(pass.draws),
                     vertexBase, indexBase, instanceBase, tally);
    }

    // Records the draws of a pass that draws meshes, geometry its viewport, view, own geometry and instances: a 3D
    // pass's (Draw3D; colour: how its colour target starts, each draw's texture bound and its blend in its pipeline) or a
    // depth-only pass's (MeshDraw; colour: none). depth is the depth target, cleared to 1 when clearDepth, else as it
    // is (and stored, so a later pass may keep it or sample it).
    template <class Draw>
    void RecordMeshes(SDL_GPUCommandBuffer* cmd, const Target& on, const R::PassWrites& writes, const SDL_GPUColorTargetInfo* colour,
                      SDL_GPUTexture* depth, const bool clearDepth, const R::MeshGeometry& pass, const std::span<const Draw> draws,
                      const std::uint32_t vertexBase, const std::uint32_t indexBase, const std::uint32_t instanceBase, FrameTally& tally) {
        constexpr bool kColour = std::is_same_v<Draw, R::Draw3D>;
        GYO_ASSERT((colour != nullptr) == kColour && "a 3D pass's draws on its colour, a depth-only pass's on none");
        const R::TargetSize size = on.size;
        SDL_GPUDepthStencilTargetInfo depthTarget{};
        depthTarget.texture = depth;
        depthTarget.clear_depth = 1.0F;
        // Cleared unless the pass keeps the depth of the passes before it (SDL cycles a target only when it is
        // cleared). Stored, so a later pass may keep it.
        depthTarget.load_op = clearDepth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        depthTarget.store_op = SDL_GPU_STOREOP_STORE;
        depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthTarget.cycle = clearDepth;
        SDL_GPURenderPass* render = SDL_BeginGPURenderPass(cmd, colour, kColour ? 1 : 0, &depthTarget);
        const R::PixelRect area = R::ViewportOf(pass, size);
        const bool drawable = area.width != 0 && area.height != 0;
        // Clip space lands on the viewport; clipping keeps every triangle inside it (no scissor needed).
        if (drawable) {
            const SDL_GPUViewport viewport{static_cast<float>(area.x), static_cast<float>(area.y), static_cast<float>(area.width),
                                           static_cast<float>(area.height), 0.0F, 1.0F};
            SDL_SetGPUViewport(render, &viewport);
        }
        // The pass's own geometry, when it has some (draws with mesh {}).
        const bool own = !pass.vertices.empty() && !pass.indices.empty() && vertices != nullptr && indices != nullptr;
        // Every draw's instances are there (a draw of none draws nothing).
        const bool instanced = !pass.instances.empty() && instances != nullptr;
        // One view for the whole pass: each instance's world is applied in the vertex shader.
        const Abi::Mesh3DViewUniform view = Abi::ToUniform(pass.clipFromWorld);
        SDL_PushGPUVertexUniformData(cmd, Abi::kMesh3DViewUniformSlot, &view, sizeof(view));
        // The pass's instances, bound once at the pass's first: a draw names its range by first_instance (as Bevy's
        // draw_indexed(.., batch_range), bevy_pbr/src/render/mesh.rs:4734-4738); instance-rate inputs start there on
        // every backend (Vulkan firstInstance, D3D12 StartInstanceLocation, Metal baseInstance). Only an instance id
        // read in a shader would not (SDL_gpu.h, SDL_DrawGPUIndexedPrimitives); this one reads none.
        if (instanced) {
            const SDL_GPUBufferBinding instanceBinding{instances, instanceBase};
            SDL_BindGPUVertexBuffers(render, Abi::kMesh3DInstanceSlot, &instanceBinding, 1);
        }
        SDL_GPUGraphicsPipeline* boundPipeline = nullptr;
        SDL_GPUBuffer* boundVertices = nullptr;
        SDL_GPUTextureSamplerBinding boundTexture{};
        for (const Draw& draw : draws) {
            GYO_ASSERT(draw.instanceOffset <= pass.instances.size() && draw.instanceCount <= pass.instances.size() - draw.instanceOffset &&
                       "a draw's instances outside its pass");
            if (draw.indexCount == 0 || draw.instanceCount == 0 || !drawable || !instanced) continue;
            SDL_GPUBufferBinding vb{};
            SDL_GPUBufferBinding ib{};
            if (draw.mesh.value == 0) {
                GYO_ASSERT(draw.indexOffset <= pass.indices.size() && draw.indexCount <= pass.indices.size() - draw.indexOffset &&
                           "a draw's indices outside its pass");
                if (!own) continue;
                vb = SDL_GPUBufferBinding{vertices, vertexBase};
                ib = SDL_GPUBufferBinding{indices, indexBase};
            } else {
                const auto it = meshes.find(draw.mesh.value);
                if (it == meshes.end()) {
                    ++tally.skippedDraws;
                    continue;
                }
                GYO_ASSERT(draw.indexOffset <= it->second.indexCount && draw.indexCount <= it->second.indexCount - draw.indexOffset &&
                           "a draw's indices outside its mesh");
                vb = SDL_GPUBufferBinding{it->second.vertices, 0};
                ib = SDL_GPUBufferBinding{it->second.indices, 0};
            }
            std::optional<SDL_GPUTextureSamplerBinding> binding;
            std::optional<MeshColour> output;
            if constexpr (kColour) {
                const std::optional<Sampled> tex = SampledOf(draw.texture);
                if (!tex || SamplesOwnTarget(writes, draw.texture)) {
                    ++tally.skippedDraws;
                    continue;
                }
                binding = SDL_GPUTextureSamplerBinding{tex->texture, tex->filter == R::TextureFilter::Nearest ? nearest : linear};
                output = MeshColour{on.format, draw.blend};
            }
            SDL_GPUGraphicsPipeline* pipeline = Pipeline3DFor(output, draw, tally.lastRefusal);
            if (pipeline == nullptr) {
                ++tally.skippedDraws;
                continue;
            }
            if (pipeline != boundPipeline) {
                SDL_BindGPUGraphicsPipeline(render, pipeline);
                boundPipeline = pipeline;
            }
            // The pass's own geometry and each mesh are in buffers of their own: bound when the draw names other ones.
            if (vb.buffer != boundVertices) {
                SDL_BindGPUVertexBuffers(render, Abi::kMesh3DVertexSlot, &vb, 1);
                SDL_BindGPUIndexBuffer(render, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                boundVertices = vb.buffer;
            }
            if (binding && (binding->texture != boundTexture.texture || binding->sampler != boundTexture.sampler)) {
                SDL_BindGPUFragmentSamplers(render, Abi::kMesh3DTextureSlot, &*binding, 1);
                boundTexture = *binding;
            }
            const auto firstVertex = static_cast<std::int32_t>(draw.vertexOffset);
            SDL_DrawGPUIndexedPrimitives(render, draw.indexCount, draw.instanceCount, draw.indexOffset, firstVertex, draw.instanceOffset);
        }
        SDL_EndGPURenderPass(render);
    }

    // Records one PassFullscreen on its target (colour: how it starts): one triangle over its viewport with its
    // fragment shader, its inputs bound in order and its parameters pushed. Nothing is drawn, and the pass is counted
    // as a skipped draw, when the device carries no shader of its name, an input has no GPU texture or is the pass's own
    // target, or SDL refuses the pipeline.
    void RecordPassFullscreen(SDL_GPUCommandBuffer* cmd, const Target& on, const R::PassWrites& writes, const SDL_GPUColorTargetInfo& colour,
                              const R::PassFullscreen& pass, FrameTally& tally) {
        SDL_GPURenderPass* render = SDL_BeginGPURenderPass(cmd, &colour, 1, nullptr);
        const R::PixelRect area = R::ViewportOf(pass, on.size);
        const EmbeddedShader* shader = FindShader(pass.shader);
        const auto skip = [&](std::string why) {
            ++tally.skippedDraws;
            if (!why.empty()) tally.lastRefusal = std::move(why);
            SDL_EndGPURenderPass(render);
        };
        if (area.width == 0 || area.height == 0) {
            SDL_EndGPURenderPass(render);
            return;
        }
        if (shader == nullptr) return skip("no shader named " + pass.shader + " for a full-screen pass");
        GYO_ASSERT(shader->stage == ShaderStage::Fragment && "a full-screen pass's shader is a fragment shader");
        GYO_ASSERT(pass.inputs.size() == shader->resources.samplers && "a full-screen pass has as many inputs as its shader samples");
        GYO_ASSERT(pass.parameters.empty() == (shader->resources.uniformBuffers == 0) &&
                   shader->resources.uniformBuffers <= Abi::kFullscreenFragmentUniformBuffersAtMost &&
                   "a full-screen pass has parameters when (and only when) its shader declares them");
        GYO_ASSERT(pass.parameters.size() % 16 == 0 && "a full-screen pass's parameters are float4 lanes (a multiple of 16 bytes)");
        std::vector<SDL_GPUTextureSamplerBinding> bindings;
        for (const R::TextureId input : pass.inputs) {
            const std::optional<Sampled> in = SampledOf(input);
            if (!in || SamplesOwnTarget(writes, input)) return skip({});
            bindings.push_back(SDL_GPUTextureSamplerBinding{in->texture, in->filter == R::TextureFilter::Nearest ? nearest : linear});
        }
        SDL_GPUGraphicsPipeline* pipeline = PipelineFullscreenFor(*shader, on.format, tally.lastRefusal);
        if (pipeline == nullptr) return skip({});
        const SDL_GPUViewport viewport{static_cast<float>(area.x), static_cast<float>(area.y), static_cast<float>(area.width),
                                       static_cast<float>(area.height), 0.0F, 1.0F};
        SDL_SetGPUViewport(render, &viewport);
        SDL_BindGPUGraphicsPipeline(render, pipeline);
        if (!bindings.empty()) {
            SDL_BindGPUFragmentSamplers(render, Abi::kFullscreenFirstInputSlot, bindings.data(), static_cast<std::uint32_t>(bindings.size()));
        }
        if (!pass.parameters.empty()) {
            SDL_PushGPUFragmentUniformData(cmd, Abi::kFullscreenParametersSlot, pass.parameters.data(),
                                           static_cast<std::uint32_t>(pass.parameters.size()));
        }
        SDL_DrawGPUPrimitives(render, Abi::kFullscreenVertices, 1, 0, 0);
        SDL_EndGPURenderPass(render);
    }
};

Engine::Base::Result<std::unique_ptr<GPUDevice>, DeviceFailure> GPUDevice::Create(SDL::NativeWindow* window, const bool debug,
                                                                                const std::span<const EmbeddedShader> more) {
    RequireMainThread();
    using Engine::Base::Err;
    auto state = std::make_unique<State>();
    // SDL's global state (sdl.zh-Hant.md section 2): os owns the signals; the hint is read when SDL's events start.
    static_cast<void>(SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"));
    state->videoStarted = SDL_InitSubSystem(SDL_INIT_VIDEO);
    std::string startError = state->videoStarted ? std::string() : SDLError("SDL_InitSubSystem(VIDEO)");
    static_cast<void>(SDL_ResetHint(SDL_HINT_NO_SIGNAL_HANDLERS));
    if (!state->videoStarted) return Err(DeviceFailure::Make(DeviceFailureCode::VideoFailed, std::move(startError)));

    // The passes' built-in shaders, by their sources' names (render's ShaderAbi.hpp), and where each goes.
    struct BuiltIn {
        std::string_view source;
        SDL_GPUShader* State::*made;
        const EmbeddedShader* shader = nullptr;
    };
    BuiltIn builtIns[] = {
        {Abi::kPass2DVertexSource, &State::vertex},         {Abi::kPass2DFragmentSource, &State::fragment},
        {Abi::kMesh3DVertexSource, &State::meshVertex},     {Abi::kMesh3DFragmentSource, &State::meshFragment},
        {Abi::kDepthVertexSource, &State::depthVertex},     {Abi::kDepthFragmentSource, &State::depthFragment},
        {Abi::kFullscreenVertexSource, &State::fullscreenVertex},
    };
    SDL_GPUShaderFormat formats = ~SDL_GPUShaderFormat{0};
    for (BuiltIn& builtIn : builtIns) {
        builtIn.shader = FindEmbeddedShader(Abi::ShaderName(builtIn.source));
        GYO_ASSERT(builtIn.shader != nullptr && "the passes' shaders are compiled into sdl_gpu");
        formats &= FormatsOf(*builtIn.shader);
    }
    // The device takes the formats every shader a pass may name is compiled to; a name is one shader's.
    for (std::size_t i = 0; i < more.size(); ++i) {
        GYO_ASSERT(FindEmbeddedShader(more[i].name) == nullptr && "a shader of more named as an embedded one");
        for (std::size_t j = 0; j < i; ++j) GYO_ASSERT(more[j].name != more[i].name && "two shaders of more with one name");
        formats &= FormatsOf(more[i]);
    }
    state->more = more;
    state->device = SDL_CreateGPUDevice(formats, debug, nullptr);
    if (state->device == nullptr) return Err(DeviceFailure::Make(DeviceFailureCode::DeviceFailed, SDLError("SDL_CreateGPUDevice")));
    state->driver = SDL_GetGPUDeviceDriver(state->device);
    if (window != nullptr) {
        auto* sdlWindow = reinterpret_cast<SDL_Window*>(window);
        if (!SDL_ClaimWindowForGPUDevice(state->device, sdlWindow)) {
            return Err(DeviceFailure::Make(DeviceFailureCode::ClaimFailed, SDLError("SDL_ClaimWindowForGPUDevice")));
        }
        state->window = sdlWindow;
    }
    std::string error;
    for (const BuiltIn& builtIn : builtIns) {
        state.get()->*builtIn.made = MakeShader(state->device, *builtIn.shader, error);
        if (state.get()->*builtIn.made == nullptr) return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, std::move(error)));
    }
    for (const SDL_GPUTextureFormat format : kDepthFormats) {
        if (SDL_GPUTextureSupportsFormat(state->device, format, SDL_GPU_TEXTURETYPE_2D, State::kDepthTargetUsage)) {
            state->depthFormat = format;
            break;
        }
    }
    if (state->depthFormat == SDL_GPU_TEXTUREFORMAT_INVALID) {
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed,
                                       "the device supports none of D32_FLOAT, D24_UNORM, D16_UNORM as a sampled depth target"));
    }
    state->linear = MakeSampler(state->device, SDL_GPU_FILTER_LINEAR);
    state->nearest = MakeSampler(state->device, SDL_GPU_FILTER_NEAREST);
    if (state->linear == nullptr || state->nearest == nullptr) {
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SDLError("SDL_CreateGPUSampler")));
    }
    // The white texture (TextureId{}): one opaque white pixel.
    state->white.texture = state->MakeTexture(1, 1, SDL_GPU_TEXTUREUSAGE_SAMPLER, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, error);
    state->white.width = 1;
    state->white.height = 1;
    if (state->white.texture == nullptr) return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, std::move(error)));
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (cmd == nullptr) return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SDLError("SDL_AcquireGPUCommandBuffer")));
    if (!state->EnsureTransfer(4, error)) {
        SDL_CancelGPUCommandBuffer(cmd);
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, std::move(error)));
    }
    auto* mapped = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(state->device, state->transfer, false));
    if (mapped == nullptr) {
        SDL_CancelGPUCommandBuffer(cmd);
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SDLError("SDL_MapGPUTransferBuffer")));
    }
    std::memset(mapped, 255, 4);
    SDL_UnmapGPUTransferBuffer(state->device, state->transfer);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = state->transfer;
    SDL_GPUTextureRegion destination{};
    destination.texture = state->white.texture;
    destination.w = 1;
    destination.h = 1;
    destination.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SDLError("SDL_SubmitGPUCommandBuffer")));
    }
    return std::unique_ptr<GPUDevice>(new GPUDevice(std::move(state)));
}

GPUDevice::GPUDevice(std::unique_ptr<State> state) : state_(std::move(state)) {}

GPUDevice::~GPUDevice() {
    RequireMainThread();
    state_.reset();
}

FrameOutcome GPUDevice::Submit(const std::span<const R::TextureOp> asks, const std::span<const R::MeshOp> meshAsks,
                               const R::RenderCommands& commands, FrameTally& tally, R::RenderFrameReceipt& receipt) {
    RequireMainThread();
    GYO_ASSERT(state_->window != nullptr && "Submit needs the window (a device without one only Captures)");
    // The frames before it, seen done before the swapchain's wait (and again after it, in DrawFrame).
    state_->LookAtFences();
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state_->device);
    if (cmd == nullptr) {
        lastError_ = SDLError("SDL_AcquireGPUCommandBuffer");
        return FrameOutcome::Failed;
    }
    // The window's picture first: the render targets are drawn whether the window gives one (a minimized window does
    // not) or not.
    SDL_GPUTexture* swapchain = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    const bool acquired = SDL_WaitAndAcquireGPUSwapchainTexture(cmd, state_->window, &swapchain, &width, &height);
    if (!acquired) lastError_ = SDLError("SDL_WaitAndAcquireGPUSwapchainTexture");
    std::optional<State::Target> main;
    if (acquired && swapchain != nullptr) {
        main = State::Target{swapchain, SDL_GetGPUSwapchainTextureFormat(state_->device, state_->window), R::TargetSize{width, height},
                             &state_->depth, nullptr};
    }
    std::string error;
    const bool uploaded = state_->DrawFrame(cmd, asks, meshAsks, commands, main, tally, error, receipt);
    if (!uploaded) lastError_ = std::move(error);
    // With its fence, not waited for: a later Submit looks at it (the frame's GPU time, render-pipeline declaration 15.3).
    const auto handed = std::chrono::steady_clock::now();
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    const bool submitted = fence != nullptr;
    if (submitted) {
        state_->Commit(receipt);
        state_->Track(fence, handed);
    }
    if (!submitted) {
        lastError_ = SDLError("SDL_SubmitGPUCommandBufferAndAcquireFence");
        return FrameOutcome::Failed;
    }
    if (!acquired || !uploaded) return FrameOutcome::Failed;
    return main ? FrameOutcome::Shown : FrameOutcome::Skipped;
}

Engine::Base::Result<CapturedImage, CaptureFailure> GPUDevice::Capture(const std::span<const R::TextureOp> asks,
                                                                    const std::span<const R::MeshOp> meshAsks,
                                                                    const R::RenderCommands& commands, const R::TargetSize size,
                                                                    FrameTally& tally, R::RenderFrameReceipt& receipt) {
    RequireMainThread();
    const auto Err = [](std::string text) { return Engine::Base::Err(CaptureFailure::Make(CaptureFailureCode::Refused, std::move(text))); };
    GYO_ASSERT(size.width > 0 && size.height > 0 && "a capture of zero size");
    SDL_GPUDevice* device = state_->device;
    std::string made;
    SDL_GPUTexture* target =
        state_->MakeTexture(size.width, size.height, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, made);
    if (target == nullptr) return Err(std::move(made));
    const std::uint32_t bytes = size.width * size.height * 4;
    SDL_GPUTransferBufferCreateInfo downloadInfo{};
    downloadInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    downloadInfo.size = bytes;
    SDL_GPUTransferBuffer* download = SDL_CreateGPUTransferBuffer(device, &downloadInfo);
    const auto cleanup = [&] {
        if (download != nullptr) SDL_ReleaseGPUTransferBuffer(device, download);
        SDL_ReleaseGPUTexture(device, target);
    };
    if (download == nullptr) {
        cleanup();
        return Err(SDLError("SDL_CreateGPUTransferBuffer"));
    }
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr) {
        cleanup();
        return Err(SDLError("SDL_AcquireGPUCommandBuffer"));
    }
    // The picture stands for the main target this frame.
    std::string error;
    const State::Target main{target, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, size, &state_->depth, nullptr};
    if (!state_->DrawFrame(cmd, asks, meshAsks, commands, main, tally, error, receipt)) {
        SDL_CancelGPUCommandBuffer(cmd);
        cleanup();
        return Err(std::move(error));
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion source{};
    source.texture = target;
    source.w = size.width;
    source.h = size.height;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = download;
    destination.pixels_per_row = size.width;
    destination.rows_per_layer = size.height;
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);
    const auto handed = std::chrono::steady_clock::now();
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence != nullptr) state_->Commit(receipt);
    if (fence == nullptr) {
        cleanup();
        return Err(SDLError("SDL_SubmitGPUCommandBufferAndAcquireFence"));
    }
    // The fence is the frame's (its GPU time): looked at, and let go, once waited for.
    state_->Track(fence, handed);
    const bool waited = SDL_WaitForGPUFences(device, true, &fence, 1);
    state_->LookAtFences();
    if (!waited) {
        cleanup();
        return Err(SDLError("SDL_WaitForGPUFences"));
    }
    const auto* mapped = static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device, download, false));
    if (mapped == nullptr) {
        cleanup();
        return Err(SDLError("SDL_MapGPUTransferBuffer"));
    }
    CapturedImage image{size, std::vector<std::uint8_t>(mapped, mapped + bytes)};
    SDL_UnmapGPUTransferBuffer(device, download);
    cleanup();
    return image;
}

namespace {

// The composition SDL_ClaimWindowForGPUDevice gives the window (SDR: render's 2D pass blends bytes as stored); setting
// the present mode keeps it. HDR changes both.
constexpr SDL_GPUSwapchainComposition kSwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

SDL_GPUPresentMode SDLPresentMode(const SwapchainPresentMode mode) noexcept {
    switch (mode) {
    case SwapchainPresentMode::Vsync: return SDL_GPU_PRESENTMODE_VSYNC;
    case SwapchainPresentMode::Immediate: return SDL_GPU_PRESENTMODE_IMMEDIATE;
    case SwapchainPresentMode::Mailbox: return SDL_GPU_PRESENTMODE_MAILBOX;
    }
    return SDL_GPU_PRESENTMODE_VSYNC;
}

} // namespace

bool GPUDevice::SetPresentMode(const R::PresentMode mode) {
    RequireMainThread();
    if (state_->window == nullptr) return true;
    SDL_GPUDevice* device = state_->device;
    SDL_Window* window = state_->window;
    const SwapchainPresentMode chosen =
        ChoosePresentMode(mode, SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_IMMEDIATE),
                          SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_MAILBOX));
    if (!SDL_SetGPUSwapchainParameters(device, window, kSwapchainComposition, SDLPresentMode(chosen))) {
        lastError_ = SDLError("SDL_SetGPUSwapchainParameters");
        return false;
    }
    state_->presentMode = chosen;
    return true;
}

SwapchainPresentMode GPUDevice::ActivePresentMode() const noexcept { return state_->presentMode; }

void GPUDevice::ForgetTextures() {
    RequireMainThread();
    for (auto& [id, t] : state_->textures) SDL_ReleaseGPUTexture(state_->device, t.texture);
    state_->textures.clear();
    for (auto& [id, t] : state_->targets) state_->ReleaseTarget(t);
    state_->targets.clear();
}

void GPUDevice::ForgetMeshes() {
    RequireMainThread();
    state_->ReleaseMeshes();
}

std::size_t GPUDevice::TextureCount() const noexcept {
    // A target has no texture until a frame sizes it (or while SDL refuses it).
    return state_->textures.size() + static_cast<std::size_t>(std::count_if(state_->targets.begin(), state_->targets.end(),
                                                                            [](const auto& entry) { return entry.second.texture != nullptr; }));
}

std::size_t GPUDevice::MeshCount() const noexcept { return state_->meshes.size(); }

const std::string& GPUDevice::DriverName() const noexcept { return state_->driver; }

const FrameTiming& GPUDevice::LastFrameTiming() const noexcept { return state_->timing; }

std::optional<GPUFrameTime> GPUDevice::LastGPUFrameTime() const noexcept { return state_->gpuTime; }

} // namespace GYO::Plugins::SDLGPU
