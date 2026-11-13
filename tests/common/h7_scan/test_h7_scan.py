"""Self-test of the H-7 host scan on synthetic owner trees only (no product is assumed; AGENTS.md
section 7). The judgement and the case numbering (positives 1-13, negatives N1-N11) are in
docs/architecture/plans/kernel/h7-scanner-declaration.zh-Hant.md section 5."""
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import h7_scan  # noqa: E402

ZS = h7_scan.zone_scan_functions()
RULES = h7_scan.load_rules(h7_scan.DEFAULT_RULES)

FIXED = "Engine::Runtime::FixedTickRuntime"
KERNEL = "GYO::Kernel::Runtime"


class Tree:
    """A synthetic repository with owners under apps/<name>."""

    def __init__(self):
        self.root = Path(tempfile.mkdtemp(prefix="h7_scan_"))

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return relative

    def owner(self, name, sources, headers=None, extra=None):
        """Writes apps/<name>/src/<file> for each source, headers under apps/<name>/include, and the list."""
        owner_root = f"apps/{name}"
        (self.root / owner_root / "include").mkdir(parents=True, exist_ok=True)
        for header, text in (headers or {}).items():
            self.write(f"{owner_root}/include/{header}", text)
        files = sorted(self.write(f"{owner_root}/src/{source}", text) for source, text in sources.items())
        listing = {"format": "gyo.h7-hosts", "version": 1, "generated_by": "manual", "repo_commit": "0123456",
                   "owner_root": owner_root, "include_roots": [f"{owner_root}/include"], "files": files}
        listing.update(extra or {})
        return self.write(f"lists/{name}.json", json.dumps(listing))

    def scan(self, list_relative):
        return h7_scan.scan(self.root, self.root / list_relative, RULES, ZS)

    def cli(self, *args, script=None, timeout=None):
        command = [sys.executable, "-I", "-B", str(script or HERE / "h7_scan.py"), "--root", str(self.root), *args]
        return subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)

    def close(self):
        shutil.rmtree(self.root, ignore_errors=True)


class Base(unittest.TestCase):
    def setUp(self):
        self.tree = Tree()

    def tearDown(self):
        self.tree.close()

    def host(self, body, headers=None):
        """One host file host.cpp whose code is `body`; returns the report."""
        return self.tree.scan(self.tree.owner("fake", {"host.cpp": body}, headers))

    def findings(self, report):
        return [(f["file"].split("/")[-1], f["line"], f["rule"]) for f in report["findings"]]

    def assert_only(self, report, line, rule, file="host.cpp"):
        self.assertEqual(self.findings(report), [(file, line, rule)], report["findings"])

    def assert_clean(self, report):
        self.assertEqual(report["findings"], [])


# ------------------------------------------------------------------------------------------------
# Positives (each reported only by the named rule, at the right line)

class Positives(Base):
    def test_p01_h7a_include_iruntime_client(self):
        self.assert_only(self.host('// host\n#include "engine/runtime/IRuntimeClient.hpp"\nint main() {}\n'), 2, "H7-a")

    def test_p02_h7a_include_runtime_loop_angle(self):
        self.assert_only(self.host("#include <vector>\n#include <engine/runtime/RuntimeLoop.hpp>\n"), 2, "H7-a")

    def test_p03_h7a_through_owner_header(self):
        report = self.host('#include "fake/App.hpp"\n',
                           headers={"fake/App.hpp": '#pragma once\n\n#include "engine/runtime/IRuntimeClient.hpp"\n'})
        self.assert_only(report, 3, "H7-a", file="App.hpp")
        self.assertEqual(report["findings"][0]["via"], ["apps/fake/src/host.cpp", "apps/fake/include/fake/App.hpp"])

    def test_p04_h7b_member_tick(self):
        self.assert_only(self.host("Product foo_;\nvoid F() {\n    foo_.Tick();\n}\n"), 3, "H7-b")

    def test_p05_h7b_loop_run(self):
        self.assert_only(self.host("game::Loop loop_;\nvoid F() { auto s = loop_.Run(1, 2); }\n"), 2, "H7-b")

    def test_p06_h7b_arrow_update_through_unique_ptr(self):
        self.assert_only(self.host("std::unique_ptr<Product> p;\nvoid F() { p->Update(); }\n"), 2, "H7-b")

    def test_p07_h7b_step(self):
        self.assert_only(self.host("Stepper s;\nvoid F() { s.Step(); }\n"), 2, "H7-b")

    def test_p08_h7b_product_advance(self):
        self.assert_only(self.host("game::Prediction m;\nvoid F() { m.Advance(0.5, true); }\n"), 2, "H7-b")

    def test_p09_h7b_template_arguments(self):
        self.assert_only(self.host("Product x;\nvoid F() { x.Run<int>(); }\n"), 2, "H7-b")

    def test_p10_whitelisted_type_with_other_method(self):
        self.assert_only(self.host(f"{FIXED} t;\nvoid F() {{ t.Run(); }}\n"), 2, "H7-b")

    def test_p11_unqualified_whitelisted_name_without_using(self):
        self.assert_only(self.host("FixedTickRuntime t;\nvoid F() { t.Advance(1.0); }\n"), 2, "H7-b")

    def test_p12_product_type_named_runtime(self):
        self.assert_only(self.host("game::Runtime r;\nvoid F() { r.Run(); }\n"), 2, "H7-b")

    def test_p12b_product_rewind(self):
        self.assert_only(self.host("game::Prediction p;\nvoid F() { p.Rewind(3); }\n"), 2, "H7-b")

    def test_p12c_product_replay(self):
        self.assert_only(self.host("game::Prediction p;\nvoid F() { p.Replay(); }\n"), 2, "H7-b")

    def test_p12d_product_restore(self):
        self.assert_only(self.host("game::Prediction p;\nvoid F() { p.Restore(snapshot); }\n"), 2, "H7-b")

    def test_p13_receiver_without_declaration(self):
        report = self.host("void F() {\n    Get().Run();\n    v[0].Tick();\n    unknown.Step();\n}\n")
        self.assertEqual(self.findings(report), [("host.cpp", 2, "H7-b"), ("host.cpp", 3, "H7-b"), ("host.cpp", 4, "H7-b")])
        self.assertTrue(all(f["text"].endswith("[type=?]") for f in report["findings"]), report["findings"])


