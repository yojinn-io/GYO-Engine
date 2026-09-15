#include "render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"

#include <SDL3/SDL_gpu.h>

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "SdlGpuEmbeddedShaders.hpp"
#include "render/ColorTransform.hpp"
#include "render/PrimitiveMesh.hpp"

namespace Engine::Render::Backend::SdlGpu {
namespace {

using Microsoft::WRL::ComPtr;

constexpr SDL_GPUTextureFormat kDepthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
constexpr SDL_GPUTextureFormat kSceneColorFormat =
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

struct Matrix4 final {
    float values[4][4]{};
};

struct alignas(16) VertexUniforms final {
    Matrix4 worldViewProjection{};
};

struct alignas(16) FragmentUniforms final {
    float tint[4]{};
    float uvScaleOffset[4]{};
    float alphaCutoff{-1.0F};
    float padding[3]{};
};

struct alignas(16) SceneColorUniforms final {
    float exposureEv{};
    float gammaAdjustment{1.0F};
    float padding[2]{};
};

static_assert(sizeof(VertexUniforms) == 64);
static_assert(sizeof(FragmentUniforms) == 48);
static_assert(sizeof(SceneColorUniforms) == 16);

[[nodiscard]] Matrix4 Identity() noexcept {
    Matrix4 result{};
    result.values[0][0] = 1.0F;
    result.values[1][1] = 1.0F;
    result.values[2][2] = 1.0F;
    result.values[3][3] = 1.0F;
    return result;
}

[[nodiscard]] Matrix4 Multiply(const Matrix4& left, const Matrix4& right) noexcept {
    Matrix4 result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result.values[row][column] +=
                    left.values[row][inner] * right.values[inner][column];
            }
        }
    }
    return result;
}

[[nodiscard]] Matrix4 Translation(Float3 value) noexcept {
    Matrix4 result = Identity();
    result.values[3][0] = value.x;
    result.values[3][1] = value.y;
    result.values[3][2] = value.z;
    return result;
}

[[nodiscard]] Matrix4 Scale(Float3 value) noexcept {
    Matrix4 result = Identity();
    result.values[0][0] = value.x;
    result.values[1][1] = value.y;
    result.values[2][2] = value.z;
    return result;
}

[[nodiscard]] Matrix4 RotationX(float radians) noexcept {
    Matrix4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[1][1] = cosine;
    result.values[1][2] = sine;
    result.values[2][1] = -sine;
    result.values[2][2] = cosine;
    return result;
}

[[nodiscard]] Matrix4 RotationY(float radians) noexcept {
    Matrix4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[0][0] = cosine;
    result.values[0][2] = -sine;
    result.values[2][0] = sine;
    result.values[2][2] = cosine;
    return result;
}

[[nodiscard]] Matrix4 RotationZ(float radians) noexcept {
    Matrix4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[0][0] = cosine;
    result.values[0][1] = sine;
    result.values[1][0] = -sine;
    result.values[1][1] = cosine;
    return result;
}

[[nodiscard]] Matrix4 WorldMatrix(const Transform3D& transform) noexcept {
    Matrix4 result = Scale(transform.scale);
    result = Multiply(result, RotationX(transform.rotationRadians.x));
    result = Multiply(result, RotationY(transform.rotationRadians.y));
    result = Multiply(result, RotationZ(transform.rotationRadians.z));
    return Multiply(result, Translation(transform.translation));
}

[[nodiscard]] Matrix4 ViewMatrix(const PerspectiveCamera3D& camera) noexcept {
    Matrix4 result = Translation({
        -camera.position.x,
        -camera.position.y,
        -camera.position.z,
    });
    result = Multiply(result, RotationZ(-camera.rotationRadians.z));
    result = Multiply(result, RotationY(-camera.rotationRadians.y));
    return Multiply(result, RotationX(-camera.rotationRadians.x));
}

[[nodiscard]] Matrix4 ProjectionMatrix(
    const PerspectiveCamera3D& camera,
    float aspectRatio) noexcept {
    Matrix4 result{};
    const float yScale = 1.0F /
                         std::tan(camera.verticalFieldOfViewRadians * 0.5F);
    const float xScale = yScale / aspectRatio;
    const float depthRange = camera.farClip - camera.nearClip;
    result.values[0][0] = xScale;
    result.values[1][1] = yScale;
    result.values[2][2] = camera.farClip / depthRange;
    result.values[2][3] = 1.0F;
    result.values[3][2] =
        -(camera.nearClip * camera.farClip) / depthRange;
    return result;
}

