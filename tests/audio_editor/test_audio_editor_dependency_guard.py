"""Self-test of audio_editor_dependency_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audio_editor_dependency_guard as guard  # noqa: E402


class AudioEditorDependencyGuardTests(unittest.TestCase):
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
            "tools/audio_editor/src/A.cpp": '#include "gyo/audio_editor/A.hpp"\n#include "engine/audio/AudioFormat.hpp"\n'
                                            '#include "engine/os/Files.hpp"\n#include "Local.hpp"\n#include <vector>\n',
            "tools/audio_editor/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Audio GYO::Os)\n",
        }), [])

    def test_any_engine_module_passes(self):
        self.assertEqual(self.scan({"tools/audio_editor/src/A.cpp": '#include "engine/input/InputFrame.hpp"\n',
                                    "tools/audio_editor/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Input)\n"}),
                         [])

    def test_sdl_use_fails(self):
        findings = self.scan({"tools/audio_editor/src/A.cpp": '#include <SDL3/SDL.h>\nSDL_OpenAudioDevice(0, nullptr);\n'})
        self.assertEqual(len(findings), 2)
        self.assertIn("includes SDL3/SDL.h", findings[0])
        self.assertIn("uses SDL_OpenAudioDevice", findings[1])

    def test_native_header_fails(self):
        findings = self.scan({"tools/audio_editor/src/A.cpp": '#include "engine/audio/sdl/native/SdlAudioNative.hpp"\n'
                                                              '#include "engine/audio/sdl/SdlAudioNative.h"\n'})
        self.assertEqual(len(findings), 2)

    def test_sdl_target_fails(self):
        findings = self.scan({"tools/audio_editor/CMakeLists.txt": "target_link_libraries(a PRIVATE SDL3::SDL3 GYO::PlatformSDLNative)\n"})
        self.assertEqual(len(findings), 2)

    def test_unlisted_include_fails(self):
        # A game's header is outside every allowed prefix, without the guard naming the game.
        self.assertEqual(len(self.scan({"tools/audio_editor/src/A.cpp": '#include "SomeGame/World.hpp"\n'})), 1)

    def test_sdl_gpu_use_fails(self):
        findings = self.scan({"tools/audio_editor/src/A.cpp": "auto* device = SDL_CreateGPUDevice(0, false, nullptr);\n"})
        self.assertEqual(len(findings), 1)
        self.assertIn("SDL_CreateGPUDevice", findings[0])

    def test_window_and_project_file_pass(self):
        # v8-tool-audio-editor-edit: the framework's ImGui binding with ImGui itself, GYO::Audio's SDL backend and
        # the project file's JSON.
        self.assertEqual(self.scan({
            "tools/audio_editor/src/W.cpp": '#include "gyo/framework/imgui/ImGuiShell.hpp"\n'
                                            '#include "engine/audio/backend/sdl/SdlAudioOutput.hpp"\n'
                                            '#include "nlohmann/json.hpp"\n#include <imgui.h>\n',
            "tools/audio_editor/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::ToolFrameworkImGui GYO::AudioBackendSDL"
                                                 " imgui::core nlohmann_json::nlohmann_json)\n",
        }), [])

    def test_sdl_beside_the_audio_backend_fails(self):
        findings = self.scan({"tools/audio_editor/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::AudioBackendSDL"
                                                                   " SDL3::SDL3 imgui::sdl3_sdlrenderer3)\n"})
        self.assertEqual(len(findings), 2)
        self.assertIn("names SDL3::SDL3", findings[0])
        self.assertIn("names imgui::sdl3_sdlrenderer3", findings[1])

    def test_unlisted_target_fails(self):
        findings = self.scan({"tools/audio_editor/CMakeLists.txt": "target_link_libraries(a PRIVATE SomeGame::World)\n"})
        self.assertEqual(len(findings), 1)


if __name__ == "__main__":
    unittest.main()
