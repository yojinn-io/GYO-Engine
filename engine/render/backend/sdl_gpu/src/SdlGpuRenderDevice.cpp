#include "render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"
#include <SDL3/SDL_gpu.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>
#include <utility>
namespace Engine::Render::Backend::SdlGpu {
namespace {
constexpr auto kDepthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
// Unique across device instances: a frame from a second SDL device must not
// accidentally match the first frame serial of this device.
std::atomic<std::uint64_t> nextFrameToken{1};
[[nodiscard]] bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool IsFinite(Float3 value) noexcept {
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

[[nodiscard]] bool IsFinite(Color value) noexcept {
    return IsFinite(value.red) && IsFinite(value.green) &&
           IsFinite(value.blue) && IsFinite(value.alpha);
}

[[nodiscard]] RenderError MakeError(
    RenderErrorCode code,
    std::string message,
    std::string detail = {}) {
    return RenderError::Make(code, std::move(message), std::move(detail));
}

[[nodiscard]] RenderError MakeSdlError(
    RenderErrorCode code,
    std::string message) {
    return MakeError(code, std::move(message), SDL_GetError());
}

[[nodiscard]] std::uint32_t NextGeneration(std::uint32_t value) noexcept {
    ++value;
    if (value == 0) {
        ++value;
    }
    return value;
}


SDL_GPUShaderFormat ToSdlFormats(ShaderFormatMask formats) {
    SDL_GPUShaderFormat value = 0;
    if (formats & FormatBit(ShaderFormat::DXIL)) value |= SDL_GPU_SHADERFORMAT_DXIL;
    if (formats & FormatBit(ShaderFormat::SPIRV)) value |= SDL_GPU_SHADERFORMAT_SPIRV;
    if (formats & FormatBit(ShaderFormat::Metallib)) value |= SDL_GPU_SHADERFORMAT_METALLIB;
    return value;
}
ShaderFormatMask FromSdlFormats(SDL_GPUShaderFormat formats) {
    ShaderFormatMask value = 0;
    if (formats & SDL_GPU_SHADERFORMAT_DXIL) value |= FormatBit(ShaderFormat::DXIL);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) value |= FormatBit(ShaderFormat::SPIRV);
    if (formats & SDL_GPU_SHADERFORMAT_METALLIB) value |= FormatBit(ShaderFormat::Metallib);
    return value;
}
SDL_GPUTextureFormat ToSdlFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::Rgba8Unorm: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case TextureFormat::Rgba8SRgb: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::Bgra8Unorm: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    case TextureFormat::Bgra8SRgb: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    case TextureFormat::Rgba16Float: return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::Depth32Float: return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }
    return SDL_GPU_TEXTUREFORMAT_INVALID;
}
SDL_GPULoadOp ToLoad(AttachmentLoad load) {
    switch(load) {
    case AttachmentLoad::Clear: return SDL_GPU_LOADOP_CLEAR;
    case AttachmentLoad::Load: return SDL_GPU_LOADOP_LOAD;
    case AttachmentLoad::DontCare: return SDL_GPU_LOADOP_DONT_CARE;
    }
    return SDL_GPU_LOADOP_DONT_CARE;
}
} // namespace
struct SdlGpuRenderDevice::Impl final {
    struct MeshSlot final {
        SDL_GPUBuffer* vertexBuffer{};
        SDL_GPUBuffer* indexBuffer{};
        SDL_GPUTransferBuffer* vertexTransfer{};
        std::uint32_t vertexCount{};
        std::uint32_t indexCount{};
        std::uint32_t generation{1};
        bool live{};
        bool reserved{};
    };

    struct TextureSlot final {
        SDL_GPUTexture* texture{};
        TextureDesc description{};
        std::uint32_t generation{1};
        bool live{};
        bool reserved{};
    };


    struct PipelineSlot final {
        SDL_GPUGraphicsPipeline* pipeline{};
        PipelineDesc description;
        ShaderResourceLayout vertexResources, fragmentResources;
        std::uint32_t generation{1};
        bool live{};
    };
    struct ShaderSlot final {
        SDL_GPUShader* shader{};
        ShaderStage stage{};
        ShaderFormat format{};
        ShaderResourceLayout resources;
        std::uint32_t generation{1};
        bool live{};
    };
    Platform::Sdl::SdlPlatform* platform{};
    SDL_Window* window{};
    SDL_GPUDevice* device{};
    bool windowClaimed{};
    bool debugMode{};
    std::thread::id ownerThread{};
    RenderDeviceInfo info;
    SDL_GPUTextureFormat swapchainFormat{SDL_GPU_TEXTUREFORMAT_INVALID};
    SDL_GPUSampler* linearClampSampler{};
    SDL_GPUSampler* linearWrapSampler{};
    std::vector<MeshSlot> meshes;
    std::vector<std::uint32_t> freeMeshes;
    std::vector<TextureSlot> textures;
    std::vector<std::uint32_t> freeTextures;
    std::vector<PipelineSlot> pipelines;
    std::vector<std::uint32_t> freePipelines;
    std::vector<ShaderSlot> shaders;
    std::vector<std::uint32_t> freeShaders;
    SDL_GPUCommandBuffer* frameCommand{};
    SDL_GPUTexture* frameSwapchain{};
    AcquiredFrame activeFrame{};
    ~Impl() { Shutdown(); }
    [[nodiscard]] Base::Result<void, RenderError> CheckThread() const {
        using Result = Base::Result<void, RenderError>;
        if (std::this_thread::get_id() != ownerThread) {
            return Result::Err(MakeError(
                RenderErrorCode::WrongThread,
                "SDL_GPU: render device must be used from its creation thread"));
        }
        return Result::Ok();
    }