[[nodiscard]] Matrix4 SpriteWorldMatrix(
    const SpriteSubmission& sprite) noexcept {
    const Float3 localPivotTranslation{
        0.5F - sprite.pivotNormalized.x,
        0.5F - sprite.pivotNormalized.y,
        0.0F,
    };
    const Float3 anchor{
        sprite.destinationPixels.x +
            sprite.pivotNormalized.x * sprite.destinationPixels.width,
        sprite.destinationPixels.y +
            sprite.pivotNormalized.y * sprite.destinationPixels.height,
        0.0F,
    };

    Matrix4 result = Translation(localPivotTranslation);
    result = Multiply(result, Scale({
        sprite.destinationPixels.width,
        sprite.destinationPixels.height,
        1.0F,
    }));
    result = Multiply(result, RotationZ(sprite.rotationRadians));
    return Multiply(result, Translation(anchor));
}

[[nodiscard]] Matrix4 PixelProjection(float width, float height) noexcept {
    Matrix4 result{};
    result.values[0][0] = 2.0F / width;
    result.values[1][1] = -2.0F / height;
    result.values[2][2] = 1.0F;
    result.values[3][0] = -1.0F;
    result.values[3][1] = 1.0F;
    result.values[3][3] = 1.0F;
    return result;
}

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

[[nodiscard]] Base::Result<std::vector<std::uint8_t>, RenderError> CompileShader(
    std::string_view source,
    const char* sourceName,
    const char* target,
    bool debugMode) {
    using Result = Base::Result<std::vector<std::uint8_t>, RenderError>;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    flags |= debugMode
                 ? (D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION)
                 : D3DCOMPILE_OPTIMIZATION_LEVEL3;

    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> diagnostics;
    const HRESULT result = D3DCompile(
        source.data(),
        source.size(),
        sourceName,
        nullptr,
        nullptr,
        "main",
        target,
        flags,
        0,
        &bytecode,
        &diagnostics);

    if (FAILED(result) || !bytecode) {
        std::string detail;
        if (diagnostics && diagnostics->GetBufferPointer()) {
            detail.assign(
                static_cast<const char*>(diagnostics->GetBufferPointer()),
                diagnostics->GetBufferSize());
        }
        return Result::Err(MakeError(
            RenderErrorCode::ShaderCompilationFailed,
            std::string("SDL_GPU: failed to compile ") + sourceName +
                " as " + target,
            std::move(detail)));
    }

    const auto* begin = static_cast<const std::uint8_t*>(bytecode->GetBufferPointer());
    return Result::Ok(std::vector<std::uint8_t>(
        begin,
        begin + bytecode->GetBufferSize()));
}

[[nodiscard]] FragmentUniforms MakeFragmentUniforms(
    Color tint,
    UvTransform uv,
    float alphaCutoff) noexcept {
    FragmentUniforms uniforms{};
    uniforms.tint[0] = tint.red;
    uniforms.tint[1] = tint.green;
    uniforms.tint[2] = tint.blue;
    uniforms.tint[3] = tint.alpha;
    uniforms.uvScaleOffset[0] = uv.scale.x;
    uniforms.uvScaleOffset[1] = uv.scale.y;
    uniforms.uvScaleOffset[2] = uv.offset.x;
    uniforms.uvScaleOffset[3] = uv.offset.y;
    uniforms.alphaCutoff = alphaCutoff;
    return uniforms;
}

} // namespace

struct SdlGpuRenderDevice::Impl final {
    struct MeshSlot final {
        SDL_GPUBuffer* vertexBuffer{};
        SDL_GPUBuffer* indexBuffer{};
        std::uint32_t indexCount{};
        std::uint32_t generation{1};
        bool live{};
        bool reserved{};
    };

    struct TextureSlot final {
        SDL_GPUTexture* texture{};
        std::uint32_t generation{1};
        bool live{};
        bool reserved{};
    };

    Platform::Sdl::SdlPlatform* platform{};
    SDL_Window* window{};
    SDL_GPUDevice* device{};
    bool windowClaimed{};
    std::thread::id ownerThread{};
    bool debugMode{};

    SDL_GPUTextureFormat swapchainFormat{SDL_GPU_TEXTUREFORMAT_INVALID};
    SDL_GPUTexture* sceneColorTexture{};
    SDL_GPUTexture* depthTexture{};
    std::uint32_t frameTargetWidth{};
    std::uint32_t frameTargetHeight{};

    SDL_GPUSampler* linearClampSampler{};
    SDL_GPUSampler* linearWrapSampler{};

    SDL_GPUGraphicsPipeline* opaqueBackPipeline{};
    SDL_GPUGraphicsPipeline* opaqueDoublePipeline{};
    SDL_GPUGraphicsPipeline* alphaBackPipeline{};
    SDL_GPUGraphicsPipeline* alphaDoublePipeline{};
    SDL_GPUGraphicsPipeline* skyPipeline{};
    SDL_GPUGraphicsPipeline* sceneSpritePipeline{};
    SDL_GPUGraphicsPipeline* overlaySpritePipeline{};
    SDL_GPUGraphicsPipeline* scenePostPipeline{};

