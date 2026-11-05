#include "gyo/model_viewer/ModelSession.hpp"

#include "engine/base/Error.hpp"

namespace Gyo::Tools::ModelViewer {

bool ModelSession::Open(const ModelContent& content, const std::string_view id) {
    Close();
    auto loaded = content.LoadModel(id);
    if (!loaded) {
        error_ = Engine::Base::Describe(loaded.error());
        return false;
    }
    model_ = std::move(loaded.value());
    if (!model_->model->clips.empty()) {
        static_cast<void>(SelectClip(0));
    }
    return true;
}

void ModelSession::Close() noexcept {
    model_.reset();
    clip_.reset();
    error_.clear();
    clock.durationSeconds = 0.0;
    clock.seconds = 0.0;
    clock.playing = false;
}

bool ModelSession::SelectClip(const std::size_t clip) noexcept {
    if (!model_ || clip >= model_->model->clips.size()) {
        return false;
    }
    clip_ = clip;
    clock.durationSeconds = model_->model->clips[clip].durationSeconds;
    clock.seconds = 0.0;
    clock.playing = false;
    return true;
}

} // namespace Gyo::Tools::ModelViewer
