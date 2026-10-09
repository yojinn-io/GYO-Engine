# 第 07 批：Match 的 Tick 改用 Waiter，IpcHost 改用 asio

狀態：**完成**（2026-10-09；A `ffe9a99`、B `10003b9`；L2 通過）。PR 線 P2（PR #73）。依賴第 06 批（`7da6b0d`，本批的 before 是第 06 批的頭）。
建議檔位 high；重設的交接、停止與 join 的順序、寫出的選擇（未開始的 snapshot 可替換）局部 xhigh。本批沒有另開 xhigh 審查 agent（需要使用者同意）。

## 起因

- Match 的 Tick 用相對的 `condition_variable_any::wait_for`（`MatchRuntimeHost.cpp:294-295`），等待從 Advance 之後才開始算，又有 macOS 計時器合併的 leeway。P2-log 量到晚醒以 2～4 ms 為主（含 Advance）。第 06 批 L2 的後兩輪，主機進入 8 ms 狀態時 `legal_match_p95_ms` 約 10→19～24 ms。
  - 更正（2026-10-09，核對）：「主機進入 8 ms 狀態時 `legal_match_p95_ms` 約 10→19～24 ms」在第 06 批 L2 第 5、6 輪的 8 個 clean 跑次中有 6 個成立；clean-60 before 是 11.1／14.4 ms（見第 06 批的更正）。`MatchRuntimeHost.cpp:294-295` 是 `660310e` 的行號，本批的 before（`dcc14be`）是 `:298`。
- IpcHost 是一條輪詢執行緒：每輪睡 1 ms（`IpcHost.cpp:331`）、accept 每 5 ms 重試（`:341`）、重設的 future 每 10 ms 輪詢（`:344`、`:349`）。P2-log 量到約 787 次／秒的迭代。
- 模擬執行緒由 `match_main` 的 `std::jthread` 建立（`match_main.cpp:54`），IpcHost 也用 `std::jthread`。

## 範圍

### A. Match 的模擬角色

- `MatchRuntimeHost` 自己擁有模擬執行緒（Engine 的 `GYO::Threads` 角色，名稱 `gyo-match-sim`），和 Client 的模擬角色相同：`Start`／`Stop`，解構時停止並 join。`Advance` 維持公開，給決定性的測試。
- 每一步：取樣時刻 → `Advance` → 以「取樣時刻＋`secondsUntilNextTick`」為絕對期限，用角色的 Waiter 等待。重設要求以 `Notify` 喚醒，停止由角色的停止要求喚醒。條件變數移除。
- Tick 的晚醒照 P2-log 的定義記錄（預定＝取樣時刻＋`secondsUntilNextTick`）；現在等待是絕對的，所以晚醒不再包含 Advance 的時間。
- 發布通知：Advance 產生 snapshot、Join／Leave 的結果、驅逐，或完成重設時，在鎖外呼叫一次 IpcHost 設定的通知（只做 post，不阻塞）。
- `snapshot_` 槽在被取走前就被新的覆蓋時計數，寫進 Match 的 `network statistics` 行（行末新增 `snapshot_overwrites=`，原有欄位不動）。

### B. IpcHost 改為一條 asio io 執行緒

- 一條角色執行緒（`gyo-match-ipc`）執行 `io_context`。accept、讀、寫、計時器都是非同步的；1／5／10 ms 的輪詢全部移除。
- 狀態：接受連線 → 等重設完成 → 連線中 → 關閉並等重設完成 → 接受下一個連線。重設的完成由 Match 的通知喚醒，不輪詢 future。
- 讀：`async_read_some`；frame 的長度、解析與版本檢查和現在相同。
- 寫：等 socket 可寫，再以非阻塞的 `write_some` 寫出，所以「在可寫的那一刻才選要寫哪一個 frame」：
  - 優先序不變：controls > actions > snapshot。
  - 未開始寫的 snapshot 會被更新的取代；已經寫出任何位元組的 frame，寫完才選下一個。
  - Transport 的 MovementTrace（合併與部分寫出的年齡）照舊。
