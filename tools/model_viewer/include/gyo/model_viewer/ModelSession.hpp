#pragma once

#include "gyo/model_viewer/ModelContent.hpp"
#include "gyo/model_viewer/Playback.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// The model the window shows (judgment V3 of -preview): the opened model with
// its clip and clock, or the reason the last open failed. A failed open shows
// nothing of the model before it.
namespace Gyo::Tools::ModelViewer {

class ModelSession final {
public:
    // Loads id. On failure the session holds no model, and Error() is the
    // loader's or the catalog's message (with Engine's cause).
    bool Open(const ModelContent& content, std::string_view id);
    void Close() noexcept;

    [[nodiscard]] const LoadedModel* Model() const noexcept { return model_ ? &*model_ : nullptr; }
    [[nodiscard]] const std::string& Error() const noexcept { return error_; }
    // The clip being played; none for a model without clips.
    [[nodiscard]] std::optional<std::size_t> Clip() const noexcept { return clip_; }
    // Starts clip from 0, paused, keeping the speed and loop setting. False
    // (and nothing changes) for an index the model does not have.
    bool SelectClip(std::size_t clip) noexcept;

    PlaybackClock clock;

private:
    std::optional<LoadedModel> model_;
    std::optional<std::size_t> clip_;
    std::string error_;
};

} // namespace Gyo::Tools::ModelViewer