# ------------------------------------------------------------------------------------------------
# Negatives (no finding)

class Negatives(Base):
    def test_n01_fixed_tick_advance(self):
        report = self.host(f"{FIXED} ticker_{{60}};\nvoid F() {{ ticker_.Advance(dt, [](auto) {{}}); }}\n")
        self.assert_clean(report)
        self.assertEqual(len(report["whitelisted"]), 1)

    def test_n02_kernel_runtime_run(self):
        self.assert_clean(self.host(f"{KERNEL} rt;\nvoid F() {{ rt.Run(); }}\n"))

    def test_n03_kernel_runtime_rewind(self):
        self.assert_clean(self.host(f"{KERNEL} rt;\nvoid F() {{ rt.Rewind(3); }}\n"))

    def test_n04_kernel_runtime_replay(self):
        self.assert_clean(self.host(f"{KERNEL} rt;\nvoid F() {{ rt.Replay(); }}\n"))

    def test_n04b_kernel_runtime_restore(self):
        self.assert_clean(self.host(f"{KERNEL} rt;\nvoid F() {{ rt.Restore(snapshot); }}\n"))

    def test_n05_auto_copy_of_whitelisted(self):
        body = f"class H {{\n    {FIXED} ticks_;\n    void F() {{\n        auto preview = ticks_;\n        preview.Advance(0.0);\n    }}\n}};\n"
        self.assert_clean(self.host(body))

    def test_n06_unique_ptr_of_whitelisted(self):
        self.assert_clean(self.host(f"std::unique_ptr<{FIXED}> t;\nvoid F() {{ t->Advance(1.0); }}\n"))

    def test_n07_using_declaration(self):
        self.assert_clean(self.host(f"using {FIXED};\nFixedTickRuntime t;\nvoid F() {{ t.Advance(1.0); }}\n"))

    def test_n08_comments_and_strings(self):
        body = ('// x.Tick(); #include "RuntimeLoop.hpp"\n/* loop_.Run(); */\n'
                'const char* s = "match_.Tick()";\n// #include "engine/runtime/IRuntimeClient.hpp"\n'
                '/*\n#include "engine/runtime/RuntimeLoop.hpp"\n*/\n')
        self.assert_clean(self.host(body))

    def test_n09_this_free_and_definitions(self):
        body = "void Host::Tick() {\n    this->Run();\n    Tick(x);\n    Run();\n}\n"
        report = self.host(body)
        self.assert_clean(report)
        self.assertEqual([c["line"] for c in report["this_calls"]], [2])

    def test_n10_method_name_is_a_whole_word(self):
        self.assert_clean(self.host("Product app;\nvoid F() { app.RunLiveFrame(); app.UpdateTitle(); }\n"))

    def test_n11_other_engine_runtime_header(self):
        self.assert_clean(self.host('#include "engine/runtime/FixedTickRuntime.hpp"\n#include "fake/RuntimeLoopAdapter.hpp"\n'))


# ------------------------------------------------------------------------------------------------
# Supplementary cases of the type approximation

