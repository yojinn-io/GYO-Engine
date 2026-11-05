"""Self-test of entity_store_guard.py on synthetic trees (no repository file is read)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import entity_store_guard as guard  # noqa: E402

CLEAN_HEADER = """#pragma once

#include "engine/base/Assert.hpp"
#include "engine/entity_store/EntityId.hpp"

#include <algorithm>
#include <vector>

namespace Engine::EntityStore {
// Static analysis is not a static variable; this comment says static int x;
template <typename T>
struct EntityKey {
    [[nodiscard]] static constexpr EntityId Of(const T& value) noexcept { return value.id; }
};
struct Helper {
    static EntityId Read(const Helper& helper) noexcept;
    static void Reset();
};
template <typename T>
bool Check(const T& value) {
    static_assert(sizeof(T) > 0);
    const auto narrow = static_cast<int>(value.id);
    static constexpr int limit = 4;
    const char* text = "std::unordered_map static int cache;";
    return narrow < limit && text != nullptr;
}
} // namespace Engine::EntityStore
"""


class GuardTree(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.root = Path(self._directory.name)
        self.write("engine/entity_store/include/engine/entity_store/Columns.hpp", CLEAN_HEADER)
        self.write("engine/entity_store/CMakeLists.txt", "add_library(gyo_entity_store INTERFACE)\n")

    def tearDown(self):
        self._directory.cleanup()

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def header(self, body):
        self.write("engine/entity_store/include/engine/entity_store/Added.hpp",
                   "#pragma once\nnamespace Engine::EntityStore {\n" + body + "\n}\n")

    def assertReported(self, needle):
        problems = guard.check(self.root)
        self.assertTrue(any(needle in line for line in problems), f"{needle!r} not in {problems}")

    def assertClean(self):
        self.assertEqual(guard.check(self.root), [])


class Rules(GuardTree):
    def test_clean_tree_passes(self):
        self.assertClean()

    def test_missing_directory_is_reported(self):
        (self.root / "engine/entity_store/CMakeLists.txt").unlink()
        (self.root / "engine/entity_store/include/engine/entity_store/Columns.hpp").unlink()
        self.assertReported("no source to scan")

    def test_product_names_are_reported_in_code_and_comments(self):
        for word in ("object_fps", "RetroFPS", "fps::pvp", "PvpMatch", "pvp", "PlayerId", "playerId", "Gateway",
                     "Match"):
            with self.subTest(word=word):
                self.header("// keyed like " + word + "\n")
                self.assertReported("Added.hpp:3: W1 ")

    def test_product_name_in_cmake_is_reported(self):
        self.write("engine/entity_store/CMakeLists.txt", "target_link_libraries(x INTERFACE RetroFPS)\n")
        self.assertReported("engine/entity_store/CMakeLists.txt:1: W1 RetroFPS")

    def test_english_verb_match_is_allowed(self):
        self.header("// the keys must match exactly\n")
        self.assertClean()

    def test_unordered_and_hash_are_reported(self):
        self.header("inline std::unordered_map<int, int> index;\n")
        self.assertReported("Added.hpp:3: W2 unordered_map")
        self.header("inline auto h = std::hash<int>{}(1);\n")
        self.assertReported("Added.hpp:3: W2 std::hash")

    def test_forbidden_headers_are_reported(self):
        for header in ("chrono", "random", "thread", "atomic", "mutex"):
            with self.subTest(header=header):
                self.write("engine/entity_store/include/engine/entity_store/Added.hpp",
                           "#pragma once\n#include <" + header + ">\n")
                self.assertReported("Added.hpp:2: W3 <" + header + ">")

    def test_forbidden_names_are_reported(self):
        for name in ("std::chrono", "std::random_device", "std::mt19937", "std::thread", "std::atomic",
                     "std::mutex", "std::rand"):
            with self.subTest(name=name):
                self.header("inline void F() { " + name + " x; }\n")
                self.assertReported("Added.hpp:3: W3 " + name)
        self.header("inline int F() { return rand(); }\n")
        self.assertReported("Added.hpp:3: W3 rand")

    def test_static_variables_are_reported(self):
        cases = {
            "inline int F() { static int cache; return cache; }": "W4 static",
            "inline int F() { static int cache = 0; return cache; }": "W4 static",
            "inline int F() { static std::vector<int> cache{}; return 0; }": "W4 static",
            "inline int F() { static int cache(0); return cache; }": "W4 static",
            "inline int F() { thread_local int cache; return cache; }": "W4 thread_local",
            "struct S { static int count; };": "W4 static",
            "struct S { static inline int count = 0; };": "W4 static",
        }
        for body, needle in cases.items():
            with self.subTest(body=body):
                self.header(body)
                self.assertReported("Added.hpp:3: " + needle)

    def test_allowed_static_forms_pass(self):
        for body in ("struct S { static constexpr int limit = 4; };",
                     "struct S { static int Count(const S& s) noexcept; };",
                     "struct S { static void Reset(); };",
                     "struct S { [[nodiscard]] static constexpr int Of(const S& s) noexcept { return 0; } };",
                     "inline int F() { static_assert(true); return static_cast<int>(1.0); }",
                     "inline int F() { static constexpr int table[] = {1, 2}; return table[0]; }"):
            with self.subTest(body=body):
                self.header(body)
                self.assertClean()

    def test_other_engine_include_is_reported(self):
        self.write("engine/entity_store/include/engine/entity_store/Added.hpp",
                   "#pragma once\n#include \"engine/math/linear/Vec3.hpp\"\n#include <engine/os/Files.hpp>\n")
        self.assertReported("Added.hpp:2: W5 engine/math/linear/Vec3.hpp")
        self.assertReported("Added.hpp:3: W5 engine/os/Files.hpp")

    def test_words_in_literals_are_not_code(self):
        self.header('inline const char* Text() { return "unordered_map std::thread static int x;"; }\n')
        self.assertClean()


if __name__ == "__main__":
    unittest.main()
