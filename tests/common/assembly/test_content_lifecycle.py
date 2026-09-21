"""Build a compiler-free graph to verify source-presence changes and deployment."""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "build"))
from workspace import TemporaryDirectory


@unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja"), "CMake and Ninja are required")
class ContentLifecycleTests(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory(prefix="gyo-content-lifecycle-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source, self.binary = self.root / "source", self.root / "build"
        self.output, self.preview, self.installed = (self.root / name for name in ("products", "preview", "installed"))
        (self.source / "build/cmake").mkdir(parents=True)
        for relative in ("build/cmake/GyoContent.cmake", "build/assemble_runtime.py", "build/content_contract.py"):
            if (ROOT / relative).is_file():
                shutil.copy2(ROOT / relative, self.source / relative)
        (self.source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.30)\nproject(ContentLifecycle NONE)\n'
            'set(GYO_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")\n'
            f'set(GYO_OUTPUT_ROOT "{self.output.as_posix()}")\n'
            'include(build/cmake/GyoContent.cmake)\n'
            'add_custom_target(game ALL)\ngyo_attach_content(game sample)\n'
            'add_custom_target(preview ALL)\n'
            f'gyo_stage_app_content(preview sample "{self.preview.as_posix()}")\n', encoding="utf-8")
        self.assets = self.source / "assets/sample"

    def run_command(self, *command, success=True):
        result = subprocess.run([str(value) for value in command], capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=60)
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def prepare_assets(self):
        self.assets.mkdir(parents=True, exist_ok=True)
        (self.assets / "content.json").write_text(json.dumps({
            "version": 1, "catalogs": ["catalog.json"], "shader_bundles": []}))
        (self.assets / "catalog.json").write_text(json.dumps({
            "version": 1, "assets": [{"id": "sample", "type": "text", "path": "data.txt"}]}))
        (self.assets / "data.txt").write_text("current")

    def test_removing_and_adding_asset_root_updates_build_install_and_preview(self):
        self.prepare_assets()
        self.run_command("cmake", "-S", self.source, "-B", self.binary, "-G", "Ninja")
        self.run_command("cmake", "--build", self.binary)
        self.run_command("cmake", "--install", self.binary, "--component", "sample", "--prefix", self.installed)
        stages = (self.output / "sample", self.preview, self.installed)
        for stage in stages:
            self.assertTrue((stage / "bin/assets/sample/data.txt").is_file())
            other = stage / "bin/assets/other/keep.txt"
            other.parent.mkdir(parents=True)
            other.write_text("keep")
        shutil.rmtree(self.assets)
        self.run_command("cmake", "--build", self.binary)
        self.run_command("cmake", "--install", self.binary, "--component", "sample", "--prefix", self.installed)
        for stage in stages:
            self.assertFalse((stage / "bin/assets/sample").exists(), str(stage))
            self.assertEqual((stage / "bin/assets/other/keep.txt").read_text(), "keep")

        # The root reappears without a descriptor: building must reconfigure and fail.
        self.assets.mkdir()
        self.run_command("cmake", "--build", self.binary, success=False)
        self.prepare_assets()
        self.run_command("cmake", "--build", self.binary)
        for stage in (self.output / "sample", self.preview):
            self.assertEqual((stage / "bin/assets/sample/data.txt").read_text(), "current")
        (self.assets / "data.txt").write_text("changed without CMake reload")
        self.run_command("cmake", "--build", self.binary)
        self.assertEqual((self.output / "sample/bin/assets/sample/data.txt").read_text(), "changed without CMake reload")

    def test_assetless_product_can_build_then_acquire_content_without_reload(self):
        self.run_command("cmake", "-S", self.source, "-B", self.binary, "-G", "Ninja")
        self.run_command("cmake", "--build", self.binary)
        self.run_command("cmake", "--install", self.binary, "--component", "sample", "--prefix", self.installed)
        self.prepare_assets()
        self.run_command("cmake", "--build", self.binary)
        self.assertTrue((self.output / "sample/bin/assets/sample/data.txt").is_file())

    def test_invalid_existing_content_preserves_every_successful_stage(self):
        self.prepare_assets()
        self.run_command("cmake", "-S", self.source, "-B", self.binary, "-G", "Ninja")
        self.run_command("cmake", "--build", self.binary)
        self.run_command("cmake", "--install", self.binary, "--component", "sample", "--prefix", self.installed)
        stages = (self.output / "sample", self.preview, self.installed)

        def snapshot(stage):
            root = stage / "bin/assets/sample"
            return {path.relative_to(root).as_posix(): path.read_bytes()
                    for path in root.rglob("*") if path.is_file()}

        previous = [snapshot(stage) for stage in stages]
        for invalid in ("missing descriptor", "invalid catalog"):
            with self.subTest(invalid=invalid):
                self.prepare_assets()
                (self.assets / "data.txt").write_text("must not be deployed")
                if invalid == "missing descriptor":
                    (self.assets / "content.json").unlink()
                else:
                    (self.assets / "catalog.json").write_text('{"version":1,"assets":[0]}')
                self.run_command("cmake", "--build", self.binary, success=False)
                self.run_command("cmake", "--install", self.binary, "--component", "sample",
                                 "--prefix", self.installed, success=False)
                self.assertEqual([snapshot(stage) for stage in stages], previous)


if __name__ == "__main__":
    unittest.main()
