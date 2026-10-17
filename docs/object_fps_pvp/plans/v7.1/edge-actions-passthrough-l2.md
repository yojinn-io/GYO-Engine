# edge-actions-passthrough 的 L2 事前宣告（第 4 版，2026-10-18 使用者預先核准，D117）

- 狀態：**定稿，第 4 版**（D117：第 3 版＋Match 的 `IpcHost::Stop()` 與 rapid runner 停止順序兩個修正的 A commit 與雜湊，門檻與判定不變；使用者預先核准）。第 3 版（D115）的第 1 次 session 在第 29 項停下（rapid runner 收尾競態，HANDOFF 2026-10-18 條目），保留在 `pvp-v71-edge-actions-l2-20261017/`。第 4 版的改動在第 13 節。以下保留第 3 版的文字（§14.1：核准後不放寬）。
  - 第 1 版經 D45 的門檻來源對抗式檢查（1 位 xhigh，約 12 分）：必須修正 3、應修正 6、記錄 8，對應表在第 11 節。
  - 第 3 版：工具寫好並乾跑（82 種逐跑次情況、驅動檢查、5 個模擬 session 全部符合預期；真實的 B 語料 66 跑次判成 B 沒有任何停止；A 沒有跑過任何項目）。B 的 rapid 開發跑次 6 次（第 5 節）。工具目錄 `build/target/_build/test/logs/pvp-v71-edge-actions-l2-prep-20261017/`（`commands.txt`、`dryrun.md`、`prep-files.sha256` `c73ed715…`）。第 2 版到第 3 版的改動在第 12 節。
- 依據：D94、D96、D107、D114；protocol-v7 §2a、§4、§9；README `edge-actions-passthrough` 列（「射擊到達與 action_latency、權威 Cooldown＝0、Expired 率、被閘門擋下的點擊數」）。
- 先例：[edge-results-passthrough 的 L2 事前宣告](edge-results-passthrough-l2.md)（D113）與 [edge-input-passthrough 的](edge-input-passthrough-l2.md)（D109、D110）。主機、凍結、雜湊核對、受干擾的處置沿用它們，下面只寫不同之處。rapid 項目的量法與分類沿用 v7 第 09 批 C2-B（`pvp-v7-09-c2-prep-20261014/judge_c2.py` 的 `classify_rapid`、`rapid_cooldown`）。
- 數字沒有標「實測」的，是程式推導或推論。「本次 B」的參考值取自上一次 L2 的 A 樹跑次（`pvp-v71-edge-results-l2-20261017b/runs/*-A`；它的 Match、Gateway 與本次的 B 逐位元相同，工具階段核對）。**門檻裡的「B 的範圍」一律用本 session 的 B 跑次計算**，參考值只用來說明門檻的大小。rapid 在 v7.1 沒有基準：乾跑時先在 B 跑開發跑次；B 的開發跑次若出現權威 Cooldown 拒收，就停下、在核准前回報。

## 1. 目的

這批是行為改變：Gateway 不再保存動作視窗。每個解碼成功的動作 datagram 原樣成為一則 IPC 訊息，排進每位玩家的 FIFO（與輸入同一個佇列、分開計數，各 64 則，滿了丟同種類最舊的），照到達順序寫給 Match；link 不再持有到 30 Hz 時段、不再合併、不再重送，A5（視窗驗證、玩家數上限）刪除。重複、衝突、ACK-only、未知 ACK 都交給 Match 判定。

可能的影響與能看到它的情境：
- link 的持有：Client 本身已用同一個期限節拍到 30 Hz（`ClientConnection.cpp:446-449`），本次 B 的 `link_action_interval` 最小 33.7 ms（實測），link 很少真的持有。預期射擊時序 A≈B。
- 合併與重送：本次 B 在 clean-60、loss-shot、loss-ack、host-ipc-1000ms、quad、soak 共 41 跑次，Match 收到的動作批都等於 Gateway handed（例如 clean-60 每跑次兩位玩家 68／18，實測），也就是這些情境裡 A 和 B 的批序列相同。有差的只有 gateway-1000ms（B 的 handed 93～95、Match 收到 63～66，停頓後 link 合併）、network0／20／40（多 5～21）與 loss-result（多 1）：A 會把這些批全部送到 Match。
- 寫出順序：以前同一輪先寫輸入再寫動作，現在照到達順序。Client 同一輪先送輸入、再送動作（`ClientConnection.cpp:680-695`），本機路徑上順序不變；FireGate 的最早 tick 推導引用的「Gateway 先寫輸入」（`FireGate.hpp:31-32`）已成過時的註解，影響在微秒級（記在 HANDOFF 給 A1）。
- FireGate 的 `FireGatePendingSpreadTicks`＝7 把 link 的持有算進去；這批之後偏保守（README 的註記），被閘門擋下的點擊數只記錄，常數留給 A1。

