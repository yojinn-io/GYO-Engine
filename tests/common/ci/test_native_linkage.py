"""Product roles, rather than package-wide SDL requirements, drive native checks."""

import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
for folder in ("build", "build/acceptance/common"):
    sys.path.insert(0, str(ROOT / folder))
from workspace import TemporaryDirectory
from package_contract import manifest_path, validate_manifest, validate_product_namespace, valid_installed_file
from validate_package import validate_linkage


def tools_manifest():
    return {"schema_version": 3, "configuration": "Release", "product": "toolchain", "kind": "toolchain",
            "platform": "linux-x64", "executables": {
                "canvas.main": {"owner": "canvas", "role": "main", "path": "bin/canvas",
                                "runtime_dependencies": ["SDL3"]},
                "canvas.cli": {"owner": "canvas", "role": "cli", "path": "bin/packer",
                                "runtime_dependencies": []}},
            "native_files": ["lib/libSDL3.so.0"], "required_files": [], "checks": []}


class NativeLinkageTests(unittest.TestCase):
    def test_mixed_cli_and_gui_use_their_own_linkage_requirements(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage = root / "stage"
            contract = tools_manifest()
            for relative in ("bin/canvas", "bin/packer", "lib/libSDL3.so.0"):
                path = stage / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("fixture")
            path = stage / manifest_path("toolchain")
            path.parent.mkdir(parents=True)
            path.write_text(json.dumps(contract))

            def ldd(command, **kwargs):
                executable = Path(command[1])
                output = "libc.so.6 => /usr/lib/libc.so.6 (0x1)\n"
                if executable.name == "canvas":
                    output += f"libSDL3.so.0 => {executable.parent.parent / 'lib/libSDL3.so.0'} (0x2)\n"
                return subprocess.CompletedProcess(command, 0, output)

            with patch("validate_package.platform.system", return_value="Linux"), \
                    patch("validate_package.subprocess.run", side_effect=ldd) as invocation:
                validate_linkage(stage, "toolchain", root / "logs")
            self.assertEqual(invocation.call_count, 2)

    def test_unregistered_nested_or_nonstandard_programs_are_rejected(self):
        contract = tools_manifest()
        for relative in ("bin/plain", "bin/nested/helper", "lib/forgotten.so"):
            with self.subTest(relative=relative), self.assertRaisesRegex(ValueError, "Unregistered"):
                validate_product_namespace([relative], "toolchain", contract)

    def test_macos_cli_does_not_require_gui_rpath(self):
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            stage = root / "stage"
            contract = tools_manifest()
            contract["platform"] = "macos-arm64"
            contract["native_files"] = ["lib/libSDL3.0.dylib"]
            for relative in ("bin/canvas", "bin/packer", "lib/libSDL3.0.dylib"):
                path = stage / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("fixture")
            path = stage / manifest_path("toolchain")
            path.parent.mkdir(parents=True)
            path.write_text(json.dumps(contract))

            def otool(command, **kwargs):
                if Path(command[-1]).name == "packer":
                    return "/usr/lib/libSystem.B.dylib"
                if command[1] == "-L":
                    return "@rpath/libSDL3.0.dylib"
                return "path @executable_path/../lib (offset 12)"

            with patch("validate_package.platform.system", return_value="Darwin"), \
                    patch("validate_package.subprocess.check_output", side_effect=otool):
                validate_linkage(stage, "toolchain", root / "logs")

    def test_duplicate_paths_and_noncanonical_roles_are_rejected(self):
        for mutation in ("path", "role"):
            contract = tools_manifest()
            if mutation == "path":
                contract["executables"]["canvas.cli"]["path"] = "bin/canvas"
            else:
                contract["executables"]["canvas.cli"]["role"] = "other"
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                validate_manifest(contract, "toolchain", "linux-x64")

    def test_only_native_files_may_use_confined_symlinks(self):
        with TemporaryDirectory() as temporary:
            stage = Path(temporary) / "stage"
            (stage / "lib").mkdir(parents=True)
            (stage / "bin").mkdir()
            target = stage / "lib/libSDL3.so.1"
            target.write_text("library")
            native = stage / "lib/libSDL3.so.0"
            executable = stage / "bin/canvas"
            try:
                native.symlink_to(target.name)
                executable.symlink_to("../lib/libSDL3.so.1")
            except OSError as error:
                self.skipTest(f"Host does not permit symbolic links: {error}")
            contract = tools_manifest()
            self.assertTrue(valid_installed_file(stage, "lib/libSDL3.so.0", contract))
            self.assertFalse(valid_installed_file(stage, "bin/canvas", contract))
            native.unlink()
            outside = Path(temporary) / "outside.so"
            outside.write_text("outside")
            native.symlink_to("../../outside.so")
            self.assertFalse(valid_installed_file(stage, "lib/libSDL3.so.0", contract))


if __name__ == "__main__":
    unittest.main()
