# 2026-11-22 GYO2：第 5 批 讀檔的實作＋存檔版本 Module

批次代號：`v8-arch-gyo2-restore-impl`。分支 `claude/gyo2-restore-impl-7e6bc6`（從 `gyo2` 45f13757；中途 merge `origin/gyo2` 879350d4〔#272〕、eacab4d7〔#270〕）。任務卡 `gyo2-restore-impl`，主會話「[GYO2][不良少年]新架構重構」開的卡。宣告：[restore-entry-declaration.zh-Hant.md](../architecture/target/v2/restore-entry-declaration.zh-Hant.md) 修訂 12 第 18、19 節（使用者 2026-11-22 動工前核准「照建議，全部核准」；第 3、4 件再確認「第3件選(a)，第4件不用補查，開始」）。存檔版本 Module 由使用者 2026-11-22 併進本卡（`v8-arch-gyo2-save-version-into-restore`，原話「合併進去。」）。

## 做了什麼

### 第 1 步：對照現況，寫修訂 12（docs only）

- 11-17 核准的修訂 11 寫在第 4 批與生成世界之前。逐條對照 `origin/gyo2` 之後：讀檔放在新 World、移下界的時機、遊戲中讀檔、讀兩次相同、新 World 的紀錄從空開始，骨架與生成世界兩批已做掉並有案例（宣告 18.1 B 類）；前提變了的 11 件（C-1～C-11）照 L17 判或交使用者。
- 換模型檢查（Fable 5.2／high，唯讀）三輪：修訂 12 第一輪 blocker 2／should 7／nit 8、第二輪（含第 19 節）blocker 2／should 7／nit 9、突變清單 blocker 1／should 5／nit 8，全部採用。
- 交使用者的五件（宣告 18.2、19.4）都照建議：配發器四個突變改出貨的 `RangeAllotter`；判定 1 沿用六個既有案例、新寫四個；Engine 不發「讀檔完成」；最舊支援線；作者呼叫 `Policy::Check`。
- 事後補報：第 3 件漏了 Bevy 的 `WorldInstanceReady`、第 4 件沒有別家實作撐著；使用者再確認維持 (a)、不補查。
- 交界：狀態機卡回答 C-8（`SetInitial`＋`AfterGenerating()`，第一次套用直接進存檔的狀態）；threads／io 重審卡回答 io 需求（整檔原子寫入、整檔讀留下；存檔目錄由 os 層另一批補，使用者 Q3 選 (a)）。

### 第 2 步：讀檔的四個案例（`tests/common/orchestration/RestoreFlowTests.cpp`）

都走生成期間的路（讀檔 System 在 `WhileGenerating()` 的 set、延後的生成）：
- 重新配號（宣告案例 2）：不移下界、依存檔編號由小到大 `Spawn`、作者的對照表改寫 `ChaseTarget`；同一個 System 用 `ResMut` 放回金幣。`[or-renumber-fresh]`、`[or-renumber-order]`、`[or-renumber-ref]`、`[or-gen-resmut-gold]`。
- hook（案例 6）：每個放回的格子 OnAdd 一次；`Monster` 的 OnAdd 生成血條（Bevy #14300 的同型）。`[or-restore-onadd-once]`、`[or-restore-hook-spawn]`。
- `Added`（案例 7）：讀者在讀檔之前跑過；讀檔後成立一次。`[or-restore-added-seen]`、`[or-restore-added-once]`。
- 先生成再插入（案例 8）：`[or-restore-spawn-insert]`。
- 既有案例只改標記字樣：金幣的 `[or-restore-gold]` 兩處期待值不同，拆成 `[or-restore-gold-startup]`／`[or-restore-gold-in-game]`；讀兩次相同加 `[or-restore-twice-same]`。判定不變。

### 第 3 步：存檔版本 Module（`engine/modules/save_version`）

- `SaveVersionOptions{current, oldestSupported}`、`Policy::Current()`、`Policy::Check(版本)`（收 `oldestSupported ≤ 版本 ≤ current`，否則 `PrepareFailed`＋`reporterCode` `"save-version-too-old"`／`"save-version-too-new"`）、`SaveVersionModule`（`Configure` 檢查設定、把 `Policy` 放進 Product 的共用處；`Build` 不登記任何東西）。
- `config/modules.csv` 一列 `library_only=no`、`default=no`（第一個這樣的單位）。
- 測試 `tests/common/save_version/`：`save_version.check`、`.configure`、`.default`（V1～V8＋設定相等的一案）。
- 舊的合成範例碼 `"save-version-unsupported"`（`OrchestrationError.hpp` 的註解、`RestoreFlowTests.cpp`、`DefectTests.cpp`）改成 `"save-version-too-old"`。

### 第 4 步：文件

