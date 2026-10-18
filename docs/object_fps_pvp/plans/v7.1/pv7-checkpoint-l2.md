# pv7-checkpoint 的 L2 事前宣告（第 2 版草稿）

- 狀態：**草稿**。第 1 版經 D45 的門檻來源對抗式檢查（1 位 xhigh）：MUST-FIX 4、SHOULD-FIX 9、NOTE 9，對應表在第 9 節。標「待定」的 4 處要使用者決定（第 8 節），之後寫工具並乾跑，再請使用者核准（AGENTS.md §14.1：核准前不看 A 的回歸指標；核准後不放寬）。
- 依據：D129、D130、D131、D127、D128；README `pv7-checkpoint` 列；`pv7-design.md` 第 119～120 行（K 與 S 的資料）；`protocol-v7.zh-Hant.md` §6「影子量測」；v7 基準 [STABLE_BASELINE](../v7/STABLE_BASELINE.md) 與 16-L2 宣告（`build/target/_build/test/logs/pvp-v7-16-prep-20261014/declaration.md`）。
- 數字沒有標「實測」的，是推導或推論。門檻裡的「B 的範圍」一律用本 session 的 B 跑次計算。

## 1. 目的

A1（`a1-shot-step`）之前的整體回歸，一次 session 做兩件事：

1. **回歸**：v7.1 pv7-line 到目前為止的產品，和 v7 穩定基準在同一台機器、同一個 session 交錯比較（D130）。三個 edge 批的 L2 已經各自驗過 A−B；這裡驗的是「v7 → 現在」整段，包括那些宣告「行為不變」、只由 L1 證明的批與 K1 的修正（D121）。
2. **影子資料**：給 `a1-shot-step` 決定 K（冷卻保留的上限）與 Client 的 S 取法（N 或 N＋1）。資料本身只記錄；資料完整性（D127 的兩項）是判定，不成立就是工具無效。

不在範圍：D44 事件回歸與 60 Hz 30 分鐘長測（屬於 `d44-regression-pv7`）；edge 類的延遲指標（d、to_tick、Match 收到的批數、替代率）和 v7 的比較（三個 edge 批的 L2 已在同一份 probe 下驗過）。

## 2. 兩棵樹

| 樹 | commit | 程式 | 判定工具 |
|---|---|---|---|
| B（v7 基準） | `6d3f287`（`../GYO-Engine-v7final`，pv6） | 該樹建置的 Match、Gateway、全部 probe、arena、GUI probe（16-L1 的 artifacts 清單，46 檔逐檔相符，D45 已核對） | 16-L2 的凍結清單（117 檔，`pvp-v7-16-prep-20261014/frozen.sha256` `8263d38c…`，由該樹的 `build/acceptance` 提供）；`judge_16.py`、`judge_c2.py` 的函式（只匯入函式，不用 `run_16.py` 與 `judge_16.verify_session`：它們檢查主 checkout 對 `6d3f287` 與 v7 的遠端分支，今天一定不成立）；timing 用 16-L2b 的 v7 HUD 規則 |
| A（現在） | 工具階段時的 HEAD（之後的 commit 只改文件），主 checkout | 主 checkout 建置的 Match、Gateway、全部 probe、arena、GUI probe（工具階段寫 artifacts 清單） | 凍結清單 `pvp-v71-pv7-checkpoint-20261018/frozen-pv7-cp.sha256`（`f036b2af…`）；A 的模組一律用路徑載入並核對雜湊（edge 系列 `judge_l2.py` 的做法），而且在 judge_16 之後載入（`judge_16b.load_v7` 釘住舊的 `gameplay_evidence`）；連點 runner 的雜湊換成 D131 之後的值 |

- 兩棵樹的 probe 各自建置（pv6 對 pv6、pv7 對 pv7），不共用；arena 內容相同（工具階段核對 digest）。**兩棵樹的 probe 相位可能系統性不同**（v7 D46⑦：建置不同時 probe 幀相對 Match Tick 的相位會移動，`legal_match_p95_ms`、d 不能跨建置比較），所以第 5 節只拿和 probe 相位無關、或有有來源下限的量來比。
- 權威（L1 部分，session 之前做）：`compare_authority_trees.py` B 對 A，scale 1、3 各 35 情境 0 差分；golden 30 個 0 不符。不成立就停下，不跑 session。

## 3. 項目與順序

