#pragma once

#include <memory>
#include <string>

namespace fps { class CampaignContent; }

// Exercise the real packaged campaign and GameSession without creating a
// window, graphics device, renderer, input backend, or text rasterizer.
[[nodiscard]] bool RunHeadlessSmoke(
    std::shared_ptr<const fps::CampaignContent> content,
    std::string& report,
    std::string& error);
