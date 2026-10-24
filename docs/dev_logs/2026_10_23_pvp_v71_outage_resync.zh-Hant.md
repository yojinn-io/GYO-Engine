# PvP v7.1 outage-resync：斷線恢復＝重新對時

日期：2026-10-23 起。分支 `claude/pvp-v71-outage-resync`（從 master `cdb9b07`）。

**狀態：進行中**。計畫已核准（D172），`gate-characterize` 已決定（D173）。`resync-contract` 的條文核准（`gate-contract`，D174）。`drop-counters` 完成（L1 run 39956082177 四平台成功）。`single-store-refactor` 完成（L1 run 39967478342 四平台成功）。`match-resync` 完成（L1 run 39974643016 四平台成功）。`retention-600` 完成（判定變更 D176、L2 宣告第 2 版 D177、L2 通過）。`client-interrupted` 完成（對抗式審查後補強）。`identity-rebind` 完成（L1 run 40077174310 四平台成功）。`resync-l2-l3` 的 L2 通過（D178，33 輪全部 P）；L3 移到 `lan15-windows`（D179）。`line-close` 完成。

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
| `single-store-refactor`（`832eca4`、`187d945`） | high，紀錄身分與關閉順序局部 xhigh | 被替代序號的紀錄只存一份：新元件 `MatchSubstitutionRecords`（行為側）持有，帳本只讀到達結果、接收關閉的紀錄，只留附欄；SlackTrack 的 `substituted` 刪除；過渡規則逐位元模擬現狀 | CPU 測試全過；`ctest -L pvp` 44／45（已知的 worker）；`match_ingress_cli`、`authority_digest` 不變；突變 79／79＋4／4；xhigh 等價性審查（差分 fuzz 1380 次 0 差異）；L1 run 39967478342 四平台成功 |
| `match-resync`（`426aa4b` 等） | xhigh（使用者同意） | 晚到超過 N_o（20 Tick＝333,333 µs，Host 時鐘）的第一份副本不產生樣本、計 `late_beyond_no`，並以 `PvpMatch::RequestMovementResync` 在下一個 Tick 換 Resync epoch（不受冷卻、每 epoch 一次、和排定的重設與 LifeRespawn 合併、計入品質 resets）；`resync statistics` 行尾追加兩鍵；特性測試升格為 §7 守衛 | 先 Red（4 個以斷言失敗）再 Green；CPU 測試全過；`ctest -L pvp` 44／45（已知的 worker）；突變 10／10＋受影響的 10 個重判（見下）；L1 run 39974643016 四平台成功 |
| `retention-600`（`d1bcb47` 等） | xhigh（使用者同意） | 拿掉過渡規則：保留中每一筆紀錄的第一份副本判斷一次，600 序號以外關閉的紀錄不再判斷；S3 加進 C++ 守恆檢查 | CPU 測試全過；突變 84／84；§14.4 的 J-A～J-C 經獨立核對修正，使用者核准（D176）；L1 run 40052105536 四平台 L1 job 成功（Windows 的 toolchain Quick acceptance 因 Engine 測試的競態失敗，已由 PR #91 修正）；L2 9／9 P（D177） |
| `client-interrupted`（`d0efbed` 等） | high，計時與恢復路徑局部 xhigh | Client 的連線中斷狀態（`ConnectionLiveness`）、暫時性錯誤、Hello 250 ms、T_max 從 snapshot 起算、恢復流程（clock `reason=resume`、一次性 `resumed` 旗標、`LocalPlayerPrediction::Resume`）、HUD、新的 A 級行 3 個、clock sync 三端 `reason=` | CPU 測試 319／319；`ctest -R object_fps_pvp -L "cpu\|network"` 45／45；Go 通過；突變 72＋7 個全部 killed（見下）；L1 run 40067465114 四平台成功（補強前），補強後 L1 run 40074065448 四平台成功；對抗式審查 1 次與補強（見下） |
| `identity-rebind`（`3e2a242` 等） | xhigh（使用者同意） | 通用層 session 的最後驗證時刻與 `HelloRebinding`（四個條件都成立才改綁、舊 endpoint 隨即 `ErrUnauthorized`）；產品 Gateway 的 A 級 `session rebound` 行、驗證後靜默 ≥ T_int 時以 `reason=resume` 重開每位 Client 的時鐘視窗；`state_whitelist_test.go` 不變 | Go 全部通過；`ctest` 47／47；突變 77 個全部 killed（見下）；L1 run 40077174310 四平台成功 |

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
  - 條文 §7「被替代序號的單一紀錄」原本寫權威是「SlackTrack」。實作後權威的實體是 `MatchSubstitutionRecords`，所以照實作改正 owner 名稱，意圖不變。這是 Design Invariant 的文字變更，使用者核准（D175）。

## match-resync

