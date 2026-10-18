# pv7-checkpoint 的 L2 事前宣告（第 1 版草稿）

- 狀態：**草稿**，等 D45 的門檻來源對抗式檢查，之後寫工具並乾跑，再請使用者核准（AGENTS.md §14.1：核准前不看 A 的回歸指標；核准後不放寬）。
- 依據：D129、D130、D131、D127、D128；README `pv7-checkpoint` 列；`pv7-design.md` 第 119～120 行（K 與 S 的資料）；`protocol-v7.zh-Hant.md` §6「影子量測」；v7 基準 [STABLE_BASELINE](../v7/STABLE_BASELINE.md) 與 16-L2 宣告（`build/target/_build/test/logs/pvp-v7-16-prep-20261014/declaration.md`）。
- 數字沒有標「實測」的，是推導或推論。門檻裡的「B 的範圍」一律用本 session 的 B 跑次計算。

## 1. 目的

A1（`a1-shot-step`）之前的整體回歸，一次 session 做兩件事：

1. **回歸**：v7.1 pv7-line 到目前為止的產品，和 v7 穩定基準在同一台機器、同一個 session 交錯比較（D130）。三個 edge 批的 L2 已經各自驗過 A−B；這裡驗的是「v7 → 現在」整段，包括那些宣告「行為不變」、只由 L1 證明的批（proto-rename、version-errors、heartbeat-sync、time-echo、drop-report、edge-results-resend、cause-records、hitbox-fields）與 K1 的修正（D121）。
2. **影子資料**：給 `a1-shot-step` 決定 K（冷卻保留的上限）與 Client 的 S 取法（N 或 N＋1）。只記錄，不是判定；但資料完整性（D127 的兩項）是判定，不成立就是工具無效。

不在範圍：D44 事件回歸（屬於 `d44-regression-pv7`，pv7 合併之後、含 A1）；60 Hz 30 分鐘長測（同上）；edge 類指標（d、to_tick、Expired、替代率、Match 收到的批數）和 v7 的比較（v7 的 pv6 原始檔不能用凍結 pv7 分析器讀，三個 edge 批的 L2 已驗過）。

## 2. 兩棵樹

| 樹 | commit | 程式 | 判定工具 |
|---|---|---|---|
| B（v7 基準） | `6d3f287`（`../GYO-Engine-v7final`，pv6） | 該樹建置的 Match、Gateway、全部 probe、arena、GUI probe | 16-L2 的凍結清單（117 檔，`pvp-v7-16-prep-20261014/frozen.sha256`，由該樹的 `build/acceptance` 提供）；`judge_16.py`、`judge_c2.py`；timing 用 16-L2b 的 v7 HUD 規則（`check_v7_rule.py`、`pvp-v7-combat-hud-2`） |
| A（現在） | 工具階段時的 HEAD（之後的 commit 只改文件） | 該樹建置的 Match、Gateway、全部 probe、arena、GUI probe | 凍結清單 `pvp-v71-pv7-checkpoint-20261018/frozen-pv7-cp.sha256`（`f036b2af…`，＝`frozen-pv7-rl.sha256` 加 D131 的 `run_action_short.py` 與其測試）；`judge_16.py`、`judge_c2.py` 的分類（與協議版本無關的部分，edge 系列的 L2 已在 pv7 上用過） |

- 兩棵樹的 probe 各自建置（pv6 對 pv6、pv7 對 pv7），不共用；arena 內容相同（工具階段核對 digest）。
- 權威（L1 部分，session 之前做）：`compare_authority_trees.py` B 對 A，scale 1、3 各 35 情境 0 差分；golden 30 個 0 不符。不成立就停下，不跑 session。

## 3. 項目與順序

每個項目在 B、A 各跑一次，B／A 的先後逐重複交替（第 1 輪 B 先）。不隨機化；不補跑、不重跑。閒置閘門、每項前後的 sleeper、STOP-REQUEST、逾時與 SIGTERM／SIGKILL 的規則照 edge 系列（`edge-rate-limit-l2.md` 第 4 版沿用的規則）。

