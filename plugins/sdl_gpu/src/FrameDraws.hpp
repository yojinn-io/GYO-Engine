#pragma once

// The render targets one frame's passes drew on, held until the frame is handed to the GPU (render-pipeline
// declaration 14.14): what a pass draws on a target is its picture only once the frame that drew it was submitted, as
// the frame's receipt says. A frame that was not (cancelled, refused) leaves every target as it was. No SDL here: the
// adapter (src/adapters/sdl/GPUDevice.cpp) holds one and asks it.

#include <algorithm>
#include <cstdint>
#include <vector>

#include "engine/render/RenderTextures.hpp"

namespace GYO::Plugins::SDLGPU {

class FrameDraws final {
public:
    // A pass of this frame begins drawing on the target, which has a picture or not (from a frame handed before):
    // true when its picture is undefined here (it has none, and no pass of this frame drew on it yet), so the pass
    // clears it instead of keeping it.
    bool Begin(const std::uint32_t target, const bool picture) {
        if (Drew(target)) return false;
        drawn_.push_back(target);
        return !picture;
    }
    // Whether a pass of this frame drew on the target: a later pass of the frame may sample it.
    [[nodiscard]] bool Drew(const std::uint32_t target) const noexcept {
        return std::find(drawn_.begin(), drawn_.end(), target) != drawn_.end();
    }
    // The frame was handed to the GPU: each target its passes drew on goes in the receipt, and is given back (the
    // backend keeps its picture from then on); the frame's account is cleared.
    std::vector<std::uint32_t> Commit(Engine::Render::RenderFrameReceipt& receipt) {
        for (const std::uint32_t target : drawn_) receipt.RecordDrawn(Engine::Render::TextureId{target});
        std::vector<std::uint32_t> drawn = std::move(drawn_);
        drawn_.clear();
        return drawn;
    }
    // A new frame: nothing drawn yet (a frame not handed to the GPU is forgotten here).
    void Clear() noexcept { drawn_.clear(); }

private:
    std::vector<std::uint32_t> drawn_;
};

} // namespace GYO::Plugins::SDLGPU