每個項目在 B、A 各跑一次，B／A 的先後逐重複交替（第 1 輪 B 先）。不隨機化；不補跑、不重跑。閒置閘門、每項前後的 sleeper、STOP-REQUEST、逾時與 SIGTERM／SIGKILL 的規則照 edge 系列。順序 I → S → G（G 開視窗、最容易受干擾，排最後；16-L2 D45 檢查 8 的做法），G 停下時前面的資料已經完整。

| 部分 | 項目 | 每棵樹的次數 | runner |
|---|---|---|---|
| I | 25 案 | 3 輪（75） | `run_gameplay.py --case` |
| | 背壓（6 案） | 3 | `backpressure_probe.py --layers host-ipc gateway downstream --durations-ms 250 1000` |
| | 網路一般模式 | 3 | `run_network.py` |
| | 網路 impairments | 3 | `run_network.py --network-impairments` |
| | quad clean-60 | 1（5 輪） | `run_quad.py --case clean-60 --rounds 5` |
| | 長測短模式 | 3（各 2 循環） | `run_gameplay_soak.py --fps 60` |
| S（只 A） | Gateway→Match 延遲注入，T＝2.0 與 11.5 秒 | 各 3（6） | `run_short_stall.py --delay-ms 50 --window-ms 100 --fault-at T` |
| G（開視窗） | 1 GUI＋3 bot、action short、player short、GUI timing、2 GUI＋2 bot 三項 | 各 1（7） | 同 16-L2 的 G |
| | 連點 6 格（interval12、edge × 30／60／144） | 各 3（18） | `run_action_short.py --case` |

- 每棵樹 75＋3＋3＋3＋1＋3＋7＋18＝113 項，加 S 的 6 項，共 232 項。時間約 2.5 小時〔推論：用 edge 系列每項的實測牆鐘時間，遊戲 30.8 秒〕，工具階段以開發跑次更正。
- 長測短模式改成 3 次（第 1 版 1 次：範圍是 0，比較只剩下限，D45 S8）；連點改成每格 3 次（C2-B／L4 的覆蓋規則：每格 ≥3 個 P，D45 N8）。
- G 與連點要顯示器亮著、不碰滑鼠。

## 4. 每項的停止條件（任一棵樹不成立就停）