L2 **判定**：權威 Cooldown＝0（遊戲情境與經過產品 FireGate 的連射）；Match 不拒收任何動作批、每個送出的動作都有裁決；射擊時序沒有變差；Expired 沒有變多；帳目（Gateway 的封包守恆、relay→Match 的端到端核對）與回報成立；既有判定不退步。

**只記錄、不判定**：被閘門擋下的點擊數、送出數、`submit_to_verdict`（rapid 的 interval12 格）；Match 收到的動作批數；`link_action_min_interval_ms`（A 的 link 不再節拍）；`to_worker_ms`；network 模式的延遲與 Cooldown。

**L2 走不到的路徑（限制）**：Leave 與踢出時的佇列（本次 B 的 D56 丟棄鍵全部是 0，R8 又要求逐出 0）、FIFO 溢出（33，預期未驗到）、寫出失敗時的放棄。它們只由 Go 測試（`TestLinkConservesActionBatchesAcrossEveryExit` 等）與突變（`v71ap-*`）覆蓋。

## 2. 兩棵樹

| 樹 | commit | worktree |
|---|---|---|
| before（B） | `f79bb10`（之後的 commit 只改文件） | `../GYO-Engine-rp-after`（已建置、乾淨） |
| after（A） | `5b4d626`（xhigh 程式碼審查的修正之後；之後的 commit 只改文件） | `../GYO-Engine-ap-after`（已建置、乾淨） |

- B→A 的程式差分：Gateway（`gateway/*.go`）、`runtime_v7.proto` 的 reserved 17、21、25、26（重產的 enum：Match 的 `DropReason_IsValid` 對它們判 `reason_range`；Match 的其他程式不變；兩棵樹的 Match 雜湊都記下）、測試、突變與文件。Client 不變（工具階段以 `git diff` 核對 `apps/object_fps_pvp/src`、`include` 只有生成碼的差異）。
- gameplay、timing、network、quad probe 與 arena 一律用 `../GYO-Engine-ip-before` 的建置（上兩次 L2 的那一支）。rapid 的 GUI probe 用 B 樹的建置，兩棵樹共用。

## 3. 工具

| 工具 | 雜湊 |
|---|---|
| `run_l2.py`（改寫） | `91b2d7462c2d5bd3e39a320e2b66262cb05a28b2ed5292d63c8eb729ff2f41d3` |
| `judge_l2.py`（改寫） | `42708b4c93e641cfbc8efad4aa8444400e988efa254805909bddfb49f57a6023` |
| `artifacts.sha256`（B、A 的 Match 與 Gateway；ip-before 的 4 個 probe 與 arena；B 的 GUI probe） | `ee153ca0fb6df40de46ef250e8e35ba2549d8d946d69ca19b4a454833538e8bc` |
| `frozen-pv7.sha256` | `b4a8963001883160d5bb7642b7da89afd343c686d29752874461762e17a291c2` |
| `run_action_short.py` | `3a4e39a24de49684dd3474dab810b41737083f57530c345a333d7ae1f893d5e2` |
| `judge_16.py`、`judge_c2.py`、`compare_reports.py`、`sleeper.py` | 同上一次（`29258be1…`、`5d2ed0f3…`、`36e65f3a…`、`98105dbf…`） |

B 的 Match `1bd67b4e…`、Gateway `b81c6278…`（與上一次 L2 的 A 逐位元相同）；A 的 Match `77e83f91…`、Gateway `82f9da97…`；GUI probe `b1a9b53a…`；兩棵樹的 Gateway `vcs.revision` 等於各自的 HEAD、`vcs.modified=false`。B→A 在 `apps/object_fps_pvp/src`、`include` 與 `build/acceptance` 沒有差異（已核對）。

## 4. 主機

同 edge-input-passthrough 宣告第 4 節；rapid 需要顯示器亮著、不碰滑鼠與鍵盤（SDL 注入點擊，`run_action_short.py`）。

## 5. 情境 × 次數 × 順序

B、A 交錯，每個情境輪流先跑；每個項目 1 輪；soak 排在最後、B 先 A 後。不補跑。

