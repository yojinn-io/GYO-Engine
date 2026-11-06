"""Self-test of voxel_map_editor_dependency_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import voxel_map_editor_dependency_guard as guard  # noqa: E402

TOOL = "tools/voxel_map_editor"


class VoxelMapEditorDependencyGuardTests(unittest.TestCase):
    def scan(self, files: dict[str, str], gui: bool = True) -> list[str]:
        """The tool's CMake files are put in the gui variant's branch unless gui is False (as the real one is)."""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, text in files.items():
                if gui and (name.endswith("CMakeLists.txt") or name.endswith(".cmake")):
                    text = 'if(GYO_TOOL_VARIANT STREQUAL "gui")\n' + text + "endif()\n"
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
                                      "GYO::Engine GYO::Math GYO::VoxelMesh GYO::MapMarkers PRIVATE nlohmann_json::nlohmann_json)\n"
                                      "target_link_libraries(gyo_voxel_map_editor PRIVATE gyo_voxel_map_editor_core "
                                      "GYO::ToolFramework)\n",
        }), [])

    def test_sdl_use_fails(self):
        findings = self.scan({f"{TOOL}/src/A.cpp": '#include <SDL3/SDL.h>\nSDL_CreateWindow("", 1, 1, 0);\n'})
        self.assertEqual(len(findings), 2)

    def test_every_imgui_header_fails_in_the_core(self):
        findings = self.scan({f"{TOOL}/src/A.cpp": '#include "imgui.h"\n#include <imgui/imgui_internal.h>\n'
                                                   '#include "backends/imgui_impl_sdl3.h"\n'
                                                   '#include "gyo/framework/imgui/ImGuiShell.hpp"\n'})
        self.assertEqual(len(findings), 4)

    def test_the_window_has_imgui_h_and_the_shell_only(self):
        findings = self.scan({f"{TOOL}/src/window/W.cpp": '#include <imgui.h>\n#include "gyo/framework/imgui/ImGuiShell.hpp"\n'
                                                          '#include "window/PreviewScene.hpp"\n'
                                                          '#include "engine/render/backend/sdl/SdlRenderer.hpp"\n'
                                                          '#include "imgui_internal.h"\n'
                                                          '#include "backends/imgui_impl_sdl3.h"\n'
                                                          '#include <SDL3/SDL.h>\n'
                                                          '#include "engine/render/backend/sdl/SdlRendererNative.hpp"\n'
                                                          'SDL_GetTicks();\n'})
        self.assertEqual(sorted(findings), sorted([
            f"{TOOL}/src/window/W.cpp: includes imgui_internal.h",
            f"{TOOL}/src/window/W.cpp: includes backends/imgui_impl_sdl3.h",
            f"{TOOL}/src/window/W.cpp: includes SDL3/SDL.h",
            f"{TOOL}/src/window/W.cpp: includes engine/render/backend/sdl/SdlRendererNative.hpp",
            f"{TOOL}/src/window/W.cpp: uses SDL_GetTicks",
        ]))

    def test_the_core_includes_only_the_window_entry_header(self):
        findings = self.scan({f"{TOOL}/src/Main.cpp": '#include "window/EditorWindow.hpp"\n#include "window/PreviewScene.hpp"\n'})
        self.assertEqual(findings, [f"{TOOL}/src/Main.cpp: includes window/PreviewScene.hpp"])

    def test_window_targets_only_for_the_window_library(self):
        findings = self.scan({f"{TOOL}/CMakeLists.txt":
                              "target_link_libraries(gyo_voxel_map_editor_window PUBLIC gyo_voxel_map_editor_core "
                              "GYO::ToolFrameworkPreview PRIVATE GYO::ToolFrameworkImGui GYO::RenderBackendSDL "
                              "GYO::PlatformSDL imgui::core GYO::Render)\n"
                              "target_link_libraries(gyo_voxel_map_editor PRIVATE gyo_voxel_map_editor_window)\n"
                              "target_link_libraries(gyo_voxel_map_editor_core PUBLIC GYO::Render imgui::core)\n"
                              "target_link_libraries(gyo_voxel_map_editor PRIVATE GYO::PlatformSDL)\n"})
        self.assertEqual(sorted(findings), sorted([
            f"{TOOL}/CMakeLists.txt: gyo_voxel_map_editor_core links GYO::Render",
            f"{TOOL}/CMakeLists.txt: gyo_voxel_map_editor_core links imgui::core",
            f"{TOOL}/CMakeLists.txt: gyo_voxel_map_editor links GYO::PlatformSDL",
            f"{TOOL}/CMakeLists.txt: names GYO::Render outside the window's links",
            f"{TOOL}/CMakeLists.txt: names imgui::core outside the window's links",
            f"{TOOL}/CMakeLists.txt: names GYO::PlatformSDL outside the window's links",
        ]))

    def test_sdl_and_native_targets_fail_everywhere(self):
        findings = self.scan({f"{TOOL}/CMakeLists.txt": "target_link_libraries(gyo_voxel_map_editor_window PRIVATE "
                                                        "SDL3::SDL3 GYO::RenderBackendSDLNative)\n"})
        # Each is named outside both lists and linked outside the window's list.
        self.assertEqual(len(findings), 4)

    def test_hidden_ways_round_the_lists_fail(self):
        # Found by the §14.4 verifier: each passed the first split guard.
        findings = self.scan({
            f"{TOOL}/src/window/W.cpp": '#include "gyo/framework/imgui/XNative.hpp"\n'
                                        '#include "gyo/framework/imgui/imgui_internal.h"\n'
                                        '#include "gyo/framework/imgui/backends/imgui_impl_sdl3.h"\n'
                                        '#include "window/../SDL3/SDL.h"\n'
                                        '#include "engine/../SomeGame/World.hpp"\n',
            f"{TOOL}/CMakeLists.txt": "link_libraries(GYO::Render)\n"
                                      "set_property(TARGET gyo_voxel_map_editor_core APPEND PROPERTY "
                                      "INTERFACE_LINK_LIBRARIES GYO::PlatformSDL)\n"
                                      "set(_v imgui::core)\n"
                                      "target_link_libraries(gyo_voxel_map_editor_core PRIVATE "
                                      "$<TARGET_NAME:GYO::ToolFrameworkImGui>)\n"
                                      "target_link_libraries(gyo_voxel_map_editor_core PUBLIC gyo_voxel_map_editor_window)\n"
                                      "foreach(_target IN ITEMS GYO::ToolFrameworkPreview)\nendforeach()\n"
                                      "target_link_libraries(gyo_voxel_map_editor_window PRIVATE GYO::RenderBackendSDL)\n"
                                      "target_link_libraries(gyo_voxel_map_editor PRIVATE gyo_voxel_map_editor_window)\n"})
        self.assertEqual(sorted(findings), sorted([
            f"{TOOL}/src/window/W.cpp: includes gyo/framework/imgui/XNative.hpp",
            f"{TOOL}/src/window/W.cpp: includes gyo/framework/imgui/imgui_internal.h",
            f"{TOOL}/src/window/W.cpp: includes gyo/framework/imgui/backends/imgui_impl_sdl3.h",
            f"{TOOL}/src/window/W.cpp: includes window/../SDL3/SDL.h",
            f"{TOOL}/src/window/W.cpp: includes engine/../SomeGame/World.hpp",
            f"{TOOL}/CMakeLists.txt: names GYO::Render outside the window's links",
            f"{TOOL}/CMakeLists.txt: names GYO::PlatformSDL outside the window's links",
            f"{TOOL}/CMakeLists.txt: names imgui::core outside the window's links",
            f"{TOOL}/CMakeLists.txt: names GYO::ToolFrameworkImGui outside the window's links",
            f"{TOOL}/CMakeLists.txt: gyo_voxel_map_editor_core links gyo_voxel_map_editor_window",
            f"{TOOL}/CMakeLists.txt: names gyo_voxel_map_editor_window outside its own commands and the executable's links",
            f"{TOOL}/CMakeLists.txt: links outside target_link_libraries",
            f"{TOOL}/CMakeLists.txt: gyo_voxel_map_editor_core links $<TARGET_NAME:GYO::ToolFrameworkImGui>, "
            "not a literal name",
        ]))

    def test_links_only_by_literal_names(self):
        # Found by the §14.4 verifier's second round.
        findings = self.scan({f"{TOOL}/CMakeLists.txt":
                              "foreach(_target IN ITEMS GYO::ToolFrameworkPreview)\n"
                              "    target_link_libraries(gyo_voxel_map_editor_core PUBLIC ${_target})\n"
                              "endforeach()\n"
                              "target_link_libraries(gyo_voxel_map_editor_core PUBLIC $<TARGET_NAME:gyo_voxel_map_editor_window>)\n"
                              "set_property(TARGET gyo_voxel_map_editor_core APPEND PROPERTY INTERFACE_LINK_LIBRARIES x)\n"
                              'set(_x "#") link_libraries(GYO::Render)\n'})
        self.assertEqual(sorted(findings), sorted([
            f"{TOOL}/CMakeLists.txt: links outside target_link_libraries",
            f"{TOOL}/CMakeLists.txt: names GYO::Render outside the window's links",
            f"{TOOL}/CMakeLists.txt: gyo_voxel_map_editor_core links ${{_target}}, not a literal name",
            f"{TOOL}/CMakeLists.txt: gyo_voxel_map_editor_core links $<TARGET_NAME:gyo_voxel_map_editor_window>, not a literal name",
            f"{TOOL}/CMakeLists.txt: names gyo_voxel_map_editor_window outside its own commands and the executable's links",
        ]))

    def test_third_round_ways_round_the_lists_fail(self):
        # Found by the §14.4 verifier's third round, and the pre-existing weaknesses it listed.
        findings = self.scan({
            f"{TOOL}/src/window/W.cpp": '#include "gyo/framework/imgui/XNATIVE.hpp"\n'
                                        '#include "window\\..\\SDL3\\SDL.h"\n'
                                        '#include "gyo/framework/imgui/./X.hpp"\n'
                                        '#include "gyo/framework/imgui/backendsX.hpp"\n',
            f"{TOOL}/src/Main.cpp": '#include "engine/render/RenderTypes.hpp"\n'
                                    '#include "engine/platform/sdl/SdlPlatform.hpp"\n'
                                    '#include "gyo/framework/Preview.hpp"\n',
            f"{TOOL}/CMakeLists.txt":
                "set_property(TARGET gyo_voxel_map_editor_core APPEND PROPERTY INTERFACE_LINK_LIBRARIES_DIRECT x)\n"
                "target_sources(gyo_voxel_map_editor_core PRIVATE gyo_voxel_map_editor_window)\n"
                "set(X [[#]]) target_link_libraries(gyo_voxel_map_editor_core PUBLIC GYO::Render)\n"
                "target_link_options(gyo_voxel_map_editor_core PRIVATE -lSDL3)\n"})
        self.assertEqual(sorted(findings), sorted([
            f"{TOOL}/src/window/W.cpp: includes gyo/framework/imgui/XNATIVE.hpp",
            f"{TOOL}/src/window/W.cpp: includes window\\..\\SDL3\\SDL.h",
            f"{TOOL}/src/window/W.cpp: includes gyo/framework/imgui/./X.hpp",
            f"{TOOL}/src/window/W.cpp: includes gyo/framework/imgui/backendsX.hpp",
            f"{TOOL}/src/Main.cpp: includes engine/render/RenderTypes.hpp",
            f"{TOOL}/src/Main.cpp: includes engine/platform/sdl/SdlPlatform.hpp",
            f"{TOOL}/src/Main.cpp: includes gyo/framework/Preview.hpp",
            f"{TOOL}/CMakeLists.txt: links outside target_link_libraries",
            f"{TOOL}/CMakeLists.txt: names gyo_voxel_map_editor_window outside its own commands and the executable's links",
            f"{TOOL}/CMakeLists.txt: names GYO::Render outside the window's links",
            f"{TOOL}/CMakeLists.txt: gyo_voxel_map_editor_core links GYO::Render",
        ]))

    def test_other_engine_modules_still_fail(self):
        # Allowing GYO::MapMarkers allows that one module, not Engine at large: each other name is reported as
        # named and as linked, exactly, and GYO::MapMarkers itself never.
        findings = self.scan({f"{TOOL}/CMakeLists.txt": "target_link_libraries(gyo_voxel_map_editor_core PUBLIC "
                                                        "GYO::Collision GYO::MapMarkersNative GYO::MapMarkers.extra "
                                                        "GYO::MapMarkers)\n"})
        expected = []
        for name in ("GYO::Collision", "GYO::MapMarkersNative", "GYO::MapMarkers.extra"):
            expected += [f"{TOOL}/CMakeLists.txt: names {name}", f"{TOOL}/CMakeLists.txt: gyo_voxel_map_editor_core links {name}"]
        self.assertEqual(sorted(findings), sorted(expected))

    def test_the_entry_header_and_the_stand_in_keep_the_core_rules(self):
        # Review round of v8-tool-voxel-window-1 (A): the core includes the entry header and compiles the stand-in.
        findings = self.scan({f"{TOOL}/src/window/EditorWindow.hpp": '#include "imgui.h"\n#include "engine/render/Renderer.hpp"\n',
                              f"{TOOL}/src/window/EditorWindowUnavailable.cpp": '#include "gyo/framework/imgui/ImGuiShell.hpp"\n',
                              f"{TOOL}/src/window/Other.cpp": '#include "imgui.h"\n#include "engine/render/Renderer.hpp"\n'})
        self.assertEqual(sorted(findings), sorted([
            f"{TOOL}/src/window/EditorWindow.hpp: includes imgui.h",
            f"{TOOL}/src/window/EditorWindow.hpp: includes engine/render/Renderer.hpp",
            f"{TOOL}/src/window/EditorWindowUnavailable.cpp: includes gyo/framework/imgui/ImGuiShell.hpp",
        ]))

    def test_window_sources_only_in_the_window_library(self):
        # (B) A file of src/window/ gets the window's rules, so only the window library may compile it.
        findings = self.scan({f"{TOOL}/CMakeLists.txt":
                              "add_library(gyo_voxel_map_editor_window STATIC src/window/A.cpp)\n"
                              "target_sources(gyo_voxel_map_editor_window PRIVATE src/window/B.cpp)\n"
                              "add_library(gyo_voxel_map_editor_core STATIC src/Core.cpp src/window/W.cpp)\n"
                              "target_sources(gyo_voxel_map_editor PRIVATE src/window/X.cpp)\n"
                              "target_sources(gyo_voxel_map_editor PRIVATE src/window/EditorWindowUnavailable.cpp)\n"})
        self.assertEqual(sorted(findings), sorted([
            f"{TOOL}/CMakeLists.txt: compiles src/window/W.cpp outside the window library",
            f"{TOOL}/CMakeLists.txt: compiles src/window/X.cpp outside the window library",
        ]))

    def test_the_window_only_in_the_gui_variant(self):
        # (C) The window library and its targets only inside if(GYO_TOOL_VARIANT STREQUAL "gui"), up to its else.
        cmake = ('if(GYO_TOOL_VARIANT STREQUAL "gui")\n'
                 "    if(WIN32)\n    endif()\n"
                 "    add_library(gyo_voxel_map_editor_window STATIC src/window/A.cpp)\n"
                 "    target_link_libraries(gyo_voxel_map_editor_window PRIVATE imgui::core)\n"
                 "else()\n"
                 "    target_link_libraries(gyo_voxel_map_editor_window PRIVATE GYO::Render)\n"
                 "endif()\n"
                 "add_library(gyo_voxel_map_editor_window STATIC src/window/B.cpp)\n")
        findings = self.scan({f"{TOOL}/CMakeLists.txt": cmake}, gui=False)
        self.assertEqual(sorted(findings), sorted([
            f"{TOOL}/CMakeLists.txt: names gyo_voxel_map_editor_window outside the gui variant",
            f"{TOOL}/CMakeLists.txt: names gyo_voxel_map_editor_window outside the gui variant",
            f"{TOOL}/CMakeLists.txt: names GYO::Render outside the gui variant",
        ]))

    def test_bare_sdl_headers_and_upper_case_imgui_paths_fail(self):
        findings = self.scan({f"{TOOL}/src/A.cpp": '#include <SDL.h>\n#include "SDL_events.h"\n'
                                                   '#include "gyo/framework/IMGUI/Foo.hpp"\n',
                              f"{TOOL}/src/window/B.cpp": '#include <SDL.h>\n'})
        self.assertEqual(sorted(findings), sorted([
            f"{TOOL}/src/A.cpp: includes SDL.h",
            f"{TOOL}/src/A.cpp: includes SDL_events.h",
            f"{TOOL}/src/A.cpp: uses SDL_events",  # the SDL_ name rule sees it too
            f"{TOOL}/src/A.cpp: includes gyo/framework/IMGUI/Foo.hpp",
            f"{TOOL}/src/window/B.cpp: includes SDL.h",
        ]))

    def test_game_include_and_plain_target_fail(self):
        findings = self.scan({f"{TOOL}/src/A.cpp": '#include "SomeGame/World.hpp"\n',
                              f"{TOOL}/CMakeLists.txt": "target_link_libraries(gyo_voxel_map_editor PRIVATE some_game_domain)\n"
                                                        "target_link_libraries(gyo_voxel_map_editor_window PRIVATE other_game)\n"})
        self.assertEqual(len(findings), 3)

    def test_registered_id_fails_but_the_own_id_passes(self):
        files = {"engine/config/tools.csv": "name,description\nvoxel_map_editor,x\nsome_tool,y\n",
                 "engine/config/projects.csv": "name,description\nsome_game,z\n",
                 f"{TOOL}/src/A.cpp": "// voxel_map_editor builds some_game maps\n"}
        self.assertEqual(self.scan(files), [f"{TOOL}/src/A.cpp: names the registered id some_game"])


if __name__ == "__main__":
    unittest.main()
