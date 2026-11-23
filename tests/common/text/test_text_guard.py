"""Self-test of text_guard.py on synthetic trees (no repository file is read)."""
import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import text_guard as guard  # noqa: E402

A = "engine/modules/text/src/adapters/"


def write(root, relative, text):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def clean_tree(root):
    write(root, A + "freetype/FreeTypeRaster.cpp",
          '#include "adapters/freetype/FreeTypeRaster.hpp"\n#include <ft2build.h>\n#include FT_FREETYPE_H\n'
          "FT_Face face = nullptr;\n")
    write(root, A + "harfbuzz/HarfBuzzShaping.cpp", "#include <hb.h>\n#include <hb-ot.h>\nhb_font_t* font;\n")
    write(root, A + "libunibreak/Segmentation.cpp",
          "#include <linebreak.h>\n#include <graphemebreak.h>\nset_linebreaks_utf32(a, b, c, d);\n")
    write(root, "engine/modules/text/src/TextInternal.hpp",
          '#include "adapters/freetype/FreeTypeRaster.hpp"\n#include "adapters/harfbuzz/HarfBuzzShaping.hpp"\n'
          '// mentions hb_shape, FT_Face and <hb-ft.h> in a comment\nconst char* s = "hb_font_t FT_Face";\n'
          "/*\n#include <hb.h>\n*/\n")
    write(root, "engine/modules/text/CMakeLists.txt", "target_link_libraries(gyo_text PRIVATE gyo_freetype gyo_harfbuzz)\n")
    write(root, "tests/common/text/CMakeLists.txt", 'set(x "${libunibreak_SOURCE_DIR}/src")\n')
    write(root, "tests/common/text/AdapterTests.cpp", '#include "adapters/libunibreak/Segmentation.hpp"\n')
    write(root, "build/cmake/GyoBuild.cmake", "# gyo_freetype in a comment\nset(a b)\n")
    write(root, "CMakeLists.txt", "project(x)\n")


def run(root):
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        status = guard.main(["--root", str(root)])
    return status, output.getvalue()


class TextGuard(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        clean_tree(self.root)

    def tearDown(self):
        self.directory.cleanup()

    def expect(self, relative, text, finding):
        write(self.root, relative, text)
        status, output = run(self.root)
        self.assertEqual(status, 1, output)
        self.assertIn(finding, output)

    def test_clean_tree_passes(self):
        status, output = run(self.root)
        self.assertEqual(status, 0, output)

    def test_freetype_include_outside_its_adapter(self):
        self.expect("engine/modules/text/src/GlyphAtlas.cpp", "#include <ft2build.h>\n",
                    "GlyphAtlas.cpp:1: TX1-FREETYPE")
        self.expect("engine/modules/ui/src/Draw.cpp", "#include <freetype/freetype.h>\n", "Draw.cpp:1: TX1-FREETYPE")

    def test_freetype_in_the_harfbuzz_adapter(self):
        self.expect(A + "harfbuzz/Mixed.cpp", "#include <ft2build.h>\n", "harfbuzz/Mixed.cpp:1: TX1-FREETYPE")

    def test_harfbuzz_include_outside_its_adapter(self):
        self.expect("engine/modules/text/include/engine/text/TextContext.hpp", "#pragma once\n#include <hb.h>\n",
                    "TextContext.hpp:2: TX1-HARFBUZZ")
        self.expect("tests/common/text/Shape.cpp", '#include "hb-ot.h"\n', "Shape.cpp:1: TX1-HARFBUZZ")

    def test_hb_ft_is_nowhere_allowed(self):
        self.expect(A + "harfbuzz/Glue.cpp", "#include <hb-ft.h>\n", "harfbuzz/Glue.cpp:1: TX1-HBFT")

    def test_libunibreak_include_outside_its_adapter(self):
        self.expect("engine/modules/text/src/Layout.cpp", "#include <linebreak.h>\n", "Layout.cpp:1: TX1-LIBUNIBREAK")

    def test_names_outside_their_adapter(self):
        self.expect("engine/modules/text/src/A.cpp", "FT_Library library;\n", "A.cpp:1: TX1-FREETYPE-NAME")
        self.expect("engine/modules/text/src/B.cpp", "hb_buffer_t* b;\n", "B.cpp:1: TX1-HARFBUZZ-NAME")
        self.expect("engine/modules/text/src/C.cpp", "int x = LINEBREAK_NOBREAK;\n", "C.cpp:1: TX1-LIBUNIBREAK-NAME")

    def test_link_outside_the_module(self):
        self.expect("engine/modules/ui/CMakeLists.txt", "target_link_libraries(gyo_ui PRIVATE gyo_harfbuzz)\n",
                    "engine/modules/ui/CMakeLists.txt:1: TX1-LINK")
        self.expect("tests/common/ui/CMakeLists.txt", 'include_directories("${harfbuzz_SOURCE_DIR}/src")\n',
                    "tests/common/ui/CMakeLists.txt:1: TX1-LINK")

    def test_link_in_build_cmake_and_the_root(self):
        self.expect("build/cmake/GyoText.cmake", "target_link_libraries(x PRIVATE gyo_libunibreak)\n",
                    "build/cmake/GyoText.cmake:1: TX1-LINK")
        self.expect("CMakeLists.txt", "project(x)\nlink_libraries(gyo_freetype)\n", "CMakeLists.txt:2: TX1-LINK")

    def test_link_nothing_allowed_fails_closed(self):
        (self.root / "engine/modules/text/CMakeLists.txt").write_text("add_library(gyo_text STATIC a.cpp)\n",
                                                                       encoding="utf-8")
        (self.root / "tests/common/text/CMakeLists.txt").write_text("add_executable(t t.cpp)\n", encoding="utf-8")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("TX1-LINK: nothing allowed found", output)

    def test_missing_required_root_fails(self):
        import shutil
        shutil.rmtree(self.root / "tests")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("scan root tests/ not found", output)

    def test_nothing_allowed_fails_closed(self):
        (self.root / (A + "harfbuzz/HarfBuzzShaping.cpp")).write_text("// empty\n", encoding="utf-8")
        status, output = run(self.root)
        self.assertEqual(status, 1)
        self.assertIn("TX1-HARFBUZZ: nothing allowed found", output)
        self.assertIn("TX1-HARFBUZZ-NAME: nothing allowed found", output)


if __name__ == "__main__":
    unittest.main()
