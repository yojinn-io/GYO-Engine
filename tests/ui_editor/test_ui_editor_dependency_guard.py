"""Self-test of ui_editor_dependency_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ui_editor_dependency_guard as guard  # noqa: E402


class EditorDependencyGuardTests(unittest.TestCase):
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
            "tools/ui_editor/src/A.cpp": '#include "gyo/ui_editor/A.hpp"\n#include "engine/ui/UiTypes.hpp"\n'
                                         '#include "Local.hpp"\n#include <vector>\n',
            "tools/ui_editor/src/AssetPreviewContext.cpp":
                '#include <SDL3/SDL.h>\n#include "engine/render/backend/sdl/SdlRendererNative.hpp"\n'
                'SDL_Texture* texture = SDL_CreateTexture(renderer, 0, 0, 1, 1);\n',
            "tools/ui_editor/CMakeLists.txt":
                "target_link_libraries(a PRIVATE GYO::Ui GYO::Input imgui::core GYO::RenderBackendSDLNative)\n",
        }), [])

    def test_any_engine_module_passes(self):
        self.assertEqual(self.scan({"tools/ui_editor/src/A.cpp": '#include "engine/input/InputFrame.hpp"\n',
                                    "tools/ui_editor/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Input)\n"}),
                         [])

    def test_sdl_outside_the_native_files_fails(self):
        findings = self.scan({"tools/ui_editor/src/A.cpp": '#include <SDL3/SDL.h>\nSDL_Log("x");\n'})
        self.assertEqual(len(findings), 2)
        self.assertIn("includes SDL3/SDL.h", findings[0])
        self.assertIn("uses SDL_Log", findings[1])

    def test_native_header_outside_the_native_files_fails(self):
        findings = self.scan({"tools/ui_editor/src/A.cpp": '#include "engine/platform/sdl/SdlPlatformNative.hpp"\n'})
        self.assertEqual(len(findings), 1)

    def test_sdl_target_fails(self):
        findings = self.scan({"tools/ui_editor/CMakeLists.txt": "target_link_libraries(a PRIVATE SDL3::SDL3)\n"})
        self.assertEqual(len(findings), 1)

    def test_unlisted_include_fails(self):
        # A game's header is outside every allowed prefix, without the guard naming the game.
        self.assertEqual(len(self.scan({"tools/ui_editor/src/A.cpp": '#include "SomeGame/World.hpp"\n'})), 1)

    def test_sdl_gpu_use_fails(self):
        findings = self.scan({"tools/ui_editor/src/A.cpp": "auto* device = SDL_CreateGPUDevice(0, false, nullptr);\n"})
        self.assertEqual(len(findings), 1)
        self.assertIn("SDL_CreateGPUDevice", findings[0])

    def test_unlisted_target_fails(self):
        findings = self.scan({"tools/ui_editor/CMakeLists.txt": "target_link_libraries(a PRIVATE SomeGame::World)\n"})
        self.assertEqual(len(findings), 1)


if __name__ == "__main__":
    unittest.main()