    void Shutdown() noexcept {
        if (!device) return;
        if (frameCommand) AbandonFrame(activeFrame);
        static_cast<void>(SDL_WaitForGPUIdle(device));
        for (auto& slot : meshes) {
            if (slot.vertexBuffer) SDL_ReleaseGPUBuffer(device, slot.vertexBuffer);
            if (slot.indexBuffer) SDL_ReleaseGPUBuffer(device, slot.indexBuffer);
            if (slot.vertexTransfer) SDL_ReleaseGPUTransferBuffer(device, slot.vertexTransfer);
        }
        for (auto& slot : textures) if (slot.texture) SDL_ReleaseGPUTexture(device, slot.texture);
        for (auto& slot : pipelines) if (slot.pipeline) SDL_ReleaseGPUGraphicsPipeline(device, slot.pipeline);
        for (auto& slot : shaders) if (slot.shader) SDL_ReleaseGPUShader(device, slot.shader);
        if (linearClampSampler) SDL_ReleaseGPUSampler(device, linearClampSampler);
        if (linearWrapSampler) SDL_ReleaseGPUSampler(device, linearWrapSampler);
        if (windowClaimed) SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device); device = nullptr;
    }
    [[nodiscard]] SDL_GPUGraphicsPipeline* CreateNativePipeline(
        SDL_GPUShader* vertexShader,
        SDL_GPUShader* fragmentShader,
        SDL_GPUCullMode cullMode,
        bool blend,
        bool hasDepth,
        bool depthWrite,
        SDL_GPUTextureFormat targetFormat, bool vertexInput, bool clockwiseFrontFace) {
        SDL_GPUVertexBufferDescription vertexBuffer{};
        vertexBuffer.slot = 0;
        vertexBuffer.pitch = sizeof(Vertex3D);
        vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        const std::array attributes{
            SDL_GPUVertexAttribute{
                0,
                0,
                SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                static_cast<Uint32>(offsetof(Vertex3D, position)),
            },
            SDL_GPUVertexAttribute{
                1,
                0,
                SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                static_cast<Uint32>(offsetof(Vertex3D, uv)),
            },
        };

        SDL_GPUColorTargetDescription colorTarget{};
        colorTarget.format = targetFormat;
        if (blend) {
            colorTarget.blend_state.enable_blend = true;
            colorTarget.blend_state.src_color_blendfactor =
                SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            colorTarget.blend_state.dst_color_blendfactor =
                SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            colorTarget.blend_state.dst_alpha_blendfactor =
                SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        }

        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vertexShader;
        info.fragment_shader = fragmentShader;
        info.vertex_input_state.vertex_buffer_descriptions = &vertexBuffer;
        info.vertex_input_state.num_vertex_buffers = vertexInput ? 1 : 0;
        info.vertex_input_state.vertex_attributes = attributes.data();
        info.vertex_input_state.num_vertex_attributes =
            vertexInput ? static_cast<Uint32>(attributes.size()) : 0;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = cullMode;
        info.rasterizer_state.front_face = clockwiseFrontFace
            ? SDL_GPU_FRONTFACE_CLOCKWISE : SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        info.rasterizer_state.enable_depth_clip = true;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        info.depth_stencil_state.enable_depth_test = hasDepth;
        info.depth_stencil_state.enable_depth_write = depthWrite;
        info.target_info.color_target_descriptions = &colorTarget;
        info.target_info.num_color_targets = 1;
        info.target_info.has_depth_stencil_target = hasDepth;
        info.target_info.depth_stencil_format = kDepthFormat;
        return SDL_CreateGPUGraphicsPipeline(device, &info);
    }

    [[nodiscard]] SDL_GPUSampler* CreateSampler(SDL_GPUSamplerAddressMode address) {
        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = SDL_GPU_FILTER_LINEAR;
        info.mag_filter = SDL_GPU_FILTER_LINEAR;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        info.address_mode_u = address;
        info.address_mode_v = address;
        info.address_mode_w = address;
        return SDL_CreateGPUSampler(device, &info);
    }

    [[nodiscard]] Base::Result<void, RenderError> Initialize(
        Platform::Sdl::SdlPlatform& sourcePlatform,
        const SdlGpuOptions& options) {
        using Result = Base::Result<void, RenderError>;

        platform = &sourcePlatform;
        window = sourcePlatform.NativeWindow();
        ownerThread = std::this_thread::get_id();
        debugMode = options.debugMode;

        ShaderFormatMask formats = options.availableShaderFormats & AllShaderFormats;
        const char* driver = nullptr;
        if (options.driver == "d3d12") { driver = "direct3d12"; formats &= FormatBit(ShaderFormat::DXIL); }
        else if (options.driver == "vulkan") { driver = "vulkan"; formats &= FormatBit(ShaderFormat::SPIRV); }
        else if (options.driver == "metal") { driver = "metal"; formats &= FormatBit(ShaderFormat::Metallib); }
        else if (options.driver != "auto") return Result::Err(MakeError(RenderErrorCode::InvalidArgument,
            "SDL_GPU: driver must be auto, d3d12, vulkan or metal"));
        if (!formats) return Result::Err(MakeError(RenderErrorCode::BackendUnavailable,
            "SDL_GPU: selected driver has no complete shader artifact format"));
        device = SDL_CreateGPUDevice(ToSdlFormats(formats), options.debugMode, driver);
        if (!device) return Result::Err(MakeSdlError(RenderErrorCode::BackendUnavailable,
            "SDL_GPU: device creation failed for driver " + options.driver));
        info.shaderFormats = FromSdlFormats(SDL_GetGPUShaderFormats(device)) & formats;
        info.driver = SDL_GetGPUDeviceDriver(device);
        if (!info.shaderFormats) return Result::Err(MakeError(RenderErrorCode::BackendUnavailable,
            "SDL_GPU: driver does not accept packaged shader formats"));
        if (!SDL_ClaimWindowForGPUDevice(device, window)) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::BackendUnavailable,
                "SDL_GPU: failed to claim the SDL window"));
        }
        windowClaimed = true;

        SDL_GPUPresentMode presentMode = SDL_GPU_PRESENTMODE_VSYNC;
        if (!options.vsync &&
            SDL_WindowSupportsGPUPresentMode(
                device, window, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
            presentMode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        }
        if (!SDL_WindowSupportsGPUSwapchainComposition(
                device,
                window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
            return Result::Err(MakeError(
                RenderErrorCode::BackendUnavailable,
                "SDL_GPU: driver does not support a linear SDR swapchain"));
        }
        if (!SDL_SetGPUSwapchainParameters(
                device,
                window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                presentMode)) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::BackendUnavailable,
                "SDL_GPU: failed to configure a linear SDR swapchain"));
        }
        if (!SDL_SetGPUAllowedFramesInFlight(device, 2)) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::BackendUnavailable,
                "SDL_GPU: failed to configure frames in flight"));
        }

        swapchainFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
        if (swapchainFormat != SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB &&
            swapchainFormat != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB) {
            return Result::Err(MakeError(
                RenderErrorCode::BackendUnavailable,
                "SDL_GPU: linear SDR swapchain did not provide an sRGB target"));
        }

        linearClampSampler = CreateSampler(SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE);
        linearWrapSampler = CreateSampler(SDL_GPU_SAMPLERADDRESSMODE_REPEAT);
        if (linearClampSampler == nullptr || linearWrapSampler == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to create built-in samplers"));
        }

        return Result::Ok();
    }
    [[nodiscard]] Base::Result<MeshSlot*, RenderError> FindMesh(MeshHandle handle) {
        using Result = Base::Result<MeshSlot*, RenderError>;
        if (!handle || handle.Index() >= meshes.size()) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidHandle,
                "SDL_GPU: invalid mesh handle"));
        }
        MeshSlot& slot = meshes[handle.Index()];
        if (!slot.live || slot.generation != handle.Generation()) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidHandle,
                "SDL_GPU: stale mesh handle"));
        }
        return Result::Ok(&slot);
    }

    [[nodiscard]] Base::Result<TextureSlot*, RenderError> FindTexture(
        TextureHandle handle) {
        using Result = Base::Result<TextureSlot*, RenderError>;
        if (!handle || handle.Index() >= textures.size()) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidHandle,
                "SDL_GPU: invalid texture handle"));
        }
        TextureSlot& slot = textures[handle.Index()];
        if (!slot.live || slot.generation != handle.Generation()) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidHandle,
                "SDL_GPU: stale texture handle"));
        }
        return Result::Ok(&slot);
    }

    [[nodiscard]] Base::Result<void, RenderError> UploadBuffers(
        SDL_GPUBuffer* vertexBuffer,
        std::span<const std::byte> vertexBytes,
        SDL_GPUBuffer* indexBuffer,
        std::span<const std::byte> indexBytes) {
        using Result = Base::Result<void, RenderError>;

        const std::uint64_t totalSize = vertexBytes.size() + indexBytes.size();
        if (totalSize > std::numeric_limits<Uint32>::max()) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidArgument,
                "SDL_GPU: mesh upload exceeds the 32-bit transfer limit"));
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<Uint32>(totalSize);
        SDL_GPUTransferBuffer* transfer =
            SDL_CreateGPUTransferBuffer(device, &transferInfo);
        if (transfer == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to create a mesh transfer buffer"));
        }

        void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
        if (mapped == nullptr) {
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to map a mesh transfer buffer"));
        }
        std::memcpy(mapped, vertexBytes.data(), vertexBytes.size());
        std::memcpy(
            static_cast<std::byte*>(mapped) + vertexBytes.size(),
            indexBytes.data(),
            indexBytes.size());
        SDL_UnmapGPUTransferBuffer(device, transfer);

        SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
        if (command == nullptr) {
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: failed to acquire a mesh upload command buffer"));
        }
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(command);
        if (copyPass == nullptr) {
            static_cast<void>(SDL_CancelGPUCommandBuffer(command));
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: failed to begin a mesh copy pass"));
        }

        const SDL_GPUTransferBufferLocation vertexSource{transfer, 0};
        const SDL_GPUBufferRegion vertexDestination{
            vertexBuffer,
            0,
            static_cast<Uint32>(vertexBytes.size()),
        };
        SDL_UploadToGPUBuffer(
            copyPass, &vertexSource, &vertexDestination, false);

        const SDL_GPUTransferBufferLocation indexSource{
            transfer,
            static_cast<Uint32>(vertexBytes.size()),
        };
        const SDL_GPUBufferRegion indexDestination{
            indexBuffer,
            0,
            static_cast<Uint32>(indexBytes.size()),
        };
        SDL_UploadToGPUBuffer(copyPass, &indexSource, &indexDestination, false);
        SDL_EndGPUCopyPass(copyPass);

        const bool submitted = SDL_SubmitGPUCommandBuffer(command);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        if (!submitted) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: mesh upload submission failed"));
        }
        return Result::Ok();
    }

    [[nodiscard]] Base::Result<MeshHandle, RenderError> CreateMeshInternal(
        const MeshView& mesh,
        bool reserved) {
        using Result = Base::Result<MeshHandle, RenderError>;

        if (mesh.vertices.empty() || mesh.indices.empty()) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidArgument,
                "SDL_GPU: a mesh requires vertices and indices"));
        }
        if (mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
            mesh.indices.size() > std::numeric_limits<std::uint32_t>::max()) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidArgument,
                "SDL_GPU: mesh element count exceeds the 32-bit limit"));
        }
        for (std::uint32_t index : mesh.indices) {
            if (index >= mesh.vertices.size()) {
                return Result::Err(MakeError(
                    RenderErrorCode::InvalidArgument,
                    "SDL_GPU: mesh index lies outside the vertex array"));
            }
        }
        for (const Vertex3D& vertex : mesh.vertices) {
            if (!IsFinite(vertex.position) || !IsFinite(vertex.uv.x) ||
                !IsFinite(vertex.uv.y)) {
                return Result::Err(MakeError(
                    RenderErrorCode::InvalidArgument,
                    "SDL_GPU: mesh vertices must be finite"));
            }
        }

        const auto vertexBytes = std::as_bytes(mesh.vertices);
        const auto indexBytes = std::as_bytes(mesh.indices);
        if (vertexBytes.size() > std::numeric_limits<Uint32>::max() ||
            indexBytes.size() > std::numeric_limits<Uint32>::max()) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidArgument,
                "SDL_GPU: mesh buffers exceed the 32-bit size limit"));
        }

        SDL_GPUBufferCreateInfo vertexInfo{};
        vertexInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vertexInfo.size = static_cast<Uint32>(vertexBytes.size());
        SDL_GPUBuffer* vertexBuffer = SDL_CreateGPUBuffer(device, &vertexInfo);
        if (vertexBuffer == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to create a vertex buffer"));
        }

        SDL_GPUBufferCreateInfo indexInfo{};
        indexInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        indexInfo.size = static_cast<Uint32>(indexBytes.size());
        SDL_GPUBuffer* indexBuffer = SDL_CreateGPUBuffer(device, &indexInfo);
        if (indexBuffer == nullptr) {
            SDL_ReleaseGPUBuffer(device, vertexBuffer);
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to create an index buffer"));
        }

        auto upload = UploadBuffers(
            vertexBuffer, vertexBytes, indexBuffer, indexBytes);
        if (!upload) {
            SDL_ReleaseGPUBuffer(device, indexBuffer);
            SDL_ReleaseGPUBuffer(device, vertexBuffer);
            return Result::Err(std::move(upload).error());
        }

        std::uint32_t slotIndex{};
        if (freeMeshes.empty()) {
            slotIndex = static_cast<std::uint32_t>(meshes.size());
            meshes.emplace_back();
        } else {
            slotIndex = freeMeshes.back();
            freeMeshes.pop_back();
        }
        MeshSlot& slot = meshes[slotIndex];
        slot.vertexBuffer = vertexBuffer;
        slot.indexBuffer = indexBuffer;
        slot.vertexCount = static_cast<std::uint32_t>(mesh.vertices.size());
        slot.indexCount = static_cast<std::uint32_t>(mesh.indices.size());
        slot.live = true;
        slot.reserved = reserved;
        return Result::Ok(MeshHandle::FromParts(slotIndex, slot.generation));
    }

    [[nodiscard]] Base::Result<TextureHandle, RenderError> CreateTextureInternal(
        const ImageView& image,
        bool reserved) {
        using Result = Base::Result<TextureHandle, RenderError>;

        if (image.width == 0 || image.height == 0) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidArgument,
                "SDL_GPU: texture dimensions must be positive"));
        }
        const std::uint64_t tightRowPitch =
            static_cast<std::uint64_t>(image.width) * 4U;
        const std::uint64_t sourceRowPitch =
            image.rowPitch == 0 ? tightRowPitch : image.rowPitch;
        if (sourceRowPitch < tightRowPitch) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidArgument,
                "SDL_GPU: texture row pitch is smaller than width * 4"));
        }
        const std::uint64_t requiredSourceBytes =
            sourceRowPitch * (image.height - 1U) + tightRowPitch;
        const std::uint64_t tightSize = tightRowPitch * image.height;
        if (requiredSourceBytes > image.rgba8.size() ||
            tightSize > std::numeric_limits<Uint32>::max()) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidArgument,
                "SDL_GPU: texture pixel span is too small or too large"));
        }

        const SDL_GPUTextureFormat format =
            image.colorSpace == TextureColorSpace::SRgb
                ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        if (!SDL_GPUTextureSupportsFormat(
                device,
                format,
                SDL_GPU_TEXTURETYPE_2D,
                SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
            return Result::Err(MakeError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: requested RGBA8 texture format is unsupported"));
        }

        SDL_GPUTextureCreateInfo textureInfo{};
        textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureInfo.format = format;
        textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        textureInfo.width = image.width;
        textureInfo.height = image.height;
        textureInfo.layer_count_or_depth = 1;
        textureInfo.num_levels = 1;
        textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &textureInfo);
        if (texture == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to create a texture"));
        }

        SDL_GPUTransferBufferCreateInfo transferInfo{};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<Uint32>(tightSize);
        SDL_GPUTransferBuffer* transfer =
            SDL_CreateGPUTransferBuffer(device, &transferInfo);
        if (transfer == nullptr) {
            SDL_ReleaseGPUTexture(device, texture);
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to create a texture transfer buffer"));
        }

        void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
        if (mapped == nullptr) {
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to map a texture transfer buffer"));
        }
        for (std::uint32_t row = 0; row < image.height; ++row) {
            std::memcpy(
                static_cast<std::byte*>(mapped) + tightRowPitch * row,
                image.rgba8.data() + sourceRowPitch * row,
                static_cast<std::size_t>(tightRowPitch));
        }
        SDL_UnmapGPUTransferBuffer(device, transfer);

        SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
        if (command == nullptr) {
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: failed to acquire a texture upload command buffer"));
        }
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(command);
        if (copyPass == nullptr) {
            static_cast<void>(SDL_CancelGPUCommandBuffer(command));
            SDL_ReleaseGPUTransferBuffer(device, transfer);
            SDL_ReleaseGPUTexture(device, texture);
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: failed to begin a texture copy pass"));
        }

        const SDL_GPUTextureTransferInfo source{
            transfer,
            0,
            image.width,
            image.height,
        };
        const SDL_GPUTextureRegion destination{
            texture,
            0,
            0,
            0,
            0,
            0,
            image.width,
            image.height,
            1,
        };
        SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
        SDL_EndGPUCopyPass(copyPass);

        const bool submitted = SDL_SubmitGPUCommandBuffer(command);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        if (!submitted) {
            SDL_ReleaseGPUTexture(device, texture);
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: texture upload submission failed"));
        }

        std::uint32_t slotIndex{};
        if (freeTextures.empty()) {
            slotIndex = static_cast<std::uint32_t>(textures.size());
            textures.emplace_back();
        } else {
            slotIndex = freeTextures.back();
            freeTextures.pop_back();
        }
        TextureSlot& slot = textures[slotIndex];
        slot.texture = texture;
        slot.description = {image.width, image.height,
            image.colorSpace == TextureColorSpace::SRgb ? TextureFormat::Rgba8SRgb : TextureFormat::Rgba8Unorm,
            true, false, false};
        slot.live = true;
        slot.reserved = reserved;
        return Result::Ok(TextureHandle::FromParts(slotIndex, slot.generation));
    }

    [[nodiscard]] Base::Result<MeshHandle, RenderError> CreateMesh(
        const MeshView& mesh) {
        auto thread = CheckThread();
        if (!thread) {
            return Base::Result<MeshHandle, RenderError>::Err(
                std::move(thread).error());
        }
        return CreateMeshInternal(mesh, false);
    }

    [[nodiscard]] Base::Result<TextureHandle, RenderError> CreateTexture(
        const ImageView& image) {
        auto thread = CheckThread();
        if (!thread) {
            return Base::Result<TextureHandle, RenderError>::Err(
                std::move(thread).error());
        }
        return CreateTextureInternal(image, false);
    }

    [[nodiscard]] Base::Result<void, RenderError> UpdateMeshVertices(
        MeshHandle handle, std::span<const Vertex3D> vertices) {
        using Result = Base::Result<void, RenderError>;
        auto thread = CheckThread();
        if (!thread) {
            return thread;
        }
        auto found = FindMesh(handle);
        if (!found) {
            return Result::Err(std::move(found).error());
        }
        MeshSlot& slot = *found.value();
        if (slot.reserved) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidHandle,
                "SDL_GPU: backend-owned mesh cannot be updated"));
        }
        if (vertices.size() != slot.vertexCount) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidArgument,
                "SDL_GPU: vertex updates must preserve the mesh vertex count"));
        }
        for (const Vertex3D& vertex : vertices) {
            if (!IsFinite(vertex.position) || !IsFinite(vertex.uv.x) ||
                !IsFinite(vertex.uv.y)) {
                return Result::Err(MakeError(
                    RenderErrorCode::InvalidArgument,
                    "SDL_GPU: updated mesh vertices must be finite"));
            }
        }
        const auto bytes = std::as_bytes(vertices);
        // Allocate staging storage once per updated mesh. Both staging and
        // destination cycling preserve resources still used by earlier frames.
        if (slot.vertexTransfer == nullptr) {
            SDL_GPUTransferBufferCreateInfo info{};
            info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            info.size = static_cast<Uint32>(bytes.size());
            slot.vertexTransfer = SDL_CreateGPUTransferBuffer(device, &info);
            if (slot.vertexTransfer == nullptr) {
                return Result::Err(MakeSdlError(
                    RenderErrorCode::ResourceCreationFailed,
                    "SDL_GPU: failed to create a vertex update transfer buffer"));
            }
        }
        void* mapped = SDL_MapGPUTransferBuffer(device, slot.vertexTransfer, true);
        if (mapped == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to map a vertex update transfer buffer"));
        }
        std::memcpy(mapped, bytes.data(), bytes.size());
        SDL_UnmapGPUTransferBuffer(device, slot.vertexTransfer);

        SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
        if (command == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: failed to acquire a vertex update command buffer"));
        }
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(command);
        if (copy == nullptr) {
            static_cast<void>(SDL_CancelGPUCommandBuffer(command));
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: failed to begin a vertex update copy pass"));
        }
        const SDL_GPUTransferBufferLocation source{slot.vertexTransfer, 0};
        const SDL_GPUBufferRegion destination{
            slot.vertexBuffer, 0, static_cast<Uint32>(bytes.size()),
        };
        SDL_UploadToGPUBuffer(copy, &source, &destination, true);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: vertex update submission failed"));
        }
        return Result::Ok();
    }

    [[nodiscard]] Base::Result<void, RenderError> ReleaseMesh(MeshHandle handle) {
        using Result = Base::Result<void, RenderError>;
        auto thread = CheckThread();
        if (!thread) {
            return thread;
        }
        auto found = FindMesh(handle);
        if (!found) {
            return Result::Err(std::move(found).error());
        }
        MeshSlot& slot = *found.value();
        if (slot.reserved) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidHandle,
                "SDL_GPU: backend-owned mesh cannot be released"));
        }
        SDL_ReleaseGPUBuffer(device, slot.indexBuffer);
        SDL_ReleaseGPUBuffer(device, slot.vertexBuffer);
        if (slot.vertexTransfer != nullptr) {
            SDL_ReleaseGPUTransferBuffer(device, slot.vertexTransfer);
        }
        slot.indexBuffer = nullptr;
        slot.vertexBuffer = nullptr;
        slot.vertexTransfer = nullptr;
        slot.vertexCount = 0;
        slot.indexCount = 0;
        slot.live = false;
        slot.generation = NextGeneration(slot.generation);
        freeMeshes.push_back(handle.Index());
        return Result::Ok();
    }

    [[nodiscard]] Base::Result<void, RenderError> ReleaseTexture(
        TextureHandle handle) {
        using Result = Base::Result<void, RenderError>;
        auto thread = CheckThread();
        if (!thread) {
            return thread;
        }
        auto found = FindTexture(handle);
        if (!found) {
            return Result::Err(std::move(found).error());
        }
        TextureSlot& slot = *found.value();
        if (slot.reserved) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidHandle,
                "SDL_GPU: backend-owned texture cannot be released"));
        }
        SDL_ReleaseGPUTexture(device, slot.texture);
        slot.texture = nullptr;
        slot.live = false;
        slot.generation = NextGeneration(slot.generation);
        freeTextures.push_back(handle.Index());
        return Result::Ok();
    }

    Base::Result<TextureHandle, RenderError> CreateTexture(const TextureDesc& description) {
        using Result = Base::Result<TextureHandle, RenderError>;
        auto thread = CheckThread(); if (!thread) return Result::Err(thread.error());
        const auto format = ToSdlFormat(description.format);
        if (!description.width || !description.height || format == SDL_GPU_TEXTUREFORMAT_INVALID ||
            (description.depthTarget && (description.colorTarget || description.format != TextureFormat::Depth32Float)) ||
            (!description.depthTarget && description.format == TextureFormat::Depth32Float))
            return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: invalid texture descriptor"));
        SDL_GPUTextureUsageFlags usage = 0;
        if (description.sampled) usage |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
        if (description.colorTarget) usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        if (description.depthTarget) usage |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        if (!usage || !SDL_GPUTextureSupportsFormat(device, format, SDL_GPU_TEXTURETYPE_2D, usage))
            return Result::Err(MakeError(RenderErrorCode::UnsupportedOperation, "SDL_GPU: unsupported texture format/usage"));
        SDL_GPUTextureCreateInfo create{};
        create.type = SDL_GPU_TEXTURETYPE_2D; create.format = format; create.usage = usage;
        create.width = description.width; create.height = description.height;
        create.layer_count_or_depth = 1; create.num_levels = 1; create.sample_count = SDL_GPU_SAMPLECOUNT_1;
        auto* texture = SDL_CreateGPUTexture(device, &create);
        if (!texture) return Result::Err(MakeSdlError(RenderErrorCode::ResourceCreationFailed, "SDL_GPU: texture creation failed"));
        std::uint32_t index;
        if (freeTextures.empty()) { index = static_cast<std::uint32_t>(textures.size()); textures.emplace_back(); }
        else { index = freeTextures.back(); freeTextures.pop_back(); }
        auto& slot = textures[index]; slot.texture = texture; slot.description = description;
        slot.live = true; slot.reserved = false;
        return Result::Ok(TextureHandle::FromParts(index, slot.generation));
    }

    Base::Result<ShaderHandle, RenderError> CreateShader(const ShaderArtifact& artifact) {
        using Result = Base::Result<ShaderHandle, RenderError>;
        auto thread = CheckThread(); if (!thread) return Result::Err(thread.error());
        if ((artifact.stage != ShaderStage::Vertex && artifact.stage != ShaderStage::Fragment) ||
            (artifact.format != ShaderFormat::DXIL && artifact.format != ShaderFormat::SPIRV && artifact.format != ShaderFormat::Metallib))
            return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: invalid shader stage or format"));
        if (artifact.code.empty() || artifact.entrypoint.empty() ||
            !(info.shaderFormats & FormatBit(artifact.format)) || artifact.resources.storageTextures ||
            artifact.resources.storageBuffers || artifact.resources.uniformBufferSizes.size() > 4 ||
            (artifact.stage == ShaderStage::Vertex && artifact.resources.samplers))
            return Result::Err(MakeError(RenderErrorCode::UnsupportedOperation, "SDL_GPU: unsupported shader artifact or binding layout"));
        SDL_GPUShaderCreateInfo create{};
        create.code_size = artifact.code.size(); create.code = reinterpret_cast<const Uint8*>(artifact.code.data());
        create.entrypoint = artifact.entrypoint.c_str(); create.format = ToSdlFormats(FormatBit(artifact.format));
        create.stage = artifact.stage == ShaderStage::Vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
        create.num_samplers = artifact.resources.samplers;
        create.num_uniform_buffers = static_cast<Uint32>(artifact.resources.uniformBufferSizes.size());
        auto* shader = SDL_CreateGPUShader(device, &create);
        if (!shader) return Result::Err(MakeSdlError(RenderErrorCode::ResourceCreationFailed, "SDL_GPU: shader creation failed"));
        std::uint32_t index;
        if (freeShaders.empty()) { index = static_cast<std::uint32_t>(shaders.size()); shaders.emplace_back(); }
        else { index = freeShaders.back(); freeShaders.pop_back(); }
        auto& slot = shaders[index]; slot.shader = shader; slot.resources = artifact.resources;
        slot.stage = artifact.stage; slot.format = artifact.format; slot.live = true;
        return Result::Ok(ShaderHandle::FromParts(index, slot.generation));
    }
    Base::Result<ShaderSlot*, RenderError> FindShader(ShaderHandle handle) {
        using Result = Base::Result<ShaderSlot*, RenderError>;
        if (!handle || handle.Index() >= shaders.size() || !shaders[handle.Index()].live ||
            shaders[handle.Index()].generation != handle.Generation())
            return Result::Err(MakeError(RenderErrorCode::InvalidHandle, "SDL_GPU: invalid or stale shader handle"));
        return Result::Ok(&shaders[handle.Index()]);
    }
    Base::Result<void, RenderError> ReleaseShader(ShaderHandle handle) {
        using Result = Base::Result<void, RenderError>;
        auto thread = CheckThread(); if (!thread) return thread;
        auto found = FindShader(handle); if (!found) return Result::Err(found.error());
        auto& slot = *found.value(); SDL_ReleaseGPUShader(device, slot.shader);
        slot.shader = nullptr; slot.live = false; slot.resources = {}; slot.generation = NextGeneration(slot.generation);
        freeShaders.push_back(handle.Index()); return Result::Ok();
    }
    Base::Result<PipelineHandle, RenderError> CreatePipeline(const PipelineDesc& description) {
        using Result = Base::Result<PipelineHandle, RenderError>;
        auto thread = CheckThread(); if (!thread) return Result::Err(thread.error());
        auto vertex = FindShader(description.vertexShader); if (!vertex) return Result::Err(vertex.error());
        auto fragment = FindShader(description.fragmentShader); if (!fragment) return Result::Err(fragment.error());
        if (vertex.value()->stage != ShaderStage::Vertex || fragment.value()->stage != ShaderStage::Fragment ||
            vertex.value()->format != fragment.value()->format ||
            ToSdlFormat(description.colorFormat) == SDL_GPU_TEXTUREFORMAT_INVALID ||
            description.colorFormat == TextureFormat::Depth32Float ||
            (description.cull != CullMode::None && description.cull != CullMode::Front && description.cull != CullMode::Back) ||
            (description.depthWrite && !description.depthTest))
            return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: invalid pipeline descriptor"));
        const auto cull = description.cull == CullMode::None ? SDL_GPU_CULLMODE_NONE :
            description.cull == CullMode::Front ? SDL_GPU_CULLMODE_FRONT : SDL_GPU_CULLMODE_BACK;
        auto* pipeline = CreateNativePipeline(vertex.value()->shader, fragment.value()->shader, cull, description.blend,
            description.depthTest, description.depthWrite, ToSdlFormat(description.colorFormat), description.vertexInput,
            description.clockwiseFrontFace);
        if (!pipeline) return Result::Err(MakeSdlError(RenderErrorCode::ResourceCreationFailed, "SDL_GPU: pipeline creation failed"));
        std::uint32_t index;
        if (freePipelines.empty()) { index = static_cast<std::uint32_t>(pipelines.size()); pipelines.emplace_back(); }
        else { index = freePipelines.back(); freePipelines.pop_back(); }
        auto& slot = pipelines[index]; slot.pipeline = pipeline; slot.description = description; slot.live = true;
        slot.vertexResources = vertex.value()->resources; slot.fragmentResources = fragment.value()->resources;
        return Result::Ok(PipelineHandle::FromParts(index, slot.generation));
    }
    Base::Result<PipelineSlot*, RenderError> FindPipeline(PipelineHandle handle) {
        using Result = Base::Result<PipelineSlot*, RenderError>;
        if (!handle || handle.Index() >= pipelines.size() || !pipelines[handle.Index()].live ||
            pipelines[handle.Index()].generation != handle.Generation())
            return Result::Err(MakeError(RenderErrorCode::InvalidHandle, "SDL_GPU: invalid or stale pipeline handle"));
        return Result::Ok(&pipelines[handle.Index()]);
    }
    Base::Result<void, RenderError> ReleasePipeline(PipelineHandle handle) {
        using Result = Base::Result<void, RenderError>;
        auto thread = CheckThread(); if (!thread) return thread;
        auto found = FindPipeline(handle); if (!found) return Result::Err(found.error());
        auto& slot = *found.value(); SDL_ReleaseGPUGraphicsPipeline(device, slot.pipeline);
        slot.pipeline = nullptr; slot.live = false; slot.description = {}; slot.generation = NextGeneration(slot.generation);
        freePipelines.push_back(handle.Index()); return Result::Ok();
    }
    Base::Result<std::optional<AcquiredFrame>, RenderError> AcquireFrame() {
        using Result = Base::Result<std::optional<AcquiredFrame>, RenderError>;
        auto thread = CheckThread(); if (!thread) return Result::Err(thread.error());
        if (frameCommand) return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: a frame is already acquired"));
        auto* command = SDL_AcquireGPUCommandBuffer(device);
        if (!command) return Result::Err(MakeSdlError(RenderErrorCode::SubmissionFailed, "SDL_GPU: command buffer acquisition failed"));
        SDL_GPUTexture* swapchain{}; Uint32 width{}, height{};
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, window, &swapchain, &width, &height)) {
            static_cast<void>(SDL_CancelGPUCommandBuffer(command));
            return Result::Err(MakeSdlError(RenderErrorCode::SubmissionFailed, "SDL_GPU: swapchain acquisition failed"));
        }
        if (!swapchain || !width || !height) {
            if (swapchain) static_cast<void>(SDL_SubmitGPUCommandBuffer(command));
            else static_cast<void>(SDL_CancelGPUCommandBuffer(command));
            return Result::Ok(std::nullopt);
        }
        auto token = nextFrameToken.fetch_add(1, std::memory_order_relaxed);
        if (!token) token = nextFrameToken.fetch_add(1, std::memory_order_relaxed);
        activeFrame = {token, width, height, swapchainFormat == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
            ? TextureFormat::Rgba8SRgb : TextureFormat::Bgra8SRgb};
        frameCommand = command; frameSwapchain = swapchain;
        return Result::Ok(activeFrame);
    }
    void AbandonFrame(const AcquiredFrame& frame) noexcept {
        if (!frameCommand || frame.token != activeFrame.token || std::this_thread::get_id() != ownerThread) return;
        static_cast<void>(SDL_SubmitGPUCommandBuffer(frameCommand));
        frameCommand = nullptr; frameSwapchain = nullptr; activeFrame = {};
    }
    Base::Result<void, RenderError> Validate(const PreparedFrame& prepared) {
        using Result = Base::Result<void, RenderError>;
        for (const auto& pass : prepared.passes) {
            TextureFormat colorFormat = activeFrame.colorFormat;
            std::uint32_t width = activeFrame.width, height = activeFrame.height;
            if (!IsFinite(pass.clearColor) || !IsFinite(pass.clearDepth) || pass.clearDepth < 0 || pass.clearDepth > 1)
                return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: invalid attachment clear value"));
            const auto validLoad = [](AttachmentLoad load) {
                return load == AttachmentLoad::Clear || load == AttachmentLoad::Load || load == AttachmentLoad::DontCare;
            };
            if (!validLoad(pass.colorLoad) || !validLoad(pass.depthLoad))
                return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: invalid attachment load operation"));
            if (pass.color) {
                auto found = FindTexture(pass.color); if (!found) return Result::Err(found.error());
                const auto& desc = found.value()->description;
                if (!desc.colorTarget) return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: color attachment is not a render target"));
                colorFormat = desc.format; width = desc.width; height = desc.height;
            }
            if (pass.depth) {
                auto found = FindTexture(pass.depth); if (!found) return Result::Err(found.error());
                const auto& desc = found.value()->description;
                if (!desc.depthTarget || desc.width != width || desc.height != height)
                    return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: incompatible depth attachment"));
            }
            for (const auto& draw : pass.draws) {
                auto pipeline = FindPipeline(draw.pipeline); if (!pipeline) return Result::Err(pipeline.error());
                const auto& desc = pipeline.value()->description;
                if (desc.colorFormat != colorFormat || desc.depthTest != static_cast<bool>(pass.depth) ||
                    desc.vertexInput != static_cast<bool>(draw.mesh) || (!draw.mesh && !draw.vertexCount))
                    return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: draw pipeline/attachment/vertex mismatch"));
                if (draw.mesh) { auto found = FindMesh(draw.mesh); if (!found) return Result::Err(found.error()); }
                const auto uniformsMatch = [](const auto& supplied, const auto& sizes) {
                    if (supplied.size() != sizes.size()) return false;
                    for (std::size_t i = 0; i < sizes.size(); ++i) if (supplied[i].size() != sizes[i]) return false;
                    return true;
                };
                if (!uniformsMatch(draw.vertexUniforms, pipeline.value()->vertexResources.uniformBufferSizes) ||
                    !uniformsMatch(draw.fragmentUniforms, pipeline.value()->fragmentResources.uniformBufferSizes) ||
                    draw.fragmentTextures.size() != pipeline.value()->fragmentResources.samplers)
                    return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: shader resource bindings disagree with artifact metadata"));
                for (const auto& binding : draw.fragmentTextures) {
                    auto texture = FindTexture(binding.texture); if (!texture) return Result::Err(texture.error());
                    if (!texture.value()->description.sampled || binding.texture == pass.color || binding.texture == pass.depth ||
                        (binding.sampler != SamplerMode::LinearClamp && binding.sampler != SamplerMode::LinearWrap))
                        return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: invalid sampled texture binding"));
                }
            }
        }
        return Result::Ok();
    }
    Base::Result<PresentStatus, RenderError> SubmitFrame(const AcquiredFrame& frame, const PreparedFrame& prepared) {
        using Result = Base::Result<PresentStatus, RenderError>;
        auto thread = CheckThread(); if (!thread) return Result::Err(thread.error());
        if (!frameCommand || !frame.token || frame.token != activeFrame.token)
            return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: invalid or consumed frame token"));
        auto valid = Validate(prepared);
        if (!valid) { AbandonFrame(frame); return Result::Err(valid.error()); }
        for (const auto& preparedPass : prepared.passes) {
            SDL_GPUColorTargetInfo color{};
            color.texture = preparedPass.color ? FindTexture(preparedPass.color).value()->texture : frameSwapchain;
            color.clear_color = {preparedPass.clearColor.red, preparedPass.clearColor.green, preparedPass.clearColor.blue, preparedPass.clearColor.alpha};
            color.load_op = ToLoad(preparedPass.colorLoad);
            color.store_op = preparedPass.storeColor ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;
            color.cycle = preparedPass.color && preparedPass.colorLoad != AttachmentLoad::Load;
            SDL_GPUDepthStencilTargetInfo depth{};
            if (preparedPass.depth) {
                depth.texture = FindTexture(preparedPass.depth).value()->texture;
                depth.clear_depth = preparedPass.clearDepth; depth.load_op = ToLoad(preparedPass.depthLoad);
                depth.store_op = preparedPass.storeDepth ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;
                depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE; depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
                depth.cycle = preparedPass.depthLoad != AttachmentLoad::Load;
            }
            auto* pass = SDL_BeginGPURenderPass(frameCommand, &color, 1, preparedPass.depth ? &depth : nullptr);
            if (!pass) { auto error = MakeSdlError(RenderErrorCode::SubmissionFailed, "SDL_GPU: render pass creation failed"); AbandonFrame(frame); return Result::Err(std::move(error)); }
            for (const auto& draw : preparedPass.draws) {
                SDL_BindGPUGraphicsPipeline(pass, FindPipeline(draw.pipeline).value()->pipeline);
                for (Uint32 i = 0; i < draw.vertexUniforms.size(); ++i)
                    SDL_PushGPUVertexUniformData(frameCommand, i, draw.vertexUniforms[i].data(), static_cast<Uint32>(draw.vertexUniforms[i].size()));
                for (Uint32 i = 0; i < draw.fragmentUniforms.size(); ++i)
                    SDL_PushGPUFragmentUniformData(frameCommand, i, draw.fragmentUniforms[i].data(), static_cast<Uint32>(draw.fragmentUniforms[i].size()));
                for (Uint32 i = 0; i < draw.fragmentTextures.size(); ++i) {
                    const auto& binding = draw.fragmentTextures[i];
                    const SDL_GPUTextureSamplerBinding sampler{FindTexture(binding.texture).value()->texture,
                        binding.sampler == SamplerMode::LinearWrap ? linearWrapSampler : linearClampSampler};
                    SDL_BindGPUFragmentSamplers(pass, i, &sampler, 1);
                }
                if (draw.mesh) {
                    const auto& mesh = *FindMesh(draw.mesh).value();
                    const SDL_GPUBufferBinding vertex{mesh.vertexBuffer, 0}, index{mesh.indexBuffer, 0};
                    SDL_BindGPUVertexBuffers(pass, 0, &vertex, 1);
                    SDL_BindGPUIndexBuffer(pass, &index, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                    SDL_DrawGPUIndexedPrimitives(pass, mesh.indexCount, 1, 0, 0, 0);
                } else SDL_DrawGPUPrimitives(pass, draw.vertexCount, 1, 0, 0);
            }
            SDL_EndGPURenderPass(pass);
        }
        auto* command = frameCommand; frameCommand = nullptr; frameSwapchain = nullptr; activeFrame = {};
        if (!SDL_SubmitGPUCommandBuffer(command)) return Result::Err(MakeSdlError(RenderErrorCode::SubmissionFailed, "SDL_GPU: frame submission failed"));
        return Result::Ok(PresentStatus::Presented);
    }
    Base::Result<TextureReadback, RenderError> ReadTexture(TextureHandle handle) {
        using Result = Base::Result<TextureReadback, RenderError>;
        auto thread = CheckThread(); if (!thread) return Result::Err(thread.error());
        auto found = FindTexture(handle); if (!found) return Result::Err(found.error());
        if (frameCommand) return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: readback requires a submitted frame"));
        const auto& desc = found.value()->description;
        if (desc.format == TextureFormat::Depth32Float)
            return Result::Err(MakeError(RenderErrorCode::UnsupportedOperation, "SDL_GPU: depth readback is unsupported"));
        const std::uint32_t bpp = desc.format == TextureFormat::Rgba16Float ? 8U : 4U;
        const std::uint64_t pitch = (static_cast<std::uint64_t>(desc.width) * bpp + 255U) & ~255ULL;
        const std::uint64_t total = pitch * desc.height;
        if (total > std::numeric_limits<Uint32>::max())
            return Result::Err(MakeError(RenderErrorCode::InvalidArgument, "SDL_GPU: readback exceeds transfer limits"));
        TextureReadback output{desc.width, desc.height, static_cast<std::uint32_t>(pitch), desc.format, {}};
        output.bytes.resize(static_cast<std::size_t>(total));
        SDL_GPUTransferBufferCreateInfo create{}; create.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD; create.size = static_cast<Uint32>(total);
        auto* transfer = SDL_CreateGPUTransferBuffer(device, &create);
        if (!transfer) return Result::Err(MakeSdlError(RenderErrorCode::ResourceCreationFailed, "SDL_GPU: readback transfer allocation failed"));
        auto* command = SDL_AcquireGPUCommandBuffer(device);
        if (!command) { SDL_ReleaseGPUTransferBuffer(device, transfer); return Result::Err(MakeSdlError(RenderErrorCode::SubmissionFailed, "SDL_GPU: readback command acquisition failed")); }
        auto* copy = SDL_BeginGPUCopyPass(command);
        if (!copy) { static_cast<void>(SDL_CancelGPUCommandBuffer(command)); SDL_ReleaseGPUTransferBuffer(device, transfer); return Result::Err(MakeSdlError(RenderErrorCode::SubmissionFailed, "SDL_GPU: readback copy pass failed")); }
        SDL_GPUTextureRegion source{}; source.texture = found.value()->texture; source.w = desc.width; source.h = desc.height; source.d = 1;
        SDL_GPUTextureTransferInfo destination{}; destination.transfer_buffer = transfer; destination.pixels_per_row = static_cast<Uint32>(pitch / bpp); destination.rows_per_layer = desc.height;
        SDL_DownloadFromGPUTexture(copy, &source, &destination); SDL_EndGPUCopyPass(copy);
        auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
        if (!fence) { SDL_ReleaseGPUTransferBuffer(device, transfer); return Result::Err(MakeSdlError(RenderErrorCode::SubmissionFailed, "SDL_GPU: readback submission failed")); }
        const bool waited = SDL_WaitForGPUFences(device, true, &fence, 1); SDL_ReleaseGPUFence(device, fence);
        if (!waited) { SDL_ReleaseGPUTransferBuffer(device, transfer); return Result::Err(MakeSdlError(RenderErrorCode::SubmissionFailed, "SDL_GPU: readback wait failed")); }
        const void* data = SDL_MapGPUTransferBuffer(device, transfer, false);
        if (!data) { SDL_ReleaseGPUTransferBuffer(device, transfer); return Result::Err(MakeSdlError(RenderErrorCode::ResourceCreationFailed, "SDL_GPU: readback mapping failed")); }
        std::memcpy(output.bytes.data(), data, output.bytes.size()); SDL_UnmapGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTransferBuffer(device, transfer); return Result::Ok(std::move(output));
    }
};

