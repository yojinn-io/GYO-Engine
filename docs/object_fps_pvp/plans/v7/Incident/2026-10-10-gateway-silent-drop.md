# 2026-10-10 事故：Gateway 悄悄丟掉已解析的命令，Match 的遲到量測被截斷

## 1. 摘要

Gateway 用權威狀態的延遲副本 `lastResolved` 判斷命令是否過時，悄悄丟掉序號不大於它的命令（`apps/object_fps_pvp/gateway/server.go:421-423`）。這段過濾是 v3 加的（`391ca00`，2026-09-26），當時成立；`a4ccaa5`（2026-10-02）讓 Match 開始量「被替代之後才到的命令晚了多少」，需要的正是被丟掉的那些命令，過濾卻沒有重新檢視。Gateway 的副本只比 Match 落後約 0.5 ms，所以從那時起 Match 幾乎量不到任何遲到；late 修正的規則、backlog 餘裕、FireGate guard 的推導都在截斷的資料上校準，2026-10-10 一整天的 D48 調查也在這個盲區上展開。D48 第①步開始到問題被重新定義（D51）是 8 小時 39 分。v5 與 v6 的穩定基線（`f97beb5`、`c7d6dd3`／`object_fps_pvp-v1.1.0`）也帶著這個缺陷；權威狀態與裁決沒有錯，損失的是資訊。目前已重新定義問題、盤點受影響的工作，並在分批規劃讓 Match 知道 Gateway 丟了哪些命令、為什麼丟的機制（只規劃，未實作）。

## 2. 狀態與嚴重度

- **狀態：處置中。** 根因已確認並記入 D51（含 ⑨⑩）；丟棄回報機制的規劃第 1 批（事實核對）已完成，第 2 批等使用者決定；沒有實作；受影響的量測還沒重做。
- **嚴重度：高（量測基準的完整性）。** 理由：
  - 被截斷的是權威（Match）做決定的輸入，不是某一次量測的雜訊。建在它上面的頻率類結論全部要在新基準上重做（見第 3 節）。
  - 缺陷從 `a4ccaa5`（2026-10-02）存在到 2026-10-10。v5 在 10-03 升為穩定基準，10-04 起是 pvp-v6，最早的 L2 記錄是 10-05；所以 v5 收尾、v6 全部與 v7 的驗收都在截斷的資訊流上做（推論）。
  - `a4ccaa5` 的相位追蹤常數（負樣本連續 2 次、修正夾在 ±2 Tick、240 個樣本的 P90）從 v5 起就在 master 上，也是在截斷的樣本上決定的。M0、FireGate 常數凍結與 09a 的產品實作還沒發生（D51 ⑤ 把它們排在修資訊流之後）。
  - v5 與 v6 的穩定基線也受影響（見 3.5）。
- **嚴重度的界線（它不是什麼）：**
  - **權威狀態沒有錯。** 被過濾的都是 Match 不會執行的命令：Gateway 的 `lastResolved` 只等於或落後 Match 的游標，序號 ≤ 它的命令在 Match 端本來也會被略過（`PvpMatch.cpp:67`、`MatchRuntimeHost.cpp:88`）（推論，依程式的順序：`lastResolved` 只在接受 snapshot 時更新，`server.go:520-534`）。
  - **沒有錯誤的裁決，權威 digest 不受影響。** digest 的情境直接驅動 `PvpMatch`／`MatchRuntimeHost`，不經過 Gateway，而且明確排除 `movementSlack*`（`tests/object_fps_pvp/AuthorityDigest.cpp:46`）。
  - **損失的是資訊。** 控制器（late 規則）的校準和我們的分析都建立在截斷的資料上。真實網路變差時，系統走 Held、starvation 或重設，而不是平順的相位修正（推論，U1，待重新量測確認）。

## 3. 影響

### 3.1 被截斷的是什麼

- 截斷只發生在一種情況：**命令被 Match 替代之後才到**。Match 的負 slack 樣本只來自這類命令（`apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:104-112` 的 `substituted`）。
- Gateway 的 `lastResolved` 只在 snapshot 經過時更新（`server.go:520-534`，賦值在 `:528`），落後 Match 典型 <0.5 ms、最壞約 2 ms（推論：上界取自 `pvp-v7-infoflow-plan-20261010/facts-gateway/edge_lag.tsv` 的「snapshot 產生 → Client 收到」，clean 與 x1 跑次 p50 0.33～0.55 ms、p99 0.67～1.06 ms、最大 0.9～2.3 ms；Gateway 的更新落在這段之內）。所以命令只要晚到超過這個量，就在 `server.go:421-423` 被吞掉：late 規則對 Client 端的網路遲到幾乎完全看不見。
  - 例：D48 的原型 x1（純延遲 50 ms、視窗 100 ms）兩個跑次各有 8 個被替代的命令，第一份副本都在 Gateway 知道已解析之後才到，全部被略過；兩個跑次中最晚的分別晚 14.5、12.7 ms（`facts-gateway/arrival_vs_edge.tsv`）。
- late 規則只看得到「Gateway 的過濾看不到的延遲」，有兩種情況：
  1. **Gateway 的水位線本身被拖慢**：host-ipc-250ms 的「snapshot 產生 → Client 收到」p99 89.8 ms、最大 254.8 ms（`edge_lag.tsv`），Gateway 的 `lastResolved` 跟著落後，晚到的命令才過得了過濾。推論：這解釋了 D44 為什麼出現在 host-ipc 的跑次。
  2. **延遲發生在過濾之後**：D48 第②③步的 Gateway→Match IPC 延遲（D50）讓命令準時通過 Gateway、再晚到 Match。這是 D48 只有這種注入能產生 late 修正的原因。
