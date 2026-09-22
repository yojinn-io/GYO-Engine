"""The UI Editor owns its real release acceptance contract."""

import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "build/acceptance/common"))
from package_contract import PLATFORMS, required_checks, validate_manifest


class EditorAcceptanceContractTests(unittest.TestCase):
    def test_editor_contract_covers_every_release_platform_and_profile(self):
        contract = json.loads((ROOT / "build/acceptance/ui_editor/checks.json").read_text(encoding="utf-8"))
        self.assertEqual(contract["version"], 1)
        for platform in PLATFORMS:
            with self.subTest(platform=platform):
                executable = "bin/gyo_ui_editor" + (".exe" if platform == "windows-x64" else "")
                package = {"schema_version": 3, "configuration": "Release", "product": "toolchain", "kind": "toolchain",
                           "platform": platform, "executables": {"ui_editor.main": {
                               "owner": "ui_editor", "role": "main", "path": executable,
                               "runtime_dependencies": ["SDL3"]}},
                           "native_files": [], "required_files": [],
                           "checks": [dict(check, owner="ui_editor") for check in contract["checks"]]}
                validate_manifest(package, "toolchain", platform)
                for profile in ("quick", "release"):
                    selected = required_checks(package, profile)
                    self.assertEqual({check["name"] for check in selected}, {"editor_cli"})
                    self.assertIn("@CHECK_ROOT@/validate_cli.py", selected[0]["command"])
                    self.assertIn("@EXECUTABLE:main@", selected[0]["command"])
                    self.assertFalse(selected[0]["gpu"])


if __name__ == "__main__":
    unittest.main()