- **§14.3 的順序**：先加介面空殼（一律回 false 的 `RequestMovementResync`、`Resync` 列舉、計數欄位、常數），寫 6 個守衛測試，其中 4 個以斷言失敗（Red），再實作到 Green。合併相關的 2 個在空殼下本來就會通過，實作後才有實際意義（有突變守著）。
- **實作**：
  - Host：第一份副本的晚到用 Host 時鐘量（和 slack 樣本同一個讀數），超過 N_o 就不產生樣本，計 `late_beyond_no`，並向 PvpMatch 要求 Resync。
  - PvpMatch：Resync 排在下一個 Tick 邊界；不受冷卻；已排定 Starvation／Backlog 時回 false（由已排定者代替）；同一邊界的 LifeRespawn 會清掉它；`resets` 與新的 `resyncs` 都計。
  - Host 由品質的 `resyncs` 差分得到每個視窗的 Resync 數。
- **特性測試的值改變了**（升格為 §7 守衛時寫明，這是 D172／D174 宣告的行為改變）：

| 測試（情境） | 之前 | 之後 |
|---|---|---|
| two-way：冷卻期內暫存 0.6 s | 不換 epoch，最小 slack −566,678 µs，stall reseed 1 | 第 37 Tick Resync，沒有負樣本 |
| one-way：只上行、冷卻期內暫存 0.6 s | 不換 epoch，−566,678 µs 一個 | 第 37 Tick Resync，沒有負樣本 |
| partial：每 8 個準時 1 個、延遲 0.5 s | 第 68 Tick Backlog，Client 的 Late 決定 4 次 | 第 31、63 Tick Resync，Late 0 次 |
| partial：每 8 個準時 1 個、延遲 3 s | 不換 epoch，−1 s，late_first 187／slack_late 64 | 第 181、365 Tick Resync，沒有負樣本，2／0 |
| partial：每 4 個準時 1 個、延遲 1 s | 不換 epoch，75 個負樣本 | 4 次 Resync，沒有負樣本 |
| sparse：100～700 Tick 晚到 | 每次 −1 s 樣本 | 沒有樣本，`late_beyond_no` 1，Resync |

  其餘情境（雙向丟失、只下行、只上行丟失）不變。

- **另外兩個既有測試的判定改變**（§14.4，都不是 Design Invariant 的守衛，也不是受入判定）：
  - **`PvP host ledger keeps late arrivals beyond the slack sample bound…`（v7 08c）**：
    - 為什麼改：晚 1.2 s 的副本現在會觸發 Resync，之後的副本變成 EpochOld；
    - 怎麼改：把兩份副本的順序對調（先送 5 ms 晚的 140，再送 1.2 s 晚的 2）；
    - 斷言變化：「最後一個視窗以 end 關閉 70 筆」改成「Resync 以 epoch 關閉 69 筆」（少跑一個 Tick，替代 71 筆、到達 2 筆）；
    - 不變的部分：late_first 2、不產生樣本、J5a 守恆。
    1. 守的性質：64 筆以外的晚到記成 late_first、不成為樣本，紀錄恰好關閉一次；
    2. 放寬面：無，原本的性質都還在檢查；
    3. 收緊面：新增 `late_beyond_no == 1` 與 epoch 改變；
    4. 不適用；
    5. 突變 `v7-flow-ledger-retention-64` killed。
  - **`PvP host counts the published slack samples the clamp changed`（drop-counters）**：
    - 為什麼改：晚到 61／180 Tick 現在超過 N_o，不產生樣本，負方向到不了夾限；
    - 怎麼改：改用「Host 停頓 2 s」讓正方向的樣本被夾在 +1 s，計數 1；10 Tick 的負樣本照舊不計。
    1. 守的性質：被夾限改變的發布樣本會被計數；
    2. 放寬面：負方向的夾限不再測，因為產品裡已經到不了；
    3. 收緊面：正方向的夾限首次被測到；
    4. 計數這條性質，由正方向的情況繼續守著；
    5. 突變 `dc-clamped-*`、`oc-slack-unclamped` killed。
- **突變**：
  - 本批 10／10 killed：觸發的仍產生樣本、只壓掉要求成功的那一筆、用序號差判斷、N_o 差一、受冷卻限制、覆蓋已排定的重設、不計入品質、respawn 之後還留著 Resync、Host 不計 resyncs、不計 late_beyond_no。
  - 期待訊息指向被改測試的 10 個重判：6 個 killed；`v7-flow-ledger-end-not-closed` 與 `oc-slack-unclamped` 改指現在的判定後 killed。
  - `v7-flow-ledger-drives-sample` 在 N_o 以內變成等價（N_o 以內「帳本判成 late_first」和「是樣本候選」在產品裡相同）。因為它是 §7 列出的守衛，改成「晚到過久的判斷改跟帳本的分類走」，killed。
  - `oc-slack-track-retains-600` 退役：64 筆以外的候選一定已晚於 N_o，所以 64 這個上限在行為上看不出差別，而且 `retention-600` 會整段移除這段程式。
- **已知**：持續的部分送達會反覆 Resync（例如每 4 個準時 1 個、延遲 1 s 時，5 s 內 4 次），品質視窗因此失敗，持續 3 個視窗就會被逐出。這是 D172 的 D-10「照算」。

