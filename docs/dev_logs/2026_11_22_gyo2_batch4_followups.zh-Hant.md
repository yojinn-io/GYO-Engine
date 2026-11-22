# 2026-11-22 GYO2：第 4 批留下的三件後續

批次代號：`v8-arch-gyo2-batch4-followups`。分支 `claude/gyo2-batch4-followups-a38a84`（從 `gyo2` bb5d010d，第 4 批 #259 合併後），PR #265。任務卡，主會話「[GYO2][不良少年]新架構重構」開的。實作者 Opus 5.5／high；敵對式檢查者與設計審查者都是 Fable 5.2／high（唯讀）。

## 起因

第 4 批（權威＋回滾，#259）的 dev_log [`2026_11_22_gyo2_authority_rollback.zh-Hant.md`](2026_11_22_gyo2_authority_rollback.zh-Hant.md)（「第 8 步：從頭完整跑的突變」一節）與合併前核准頁第 2 節留下三件。使用者原話：「先修這個」。範圍：

1. `gyo2` 上本來就壞的突變逐一改寫或退役（卡片列了 10 個：Kernel 8、編排層 2）。
2. 契約頁 [`authority-sources.zh-Hant.md`](../architecture/authority-sources.zh-Hant.md) DI 1 與 Replacement Criteria 的生產端用詞。
3. DI-A 2（權威）、R-3（回滾）的常設守衛（第 4 批宣告 9.5 原本延到第 5 批）。

## 1. 壞掉的突變：10 個加敵對式檢查找到的 8 個

做法照 [`tests/common/mutation/README.md`](../../tests/common/mutation/README.md)：先問它原本守的性質現在還在不在、在哪裡。

### 卡片列的 10 個

| 突變 | 壞在哪 | 性質現在在哪 | 處置 |
|---|---|---|---|
| `el-kernel-print-back` | trace 批第 3 步（d318d8be）給 `WorldExecutor::Fail` 加了 `origin` 參數 | error-handling DI 2：Kernel 不寫 stderr；`kernel.el-silent` 探針 | 改寫：find 跟上新簽名 |
| `ks-s2-interrupted-stops-refusing` | 同一步在 `Fail` 裡插了 trace 的標記行 | DI-KS 26：中斷的、不允許動態插入的 World 仍讓登記被拒 | 改寫：find 跟上；補 `guards` |
| `obs-host-err-not-failed` | `Fail(…, false)` 多了第三個引數 | DI-KS 10 | 改寫：find 跟上 |
| `obs-exclusive-err-not-left` | 原本的 `halt(…)` 改成 `Fail(…, true, …)` | DI-KS 10、17 | 改寫：find 跟上 |
| `obs-host-no-snapshot-after` | 同 `obs-host-err-not-failed` | DI-KS 2、10 | 改寫：find 跟上 |
| `ks-k6-calls-off` | 異常分級 D1（f38e5691）拿掉 `executor_file`，find 對不上 | error-handling DI 2：K6 報出記錄呼叫 | 改寫：打 K6 的迴圈本身；合併 `test_k6_executor_file_is_checked`（見下）；補 `guards` |
| `ks-k6-every-file-exempt` | 同上 | 它守「Executor 的豁免不擴大到每個檔」。D1 拿掉了豁免本身，這個錯寫不出來 | **退役**（README 第 7 點，D1 時就該退） |
| `ks-k8-skips-executor-files` | replace 用到已不存在的 `executor_file` | 它守「K8 學 K6 豁免 Executor」，樣板（K6 的豁免）D1 拿掉了 | **退役**（設計審查第 6 項；第一版改寫成「比檔名」，等於自己造一個豁免，和上一列的處理不一致）。`test_k8_executor_file_is_checked` 合進 `ks-adv-executor-exempt-all` 的 expect（「Executor 整個豁免」是剩下唯一的 Executor 之錯） |
| `or-failure-no-product` | 異常分級 P12 讓 `RunOn` 先填 `failure.product`，拿掉它由 `Break` 的補值補上，等價 | O-8：失敗說出是哪個 Product。真正補名字的是 `Break` 的補值（Product 外中斷、DropOldFirst 的替換、丟掉的 World 都靠它），但沒有檢查讀那幾條路的 `product` | 改寫：打 `Break` 的補值；測試 `O-8: a World interrupted outside the Product…` 加一條 `[or-ended-outside-product]`（只收緊）；補 `guards` |
| `or-runner-logs-back` | 異常分級 D2 之後 `Product.cpp` 不含 `Log.hpp`，一處改動的突變建置失敗 | O-3、error-handling DI 4：預設 runner 只回原因、不寫 | 改寫：照 #254 `or-failure-logged` 的前例改成往 stderr 寫，由 `orchestration.el-silent` 抓；`silent_cases.txt` 加上 `O-3: the default runner returns*`（只收緊） |

