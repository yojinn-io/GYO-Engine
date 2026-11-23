// sdl_gpu's adapter (Design Invariant SDL-1: with ShaderTool.cpp, the only place of sdl_gpu that includes SDL3/). It
// implements GPUDevice (../../GPUDevice.hpp): the device, the window's swapchain, the textures keyed by render's ids,
// the 2D pass's pipelines and the frame (uploads in one copy pass, then one render pass per Pass2D). Shapes follow
// SDL's own GPU examples and Dear ImGui's SDL_GPU backend (imgui_impl_sdlgpu3.cpp): one transfer buffer cycled each
// frame, vertex and index buffers grown to the frame's need, the scissor per draw.
#include "../../GPUDevice.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <variant>

#include "engine/base/Assert.hpp"
#include "engine/render/ShaderAbi.hpp"

#include "../../EmbeddedShaders.hpp"

namespace GYO::Plugins::SDLGPU {

namespace R = Engine::Render;
namespace Abi = Engine::Render::ShaderAbi;

namespace {

void RequireMainThread() { GYO_ASSERT(SDL_IsMainThread() && "sdl_gpu's adapter is called on the main thread only (SDL-4)"); }

std::string SdlError(const char* step) {
    const char* text = SDL_GetError();
    return std::string(step) + ": " + (text != nullptr && *text != '\0' ? text : "(SDL gave no text)");
}

SDL_GPUShaderFormat SdlFormat(const ShaderFormat format) noexcept {
    switch (format) {
    case ShaderFormat::Spirv: return SDL_GPU_SHADERFORMAT_SPIRV;
    case ShaderFormat::Dxil: return SDL_GPU_SHADERFORMAT_DXIL;
    case ShaderFormat::Metallib: return SDL_GPU_SHADERFORMAT_METALLIB;
    }
    return SDL_GPU_SHADERFORMAT_INVALID;
}

SDL_GPUShaderFormat FormatsOf(const EmbeddedShader& shader) noexcept {
    SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_INVALID;
    for (const CompiledShader& compiled : shader.compiled) formats |= SdlFormat(compiled.format);
    return formats;
}

// Offsets inside the transfer buffer: D3D12 places texture copies on 512 bytes.
constexpr std::uint32_t kPlacement = 512;
std::uint32_t AlignUp(const std::uint32_t value) noexcept { return (value + kPlacement - 1) / kPlacement * kPlacement; }

std::uint32_t GrowTo(const std::uint32_t need) noexcept {
    std::uint32_t size = 4096;
    while (size < need) size *= 2;
    return size;
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

// One chunk of the frame's uploads, at its offset in the transfer buffer.
struct Upload {
    std::uint32_t offset = 0;
    const std::uint8_t* bytes = nullptr;
    std::uint32_t size = 0;
};

} // namespace

struct GPUDevice::State {
    SDL_GPUDevice* device = nullptr;
    SDL_Window* window = nullptr;
    bool videoStarted = false;
    std::string driver;
    SDL_GPUShader* vertex = nullptr;
    SDL_GPUShader* fragment = nullptr;
    SDL_GPUSampler* linear = nullptr;
    SDL_GPUSampler* nearest = nullptr;
    GPUTexture white;
    std::unordered_map<std::uint32_t, GPUTexture> textures;
    std::vector<Pipeline> pipelines;
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
            if (linear != nullptr) SDL_ReleaseGPUSampler(device, linear);
            if (nearest != nullptr) SDL_ReleaseGPUSampler(device, nearest);
            if (vertex != nullptr) SDL_ReleaseGPUShader(device, vertex);
            if (fragment != nullptr) SDL_ReleaseGPUShader(device, fragment);
            if (transfer != nullptr) SDL_ReleaseGPUTransferBuffer(device, transfer);
            if (vertices != nullptr) SDL_ReleaseGPUBuffer(device, vertices);
            if (indices != nullptr) SDL_ReleaseGPUBuffer(device, indices);
            if (window != nullptr) SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyGPUDevice(device);
        }
        if (videoStarted) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    // The pipeline for a target format and a blend mode, made the first time it is asked for; nullptr when SDL refuses.
    SDL_GPUGraphicsPipeline* PipelineFor(const SDL_GPUTextureFormat format, const R::BlendMode blend, std::string& error) {
        for (const Pipeline& p : pipelines) {
            if (p.format == format && p.blend == blend) return p.pipeline;
        }
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
            error = SdlError("SDL_CreateGPUGraphicsPipeline");
            return nullptr;
        }
        pipelines.push_back(Pipeline{format, blend, made});
        return made;
    }

