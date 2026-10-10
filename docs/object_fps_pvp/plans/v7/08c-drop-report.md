# 第 08c 批：Gateway 丟棄回報（讓 Match 知道丟了什麼、為什麼丟）

狀態：**實作中**（2026-10-10 分批與檔位核准，D54；F0 開始）。PR 線 P2（PR #73）。排在 09a、09b 的重做、M0 與 FireGate 常數凍結、C2 之前（D51 ⑤）。
依據的決定：D49（Gateway＝PEP、Match＝PDP）、D51（問題的重新定義；⑨ 定性為整合缺陷；⑩ 每一種丟棄都要讓 Match 知道）、D52（第 1 批之後的三個決定）、D53（照建議的組合）。
起因：[2026-10-10 事故](Incident/2026-10-10-gateway-silent-drop.md)。
檔位：規劃 ultracode，分三批、每批停下回報（使用者 2026-10-10）：第 1 批事實核對 3 位（high）；第 2 批方案 3 位＋評審 1 位（high），另評估第 6 項的去重 1 位（high）；第 3 批對抗式檢查 1 位（xhigh）。實作 high，局部 xhigh（見「分批」）。

批次名稱 08c 是主對話取的：它改的是 Gateway 與 runtime link（和 08b 同一層），而且必須在第 09 批的 M0 之前完成。

## 先讀這一段

- **問題**：產品層的 Gateway 用權威狀態的延遲副本 `lastResolved` 悄悄丟掉已解析的命令（`gateway/server.go:421-423`），link 寫出前再剪一次（`runtime_link.go:192-197`）。Gateway 的副本只比 Match 落後約 0.5 ms，所以 Match 幾乎量不到 Client 端的網路遲到；late 修正、backlog 餘裕、FireGate 的推導都在截斷的樣本上校準。Gateway 還有 30 多處「收到卻不轉送」，只有速率限制有計數；Match 自己的拒絕也是無聲的。
- **目標**：Gateway 收到、屬於某個 session 的每個資料包，只會有兩種結果之一，沒有第三種（無聲）：
  - **(F) 轉送**：命令送到 Match，由 Match 用自己的游標與時鐘分類並記帳；
  - **(G) Gateway 執行的丟棄**：依（玩家、原因、種類）計數並寫進 log；P2 期間是「人看得到、Match 不知道」的已知缺口，第 11 批 pv7 以累計值回報給 Match。
- **不改 wire**：所有 `.proto` 的欄位不變，版本仍是 6。變的是 Gateway→Match 輸入流的行為（D52 ①、D53 ②），以及 `runtime_v6.proto:31` 的註解（比照 D22／D23，維持 pv6、Gateway 與 Match 同一個 commit 切換）。
- **權威狀態與裁決不受影響**：被過濾的都是 Match 不會執行的命令；權威 digest 不變。會變的是資訊，以及 live 跑次中由資訊驅動的行為（late 修正次數、替代比例）。

## 設計

三個部分互補，少一個都不完整：

| 部分 | 範圍 | 少了它 |
|---|---|---|
| ① 轉送 | Gateway／link（Go） | Match 收不到遲到的命令 |
| ② Match 帳本 | Match 的 host 層（C++） | 轉送過去的命令被 Match 無聲拒絕，資訊沒有增加 |
| ③ Gateway 計數 | 拿不到內容的丟棄（Go） | 這幾類連人都看不到 |

### ① 轉送（D53 第 1、2、6 項）

- **主線**：Gateway 解碼成功的窗口照舊合併，已解析的命令也留在裡面（刪除 `server.go:421-423`、`:435-437` 與 link 的剪除 `runtime_link.go:192-198`）。link 合併上限 `:146-148` 只算序號大於 `l.resolved[id]` 的命令；`server.go:424` 改成 `Sequence > lastResolved && Sequence-lastResolved > 32`，修掉 `uint64` 下溢。
- **拒絕線**（新增）：Match 可能整筆拒絕的窗口不合併，原樣單獨送出：舊／未來 epoch 或 life（`server.go:413-415`；link `:117-122` 合成一條回 `ErrInput`）、內容衝突（`:427-429`）、超過未來 32 個、link 上限退回、換代時待送的舊主視窗（`:189-191` 改成移到拒絕線）。每位玩家最多 4 個窗口；超過就丟最舊的並計數。
- **已寫出去重**（D53 第 6 項，延伸 link 既有的合併去重 `runtime_link.go:134-145`，和 08b 的已送集合 `sent` 同一模式）：
  - 標記點在 `batch()` 取走窗口的那一刻（同一個 `l.mu` 臨界區）。截短與溢出都發生在取走之前，被截掉的命令不會被標記，下一份副本照樣轉送。
  - 只標記主線。拒絕線不標記（Match 可能整則拒絕它）。
  - 鍵是 (player, epoch, life, sequence) 加內容；內容不同的副本走拒絕線並計 `written_conflict`。
  - 保留期 632 個序號（Match 帳本 600＋未來上限 32），以「寫出過的最大序號」量，不讀 `lastResolved`。超過的副本照常轉送，由 Match 分類。
  - 換代（`acknowledge`）與 Leave（`forgetLocked`）時清空。**換代清空是正確性條件**：每個 epoch 的序號從 1 重新開始，沒清空的話新 epoch 的序號 1、2 會被當成副本丟掉，Match 就等不到序號 1。
  - 整包被去重時，若已有待送窗口，就把這個包的 `observed_authority_tick` 併進去（不建立新窗口），和現行行為等價。
  - Match 不改，仍必須把副本當成無操作；去重是最佳化，不是保證。
