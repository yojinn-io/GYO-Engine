# edge-results-passthrough 的 L2 事前宣告（草稿第 1 版）

- 狀態：**草稿**，依 D45 做門檻來源的對抗式檢查、寫好工具並乾跑之後送核准（AGENTS.md §14.1：門檻、試行次數、重試規則在看結果之前核准；核准後不放寬）。
- 依據：D94、D95（D-10）、D96、D107、D111；protocol-v7 §2a、§4、§9；README `edge-results-passthrough` 列。
- 先例：[edge-input-passthrough 的 L2 事前宣告](edge-input-passthrough-l2.md)（D109、D110；本宣告沿用它的結構、主機規則、停止規則與工具）。
- 數字沒有標「實測」的，是程式推導或推論。

## 1. 目的

這批是行為改變：Gateway 不再保存結果帳本（session 帳本、退役補送、結果節拍、A3 都刪除）。Match 寫來的每個結果框架，Gateway 逐則檢查後放進該玩家的單槽（新的蓋舊的），在「上次送出＋1/30 秒」時放出；被蓋掉的框架計入回報 34。結果的重送與補送只由 Match 負責（`edge-results-resend`）。link 原樣轉送 ACK-only 批（即使 ACK 不超過退役值），讓 Match 收得到補送退役的要求。GYOP 序號一律由唯一的 UDP 寫出者在寫出前指定（含 controls）。

L2 **判定**：

1. 遊戲行為沒有變差：既有判定、D44 事件、權威 Cooldown 拒收、逐出。
2. 裁決到 Client 的延遲沒有超出 D107 的差異帶（新裁決最多晚 1 個間隔）。
3. 結果送出間隔守住 30 Hz（含 host-ipc 停頓）。
4. 帳目與回報：ingress 帳目照宣告的形狀；丟棄回報兩端相等。

**只記錄、不判定**：回報 34（單槽被取代）的次數、送給非 active 玩家的結果框架數、結果資料包數與流量、替代率（結果不影響命令；只作為異常的旁證）。

## 2. 兩棵樹

| 樹 | 內容 |
|---|---|
| before（B） | `6d9ecf5`（`edge-input-passthrough` 的 A；之後的 commit 只改文件）。worktree `../GYO-Engine-ip-after`（已建置、乾淨） |
| after（A） | 本批的 commit（commit 後填入）。新的 worktree |

- B→A 的程式差分：Gateway（`gateway/*.go`）、`runtime_v7.proto` 的 reserved（16、20；重產的 enum 會讓 Match 的 `DropReason_IsValid` 把它們判成 `reason_range`；Match 其他程式不變）、測試與文件。
- probe 兩棵樹沒有原始碼差分：只用 B 的建置。其餘照 edge-input-passthrough 宣告第 2、3 節（乾淨、雜湊、`vcs.revision`、主 checkout 只能有文件變更）。

## 3. 工具

沿用 edge-input-passthrough 的 `run_l2.py`／`judge_l2.py`，複製到新的 prep 目錄後改成本批的情境與判定（新雜湊，乾跑後填入）。凍結清單 `frozen-pv7.sha256`（`b4a89630…`）、`judge_16.py`、`compare_reports.py`、`sleeper.py` 照舊。新增 `action_latency_v7.py`（主 checkout 的 `build/acceptance/object_fps_pvp/`，雜湊乾跑時記下）。

## 4. 主機

照 edge-input-passthrough 宣告第 4 節：`caffeinate -dims`、PR 的 Auto-fix 暫停、插電不移動、閒置門檻、每跑次前後的 sleeper 與 CPU 前 5 名。

## 5. 情境 × 次數 × 順序

B、A 交錯，每個情境輪流由 B 或 A 先跑；每個項目 1 輪。

| 情境 | runner | 每棵樹 | 用途 |
|---|---|---|---|
| clean-60 | `run_gameplay.py --case clean-60` | 10 | 延遲、送出間隔、Cooldown |
| loss-shot、loss-result、loss-ack | `run_gameplay.py --case …` | 各 5 | §9 的 loss 模式：結果遺失後的恢復 |
| network-impairments | `run_network.py --network-impairments` | 5 | §9 的 network 模式 |
| backpressure host-ipc-1000ms、gateway-1000ms | `backpressure_probe.py --layers host-ipc gateway --durations-ms 1000` | 10 | 停頓後的送出間隔（README：含 host-ipc 停頓） |
| quad clean-60 | `run_quad.py --case clean-60 --rounds 1` | 5 | 4 人時的送出間隔 |
| soak-long | `run_gameplay_soak.py --soak --fps 60` | 1 | 長跑次的 Cooldown 與逐出 |

