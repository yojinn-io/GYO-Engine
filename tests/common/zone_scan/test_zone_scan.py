"""Self-test of zone_scan.py on synthetic owner trees (no repository file is read).

The judgement of docs/architecture/plans/kernel/j17-scanner.zh-Hant.md: the nine positive cases are
each caught by their designated rule at the right line and by no other rule; the seven negative cases
yield no finding and no candidate. The remaining tests cover each rule's edges, the masking, header
expansion and the list contract. Test names are stable: mutations.json names them."""
import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import zone_scan  # noqa: E402

RULES = zone_scan.load_rules(zone_scan.DEFAULT_RULES)
OWNER = "owner"
HEADER = "#pragma once\n"


class OwnerTree(unittest.TestCase):
    """A temporary repository with one owner: owner/src/*.cpp and owner/include/Own/*.hpp."""

    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.root = Path(self._directory.name)
        (self.root / OWNER / "include" / "Own").mkdir(parents=True)
        (self.root / OWNER / "src").mkdir(parents=True)

    def tearDown(self):
        self._directory.cleanup()

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return relative

    def listing(self, files, **overrides):
        data = {"format": zone_scan.FORMAT, "version": 1, "generated_by": "manual", "repo_commit": "2b4618f9",
                "owner_root": OWNER, "include_roots": [OWNER + "/include"], "files": files}
        data.update(overrides)
        path = self.root / "list.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        return path

    def scan(self, files, **overrides):
        return zone_scan.scan(self.root, self.listing(files, **overrides), RULES)

    def scan_source(self, body):
        """Scans one source file owner/src/Case.cpp holding body; returns the report."""
        return self.scan([self.write(OWNER + "/src/Case.cpp", body)])

    def findings(self, report):
        return [(f["file"], f["line"], f["rule"], f["text"]) for f in report["findings"]]

    def assertOnly(self, report, line, rule, text_part=""):
        """Exactly one finding, with this line and rule (text containing text_part); no candidate."""
        found = self.findings(report)
        self.assertEqual(len(found), 1, found)
        self.assertEqual(found[0][1], line, found)
        self.assertEqual(found[0][2], rule, found)
        self.assertIn(text_part, found[0][3], found)
        self.assertEqual(report["candidates"], [], report["candidates"])

    def assertClean(self, report):
        self.assertEqual(report["findings"], [], report["findings"])
        self.assertEqual(report["candidates"], [], report["candidates"])


class PositiveCases(OwnerTree):
    """The seven synthetic violations of spec J-17 plus the two the architect added (ZS7)."""

    def test_positive_static_int(self):
        self.assertOnly(self.scan_source("namespace Own {\nint Count() {\n    static int calls = 0;\n    return ++calls;\n}\n}\n"),
                        3, "ZS2", "static int calls")

    def test_positive_random_device(self):
        self.assertOnly(self.scan_source("namespace Own {\nint Roll() {\n    std::random_device device;\n    return 0;\n}\n}\n"),
                        3, "ZS6", "random_device")

    def test_positive_anonymous_namespace_counter(self):
        self.assertOnly(self.scan_source("namespace {\nint counter;\n}\nnamespace Own {\nint Next() { return 1; }\n}\n"),
                        2, "ZS2", "int counter")

    def test_positive_time_now(self):
        self.assertOnly(self.scan_source("namespace Own {\nvoid Step() {\n    const auto now = Engine::Time::Now();\n}\n}\n"),
                        3, "ZS4", "Time::Now")

    def test_positive_imgui_getio(self):
        self.assertOnly(self.scan_source("namespace Own {\nvoid Draw() {\n    auto& io = ImGui::GetIO();\n}\n}\n"),
                        3, "ZS3", "ImGui::GetIO")

    def test_positive_sdl_getmousestate(self):
        self.assertOnly(self.scan_source("namespace Own {\nvoid Read() {\n    float x = 0; float y = 0;\n    SDL_GetMouseState(&x, &y);\n}\n}\n"),
                        4, "ZS3", "SDL_GetMouseState")

    def test_positive_this_lambda_into_member_is_a_candidate(self):
        report = self.scan_source("namespace Own {\nstruct Holder {\n    void Bind() {\n        callback_ = [this](int v) { Use(v); };\n    }\n"
                                  "    void Use(int);\n    Callback callback_;\n};\n}\n")
        self.assertEqual(report["findings"], [], report["findings"])
        self.assertEqual(len(report["candidates"]), 1, report["candidates"])
        self.assertEqual((report["candidates"][0]["line"], report["candidates"][0]["rule"]), (4, "ZS5"))
        self.assertIn("lambda capturing this", report["candidates"][0]["text"])

    def test_positive_unordered_map(self):
        self.assertOnly(self.scan_source("namespace Own {\nvoid Index() {\n    std::unordered_map<int, int> index;\n}\n}\n"),
                        3, "ZS7", "unordered_map")

    def test_positive_std_hash(self):
        self.assertOnly(self.scan_source("namespace Own {\nstd::size_t Key(int v) {\n    return std::hash<int>{}(v);\n}\n}\n"),
                        3, "ZS7", "std::hash")