- **★ 舊序號的 Input**（D53 第 1 項）：`session.go:89-91` 只看標頭就回 `ErrStale`，但 payload 完整（Client 的 Hello、Input、Actions 共用 `++sentSequence`，`ClientConnection.cpp:349-350`）。改成照樣解碼並轉送，但**不呼叫** `CommitSequence`（不刷新 liveness、不影響速率計算）。重播估計 network* 約 4～5% 的 Input 首份副本原本因此被丟（推論，第 2 批方案 ③）。
- **仍由 Gateway 執行、依玩家計數的兩種丟棄**：拒絕線溢出、主線已解析部分超過 32 個時的截短。去重之後這兩個數字等於真正的資訊損失。
- **轉送量**（推論，`facts-gateway/arrival_peak.tsv`、`arrival_vs_edge.tsv`）：約 60 命令／s／玩家（HEAD 照轉未解析命令的重送，約 180；只做主線不去重約 210、峰值 630）；Client 停頓時多出的 IPC 是 0 則。

### ② Match 帳本（D52 ③、D53 第 3、4 項）

- 放在 host 層（header-only 的 `MatchIngressLedger.hpp`），不碰 `PvpMatch` 的狀態、snapshot 欄位或 wire。
- `PvpMatch` 新增 `InputAdmission AdmitInput(...) const`，`CanSubmitInput` 改成 `AdmitInput(...) == Accepted`（判斷只有一份）。`MatchRuntimeHost::SubmitInput` 每個 `return false` 改成依原因計數。原因：`resetting`、`unknown_player`、`malformed`、`epoch_old`／`epoch_future`、`life_old`／`life_future`、`beyond_window`、`conflict_queued`、`conflict_staged`、`staged_over_window`。
  - `staged_over_window`（`MatchRuntimeHost.cpp:93`）與 `handoff_rejected` 依程式推導走不到，標成「應恆為 0」，L2 的 0 不當證據。`resetting` 只能在單元測試中構造。
- `IpcHost.cpp:354` 的重複檢查刪除，由 `ValidInput` 記 `malformed`；`:358` 不再丟掉結果。動作那一側（`:361`、`:367-373`、`:376` 與 `SubmitActionBatch`）一併計數。
- 清單外的 Match 無聲丟棄一併納入：交接被拒（`:243-254`）、換代時丟掉的已排入命令（`:282-284`，回答 D44「每次重設丟了幾個命令」）、Leave 時丟掉的暫存。
- 晚到紀錄：每個被替代的序號一筆（替代時刻、第一份到達、`late_us`、之後的副本數、關閉原因），保留 600 Tick（`ConnectionQualityWindowTicks`），每位玩家最多 1024 筆。原本的 slack 樣本路徑（`substituted` 上限 64）**不放寬**：放寬會改變 Client 的行為，不屬於這一批。
- 收到 input 時，每個命令恰好落在一類：`accepted_new`、`pending_copy`、`late_first`、`late_copy`、`resolved_copy`、`resolved_untracked`，或某個拒絕原因。
- 時鐘不變式：純重送不讀時鐘（`:96-97`）；帳本只在 `late_first` 讀，而樣本路徑在這種情況本來就會讀。
- 計數**跨過 Leave 與 `ClearState` 保留**，直到被取走（每個跑次結束時都會經過 EOF→`RequestReset`→`ClearState`）。
- `:117` 的參考年齡取樣不改（踢出在 digest 之內，要另外決定）。

### ③ Gateway 計數（D52 ②，經 D53 第 1 項縮小）

