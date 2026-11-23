# Test fonts of the text Module

Only the text Module's tests use these files; no product ships them
(declaration `docs/architecture/target/v2/text-declaration.zh-Hant.md` 8.2).

| File | Origin | Licence | SHA-256 |
|---|---|---|---|
| `GyoTextTestLatin.ttf` | `generate_test_fonts.py` (TrueType outlines) | made for these tests | `6ab869e99387941c507a0122e387f33b4c1021d60bbb4e543683965a686f93ae` |
| `GyoTextTestKana.otf` | `generate_test_fonts.py` (CFF outlines) | made for these tests | `bb58afd3b0981f754c2b6a639c4f39d2d02fbd2045b21c555f40e486b3b0d0e6` |
| `GyoTextTestPair.ttc` | `generate_test_fonts.py` (face 0 Latin, face 1 Kana) | made for these tests | `f2b150c9cfbbc41c16adf18108b0eedb03a75b29afa2efbabebf08899ee7fc62` |
| `BIZUDPGothic-Subset.ttf` | subset of BIZ UDPGothic 1.051 (`BIZUDPGothic-Regular.ttf`, SHA-256 `22f41ce68f1ce62477ca4ec1e2c0c400cc545ddff31d713e8edf87808614aeb2`, the copy Object_Connect ships) | SIL Open Font License 1.1, `BIZUDPGothic-OFL.txt` (no Reserved Font Name) | `1dda50efc3d183f688473c93efdf052c2781147dbeacc8579bda6b6b81e765fd` |

Regenerating (fontTools 4.68.0, a development-time tool; the build and L1 do
not need it). Both commands give the same bytes on every run.

```
python3 generate_test_fonts.py .
pyftsubset BIZUDPGothic-Regular.ttf --text='0123456789/ 残りステージページ操作方法：ゲームパッドの「A」ボタンで決定します。、ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz,.!?がっこう漢字' --no-hinting --name-IDs='*' --name-languages='*' --notdef-outline --output-file=BIZUDPGothic-Subset.ttf
```

The subset keeps the font's names and its layout tables for the kept
characters; its TrueType instructions are dropped (the text Module hints with
FreeType's light auto-hinter, which does not read them).
