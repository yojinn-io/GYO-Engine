"""Self-test of kernel_symbols.py on synthetic nm output (both formats), so the guard's reading is checked on
every platform, Windows included, even where nm is not run (AGENTS 14.2: the scan is shown to hit a known case)."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kernel_symbols as guard  # noqa: E402

SYSV = """
Symbols from libgyo_kernel.a[Registry.cpp.o]:

Name                  Value           Class        Type         Size             Line  Section

GYO::Kernel::Detail::TypeKeyOf<GYO::Kernel::KernelSettings>::typeKeyAnchor|0000000000000000|   u  |            OBJECT|0000000000000001|     |.bss._ZN3GYO6Kernel6Detail9TypeKeyOfINS0_14KernelSettingsEE13typeKeyAnchorE
Engine::Base::Detail::assertionHandler|0000000000000000|   V  |            OBJECT|0000000000000008|     |.data.rel.local._ZN6Engine4Base6Detail16assertionHandlerE
Engine::Base::Detail::reportingAssertion|0000000000000000|   u  |               TLS|0000000000000001|     |.tbss._ZN6Engine4Base6Detail18reportingAssertionE
vtable for GYO::Kernel::Plugin|0000000000000000|   V  |            OBJECT|0000000000000038|     |.data.rel.ro._ZTVN3GYO6Kernel6PluginE
GYO::Kernel::ToString(GYO::Kernel::StartupCode)|0000000000000000|   T  |              FUNC|0000000000000120|     |.text
operator new(unsigned long)|                |   U  |            NOTYPE|                |     |*UND*
"""

DARWIN = """
000000000002bf28 (__DATA,__data) weak external GYO::Kernel::Detail::TypeKeyOf<GYO::Kernel::KernelSettings>::typeKeyAnchor
000000000002bf20 (__DATA,__data) weak external Engine::Base::Detail::assertionHandler
000000000002bf08 (__DATA,__thread_vars) weak external Engine::Base::Detail::reportingAssertion
000000000031583b (__DATA,__thread_bss) non-external Engine::Base::Detail::reportingAssertion
0000000000001000 (__DATA,__const) weak external vtable for GYO::Kernel::Plugin
0000000000000000 (__TEXT,__text) external GYO::Kernel::ToString(GYO::Kernel::StartupCode)
                 (undefined) external operator new(unsigned long)