    std::vector<MeshSlot> meshes{};
    std::vector<std::uint32_t> freeMeshes{};
    std::vector<TextureSlot> textures{};
    std::vector<std::uint32_t> freeTextures{};
    MeshHandle spriteQuad{};
    TextureHandle whiteTexture{};

    ~Impl() {
        Shutdown();
    }

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
        if (device == nullptr) {
            return;
        }

        static_cast<void>(SDL_WaitForGPUIdle(device));

        for (MeshSlot& slot : meshes) {
            if (slot.vertexBuffer != nullptr) {
                SDL_ReleaseGPUBuffer(device, slot.vertexBuffer);
            }
            if (slot.indexBuffer != nullptr) {
                SDL_ReleaseGPUBuffer(device, slot.indexBuffer);
            }
            slot = {};
        }
        for (TextureSlot& slot : textures) {
            if (slot.texture != nullptr) {
                SDL_ReleaseGPUTexture(device, slot.texture);
            }
            slot = {};
        }

        if (sceneColorTexture != nullptr) {
            SDL_ReleaseGPUTexture(device, sceneColorTexture);
            sceneColorTexture = nullptr;
        }
        if (depthTexture != nullptr) {
            SDL_ReleaseGPUTexture(device, depthTexture);
            depthTexture = nullptr;
        }
        frameTargetWidth = 0;
        frameTargetHeight = 0;

        const std::array pipelines{
            opaqueBackPipeline,
            opaqueDoublePipeline,
            alphaBackPipeline,
            alphaDoublePipeline,
            skyPipeline,
            sceneSpritePipeline,
            overlaySpritePipeline,
            scenePostPipeline,
        };
        for (SDL_GPUGraphicsPipeline* pipeline : pipelines) {
            if (pipeline != nullptr) {
                SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
            }
        }
        opaqueBackPipeline = nullptr;
        opaqueDoublePipeline = nullptr;
        alphaBackPipeline = nullptr;
        alphaDoublePipeline = nullptr;
        skyPipeline = nullptr;
        sceneSpritePipeline = nullptr;
        overlaySpritePipeline = nullptr;
        scenePostPipeline = nullptr;

        if (linearClampSampler != nullptr) {
            SDL_ReleaseGPUSampler(device, linearClampSampler);
            linearClampSampler = nullptr;
        }
        if (linearWrapSampler != nullptr) {
            SDL_ReleaseGPUSampler(device, linearWrapSampler);
            linearWrapSampler = nullptr;
        }

