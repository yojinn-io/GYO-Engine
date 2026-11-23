"""Generates the synthetic test fonts of the text Module's tests.

The fonts are the ground truth of the exact-value tests: every advance, every
kerning value and every substitution is written here, so a test computes its
expectation from these numbers and not from the Module under test (AGENTS
14.2; declaration docs/architecture/target/v2/text-declaration.zh-Hant.md 8.2).

Run with fontTools (a development-time tool, not needed by the build or L1):

    python3 generate_test_fonts.py <output directory>

Outputs, all with unitsPerEm 1024 (a power of two, so HarfBuzz's scaling at
16 px and 32 px is exact):
- GyoTextTestLatin.ttf  TrueType outlines: ASCII, U+00E4, two combining marks,
                        U+FFFD; GSUB 'liga' f+i, 'ccmp' U+00E4 -> a + U+0308;
                        GPOS 'kern' A V = -128; hyphen V: the hyphen's
                        advance -64, V placed +64 right and +128 up.
- GyoTextTestKana.otf   CFF outlines: the kana, kanji and CJK punctuation the
                        tests use, all 1024 wide; U+3099 combining dakuten.
- GyoTextTestPair.ttc   a collection of the two fonts above (face 0 Latin,
                        face 1 Kana).
"""

import sys
from pathlib import Path

from fontTools.agl import UV2AGL
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont
from fontTools.ttLib.ttCollection import TTCollection

UPEM = 1024
ASCENT = 820
DESCENT = -204
# 2026-01-01T00:00:00Z in seconds since 1904-01-01, so the output does not
# depend on the time it is generated.
FIXED_TIME = 3849984000

# Latin: advance per glyph name; everything else in ASCII is 512.
LATIN_ADVANCES = {"space": 256, "i": 256, "l": 256, "f": 320, "M": 768, "W": 768,
                  "f_i": 600, "adieresis": 512, "uniFFFD": 700}
MARKS = {0x0301: "acutecomb", 0x0308: "dieresiscomb"}

KANA_TEXT = (
    "あいうえおかがきくけこさしすせそたちつってとなにぬねのはばぱひふへほまみむめもやゃゆゅよょらりるれろわをん"
    "アイウエオカガキクケコサシスセソタダチッツテデトドナニヌネノハバパヒビピフブプヘホボポマミムメモヤャユュヨョラリルレロワヲンー"
    "漢字決定操作方法学校残定画面説明文"
    "、。「」（）！？：・"
)


def box(pen, x0, y0, x1, y1):
    pen.moveTo((x0, y0))
    pen.lineTo((x0, y1))
    pen.lineTo((x1, y1))
    pen.lineTo((x1, y0))
    pen.closePath()


def name_for(cp):
    return UV2AGL.get(cp, "uni%04X" % cp)


def finish(fb, family, glyph_order):
    fb.setupHorizontalHeader(ascent=ASCENT, descent=DESCENT, lineGap=0)
    fb.setupNameTable({"familyName": family, "styleName": "Regular"})
    fb.setupOS2(version=4, sTypoAscender=ASCENT, sTypoDescender=DESCENT, sTypoLineGap=0,
                usWinAscent=ASCENT, usWinDescent=-DESCENT, fsSelection=0x40 | 0x80,
                achVendID="GYO ")
    fb.setupPost()
    fb.updateHead(created=FIXED_TIME, modified=FIXED_TIME)


def build_latin():
    cmap = {0x20: "space"}
    for cp in range(0x21, 0x7F):
        cmap[cp] = name_for(cp)
    cmap[0xE4] = "adieresis"
    cmap[0xFFFD] = "uniFFFD"
    for cp, name in MARKS.items():
        cmap[cp] = name
    names = sorted(set(cmap.values()) - {"space"})
    order = [".notdef", "space"] + names + ["f_i"]

    fb = FontBuilder(UPEM, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    glyphs, metrics = {}, {}
    for name in order:
        pen = TTGlyphPen(None)
        if name in MARKS.values():
            box(pen, -300, 720, -100, 800)
            metrics[name] = (0, -300)
        elif name == "space":
            metrics[name] = (LATIN_ADVANCES["space"], 0)
        else:
            adv = 512 if name == ".notdef" else LATIN_ADVANCES.get(name, 512)
            box(pen, 32, 0, adv - 32, 700)
            metrics[name] = (adv, 32)
        glyphs[name] = pen.glyph()
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(metrics)
    finish(fb, "GYO Text Test Latin", order)
    marks = " ".join(MARKS.values())
    fb.addOpenTypeFeatures(f"""
languagesystem DFLT dflt;
languagesystem latn dflt;
@MARKS = [{marks}];
table GDEF {{ GlyphClassDef , , @MARKS, ; }} GDEF;
feature ccmp {{ sub adieresis by a dieresiscomb; }} ccmp;
feature liga {{ sub f i by f_i; }} liga;
feature kern {{ pos A V -128; pos hyphen <0 0 -64 0> V <64 128 0 0>; }} kern;
""")
    return fb.font


def build_kana():
    cmap = {0x3000: "uni3000", 0x3099: "uni3099"}
    for ch in KANA_TEXT:
        cmap[ord(ch)] = "uni%04X" % ord(ch)
    order = [".notdef"] + sorted(set(cmap.values()))
    fb = FontBuilder(UPEM, isTTF=False)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    charstrings, metrics = {}, {}
    for name in order:
        if name == "uni3099":
            pen = T2CharStringPen(0, None)
            box(pen, -300, 720, -100, 800)
            metrics[name] = (0, -300)
        elif name == "uni3000":
            pen = T2CharStringPen(UPEM, None)
            metrics[name] = (UPEM, 0)
        else:
            pen = T2CharStringPen(UPEM, None)
            box(pen, 64, -64, UPEM - 64, 760)
            metrics[name] = (UPEM, 64)
        charstrings[name] = pen.getCharString()
    fb.setupCFF("GYOTextTestKana-Regular", {"FullName": "GYO Text Test Kana"}, charstrings, {})
    fb.setupHorizontalMetrics(metrics)
    finish(fb, "GYO Text Test Kana", order)
    fb.addOpenTypeFeatures("""
languagesystem DFLT dflt;
languagesystem kana dflt;
languagesystem hani dflt;
@MARKS = [uni3099];
table GDEF { GlyphClassDef , , @MARKS, ; } GDEF;
""")
    return fb.font


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    latin = build_latin()
    kana = build_kana()
    latin.save(out / "GyoTextTestLatin.ttf")
    kana.save(out / "GyoTextTestKana.otf")
    collection = TTCollection()
    collection.fonts = [TTFont(out / name, recalcTimestamp=False)
                        for name in ("GyoTextTestLatin.ttf", "GyoTextTestKana.otf")]
    collection.save(out / "GyoTextTestPair.ttc")


if __name__ == "__main__":
    main()