- [說明頁](../architecture/target/v2/system-contract-page.zh-Hant.md)新第 15 節「讀檔」與 15.1「存檔版本 Module」；第 13、14 節的讀檔改成指過去。
- 新頁 [save-version.zh-Hant.md](../architecture/target/v2/save-version.zh-Hant.md)：SV-1～SV-3、RC-SV1～RC-SV3（最終文字等合併前核准）。
- `docs/architecture/README.md` 的 Module 清單加 `save_version`；`target/v2/README.md` 加本 dev_log。

## 驗證與經過

- 本機 macOS（Intel，RelWithDebInfo，preset `core`）：`ctest -LE "gpu|network"` 172/172。
- 突變（只跑本批新的，D184）：結果見下節「突變的結果」。
- `orchestration_mutations.json` 重新寫出時，HEAD 第 273 個物件的鍵重複（#270 解衝突留下：`or-exch-round-after-failed-in` 的鍵後面接 `wg-held-not-cleared` 的鍵，一直被讀成後者；前者另有完整的一筆）被正規化。解析結果相同（Fable 第三輪確認：282 筆順序與內容不變，加本批 5 筆）。
- 證據：主 checkout `build/evidence/gyo2-restore-impl/`（git 不追蹤，這台 macOS Intel 機器）：三輪檢查報告、突變記錄。

## 突變的結果

- 本批新的 21 個：第一輪 20/20 killed（`save_version` 12、`entity_ids` 的 `re-` 3、`orchestration` 的 `re-` 5；記錄 `mutants-run1.log`）；之後補的 `re-spawn-new-wrong-id` 1/1 killed（`mutants-run2.log`）。沒有存活、沒有 wrong_failure、沒有建置失敗。
- 既有、殺它的測試檔被本批改過的（D184）：`or-extra-skipped`、`or-beforeswap-skipped`、`or-prepare-failure-swaps`、`or-replace-gameinit-again`、`or-carry-not-given-back`、`g8-beforeswap-failure-swaps`（`RestoreFlowTests.cpp` 改了標記字樣）、`ei-not-default`（`config/modules.csv` 多一列）：7/7 killed（`mutants-run3-existing.log`）。
- 第一次跑之前，突變清單經 Fable／high 對抗檢查（§14.2）：四個存檔版本突變的標記前面有不帶標記的 `REQUIRE_FALSE` 先中止（會被算成存活），把標記掛到先判的那一行；`Configure` 的 `>`→`>=`、碼的字串值兩處存活，補案例「設定相等」與突變 `sv-equal-refused`、`sv-codes-equal`，測試改和字面比；`[sv-buffer-policy]` 拆成兩個。

## 加的突變（§14.5 第 8 點）

共 21 個，每個都寫了它獨抓的誤り與 `guards`：
- `entity_ids`（真 `RangeAllotter`，宣告 18.2 第 1 件）3 個：`re-owns-always`、`re-owns-never`、`re-allot-stays`。`Of` 忽略下界不另立（就是既有的 `sc-lower-bound-not-moved`）。
- Kernel（讀檔形狀的案例抓）6 個：`re-spawn-cells-no-onadd`、`re-hook-spawn-lost`、`re-spawn-stamp-one`、`re-insert-before-spawn`、`re-resmut-write-lost`、`re-spawn-new-wrong-id`。「拿掉範圍檢查」「同一記錄者留最後一筆」「不同記錄者留一筆」不另立（`sr-dropped-made-alive`、`sr-keep-last`、`sr-two-emitters-keep-first`）。
- `save_version` 12 個。
- README 第 8 點的門檻：本批改的 Design Invariants 是新頁 SV-1～SV-3（3 條 × 5＝15）。存檔版本 12 個在門檻內；讀檔的 9 個守的是宣告的判定（18.3），不是 Design Invariants 的條文。合計 21 個超過 15，合併前核准時說明。
- 前綴用 `re-`（restore-entry），不用 `rs-`（Kernel 清單 registry-schedule 那一批已用）。

## 和宣告的出入

- `re-insert-before-spawn` 不是「調換兩段」，是在第 5 步加條件模擬「插入在生成之前」（單一 find／replace 的限制）；和既有的 `sr-same-call-inserts-dropped` 改同一行但守不同的誤り、不同的執行檔。
- 宣告 18.2 第 1 件的代價寫「案例 2 用到的 Engine 部分由 Kernel 既有的突變守」：查了沒有守「`Spawn::New` 回傳配到的編號」的既有突變，補 `re-spawn-new-wrong-id`（只收緊）。
- SV-2 後半「啟用也不登記任何 World 的東西」不立守衛：它綁實作的方式（AGENTS §3「ガードを立てる時期」），由合併前的設計審查看。

## 未結

- 合併前的設計審查（Fable／high，唯讀）、L1 三平台、合併前核准。