| 部分 | 項目 | 每棵樹的次數 | runner |
|---|---|---|---|
| I | 25 案 | 3 輪（75） | `run_gameplay.py --case` |
| | 背壓（6 案） | 3 | `backpressure_probe.py --layers host-ipc gateway downstream --durations-ms 250 1000` |
| | 網路一般模式 | 3 | `run_network.py` |
| | 網路 impairments | 3 | `run_network.py --network-impairments` |
| | quad clean-60 | 1（5 輪） | `run_quad.py --case clean-60 --rounds 5` |
| | 長測短模式 | 1（2 循環） | `run_gameplay_soak.py --fps 60` |
| G（開視窗） | 1 GUI＋3 bot、action short、player short、GUI timing、2 GUI＋2 bot 三項 | 各 1（7） | 同 16-L2 的 G |
| | 連點 6 格（interval12、edge × 30／60／144） | 各 2（12） | `run_action_short.py --case` |
| S（只 A，影子資料） | Gateway→Match 延遲注入，T＝2.0 與 11.5 秒 | 各 3（6） | `run_short_stall.py --delay-ms 50 --window-ms 100 --fault-at T` |

- 每棵樹 106 項，加 S 的 6 項，共 218 項。時間約 2.2 小時〔推論：遊戲 25～31 秒、背壓 82 秒、網路 30 秒、impairments 90 秒、quad 150 秒、G 每項約 1 分、連點 32 秒〕，工具階段以開發跑次更正。
- G 與連點要顯示器亮著、不碰滑鼠。

## 4. 每項的停止條件（任一棵樹不成立就停）

| 代號 | 條件 | 適用 | 來源 |
|---|---|---|---|
| R1 | 凍結判定 P（`judge_16.classify`；timing 用 16-L2b 的 v7 規則；連點用 `judge_c2.classify_rapid`；player144 的 `invalid` 只以 D11③ 接受）。A 的 F 停下；B 的 F、兩棵的 E／A 都是工具或環境無效 | 全部 | 16-L2、edge 系列 |
| R2 | clean 跑次的權威 Cooldown 拒絕＝0；連點的權威 Cooldown（`judge_c2.rapid_cooldown`）＝0 | clean-30／60／144、長測短模式、quad；連點 | 16-L2 停止條件 2；C2-B |
| R3 | clean 跑次的非 respawn 重設＝0；遊戲案例的 D61 即時規則（故障窗外的非 respawn 重設） | 25 案、長測短模式 | 16-L2 停止條件 3、L2-1 |
| R3b | **例外**：B 的 gateway-1000ms 與背壓的 gateway 層，故障窗外的重設只記錄（K1，D118：v7 的固定窗口限速在 Gateway 停頓後丟輸入，恢復後約 0.93～1.0 秒有第二次 starvation，偶爾再接 backlog 重設） | B 的 gateway-1000ms、背壓 gateway 層 | `edge-rate-limit-l2.md` §3.1 的實測 |
| R4 | relay 結果任一秒 ≤31；Gateway `results_min_interval_ms` 印出值 ≥33.3 | 25 案（兩棵） | 16-L2 L2-1 |
| K1a | Gateway 的限速拒收＝0（每個 Gateway 日誌、每位玩家，四個 `drop_rate_limited_*_packets`） | A 的全部項目 | `edge-rate-limit-l2.md` §3.1 |
| H1 | 影子紀錄完整：每個傳了 `--movement-trace` 的項目都有 `match-shots.jsonl`，結尾 `trace_end` 的 `records` 等於紀錄行數、`dropped`＝0；Client 收到的每個裁決（`actions.jsonl`，或 quad 的 summary）都有一筆影子紀錄，以 (player, action_id) 加時間順序對上 | A 的全部此類項目 | D127 的精神（「不得悄悄少算」） |
| H2 | **對接（D127 (1)）**：每筆影子紀錄以 (player, E, life, S) 加時間順序對上 `match-commands.jsonl` 的 resolved 事件——事件的 `time_ns` 必須在紀錄的 `time_ns` 之後，且不晚於紀錄時刻＋(step_tick−a＋1)×Tick＋1.5 秒（故障窗的寬限，D61）；同一鍵在這個窗口裡有兩個以上的事件，或窗口裡跨過了一次 Match 的 reset（`match.log` 的 `ipc closed` 之後的新 session）就是對接無效 | A | D127、D128 |
| H3 | **對接率（D127 (2)）**：每個 `path`＝`on_time` 的紀錄都要依 H2 對到 resolved 事件；任一筆對不到就是工具無效（不是判定，session 停下） | A | D127 |
| T | 工具有效性照 edge 系列（執行檔身分、雜湊、trace 缺漏等） | 全部 | edge 系列 |
| D | 受干擾：sleeper 最大 ≥10 ms 或連點分類 D；第 3 個受干擾的項目停下 | 全部 | edge 系列 |

## 5. 結束時的判定（只在完整的 session 上判）

