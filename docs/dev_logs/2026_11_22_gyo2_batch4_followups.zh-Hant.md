# 2026-11-22 GYO2：第 4 批留下的三件後續

批次代號：`v8-arch-gyo2-batch4-followups`。分支 `claude/gyo2-batch4-followups-a38a84`（從 `gyo2` bb5d010d，第 4 批 #259 合併後）。任務卡，主會話「[GYO2][不良少年]新架構重構」開的。實作者 Opus 5.5／high；敵對式檢查者 Fable 5.2／high（唯讀）。

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
| `ks-k6-every-file-exempt` | 同上 | 它守「Executor 的豁免不擴大到每個檔」。D1 拿掉了豁免本身，這個錯已經寫不出來 | **退役**（README 第 7 點，D1 時就該退）。`git grep` 從各清單的 `guards` 找不到指向它的條文（它本來沒有 `guards`）；它的 id 在 docs 裡只出現在第 4 批 dev_log 的後續清單，那是當時的紀錄，不改 |
| `ks-k8-skips-executor-files` | replace 用到已不存在的 `executor_file` | kernel DI-KS 14：K8 也讀 Executor | 改寫：replace 直接比檔名；`what` 拿掉「like K6」；補 `guards` |
| `or-failure-no-product` | 異常分級 P12 讓 `RunOn` 先填 `failure.product`，拿掉它由 `Break` 的補值補上，等價 | O-8：失敗說出是哪個 Product。真正補名字的是 `Break` 的補值（Product 外中斷、DropOldFirst 的替換、丟掉的 World 都靠它），但沒有檢查讀那幾條路的 `product` | 改寫：打 `Break` 的補值；測試 `O-8: a World interrupted outside the Product…` 加一條 `[or-ended-outside-product]`（只收緊）；補 `guards` |
| `or-runner-logs-back` | 異常分級 D2 之後 `Product.cpp` 不含 `Log.hpp`，一處改動的突變建置失敗 | O-3、error-handling DI 4：預設 runner 只回原因、不寫 | 改寫：照 #254 `or-failure-logged` 的前例改成往 stderr 寫，由 `orchestration.el-silent` 抓；`silent_cases.txt` 加上 `O-3: the default runner returns*`（只收緊） |

- `test_k6_executor_file_is_checked` 原本抓不到「Executor 的記錄呼叫被豁免」：第二個斷言 `assertReported("K6", "Log")` 用子字串，標頭那條 `K6 engine/base/Log.hpp` 也含 `Log`。第一版把 `ks-k6-every-file-exempt` 改寫成「K6 又豁免 Executor」時它存活（`run-kernel8.log`），由此發現。照 README 第 6 點補測試：第二個斷言改成要有以 `": K6 Log"` 結尾的那條（同檔 K8 的測試已用這個寫法）。只收緊。
- 敵對式檢查 F2 指出：上面那個改寫其實是「退役＋新增」，而且 `ks-k6-calls-off` 也會讓收緊後的斷言失敗。所以照 README 第 1 點與「合わせ方」：`ks-k6-every-file-exempt` 退役，`ks-k6-calls-off` 的 expect 改成兩個測試都失敗才一致（`(?=[\s\S]*(?:FAIL: test_k6_log_call_is_reported))[\s\S]*(?:FAIL: test_k6_executor_file_is_checked)`），再跑 killed。
- 檢查 F1：上面的改動讓 K6 的標頭分支（`if target in LOG_HEADERS`）失去唯一的突變（K5 准許 `engine/base/Log.hpp`、`<iostream>`、`<cstdio>`、`<print>`，K5 不會補位）。新增 `ks-k6-log-header-off`。
- `or-runner-logs-back` 改寫後，`[or-runner-default-silent]`（數 `Base::Log` 的行）沒有突變；`or-failure-no-product` 改寫後，`[or-ended-failure-who]` 也沒有（它的 product 條件由 `RunOn` 與 `Break` 兩處冗餘地滿足，一處改動殺不掉；world、kind 兩個條件從來沒有突變）。前者和 #254 的 `[or-failure-not-logged]` 同一個處理，「不寫」由 `orchestration.el-silent` 守；後者的性質由 `[or-ended-outside-product]` 守。
- `RunOn` 裡的 `failure.product = name_;` 拿掉仍然等價；它存在是為了例外路徑上不再配置記憶體（P12），測試不到，不另立突變。

### 敵對式檢查找到的另外 8 個（F7）

