"""Self-test of ui_dependency_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ui_dependency_guard as guard  # noqa: E402


class UiDependencyGuardTests(unittest.TestCase):
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
            "engine/ui/src/Ui.cpp": '#include "engine/ui/UiTypes.hpp"\n#include <string>\n',
            "engine/ui/CMakeLists.txt": "target_link_libraries(gyo_ui PUBLIC GYO::Engine GYO::Math)\n",
        }), [])

    def test_input_include_fails(self):
        findings = self.scan({"engine/ui/src/Ui.cpp": '#include "engine/input/InputFrame.hpp"\n'})
        self.assertEqual(len(findings), 1)
        self.assertIn("engine/input/InputFrame.hpp", findings[0])

    def test_input_target_fails(self):
        findings = self.scan({"engine/ui/CMakeLists.txt": "target_link_libraries(gyo_ui PUBLIC GYO::Input)\n"})
        self.assertEqual(len(findings), 1)

    def test_other_owners_are_not_scanned(self):
        self.assertEqual(self.scan({"engine/runtime/src/A.cpp": '#include "engine/input/InputFrame.hpp"\n'}), [])


if __name__ == "__main__":
    unittest.main()
