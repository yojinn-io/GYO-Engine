# v8-product-hitbox-lag-l2 的事前宣告

更新：2026-11-05。Owner：`object_fps_pvp`。**狀態：已核准**（`v8-product-hitbox-lag-l2-declaration-approve`，2026-11-05，使用者：「Approve as recommended」；`-session` (a)、`-r-quantile` (a)）。
估時：工具與場次腳本 3～5 h，加一次 L2 場次（你的時間約 30 分鐘）。超過 1.5 倍停下回報。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | 主會話 Opus 5.5／medium | 分析工具、場次腳本、判定的記錄 |
| 審查者 | Opus 5.5／high（子 agent；和實作者同一個模型） | R 會變成 Match 的權威常數，分析工具的誤差直接進 R |
| §14.4 檢證者 | 不需要（沒有改既有判定） | — |

上段／下段：**上段**。R 決定回溯的權威範圍（protocol-v9 第 113 行），屬於 Architecture 判斷與權威結果的前提。

## 這批做什麼

protocol-v9 第 113 行：τ＝clamp(view_tick/65536, J−R, J)，R≤15（`maximum_reference_age_ms` 250 ms×60 Hz），**R 由 lag-l2 的分位數決定**（`v8-product-hitbox-rewind-r` (a)，已核准）。本批：

1. 寫分析工具 `build/acceptance/object_fps_pvp/lag_l2_summary.py`：讀一個場次的 `match*-shots.jsonl`（只接受 version 3），輸出 view_lag 的分布與 R。
2. 寫它的辨別力測試 `tests/object_fps_pvp/test_lag_l2_summary.py`（CTest `object_fps_pvp.lag_l2_summary`，標籤 `cpu;pvp`），用手寫的合成資料。
3. 寫場次腳本（每台機器一支，交出前每條命令實際跑過；程式事先放好，不假設現場有網路）。
4. 跑一次 L2 場次，用工具算出 R，記錄在 protocol-v9 與計畫。R 寫進程式是 `v8-product-hitbox-rewind` 的事，本批不改 Match。

不在這批：
- 回溯本身（rewind 批）。
- snapshot 漏收或被合併時的落差（protocol-v9 第 120 行的「接受的限制」）：影子紀錄 v3 沒有這個欄位，本批量不到，照實記為範圍的限界。
- WAN：本批只有 LAN。見「範圍的限界」。

## 開工前的事實

1. `match-shots.jsonl` v3（PR #154）：每個裁決一行，`view_lag_q16`＝J×65536−view_tick_q16，view tick 無效或為 0 時是 `null`；`trace_end` 有 `records`、`dropped`、`view_tick_invalid`。
2. view tick 有效的條件是 65536≤v≤observed×65536（`ValidViewTick`），observed≤J，所以有效的 `view_lag_q16`≥0。
3. Match 的 Tick 是 60 Hz（`Movement.hpp:15` 的 `AuthorityTickRate`），1 Tick≈16.7 ms；R≤15 等於 250 ms。
4. 過去的真實 LAN 場次（lan15，v7.1）的構成是 Mac（Intel）跑 Match＋Gateway、Windows 跑 Client；`lan15_judge.py` 讀 `mac/match*-shots.jsonl`。本批沿用這個構成。
5. hitbox-line 的 K8 試玩也在等實機（`v8-product-hitbox-acceptance` A4）。

## 判定（結果出來前宣告）

### 工具（本機）

- **T1**：`test_lag_l2_summary.py` 通過，至少包含：
  - 分位數用「最近秩」（nearest-rank，`ceil(q×n)` 的那一個），不內插；有手算的期望值；
  - 只算 `kind=decision`、`action_kind=shot`、`view_lag_q16` 不是 `null` 的行；
  - version 不是 3 的行、負的 `view_lag_q16`、`trace_end` 不是恰好一行、`dropped`≠0：工具報錯並以非 0 結束；
  - R 的計算見下面的 R1，包括超過 15 時取 15。
- **T2**：本機 PvP `cpu|shader` 全部通過。
- **T3（突變）**：新增 3 個，全部要被抓到：
  - 分位數改成內插；
  - 不排除 `null`（當成 0）；
  - 不檢查版本。

### 場次（L2）

