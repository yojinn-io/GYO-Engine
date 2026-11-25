// sdl_gpu's adapter (Design Invariant SDL-1: with ShaderTool.cpp, the only place of sdl_gpu that includes SDL3/). It
// implements GPUDevice (../../GPUDevice.hpp): the device, the window's swapchain, the textures and meshes keyed by
// render's ids, the passes' pipelines and the frame (uploads in one copy pass, then one render pass per Pass2D or
// Pass3D, in their order). Shapes follow SDL's own GPU examples and Dear ImGui's SDL_GPU backend
// (imgui_impl_sdlgpu3.cpp): one transfer buffer cycled each frame, vertex and index buffers grown to the frame's need,
// the scissor per draw; the 3D pass's depth texture as SDL's examples make one (a depth target cleared by its pass's
// load op, made again when the target's size changes).
#include "../../GPUDevice.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <variant>

#include "engine/base/Assert.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/render/ShaderAbi.hpp"

#include "../../EmbeddedShaders.hpp"
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

struct GPUTexture {
    SDL_GPUTexture* texture = nullptr;
    R::TextureFilter filter = R::TextureFilter::Linear;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct Pipeline {
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    R::BlendMode blend = R::BlendMode::Alpha;
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
};

// A 3D pipeline and its key. depthWrite is false whenever depthTest is (a draw that does not test writes no depth).
struct Pipeline3D {
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    R::BlendMode blend = R::BlendMode::Opaque;
    bool depthTest = true;
    bool depthWrite = true;
    R::CullMode cull = R::CullMode::None;
    SDL_GPUGraphicsPipeline* pipeline = nullptr;
};

// A mesh of render's asks: its vertices and indices in buffers of their own.
struct GPUMesh {
    SDL_GPUBuffer* vertices = nullptr;
    SDL_GPUBuffer* indices = nullptr;
    std::uint32_t indexCount = 0;
};

// The depth formats a 3D pass may use, the first the device supports as a depth target taken (SDL guarantees D16).
constexpr SDL_GPUTextureFormat kDepthFormats[] = {SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                                                  SDL_GPU_TEXTUREFORMAT_D16_UNORM};

// The bytes of a pass's own vertices and indices (Pass2D or Pass3D).
struct PassGeometry {
    const std::uint8_t* vertices = nullptr;
    std::uint64_t vertexBytes = 0;
    const std::uint8_t* indices = nullptr;
    std::uint64_t indexBytes = 0;
};

PassGeometry GeometryOf(const R::RenderPass& pass) {
    return std::visit(
        [](const auto& p) {
            return PassGeometry{reinterpret_cast<const std::uint8_t*>(p.vertices.data()), p.vertices.size() * sizeof(p.vertices[0]),
                                reinterpret_cast<const std::uint8_t*>(p.indices.data()), p.indices.size() * sizeof(std::uint32_t)};
        },
        pass);
}

// One chunk of the frame's uploads, at its offset in the transfer buffer.
struct Upload {
    std::uint64_t offset = 0;
    const std::uint8_t* bytes = nullptr;
    std::uint64_t size = 0;
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
    SDL_GPUSampler* linear = nullptr;
    SDL_GPUSampler* nearest = nullptr;
    GPUTexture white;
    std::unordered_map<std::uint32_t, GPUTexture> textures;
    std::vector<Pipeline> pipelines;
    std::unordered_map<std::uint32_t, GPUMesh> meshes;
    std::vector<Pipeline3D> pipelines3d;
    // The 3D passes' depth texture, for the size of the last target a 3D pass drew on.
    SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUTexture* depth = nullptr;
    R::TargetSize depthSize{};
    SDL_GPUTransferBuffer* transfer = nullptr;
    std::uint32_t transferSize = 0;
    SDL_GPUBuffer* vertices = nullptr;
    std::uint32_t verticesSize = 0;
    SDL_GPUBuffer* indices = nullptr;
    std::uint32_t indicesSize = 0;