- actions 的通道：每位玩家 30 Hz 的期限，用一個 `steady_timer` 等最早的期限。
- 連線結束的路徑明確化：讀錯誤或 EOF、協議違規（長度、解析、版本、未知訊息）、佇列溢位（joins／controls 超過 64）、寫錯誤，各自寫一行 `ipc closed reason=…`，然後重設、等待完成、接受下一個連線。
- P2-log 的 `ipc_iterations` 改為計算 io 處理函式的執行次數（accept、讀、寫、計時器、pump），語意隨之改變。

## 驗收

- **L1**：
  - Host：`Start`／`Stop`；實時下 Tick 約 60 Hz；重設在通知下及時完成；發布通知在 snapshot、結果、驅逐、重設完成時發出；覆蓋計數；Tick 晚醒的統計（含 notified）。
  - IpcHost（新的測試，用 loopback 的 TCP 客戶端）：先收到 Ready；Join→JoinResult；snapshot 送達；controls 優先於 snapshot；讀得慢時 snapshot 合併、只送最新、部分寫出的 frame 會寫完；斷線→重設→下一個連線收到 Ready 且狀態已清空；連線中與等待連線時停止都能及時返回。
  - 既有：全量 CTest（權威 digest 不變）、Go 的測試、突變。
- **開發跑次**：矩陣的 clean 與故障案例、`backpressure_probe.py` 的 host-ipc 層。
- **L2**：L1 完成後寫事前宣告，送使用者核准。before＝第 06 批的頭，after＝本批的頭；看 Match 的 Tick 晚醒、IPC 的迭代與 CPU、gameplay 判定，並依主機狀態分組。完成後做 P2-log、第 06、07 批的橫向對比（交接的「未結事項」）。

## 停止條件

- 權威 digest 改變。
- 需要改 wire。
- macOS 的 Tick 晚醒沒有改善。
- 既有的驗收斷言需要放寬。

## 結果（L1 與開發跑次，2026-10-09）

- L1：
  - 全量 CTest 69／69（新增 `object_fps_pvp.ipc`；權威 digest 不變）；Go 的測試通過。
  - 新的測試：Match 角色 5 件（刻步、重設即時喚醒、發布通知、覆蓋計數、重複 Start）；IpcHost 4 件（Ready 與 Join 結果先於 snapshot、連線之間的重設〔下一個連線之前 Match 就已清空〕、協議違規後接受下一個連線、停止及時）。既有的 host 測試改用 `Start`／`Stop`。
  - 突變 3／3 killed（`v7-07-ipc-no-publish-wake`、`v7-07-ipc-close-without-reset`、`v7-07-ipc-snapshot-before-controls`）。「改回相對等待」的突變要對晚醒時間下斷言才抓得到，在忙碌的 CI 上會誤判，所以不放；絕對期限的效果由開發跑次與 L2 的晚醒分布證明。
  - TSan：IPC 與 host 角色的測試沒有報告。
- 開發跑次（只記錄）：
  - clean-60（主機 4 ms 狀態）：Match 的 Tick 晚醒全部 <250 µs（最大 101～171 µs；P2-log 以 2～4 ms 為主、最大 4.2 ms）。Match 程序 CPU：P2-log 約 0.042 秒／秒 → A 之後約 0.022 → B 之後約 0.015。IPC 的處理次數約 513 次／秒（P2-log 的輪詢約 787 次／秒）。
    - 更正（2026-10-09，核對）：P2-log 的 0.042、A 的 0.022、B 的 0.015 秒／秒是三個不同 session 的開發跑次（各 1～2 個 10 秒視窗），不能當作 A、B 各自的效果。和 P2-log 相同的 Match C++，在第 06 批 L2 是 0.030（61 個視窗的中位數），在第 07 批 L2 before 是 0.0315；P2-log 的兩次開發跑次（0.042、0.041）都比這高，原因未查。交錯執行的第 07 批 L2 是 0.0315→0.0185（約 −41％；4 ms 狀態 −42％、8 ms 狀態 −41％）。Tick 晚醒的結論不變。
  - `backpressure_probe.py` 6 案（host-ipc、gateway、downstream 各 250／1000 ms）全部通過。第一次誤把 action probe 當成 `--probe`（它需要 timing probe），那次保留在 `dev-backpressure-wrong-probe/`。
  - `run_gameplay.py` 完整的 25 案矩陣：25／25 通過。