| 情境 | runner | 每棵樹 | 用途 |
|---|---|---|---|
| clean-60 | `run_gameplay.py --case clean-60` | 10 | 射擊時序、Cooldown、Expired、帳目（批序列兩棵樹相同） |
| loss-shot、loss-result、loss-ack | `run_gameplay.py --case …` | 各 5 | 上行丟動作、下行丟結果（loss-result 的 Client 重送在 B 會被合併 1 則）、丟 ACK |
| gateway-1000ms | `run_gameplay.py --case …` | 5 | Gateway 停頓後讀端爆量：A 把 B 會合併的約 30 則都送到 Match（拒收、Expired、溢出 33） |
| host-ipc-1000ms | `run_gameplay.py --case …` | 5 | Match 端停頓（寫出進 kernel 與 relay，不會填滿 FIFO） |
| network0、network20、network40 | `run_gameplay.py --case …` | 各 3 | 隨機丟包與延遲：A 多送的批（拒收、裁決完整）；延遲與 Cooldown 只記錄 |
| network-impairments | `run_network.py --network-impairments` | 5 | 回歸（兩個 Gateway 行程，帳目逐檔判） |
| backpressure gateway、host-ipc | `backpressure_probe.py --layers gateway host-ipc --durations-ms 1000` | 5 | 回歸、丟棄回報 |
| quad clean-60 | `run_quad.py --case clean-60 --rounds 1` | 5 | 4 人 |
| rapid-interval12、rapid-edge × 30／60／144 FPS | `run_action_short.py --case rapid-…` | 每格 4（共 24） | 經過產品 FireGate 的連射：Cooldown（主要的檢出力）、被擋下的點擊 |
| soak-long | `run_gameplay_soak.py --soak --fps 60` | 1 | 長跑次 |

共 168 個項目（每棵樹 84）。估計約 2 h 46 分加閒置門檻（`run_l2.py --list-only`；rapid 以每項 32 秒計，B 的開發跑次實測 14～21 秒加兩次 sleeper）。

B 的 rapid 開發跑次（2026-10-17，各 1 次）：全部 class P、權威 Cooldown 0、sleeper 約 4 ms。interval12 在 30／60／144 FPS 都是 44 次點擊全部送出、擋下 0，`submit_to_verdict` 的 p50 34.0／17.2／14.5 ms；edge 送出 31／31／32 發、擋下 114／255／635 次，p50 34.0／17.7／14.7 ms。L4 的 40 發在 edge 格要 2 個跑次，排了 4 個。rapid 的輸出巢狀是 `runs/<項目名>/<case>/action-short-matrix.json`（沒有 `match-commands.jsonl`、`match-ingress.jsonl`），工具階段照實際輸出寫進 judge。

## 6. 判定

to_tick＝`action_latency_v7.py` 的 `to_tick_ms`：每個 rejection＝0 的裁決（射擊與換彈）的裁決 tick 第一筆 `snapshot_produced` 減去它的 `submitted`。Expired＝`actions.jsonl` 每個 (player, action_id) 第一筆 decision 的 rejection 是 Expired 的個數。「Match 的動作拒收」＝Match 的 ingress 行（含 `final=1`）所有 `*_actions_batches` 拒收原因（over_batch、malformed、resetting、invalid_player、invalid_batch、conflict、outside_window、full）的和。

### 6.1 逐跑次判定（任一棵樹不成立就立刻停）