- 同一條路徑上還有其他沒有計數的丟棄（是否發生過大多判斷不了，見 U3）：
  - link 寫出前再剪一次（`apps/object_fps_pvp/gateway/runtime_link.go:192-197`）。
  - epoch／life 前進時整個刪掉待送視窗（`runtime_link.go:189-191`）。
  - link 合併上限（`runtime_link.go:146-148`）超過時回傳 `ErrInput`，整批在 `server.go:439-441` 被悄悄丟掉；而 `:432` 的 `CommitSequence` 在那之前就已提交。
  - epoch／life 不符（`server.go:413-415`）、超過未來上限（`:424-426`）、內容衝突（`:427-429`）。
  - 第 1 批的普查（20:22）：Gateway 有 30 多處「收到卻不轉送」，只有速率限制有計數，而且是全 Gateway 的總數（`action_delivery.go:394-396`）。完整清單見 `facts-gateway/report-from-workflow.md`、`facts-contract/report-from-workflow.md`。
  - 不在 D49 清單上的幾處：link 對舊 epoch／life 的輸入直接回 nil（`runtime_link.go:117-119`）；UDP 舊序號回 `ErrStale`（`services/gyo_gateway/session/session.go:89-91`），還在 `action_delivery.go:398-403` 被算成「接受」；解碼失敗（`server.go:407-410`）；未授權（`action_delivery.go:391-393`，不計數）。
  - Match 自己的拒絕也是無聲的：`IpcHost.cpp:354`（命令數 0 或超過 12 時 `continue`）、`:358`（丟掉 `SubmitInput` 的回傳值）；`MatchRuntimeHost.cpp:72`（重設中或 `CanSubmitInput` 不成立）、`:90`（內容衝突）、`:93`（超過 32 個）都回 false，沒有紀錄。
- 沒有替代的跑次不受影響：Gateway 在那裡丟的只是 Match 早就收過的重送。佐證是 `facts-gateway/relay_input_fate.tsv`：clean／burst／loss 類的替代都是 0，故障案例是 2～88。這份表只涵蓋 08b 的 dev-matrix 與 D48 第②步的開發跑次，不含 L2。
- 修好之後仍有的限制：Match 的 `substituted` 只保留 64 個序號（約 1.07 秒），epoch 一換就清空（`MatchRuntimeHost.cpp:15`、`:396-397`、`:409`），所以晚超過約 1 秒的遲到仍量不到。

### 3.2 受影響的決定與分析

- **Match 的決定**：Client 的相位追蹤（late 修正）在偏差的樣本上做控制；Client 端網路造成的遲到，Match 幾乎看不見。截斷主要影響的是 late 修正觸發的**次數**；修正幅度由 ±2 Tick 的夾限決定（`facts-match/report-from-workflow.md` 第 3.3 節）。
- **我們的分析**：late 修正的規則、backlog 餘裕、FireGate guard 的推導，都在「Client 端網路遲到看不見」的世界裡校準（D51 ②）。`09b-phase-tracking.md` 第②步一節的「意涵」寫的「真正的觸發來源是伺服器端的輸入延遲」，是發現過濾之後才寫的，把過濾的效果當成了系統性質（見 P2）。
- **調查的方式**：D48 第②步的注入改到 Gateway→Match（D50），第③步的範圍被限縮，結論一再標成推論（D51 ③）。

### 3.3 花掉的時間

| 區間 | 長度 | 證據 |
|---|---|---|
| D48 第①步開始（10:56:42）→ 使用者重新定義問題（19:35:52） | 8 小時 39 分 10 秒 | 對話紀錄；以 commit 計 `fcda6e0`→`d0192db` 為 8 小時 41 分 36 秒 |
| D48 第①步 | 42 分 45 秒（含為 S1 暫停的 7 分 49 秒，10:58:27～11:06:16） | `ae5678d` |
| D48 第②步 | 2 小時 43 分 11 秒 | `fe25ba6`→`a5f3ce4` |
| D48 第③步 | 準備＋session 實際 4 小時 25 分 55 秒；牆鐘 4 小時 50 分 51 秒（14:29:21→19:20:12，含等待核准的 17 分 18 秒與判定） | 準備 workflow `w1ipveftf` 3 小時 44 分 40 秒；session 41 分 15 秒（`session-wallclock.txt`） |
| D48 的 agent 與 session 時間（不含等待使用者） | 約 405 分（含上面的 S1 暫停） | 盤點第 6 節 |
| 本 session 內第一次有 agent 讀到過濾 → D51 | 23 小時 42 分 12 秒 | 10-09 19:53:40，08b 規劃 workflow `w090qg0x1`（當時過濾在 `server.go:419`） |
| 主對話第一次讀到過濾 → D51 | 7 小時 24 分 35 秒 | 12:11:17 |

D51 的貼文寫 D48「跑了約 4 小時」，和上面的牆鐘時間對不上。Claude 在 19:43:04 的回覆與 memory 的時間上限規則也照用了「約 4 小時」，沒有核對。

### 3.4 仍然有效與要重做的工作

判準：結論是否依賴 Match 看得見的「替代之後的到達」（負 slack 樣本、late 修正的次數、late 修正造成的領先跳升與 backlog 重設的次數）。單次事件的力學若是用 Gateway→Match 注入量的，被延遲的命令已經過了 Gateway 的過濾，Match 端看得到完整的到達，所以保留。