    bool EnsureBuffer(SDL_GPUBuffer*& buffer, std::uint32_t& size, const std::uint32_t need, const SDL_GPUBufferUsageFlags usage,
                      std::string& error) {
        if (need <= size && buffer != nullptr) return true;
        if (buffer != nullptr) SDL_ReleaseGPUBuffer(device, buffer);
        SDL_GPUBufferCreateInfo info{};
        info.usage = usage;
        info.size = GrowTo(need);
        buffer = SDL_CreateGPUBuffer(device, &info);
        size = buffer != nullptr ? info.size : 0;
        if (buffer == nullptr) error = SdlError("SDL_CreateGPUBuffer");
        return buffer != nullptr;
    }

    bool EnsureTransfer(const std::uint32_t need, std::string& error) {
        if (need <= transferSize && transfer != nullptr) return true;
        if (transfer != nullptr) SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = GrowTo(need);
        transfer = SDL_CreateGPUTransferBuffer(device, &info);
        transferSize = transfer != nullptr ? info.size : 0;
        if (transfer == nullptr) error = SdlError("SDL_CreateGPUTransferBuffer");
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
            std::uint32_t offset;
            SDL_GPUTexture* texture;
            R::TextureRegion region;
        };
        std::vector<TextureCopy> textureCopies;
        std::uint32_t vertexOffset = 0;
        std::uint32_t vertexBytes = 0;
        std::uint32_t indexOffset = 0;
        std::uint32_t indexBytes = 0;
        std::uint32_t total = 0;
    };

    // Applies the asks to the texture table (makes and lets go now; the pixels go in the frame's copy pass).
    void ApplyAsks(const std::span<const R::TextureOp> asks, Frame& frame, FrameTally& tally, std::string& error) {
        const auto place = [&](const std::vector<std::uint8_t>& pixels) {
            frame.total = AlignUp(frame.total);
            const std::uint32_t offset = frame.total;
            frame.uploads.push_back(Upload{offset, pixels.data(), static_cast<std::uint32_t>(pixels.size())});
            frame.total += static_cast<std::uint32_t>(pixels.size());
            return offset;
        };
        for (const R::TextureOp& ask : asks) {
            if (const auto* make = std::get_if<R::TextureCreate>(&ask)) {
                SDL_GPUTexture* texture = MakeTexture(make->desc.width, make->desc.height, SDL_GPU_TEXTUREUSAGE_SAMPLER);
                if (texture == nullptr) {
                    ++tally.refusedAsks;
                    error = SdlError("SDL_CreateGPUTexture");
                    continue;
                }
                textures[make->id.value] = GPUTexture{texture, make->desc.filter, make->desc.width, make->desc.height};
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
                SDL_ReleaseGPUTexture(device, it->second.texture);
                textures.erase(it);
            }
        }
    }

    // Places the vertices and indices of the passes on target 0 after the texture pixels.
    void PlaceGeometry(const R::RenderCommands& commands, Frame& frame, std::vector<Upload>& geometry, FrameTally& tally) {
        std::uint32_t vertexBytes = 0;
        std::uint32_t indexBytes = 0;
        for (const R::Pass2D& pass : commands.passes) {
            if (pass.target != 0) {
                ++tally.unmappedPasses;
                continue;
            }
            vertexBytes += static_cast<std::uint32_t>(pass.vertices.size() * sizeof(R::Vertex2D));
            indexBytes += static_cast<std::uint32_t>(pass.indices.size() * sizeof(std::uint32_t));
        }
        frame.total = AlignUp(frame.total);
        frame.vertexOffset = frame.total;
        frame.vertexBytes = vertexBytes;
        frame.total += vertexBytes;
        frame.total = AlignUp(frame.total);
        frame.indexOffset = frame.total;
        frame.indexBytes = indexBytes;
        frame.total += indexBytes;
        std::uint32_t v = frame.vertexOffset;
        std::uint32_t i = frame.indexOffset;
        for (const R::Pass2D& pass : commands.passes) {
            if (pass.target != 0) continue;
            const auto vb = static_cast<std::uint32_t>(pass.vertices.size() * sizeof(R::Vertex2D));
            const auto ib = static_cast<std::uint32_t>(pass.indices.size() * sizeof(std::uint32_t));
            geometry.push_back(Upload{v, reinterpret_cast<const std::uint8_t*>(pass.vertices.data()), vb});
            geometry.push_back(Upload{i, reinterpret_cast<const std::uint8_t*>(pass.indices.data()), ib});
            v += vb;
            i += ib;
        }
    }

    // Records the copy pass of the frame. false: SDL refused a step (error says which).
    bool RecordUploads(SDL_GPUCommandBuffer* cmd, const std::span<const R::TextureOp> asks, const R::RenderCommands& commands,
                       FrameTally& tally, std::string& error) {
        Frame frame;
        ApplyAsks(asks, frame, tally, error);
        std::vector<Upload> geometry;
        PlaceGeometry(commands, frame, geometry, tally);
        if (frame.total == 0) return true;
        if (!EnsureTransfer(frame.total, error)) return false;
        if (frame.vertexBytes > 0 && !EnsureBuffer(vertices, verticesSize, frame.vertexBytes, SDL_GPU_BUFFERUSAGE_VERTEX, error)) return false;
        if (frame.indexBytes > 0 && !EnsureBuffer(indices, indicesSize, frame.indexBytes, SDL_GPU_BUFFERUSAGE_INDEX, error)) return false;
        auto* mapped = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device, transfer, true));
        if (mapped == nullptr) {
            error = SdlError("SDL_MapGPUTransferBuffer");
            return false;
        }
        for (const Upload& u : frame.uploads) std::memcpy(mapped + u.offset, u.bytes, u.size);
        for (const Upload& u : geometry) {
            if (u.size > 0) std::memcpy(mapped + u.offset, u.bytes, u.size);
        }
        SDL_UnmapGPUTransferBuffer(device, transfer);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
        for (const Frame::TextureCopy& c : frame.textureCopies) {
            SDL_GPUTextureTransferInfo source{};
            source.transfer_buffer = transfer;
            source.offset = c.offset;
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
        if (frame.vertexBytes > 0) {
            SDL_GPUTransferBufferLocation source{transfer, frame.vertexOffset};
            SDL_GPUBufferRegion destination{vertices, 0, frame.vertexBytes};
            SDL_UploadToGPUBuffer(copy, &source, &destination, true);
        }
        if (frame.indexBytes > 0) {
            SDL_GPUTransferBufferLocation source{transfer, frame.indexOffset};
            SDL_GPUBufferRegion destination{indices, 0, frame.indexBytes};
            SDL_UploadToGPUBuffer(copy, &source, &destination, true);
        }
        SDL_EndGPUCopyPass(copy);
        return true;
    }

    // Records one render pass per Pass2D on target 0 into texture (format, size); none: one pass that clears.
    void RecordPasses(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* texture, const SDL_GPUTextureFormat format, const R::TargetSize size,
                      const R::RenderCommands& commands, FrameTally& tally, std::string& error) {
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
        for (const R::Pass2D& pass : commands.passes) {
            if (pass.target != 0) continue;
            SDL_GPURenderPass* render = begin(pass.clear);
            const SDL_GPUViewport viewport{0.0F, 0.0F, static_cast<float>(size.width), static_cast<float>(size.height), 0.0F, 1.0F};
            SDL_SetGPUViewport(render, &viewport);
            const Engine::Math::Rect view = R::ViewOf(pass, size);
            const Abi::Pass2DViewUniform uniform = Abi::ToUniform(R::ViewToClip(view));
            SDL_PushGPUVertexUniformData(cmd, Abi::kPass2DViewUniformSlot, &uniform, sizeof(uniform));
            // A pass with no vertices or no indices has nothing a draw could name.
            const bool bound = !pass.vertices.empty() && !pass.indices.empty() && vertices != nullptr && indices != nullptr;
            if (bound) {
                const SDL_GPUBufferBinding vb{vertices, vertexBase};
                SDL_BindGPUVertexBuffers(render, 0, &vb, 1);
                const SDL_GPUBufferBinding ib{indices, indexBase};
                SDL_BindGPUIndexBuffer(render, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            }
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
                const R::PixelRect scissor = R::ClipToPixels(draw.clip, view, size);
                if (scissor.width == 0 || scissor.height == 0) continue;
                SDL_GPUGraphicsPipeline* pipeline = PipelineFor(format, draw.blend, error);
                if (pipeline == nullptr) {
                    ++tally.skippedDraws;
                    continue;
                }
                SDL_BindGPUGraphicsPipeline(render, pipeline);
                const SDL_Rect rect{static_cast<int>(scissor.x), static_cast<int>(scissor.y), static_cast<int>(scissor.width),
                                    static_cast<int>(scissor.height)};
                SDL_SetGPUScissor(render, &rect);
                const SDL_GPUTextureSamplerBinding binding{t->texture, t->filter == R::TextureFilter::Nearest ? nearest : linear};
                SDL_BindGPUFragmentSamplers(render, Abi::kPass2DTextureSlot, &binding, 1);
                SDL_DrawGPUIndexedPrimitives(render, draw.indexCount, 1, draw.indexOffset, static_cast<std::int32_t>(draw.vertexOffset), 0);
            }
            SDL_EndGPURenderPass(render);
            vertexBase += static_cast<std::uint32_t>(pass.vertices.size() * sizeof(R::Vertex2D));
            indexBase += static_cast<std::uint32_t>(pass.indices.size() * sizeof(std::uint32_t));
        }
        if (first) SDL_EndGPURenderPass(begin(std::nullopt));
    }
};