        if (windowClaimed && window != nullptr) {
            SDL_ReleaseWindowFromGPUDevice(device, window);
            windowClaimed = false;
        }
        SDL_DestroyGPUDevice(device);
        device = nullptr;
    }

    [[nodiscard]] Base::Result<SDL_GPUShader*, RenderError> CreateShader(
        const std::vector<std::uint8_t>& bytecode,
        SDL_GPUShaderStage stage,
        Uint32 samplerCount,
        Uint32 uniformBufferCount) {
        using Result = Base::Result<SDL_GPUShader*, RenderError>;

        SDL_GPUShaderCreateInfo info{};
        info.code_size = bytecode.size();
        info.code = bytecode.data();
        info.entrypoint = "main";
        info.format = SDL_GPU_SHADERFORMAT_DXBC;
        info.stage = stage;
        info.num_samplers = samplerCount;
        info.num_uniform_buffers = uniformBufferCount;

        SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
        if (shader == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::ShaderCompilationFailed,
                "SDL_GPU: failed to create a shader"));
        }
        return Result::Ok(shader);
    }

    [[nodiscard]] SDL_GPUGraphicsPipeline* CreatePipeline(
        SDL_GPUShader* vertexShader,
        SDL_GPUShader* fragmentShader,
        SDL_GPUCullMode cullMode,
        bool blend,
        bool hasDepth,
        bool depthWrite,
        SDL_GPUTextureFormat targetFormat) {
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
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = attributes.data();
        info.vertex_input_state.num_vertex_attributes =
            static_cast<Uint32>(attributes.size());
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = cullMode;
        info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
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

    [[nodiscard]] SDL_GPUGraphicsPipeline* CreateScenePostPipeline(
        SDL_GPUShader* vertexShader,
        SDL_GPUShader* fragmentShader) {
        SDL_GPUColorTargetDescription colorTarget{};
        colorTarget.format = swapchainFormat;

        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vertexShader;
        info.fragment_shader = fragmentShader;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.target_info.color_target_descriptions = &colorTarget;
        info.target_info.num_color_targets = 1;
        return SDL_CreateGPUGraphicsPipeline(device, &info);
    }

    [[nodiscard]] Base::Result<void, RenderError> CreatePipelines() {
        using Result = Base::Result<void, RenderError>;

        auto vertexBytecode = CompileShader(
            EmbeddedShaders::kUnlitVertexSource,
            EmbeddedShaders::kUnlitVertexName.data(),
            "vs_5_1",
            debugMode);
        if (!vertexBytecode) {
            return Result::Err(std::move(vertexBytecode).error());
        }

        auto fragmentBytecode = CompileShader(
            EmbeddedShaders::kUnlitFragmentSource,
            EmbeddedShaders::kUnlitFragmentName.data(),
            "ps_5_1",
            debugMode);
        if (!fragmentBytecode) {
            return Result::Err(std::move(fragmentBytecode).error());
        }

        auto scenePostVertexBytecode = CompileShader(
            EmbeddedShaders::kScenePostVertexSource,
            EmbeddedShaders::kScenePostVertexName.data(),
            "vs_5_1",
            debugMode);
        if (!scenePostVertexBytecode) {
            return Result::Err(std::move(scenePostVertexBytecode).error());
        }

        auto scenePostFragmentBytecode = CompileShader(
            EmbeddedShaders::kScenePostFragmentSource,
            EmbeddedShaders::kScenePostFragmentName.data(),
            "ps_5_1",
            debugMode);
        if (!scenePostFragmentBytecode) {
            return Result::Err(std::move(scenePostFragmentBytecode).error());
        }

        auto vertexResult = CreateShader(
            vertexBytecode.value(), SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
        if (!vertexResult) {
            return Result::Err(std::move(vertexResult).error());
        }
        SDL_GPUShader* vertexShader = vertexResult.value();

        auto fragmentResult = CreateShader(
            fragmentBytecode.value(), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
        if (!fragmentResult) {
            SDL_ReleaseGPUShader(device, vertexShader);
            return Result::Err(std::move(fragmentResult).error());
        }
        SDL_GPUShader* fragmentShader = fragmentResult.value();

        auto scenePostVertexResult = CreateShader(
            scenePostVertexBytecode.value(), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
        if (!scenePostVertexResult) {
            SDL_ReleaseGPUShader(device, fragmentShader);
            SDL_ReleaseGPUShader(device, vertexShader);
            return Result::Err(std::move(scenePostVertexResult).error());
        }
        SDL_GPUShader* scenePostVertexShader = scenePostVertexResult.value();

        auto scenePostFragmentResult = CreateShader(
            scenePostFragmentBytecode.value(), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
        if (!scenePostFragmentResult) {
            SDL_ReleaseGPUShader(device, scenePostVertexShader);
            SDL_ReleaseGPUShader(device, fragmentShader);
            SDL_ReleaseGPUShader(device, vertexShader);
            return Result::Err(std::move(scenePostFragmentResult).error());
        }
        SDL_GPUShader* scenePostFragmentShader =
            scenePostFragmentResult.value();

        opaqueBackPipeline = CreatePipeline(
            vertexShader, fragmentShader, SDL_GPU_CULLMODE_BACK, false, true, true,
            kSceneColorFormat);
        opaqueDoublePipeline = CreatePipeline(
            vertexShader, fragmentShader, SDL_GPU_CULLMODE_NONE, false, true, true,
            kSceneColorFormat);
        alphaBackPipeline = CreatePipeline(
            vertexShader, fragmentShader, SDL_GPU_CULLMODE_BACK, true, true, true,
            kSceneColorFormat);
        alphaDoublePipeline = CreatePipeline(
            vertexShader, fragmentShader, SDL_GPU_CULLMODE_NONE, true, true, true,
            kSceneColorFormat);
        skyPipeline = CreatePipeline(
            vertexShader, fragmentShader, SDL_GPU_CULLMODE_FRONT, false, true, false,
            kSceneColorFormat);
        sceneSpritePipeline = CreatePipeline(
            vertexShader, fragmentShader, SDL_GPU_CULLMODE_NONE, true, false, false,
            kSceneColorFormat);
        overlaySpritePipeline = CreatePipeline(
            vertexShader, fragmentShader, SDL_GPU_CULLMODE_NONE, true, false, false,
            swapchainFormat);
        scenePostPipeline = CreateScenePostPipeline(
            scenePostVertexShader, scenePostFragmentShader);

        SDL_ReleaseGPUShader(device, scenePostFragmentShader);
        SDL_ReleaseGPUShader(device, scenePostVertexShader);
        SDL_ReleaseGPUShader(device, fragmentShader);
        SDL_ReleaseGPUShader(device, vertexShader);

        if (opaqueBackPipeline == nullptr || opaqueDoublePipeline == nullptr ||
            alphaBackPipeline == nullptr || alphaDoublePipeline == nullptr ||
            skyPipeline == nullptr || sceneSpritePipeline == nullptr ||
            overlaySpritePipeline == nullptr || scenePostPipeline == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to create a built-in graphics pipeline"));
        }
        return Result::Ok();
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

        device = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_DXBC,
            options.debugMode,
            "direct3d12");
        if (device == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::BackendUnavailable,
                "SDL_GPU: Direct3D 12 device creation failed"));
        }
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
                "SDL_GPU: Direct3D 12 does not support a linear SDR swapchain"));
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
        constexpr SDL_GPUTextureUsageFlags kSceneColorUsage =
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
            SDL_GPU_TEXTUREUSAGE_SAMPLER;
        if (!SDL_GPUTextureSupportsFormat(
                device,
                kSceneColorFormat,
                SDL_GPU_TEXTURETYPE_2D,
                kSceneColorUsage)) {
            return Result::Err(MakeError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: linear scene color target format is unsupported"));
        }
        auto pipelinesResult = CreatePipelines();
        if (!pipelinesResult) {
            return pipelinesResult;
        }

        linearClampSampler = CreateSampler(SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE);
        linearWrapSampler = CreateSampler(SDL_GPU_SAMPLERADDRESSMODE_REPEAT);
        if (linearClampSampler == nullptr || linearWrapSampler == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to create built-in samplers"));
        }

        const std::array<std::uint8_t, 4> whitePixels{255, 255, 255, 255};
        const ImageView whiteImage{
            1,
            1,
            4,
            std::as_bytes(std::span{whitePixels}),
            TextureColorSpace::Linear,
        };
        auto whiteResult = CreateTextureInternal(whiteImage, true);
        if (!whiteResult) {
            return Result::Err(std::move(whiteResult).error());
        }
        whiteTexture = whiteResult.value();

        const MeshData quad = MakeUnitQuadXY();
        auto quadResult = CreateMeshInternal(quad.View(), true);
        if (!quadResult) {
            return Result::Err(std::move(quadResult).error());
        }
        spriteQuad = quadResult.value();
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

    [[nodiscard]] Base::Result<TextureSlot*, RenderError> ResolveTexture(
        TextureHandle handle) {
        return FindTexture(handle ? handle : whiteTexture);
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
        slot.indexBuffer = nullptr;
        slot.vertexBuffer = nullptr;
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

    [[nodiscard]] Base::Result<void, RenderError> EnsureFrameTargets(
        std::uint32_t width,
        std::uint32_t height) {
        using Result = Base::Result<void, RenderError>;
        if (sceneColorTexture != nullptr && depthTexture != nullptr &&
            frameTargetWidth == width && frameTargetHeight == height) {
            return Result::Ok();
        }

        SDL_GPUTextureCreateInfo sceneInfo{};
        sceneInfo.type = SDL_GPU_TEXTURETYPE_2D;
        sceneInfo.format = kSceneColorFormat;
        sceneInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                          SDL_GPU_TEXTUREUSAGE_SAMPLER;
        sceneInfo.width = width;
        sceneInfo.height = height;
        sceneInfo.layer_count_or_depth = 1;
        sceneInfo.num_levels = 1;
        sceneInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        SDL_GPUTexture* newSceneColor =
            SDL_CreateGPUTexture(device, &sceneInfo);
        if (newSceneColor == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to create the linear scene color target"));
        }

        SDL_GPUTextureCreateInfo depthInfo{};
        depthInfo.type = SDL_GPU_TEXTURETYPE_2D;
        depthInfo.format = kDepthFormat;
        depthInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        depthInfo.width = width;
        depthInfo.height = height;
        depthInfo.layer_count_or_depth = 1;
        depthInfo.num_levels = 1;
        depthInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        SDL_GPUTexture* newDepth = SDL_CreateGPUTexture(device, &depthInfo);
        if (newDepth == nullptr) {
            SDL_ReleaseGPUTexture(device, newSceneColor);
            return Result::Err(MakeSdlError(
                RenderErrorCode::ResourceCreationFailed,
                "SDL_GPU: failed to create the depth target"));
        }

        SDL_GPUTexture* oldSceneColor = sceneColorTexture;
        SDL_GPUTexture* oldDepth = depthTexture;
        sceneColorTexture = newSceneColor;
        depthTexture = newDepth;
        frameTargetWidth = width;
        frameTargetHeight = height;
        if (oldSceneColor != nullptr) {
            SDL_ReleaseGPUTexture(device, oldSceneColor);
        }
        if (oldDepth != nullptr) {
            SDL_ReleaseGPUTexture(device, oldDepth);
        }
        return Result::Ok();
    }

    [[nodiscard]] SDL_GPUGraphicsPipeline* PipelineFor(
        const MeshSubmission& submission) const noexcept {
        switch (submission.surface) {
        case SurfaceMode::Opaque:
            return submission.doubleSided
                       ? opaqueDoublePipeline
                       : opaqueBackPipeline;
        case SurfaceMode::AlphaMasked:
            return submission.doubleSided
                       ? alphaDoublePipeline
                       : alphaBackPipeline;
        case SurfaceMode::Sky:
            return skyPipeline;
        }
        return opaqueBackPipeline;
    }

    [[nodiscard]] SDL_GPUSampler* SamplerFor(SamplerMode mode) const noexcept {
        return mode == SamplerMode::LinearWrap
                   ? linearWrapSampler
                   : linearClampSampler;
    }

    [[nodiscard]] Base::Result<void, RenderError> ValidateQueue(
        const RenderQueue& queue) {
        using Result = Base::Result<void, RenderError>;
        const Color clear = queue.Frame().clearColor;
        if (!IsFinite(clear)) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidArgument,
                "SDL_GPU: clear color must be finite"));
        }
        const SceneColorTransform sceneColor =
            queue.Frame().sceneColorTransform;
        if (!IsValidSceneColorTransform(sceneColor)) {
            return Result::Err(MakeError(
                RenderErrorCode::InvalidArgument,
                "SDL_GPU: scene color transform is outside supported bounds"));
        }
        if (!queue.Meshes().empty()) {
            if (!queue.Camera()) {
                return Result::Err(MakeError(
                    RenderErrorCode::InvalidArgument,
                    "SDL_GPU: mesh submissions require a camera"));
            }
            const PerspectiveCamera3D& camera = *queue.Camera();
            if (!IsFinite(camera.position) || !IsFinite(camera.rotationRadians) ||
                !IsFinite(camera.verticalFieldOfViewRadians) ||
                camera.verticalFieldOfViewRadians <= 0.0F ||
                camera.verticalFieldOfViewRadians >= std::numbers::pi_v<float> ||
                !IsFinite(camera.nearClip) || camera.nearClip <= 0.0F ||
                !IsFinite(camera.farClip) || camera.farClip <= camera.nearClip) {
                return Result::Err(MakeError(
                    RenderErrorCode::InvalidArgument,
                    "SDL_GPU: camera parameters are invalid"));
            }
        }
        for (const MeshSubmission& submission : queue.Meshes()) {
            auto mesh = FindMesh(submission.mesh);
            if (!mesh) {
                return Result::Err(std::move(mesh).error());
            }
            auto texture = ResolveTexture(submission.texture);
            if (!texture) {
                return Result::Err(std::move(texture).error());
            }
        }
        for (const SpriteSubmission& submission : queue.Sprites()) {
            auto texture = ResolveTexture(submission.texture);
            if (!texture) {
                return Result::Err(std::move(texture).error());
            }
        }
        return Result::Ok();
    }

    void DrawMesh(
        SDL_GPUCommandBuffer* command,
        SDL_GPURenderPass* pass,
        const MeshSubmission& submission,
        const Matrix4& viewProjection) {
        MeshSlot& mesh = *FindMesh(submission.mesh).value();
        TextureSlot& texture = *ResolveTexture(submission.texture).value();

        const VertexUniforms vertexUniforms{
            Multiply(WorldMatrix(submission.transform), viewProjection),
        };
        const float alphaCutoff =
            submission.surface == SurfaceMode::AlphaMasked ? 0.01F : -1.0F;
        const FragmentUniforms fragmentUniforms = MakeFragmentUniforms(
            submission.tint, submission.uv, alphaCutoff);
        SDL_PushGPUVertexUniformData(
            command, 0, &vertexUniforms, sizeof(vertexUniforms));
        SDL_PushGPUFragmentUniformData(
            command, 0, &fragmentUniforms, sizeof(fragmentUniforms));

        SDL_BindGPUGraphicsPipeline(pass, PipelineFor(submission));
        const SDL_GPUBufferBinding vertexBinding{mesh.vertexBuffer, 0};
        const SDL_GPUBufferBinding indexBinding{mesh.indexBuffer, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
        SDL_BindGPUIndexBuffer(
            pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        const SDL_GPUTextureSamplerBinding textureBinding{
            texture.texture,
            SamplerFor(submission.sampler),
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &textureBinding, 1);
        SDL_DrawGPUIndexedPrimitives(pass, mesh.indexCount, 1, 0, 0, 0);
    }

    void DrawSprite(
        SDL_GPUCommandBuffer* command,
        SDL_GPURenderPass* pass,
        const SpriteSubmission& submission,
        const Matrix4& pixelProjection,
        SDL_GPUGraphicsPipeline* pipeline) {
        if (submission.destinationPixels.width == 0.0F ||
            submission.destinationPixels.height == 0.0F ||
            submission.sourceUv.width == 0.0F ||
            submission.sourceUv.height == 0.0F) {
            return;
        }

        MeshSlot& mesh = *FindMesh(spriteQuad).value();
        TextureSlot& texture = *ResolveTexture(submission.texture).value();
        const VertexUniforms vertexUniforms{
            Multiply(SpriteWorldMatrix(submission), pixelProjection),
        };
        const FragmentUniforms fragmentUniforms = MakeFragmentUniforms(
            submission.tint,
            MakeSpriteUvTransform(submission.sourceUv),
            -1.0F);
        SDL_PushGPUVertexUniformData(
            command, 0, &vertexUniforms, sizeof(vertexUniforms));
        SDL_PushGPUFragmentUniformData(
            command, 0, &fragmentUniforms, sizeof(fragmentUniforms));

        SDL_BindGPUGraphicsPipeline(pass, pipeline);
        const SDL_GPUBufferBinding vertexBinding{mesh.vertexBuffer, 0};
        const SDL_GPUBufferBinding indexBinding{mesh.indexBuffer, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
        SDL_BindGPUIndexBuffer(
            pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        const SDL_GPUTextureSamplerBinding textureBinding{
            texture.texture,
            SamplerFor(submission.sampler),
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &textureBinding, 1);
        SDL_DrawGPUIndexedPrimitives(pass, mesh.indexCount, 1, 0, 0, 0);
    }

    void DrawScenePost(
        SDL_GPUCommandBuffer* command,
        SDL_GPURenderPass* pass,
        const SceneColorTransform& transform) {
        const SceneColorUniforms uniforms{
            transform.exposureEv,
            transform.gammaAdjustment,
            {},
        };
        SDL_PushGPUFragmentUniformData(
            command, 0, &uniforms, sizeof(uniforms));

        SDL_BindGPUGraphicsPipeline(pass, scenePostPipeline);
        const SDL_GPUTextureSamplerBinding sceneBinding{
            sceneColorTexture,
            linearClampSampler,
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &sceneBinding, 1);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    }

    [[nodiscard]] Base::Result<PresentStatus, RenderError> Render(
        const RenderQueue& queue) {
        using Result = Base::Result<PresentStatus, RenderError>;

        auto thread = CheckThread();
        if (!thread) {
            return Result::Err(std::move(thread).error());
        }
        auto validation = ValidateQueue(queue);
        if (!validation) {
            return Result::Err(std::move(validation).error());
        }

        SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
        if (command == nullptr) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: failed to acquire a render command buffer"));
        }

        SDL_GPUTexture* swapchainTexture = nullptr;
        Uint32 width = 0;
        Uint32 height = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                command, window, &swapchainTexture, &width, &height)) {
            static_cast<void>(SDL_CancelGPUCommandBuffer(command));
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: failed to acquire the swapchain texture"));
        }
        if (swapchainTexture == nullptr || width == 0 || height == 0) {
            // A minimized window can legitimately provide no texture. No
            // swapchain texture is referenced, so cancellation is valid.
            static_cast<void>(SDL_CancelGPUCommandBuffer(command));
            return Result::Ok(PresentStatus::Skipped);
        }

        auto frameTargets = EnsureFrameTargets(width, height);
        if (!frameTargets) {
            // Once a swapchain texture is acquired SDL forbids cancellation.
            // Submit the otherwise empty buffer before surfacing the error.
            static_cast<void>(SDL_SubmitGPUCommandBuffer(command));
            return Result::Err(std::move(frameTargets).error());
        }

        const Color clear = queue.Frame().clearColor;
        SDL_GPUColorTargetInfo sceneTarget{};
        sceneTarget.texture = sceneColorTexture;
        sceneTarget.clear_color = {
            clear.red, clear.green, clear.blue, clear.alpha,
        };
        sceneTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        sceneTarget.store_op = SDL_GPU_STOREOP_STORE;
        sceneTarget.cycle = true;

        SDL_GPUDepthStencilTargetInfo depthTarget{};
        depthTarget.texture = depthTexture;
        depthTarget.clear_depth = 1.0F;
        depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthTarget.cycle = true;

        SDL_GPURenderPass* scenePass =
            SDL_BeginGPURenderPass(command, &sceneTarget, 1, &depthTarget);
        if (scenePass == nullptr) {
            static_cast<void>(SDL_SubmitGPUCommandBuffer(command));
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: failed to begin the scene render pass"));
        }

        if (!queue.Meshes().empty()) {
            const PerspectiveCamera3D& camera = *queue.Camera();
            const Matrix4 viewProjection = Multiply(
                ViewMatrix(camera),
                ProjectionMatrix(
                    camera,
                    static_cast<float>(width) / static_cast<float>(height)));
            for (const MeshSubmission& submission : queue.Meshes()) {
                DrawMesh(command, scenePass, submission, viewProjection);
            }
        }
        SDL_EndGPURenderPass(scenePass);

        bool hasSceneSprites = false;
        bool hasOverlaySprites = false;
        for (const SpriteSubmission& submission : queue.Sprites()) {
            hasSceneSprites = hasSceneSprites ||
                submission.layer == CompositeLayer::Scene;
            hasOverlaySprites = hasOverlaySprites ||
                submission.layer == CompositeLayer::Overlay;
        }
        const Matrix4 spriteProjection = PixelProjection(
            static_cast<float>(width), static_cast<float>(height));

        if (hasSceneSprites) {
            sceneTarget.load_op = SDL_GPU_LOADOP_LOAD;
            sceneTarget.cycle = false;
            SDL_GPURenderPass* sceneSpritePass =
                SDL_BeginGPURenderPass(command, &sceneTarget, 1, nullptr);
            if (sceneSpritePass == nullptr) {
                static_cast<void>(SDL_SubmitGPUCommandBuffer(command));
                return Result::Err(MakeSdlError(
                    RenderErrorCode::SubmissionFailed,
                    "SDL_GPU: failed to begin the scene sprite render pass"));
            }
            for (const SpriteSubmission& submission : queue.Sprites()) {
                if (submission.layer == CompositeLayer::Scene) {
                    DrawSprite(
                        command,
                        sceneSpritePass,
                        submission,
                        spriteProjection,
                        sceneSpritePipeline);
                }
            }
            SDL_EndGPURenderPass(sceneSpritePass);
        }

        SDL_GPUColorTargetInfo swapchainTarget{};
        swapchainTarget.texture = swapchainTexture;
        swapchainTarget.load_op = SDL_GPU_LOADOP_DONT_CARE;
        swapchainTarget.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* scenePostPass =
            SDL_BeginGPURenderPass(command, &swapchainTarget, 1, nullptr);
        if (scenePostPass == nullptr) {
            static_cast<void>(SDL_SubmitGPUCommandBuffer(command));
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: failed to begin the scene color post-process pass"));
        }
        DrawScenePost(
            command,
            scenePostPass,
            queue.Frame().sceneColorTransform);
        SDL_EndGPURenderPass(scenePostPass);

        if (hasOverlaySprites) {
            swapchainTarget.load_op = SDL_GPU_LOADOP_LOAD;
            SDL_GPURenderPass* overlayPass =
                SDL_BeginGPURenderPass(command, &swapchainTarget, 1, nullptr);
            if (overlayPass == nullptr) {
                static_cast<void>(SDL_SubmitGPUCommandBuffer(command));
                return Result::Err(MakeSdlError(
                    RenderErrorCode::SubmissionFailed,
                    "SDL_GPU: failed to begin the overlay sprite render pass"));
            }
            for (const SpriteSubmission& submission : queue.Sprites()) {
                if (submission.layer == CompositeLayer::Overlay) {
                    DrawSprite(
                        command,
                        overlayPass,
                        submission,
                        spriteProjection,
                        overlaySpritePipeline);
                }
            }
            SDL_EndGPURenderPass(overlayPass);
        }

        if (!SDL_SubmitGPUCommandBuffer(command)) {
            return Result::Err(MakeSdlError(
                RenderErrorCode::SubmissionFailed,
                "SDL_GPU: render submission failed"));
        }
        return Result::Ok(PresentStatus::Presented);
    }
};