| 分類 | 項目 |
|---|---|
| 保留 | K1、K2：09a 的機制、設計與原型實測（全在 Client 端）<br>K3：第①、②步的模型「領先 ≈ 修正前＋33.3−W_post」（V3 最大差 0.645 ms）<br>K4：第③步 J1（2／10 對 10／10）與 J2（都是「給定一次 late 修正」之後的條件力學）<br>K5、K6：W_post 的分布；缺陷 B 的串接重設途徑<br>K7：FireGate 的推導公式，以及 guard 的實測值（只取 08b L2 **after** 的 clean 中被 actual 解析的命令，那些跑次替代為 0；`clean-30-r1-before` 有 4 次替代）<br>K8、K9：08b 的結果路徑；08b「不改輸入路徑」的程式分析<br>K10：S1 的結果（閒置主機，替代 0）<br>K11：clean 下的延遲位移（約 38 ms、多約 1 幀）<br>K12：Cooldown 拒絕「存在」的單次證明<br>K13：backlog 餘裕 45 的算術 |
| 重做 | R1：第①步普查的次數<br>R2：D44 的頻率與歸因<br>R3：缺陷 A、B 的影響大小與 A1<br>R4：B2 的設計<br>R5：backlog 餘裕需要多少<br>R6：第 09 批「D44 不需要提前」的理由<br>R7：「領先＝2」這個前提多常被打破<br>R8：Cooldown 拒絕的發生率<br>R9：08b L2 與草案中的次數（「遲到修正 0 次」「180 次：0 次」）<br>R10：有損傷網路下的延遲位移<br>R11：注入的位置與方式<br>R12：09a「修好之後 D44 變多」的優先度判斷 |
| 判斷不了 | U1：D51 ⑨②「脆弱度以 Held／starvation 的形式看不見」（19:47 的對話推出來的，沒有量測，D51 沒標推論）<br>U2：「host-ipc 的 relay 拉長了窗口」<br>U3：Gateway 其他丟棄有沒有發生過（大多沒有計數）<br>U4：閒置主機的「0 筆」能不能保留（`match-commands.jsonl` 有替代資料，用既有資料就能分，還沒做）<br>U5：「FireGate 的 R 預測偏了」（trace 沒有記錄 R 預測）<br>U6：09a 和 B2 的實作順序<br>U7：network20／40 的故障解除事件 |

盤點和 D51 ⑨③ 的分類有幾處不一致，以盤點為準，之後要回寫 D51 或加註：
- 延遲位移：D51 整個列為重做；clean 下應保留（K11），只有損傷網路下的要重做（R10）。
- 第 08b 批：D51 整批保留；結果路徑保留（K8），但 L2 與草案裡的次數是頻率類，要重做（R9）。
- Cooldown：存在與力學保留（K12），發生率重做（R8）。
- FireGate：D51 只說公式保留；guard 的實測值也可保留（K7），但「領先＝2」的前提多常被打破要重做（R7）。
- 注入的位置：「被迫改到 Gateway→Match」只說對一半。第②步試跑的 0／5 有三層原因：
  1. 我們的 relay「擋住再放出」時，每個 session 只保留最新一包，其餘丟棄（`build/acceptance/object_fps_pvp/action_probe.py:254`；例：upstream-250ms 收到 1922 包、沒送達 28 包；`facts-gateway/relay_input_fate.tsv`）；
  2. 放出的那一包只落在一次發布裡（`pilot_scan.txt` 中 p1～p3 都是 `longest consecutive 1`），而 late 修正需要連續 2 次發布都帶負樣本；
  3. 送得到的少數晚到命令再被 Gateway 過濾。

  也就是說，注入工具本身也在丟資料。只有原型 x1（純延遲）是單純被過濾擋下的。

### 3.5 穩定基線

**結論：v5 與 v6 的穩定基線都帶著這個缺陷；v4 不受影響（推論）。**

| 基線 | 識別 | 含 `391ca00`（過濾） | 含 `a4ccaa5`（遲到量測） | 判斷 |
|---|---|---|---|---|
| v4（2026-09-28 升格） | 起始 HEAD `bfeb047`（`plans/v4/STABLE_BASELINE.md:17`；工作樹另有未提交的內容，見同檔） | 是 | 否 | 有過濾、沒有需要已解析命令的消費者，前提仍成立。推論：不受影響（依 blame，`MatchRuntimeHost.cpp:104-112` 由 `a4ccaa5` 加入） |
| v5（2026-10-03 升格，`plans/v4/HANDOFF.md:5`） | master 合併 `f97beb5`（`plans/v5/STABLE_BASELINE.md:22`） | 是 | 是 | 受影響 |
| v6（2026-10-08 升格，`protocol-v6.zh-Hant.md:3`） | master `c7d6dd3`、量測來源 `fee92ff`（`plans/v6/STABLE_BASELINE.md:23`）；tag `object_fps_pvp-v1.1.0`＝`9a6fa8e` | 是 | 是 | 受影響 |

- 核對命令：`git merge-base --is-ancestor <commit> <基線>`，對 `bfeb047`、`f97beb5`、`c7d6dd3`、`fee92ff`、`object_fps_pvp-v1.1.0` 逐一執行（2026-10-10 20:32）。另外，2026-10-02 的 snapshot tag 中，`0bd5363`、`1f96b50` 只含 `391ca00`，`bec86b7`（PR #11，v5 相位追蹤）起兩者都含。
- **對 C1／C2 的意義：** C1／C2 比較的「v6final 對 v7」兩邊都在同一個盲區裡。比較本身仍然成立（同一個盲區下的新舊對比），但兩邊都不代表修正後的世界。
- 穩定基線在修正之後要怎麼處理（重新發佈，或在各 `STABLE_BASELINE.md` 加註），由使用者決定（第 8 節）。

## 4. 時間線（JST）