Base::Result<std::unique_ptr<SdlGpuRenderDevice>, RenderError> SdlGpuRenderDevice::Create(
    Platform::Sdl::SdlPlatform& platform, const SdlGpuOptions& options) {
    using Result = Base::Result<std::unique_ptr<SdlGpuRenderDevice>, RenderError>;
    auto impl = std::make_unique<Impl>(); auto initialized = impl->Initialize(platform, options);
    if (!initialized) return Result::Err(initialized.error());
    return Result::Ok(std::unique_ptr<SdlGpuRenderDevice>(new SdlGpuRenderDevice(std::move(impl))));
}
SdlGpuRenderDevice::SdlGpuRenderDevice(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
SdlGpuRenderDevice::~SdlGpuRenderDevice() = default;
RenderDeviceInfo SdlGpuRenderDevice::GetInfo() const { return impl_->info; }
Base::Result<ShaderHandle, RenderError> SdlGpuRenderDevice::CreateShader(const ShaderArtifact& artifact) { return impl_->CreateShader(artifact); }
Base::Result<void, RenderError> SdlGpuRenderDevice::ReleaseShader(ShaderHandle handle) { return impl_->ReleaseShader(handle); }
Base::Result<MeshHandle, RenderError> SdlGpuRenderDevice::CreateMesh(const MeshView& mesh) { return impl_->CreateMesh(mesh); }
Base::Result<TextureHandle, RenderError> SdlGpuRenderDevice::CreateTexture(const ImageView& image) { return impl_->CreateTexture(image); }
Base::Result<TextureHandle, RenderError> SdlGpuRenderDevice::CreateTexture(const TextureDesc& description) { return impl_->CreateTexture(description); }
Base::Result<void, RenderError> SdlGpuRenderDevice::UpdateMeshVertices(MeshHandle handle, std::span<const Vertex3D> vertices) { return impl_->UpdateMeshVertices(handle, vertices); }
Base::Result<void, RenderError> SdlGpuRenderDevice::ReleaseMesh(MeshHandle handle) { return impl_->ReleaseMesh(handle); }
Base::Result<void, RenderError> SdlGpuRenderDevice::ReleaseTexture(TextureHandle handle) { return impl_->ReleaseTexture(handle); }
Base::Result<PipelineHandle, RenderError> SdlGpuRenderDevice::CreatePipeline(const PipelineDesc& description) { return impl_->CreatePipeline(description); }
Base::Result<void, RenderError> SdlGpuRenderDevice::ReleasePipeline(PipelineHandle handle) { return impl_->ReleasePipeline(handle); }
Base::Result<std::optional<AcquiredFrame>, RenderError> SdlGpuRenderDevice::AcquireFrame() { return impl_->AcquireFrame(); }
Base::Result<PresentStatus, RenderError> SdlGpuRenderDevice::SubmitFrame(const AcquiredFrame& frame, const PreparedFrame& prepared) { return impl_->SubmitFrame(frame, prepared); }
void SdlGpuRenderDevice::AbandonFrame(const AcquiredFrame& frame) noexcept { impl_->AbandonFrame(frame); }
Base::Result<TextureReadback, RenderError> SdlGpuRenderDevice::ReadTexture(TextureHandle handle) { return impl_->ReadTexture(handle); }
} // namespace Engine::Render::Backend::SdlGpu
