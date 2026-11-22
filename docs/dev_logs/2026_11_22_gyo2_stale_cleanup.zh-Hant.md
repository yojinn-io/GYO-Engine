# 2026-11-22 GYO2：三件清理（include 守衛的 `..`、master 時代的突變清單、文件路徑）

批次代號：`v8-arch-gyo2-stale-cleanup`。分支 `claude/gyo2-stale-cleanup-9690e7`（從 `gyo2` 45f13757，#266 合併後）。任務卡，主會話「[GYO2][不良少年]新架構重構」開的。實作者 Opus 5.5／high。

## 起因

使用者原話（2026-11-22）：「照建議開卡處理」，回覆主會話提的三項建議：

1. entity_store、trace、logging 三個 include 守衛的 `..` 洞，照 O4 的先例補上。出處：PR #265 的設計審查（主 checkout `build/evidence/gyo2-batch4-followups/design-review/REPORT.md` C 節）、dev_log [`2026_11_22_gyo2_batch4_followups.zh-Hant.md`](2026_11_22_gyo2_batch4_followups.zh-Hant.md)。先例：O4 在 #265 補了同一個條件、Kernel 的 K5 早就有；使用者對 O4 的判斷是「既有准許清單自己的洞就補」（HANDOFF `v8-arch-gyo2-o4-dotdot-kept`）。
2. 刪掉 15 份 master 時代的突變清單。出處：dev_log [`2026_11_22_gyo2_namespace_root.zh-Hant.md`](2026_11_22_gyo2_namespace_root.zh-Hant.md)「後續」。先例：17 個失效突變選刪除（HANDOFF `v8-arch-gyo2-os-stale-mutants-removed`）。
3. 文件：`entity-store.md`、`math.md`、`error-handling.md` 的目錄路徑補上層名；刪掉 master 時代的 `docs/architecture/trace.md`、`input.md`，現行文件裡指向它們的連結改指新文件或拿掉。

## 1. 三個 include 守衛的 `..`