## retention-600

- **match-resync 之後剩下的改變**：
  - 樣本這一面不變：N_o 以內的晚到，比它新的被替代序號最多約 30 個，一定在最新 64 筆之內（獨立核對的計算）；
  - 改變的是 600 序號以外的晚到：以前以「只供樣本」的紀錄保留，到達時算 `late_beyond_no` 並 Resync；現在紀錄已關閉，什麼都不判斷。
  - 計畫原本預期 `:1363` 的判定會反轉，現在不會反轉，維持 CHECK_FALSE。
- **程式**：`MatchSubstitutionRecords` 拿掉候選集合、只供樣本的紀錄、`PruneSamples`、`Unsample`，以及 `SlackSampleRecords`；Host 只看 `arrival.first`。
- **S3**：`CheckSlackConservation` 加進每一個 `CheckSubstitutionConservation`。
- **判定變更**：見 [retention-600-judgment-change.md](../object_fps_pvp/plans/v7.1/retention-600-judgment-change.md)。
  - J-A：稀疏替代的守衛，610／700 Tick 不再 Resync；
  - J-B：紀錄的單元測試，換成新規則；
  - J-C：`v7-flow-ledger-drives-sample` 的改寫。
  - 獨立核對（另一位審查者，只讀 commit）指出措辭錯誤與漏報（J-C、退役的記錄），都已修正。
- **突變**：
  - 退役 2 個：`ssr-unsample-empty`、`ssr-aged-noncandidate-kept`。它們改的程式已刪除；single-store-refactor 那一節寫的「3／3 killed」是當時的結果。
  - 改寫：`v7-flow-ledger-late-copies-undercount`（find 改指新程式）、`v7-flow-ledger-drives-sample`（J-C；`expect` 先放寬，結果出來後收窄成實際殺掉它的 §7 守衛，killed）。
  - 變動檔案上的 84 個：84／84 killed。
- **補記**：match-resync 那一節對 `v7-flow-ledger-drives-sample` 的描述（「晚到過久的判斷改跟帳本的分類走」），是 match-resync 時的版本；retention-600 之後的版本見 J-C。
- **未做**：L1（等核准後與 L2 一起）、L2 不退步紀錄（宣告草案在判定變更文件第 6 節）。
- **L1（run 40052105536，`1713191`）**：
  - 四平台的 L1 job 都成功；
  - Quick acceptance / toolchain / windows-x64 失敗：Engine 的 `platform_sdl_concurrency` 中 `SdlLogBridge keeps every line under concurrent logging`（`tests/common/platform_sdl/LogBridgeTests.cpp:172`），8 個執行緒的 `pushedBy` 全部是 0。
  - **歸類**：〔推論〕Test 自身的競態，還沒有重現。主執行緒的 200 輪沒有等記錄執行緒開始，就把 `stop` 設成 true；Windows 的執行緒啟動延遲讓每個執行緒一行都還沒記錄。這和 IP-4 時修過的 Windows 並行測試沒有交握是同一類問題。
  - 本線沒有改到 Engine。這次的失敗記錄在這裡，不以之後的成功覆蓋（§14.1）。
- **L2 的準備**：
  - 工具：`run_r600.py`、`judge_r600.py`、凍結清單、artifacts；
  - 試跑 3 次，都判 P；
  - D45 檢查：小規模 ultracode，判定「還不能開跑」，必修 3 項、建議 7 項，都已處理；
  - judge 的竄改測試 9／9 判對。
  - 宣告第 6 節依此改成第 2 版，新增有效性前提（E）、J5a 的範圍、更嚴格的 runtime、旗標（FLAG），並更正 runner 名稱。新增的部分等使用者確認。
- **L2**（2026-10-24，session `pvp-v71-retention-600-l2-20261024`，宣告第 2 版 D177，檔位 medium）：**通過**，9／9 P，沒有 E 或 FLAG。
  - x1、ramp、gateway-250ms 各 3 次，S1、S2、J5a、S3、runtime 都成立；resyncs、late_beyond_no、slack_clamped 都是 0。
  - x1、ramp 的換 epoch 組合和語料相同。
  - 細節見判定變更文件第 7 節。
- **Engine 測試的競態**：L1 Windows 失敗的 `SdlLogBridge keeps every line under concurrent logging`，由另一個會話以 PR #91 修正並合併進 master（`253808a`）：加上開始的交握；注入 50 ms 延遲 3／3 Red → 20／20 Green。本線之後和 master 同步時帶進來。

## client-interrupted

檔位：high，計時（`ConnectionLiveness`）與恢復路徑（`LocalPlayerPrediction::Resume`、旗標的 mutex 區段）局部 xhigh（使用者 2026-10-24 指定照計畫）。開始前把本線和 master 同步（`4d40ab8`，帶進 PR #91）。

### 做了什麼

照 network-architecture §4「連線中斷與恢復」的 Client 部分：