| 時刻 | 事件 | 證據 |
|---|---|---|
| 09-26 17:28:41 | v3 加入 Gateway 的過濾與註解「Resolved steps are obsolete」 | `391ca00`；blame `server.go:416-417`、`:420-423` |
| 10-02 19:02:39 | 以持續的相位追蹤取代 A1，Match 開始量 `substituted` 的遲到；同一個 commit 在註解正下方改了 `server.go:418-419`，`runtime_link.go` 有 13 行變更（`--stat`），過濾沒動 | `a4ccaa5`（Co-Authored-By Claude）；blame `MatchRuntimeHost.cpp:104-112` |
| 10-09 17:55:21 / 18:00:11 | D44（偶發 epoch 重設，提高嚴重度，第 16 批回歸）決定 / 記入 | 使用者訊息；`491fcb6` |
| 10-09 18:12:30 | D45（L2 宣告前做對抗式檢查等換檔規則） | `7054128` |
| 10-09 19:53:28～20:45:59 | 08b 規劃 workflow。19:53:40 與 20:24:25 有 agent 讀到過濾（當時在 `:419`），沒有標出 | `w090qg0x1` |
| 10-09 20:47:07～21:29:56 | 08b D44 一節的 workflow。20:49:07 分析 agent 讀 `sed -n 340,450p`、`471,650p`；21:08:44 對抗式檢查讀 `sed -n 330,450p`；都沒有標出過濾 | `ws8kdjqhu` |
| 10-10 00:11:30 / 00:39:27 | 第 08b 批 L2 開始 / 通過（72 次） | `190a827` |
| 00:48:22～01:50:46 | 第 09 批規劃（FireGate 常數）。01:01:29 agent 讀到過濾，沒有標出 | `w8vlpxiqb`；`9fedd79`（01:53:39） |
| 02:03:59～03:11:53 | 09a 設計（token bucket；和 S1 準備同一個 workflow） | `ww3re9r5i` |
| 02:07:32～03:04:59 | 09b 規劃（相位追蹤缺陷） | `w8jnhdu4p`；`50ea0fc`（03:29:29） |
| 10:55:51 / 10:56:20 | D48（先用測試確認 D44 的提前條件，分三步，第②③步要事前宣告）提出 / 記入 | 使用者訊息；`fcda6e0` |
| 10:56:42 → 11:39:27 | 第①步：既有證據普查（5 筆／4 事件） | `ae5678d` |
| 11:42:31 | 第②步開始 | `fe25ba6` |
| 11:52:15～11:53:55 | 試跑 5 次，late 修正 0 次：relay 擋住 client→Gateway 再放出 4 次（p1 的 r1／r2、p2、p3），`gateway` 模式 1 次（p4，relay 沒有擋住輸入）。HANDOFF 與 `7d6dce5` 寫成「client→Gateway 0／5」 | `pvp-v7-d48-step2-dev-20261010/p1`～`p4`（`p4-gateway-100-at2.0/plan.json` 的 `"mode": "gateway"`）；`pilot_scan.txt` |
| 11:57:58 | 第②步 agent 讀到 `server.go:421` | 對話紀錄（subagent） |
| 12:11:17 | 主對話第一次自己讀到過濾那一行 | tool 結果 `sed -n 412,426p` |
| 12:11:43 | 第②步碰到停止條件，記入；「意涵」寫「client 端的網路遲到幾乎不會觸發 late 修正；真正的觸發來源是伺服器端的輸入延遲」 | `7d6dce5`；`09b-phase-tracking.md:44` |
| 12:23:17 | 使用者第一次提到 Gateway 的過濾（觀測缺口清單 → 第 10 批的輸入，「只記錄，不實作」）；記錄寫成「這是協議契約定下的…平常無害」 | 使用者訊息；`e1fa218`（`HANDOFF.md:258`） |
| 13:06:38 / 13:07:33 | D49（Gateway 可執行丟棄，決定權與知情權在 Match）、D50（注入改到 Gateway→Match，使用者依 Claude 的建議決定）提出 / 記入。主對話的核對寫「執行結果與權威 digest 不受影響，失去的只有時間訊號」 | `4c227f0` |
| 13:09:52 | 使用者提出 PEP／PDP 的名詞 | `856569d` |
| 14:19:29 → 14:26:12 | 第②步核准 → `step2`＝passed | `a5f3ce4` |
| 14:28:09 | 第③步開始 | 使用者訊息 |
| 14:29:21～18:14:02 | 第③步準備 workflow（3 小時 44 分 40 秒） | `w1ipveftf` |
| 18:31:20 | 使用者要求先補三處文件（重推條件 11、第③步宣告的範圍限制、D44 未結事項），並核准第③步；已要求寫明「結論只在現在的 Gateway 過濾下成立」 | 使用者訊息；`eac65bb` |
| 18:33:39～19:14:54 | 第③步 session | `session-wallclock.txt` |
| 19:20:12 | 判定 `step3`＝stopped（權威 Cooldown 拒絕 2 次） | `b345bbd` |
| 19:35:52 / 19:37:56 | 使用者重新定義問題：先修 PEP／PDP 的資訊流，再談 late 修正 / D51 記入（D49 的加註也在這個 commit） | `d0192db` |
| 19:37:46 | 時間上限與決策關卡的規則記入 memory（檔案最後修改時刻） | `time-box-and-decision-gate.md` 的 modified |
| 19:43:04 | Claude 把盲區稱為「意外的保護」「意外的低通濾波」，提議先只記錄再轉送 | 主對話 |
| 19:46:09 | 使用者糾正：「我們已經在一個錯誤的基準上做了大量的工作」 | 主對話 |
| 19:47:23 | Claude 只收回一部分，仍說「可以接受的中間狀態」「P2 的合併條件要寫明」 | 主對話 |
| 19:47:27 | 使用者：「嚴格來說目前的資訊流就是不正常的」 | 主對話 |
| 19:48:24 | Claude 全部撤回，查出 `391ca00`、`a4ccaa5`，定性為整合缺陷 | 主對話 |
| 19:49:52 | 使用者：「同意，補上 D51 之後開始規劃，檔位使用ultracode」 | 主對話 |
| 19:50:40 | D51 ⑨（定性：整合缺陷，不是取捨）記入 | `7666734` |
| 19:50:59、19:53:39 | 使用者分兩則送出 ⑩ 的原話（第一則在中途斷掉） | 主對話 |
| 19:51:16 | Claude 發現訊息斷掉而停下；回覆中寫 ultracode 估 2～2.5 小時 | 主對話 |
| 19:55:06 | D51 ⑩（Gateway 的每一種丟棄都要讓 Match 知道）記入 | `6ee80c7` |
| 19:56:30 | 丟棄回報機制的規劃 workflow 啟動（一次跑完版） | `wsu7h2j4s` |
| 20:00:29 | 使用者要求寫這份事故紀錄 | 使用者訊息 |
| 20:02:19 | 使用者指出估計超過單一問題 2 小時的上限，要求分批 | 主對話 |
| 20:02:47 / 20:03:02 | 一次跑完版停掉（6 分 16 秒）/ 第 1 批（事實核對）啟動 | `wsu7h2j4s`；`wuq2e3wc2`；`505627d`（20:03:14） |
| 20:03～20:22 | 第 1 批完成：30 多處無聲丟棄、Gateway 副本落後 <0.5 ms、注入工具也在丟資料、只刪過濾會讓 `:424` 下溢、新增回報欄位依 `protocol-v6.zh-Hant.md:50` 算改 wire | `26805e5`；`pvp-v7-infoflow-plan-20261010/facts-*/` |
| 20:00～20:26 | 事故紀錄的 workflow（時間線與盤點 2 位、撰寫 1 位、對抗式核對 1 位）完成；子 agent 寫報告檔被 harness 拒絕，結果由主對話存成各子目錄的 `from-workflow.md` | `wybzpf4fx`；`pvp-v7-incident-20261010/` |
| 20:31:33 | 套用核對的 18 項修正，初版放進 `docs/`（本資料夾） | `08a02c7` |
| 20:32 | 使用者要求補完：第 1 批的五點與穩定基線的確認；穩定基線的 ancestry 核對 | 使用者訊息；3.5 |