Base::Result<std::unique_ptr<SdlGpuRenderDevice>, RenderError>
SdlGpuRenderDevice::Create(
    Platform::Sdl::SdlPlatform& platform,
    const SdlGpuOptions& options) {
    using Result = Base::Result<std::unique_ptr<SdlGpuRenderDevice>, RenderError>;

    auto impl = std::make_unique<Impl>();
    auto initialized = impl->Initialize(platform, options);
    if (!initialized) {
        return Result::Err(std::move(initialized).error());
    }
    return Result::Ok(std::unique_ptr<SdlGpuRenderDevice>(
        new SdlGpuRenderDevice(std::move(impl))));
}

SdlGpuRenderDevice::SdlGpuRenderDevice(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

SdlGpuRenderDevice::~SdlGpuRenderDevice() = default;

Base::Result<MeshHandle, RenderError> SdlGpuRenderDevice::CreateMesh(
    const MeshView& mesh) {
    return impl_->CreateMesh(mesh);
}

Base::Result<TextureHandle, RenderError> SdlGpuRenderDevice::CreateTexture(
    const ImageView& image) {
    return impl_->CreateTexture(image);
}

Base::Result<void, RenderError> SdlGpuRenderDevice::ReleaseMesh(
    MeshHandle handle) {
    return impl_->ReleaseMesh(handle);
}

Base::Result<void, RenderError> SdlGpuRenderDevice::ReleaseTexture(
    TextureHandle handle) {
    return impl_->ReleaseTexture(handle);
}

Base::Result<PresentStatus, RenderError> SdlGpuRenderDevice::Render(
    const RenderQueue& queue) {
    return impl_->Render(queue);
}

} // namespace Engine::Render::Backend::SdlGpu