- 拿不到內容、在 Gateway 計數的：速率限制（全部種類）、解碼失敗（資料包層、Hello、Input、Actions）、未授權（只能記到「宣稱的」session，可被偽造，只能看、不能拿來處罰）、舊序號的 Actions／其他種類、未知種類、非 active 時的 input／actions。
- 動作的語意丟棄 A3、A5（`action_delivery.go:318-326`）P2 只計數（D53 第 8 項）。
- 下行 D1、D2（拒收 Match 的 snapshot，`server.go:505-516`）計數；D5、D6 留給第 10 批。
- 全部在產品層（新檔 `gateway/ingress_statistics.go`），用 `services/gyo_gateway` 既有的型別化錯誤（`session.go:17-21`）分類，可重用層不改。遇到不認得的 admission 錯誤計 `admission_unknown`。
- 既有計數不變：舊序號的包仍算進 `rate_accepted_packets`；`rate_limited_packets` 仍是全 Gateway 的總數（也包含 Hello）；`server.go:163` 那一行不動。

### log 行族與細節檔（D53 第 3 項：暫定 v1，第 10 批決定沿用或改版）

- 三種行，鍵名規則 `<原因>[_<種類>]_<單位>`，開頭固定 `version`、`player`、`window_ms`、`final`，新鍵只加在行尾，值一律十進位整數；鍵名由單一的名稱對照函式產生（Go、C++ 各一份）：
  - `player ingress statistics version=1 player=<id> window_ms=<int> final=<0|1> …`（Gateway，`fwd_*`＝已交給 Match，`drop_*`＝Gateway 執行的丟棄）
  - `gateway ingress statistics version=1 window_ms=<int> final=<0|1> …`（Gateway 全域）
  - `[ObjectFPS/PvP Match] ingress statistics version=1 player=<id> window_ms=<int> final=<0|1> …`（Match，player=0 桶每個視窗都寫）
- **每行是自上一行以來的增量**；`final=1` 是最後一段。分析器加總同一程序、同一玩家的所有行，並要求視窗連續。
- `final=1` 由 `sync.Once` 包的 `flushFinal()` 寫出，排在 `link.close()` 之後；玩家清單在 `runtimeFailed` 的 `clear(s.players)` 之前先複製。Match 在 `match_main.cpp:82` 之後補一行 `final=1`（現在結束時不寫任何統計行）。
- 細節檔 `match-ingress.jsonl`（`{"schema":"object_fps_pvp.match_ingress","version":1,...}`）：有 `--movement-trace` 時在同目錄推出（`-commands.jsonl`→`-ingress.jsonl`，其他檔名加 `.ingress.jsonl`），可用 `--ingress-trace` 覆寫；**沒有 `--movement-trace` 就不寫**。檔名不得以 `commands.jsonl` 結尾。有界，超過的只計 `suppressed`／`dropped`；I/O 失敗 exit 1（凍結的 runner 會判 Match 失敗，宣告要寫明）。
- **凍結分析器的約束**（已核對）：新行不得含 `rate_accepted_packets=… rate_limited_packets=… max_session_window_packets=…` 的連續組合（`action_probe.py:594`、`run_gameplay_soak.py:29` 取第一個）；不得含 `player send statistics` 等 `network_statistics.py:17-19` 的 pattern；不新增 `match-commands.jsonl` 的事件種類（`command_evidence.py:117-119`、`:138`）。
- 分析器把「缺 `final=1`」「缺某個視窗行」判成**未判定或停止，絕不當成 0**（Go 的 `log` 與 C++ 的 `std::clog` 都忽略寫入錯誤）。

## 不變量與守恆（L1 用決定性的測試釘住，L2 用 log 核對）

- **IG（Gateway 資料包層，每位玩家，精確）**：`received_datagrams`（`server.go:371` 的 `p.received`）＝Σ`fwd_*_packets`（主線、拒絕線、整包被已寫出去重、Hello 已處理、Actions 已交給 link）＋Σ`drop_*_packets`。`receivePacket` 與 `receiveActions` 的每一個 return 都必須恰好遞增一個結果計數。不屬於 session 的包另計全域的 `undecodable`、`other_version`、`room_unavailable`、`unknown_session`。**這一條抓的正是這次事故的型態**（第 3 批對抗式檢查，嚴重度高）。
- **I0（link 內部，每位玩家，精確）**：交給 link 的命令數（`input()`／`reject()` 成功返回時計；線間轉移不重算）＝寫出（在 `l.conn.Write` 成功之後、`l.mu` 下計）＋合併去重＋**已寫出去重**＋已解析截短＋拒絕線溢出＋Leave／forget 刪掉＋close 清掉＋寫出失敗後放棄（`drop_abandoned_on_close_commands`）。「交給 link」與「已寫出」用不同的鍵。
- **I1（IPC 邊界）**：Gateway 的寫出數＝Match 的 `received_commands`。`received_commands` 在 host 內、同一把 `mutex_` 下、用和拒絕相同的分桶規則計。以「整條 link 的總和（各玩家＋0 桶）」判定，依玩家的比對只用在沒被踢出的玩家。**只在下列條件都成立時判定，否則記成未判定**：match.log 在 `final=1` 之前有 `ipc closed reason=eof`；整個跑次只有一次 IPC 連線；沒有經過 IpcPause（host-ipc 模式）。
- **I2（Match 內部，精確）**：`received_commands`＝`accepted_new`＋`pending_copy`＋`late_first`＋`late_copy`＋`resolved_copy`＋`resolved_untracked`＋Σ`<拒絕原因>_commands`。
- **I3（條件式判定）**：在一個跑次的 `rejectFuture`、`rejectConflict`、`rejectLinkCap`、`written_conflict` 都是 0 時，Match 的 `pending_copy`＋`late_copy`＋`resolved_copy` ≤ Gateway 拒絕線的命令數＋過期後再轉送的命令數；clean 跑次兩邊都應為 0。
- **T3**：各玩家 `drop_rate_limited_*`（含 Hello）的總和＝全域的 `rate_limited_packets`。