## 5. 根因

### 5.1 技術上的根因：前提失效

- `391ca00`（2026-09-26）在 Gateway 加入過濾：序號 ≤ `lastResolved` 的命令略過（`server.go:420-423`），註解寫「Resolved steps are obsolete」（`:416-417`）。當時 Match 還沒有 `:104-112` 的遲到量測，不使用已解析命令的到達時刻，所以這個前提成立（推論：依 blame，這段量測由 `a4ccaa5` 加入）；Gateway 只是省掉沒用的轉送。link 端的剪除（`runtime_link.go:192-197`）也來自同一個 commit（`:189` 來自 `de87bb9`，2026-09-30）。
- `a4ccaa5`（2026-10-02）讓 Match 對「已被替代、之後才到」的命令記錄負 slack（`MatchRuntimeHost.cpp:104-112`）。這個新消費者要的正是 Gateway 認為「obsolete」的命令。同一個 commit 在註解正下方改了 Gateway 的同一個函式（`server.go:418-419`），diff 的上下文就顯示這行註解，過濾卻沒有重新檢視。這個 commit 是使用者與 Claude 共同完成的（Co-Authored-By Claude）。
- 從此兩個元件的假設互相矛盾：Gateway 認為已解析的命令沒有用，Match 需要它們來量遲到。Gateway 的 `lastResolved` 又是延遲副本，所以它丟掉的範圍取決於 snapshot 經過的時機，而不是 Match 的實際狀態。
- Match 自己也過濾已解析的命令（`PvpMatch.cpp:67`、`:78-81`、`MatchRuntimeHost.cpp:88`），但 Match 端的過濾只決定命令是否排進佇列；量測的迴圈（`MatchRuntimeHost.cpp:104-112`）另外檢查這些 ≤cursor 的命令是否在 `substituted` 裡，所以不影響量測。Gateway 的過濾是重複的，而且發生在命令到達 Match 之前，量測因此看不到。

這是整合缺陷，不是取捨（D51 ⑨）：沒有任何決定是「為了某個好處接受 Match 看不到遲到」。

### 5.2 為什麼一直沒被發現

1. **前提沒有寫成契約或測試。** 協議條文 `network-architecture.zh-Hant.md:230-233` 和過濾一樣來自 `391ca00`，只寫「已完成／舊 epoch 不重執行」，沒有規定 Gateway 不轉送，也沒有規定遲到要被量；這段條文有歧義，而且在 12:23 被讀成「這是協議契約定下的…平常無害」（`HANDOFF.md:258`）。`a4ccaa5` 新增的 Gateway 測試是 eviction 與 observed tick 的，沒有測試斷言「晚到的命令會送到 Match」；第 1 批的突變也證實，只刪掉過濾的突變跑完所有 Go 測試都存活。所以加入下游消費者時，沒有任何東西失敗或提醒。
2. **沒有丟棄計數。** 輸入路徑（`server.go:400-441`）的丟棄沒有計數，也沒有 log（`:421` 是 `continue`，其餘是 `return nil`）。例外只有速率限制（`rateRejectedPackets`，`server.go:89`；log 在 `:163`、`:748`），而且是全 Gateway 的總數。截斷在證據裡沒有痕跡。
3. **Match 端也過濾，所以看起來無害。** 讀到 Gateway 過濾的人很容易把它當成 Match 那道檢查的重複、無害的提前執行。本 session 內，10-09 19:53 到 10-10 01:01，三個 workflow 的四個 agent 讀到這一行，都沒有標出；08b 草案把「已解析過的序號會被丟掉」只歸給 Match（`08b-event-driven-results.md:132`）。
4. **症狀不像故障。** 截斷的後果是「沒有 late 修正」，或只以 Held／starvation 重設的形式出現（推論，U1），這兩者都可以被解讀成系統健康或別的原因。
5. **注入工具本身也在丟資料。** 我們的 relay「擋住再放出」時，每個 session 只保留最新一包，其餘丟棄（`build/acceptance/object_fps_pvp/action_probe.py:254`；例：upstream-250ms 收到 1922 包、轉送 1892、放出 2、沒送達 28；D48 第②步 p1～p3 各放出 2 包、沒送達 4～16 包；`facts-gateway/relay_input_fate.tsv`）。Gateway 再把少數送得到的晚到命令過濾掉。所以 Client→Gateway 型的注入「量不到 late 修正」，有一部分是工具造成的，看起來就更像系統性質。
6. **沒有任何測試釘住過濾，而且「只拔一行」會更糟。** 第 1 批在副本上逐一刪除各個丟棄（`facts-gateway/probe_mutants.tsv`）：只刪過濾的突變 M1 跑完所有 Go 測試都存活。刪掉過濾之後，`server.go:424` 的 `command.Sequence-p.lastResolved` 會無號數下溢（`lastResolved` 是 `uint64`，`server.go:50`），整包被丟、連包內的新命令一起；要同時把 `:424` 改成只檢查 `Sequence > lastResolved` 的命令（M2），`server_test.go:690` 才會失敗。

