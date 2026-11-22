# 2026-11-22 GYO2：第 4 批留下的三件後續

批次代號：`v8-arch-gyo2-batch4-followups`。分支 `claude/gyo2-batch4-followups-a38a84`（從 `gyo2` bb5d010d，第 4 批 #259 合併後）。任務卡，主會話「[GYO2][不良少年]新架構重構」開的。實作者 Opus 5.5／high。

## 起因

第 4 批（權威＋回滾，#259）的 dev_log [`2026_11_22_gyo2_authority_rollback.zh-Hant.md`](2026_11_22_gyo2_authority_rollback.zh-Hant.md)（「第 8 步：從頭完整跑的突變」一節、合併前核准頁第 2 節）留下三件。使用者原話：「先修這個」。範圍：

1. `gyo2` 上本來就壞的 10 個突變（Kernel 8、編排層 2）逐一改寫或退役。
2. 契約頁 [`authority-sources.zh-Hant.md`](../architecture/authority-sources.zh-Hant.md) DI 1 與 Replacement Criteria 的生產端用詞。
3. DI-A 2（權威）、R-3（回滾）的常設守衛（第 4 批宣告 9.5 原本延到第 5 批）。

## 1. 10 個突變

先問：它原本守的性質現在還在不在、在哪裡。10 個的性質都還在，全部改寫、沒有退役，所以不必從 `guards` 找指向它們的條文（README 第 7 點）。

| 突變 | 壞在哪 | 性質現在在哪 | 處置 |
|---|---|---|---|
| `el-kernel-print-back` | trace 批第 3 步（d318d8be）給 `WorldExecutor::Fail` 加了 `origin` 參數，find 對不上 | error-handling DI 2：Kernel 不寫 stderr；`kernel.el-silent` 探針 | find 跟上新簽名 |
| `ks-s2-interrupted-stops-refusing` | 同一步在 `Fail` 裡插了 trace 的標記行 | DI-KS 26：中斷的、不允許動態插入的 World 仍讓登記被拒（`refusingWorlds`） | find 跟上；補 `guards` |
| `obs-host-err-not-failed` | `Fail(…, false)` 多了第三個引數 | DI-KS 10：Host 的 Trigger 出錯時中斷 World | find 跟上 |
| `obs-exclusive-err-not-left` | 原本的 `halt(…)` 改成 `Fail(…, true, …)` | DI-KS 10、17：ExclusiveWorld::Trigger 的錯誤中斷 World、留給最外層 | find 跟上 |
| `obs-host-no-snapshot-after` | 同 `obs-host-err-not-failed` | DI-KS 2、10：Host 的 Trigger 後取快照 | find 跟上 |
| `ks-k6-calls-off` | 異常分級 D1（f38e5691）拿掉 `executor_file`，find 對不上 | error-handling DI 2：K6 報出記錄呼叫 | 改打 K6 的迴圈本身；補 `guards` |
| `ks-k6-every-file-exempt` | 同上（replace 改的那行不在了） | 原本守「豁免不擴大到 Executor 以外」；D1 之後豁免的範圍是空的（沒有檔案豁免），能寫出的錯只剩「Executor 的豁免回來」 | 改寫成「K6 又豁免 Executor」，expect 改成 `test_k6_executor_file_is_checked`；補 `guards`。第一次跑存活，原因與修正見下 |
| `ks-k8-skips-executor-files` | replace 用到已不存在的 `executor_file` | kernel DI-KS 14：K8 也讀 Executor | replace 改成直接比檔名；補 `guards` |
| `or-failure-no-product` | 異常分級 P12 讓 `RunOn` 先填 `failure.product`，拿掉它由 `Break` 的補值補上，等價 | O-8：失敗說出是哪個 Product。真正補名字的是 `Break` 的補值（Product 外中斷、DropOldFirst 的替換、丟掉的 World 都靠它），但沒有任何檢查讀那幾條路的 `product` | 改打 `Break` 的補值；測試 `O-8: a World interrupted outside the Product…` 加一條 `[or-ended-outside-product]`（只收緊）；補 `guards` |
| `or-runner-logs-back` | 異常分級 D2 之後 `Product.cpp` 不含 `Log.hpp`，一處改動的突變建置失敗 | O-3、error-handling DI 4：預設 runner 只回原因、不寫 | 照 #254 `or-failure-logged` 的前例改成往 stderr 寫，由 `orchestration.el-silent` 抓；`silent_cases.txt` 加上 `O-3: the default runner returns*`（只收緊） |

