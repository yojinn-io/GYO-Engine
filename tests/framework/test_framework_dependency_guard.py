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
        # The core library's own link list refuses it too (v8-framework-preview-component).
        self.assertEqual(sorted(findings), ["framework/CMakeLists.txt: gyo_tool_framework links imgui::core",
                                            "framework/CMakeLists.txt: names imgui::core",
                                            "framework/src/A.cpp: includes SDL3/SDL.h",
                                            "framework/src/A.cpp: includes backends/imgui_impl_sdl3.h"])

    def test_input_neutral_include_passes(self):
        # Engine's input is open to the framework as to a game or tool (v8-tool-ui-editor-ime-composition-source).
        self.assertEqual(self.scan({"framework/src/A.cpp": '#include "engine/input/InputFrame.hpp"\n'}), [])

    def test_input_native_header_fails_in_the_core(self):
        self.assertEqual(self.scan({"framework/src/A.cpp": '#include "engine/input/backend/sdl/SdlInputNative.hpp"\n'}),
                         ["framework/src/A.cpp: includes engine/input/backend/sdl/SdlInputNative.hpp"])

    def test_input_native_header_and_an_allowlist_line_still_fail(self):
        # Since v8-framework-invariants there are no exceptions: the removed allowlist coming back is itself a finding.
        findings = self.scan({
            "framework/src/A.cpp": '#include "engine/input/backend/sdl/SdlInputNative.hpp"\n',
            self.ALLOWLIST: "framework/src/A.cpp include 1\n",
        })
        self.assertEqual(sorted(findings), sorted([
            "framework/src/A.cpp: includes engine/input/backend/sdl/SdlInputNative.hpp",
            f"{self.ALLOWLIST}: the framework has no exceptions; remove the file"]))

    def test_input_targets_in_the_core_pass(self):
        # Since v8-framework-engine-access the core may use any Engine module (Design Invariant 2); before it, only
        # the ImGui binding could link the input backend (v8-tool-ui-editor-ime-composition-source).
        self.assertEqual(self.scan({
            "framework/src/A.cpp": '#include "engine/input/backend/sdl/SdlInput.hpp"\n',
            "framework/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Input GYO::InputBackendSDL)\n",
        }), [])

    def test_input_target_in_a_binding_fails(self):
        # Each binding keeps its own positive list; the core's additions do not reach it.
        self.assertEqual(self.scan({"framework/bindings/json/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Input)\n"}),
                         ["framework/bindings/json/CMakeLists.txt: names GYO::Input"])

    def test_model_and_audio_targets_in_the_core_pass(self):
        # v8-arch-event-sounds-impl: gyo.event_sounds binds to Engine's event set and checks gains with Engine.
        self.assertEqual(self.scan({
            "framework/src/A.cpp": '#include "engine/model/AnimationEvents.hpp"\n#include "engine/audio/AudioFormat.hpp"\n',
            "framework/CMakeLists.txt": "target_link_libraries(a PUBLIC GYO::Model PRIVATE GYO::Audio)\n",
        }), [])

    def test_audio_backend_target_in_the_core_fails(self):
        # The SDL audio backend carries SDL; Engine's mixer alone is listed.
        self.assertEqual(self.scan({"framework/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::AudioBackendSDL)\n"}),
                         ["framework/CMakeLists.txt: names GYO::AudioBackendSDL"])

    def test_model_target_in_the_json_binding_fails(self):
        # The JSON binding reaches Engine's event set through the core and names it nowhere.
        self.assertEqual(self.scan({"framework/bindings/json/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Model)\n"}),
                         ["framework/bindings/json/CMakeLists.txt: names GYO::Model"])

    def test_ui_target_in_the_core_passes(self):
        # v8-input-ui-adapter: the products' shared text input mapping turns Engine's input entries into Ui's.
        self.assertEqual(self.scan({
            "framework/src/A.cpp": '#include "engine/ui/UiTypes.hpp"\n',
            "framework/CMakeLists.txt": "target_link_libraries(a PUBLIC GYO::Input GYO::Ui)\n",
        }), [])

    def test_ui_target_in_the_imgui_binding_fails(self):
        # The binding reaches Ui only through the core; its own list does not take the core's addition.
        self.assertEqual(self.scan({"framework/bindings/imgui/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Ui)\n"}),
                         ["framework/bindings/imgui/CMakeLists.txt: names GYO::Ui"])

    def test_ui_target_in_the_json_binding_fails(self):
        self.assertEqual(self.scan({"framework/bindings/json/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Ui)\n"}),
                         ["framework/bindings/json/CMakeLists.txt: names GYO::Ui"])

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
        findings = self.scan({"framework/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::ModelRenderer)\n"})
        self.assertEqual(findings, ["framework/CMakeLists.txt: names GYO::ModelRenderer"])

    # v8-framework-preview-component: the offscreen preview is the one core library on Engine's render backends,
    # and each core library links only its own list.

    def test_preview_library_linking_the_render_targets_passes(self):
        self.assertEqual(self.scan({
            "framework/src/Preview.cpp": '#include "engine/render/Renderer.hpp"\n'
                                         '#include "engine/render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"\n',
            "framework/CMakeLists.txt":
                "if(TARGET GYO::RenderBackendSDLGPU AND TARGET GYO::RenderBackendSDL)\n"
                "add_library(gyo_tool_framework_preview STATIC src/Preview.cpp)\n"
                "add_library(GYO::ToolFrameworkPreview ALIAS gyo_tool_framework_preview)\n"
                "target_link_libraries(gyo_tool_framework_preview\n"
                "    PUBLIC gyo_tool_framework GYO::Render GYO::RenderBackendSDLGPU GYO::RenderBackendSDL PRIVATE GYO::Os)\n"
                "endif()\n",
        }), [])

    def test_window_shell_linking_a_render_target_fails(self):
        # The windowed shell creates no render device (Design Invariant 6); the name is in the core's list, the
        # shell's own list refuses it.
        findings = self.scan({"framework/CMakeLists.txt":
                              "target_link_libraries(gyo_tool_framework_window PUBLIC gyo_tool_framework GYO::PlatformSDL GYO::RenderBackendSDL)\n"})
        self.assertEqual(findings, ["framework/CMakeLists.txt: gyo_tool_framework_window links GYO::RenderBackendSDL"])

    def test_core_library_linking_outside_its_list_fails(self):
        findings = self.scan({"framework/CMakeLists.txt": "target_link_libraries(gyo_tool_framework PRIVATE GYO::PlatformSDL)\n"})
        self.assertEqual(findings, ["framework/CMakeLists.txt: gyo_tool_framework links GYO::PlatformSDL"])

    def test_preview_library_linking_another_engine_target_fails(self):
        findings = self.scan({"framework/CMakeLists.txt": "target_link_libraries(gyo_tool_framework_preview PUBLIC GYO::Ui)\n"})
        self.assertEqual(findings, ["framework/CMakeLists.txt: gyo_tool_framework_preview links GYO::Ui"])

    def test_window_shell_linking_a_render_target_in_a_generator_expression_fails(self):
        findings = self.scan({"framework/CMakeLists.txt":
                              "target_link_libraries(gyo_tool_framework_window PUBLIC $<BUILD_INTERFACE:GYO::RenderBackendSDL>)\n"})
        self.assertEqual(findings, ["framework/CMakeLists.txt: gyo_tool_framework_window links GYO::RenderBackendSDL"])

    def test_quoted_link_names_are_read_without_their_quotes(self):
        # CMake reads "X" as X: a quoted generator expression is judged by the names inside it, and a quoted name on
        # the library's list passes (before, the quotes made it look like another name).
        findings = self.scan({"framework/CMakeLists.txt":
                              'target_link_libraries(gyo_tool_framework_window PUBLIC "$<BUILD_INTERFACE:GYO::RenderBackendSDL>")\n'})
        self.assertEqual(findings, ["framework/CMakeLists.txt: gyo_tool_framework_window links GYO::RenderBackendSDL"])
        self.assertEqual(self.scan({"framework/CMakeLists.txt":
                                    'target_link_libraries(gyo_tool_framework PRIVATE "GYO::Os")\n'}), [])
        self.assertEqual(self.scan({"framework/CMakeLists.txt":
                                    'target_link_libraries(gyo_tool_framework PRIVATE "GYO::PlatformSDL")\n'}),
                         ["framework/CMakeLists.txt: gyo_tool_framework links GYO::PlatformSDL"])

    def test_core_library_defined_through_a_variable_fails(self):
        findings = self.scan({"framework/CMakeLists.txt": "add_library(${_name} STATIC src/Other.cpp)\n"})
        self.assertEqual(findings, ["framework/CMakeLists.txt: defines a library through a variable"])

    def test_core_library_without_a_link_list_fails(self):
        findings = self.scan({"framework/CMakeLists.txt": "add_library(gyo_tool_framework_other STATIC src/Other.cpp)\n"})
        self.assertEqual(findings, ["framework/CMakeLists.txt: gyo_tool_framework_other has no link list in the guard"])

    def test_link_call_through_a_variable_is_read_by_the_union_rule(self):
        # A link call on a variable has no library to look up; its names still have to be in the core's list. What
        # the core libraries really link is read at configure time (tests/framework/CMakeLists.txt).
        self.assertEqual(self.scan({"framework/CMakeLists.txt":
                                    "target_link_libraries(${_target} PRIVATE GYO::Render)\n"}), [])
        self.assertEqual(self.scan({"framework/CMakeLists.txt":
                                    "target_link_libraries(${_target} PRIVATE GYO::ModelRenderer)\n"}),
                         ["framework/CMakeLists.txt: names GYO::ModelRenderer"])

    def test_render_native_target_in_the_core_fails(self):
        findings = self.scan({"framework/CMakeLists.txt":
                              "target_link_libraries(gyo_tool_framework_preview PRIVATE GYO::RenderBackendSDLNative)\n"})
        self.assertEqual(sorted(findings), ["framework/CMakeLists.txt: gyo_tool_framework_preview links GYO::RenderBackendSDLNative",
                                            "framework/CMakeLists.txt: names GYO::RenderBackendSDLNative"])

    def test_render_targets_in_a_binding_fail(self):
        self.assertEqual(self.scan({"framework/bindings/json/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::Render)\n"}),
                         ["framework/bindings/json/CMakeLists.txt: names GYO::Render"])

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

    def test_imgui_binding_reads_text_through_the_input_backend(self):
        self.assertEqual(self.scan({
            "framework/bindings/imgui/src/A.cpp": '#include "engine/input/backend/sdl/SdlInput.hpp"\n',
            "framework/bindings/imgui/CMakeLists.txt": "target_link_libraries(a PRIVATE GYO::InputBackendSDL)\n",
        }), [])

    def test_imgui_binding_input_native_target_fails(self):
        self.assertEqual(self.scan({"framework/bindings/imgui/CMakeLists.txt":
                                    "target_link_libraries(a PRIVATE GYO::InputBackendSDLNative)\n"}),
                         ["framework/bindings/imgui/CMakeLists.txt: names GYO::InputBackendSDLNative"])

    def test_json_binding_input_backend_fails(self):
        self.assertEqual(self.scan({"framework/bindings/json/CMakeLists.txt":
                                    "target_link_libraries(a PRIVATE GYO::InputBackendSDL)\n"}),
                         ["framework/bindings/json/CMakeLists.txt: names GYO::InputBackendSDL"])

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

    # No exceptions (v8-framework-invariants): the ratchet allowlist was emptied and removed.

    ALLOWLIST = "tests/framework/framework_guard_allowlist.txt"

    def test_the_removed_allowlist_coming_back_fails(self):
        self.assertEqual(self.scan({self.ALLOWLIST: "# reason\n"}),
                         [f"{self.ALLOWLIST}: the framework has no exceptions; remove the file"])

    def test_former_allowlisted_kinds_fail_in_the_core(self):
        # What the allowlist used to cover: SDL calls, an SDL include and a third-party link in the core.
        findings = self.scan({
            "framework/src/Old.cpp": "#include <SDL3/SDL.h>\nvoid F() { SDL_Log(\"a\"); }\n",
            "framework/Old.cmake": "target_link_libraries(a PRIVATE nlohmann_json::nlohmann_json)\n",
        })
        self.assertEqual(sorted(findings), ["framework/Old.cmake: names nlohmann_json::nlohmann_json",
                                            "framework/src/Old.cpp: includes SDL3/SDL.h",
                                            "framework/src/Old.cpp: uses SDL_Log"])

    def test_sdl_gpu_is_an_sdl_symbol_in_code_only(self):
        # Without the allowlist the SDL_GPU rule went with it: the core's SDL_ symbol rule reports SDL_GPU in code,
        # as it reports every SDL_ name; a mention in a comment or a string is not code.
        self.assertEqual(self.scan({"framework/src/A.cpp": "void F() { SDL_GPUDevice* d = nullptr; }\n"}),
                         ["framework/src/A.cpp: uses SDL_GPUDevice"])
        self.assertEqual(self.scan({"framework/src/A.cpp": "// not SDL_GPUDevice\nconst char* s = \"SDL_GPU\";\n"}), [])
        self.assertEqual(self.scan({"framework/bindings/json/src/A.cpp": "void F() { SDL_GPUDevice* d = nullptr; }\n"}),
                         ["framework/bindings/json/src/A.cpp: uses SDL_GPUDevice"])

    def test_local_headers_are_not_library_headers(self):
        self.assertEqual(self.scan({"framework/src/A.cpp": '#include "sdl_utils.h"\n#include "ImGuiHelpers.hpp"\n'
                                                           '#include "FooNative.hpp"\n#include "gyo/framework/WindowNative.hpp"\n'}), [])

    def test_library_bare_headers_fail_in_the_core(self):
        findings = self.scan({"framework/src/A.cpp": "#include <SDL.h>\n#include <SDL_video.h>\n#include <imgui_internal.h>\n#include <json.hpp>\n"})
        self.assertEqual(len(findings), 4)


if __name__ == "__main__":
    unittest.main()
