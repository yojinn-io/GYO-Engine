# edge-results-passthrough 的 L2 事前宣告（草稿第 3 版，送核准）

- 狀態：**草稿，等使用者核准**（AGENTS.md §14.1）。
  - 第 1 版經 D45 的門檻來源對抗式檢查（1 位 xhigh，約 18 分）：必須修正 5、應修正 10、記錄 7；對應表在第 11 節。
  - 第 3 版：工具寫好並乾跑（57 種逐跑次情況、模擬 session、驅動檢查全部符合預期；A 沒有跑過任何項目；B 的開發跑次 9 個）。乾跑提出的 16 處措辭已寫進正文（第 12 節）。工具目錄 `build/target/_build/test/logs/pvp-v71-edge-results-l2-prep-20261017/`（`commands.txt`、`dryrun.md`、`prep-files.sha256` `5493e8df…`）。
- 依據：D94、D95（D-10）、D96、D107、D110、D111；protocol-v7 §2a、§4、§9；README `edge-results-passthrough` 列。
- 先例：[edge-input-passthrough 的 L2 事前宣告](edge-input-passthrough-l2.md)（D109、D110）。主機、凍結、雜湊核對、停止規則的細節沿用它，下面只寫不同之處。
- 數字沒有標「實測」的，是程式推導或推論。B 的基準與跑次間差距取自上一次 L2 的 A 樹跑次（`pvp-v71-edge-input-l2-20261017b/runs/*-A`；它的 Match、Gateway、arena 與本次的 B 逐位元相同，已核對），那些結果在定門檻之前就已公開，可以用來校準。

## 1. 目的

這批是行為改變：Gateway 不再保存結果帳本，Match 寫來的每個結果框架放進該玩家的單槽（新的蓋舊的），在「上次寫出完成＋1/30 秒」時放出；被蓋掉的計入回報 34。結果的重送與補送只由 Match 負責。link 原樣轉送 ACK-only 批（ACK 不超過退役值時也寫出）。GYOP 序號一律由唯一的 UDP 寫出者在寫出前指定（含 controls）。

L2 **判定**：遊戲行為與射擊時序沒有變差；結果的下行延遲沒有變差；結果送出守住 30 Hz；帳目與回報成立。

**只記錄、不判定**：`to_worker_ms`（產生到 Client 收到，大部分是上行與 tick）；回報 34 的次數（Gateway 的計數，含 `final=1` 行）；只帶退役的框架被丟後，到同 session 下一則 retired 不小於它的框架之間的時間；relay 上結果框架的種類分布；link 的 ActionBatch 寫出數（send statistics 的 `link_actions`，只有寫出的視窗；ACK-only 寫出數本身沒有計數器）與 relay 上 Client 的 ACK-only 資料包數；被 A5 擋下的批數（以前會被 A3 擋下的現在算這裡）；network 模式的延遲。

## 2. 兩棵樹

| 樹 | commit | worktree |
|---|---|---|
| before（B） | `6d9ecf5`（之後的 commit 只改文件） | `../GYO-Engine-ip-after`（已建置、乾淨） |
| after（A） | `f79bb10` | `../GYO-Engine-rp-after` |

- B→A 的程式差分：Gateway（`gateway/*.go`）、`runtime_v7.proto` 的 reserved 16、20（重產的 enum：Match 的 `DropReason_IsValid` 對它們判 `reason_range`；Match 的其他程式不變；兩棵樹的 Match 雜湊都記下）、測試與文件。
- probe 一律用 `../GYO-Engine-ip-before` 的建置（產生 B 基準語料的那一支，`ea5cf3ad…`），兩棵樹共用。

## 3. 工具