## 6. 流程上的因素

以下依時間順序寫出當時的決定、當時的理由、事後看哪裡錯了。目的是找出流程上可以改的地方，不是歸咎個人。

| # | 決定 | 當時的理由 | 事後看 |
|---|---|---|---|
| P1 | 10-09 19:53 到 10-10 01:01，三個 workflow 的四個 agent 讀到過濾，產出裡都沒有標出 | 推論：分析的焦點是排隊命令的上限，過濾看起來和 Match 的那道檢查等價（`ws8kdjqhu` 在 21:07:36 的輸出把丟棄只歸給 Match） | 沒有人問「這個過濾的前提，在 `a4ccaa5` 之後還成立嗎」。對抗式檢查核對的是草案的主張（例如「08b 不在輸入路徑上」判 partly），沒有檢查元件之間的假設 |
| P2 | 12:11 第②步試跑 0／5 碰到停止條件後，把過濾的效果寫成「真正的觸發來源是伺服器端的輸入延遲」；13:07 記 D50，把注入改到 Gateway→Match；盲區記成未結事項與第 10 批的輸入（D49 分兩階段） | 讓 D48 繼續前進；使用者 12:23:17 與 13:06:38 的貼文寫「只記錄，不實作」「要不要改協議另立」；D50 是使用者依 Claude 的建議決定的 | 「繞過」是雙方共同接受的。發現盲區時沒有停下來問「基準本身是否成立」，還把過濾的效果寫成系統性質。從主對話讀到過濾到 D49 記入只花 56 分鐘，但從 D49 記入到重新定義問題又花了 6 小時 28 分 |
| P3 | 18:31 第③步在補上範圍限制後核准並執行 | 使用者已要求寫明「結論只在現在的 Gateway 過濾下成立」；準備成果已經完成 | 第③步不是在不知情下跑的，但範圍限制讓它的結果只能回答條件力學（K4）。Claude 在 19:43:04 事後評估：J1 幾乎可由第①、②步的模型推出 |
| P4 | 一個問題沒有時間上限 | D48 一開始拆成三步、每步回報，看起來已經有控制 | 每步有回報，但沒有累計時間的上限。D48 牆鐘 8 小時 39 分，第③步的準備 workflow 單獨 3 小時 45 分 |
| P5 | 探索性的問題套了多層事前宣告與對抗式檢查 | 第②③步的事前宣告是 D48（使用者 10:55:51 的貼文）要求的，D45 是使用者的決定；Claude 在 10:56:52 把第①步定位為「只是讀證據，不是事前宣告」；沒有人主張降低第②③步的嚴格度 | 對驗收（L2）合適的嚴格度，套到探索性的問題上，讓每一步都很貴，卻沒有提高「問對問題」的機率 |
| P6 | 沒有「這一步的結果會改變哪個決定？」的關卡 | 每一步都有明確的判定條件 | 推論：第③步的 J1 不會改變 09a 要不要做的決定 |
| P7 | Claude 的錯誤框架：12:11:43 寫「client 端的網路遲到幾乎不會觸發 late 修正」；13:07:33 核對 D49 時寫「執行結果與權威 digest 不受影響，失去的只有時間訊號」；19:43 把盲區稱為「意外的保護」「意外的低通濾波」，提議先只記錄再轉送；19:47:23 仍稱「可以接受的中間狀態」 | 想讓修正的範圍小、讓 P2 照原計畫走 | 把缺陷描述成無害或好處，模糊了「這是缺陷」的判斷；13:07 說「失去的只有時間訊號」，而被丟掉的正是量測需要的時間訊號。使用者在 19:46:09 與 19:47:27 兩次糾正，19:48:24 才全部撤回 |
| P8 | 跨元件的前提沒有可被測試失敗的形式 | v3 時只有一個消費者 | 見 5.2 第 1 點。這是程式層面的流程因素 |
| P9 | 時間上限規則在 19:37 記進 memory；19:51 的 ultracode 規劃估 2～2.5 小時，一開始就超過上限，Claude 沒有在開始前指出 | 使用者選 ultracode 時已知道估計 | 規則記下約 14 分鐘就被打破，是使用者在 20:02:19 指出的；之後才改成分三批、每批回報 |

## 7. 偵測

