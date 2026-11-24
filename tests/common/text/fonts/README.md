# Test fonts of the text Module

Only the text Module's tests use these files; no product ships them
(declaration `docs/architecture/target/v2/text-declaration.zh-Hant.md` 8.2).

| File | Origin | Licence | SHA-256 |
|---|---|---|---|
| `GyoTextTestLatin.ttf` | `generate_test_fonts.py` (TrueType outlines) | made for these tests | `f9448edf0fd2f992df1e3251996def22b0b54159153e123fd15c2411f87d8aca` |
| `GyoTextTestKana.otf` | `generate_test_fonts.py` (CFF outlines) | made for these tests | `bb58afd3b0981f754c2b6a639c4f39d2d02fbd2045b21c555f40e486b3b0d0e6` |
| `GyoTextTestPair.ttc` | `generate_test_fonts.py` (face 0 Latin, face 1 Kana) | made for these tests | `eb69f694fb10c3961600d133ea172e566d7c9a6ecd9e04893b0a7cadb6b33c7a` |
| `BIZUDPGothic-Subset.ttf` | subset of BIZ UDPGothic 1.051 (`BIZUDPGothic-Regular.ttf`, SHA-256 `22f41ce68f1ce62477ca4ec1e2c0c400cc545ddff31d713e8edf87808614aeb2`) | SIL Open Font License 1.1, `BIZUDPGothic-OFL.txt` (no Reserved Font Name) | `1dda50efc3d183f688473c93efdf052c2781147dbeacc8579bda6b6b81e765fd` |

Regenerating (fontTools 4.68.0, a development-time tool; the build and L1 do
not need it). Both commands give the same bytes on every run.

```
python3 generate_test_fonts.py .
pyftsubset BIZUDPGothic-Regular.ttf --text='0123456789/ 残りステージページ操作方法：ゲームパッドの「A」ボタンで決定します。、ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz,.!?がっこう漢字' --no-hinting --name-IDs='*' --name-languages='*' --notdef-outline --output-file=BIZUDPGothic-Subset.ttf
```

The subset keeps the font's names and its layout tables for the kept
characters; its TrueType instructions are dropped (the text Module hints with
FreeType's light auto-hinter, which does not read them).