| 代號 | 判定 | 適用 | 門檻與來源 |
|---|---|---|---|
| R1 | 既有判定與 D44 事件 | gameplay、quad、network、backpressure、soak：同上一次 R1。rapid：`judge_c2.classify_rapid`，P 成立；E、A＝工具無效（停）；D＝受干擾（計入第 8 節的 3 次）；E1～E3 不適用 | 上一次 L2 全數通過；C2-B 的 after 18 跑次都是 P（實測） |
| R2 | 權威 Cooldown 拒收 | clean-60、loss-*、host-ipc／gateway-1000ms、soak：0（`judge_c2.gameplay_cooldowns`）；quad：凍結的 `quad_evidence.py`；network：只記錄 | 本次 B 參考：這些 gameplay 情境 36 跑次（clean 10、loss 15、host-ipc 5、gateway 5、soak 1）都是 0；network20 有 1 次是 1（實測）。限制：gameplay 已接受射擊的裁決 tick 最小間距在 clean 是 15、loss-shot 13、gateway-1000ms 10（冷卻 10 tick），對 FireGate 類缺陷幾乎沒有檢出力，主要靠 R2r |
| R2r | rapid 的權威 Cooldown 拒收 | rapid 每個 class P 跑次：`judge_c2.rapid_cooldown`（create 取 max(產品計數, 逐點擊歸因)，加 join 的計數，扣掉刻意注入；rapid 模式沒有注入）＝0；計數不明＝工具無效 | C2-B 的 after：interval12、edge 在 30／60／144 FPS 各 3 次都是 0（實測，pv6 時的樹）；本次 B 由乾跑的開發跑次確認 |
| R3 | 結果下行延遲上限（回歸） | 同上一次 R3（含 D113：受干擾的跑次只記錄） | 結果路徑在這批沒有改動；保留是因為 `sendUDP` 與 link 共用 Gateway 的寫出 |
| R4 | 結果送出間隔（不變式） | 同上一次 R4 | 同上 |
| R6 | ingress 帳目 | 所有項目 | 同上一次 R6（兩棵樹同一張表；rapid 允許 trace 缺檔）。鍵的規則（兩棵樹）：`drop_results_over_rate_packets` 必須存在；`drop_link_rejected_actions_packets`、`_shots`、`drop_malformed_actions_packets` 必須存在且為 0（本次 B 全部 0；A 把理論上到不了的 `ErrActions` 計到 malformed）。只有 A：`fwd_link_written_actions_packets`、`drop_ipc_queue_overflow_actions_packets`、`drop_cleared_on_close_actions_packets`、`drop_abandoned_on_close_actions_packets` 必須存在，D56／D59 的 shot、ACK 鍵必須存在且為 0；B 沒有這四個鍵是預期的，不判 |
| R6a | Gateway 的動作封包守恆（新） | A 的所有項目 | 每位玩家整個跑次（含 `final=1` 行；network-impairments 每個 Gateway 日誌檔各自判）：`fwd_handed_actions_packets`＝寫出＋溢出＋關閉時清掉＋放棄 |
| R6b | Match 收到＝Gateway 寫出（新） | A 的 gameplay、quad、soak，前提成立時 | 前提（逐條，任一不成立＝undetermined，記錄、不停）：Match 只有一次 IPC 連線（`ipc closed` 至多一次且在最後一個視窗之後）；整個跑次所有玩家的 `drop_cleared_on_close_actions_packets`、`drop_abandoned_on_close_actions_packets` 都是 0。Match 在判斷 reset 與玩家之前就計入收到數（`MatchRuntimeHost.cpp:263-270`），所以 D110 的 reset 競態不影響這個等式。判法照 `judge_i1`：先比整條 link 的總和（Match 側含 player＝0 的桶：`IngressBucketId` 把不在 Match 的玩家記到 0，`MatchRuntimeHost.cpp:95-97`），再比每位玩家（排除被逐出的玩家）。來源：本次 B 的 41 跑次 Match 收到＝handed（實測），A 的寫出取代 B 的 handed |
| R6c | relay→Match 端到端（新） | A 的 gameplay（有 relay.json），R6b 的前提成立時 | 每位玩家：relay 上行 kind 6 的 forwarded 數＝Match 的 `received_actions_batches`＋Gateway 對動作 datagram 的丟棄（`drop_{rate_limited,unauthorized,stale_sequence,malformed,inactive}_actions_packets`）＋溢出＋清掉＋放棄。不依賴 Gateway 的 handed 與寫出計數。本次 B 的 clean-60：relay 86、Match 68＋18（實測） |
| R7 | 丟棄回報 | 同上一次 R7 | 同上 |
| R8 | 佇列溢出與逐出 | clean-60、quad、soak：`drop_ipc_queue_overflow_input_packets`＝0，A 另外 `_actions_packets`＝0；所有項目：逐出 0 | 上一次 L2 輸入溢出在所有項目都是 0（包含 gateway-1000ms，實測） |
| R9 | Match 不拒收、裁決完整（新） | 兩棵樹的 gameplay、quad、soak | (a) Match 的動作拒收＝0；(b) gameplay 與 soak：`actions.jsonl` 每個 submitted 的 (player, action_id) 都有 decision | 本次 B：所有情境拒收 0、gameplay 每跑次 43／43（實測）。A 在 gateway-1000ms、loss-result、network 會把 B 合併掉的批送到 Match，重複與重送必須被 Match 接受為冪等 |

上一次的 R5（link 動作送出間隔 ≥33.3）刪除：A 的 link 不再節拍，這是本批刻意改變的行為；`link_action_min_interval_ms` 改成只記錄。