- 證據：`build/target/_build/test/logs/pvp-v7-batch07-20261009/`（git 忽略）：`mutations.json`、`ctest-full.log`、`tsan-ipc.txt`、`tsan-host.txt`、`dev-a-clean-60/`、`dev-b-clean-60/`、`dev-backpressure/`、`dev-matrix/`。

## L2 的事前宣告（2026-10-09 使用者核准）

使用者原話：「L2 宣告核准，開始跑，chrome 我還需要做其他工作，但是我可以不動他。」宣告的 SHA-256 是 `e5ef8087…`（`5749c6a` 時的本文件）。Chrome 開著但沒有操作，記在 `progress.jsonl` 的開頭。

- **目的**：
  - 確認停止條件「macOS 的 Tick 晚醒沒有改善」不成立。
  - 確認 IPC 改為事件驅動後，喚醒與 CPU 減少，gameplay 沒有變差。
  - 留下依主機狀態分組的資料，給第 07 批之後的橫向對比與第 09 批。
- **產物**：
  - before 是第 06 批的頭建置的 Match，after 是本批的頭建置的 Match。
  - Gateway 與 probe 共用本批的頭建置的版本：Gateway 的 Go 在兩者之間沒有差異；probe 是 Client 端，不含 Match。
  - 雜湊寫進 `artifacts.sha256`，跑次前後各核對一次。
- **主機**：沿用第 06 批 L2 的做法：開始時與每輪前後量 5 秒 sleeper，記錄背景的高 CPU 程序；機器閒置（關掉 Chrome 等）。
  - 每個跑次依 probe 的 TimerBaseline 分為兩種主機狀態：間隔 P99 ≥22 ms 為 8 ms 狀態，其餘為 4 ms 狀態。
    - 更正（2026-10-09，核對）：TimerBaseline 是 probe 連線前量的 3 秒（`timer_baseline.hpp:3-5`），抓不到跑次中的轉換。第 07 批 L2 的 before 第 2 輪 clean-60 被分為 4 ms（20.4 ms），但 Match 自己的晚醒有 19.7％ 落在 4～8 ms（其他 4 ms 的舊 Match 跑次 ≤3％，8 ms 的 ≥41％）；這一次決定了 before／4 ms 與橫向對比 B／4 ms 的 Tick P99 上界 8 ms，也是「4 ms 狀態也有 21.0 ms」的來源。舊 Match 的跑次可以用 Match 自己的晚醒分布交叉確認；新 Match 沒有晚醒，只能靠 TimerBaseline 與 sleeper。
  - 舊的分類用 Match 的 snapshot 晚醒，本批改變的正是 Match，所以不再適用（README 已預定第 07 批之後改用 TimerBaseline）。
- **案例與順序**：
  - clean-60、clean-30、host-ipc-250ms、host-ipc-1000ms、gateway-250ms，共 5 案。
  - 6 輪，每輪每案 before 與 after 各 1 次，奇數輪先跑 before。共 60 次，約 30 分鐘。
- **判定**（全部成立才算通過）：
  1. after 的所有跑次，gameplay 判定都通過。before 的失敗只記錄。
  2. after 所有視窗合計的 Tick 晚醒，P99 的上界 ≤1 ms。
  3. before 所有視窗合計的 Tick 晚醒，P99 的上界 ≥2 ms（確認 before 重現了舊的晚醒）。
- **只記錄**：
  - 各主機狀態下兩棵 tree 的 Tick 晚醒分布。
  - IPC 處理次數與 Match 程序 CPU（每秒，中位數）、`snapshot_overwrites`。
  - `legal_match_p95_ms`、`legal_client_p95_ms`；Gateway 的結果與 snapshot 間隔。
- **停止條件**：
  - 跑次錯誤時停下，不自行重跑。
  - after 出現 gameplay 失敗，或判定 2 不成立時，停下回報（後者就是本批的停止條件）。
  - 產物雜湊不符時停下。
- **證據**：`build/target/_build/test/logs/pvp-v7-batch07-l2-<日期>/`。