## 已知缺口（寫進協議文件，到 pv7 或第 10 批為止）

1. Gateway 拿不到內容的丟棄（③ 的各類）：P2 只計數，Match 不知道。
2. ① 留下的兩種執行丟棄：拒絕線溢出、已解析截短。
3. 動作的語意丟棄 A3、A5。
4. 下行：D1、D2（Match 不知道自己的 snapshot 被拒）；D5、D6 在第 10 批。
5. 時間：Gateway 的收包時刻沒有送給 Match（pv7 的 dwell 是候選），Match 分不開 IPC 延遲與網路遲到。
6. Client 端的樣本流失（每次發布只帶最小值、`snapshot_` 被覆寫、IPC latest-wins、Client 佇列溢出）：host 端的計數由 ② 提供；Client 端的欄位在第 10 批（觀測缺口第 3 項）。
7. 帳本保留 600 Tick；晚超過 10 s 的記為 `aged`。
8. kernel 的 UDP 接收緩衝溢出（Gateway 讀不到，不屬於「Gateway 收到」）、Close 之後收到的包（範圍寫成「`final=1` 行之前處理的資料包」）。
9. 不正常的 Client：衝突窗口被併進主線時整則被 Match 拒絕，同一則裡其他已被標記的命令之後的副本會被去重，損失變成永久的（限該玩家）。契約寫明「`conflict_*` > 0 的玩家，其遲到與替代統計不可信」。

## 契約與文件（和實作同一個 PR）

- `protocol-v6.zh-Hant.md`：比照 `:54-56` 的先例加一節：

  > ### Runtime 輸入流（P2 資訊流修訂）
  > Gateway→Match 的 `PlayerInput` 不再是 Client 未確認窗口的完整副本：已寫出、而且內容相同的命令不再寫出；舊或未來 epoch、衝突與超過上限的窗口以不合併的單獨訊息轉送；舊序號的 Input 照樣轉送但不刷新 liveness。proto 欄位與 Match 的處理都不變。依候選期規則這屬於「改語意」，經使用者同意（D53）：Gateway 與 Match 在同一個 commit 切換，不混用修訂前後的 v6 候選。

- `runtime_v6.proto:31` 的註解改成：`Commands this link has not written before for the current epoch/life, strictly ordered, at most 12 per message. Merged and deduplicated by the Gateway; the Match treats repeats as no-ops.`；`protocol/README.md:22-23` 一起改。
- `network-architecture.zh-Hant.md`：
  - `:230`：32 只算游標以後的命令；游標以 Match 為準；Gateway 只用副本限制合併大小，超出的窗口原樣轉送。
  - `:231-233`：「不重執行、拒絕」由 Match 判斷；Gateway 照常轉送已解析的命令；已寫出去重的條文（四元組、保留期、換代清空、拒絕線不記入、Match 不得依賴去重）；每則 IPC ≤12 個命令；Gateway 執行的丟棄依原因計數（已知缺口到 pv7）。
  - `:242`「各層失效舊窗口」：改成由 Match 失效、Gateway 單獨轉送。
  - `:411-413` 附近：Match 略過移動輸入時也要依原因記錄。
  - `:856-858`：每秒 120 包的上限，被限制的包依玩家計數。
- 上下行同一原則（加在本批文件與 `08b-event-driven-results.md` 的「之後」）：事件＝新內容。下行由 08b 拿掉只帶 retired 的重複包；上行由 link 拿掉已寫出命令的副本。兩者都是可靠 TCP 上發送端的已送集合，失敗時連線致命。
- HANDOFF 未結事項「Gateway 與 Match 重複過濾已解析的命令」在實作後標為已解決。
- 「Gateway 職責」一節照 D49 留給第 11 批；第 11 批的 pv7 `GatewayDropReport` 規格見 `batch2/design3-counts-pv7.md` B（原因列舉要一次定完整）。