class TypeResolution(Base):
    def test_all_declarations_must_match(self):
        body = f"class H {{ {FIXED} clock; }};\nvoid F() {{ Product clock; clock.Advance(1.0); }}\n"
        report = self.host(body)
        self.assert_only(report, 2, "H7-b")
        self.assertIn(FIXED, report["findings"][0]["text"])

    def test_declaration_in_owner_header(self):
        report = self.host('#include "fake/Host.hpp"\nvoid H::F() { ticker_.Advance(1.0); match_.Tick(1); }\n',
                           headers={"fake/Host.hpp": f"class H {{\n    {FIXED} ticker_;\n    game::Match match_;\n}};\n"})
        self.assert_only(report, 2, "H7-b")
        self.assertIn("match_.Tick", report["findings"][0]["text"])

    def test_auto_second_hop_is_reported(self):
        body = f"{FIXED} a;\nvoid F() {{ auto b = a; auto c = b; c.Advance(1.0); }}\n"
        self.assert_only(self.host(body), 2, "H7-b")

    def test_auto_from_call_is_reported(self):
        self.assert_only(self.host("void F() { auto p = Make(); p.Run(); }\n"), 1, "H7-b")

    def test_using_namespace_for_a_suffix(self):
        self.assert_clean(self.host(f"using namespace Engine;\nRuntime::FixedTickRuntime t;\nvoid F() {{ t.Advance(1.0); }}\n"))

    def test_partial_qualification_without_using_is_reported(self):
        self.assert_only(self.host("Runtime::FixedTickRuntime t;\nvoid F() { t.Advance(1.0); }\n"), 2, "H7-b")

    def test_namespace_alias_is_reported(self):
        body = "namespace ER = Engine::Runtime;\nER::FixedTickRuntime t;\nvoid F() { t.Advance(1.0); }\n"
        self.assert_only(self.host(body), 3, "H7-b")

    def test_parameter_and_reference(self):
        self.assert_clean(self.host(f"void F(const {FIXED}& t, int n) {{ t.Advance(n); }}\n"))

    def test_chain_uses_last_identifier(self):
        report = self.host(f"struct S {{ {FIXED} clock; }};\nS session_;\nvoid F() {{ session_.clock.Advance(1.0); }}\n")
        self.assert_clean(report)


# ------------------------------------------------------------------------------------------------
# Contract, command line, environment

class Contract(Base):
    def test_unknown_version_rejected(self):
        listing = self.tree.owner("fake", {"host.cpp": "int x;\n"}, extra={"version": 2})
        result = self.tree.cli("--list", str(self.tree.root / listing))
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("unsupported list version", result.stderr)

    def test_unknown_format_rejected(self):
        listing = self.tree.owner("fake", {"host.cpp": "int x;\n"}, extra={"format": "gyo.zone-c-sources"})
        self.assertEqual(self.tree.cli("--list", str(self.tree.root / listing)).returncode, 2)

    def test_owner_whitelist_field_rejected(self):
        listing = self.tree.owner("fake", {"host.cpp": "int x;\n"}, extra={"whitelist": [{"type": "Product", "methods": ["Run"]}]})
        result = self.tree.cli("--list", str(self.tree.root / listing))
        self.assertEqual(result.returncode, 2)
        self.assertIn("unknown field", result.stderr)

    def test_missing_list_file_rejected(self):
        self.assertEqual(self.tree.cli("--list", str(self.tree.root / "lists/none.json")).returncode, 2)

    def test_missing_listed_source_rejected(self):
        listing = self.tree.owner("fake", {"host.cpp": "int x;\n"})
        (self.tree.root / "apps/fake/src/host.cpp").unlink()
        result = self.tree.cli("--list", str(self.tree.root / listing))
        self.assertEqual(result.returncode, 2)
        self.assertIn("does not exist", result.stderr)

    def test_exit_status_and_no_fail(self):
        listing = str(self.tree.root / self.tree.owner("fake", {"host.cpp": "Product p;\nvoid F() { p.Tick(); }\n"}))
        self.assertEqual(self.tree.cli("--list", listing).returncode, 1)
        result = self.tree.cli("--list", listing, "--no-fail")
        self.assertEqual(result.returncode, 0)
        self.assertIn("apps/fake/src/host.cpp:2: H7-b p.Tick [type=Product]", result.stdout)
        self.assertIn("h7_scan: 1 file(s) listed, 1 scanned (0 expanded headers), 1 finding(s), 0 whitelisted", result.stdout)

    def test_clean_scan_exits_zero(self):
        listing = str(self.tree.root / self.tree.owner("fake", {"host.cpp": "int main() { return 0; }\n"}))
        self.assertEqual(self.tree.cli("--list", listing).returncode, 0)

    def test_expect_finding_present_passes(self):
        listing = str(self.tree.root / self.tree.owner("fake", {"host.cpp": "Product p;\nvoid F() { p.Tick(); }\n"}))
        result = self.tree.cli("--list", listing, "--no-fail", "--expect-finding", r"apps/fake/src/host.cpp|H7-b|p\.Tick")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_expect_finding_missing_fails(self):
        listing = str(self.tree.root / self.tree.owner("fake", {"host.cpp": "int main() { return 0; }\n"}))
        result = self.tree.cli("--list", listing, "--no-fail", "--expect-finding", r"apps/fake/src/host.cpp|H7-b|p\.Tick")
        self.assertEqual(result.returncode, 1)
        self.assertIn("expected finding absent", result.stderr)

    def test_nothing_to_scan(self):
        result = self.tree.cli()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "h7_scan: nothing to scan")

    def test_non_utf8_source_is_an_error(self):
        listing = self.tree.owner("fake", {"host.cpp": "int x;\n"})
        (self.tree.root / "apps/fake/src/host.cpp").write_bytes(b"int \xff x;\n")
        result = self.tree.cli("--list", str(self.tree.root / listing))
        self.assertEqual(result.returncode, 2)
        self.assertIn("UTF-8", result.stderr)

    def test_report_json(self):
        listing = str(self.tree.root / self.tree.owner("fake", {"host.cpp": f"{FIXED} t;\nProduct p;\nvoid F() {{ t.Advance(1); p.Run(); }}\n"}))
        out = self.tree.root / "out/report.json"
        self.assertEqual(self.tree.cli("--list", listing, "--report", str(out), "--no-fail").returncode, 0)
        data = json.loads(out.read_text(encoding="utf-8"))
        self.assertEqual(data["per_rule"], {"H7-a": 0, "H7-b": 1})
        self.assertEqual(len(data["reports"][0]["whitelisted"]), 1)
        self.assertEqual(data["reports"][0]["list"]["format"], "gyo.h7-hosts")