- **構成**：Mac（Intel）跑 Match＋Gateway；Windows Client 至少 2 台。程式是本批 PR 的 L1 產物（Windows 套件；Mac 從同一個 commit 建置），SHA-256 記錄在證據裡。
- **進行**：全員加入後連續對戰至少 10 分鐘，照常遊玩（移動中互射）；不做刻意的斷線。見選項 `-session`。
- **S1 的區間**（修訂 3）：從 Match log 上 `s1_end` 之前最後一行 `join player=N accepted`，到 `marks.txt` 的 `s1_end`（Mac 的時刻，你在連續對戰結束時記下）。影子紀錄的 `time_ns` 和 Match log 的 `mono_ns` 是同一個 Match 程序的 steady_clock，`s1_end` 經 Match log 上牆鐘最接近的那一行換算過去。K8 的斷線與重連都在 `s1_end` 之後。
- **S1（資料足夠）**：有效的 Shot 至少 500 發（`view_lag_q16` 不是 `null`），而且至少 2 位玩家各至少 100 發。不足時不算 R，停下回報（不延長、不補跑；要不要再排一次由你決定）。
- **S2（帳目）**：`trace_end` 的 `dropped`＝0。另外記錄：送出的射擊數（各 Client log）、收到的決定數、`view_tick_invalid` 數，三者對帳。不一致時不確定 R，等你判斷（§14 開頭）。
- **R1（R 的規則，見選項 `-r-quantile`）**：R＝min(15, ceil(p99 的 Tick 數)＋2)。p99 是全部有效 Shot 的 `view_lag_q16/65536`。另外記錄 p50、p90、p95、p99、最大值（Tick 與 ms），以及 R＝8～15 各自會被夾住的比例。
- **R2（記錄，不判定）**：`view_lag_q16−view_lag_a_q16` 的分布，也就是冷卻保留的 Tick 數，和 `cooldown_hold_ticks` 應該相同。不同時是工具或紀錄的錯，停下回報。
- 不重跑：場次只跑一次；判定不成立就停下回報（§14.1）。

## 範圍的限界

- **只有 LAN**：LAN 的 view_lag 主要是 Client 的內插延遲加上傳輸，WAN 會更大。R1 的「＋2 Tick」與上限 15（250 ms）就是為此留的餘裕；WAN 的分布要等之後有 WAN 資料時再量（`v8-product-input-delay` 也需要 WAN 資料）。
- **R 同時是濫用的上限**：惡意 Client 可以固定把時刻填成 J−R（hitbox-plan 第 1 版的對抗式檢查第 13 點）。R 越大，偏袒射手的範圍越大；本批只記錄這一點，不處理。
- **漏收與合併**：見「不在這批」。
- **`network` 標籤的測試不在 CI**：本批的工具測試是 `cpu` 標籤，會在 CI 跑；場次本身不在 CI。

## 證據

- 工具：主 checkout 的 `build/evidence/v8-hitbox-lag-l2/`（Intel Mac）：ctest 輸出、突變報告。
- 場次：同一資料夾的 `session/`（Match、Gateway、各 Client 的 log 與 trace，`marks.txt`，SHA-256），以及工具輸出的 JSON。

## 選項（附建議）

| 代號 | 問題 | 選項 | 建議 |
|---|---|---|---|
| `-session` | 場次怎麼排 | (a) 和 K8 試玩合成同一個場次：先照本批跑 10 分鐘以上的連續對戰，再做 K8 的斷線、重連與形狀的感想；(b) 本批單獨一個場次 | (a)：兩者都要同樣的機器與人，合併省一次準備；K8 的斷線放在本批的 10 分鐘之後，不影響 S1 |
| `-r-quantile` | R 用哪個分位數 | (a) p99＋2 Tick；(b) p95＋2 Tick；(c) 最大值（不超過 15） | (a)：p95 會讓約 5% 的射擊被夾住；最大值容易被單一次卡頓拉高 |

## 修訂紀錄

1. 2026-11-05 草稿。
2. 2026-11-05 核准，`-session` (a)、`-r-quantile` (a)。
3. 2026-11-05（看到任何場次結果之前）：補上 S1 的區間定義。核准的版本只寫「K8 放在 10 分鐘之後，不影響 S1」，沒寫怎麼把 K8 排除在 R 的資料之外；這裡只把它寫成可以計算的規則，判定的門檻不變。
4. 2026-11-05（看到任何場次結果之前，審查後）：
   - S2 照本文的三方對帳實作：Client 的送出數＝接受＋拒絕且沒有未決（lan15 的 K1）、Client 依原因的計數＝Match 的裁決（K2）、`trace_end` 的 `records`＝裁決行數、`view_tick_invalid`＝`view_valid: false` 的行數。第一版工具漏了前兩項之外的檢查，審查指出後補上。
   - R2 不成立時 R1 也不給（本文「停下回報」的實作）。
   - 「至少 10 分鐘」是進行方式，不是 S1 的門檻：工具只記錄長度，不判定。
   - T3 的突變由 3 個增為 5 個（加上 S1 起點與送出數的檢查），只收緊。
   - 新增限界：Client 的 `PvP summary` 每秒寫一次，離開對局時最後不滿 1 秒的那一段不會寫出；離開前最後 1 秒內的射擊會讓 S2 不一致。場次腳本要求「停止射擊與換彈至少 3 秒後才離開」（同 lan15）。K8 的斷線也算在全場的對帳裡，造成不一致時 R 不給，由你判斷。
5. 2026-11-05（看到任何場次結果之前，場次腳本的審查後）：修正修訂 4 最後一句的說法。K8 的斷線若超過 5 秒，Gateway 讓舊的 id 逾時，那個 id 不進 K1、K2（lan15 的 `normal_leavers` 排除它）；重新加入的新 id 照常對帳。全場都算的只有 `trace_end` 的 `records`、`dropped`、`view_tick_invalid`。手冊要求斷線一直持續到回到 Lobby。
