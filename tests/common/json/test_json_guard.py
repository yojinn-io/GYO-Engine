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
    write(root, "engine/json/src/adapters/yyjson/YyjsonDocument.cpp",
          "// the yyjson half\n#include <yyjson.h>\nyyjson_doc* doc = nullptr;\n")
    write(root, "engine/json/src/adapters/fast_float/DecimalToDouble.cpp",
          "// the fast_float half\n#include <fast_float/fast_float.h>\n#include \"DecimalToDouble.hpp\"\n"
          "auto r = fast_float::from_chars(a, b, v);\n")
    write(root, "engine/json/src/adapters/yyjson/Uses.cpp", '#include "../fast_float/DecimalToDouble.hpp"\n')
    write(root, "build/cmake/GyoBuild.cmake", 'set(GYO_APP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")\n')
    write(root, "CMakeLists.txt", "project(x)\n")
    write(root, "engine/json/CMakeLists.txt", "target_link_libraries(gyo_json PRIVATE gyo_yyjson gyo_fast_float)\n")
    write(root, "engine/json/src/Writer.cpp", '// mentions yyjson.h, yyjson_write_number and fast_float:: in a comment\n'
                                               'const char* text = "#include <yyjson.h> yyjson_read";\n'
                                               'const char* raw = R"x(yyjson_doc fast_float::x)x";\n'
                                               "/*\n#include <yyjson.h>\nyyjson_val*\n*/\n")
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
        self.assertIn("J1-YYJSON: nothing allowed found", output)

    def test_positive_control_fast_float(self):
        (self.root / "engine/json/src/adapters/fast_float/DecimalToDouble.cpp").write_text("// nothing\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("J1-FASTFLOAT: nothing allowed found", output)

    def test_missing_root_fails_closed(self):
        with tempfile.TemporaryDirectory() as empty:
            status, output = run(Path(empty))
        self.assertEqual(status, 1)
        self.assertIn("source scan root engine/ not found", output)
        self.assertIn("CMake scan root engine/ not found", output)


    def test_yyjson_link_outside_the_module(self):
        write(self.root, "engine/other/CMakeLists.txt", "target_link_libraries(gyo_other PRIVATE gyo_yyjson)\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/other/CMakeLists.txt:1: J1-LINK: gyo_yyjson", output)

    def test_link_in_other_tests(self):
        write(self.root, "tests/common/kernel/CMakeLists.txt", "target_link_libraries(k PRIVATE gyo_yyjson)\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("tests/common/kernel/CMakeLists.txt:1: J1-LINK", output)

    def test_link_in_a_cmake_module(self):
        write(self.root, "engine/os/cmake/Helper.cmake", "target_link_libraries(x gyo_fast_float)\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/os/cmake/Helper.cmake:1: J1-LINK: gyo_fast_float", output)

    def test_source_dir_in_build_cmake(self):
        write(self.root, "build/cmake/GyoBuild.cmake", "include_directories(${yyjson_SOURCE_DIR}/src)\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("build/cmake/GyoBuild.cmake:1: J1-LINK: yyjson_SOURCE_DIR", output)

    def test_link_in_the_root_cmakelists(self):
        write(self.root, "CMakeLists.txt", "target_link_libraries(app gyo_yyjson)\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("CMakeLists.txt:1: J1-LINK: gyo_yyjson", output)

    def test_fast_float_in_the_yyjson_adapter(self):
        write(self.root, "engine/json/src/adapters/yyjson/Mixed.cpp", "#include <fast_float/fast_float.h>\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("adapters/yyjson/Mixed.cpp:1: J1-FASTFLOAT", output)

    def test_include_with_a_path(self):
        write(self.root, "engine/a/A.cpp", "int x;\n#include \"third/src/yyjson.h\"\n")
        write(self.root, "engine/b/B.cpp", "int y;\n#include <include/fast_float/parse_number.h>\n#include <fast_float.h>\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/a/A.cpp:2: J1-YYJSON", output)
        self.assertIn("engine/b/B.cpp:2: J1-FASTFLOAT", output)
        self.assertIn("engine/b/B.cpp:3: J1-FASTFLOAT", output)

    def test_include_with_spacing(self):
        write(self.root, "engine/a/A.cpp", "int x;\n  #  include <yyjson.h>\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/a/A.cpp:2: J1-YYJSON", output)

    def test_every_source_suffix_is_scanned(self):
        for suffix in (".c", ".cc", ".cxx", ".h", ".hh", ".hxx", ".inl"):
            write(self.root, f"engine/s/File{suffix}", "#include <yyjson.h>\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        for suffix in (".c", ".cc", ".cxx", ".h", ".hh", ".hxx", ".inl"):
            self.assertIn(f"engine/s/File{suffix}:1: J1-YYJSON", output)

    def test_yyjson_name_outside_its_adapter(self):
        write(self.root, "engine/json/src/Writer.cpp", '#include "adapters/yyjson/ParseWith.hpp"\n'
                                                   "void F(const yyjson_alc* a);\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("engine/json/src/Writer.cpp:2: J1-YYJSON-NAME: yyjson_alc", output)

    def test_fast_float_name_outside_its_adapter(self):
        write(self.root, "engine/json/src/adapters/yyjson/Num.cpp", "double d = fast_float::x;\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("adapters/yyjson/Num.cpp:1: J1-FASTFLOAT-NAME", output)

    def test_positive_control_names(self):
        write(self.root, "engine/json/src/adapters/fast_float/DecimalToDouble.cpp", "#include <fast_float/fast_float.h>\n")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("J1-FASTFLOAT-NAME: nothing allowed found", output)

if __name__ == "__main__":
    unittest.main()