工具有效性（不成立就停，不算判定）：同上一次；rapid 的每個跑次有每次點擊的紀錄與產品計數。

### 6.2 session 結束才判

session 完整＝完成的項目數等於排程數、名稱逐一相符；不完整就不判，回報。

| 代號 | 判定 | 門檻 | 來源與限制 |
|---|---|---|---|
| L1 | 射擊時序 | clean-60、loss-*、host-ipc、gateway：「每跑次 `to_tick_ms` 的 p95」的中位數，A−B ≤ max(B 的範圍, 1 ms) | 本次 B 參考：每跑次 p95 的範圍 clean-60 3.65 ms、loss-shot 5.00、loss-result 1.88、loss-ack 3.60、host-ipc 2.16、gateway 1.58（實測）；上一次 L2 同一組情境的 A−B 都在 B 的範圍內。第 1 版的 16.7 ms 是範圍的 4.6 倍，抓不到 1 tick 以下的變慢（例如 link 少了 `notify`，動作改等下一次輸入喚醒） |
| L2 | Expired | clean-60、loss-*、soak：每跑次 0（兩棵樹）；host-ipc、gateway：每跑次 Expired 數的中位數，A−B ≤ max(B 的範圍, 1) | 本次 B 參考：clean、loss、soak（4971 個裁決）都是 0；host-ipc 每跑次 1；gateway-1000ms 3～4 |
| L3 | 回報的覆蓋 | 兩棵樹各至少 1 個 R7 的合格項目；A 的回報有 `drop_ipc_queue_overflow_actions_packets` > 0 時記為已驗到，沒有就記「未驗到」（不算失敗） | 33 只在 FIFO 溢出時出現；預期未驗到 |
| L4 | rapid 的覆蓋 | 每格每棵樹至少 3 個 class P 跑次、合計送出 ≥40 發（C2-B 的規則） | 不足就是「未判定」，回報 |

R2r、R9、L1 是這批行為的主要判定；R6a～R6c 驗帳目。

## 7. 預測（不是停止條件）

| 代號 | 項目 | 預測 | 推翻條件 |
|---|---|---|---|
| P1 | clean-60 的 `to_tick_ms` p95 | A≈B（Client 已節拍到 30 Hz，link 很少持有） | \|A 的中位數−B 的中位數\| > B 的範圍 |
| P2 | Match 收到的動作批 | clean-60、loss-shot、loss-ack 兩棵樹每跑次相等；gateway-1000ms、network* 的 A ≈ B 的 handed（> B 的 Match 收到） | clean 的兩棵樹不等，或 gateway 的 A 的中位數 ≤ B 的中位數 |
| P3 | rapid 被閘門擋下的點擊、送出數 | edge 格 A≈B；interval12 格兩棵樹都是 0 | edge 任一格差 > 10%，或 interval12 任一格差 > 3 次 |
| P4 | 回報 33 | 所有項目 0 | 任一項目 > 0 |

## 8. 停止條件與重試規則

同上一次第 8 節（不重跑、不補跑；受干擾累計 3 次就停；R3 只在沒受干擾的跑次停下）。rapid 的受干擾＝sleeper 規則或 class D（滑鼠干擾），兩者都計入 3 次。

## 9. 差異帶（給 `pv7-checkpoint`）

本 session 實測的 A−B（各指標的中位數差與範圍）。

## 10. 等你決定

1. **核准本宣告第 3 版**。
2. **什麼時候跑**：約 2 h 46 分加閒置門檻，需要機器閒置（插電、不使用）；rapid 需要顯示器亮著、不碰滑鼠與鍵盤。上兩次的經驗：白天的干擾（Chrome 等）容易累計到 3 次就停，建議在你不使用電腦的時段跑，開始前結束 Chrome，Auto-fix 在 session 期間暫停。

## 11. D45 檢查的修正對應（第 1 版 → 第 2 版）