class NegativeCases(OwnerTree):
    """The six synthetic non-violations of the card plus std::map: no finding, no candidate."""

    def test_negative_static_constexpr(self):
        self.assertClean(self.scan_source("namespace Own {\nstruct S {\n    static constexpr int limit = 4;\n};\n"
                                          "int F() {\n    static constexpr int table[] = {1, 2};\n    return table[0];\n}\n}\n"))

    def test_negative_const_global(self):
        self.assertClean(self.scan_source("namespace Own {\nconst int limit = 3;\nconstexpr double scale = 0.5;\n"
                                          "inline constexpr int tick = 16;\nconst std::string_view name = \"x\";\n}\n"
                                          "namespace {\nconst int hidden = 1;\n}\n"))

    def test_negative_function_declarations(self):
        self.assertClean(self.scan_source("namespace Own {\nint F(int);\nint G(const Foo& x) noexcept;\nvoid H();\n"
                                          "[[nodiscard]] static int Count(const S& s);\nstruct S {\n    static void Reset();\n"
                                          "    static int Of(const S& s) noexcept;\n};\n"
                                          "template <typename T>\nT Make(T value);\nint K(int a, double b = 1.5) { return a; }\n}\n"))

    def test_negative_imgui_in_comment(self):
        self.assertClean(self.scan_source("namespace Own {\n// the editor calls ImGui::GetIO() here, not us\n"
                                          "/* SDL_GetMouseState(nullptr, nullptr);\n   ImGui::Begin(\"x\"); */\nvoid Step();\n}\n"))

    def test_negative_chrono_in_string(self):
        self.assertClean(self.scan_source("namespace Own {\nconst char* Banner() {\n"
                                          "    return \"#include <chrono> std::chrono::steady_clock::now() rand()\";\n}\n}\n"))

    def test_negative_int64_ns_time(self):
        self.assertClean(self.scan_source("#include <cstdint>\nnamespace Own {\nstruct FixedStep {\n    std::int64_t stepNs = 16'000'000;\n};\n"
                                          "std::int64_t Advance(std::int64_t nowNs, const FixedStep& step) {\n    return nowNs + step.stepNs;\n}\n}\n"))

    def test_negative_std_map(self):
        self.assertClean(self.scan_source("#include <map>\nnamespace Own {\nvoid Index() {\n    std::map<int, int> ordered;\n    ordered[1] = 2;\n}\n}\n"))