- **`ConnectionLiveness.hpp`（新元件）**：只收到達時刻與 `Movement.hpp` 的兩個常數（T_int＝500 ms、T_max＝5000 ms）。
  - 收到 Welcome 之前：從 join 起 T_max。
  - 第一個 Welcome 之後、第一份 snapshot 之前：從第一個 Welcome 起 T_max，之後的 Welcome 不延長。
  - Playing：最後一份有效 snapshot 起 T_int 進入中斷（只報一次）、T_max 斷線。
  - 列入 `test_diagnostic_isolation.py` 的 DECIDING 清單。
- **網路 worker（`ClientConnection.cpp`）**：
  - 中斷期間保留 socket、憑證、傳輸序號、動作帳本，Hello 改成 250 ms；`ClientConnectionState` 帶 `interrupted`、`interruptedSince`（只供顯示）。
  - 暫時性 socket 錯誤 5 種只計數，判斷寫成公開的 `TransientSocketError`（兩種 error category 都認得）。
  - 恢復：先以 `reason=resume` 提前結束 clock sync 視窗，再走既有的 epoch 路徑；交給模擬端的一次性 `resumed` 旗標和那份 snapshot 在同一個 mutex 區段設定，連線換代的三個邊界清除。
  - 新的 A 級行：`connection interrupted／resumed／summary version=1`。
- **相位追蹤**：`LocalPlayerPrediction::Resume`。游標＝tip 時 SeedLead；游標＜tip 時不動未確認的命令，只重設擷取（`RestartPhaseAcquisition`，和 SeedLead 共用），寫 `phase_path=resume_reset`、`count=0`、`phase_shift_seconds`＝取消的剩餘修正。`ClientSimulation::Observe` 在處理同一批樣本之前呼叫它。
- **clock sync 行（三端）**：行尾追加 `reason=window／final／resume`（`ClockWindowEnd`）；`clock-sync.tsv` 加一欄。Gateway 目前只會寫 window／final。
- **HUD**：「CONNECTION INTERRUPTED - reconnecting (N.Ns)」。
- **常數兩端綁定**：`fixtures/connection_timing.json`，C++（`ConnectionLivenessTests.cpp`）與 Go（`connection_timing_test.go`）都讀。Go 新增 `connectionInterruptedAfter`，給 `identity-rebind` 用。
- **速率註解**（`Movement.hpp`）：82＋30＋4＝116，低於 Gateway 的 120。

### 和計畫不同的地方

- **Gateway 每位 Client 時鐘視窗的重開**移到 `identity-rebind`：條文規定它以「最後驗證時刻」判斷，而這個欄位是 Delta-3 的通用層新欄位；它的守衛也列在 `identity-rebind`。本批只在 Gateway 加 `reason=`。
- **凍結讀者的先查結果**（log-contract §1.4 要求）：`command_evidence.py` 只接受 first／window／late，而且要求 `count ≥ 1`，條文的 `resume_reset`、`count=0` 會被當成損壞。repo 裡的讀者只多接受這一種組合（`resume_reset` 必須 `count=0`，其他路徑照舊 `≥1`），附反向測試與突變。先前各 L2 的凍結副本讀不到含 `resume_reset` 的紀錄，`resync-l2-l3` 宣告時要重新凍結。
- **worker 測試不檢查 `resumed` 旗標**：既有守衛 `test_probe_command_path.py` 禁止 probe 呼叫 `DrainSimulation`，我不放寬它。旗標改用兩個靜態守衛：「相位追蹤的檔案不提中斷狀態」與「旗標和 snapshot 在同一個 lock 區段」（`test_diagnostic_isolation.py`）。旗標「只交一次、換代清除」沒有執行期的測試。
- **§7 守衛清單的兩項，做法和字面不同**（Design Invariants 的守衛，待使用者確認）：
  - 「差分 oracle（中斷判定逐位元相同）」：worker 是真實時間，事件時刻每次不同，做不到逐位元。改成比較每次中斷的 6 項結果（中斷、保留 session、探測、恢復、傳輸序號連續、T_max 斷線）必須相同；突變「用時鐘回聲平移 snapshot 到達時刻」被它殺掉。`ConnectionLiveness` 的介面本身不收任何時鐘值。
  - 「競態：drain 落在放進 snapshot 與設定旗標之間」：沒有可注入停頓的接縫，改成靜態守衛（旗標在放進模擬端歷史的同一個 lock 區段內設定，中間沒有其他 lock）。
- **暫時性錯誤的呼叫點**沒有執行期的測試：macOS／Linux 的未連線 UDP socket 不回報 ICMP 錯誤，造不出來。分類函式有單元測試與突變；Windows 的 WSAECONNRESET 照計畫在 L3 確認。

### 驗證

