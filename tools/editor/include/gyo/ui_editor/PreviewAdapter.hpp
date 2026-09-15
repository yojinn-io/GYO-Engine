#pragma once

#include <nlohmann/json_fwd.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ImDrawList;
namespace Gyo::Tools::UiEditor {

class AssetPreviewContext;

// ImGui is only a drawing backend here: node placement always comes from the
// GYO UI document/evaluator seam, never from ImGui widget layout.
class PreviewAdapter final {
public:
    PreviewAdapter();
    ~PreviewAdapter();
    PreviewAdapter(PreviewAdapter&&) noexcept;
    PreviewAdapter& operator=(PreviewAdapter&&) noexcept;
    PreviewAdapter(const PreviewAdapter&) = delete;
    PreviewAdapter& operator=(const PreviewAdapter&) = delete;

    struct Viewport final {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    struct Result final {
        std::string clickedNodeId;
        std::string hoveredNodeId;
        std::string error;

        struct Action final {
            std::string id;
            std::string sourceElement;
            std::optional<double> numberPayload;
        };
        std::vector<Action> actions;

        enum class GizmoField {
            AnchorMin,
            AnchorMax,
            Pivot,
        };
        struct GizmoEdit final {
            GizmoField field{GizmoField::Pivot};
            float x{};
            float y{};
            bool began{};
            bool finished{};
        };
        std::optional<GizmoEdit> gizmoEdit;
    };

    [[nodiscard]] Result Draw(
        ImDrawList& drawList,
        const nlohmann::json& document,
        std::string_view canvasId,
        const Viewport& viewport,
        std::string_view selectedNodeId,
        AssetPreviewContext* assets,
        bool previewMode,
        bool inputEnabled,
        bool snapToGrid,
        float gridSize) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Gyo::Tools::UiEditor