class SharedScanner(Base):
    def copy_scanner(self, zone_scan_text):
        """h7_scan.py and its rules in <tmp>/common/h7_scan with <tmp>/common/zone_scan holding the given text (None: absent)."""
        common = self.tree.root / "copy/common"
        (common / "h7_scan").mkdir(parents=True)
        shutil.copy(HERE / "h7_scan.py", common / "h7_scan/h7_scan.py")
        shutil.copy(HERE / "h7_scan_rules.json", common / "h7_scan/h7_scan_rules.json")
        if zone_scan_text is not None:
            (common / "zone_scan").mkdir()
            (common / "zone_scan/zone_scan.py").write_text(zone_scan_text, encoding="utf-8")
        return common / "h7_scan/h7_scan.py"

    def test_missing_zone_scan_module_fails_loudly(self):
        script = self.copy_scanner(None)
        listing = str(self.tree.root / self.tree.owner("fake", {"host.cpp": "int x;\n"}))
        result = self.tree.cli("--list", listing, script=script)
        self.assertEqual(result.returncode, 2)
        self.assertIn("zone_scan.py is missing", result.stderr)

    def test_zone_scan_without_the_three_functions_fails_loudly(self):
        script = self.copy_scanner("def blank_comments_and_literals(text):\n    return text\n")
        listing = str(self.tree.root / self.tree.owner("fake", {"host.cpp": "int x;\n"}))
        result = self.tree.cli("--list", listing, script=script)
        self.assertEqual(result.returncode, 2)
        self.assertIn("lacks ['resolve_quoted_include', 'validate_path']", result.stderr)


class Removability(Base):
    def test_removing_one_owner_changes_nothing_else(self):
        a = self.tree.owner("alpha", {"main.cpp": "Product p;\nvoid F() { p.Tick(); }\n"})
        b = self.tree.owner("beta", {"main.cpp": f"{FIXED} t;\ngame::Loop l;\nvoid F() {{ t.Advance(1); l.Run(); }}\n",
                                     "window.cpp": '#include "beta/W.hpp"\n'},
                            headers={"beta/W.hpp": '#include "engine/runtime/RuntimeLoop.hpp"\n'})
        common = {p.name: p.read_bytes() for p in HERE.iterdir() if p.is_file() and p.suffix in (".py", ".json")}
        before_b = self.tree.scan(b)
        self.assertEqual(len(self.tree.scan(a)["findings"]), 1)
        self.assertEqual(before_b["per_rule"], {"H7-a": 1, "H7-b": 1})
        shutil.rmtree(self.tree.root / "apps/alpha")
        (self.tree.root / a).unlink()
        self.assertEqual(self.tree.scan(b), before_b)
        self.assertEqual({p.name: p.read_bytes() for p in HERE.iterdir() if p.is_file() and p.suffix in (".py", ".json")}, common)
        self.assertEqual(self.tree.cli().stdout.strip(), "h7_scan: nothing to scan")
        self.assertEqual(self.tree.cli("--list", str(self.tree.root / b), "--no-fail").returncode, 0)


# ------------------------------------------------------------------------------------------------
# Review 2026-11-13 (Fable): fail-closed hardening, missed forms, contract (dev_log, review section)

