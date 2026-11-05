"""Self-test of model_viewer_dependency_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import model_viewer_dependency_guard as guard  # noqa: E402


class ModelViewerDependencyGuardTests(unittest.TestCase):
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
            "tools/model_viewer/src/A.cpp": '#include "gyo/model_viewer/A.hpp"\n#include "engine/model/Animation.hpp"\n'
                                            '#include "gyo/framework/ReadOnlyAssetCatalog.hpp"\n#include "Local.hpp"\n'
                                            '#include <vector>\n',
            "tools/model_viewer/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Model GYO::AssetUfbx)\n",
        }), [])

    def test_any_engine_module_passes(self):
        self.assertEqual(self.scan({"tools/model_viewer/src/A.cpp": '#include "engine/render/Renderer.hpp"\n'}), [])

    def test_sdl_use_fails(self):
        findings = self.scan({"tools/model_viewer/src/A.cpp": '#include <SDL3/SDL.h>\nSDL_CreateWindow("", 1, 1, 0);\n'})
        self.assertEqual(len(findings), 2)
        self.assertIn("includes SDL3/SDL.h", findings[0])
        self.assertIn("uses SDL_CreateWindow", findings[1])

    def test_native_header_fails(self):
        findings = self.scan({"tools/model_viewer/src/A.cpp": '#include "engine/render/backend/sdl/native/SdlRendererNative.hpp"\n'
                                                              '#include "engine/platform/sdl/SdlPlatformNative.hpp"\n'})
        self.assertEqual(len(findings), 2)

    def test_imgui_internal_fails_with_or_without_a_path(self):
        findings = self.scan({"tools/model_viewer/src/A.cpp": '#include "imgui_internal.h"\n#include <imgui/imgui_internal.h>\n'})
        self.assertEqual(len(findings), 2)

    def test_sdl_and_native_targets_fail(self):
        findings = self.scan({"tools/model_viewer/CMakeLists.txt": "target_link_libraries(a PRIVATE SDL3::SDL3 GYO::RenderBackendSDLNative)\n"})
        self.assertEqual(len(findings), 2)

    def test_game_include_and_target_fail(self):
        # A game's header and library are outside every allowed prefix, without the guard naming the game.
        findings = self.scan({"tools/model_viewer/src/A.cpp": '#include "SomeGame/World.hpp"\n',
                              "tools/model_viewer/CMakeLists.txt": "target_link_libraries(a PRIVATE SomeGame::World)\n"})
        self.assertEqual(len(findings), 2)

    def test_plain_game_target_fails(self):
        # Games' targets are plain names (gyo_app_add_library), not Name::Name.
        findings = self.scan({"tools/model_viewer/CMakeLists.txt":
                              "target_link_libraries(gyo_model_viewer PRIVATE gyo_model_viewer_core some_game_domain)\n"})
        self.assertEqual(findings, ["tools/model_viewer/CMakeLists.txt: links some_game_domain"])

    def test_window_targets_pass(self):
        self.assertEqual(self.scan({"tools/model_viewer/CMakeLists.txt":
                                    "target_link_libraries(gyo_model_viewer_preview PUBLIC GYO::Render GYO::ModelRenderer "
                                    "GYO::PlatformSDL GYO::RenderBackendSDL PRIVATE GYO::RenderBackendSDLGPU)\n"
                                    "target_link_libraries(gyo_model_viewer PRIVATE GYO::ToolFrameworkImGui imgui::core)\n"}), [])

    def test_audio_targets_pass_and_their_native_one_fails(self):
        self.assertEqual(self.scan({"tools/model_viewer/CMakeLists.txt":
                                    "target_link_libraries(a PRIVATE GYO::Audio GYO::AudioBackendSDL)\n"}), [])
        findings = self.scan({"tools/model_viewer/CMakeLists.txt":
                              "target_link_libraries(a PRIVATE GYO::AudioBackendSDLNative GYO::Audio-Native "
                              "GYO::Audio::Native GYO::PlatformSDL::Native)\n"})
        self.assertEqual(len(findings), 4)

    def test_native_and_other_imgui_targets_still_fail(self):
        findings = self.scan({"tools/model_viewer/CMakeLists.txt":
                              "target_link_libraries(a PRIVATE GYO::RenderBackendSDLGPUNative GYO::PlatformSDLNative "
                              "imgui::sdl3_sdlrenderer3 SDL3::SDL3-static)\n"})
        self.assertEqual(len(findings), 4)

    def test_a_name_going_on_after_an_allowed_one_fails(self):
        findings = self.scan({"tools/model_viewer/CMakeLists.txt":
                              "target_link_libraries(a PRIVATE GYO::PlatformSDL-Native imgui::core.sdl3 GYO::Render+x)\n"
                              "# GYO::Model.\n"})
        self.assertEqual(sorted(findings), ["tools/model_viewer/CMakeLists.txt: names GYO::PlatformSDL-Native",
                                            "tools/model_viewer/CMakeLists.txt: names GYO::Render+x",
                                            "tools/model_viewer/CMakeLists.txt: names imgui::core.sdl3"])

    def test_own_targets_and_variables_pass(self):
        self.assertEqual(self.scan({"tools/model_viewer/CMakeLists.txt":
                                    "target_link_libraries(gyo_model_viewer_core\n    PUBLIC GYO::Model\n"
                                    "    PRIVATE ${EXTRA} $<$<BOOL:1>:GYO::Engine>)\n"
                                    "target_link_libraries(gyo_model_viewer PRIVATE gyo_model_viewer_core)\n"}), [])

    def test_registered_id_fails_but_the_own_id_passes(self):
        files = {"engine/config/tools.csv": "name,description\nmodel_viewer,x\nsome_tool,y\n",
                 "engine/config/projects.csv": "name,description\nsome_game,z\n",
                 "tools/model_viewer/src/A.cpp": "// model_viewer reads some_game data\n",
                 "tools/model_viewer/src/B.cpp": "const char* tool = \"some_tool\"; // some_game_extra is another word\n"}
        findings = self.scan(files)
        self.assertEqual(sorted(findings), ["tools/model_viewer/src/A.cpp: names the registered id some_game",
                                            "tools/model_viewer/src/B.cpp: names the registered id some_tool"])


if __name__ == "__main__":
    unittest.main()
