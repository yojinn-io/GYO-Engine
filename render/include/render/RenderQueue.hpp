#pragma once

#include <optional>
#include <span>
#include <vector>

#include "engine/base/Result.hpp"
#include "render/RenderError.hpp"
#include "render/RenderTypes.hpp"

namespace Engine::Render {

// A backend-neutral, CPU-side description of one frame. It is deliberately a
// fixed scene-and-overlay queue rather than a generic render graph.
class RenderQueue final {
public:
    explicit RenderQueue(const FrameDescription& frame = {});

    void Reset(const FrameDescription& frame = {});
    void SetCamera(const PerspectiveCamera3D& camera);
    void ClearCamera() noexcept;
    void SetViewModelCamera(const PerspectiveCamera3D& camera);
    void ClearViewModelCamera() noexcept;

    [[nodiscard]] Base::Result<void, RenderError> Submit(
        const MeshSubmission& submission);
    [[nodiscard]] Base::Result<void, RenderError> Submit(
        const SpriteSubmission& submission);

    [[nodiscard]] const FrameDescription& Frame() const noexcept;
    [[nodiscard]] const std::optional<PerspectiveCamera3D>& Camera() const noexcept;
    [[nodiscard]] const std::optional<PerspectiveCamera3D>& ViewModelCamera() const noexcept;
    [[nodiscard]] std::span<const MeshSubmission> Meshes() const noexcept;
    [[nodiscard]] std::span<const SpriteSubmission> Sprites() const noexcept;

private:
    FrameDescription frame_{};
    std::optional<PerspectiveCamera3D> camera_{};
    std::optional<PerspectiveCamera3D> viewModelCamera_{};
    std::vector<MeshSubmission> meshes_{};
    std::vector<SpriteSubmission> sprites_{};
};

} // namespace Engine::Render
