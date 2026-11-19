"""Self-test of json_guard.py on synthetic trees (no repository file is read)."""
import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import json_guard as guard  # noqa: E402


def write(root, relative, text):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def clean_tree(root):
    write(root, "engine/json/src/adapters/yyjson/YyjsonDocument.cpp", "#include <yyjson.h>\n")
    write(root, "engine/json/src/adapters/fast_float/DecimalToDouble.cpp", "#include <fast_float/fast_float.h>\n")
    write(root, "engine/json/CMakeLists.txt", "target_link_libraries(gyo_json PRIVATE gyo_yyjson gyo_fast_float)\n")
    write(root, "engine/json/src/Writer.cpp", '// mentions yyjson.h and fast_float/ in a comment\n'
                                               'const char* text = "#include <yyjson.h>";\n'
                                               "/*\n#include <yyjson.h>\n*/\n")
    write(root, "engine/other/CMakeLists.txt", "add_library(gyo_other STATIC a.cpp) # not gyo_yyjson\n")
    write(root, "tests/common/json/ComponentTests.cpp", "#include <yyjson.h>\n#include <fast_float/fast_float.h>\n")
    write(root, "tests/common/json/CMakeLists.txt", "target_link_libraries(t PRIVATE gyo_yyjson)\n")


def run(root):
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        status = guard.main(["--root", str(root)])
    return status, output.getvalue()


class JsonGuard(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        clean_tree(self.root)

    def tearDown(self):
        self.directory.cleanup()

    def test_clean_tree_passes(self):
        status, output = run(self.root)
        self.assertEqual(status, 0, output)

    def test_yyjson_outside_its_adapter(self):
        write(self.root, "engine/kernel/src/Fingerprint.cpp", "#include \"yyjson.h\"\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/kernel/src/Fingerprint.cpp:1: J1-YYJSON", output)

    def test_yyjson_in_the_other_adapter(self):
        write(self.root, "engine/json/src/adapters/fast_float/Mixed.cpp", "#include <yyjson.h>\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("adapters/fast_float/Mixed.cpp:1: J1-YYJSON", output)

    def test_fast_float_outside_its_adapter(self):
        write(self.root, "engine/json/src/Writer.cpp", "#include <fast_float/fast_float.h>\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/json/src/Writer.cpp:1: J1-FASTFLOAT", output)

    def test_public_header_including_a_component(self):
        write(self.root, "engine/json/include/engine/json/Document.hpp", "#pragma once\n#include <yyjson.h>\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("Document.hpp:2: J1-YYJSON", output)

    def test_other_tests_are_scanned(self):
        write(self.root, "tests/common/kernel/KernelTests.cpp", "#include <fast_float/fast_float.h>\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("tests/common/kernel/KernelTests.cpp:1: J1-FASTFLOAT", output)

    def test_link_outside_the_module(self):
        write(self.root, "engine/other/CMakeLists.txt", "target_link_libraries(gyo_other PRIVATE gyo_fast_float)\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/other/CMakeLists.txt:1: J1-LINK: gyo_fast_float", output)

    def test_positive_control_yyjson(self):
        (self.root / "engine/json/src/adapters/yyjson/YyjsonDocument.cpp").write_text("// nothing\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("J1-YYJSON: no allowed include found", output)

    def test_positive_control_fast_float(self):
        (self.root / "engine/json/src/adapters/fast_float/DecimalToDouble.cpp").write_text("// nothing\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("J1-FASTFLOAT: no allowed include found", output)

    def test_missing_root_fails_closed(self):
        with tempfile.TemporaryDirectory() as empty:
            status, output = run(Path(empty))
        self.assertEqual(status, 1)
        self.assertIn("scan root engine/ not found", output)


if __name__ == "__main__":
    unittest.main()
