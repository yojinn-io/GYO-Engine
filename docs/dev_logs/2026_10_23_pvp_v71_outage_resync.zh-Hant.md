# PvP v7.1 outage-resync：斷線恢復＝重新對時

日期：2026-10-23 起。分支 `claude/pvp-v71-outage-resync`（從 master `cdb9b07`）。

**狀態：進行中**。計畫已核准（D172），`gate-characterize` 已決定（D173）。`resync-contract` 的條文核准（`gate-contract`，D174）。`drop-counters` 完成（L1 run 39956082177 四平台成功）。`single-store-refactor` 完成（L1 待確認），下一批是 `match-resync`。

相關文件：

- 計畫：[outage-resync.md](../object_fps_pvp/plans/v7.1/outage-resync.md)。
- 決策 D171 與設計原則：v7.1 記錄器分支 `claude/pvp-v71-after-re-evaluate` 的 HANDOFF。

## 批次

| 批次 | 檔位 | 做了什麼 | 驗收 |
|---|---|---|---|
| `plan-commit` | medium | 提交計畫草案第 2 版與本紀錄 | 只改文件 |
| `outage-characterize`（`c200992` 等） | high | 用測試固定中斷與晚到的現狀：Host 側接上產品的 `LocalPlayerPrediction`，注入雙向、只上行、只下行、部分送達的中斷；Client 側 3 個；稀疏替代 1 個；語料唯讀掃描。產品程式不改 | cpu 測試 293／293；突變 4／4 killed；L1 run 39944311104 四平台成功；報告：[outage-characterize-report.md](../object_fps_pvp/plans/v7.1/outage-characterize-report.md) |
| `resync-contract`（`02f4b6d`～`6859fd9`） | high，審查 xhigh | network-architecture §4「連線中斷與恢復」、§7 兩節 Design Invariants；protocol-v7 §2；log-contract §1.4；ingress README | 只改文件；xhigh 對抗式審查 3 輪（必須修訂 → 必須修訂 → 可送核准，全部處理）；`gate-contract` 核准（D174） |
| `drop-counters`（`3a3e1b1`） | high | Host：被 ±1 s 夾限改變的發布樣本計 `slack_clamped_samples`，寫新的 A 級 `resync statistics` 行。Client：命令已離開 64 格環的相位樣本（死信）計 `phase_samples_unmatched`，模擬 role 在連線換代與停止時寫新的 A 級 `phase summary` 行。判定都不讀 | `ctest -L pvp` 44／45（唯一失敗是已知的 worker，見下）；突變 7／7 killed；突變目錄 559 個的 find 字串都仍唯一；L1 run 39956082177 四平台成功 |
| `single-store-refactor`（`832eca4`、`187d945`） | high，紀錄身分與關閉順序局部 xhigh | 被替代序號的紀錄只存一份：新元件 `MatchSubstitutionRecords`（行為側）持有，帳本只讀到達結果、接收關閉的紀錄，只留附欄；SlackTrack 的 `substituted` 刪除；過渡規則逐位元模擬現狀 | CPU 測試全過；`ctest -L pvp` 44／45（已知的 worker）；`match_ingress_cli`、`authority_digest` 不變；突變 79／79＋4／4；xhigh 等價性審查（差分 fuzz 1380 次 0 差異）；L1 待確認 |

## 規劃

- **D171**：C13 選 (b)，1～10 秒晚到的大負值用斷線重連（重新對時）處理，新開 PR 線，規劃用 ultracode。
- **ultracode**：
  - 參與者：2 位偵察、3 份計畫、1 位評審、1 次對抗式審查。
  - 評審以「風險優先」版為底（58／70）。
- **對抗式審查的結論是「必須修訂」**，主要有三點：
  - 草案漏讀使用者補充的設計原則：身分接回被縮成「同一 endpoint」，並移出範圍。
  - 部分送達、只有上行中斷、冷卻期內這三種情況，大負值現在就會進入 Client 的相位追蹤，草案卻只記錄。
  - `slack_clamped` 追加到 ingress 行，會觸發 log-contract 的 Replacement Criteria。
- **抽查**：主會話對照程式碼，確認了上述三點，以及 endpoint 屬於允許的 session 軟狀態（`network-architecture.zh-Hant.md:137`）。
- **第 2 版的處置**：
  - 把狀態與規則寫成表（計畫第 3 節）；
  - Match 端加上「晚到過久就換 epoch」；
  - 身分接回列為首要決策點（D-2）；
  - L2 要讀的行改成新的 A 級行族。
  - 對照表見計畫第 11 節。

## outage-characterize

### 結果摘要

- **雙向中斷**：大負值目前都被 Client 擋掉，有兩層保護：`settledAfter_`，以及 SeedLead 清空命令環。
- **缺口**：冷卻期內、只有上行、部分送達。
  - 部分送達（延遲 0.5～1 s）時，Client 做了 3～6 次 Late 修正，接著觸發 Backlog 換 epoch。
  - 延遲 3 s 的樣本夾在 −1 s，Client 因為序號超出 64 格環而無聲丟棄。