    ~State() {
        if (device != nullptr) {
            for (auto& [id, t] : textures) SDL_ReleaseGPUTexture(device, t.texture);
            if (white.texture != nullptr) SDL_ReleaseGPUTexture(device, white.texture);
            for (Pipeline& p : pipelines) SDL_ReleaseGPUGraphicsPipeline(device, p.pipeline);
            for (Pipeline3D& p : pipelines3d) SDL_ReleaseGPUGraphicsPipeline(device, p.pipeline);
            ReleaseMeshes();
            if (depth != nullptr) SDL_ReleaseGPUTexture(device, depth);
            if (linear != nullptr) SDL_ReleaseGPUSampler(device, linear);
            if (nearest != nullptr) SDL_ReleaseGPUSampler(device, nearest);
            if (vertex != nullptr) SDL_ReleaseGPUShader(device, vertex);
            if (fragment != nullptr) SDL_ReleaseGPUShader(device, fragment);
            if (meshVertex != nullptr) SDL_ReleaseGPUShader(device, meshVertex);
            if (meshFragment != nullptr) SDL_ReleaseGPUShader(device, meshFragment);
            if (transfer != nullptr) SDL_ReleaseGPUTransferBuffer(device, transfer);
            if (vertices != nullptr) SDL_ReleaseGPUBuffer(device, vertices);
            if (indices != nullptr) SDL_ReleaseGPUBuffer(device, indices);
            if (window != nullptr) SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyGPUDevice(device);
        }
        if (videoStarted) SDL_QuitSubSystem(SDL_INIT_VIDEO);
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
    SDL_GPUGraphicsPipeline* Pipeline3DFor(const SDL_GPUTextureFormat format, const R::Draw3D& draw, std::string& error) {
        const bool writes = draw.depthTest && draw.depthWrite;
        for (const Pipeline3D& p : pipelines3d) {
            if (p.format == format && p.blend == draw.blend && p.depthTest == draw.depthTest && p.depthWrite == writes &&
                p.cull == draw.cull) {
                return p.pipeline;
            }
        }
        const SDL_GPUColorTargetDescription target = ColorTarget(format, draw.blend);
        SDL_GPUVertexBufferDescription buffer{};
        buffer.slot = 0;
        buffer.pitch = Abi::kMesh3DVertexStride;
        buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        SDL_GPUVertexAttribute attributes[2]{};
        attributes[0] = {Abi::kMesh3DPositionLocation, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, Abi::kMesh3DPositionOffset};
        attributes[1] = {Abi::kMesh3DUvLocation, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, Abi::kMesh3DUvOffset};
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = meshVertex;
        info.fragment_shader = meshFragment;
        info.vertex_input_state.vertex_buffer_descriptions = &buffer;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = attributes;
        info.vertex_input_state.num_vertex_attributes = 2;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = draw.cull == R::CullMode::Back ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
        info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.compare_op = draw.depthTest ? SDL_GPU_COMPAREOP_LESS_OR_EQUAL : SDL_GPU_COMPAREOP_ALWAYS;
        info.depth_stencil_state.enable_depth_write = writes;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        info.target_info.depth_stencil_format = depthFormat;
        info.target_info.has_depth_stencil_target = true;
        SDL_GPUGraphicsPipeline* made = SDL_CreateGPUGraphicsPipeline(device, &info);
        if (made == nullptr) {
            error = SDLError("SDL_CreateGPUGraphicsPipeline");
            return nullptr;
        }
        pipelines3d.push_back(Pipeline3D{format, draw.blend, draw.depthTest, writes, draw.cull, made});
        return made;
    }

    // The depth texture for a target of this size (made again when the size changes). false: SDL refused it.
    bool EnsureDepth(const R::TargetSize size, std::string& error) {
        if (depth != nullptr && depthSize == size) return true;
        if (depth != nullptr) SDL_ReleaseGPUTexture(device, depth);
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = depthFormat;
        info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        info.width = size.width;
        info.height = size.height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        depth = SDL_CreateGPUTexture(device, &info);
        depthSize = depth != nullptr ? size : R::TargetSize{};
        if (depth == nullptr) error = SDLError("SDL_CreateGPUTexture (depth)");
        return depth != nullptr;
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

    SDL_GPUTexture* MakeTexture(const std::uint32_t width, const std::uint32_t height, const SDL_GPUTextureUsageFlags usage) const {
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage = usage;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        return SDL_CreateGPUTexture(device, &info);
    }

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
        std::uint64_t total = 0;
    };

    // Applies the asks to the texture table (makes now, lets go after the copy pass; the pixels go in the copy pass).
    void ApplyAsks(const std::span<const R::TextureOp> asks, Frame& frame, FrameTally& tally) {
        const auto place = [&](const std::vector<std::uint8_t>& pixels) {
            frame.total = AlignUp(frame.total);
            const std::uint64_t offset = frame.total;
            frame.uploads.push_back(Upload{offset, pixels.data(), pixels.size()});
            frame.total += pixels.size();
            return offset;
        };
        for (const R::TextureOp& ask : asks) {
            if (const auto* make = std::get_if<R::TextureCreate>(&ask)) {
                SDL_GPUTexture* texture = MakeTexture(make->desc.width, make->desc.height, SDL_GPU_TEXTUREUSAGE_SAMPLER);
                if (texture == nullptr) {
                    ++tally.refusedAsks;
                    tally.lastRefusal = SDLError("SDL_CreateGPUTexture");
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
                const auto it = textures.find(release->id.value);
                if (it == textures.end()) continue;
                frame.released.push_back(it->second.texture);
                textures.erase(it);
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

    // Places the vertices and indices of the passes on target 0 (2D and 3D, in their order) after the asks' bytes.
    void PlaceGeometry(const R::RenderCommands& commands, Frame& frame, std::vector<Upload>& geometry, FrameTally& tally) {
        std::uint64_t vertexBytes = 0;
        std::uint64_t indexBytes = 0;
        for (const R::RenderPass& pass : commands.passes) {
            if (R::TargetOf(pass) != 0) {
                ++tally.unmappedPasses;
                continue;
            }
            const PassGeometry g = GeometryOf(pass);
            vertexBytes += g.vertexBytes;
            indexBytes += g.indexBytes;
        }
        frame.total = AlignUp(frame.total);
        frame.vertexOffset = frame.total;
        frame.vertexBytes = vertexBytes;
        frame.total += vertexBytes;
        frame.total = AlignUp(frame.total);
        frame.indexOffset = frame.total;
        frame.indexBytes = indexBytes;
        frame.total += indexBytes;
        std::uint64_t v = frame.vertexOffset;
        std::uint64_t i = frame.indexOffset;
        for (const R::RenderPass& pass : commands.passes) {
            if (R::TargetOf(pass) != 0) continue;
            const PassGeometry g = GeometryOf(pass);
            geometry.push_back(Upload{v, g.vertices, g.vertexBytes});
            geometry.push_back(Upload{i, g.indices, g.indexBytes});
            v += g.vertexBytes;
            i += g.indexBytes;
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

    // Records the copy pass of the frame. false: SDL refused a step, or the uploads are too large (error says which).
    bool RecordUploads(SDL_GPUCommandBuffer* cmd, const std::span<const R::TextureOp> asks, const std::span<const R::MeshOp> meshAsks,
                       const R::RenderCommands& commands, FrameTally& tally, std::string& error) {
        Frame frame;
        ApplyAsks(asks, frame, tally);
        ApplyMeshAsks(meshAsks, frame, tally);
        std::vector<Upload> geometry;
        PlaceGeometry(commands, frame, geometry, tally);
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
        SDL_EndGPUCopyPass(copy);
        ReleaseLater(frame);
        return true;
    }

    // Records one render pass per Pass2D or Pass3D on target 0 into texture (format, size), in their order; none: one
    // pass that clears.
    void RecordPasses(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* texture, const SDL_GPUTextureFormat format, const R::TargetSize size,
                      const R::RenderCommands& commands, FrameTally& tally) {
        bool first = true;
        std::uint32_t vertexBase = 0;
        std::uint32_t indexBase = 0;
        const auto begin = [&](const std::optional<R::Color8>& clear) {
            SDL_GPUColorTargetInfo target{};
            target.texture = texture;
            target.store_op = SDL_GPU_STOREOP_STORE;
            const R::Color8 colour = clear.value_or(R::Color8{0, 0, 0, 0});
            target.clear_color = SDL_FColor{colour.r / 255.0F, colour.g / 255.0F, colour.b / 255.0F, colour.a / 255.0F};
            target.load_op = (first || clear) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
            first = false;
            return SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
        };
        for (const R::RenderPass& item : commands.passes) {
            if (const auto* pass3d = std::get_if<R::Pass3D>(&item)) {
                if (pass3d->target != 0) continue;
                RecordPass3D(cmd, texture, format, size, *pass3d, first, vertexBase, indexBase, tally);
                const PassGeometry g = GeometryOf(item);
                vertexBase += static_cast<std::uint32_t>(g.vertexBytes);
                indexBase += static_cast<std::uint32_t>(g.indexBytes);
                continue;
            }
            const R::Pass2D& pass = std::get<R::Pass2D>(item);
            if (pass.target != 0) continue;
            SDL_GPURenderPass* render = begin(pass.clear);
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
                const GPUTexture* t = &white;
                if (draw.texture.value != 0) {
                    const auto it = textures.find(draw.texture.value);
                    if (it == textures.end()) {
                        ++tally.skippedDraws;
                        continue;
                    }
                    t = &it->second;
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
            vertexBase += static_cast<std::uint32_t>(pass.vertices.size() * sizeof(R::Vertex2D));
            indexBase += static_cast<std::uint32_t>(pass.indices.size() * sizeof(std::uint32_t));
        }
        if (first) SDL_EndGPURenderPass(begin(std::nullopt));
    }

    // Records one Pass3D: its depth cleared to 1 (each 3D pass has a depth of its own), its colour kept as the target
    // is (cleared to transparent black when it is the target's first pass of the frame, as a Pass2D without a clear).
    // vertexBase, indexBase: where the pass's own vertices and indices are in the frame's buffers.
    void RecordPass3D(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* texture, const SDL_GPUTextureFormat format, const R::TargetSize size,
                      const R::Pass3D& pass, bool& first, const std::uint32_t vertexBase, const std::uint32_t indexBase,
                      FrameTally& tally) {
        SDL_GPUColorTargetInfo colour{};
        colour.texture = texture;
        colour.store_op = SDL_GPU_STOREOP_STORE;
        colour.clear_color = SDL_FColor{0.0F, 0.0F, 0.0F, 0.0F};
        colour.load_op = first ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        first = false;
        if (!EnsureDepth(size, tally.lastRefusal)) {
            // No depth to draw with: the pass's draws are skipped; the colour is still what the rule above says.
            tally.skippedDraws += pass.draws.size();
            SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &colour, 1, nullptr));
            return;
        }
        SDL_GPUDepthStencilTargetInfo depthTarget{};
        depthTarget.texture = depth;
        depthTarget.clear_depth = 1.0F;
        // Cleared unless the pass keeps the depth of the passes before it (Pass3D::clearDepth; SDL cycles a target
        // only when it is cleared). Stored, so a later pass may keep it.
        depthTarget.load_op = pass.clearDepth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        depthTarget.store_op = SDL_GPU_STOREOP_STORE;
        depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthTarget.cycle = pass.clearDepth;
        SDL_GPURenderPass* render = SDL_BeginGPURenderPass(cmd, &colour, 1, &depthTarget);
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
        SDL_GPUGraphicsPipeline* boundPipeline = nullptr;
        SDL_GPUBuffer* boundVertices = nullptr;
        SDL_GPUTextureSamplerBinding boundTexture{};
        for (const R::Draw3D& draw : pass.draws) {
            if (draw.indexCount == 0 || !drawable) continue;
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
            const GPUTexture* tex = &white;
            if (draw.texture.value != 0) {
                const auto it = textures.find(draw.texture.value);
                if (it == textures.end()) {
                    ++tally.skippedDraws;
                    continue;
                }
                tex = &it->second;
            }
            SDL_GPUGraphicsPipeline* pipeline = Pipeline3DFor(format, draw, tally.lastRefusal);
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
                SDL_BindGPUVertexBuffers(render, 0, &vb, 1);
                SDL_BindGPUIndexBuffer(render, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                boundVertices = vb.buffer;
            }
            const Abi::Mesh3DTransformUniform transform = Abi::ToUniform(Engine::Math::Multiply(pass.clipFromWorld, draw.world));
            SDL_PushGPUVertexUniformData(cmd, Abi::kMesh3DTransformUniformSlot, &transform, sizeof(transform));
            const Abi::Mesh3DColorUniform tint = Abi::ToUniform(draw.color);
            SDL_PushGPUFragmentUniformData(cmd, Abi::kMesh3DColorUniformSlot, &tint, sizeof(tint));
            const SDL_GPUTextureSamplerBinding binding{tex->texture, tex->filter == R::TextureFilter::Nearest ? nearest : linear};
            if (binding.texture != boundTexture.texture || binding.sampler != boundTexture.sampler) {
                SDL_BindGPUFragmentSamplers(render, Abi::kMesh3DTextureSlot, &binding, 1);
                boundTexture = binding;
            }
            const auto firstVertex = static_cast<std::int32_t>(draw.vertexOffset);
            SDL_DrawGPUIndexedPrimitives(render, draw.indexCount, 1, draw.indexOffset, firstVertex, 0);
        }
        SDL_EndGPURenderPass(render);
    }
};

namespace {

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

} // namespace

Engine::Base::Result<std::unique_ptr<GPUDevice>, DeviceFailure> GPUDevice::Create(SDL::NativeWindow* window, const bool debug) {
    RequireMainThread();
    using Engine::Base::Err;
    auto state = std::make_unique<State>();
    // SDL's global state (sdl.zh-Hant.md section 2): os owns the signals; the hint is read when SDL's events start.
    static_cast<void>(SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"));
    state->videoStarted = SDL_InitSubSystem(SDL_INIT_VIDEO);
    std::string startError = state->videoStarted ? std::string() : SDLError("SDL_InitSubSystem(VIDEO)");
    static_cast<void>(SDL_ResetHint(SDL_HINT_NO_SIGNAL_HANDLERS));
    if (!state->videoStarted) return Err(DeviceFailure::Make(DeviceFailureCode::VideoFailed, std::move(startError)));

    const EmbeddedShader* vertex = FindEmbeddedShader("pass2d.vert");
    const EmbeddedShader* fragment = FindEmbeddedShader("pass2d.frag");
    const EmbeddedShader* meshVertex = FindEmbeddedShader("mesh3d.vert");
    const EmbeddedShader* meshFragment = FindEmbeddedShader("mesh3d.frag");
    GYO_ASSERT(vertex != nullptr && fragment != nullptr && meshVertex != nullptr && meshFragment != nullptr &&
               "the passes' shaders are compiled into sdl_gpu");
    state->device = SDL_CreateGPUDevice(FormatsOf(*vertex) & FormatsOf(*fragment) & FormatsOf(*meshVertex) & FormatsOf(*meshFragment),
                                        debug, nullptr);
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
    state->vertex = MakeShader(state->device, *vertex, error);
    if (state->vertex != nullptr) state->fragment = MakeShader(state->device, *fragment, error);
    if (state->fragment != nullptr) state->meshVertex = MakeShader(state->device, *meshVertex, error);
    if (state->meshVertex != nullptr) state->meshFragment = MakeShader(state->device, *meshFragment, error);
    if (state->meshFragment == nullptr) return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, std::move(error)));
    for (const SDL_GPUTextureFormat format : kDepthFormats) {
        if (SDL_GPUTextureSupportsFormat(state->device, format, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
            state->depthFormat = format;
            break;
        }
    }
    if (state->depthFormat == SDL_GPU_TEXTUREFORMAT_INVALID) {
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, "the device supports none of D32_FLOAT, D24_UNORM, D16_UNORM as a depth target"));
    }
    state->linear = MakeSampler(state->device, SDL_GPU_FILTER_LINEAR);
    state->nearest = MakeSampler(state->device, SDL_GPU_FILTER_NEAREST);
    if (state->linear == nullptr || state->nearest == nullptr) {
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SDLError("SDL_CreateGPUSampler")));
    }
    // The white texture (TextureId{}): one opaque white pixel.
    state->white.texture = state->MakeTexture(1, 1, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    state->white.width = 1;
    state->white.height = 1;
    if (state->white.texture == nullptr) return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SDLError("SDL_CreateGPUTexture")));
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
                               const R::RenderCommands& commands, FrameTally& tally) {
    RequireMainThread();
    GYO_ASSERT(state_->window != nullptr && "Submit needs the window (a device without one only Captures)");
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state_->device);
    if (cmd == nullptr) {
        lastError_ = SDLError("SDL_AcquireGPUCommandBuffer");
        return FrameOutcome::Failed;
    }
    bool uploaded = state_->RecordUploads(cmd, asks, meshAsks, commands, tally, lastError_);
    SDL_GPUTexture* swapchain = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, state_->window, &swapchain, &width, &height)) {
        lastError_ = SDLError("SDL_WaitAndAcquireGPUSwapchainTexture");
        static_cast<void>(SDL_SubmitGPUCommandBuffer(cmd));
        return FrameOutcome::Failed;
    }
    FrameOutcome outcome = FrameOutcome::Skipped;
    if (swapchain != nullptr) {
        // When the uploads failed the window still gets a picture: one cleared to transparent black.
        const SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(state_->device, state_->window);
        state_->RecordPasses(cmd, swapchain, format, R::TargetSize{width, height}, uploaded ? commands : R::RenderCommands{}, tally);
        outcome = FrameOutcome::Shown;
    }
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        lastError_ = SDLError("SDL_SubmitGPUCommandBuffer");
        return FrameOutcome::Failed;
    }
    return uploaded ? outcome : FrameOutcome::Failed;
}

