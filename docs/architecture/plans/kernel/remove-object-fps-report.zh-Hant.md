# `-remove-object-fps` 報告：移除 object_fps、object_fps_v2、object_fps_preview，並實測 §7 的 removability

日期：2026-11-12。Owner：架構（kernel 研究線）。事前宣告：[remove-object-fps-declaration.zh-Hant.md](remove-object-fps-declaration.zh-Hant.md)。PR #185（分支 `claude/arch-remove-object-fps`，基準 master `56b8dda3`）。證據：主 checkout `build/evidence/remove-object-fps/`（git 管理外；`before-*`／`after-*` 主會話、`impl/` 實作者、`review-report.md` 審查者）。

件数の突き合わせ：一致——刪除檔 308（宣告 308／diff 308，逐檔相同）；修改檔 38（實作者 32＋commit 4 的 4＋審查後 2；全在宣告清單與審查處置內）；新增 2（宣告、dev_log）＋本報告。ctest（主會話基準環境 preset `test`）152→152、名單相同、全過；doctest 案例數 `engine_tests` 140→139、`gyo_model_tests` 24→22（第五個 commit 前 21）、其餘逐一相同；pvp 57 個測試項目的案例數前後相同。**三平台（L1 run 43188319281，commit `e9324903`）：linux-x64 ctest 138/138、windows-x64 142 過 1 skip（`os.console_close`，既有的 OS 子系統 skip、與本批無關）、macos-arm64 138/138；`engine_tests` 139、`gyo_model_tests` 22 三平台一致（讀 artifact `tests.xml`，存主 checkout `build/evidence/remove-object-fps/l1-43188319281/<platform>/`）。**

## 判定（照宣告原文）

| 判定 | 結果 | 證據 |
|---|---|---|
| 1 清單逐項 | **成立**：`--diff-filter=D` 308 檔與宣告集合 `comm -3` 為空；M 全在清單內；A＝2；R＝0 | `impl/j1-*.txt`；審查者獨立重做（review-report §判定 1） |
| 2 共通層零修改（§7 實測） | **成立**：`build/`（`acceptance/object_fps*` 以外）、`.github/`、頂層 CMake、preset、`engine/`（兩型別以外）、`framework/`、`plugins/`、`tools/`、`tests/common/`（三檔以外）、`tests/framework/`、`apps/object_fps_pvp/` 零變更；commit 1 的兩份 CSV 只少 2／1 列、無 `+` 行 | `impl/j2-*.txt`；審查者重做 |
| 3 零殘留 | **成立（commit 4 之後）**：非 docs 的 `git grep -P` 只剩 `entity_store_guard.py`／`test_entity_store_guard.py` 禁字表 3 行；docs 命中全在 dev_logs／checkup／舊計畫／已標「已移除」的句子；空目錄 0；`-DGYO_APPS=object_fps`、`object_fps_v2`、`-DGYO_TOOLS=object_fps_preview` 皆以 `GyoAppRegistry.cmake:239`／`GyoToolRegistry.cmake:79` 的「not registered」失敗 | `impl/j3-*.txt`、`impl/probe-*.log`、`after-empty-dirs.txt`；審查者重做 |
| 4 建置與測試 | **成立**：本機 preset `test`（AUTO／AUTO）configure／build 零錯誤；ctest 152/152；doctest 差額恰等於刪掉的案例（engine_tests −1；gyo_model_tests −3→第五個 commit 後 −2）；三平台 L1 全綠（138／142＋1 skip／138；engine_tests 139、gyo_model_tests 22 一致） | `after-build.log`、`after-ctest.log`、`before-/after-doctest-counts.tsv` |
| 5 pvp 不受影響 | **成立**：pvp 57 個測試項目（含 python 與 go）名單相同，doctest 案例數逐一相同；pvp 原始碼零變更 | `before-/after-doctest-counts.tsv` diff |

## §7 實測的結論

刪除兩個產品與一個工具，共通層只動了登記表的三列。產品由 `projects.csv`／`tools.csv` 列舉，`tests/<app>`、`build/acceptance/<app>`、`assets/<app>`、`tools/<tool>` 由 `GyoBuild.cmake`／`GyoAcceptance.cmake`／`GyoAppProject.cmake`／`GyoTools.cmake` 依名字慣例發現；沒有 Dummy Target、沒有 Compatibility Branch。**§7 的 removability 在這三個 owner 上成立，無 Architecture Delta。**未驗的是 §12 的 Add／Clone（要新產品當樣本）。

## 停止與處置

- 實作者（Opus／high，27 分鐘）在判定 3 停止：`tests/object_fps_pvp/` 有 6 行提到已刪產品＋2 處指向已刪的 `3d-assets.md`。宣告的前提「pvp 零引用」是架構師的掃描錯誤（`xargs grep -P`：macOS BSD grep 無 `-P`，錯誤被 `2>/dev/null` 吞掉）。使用者選 (a)：主會話以 commit `4bb944cb` 改寫這 8 行（文字與出處，不是依賴），宣告檔標「宣告錯誤的修正」。
- 審查者（Fable／high，只讀）：0 blocker／5 should／6 nit，判定 1～3 獨立重做成立。
  - S1：宣告要刪的第三個 `ModelTests` 案例，前 5 個 CHECK 直接測保留的 `PlaybackInterval::Crossed`，整個刪掉會讓 `tests/common` 失去直接測試（§14.4 的緩める面，宣告沒寫明）。處置：5 個 CHECK 以同名案例留回（第五個 commit），實際只刪 2 個案例；留回沒有緩める面，事後報告。
  - S2～S4：dev_log 追記、README dev_log 一覽與狀態、HANDOFF 狀態已補。
  - S5：`rendering_architecture.*` 第 204 行泛化後描述了不存在的 `gyo_<game>_acceptance`，改成原則句（probe 由產品在 `build/acceptance/<product>/` 註冊）。
  - N2（註解格式與來源檔名）、N3（`architecture.md:301` 的連結語意）、N4（`tools/plans/v8/assets.md:36` 懸空參照）已改；N1 `<memory>` 保留（pvp 三檔可能間接依賴，拿掉會逼 pvp 改）；N5 宣告「其餘 14 個」實為 15→12 已在宣告檔更正；N6 大小寫不同的「Object_FPS」字樣（`architecture.md`、`creating_apps.md`、`rendering_architecture.*`、`ui_toolchain.md`）交圖書管理員。

## 範囲の限界（本批後）

- 未驗 §12 Add／Clone。
- git 歷史保留約 100 MB 資產。
- `RetroFPS` include 目錄名留在 pvp（更正 2026-11-12：程式 namespace 是 `fps::pvp`，不是 `RetroFPS`）（target/naming 第 1 條，遷移批）。
- `PlaybackInterval::Crossed` 的直接測試只剩留回的 5 個 CHECK；`AnimationInstance` 的行為（Play／Advance／轉場）不再有測試，因型別已刪。
- 現行文件的「Object_FPS」字樣（指產品族或 v1）未整理。
- 刪除的是停用產品，CI 從未建置它們；判定 4 驗的是「刪了以後其他東西還成立」。

## 給後續批的輸入

- 遷移批規劃可以把 object_fps／v2／preview 從對象中移除；`IRuntimePort` 不再是 Link 的前身候選（spec L.1 已標刪除）。
- 判定用的掃描一律 `git grep -P` 或 Python、不丟 stderr、先用正例驗證（記憶 `grep-p-macos`）。
