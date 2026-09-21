"""Exercise real subprocess failures so CI cannot treat missing GPU work as a pass."""

from contextlib import redirect_stdout
import importlib.util
import io
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
import uuid


MODULE_PATH = Path(__file__).resolve().parents[1] / "manual_gpu_smoke.py"
sys.path.insert(0, str(MODULE_PATH.parent))
SPEC = importlib.util.spec_from_file_location("manual_gpu_smoke", MODULE_PATH)
SMOKE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SMOKE)
GPU_LINE = "GYO GPU: driver=vulkan, shader=spirv, available_formats=2, bundle=test-bundle"


class GpuSmokeTests(unittest.TestCase):
    def setUp(self):
        # Inherit normal workspace permissions, including restricted Windows CI
        # environments where Python 3.13's owner-only temporary ACL is unsuitable.
        self.root = Path(tempfile.gettempdir()).resolve() / f"gyo-smoke-test-{uuid.uuid4().hex}"
        self.root.mkdir()
        self.script = self.root / "diagnostic.py"

    def tearDown(self):
        self.assertEqual(self.root.parent, Path(tempfile.gettempdir()).resolve())
        self.assertTrue(self.root.name.startswith("gyo-smoke-test-"))
        shutil.rmtree(self.root)

    def run_script(self, source, timeout=5):
        self.script.write_text(source, encoding="utf-8")
        with redirect_stdout(io.StringIO()):
            return SMOKE.run_case("probe", [sys.executable, str(self.script)],
                                  self.root, self.root, "vulkan", timeout)

    def test_success_records_actual_gpu_contract(self):
        result = self.run_script(f"print({GPU_LINE!r})")
        self.assertTrue(result["passed"])
        self.assertEqual(result["actual_driver"], "vulkan")
        self.assertEqual(result["shader_format"], "spirv")
        self.assertEqual(result["bundle_version"], "test-bundle")

    def test_nonzero_exit_preserves_failure_and_log(self):
        result = self.run_script(f"print({GPU_LINE!r})\nraise SystemExit(17)")
        self.assertFalse(result["passed"])
        self.assertEqual(result["exit_code"], 17)
        self.assertIn("code 17", (self.root / "probe.log").read_text(encoding="utf-8"))

    def test_timeout_preserves_partial_output(self):
        result = self.run_script(f"import time\nprint({GPU_LINE!r}, flush=True)\ntime.sleep(30)", 0.5)
        self.assertFalse(result["passed"])
        self.assertEqual(result["exit_code"], -1)
        self.assertIn("timed out", result["failure"])
        self.assertIn(GPU_LINE, (self.root / "probe.log").read_text(encoding="utf-8"))

    def test_missing_program_is_failure(self):
        with redirect_stdout(io.StringIO()):
            result = SMOKE.run_case("missing", [str(self.root / "absent-program")],
                                    self.root, self.root, "vulkan", 5)
        self.assertFalse(result["passed"])
        self.assertEqual(result["exit_code"], -2)

    def test_zero_exit_without_gpu_initialization_is_failure(self):
        result = self.run_script("print('GPU unavailable; skipping')")
        self.assertFalse(result["passed"])
        self.assertEqual(result["exit_code"], 0)
        self.assertIn("did not report", result["failure"])

    def test_wrong_driver_or_shader_format_is_failure(self):
        for log in (GPU_LINE.replace("driver=vulkan", "driver=metal"),
                    GPU_LINE.replace("shader=spirv", "shader=dxil")):
            with self.subTest(log=log):
                self.assertFalse(self.run_script(f"print({log!r})")["passed"])

    def test_ci_suite_continues_after_failure_and_retains_all_results(self):
        self.script.write_text(
            f"import sys\nprint({GPU_LINE!r})\n"
            "raise SystemExit(9 if '--shader-smoke-test' in sys.argv else 0)\n",
            encoding="utf-8")
        with redirect_stdout(io.StringIO()):
            results = SMOKE.run_suite([sys.executable, str(self.script)], self.root,
                                      self.root, "vulkan", "ci", 5)
        self.assertEqual([item["case"] for item in results], ["custom-shader", "world", "menu"])
        self.assertEqual([item["passed"] for item in results], [False, True, True])
        self.assertEqual(len(list(self.root.glob("*.log"))), 3)
        self.assertFalse(all(item["passed"] for item in results))

    def test_quick_suite_runs_only_the_shader_render(self):
        self.script.write_text(
            f"import sys\nprint({GPU_LINE!r})\n"
            "raise SystemExit(0 if '--shader-smoke-test' in sys.argv else 37)\n",
            encoding="utf-8")
        with redirect_stdout(io.StringIO()):
            results = SMOKE.run_suite([sys.executable, str(self.script)], self.root,
                                      self.root, "vulkan", "quick", 5)
        self.assertEqual([item["case"] for item in results], ["custom-shader"])
        self.assertTrue(results[0]["passed"])
        self.assertEqual([path.name for path in self.root.glob("*.log")], ["custom-shader.log"])


if __name__ == "__main__":
    unittest.main()