指標從各樹的 `result.json` 讀（兩個協議版本的 `movement`、`legal_*` 鍵相同，工具階段核對）。每個情境取每棵樹各跑次的值，比較中位數。

| 代號 | 指標 | 情境 | 規則 | 來源 |
|---|---|---|---|---|
| C1 | 移動「產生→Actual」P95 | clean-30／60／144、loss-*、burst2、host-ipc／gateway 的 250 與 1000 ms、downstream-*、drain-stall、cross-life、socket-path-*、quad、長測短模式 | median A − median B ≤ max(range B, 1 ms)（只看 A 變慢的方向） | 下限 1 ms：edge 系列 L1 的下限；16-L2 的實測 clean P95 37.7～37.9 ms、三輪的範圍 0.06 ms，所以實際由下限決定 |
| C2 | 移動「產生→Actual」P50 | 同 C1 | 同 C1 | 同上 |
| C3 | Actual 比例 | 同 C1 | A 每個跑次 ≥99%（絕對門檻） | 16-L2 的門檻（v5／v6 契約） |
| C4 | 合法射擊 Match P95 | 有合法射擊的案例 | median A − median B ≤ max(range B, 1 ms) | 同 C1 |
| C5 | 合法射擊 Client P95 | 同 C4 | median A − median B ≤ max(range B, 1 Tick＝16.7 ms) | 結果從 v7 的 Gateway 30 Hz 持有改成 Match 重送（edge-results 系列），Client 端延遲有已宣告的變化（D107：新裁決最多晚 1 個間隔）；1 Tick 的下限〔推論，待 D45〕 |
| C6 | network0／20／40、upstream-*、network impairments | — | 只記錄（edge 系列也只記錄；網路情境的跑次間變異大） | edge 系列 |
| C7 | G 的 GUI 項目 | — | 兩棵都 P（R1）；可見交越、裁決抵達只記錄 | 16-L2 L2-6 |

- C1～C5 不成立時，報告列出該情境、兩棵樹的值與範圍，**不自動當成退步**：交使用者判斷是否在線內 commit 之間二分（`pv7-design.md:138`）。
- 只記錄：K1 的對照（B 的 gateway-1000ms 恢復後 starvation、A 沒有，P 類預測）；A 的 `max_session_window_packets`；兩棵樹的 Cooldown、重設原因；TimerBaseline 狀態。

## 6. 影子資料（只記錄，給 `a1-shot-step`）

在 H1～H3 成立的前提下，對 A 的每個情境列出：

- 路徑分布（on_time／late／epoch／life／hold_limit）；clean 的退回率（late＋epoch＋life，目標 0，Client 的 S 取法用）。
- on_time 的領先 S−L(A) 與 step_tick−A 的分布（準時餘裕）。
- Shot 的冷卻保留量 `cooldown_hold_ticks` 的分布，尤其是「退回之後下一發落在冷卻內」：同一位玩家前一發走退回路徑、這一發 `cooldown_hold_ticks`>0 的次數（K 用）。
- 命令 S 的來源（H2 對上的 resolved 事件的 `source`：Actual／Held／Neutral）與晚到（`match-ingress.jsonl` 的替代紀錄）。
- view tick：`view_tick_invalid`、view_lag 的分布（v8 回溯窗口的參考）。

資料來源：25 案（clean、network20／40、upstream、各故障）、quad、長測短模式、連點 12 項（真正的遊戲本體）、GUI timing、延遲注入。probe 的 E、S 照 Client 規則填（D131），但不是 Client 本身；報告分開列 probe 與 GUI 的資料。

## 7. 預測（不是停止條件）

| 代號 | 預測 | 推翻條件 |
|---|---|---|
| P1 | C1、C2 都成立：Actual 的時序由 Client 的模擬步決定，v7.1 沒有改 Client 的移動 | 任一情境不成立 |
| P2 | B 的 gateway-1000ms 每跑次恢復後有 starvation（K1），A 沒有 | 任一跑次相反 |
| P3 | A 的 clean 退回率為 0（開發跑次：clean-60 的 on_time 41／43，另 2 筆是跨生命的 life） | clean 有 late 或 epoch |
| P4 | 時間約 2.2 小時 | 實測差 30% 以上 |

## 8. 等你決定

1. 核准本宣告（D45 與乾跑之後的版本）。
2. 什麼時候跑：約 2.2 小時加閒置門檻；G 與連點要顯示器亮著、不碰滑鼠；跑前關 Chrome、暫停 PR 的 Auto-fix。