Engine::Base::Result<CapturedImage, CaptureFailure> GPUDevice::Capture(const std::span<const R::TextureOp> asks,
                                                                    const std::span<const R::MeshOp> meshAsks,
                                                                    const R::RenderCommands& commands, const R::TargetSize size,
                                                                    FrameTally& tally) {
    RequireMainThread();
    const auto Err = [](std::string text) { return Engine::Base::Err(CaptureFailure::Make(CaptureFailureCode::Refused, std::move(text))); };
    GYO_ASSERT(size.width > 0 && size.height > 0 && "a capture of zero size");
    SDL_GPUDevice* device = state_->device;
    SDL_GPUTexture* target = state_->MakeTexture(size.width, size.height, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    if (target == nullptr) return Err(SDLError("SDL_CreateGPUTexture"));
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
    std::string error;
    if (!state_->RecordUploads(cmd, asks, meshAsks, commands, tally, error)) {
        SDL_CancelGPUCommandBuffer(cmd);
        cleanup();
        return Err(std::move(error));
    }
    state_->RecordPasses(cmd, target, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, size, commands, tally);
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
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        cleanup();
        return Err(SDLError("SDL_SubmitGPUCommandBufferAndAcquireFence"));
    }
    const bool waited = SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
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
}

void GPUDevice::ForgetMeshes() {
    RequireMainThread();
    state_->ReleaseMeshes();
}

std::size_t GPUDevice::TextureCount() const noexcept { return state_->textures.size(); }

std::size_t GPUDevice::MeshCount() const noexcept { return state_->meshes.size(); }

const std::string& GPUDevice::DriverName() const noexcept { return state_->driver; }

} // namespace GYO::Plugins::SDLGPU