## Architecture Delta

1. **責任移動**：「已解析、過時、衝突、超過上限」的判斷由 Gateway 移回 Match（PEP→PDP，D49）。Gateway 留下的是執行：合併、路由、已寫出去重、兩種有上限的丟棄，以及依原因計數。`IpcHost.cpp:354` 的重複檢查併入 `ValidInput`。
2. **新概念**：link 的「拒絕線」與「已寫出集合」（產品內的結構，不是新的 subsystem）；Match 的 ingress 帳本（host 層、header-only）；一個 log 行族與一個細節檔（新的產品 Data Contract，暫定 v1）。
3. **介面**：`PvpMatch` 新增公開的 `AdmitInput`／`InputAdmission`；`MatchRuntimeHost` 新增 `TakeIngressStatistics()`、`DrainIngressRecords()`、`NoteWireRejection()`；Match 新增 CLI `--ingress-trace`。
4. **依賴**：沒有新的邊；`services/gyo_gateway` 與 Engine 不改。
5. **Removability**：沒有影響，全部在 object_fps_pvp 之下。
6. **Code smell（記錄，不在這次修）**：`substituted`（64）與帳本（600）重複保存被替代的序號，一致性靠測試；Gateway 仍持有 `lastResolved` 的副本（上限、清理、通知、epoch），留給第 11 批重新檢視；`p.commands` 的衝突檢查和 link 的內容比對重疊。

## 測試與突變（batch `v7-flow`，id 前綴 `v7-flow-`）

完整清單與預期訊息在 `batch2/plan-draft.md` §7、`batch2/dedup-evaluation.md` §7、`batch3/adversarial-report.md` §8；原型上的實測在 `batch2/design1-prototype/`（18 個）與 `batch2/dedup-prototype/`（9 個，① 的 18 個也重跑通過）。合計約 50 個 Go 突變、約 18 個 C++ 突變。要點：

- 「又悄悄丟了」的守門：`v7-flow-ig-silent-return`（在 `receivePacket` 加一個無聲的 return，`datagram outcomes N != received N`）、`v7-flow-fwd-filters-resolved`、`v7-flow-fwd-link-prunes-before-write`、`v7-flow-fwd-link-cap-counts-resolved`、`v7-flow-fwd-future-bound-underflow`、`v7-flow-ledger-*-silent`。
- 去重：只比身分不比內容、在合併時標記、拒絕線也標記、保留期少於帳本、永不過期、換代沒清空、Leave 後沒清、沒有計數、`observed` 沒有併入。
- 守恆：寫出數在 Write 之前就計、只計主線、close 清掉時不計、final 行沒有用 Once、帳本被 `ClearState` 清掉（`v7-flow-ledger-cleared-on-reset`）、分類不完整、帳本掛鉤改變 slack（`v7-flow-ledger-hook-perturbs-slack`）。
- ★：測試要用**從沒寫出過的命令**（例如先送序號 5 帶命令 3，再送序號 4 帶命令 2），否則會被去重蓋住，變成等價突變；另一個突變守住「舊序號的 Input 不刷新 liveness」。
- 要改寫的既有測試（列出原文與理由，比照 08b）：`server_test.go:306-310`、`:675-679`（v3 起斷言重送要照轉；原本的意圖「重送不會被當成錯誤」保留）、`:311-313`、`:653-660`、`:680-681`、`:689-690`、`:744-753`、`:788-797`、`lifecycle_test.go:49-51`；採用 ★ 後 `:682-684`。既有突變 `v7-08b-nonactive-actions-forwarded` 的 find 要更新；合併後重新核對全部 Gateway 相關突變的 find 唯一性。
- Go check 沒有 build 步驟，編譯失敗會被歸成 `wrong_failure`（`run_mutations.py:39-49`、`:86-92`），一樣不算 killed。
- 三端（Go、C++、Python）解析同一份 golden 樣本（`tests/object_fps_pvp/fixtures/ingress_v1/*`）；新的驗收工具 `ingress_evidence.py` 與 `test_ingress_evidence.py`，L2 之前凍結。

## 權威 digest 與凍結分析器