namespace {

SDL_GPUShader* MakeShader(SDL_GPUDevice* device, const EmbeddedShader& shader, std::string& error) {
    const SDL_GPUShaderFormat available = SDL_GetGPUShaderFormats(device);
    for (const CompiledShader& compiled : shader.compiled) {
        if ((available & SdlFormat(compiled.format)) == 0) continue;
        const std::string entry(compiled.entryPoint);
        SDL_GPUShaderCreateInfo info{};
        info.code = compiled.code.data();
        info.code_size = compiled.code.size();
        info.entrypoint = entry.c_str();
        info.format = SdlFormat(compiled.format);
        info.stage = shader.stage == ShaderStage::Vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
        info.num_samplers = shader.resources.samplers;
        info.num_storage_textures = shader.resources.storageTextures;
        info.num_storage_buffers = shader.resources.storageBuffers;
        info.num_uniform_buffers = shader.resources.uniformBuffers;
        SDL_GPUShader* made = SDL_CreateGPUShader(device, &info);
        if (made == nullptr) error = SdlError("SDL_CreateGPUShader");
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

Engine::Base::Result<std::unique_ptr<GPUDevice>, DeviceFailure> GPUDevice::Create(SDL::NativeWindow* window) {
    RequireMainThread();
    using Engine::Base::Err;
    auto state = std::make_unique<State>();
    // SDL's global state (sdl.zh-Hant.md section 2): os owns the signals; the hint is read when SDL's events start.
    static_cast<void>(SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"));
    state->videoStarted = SDL_InitSubSystem(SDL_INIT_VIDEO);
    std::string startError = state->videoStarted ? std::string() : SdlError("SDL_InitSubSystem(VIDEO)");
    static_cast<void>(SDL_ResetHint(SDL_HINT_NO_SIGNAL_HANDLERS));
    if (!state->videoStarted) return Err(DeviceFailure::Make(DeviceFailureCode::VideoFailed, std::move(startError)));

    const EmbeddedShader* vertex = FindEmbeddedShader("pass2d.vert");
    const EmbeddedShader* fragment = FindEmbeddedShader("pass2d.frag");
    GYO_ASSERT(vertex != nullptr && fragment != nullptr && "the 2D pass's shaders are compiled into sdl_gpu");
    state->device = SDL_CreateGPUDevice(FormatsOf(*vertex) & FormatsOf(*fragment), false, nullptr);
    if (state->device == nullptr) return Err(DeviceFailure::Make(DeviceFailureCode::DeviceFailed, SdlError("SDL_CreateGPUDevice")));
    state->driver = SDL_GetGPUDeviceDriver(state->device);
    if (window != nullptr) {
        auto* sdlWindow = reinterpret_cast<SDL_Window*>(window);
        if (!SDL_ClaimWindowForGPUDevice(state->device, sdlWindow)) {
            return Err(DeviceFailure::Make(DeviceFailureCode::ClaimFailed, SdlError("SDL_ClaimWindowForGPUDevice")));
        }
        state->window = sdlWindow;
    }
    std::string error;
    state->vertex = MakeShader(state->device, *vertex, error);
    if (state->vertex != nullptr) state->fragment = MakeShader(state->device, *fragment, error);
    if (state->fragment == nullptr) return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, std::move(error)));
    state->linear = MakeSampler(state->device, SDL_GPU_FILTER_LINEAR);
    state->nearest = MakeSampler(state->device, SDL_GPU_FILTER_NEAREST);
    if (state->linear == nullptr || state->nearest == nullptr) {
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SdlError("SDL_CreateGPUSampler")));
    }
    // The white texture (TextureId{}): one opaque white pixel.
    state->white.texture = state->MakeTexture(1, 1, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    state->white.width = 1;
    state->white.height = 1;
    if (state->white.texture == nullptr) return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SdlError("SDL_CreateGPUTexture")));
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (cmd == nullptr) return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SdlError("SDL_AcquireGPUCommandBuffer")));
    if (!state->EnsureTransfer(4, error)) {
        SDL_CancelGPUCommandBuffer(cmd);
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, std::move(error)));
    }
    auto* mapped = static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(state->device, state->transfer, false));
    if (mapped == nullptr) {
        SDL_CancelGPUCommandBuffer(cmd);
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SdlError("SDL_MapGPUTransferBuffer")));
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
        return Err(DeviceFailure::Make(DeviceFailureCode::SetupFailed, SdlError("SDL_SubmitGPUCommandBuffer")));
    }
    return std::unique_ptr<GPUDevice>(new GPUDevice(std::move(state)));
}