## L2 結果（2026-10-09，**通過**）

- before 的 Match 在 worktree 從零建置（第 06 批的頭 `dcc14be`）。建置與刪除 worktree 觸發 Spotlight 索引，等它平靜（10 秒）後才開始。
- 60 次全部完成，沒有停止。三項判定都成立：after 全部通過（before 也全部通過）；after 的 Tick 晚醒 P99 上界 244 µs；before 的 P99 上界 8 ms。
- 主機：第 1 輪是 4 ms 狀態，第 2 輪起進入 8 ms 狀態（sleeper P99 約 7.3～8.2 ms）。每棵 tree 有 6 次落在 4 ms 狀態、24 次落在 8 ms 狀態。
  - 更正（2026-10-09，核對）：主機是在第 2 輪之中轉入 8 ms 狀態（sleeper P99 第 2 輪開始 3.9 ms、結束 7.5 ms）。依 TimerBaseline，第 2 輪的 clean-60（第 1、2 個跑次）仍是 4 ms（20.7／20.4 ms），其餘才是 8 ms；但 before 那一次 Match 自己的晚醒已有 19.7％ 落在 4～8 ms（其他 4 ms 跑次 ≤3％），TimerBaseline 是 probe 連線前量的 3 秒（`timer_baseline.hpp:3-5`），沒抓到跑次中的轉換。每棵 tree 4 ms 6 次、8 ms 24 次是依 TimerBaseline 的分法；之後的 8 ms 狀態 sleeper P99 是 7.2～8.2 ms。

| tree／主機狀態 | Tick 晚醒的分布 | 最大 | P99 上界 |
|---|---|---|---|
| before／4 ms | 以 2～4 ms 為主（bins 139,100,200,684,2935,144,1,1,…） | 24.6 ms | 8 ms |
| before／8 ms | 以 4～8 ms 為主（bins 257,264,608,1627,3726,7872,57,…） | 8.4 ms | 8 ms |
| after／4 ms | 全部 <250 µs | 207 µs | 207 µs |
| after／8 ms | 全部 <250 µs | 244 µs | 244 µs |

- 更正（2026-10-09，核對）：before／4 ms 的 4～8 ms 晚醒（144 次）中有 118 次、以及 P99 上界 8 ms，都來自第 2 輪 clean-60 before 那一次（TimerBaseline 20.4 ms，但 Match 已在轉入 8 ms 狀態，見上一項更正）；去掉它，before／4 ms 的 P99 上界是 4 ms，和 P2-log 一致。最大 24.6 ms 是第 1 輪 clean-30 before 的單一一次晚醒（clean 案例，不是故障案例）。

- 只記錄的項目（中位數，before→after）：Match 程序 CPU 0.0315→0.0185 秒／秒；IPC 處理次數 722→501 次／秒；`snapshot_overwrites`（after）3。clean-60 的 `legal_match_p95_ms` 21.9→11.0 ms、`legal_client_p95_ms` 77.2→68.2 ms；clean-30 的 `legal_match_p95_ms` 21.8→16.2 ms、`legal_client_p95_ms` 100.4→84.9 ms。
  - 更正（2026-10-09，核對）：數字正確，但 clean-30 的 `legal_client_p95_ms` after 84.9 ms 與 `legal_match_p95_ms` after 16.2 ms、clean-60 的 `legal_client_p95_ms` before 77.2 ms，都是 6 個跑次兩群的中點（3 快 3 慢，或 4 幀與 5 幀），不是出現過的值。clean-30 的 100.4→84.9 只是快群的跑次數從 2 變 3，不能讀成改善；clean-60 的 `legal_match_p95_ms` 21.9→11.0 是舊 Match 在 8 ms 狀態的晚醒消失（見橫向對比的更正）。IPC 處理次數 722→501 在 B 改了定義（輪詢次數→io 處理函式次數），不是同一個量。
- 證據：`build/target/_build/test/logs/pvp-v7-batch07-l2-20261009/`（git 忽略）：`build-before.sh`／`.log`、`artifacts.sha256`、`declaration.sha256`、`session.sha256`、`progress.jsonl`（`a12b6c3d…`）、`judgement.json`（`10500660…`）、`runs/`。

