#pragma once

#include "engine/model/ModelAsset.hpp"

#include <string>

// What the viewer says about a model: what Engine loaded, as text.
namespace Gyo::Tools::ModelViewer {

// One line per item, sections in this order: the node tree (two spaces of
// indent per depth), meshes (vertices, triangles, joints, node, material),
// materials (linear base color) and clips (duration, tracks). Numbers with a
// fraction are printed with three decimals.
[[nodiscard]] std::string DescribeModel(const Engine::Model::ModelAsset& model);

} // namespace Gyo::Tools::ModelViewer