GPUDevice::GPUDevice(std::unique_ptr<State> state) : state_(std::move(state)) {}

GPUDevice::~GPUDevice() {
    RequireMainThread();
    state_.reset();
}

FrameOutcome GPUDevice::Submit(const std::span<const R::TextureOp> asks, const R::RenderCommands& commands, FrameTally& tally) {
    RequireMainThread();
    GYO_ASSERT(state_->window != nullptr && "Submit needs the window (a device without one only Captures)");
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state_->device);
    if (cmd == nullptr) {
        lastError_ = SdlError("SDL_AcquireGPUCommandBuffer");
        return FrameOutcome::Failed;
    }
    bool uploaded = state_->RecordUploads(cmd, asks, commands, tally, lastError_);
    SDL_GPUTexture* swapchain = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, state_->window, &swapchain, &width, &height)) {
        lastError_ = SdlError("SDL_WaitAndAcquireGPUSwapchainTexture");
        static_cast<void>(SDL_SubmitGPUCommandBuffer(cmd));
        return FrameOutcome::Failed;
    }
    FrameOutcome outcome = FrameOutcome::Skipped;
    if (swapchain != nullptr) {
        // When the uploads failed the window still gets a picture: one cleared to transparent black.
        const SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(state_->device, state_->window);
        state_->RecordPasses(cmd, swapchain, format, R::TargetSize{width, height}, uploaded ? commands : R::RenderCommands{}, tally,
                             lastError_);
        outcome = FrameOutcome::Shown;
    }
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        lastError_ = SdlError("SDL_SubmitGPUCommandBuffer");
        return FrameOutcome::Failed;
    }
    return uploaded ? outcome : FrameOutcome::Failed;
}