class Hardening(Base):
    def test_multi_declarator_receiver_is_reported(self):
        self.assert_only(self.host(f"struct A {{ {FIXED} clock; }};\nvoid F() {{ Product other, clock; clock.Advance(1.0); }}\n"), 2, "H7-b")

    def test_range_for_receiver_is_reported(self):
        body = f"struct A {{ {FIXED} m; }};\nvoid F(std::vector<Product>& ms) {{ for (auto& m : ms) m.Advance(1.0); }}\n"
        self.assert_only(self.host(body), 2, "H7-b")

    def test_structured_binding_receiver_is_reported(self):
        body = f"struct A {{ {FIXED} b; }};\nvoid F() {{ auto [a, b] = Pair(); b.Advance(1.0); }}\n"
        self.assert_only(self.host(body), 2, "H7-b")

    def test_lambda_capture_receiver_is_reported(self):
        body = f"struct A {{ {FIXED} c; }};\nvoid F() {{ auto f = [c = Make()]() mutable {{ c.Advance(1.0); }}; }}\n"
        self.assert_only(self.host(body), 2, "H7-b")

    def test_parameter_in_owner_header_is_not_evidence(self):
        report = self.host('#include "fake/Api.hpp"\nstruct Host : framework::Base { void F() { clock_.Advance(1.0); } };\n',
                           headers={"fake/Api.hpp": f"void Configure({FIXED}& clock_);\n"})
        self.assert_only(report, 2, "H7-b")

    def test_using_namespace_in_owner_header_is_not_evidence(self):
        report = self.host('#include "fake/H.hpp"\nnamespace game { struct FixedTickRuntime {}; FixedTickRuntime t; void F() { t.Advance(1.0); } }\n',
                           headers={"fake/H.hpp": "inline void G() { using namespace Engine::Runtime; }\n"})
        self.assert_only(report, 2, "H7-b")

    def test_auto_copy_of_member_chain_is_reported(self):
        body = f"struct S {{ {FIXED} ticks_; }};\nS s;\nvoid F() {{ auto p = s.ticks_; p.Advance(1.0); }}\n"
        self.assert_only(self.host(body), 3, "H7-b")

    def test_auto_hop_with_mixed_declarations_is_reported(self):
        body = f"struct A {{ {FIXED} b; }};\nvoid G() {{ auto b = Make(); auto c = b; c.Advance(1.0); }}\n"
        self.assert_only(self.host(body), 2, "H7-b")

    def test_call_result_receiver_never_whitelisted(self):
        # `sizeof(FIXED))` reads as a parameter-like declaration of the name `)`; a call result must still be reported.
        self.assert_only(self.host(f"void F() {{ Use(sizeof({FIXED})); Get().Advance(1.0); }}\n"), 1, "H7-b")

    def test_auto_alias_in_untyped_position_is_reported(self):
        body = f"struct A {{ {FIXED} b; }};\nvoid G() {{ Product a, b; auto c = b; c.Advance(1.0); }}\n"
        self.assert_only(self.host(body), 2, "H7-b")

    def test_braced_temporary_receiver_is_reported(self):
        report = self.host("void F() {\n    Product{}.Run();\n    game::Loop{cfg}.Run(client);\n}\n")
        self.assertEqual(self.findings(report), [("host.cpp", 2, "H7-b"), ("host.cpp", 3, "H7-b")])

    def test_qualified_member_call_is_reported(self):
        report = self.host(f"{FIXED} t;\nProduct p;\nvoid F() {{\n    t.Base::Advance(1.0);\n    p->Product::Update();\n}}\n")
        self.assertEqual(self.findings(report), [("host.cpp", 4, "H7-b"), ("host.cpp", 5, "H7-b")])

    def test_template_keyword_call_is_reported(self):
        self.assert_only(self.host("Product x;\nvoid F() { x.template Run<int>(); }\n"), 2, "H7-b")

    def test_h7b_nested_and_parenthesised_template_arguments(self):
        report = self.host("Product x;\nvoid F() {\n    x.Run<std::vector<int>>();\n    x.Run<decltype(a)>();\n"
                           "    x.Run<A<B<C>>>();\n    x.Run<std::function<void()>>();\n}\n")
        self.assertEqual([f[1] for f in self.findings(report)], [3, 4, 5, 6])

    def test_h7b_newline_around_operator(self):
        report = self.host("Product foo_;\nvoid F() {\n    foo_\n        .Tick();\n    foo_.\n        Run();\n}\n")
        self.assertEqual([f[1] for f in self.findings(report)], [4, 5])

    def test_h7b_space_before_parenthesis(self):
        report = self.host("Product foo_;\nvoid F() { foo_.Tick (); foo_.Run <int> (); }\n")
        self.assertEqual(len(report["findings"]), 2)

    def test_known_gap_same_name_whitelisted_member_elsewhere(self):
        # Limit L4 (dev_log): the receiver's real declaration is outside the closure (a base class of another
        # owner) and the closure has a same-name whitelisted member. This passes; the test pins the known gap
        # so that a change in either direction is seen.
        report = self.host('#include "fake/Other.hpp"\nstruct Host : other::Base { void F() { clock.Advance(1.0); } };\n',
                           headers={"fake/Other.hpp": f"struct Other {{ {FIXED} clock; }};\n"})
        self.assert_clean(report)
        self.assertEqual(len(report["whitelisted"]), 1)