| 檢查項 | 修正 |
|---|---|
| M1 R6b 要求乾跑看 A、等式沒定義 | 前提逐條寫出；照 `judge_i1` 先比總和（含 player＝0）再比每位玩家；來源改用 B 的 41 跑次，不看 A |
| M2 rapid 被 R1／R6 套用會在第一項停下 | rapid 的 R1 用 `classify_rapid`；R6 允許 trace 缺檔；寫明輸出巢狀 |
| M3 重送到 Match 的變化在會發生的情境沒有判定，P2 必推翻 | 新增 R9（拒收 0、裁決完整），適用到 gateway-1000ms、loss-result、network；P2 改寫 |
| S1 L1 的 16.7 ms 太寬 | 改成 max(B 的範圍, 1 ms)；補 `notify` 的突變 |
| S2 R2 的來源次數與檢出力 | 改成 36 跑次；寫進限制，主要靠 R2r |
| S3 R2r、L4 的定義 | 用 `rapid_cooldown`；有效＝class P；class D 計入受干擾；每格排 4 次；B 的開發跑次有 Cooldown 就停下回報 |
| S4 鍵的規則會誤判 B | 分成兩棵樹與只有 A 的規則；A5 與 malformed 的鍵在兩棵樹都必須是 0 |
| S5 只靠 Gateway 自己的計數 | 新增 R6c（relay→Match） |
| S6 soak 的 Expired | L2 加 soak＝0 |
| N1 範圍寫死 | 前言寫明範圍在本 session 計算 |
| N2 P3 的 10% 沒有分母 | interval12 改用絕對差 3 次 |
| N3 用途欄 | 改寫第 5 節 |
| N4 P1 的依據 | 改成 A≈B 並寫依據 |
| N5 Leave／逐出走不到 | 寫進第 1 節的限制 |
| N6 FireGate 註解過時 | 第 1 節記錄，HANDOFF 給 A1 |
| N7 to_tick 的對象 | 第 6 節改成「rejection＝0 的裁決（射擊與換彈）」 |
| N8 network-impairments 兩個 Gateway | R6a 逐檔判 |

## 12. 工具與乾跑帶來的改動（第 2 版 → 第 3 版）

| # | 改動 |
|---|---|
| 1 | 第 1 節：FIFO 改成「與輸入同一個佇列、分開計數，各 64 則，滿了丟同種類最舊的」（xhigh 程式碼審查的必須修正，`5b4d626`；§2a 的「每位玩家不少於 64 則輸入窗口」） |
| 2 | 第 2 節：A＝`5b4d626`；第 3 節填入雜湊；Client 沒有差異的核對 |
| 3 | R6b：Match 在判斷 reset 與玩家之前就計入收到數（`MatchRuntimeHost.cpp:263-270`），刪掉「D110 的 reset 競態只記錄」 |
| 4 | 第 5 節：B 的 rapid 開發跑次結果與估計時間 |
| 5 | 33 的 ACK-only 樣本：action id 為 0（審查的應修正），不影響判定 |

## 13. 第 3 版 → 第 4 版（D117）

| # | 改動 |
|---|---|
| 1 | A＝`67d7da7`：`5b4d626`＋Match 的 `IpcHost::Stop()` 對還連著的 session 記錄結束（`reason=stopped`，不重設 Match）＋rapid runner 的停止順序（客戶端 → Gateway，等 Match 記下 session 結束（上限 2 秒）→ Match）。A 的 Match `8961b8a0…`、Gateway `36009f47…`（程式同 `5b4d626`，`vcs.revision` 不同）；B 不變 |
| 2 | `run_action_short.py` `89e54bb8b0850327d99f3fd721cd2d2d1511deea13312de6d91cc479b6aee74b`；凍結清單 `frozen-pv7-d117.sha256` `4ff8b52ba506b2c3d8f4f59d57e52e888fd2a2da69bd94ff7e03732725fac6c6`（＝`frozen-pv7.sha256` 只更新 `run_action_short.py` 與 `test_run_action_short.py` 兩行） |
| 3 | 工具目錄 `build/target/_build/test/logs/pvp-v71-edge-actions-l2-prep-20261018/`：`run_l2.py` `843d272d70ff2dc2b9065fe1e41ef9acec4c5e94e1e37d102f5f868ef08f861e`、`judge_l2.py` `ccfb26c878a016a056416db3a5024fc79f857bbe540d042a15426a56e26e131e`（只改 A commit、runner 雜湊、凍結清單與版次字樣）、`artifacts.sha256` `7900e21b6d60381beb03af7c65ced581168e9ed1deee834150472382be7969fc`、`prep-files.sha256` `1a913829…` |
| 4 | B 的 rapid 開發跑次（修正後的 runner）6 次：全部 class P、每次的 Match 日誌都以 `ipc closed reason=eof` 結束 session |
| 5 | 乾跑：84 列（新增兩列：第 1 次 session 停下的形狀（沒有 session 結束行）→ 停；修正後的 `reason=stopped` 是 D110 的形狀 → 不停）、驅動檢查（含 preflight）、5 個模擬 session 全部符合預期；B 語料 66 跑次 0 誤停 |