| 代號 | 條件 | 適用 | 來源 |
|---|---|---|---|
| R1 | 凍結判定 P（`judge_16.classify`；timing 用 16-L2b 的 v7 規則；連點用 `judge_c2.classify_rapid`；player144 的 `invalid` 只以 D11③ 接受）。A 的 F 停下；E／A 是工具或環境無效。**B 的 F**：屬於 v7 已知限制（`STABLE_BASELINE.md`「已知限制」）的，記為「B 基準的已知限制」、不停下；其他停下 | 全部（S 部分見 R1s） | 16-L2、edge 系列；D45 S1 |
| R1s | 延遲注入的 `run_case` 判定只記錄；分類看結束碼與 `injection_unproven`（edge-input 的寫法，D60） | S | `edge-input-passthrough-l2.md` J1e；D45 S9 |
| R2 | clean 跑次的權威 Cooldown 拒絕＝0；A 的連點權威 Cooldown（`judge_c2.rapid_cooldown`）＝0；B 的連點 Cooldown 只記錄（v7 已知限制 2，FireGate 殘餘） | clean-30／60／144、長測短模式、quad；連點 | 16-L2 停止條件 2；C2-B；D45 S1 |
| R3 | clean 跑次的非 respawn 重設＝0 | 25 案的 clean、長測短模式 | 16-L2 停止條件 3 |
| R3f | **待定 1**：故障情境的 D61 即時規則（故障窗外的非 respawn 重設，＝D44 的 E1）。建議照 16-L2：每次立旗標並計數，兩棵樹各自累計 ≥3 才停（16-L2 停止條件 5）；B 的 gateway-1000ms 依 K1（D118）只記錄 | 25 案的故障情境 | 16-L2 停止條件 5、`edge-rate-limit-l2.md` §3.1；D45 S1 |
| R4 | relay 結果任一秒 ≤31；Gateway `results_min_interval_ms` 印出值 ≥33.3，每個需要的玩家至少 1 個有值的視窗（不得空判）。**A 不判 `link_action_min_interval_ms`**（pv7 已刪這條，`edge-actions-passthrough-l2.md:95`；實測 33.2）；B 照 16-L2 的 `judge_c2.r_record` 判 | 25 案 | `edge-results-passthrough-l2.md:75` R4 (a)(b)(c)；D45 S6 |
| R8 | 逐出＝0；A 的 Match 動作批拒收＝0、每個送出的動作都有裁決（edge 系列 R8、R9）。A 第一次跑的情境（D45 S4 列的 16 案等）先只記錄 | 全部 | edge 系列；D45 N7 |
| K1a | Gateway 的限速拒收＝0（每個 Gateway 日誌、每位玩家，四個 `drop_rate_limited_*_packets`） | A 的全部項目 | `edge-rate-limit-l2.md` §3.1 |
| T1 | 三個 trace 都完整：`match-commands.jsonl`、`match-ingress.jsonl`、`match-shots.jsonl` 都有 `trace_end` 而且 `dropped`＝0（movement trace 兩次 drain 之間超過 8192 筆會丟，`MovementTrace.hpp:50-55`） | A 的所有傳 `--movement-trace` 的項目 | D45 S2 |
| H1 | 影子紀錄涵蓋 Client 收到的每個裁決，以 (player, action_id) 對上。Client 端的來源逐 runner：遊戲案例、長測、延遲注入用 `actions.jsonl`；quad 與 gui-bots 用每輪 `quad-client.json` 的 `clients[].decisions`（沒有時間欄位，只對 id；每輪是一次 Match session）；連點用 `create-rapid*.json` 的 clicks；GUI timing、action／player short、網路不判（沒有逐裁決的 Client 檔，只記錄影子紀錄筆數） | A | D127 的精神；D45 S5 |
| H2 | **對接（D127 (1)）**：session 邊界由 trace 推出（`snapshot_produced` 的 `authority_tick` 回落；影子紀錄的 `a` 回落），不用 `match.log`（它的行沒有時間戳，D45 M4）。在同一個 session 裡，每筆 `path`＝`on_time` 的紀錄以 (player, E, life, S) 對 `match-commands.jsonl` 的 resolved 事件：事件在紀錄之後、step_tick 的時刻之前加 1 秒的寬限〔寬限只防主機停頓，實測 on_time 的間隔約 1 Tick；窗口寬不會造成誤對〕，而且恰好 1 筆。`late` 紀錄對同一 session 內、紀錄之前最後一筆同鍵事件（只記錄來源）。`epoch`、`life` 的鍵預期沒有事件（請求的身分不是現在的），只記錄。`hold_limit` 只記錄 | A | D127、D128；D45 M3、M4、N6 |
| H3 | **對接率（D127 (2)）**：每筆 on_time 紀錄都要依 H2 對到 resolved 事件；**待定 2**：對不到的，先在同一份 trace 找產品行為的解釋（該 (player, E) 在 T(S) 之前被重設、同鍵的 `lifecycle_cancelled`、或先到 `trace_end`），有解釋的記為「有解釋的未對上」並列出，找不到的才是工具無效 | A | D127；D45 S2 |
| D | 受干擾：sleeper 最大 ≥10 ms 或連點分類 D。停止規則：同一情境、同一棵樹受干擾 ≥2 次，或全部累計 ≥6 次（v7.1 五次 session 的受干擾率 7／517≈1.35%，232 項的期望約 3.1 次；edge 系列的「第 3 次就停」在這個項數下約有一半機會因環境停下，D45 S3）。受干擾的跑次不進第 5 節的帶 | 全部 | edge 系列；D45 S3 |

## 5. 結束時的判定（只在完整的 session 上判）

指標的鍵與單位（D45 S8）：遊戲案例從 `result.json` 的 `movement` 算（P50、P95；Actual 比例＝actual／remaining_originals）；長測短模式用 `movement.actual_fraction` 與其 P50／P95；quad 用每輪 `result.json` 的 `command_evidence`（5 輪各取一值，取中位數與範圍）。背壓的 host-ipc／gateway 是 `backpressure.json` 的另一組，**不和 25 案的同名情境合併**。每個情境取每棵樹各跑次的值，比較中位數；受干擾的跑次不計。