class IncludeForms(Base):
    def test_h7a_indented_include(self):
        report = self.host('  #include "engine/runtime/RuntimeLoop.hpp"\n#  include <engine/runtime/IRuntimeClient.hpp>\n')
        self.assertEqual(self.findings(report), [("host.cpp", 1, "H7-a"), ("host.cpp", 2, "H7-a")])

    def test_h7a_case_insensitive(self):
        self.assert_only(self.host('#include "engine/runtime/RUNTIMELOOP.HPP"\n'), 1, "H7-a")

    def test_h7a_backslash_path(self):
        self.assert_only(self.host('#include "engine\\runtime\\RuntimeLoop.hpp"\n'), 1, "H7-a")

    def test_h7a_comments_between_tokens(self):
        report = self.host('/**/#include "engine/runtime/RuntimeLoop.hpp"\n#include /* x */ "engine/runtime/IRuntimeClient.hpp"\n'
                           '/* a\n b */ #include <engine/runtime/RuntimeLoop.hpp>\n')
        self.assertEqual([f[1] for f in self.findings(report)], [1, 2, 4])

    def test_h7a_after_bom(self):
        tree = self.tree
        listing = tree.owner("fake", {"host.cpp": "int x;\n"})
        (tree.root / "apps/fake/src/host.cpp").write_bytes(b'\xef\xbb\xbf#include "engine/runtime/RuntimeLoop.hpp"\n')
        self.assert_only(tree.scan(listing), 1, "H7-a")

    def test_h7a_transitive_owner_header(self):
        report = self.host('#include "fake/A.hpp"\n', headers={"fake/A.hpp": '#include "fake/B.hpp"\n',
                                                              "fake/B.hpp": '\n#include "engine/runtime/RuntimeLoop.hpp"\n'})
        self.assert_only(report, 2, "H7-a", file="B.hpp")
        self.assertEqual(len(report["findings"][0]["via"]), 3)

    def test_h7a_angle_owner_header_expanded(self):
        report = self.host('#include <fake/App.hpp>\n', headers={"fake/App.hpp": '#include "engine/runtime/IRuntimeClient.hpp"\n'})
        self.assert_only(report, 1, "H7-a", file="App.hpp")

    def test_include_cycle_terminates(self):
        listing = self.tree.owner("fake", {"host.cpp": '#include "fake/A.hpp"\n'},
                                  headers={"fake/A.hpp": '#include "fake/B.hpp"\n', "fake/B.hpp": '#include "fake/A.hpp"\n'})
        try:
            result = self.tree.cli("--list", str(self.tree.root / listing), timeout=30)
        except subprocess.TimeoutExpired:
            self.fail("the scan of an include cycle did not terminate")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("3 scanned (2 expanded headers)", result.stdout)

    def test_h7b_not_scanned_in_expanded_headers(self):
        report = self.host('#include "fake/H.hpp"\n', headers={"fake/H.hpp": "Product p;\ninline void G() { p.Tick(); }\n"})
        self.assert_clean(report)

    def test_listed_header_is_scanned_for_h7b(self):
        tree = self.tree
        tree.write("apps/fake/include/fake/Host.hpp", "Product p;\ninline void G() { p.Tick(); }\n")
        listing = tree.owner("fake", {"host.cpp": "int x;\n"})
        data = json.loads((tree.root / listing).read_text())
        data["files"] = sorted(data["files"] + ["apps/fake/include/fake/Host.hpp"])
        (tree.root / listing).write_text(json.dumps(data))
        self.assert_only(tree.scan(listing), 2, "H7-b", file="Host.hpp")


class Peeling(Base):
    def test_shared_ptr_of_whitelisted(self):
        self.assert_clean(self.host(f"std::shared_ptr<{FIXED}> t;\nvoid F() {{ t->Advance(1.0); }}\n"))

    def test_optional_of_whitelisted(self):
        self.assert_clean(self.host(f"std::optional<{FIXED}> t;\nvoid F() {{ t->Advance(1.0); }}\n"))

    def test_reference_wrapper_of_whitelisted(self):
        self.assert_clean(self.host(f"std::reference_wrapper<{FIXED}> t;\nvoid F() {{ t.get(); t.Advance(1.0); }}\n"))

    def test_unique_ptr_of_const_whitelisted(self):
        self.assert_clean(self.host(f"std::unique_ptr<const {FIXED}> t;\nvoid F() {{ t->Advance(1.0); }}\n"))

    def test_unlisted_template_is_not_peeled(self):
        self.assert_only(self.host(f"Wrapper<{FIXED}> w;\nvoid F() {{ w.Advance(1.0); }}\n"), 2, "H7-b")

    def test_pointer_inside_wrapper_is_reported(self):
        self.assert_only(self.host(f"std::optional<{FIXED}*> t;\nvoid F() {{ t->Advance(1.0); }}\n"), 2, "H7-b")

    def test_return_statement_is_not_a_declaration(self):
        body = f"{FIXED} ticker_;\n{FIXED}& Get() {{ return ticker_; }}\nvoid F() {{ ticker_.Advance(1.0); }}\n"
        self.assert_clean(self.host(body))

    def test_last_parameter_and_east_const(self):
        self.assert_clean(self.host(f"void F(int n, {FIXED} const& t) {{ t.Advance(n); }}\n"))

    def test_array_pointer_and_constructor_forms(self):
        self.assert_clean(self.host(f"{FIXED} ts[2];\n{FIXED}* p;\n{FIXED} c(60);\n"
                                    "void F() { ts->Advance(1.0); p->Advance(1.0); c.Advance(1.0); }\n"))

    def test_glued_const_is_not_peeled(self):
        self.assert_only(self.host("constEngine::Runtime::FixedTickRuntime t;\nvoid F() { t.Advance(1.0); }\n"), 2, "H7-b")

    def test_using_namespace_of_other_namespace_is_reported(self):
        self.assert_only(self.host("using namespace std;\nFixedTickRuntime t;\nvoid F() { t.Advance(1.0); }\n"), 3, "H7-b")

    def test_using_declaration_of_other_namespace_is_reported(self):
        self.assert_only(self.host("using game::FixedTickRuntime;\nFixedTickRuntime t;\nvoid F() { t.Advance(1.0); }\n"), 3, "H7-b")

    def test_suffix_with_other_qualifier_is_reported(self):
        self.assert_only(self.host("using namespace GYO;\ngame::Runtime r;\nvoid F() { r.Run(); }\n"), 3, "H7-b")

    def test_partial_qualification_with_using_declaration_is_reported(self):
        body = f"using {FIXED};\nRuntime::FixedTickRuntime t;\nvoid F() {{ t.Advance(1.0); }}\n"
        self.assert_only(self.host(body), 3, "H7-b")