- `ks-k6-every-file-exempt` 第一次跑存活：`test_k6_executor_file_is_checked` 的第二個斷言 `assertReported("K6", "Log")` 用子字串比對，標頭那條 `K6 engine/base/Log.hpp` 也含 `Log`，所以 Executor 的記錄呼叫被豁免時測試照樣通過。照 README 第 6 點先補測試：第二個斷言改成要有以 `": K6 Log"` 結尾的那條（同檔 K8 的測試已用這個寫法）。只收緊。補完後 `ks-k6-calls-off`、`ks-k6-every-file-exempt`、`ks-adv-executor-exempt-all` 3/3 killed。
- `or-runner-logs-back` 改寫後，`[or-runner-default-silent]`（數 `Base::Log` 的行）沒有突變了：一處改動的突變加不進 `Log.hpp` 的 include。和 #254 的 `[or-failure-not-logged]` 同一個處理；「不寫」這個性質由 `orchestration.el-silent` 守。
- `RunOn` 裡那行 `failure.product = name_;` 拿掉仍然等價（`Break` 補上）；它存在是為了例外路徑上不再配置記憶體（P12），測試不到，不另立突變。

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

| 目錄 | include 方向 | 已有的守衛 |
|---|---|---|
| `engine/ecs/kernel` | 守住 | `kernel_guard.py` K5 的准許清單只有 `engine/base/`、`engine/entity_store/`、`engine/kernel/` 與標準庫；連結集 `gyo_expect_links(gyo_kernel GYO::Base GYO::EntityStore)` |
| `engine/orchestration` | 守住 | `orchestration_guard.py` O4 的准許清單 `engine/kernel/`、`engine/base/`、`engine/orchestration/`；連結集 `gyo_expect_links(gyo_orchestration GYO::Base GYO::Kernel)` |
| `engine/modules/world_time` | 守住 | 連結集（WT-4）`GYO::Base GYO::Kernel GYO::Logging GYO::Orchestration GYO::Time`。沒有全域的 `include_directories`，標頭目錄只隨連結傳遞，沒連到就 include 不到 |
| `engine/modules/rollback`（R-3） | **沒守住** | 沒有連結集的檢查。權威這一側另有登記檔的順序（authority 排在 rollback 之後）與 O6（`engine/` 裡手寫連結登記的 target），但從 `tests/` 等較晚 configure 的目錄連進去、或之後才登記的網路、狀態機 Module，沒有東西擋 |

- DI-A 2 已由現有守衛守住，只照實改寫條文的「守衛」欄。
- 判定 4 的名字（`Authority::`、`Rollback::`）不另立掃描：沒有標頭就用不到權威、回滾的型別，只剩自己宣告同名命名空間這種誤用，交審查（不為誤用加守衛）。Kernel 自己長回區、寫者等舊概念不屬 DI-A 2，由宣告 9.5 的「拿掉的名字不回來」管。
- R-3 缺的一塊：`tests/common/rollback/CMakeLists.txt` 加一行 `cmake_language(DEFER … CALL gyo_expect_links gyo_rollback GYO::Base GYO::Kernel GYO::Orchestration GYO::WorldTime)`，用既有的 `gyo_expect_links`（`tests/common/time/CMakeLists.txt`）。連結集釘死而不是只列「不准連權威」，是因為網路、狀態機 Module 還不存在、叫不出名字；釘死的連結集對之後登記的 Module 也有效（`world_time` WT-4 同一個做法）。

### 突變（README 第 4 點）

| 突變 | 守的錯 | 結果 |
|---|---|---|
| `rb-r3-links-more` | 真實樹上的陽性對照：登記檔 rollback 列 depends 加 `json`（排在 rollback 之前，登記檔的順序檢查放行），只有新守衛擋 | killed |
| `rb-r3-authority-from-later-directory` | 從較晚 configure 的 `tests/common/authority` 把 `GYO::Authority` 連進 `gyo_rollback`：登記檔與 O6 都管不到的那條路，只有 DEFER 的檢查看得到 | killed |

- 規則整條關掉、fail closed：守衛的本體是既有的 `gyo_expect_links`（比對實際與期待的整串連結，沒有連結時實際為空、照樣失敗），這次只多一個呼叫。呼叫本身用反向確認示範：拿掉呼叫 → 兩個突變都該存活；DEFER 改成當場呼叫 → 只有 `rb-r3-authority-from-later-directory` 該存活。結果見下。
- 本批加的突變 2 個（改的 DI 條數 2 × 5 以內）；各自只捕自己的錯（寫在 `what`），都有 `guards`。

## 驗證

（待補：反向確認、敵對式檢查、全量 ctest、L1）

## §14.4（Design Invariants 的守衛判定）

（待補）

## 證據

主 checkout 的 `build/evidence/gyo2-batch4-followups/`（使用者的 Intel Mac，git 不追蹤）：`configure.log`、`build.log`、`configure2.log`、`run-kernel8.log`（第一次：7/8，`ks-k6-every-file-exempt` 存活）、`run-k6-after-test-fix.log`（3/3）、`run-orch2.log`（2/2）、`run-rb2.log`（2/2）、`reverse.py`、`reverse-check.log`。

## 後續（不在本卡）

- 宣告 9.5 延到第 5 批的其餘幾條：Kernel 的「拿掉的名字不回來」、`naming_guard` 加 `engine/modules/authority`、`engine/modules/rollback`、權威 Module 的 `gyo_expect_links`。