| 工具 | 雜湊 |
|---|---|
| `run_l2.py`（新寫） | `958469e89b0b48de5925f233d5f0b255d8b62017f57c4c1293b055cefd8744b3` |
| `judge_l2.py`（新寫） | `7c186078751be1f64dead469fa2dea9f3144718d6d0a964bdc84a4f9414b39d5` |
| `artifacts.sha256`（B、A 的 Match 與 Gateway；ip-before 的 4 個 probe 與 arena） | `34dc17664335c93a3e3bbc19d72fff7b478ddb69b0d36686443c6d27d0f62b26` |
| `frozen-pv7.sha256`（含 `action_latency_v7.py`、`ingress_evidence.py`、`gameplay_evidence.py`、`quad_evidence.py`、`backpressure_probe.py`） | `b4a8963001883160d5bb7642b7da89afd343c686d29752874461762e17a291c2` |
| `judge_16.py` | `29258be1…` |
| `judge_c2.py`（`pvp-v7-09-c2-prep-20261014/`） | `5d2ed0f3af7e5c7618c79cd850f663d6197e57c676fdcef8c9684ab3cac30355` |
| `compare_reports.py`、`sleeper.py` | `36e65f3a…`、`98105dbf…` |

B 的 Match `49622476…`、Gateway `fc3faabc…`；A 的 Match `1bd67b4e…`、Gateway `b81c6278…`；兩棵樹的 Gateway `vcs.revision` 等於各自的 HEAD、`vcs.modified=false`。

## 4. 主機

同 edge-input-passthrough 宣告第 4 節。

## 5. 情境 × 次數 × 順序

B、A 交錯，每個情境輪流先跑；每個項目 1 輪；soak 排在最後、B 先 A 後（每棵樹 1 次，沒有範圍可言）。

| 情境 | runner | 每棵樹 | 用途 |
|---|---|---|---|
| clean-60 | `run_gameplay.py --case clean-60` | 10 | 下行延遲、射擊時序、送出間隔、Cooldown |
| loss-shot、loss-result、loss-ack | `run_gameplay.py --case …` | 各 5 | §9 的 loss 模式；loss-result 丟帶裁決的框架 |
| host-ipc-1000ms、gateway-1000ms | `run_gameplay.py --case …` | 各 5 | README 的「含 host-ipc 停頓」：有動作的停頓 |
| network0、network20、network40 | `run_gameplay.py --case …` | 各 3 | §9 的 network 模式（下行 5% 隨機丟包，唯一會丟到只帶退役的框架） |
| network-impairments | `run_network.py --network-impairments` | 5 | 回歸（重連、Welcome：controls 序號的改動） |
| backpressure gateway、host-ipc | `backpressure_probe.py --layers gateway host-ipc --durations-ms 1000` | 5 | 回歸、丟棄回報（timing probe 不開槍，結果判定不適用） |
| quad clean-60 | `run_quad.py --case clean-60 --rounds 1` | 5 | 4 人的送出間隔 |
| soak-long | `run_gameplay_soak.py --soak --fps 60` | 1 | 長跑次的 Cooldown、逐出、下行延遲 |

共 120 個項目（每棵樹 60），10 輪。估計（B 的開發跑次與上一次 session 的時間）：每棵樹短項目 36.7 分、兩次 soak 66.5 分，共約 2 h 20 分加閒置門檻。

## 6. 判定

d＝每個已接受的裁決（每個 (player, action_id) 的第一筆 decision 紀錄），Client 收到的時刻（`actions.jsonl` 的 `received_ns`）減去它的 resolved tick 的第一筆 `snapshot_produced`（`match-commands.jsonl`）；兩者同一個 steady clock 時域。已接受的裁決缺 `received_ns` 或缺 tick 時刻＝工具無效。由 judge 自算（`action_latency_v7.py` 在 soak 沒有 `relay.json` 會失敗，只在其他情境記錄 `to_worker_ms`）。I＝1/30 秒＝33.33 ms。

### 6.1 逐跑次判定（任一棵樹不成立就立刻停）