## 橫向對比（2026-10-09，使用者要求）

- 對象：兩次 L2 涵蓋三種組合。A＝P2-log 的 Gateway＋舊 Match（第 06 批 L2 的 before）；B＝第 06 批的 Gateway＋舊 Match（第 06 批 L2 的 after 與第 07 批 L2 的 before，兩個 session）；C＝第 06 批的 Gateway＋第 07 批的 Match（第 07 批 L2 的 after）。每個跑次依 probe 的 TimerBaseline 分主機狀態（間隔 P99 ≥22 ms 為 8 ms 狀態）。

| 組合 | 主機狀態 | clean-60 `legal_match_p95_ms` | clean-30 `legal_match_p95_ms` | clean-30 `legal_client_p95_ms` | Gateway 量到的 snapshot 間隔 P50 | Tick 晚醒 P99 上界 |
|---|---|---|---|---|---|---|
| A | 4 ms | 11.1 | 9.9 | 100.6 | 16.6 | 4 ms |
| A | 8 ms | 12.8 | 19.0 | 116.1 | 16.35～16.6 | 8 ms |
| B | 4 ms | 10.0 | 9.5 | 68.1 | 16.6～16.65 | 8 ms |
| B | 8 ms | 23.1 | 22.0 | 100.8 | 16.4～16.5 | 8 ms |
| C | 4 ms | 11.0 | 12.6（1 次） | 70.2（1 次） | 16.7 | 207 µs |
| C | 8 ms | 10.3 | 19.8 | 99.6 | 16.7 | 244 µs |

- 更正（2026-10-09，核對）：(1) B／4 ms 的 Tick P99 上界 8 ms 來自第 07 批 L2 before 第 2 輪 clean-60 那一次（TimerBaseline 是 4 ms，但 Match 已在轉入 8 ms 狀態）；去掉它是 4 ms，和 A／4 ms 相同。B／4 ms 的 clean-60 `legal_match_p95_ms` 10.0 也含這一次的 21.0 ms。(2) A／8 ms 的 clean-30 `legal_client_p95_ms` 116.1 ms 是兩個跑次（102.2、129.9 ms，3 幀與 4 幀）的平均，不是幀長的倍數；A／8 ms、C／4 ms 各只有 1～2 次。

- 結論：
  - 第 06 批 L2 後段變慢的原因，是主機進入 8 ms 狀態（系統的計時器合併）時，舊 Match 的相對等待晚醒 4～8 ms。晚醒後補步，讓 Gateway 量到的 snapshot 間隔短於名目的 16.67 ms，`legal_match_p95_ms` 約翻倍。
    - 更正（2026-10-09，核對）：(1) 舊 Match 的 `legal_match_p95_ms` 變慢確實跟著主機的計時狀態。以 Match 自己的 Tick 晚醒分布看，TimerBaseline 為 8 ms 的舊 Match clean 跑次，晚醒落在 4～8 ms 的佔 41～82％（4 ms 跑次 ≤3％），Tick 間隔 >20 ms 的佔 9～18％；慢跑次裡 a＋b ≥17.5 ms 的動作每次 2～17 筆，其中約 2/3 的 k＝0：裁決 Tick 就是 relay 上行後的下一個 Tick，只是那個 Tick 晚到。機制是 Tick 晚到，不是補步。(2) snapshot 間隔 P50 低於名目在 4 ms 狀態也出現（A、B 的 clean 案例 16.6～16.65 ms），8 ms 狀態更低（16.35～16.5 ms）；它是晚醒的旁證，不是 `legal_match_p95_ms` 變慢的機制。(3)「約翻倍」在第 06 批 L2 第 5、6 輪的 8 個 clean 跑次中有 6 個成立；clean-60 before 是 11.1／14.4 ms。(4) 下面既有更正第 2 點的反例（第 07 批 L2 before 第 2 輪 clean-60，21.0 ms）是主機狀態的分類問題，見該點的補充更正。
  - 第 07 批之後 Match 不再受主機狀態影響：兩種狀態下 Tick 晚醒都 <250 µs，snapshot 間隔回到 16.7 ms，clean-60 的 `legal_match_p95_ms` 在 8 ms 狀態也是 10.3 ms。
  - 仍受主機狀態影響的是 clean-30：`legal_match_p95_ms` 與 `legal_client_p95_ms` 在 8 ms 狀態仍偏高。可能的來源在 Client 端：網路 worker 每 2 ms 的睡眠輪詢（第 08 批要改），以及 probe 自己 30 FPS 的幀節拍（結果在幀上觀測）。第 08 批之後再對比一次。
    - 更正（2026-10-09，核對）：既有更正成立（原結論不成立）。另外，「worker 每 2 ms 的睡眠輪詢」這個候選在送出側沒有證據支持：第 06、07 批 L2 的 a（送出→relay 上行，含舊 worker 的輪詢等待）在 8 ms 狀態 P95 是 3.0～3.6 ms，4 ms 狀態 2.4～3.3 ms，舊 worker 的 2 ms 睡眠沒有被拉長到 8 ms。收包側沒有獨立的時間戳，未能確認。
  - 限制：C 在 4 ms 狀態只有 1～2 次（主機大多在 8 ms 狀態），只供參考。