- **新發現**：
  - 只有上行中斷之後，中斷期間累積的 12 命令領先量要等 Backlog 清掉，約 0.5～1 s。
  - 稀疏替代不論多晚都產生 −1 s 樣本。S3 兩個方向都不成立：
    - 稠密替代時 slack_late < late_first（64 筆上限）；
    - 稀疏且超過 600 序號時 slack_late > late_first。
- **B_fuse 實測**：冷卻期外 32 Tick，只有下行 42 Tick，冷卻期內距上一次換 epoch 61 Tick。
- **語料**：
  - clean 跑次的 snapshot 最大間隔 0.119 s（約 2,700 條串流）；
  - `late_us` 最大 221,470 µs，沒有一筆 ≥250 ms。

### 經過

- **第一版的模擬 Client 是自己寫的簡化模型**。為了讓證據來自產品程式（§14.2），改用產品的 `LocalPlayerPrediction`。兩版在 72 個情境的 Host 結果完全一致。
- **Client 的 Late 修正一度量成 0**。原因是 Client 的 Observation 在換 epoch 時歸零，而部分送達的跑次中途有 Backlog 換 epoch。改成從移動紀錄讀 Late 決定後，量到 3～6 次。這是量測方法的問題，不是產品的缺陷。
- **突變 `oc-client-samples-before-seed` 第一次是 wrong_failure**：拿掉 `settledAfter_` 的過濾後，outage 測試仍然通過，因為 SeedLead 同時清空了命令環。殺掉它的是既有的 2 個測試，所以把期待訊息改成那 2 個測試，並在目錄的說明裡寫明。這不是放寬判定：新突變第一次登記，期待訊息依實際的守衛修正。
- **證據夾**：`build/target/_build/test/logs/pvp-v71-outage-characterize-20261023/`。

## resync-contract

- **決定**：`gate-characterize` 的決定（D173）：T_int 0.5 s、N_o 20 Tick、D-15 (a)。
- **第一版條文**（`02f4b6d`）把計畫第 3 節寫成 §4 的狀態與規則、§7 的不變式。
- **xhigh 對抗式審查**：判定「必須修訂」（Critical 2、Major 8、Minor 12）。主會話對照程式，確認兩個 Critical 成立：
  - **C1**：`SeedLead` 會清空未確認的命令再重新產生（`LocalPlayerPrediction.cpp:72`）。在權威還沒追上 tip 時強制呼叫它，會用不同內容改寫還在途中的序號。這是計畫第 3.1 節第 3 步的設計錯誤：特性測試的 rig 每幀固定 1 Tick，沒有「Match 停頓、只追趕 5 步」的情境，所以沒有量到。
  - **C2**：「只壓掉觸發的那一筆樣本」、slack 下限、S3 等式三者互相矛盾。只有上行、暫存 0.6 s、冷卻期內的情境，一次送達的 34 個晚到裡約一半超過 20 Tick。
- **修訂**：全部處理，和計畫不同的地方寫在計畫第 13 節。最大的兩項：
  - 恢復時權威 < tip 就只重設相位的擷取，不呼叫 SeedLead；
  - N_o 改用時間判斷（值不變），晚到過久的第一次到達一律不產生樣本，計 `late_beyond_no`，S3 改成 `late_first == slack_late + late_beyond_no`。
- **新增的測試要求**（之後的批次）：
  - Match 停頓超過追趕上限時的恢復情境；
  - 冷卻期內暫存 0.6 s 的 slack 下限與 S3；
  - 以跳號製造靜默的改綁攻擊測試；
  - T_int／T_max 兩端相同的 fixture。
- **第二輪 xhigh 複核**（使用者要求）：判定「必須修訂」。
  - 上一輪的 C2、M1、M3、M6、M8 已解決，其餘部分解決。
  - 新的 Critical：只有 Hello 帶 token（`client_v7.proto:8`），Input 與 Actions 只在 `Admit` 比對 endpoint（`session.go:97-105`）。所以「通過 token 驗證的最後時刻」在正常遊玩中，每個 1 s 的 Hello 週期裡約有一半時間會被判成靜默，可以被改綁。主會話對照程式，確認成立。
  - 修正：改成 token 比對或 endpoint 比對都刷新。
  - Major：S3 在 `retention-600` 之前兩個方向都不保證；epoch 相同、權威 < tip 時，領先量留到 Backlog，列為 P2 的第二個例外。
  - Minor 8 項：見計畫第 13 節第 8 點。
- **第三輪 xhigh 複核**（使用者要求）：判定「可送核准」。沒有 Critical 或 Major，Minor 7 項在送 `gate-contract` 之前全部處理。審查者建議先修的是 m1：照字面，從其他位址送來、帶同一個 session id 的封包也會觸發 Gateway 的時鐘視窗重開，可以被外部用來灌爆 A 級日誌。

## drop-counters