| 代號 | 判定 | 適用 | 門檻與來源 |
|---|---|---|---|
| R1 | 既有判定與 D44 事件 | 所有項目 | 凍結分析器的判定通過；E1、E2、E3 都是 0（`judge_16.py`）。上一次 L2 與 v7 第 16 批都全數通過（實測） |
| R2 | 權威 Cooldown 拒收 | clean-60、loss-*、host-ipc／gateway-1000ms、soak：0（`judge_c2.gameplay_cooldowns`，讀 `actions.jsonl`）；quad：照凍結的 `quad_evidence.py`（ALLOWED 只有 Dead、StaleLife）；network：只記錄 | v7 第 16 批：loss、host-ipc、gateway 24 次都是 0；network0 有 1 次是 1（實測）。非 clean 模式的凍結判定允許 Cooldown，所以這裡另判 |
| R3 | 下行延遲上限 | clean-60、loss-shot、loss-ack、host-ipc／gateway-1000ms、soak：每個裁決 d ≤ I＋3 ms。loss-result：被丟框架裡的裁決（relay.json 中第一個帶該裁決的下行 kind 7 `received` 框架，同一 session、同一 time_ns 有 `dropped_*` 事件）d ≤ 2I＋3 ms，其餘 ≤ I＋3 ms。network：只記錄 | 單槽最多讓一個框架等一個間隔；Match 的通道最多晚一個間隔。B 實測：clean-60 每跑次最大 ≤1.9 ms、soak 最大 2.53 ms（受干擾的開發跑次見第 10 節第 3 項）；v7 的 loss-result 被丟框架的裁決 34.4～35.3 ms |
| R4 | 結果送出間隔（不變式） | 所有有結果的項目 | (a) `player send statistics` 的 `results_min_interval_ms` 印出值 ≥ 33.3（合法的 33.334 印成 33.3）；(b) 只適用 `run_gameplay.py` 項目（有 relay.json；quad、soak 只判 (a)）：每個 session 下行結果框架任一 1 秒內 ≤ 31 則（`judge_c2` 判定 3 的上限，涵蓋沒寫出的最後視窗）；(c) 不得空判：gameplay 與 soak 的「actions.jsonl 中有 ≥1 個裁決的玩家」、quad 的「所有有 send statistics 行的玩家」，各至少 1 個有值的視窗，否則算工具無效、停下。network-impairments 與 backpressure 沒有結果與動作流量，R4、R5 不適用 |
| R5 | link 動作送出間隔（不變式） | 所有有動作的項目 | `link_action_min_interval_ms` 印出值 ≥ 33.3；不得空判的規則同 R4 (c)。A 的 link 在 ACK 不超過退役值時也寫 ACK-only 批，佔用送出時段 |
| R6 | ingress 帳目 | 所有項目，兩棵樹同一張表 | 照 edge-input-passthrough 宣告 6.3 的 **A 欄**（I3 宣告的失敗形狀；被刪路徑的鍵為 0；D110 的 I1 規則）。I1 允許 undetermined 的名單加上 host-ipc-1000ms 的 gameplay 情境；network0／20／40 是單一 IPC 連線，必須 passed。另外：兩棵樹的 player ingress 行都必須有 `drop_invalid_actions_packets`、`_shots` 且為 0（A 只拿掉它們的回報對應，鍵照樣輸出）；`drop_results_over_rate_packets` 在 A 的 ingress 行必須存在，B 沒有這個鍵是預期的 |
| R7 | 丟棄回報 | 任何有 ≥1 則 Gateway 回報的項目 | Gateway 寫出與 Match 讀到的回報依 (player, report_sequence) 逐則相等、違規 0。network-impairments 重連後的 `gateway-reconnected.log` 不在比對範圍；若因此出現「只在 Match」的回報，judge 標記並回報（10 份語料都沒有發生） |
| R8 | 佇列溢出與逐出 | clean-60、quad、soak：`drop_ipc_queue_overflow_input_packets`＝0（上一批的 J5）；所有項目：逐出 0 | 上一次 L2 都是 0（實測） |

工具有效性（不成立就停，不算判定）：每個 `run_gameplay.py` 項目都有 `relay.json` 與 `actions.jsonl`；每個 clean-60 跑次至少 34 個帶 `received_ns` 的已接受裁決（B 實測：43 個裁決中 34 個已接受，跑次間不變）。

### 6.2 session 結束才判

session 完整＝完成的項目數等於排程數、名稱逐一相符；不完整就不判，回報。