- 突變：本批新增 11 個、和 Client 端既有的 61 個（`ClientConnection.cpp`、`LocalPlayerPrediction.cpp`、`ClientSimulation*.cpp`、`ClockSync.hpp`）一起跑，72 個中 70 個 killed、2 個 wrong_failure：
  - `lc-clock-cpp-key`：被 golden 測試抓到，但預期文字是 `f[11]`；fixture 加了一欄後是 `f[12]`，改預期文字。
  - `ci-interrupt-resets-actions`：`ResetActions` 也清掉戰鬥規則，snapshot 全被丟掉，worker 測試在等恢復時逾時。改成中斷段的每項確認失敗時就回報，並把中斷段移到 worker 測試最前面（mock 只在要求時送 snapshot，後面的段落也會經過中斷）。
  - 改完後 worker 類的 25 個全部重跑：25／25 killed。
  - 移動位置的既有突變 3 個（`v7-09b-seedlead-partial-clear`、`v7-09b-seedlead-keeps-reacquiring`、`lc-clock-session-player-cleared`）只更新 find 字串，判定不變。
  - 證據：`build/target/_build/test/logs/pvp-v71-client-interrupted-mutants-20261024/`、`-rerun-20261024/`、`-rerun2-20261024/`。

- worker CTest（真實 socket＋mock Gateway）新增 `RunOutage`：雙向靜默 → T_int 中斷（保留 session、動作、傳輸序號連續、Hello 每秒 3～5 次）→ 恢復 → 只有 Welcome 時從最後一份 snapshot 起 5 s 斷線；同一段以極端時鐘回聲再跑一次，結果必須相同（差分 oracle）。worker CTest 的 TIMEOUT 25 → 60 s（這段約 13 s）。
- 新的 cpu 測試：`ConnectionLivenessTests.cpp` 7 個、`PredictionTests.cpp` 2 個、`ClientSimulationTests.cpp` 1 個、`PvpMatchTests.cpp` 1 個（Match 停頓超過追趕上限、epoch 相同、權威＜tip 時恢復：在途命令不變、沒有 ConflictQueued／ConflictStaged、沒有恢復前命令的樣本）、`ClockSyncTests.cpp` 1 個。
- 本機：cpu 測試 316／316；`ctest -R object_fps_pvp -L "cpu|network"` 45／45；Go `./...` 通過。
- 本線先帶進記錄器分支 `d2844a5` 的 worker probe 修正（`3547c05`）：沒有它，worker CTest 在本線上會因 mock 的 Welcome 缺欄位而失敗（drop-counters 以來記錄的「已知的 worker」）。

### 對抗式審查與補強（2026-10-24）

使用者要求跑一次小規模對抗（xhigh 審查者 1 位，只讀）。審查者拿到 diff 與條文，不附實作者的結論。結果：Critical 0、Major 2、Minor 6，主會話逐項對照程式核實。實作符合 §4，問題在守衛：有幾種改壞的寫法能通過所有測試。使用者核准補強計畫（`eeb8207`、`08f490a`）：

- **M1 旗標的傳遞沒有執行期守衛**：模擬端的歷史與一次性旗標抽成 `ClientSimulationHistory.hpp`，旗標只能和它的 snapshot 一起 `Push`。單元測試驗證「只交一次、Clear 清除、溢出計數」；模擬 role 的測試用既有的 drain `transform` 設一次 `resumed`，斷言只寫一筆 `resume_reset`，之後照常回到 Tracking。換代時清旗標的三處是等價突變（新一代先 `Reseeds`，不會走到 `Resume`），寫在型別的註解。
- **M2 差分 oracle 只抓得到粗的洩漏**：見下方 §14.4。
- **m1**：中斷前送一個窗口，斷言恢復後 worker 重送的是同一個 epoch、同樣的 3 個命令（等不到重送也算失敗，最多等 500 ms）。
- **m2**：補 asio（system category）的錯誤碼；靜態檢查兩個 Failure 的丟例外處前面都有暫時性錯誤的判斷。理由改正：Windows 的未連線 UDP socket 會在下一次 `recvfrom` 回報 WSAECONNRESET，所以只是本機（macOS／Linux）造不出來；worker 測試不在 CI 的 L1 範圍（network 標籤），Windows 的實際行為留給 L3。
- **m3**：暫時性錯誤只在 Playing（連線與中斷狀態）適用，握手期照舊立刻 Failure，和條文一致。
- **m4**：收包遇到暫時性錯誤改用 `continue`，同一輪繼續讀後面的封包。
- **m5**：直接超過 T_max 的逾時（worker 卡住、睡眠）也先記一次中斷、寫中斷行。
- **m6**：`resume_reset` 的寫出端 golden（`DiagnosticFileTests.cpp`）。
- **只記錄、不改**：寫新 A 級行的呼叫點沒有執行期守衛（m6 的另一半）；在 tip 恢復的 SeedLead 沒有移動紀錄（m7）；取消的 join 也寫摘要，和既有的時鐘行一致（m8）；握手期與直接逾時這兩個新分支沒有執行期測試。
- **突變**：補強新增 7 個，`ci-liveness-reads-time-echo` 改成 500000 的餘數（平移約 0.24～0.37 s，舊 oracle 會放過）。補強後跑 48 個（本批與 `ClientConnection.cpp`、`ClientSimulationRole.cpp` 的既有突變）47 killed、1 wrong_failure（`ci-resume-drops-window`：等重送逾時，改成有時限後判定）；之後 worker 類 28 個全部重跑，28／28 killed。證據：`pvp-v71-client-interrupted-mutants-reinforce-20261024/`、`-worker3-20261024/`。
- 本機：cpu 測試 319／319；`ctest -R object_fps_pvp -L "cpu|network"` 45／45；worker probe 連跑 3 次通過，兩次 run 的中斷延遲 500.15～500.25 ms、斷線 5000.18～5000.30 ms。

