#pragma once
#include "render/IRenderDevice.hpp"

namespace Gyo::Tests {
// Resource-only test fixtures reject accidental frame execution explicitly.
class RenderDeviceStub : public Engine::Render::IRenderDevice {
    template<class T> static Engine::Base::Result<T, Engine::Render::RenderError> Unsupported() {
        return Engine::Base::Result<T, Engine::Render::RenderError>::Err(
            Engine::Render::RenderError::Make(Engine::Render::RenderErrorCode::UnsupportedOperation,
                "resource-only test device does not execute frames"));
    }
public:
    Engine::Render::RenderDeviceInfo GetInfo() const override { return {}; }
    Engine::Base::Result<Engine::Render::ShaderHandle, Engine::Render::RenderError>
    CreateShader(const Engine::Render::ShaderArtifact&) override { return Unsupported<Engine::Render::ShaderHandle>(); }
    Engine::Base::Result<void, Engine::Render::RenderError>
    ReleaseShader(Engine::Render::ShaderHandle) override {
        return Engine::Base::Result<void, Engine::Render::RenderError>::Ok();
    }
    Engine::Base::Result<Engine::Render::TextureHandle, Engine::Render::RenderError>
    CreateTexture(const Engine::Render::TextureDesc&) override { return Unsupported<Engine::Render::TextureHandle>(); }
    Engine::Base::Result<Engine::Render::PipelineHandle, Engine::Render::RenderError>
    CreatePipeline(const Engine::Render::PipelineDesc&) override { return Unsupported<Engine::Render::PipelineHandle>(); }
    Engine::Base::Result<void, Engine::Render::RenderError>
    ReleasePipeline(Engine::Render::PipelineHandle) override { return Unsupported<void>(); }
    Engine::Base::Result<std::optional<Engine::Render::AcquiredFrame>, Engine::Render::RenderError>
    AcquireFrame() override { return Unsupported<std::optional<Engine::Render::AcquiredFrame>>(); }
    Engine::Base::Result<Engine::Render::PresentStatus, Engine::Render::RenderError>
    SubmitFrame(const Engine::Render::AcquiredFrame&, const Engine::Render::PreparedFrame&) override {
        return Unsupported<Engine::Render::PresentStatus>();
    }
    void AbandonFrame(const Engine::Render::AcquiredFrame&) noexcept override {}
    Engine::Base::Result<Engine::Render::TextureReadback, Engine::Render::RenderError>
    ReadTexture(Engine::Render::TextureHandle) override { return Unsupported<Engine::Render::TextureReadback>(); }
};
}