- 兩個退役的，`git grep` 從各清單的 `guards` 找不到指向它們的條文（兩個本來都沒有 `guards`）；它們的 id 在 docs 裡只出現在第 4 批 dev_log 的後續清單，那是當時的紀錄，不改。
- `test_k6_executor_file_is_checked` 原本抓不到「Executor 的記錄呼叫被豁免」：第二個斷言 `assertReported("K6", "Log")` 用子字串，標頭那條 `K6 engine/base/Log.hpp` 也含 `Log`。第一版把 `ks-k6-every-file-exempt` 改寫成「K6 又豁免 Executor」時它存活（`run-kernel8.log`），由此發現。照 README 第 6 點補測試：第二個斷言改成要有以 `": K6 Log"` 結尾的那條（同檔 K8 的測試已用這個寫法）。只收緊。
- 敵對式檢查 F2：上面那個改寫其實是「退役＋新增」，而且 `ks-k6-calls-off` 也會讓收緊後的斷言失敗。照 README 第 1 點與「合わせ方」：`ks-k6-every-file-exempt` 退役，`ks-k6-calls-off` 的 expect 改成兩個測試都失敗才一致。
- 敵對式檢查 F1：上面的改動讓 K6 的標頭分支（`if target in LOG_HEADERS`）失去唯一的突變（K5 准許 `engine/base/Log.hpp`、`<iostream>`、`<cstdio>`、`<print>`，不會補位）。新增 `ks-k6-log-header-off`。
- `or-runner-logs-back` 改寫後，`[or-runner-default-silent]`（數 `Base::Log` 的行）沒有突變；`or-failure-no-product` 改寫後，`[or-ended-failure-who]` 也沒有（它的 product 條件由 `RunOn` 與 `Break` 兩處冗餘地滿足，一處改動殺不掉；world、kind 兩個條件從來沒有突變）。前者和 #254 的 `[or-failure-not-logged]` 同一個處理，「不寫」由 `orchestration.el-silent` 守（兩者看兩個出口，互補）；後者的性質由 `[or-ended-outside-product]` 守。
- `RunOn` 裡的 `failure.product = name_;` 拿掉仍然等價；它存在是為了例外路徑上不再配置記憶體（P12），測試不到，不另立突變。

### 敵對式檢查找到的另外 8 個（F7）

全部清單做靜態掃描（每個突變的 find 在檔案裡出現幾次，不用建置），另有 8 個 find 為 0：都是第 4 批第 8 步 A3（5263c865）、A5（61d6f72c）的修正落在完整跑（ecae9977）之後，每步只跑相關突變時沒挑出來。和第 1 件同性質（`gyo2` 上本來就壞），一併處理。