class MoreContract(Base):
    def listing_with(self, **fields):
        return str(self.tree.root / self.tree.owner("fake", {"host.cpp": "int x;\n"}, extra=fields))

    def expect_two(self, message, *args):
        result = self.tree.cli(*args)
        self.assertEqual(result.returncode, 2, (result.stdout, result.stderr))
        self.assertIn(message, result.stderr)

    def test_missing_field_rejected(self):
        listing = self.tree.owner("fake", {"host.cpp": "int x;\n"})
        data = json.loads((self.tree.root / listing).read_text())
        del data["generated_by"]
        (self.tree.root / listing).write_text(json.dumps(data))
        self.expect_two("missing field", "--list", str(self.tree.root / listing))

    def test_blank_generated_by_rejected(self):
        self.expect_two("generated_by must be", "--list", self.listing_with(generated_by=" "))

    def test_bad_repo_commit_rejected(self):
        self.expect_two("repo_commit", "--list", self.listing_with(repo_commit="HEAD"))

    def test_include_root_outside_owner_rejected(self):
        (self.tree.root / "apps/other").mkdir(parents=True)
        self.expect_two("is not under owner_root", "--list", self.listing_with(include_roots=["apps/other"]))

    def test_missing_include_root_rejected(self):
        self.expect_two("is not a directory", "--list", self.listing_with(include_roots=["apps/fake/none"]))

    def test_repeated_include_roots_rejected(self):
        self.expect_two("without repeats", "--list", self.listing_with(include_roots=["apps/fake/include", "apps/fake/include"]))

    def test_file_outside_owner_rejected(self):
        self.tree.write("apps/other/x.cpp", "int y;\n")
        self.expect_two("is not under owner_root", "--list", self.listing_with(files=["apps/other/x.cpp"]))

    def test_unsorted_files_rejected(self):
        self.tree.write("apps/fake/src/a.cpp", "int a;\n")
        self.expect_two("must be sorted", "--list", self.listing_with(files=["apps/fake/src/host.cpp", "apps/fake/src/a.cpp"]))

    def test_repeated_files_rejected(self):
        self.expect_two("must not repeat", "--list", self.listing_with(files=["apps/fake/src/host.cpp", "apps/fake/src/host.cpp"]))

    def test_empty_files_rejected(self):
        self.expect_two("at least one", "--list", self.listing_with(files=[]))

    def test_non_string_files_rejected(self):
        self.expect_two("array of strings", "--list", self.listing_with(files=["apps/fake/src/host.cpp", 3]))
        self.expect_two("array of strings", "--list", self.listing_with(files=[["x"]]))

    def test_non_cpp_file_rejected(self):
        self.tree.write("apps/fake/src/notes.txt", "x\n")
        self.expect_two("not a C++ source or header", "--list", self.listing_with(files=["apps/fake/src/notes.txt"]))

    def test_boolean_and_float_version_rejected(self):
        self.expect_two("unsupported list version", "--list", self.listing_with(version=True))
        self.expect_two("unsupported list version", "--list", self.listing_with(version=1.0))

    def test_list_not_an_object_rejected(self):
        self.tree.write("lists/array.json", "[1, 2]")
        self.expect_two("is not a JSON object", "--list", str(self.tree.root / "lists/array.json"))

    def test_list_not_json_rejected(self):
        self.tree.write("lists/bad.json", "{ nope")
        self.expect_two("list: cannot read", "--list", str(self.tree.root / "lists/bad.json"))

    def rules_with(self, mutate):
        data = json.loads((HERE / "h7_scan_rules.json").read_text(encoding="utf-8"))
        mutate(data)
        return self.tree.write("rules.json", json.dumps(data))

    def test_rules_unknown_version_rejected(self):
        rules = self.rules_with(lambda d: d.update(version=2))
        self.expect_two("rules: unsupported version", "--list", self.listing_with(), "--rules", str(self.tree.root / rules))

    def test_rules_empty_or_non_string_methods_rejected(self):
        for methods in ([], [1]):
            rules = self.rules_with(lambda d: d["H7b"].update(methods=methods))
            self.expect_two("H7b.methods must be", "--list", self.listing_with(), "--rules", str(self.tree.root / rules))

    def test_malformed_rules_rejected(self):
        rules = self.rules_with(lambda d: d["whitelist"][0].pop("methods"))
        self.expect_two("rules: malformed rule data", "--list", self.listing_with(), "--rules", str(self.tree.root / rules))

    def test_missing_rules_file_rejected(self):
        self.expect_two("rules: cannot read", "--list", self.listing_with(), "--rules", str(self.tree.root / "none.json"))

    def test_unwritable_report_rejected(self):
        self.tree.write("blocker", "a file, not a directory\n")
        self.expect_two("cannot write the report", "--list", self.listing_with(), "--report", str(self.tree.root / "blocker/r.json"))

    def test_unreadable_header_is_an_error(self):
        listing = self.tree.owner("fake", {"host.cpp": '#include "fake/A.hpp"\n'}, headers={"fake/A.hpp": "int a;\n"})
        (self.tree.root / "apps/fake/include/fake/A.hpp").write_bytes(b"\xff\xfe\x00")
        self.expect_two("cannot read apps/fake/include/fake/A.hpp", "--list", str(self.tree.root / listing))

    def test_expect_finding_without_list_rejected(self):
        self.expect_two("needs at least one --list", "--expect-finding", "a|H7-b|x")

    def test_expect_finding_unknown_rule_rejected(self):
        self.expect_two("must be '<path>|<rule>|<regex>'", "--list", self.listing_with(), "--expect-finding", "a|H7-c|x")


