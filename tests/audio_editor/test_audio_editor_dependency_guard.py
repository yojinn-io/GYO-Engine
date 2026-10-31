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

    def test_input_include_fails(self):
        self.assertEqual(len(self.scan({"tools/audio_editor/src/A.cpp": '#include "engine/input/InputFrame.hpp"\n'})), 1)

    def test_unlisted_include_fails(self):
        # A game's header is outside every allowed prefix, without the guard naming the game.
        self.assertEqual(len(self.scan({"tools/audio_editor/src/A.cpp": '#include "SomeGame/World.hpp"\n'})), 1)

    def test_sdl_gpu_use_fails(self):
        findings = self.scan({"tools/audio_editor/src/A.cpp": "auto* device = SDL_CreateGPUDevice(0, false, nullptr);\n"})
        self.assertEqual(len(findings), 1)
        self.assertIn("SDL_CreateGPUDevice", findings[0])

    def test_unlisted_target_fails(self):
        findings = self.scan({"tools/audio_editor/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Input)\n"})
        self.assertEqual(len(findings), 1)


if __name__ == "__main__":
    unittest.main()