| 代號 | 指標 | 情境 | 規則 | 來源 |
|---|---|---|---|---|
| C1 | 移動「產生→Actual」P95 | 25 案中有移動的全部情境（網路類除外）、quad、長測短模式 | median A − median B ≤ max(range B, 1 ms)（判 A 變慢的方向）；另記雙側差 | 1 ms：edge 系列 L1 的下限。Actual 的時序由 Client 的模擬步決定，和 probe 幀的相位無關（v7 第 04、09b 批：產生在步邊界） |
| C2 | 移動「產生→Actual」P50 | 同 C1 | 同 C1 | 同上 |
| C3 | Actual 比例 | 故障情境（clean 由 R1 的凍結判定判 ≥99%） | median B − median A ≤ max(range B, 2000÷B 已解析命令數 ‰) | `edge-input-passthrough-l2.md:97` J2 的下限（一個 2 秒窗口的量化）；v7 的故障情境三輪幾乎是決定性的（例如 socket-path-1000ms 三輪都 0.9873，D45 M1）；B 的 gateway-1000ms 受 K1 影響（0.965），A 預期較高 |
| C4 | 合法射擊 Match P95 | 有合法射擊的情境 | **待定 3**：(a) 只記錄（建議）；(b) 換成和相位無關的 Match 端「交接 Tick→裁決」；(c) 照判，下限 1 Tick | v7 D46⑦：probe 建置不同時這個量不能跨建置比較；A clean-60 的 10 跑次就有 9.7～16.0 ms（D45 M2） |
| C5 | 合法射擊 Client P95 | 同 C4 | **待定 3** 同上；(c) 的下限是 1 個結果間隔 33.3 ms（D107：新裁決最多晚 1 個間隔） | 同上 |
| C6 | 每案「每個 (player, action_id) 第一筆裁決的拒絕原因分布」 | 25 案、長測短模式 | A＝B（gateway-1000ms 依 K1 可以不同）；不相等時立旗標、列出 | 兩個協議的 `actions.jsonl` 格式相同（`judge_c2.gameplay_cooldowns` 兩邊都讀）；v7 三輪幾乎是決定性的（D45 S7） |
| C7 | network0／20／40、upstream-*、network impairments、duplicate-reorder-conflict | — | 只記錄 | edge 系列 |
| C8 | G 的 GUI 項目 | — | 兩棵都 P（R1）；可見交越、裁決抵達只記錄 | 16-L2 L2-6 |

- C1～C3、C6 不成立時，報告列出該情境、兩棵樹的值與範圍，**不自動當成退步**：交使用者判斷是否在線內 commit 之間二分（`pv7-design.md:138`）。
- 統計的讀法：每棵樹 3 次的中位數加上 B 的範圍，只在 v7 幾乎決定性的量（Actual、比例、拒絕分布）上有意義；延遲類的量在跑次之間變異大，3 對 3 只用 B 的範圍當帶時，沒有回歸也約有 8% 的情境會被標出（D45 M2 的模擬），所以 C4、C5 才列為待定。
- 只記錄：K1 的對照（B 的 gateway-1000ms 恢復後 starvation、A 沒有）；A 的 `max_session_window_packets`；兩棵樹的 Cooldown、重設原因；TimerBaseline 狀態。

## 6. 影子資料（只記錄，給 `a1-shot-step`）

在 T1、H1～H3 成立的前提下，對 A 的每個情境列出：

- 路徑分布（on_time／late／epoch／life／hold_limit）。**退回率**定義為 (late＋epoch)／全部，Client 的 S 取法用，clean 的目標 0；life 另列（跨生命的射擊，和 S 取法無關；開發跑次 clean-60 有 2／43 筆 life）。
- on_time 的領先 S−L(A) 與 step_tick−A 的分布（準時餘裕）。
- Shot 的冷卻保留量 `cooldown_hold_ticks` 的分布，尤其是「同一位玩家前一發走退回路徑、這一發 `cooldown_hold_ticks`>0」的次數（K 用）。
- 命令 S 的來源（H2 對上的 resolved 事件的 `source`：Actual／Held／Neutral）與晚到（`match-ingress.jsonl` 的替代紀錄）。
- view tick：`view_tick_invalid`、view_lag 的分布（v8 回溯窗口的參考）。

資料來源：25 案（clean、network20／40、upstream、各故障）、quad、長測短模式、延遲注入（probe，照 Client 規則填 E、S，D131）；連點 18 項、GUI timing、gui-bots（真正的遊戲本體）。報告分開列 probe 與遊戲本體的資料。

## 7. 預測（不是停止條件）

| 代號 | 預測 | 推翻條件 |
|---|---|---|
| P1 | C1、C2 都成立：Actual 的時序由 Client 的模擬步決定，v7.1 沒有改 Client 的移動 | 任一情境不成立 |
| P2 | B 的 gateway-1000ms 每跑次恢復後有 starvation（K1），A 沒有；C3 在 gateway-1000ms 上 A 較高 | 任一跑次相反 |
| P3 | A 的 clean 退回率（late＋epoch）為 0（開發跑次：clean-60 on_time 41／43，另 2 筆 life） | clean 有 late 或 epoch |
| P4 | 時間約 2.5 小時 | 實測差 30% 以上 |

