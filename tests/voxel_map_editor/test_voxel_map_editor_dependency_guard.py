"""Self-test of voxel_map_editor_dependency_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import voxel_map_editor_dependency_guard as guard  # noqa: E402

TOOL = "tools/voxel_map_editor"


class VoxelMapEditorDependencyGuardTests(unittest.TestCase):
    def scan(self, files: dict[str, str]) -> list[str]:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, text in files.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
            return guard.scan(root)

    def test_clean_tree_passes(self):
        self.assertEqual(self.scan({
            f"{TOOL}/src/A.cpp": '#include "gyo/voxel_map_editor/VoxelMap.hpp"\n#include "engine/voxel_mesh/VoxelMesh.hpp"\n'
                                 '#include "engine/map_markers/MapMarkers.hpp"\n'
                                 '#include "gyo/framework/HeadlessShell.hpp"\n#include <nlohmann/json.hpp>\n'
                                 '#include "JsonRules.hpp"\n#include <vector>\n',
            f"{TOOL}/CMakeLists.txt": "target_link_libraries(gyo_voxel_map_editor_core PUBLIC GYO::Base "
                                      "GYO::Engine GYO::VoxelMesh GYO::MapMarkers PRIVATE nlohmann_json::nlohmann_json)\n"
                                      "target_link_libraries(gyo_voxel_map_editor PRIVATE gyo_voxel_map_editor_core "
                                      "GYO::ToolFramework)\n",
        }), [])

    def test_sdl_use_fails(self):
        findings = self.scan({f"{TOOL}/src/A.cpp": '#include <SDL3/SDL.h>\nSDL_CreateWindow("", 1, 1, 0);\n'})
        self.assertEqual(len(findings), 2)

    def test_every_imgui_header_fails(self):
        findings = self.scan({f"{TOOL}/src/A.cpp": '#include "imgui.h"\n#include <imgui/imgui_internal.h>\n'
                                                   '#include "backends/imgui_impl_sdl3.h"\n'})
        self.assertEqual(len(findings), 3)

    def test_window_and_render_targets_fail_for_now(self):
        findings = self.scan({f"{TOOL}/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Render GYO::PlatformSDL "
                                                        "GYO::ToolFrameworkImGui imgui::core SDL3::SDL3)\n"})
        self.assertEqual(len(findings), 5)

    def test_other_engine_modules_still_fail(self):
        # Allowing GYO::MapMarkers allows that one module, not Engine at large.
        findings = self.scan({f"{TOOL}/CMakeLists.txt": "target_link_libraries(gyo_voxel_map_editor_core PUBLIC "
                                                        "GYO::Collision GYO::MapMarkersNative GYO::MapMarkers.extra)\n"})
        self.assertEqual(len(findings), 3)

    def test_game_include_and_plain_target_fail(self):
        findings = self.scan({f"{TOOL}/src/A.cpp": '#include "SomeGame/World.hpp"\n',
                              f"{TOOL}/CMakeLists.txt": "target_link_libraries(gyo_voxel_map_editor PRIVATE some_game_domain)\n"})
        self.assertEqual(len(findings), 2)

    def test_registered_id_fails_but_the_own_id_passes(self):
        files = {"engine/config/tools.csv": "name,description\nvoxel_map_editor,x\nsome_tool,y\n",
                 "engine/config/projects.csv": "name,description\nsome_game,z\n",
                 f"{TOOL}/src/A.cpp": "// voxel_map_editor builds some_game maps\n"}
        self.assertEqual(self.scan(files), [f"{TOOL}/src/A.cpp: names the registered id some_game"])


if __name__ == "__main__":
    unittest.main()