1. **10-09 19:53～10-10 01:01，讀到但沒認出。** 四個 agent 在分析中讀到 `server.go` 的過濾，沒有標出（P1）。
2. **10-10 11:52～11:54，第②步試跑。** 5 次試跑 late 修正 0 次，沒能重現 D48 第②步要求的「成對遲到 → late 修正」，碰到停止條件。
3. **11:57～12:11，讀程式。** 第②步 agent 在 11:57:58 讀到 `server.go:421`；主對話在 12:11:17 自己讀到，12:11:43 記入停止。
4. **12:23～13:09，D49。** 使用者在 12:23 把 Gateway 的過濾列入觀測缺口，13:07 記入 D49（Gateway＝PEP、Match＝PDP），13:09 提出名詞。此時仍把修正排在第 10、11 批。
5. **19:35～19:55，D51。** 第③步 stopped 之後，使用者重新定義問題（19:35:52），定性為整合缺陷（⑨，19:50:40），並把目標擴大到 Gateway 的每一種丟棄（⑩，19:55:06）。
6. **20:03～20:22，第 1 批事實核對。** 確認截斷的程度（Gateway 副本落後 <0.5 ms）、無聲丟棄的全貌（30 多處，Match 端也有），以及注入工具本身也在丟資料。

偵測最後是靠使用者把問題重新框定，不是靠任何自動化的訊號。

## 8. 處置與行動項目

| 項目 | 負責 | 狀態 | 位置 |
|---|---|---|---|
| D51 重新定義問題（含 ⑨ 定性、⑩ 目標擴大） | 使用者決定；Claude 記錄 | 完成 | HANDOFF D51（`d0192db`、`7666734`、`6ee80c7`） |
| D49 加註 | 使用者決定；Claude 記錄 | 完成（內容已過時，見下一列） | `d0192db` |
| D44 未結事項加註、09 的重推條件 11、第③步宣告的範圍限制 | 使用者要求；Claude 記錄 | 完成 | `eac65bb`；`09-firegate-c2.md` 重推條件 11 |
| D49 的 D51 加註仍寫「其他丟棄只計數、寫 log，正式回報仍留給第 10、11 批」，和 D51 ⑩ 矛盾 | Claude | 完成（本紀錄的同一個 commit 改寫） | HANDOFF D49 |
| 「重複過濾」記成未結事項 | 使用者要求（12:23:17）；Claude 記錄 | 完成 | `e1fa218`；HANDOFF 未結事項 |
| memory：時間上限（估計的 1.5 倍、單一問題 2 小時）與決策關卡 | Claude | 完成 | memory `time-box-and-decision-gate.md` |
| 時間上限的套用：每次規劃開始前先檢查估計有沒有超過 2 小時，超過就先分批或先問（P9） | Claude | 進行中（丟棄回報的規劃已分三批） | 本紀錄 |
| 受影響工作的盤點 | Claude（workflow） | 完成 | `pvp-v7-incident-20261010/inventory/from-workflow.md` |
| 本事故紀錄 | Claude（workflow）；使用者核對 | 完成初版 | 本檔 |
| 丟棄回報機制的規劃（只規劃）：第 1 批事實核對 → 第 2 批方案與評審（另評估第 6 項的去重）→ 第 3 批對抗式檢查，每批停下回報 | Claude（workflow／agent）；使用者在每批後決定 | 三批完成（D52、D53）；批次文件撰寫中 | HANDOFF D52、D53；`pvp-v7-infoflow-plan-20261010/` |
| 依盤點回寫 D51 ⑨③ 的分類與 U1 的推論標記 | Claude；使用者核准（2026-10-10） | 完成 | HANDOFF D51 ⑨ 的更正 |
| 加註建在截斷資訊流上的說法：`HANDOFF.md` 未結事項「重複過濾」的「平常無害」、`09b-phase-tracking.md` 的「意涵」與「意外的緩衝」 | Claude；使用者核准（2026-10-10） | 完成 | HANDOFF 未結事項；`09b-phase-tracking.md` |
| `network-architecture.zh-Hant.md:230-233` 的條文歧義 | 丟棄回報計畫或第 11 批 | 未開始 | 協議文件 |
| U4：用既有的 `match-commands.jsonl` 分出閒置主機的替代 | 使用者決定（2026-10-10）：不單獨做，併入重新量測 | 併入 | 丟棄回報計畫的 L2 |
| 實作丟棄回報（轉送已解析的命令並處理 link 的剪除、合併上限與 `:424` 的下溢；每一種丟棄都讓 Match 知道；Match 記下自己的拒絕），加入「晚到的命令會送到 Match」的測試、丟棄計數與突變 | 使用者核准規劃後 | 未開始 | D51 ⑥⑩；P2 |
| 在完整的資料上重新量測（真實流量：network20／40、upstream 等既有案例），並依重推條件 11 加跑 Client→Gateway 短停頓的注入。注入的方式：使用者在補完指示（20:32）中要求之後的注入用**純延遲型**（例如原型 x1，或 D48 第③步的 IPC 延遲那種不丟包的延遲），不用只保留最新一包的「擋住再放出」 | 實作後 | 未開始 | D51 ⑤；`09-firegate-c2.md` 重推條件 11 |
| 穩定基線（v5 `f97beb5`、v6 `c7d6dd3`／`fee92ff`／`object_fps_pvp-v1.1.0`）：先在 `STABLE_BASELINE.md` 加「已知問題」並連到本紀錄；要不要重新發佈，修好之後再決定 | 使用者決定（2026-10-10） | 「已知問題」完成；重新發佈待修正後決定 | 3.5；`plans/v5/STABLE_BASELINE.md`、`plans/v6/STABLE_BASELINE.md` |
| 注入工具：「擋住再放出」只保留最新一包的行為要寫明，Client→Gateway 的純延遲注入要補 | 使用者決定（2026-10-10）：併入丟棄回報計畫的 L2（斜坡注入） | 併入 | 丟棄回報計畫的 L2；`build/acceptance/object_fps_pvp/action_probe.py:254` 的 relay（凍結檔，不改；新注入另寫分支） |
| 09b 重做（R3、R4、R12；09a 與 B2 的順序 U6） | 重新量測後 | 未開始 | `09b-phase-tracking.md` |
| M0 與 FireGate 常數凍結、C2，排在修資訊流與重新量測之後 | 使用者決定 | 已決定（D51 ⑤） | D51 ⑤⑥；`09-firegate-c2.md` |
| D44 的頻率與歸因重做（R2）；第 16 批的整體回歸照 D44 不變，但基準換成修好之後的資訊流 | 重新量測後；第 16 批 | 未開始 | HANDOFF D44、未結事項 |