- digest 的情境直接驅動 `PvpMatch`／`MatchRuntimeHost`，不經過 Gateway，排除 `movementSlack*`（`AuthorityDigest.cpp:46`），每 Tick 每位玩家只送 1 個新命令。所以「digest 不變」只證明 `AdmitInput` 與正常路徑等價，**不證明帳本的掛鉤沒有改變 slack**（由上面的突變守住）。digest 改變就是停止條件。
- 去重與 ★ 不經過 digest 情境；live 跑次的權威結果會變（★ 讓替代變少，見預測 P11）。
- 凍結分析器不修改，L2 前核對 `analyzers.sha256`；不需要另出副本。但判定結果可能因行為改變而改變（例如 `command_evidence.py:415-422`「重設前沒有干擾」），宣告要寫明這是預期的行為變化。`coalesced_input_windows` 會下降，沒有凍結分析器讀它。

## 預期的行為變化與可被推翻的預測（L2 用；全部是推論，數字附來源）

| # | 情境 | 修之前 | 修之後的預測 | 推翻條件 |
|---|---|---|---|---|
| P1 | clean-* | late_* 為 0 | 相同；Match 的副本類＝0（I3），Gateway 的 `fwd_written_duplicate_commands` 約為每個命令 2 份 | after 的 clean 出現 late_first > 0，或副本類 > 0 |
| P2 | Client→Gateway 純延遲（x1 型） | 被替代的命令全部被 Gateway 吞掉，0 次 late 修正 | 見 J5a、J5b | — |
| P3 | Client→Gateway 延遲持續上升（斜坡） | 沒有負樣本、沒有 late 修正，靠 starvation 重設恢復 | 出現 late 修正（幅度被 ±2 Tick 夾住）；starvation 重設比 before 少或延後 | after 仍 0 次 late 修正，而 host 端有**連續**兩次發布都帶負樣本（中間沒有非負樣本），Client 收到且序號 > `settledAfter_` |
| P4 | network0／20／40 | late 修正次數為 before 的值 | late 修正次數上升（總和 after > before，只比方向）；late_first 每跑次 0～50 個（去重後 late_copy≈0） | 總和 after ≤ before |
| P5 | network*（★） | 舊序號的 Input 被丟 | `fwd_stale_sequence_input_packets` 中位數約 18／32／32（network0／20／40，故障 4 秒內；重播） | 差兩倍以上 → 先核對 relay 的 forwarded 與 Gateway 的 IG 資料包數，再判斷重播模型或前提 |
| P6 | downstream-1000ms | 已解析的副本不轉送 | Gateway 去重的副本約 1118～1284／跑次；Match 的 late_first＝0 | late_first > 0 |
| P7 | downstream-respawn-1000ms | 舊 life 的副本在 `server.go:413-415` 被丟 | Match 記 life_old／epoch_old 約 147 份（`arrival_identity.tsv`） | 為 0 |
| P8 | gateway-1000ms（SIGSTOP） | 只有全域的 rate_limited 總數 | 依玩家總和等於全域（T3）；47～60 包／跑次 | 不相等 |
| P9 | 所有跑次 | — | IG、I0、I2 精確成立；I1 在條件成立時精確成立 | 任何不相等 |
| P10 | D44 | 偶發的 epoch 重設 | 暴露上升（late 修正變多），只記錄 | 比例上升時依 D44 停下 |
| P11 | network*（★） | — | 替代的千分比 after ≤ before | after > before |

## L2 重新量測（事前宣告的大綱；宣告送核准前依 D45 做 1 次對抗式檢查，專門核對門檻來源）