估計（推論，依上一次 L2 的跑次時間）：約 2 h。

## 6. 判定

### 6.1 逐跑次判定（任一棵樹不成立就立刻停）

| 代號 | 判定 | 門檻與來源 |
|---|---|---|
| R1 | 既有判定與 D44 事件 | 凍結分析器的判定都通過（loss 系列照 runner 自己的判定）；E1、E2、E3 都是 0（`judge_16.py`）。來源：v7 第 16 批與上一次 L2 都是全數通過（實測） |
| R2 | 權威 Cooldown 拒收 | clean-60、quad、soak 是 0（`judge_16` 的停止條件 2 的定義）。來源：同上 |
| R3 | 結果送出間隔 | 每位玩家每個視窗的 `results_min_interval_ms` ≥ 33.3（Gateway 的 `player send statistics` 行）。來源：單槽在「上次寫出完成＋1/30 秒」才放出（§2a），完成時刻量的間隔不可能更短（程式推導） |
| R4 | ingress 帳目 | 照 edge-input-passthrough 宣告 6.3 的逐項表（含 D110 的 I1 規則、I3 的宣告形狀）；本批新增「被刪路徑」的鍵：`drop_invalid_actions_packets`、`drop_invalid_actions_shots` 在 A 都是 0 |
| R5 | 丟棄回報 | 合格跑次（有回報的 backpressure 跑次）兩端逐則相等、違規 0；兩棵樹各至少 1 個合格跑次 |
| R6 | 逐出 | 0 |

### 6.2 session 結束才判

| 代號 | 判定 | 門檻 | 來源與限制 |
|---|---|---|---|
| L1 | 裁決到 Client 的延遲（`action_latency_v7.py` 的 `to_worker_ms` p95） | 有動作的情境（clean-60、loss 系列、soak）：A 的中位數 − B 的中位數 ≤ 1 個間隔（33.3 ms） | D107 的差異帶（`edge-results-resend` 實測：新裁決最多晚 1 個間隔）。這批刪掉 Gateway 的第二層排程，預期 A 不比 B 慢（推論） |
| L2 | 結果送出頻率 | 每位玩家任一個 10 秒視窗的結果資料包數 ≤ 301（30 Hz × 10 s ＋ 1） | §2a 的 30 Hz；R3 已逐跑次保證間隔，這項是彙總的旁證 |

## 7. 預測（不是停止條件）

| 代號 | 項目 | 預測 | 推翻條件 |
|---|---|---|---|
| P1 | clean-60 的 `to_worker_ms` p95 | A ≤ B | A 的中位數 > B 的中位數 |
| P2 | 回報 34（單槽被取代）的次數 | clean 很少（Match 的通道每間隔最多一框，到達間隔約等於放出間隔）；host-ipc 停頓後較多 | clean 每跑次 > 10 |
| P3 | 結果資料包數 | A ≈ B（兩者都是未確認期間每間隔一框） | 差 > 20% |

## 8. 停止條件與重試規則

照 edge-input-passthrough 宣告第 8 節：不重跑、不補跑；任一 6.1 不成立就停在那個跑次；受干擾的跑次累計 3 次就停；session 結束後 6.2 不成立就回報。

## 9. 差異帶（給 `pv7-checkpoint`）

本 session 實測的 A−B。

## 10. 等你決定

1. **核准本宣告**（D45 檢查與乾跑之後的版本）。
2. **D112：protocol-v7 §9 的「每隔一則丟一則時 ≤2⌈n/8⌉ 則」**。實作照 §2a（先放最小的 4 筆、其餘循環）只能做到 ≤2⌈n/8⌉＋2（Go 測試實測：n＝16 時 6 則、24 時 8 則、32 時 10 則）。原因：Client 只能連續 ACK，出現缺口後 Match 不知道 Client 已有哪些較大的 id，循環槽位會在一個收到的框架裡重送 Client 已有的 id。選項：(a) §9 改成 ≤2⌈n/8⌉＋2（建議；契約行為不變）；(b) 改設計讓它達到 2⌈n/8⌉（要 Client 回報缺口，是 wire 變更）。
