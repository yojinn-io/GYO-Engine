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

    def cli(self, *args, script=None):
        command = [sys.executable, "-I", "-B", str(script or HERE / "h7_scan.py"), "--root", str(self.root), *args]
        return subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

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

    def test_n05_auto_copy_of_whitelisted(self):
        body = f"class H {{\n    {FIXED} ticks_;\n    void F() {{\n        auto preview = ticks_;\n        preview.Advance(0.0);\n    }}\n}};\n"
        self.assert_clean(self.host(body))

    def test_n06_unique_ptr_of_whitelisted(self):
        self.assert_clean(self.host(f"std::unique_ptr<{FIXED}> t;\nvoid F() {{ t->Advance(1.0); }}\n"))

    def test_n07_using_declaration(self):
        self.assert_clean(self.host(f"using {FIXED};\nFixedTickRuntime t;\nvoid F() {{ t.Advance(1.0); }}\n"))

    def test_n08_comments_and_strings(self):
        body = ('// x.Tick(); #include "RuntimeLoop.hpp"\n/* loop_.Run(); */\n'
                'const char* s = "match_.Tick()";\n// #include "engine/runtime/IRuntimeClient.hpp"\n')
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


if __name__ == "__main__":
    unittest.main()