Engine::Base::Result<CapturedImage, CaptureFailure> GPUDevice::Capture(const std::span<const R::TextureOp> asks,
                                                                    const R::RenderCommands& commands, const R::TargetSize size,
                                                                    FrameTally& tally) {
    RequireMainThread();
    const auto Err = [](std::string text) { return Engine::Base::Err(CaptureFailure::Make(CaptureFailureCode::Refused, std::move(text))); };
    GYO_ASSERT(size.width > 0 && size.height > 0 && "a capture of zero size");
    SDL_GPUDevice* device = state_->device;
    SDL_GPUTexture* target = state_->MakeTexture(size.width, size.height, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    if (target == nullptr) return Err(SdlError("SDL_CreateGPUTexture"));
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
        return Err(SdlError("SDL_CreateGPUTransferBuffer"));
    }
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr) {
        cleanup();
        return Err(SdlError("SDL_AcquireGPUCommandBuffer"));
    }
    std::string error;
    if (!state_->RecordUploads(cmd, asks, commands, tally, error)) {
        SDL_CancelGPUCommandBuffer(cmd);
        cleanup();
        return Err(std::move(error));
    }
    state_->RecordPasses(cmd, target, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, size, commands, tally, error);
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
        return Err(SdlError("SDL_SubmitGPUCommandBufferAndAcquireFence"));
    }
    const bool waited = SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    if (!waited) {
        cleanup();
        return Err(SdlError("SDL_WaitForGPUFences"));
    }
    const auto* mapped = static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device, download, false));
    if (mapped == nullptr) {
        cleanup();
        return Err(SdlError("SDL_MapGPUTransferBuffer"));
    }
    CapturedImage image{size, std::vector<std::uint8_t>(mapped, mapped + bytes)};
    SDL_UnmapGPUTransferBuffer(device, download);
    cleanup();
    return image;
}

void GPUDevice::ForgetTextures() {
    RequireMainThread();
    for (auto& [id, t] : state_->textures) SDL_ReleaseGPUTexture(state_->device, t.texture);
    state_->textures.clear();
}

std::size_t GPUDevice::TextureCount() const noexcept { return state_->textures.size(); }

const std::string& GPUDevice::DriverName() const noexcept { return state_->driver; }

} // namespace GYO::Plugins::SDLGPU