class RuleZS1Includes(OwnerTree):
    BANNED = ("chrono", "random", "thread", "atomic", "mutex", "condition_variable", "future", "ctime",
              "windows.h", "unistd.h", "pthread.h", "sys/time.h", "sys/socket.h", "SDL3/SDL.h", "SDL.h", "SDL_timer.h",
              "fstream", "filesystem", "iostream", "cstdio", "stdio.h")

    def test_zs1_banned_headers_angle_form(self):
        for header in self.BANNED:
            with self.subTest(header=header):
                self.assertOnly(self.scan_source("#include <" + header + ">\n"), 1, "ZS1", "<" + header + ">")

    def test_zs1_monotonic_clock_either_form(self):
        self.assertOnly(self.scan_source('#include "engine/time/MonotonicClock.hpp"\n'), 1, "ZS1", "engine/time/MonotonicClock.hpp")
        self.assertOnly(self.scan_source("#include <engine/time/MonotonicClock.hpp>\n"), 1, "ZS1", "engine/time/MonotonicClock.hpp")

    def test_zs1_allowed_headers_pass(self):
        self.assertClean(self.scan_source("#include <cstdint>\n#include <vector>\n#include <string>\n#include <sstream>\n#include <format>\n"
                                          "#include <algorithm>\n#include <span>\n#include <optional>\n#include <cmath>\n#include <map>\n"
                                          '#include "engine/math/scalar/Angle.hpp"\n'))

    def test_zs1_include_spelled_with_spaces(self):
        self.assertOnly(self.scan_source("  #  include   <chrono>\n"), 1, "ZS1", "<chrono>")

    def test_zs1_include_inside_comment_or_string_is_not_an_include(self):
        self.assertClean(self.scan_source("// #include <chrono>\n/* #include <random> */\nconst char* S() { return \"#include <thread>\"; }\n"))


class RuleZS2State(OwnerTree):
    def test_zs2_namespace_scope_variables(self):
        cases = {
            "inline int inlined;": "inline int inlined",
            "thread_local int tl;": "thread_local int tl",
            "extern int imported;": "extern int imported",
            "std::vector<int> values{1, 2};": "std::vector<int> values",
            "Foo direct(1);": "Foo direct(1)",
            "Foo quoted(\"name\");": "Foo quoted(",
            "auto f = [](int x) { return x; };": "auto f = [](int x)",
            "const char* mutablePointer;": "const char* mutablePointer",
            "std::span<const int> view;": "std::span<const int> view",
            "int (*hook)(int);": "int (*hook)(int)",
            "constinit int early = 1;": "constinit int early = 1",
            "std::int64_t lastNs;": "std::int64_t lastNs",
        }
        for body, text in cases.items():
            with self.subTest(body=body):
                self.assertOnly(self.scan_source("namespace Own {\n" + body + "\n}\n"), 2, "ZS2", text)

    def test_zs2_struct_instance_after_definition(self):
        self.assertOnly(self.scan_source("namespace {\nstruct Table {\n    int rows;\n} table;\n}\n"), 4, "ZS2", "table")
        self.assertOnly(self.scan_source("namespace Own {\nstruct Table t;\n}\n"), 2, "ZS2", "struct Table t")

    def test_zs2_class_static_members(self):
        for body, text in {"struct S { static int count; };": "static int count",
                           "struct S { static inline int count = 0; };": "static inline int count",
                           "class C { static std::vector<int> cache_; };": "static std::vector<int> cache_"}.items():
            with self.subTest(body=body):
                self.assertOnly(self.scan_source("namespace Own {\n" + body + "\n}\n"), 2, "ZS2", text)

    def test_zs2_static_in_function_forms(self):
        for body in ("static int cache;", "static int cache = 0;", "static std::vector<int> cache{};", "static int cache(0);",
                     "thread_local int cache;", "static const char* names[] = {\"a\"};"):
            with self.subTest(body=body):
                self.assertOnly(self.scan_source("namespace Own {\nint F() {\n    " + body + "\n    return 0;\n}\n}\n"), 3, "ZS2")

    def test_zs2_allowed_forms_pass(self):
        for body in ("static constexpr int limit = 4;",
                     "static const int limit = 4;",
                     "static const std::string_view name = \"x\";",
                     "constexpr std::array<int, 2> table{1, 2};",
                     "const auto f = [](int x) { return x; };",
                     "extern const int imported;",
                     "inline constexpr int tick = 16;",
                     "using Clock = int;",
                     "namespace fs = Own::Files;",
                     "using namespace Own::Detail;",
                     "typedef int Id;",
                     "struct Forward;",
                     "enum class Kind : int { A, B };",
                     "struct [[nodiscard]] Result { int code; };",
                     "struct Derived : Base { int x; };",
                     "struct alignas(16) Aligned { float v[4]; };",
                     "template <typename T> struct Box { T value; };",
                     "template <typename T> int Size(const T& box);",
                     "static int Helper(int);",
                     "static void Reset();",
                     "int F(int a, int b = 2);",
                     "void G(std::string_view text = \"x\");",
                     "bool operator==(const Foo& a, const Foo& b);",
                     "int H() { int local = 1; return local; }",
                     "int I() { static_assert(sizeof(int) == 4); return static_cast<int>(1.0); }",
                     "static_assert(true);",
                     "extern template class Box<int>;"):
            with self.subTest(body=body):
                self.assertClean(self.scan_source("namespace Own {\n" + body + "\n}\n"))

    def test_zs2_extern_c_block_is_namespace_scope(self):
        self.assertOnly(self.scan_source('extern "C" {\nint cGlobal;\nint c_function(int);\n}\n'), 2, "ZS2", "int cGlobal")

    def test_zs2_file_scope_without_namespace(self):
        self.assertOnly(self.scan_source("#include <cstdint>\nint fileScope = 0;\nint F();\n"), 2, "ZS2", "int fileScope")

    def test_zs2_lone_identifier_argument_reads_as_function(self):
        # Limit, stated in the card: `Foo f(x);` is a function declaration when x names a type; the
        # scanner reads every lone identifier that way.
        self.assertClean(self.scan_source("namespace Own {\nFoo f(x);\n}\n"))


