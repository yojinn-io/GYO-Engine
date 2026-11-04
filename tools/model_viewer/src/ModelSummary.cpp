#include "gyo/model_viewer/ModelSummary.hpp"

#include <array>
#include <cstdio>
#include <vector>

namespace Gyo::Tools::ModelViewer {
namespace {

std::string Fixed(const double value) {
    std::array<char, 64> text{};
    std::snprintf(text.data(), text.size(), "%.3f", value);
    return text.data();
}

std::string NameOrIndex(const std::string& name, const std::size_t index) {
    return name.empty() ? "#" + std::to_string(index) : name;
}

} // namespace

std::string DescribeModel(const Engine::Model::ModelAsset& model) {
    std::string out = "nodes: " + std::to_string(model.nodes.size()) + "\n";
    // Parents precede their children (ModelAsset.hpp), so a depth is known
    // before any child needs it.
    std::vector<std::size_t> depth(model.nodes.size(), 0);
    for (std::size_t index = 0; index < model.nodes.size(); ++index) {
        const auto& node = model.nodes[index];
        if (node.parentIndex) depth[index] = depth[*node.parentIndex] + 1;
        out += std::string(2 * (depth[index] + 1), ' ') + NameOrIndex(node.name, index) + "\n";
    }
    out += "meshes: " + std::to_string(model.meshes.size()) + "\n";
    for (std::size_t index = 0; index < model.meshes.size(); ++index) {
        const auto& mesh = model.meshes[index];
        out += "  " + NameOrIndex(mesh.name, index) + ": " + std::to_string(mesh.vertices.size()) + " vertices, " +
               std::to_string(mesh.indices.size() / 3) + " triangles, " + std::to_string(mesh.joints.size()) +
               " joints, node " + NameOrIndex(model.nodes[mesh.nodeIndex].name, mesh.nodeIndex) + ", material " +
               NameOrIndex(model.materials[mesh.materialIndex].name, mesh.materialIndex) + "\n";
    }
    out += "materials: " + std::to_string(model.materials.size()) + "\n";
    for (std::size_t index = 0; index < model.materials.size(); ++index) {
        const auto& material = model.materials[index];
        const auto& color = material.baseColorLinear;
        out += "  " + NameOrIndex(material.name, index) + ": base color " + Fixed(color[0]) + " " + Fixed(color[1]) +
               " " + Fixed(color[2]) + " " + Fixed(color[3]) + "\n";
    }
    out += "clips: " + std::to_string(model.clips.size()) + "\n";
    for (const auto& clip : model.clips) {
        out += "  " + clip.name + ": " + Fixed(clip.durationSeconds) + " s, " + std::to_string(clip.tracks.size()) +
               " tracks\n";
    }
    return out;
}

} // namespace Gyo::Tools::ModelViewer
