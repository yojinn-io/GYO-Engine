"""Runs a text Module test executable with environment variables that would change
HarfBuzz's shaping and FreeType's hinting if either library read them (Design
Invariants TX-7 and TX-8 of docs/architecture/target/v2/text.zh-Hant.md). The one
place these variables are written: CTest's gyo_text_environment and the mutant
tx7-harfbuzz-reads-environment both run through this script.

Usage: python3 run_with_environment.py <executable> [arguments...]
"""
import os
import subprocess
import sys

VARIABLES = {
    # HarfBuzz: the fallback shaper has no OpenType features (no kerning, no ligatures).
    "HB_SHAPER_LIST": "fallback",
    "HB_OPTIONS": "uniscribe-bug-compatible",
    # FreeType: other hinting choices for every module the text Module uses.
    "FREETYPE_PROPERTIES": "truetype:interpreter-version=35 cff:no-stem-darkening=0 cff:hinting-engine=freetype "
                           "autofitter:no-stem-darkening=0 autofitter:fallback-script=none "
                           "autofitter:default-script=none autofitter:increase-x-height=40",
}


def main():
    environment = dict(os.environ, **VARIABLES)
    return subprocess.run(sys.argv[1:], env=environment).returncode


if __name__ == "__main__":
    sys.exit(main())