class RuleZS3ZS4ZS6ZS7Names(OwnerTree):
    def body(self, statement):
        return "namespace Own {\nvoid F() {\n    " + statement + "\n}\n}\n"

    def test_zs3_third_party_globals(self):
        for statement, text in {"ImGui::Begin(nullptr);": "ImGui::Begin", "auto k = SDL_GetKeyboardState(nullptr);": "SDL_GetKeyboardState",
                                "SDL_GetError();": "SDL_GetError", "TTF_Init();": "TTF_Init", "Mix_Volume(1, 2);": "Mix_Volume"}.items():
            with self.subTest(statement=statement):
                self.assertOnly(self.scan_source(self.body(statement)), 3, "ZS3", text)

    def test_zs4_clock_aliases(self):
        for statement, text in {"auto t = Engine::Time::NowNs();": "Time::NowNs", "auto t = Clock::now();": "Clock::now",
                                "auto t = std::chrono::steady_clock::now();": "steady_clock",
                                "auto t = std::chrono::system_clock::now();": "system_clock",
                                "auto t = high_resolution_clock::now();": "high_resolution_clock",
                                "auto t = SDL_GetTicks();": "SDL_GetTicks", "auto t = SDL_GetTicksNS();": "SDL_GetTicksNS",
                                "auto t = SDL_GetPerformanceCounter();": "SDL_GetPerformanceCounter",
                                "QueryPerformanceCounter(&li);": "QueryPerformanceCounter",
                                "clock_gettime(CLOCK_MONOTONIC, &ts);": "clock_gettime", "gettimeofday(&tv, nullptr);": "gettimeofday"}.items():
            with self.subTest(statement=statement):
                self.assertOnly(self.scan_source(self.body(statement)), 3, "ZS4", text)

    def test_zs4_sdl_clock_is_not_also_zs3(self):
        report = self.scan_source(self.body("auto t = SDL_GetTicks();"))
        self.assertEqual([f["rule"] for f in report["findings"]], ["ZS4"])

    def test_zs6_randomness(self):
        for statement, text in {"int r = rand();": "rand", "srand(1);": "srand", "int r = std::rand();": "std::rand",
                                "std::mt19937 gen(1);": "mt19937", "std::mt19937_64 gen(1);": "mt19937_64",
                                "std::uniform_int_distribution<int> d(0, 1);": "std::uniform_int_distribution"}.items():
            with self.subTest(statement=statement):
                self.assertOnly(self.scan_source(self.body(statement)), 3, "ZS6", text)

    def test_zs6_similar_names_pass(self):
        self.assertClean(self.scan_source(self.body("int r = my_rand(); int b = brand(); auto v = Random::Next(); operand(1);")))

    def test_zs7_hashing(self):
        for statement, text in {"std::unordered_set<int> s;": "unordered_set", "auto h = std::hash<std::string>{}(\"x\");": "std::hash"}.items():
            with self.subTest(statement=statement):
                self.assertOnly(self.scan_source(self.body(statement)), 3, "ZS7", text)