#### §14.4：差分 oracle 的判定變更（Design Invariants 的守衛；2026-10-24 使用者核准）

1. **守護的性質**：Client 的中斷與斷線只由到達時刻和兩個常數決定，時鐘回聲、offset、delay 不進判定（§7「中斷判定只讀到達時刻與常數」）。
2. **放寬的面**：沒有。新判定在每一項都同時要求初版的條件：中斷在靜默之後 ≥ T_int−5 ms、斷線在最後一份 snapshot 之後 ≥ T_max−5 ms，以及初版的 6 項真假值相同。所以初版判失敗的情況，新判定全部也判失敗。
3. **收緊的面**：
   - 中斷要在 mock 送出最後一份 snapshot 後 [T_int−1, T_int+100] ms 內被偵測（初版只有等待上限 1.5 s）；
   - `interruptedSince` 必須和最後一份 snapshot 的送出時刻相差 ≤ 20 ms；
   - 斷線在 [T_max−1, T_max+100] ms 內（初版上限 +600 ms）；
   - 兩次 run 的中斷與斷線延遲相差 ≤ 50 ms；
   - 極端模式改送有效、offset 約 2⁵² µs 的樣本（初版的樣本全部無效，從未進入 ClockSync 的計算）；
   - 新增 worker 窗口的保留；
   - 另加靜態檢查：`liveness.*` 的參數只能是到達時刻、`Clock::now()` 或 `polledAt`。
4. **（沒有放寬的面，不適用）**。
5. **根據與證據**：審查報告 M2（`%500000` 的突變在初版下存活的推算）；補強後該突變被 worker 判定與靜態檢查都殺掉（上方突變證據）；worker probe 輸出 `outage_interrupt_ms`、`outage_timeout_ms`；極端 run 的 clock sync 行 `samples=1`、`offset_us=4501769046661084`。

#### §14.4 形式：`command_evidence.py` 多接受 `resume_reset`（分析器的判定變更）

1. **守護的性質**：移動紀錄的 `phase_decision` 只有已知的路徑，而且每次決定至少用到 1 個樣本。
2. **放寬的面**：`phase_path=resume_reset` 且 `count=0` 從「損壞的紀錄」改成可讀。
3. **收緊的面**：沒有。
4. **其他判定是否仍守住**：其他路徑照舊要求 `count ≥ 1`（反向測試 `{"count": 0}`）；`resume_reset` 配 `count=1` 被拒絕（新的反向測試）；寫出端的名稱與欄位由 `DiagnosticFileTests.cpp` 的 golden 固定。
5. **根據**：log-contract §1.4 的條文（D174）規定這種紀錄；突變 `ci-reader-rejects-resume-reset` killed。已凍結的副本讀不到它，`resync-l2-l3` 宣告時重新凍結。

## identity-rebind

檔位：xhigh（使用者 2026-10-24 同意，照計畫）。

### 做了什麼

照 network-architecture §4「連線中斷與恢復」的 Gateway 部分（Delta-3）：

- **通用層 `session`**：
  - 新欄位「最後驗證時刻」`lastVerified`：已綁定 endpoint 的 token 相符 Hello、`Admit` 的 endpoint 比對通過時刷新，不論序號、payload、限速（刷新在限速之前）。其他 endpoint 的資料包不刷新它。
  - `HelloRebinding(token, peer, sequence, now, Rebinding{Silence, Timeout})`：新 endpoint 的 Hello 在四個條件都成立時改綁（token 正確、序號較新、原 endpoint 靜默 ≥ Silence、未到期），判斷在任何刷新之前。改綁後原 endpoint 是 `ErrUnauthorized`。
  - `Hello` 維持原樣（規則為零，不改綁），`LastVerified()` 供產品讀取。
- **產品 Gateway**：
  - Hello 走 `HelloRebinding`，規則在呼叫處給 T_int（`connectionInterruptedAfter`）與 T_max（`sessionTimeout`），不新增套件變數，`state_whitelist_test.go` 不變。
  - 每次改綁寫 A 級的 `session rebound version=1 player= old_port= new_port= silent_ms=`（只記埠，不記 token 與完整位址）。
  - 時鐘：Hello 與 `admit` 前後各讀一次最後驗證時刻。驗證通過、而且和前一次相隔 ≥ T_int 時，以 `reason=resume` 提前結束該 Client 的時鐘視窗（`echo.go` `noteVerified`，`clockSync.resume`）。沒有通過驗證的資料包不刷新、不重開。