## 9. 學到的事

1. **註解裡的前提會在新消費者出現時悄悄失效。**
   之後怎麼做：丟棄或過濾資料的地方，把前提寫成測試（例如「晚到的命令會送到 Match」）；新增消費者的 commit，要列出它依賴的上游資料，並檢查上游有沒有丟掉它。
2. **看不見的丟棄等於沒有證據。**
   之後怎麼做：每一種丟棄都要有計數與原因（D49、D51 ⑩）；量測報告裡「0 次」要附上「這個 0 有沒有可能是被上游丟掉」的核對，包括我們自己的注入工具。
3. **注入工具要先確認自己不丟資料。**
   之後怎麼做：之後的注入用純延遲型（不丟包、不只保留最新一包）；注入前先核對工具的轉送紀錄（收到、轉送、放出、沒送達），「擋住再放出」只在要測的就是遺失時使用，並寫明。
4. **「只拔一行」之前，先看那個值還被誰用。**
   之後怎麼做：移除過濾或檢查時，先列出同一個變數的所有用途（例如 `lastResolved` 還用在 `:424` 的上限），並為移除的那一行補上能單獨失敗的測試與突變。
5. **發現盲區時，先問基準是否成立，再決定要不要繞過。**
   之後怎麼做：碰到「量不到預期的現象」的停止條件時，先停下來回報「這是系統性質，還是觀測路徑的缺口」，由使用者決定要不要繼續在現在的基準上做。
6. **探索與驗收要用不同的嚴格度。**
   之後怎麼做：每一步開始前寫出時間估計，超過 1.5 倍或單一問題累計超過 2 小時就停下回報；估計一開始就超過上限時，先分批或先問；每個關卡先問「這一步的結果會改變哪個決定？」，答不出來就不做（memory `time-box-and-decision-gate.md`）。事前宣告與對抗式檢查留給驗收與要凍結的東西。
7. **不要把缺陷描述成無害或好處。**
   之後怎麼做：Claude 碰到「意外的保護／緩衝」「失去的只有某某訊號」這類說法時，先把它當成缺陷描述清楚（誰做了什麼決定、誰不知道、誰需要被丟掉的東西），再討論它有沒有好處；修正範圍的取捨交給使用者決定。
8. **讀程式的 agent 要看元件之間的假設，不只是單一元件的行為。**
   之後怎麼做：跨 Gateway／Match 的分析，要求列出「這個元件丟掉或改寫了什麼、下游誰需要它」；對抗式檢查也要涵蓋元件之間的假設。

## 10. 證據位置

- 程式：`apps/object_fps_pvp/gateway/server.go:413-441`、`:520-534`；`apps/object_fps_pvp/gateway/runtime_link.go:146-148`、`:189-197`；`apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:88`、`:104-112`；`apps/object_fps_pvp/src/Pvp/PvpMatch.cpp:67`、`:78-81`；`apps/object_fps_pvp/src/Pvp/IpcHost.cpp:354`、`:358`。
- commit：`391ca00`、`de87bb9`、`a4ccaa5`；這一天的 `491fcb6`、`7054128`、`190a827`、`9fedd79`、`50ea0fc`、`fcda6e0`、`ae5678d`、`fe25ba6`、`7d6dce5`、`e1fa218`、`4c227f0`、`856569d`、`a5f3ce4`、`eac65bb`、`b345bbd`、`d0192db`、`7666734`、`6ee80c7`、`505627d`、`26805e5`。原型 worktree：`8fabfa6`、`e6885b7`、`c2705be`。
- 穩定基線：`docs/object_fps_pvp/plans/v4/STABLE_BASELINE.md:17`、`plans/v5/STABLE_BASELINE.md:22`、`plans/v6/STABLE_BASELINE.md:23`、`plans/v4/HANDOFF.md:5`、`docs/object_fps_pvp/protocol-v6.zh-Hant.md:3`；ancestry 核對見 3.5。
- 文件：`docs/object_fps_pvp/plans/v7/HANDOFF.md`（D44～D51、進度記錄器 2026-10-10、未結事項）、`09-firegate-c2.md`（重推條件 11）、`09a-input-send-pinning.md`、`09b-phase-tracking.md`（D48 各節）、`08b-event-driven-results.md:132`、`docs/object_fps_pvp/network-architecture.zh-Hant.md:230-233`、`protocol-v6.zh-Hant.md:50`。
- 證據目錄（`build/target/_build/test/logs/`，不在儲存庫內）：`pvp-v7-d48-step1-20261010`、`pvp-v7-d48-step2-dev-20261010`（`pilot_scan.txt`、`p4-gateway-100-at2.0/plan.json`）、`pvp-v7-d48-step2-20261010`、`pvp-v7-d48-step3-prep-20261010`、`pvp-v7-d48-step3-20261010`（`session-wallclock.txt`）、`pvp-v7-infoflow-plan-20261010`（`facts-*/report-from-workflow.md`；`facts-gateway/edge_lag.tsv`、`arrival_vs_edge.tsv`、`relay_input_fate.tsv`、`probe_mutants.tsv` 等）。
- 本事故的工作檔：`pvp-v7-incident-20261010/`（`timeline/`、`inventory/`、`draft/`、`check/` 的 `from-workflow.md`，以及各自的 `commands.txt`）。
- 對話紀錄與 workflow 紀錄：Claude Code 的 session 紀錄（本機，不在儲存庫內），時刻由這些紀錄的 UTC timestamp 換算成 JST。
