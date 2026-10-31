"""Self-test of framework_dependency_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import framework_dependency_guard as guard  # noqa: E402

TOOLS = "name,description,version,enabled,default,release,windows,linux,macos\nsketch_tool,,1,true,true,false,true,true,true\n"
PROJECTS = "name,description,version,enabled,windows,linux,macos\nsome_game,,v1,1,1,1,1\n"


class FrameworkDependencyGuardTests(unittest.TestCase):
    def scan(self, files: dict[str, str]) -> list[str]:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, text in {"engine/config/tools.csv": TOOLS, "engine/config/projects.csv": PROJECTS, **files}.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
            return guard.scan(root)

    def test_clean_tree_passes(self):
        self.assertEqual(self.scan({
            "tools/framework/src/A.cpp": '#include "gyo/framework/A.hpp"\n#include "engine/os/Files.hpp"\n'
                                         '#include <SDL3/SDL.h>\n#include <backends/imgui_impl_sdl3.h>\n'
                                         '#include "Local.hpp"\n#include <vector>\n',
            "tools/framework/CMakeLists.txt": "add_library(gyo_tool_framework STATIC a.cpp)\n"
                                              "target_link_libraries(gyo_tool_framework PUBLIC GYO::Engine imgui::core)\n",
        }), [])

    def test_input_include_fails(self):
        self.assertEqual(len(self.scan({"tools/framework/src/A.cpp": '#include "engine/input/InputFrame.hpp"\n'})), 1)

    def test_tool_header_fails(self):
        # A tool's header is outside every allowed prefix, without the guard naming the tool.
        self.assertEqual(len(self.scan({"tools/framework/src/A.cpp": '#include "gyo/some_tool/Thing.hpp"\n'})), 1)

    def test_sdl_gpu_use_fails(self):
        findings = self.scan({"tools/framework/src/A.cpp": "auto* device = SDL_CreateGPUDevice(0, false, nullptr);\n"})
        self.assertEqual(len(findings), 1)
        self.assertIn("SDL_CreateGPUDevice", findings[0])

    def test_unlisted_target_fails(self):
        findings = self.scan({"tools/framework/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Input)\n"})
        self.assertEqual(len(findings), 1)

    def test_registered_tool_name_fails(self):
        findings = self.scan({"tools/framework/src/A.cpp": "// opens what sketch_tool saved\n"})
        self.assertEqual(findings, ["tools/framework/src/A.cpp: names sketch_tool"])

    def test_registered_product_name_fails(self):
        findings = self.scan({"tools/framework/CMakeLists.txt": "if(some_game)\nendif()\n"})
        self.assertEqual(findings, ["tools/framework/CMakeLists.txt: names some_game"])

    def test_a_longer_word_is_not_a_name(self):
        self.assertEqual(self.scan({"tools/framework/src/A.cpp": "// some_game_like and sketch_tools are not ids\n"}), [])

    def test_tool_project_file_fails(self):
        self.assertEqual(len(self.scan({"tools/framework/project.json": "{}\n"})), 1)

    def test_executable_and_install_fail(self):
        findings = self.scan({"tools/framework/CMakeLists.txt": "add_executable(x a.cpp)\ngyo_tool_install_target(TARGET x ROLE main)\n"})
        self.assertEqual(len(findings), 2)

    def test_registering_framework_fails(self):
        findings = self.scan({"engine/config/tools.csv": TOOLS + "framework,,1,false,false,false,true,true,true\n"})
        self.assertEqual(findings, ["engine/config: registers framework as a product or tool"])


if __name__ == "__main__":
    unittest.main()