- 證據：`build/target/_build/test/logs/pvp-v7-cross-20261009/`（`cross.py` `c61c85af…`、`cross.json` `4656cfeb…`）。

### 更正（2026-10-09，第 08 批的評估）

上面的結論有兩處不成立，原文保留：

- 「仍受主機狀態影響的是 clean-30」不成立。clean-30 的快慢兩群同在 8 ms 狀態，差別來自路徑上幾個 30 Hz 節拍的相位，以及 probe 把時間戳記在幀開始（`legal_client_p95_ms` 只能是幀長的整數倍，P95 落在第 2 或第 3 幀）。見[第 08 批](08-client-worker-asio.md)的「評估」。
- 「後段變慢的原因是 8 ms 主機狀態」只對 Tick 晚醒與 snapshot 間隔成立（這兩項是直接量到的）。`legal_match_p95_ms` 的快慢兩群在舊 Match 下 4 ms 狀態也會出現（第 07 批 L2 before 第 2 輪 21.0 ms），還受到 Gateway→Match action batch「每隔一次 ticker」的相位影響。新 Match 的 clean-60 不論狀態都是 8.8～11.9 ms。
  - 補充更正（2026-10-09，核對）：本點兩個根據都不成立。(1)「4 ms 狀態也會出現（第 07 批 L2 before 第 2 輪 21.0 ms）」：那一次 TimerBaseline 是 20.4 ms（probe 連線前量 3 秒，`timer_baseline.hpp:3-5`），但 Match 自己的晚醒有 19.7％ 落在 4～8 ms（其他 4 ms 跑次 ≤3％，8 ms 跑次 ≥41％），主機在這一輪內轉入 8 ms 狀態（sleeper P99 第 2 輪開始 3.9 ms、結束 7.5 ms）。真正在 4 ms 狀態下的舊 Match 慢跑次只有第 06 批 L2 before 第 2 輪 clean-30（19.4 ms），來自 2 個在 Tick 前 0.1～0.2 ms 才到 relay 的動作。所以對舊 Match 而言，原結論「`legal_match_p95_ms` 變慢是 8 ms 狀態的晚醒」大致成立（例外是第 06 批 L2 clean-60 before 第 5、6 輪）。(2)「Gateway→Match action batch『每隔一次 ticker』的相位」沒有根據：link 迴圈每收到輸入、動作、結果就被叫醒（`runtime_link.go:163`、`:246`，`action_delivery.go:158`），第 06、07 批 L2 clean 的新動作到達 relay 時，link 上可能還有待送內容（距今 <I）的只有 0～2／192 筆（`pvp-v7-08b-plan-20261009/evidence/h1_identity.json`）。本點最後一句（新 Match 的 clean-60 不論狀態都是 8.8～11.9 ms）成立。

## 架構

- 模擬執行緒的擁有者從 `match_main` 移到 `MatchRuntimeHost`（產品內，和 Client 的模擬角色一致）。新的依賴邊：`runtime_host`→`GYO::Threads`（Engine 既有的模組，方向是產品→Engine）。