class RuleZS5Candidates(OwnerTree):
    def test_zs5_std_function_member_and_raw_pointer_member(self):
        report = self.scan_source("namespace Own {\nstruct Holder {\n    std::function<void(int)> callback_;\n    Impl* impl_ = nullptr;\n"
                                  "    const Arena& arena_;\n    void Set(std::function<void(int)> cb);\n    int plain_;\n};\n}\n")
        self.assertEqual(report["findings"], [])
        self.assertEqual([(c["line"], c["rule"]) for c in report["candidates"]], [(3, "ZS5"), (4, "ZS5"), (5, "ZS5")])

    def test_zs5_lambda_pushed_into_member_container(self):
        report = self.scan_source("namespace Own {\nstruct Holder {\n    void Add() { handlers_.push_back([=](int v) { Use(v); }); }\n};\n}\n")
        self.assertEqual([(c["line"], c["rule"]) for c in report["candidates"]], [(3, "ZS5")])

    def test_zs5_candidates_do_not_fail(self):
        source = self.write(OWNER + "/src/Case.cpp", "namespace Own {\nstruct Holder {\n    Impl* impl_;\n};\n}\n")
        with redirect_stdout(io.StringIO()) as out:
            status = zone_scan.main(["--root", str(self.root), "--list", str(self.listing([source]))])
        self.assertEqual(status, 0)
        self.assertIn("ZS5? raw pointer", out.getvalue())
        self.assertIn("0 finding(s), 1 candidate(s)", out.getvalue())


class Masking(OwnerTree):
    def test_block_comment_and_raw_string_are_not_code(self):
        self.assertClean(self.scan_source("namespace Own {\n/* static int cache; thread_local int t;\n   #include <pthread.h> rand(); */\n"
                                          'const char* Text() { return R"x(std::thread static int y; ImGui::GetIO() ")x"; }\n}\n'))

    def test_code_after_block_comment_or_raw_string_is_scanned(self):
        self.assertOnly(self.scan_source('namespace Own {\nconst char* T() { return R"x(text)x"; } /* note */ int leaked;\n}\n'),
                        2, "ZS2", "int leaked")

    def test_digit_separator_does_not_open_a_character_literal(self):
        self.assertOnly(self.scan_source("namespace Own {\nint F() { constexpr int k = 1'000; static int cache = k; return cache; }\n}\n"),
                        2, "ZS2", "static int cache")

    def test_escaped_quote_inside_string(self):
        self.assertClean(self.scan_source('namespace Own {\nconst char* T() { return "say \\"rand()\\" ImGui::X"; }\n}\n'))

    def test_character_literal_with_brace_does_not_break_scopes(self):
        self.assertOnly(self.scan_source("namespace Own {\nchar Open() { return '{'; }\nint afterwards;\n}\n"), 3, "ZS2", "int afterwards")

    def test_preprocessor_lines_do_not_count_as_namespace_statements(self):
        self.assertClean(self.scan_source("#pragma once\n#define OWN_VALUE 1\n#if OWN_VALUE\nnamespace Own {\nint F();\n}\n#endif\n"))

    def test_preprocessor_macro_body_is_still_scanned_for_names(self):
        self.assertOnly(self.scan_source("#define OWN_NOW() SDL_GetTicks()\n"), 1, "ZS4", "SDL_GetTicks")


