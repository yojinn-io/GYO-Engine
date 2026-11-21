"""Self-test of trace_guard.py on synthetic trees (no repository file is read)."""
import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import trace_guard as guard  # noqa: E402


def write(root, relative, text):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def clean_tree(root):
    write(root, "engine/modules/trace/include/engine/trace/Trace.hpp", '#pragma once\n#include <cstdint>\n')
    write(root, "engine/modules/trace/src/Trace.cpp",
          '#include "engine/trace/Trace.hpp"\n#include "engine/base/Trace.hpp"\n#include "engine/logging/Write.hpp"\n'
          '#include "engine/time/MonotonicClock.hpp"\n/* #include "engine/kernel/World.hpp" */\n')


def run(root):
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        status = guard.main(["--root", str(root)])
    return status, output.getvalue()


class TraceGuard(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        clean_tree(self.root)

    def tearDown(self):
        self.directory.cleanup()

    def test_clean_tree_passes(self):
        status, output = run(self.root)
        self.assertEqual(status, 0, output)

    def test_trace_including_the_kernel(self):
        write(self.root, "engine/modules/trace/src/Bad.cpp", '#include "engine/kernel/World.hpp"\n')
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/modules/trace/src/Bad.cpp:1: TR1-TRACE", output)

    def test_trace_including_the_orchestration_layer(self):
        write(self.root, "engine/modules/trace/include/engine/trace/Bad.hpp", '#include <engine/orchestration/Product.hpp>\n')
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("TR1-TRACE", output)

    def test_each_allowed_module_passes(self):
        for module in sorted(guard.ALLOWED):
            write(self.root, "engine/modules/trace/src/Each.cpp", f'#include "engine/{module}/X.hpp"\n')
            status, output = run(self.root)
            self.assertEqual(status, 0, (module, output))

    def test_a_commented_include_is_not_read(self):
        status, _ = run(self.root)
        self.assertEqual(status, 0)

    def test_no_allowed_include_fails(self):
        write(self.root, "engine/modules/trace/src/Trace.cpp", '#include <cstdint>\n')
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("the scan sees nothing", output)

    def test_missing_directory_fails(self):
        empty = tempfile.TemporaryDirectory()
        try:
            status, output = run(Path(empty.name))
        finally:
            empty.cleanup()
        self.assertEqual(status, 1)
        self.assertIn("not found", output)


if __name__ == "__main__":
    unittest.main()