- **目的**：照 D51 ⑤，在完整的資料上重新量測。這次 L2 的判定只核對「資訊流修好了」；下游（late 修正、B2、backlog 餘裕、09a／09b 的範圍）只記錄資料。
- **三棵樹，同一個 session 交錯**（D53 第 7 項）：before＝P2 的頭；flow＝本批；flow+09a＝本批加上 D48 第③步的 09a 原型（rebase 到 flow 上）。**三棵樹都各自建 Client（probe）、Gateway、Match**（09a 改的是 Client）。共用 arena。
- **案例**（約 114 次、約 1 小時；推論）：clean-60、network0／20／40、downstream-1000ms、downstream-respawn-1000ms、gateway-1000ms、x1 型 Client→Gateway 純延遲、斜坡型。U4（閒置主機的替代）併入這次。
- **注入工具**（事故紀錄第 9 節：之後的注入用純延遲型）：在 ActionRelay 寫一個獨立分支，只做 heap 延遲；**不得**經過三條既有的丟包路徑：`_ImpairedGateway._receive` 的 first／burst／5% 隨機丟包（`impaired_network.py:175-194`）、ActionRelay network 模式的 5%（`action_probe.py:232`）、block 模式的「只保留最新一包」（`:254`）。佇列上限 256（`:242-243`），最大延遲×每秒包數要 <256；延遲遞減段的斜率不得小於 −1（否則亂序）；注入前後核對 received＝forwarded。斜坡型是新參數，先在開發跑次確認不丟包；這次只記錄（綁到 09b 的 B2 與 ±2 Tick 夾限的決定）。
- **判定**：

  | 判定 | 門檻 | 來源 |
  |---|---|---|
  | J1 | digest 不變 | 停止條件；CTest 的兩棵樹比較 |
  | J2 | I1 精確相等（條件成立的跑次）；條件不成立記成未判定 | 程式推導：TCP 可靠而且有序；`final=1` 行 |
  | J3 | IG、I0、I2 精確相等 | 程式推導 |
  | J4 | clean 的拒絕線溢出＝0、已解析截短＝0；network* 與注入型只記錄 | 程式推導（第 3 批推翻了「只在寫出端阻塞時發生」：relay 一次放出大量包時也會溢出） |
  | J5a | 每次注入：late_first＋依 epoch／life／reset 關閉的數＋aged＝substituted，而且 never_arrived＝0（前提：relay 的 forwarded＝received） | 程式推導 |
  | J5b | host 端顯示連續 ≥`MovementPhaseLateSamples` 次發布帶負樣本、而且沒有被覆寫或取代的注入，Client 至少有一次 late 修正 | Client 規則 `LocalPlayerPrediction.cpp:118`、`:128`、`:131` |
  | J6 | 凍結分析器的 sha256 不變，每個跑次都能解析 | `analyzers.sha256` |
  | J7（驗證項） | downstream-1000ms 的 Match `tick_late_p99` 與 Tick 間隔最大值 after 不比 before 差超過 1 Tick（16.7 ms）；`cpu_s`、`ipc_iterations` 只比方向（預測 after ≤ before） | 程式常數；去重後 IPC 則數下降 |

- **停止條件**：digest 改變；J2／J3 不相等；after 出現 before 沒有的 epoch 重設種類，或比例明顯上升（D44）；乾淨跑次出現權威 Cooldown 拒絕；需要改 wire。

## 分批與時間（推論；整體約 15～19 小時，超過單一問題 2 小時的上限，每批結束都停下回報）

| 批 | 內容 | 檔位 | 時間 |
|---|---|---|---|
| F0 | 鍵名對照（Go、C++）、golden 樣本 v1、log 行格式；F1 與 F2 並行前先定 | high | 0.5～1 h |
| F1a（Go） | ①：主線、拒絕線、已寫出去重、★、link 的 I0 計數 | high；拒絕線的順序與切段、★ 的提交與 liveness 局部 xhigh | 3～3.5 h |
| F1b-1（Go，F1a 之後同一棵樹） | ③ 的計數與鍵、IG | high | 1.5～2 h |
| F1b-2（Go） | log 行、`flushFinal`（Once）、寫出計數點與放棄的計數 | high | 1～1.5 h |
| F2a（C++，與 F1 並行） | `AdmitInput`、拒絕計數、IpcHost、統計行、`final=1` 行 | high；`AdmitInput` 的等價性 xhigh 審查 | 約 3 h |
| F2b-1（C++） | 晚到紀錄、保留期、跨過 `ClearState` | high | 1.5～2 h |
| F2b-2（C++） | 細節檔、樣本流失計數 | high | 1.5～2 h |
| F3 | 跨端測試（I1、I2）、`ingress_evidence.py`、文件條文、HANDOFF | medium～high | 1.5～2 h |
| L1 | Go test／vet／race、約 50 個 Go 突變、全量 CTest、digest 兩棵樹比較、約 18 個 C++ 突變、backpressure 與 IPC 驗收、凍結 runner 的短跑次加 sha 核對 | medium | 約 2～3 h 機器時間 |
| L2 | 注入工具與斜坡的準備、三棵樹的開發跑次、正式 session、分析 | 宣告 high，對抗式檢查 xhigh | 約 3～4 h |

- ① 與 ③ 都改 `server.go` 的 `receivePacket`，不能並行實作；Go（F1）與 C++（F2）可以並行。
- ① 的拒絕線不得早於 ② 的拒絕計數合併（只做 ① 的話，epoch、未來、衝突三類會從「Gateway 無聲丟」變成「Match 無聲丟」）。

## 對既有順序的影響