"""


def sysv_with(*lines):
    return SYSV + "\n".join(lines) + "\n"


def darwin_with(*lines):
    return DARWIN + "\n".join(lines) + "\n"


class Classification(unittest.TestCase):
    def assert_clean(self, symbols):
        present, foreign, findings = guard.classify(symbols)
        self.assertEqual(findings, [])
        return present, foreign

    def assert_found(self, symbols, name):
        _, _, findings = guard.classify(symbols)
        self.assertIn(name, findings)

    def test_sysv_baseline_is_clean_with_the_anchor_present(self):
        present, foreign = self.assert_clean(guard.parse_sysv(SYSV))
        self.assertEqual(present, {"TypeKey anchor"})
        self.assertEqual(foreign, ["Engine::Base::Detail::assertionHandler",
                                   "Engine::Base::Detail::reportingAssertion"])

    def test_darwin_baseline_is_clean_with_the_anchor_present(self):
        present, foreign = self.assert_clean(guard.parse_darwin(DARWIN))
        self.assertEqual(present, {"TypeKey anchor"})
        self.assertEqual(sorted(foreign), ["Engine::Base::Detail::assertionHandler",
                                           "Engine::Base::Detail::reportingAssertion"])

    def test_sysv_read_only_and_text_and_undefined_are_not_writable(self):
        symbols = guard.parse_sysv(SYSV)
        self.assertNotIn("vtable for GYO::Kernel::Plugin", symbols)
        self.assertNotIn("GYO::Kernel::ToString(GYO::Kernel::StartupCode)", symbols)
        self.assertNotIn("operator new(unsigned long)", symbols)
        # .data.rel.local is writable (only .data.rel.ro is read-only after relocation).
        self.assertIn("Engine::Base::Detail::assertionHandler", symbols)

    def test_darwin_const_text_and_undefined_are_not_writable(self):
        symbols = guard.parse_darwin(DARWIN)
        self.assertNotIn("vtable for GYO::Kernel::Plugin", symbols)
        self.assertNotIn("GYO::Kernel::ToString(GYO::Kernel::StartupCode)", symbols)
        self.assertNotIn("operator new(unsigned long)", symbols)

    def test_inline_variable_without_static_is_found(self):
        # The shape K4 cannot see: "inline int g = 0;" in namespace GYO::Kernel.
        self.assert_found(guard.parse_sysv(sysv_with(
            "GYO::Kernel::g|0000000000000000|   u  |            OBJECT|0000000000000004|     |.bss._ZN3GYO6Kernel1gE")),
            "GYO::Kernel::g")
        self.assert_found(guard.parse_darwin(darwin_with(
            "0000000000002000 (__DATA,__common) weak external GYO::Kernel::g")), "GYO::Kernel::g")

    def test_anonymous_namespace_extern_and_thread_local_variables_are_found(self):
        cases = [
            ("(anonymous namespace)::counter", ".data._ZN12_GLOBAL__N_17counterE", "__data"),
            ("GYO::Kernel::exported", ".data", "__data"),
            ("GYO::Kernel::perThread", ".tbss._ZN3GYO6Kernel9perThreadE", "__thread_bss"),
            ("guard variable for GYO::Kernel::Cache()::table", ".bss._ZGVZN3GYO6Kernel5CacheEvE5table", "__bss"),
        ]
        for name, elf_section, darwin_section in cases:
            with self.subTest(name=name):
                self.assert_found(guard.parse_sysv(sysv_with(
                    f"{name}|0000000000000000|   d  |            OBJECT|0000000000000004|     |{elf_section}")), name)
                self.assert_found(guard.parse_darwin(darwin_with(
                    f"0000000000002000 (__DATA,{darwin_section}) non-external {name}")), name)

    def test_tlv_companion_is_named_after_its_variable(self):
        symbols = guard.parse_darwin(darwin_with(
            "0000000000003000 (__DATA,__thread_bss) non-external GYO::Kernel::perThread$tlv$init"))
        self.assertIn("GYO::Kernel::perThread", symbols)

    def test_gcc_personality_reference_is_toolchain_data(self):
        present, foreign = self.assert_clean(guard.parse_sysv(sysv_with(
            "DW.ref.__gxx_personality_v0|0000000000000000|   V  |            OBJECT|0000000000000008|     |"
            ".data.rel.local.DW.ref.__gxx_personality_v0")))
        self.assertIn("DW.ref.__gxx_personality_v0", foreign)
        self.assert_found({"DW.ref.other"}, "DW.ref.other")

    def test_assembler_section_labels_are_toolchain_data(self):
        _, foreign = self.assert_clean(guard.parse_darwin(darwin_with(
            "0000000000000040 (__DATA,__data) non-external ltmp5",
            "0000000000000048 (__DATA,__bss) non-external ltmp7")))
        self.assertIn("ltmp5", foreign)
        self.assertIn("ltmp7", foreign)
        for name in ("ltmp", "ltmpx", "xltmp5", "GYO::Kernel::ltmp5"):
            with self.subTest(name=name):
                self.assert_found({name}, name)

    def test_required_name_missing_fails_the_run(self):
        # An empty symbol table (nm read nothing writable) must not pass when the anchor is required.
        present, _, findings = guard.classify(set())
        self.assertEqual(findings, [])
        self.assertNotIn("TypeKey anchor", present)

    def test_tl_current_is_allowed_and_reported_present(self):
        present, _ = self.assert_clean(guard.parse_sysv(sysv_with(
            "GYO::Kernel::Detail::tlCurrent|0000000000000000|   b  |               TLS|0000000000000008|     |.tbss")))
        self.assertEqual(present, {"TypeKey anchor", "tlCurrent"})

    def test_allowlist_is_exactly_two_kernel_names_and_two_foreign_names(self):
        self.assertEqual(set(guard.KERNEL_ALLOWED), {"TypeKey anchor", "tlCurrent"})
        self.assertEqual(set(guard.FOREIGN), {"GYO::Base assertion handler", "GYO::Base assertion reentry flag"})
        self.assertEqual(set(guard.TOOLCHAIN), {"GCC personality reference", "assembler section label"})

    def test_lookalikes_of_allowed_names_are_found(self):
        for name in ("GYO::Kernel::Detail::TypeKeyOf<int>::typeKeyAnchorCopy",
                     "GYO::Kernel::Detail::OtherOf<int>::typeKeyAnchor",
                     "Other::Detail::TypeKeyOf<int>::typeKeyAnchor",
                     "GYO::Kernel::tlCurrentCopy",
                     "Engine::Base::Detail::assertionHandlerCopy",
                     "Engine::Base::Detail::logHandler"):
            with self.subTest(name=name):
                self.assert_found({name}, name)


if __name__ == "__main__":
    unittest.main()