| 代號 | 判定 | 門檻 | 來源與限制 |
|---|---|---|---|
| L1 | 下行延遲 | 每個情境（clean-60、loss-*、host-ipc、gateway；soak 只有 1 次，只記錄）：「每跑次 d 的 p95」的中位數，A−B ≤ max(B 的範圍, 1 ms) | B 的每跑次 p95 在 1.09～1.63 ms（實測）。33.3 ms（第 1 版）是跑次間差距的 8 倍，抓不到「每個框架多等一個間隔」的缺陷 |
| L2 | 射擊時序 | 與 L1 同一組情境（clean-60、loss-*、host-ipc、gateway；network 與 soak 只記錄）：「每跑次 `to_tick_ms`（產生到裁決 tick）的 p95」的中位數，A−B ≤ max(B 的範圍, 16.7 ms) | B 的範圍 11.22～14.64 ms（實測）；16.7 ms＝1 tick。A 的 link 多寫的 ACK-only 批會把之後 33 ms 內的射擊推到下一個時段 |
| L3 | 回報的覆蓋 | 兩棵樹各至少 1 個 R7 的合格項目；A 至少 1 則回報的內容有 `drop_results_over_rate_packets` > 0，沒有就記為「未驗到」（不算失敗） | 34 只在單槽被取代時出現 |

R4 與 L1 的覆蓋範圍（限制）：`player send statistics` 只在滿 10 秒時寫、只寫現存的玩家；clean-60 與 quad 每跑次只有 1 個視窗（clean-60 的玩家 1 涵蓋約 44／68 則結果）。R4 (b) 的 relay 檢查補上整個跑次。

## 7. 預測（不是停止條件）

| 代號 | 項目 | 預測 | 推翻條件 |
|---|---|---|---|
| P1 | clean-60 的 d 的 p95 | A 不比 B 慢 | A 的中位數 > B 的中位數＋B 的範圍 |
| P2 | 回報 34 | clean 很少；host-ipc 停頓釋放後較多 | clean 每跑次 > 10 |
| P3 | relay 上的結果框架，三類互斥（帶裁決／只帶退役／重複＝與同 session 前一則完全相同） | A ≈ B（B 的 clean 每跑次都是 86 則：43＋43、重複 0，實測） | 帶裁決或只帶退役差 > 20%，或 clean 的 A 出現重複 |

## 8. 停止條件與重試規則

- 不重跑、不補跑；所有證據保留；任一 6.1 不成立或工具無效就停在那個跑次，回報（§14.1）。
- 受干擾的跑次（跑次前或後的 sleeper 最大值 ≥ 10 ms）：記錄、不重跑；累計 3 次就停。
- L1、L2 某情境沒有值：記為「未判定」並回報，不算通過。
- session 結束後 6.2 不成立：回報。

## 9. 差異帶（給 `pv7-checkpoint`）

本 session 實測的 A−B（各指標的中位數差與範圍）。

## 10. 等你決定

1. **核准本宣告**。
2. **D112：protocol-v7 §9 的「每隔一則丟一則時 ≤2⌈n/8⌉ 則」**。照 §2a 的設計實測只能 ≤2⌈n/8⌉＋2（n＝16：6 則；24：8 則；32：10 則）。原因：Client 只能連續 ACK，有缺口時 Match 不知道 Client 已有哪些較大的 id，循環槽位會在一個收到的框架裡重送它已有的 id。選項：(a) §9 改成 ≤2⌈n/8⌉＋2（建議；契約行為不變）；(b) 讓 Client 回報缺口，達到 2⌈n/8⌉（wire 變更）。
3. **R3（下行延遲上限 I＋3 ms）遇到主機干擾時怎麼處理**。B 的開發跑次 `clean-60-r1-B` 有一個裁決 d＝40.3 ms（上限 36.3），當時 Chrome 與 Claude app 忙碌，跑次後的 sleeper 38.9 ms、那一槍的 `to_tick` 32 ms、Client 送了兩次。9 個開發跑次有 8 個 sleeper ≥10 ms。照現在的規則，受干擾的跑次只要 R3 超標就會停下整個 session。選項：(a) R3 只在沒有受干擾的跑次停下；受干擾的跑次 R3 超標只記錄（受干擾累計 3 次就停的規則不變；系統性的變慢由 L1 的 session 比較抓）（建議）；(b) 維持嚴格，任何超標都停（等於要求整段時間完全閒置）。選 (a) 要改 `judge_l2.py` 一處、重算雜湊。
4. **什麼時候跑**：約 2 h 20 分加閒置門檻，需要機器閒置（插電、不移動、不使用）。上一次在夜間跑，98 項只有 1 項受干擾；白天的開發跑次 9 項有 8 項受干擾，照「受干擾累計 3 次就停」很快就會停。建議在你不使用電腦的時段跑（例如今晚）。