全部清單做靜態掃描（每個突變的 find 在檔案裡出現幾次，不用建置），另有 8 個 find 為 0：都是第 4 批第 8 步 A3（5263c865）、A5（61d6f72c）的修正落在完整跑（ecae9977）之後，每步只跑相關突變時沒挑出來。和第 1 件同性質（`gyo2` 上本來就壞），一併處理。

| 突變 | 壞在哪 | 處置 |
|---|---|---|
| `rs-iv-fc3-step0-skipped`、`hf-round-skips-initial-values` | A3 給 5a 步加了 `derive == Detail::Derive::InitialValues` | 改寫：find 跟上 |
| `ks-s5-change-deferred`、`ks-s5-change-stamp` | A3 給 `ApplySync` 加了 `derive` 引數 | 改寫：find 跟上 |
| `ks-restore-record-by-record` | A3 給 `ApplyImmediateRecords` 加了 `derive` 參數 | 改寫：find、replace 跟上 |
| `rb-input-capacity-unchecked` | A5 加了容量 0 的情形 | 改寫：find 跟上 |
| `au-input-rolled` | A5 把輸入的過濾搬到前面自己的分支（代為登記輸入紀錄、然後 `return`），舊的那行 `\|\| data_->options.input` 不在了 | 改寫：拿掉那個分支的 `return`，讓輸入落到回滾狀態的代為登記。檢查者建議退役（「A5 反轉了性質」），我不同意：「輸入永遠不登記成回滾狀態」（Z8，AU-E7）仍在，只是判斷搬了位置。改寫後 killed，expect（AU-E7）不變 |
| `au-digest-resource-dropped` | A5 給 `kResourceOps` 加了 `recordInput` 欄 | 改寫：find 跟上 |

18 個合計：改寫 17、退役 1。全部掃描之後，6 份清單（Kernel、authority、base、orchestration、rollback、world_time）沒有 find 次數不是 1 的突變。

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

| 範圍 | 已有的守衛 | 敵對式檢查之後的判斷 |
|---|---|---|
| `engine/ecs/kernel`（DI-A 2） | `kernel_guard.py` K5 的准許清單（`engine/base/`、`engine/entity_store/`、`engine/kernel/` 與標準庫，不准 `..`）；連結集 `{Base, EntityStore}` | 守住：include 的文字本身就要寫出標頭，相對路徑、另加標頭目錄都過不了 K5 |
| `engine/orchestration`（DI-A 2） | O4 的准許清單；連結集 `{Base, Kernel}` | **有洞（F5）**：O4 沒有 `..` 條件，`"engine/kernel/../../../../../modules/rollback/include/…"` 過得了、也編得過。照 K5 補上（下面） |
| `engine/modules/world_time`（DI-A 2） | 連結集（WT-4） | **只守住連結（F6）**：沒有文字層的 include 檢查，相對路徑（`../../rollback/include/…`）或另加標頭目錄可以不經連結 include 到。一次性掃描用的名字（`Authority::`、`Rollback::`）抓得到這種用法，常設的沒有 |
| `engine/modules/rollback`（R-3） | 沒有連結集檢查；權威這一側只有登記檔的順序（authority 排在 rollback 之後）與 O6（`engine/` 裡手寫連結登記的 target） | **缺**：從較晚 configure 的目錄連進去、之後才登記的網路、狀態機 Module 都沒人擋。補連結集（下面）。相對路徑與另加標頭目錄同 `world_time`（F4） |

### 補了什麼