- **clock-sync.tsv** 加一列 `resume`，C++ 與 Go 兩端寫出相同的本文。

### 決定（契約交給本批的）

- **Hello 間隔維持 1 s**，不縮到 T_int／2 以下。契約的安全取捨已寫明：只送 Hello 的期間（收到 Welcome 後、第一份 snapshot 前；沒有新命令、窗口全部確認時），最後驗證時刻有一半時間超過 T_int，拿到 token 又能讓原 endpoint 靜默的人可以改綁。Client 的模擬端每個 Tick 都產生命令，Playing 期間 60 Hz 的 Input 一直刷新它，所以這段只出現在握手後的短時間。縮短 Hello 會改變既有 L2 的 Hello／Welcome 計數基線；使用者指示保住基線、不擴大範圍，所以只記錄。

### 驗證

- Go：`services/gyo_gateway/session` 新增 4 個測試（改綁與鎖住舊 endpoint；條件少一項就拒絕 5 種；原 endpoint 以跳號封包、只送 Input、stale 與限速的資料包都算驗證；其他 endpoint 不刷新）。產品 Gateway 新增 2 個（改綁的端到端與 A 級行、時鐘視窗只在驗證後的靜默重開：60 Hz Input＋每秒 Hello 不重開、其他 endpoint 不重開也不刷新、靜默 0.8 s 後恰好重開 1 次）。
- 突變：本批 9 個，加上 Gateway 的既有突變（`session.go`、`echo.go`、`clock_sync.go`、`server.go` 的 go／gateway_session 類）共 77 個：72 killed、4 wrong_failure、1 timeout。
  - 3 個 wrong_failure 是預期文字的問題：Go 把子測試名稱的空白換成底線。改預期文字。
  - `ir-clock-unverified` 的替換讓變數沒被用到，Go 編譯失敗。改成仍用到它的寫法。
  - 既有的 `v71rl-credit-unbounded` 逾時：新測試「送到被限速為止」的迴圈沒有上限。改成最多 1000 次，送不到限速就失敗。
  - 這 5 個重跑 5／5 killed。證據：`pvp-v71-identity-rebind-mutants-20261024/`、`-rerun-20261024/`。
- 本機：Go 兩個模組全部通過；`ctest -R object_fps_pvp -L "cpu|network|go"` 47／47。

## resync-l2-l3

檔位：medium，工具 high；門檻來源的對抗式檢查用小規模 ultracode（計畫）。

- **注入工具** `build/acceptance/object_fps_pvp/run_outage.py`（`11ed8a6`、`54c0022`）：
  - 照 `run_client_delay.py` 的方式，凍結的 `action_probe.py`、`impaired_network.py` 原樣匯入，每一輪只在程序內替換 `ActionRelay`，既有情境的位元組不變。
  - 種類：both、uplink、downlink、partial、port、none。
  - 單元測試 9 個（CTest `outage_runner`）。
- **驅動與判定**：`run_resync_l2.py`、`judge_outage.py`，放在 git 忽略的證據目錄（同 retention-600）。sha256 寫在宣告第 1 節。
- **L2 宣告**：[resync-l2-l3-declaration.md](../object_fps_pvp/plans/v7.1/resync-l2-l3-declaration.md)。
  - 第 1 版經 D45（`wf_603e73b4-cb9`：程式、證據、反駁各 1 位）判定「正確的程式也幾乎一定會停」：Critical 1（C8 造不出晚到過久）、Major 7、Minor 6。全部處置後成為第 2 版（宣告第 7 節）。
  - 新增 J-R9（恢復後回到遊玩），J-R3 改成只記錄（L2 造不出 `resume_reset`）。
- **乾跑**（不作判定）：11 項各 1 輪全部 P。途中修正驅動的相對路徑，以及判定工具裡太寬的重生前提。
- **竄改檢查**：判定工具 10／10 判出預期的 F／E。
- **探索紀錄**：工具開發期間的 17 輪（5 個工具版本），只用來確認注入有效。
- **只記錄的觀察**：換來源埠時，Gateway 在改綁前仍把 snapshot 送往舊埠，Client 先恢復，改綁要等下一個每秒一次的 Hello（約 2 s）。
- **L2**（D178：使用者核准宣告第 2 版）：session `pvp-v71-resync-l2-20261024`，33 輪全部 P。
  - 中斷行 `gap_ms` 全部 500；C9 的 T_max 4983.6～4999.5 ms；恢復後 3 s 內被接受的命令 130～191 個；改綁每位玩家 1 次。
  - J-R3（只記錄）只在只上行的 C4、C5 出現，是 §7 已知的 P2 例外，見宣告第 9 節。
- **L3**：D179 移到 after-re-evaluate 的 `lan15-windows`，內容寫在該批的驗收欄。宣告第 2 版改寫時漏掉了第 1 版的 L3 一節，以該欄為準。