條件和 K5、O4 同一個：路徑以 `/` 或 `\` 切開後，有一段是 `..` 就報（trace、logging 只對它們讀的 `engine/<module>/…` 形式、模組名之後的部分查，見下「沒做」）。准許的前綴後面接 `..` 可以走到任何 header（例：`engine/base/../../../ecs/kernel/include/engine/kernel/World.hpp`），所以這是准許清單自己的洞，不是替誤用另加守衛。只補這三個准許清單，範圍不擴大。

| 守衛 | 改了什麼 | 自測 | 突變 |
|---|---|---|---|
| `tests/common/trace/trace_guard.py`（TR-1） | `ENGINE_INCLUDE` 多抓 header 的路徑；路徑有 `..` 段就報 `TR1-TRACE … (".." in the path)`，不算進正例 | `test_parent_directory_after_an_allowed_module_is_reported`（`/`、`\`、子目錄後單一個 `..` 各一例） | `tr-guard-parent-directory`（`tests/common/trace/mutations.json`） |
| `tests/common/logging/logging_guard.py`（LG-4、LG-10） | 同上，三條規則都讀 | `test_parent_directory_after_an_allowed_module_is_reported`（三條規則各一例，加子目錄後單一個 `..`） | `lg-guard-parent-directory`（`tests/common/logging/mutations.json`） |
| `tests/common/entity_store/entity_store_guard.py`（entity-store DI 9，W5） | 專案 include 的准許改成「前綴對、且沒有 `..` 段」，寫法照 K5 | `test_parent_directory_after_an_allowed_prefix_is_reported`（`/`、前綴後接 `\`、子目錄後的 `..` 各一例） | `es-guard-parent-directory`（`docs/architecture/plans/entity-store/scripts/mutations.json`） |

- 條件照 O4 的寫法寫在各守衛裡，沒有從 `kernel_guard` 匯入共用函式：#265 設計審查的選項 (b)（收成 `kernel_guard` 的一個函式）沒有被採用；共用讀法函式的抽出由 `gyo2-naming-cleanup` 卡做，這批不先動同一批檔案。
- trace 的改法保留原本的 `if module in ALLOWED:` 那一行（`..` 的判斷放在前面、`continue`），既有突變 `tr-guard-rule-off` 的 find 不必改。logging 用同一個形狀。
- 沒做（範圍外）：trace、logging 只讀 `engine/<module>/…` 形式的 include。模組位本身是 `..` 的 `engine/../…`、`engine/base\..\…`（模組名後面直接接反斜線）、不以 `engine/` 開頭的相對路徑本來就不讀，仍然通過（設計審查實測）。屬於「讀的範圍」，不是准許清單的洞；照 `v8-arch-gyo2-include-bypass-to-review` 交審查。所以這兩個守衛是關掉同效路徑中的一條，不是把路全部封死。entity_store 是完整的准許清單（不在清單就報），這三種都報 W5。

### AGENTS §14.4 的 1～5（三個守衛相同）

1. 守的性質：該 Module 只 include 它的連結准許的單位（TR-1、LG-4／LG-10、entity-store DI 9）。
2. 緩める面：沒有。舊判定報的輸入，新判定照樣報：三份自測裡原有的案例全部通過（舊判定的失敗例），原有的守衛突變全部 killed（見下）。
3. 締める面：准許的前綴或模組後面有 `..` 段的 include，以前通過，現在報。
4. 不適用（沒有緩める面）。
5. 根據：#265 設計審查 C 節、O4／K5 的先例、使用者 `v8-arch-gyo2-o4-dotdot-kept`；證據見下。

### 突變（只跑相關的，`-B`、先清 `__pycache__`）

| 清單 | 跑的突變 | 結果 |
|---|---|---|
| trace | `tr-guard-parent-directory`（新）、`tr-guard-rule-off`、`tr-guard-sees-nothing`、`tr-guard-missing-dir-passes`、`tr-guard-list-item` | 5/5 killed |
| logging | `lg-guard-parent-directory`（新）、`lg-guard-logging-off`、`lg-guard-public-off`、`lg-guard-world-off`、`lg-guard-list-item`、`lg-guard-sees-nothing`、`lg-guard-zero-not-failed`、`lg-guard-missing-dir-passes` | 8/8 killed |
| entity-store | `es-guard-parent-directory`（新）、`es-guard-unordered-index`、`es-guard-static-cache`、`es-guard-clock`、`es-guard-product-name` | 5/5 killed |

- 第一版 trace 新突變的 expect 用 verbose 那一行的 `… \.\.\. FAIL`，結果 `wrong_failure`：用了 `subTest`，那一行後面是空的，失敗在子測試的行。改成 `FAIL: test_parent_directory_after_an_allowed_module_is_reported`（和 orchestration、logging 相同的寫法）後 killed。
- 加的突變照 `tests/common/mutation/README.md` 第 4 點：規則裡獨立可錯的分支一個，三個守衛各一個，共 3 個（變更的條文 3 條 × 5 以內）。每個都寫了只有它捕到的錯（拿掉 `..` 條件）和 `guards`。

### 敵對式檢查（Fable 5.2／high，唯讀）

報告：主 checkout `build/evidence/gyo2-stale-cleanup/design-review/REPORT.md` A 節。

- 新突變 3 個、原有突變 18 個都 killed；新舊守衛各 1500 棵隨機樹比對，放寬 0 件（收緊 427／907／234 件）。
- 套 54 個改壞的實作，32 個自測沒失敗，歸 5 類。處理 2 類，只改自測、不加突變：
  - entity_store 的 `\` 例沒走到 `..` 條件：第二例 `engine/entity_store\..\…` 在前綴就被擋掉，改成 `engine/entity_store/..\…`（和 K5 自測同寫法）。改前「只切 `/`」「`"../" not in`」等存活。
  - 三個守衛都只驗「`..` 緊接在前綴後、連續三個以上」：各補一例子目錄後面單一個 `..`（`engine/trace/detail/../Span.hpp`、`engine/logging/detail/../Write.hpp`、`engine/base/detail/../../../../modules/…`）。改前「只看第一段」「至少兩個才報」存活。
  - 改後重套審查者列的 7 個存活改法，全部被自測抓到（`build/evidence/gyo2-stale-cleanup/adversarial-recheck.txt`）；三份清單的相關突變重跑 5/5、8/8、5/5 killed（`mut-*-2.txt`）。
  - 沒加突變：這兩類是自測的案例不足，照 `tests/common/mutation/README.md` 第 6 點補 case；條件本身的突變各一個（拿掉整個條件），和 K5、O4 相同。
- 不處理的 3 類：多報方向的反例（`a..b.hpp`、`..hidden/`、`.../`；現行實作不報，讀法細節，觀測到再加）、只驗引號形式（實作不依括號分支）、等價的改法。

## 2. 刪掉 15 份 master 時代的突變清單

掃描腳本與輸出：主 checkout `build/evidence/gyo2-stale-cleanup/stale_lists_scan.py`、`stale_lists_scan.txt`。做法：

- 每份清單的每個 `file` 在 gyo2 都不存在（`os.path.exists`）。
- 用 `git grep -F` 在 Markdown 以外（runner、CMake、CI、腳本、其他清單）找清單的完整路徑與 `<計畫>/scripts/<檔名>`。不用單獨的 `scripts/<檔名>`：`mutations.json`、`framework_mutations.json` 在好幾個計畫裡同名，第一次掃描因此有假命中（例：`kernel/scripts/mutations.json`）。
- 正例：先掃 `target/v2/scripts/orchestration_mutations.json`，有 4 處命中，證明掃描會命中。`git grep` 的結束碼 0／1 以外當失敗。
- 另查有沒有 runner 用 glob 一次讀所有清單：沒有（`rglob`／`glob(` 都不碰 `docs/**/mutations*.json`）。

結果：15 份的 `file` 全部不存在；Markdown 以外的引用只剩這 15 份清單之間 `description` 互相提到（`framework-rework/scripts/framework_mutations.json`、`tools/plans/v8/scripts/framework_mutations.json`、`audio_editor_mutations.json`），一起刪掉，沒有一份要留。

| 清單 | 突變數 |
|---|---|
| `docs/architecture/plans/big-map/scripts/engine_grid_mutations.json` | 19 |
| `docs/architecture/plans/event-sounds/scripts/event_sounds_mutations.json` | 33 |
| `docs/architecture/plans/framework-preview/scripts/framework_preview_mutations.json` | 25 |
| `docs/architecture/plans/framework-rework/scripts/framework_catalog_mutations.json` | 12 |
| `docs/architecture/plans/framework-rework/scripts/framework_mutations.json` | 26 |
| `docs/architecture/plans/framework-rework/scripts/framework_shell_mutations.json` | 33 |
| `docs/architecture/plans/framework-rework/scripts/platform_path_dialog_mutations-asan.json` | 1 |
| `docs/architecture/plans/framework-rework/scripts/platform_path_dialog_mutations.json` | 17 |
| `docs/architecture/plans/input-and-present/scripts/mutations.json` | 83 |
| `docs/tools/plans/v8/scripts/anim_events_mutations.json` | 13 |
| `docs/tools/plans/v8/scripts/audio_editor_mutations.json` | 13 |
| `docs/tools/plans/v8/scripts/framework_mutations.json` | 54 |
| `docs/tools/plans/v8/scripts/model_viewer_mutations.json` | 41 |
| `docs/tools/plans/v8/scripts/voxel_map_editor_mutations.json` | 114 |
| `docs/tools/plans/v8/scripts/voxel_mesh_mutations.json` | 20 |

- 卡片寫的是「framework-rework 的 framework_*」；`platform_path_dialog_mutations*.json` 兩份不是這個名字，但同樣全部指向不存在的檔案，namespace-root dev_log 的「15 份」就是把它們算進去的數，所以一起刪。
- 計畫資料夾裡的 Markdown（PLAN、README、宣告）提到這些清單的地方是當時的紀錄，不改。
- 設計審查另外重查（504 個突變的 `file` 全不存在、Markdown 以外引用 0），結論相同。

## 3. 文件

- 目錄路徑補層名（include 路徑 `engine/<單位>/<header>` 不動）：
  - `entity-store.md`：`engine/kernel` → `engine/ecs/kernel`、`engine/entity_store` → `engine/ecs/entity_store`。表 9 列與 DI 9 的 `engine/base`、`engine/math` 是 include 路徑，不動。
  - `math.md`：`engine/math` → `engine/core/math`（開頭與 Layout 的樹）。
  - `error-handling.md`：`engine/kernel` → `engine/ecs/kernel`；`engine/base/include/engine/base/{Assert,Result,Error}.hpp` → `engine/core/base/include/…`（4 處）。
- 刪掉 `docs/architecture/trace.md`、`docs/architecture/input.md`。`git grep` 找到的連結，現行文件改了這些：
  - `README.md`、`README.en.md`、`README.zh-Hant.md`：設計資料清單的 trace 連結改指 `target/v2/trace.zh-Hant.md`，標題改成 GYO2 的 trace（舊標題的「傳輸」是 master 的設計）。
  - `docs/architecture.md`：`GYO::Trace` 那一列與 TT-2 的歷史段落的連結拿掉，寫明是 master 的設計、gyo2 的 trace 在 `target/v2/trace.zh-Hant.md`。那一列描述的仍是 master 的 trace（佇列＋寫者執行緒），內容沒改（見「後續」）。
  - `docs/architecture/time.md`：「不能輪詢的呼叫者（trace.md Invariant 2）」改成 gyo2 的對應者：日誌檔案輸出端的寫者（[logging LG-6](../architecture/target/v2/logging.zh-Hant.md#lg-6)，`JsonLinesFileSink.cpp` 的 `context.Wait()`）。
  - `docs/architecture/platform-sdl.md`：指向 `input.md` 的連結改成文字，寫明是 master、沒帶回 gyo2。
  - 沒改：`docs/architecture/plans/**`、`docs/tools/plans/**`、`docs/object_fps_pvp/**`、`docs/checkup/**`、dev_log、HANDOFF 裡的提及。都是當時的紀錄。

## 驗證

- 本機（macOS arm64）：`ctest --preset test` 161/161（主 checkout `build/evidence/gyo2-stale-cleanup/ctest.txt`）。
- 突變：見第 1 節（`build/evidence/gyo2-stale-cleanup/mut-*.txt`）。
- L1 三平台：（待填）。

## 設計審查（Fable 5.2／high，唯讀）

報告：主 checkout `build/evidence/gyo2-stale-cleanup/design-review/REPORT.md` B 節。8 項中 7 項保留；要改的 3 件都是本 dev_log（trace、logging 補的範圍寫清楚、刪清單的文字出處列進後續、填驗證），已改。

要使用者在合併前核准時確認的 1 件：

- `docs/architecture/time.md` 的 `Design Invariants` 第 6 條，括號裡的例子「(trace.md Invariant 2)」改成「(the logging file sink's writer, logging LG-6)」。條文的承諾（`Wait()` 沒有計時器）不變，只換「誰依賴它」的例子，因為 trace.md 刪掉了。照 AGENTS §3，改 Design Invariants 節要使用者同意。

## 後續（範圍外，沒有做）

- 計畫資料夾以外有 6 份文件用文字指著這次刪掉的清單當守衛出處：`docs/architecture/animation-events.md:99`、`collision.md:94`、`voxel-mesh.md:245`、`docs/cross-component-rules.md:24`（CCR-1）、`docs/framework/event-sounds.md:163`、`docs/framework/preview.md:88`。描述的都是 gyo2 上已不存在的 master 程式，交 Legacy 整理（CCR-1 屬圖書管理員）。
- O4 的自測（#265，`tests/common/orchestration/test_orchestration_guard.py:74`）有和 entity_store 第二例同樣的洞（`engine/base\..` 在前綴就被擋掉，O4 改成只切 `/` 仍存活）。不在本批的三個守衛裡，沒改。
- 同一個「切段找 `..`」的運算式現在有 5 份（K5、O4、entity_store、trace、logging），各有一個 find 綁著它的突變；`gyo2-naming-cleanup` 抽共用讀法函式時要一起改寫突變。

- `docs/architecture.md` 的 `GYO::Trace` 列、`docs/architecture/platform-sdl.md` 整份描述的是 gyo2 上已不存在的程式（master 的 trace 傳輸、`engine/platform/sdl`）。`error-handling.md` 第 73 行的「today `engine/platform/sdl`」也一樣。要不要改寫或刪，屬 Legacy 文件的整理。
- `docs/architecture/plans/framework-rework/scripts/run_shell_test.py`：刪掉的 `framework_shell_mutations.json` 的伴隨腳本，指向的也是 master 的 framework；沒查是否還有人用，沒動。
- `docs/architecture/plans/big-map/scripts/engine_format_mutations.json`、`docs/architecture/plans/audio/scripts/mutations.json`、`docs/architecture/plans/time-threads-trace/scripts/mutations.json`：有一部分 `file` 不存在（不是全部），不在這次的 15 份裡。
- `target/v2/README.md` 的 dev_log 清單沒有 #265 的 `2026_11_22_gyo2_batch4_followups.zh-Hant.md`（圖書管理員的範圍；本批只加自己的一行）。