class Expansion(OwnerTree):
    def test_header_reached_through_include_root_is_scanned_with_chain(self):
        self.write(OWNER + "/include/Own/A.hpp", HEADER + '#include "Own/B.hpp"\n')
        self.write(OWNER + "/include/Own/B.hpp", HEADER + "#include <chrono>\nnamespace Own { int leaked; }\n")
        report = self.scan([self.write(OWNER + "/src/A.cpp", '#include "Own/A.hpp"\n')])
        self.assertEqual(report["files_listed"], 1)
        self.assertEqual(report["files_scanned"], 3)
        self.assertEqual([e["file"] for e in report["expanded"]], [OWNER + "/include/Own/A.hpp", OWNER + "/include/Own/B.hpp"])
        self.assertEqual(report["expanded"][1]["via"], [OWNER + "/src/A.cpp", OWNER + "/include/Own/A.hpp", OWNER + "/include/Own/B.hpp"])
        self.assertEqual(self.findings(report), [(OWNER + "/include/Own/B.hpp", 2, "ZS1", "<chrono>"),
                                                 (OWNER + "/include/Own/B.hpp", 3, "ZS2", "int leaked")])

    def test_header_next_to_the_including_file_wins(self):
        self.write(OWNER + "/src/Local.hpp", HEADER + "#include <random>\n")
        self.write(OWNER + "/include/Local.hpp", HEADER + "#include <thread>\n")
        report = self.scan([self.write(OWNER + "/src/A.cpp", '#include "Local.hpp"\n')])
        self.assertEqual(self.findings(report), [(OWNER + "/src/Local.hpp", 2, "ZS1", "<random>")])

    def test_header_outside_owner_is_not_expanded(self):
        self.write("engine/time/Other.hpp", HEADER + "#include <chrono>\nint leaked;\n")
        report = self.scan([self.write(OWNER + "/src/A.cpp", '#include "engine/time/Other.hpp"\n')], include_roots=[])
        self.assertClean(report)
        self.assertEqual(report["files_scanned"], 1)
        self.assertEqual(report["unresolved_includes"], [{"file": OWNER + "/src/A.cpp", "line": 1, "include": "engine/time/Other.hpp"}])

    def test_angle_include_of_an_owner_header_is_not_expanded(self):
        self.write(OWNER + "/include/Own/A.hpp", HEADER + "#include <chrono>\n")
        report = self.scan([self.write(OWNER + "/src/A.cpp", "#include <Own/A.hpp>\n")])
        self.assertClean(report)
        self.assertEqual(report["files_scanned"], 1)

    def test_include_cycle_scans_each_file_once(self):
        self.write(OWNER + "/include/Own/A.hpp", HEADER + '#include "Own/B.hpp"\nint a;\n')
        self.write(OWNER + "/include/Own/B.hpp", HEADER + '#include "Own/A.hpp"\nint b;\n')
        report = self.scan([self.write(OWNER + "/src/A.cpp", '#include "Own/A.hpp"\n#include "Own/B.hpp"\n'),
                            self.write(OWNER + "/src/B.cpp", '#include "Own/B.hpp"\n')])
        self.assertEqual(report["files_scanned"], 4)
        self.assertEqual(len(self.findings(report)), 2)

    def test_resolution_never_leaves_owner_root(self):
        # The list contract already keeps include_roots under owner_root; the resolver guards it again.
        self.write("engine/time/Other.hpp", HEADER)
        self.assertIsNone(zone_scan.resolve_quoted_include(self.root, OWNER + "/src/A.cpp", "time/Other.hpp", ["engine"], OWNER))
        self.assertIsNone(zone_scan.resolve_quoted_include(self.root, "engine/time/A.cpp", "Other.hpp", [], OWNER))

    def test_dot_dot_in_include_is_not_followed(self):
        self.write(OWNER + "/include/Own/A.hpp", HEADER + "#include <chrono>\n")
        report = self.scan([self.write(OWNER + "/src/A.cpp", '#include "../include/Own/A.hpp"\n')])
        self.assertClean(report)
        self.assertEqual(len(report["unresolved_includes"]), 1)


