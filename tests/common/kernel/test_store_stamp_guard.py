"""Self-test of store_stamp_guard.py: each rule catches a planted hit and passes a clean tree."""
import io
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import store_stamp_guard  # noqa: E402


def run(files):
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        for relative, text in files.items():
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
        out = io.StringIO()
        with redirect_stdout(out):
            status = store_stamp_guard.main(["--root", str(root)])
        return status, out.getvalue()


CLEAN = "template <typename T>\nT& InsertEntity(std::vector<T>& column, T value);\n// Keys only; the caller gives every value.\n"


class StoreStampGuardTest(unittest.TestCase):
    def test_clean_tree_passes(self):
        status, out = run({"engine/entity_store/include/engine/entity_store/EntityColumns.hpp": CLEAN})
        self.assertEqual(status, 0, out)

    def test_missing_root_is_a_finding(self):
        status, out = run({"engine/other/x.hpp": CLEAN})
        self.assertEqual(status, 1)
        self.assertIn("S0", out)

    def test_stamp_field_is_reported(self):
        status, out = run({"engine/entity_store/a.hpp": CLEAN + "std::uint64_t changed;\n"})
        self.assertEqual(status, 1)
        self.assertIn("S1 changed", out)

    def test_stamp_word_in_a_comment_is_reported(self):
        status, out = run({"engine/entity_store/a.hpp": CLEAN + "// skip cells whose stamp is old\n"})
        self.assertEqual(status, 1)
        self.assertIn("S1 stamp", out)

    def test_change_stamp_type_is_reported(self):
        status, out = run({"engine/entity_store/CMakeLists.txt": "# ChangeStamp lives here\n"})
        self.assertEqual(status, 1)
        self.assertIn("S1 ChangeStamp", out)

    def test_added_and_tick_are_reported(self):
        status, out = run({"engine/entity_store/a.hpp": "bool added = false;\nint tickOf(int);\n"})
        self.assertEqual(status, 1)
        self.assertIn("S1 added", out)
        self.assertIn("S1 tick", out)

    def test_words_that_only_contain_the_letters_pass(self):
        status, out = run({"engine/entity_store/a.hpp": CLEAN + "// unchanged order; padded; ticket; stampede-free\n"})
        self.assertEqual(status, 0, out)


if __name__ == "__main__":
    unittest.main()
