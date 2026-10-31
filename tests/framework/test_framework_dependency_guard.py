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
            "framework/src/A.cpp": '#include "gyo/framework/A.hpp"\n#include "engine/os/Files.hpp"\n'
                                   '#include "Local.hpp"\n#include <vector>\n',
            "framework/CMakeLists.txt": "add_library(gyo_tool_framework STATIC a.cpp)\n"
                                        "target_link_libraries(gyo_tool_framework PUBLIC GYO::Engine GYO::Os)\n",
            "framework/bindings/imgui/src/Shell.cpp":
                '#include "gyo/framework/shell/WindowShell.hpp"\n#include "engine/platform/sdl/SdlPlatformNative.hpp"\n'
                '#include "engine/render/backend/sdl/SdlRendererNative.hpp"\n#include <imgui.h>\n'
                '#include <backends/imgui_impl_sdl3.h>\n#include <backends/imgui_impl_sdlrenderer3.h>\n'
                "void Frame() { ImGui::NewFrame(); }\n",
            "framework/bindings/imgui/CMakeLists.txt":
                "target_link_libraries(gyo_tool_framework_imgui PUBLIC GYO::ToolFrameworkWindow\n"
                "    PRIVATE GYO::PlatformSDLNative GYO::RenderBackendSDLNative imgui::core imgui::sdl3_sdlrenderer3)\n",
            "framework/bindings/json/src/Catalog.cpp": '#include "gyo/framework/A.hpp"\n#include <nlohmann/json.hpp>\n'
                                                       "using Json = nlohmann::ordered_json;\n",
            "framework/bindings/json/CMakeLists.txt":
                "target_link_libraries(gyo_tool_framework_json PUBLIC GYO::ToolFramework PRIVATE nlohmann_json::nlohmann_json)\n",
        }), [])

    def test_sdl_and_imgui_in_the_core_fail(self):
        # Before v8-framework-guard-ratchet the core could include SDL3 and ImGui's SDL backend and link imgui::core.
        findings = self.scan({
            "framework/src/A.cpp": '#include <SDL3/SDL.h>\n#include <backends/imgui_impl_sdl3.h>\n',
            "framework/CMakeLists.txt": "target_link_libraries(gyo_tool_framework PUBLIC imgui::core)\n",
        })
        self.assertEqual(sorted(findings), ["framework/CMakeLists.txt: names imgui::core",
                                            "framework/src/A.cpp: includes SDL3/SDL.h",
                                            "framework/src/A.cpp: includes backends/imgui_impl_sdl3.h"])

    def test_input_include_fails(self):
        self.assertEqual(len(self.scan({"framework/src/A.cpp": '#include "engine/input/InputFrame.hpp"\n'})), 1)

    def test_tool_header_fails(self):
        # A tool's header is outside every allowed prefix, without the guard naming the tool.
        self.assertEqual(len(self.scan({"framework/src/A.cpp": '#include "gyo/some_tool/Thing.hpp"\n'})), 1)

    def test_game_header_fails(self):
        # A game's header is outside every allowed prefix, without the guard naming the game.
        self.assertEqual(len(self.scan({"framework/src/A.cpp": '#include "SomeGame/World.hpp"\n'})), 1)

    def test_sdl_gpu_use_fails(self):
        findings = self.scan({"framework/src/A.cpp": "auto* device = SDL_CreateGPUDevice(0, false, nullptr);\n"})
        self.assertEqual(len(findings), 1)
        self.assertIn("SDL_CreateGPUDevice", findings[0])

    def test_unlisted_target_fails(self):
        findings = self.scan({"framework/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Input)\n"})
        self.assertEqual(len(findings), 1)

    def test_registered_tool_name_fails(self):
        findings = self.scan({"framework/src/A.cpp": "// opens what sketch_tool saved\n"})
        self.assertEqual(findings, ["framework/src/A.cpp: names sketch_tool"])

    def test_registered_product_name_fails(self):
        findings = self.scan({"framework/CMakeLists.txt": "if(some_game)\nendif()\n"})
        self.assertEqual(findings, ["framework/CMakeLists.txt: names some_game"])

    def test_a_longer_word_is_not_a_name(self):
        self.assertEqual(self.scan({"framework/src/A.cpp": "// some_game_like and sketch_tools are not ids\n"}), [])

    def test_tool_project_file_fails(self):
        self.assertEqual(len(self.scan({"framework/project.json": "{}\n"})), 1)

    def test_executable_and_install_fail(self):
        findings = self.scan({"framework/CMakeLists.txt": "add_executable(x a.cpp)\ngyo_tool_install_target(TARGET x ROLE main)\n"})
        self.assertEqual(len(findings), 2)

    def test_registering_framework_fails(self):
        findings = self.scan({"engine/config/tools.csv": TOOLS + "framework,,1,false,false,false,true,true,true\n"})
        self.assertEqual(findings, ["engine/config: registers framework as a product or tool"])


    # Design Invariant 2: the core and the bindings (v8-framework-guard-ratchet).

    def test_bare_imgui_include_in_the_core_fails(self):
        self.assertEqual(self.scan({"framework/src/A.cpp": "#include <imgui.h>\n"}),
                         ["framework/src/A.cpp: includes imgui.h"])

    def test_sdl_forward_declaration_in_the_core_fails(self):
        self.assertEqual(self.scan({"framework/include/gyo/framework/A.hpp": "struct SDL_Window;\n"}),
                         ["framework/include/gyo/framework/A.hpp: uses SDL_Window"])

    def test_native_header_in_the_core_fails(self):
        self.assertEqual(self.scan({"framework/src/A.cpp": '#include "engine/platform/sdl/SdlPlatformNative.hpp"\n'}),
                         ["framework/src/A.cpp: includes engine/platform/sdl/SdlPlatformNative.hpp"])

    def test_json_in_the_core_fails(self):
        findings = self.scan({
            "framework/src/A.cpp": "#include <nlohmann/json.hpp>\nusing Json = nlohmann::json;\n",
            "framework/CMakeLists.txt": "target_link_libraries(a PRIVATE nlohmann_json::nlohmann_json)\n",
        })
        self.assertEqual(sorted(findings), ["framework/CMakeLists.txt: names nlohmann_json::nlohmann_json",
                                            "framework/src/A.cpp: includes nlohmann/json.hpp",
                                            "framework/src/A.cpp: uses nlohmann"])

    def test_native_target_in_the_core_fails(self):
        self.assertEqual(self.scan({"framework/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::PlatformSDLNative)\n"}),
                         ["framework/CMakeLists.txt: names GYO::PlatformSDLNative"])

    def test_symbols_in_comments_and_strings_are_not_code(self):
        self.assertEqual(self.scan({"framework/src/A.cpp": '// unlike SDL_Log or ImGui::Text\nconst char* s = "SDL_Window";\n'}), [])

    def test_imgui_binding_calling_sdl_fails(self):
        self.assertEqual(self.scan({"framework/bindings/imgui/src/A.cpp": "void F() { SDL_Log(\"x\"); }\n"}),
                         ["framework/bindings/imgui/src/A.cpp: uses SDL_Log"])

    def test_imgui_binding_naming_sdl3_fails(self):
        self.assertEqual(self.scan({"framework/bindings/imgui/CMakeLists.txt": "target_link_libraries(a PRIVATE SDL3::SDL3)\n"}),
                         ["framework/bindings/imgui/CMakeLists.txt: names SDL3::SDL3"])

    def test_imgui_binding_other_native_header_fails(self):
        self.assertEqual(self.scan({"framework/bindings/imgui/src/A.cpp": '#include "engine/input/backend/sdl/SdlInputNative.hpp"\n'}),
                         ["framework/bindings/imgui/src/A.cpp: includes engine/input/backend/sdl/SdlInputNative.hpp"])

    def test_json_binding_imgui_fails(self):
        self.assertEqual(self.scan({"framework/bindings/json/src/A.cpp": "#include <imgui.h>\n"}),
                         ["framework/bindings/json/src/A.cpp: includes imgui.h"])

    def test_json_binding_native_header_fails(self):
        self.assertEqual(self.scan({"framework/bindings/json/src/A.cpp": '#include "engine/platform/sdl/SdlPlatformNative.hpp"\n'}),
                         ["framework/bindings/json/src/A.cpp: includes engine/platform/sdl/SdlPlatformNative.hpp"])

    def test_unlisted_binding_fails(self):
        self.assertEqual(self.scan({"framework/bindings/qt/src/A.cpp": "\n"}),
                         ["framework/bindings/qt/src/A.cpp: binding qt has no positive list in the guard"])

    # The allowlist: a ratchet of existing core sites, in the format of the OS boundary guard.

    ALLOWLIST = "tests/framework/framework_guard_allowlist.txt"

    def test_allowlisted_core_sites_pass(self):
        self.assertEqual(self.scan({
            "framework/src/Old.cpp": "#include <SDL3/SDL.h>\nvoid F() { SDL_Log(\"a\"); SDL_Log(\"b\"); }\n",
            self.ALLOWLIST: "# reason\nframework/src/Old.cpp include 1  # old\nframework/src/Old.cpp symbol 2  # old\n"
                            "target:gyo_tool_framework link 1 nlohmann_json::nlohmann_json  # checked by CMake\n",
        }), [])

    def test_one_more_site_in_an_allowlisted_file_fails(self):
        findings = self.scan({
            "framework/src/Old.cpp": "void F() { SDL_Log(\"a\"); SDL_Log(\"b\"); }\n",
            self.ALLOWLIST: "framework/src/Old.cpp symbol 1\n",
        })
        self.assertIn(f"{self.ALLOWLIST}:1: framework/src/Old.cpp symbol has 2 findings, allowlisted 1", findings)

    def test_a_removed_site_asks_to_shrink(self):
        findings = self.scan({"framework/src/Old.cpp": "\n", self.ALLOWLIST: "framework/src/Old.cpp symbol 1\n"})
        self.assertEqual(findings, [f"{self.ALLOWLIST}:1: framework/src/Old.cpp symbol has 0 findings, allowlisted 1; "
                                    "shrink the allowlist"])

    def test_an_entry_for_a_missing_file_fails(self):
        findings = self.scan({self.ALLOWLIST: "framework/src/Gone.cpp symbol 1\n"})
        self.assertEqual(findings, [f"{self.ALLOWLIST}:1: framework/src/Gone.cpp does not exist; remove the entry"])

    def test_a_binding_has_no_exceptions(self):
        findings = self.scan({
            "framework/bindings/json/src/A.cpp": "#include <imgui.h>\n",
            self.ALLOWLIST: "framework/bindings/json/src/A.cpp include 1\n",
        })
        self.assertIn(f"{self.ALLOWLIST}:1: framework/bindings/json/src/A.cpp is not in the framework's core; "
                      "bindings have no exceptions", findings)
        self.assertIn("framework/bindings/json/src/A.cpp: includes imgui.h", findings)

    def test_hard_rules_are_never_allowlisted(self):
        findings = self.scan({
            "framework/src/A.cpp": '#include "engine/input/InputFrame.hpp"\n',
            self.ALLOWLIST: "framework/src/A.cpp include 1\n",
        })
        self.assertIn("framework/src/A.cpp: includes engine/input/InputFrame.hpp", findings)

    def test_trading_an_allowlisted_site_for_an_old_refusal_fails(self):
        # The count stays 1, but GYO::Input was refused before the ratchet, so it is never covered.
        findings = self.scan({
            "framework/Old.cmake": "target_link_libraries(a PRIVATE GYO::Input)\n",
            self.ALLOWLIST: "framework/Old.cmake target 1\n",
        })
        self.assertIn("framework/Old.cmake: names GYO::Input", findings)
        findings = self.scan({
            "framework/src/Old.cpp": '#include "gyo/some_tool/Thing.hpp"\n',
            self.ALLOWLIST: "framework/src/Old.cpp include 1\n",
        })
        self.assertIn("framework/src/Old.cpp: includes gyo/some_tool/Thing.hpp", findings)

    def test_a_link_line_names_its_targets(self):
        findings = self.scan({self.ALLOWLIST: "target:gyo_tool_framework link 2 nlohmann_json::nlohmann_json\n"})
        self.assertEqual(len(findings), 1)
        self.assertIn("expected 'target:<cmake target> link count name,name...'", findings[0])

    def test_local_headers_are_not_library_headers(self):
        self.assertEqual(self.scan({"framework/src/A.cpp": '#include "sdl_utils.h"\n#include "ImGuiHelpers.hpp"\n'
                                                           '#include "FooNative.hpp"\n#include "gyo/framework/WindowNative.hpp"\n'}), [])

    def test_library_bare_headers_fail_in_the_core(self):
        findings = self.scan({"framework/src/A.cpp": "#include <SDL.h>\n#include <SDL_video.h>\n#include <imgui_internal.h>\n#include <json.hpp>\n"})
        self.assertEqual(len(findings), 4)

    def test_a_malformed_line_fails(self):
        findings = self.scan({"framework/src/A.cpp": "\n", self.ALLOWLIST: "framework/src/A.cpp symbol\n"})
        self.assertEqual(findings, [f"{self.ALLOWLIST}:1: expected 'path rule count', got 'framework/src/A.cpp symbol'"])


if __name__ == "__main__":
    unittest.main()