- **R-3**：`tests/common/rollback/CMakeLists.txt` 加一行 `cmake_language(DEFER … CALL gyo_expect_links gyo_rollback GYO::Base GYO::Kernel GYO::Orchestration GYO::WorldTime)`，用既有的 `gyo_expect_links`。釘死整個連結集而不是只列「不准連權威」，是因為網路、狀態機 Module 還不存在、叫不出名字（`world_time` WT-4 同一個做法）。檢查者在小型 CMake 專案裡試過 PRIVATE／PUBLIC、裸名 `gyo_authority`、`$<LINK_ONLY:>`、`$<BUILD_INTERFACE:>`、`$<COMPILE_ONLY:>`、從較晚的目錄 `set_property(INTERFACE_LINK_LIBRARIES)`，全部擋下。
- **O4 的 `..`（F5）**：`orchestration_guard.py` O4 的引號 include 加上「路徑不准有 `..`」（`/`、`\` 都當分隔，和 K5 同一個條件），自測 `test_o4_parent_directory_include_is_reported`，突變 `or-guard-o4-parent-directory`。只收緊既有規則的讀法（舊判定失敗的輸入仍失敗，見 §14.4）。
- **DI-A 2**：兩條條文的「守衛」欄照實改寫，寫明哪裡守住、哪裡沒有文字層的守衛。

### 沒做、交使用者決定的：`world_time` 與回滾 Module 的 include 准許清單（F4、F6）

- 現況：用相對路徑或另加標頭目錄、不經連結去 include 權威或回滾的標頭，`world_time` 與回滾 Module 都沒有守衛（只有標頭層級的知識漏得進去；用到權威的非 inline 函式時，測試程式的連結會失敗）。
- 這是刻意繞過連結的寫法，我先照「不為誤用加守衛」交審查，條文的守衛欄照實寫。
- 選項：(a) 照 O4 的樣子給 `engine/modules/world_time`、`engine/modules/rollback` 立 include 准許清單（一條文字規則擋住上面所有路徑；是立新規則，README「尋ねるもの」）；(b) 在同一個 DEFER 裡再釘 `INCLUDE_DIRECTORIES`（便宜，但擋不住相對路徑與 `-I`）；(c) 維持審查（本卡現在的做法）。檢查者傾向 (a)，理由是 R-3 的條文字面上就是「不 include」。

### 突變（README 第 4 點）與反向確認

| 突變 | 守的錯 | 結果 |
|---|---|---|
| `rb-r3-links-more` | 真實樹上的陽性對照：登記檔 rollback 列 depends 加 `json`（排在 rollback 之前，登記檔的順序檢查放行），只有新守衛擋 | killed |
| `rb-r3-authority-from-later-directory` | 從較晚 configure 的 `tests/common/authority` 把 `GYO::Authority` 連進 `gyo_rollback`：登記檔與 O6 都管不到的那條路，只有 DEFER 的檢查看得到 | killed |
| `or-guard-o4-parent-directory` | O4 的 `..` 條件拿掉 | killed |
| `ks-k6-log-header-off` | K6 的標頭分支關掉（F1） | killed |

- 兩個 R-3 突變的 expect 在 `GYO::WorldTime` 之後加了 `(?![;\w])`（檢查的小處：期待清單多一個排在後面的名字時不該還中）。
- 規則整條關掉、fail closed：守衛本體是既有的 `gyo_expect_links`（比對整串連結，沒有連結時實際為空、照樣失敗；target 不存在時 CMake 報錯），這次只多一個呼叫。呼叫本身用反向確認示範（`reverse.py`）：拿掉呼叫 → 兩個 R-3 突變都存活（0/2）；DEFER 改成當場呼叫 → 只有 `rb-r3-authority-from-later-directory` 存活（1/2）。和 `wt-links-more`、`os-g19`、`ti-x5` 的前例一樣。
- 本批新增 4 個、退役 1 個（改的 DI 條數 R-3、DI-A 2、O-4 的守衛 3 × 5 以內）；新增的各自只捕自己的錯（寫在 `what`），都有 `guards`。改寫的 17 個裡原本沒有 `guards` 的 8 個補上（`ks-s2-interrupted-stops-refusing` DI-KS 26、`ks-k6-calls-off` error-handling DI 2、`ks-k8-skips-executor-files` DI-KS 14、`or-failure-no-product` O-8、F7 的 `rs-iv-fc3-step0-skipped`、`hf-round-skips-initial-values` DI-KS 9、`ks-s5-change-deferred`、`ks-s5-change-stamp` DI-KS 18）。

## 敵對式檢查

- 檢查者：Fable 5.2／high，唯讀（作者 Opus 5.5）。探測放在暫存區（`adv/`），未保存。
- 找到 6 處要處理（F1、F2、F4、F5、F6、F7），處置如上：F1、F2、F5、F7 修；F4、F6 措辭照實改，選項交使用者；F7 中 `au-input-rolled` 的退役建議不採（理由見上）。
- 判定不是缺口的：五個 find 跟上的突變、`ks-k6-calls-off` 的寫法、收緊的測試、`or-failure-no-product` 打 `Break`、`or-runner-logs-back` 的探針路線、R-3 守衛的各種連結寫法、從較晚目錄 DEFER 連結（刻意規避，所有 DEFER 守衛共通，不加）、把檔案放進別的 Module 的 `include/engine/kernel/`（刻意誤用，審查）。
- 補完後再跑：這輪改動的 14 個 14/14 killed；O4 改動後 orchestration_guard 的全部 14 個 14/14 killed。

## 驗證

（本機 macOS Intel，AppleClang，preset `test`；`run_mutations.py` 加 `-B`、先清 `__pycache__`；每次都先確認未改動的樹通過）

- 卡片 10 個（第一輪）：9/10 killed，`ks-k6-every-file-exempt` 存活 → 補測試後 3/3（`ks-k6-calls-off`、`ks-k6-every-file-exempt`、`ks-adv-executor-exempt-all`）；之後照 F2 退役。
- 新增的 R-3 兩個：2/2 killed；反向確認如上。
- 檢查後的修正：14/14 killed（`ks-k6-calls-off`、`ks-k6-log-header-off`、`ks-k8-skips-executor-files`、F7 的 8 個、`or-guard-o4-parent-directory`、兩個 R-3）。
- orchestration_guard 的全部 14 個突變（O4 舊的 5 個、實樹陽性對照 3 個、O0／O3／O6 與新的 `..`）：14/14 killed（O4 改動不放寬的證明）。
- 全量 ctest：3bf4e45d 161/161；檢查後的修正之後 161/161。
- L1：PR 上跑。

## §14.4：改了 Design Invariants 的守衛判定

都只收緊，沒有緩める面；照 §14.4，DI 的守衛測試仍要使用者核准。

| | 1 守的性質 | 2 緩める面 | 3 締める面 | 4 | 5 根據與證據 |
|---|---|---|---|---|---|
| `gyo_rollback` 的連結集（新） | R-3：回滾 Module 不認得權威、網路、狀態機 | 無（新判定） | `gyo_rollback` 多連任何 target 就 configure 失敗（以前通過）。正當地加依賴時，要同時改這一行 | — | `rollback.zh-Hant.md` R-3；`run-rb2.log`、`run-after-check.log`、`reverse-check.log` |
| O4 的 `..` | O-4、DI-A 2：編排層只 include kernel、base、orchestration | 無：條件只用 `or` 加上去，舊判定失敗的輸入仍失敗；O4 舊的突變與實樹陽性對照仍 killed | 准許前綴後面帶 `..` 的引號 include 失敗（以前通過） | — | `orchestration.zh-Hant.md` O-4；`run-orchestration-guard.log` |
| `test_k6_executor_file_is_checked` | error-handling DI 2：Kernel（含 Executor）不呼叫記錄 | 無：新斷言要求以 `: K6 Log` 結尾的一條，舊斷言（含 `Log` 的任一條）在它成立時一定成立 | Executor 只有標頭、記錄呼叫被豁免時失敗（以前通過） | — | `run-kernel8.log`（舊斷言下存活）、`run-k6-after-test-fix.log` |
| `[or-ended-outside-product]`（新斷言） | O-8：失敗說出是哪個 Product | 無 | Product 外中斷的失敗沒有 Product 名字時失敗 | — | `run-orch2.log` |
| `orchestration.el-silent` 加 O-3 那一案 | O-3、error-handling DI 4：預設 runner 不寫 | 無 | 預設 runner 往 stderr 寫時失敗 | — | `run-orch2.log` |

## 證據

主 checkout 的 `build/evidence/gyo2-batch4-followups/`（使用者的 Intel Mac，git 不追蹤）：`configure.log`、`build.log`、`configure2.log`、`run-kernel8.log`、`run-k6-after-test-fix.log`、`run-orch2.log`、`run-rb2.log`、`reverse.py`、`reverse-check.log`、`run-after-check.log`、`run-orchestration-guard.log`、`ctest-full.log`、`ctest-full-2.log`。敵對式檢查的探測未保存。

## 後續（不在本卡）

- `world_time`、回滾 Module 的 include 准許清單（上面的 (a)～(c)），待使用者決定。
- 宣告 9.5 延到第 5 批的其餘幾條：Kernel 的「拿掉的名字不回來」、`naming_guard` 加 `engine/modules/authority`、`engine/modules/rollback`、權威 Module 的 `gyo_expect_links`。
- O4 掃的是原始行，`/* x */ #include "…"` 這種同一行有註解的寫法讀不到（K5 用去掉註解後的碼）。讀法的缺口，照 README 第 4 點等觀測到再補。
- 每步只跑相關突變漏掉 stale 的情形，第 4 批就出現了 8 個。靜態的 find 次數掃描不用建置，可以考慮在每步的相關突變之外一併跑（流程的事，交主會話）。