## 8. 等你決定

1. **待定 1（R3f）**：故障情境的 D61 即時規則不成立時，(a)（建議）照 16-L2 立旗標並計數，同一棵樹累計 ≥3 才停；(b) 第 1 次就停。D44 的回歸屬於 `d44-regression-pv7`；v7 的觀測是 0／260，在約 150 個故障 case-run 裡出現 1 次並不罕見（D45 S1）。
2. **待定 2（H3）**：on_time 對不上時，先找產品行為的解釋（被重設、被取消、先到結尾），有解釋的只記錄、沒有的才是工具無效。這會改到 `protocol-v7.zh-Hant.md` §6 那一句（D127 寫的是「對不到就是工具無效」），所以要你同意。
3. **待定 3（C4、C5）**：(a)（建議）只記錄；(b) 改量 Match 端的交接→裁決；(c) 照判，下限 1 Tick 與 33.3 ms。
4. **A 的開發跑次**：D45 S4 列出 pv7 從沒在 L2 跑過的項目（25 案中的 16 案、網路一般模式、長測短模式、quad 5 輪、背壓的 downstream 與 250 ms、G 的全部 7 項）。建議核准前在 A 上各跑 1 次，只看分類、鍵與 T1、H1～H3，不看第 5 節的指標（D131 的開發跑次同一個做法）。
5. 核准本宣告（上面決定、工具與乾跑之後的版本）。
6. 什麼時候跑：約 2.5 小時加閒置門檻；G 與連點要顯示器亮著、不碰滑鼠；跑前關 Chrome、暫停 PR 的 Auto-fix。

## 9. D45 檢查的修正對應（第 1 版 → 第 2 版）

| 檢查項 | 修正 |
|---|---|
| M1 C3 套到故障情境一定不成立 | C3 改成故障情境的 B−A 帶（下限 2000÷B 已解析命令數 ‰）；clean 的 99% 由 R1 的凍結判定判；寫明各 runner 的鍵 |
| M2 C4、C5 不能跨建置比較；1 ms 下限小於變異 | 待定 3；第 2 節寫明 probe 相位的限制 |
| M3 H2 要求每筆都對到之後的事件 | H2 只判 on_time；late 對之前、epoch／life 預期沒有事件 |
| M4 `ipc closed` 沒有時間戳 | session 邊界改由 trace 的 tick 回落推出 |
| S1 一次 D44 事件就停、B 的 F 被當成工具無效 | 待定 1；R1 加「B 基準的已知限制」；R2 的 B 連點 Cooldown 只記錄 |
| S2 產品行為讓 on_time 對不上 | 待定 2；新增 T1（三個 trace 的 `trace_end` 與 `dropped`） |
| S3 受干擾第 3 次就停在 232 項上容易誤停 | D 改成同一情境同一樹 ≥2 次或累計 ≥6 次；受干擾的跑次不進帶 |
| S4 A 第一次跑的項目多 | 第 8 節第 4 項（核准前的 A 開發跑次）；G 排到最後 |
| S5 H1 的資料來源 | 逐 runner 寫出，沒有逐裁決檔的不判 |
| S6 A 不能用 `r_record` | R4 照 edge-results 的 (a)(b)(c)，A 不判 link 間隔 |
| S7 缺直接的行為不變檢查 | 新增 C6（拒絕原因分布 A＝B） |
| S8 指標的鍵與單位 | 第 5 節開頭寫明；長測短模式改 3 次；背壓不和 25 案合併 |
| S9 延遲注入的分類 | 新增 R1s |
| N1 項數 | 更正為每棵 113、共 232（含長測與連點的增加） |
| N2 不能沿用 16-L2 的 driver 與完整性檢查 | 第 2 節寫明只匯入函式、載入順序 |
| N3 R3b 的背壓一句沒有作用 | 刪除，K1 的例外併入 R3f |
| N4 單側方向 | C1、C2 另記雙側差 |
| N5 退回率定義不一 | 第 6 節統一為 (late＋epoch)，life 另列 |
| N6 H2 窗口的來源 | 改為「防主機停頓的寬限 1 秒」，寫明不是 D61 的寬限 |
| N7 拿掉的不變式 | 新增 R8 |
| N8 連點覆蓋 | 每格 3 次 |
| N9 A 的建置位置與連點 runner 的雜湊 | 第 2 節寫明 |
