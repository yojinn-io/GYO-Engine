#pragma once
#include <memory>
#include <optional>
#include <vector>
#include "render/IRenderDevice.hpp"
#include "render/RenderQueue.hpp"
#include "render/ShaderLibrary.hpp"

namespace Engine::Render {
// Scene color before exposure/gamma and overlays, encoded as top-left RGBA8
// sRGB with straight alpha. Diagnostic capture never blocks ordinary frames.
struct SceneCapture final {
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> rgba8;
};
class Renderer final {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    // Device and immutable library must outlive this renderer. Use all methods
    // on the device's creation thread; Reset releases device-owned resources.
    [[nodiscard]] Base::Result<void, RenderError> Initialize(
        IRenderDevice&, const ShaderLibrary&);
    void Reset() noexcept;
    [[nodiscard]] std::optional<ShaderFormat> ActiveShaderFormat() const noexcept;
    [[nodiscard]] Base::Result<PresentStatus, RenderError> Render(const RenderQueue&);
    void RequestSceneCapture() noexcept;
    [[nodiscard]] std::optional<SceneCapture> TakeSceneCapture();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace Engine::Render