- **Host**：
  - 計數放在既有的 slack 發布點，和 `slack_published` 同一處，每個發布的樣本最多計一次；
  - 欄位在 `MatchIngressCounts`（slack 流的計數都在這裡），但不寫進 ingress 行（已有 8 組，log-contract 的 Replacement Criteria 1）；
  - 新行由 `MatchResyncStatisticsWriter` 寫：每個統計視窗、每位玩家一行，沒有 player=0，最後一個視窗以 `final=1` 關閉。
- **Client**：
  - 計數放在 `LocalPlayerPrediction`，`Reset()` 不清除；
  - 模擬 loop 每一步取走並累加。這樣做是因為 `ClientSimulation::SelectArena` 會整個換掉預測物件，計數不能只放在預測物件裡；
  - 連線換代或換玩家時，以及 role 停止時，寫一行 `phase summary`。Lobby（player 0）不寫（第三輪審查 m5）。
- **測試**：
  - 特性測試「64 格環外的樣本」改名為「…dropped and counted as unmatched」，並加上計數 2 的檢查。原本的檢查都保留，只是收緊，照 §14.4 事後回報：
    1. 守的性質：環外的樣本不進相位追蹤；
    2. 放寬面：無；
    3. 收緊面：環外的樣本現在必須被計數；
    4. 不適用；
    5. 突變 `dc-unmatched-*`。
  - 新增 Host 夾限計數（晚到 10／59／61／180 Tick）、resync 行逐字、loop 的換代摘要。
- **已知的失敗**：`object_fps_pvp.worker`（`Invalid Match combat rules`）。這是 `N-worker-probe-stale`，修正 `d2844a5` 只在記錄器分支、不在 master，和本批無關。

## single-store-refactor

- **結構**：
  - 「被替代過、第一次是否已到」只由新元件 `MatchSubstitutionRecords`（`apps/object_fps_pvp/include/RetroFPS/Pvp/MatchSubstitutionRecords.hpp`，MatchRuntimeHost 的 `substitutions_`）記錄。開啟、保留（600 序號）、容量、關閉原因、樣本資格都在這裡。
  - 帳本 `MatchIngressLedger` 只讀每次到達的結果（`Classify`），並把關閉的紀錄做成細節紀錄（`Closed`）；它自己只留不參與判定的附欄（副本數、參考年齡、看著解析的範圍、溢出的序號）。
  - SlackTrack 的 `substituted` 刪除；回看保險拆成 `kSlackLookbackSequences`（64）。
  - 關閉原因的列舉搬到紀錄側的標頭，診斷側引用它，依賴方向變成「診斷 → 紀錄」。
- **過渡規則**（到 `retention-600` 為止）：600 序號內，或屬於最新 64 個還沒到達的候選，就保留；候選老化之後成為「只供樣本」的紀錄，直到到達、被修剪，或 slack track 重來（`Unsample`）。
- **方向守衛**：`test_diagnostic_isolation.py` 新增 `test_the_substitution_records_read_no_ledger`，紀錄的標頭列入 DECIDING。新突變 `v7-flow-ledger-drives-sample`（樣本改跟帳本的分類走）killed。
- **測試**：
  - 帳本的 4 個機制測試改成驅動「紀錄＋帳本」這一對。舊的 49 條斷言原文保留，只把常數名改成 `SubstitutionRetentionSequences`（xhigh 審查核對過）。
  - 新增紀錄的單元測試 2 個、Leave 關閉時保留附欄的 Host 測試 1 個。
- **突變**：
  - 指向舊程式的 17 個改指新位置（意圖相同）；
  - 變動檔案上的 79 個全部重新判定：79／79 killed；
  - 審查找到的缺口新增 3 個：`ssr-unsample-empty`、`ssr-aged-noncandidate-kept`、`ssr-remove-ledger-first`，3／3 killed。
- **xhigh 等價性審查**（紀錄身分與關閉順序）：
  - 不變式推導，加上新舊兩版的差分 fuzz（1380 次執行，涵蓋 join／leave／evict／reset／respawn／epoch 輪替／最遠 1300 序號前的晚到），產品可達的所有觀測值 0 差異。
  - 唯一的不等價是產品走不到的 API 路徑：`TakeIngressStatistics(final)` 之後繼續送輸入。舊版保留 slack 的候選，新版連同紀錄一起關閉。已在介面說明寫明「final 之後不再送輸入」。
  - 其他產品不可達的差異（同一序號再次被替代、容量溢出時的候選）只記錄，並在程式註明前提。
  - 缺口的突變之一「關閉後不刪附欄」在同一身分內不可觀測（序號不重複，換身分時整份重來），列為等價突變，不登記。
- **經過**：
  - 79 個突變的判定還在執行時，我誤以為它已經中斷，用 `git checkout` 還原了它正在套用的突變（`v7-flow-d56-match-key-order`）。那一次的 killed 不可信，之後單獨重跑，killed。其他突變不受影響：工具在每個突變之後都會自己還原。
  - 條文 §7「被替代序號的單一紀錄」原本寫權威是「SlackTrack」。實作後權威的實體是 `MatchSubstitutionRecords`，所以照實作改正 owner 名稱，意圖不變。這是 Design Invariant 的文字變更，交使用者確認。