| 突變 | 壞在哪 | 處置 |
|---|---|---|
| `rs-iv-fc3-step0-skipped`、`hf-round-skips-initial-values` | A3 給 5a 步加了 `derive == Detail::Derive::InitialValues` | 改寫：find 跟上；補 `guards`（DI-KS 9） |
| `ks-s5-change-deferred`、`ks-s5-change-stamp` | A3 給 `ApplySync` 加了 `derive` 引數 | 改寫：find 跟上；補 `guards`（DI-KS 18） |
| `ks-restore-record-by-record` | A3 給 `ApplyImmediateRecords` 加了 `derive` 參數 | 改寫：find、replace 跟上 |
| `rb-input-capacity-unchecked` | A5 加了容量 0 的情形 | 改寫：find 跟上 |
| `au-input-rolled` | A5 把輸入的過濾搬到前面自己的分支（代為登記輸入紀錄、然後 `return`），舊的那行 `\|\| data_->options.input` 不在了 | 改寫：拿掉那個分支的 `return`，讓輸入落到回滾狀態的代為登記。檢查者建議退役（「A5 反轉了性質」），我不同意：「輸入永遠不登記成回滾狀態」（Z8，AU-E7）仍在，只是判斷搬了位置（設計審查同意）。expect（AU-E7）不變 |
| `au-digest-resource-dropped` | A5 給 `kResourceOps` 加了 `recordInput` 欄 | 改寫：find 跟上 |

18 個合計：改寫 16、退役 2。全部掃描之後，6 份清單（Kernel、authority、base、orchestration、rollback、world_time）沒有 find 次數不是 1 的突變。

## 2. 契約頁的兩處文字

只改生產端的用詞，條文的意思不變。使用者以「先修這個」同意（第 4 批合併前核准頁第 2 節的建議）。

- DI 1
  - 改前：「消費端不讀 CMake、不讀 Kernel 內部結構。」
  - 改後：「消費端不讀 CMake、不讀 Engine 內部結構。」
- Replacement Criteria
  - 改前：「Kernel 的 G-5 匯出落地後，若匯出的內容無法以本格式表達……」
  - 改後：「權威 Module 的匯出落地後，若匯出的內容無法以本格式表達……」
- 頁末的註改寫成這次改了什麼、為什麼；頁首「更新」加一句。

## 3. DI-A 2、R-3 的常設守衛

### 先查現有的守衛守住多少

第 4 批宣告第 13 節判定 4 的「方向」：`engine/ecs/kernel`、`engine/orchestration`、`engine/modules/world_time` 裡沒有 `engine/authority/`、`engine/rollback/` 的 include，也沒有 `Authority::`、`Rollback::` 的名字。

| 範圍 | 已有的守衛 | 判斷 |
|---|---|---|
| `engine/ecs/kernel`（DI-A 2） | `kernel_guard.py` K5 的准許清單（`engine/base/`、`engine/entity_store/`、`engine/kernel/` 與標準庫，不准 `..`）；連結集 `{Base, EntityStore}` | 守住 |
| `engine/orchestration`（DI-A 2） | O4 的准許清單；連結集 `{Base, Kernel}` | 連結守住。O4 原本不查 `..`，`"engine/kernel/../../../../../modules/rollback/include/…"` 過得了（檢查 F5）：既有准許清單自己的洞，照 K5 補上（下） |
| `engine/modules/world_time`（DI-A 2） | 連結集（WT-4） | 連結守住。相對路徑或另加標頭目錄可以不經連結 include（檢查 F6），交設計審查 |
| `engine/modules/rollback`（R-3） | 沒有連結集檢查；權威這一側只有登記檔的順序（authority 排在 rollback 之後）與 O6 | **缺**：從較晚 configure 的目錄連進去、之後才登記的網路、狀態機 Module 都沒人擋。補連結集。相對路徑同上（檢查 F4） |

### 使用者的決定：不經連結的 include 交審查

- **原話**（2026-11-22，side chat 轉主會話）：「嗯找你建議。同時回報主會話，讓他建立review機制。」
- **比過的選項**：(a) 給 `engine/modules/rollback`、`engine/modules/world_time` 立 include 准許清單（新規則）／(b) 再釘 `INCLUDE_DIRECTORIES`（擋不住相對路徑與 `-I`）／(c) 不加守衛、交 review ← 選。
- **理由**：刻意繞過連結的寫法屬於「誤用不加守衛、交 review」；include 准許清單擋得住寫法、擋不住「認得權威」這個概念（例如自己複製一份型別）。review 就是同日定的每批設計審查（`v8-arch-gyo2-design-review-stage`）。
- **出處**：主會話 HANDOFF `v8-arch-gyo2-include-bypass-to-review`；本批敵對式檢查 F4、F6。
- 這個決定的對象是「不給 `world_time`、回滾新立准許清單」，不包括 O4（O4 的 `..` 見下）。

