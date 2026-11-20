"""Self-test of logging_guard.py on synthetic trees (no repository file is read)."""
import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import logging_guard as guard  # noqa: E402


def write(root, relative, text):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def clean_tree(root):
    write(root, "engine/logging/include/engine/logging/Logger.hpp",
          '#pragma once\n#include <string>\n#include "engine/base/Result.hpp"\n#include "engine/logging/Level.hpp"\n')
    write(root, "engine/logging/src/Logger.cpp",
          '#include "LoggerState.hpp"\n#include "engine/os/Process.hpp"\n#include "engine/time/MonotonicClock.hpp"\n'
          '#include "engine/json/Writer.hpp"\n#include "engine/threads/RoleThread.hpp"\n'
          '/* #include "engine/kernel/World.hpp" */\n')
    write(root, "engine/world_logging/include/engine/world_logging/Forward.hpp",
          '#include "engine/kernel/Params.hpp"\n#include "engine/orchestration/Setup.hpp"\n'
          '#include "engine/logging/Write.hpp"\n#include "engine/world_logging/LogWorldLabel.hpp"\n')


def run(root):
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        status = guard.main(["--root", str(root)])
    return status, output.getvalue()


class LoggingGuard(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        clean_tree(self.root)

    def tearDown(self):
        self.directory.cleanup()

    def test_clean_tree_passes(self):
        status, output = run(self.root)
        self.assertEqual(status, 0, output)

    def test_logging_including_the_kernel(self):
        write(self.root, "engine/logging/src/Bad.cpp", '#include "engine/kernel/World.hpp"\n')
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/logging/src/Bad.cpp:1: LG4-LOGGING", output)

    def test_logging_including_world_time(self):
        write(self.root, "engine/logging/src/Bad.cpp", '#include <engine/world_time/TimeTypes.hpp>\n')
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("LG4-LOGGING", output)

    def test_public_header_including_a_private_dependency(self):
        write(self.root, "engine/logging/include/engine/logging/Bad.hpp", '#include "engine/json/Writer.hpp"\n')
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("Bad.hpp:1: LG4-LOGGING-PUBLIC", output)

    def test_each_allowed_module_of_logging(self):
        for module in ("base", "json", "os", "threads", "time", "logging"):
            write(self.root, "engine/logging/src/Each.cpp", f'#include "engine/{module}/X.hpp"\n')
            status, output = run(self.root)
            self.assertEqual(status, 0, module + ": " + output)

    def test_world_logging_including_world_time(self):
        write(self.root, "engine/world_logging/src/Bad.cpp", '#include "engine/world_time/TimeTypes.hpp"\n')
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/world_logging/src/Bad.cpp:1: LG4-WORLD", output)

    def test_each_allowed_module_of_world_logging(self):
        for module in ("base", "logging", "kernel", "orchestration", "world_logging"):
            write(self.root, "engine/world_logging/src/Each.cpp", f'#include "engine/{module}/X.hpp"\n')
            status, output = run(self.root)
            self.assertEqual(status, 0, module + ": " + output)

    def test_an_include_in_a_block_comment_is_not_code(self):
        status, output = run(self.root)
        self.assertNotIn("Logger.cpp:6", output)
        self.assertEqual(status, 0, output)

    def test_missing_directory_fails(self):
        import shutil
        shutil.rmtree(self.root / "engine" / "world_logging")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("LG4-WORLD: engine/world_logging/ not found", output)

    def test_nothing_allowed_seen_fails(self):
        write(self.root, "engine/logging/include/engine/logging/Logger.hpp", "#pragma once\n#include <string>\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("LG4-LOGGING-PUBLIC: no allowed engine include found", output)


if __name__ == "__main__":
    unittest.main()