class Expectations(Base):
    def run_expect(self, spec):
        listing = str(self.tree.root / self.tree.owner("fake", {"host.cpp": "Product p;\nvoid F() { p.Tick(); }\n",
                                                               "other.cpp": "int y;\n"}))
        return self.tree.cli("--list", listing, "--no-fail", "--expect-finding", spec).returncode

    def test_expect_finding_other_file_fails(self):
        self.assertEqual(self.run_expect(r"apps/fake/src/other.cpp|H7-b|p\.Tick"), 1)

    def test_expect_finding_other_rule_fails(self):
        self.assertEqual(self.run_expect(r"apps/fake/src/host.cpp|H7-a|p\.Tick"), 1)

    def test_expect_finding_other_text_fails(self):
        self.assertEqual(self.run_expect(r"apps/fake/src/host.cpp|H7-b|q\.Run"), 1)

    def test_two_lists_both_scanned(self):
        a = str(self.tree.root / self.tree.owner("alpha", {"m.cpp": "int x;\n"}))
        b = str(self.tree.root / self.tree.owner("beta", {"m.cpp": "Product p;\nvoid F() { p.Run(); }\n"}))
        result = self.tree.cli("--list", a, "--list", b)
        self.assertEqual(result.returncode, 1)
        self.assertIn("apps/beta/src/m.cpp:2: H7-b", result.stdout)


class ReportFields(Base):
    def test_unresolved_and_expanded_reported(self):
        report = self.host('#include "fake/A.hpp"\n#include "nowhere/X.hpp"\n', headers={"fake/A.hpp": "int a;\n"})
        self.assertEqual([e["file"] for e in report["expanded"]], ["apps/fake/include/fake/A.hpp"])
        self.assertEqual([u["include"] for u in report["unresolved_includes"]], ["nowhere/X.hpp"])
        self.assertEqual(report["files_scanned"], 2)

    def test_summary_counts(self):
        listing = str(self.tree.root / self.tree.owner("fake", {"host.cpp": f'#include "fake/A.hpp"\n{FIXED} t;\nvoid F() {{ t.Advance(1); }}\n'},
                                                       headers={"fake/A.hpp": "int a;\n"}))
        result = self.tree.cli("--list", listing)
        self.assertIn("h7_scan: 1 file(s) listed, 2 scanned (1 expanded headers), 0 finding(s), 1 whitelisted", result.stdout)

    def test_findings_sorted_by_file_and_line(self):
        report = self.host('Product p;\nvoid F() { p.Tick(); }\n#include "engine/runtime/RuntimeLoop.hpp"\n')
        self.assertEqual(self.findings(report), [("host.cpp", 2, "H7-b"), ("host.cpp", 3, "H7-a")])

    def test_zone_scan_import_error_fails_loudly(self):
        script = SharedScanner.copy_scanner(self, "this is not python\n")
        listing = str(self.tree.root / self.tree.owner("fake", {"host.cpp": "int x;\n"}))
        result = self.tree.cli("--list", listing, script=script)
        self.assertEqual(result.returncode, 2)
        self.assertIn("cannot import the zone-C scanner", result.stderr)


if __name__ == "__main__":
    unittest.main()