## line-close（不依賴 L3 的部分，2026-10-24）

檔位：medium。本線同步 master（含 PR #93，記錄器分支已合進 master）。記錄器的文件（HANDOFF、README、after-re-evaluate、v8 README）更新在本線，隨 PR #90 合併。

### Fitness Check（AGENTS.md §12，對 master 的差異：65 個檔案）

- **Dependency**：
  - Engine 沒有任何改動。
  - 新增的依賴邊有兩條：
    - Host → PvpMatch 的排程介面 `RequestMovementResync`（Delta-2，方向和既有的 Host → PvpMatch 相同）；
    - 產品 Gateway → 通用層 session 的 `HelloRebinding`／`LastVerified`（Delta-3，產品依賴通用層，方向不變）。
  - 沒有循環依賴，CMake 的依賴圖沒有變。
- **Ownership**：
  - 被替代序號只由 `MatchSubstitutionRecords` 持有（Delta-1，D175），帳本只讀。
  - 中斷狀態由 Client 的網路 worker 持有，相位追蹤只收一次性的旗標（`ClientSimulationHistory`），有靜態守衛。
  - endpoint 與最後驗證時刻由通用層 session 持有。
  - Gateway 的 `state_whitelist_test.go` 沒有改，沒有新增 Gateway 狀態。
- **Product Removability**：
  - 新檔案都在 `apps/object_fps_pvp`、`tests/object_fps_pvp`、`build/acceptance/object_fps_pvp`。
  - 通用層 session 的新介面不含產品名稱：T_int、T_max 由產品以 `Rebinding` 傳入。
  - 刪除產品不需要改公共層。
- **Editor／Runtime Separation**：不涉及。
- **Architecture Concepts**：沒有新的 top-level 目錄。新元件 4 個，都在產品內：`MatchSubstitutionRecords`、`ConnectionLiveness`、`ClientSimulationHistory`，以及驗收工具 `run_outage.py`。

### Architecture Report（AGENTS.md §15）

- **Architecture Delta**（D172 D-14 同意）：
  - Delta-1：被替代序號的單一紀錄（`single-store-refactor`、`retention-600`）。
  - Delta-2：連線中斷狀態與重新對時的契約化。包含：
    - `ConnectionLiveness`；
    - Resync 這個換 epoch 的原因與 Host → PvpMatch 的排程介面；
    - `LocalPlayerPrediction::Resume`；
    - 一次性的恢復旗標。
  - Delta-3：session 的 endpoint 重新綁定與最後驗證時刻。
- **新的依賴**：見上方 Dependency。
- **Data Contract 的變更**（wire 留在 pv7）：
  - 新的 A 級行：`resync statistics`、`phase summary`、`connection interrupted／resumed／summary`、`session rebound`。
  - clock sync 行三端都加了 `reason=`。
  - schema 3 的 `reset_reason=resync` 與 `phase_path=resume_reset`，凍結讀者 `command_evidence.py` 只多接受後者。
  - 共用 fixture `connection_timing.json`。
- **Design Invariants**：network-architecture §7 的「連線中斷與重新對時」與「被替代序號的單一紀錄」（D174、D175），附 Replacement Criteria。本線期間沒有觸發任何一條 Replacement Criteria。
- **發現的 Code Smell（只記錄）**：
  - 換來源埠時，Gateway 在改綁之前仍把 snapshot 送往舊埠，Client 因此先恢復，改綁要等下一個每秒一次的 Hello（約 2 s）。
  - Hello 間隔維持 1 s，所以只送 Hello 的期間有改綁的空窗（契約的安全取捨）。
  - 凍結的 `run_case` 在 probe 失敗後立刻結束服務，L2 看不到 Gateway 端的 session 到期。
  - 寫出新 A 級行的呼叫點沒有執行期守衛。
  - 在 tip 恢復時的 SeedLead 不留移動紀錄。
- **將來的 Refactoring 候選**：
  - v8 B6：P2 的兩個已知例外（多出的領先量留到 Backlog 清掉），基準要在本線合併後重新取得（已寫進 v8 README）。
  - `identity-rebind` 的 Hello 間隔。
  - I-3（防竄改的 session 證明，pv8）。

### 收尾

- **L3**：D179 移到 after-re-evaluate 的 `lan15-windows`（預定 2026-10-26 前後），內容寫在該批的驗收欄。
- **已知項目**：Windows 的 WSAECONNRESET（D172 D-7 的推論）在 `lan15-windows` 之前未經實機確認。
  - 已確認：分類函式在 windows-x64 的 L1 上通過；兩個會讓連線失敗的位置都先做暫時性判斷（靜態守衛）。
  - 未確認：拔線或關 Wi-Fi 時，Windows 實際丟出的是不是這幾種錯誤。
  - 猜錯時：Client 立刻回 Lobby，也就是改版前的行為，不會比以前更差。
- PR #90 改成 ready，跑全量 L1，使用者確認後以 `gh pr merge --merge` 合併。