### 補了什麼

- **R-3**：`tests/common/rollback/CMakeLists.txt` 加一行 `cmake_language(DEFER … CALL gyo_expect_links gyo_rollback GYO::Base GYO::Kernel GYO::Orchestration GYO::WorldTime)`，用既有的 `gyo_expect_links`。釘死整個連結集而不是只列「不准連權威」，是因為網路、狀態機 Module 還不存在、叫不出名字（`world_time` WT-4 同一個做法）。它比 R-3 嚴：加的依賴不屬權威、網路、狀態機時，改登記列與那一行，R-3 不變（條文的守衛欄照這樣寫）。檢查者在小型 CMake 專案裡試過 PRIVATE／PUBLIC、裸名 `gyo_authority`、`$<LINK_ONLY:>`、`$<BUILD_INTERFACE:>`、`$<COMPILE_ONLY:>`、從較晚的目錄 `set_property(INTERFACE_LINK_LIBRARIES)`，全部擋下。
- **O4 的 `..`（檢查 F5）**：`orchestration_guard.py` O4 的引號 include 加上「路徑不准有 `..`」（`/`、`\` 都當分隔，和 K5 同一個條件），自測 `test_o4_parent_directory_include_is_reported`，突變 `or-guard-o4-parent-directory`。設計審查後一度拿掉，使用者定留著（見「使用者的決定：O4 的 `..` 留著」）。
- **DI-A 2**：條文的「守衛」欄照現況改寫：機械守衛是什麼、`world_time` 不經連結的 include 與名字交設計審查。

### 使用者的決定：R-3 本文改成本質的寫法

- **原話**（2026-11-22，side chat「來展開那3件事」轉主會話）：「照建議」（回覆 side chat 建議的 (a)）。
- **比過的選項**：(a) 本文寫成「不依賴：不連結，也不用它們的型別與名字」← 選／(b) 維持「不 include」。
- **理由**：標題是「不認得權威、網路、狀態機」，本文停在 include 這個寫法層級。沒有 include 也能違反標題的意思（在回滾裡抄一份權威的欄位表、用名字字串去找、複製一份型別），照原文卻不算違反。這類誤用已決定不加守衛、交設計審查，審查拿條文判斷，所以條文要寫到本質。改完和 DI-A 2 的寫法一致。
- **出處**：主會話轉來（主會話記進 HANDOFF）；設計審查第 9 項。
- 改前：「**R-3 不認得權威、網路、狀態機**：回滾 Module 不 include 它們。」
- 改後：「**R-3 不認得權威、網路、狀態機**：回滾 Module 不依賴它們：不連結，也不用它們的型別與名字。」
- 不加守衛：「不連結」由 `gyo_rollback` 的連結集守，「不用它們的型別與名字」交設計審查；守衛欄照這樣寫。
- 回滾 Module 的宣告（`rollback-module-declaration.zh-Hant.md` 第 339 行）還是原文：那是核准當時的紀錄，不改；正式條文在 `rollback.zh-Hant.md`。

### 突變（README 第 4 點）與反向確認

| 突變 | 守的錯 | 結果 |
|---|---|---|
| `rb-r3-links-more` | 真實樹上的陽性對照：登記檔 rollback 列 depends 加 `json`（排在 rollback 之前，登記檔的順序檢查放行），只有新守衛擋 | killed |
| `rb-r3-authority-from-later-directory` | 從較晚 configure 的 `tests/common/authority` 把 `GYO::Authority` 連進 `gyo_rollback`：登記檔與 O6 都管不到的那條路，只有 DEFER 的檢查看得到；也是唯一「權威真的連進回滾」的突變 | killed |
| `ks-k6-log-header-off` | K6 的標頭分支關掉（F1） | killed |
| `or-guard-o4-parent-directory` | O4 的 `..` 條件拿掉 | killed |

- 兩個 R-3 突變的 expect 只認多出來的那個名字加 `expected exactly`，不重抄期待清單（設計審查第 2 項：回滾正當地多一個依賴時，不必連突變一起改）。
- 規則整條關掉、fail closed：守衛本體是既有的 `gyo_expect_links`（比對整串連結，沒有連結時實際為空、照樣失敗；target 不存在時 CMake 報錯），這次只多一個呼叫。呼叫本身用反向確認示範（`reverse.py`）：拿掉呼叫 → 兩個 R-3 突變都存活（0/2）；DEFER 改成當場呼叫 → 只有 `rb-r3-authority-from-later-directory` 存活（1/2）。和 `wt-links-more`、`os-g19`、`ti-x5` 的前例一樣。
- 本批新增 4 個、退役 2 個（改的 DI 條文的守衛 R-3、DI-A 2、O-4 3 × 5 以內）；新增的各自只捕自己的錯（寫在 `what`），都有 `guards`。改寫的 16 個裡原本沒有 `guards` 的 7 個補上（`ks-s2-interrupted-stops-refusing` DI-KS 26、`ks-k6-calls-off` error-handling DI 2、`or-failure-no-product` O-8、F7 的 `rs-iv-fc3-step0-skipped`、`hf-round-skips-initial-values` DI-KS 9、`ks-s5-change-deferred`、`ks-s5-change-stamp` DI-KS 18）。

## 敵對式檢查（守衛與突變清單）

- 檢查者：Fable 5.2／high，唯讀（作者 Opus 5.5）。探測放在暫存區（`adv/`），未保存。
- 找到 6 處要處理：F1、F2、F5、F7 修（F5 設計審查後一度拿掉、使用者定留著）；F4、F6 使用者定 (c)。F7 中 `au-input-rolled` 的退役建議不採（理由見上）。
- 判定不是缺口的：五個 find 跟上的突變、`ks-k6-calls-off` 的寫法、收緊的測試、`or-failure-no-product` 打 `Break`、`or-runner-logs-back` 的探針路線、R-3 守衛的各種連結寫法、從較晚目錄 DEFER 連結（刻意規避，所有 DEFER 守衛共通，不加）、把檔案放進別的 Module 的 `include/engine/kernel/`（刻意誤用，審查）。

## 設計審查（每批固定的一步，`v8-arch-gyo2-design-review-stage`）

- 審查者：Fable 5.2／high，唯讀（實作者 Opus 5.5）。看的是設計合不合理，不是找 bug（批次表〈每批的設計審查〉的五個看點）。報告：主 checkout `build/evidence/gyo2-batch4-followups/design-review/REPORT.md`。
- 11 項：保留 6、改 3、要使用者決定 2。

| # | 項目 | 審查的判斷 | 處置 |
|---|---|---|---|
| 1 | `gyo_rollback` 的連結集 | 守本質（依賴以連結為單位，O-6；使用者的決定也把連結當邊界）。比 R-3 嚴，真正釘的是「多一條依賴邊要有人看見」。和別家：方向同 bevy_replicon、比 lightyear（prediction 依賴 replication、connection）嚴；三家都不釘「恰好這幾個」 | 保留；條文照實寫比 R-3 嚴 |
| 2 | 同一個集合寫了五處 | 正當加依賴要改五處，三處不是事實的來源 | 改：R-3 守衛欄不列名字；突變的 expect 只認多出來的名字 |
| 3 | `rb-r3-authority-from-later-directory` | 情境是刻意的寫法，但不加守衛程式、有前例，且是唯一「權威真的連進回滾」的突變 | 保留 |
| 4 | O4 的 `..` | 擋的是誤用，和 (c) 原則上不一致（HANDOFF 那條明寫 review 要涵蓋相對路徑 `..`）；「和 K5 一致」站不住：五個 include 准許清單只有 K5 查 `..` | 我先自己判拿掉（以為 (c) 涵蓋相對路徑 `..`），**使用者定留著**、已恢復（見下） |
| 5 | `test_k6_executor_file_is_checked` 的收緊 | 把空的斷言變成真的；守「Kernel 不寫」的本質 | 保留 |
| 6 | `ks-k8-skips-executor-files` 的改寫 | 自己造了一個 Executor 豁免；退役 `ks-k6-every-file-exempt` 的理由對它一樣成立 | 改：退役，測試合進 `ks-adv-executor-exempt-all` |
| 7 | `[or-ended-outside-product]`、silent_cases 的 O-3 | 行為的契約；`Break` 是補名字的唯一出口（看點 1） | 保留 |
| 8 | 兩條條文的守衛欄與 dev_log | 還寫「待使用者決定」；欄裡有批次代號、dev_log 路徑等經過 | 改：寫現況、引決定的代號 |
| 9 | R-3 本文「回滾 Module 不 include 它們」 | 標題是「不認得」（概念），本文停在「不 include」（寫法）；使用者的決定正是「准許清單擋得住寫法、擋不住認得」 | 交使用者，**使用者選 (a)**，已改（見「使用者的決定：R-3 本文改成本質的寫法」） |
| 10 | 契約頁的用詞 | 和合併前核准頁的建議一致 | 保留；「落地後」已是過去式、頁末註與頁首重複，下次動這頁時處理 |
| 11 | 其餘改寫、退役與 `ks-k6-log-header-off` | 守的性質沒變；同意 `au-input-rolled` 不退役。K6 的標頭分支本身守的是寫法（不 include 也寫得出 `std::fputs`），真正守「Kernel 不寫」的是探針與呼叫掃描 | 保留；標頭分支值不值得留，建立 review 機制時一併看 |

### 使用者的決定：O4 的 `..` 留著

- **原話**（2026-11-22，side chat「來展開那3件事」轉主會話）：「我先說第二題，這個是簡單的可以留著。但是我在想真的會這麼幹嗎？人類工程師要這麼幹，第二天就可以回家休息了。」「你先把第二題轉回去。」
- **比過的選項**：(甲) 拿掉（我照設計審查第 4 項的做法）／(乙) 留著 ← 選。
- **理由**：O4 本來就是編排層的 include 准許清單，用 `..` 能繞過是這條既有守衛自己的洞，K5 早有同樣的條件；補洞是維持既有守衛，不是替誤用另外加一道。(c) 的對象是「不給 `world_time`、回滾新立准許清單」，不包括 O4。
- **出處**：主會話轉來（主會話記進 HANDOFF）。
- 處置：條件、自測、突變照 60f82794 的內容恢復；orchestration_guard 的 14 個突變重跑 14/14 killed（`run-o4-restored.log`）。

- 本批以外、交主會話：`gyo_expect_links` 定義在 `tests/common/time/CMakeLists.txt`，十幾個目錄靠 configure 的先後拿到它（出口一個，家在某一個實作底下）；`..` 的洞在 entity_store、trace、logging 三個 include 守衛也有，照 (c) 交設計審查、不逐個補。

## 驗證

（本機 macOS Intel，AppleClang，preset `test`；`run_mutations.py` 加 `-B`、先清 `__pycache__`；每次都先確認未改動的樹通過）

- 卡片 10 個（第一輪）：9/10 killed，`ks-k6-every-file-exempt` 存活 → 補測試後 3/3；之後照 F2 退役。
- 新增的 R-3 兩個：2/2 killed；反向確認如上。
- 敵對式檢查後：14/14 killed（`run-after-check.log`）；當時 O4 的改動之後，orchestration_guard 的 14 個 14/14 killed（`run-orchestration-guard.log`）。
- 設計審查後：`ks-adv-executor-exempt-all`（合併 K8 的測試）、兩個 R-3（expect 改短）3/3 killed（`run-after-design-review.log`）；當時把 O4 的 `..` 拿掉（`orchestration_guard.py` 與自測回到 bb5d010d 的內容）。
- 使用者定 O4 的 `..` 留著，恢復後：自測 13/13、實樹 clean、orchestration_guard 的 14 個 14/14 killed（`run-o4-restored.log`）；全量 ctest 161/161（`ctest-full-4.log`）。
- 全量 ctest：3bf4e45d 161/161；敵對式檢查後的修正之後 161/161；設計審查後 161/161（`ctest-full-3.log`）。清單：Kernel 671→670、編排層 241→242、回滾 77→79、權威 117。
- L1：PR 上跑。

## §14.4：改了 Design Invariants 的守衛判定

都只收緊，沒有緩める面；照 §14.4，DI 的守衛測試仍要使用者核准。

**核准紀錄**（2026-11-22）：使用者在 side chat「來展開那3件事」說「核准」，經主會話轉來。範圍：下表 5 處（`gyo_rollback` 連結集、O4 的 `..`、`test_k6_executor_file_is_checked` 的精確比對、`[or-ended-outside-product]`、`orchestration.el-silent` 的 O-3 案），都只收緊。

| | 1 守的性質 | 2 緩める面 | 3 締める面 | 4 | 5 根據與證據 |
|---|---|---|---|---|---|
| `gyo_rollback` 的連結集（新） | R-3：回滾 Module 不依賴權威、網路、狀態機（這個判定守「不連結」那一半） | 無（新判定） | `gyo_rollback` 多連任何 target 就 configure 失敗（以前通過）。正當地加依賴時，要同時改這一行 | — | `rollback.zh-Hant.md` R-3；`run-rb2.log`、`run-after-check.log`、`run-after-design-review.log`、`reverse-check.log` |
| O4 的 `..` | O-4、DI-A 2：編排層只 include kernel、base、orchestration | 無：條件只用 `or` 加上去，舊判定失敗的輸入仍失敗；O4 舊的突變與實樹陽性對照仍 killed | 准許前綴後面帶 `..` 的引號 include 失敗（以前通過） | — | `orchestration.zh-Hant.md` O-4；`run-o4-restored.log` |
| `test_k6_executor_file_is_checked` | error-handling DI 2：Kernel（含 Executor）不呼叫記錄 | 無：新斷言要求以 `: K6 Log` 結尾的一條，它成立時舊斷言（含 `Log` 的任一條）一定成立 | Executor 只有標頭、記錄呼叫被豁免時失敗（以前通過） | — | `run-kernel8.log`（舊斷言下存活）、`run-k6-after-test-fix.log` |
| `[or-ended-outside-product]`（新斷言） | O-8：失敗說出是哪個 Product | 無 | Product 外中斷的失敗沒有 Product 名字時失敗 | — | `run-orch2.log` |
| `orchestration.el-silent` 加 O-3 那一案 | O-3、error-handling DI 4：預設 runner 不寫 | 無 | 預設 runner 往 stderr 寫時失敗 | — | `run-orch2.log` |

## 證據

主 checkout 的 `build/evidence/gyo2-batch4-followups/`（使用者的 Intel Mac，git 不追蹤）：`configure.log`、`build.log`、`configure2.log`、`run-kernel8.log`、`run-k6-after-test-fix.log`、`run-orch2.log`、`run-rb2.log`、`reverse.py`、`reverse-check.log`、`run-after-check.log`、`run-orchestration-guard.log`、`run-after-design-review.log`、`run-o4-restored.log`、`ctest-full.log`、`ctest-full-2.log`、`ctest-full-3.log`、`ctest-full-4.log`、`design-review/REPORT.md`。敵對式檢查的探測未保存。

## 後續（不在本卡）

- 宣告 9.5 延到第 5 批的其餘幾條：Kernel 的「拿掉的名字不回來」、`naming_guard` 加 `engine/modules/authority`、`engine/modules/rollback`、權威 Module 的 `gyo_expect_links`。宣告 9.5 的「R-3 與 DI-A 2 的方向掃描」中，`world_time` 與回滾 Module 不經連結 include 的部分，已由 (c) 改成交設計審查，第 5 批不照做（交主會話記）。第 5 批立權威的連結集時，不必再加一個 DEFER 的突變（DEFER 是 `gyo_expect_links` 的性質，已有三個突變在證）。
- 每步只跑相關突變漏掉 stale 的情形，第 4 批就出現了 8 個。靜態的 find 次數掃描不用建置，可以考慮在每步的相關突變之外一併跑（流程的事，交主會話）。