## 11. D45 檢查的修正對應（第 1 版 → 第 2 版）

| 檢查項 | 修正 |
|---|---|
| M1 backpressure 沒有結果流量，R3、P2 空判 | 加 `run_gameplay.py` 的 host-ipc／gateway-1000ms；backpressure_probe 只留給回歸與回報；R4 (c) 不得空判 |
| M2 network-impairments 沒走結果路徑 | 加 network0／20／40（gameplay）；network-impairments 只當回歸；network 的延遲只記錄 |
| M3 L1 的指標與差異帶 | 改成 judge 自算的下行段 d；逐跑次上限（R3）與 session 的 A−B 帶（L1）；`to_worker_ms` 只記錄；soak 不用 `action_latency_v7.py` |
| M4 射擊時序沒有判定 | L2（`to_tick_ms`）與 R5（link 的送出間隔不變式）；記錄 link 的 ACK-only 寫出數 |
| M5 R4 沿用上一份的表會在 B 停 | 兩棵樹都用上一份的 A 欄；I1 名單；新鍵的存在規則 |
| S1 R3 是同義反覆 | 標成不變式、比對印出值；另加 relay 每秒上限 |
| S2 L2（每 10 秒 ≤301）可由 R3 推出 | 刪除，改用 relay 每秒上限 |
| S3 視窗覆蓋 | 寫進 6.2 的限制 |
| S4 R2 的來源與範圍 | 擴大到 loss、停頓；quad 用凍結判定；network 只記錄 |
| S5 R7 沒驗到 34 | 合格改成「有回報的任何項目」；L3 要求 A 至少 1 則帶 34（否則記錄未驗到） |
| S6 退役遺失的恢復 | network 模式記錄恢復時間與次數；證明靠 Go 測試 |
| S7 P1 的推翻條件 | 改成「＞B 的中位數＋B 的範圍」 |
| S8 P3 的來源 | 改用 relay 分三類計數 |
| S9 B 的開發跑次 | 乾跑階段先在 B 跑 loss、停頓、network 的開發跑次 |
| S10 第 8 節太簡略 | 寫明完整性、工具有效性、無值規則 |
| N1～N7 | N1 `action_latency_v7.py` 已凍結；N2 第 2 節填 A 與 probe 來源；N3 第 2 節改寫；N4 記錄 A5 的計數；N5 soak 順序；N6 不影響解析；N7「錨在選取時」的缺陷 L2 看不到，Go 測試 `TestResultsIdleWithoutRepeatsAndANewDecisionGoesOutAtOnce` 涵蓋閒置後立刻送出 |

## 12. 乾跑提出的措辭修正（第 2 版 → 第 3 版）

| # | 修正 |
|---|---|
| 1 | 工具有效性的「43」是裁決數；已接受的是 34（跑次間不變），門檻 34 等於 B 的實際值 |
| 2 | R3 遇到主機干擾的處置：列為第 10 節第 3 項 |
| 3 | R4 (b) 只適用 gameplay 項目；network-impairments、backpressure 不適用 R4、R5 |
| 4 | R4 (c) 的「每位玩家」定義；R5 也適用不得空判 |
| 5 | d 的定義（第一筆 decision、第一筆 snapshot_produced）；缺值＝工具無效 |
| 6 | loss-result 被丟框架的辨識方式 |
| 7 | L2 的情境組與 16.7 ms 的字面值 |
| 8 | link 的 ACK-only 寫出數沒有計數器，改記 `link_actions` 與 relay 的 ACK-only 數 |
| 9 | P3 三類互斥與推翻條件 |
| 10 | 只帶退役的框架被丟後的恢復時間定義（B 的 network 開發跑次：33.4、33.5、109.8 ms） |
| 11 | A3 的鍵在兩棵樹都要存在且為 0 |
| 12 | L3 以鍵名判斷 34 |
| 13 | R7 不比對 `gateway-reconnected.log`，出現時標記 |
| 14 | soak 的 d 最大值改成這個 B 的 2.53 ms |
| 15 | 估計時間改成約 2 h 20 分 |
| 16 | quad 的 R2 即凍結判定；loss 注入由凍結分析器檢查，次數記錄 |
