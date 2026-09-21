"""Test isolation/failure policies of the opt-in native app-copy integration."""

import argparse
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ci_workspace import TemporaryDirectory
from app_copy_integration import (ASSET_ID, ASSET_MARKER, MODEL_ASSET_ID, SHADER_ID, Commands,
    absent_optional_tooling, assert_package_isolation, assert_product_configuration,
    assert_product_install, configuration_options, create_copy, is_build_input, mirror_sources,
    verify_catalog_relocation)
from test_release_pipeline import manifest


class AppCopyIntegrationTests(unittest.TestCase):
    def setUp(self):
        work = TemporaryDirectory(prefix="gyo-app-copy-test-")
        self.addCleanup(work.cleanup)
        self.root = Path(work.name)

    def write(self, relative, data="fixture"):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(data, encoding="utf-8")
        return path

    def test_source_mirror_uses_working_tree_inputs_and_excludes_other_apps_and_ignored_data(self):
        repository = self.root / "repository"
        repository.mkdir()
        subprocess.run(["git", "init", str(repository)], check=True, capture_output=True)
        # Git object files are read-only on Windows. Release only this test's
        # attributes before the guarded temporary-directory cleanup runs.
        def writable_git_files():
            for path in (repository / ".git").rglob("*"):
                if path.is_file():
                    os.chmod(path, stat.S_IREAD | stat.S_IWRITE)
        self.addCleanup(writable_git_files)
        required = ("CMakeLists.txt", "cmake/GyoAppProject.cmake", "apps/object_fps/CMakeLists.txt",
                    "assets/object_fps/asset_catalog.json")
        for relative in required:
            self.write("repository/" + relative)
        self.write("repository/.gitignore", "build/\napps/object_fps/art_source/\napps/object_fps/tools/\n")
        self.write("repository/obsolete.cpp")
        subprocess.run(["git", "-C", str(repository), "add", "."], check=True, capture_output=True)
        (repository / "obsolete.cpp").unlink()
        self.write("repository/CMakeLists.txt", "modified working-tree build")
        self.write("repository/cmake/NewModule.cmake", "new working-tree module")
        for relative in ("build/heavy.bin", "apps/object_fps/art_source/heavy.bin",
                         "apps/object_fps/tools/heavy.bin", "apps/object_fps_v2/main.cpp",
                         "assets/object_fps_v2/asset_catalog.json", ".idea/workspace.xml"):
            self.write("repository/" + relative)
        destination = self.root / "fixture"
        copied = mirror_sources(repository, destination)
        self.assertEqual(set(copied), {*required, "cmake/NewModule.cmake"})
        self.assertEqual((destination / "CMakeLists.txt").read_text(), "modified working-tree build")
        self.assertFalse((destination / "apps/object_fps_v2").exists())
        with self.assertRaisesRegex(ValueError, "fresh"):
            mirror_sources(repository, destination)
        for path in ("../outside", "/absolute", "C:/drive", "apps\\object_fps"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                is_build_input(path)

    def test_copy_has_independent_assets_and_retains_existing_asset_and_program_ids(self):
        catalog_path = self.write("source/assets/object_fps/asset_catalog.json",
            json.dumps({"version": 1, "assets": [{"id": "object_fps.existing", "type": "text", "path": "data.txt"}]}))
        self.write("source/assets/object_fps/data.txt", "original content")
        spec_path = self.write("source/apps/object_fps/shaders/bundle.json", json.dumps({"version": 1,
            "programs": [{"id": "game/object_fps/original", "vertex": "valid.hlsl", "fragment": "valid.hlsl"}]}))
        self.write("source/apps/object_fps/CMakeLists.txt", "identity derived from directory")
        csv = self.write("source/config/engine/projects.csv", "original registry bytes")
        original_catalog, original_spec = catalog_path.read_bytes(), spec_path.read_bytes()
        source = self.root / "source"
        create_copy(source, "arbitrary_game")
        self.assertEqual(catalog_path.read_bytes(), original_catalog)
        self.assertEqual(spec_path.read_bytes(), original_spec)
        self.assertFalse((source / "assets/arbitrary_game/data.txt").samefile(source / "assets/object_fps/data.txt"))
        (source / "assets/arbitrary_game/data.txt").write_text("copy modified")
        self.assertEqual((source / "assets/object_fps/data.txt").read_text(), "original content")
        copy_catalog = json.loads((source / "assets/arbitrary_game/asset_catalog.json").read_text())
        copy_spec = json.loads((source / "apps/arbitrary_game/shaders/bundle.json").read_text())
        self.assertEqual([entry["id"] for entry in copy_catalog["assets"]], ["object_fps.existing", ASSET_ID])
        self.assertEqual([entry["id"] for entry in copy_spec["programs"]], ["game/object_fps/original", SHADER_ID])
        self.assertEqual(copy_spec["programs"][0]["vertex"], copy_spec["programs"][1]["vertex"])
        self.assertIn("arbitrary_game,Independent integration fixture,,1,1,0,0", csv.read_text())

    def test_reuse_cache_forwards_native_tools_and_sources_without_source_or_app_state(self):
        dependency = self.write("cache/_deps/sdl3-src/CMakeLists.txt").parent
        explicit = self.write("explicit/nlohmann_json/CMakeLists.txt").parent
        cache = self.write("cache/CMakeCache.txt", "\n".join((
            "CMAKE_GENERATOR:INTERNAL=Ninja", "CMAKE_CXX_COMPILER:FILEPATH=cl.exe",
            "CMAKE_HOME_DIRECTORY:INTERNAL=old/source", "GYO_APPS:STRING=wrong_app",
            "GYO_BUILD_UI_EDITOR:BOOL=ON", "GYO_SHADER_HOST_BUILD_DIR:PATH=old/host-build",
            f"FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON:PATH={explicit}")))
        args = argparse.Namespace(reuse_cache=cache, dependency_source_cache=[], generator=None,
            c_compiler=None, cxx_compiler=None, make_program=None, shader_tool=None, configuration="Release")
        generator, values = configuration_options(args)
        self.assertEqual(generator, "Ninja")
        self.assertEqual(values["FETCHCONTENT_SOURCE_DIR_SDL3"], str(dependency.resolve()))
        self.assertEqual(values["FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON"], str(explicit))
        self.assertEqual(values["GYO_BUILD_UI_EDITOR"], "OFF")
        self.assertEqual(values["CMAKE_CXX_COMPILER"], "cl.exe")
        for key in ("CMAKE_HOME_DIRECTORY", "GYO_APPS", "GYO_SHADER_HOST_BUILD_DIR"):
            self.assertNotIn(key, values)
        for key in ("BUILD_TESTING", "GYO_ENABLE_PACKAGING"):
            self.assertNotIn(key, values, "The product phase must prove defaults rather than force OFF")

    def test_optional_sources_are_physically_absent_and_restored_even_after_failure(self):
        paths = ("tests/RootTest.cpp", "third_party/doctest/CMakeLists.txt",
                 "render/tests/RenderTests.cpp", "tools/shader_pipeline/tests/ShaderTest.cpp",
                 ".github/workflows/test.yml", "tools/ci/check.py",
                 "apps/other_game/tests/Tests.cmake", "apps/other_game/tests/package/check.py",
                 "apps/other_game/packaging/Package.cmake", "apps/other_game/ci/old.py")
        for relative in paths:
            self.write("source/" + relative, relative)
        main = self.write("source/apps/other_game/main.cpp", "product source")
        source = self.root / "source"
        with self.assertRaisesRegex(RuntimeError, "product failed"):
            with absent_optional_tooling(source, self.root / "hidden", ("other_game",)):
                for relative in paths:
                    self.assertFalse((source / relative).exists())
                    self.assertEqual((self.root / "hidden" / relative).read_text(), relative)
                self.assertEqual(main.read_text(), "product source")
                raise RuntimeError("product failed")
        for relative in paths:
            self.assertEqual((source / relative).read_text(), relative)
        with self.assertRaisesRegex(ValueError, "outside"):
            with absent_optional_tooling(source, source / "hidden", ("other_game",)):
                self.fail("An in-source hiding directory must not be accepted")

    def test_product_assertions_reject_enabled_axes_dependency_discovery_and_installed_qa(self):
        base = "BUILD_TESTING:BOOL=OFF\nGYO_ENABLE_PACKAGING:BOOL=OFF\n"
        path = self.write("native/CMakeCache.txt", base + "FETCHCONTENT_SOURCE_DIR_DOCTEST:PATH=/inert\n")
        assert_product_configuration(path.parent)
        for content in (base.replace("BUILD_TESTING:BOOL=OFF", "BUILD_TESTING:BOOL=ON"),
                        base.replace("GYO_ENABLE_PACKAGING:BOOL=OFF", "GYO_ENABLE_PACKAGING:BOOL=ON"),
                        base + "Python3_EXECUTABLE:FILEPATH=python\n", base + "doctest_SOURCE_DIR:PATH=/qa\n"):
            with self.subTest(content=content), self.assertRaises(ValueError):
                path.write_text(content, encoding="utf-8")
                assert_product_configuration(path.parent)
        stage = self.root / "product"
        stage.mkdir()
        assert_product_install(stage)
        script = self.write("product/check.py")
        with self.assertRaisesRegex(ValueError, "QA tooling"):
            assert_product_install(stage)
        script.unlink()
        self.write("product/share/gyo/apps/other_game/manifest.json", "{}")
        with self.assertRaisesRegex(ValueError, "QA tooling"):
            assert_product_install(stage)

    def prepare_stage(self, app, copied):
        prefix = f"stage-{app}"
        contract = manifest(app=app)
        contract["executable"] = f"bin/gyo_{app}.exe"
        self.write(f"{prefix}/share/gyo/apps/{app}/manifest.json", json.dumps(contract))
        assets = [dict(id=ASSET_ID)] if copied else []
        shaders = [dict(id=SHADER_ID)] if copied else []
        self.write(f"{prefix}/bin/assets/{app}/asset_catalog.json", json.dumps(dict(assets=assets)))
        self.write(f"{prefix}/bin/shaders/{app}/manifest.json", json.dumps(dict(programs=shaders)))
        if copied:
            self.write(f"{prefix}/bin/assets/{app}/{ASSET_MARKER}")
        return self.root / prefix

    def test_package_assertions_detect_cross_app_content_and_lost_clone_edits(self):
        original = self.prepare_stage("object_fps", False)
        clone = self.prepare_stage("other_game", True)
        assert_package_isolation(original, "object_fps", "other_game")
        assert_package_isolation(clone, "other_game", "other_game")
        for relative in ("bin/gyo_object_fps.exe", "bin/assets/object_fps/unrelated",
                         "bin/shaders/object_fps/manifest.json", "share/gyo/apps/object_fps/manifest.json"):
            with self.subTest(relative=relative):
                foreign = self.write("stage-other_game/" + relative)
                with self.assertRaisesRegex(ValueError, "another application"):
                    assert_package_isolation(clone, "other_game", "other_game")
                foreign.unlink()
                # Remove only this test's now-empty ancestors inside its stage.
                parent = foreign.parent
                while parent != clone and not any(parent.iterdir()):
                    parent.rmdir()
                    parent = parent.parent
        (clone / f"bin/assets/other_game/{ASSET_MARKER}").unlink()
        with self.assertRaisesRegex(ValueError, "leaked or were lost"):
            assert_package_isolation(clone, "other_game", "other_game")
        self.write(f"stage-object_fps/bin/assets/object_fps/{ASSET_MARKER}")
        with self.assertRaisesRegex(ValueError, "leaked or were lost"):
            assert_package_isolation(original, "object_fps", "other_game")

    def test_command_failure_is_fatal_and_keeps_diagnostics(self):
        runner = Commands(self.root, self.root)
        with self.assertRaisesRegex(RuntimeError, "failed \\(19\\)"):
            runner.run("failure", [sys.executable, "-c", "print('native failure evidence'); raise SystemExit(19)"])
        self.assertIn("native failure evidence", (self.root / "failure.log").read_text())

    def relocation_fixture(self):
        executable = self.write("build/arbitrary output/bin/different-model-runner.exe")
        model = self.write("build/arbitrary output/bin/assets/other_game/models/fixture.fbx", "model bytes")
        catalog = self.write("build/arbitrary output/bin/assets/other_game/asset_catalog.json",
            json.dumps({"assets": [{"id": MODEL_ASSET_ID, "type": "model", "path": "models/fixture.fbx"}]}))
        tests = [{"name": "other_game.model_assets", "command": [str(executable), "--no-colors"]}]
        return executable, model, catalog, tests

    def test_catalog_relocation_uses_ctest_command_and_restores_after_success_or_failure(self):
        executable, model, catalog, tests = self.relocation_fixture()
        original_catalog, original_model = catalog.read_bytes(), model.read_bytes()
        runner = Commands(self.root, self.root)
        relocated = model.with_name("catalog-relocated-" + model.name)
        for fail in (False, True):
            def inspect_run(name, command, timeout):
                self.assertEqual(name, "other_game-catalog-relocation")
                self.assertEqual(command, [str(executable), "--no-colors"])
                self.assertEqual(timeout, 300)
                self.assertFalse(model.exists())
                self.assertEqual(relocated.read_bytes(), original_model)
                entry = json.loads(catalog.read_bytes())["assets"][0]
                self.assertEqual(entry["path"], "models/catalog-relocated-fixture.fbx")
                if fail:
                    raise RuntimeError("model test rejected renamed fixture")
            with self.subTest(fail=fail), patch.object(runner, "run", side_effect=inspect_run) as run:
                if fail:
                    with self.assertRaisesRegex(RuntimeError, "rejected renamed"):
                        verify_catalog_relocation(runner, tests, "other_game", self.root / "build")
                else:
                    verify_catalog_relocation(runner, tests, "other_game", self.root / "build")
                run.assert_called_once()
                self.assertEqual(catalog.read_bytes(), original_catalog)
                self.assertEqual(model.read_bytes(), original_model)
                self.assertFalse(relocated.exists())

    def test_catalog_relocation_rejects_missing_or_ambiguous_entries_and_unsafe_paths(self):
        _, model, catalog, tests = self.relocation_fixture()
        runner = Commands(self.root, self.root)
        cases = [[], [{"id": "different.model", "path": "models/fixture.fbx"}],
            [{"id": MODEL_ASSET_ID, "path": "models/fixture.fbx"}] * 2]
        for path in (None, "", "../outside.fbx", "/absolute.fbx", "C:/outside.fbx",
                     "models\\fixture.fbx", "models/missing.fbx", "models"):
            cases.append([{"id": MODEL_ASSET_ID, "path": path}])
        with patch.object(runner, "run") as run:
            for entries in cases:
                with self.subTest(entries=entries):
                    catalog.write_text(json.dumps({"assets": entries}), encoding="utf-8")
                    original = catalog.read_bytes()
                    with self.assertRaises(ValueError):
                        verify_catalog_relocation(runner, tests, "other_game", self.root / "build")
                    self.assertEqual(catalog.read_bytes(), original)
                    self.assertEqual(model.read_text(), "model bytes")
            run.assert_not_called()

    def test_catalog_relocation_rejects_unknown_test_command_and_existing_destination(self):
        executable, model, catalog, tests = self.relocation_fixture()
        runner = Commands(self.root, self.root)
        for invalid in ([], tests * 2, [{"name": "other_game.model_assets"}],
                [{"name": "other_game.model_assets", "command": ["relative.exe"]}],
                [{"name": "other_game.model_assets", "command": [str(self.write("outside.exe"))]}]):
            with self.subTest(tests=invalid), self.assertRaises(ValueError):
                verify_catalog_relocation(runner, invalid, "other_game", self.root / "build")
        with self.assertRaisesRegex(ValueError, "outside the executable directory"):
            verify_catalog_relocation(Commands(self.root, executable.parent), tests, "other_game", self.root / "build")
        relocated = model.with_name("catalog-relocated-" + model.name)
        relocated.write_text("preserve existing data", encoding="utf-8")
        original = catalog.read_bytes()
        with self.assertRaisesRegex(ValueError, "already exists"):
            verify_catalog_relocation(runner, tests, "other_game", self.root / "build")
        self.assertEqual(catalog.read_bytes(), original)
        self.assertEqual(model.read_text(), "model bytes")
        self.assertEqual(relocated.read_text(), "preserve existing data")


if __name__ == "__main__":
    unittest.main()