class ListContract(OwnerTree):
    def setUp(self):
        super().setUp()
        self.source = self.write(OWNER + "/src/A.cpp", "namespace Own {\nint F();\n}\n")
        self.other = self.write(OWNER + "/src/B.cpp", "namespace Own {\nint G();\n}\n")

    def assertRefused(self, part, files=None, **overrides):
        with self.assertRaises(zone_scan.ContractError) as caught:
            self.scan([self.source] if files is None else files, **overrides)
        self.assertIn(part, str(caught.exception))

    def test_unknown_version_is_refused_with_the_contract_message(self):
        self.assertRefused("unsupported list version 2", version=2)
        self.assertRefused("(supported: 1); refusing to scan", version=2)
        self.assertRefused("unsupported list version", version="1")
        self.assertRefused("unsupported list version", version=True)

    def test_unknown_format_is_refused(self):
        self.assertRefused("unknown format 'gyo.other'", format="gyo.other")

    def test_unknown_or_missing_field_is_refused(self):
        self.assertRefused("unknown field(s) ['extra']", extra=1)
        data = json.loads(self.listing([self.source]).read_text(encoding="utf-8"))
        del data["repo_commit"]
        (self.root / "list.json").write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaises(zone_scan.ContractError) as caught:
            zone_scan.scan(self.root, self.root / "list.json", RULES)
        self.assertIn("missing field(s) ['repo_commit']", str(caught.exception))

    def test_path_rules_are_enforced(self):
        self.assertRefused("must be sorted", files=[self.other, self.source])
        self.assertRefused("must not repeat", files=[self.source, self.source])
        self.assertRefused("backslash", files=[OWNER + "\\src\\A.cpp"])
        self.assertRefused("'..'", files=[OWNER + "/src/../src/A.cpp"])
        self.assertRefused("repo-relative", files=["/" + self.source])
        self.assertRefused("is not under owner_root", files=["elsewhere/A.cpp"])
        self.assertRefused("does not exist", files=[OWNER + "/src/Missing.cpp"])
        self.assertRefused("headers are expanded, not listed", files=[self.write(OWNER + "/include/Own/A.hpp", HEADER)])
        self.assertRefused("include_roots item 'other/include' is not under owner_root", include_roots=["other/include"])
        self.assertRefused("is not a directory", include_roots=[OWNER + "/missing"])
        self.assertRefused("repo_commit 'xyz' must be 7 to 40 hexadecimal digits", repo_commit="xyz")

    def test_empty_file_list_is_clean(self):
        report = self.scan([])
        self.assertEqual((report["files_listed"], report["files_scanned"], report["findings"]), (0, 0, []))

    def test_main_exit_codes(self):
        clean = str(self.listing([self.source]))
        with redirect_stdout(io.StringIO()):
            self.assertEqual(zone_scan.main(["--root", str(self.root), "--list", clean]), 0)
        dirty = self.write(OWNER + "/src/C.cpp", "#include <chrono>\n")
        dirty_list = str(self.listing([dirty]))
        report_path = self.root / "out" / "report.json"
        with redirect_stdout(io.StringIO()) as out:
            self.assertEqual(zone_scan.main(["--root", str(self.root), "--list", dirty_list, "--report", str(report_path)]), 1)
        self.assertEqual(out.getvalue().splitlines()[0], OWNER + "/src/C.cpp:1: ZS1 <chrono>")
        self.assertIn("1 file(s) listed, 1 scanned (0 expanded headers), 1 lines, 1 finding(s), 0 candidate(s)", out.getvalue())
        written = json.loads(report_path.read_text(encoding="utf-8"))
        self.assertEqual(written["per_rule"]["ZS1"], 1)
        self.assertEqual(sum(written["per_rule"][r] for r in zone_scan.RULES if r != "ZS5"), len(written["findings"]))
        with redirect_stdout(io.StringIO()):
            self.assertEqual(zone_scan.main(["--root", str(self.root), "--list", dirty_list, "--no-fail"]), 0)
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()) as err:
            self.assertEqual(zone_scan.main(["--root", str(self.root), "--list", str(self.listing([self.source], version=2))]), 2)
        self.assertIn("unsupported list version 2", err.getvalue())

    def test_rules_file_version_is_checked(self):
        bad = self.root / "rules.json"
        bad.write_text(json.dumps({"version": 2}), encoding="utf-8")
        with self.assertRaises(zone_scan.ContractError) as caught:
            zone_scan.load_rules(bad)
        self.assertIn("unsupported rules version 2", str(caught.exception))


if __name__ == "__main__":
    unittest.main()