- **09a**：機制與設計保留；實作排在 L2 之後，範圍依量測決定。L2 用 D48 第③步的 09a 原型當第三棵樹。
- **09b**：缺陷 A／B 的影響大小、B2 的設計、頻率類的結論（R3、R4、R12）在 L2 之後於新基準上重做。
- **D48 第③步**：結果保留，標示「在截斷的資料上量到」；不重跑。
- **M0、FireGate 凍結、C2**：排在本批、L2、09a／09b 的決定之後（D51 ⑤）。C2 的 after 要宣告包含資訊流修正；C2 的 before（`fee92ff`）不變，凍結分析器照常可用（版本仍是 6）。
- **FireGate 重推條件 11**（`09-firegate-c2.md`）：M0 還沒跑，P2 的部分靠順序滿足；建議改寫成「第 11 批若讓 Gateway 的回報進入相位樣本或品質判定，就重推；只做計數時不重推」，加跑的注入用純延遲型。
- **D44**：暴露上升，L2 依樹記錄偶發 epoch 重設的頻率。**第 16 批**：整體回歸的基準換成修好之後的資訊流。
- **第 10 批**：觀測缺口第 1、2、4 項大部分在本批完成；剩下 D5、D6、舊序號的 Actions、Client 端的欄位（第 3 項）；log 契約決定沿用或取代 ingress v1。
- **第 11 批**：pv7 的 `GatewayDropReport`（原因列舉一次定完整，dwell 的定義只有一個）；「Gateway 職責」一節；重新檢視 Gateway 的狀態；是否轉送 A3／A5。
- **穩定基線**：v5、v6 的 `STABLE_BASELINE.md` 已加「已知問題」；要不要重新發佈，本批修好之後再決定。

## 停止條件（實作期間）

- 需要改 wire（proto 欄位）或動 `services/gyo_gateway`、Engine。
- 權威 digest 改變。
- 任何既有測試或凍結分析器需要放寬判定才能通過（改寫須列出原文與理由，經使用者同意）。
- 守恆在 L1 的決定性測試中不成立。

## 風險

1. 行為改變：late 修正變多，D44 的重設可能增加，C2 的歸因跟著改變（L2 的停止條件；C2 宣告寫明）。
2. 關閉時的競態：已用「寫出後計數、Once、EOF 條件、未判定」處理；頻率是推論。
3. 規模：15～19 小時，分 8 批，每批停下回報。
4. 重複的資料結構（64 與 600）：一致性只靠測試。
5. Data Contract 先於第 10 批：所有檔與行都帶版本，三端解析同一份 golden 樣本。
6. ★ 的重播估計依賴「Gateway 照 relay 放出的順序讀包」（第 3 批判定在 loopback 上成立，推論；唯一例外是 kernel 丟包）。
7. 偽造：未授權、Hello 解碼失敗的歸屬可以被偽造；範圍外的既有弱點（偽造 endpoint 加 session id 就能讓受害者觸發速率限制）只報告。

## 需要決定的事

1. **核准實作的分批**（F0～F3、L1、L2）與檔位（high，局部 xhigh：拒絕線的順序與切段、★ 的提交與 liveness、`AdmitInput` 的等價性）。每批結束停下回報；高於主對話的檔位以這次核准為準。
2. **批次名稱 08c**（主對話取的；要改的話告訴我）。

## 規劃的過程與證據

- 證據目錄：`build/target/_build/test/logs/pvp-v7-infoflow-plan-20261010/`
  - 第 1 批：`facts-gateway/`、`facts-match/`、`facts-contract/`（`report-from-workflow.md`，以及 `facts-gateway/*.tsv` 與分析腳本）；一次跑完版停掉後的部分輸出在 `facts-*-stopped-2002/`。
  - 第 2 批：`batch2/design1-forwarding.md`、`design2-ledger.md`、`design3-counts-pv7.md`、`plan-draft.md`；原型 `design1-prototype/`；重播腳本 `design3-scripts/`；第 6 項的評估 `dedup-evaluation.md` 與原型 `dedup-prototype/`。
  - 第 3 批：`batch3/adversarial-report.md`。
- 被推翻或修正的說法（依時間）：
  - 「只刪 `server.go:421-423` 就好」：會讓 `:424` 下溢、整包被丟（第 1 批）。
  - 「D48 第②步的 0／5 只是 Gateway 吞掉」：relay 也只保留最新一包（第 1 批）。
  - 「UDP 舊序號拿不到內容」：payload 完整，只是不解碼（第 2 批 ③）。
  - 「Gateway 轉送數＝Match 分類總數」：合併去重與游標落差讓它不成立，改成精確的 I0～I2（第 2 批評審）。
  - 「不在 Gateway 去重，bitmap 會說謊」：標記點在 `batch()` 時不會（第 6 項評估）。
  - 「重新量測只量『資訊流＋09a』」：和「09a 的範圍要等量測後才決定」矛盾，改三棵樹（第 2 批評審）。
  - 「拒絕線溢出只在寫出端阻塞時發生」、J5 的「一半」、I1 沒有前提、缺 IG（第 3 批）。
- 每批的檔位、時間與使用者的決定記在 HANDOFF 的進度記錄器（2026-10-10）與 D52、D53。
