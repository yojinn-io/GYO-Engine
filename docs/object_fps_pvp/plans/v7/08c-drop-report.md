# 第 08c 批：Gateway 丟棄回報（讓 Match 知道丟了什麼、為什麼丟）

狀態：**實作中**（2026-10-10 分批與檔位核准，D54；F0、F1a、F1b-1、F1b-2、F2a、F2b-1、F2b-2、F3、L1、D56 修正批、L2 準備（L2P-analyzer、L2P-injector、L2P-trees）完成；L2 通過（2026-10-11，D61 的宣告；114／114 次，J1～J7 全部成立））。PR 線 P2（PR #73）。排在 09a、09b 的重做、M0 與 FireGate 常數凍結、C2 之前（D51 ⑤）。
依據的決定：D49（Gateway＝PEP、Match＝PDP）、D51（問題的重新定義；⑨ 定性為整合缺陷；⑩ 每一種丟棄都要讓 Match 知道）、D52（第 1 批之後的三個決定）、D53（照建議的組合）。
起因：[2026-10-10 事故](Incident/2026-10-10-gateway-silent-drop.md)。
檔位：規劃 ultracode，分三批、每批停下回報（使用者 2026-10-10）：第 1 批事實核對 3 位（high）；第 2 批方案 3 位＋評審 1 位（high），另評估第 6 項的去重 1 位（high）；第 3 批對抗式檢查 1 位（xhigh）。實作 high，局部 xhigh（見「分批」）。

批次名稱 08c 是主對話取的：它改的是 Gateway 與 runtime link（和 08b 同一層），而且必須在第 09 批的 M0 之前完成。

## 先讀這一段

- **問題**：產品層的 Gateway 用權威狀態的延遲副本 `lastResolved` 悄悄丟掉已解析的命令（`gateway/server.go:421-423`），link 寫出前再剪一次（`runtime_link.go:192-197`）。Gateway 的副本只比 Match 落後約 0.5 ms，所以 Match 幾乎量不到 Client 端的網路遲到；late 修正、backlog 餘裕、FireGate 的推導都在截斷的樣本上校準。Gateway 還有 30 多處「收到卻不轉送」，只有速率限制有計數；Match 自己的拒絕也是無聲的。
- **目標**：Gateway 收到、屬於某個 session 的每個資料包，只會有兩種結果之一，沒有第三種（無聲）：（更正，2026-10-11 死信隊列評估：這一條在資料包層成立（IG）；動作交給 link 之後，在 Leave、close、寫出失敗時仍可能在 link 內被清掉，見已知缺口第 10 項，D56 補計數。）
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
- **★ 舊序號的 Input**（D53 第 1 項）：`session.go:89-91` 只看標頭就回 `ErrStale`，但 payload 完整（Client 的 Hello、Input、Actions 共用 `++sentSequence`，`ClientConnection.cpp:349-350`）。改成照樣解碼並轉送，但**不呼叫** `CommitSequence`（不刷新 liveness、不影響速率計算）。（更正，2026-10-11，F3 核對：「不影響速率計算」不對；舊序號的包照樣算進速率限制與 `rate_accepted_packets`（`server.go` Input 分支的 `admit` 已計入，見該處註解「The rate limit already counted it」），和 ③ 一致；契約只寫「不刷新 liveness」。）重播估計 network* 約 4～5% 的 Input 首份副本原本因此被丟（推論，第 2 批方案 ③）。
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
4. 下行：D1、D2（Match 不知道自己的 snapshot 被拒）；D5、D6 在第 10 批。另外，下面幾個下行出口不計數，也不是 ingress 丟棄：D4（給已移除或非 active 玩家的 snapshot；編碼失敗的分支走不到）、D7（非 active 玩家的整批 ActionResults；規劃時已決定「Match 已知、不計數」：Match 自己的帳本保留未退役的決定，直到移除玩家）、D9（玩家不存在時的 JoinResult、Evicted；Match 最終一定知道，因為 Leave 排在前面、TCP 有序）、D8（結果編碼失敗，走不到）。（2026-10-11 依死信隊列評估補記，D56。）
5. 時間：Gateway 的收包時刻沒有送給 Match（pv7 的 dwell 是候選），Match 分不開 IPC 延遲與網路遲到。
6. Client 端的樣本流失（每次發布只帶最小值、`snapshot_` 被覆寫、IPC latest-wins、Client 佇列溢出）：host 端的計數由 ② 提供；Client 端的欄位在第 10 批（觀測缺口第 3 項）。
7. 帳本保留 600 Tick；晚超過 10 s 的記為 `aged`。
8. kernel 的 UDP 接收緩衝溢出（Gateway 讀不到，不屬於「Gateway 收到」）、Close 之後收到的包（範圍寫成「`final=1` 行之前處理的資料包」）。
9. 不正常的 Client：衝突窗口被併進主線時整則被 Match 拒絕，同一則裡其他已被標記的命令之後的副本會被去重，損失變成永久的（限該玩家）。契約寫明「`conflict_*` > 0 的玩家，其遲到與替代統計不可信」。
10. 動作在 link 內的丟棄（2026-10-11 死信隊列評估找到，D56 補計數）：Leave／踢出、close、寫出失敗時，link 動作視窗裡**從沒寫出過**的 shot 請求與還沒轉送的 ACK 被清掉；P2 由 Gateway 依玩家與原因計數（鍵見 fixtures README），Match 不知道（和第 1 項同一等級）。已寫出的部分 Match 已有，Leave 時計成 `leave_discarded_actions_shots`。close 與寫出失敗時被清掉的 controls（Join／Leave 訊息）不計數：房間已經結束。link 內 shot 的守恆（交給 link 的不同請求＝寫出過的＋三種丟棄）由 `fwd_handed_actions_shots`、`fwd_link_written_actions_shots` 兩個鍵在 log 上判定（D59，`ingress_evidence.py` 的 `link_shots`）。

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
| P5 | network*（★） | 舊序號的 Input 被丟 | `fwd_stale_sequence_input_packets` 中位數約 18／32／32（network0／20／40，故障 4 秒內；重播）（更正，2026-10-11，09R-eval：約 18／32／32 借自其他建置的分布；v7 自己約 13／16.5／24.5，而且隨送出相位變動。見「L2 的結果」的 P5。） | 差兩倍以上 → 先核對 relay 的 forwarded 與 Gateway 的 IG 資料包數，再判斷重播模型或前提 |
| P6 | downstream-1000ms | 已解析的副本不轉送 | Gateway 去重的副本約 1118～1284／跑次；Match 的 late_first＝0 | late_first > 0 |
| P7 | downstream-respawn-1000ms | 舊 life 的副本在 `server.go:413-415` 被丟 | Match 記 life_old／epoch_old 約 147 份（`arrival_identity.tsv`） | 為 0 |
| P8 | gateway-1000ms（SIGSTOP） | 只有全域的 rate_limited 總數 | 依玩家總和等於全域（T3）；47～60 包／跑次 | 不相等 |
| P9 | 所有跑次 | — | IG、I0、I2 精確成立；I1 在條件成立時精確成立 | 任何不相等 |
| P10 | D44 | 偶發的 epoch 重設 | 暴露上升（late 修正變多），只記錄（更正，2026-10-11，09R-eval：只在持續延遲（x1、ramp）時成立；network 型（遺失加 ≤30 ms 抖動）不成立，L2 的 late 修正 0，機制見「L2 的結果」的 P4。） | 比例上升時依 D44 停下 |
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

## L2 的事前宣告（2026-10-11 使用者核准，D61；依 D45 做了 1 次門檻來源的對抗式檢查（xhigh），修正已併入）

草稿原檔 `build/target/_build/test/logs/pvp-v7-08c-l2-prep-20261011/declaration-draft.md`（sha256 `15de5512…`），腳本 `run_l2.py` `f95839db…`、`judge_l2.py` `e6a8916f…`、`sleeper.py` `98105dbf…`，凍結清單 `frozen-l2.sha256` `d2a8699b…`（44 檔）。下文的 `prep/` 指那個目錄。

- 狀態：**定稿**（2026-10-11 使用者核准，D61：「OK，照建議，行きましょう」；第 14 節 1～7 照建議）。以下保留草稿的文字，只改這一行與第 14 節的標題。
  - 第 1 版：L2P-declare 的草稿部分，2026-10-11 14:55～15:13，1 位 high；保留在 `prep/v1/`（`declaration-draft.md` `6aef2975…`、`judge_l2.py` `977f91e9…`、`run_l2.py` `7f7957c8…`）。
  - D45 的門檻來源對抗式檢查：1 位 xhigh，`prep/xhigh-check.md`。
  - 本版（15:32 起）依該檢查的「送核准前必須修正」6 項與「順手修」各項修正；對應表在第 15 節。
  - 下一步：主對話整合後送使用者核准。核准後定稿併進 `docs/object_fps_pvp/plans/v7/08c-drop-report.md` 的「L2 重新量測」。
- 依據的頭：
  - 主 checkout `claude/pvp-v7-p2`，修正時是 `5a78831`（乾淨）。
  - 工具（`run_client_delay.py` 的 CRLF 修正）在 `b57291d`；`b57291d..5a78831` 只改 `docs/`。
  - 宣告提交後的 commit 記為 **H**。主 checkout 從 H 到 session 結束不得改動（driver 要求 `--main-head H`，每個跑次之前都核對）。
- 依據的決定：D44、D45、D48（第③步的做法）、D50、D51 ⑤、D53 第 7 項、D56、D57、D58、D59、D60。
- 本草稿的工具與開發跑次都在 `build/target/_build/test/logs/pvp-v7-08c-l2-prep-20261011/`（以下簡稱 `prep/`）。指令見 `prep/commands.txt`，雜湊見 `prep/prep-files.sha256`。
- 先例：第 08b 批「L2 的事前宣告」；D48 第③步的 `declaration.md` 與 `session3.py`。
- 數字沒有標「實測」的，是程式推導或推論。

### 1. 目的（D51 ⑤）

這次 L2 **只判定「資訊流修好了」**：

- Gateway 收到的每個資料包都有結果（IG）。
- 交給 link 的命令與 shot 都有去向（I0、`link_shots`）。
- Gateway 寫出的就是 Match 收到的（I1）。
- Match 收到的每個命令恰好落在一類（I2）。
- 注入的 Client→Gateway 遲到，Match 收得到、也分得出來（J5a）。這一項的範圍見下面兩點。

J5a 能判的範圍（依 xhigh 檢查 A 更正；第 1 版寫「注入的遲到 Match 看得到」太寬）：

- **(b)**：每個注入跑次至少有 1 個 `late_first`，也就是 Match 確實收到了被替代之後才到的命令。
- **(a)**：Client 送出過、但到跑次結尾都沒有到 Match 的被替代命令，數量是 0。這只限關閉原因是 removed、reset、overflow、aged、end 的紀錄。關閉原因是 epoch 或 life 的紀錄不在 (a) 之內：那些命令後來以舊 epoch／life 到達，Match 依原因計數。

下游的事項**只記錄、不判定**：late 修正的次數與幅度、B2、backlog 餘裕、starvation／backlog 重設、09a／09b 的範圍、遊戲判定（run_case、backpressure 的判定）。這些在 L2 之後於新基準上決定（D51 ⑤）。

### 2. 三棵樹（D53 第 7 項、L2P-trees）

| 樹 | worktree | commit | 內容 |
|---|---|---|---|
| before | `/Users/karasu/Code/Source/GYO-Engine-l2-before` | `c2b76cd05eb13ab7a30d59f6dc3808834e0b1c87` | P2 在 08c 之前的頭 |
| flow | `/Users/karasu/Code/Source/GYO-Engine-l2-flow` | `eabb107e610a0423aa7c6903e37a9e3be424524b` | 08c（含 D56、D59） |
| flow+09a | `/Users/karasu/Code/Source/GYO-Engine-l2-flow09a` | `cb921e013a51205965fd3779aac78088b7049d1b` | `eabb107` 加上 D48 第③步 09a 原型 `e6885b7` 的 Client 改動（`git diff 8fabfa6 e6885b7`，存為 `l2p-trees/09a-e6885b7.diff`）；本地分支 `l2-flow09a-cb921e0`，不推送 |

- 三棵都是 detached、乾淨（`git status --porcelain` 空），各自乾淨建 Match、Gateway、probe（L2P-trees，`l2p-trees/build-trees.sh`）。
- 各樹 Gateway 內嵌的 `vcs.revision` 等於各自的 commit，`vcs.modified=false`（`l2p-trees/gateway-vcs.txt`）。
- flow+09a 對 flow 的差分只有 4 檔：
  - `Movement.hpp`：新增常數 `InputSendDrainRate` 與註解；
  - `ClientConnection.cpp`；
  - `worker_main.cpp`（probe）；
  - `MovementRecoveryTests.cpp`。
  - `InputSendDrainRate` 只在 `ClientConnection.cpp:128`、`:609`、`:617`、`worker_main.cpp` 與測試中使用（`git grep`），Match 不讀它。
- 執行檔 sha256（`l2p-trees/artifacts.sha256`；完整值寫在 `run_l2.py` 的 `ARTIFACTS`；driver 每個跑次之前核對）：

| 樹 | Match | Gateway | action probe | timing probe |
|---|---|---|---|---|
| before | `2473c68c…` | `36615477…` | `e8adc287…` | `ca3bdc51…` |
| flow | `8d154a3a…` | `4f0caaca…` | `4949302f…` | `ea5c569e…` |
| flow+09a | `b83f3548…` | `e5041519…` | `adf1d13f…` | `8a380cab…` |

- arena：主 checkout 的 `assets/object_fps_pvp/pvp_arena.json`，`0026013c731a7d7317ce6be4f84f897b91fa1b807c9621d60fb608570f1775a5`。三棵樹的原始檔與建置目錄裡的副本都相同。
- 三棵樹的執行檔雜湊彼此不同，不代表程式不同（建置路徑不同）。Match 的等價由 J1（digest）與上面的差分範圍證明。

### 3. 工具與凍結清單（D60）

- 所有 runner 與分析器都從主 checkout 執行。三棵樹用同一份工具，所以彼此可比。
- **凍結清單 `prep/frozen-l2.sha256`**：44 檔，清單本身的 sha256 是 `d2a8699bd5dca93387df15ce891e0d26ff4f3937c7cd6976a153fa62462a5b89`。內容＝既有凍結分析器 40 檔（`pvp-v7-08c-impl-20261011/l1/analyzers-from-09s1.sha256`，`78bba320…`）＋L2 的 4 檔（`l2-frozen-candidates.sha256`，`5d6110d9…`）：

  | 檔 | sha256 |
  |---|---|
  | `ingress_evidence.py` | `95ec65425e9b5a29604ea0809969bd41a73f7f5891f39aed20a42ee240fcbea1` |
  | `test_ingress_evidence.py` | `040b83d226efbd6bf7e65a1d247a1f7d1badbe3c9ded80d995ba5a360fd2c369` |
  | `run_client_delay.py` | `4f522e92496f877315a9969c0f01f21fe94eecd41fcbae6d3e729ed9ac5a04b3` |
  | `test_run_client_delay.py` | `ee89b282bf42e670405b99bb30e8541d81d118044ae6f610a3e485d6c7eb8c2c` |

- 核對時點：
  - 2026-10-11 在 `a1652b0` 與 `5a78831` 上 `shasum -a 256 -c` 44／44 OK；
  - session 開頭不符就拒絕執行；
  - 每個跑次之前都核對；
  - 結尾的結果記在 `summary.json` 的 `frozen_ok_end`。
- `run_client_delay.py` 以 `--frozen-list prep/frozen-l2.sha256` 執行。它自己會再核對清單中的每個檔，並要求 `action_probe.py`、`impaired_network.py` 在清單內、而且等於檔內寫死的 `FROZEN`（`run_client_delay.py:409-427`；追跡的文字檔先把 CRLF 換成 LF 再算，`b57291d`）。
- `run_gameplay.py`、`backpressure_probe.py` 沒有自己的核對，由 driver 核對（兩者都在 40 檔之內）。
- **`run_network.py` 不在凍結清單內**：第 09 批 S1 起就移出清單，但 runner 會 import 它。它只受 driver 的 git 狀態核對保護：H 乾淨，而且 `b57291d..H` 在 `docs/` 以外沒有差分。
- `tests/object_fps_pvp/compare_authority_trees.py`（J1）同樣只受 git 狀態核對保護。
- 本批新寫、核准時一起凍結的檔，雜湊在 `prep/prep-files.sha256`（本版：`run_l2.py` `f95839dbdbc6cfa9b18e812d673fca839c6f89b8afdec5cfd97d6afa96d48956`、`judge_l2.py` `e6a8916f83063e0233fbb810608397d395a7187292dd063483c6e8c0d056d480`、`sleeper.py` `98105dbf71a5818d20bb2c82083f7a3cbbdb9c236b43514537c45845442a9567`）：
  - `run_l2.py`：session 驅動；
  - `judge_l2.py`：判定。它只呼叫凍結的 `ingress_evidence.py`，不重新實作守恆；J5a 的注入部分與 relay 對 Gateway 的核對由它自己判定（第 8 節）；
  - `sleeper.py`：照抄 D48 第③步的 `declare/sleeper.py`。
- driver 的核對：
  - 正式執行要求 `--judge-sha`、`--driver-sha`，和實際的檔案不符就拒絕；
  - `sleeper.py` 的 sha256 寫死在 `SLEEPER_SHA`；
  - 三者都在每個跑次之前的狀態核對（`state()`）之內。

### 4. 主機

- 照 D48 第③步與 08b：
  - session 期間不做開發；
  - 以 `caffeinate -dims` 執行；
  - 前景沒有其他使用者程式；
  - **暫停 PR #73 的 auto-fix**；
  - 背景的其他 agent 與分析先停。
- **閒置閘門**（`--idle-gate`，正式執行必須帶；和 C1 第 2 次、D48 第③步相同）：第 1 輪之前連續 3 次 5 秒 sleeper，每次最大值都 <10 ms。不成立就每 30 秒重試，最多 30 分鐘，仍不成立就停下。
- 每輪前後各記 1 次 5 秒 sleeper 與前 5 名 CPU 程序（`host.jsonl`）。gameplay 跑次的 `timer_baseline`（`result.json`）照記，用來分層，不判定。
- 子程序的管理：
  - 每個子程序都跑在自己的 session（`start_new_session`），所以終端機的 Ctrl-C 只會打到 driver。driver 跑完目前的跑次後停下。
  - 逾時（300 秒）時先送 SIGTERM 給 runner，讓它的收尾照常執行；15 秒後再對整個 process group 送 SIGKILL。
  - 每個跑次結束後，group 裡留下的程序一律殺掉，並記在 `group_left_over`。

### 5. 案例、次數、順序與時間

每棵樹 38 次、合計 **114 次**。各案次數照第 2 批計畫 §10（`pvp-v7-infoflow-plan-20261010/batch2/plan-draft.md`）；08c 文件只寫「約 114 次」。

| 案例 | 每樹次數 | runner | probe | 參數 |
|---|---|---|---|---|
| clean-60 | 4 | `run_gameplay.py --case clean-60` | action | — |
| network0 | 4 | `run_gameplay.py --case network0` | action | 凍結的 relay network 模式 |
| network20 | 6 | 同上 | action | |
| network40 | 6 | 同上 | action | |
| downstream-1000ms | 4 | `backpressure_probe.py --layers downstream --durations-ms 1000`（D60） | timing | |
| downstream-respawn-1000ms | 2 | `run_gameplay.py --case downstream-respawn-1000ms` | action | fault_at 9.6 s（`run_gameplay.py:21`） |
| gateway-1000ms | 2 | `backpressure_probe.py --layers gateway --durations-ms 1000`（D60） | timing | SIGSTOP |
| x1（Client→Gateway 固定延遲） | 6 | `run_client_delay.py --profile x1 --delay-ms 200 --window-ms 2000 --fault-at 7.5` | action | D60 |
| ramp（斜坡） | 4 | `run_client_delay.py --profile ramp --peak-ms 250 --rise-ms 1500 --fall-ms 500 --fault-at 7.5` | action | D60 |

- **交錯**：
  - 共 6 輪，第 k 輪跑 `ROUND_CASES[k]` 的案例，依上表順序。
  - 各案例的輪次：network20、network40、x1 每輪都跑；clean-60、ramp 在第 1、3、4、6 輪；network0 在第 1、2、4、5 輪；downstream-1000ms 在第 2、3、5、6 輪；downstream-respawn-1000ms、gateway-1000ms 在第 2、5 輪。每輪 18～21 次。
  - 每個案例的三棵樹連續跑。
  - **開頭的樹依案例與重複次數輪替**：順序是 TREES 輪轉 (rep−1＋案例序號) mod 3。所以在每個案例內，開頭的樹在次數允許的範圍內平衡：6 次的案例 2／2／2，4 次的 2／1／1，2 次的 1／1／0（`--list-only` 會印出 `first_tree_per_case`）。
  - 第 1 版只依輪次輪轉；xhigh 檢查指出 downstream-1000ms 的 before 從來不排第一，gateway 與 respawn 案例總是 flow 先、before 後，所以改成依案例輪替。
  - 完整清單：`python3 -I prep/run_l2.py --list-only`。
- **時間**（依開發跑次的實測推算）：
  - gameplay 與 Client 延遲每次 20～22 秒（L2P-trees 的 `runs/*/timing.txt`）；
  - backpressure 每次 13～14 秒（`prep/dev-bp/timing.txt`）；
  - 每次判定 1～2 秒，狀態核對約 0.5 秒（本機量 0.52 秒）；
  - 合計 96×約 23 秒＋18×約 15 秒，約 41 分；加上 sleeper 約 1.5 分、J1 比較約 2 秒（本機以既有 runner 量 1.9 秒）、閒置閘門 ≥15 秒：**約 43～50 分**。閒置閘門要等的話，最多再加 30 分。
  - session 之前要先建兩個 digest runner（第 12 節），約 5～15 分（推論，視增量建置而定）。

### 6. 注入的參數與有效性（D60、L2P-injector）

- 注入的是 **Client→Gateway 方向的純延遲**（`run_client_delay.py`）。做法是把 D50 (b1) 套在 UDP relay 上：在 runner 自己的行程裡替換 `action_probe.ActionRelay`，凍結檔不變。
  - Gateway→Client 方向照收照送。
  - 三條既有的丟包路徑都走不到：`impaired_network.py:175-194`、`action_probe.py:232`、`:254`。
- 參數：
  - x1：200 ms×2000 ms；
  - 斜坡：上限 250 ms、上升 1500 ms、下降 500 ms（斜率 −0.5 ≥ −1，所以不會亂序）；
  - 兩者都在 probe 時鐘的 7.5 s 開始：7.35～10.25 s 之間只有 2 個預排的動作（`l2p-injector/report-from-agent.md`「建議的 L2 參數」）。
  - x1 結束時不超車：窗口結束後 200 ms 內到的包，排在延遲包後面一起放出（相當於斜率 −1 的排空）。
- **延遲上限 ≤250 ms 的依據**：
  - 瓶頸是 Gateway 每個 session 每秒 120 包（`services/gyo_gateway/session/session.go:122`）。
  - 實測 200 ms 時每個 session 1 秒最多 99 包、250 ms 時 103 包（`l2p-injector/report-from-agent.md`）；推論約 300 ms 會碰到 120。
  - 佇列（依 xhigh 檢查更正）：`action_probe.py:242-243` 的 256 是凍結 relay **network 模式**的佇列上限。`DelayRelay` 自己的佇列沒有硬上限；`run_client_delay.py` 沿用 256（`QUEUE_LIMIT`，`:77`），用它做開跑前的設計約束「最大延遲×每秒包數 <256」（`:151`），並以它的 3／4＝192（`QUEUE_NEAR`，`:78`）當無效門檻。以每秒 240 包算，設計約束到約 1067 ms 才會碰到，所以不是瓶頸。
- **無效門檻**（runner 自己判定；任一成立，該跑次就是 `injection_invalid`，driver 停下，第 11 節）：
  - Gateway 有任何速率拒絕：`rate_limited_packets` >0（`run_client_delay.py:467-469`）。這是 Close 時的累計值（`server.go:183`）。
  - 佇列最大深度 ≥192（`QUEUE_NEAR`，`:78`）。
  - 送出延遲的 p99 >10 ms（`LATENESS_P99_MS`，`:80`）。
  - 注入前或結束時，任一方向 received≠forwarded、佇列沒清空、有轉送不出去的 Gateway 包、relay 錯誤、實際延遲偏離參數，或 `delayed`＝0（`round_problems`，`:429-470`）。
  - 這些都是 L2P-injector 訂的工具門檻，不是由程式常數推導的量測門檻；D60 照建議接受。
  - 開發跑次的實測：佇列最大 32～35；送出延遲 p99 2.6～2.8 ms（L2P-injector）；速率拒絕 0（L2P-trees 6 次）。
- **run_case 的遊戲判定在三棵樹都只記錄**（D60：注入會擾動預排的射擊）。L2P-trees 的 before x1 判 failed（`post-life-recovery-shot`），只記錄。
  - 但 run_case 拋出例外時（`result.json` 沒有 `artifacts`），算 runner 錯誤，比照 `run_client_delay.py:572-574`。

### 7. backpressure runner 的開發跑次（D60；只記錄，不是量測）

2026-10-11 14:58～15:00，`prep/dev_bp.sh`：

- 每棵樹各跑 1 次 downstream-1000ms、1 次 gateway-1000ms，指令和 session 相同：各樹自己的 Match、Gateway、timing probe；主 checkout 的 arena 與 runner。
- 開頭核對了兩份凍結清單與 `l2p-trees/artifacts.sha256`。
- 主機沒有過閒置閘門。

| 樹 | 案例 | 秒 | backpressure 判定 | ingress | I1 | `link_shots`（carried／lines） | J4 溢出／截短 | 拿不到內容的丟棄 | late_first（每位） | 副本類 | 重設 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| before | downstream | 13 | passed | undetermined（沒有 ingress 行，預期如此） | — | — | — | — | — | — | starvation×2（+0.68 s） |
| before | gateway | 13 | passed | undetermined（同上） | — | — | — | — | — | — | starvation×2（+0.53 s） |
| flow | downstream | 13 | passed | passed | passed | 4／4 | 0／0 | 0 | 0／0 | 0 | starvation×2（+0.68 s） |
| flow | gateway | 13 | passed | passed | passed | 4／4 | 120／0 | 0 | 0／0 | 0 | starvation×2（+0.53 s） |
| flow+09a | downstream | 14 | passed | passed | passed | 4／4 | 0／0 | 0 | 0／0 | 0 | starvation×2（+0.68 s） |
| flow+09a | gateway | 13 | passed | passed | passed | 4／4 | 36／0 | 2（速率限制的 Input） | 0／0 | 0 | starvation×2（+0.53 s） |

- 結論：
  - backpressure runner 在三棵樹都能用。
  - 修好的兩棵樹，ingress 全部 passed：IG、I0、全域、T3、I2、J5a 等式、S1、S2、I1、I3、trace、`link_shots`。
  - 新鍵全部帶齊：carried＝lines＝4。
- 只記錄：
  - 恢復時間 0.30～0.33 s；`stable_queue_30_tick_sum_max` 36～38；`maximum_authority_gap_seconds` 16.73～16.79 ms。
  - 兩個案例在三棵樹都有 2 次 starvation 重設（每位玩家 1 次，都在故障窗內）。
  - Match 的 `substituted` 每位 29。依 `match-ingress.jsonl`，這些都是被 epoch 重設關掉、沒有到達的紀錄：downstream 的 58 筆 Client 都沒有送出；gateway 的約 20 筆 Client 送出過，其餘沒有送出（本草稿以和 `judge_l2.py` 的 (a) 相同的比對臨時分類：`match-ingress.jsonl` 對 `clients-commands.jsonl` 的 `sent`；沒有存檔）。
  - `epoch_old` 每位：downstream 228，gateway 274～334。
  - gateway-1000ms 的拒絕線溢出（120、36）和 L1 的 36、132 同一量級（`l1/summary.md`），依 J4 只記錄。
- 證據：`prep/dev-bp/`（`timing.txt`、`<tree>-<layer>-1000ms/`、`*.ingress.json`、`*.stdout.txt`）。

### 8. 判定（session 完整、J1～J7 全部成立、而且沒有 D44 事後的停止旗標，才通過；J5b 只記錄）

- **判定對象**：
  - 判定只看修好之後的兩棵樹（flow、flow+09a，以下稱「修好的樹」）。
  - before 樹只用來對照與記錄。它的 ingress 計數輸出 null，不是 0：08c 之前沒有這些行。
- **判定的程式**：`prep/judge_l2.py`。
  - 每個跑次結束時，driver 呼叫 `evaluate()`；session 結束後，`judge_l2.py --session` 彙總成 `judgement.json`。
  - 守恆本身由凍結的 `ingress_evidence.py` 判定，`judge_l2.py` 只讀它的輸出。
  - 呼叫時不加 `--injection`（理由見 J5a）。
- **session 完整**（依 xhigh 檢查 C 新增）：以下都要成立，否則不判通過：
  - `summary.json` 的 `completed` 是 true；
  - 沒有中斷、沒有停止；
  - completed 的次數等於 `plan.json` 排程的次數（114）；
  - 名稱逐一相符。
- **J1：權威 digest 不變**（session 開始前由 driver 執行）。
  - **base runner**：在 before worktree（`c2b76cd`）建的 `gyo_object_fps_pvp_authority_digest`。
    - 第 1 版指定的 `l1/digest-base-runner`（`5c370540…`）內嵌已移除的 worktree 的 fixture 路徑，現在載不到 arena，無法使用（本機實測 exit 1）。
  - **branch runner**：在 H 的主 checkout 建。
    - `git diff --name-only eabb107 H` 在 `docs/` 以外只有 `run_client_delay.py` 與它的測試，所以主 checkout 的 C++ 等於 flow。
    - flow+09a 的差分不進 Match（第 2 節）。
  - driver 先核對兩個 runner 的身分：
    - 路徑固定；
    - sha256 必須等於 `--base-runner-sha`、`--branch-runner-sha`；
    - branch runner 的修改時間不得早於 H 的 commit 時間。
  - 再以 `compare_authority_trees.py` 跑 scale 1 與 3，寫到 `<output>/digest/s1`、`s3`。兩次都要：exit 0、`all_same` 為 true、`scale` 欄位等於宣告的值、情境數 35。
  - 不成立就是停止條件：不開始跑次。
  - 本機以既有的 runner 試過這段程式（base 用 `l2p-analyzer/base-runner`，只為測程式）：scale 1、3 都 35 情境 `all_same`，耗時 1.9 秒。
- **J2（I1）**：
  - 修好的樹上，每個 I1 條件成立的跑次都要精確相等：Gateway 的 `fwd_link_written_*_commands` 總和＝Match 的 `received_commands` 總和（含 0 桶）。
  - 條件（`ingress_evidence.py:561-598`）：`final=1` 之前有 `ipc closed reason=eof`；整個跑次只有一次 IPC 連線；沒有經過 IpcPause。
  - 條件不成立的跑次記成未判定，**未判定的比例照記**（D58 (7)）。
  - 另外，每個（修好的樹、案例）至少要有 1 次條件成立的跑次（提議；xhigh 同意，L1 19／19、開發 12／12 都做得到）。
  - 本 L2 不用 host-ipc，所以不會出現 IpcPause：`action_probe.py:531-533` 與 `backpressure_probe.py:342-343` 都只在 host-ipc 時建 IpcPause。
  - 非 EOF 的關閉只會讓 I1 未判定，不影響 status（`ingress_evidence.py:782-784`）。
- **J3（IG、I0、I2，加上全域、T3、S1、S2、trace 對帳；I3）**：修好的樹上，每個跑次 `ingress_evidence.py` 的 status 都要是 passed，**而且**：
  - `link_shots`＝passed，而且帶 D56／D59 鍵的玩家行數等於玩家行數（carried＝lines >0）。
    - D59 (11)：這些鍵在分析器裡維持可選，由本宣告規定修好的樹每一行都必須帶齊（`ingress_evidence.py:514-534`）。
  - `action_drops` 的 Gateway 與 Match 兩側 `complete`＝true（D56 的鍵）。
  - **I3 不成立也讓 status＝failed**（`ingress_evidence.py:778-781`）。依 xhigh 檢查，這裡明寫：**I3 失敗就停**，雖然 I3 的定義（拒絕線用寫出數）仍是 D58 (8) 的暫定。
  - status＝undetermined（缺 `final=1`、缺視窗行、缺 `trace_end`）不當成通過，也不當成 0（08c「log 行族」最後一點）；在修好的樹上是停止條件（第 11 節）。
- **J4**：
  - 修好的樹的 clean-60：Gateway 的 `drop_rejected_lane_overflow_commands`＝0、`drop_resolved_trim_commands`＝0。
  - 其他案例只記錄。
- **J5a（建議，待使用者確認；依 xhigh 檢查 A 補強）**：修好的樹上，x1 與 ramp 的每個跑次要同時滿足以下各項：
  1. `ingress_evidence.py` 的 J5a 等式成立：`substituted`＝`late_first`＋7 種 `unarrived_*`。
  2. never_arrived＝`unarrived_overflow`＋`unarrived_end`＝0。**end 只在 I1 的 `eof_before_final` 成立時才算進去**：非 EOF 的關機競態下，final 行之前的開啟紀錄會以 end 關閉，會變成誤導的停止。
     - 所以 `ingress_evidence.py` 不加 `--injection`：加了的話，它會把 end 一律算進 never_arrived。
     - 改由 `judge_l2.py` 的 `j5a_injection()` 判定；被排除的 end 數照記（`end_excluded`）。
  3. **(a)**：`match-ingress.jsonl` 中「未到達、關閉原因屬於 {removed, reset, overflow, aged}（加上 `eof_before_final` 成立時的 end）、而且 Client trace（`clients-commands.jsonl`）有 `sent` 紀錄（player、epoch、life、sequence 相同）」的被替代紀錄數＝0。
     - `trace_end` 的 `dropped` >0 時，紀錄可能不完整，判為不成立。
  4. **(b)**：該跑次的 `late_first` 合計 ≥1，作為判別力的下限。
  - 前提「relay 的 forwarded＝received」由 runner 的有效性判定保證（第 6 節）。
  - **新增的精確核對**（只在注入跑次）：relay 的上行 forwarded（`relay.json` 的 `client_delay.counts_after.upstream.forwarded`）＝Gateway 全域行的 `received_datagrams` 總和。開發 4 次：2063、2056、2069、2069，完全相等。
  - **為什麼要補強**（xhigh 檢查 A）：在本 L2 的設定下，never_arrived 結構上恆為 0。
    - 注入在 7.5～9.7 s，probe 跑 16 s，到不了 600 Tick 的保留期，不會變成 aged。
    - 帳本容量 1024 大於保留期＋1，不會 overflow（`MatchIngressLedger.hpp:14-15`）。
    - end 只在 final 先於 Leave／EOF 時出現（`MatchRuntimeHost.cpp:526`、`:713`）。
    - 所以被替代的命令就算始終沒送到 Match，紀錄也會以 removed 或 reset 關閉，原本的 J5a 照樣通過。
  - **D58 (2) 理由的更正**：第 1 版寫「removed 是跑次結尾 Leave 關掉的紀錄」，不精確。
    - L1 的 `l1/accept/bp-1/downstream-250ms/` 那 8 筆 removed，是故障窗內 +0.20～0.25 s 被替代的序號。
    - Client 從來沒有產生過這些序號：`clients-commands.jsonl` 缺 309～313。
    - 它們只是到跑次結尾才由 Leave 關閉。
    - 所以是良性的；(a) 依「Client 有沒有送出」區分，這類紀錄不會觸發。本草稿以 `judge_l2.py --one` 對它試過：removed／unsent 8，(a)＝0。
  - **開發跑次**（修好的樹 4 次）：
    - 未到達的紀錄全部是 epoch 關閉、而且都送出過：x1 18／16、ramp 4／5。它們不屬於 (a) 的範圍，(a)＝0。
    - (b) 的 late_first：x1 140／142、ramp 108／109。
    - before 樹在結構上是 0（Gateway 會吞掉）。
- **J5b（只記錄，D58 (6)）**：Client 的 late 修正次數，取 action probe `action-frames.jsonl` 中 `phase_late_corrections_{a,b}` 的最大值，依樹與案例記錄。
- **J6**：
  - `frozen-l2.sha256` 的 44 檔在 session 開頭與結尾都吻合，清單本身等於 `d2a8699b…`；
  - 修好的樹每個跑次 `ingress_evidence.py` 都能解析（status 不是 stopped 或 missing）。
- **J7（驗證項）**：downstream-1000ms，修好的樹的最大值和 before 的最大值比較，差 ≤1 Tick（16 666.7 µs）。
  - 各樹排程的 4 次都必須有值，缺一次 J7 就不成立（依 xhigh 檢查 D）。
  - 比較的兩個指標：
    - (a) match.log `network statistics` 的 `tick_late_p99_le_us`：Match 的 Tick 計時器**到期喚醒的延遲**（被通知的喚醒不計）。它是第 99 百分位落在的**分箱上界**；分箱是 250…64000 µs，最後一格取 max（`LateWakeStats.hpp:19-21`、`.cpp:31-44`）。每個 backpressure 跑次只有 1 個 10 秒視窗（final 視窗不寫 network 行）。
    - (b) `backpressure.json` 的 `maximum_authority_gap_seconds`：**只量故障窗 [start, release) 之內**相鄰權威 Tick 的最大間隔（`backpressure_probe.py:211-224`）。
  - 限制：
    - 因為是分箱上界，before ≤16 ms 那一格時，修好的樹只要也在 ≤16 ms 那一格就通過；在 32／64 ms 的邊界上，真實差 0.2 ms 也可能判失敗。
    - 實測約 150 µs，對 16.7 ms 的門檻只抓得到粗大的退化。
    - 用最大值彙總，故障窗內的 1 次主機雜訊（>17 ms）就可能讓 (b) 失敗。
  - `cpu_s`、`ipc_iterations` 的中位數只比方向、只記錄（預測修好的樹 ≤ before）。
- **注入的有效性**（D60）：見第 6 節的無效門檻。它不是判定，是停止條件。

#### 判定門檻與來源（D45 的核對對象；本版依 xhigh 檢查修正）

| 判定 | 門檻 | 來源（程式推導附 file:line；實測附證據路徑） | 指標／取自哪個建置 | 混入的流量 |
|---|---|---|---|---|
| session | 114 次全部 completed，沒有停止或中斷 | 宣告（xhigh 檢查 C） | `plan.json` 的 `schedule`、`summary.json`、`progress.jsonl` | — |
| J1 | scale 1、3 各 35 情境 `all_same`，exit 0，scale 相符；runner 身分相符 | 程式推導：digest 情境直接驅動 `PvpMatch`／`MatchRuntimeHost`，不經 Gateway（08c「權威 digest 與凍結分析器」）。既往 0 差分：`l1/digest-s1`、`s3`、`l2p-analyzer/digest-s1`、`s3` | base：before worktree（`c2b76cd`）建；branch：H 的主 checkout 建（C++ 等於 `eabb107`） | 排除 `movementSlack*`（`AuthorityDigest.cpp:46`），所以不證明帳本掛鉤沒有改 slack（由突變守住，不屬 L2） |
| J2 | I1 精確（條件成立時）；未判定照記；每個（樹、案例）至少 1 次條件成立 | 程式推導：TCP 可靠而且有序；寫出數在 `l.conn.Write` 成功之後、`l.mu` 下計（08c I1、第 3 批 (a)）；條件 `ingress_evidence.py:561-598`；IpcPause 只在 host-ipc：`action_probe.py:531-533`、`backpressure_probe.py:342-343` | 修好的樹 gateway.log 的 `fwd_link_written_*_commands`、match.log 的 `received_commands`（含 player=0） | 被踢出的玩家只在 link 總和比；Gateway 換接第二個 Match 的跑次不在本 L2 |
| J3 | IG、I0、I2、全域、T3、S1、S2、trace 精確；`link_shots` 精確且 carried＝lines；`action_drops` 完整；**I3 失敗即停** | 程式推導（08c「不變量與守恆」；`ingress_evidence.py:426-450`、`:451-497`、`:514-534`、`:778-784`）；D59 (11)；I3 是 D58 (8) 的暫定定義 | 修好的樹的 gateway.log、match.log、match-ingress.jsonl | IG 只到資料包層；link 內的動作由 `link_shots` 與 `action_drops` 補（D56） |
| J4 | clean-60 的溢出＝0、截短＝0 | 程式推導：溢出要寫出端停住，而且同一玩家拒絕線待寫的窗口 >4（`runtime_link.go:182-183`、`:363-369`）；截短要寫出端停住約 0.5 s，讓主線已解析的部分 >32（`:183`、`:323-327`）；clean 沒有這種故障。第 3 批推翻了「只在寫出端阻塞時」：relay 一次放出大量包也會溢出，所以非 clean 只記錄。實測：L1 clean-60 為 0；`prep/mock-session/progress.jsonl` 中修好的樹 clean-60 的 `sums`，8 個路由鍵（`fwd_routed_*_inputs`）與兩個 J4 鍵都是 0 | 修好的樹的 Gateway 玩家行 | — |
| J5a | 等式；never_arrived（end 只在 EOF 條件下）＝0；(a)＝0；(b) late_first ≥1；relay forwarded＝Gateway received（建議，待確認） | 程式推導：帳本每個被替代的序號一筆，關閉原因 7 種（`MatchIngressLedger.hpp`）；end 的條件見 `MatchRuntimeHost.cpp:526`、`:713`；(a) 用 Client trace 的 `sent`；removed 的良性例證：`l1/accept/bp-1/downstream-250ms/`（Client 缺號 309～313）；(b) 開發 108～142 | 修好的樹的 match.log、match-ingress.jsonl、clients-commands.jsonl、relay.json、gateway.log 全域行；只看 x1、ramp | 注入只延遲不丟（runner 的 `round_problems` 保證）；epoch／life 關閉的紀錄不在 (a) 之內（之後以舊身分到達，由拒絕原因計數） |
| J5b | 只記錄 | Client 規則：`LocalPlayerPrediction.cpp:128`、`:134-135`；`MovementPhaseLateSamples`＝2（`Movement.hpp:58`）；逐次發布的資料不存在（D58 (6)） | action probe 的 `action-frames.jsonl` | — |
| J6 | 44 檔 sha256；清單 `d2a8699b…` | D60；來源清單 `l1/analyzers-from-09s1.sha256`（40／40 OK，`l1/analyzers-check-09s1.txt`）、`l2-frozen-candidates.sha256` | 主 checkout | `run_network.py` 不在清單內，只受 git 狀態核對保護 |
| J7 | 各樹 4／4 有值；最大值差 ≤16 666.7 µs | 程式常數：`AuthorityTickRate`＝60（`Movement.hpp:15`）。分箱：`LateWakeStats.hpp:19-21`、`.cpp:31-44`。間隔只量故障窗：`backpressure_probe.py:211-224` | downstream-1000ms 各 4 次的最大值；同一 session | `tick_late` 只含到期喚醒的延遲；間隔是故障窗內的權威 Tick（開發三樹 16.73～16.79 ms） |
| 注入無效 | 速率拒絕 >0、佇列 ≥192、送出延遲 p99 >10 ms、收發不等 | 工具門檻（L2P-injector 訂、D60 接受）：`run_client_delay.py:77-80`、`:151`、`:429-470`；速率上限 `session.go:122`；`rate_limited_packets` 是 Close 時的累計（`server.go:183`） | relay.json（run_case 寫出）、Gateway 計數 | relay 的收發計數兩個方向分開 |

### 9. 預測（可被推翻；全部是推論；只記錄，不是判定，也不是停止條件）

依 L2P-trees、第 7 節的開發跑次與 xhigh 檢查看過。改寫處列出 08c 文件的原文與理由。

| # | 情境 | 修之前 | 修之後的預測 | 推翻條件 |
|---|---|---|---|---|
| P1（改寫） | clean-60 | late_* 看不到 | 副本類（`pending_copy`＋`late_copy`＋`resolved_copy`）＝0；Gateway 的 `fwd_written_duplicate_commands`／`fwd_link_written_main_commands` 約 2（開發 1.99～2.00）；**late_first 可能少量出現**：修好的樹 8 次中，late_first>0 的跑次 ≤2，而且這些跑次每位玩家 late_first ≤4 | 副本類 >0；或 late_first>0 的跑次 ≥3／8；或某跑次某位玩家 late_first ≥5（用 `progress.jsonl` 依玩家的 `sums` 判定） |
| P2 | x1 | 被替代的命令全部被 Gateway 吞掉，0 次 late 修正 | 見 J5a、J5b（開發：late_first 每位 70～71；probe 的 late 修正每位 2～3；before 0） | — |
| P3（改寫） | ramp | 沒有負樣本、沒有 late 修正。延遲期間 Match 的佇列變空，以 starvation 重設**重新對齊**（starvation 的條件包含佇列為空，`PvpMatch.cpp:452-453`；是重新對齊，不是清除堵塞） | 出現 late 修正：修好的樹每個 ramp 跑次，兩位 Client 的 late 修正合計 ≥1（幅度被 ±2 Tick 夾住，`LocalPlayerPrediction.cpp:98`）。starvation 重設的次數和 before 相同，時刻（相對 `fault.start_ns`）和 before 相差在 ±1 Tick（16.7 ms）以內（late 修正最多搬 2 Tick＝33 ms；延遲到 250 ms 時佇列照樣變空） | 修好的樹有 ramp 跑次的 late 修正合計為 0；或 starvation 次數的樹平均和 before 差 ≥1／跑次；或同一順位的 starvation 時刻，樹中位數和 before 差 >1 Tick |
| P4 | network0／20／40 | late 修正次數為 before 的值 | late 修正次數上升（總和 after > before，只比方向）；late_first 每跑次 0～50 個（去重後 late_copy≈0） | 總和 after ≤ before |
| P5（加限定） | network*（★） | 舊序號的 Input 被丟 | **只對 flow**：`fwd_stale_sequence_input_packets` 每跑次（兩位玩家合計）的中位數約 18／32／32（network0／20／40；第 2 批 ③ 的重播，`batch2/design3-counts-pv7.md`）。flow+09a 預期偏低，只記錄（開發 network20：flow 25、flow+09a 15）（更正，2026-10-11，09R-eval：約 18／32／32 借自其他建置的分布；v7 自己約 13／16.5／24.5，而且隨送出相位變動。見「L2 的結果」的 P5。） | flow 差兩倍以上 → 先核對 relay 的 forwarded 與 Gateway 的 IG 資料包數，再判斷是重播模型還是前提出錯 |
| P6（改寫） | downstream-1000ms | 已解析的副本不轉送 | Match 的 late_first＝0、副本類＝0（副本被已寫出去重吸收）；Gateway 的 `fwd_written_duplicate_commands` 只記錄 | late_first >0，或副本類 >0 |
| P7 | downstream-respawn-1000ms | 舊 life 的副本在 `server.go:413-415` 被丟 | Match 記 `life_old`／`epoch_old` 約 147 個命令／跑次（`arrival_identity.tsv`） | 為 0 |
| P8（改寫） | gateway-1000ms（SIGSTOP） | 只有全域的 rate_limited 總數 | 依玩家的總和等於全域（T3，屬 J3）；速率拒絕 0～4 包／跑次 | T3 不相等；或任一跑次 >8 包 |
| P9 | 所有跑次 | — | IG、I0、I2 精確成立；I1 在條件成立時精確成立（＝J2、J3） | 任何不相等 |
| P10 | D44 | 偶發的 epoch 重設 | 暴露上升（late 修正變多），只記錄；規則見第 10、11 節（更正，2026-10-11，09R-eval：只在持續延遲（x1、ramp）時成立；network 型（遺失加 ≤30 ms 抖動）不成立，L2 的 late 修正 0，機制見「L2 的結果」的 P4。） | 第 11 節的 D44 停止條件 |
| P11 | network*（★） | — | 替代的千分比 after ≤ before（`match-commands.jsonl` 的 resolved 中 source≠actual 的比例；三棵樹都有 trace） | after > before |

- **P1**
  - **原文**：「clean-* ｜ late_* 為 0 ｜ 相同；Match 的副本類＝0（I3），Gateway 的 `fwd_written_duplicate_commands` 約為每個命令 2 份 ｜ after 的 clean 出現 late_first > 0，或副本類 > 0」。
  - **理由**（D58 (9)）：F3 在開發機跑的 clean timing probe 約 22 次中，有 2 次 late_first>0（`f3/report-evidence-from-agent.md:125`）：
    - 晚 2～25 ms；
    - 一次是開局的序號 37～38，一次是兩位玩家同時在 407～408；
    - 推論 clean 環境本來就有少量遲到，修好之後才看得到。
  - **p 的不確定性**（依 xhigh 檢查補）：
    - 點估計 p＝2／22≈9％，此時 8 次中 ≥3 次的機率約 3％。
    - 但 p 的 95％ 信賴區間約 1～29％；p 取 0.29 時，P(≥3／8) 約 0.42。所以「3％」只是點估計下的值，這條預測容易被推翻。推翻不是停止條件。
    - F3 的每次最多 2 個連續序號，所以「每位 ≤4」留了一倍的餘裕。
  - 其他 clean 證據：L1 clean-60 0／1；L2P-trees clean-60 0／2（修好的樹）。
  - F3 用的是 timing probe（`ingress_cross`），本 L2 的 clean-60 是 action probe。
- **P3**
  - **原文**：「沒有負樣本、沒有 late 修正，靠 starvation 重設恢復 ｜ 出現 late 修正（幅度被 ±2 Tick 夾住）；starvation 重設比 before 少或延後 ｜ after 仍 0 次 late 修正，而 host 端有**連續**兩次發布都帶負樣本（中間沒有非負樣本），Client 收到且序號 > `settledAfter_`」。
  - **理由**：
    1. 死信評估 B1 的 A4（`pvp-v7-dlq-eval-20261011/b-report-final.md:35`、`:215`）：starvation 判定時佇列是空的，是重新對齊，不是清除堵塞。
    2. L2P-trees 的 ramp 在三棵樹都在故障開始後 +0.64 s、+1.66 s 各有兩位玩家的 starvation，+2.68 s 一次 backlog，時刻相同。「少或延後」已有反方向的跡象，所以改成可被推翻的「相同」，並依 xhigh 檢查補上 ±1 Tick 的時刻容差。
    3. 原本的推翻條件需要逐次發布的資料，而這種資料不存在（D58 (6)），所以改用 probe 的 late 修正次數。
- **P5**：原表的「故障 4 秒內 Input 包約 441」是兩位玩家合計，所以中位數是每跑次兩位合計。
  - 重播模型建在 09a 之前的 Client 送包節奏上（xhigh 檢查）。flow+09a 改了送包節奏，開發的 15 已經低於 32／2＝16，所以 P5 只適用 flow。
- **P6**
  - **原文**：「已解析的副本不轉送 ｜ Gateway 去重的副本約 1118～1284／跑次；Match 的 late_first＝0 ｜ late_first > 0」。
  - **理由**：
    - 1118～1284 是修之前、matrix runner（action probe）中「已解析後才到的副本」（`facts-gateway/report-from-workflow.md`）。
    - D60 改用 backpressure runner（timing probe）。
    - 而且修之後這些副本被已寫出去重計進 `fwd_written_duplicate_commands`，和穩態每個命令約 2 份的副本混在一起，不能直接比。
  - 開發跑次：修好的樹 late_first 0、副本類 0。
- **P8**
  - **原文**：「依玩家總和等於全域（T3）；47～60 包／跑次 ｜ 不相等」。
  - **理由**：
    - 47～60 是 matrix runner 的 gateway-1000ms（`batch2/design3-counts-pv7.md` C：matrix 47～60、backpressure 1～4）；D60 改用 backpressure runner。
    - 實測：L1 的 bp-1、bp-2 各 2（L2P-analyzer 以新分析器重判，`l2p-analyzer/report-from-agent.md:120`）；本草稿的開發跑次 flow 0、flow+09a 2。
    - 「>8」是 4 的兩倍，沒有程式來源。
- **P4 的註**（不改）：開發跑次 network20 三棵樹的 probe late 修正都是 0，late_first 每位 0／3（L2P-trees）。只有 1 次樣本，P4 有被推翻的可能。推翻不是停止條件。
- **P7 的註**（不改）：
  - 「147 份」的單位是命令（每個命令一份）；這個案例沒有開發跑次。
  - 開發跑次的 x1 每位 `epoch_old` 204～228、downstream-1000ms 228，表示舊 epoch 的窗口照常走拒絕線，Match 依原因計數。

### 10. 記錄項（只記錄）

1. **Gateway 拿不到內容的丟棄（D58）**：
   - 每個跑次取 `ingress_evidence.py` 的 `gateway_contentless_drops`：速率限制、解碼失敗、未授權、舊序號的 Actions／其他、未知種類、非 active；依原因與種類、依玩家，另加 Gateway 全域的資料包丟棄。
   - 依樹與案例彙總（`judgement.json` 的 `D58_contentless_drops`）。用途：以後要不要提前 pv7（`GatewayDropReport`）。
   - before 樹沒有這些鍵（08c 之前沒有計數），輸出 null。
   - 開發跑次：network20 舊序號的 Actions 9、11；gateway-1000ms 速率限制 0、2；其他 0。
2. **D44（依樹記錄偶發 epoch 重設的頻率）**：
   - 每個跑次取 `match-commands.jsonl` 的 `reset` 事件，原因有 backlog、starvation、sequence_exhausted、none、life_respawn。
   - **單位是每位玩家的一個事件**：兩位玩家同時的 starvation 算 2 個。
   - 依「故障窗 (fault.start_ns, fault.release_ns＋1.5 s] 之內／之外」分開，依樹與案例彙總（`D44`）。
   - before 樹也記：新舊分開累計（D44）。
3. **U4（閒置主機的替代）**：在閒置閘門之下，記 clean-60 的替代數。
   - 修好的樹：Match 的 `substituted_commands`、`late_first_commands`（before 樹是 null）；
   - 三棵樹：`match-commands.jsonl` 的 resolved 中 source≠actual 的數（`U4_and_P_inputs`）。
   - 回答事故紀錄 U4：「閒置主機 0 筆」能不能保留。
4. **P1～P11 的輸入**：
   - Gateway 與 Match 的 ingress 計數總和（每個跑次的 `sums`，含 Gateway 全域行）；
   - probe 的 late 修正與權威拒絕；
   - `network statistics`；
   - `backpressure.json` 的恢復時間與 30 Tick 合計的最大值。
5. **工具自己的判定**：run_case（`result.json`）與 backpressure（`backpressure.json`，或 `failure.json`）的 passed 與錯誤。三棵樹都只記錄（D60）。
6. **主機**：sleeper、前 5 名 CPU 程序、`timer_baseline`、每個跑次的 `group_left_over`。

### 11. 停止條件（立即停下，保留全部證據，不重跑，交使用者決定）

session 開始前（driver 拒絕執行，什麼都不寫）：

1. 正式執行缺 `--idle-gate`、`--main-head`、`--judge-sha`、`--driver-sha`、`--base-runner-sha`、`--branch-runner-sha` 任一項。
2. 凍結清單 44 檔或清單本身不符；`judge_l2.py`、`run_l2.py`、`sleeper.py` 和宣告的 sha256 不符；`ingress_evidence.py` 不等於 `95ec6542…`。
3. 主 checkout：HEAD 不是 H、不乾淨，或在 `docs/` 以外和 `b57291d` 不同。worktree：HEAD 不符或不乾淨。任一執行檔或 arena 的 sha256 不符。
4. J1 的 runner 身分不符：不存在、sha256 不符，或 branch runner 比 H 的 commit 時間舊。

開始後、第 1 個跑次之前：

5. **J1**：任一 scale 的比較不是 exit 0、`all_same`、scale 相符、35 情境。driver 會寫出 `plan.json` 與 `digest/`，其餘跑次記成 not_run。
6. 閒置閘門 30 分鐘內不成立。

session 中（其餘跑次記成 not_run）：

7. runner 錯誤：
   - 案例目錄缺 `gateway.log`、`match.log` 或 `match-commands.jsonl`；
   - gameplay 或 Client 延遲的 `result.json` 沒有 `artifacts`（run_case 拋出例外；比照 `run_client_delay.py:572-574`）；
   - backpressure 有 `failure.json`（analyze 拋出例外），或沒有 `backpressure.json`。這類錯誤記成 runner_error，不會誤標成 D44；
   - 逾時（300 秒）；例外。
8. **注入無效**（D60）：`run_client_delay.py` 的 `summary.json` 不是 completed（第 6 節的無效門檻）。
9. 每個跑次之前的核對：主 checkout、worktree、執行檔、arena、凍結檔、`judge_l2.py`、`run_l2.py`、`sleeper.py`、兩個 digest runner，任一有改變。
10. **修好的樹**（before 樹只記錄）：
    - `ingress_evidence.py` 的 status 不是 passed：
      - failed：J2／J3 不相等、J5a 等式、**I3**、`link_shots`、trace；
      - undetermined：缺 `final=1`、缺視窗行、缺 `trace_end`；
      - stopped：行格式違反契約。
    - `link_shots` 不是 passed，或 carried≠lines；`action_drops` 不完整。
    - **J5a**（x1、ramp）：never_arrived >0（end 只在 EOF 條件下計）；(a) >0 或無法判定；(b) late_first＝0。另外 relay 的上行 forwarded≠Gateway 全域的 `received_datagrams`。
    - **D44（即時；從 08b 放寬）**：非 respawn 的重設（backlog、starvation、sequence_exhausted、none、unrecorded）出現在故障窗 (start, release＋1.5 s] 之外。clean-60 沒有故障窗，出現就停。
      - **放寬之處**：08b 的宣告（`08b-event-driven-results.md:263`）還有「故障案例出現 backlog 就停」。三棵樹的 x1、ramp 都在 +2.56～2.68 s 有 1 次 backlog（L2P-trees），所以本 L2 拿掉這條。故障窗內的 backlog 只記錄，由事後規則比較三棵樹。
      - **引用更正**：`command_evidence.py:413-422` 用的是干擾窗（含 runtime gap、transport），不是故障窗。本 L2 的故障窗規則比它嚴，取自 08b 的 L2 宣告，不是取自 `command_evidence.py`。
    - clean-60 出現權威 Cooldown 拒絕（`actions.jsonl` 的 decision `rejection`＝3，`Combat.hpp:52`）。
11. sleeper 失敗；SIGINT／SIGTERM（跑完目前的跑次後停；子程序在自己的 session，不會被 Ctrl-C 直接打斷）。

session 之後（`judge_l2.py --session`；成立時 `passed`＝false，停下回報）：

12. **D44（事後）**，單位是每位玩家的重設事件：
    - **停止旗標**：
      - (i) 同一案例，before 沒有出現的非 respawn 原因，在某棵修好的樹的 **≥2 個跑次**出現；
      - (ii) 同一案例，修好的樹每跑次的平均事件數比 before 多 ≥1。開發跑次三棵樹逐案相同：x1、ramp 各 5 個（starvation 4＋backlog 1），backpressure 2 個，其他 0。
    - **檢視旗標**：before 沒有的原因只在 1 個跑次出現時，記成 `review_flags_for_the_user`，不影響 `passed`，交使用者判斷。
      - 依 xhigh 檢查：歷史上 network20 有 3／15 次 backlog（+4.27～4.48 s，在寬限之內）；若三棵樹機率相同，只看 1 次就觸發的誤報率約 24％。
13. session 不完整（第 8 節）。
14. 分析時發現需要改 wire（proto 欄位）。

### 12. 證據目錄、腳本與執行步驟

- **證據目錄**：`build/target/_build/test/logs/pvp-v7-08c-l2-<日期>/`
  - `declaration.md`（定稿）與 `declaration.sha256`；`commands.txt`；
  - `session/`（`run_l2.py` 的輸出）：
    - `plan.json`：排程、每個跑次的指令、開頭的狀態與雜湊、digest 閘門與 runner 身分；
    - `digest/s1/`、`digest/s3/`（J1）；
    - `idle_gate.jsonl`、`host.jsonl`；
    - `progress.jsonl`：每個跑次一行，含判定紀錄；
    - `runs/<案例>-r<n>-<樹>/`：工具自己的輸出；`runs/<名稱>.stdout.txt`；`runs/<名稱>.judge/ingress.json`；
    - `summary.json`：結尾的狀態與凍結核對；
    - `judgement.json`：`judge_l2.py --session` 的輸出。
- **腳本**（`prep/`，核准時凍結）：
  - `run_l2.py`：session 驅動，照 D48 第③步的 `session3.py`。
    - `--list-only`：只列 114 個跑次，以及各案例開頭的樹；
    - `--dry-run`：只做核對、列出指令，不跑也不寫；結束碼反映核對與 J1 runner 身分的問題；
    - 正式執行的參數見第 11 節 #1。
  - `judge_l2.py`：判定。`evaluate()` 每個跑次呼叫；`--session` 彙總；`--one` 判單一案例目錄（開發用）。
  - `sleeper.py`、`frozen-l2.sha256`。
  - 開發用、不凍結：`dev_bp.sh`、`dev_extract.py`、`mock_session.py`。
- **腳本的驗證**（本版重跑）：
  - `mock-session/`：18 個開發跑次，排程 18、summary 完整。session、J1～J7 全部成立，`passed`＝true。
  - `mock-session-incomplete/`（負向）：同樣的跑次，但 summary 是中斷、排程多 1 項。session＝false，所以 `passed`＝false，exit 1。
  - D44 旗標（負向，scratchpad 的副本）：x1-flow 加 2 個 `sequence_exhausted`、ramp-flow09a 加 1 個 `none`。結果是 2 個停止旗標（每跑次 +2、+1）與 2 個檢視旗標（新原因只在 1 個跑次出現），`passed`＝false。
  - `--one`：flow x1 的 (a)＝0、(b)＝140、relay＝Gateway 2063；L1 的 `bp-1/downstream-250ms` 的 removed／unsent 8，(a)＝0（舊 log 缺 D59 鍵，所以那份另有 d59 停止，屬預期）。
  - 第 1 版的負向檢查（加一筆 starvation→D44 停；拿掉一行的 D59 鍵→D59 停）沿用。
  - 子程序處理（scratchpad 試驗）：逾時後先 SIGTERM，GRACE 後殺整個 group，沒有留下程序。
- **步驟**（核准後）：
  1. 宣告提交為 H（只改 `docs/`）。
  2. 建 J1 的兩個 runner（`commands.txt`）：
     - before worktree：`cmake --build --preset test --target gyo_object_fps_pvp_authority_digest`；
     - 主 checkout：同一個 target。
     - 建完記下兩者的 sha256，並確認三棵樹的執行檔雜湊不變（`--dry-run` 會核對）。
  3. 暫停 PR #73 的 auto-fix 與其他工作。
  4. 執行：`caffeinate -dims python3 -I prep/run_l2.py --output <E>/session --idle-gate --main-head <H> --judge-sha <sha> --driver-sha <sha> --base-runner-sha <sha> --branch-runner-sha <sha>`。
  5. `python3 -I prep/judge_l2.py --session <E>/session`；結果寫進 08c 文件與 HANDOFF。

### 13. 和 08c 文件原大綱不同的地方

| # | 原文（08c「L2 重新量測」） | 本草稿 | 理由 |
|---|---|---|---|
| 1 | 「案例（約 114 次、約 1 小時；推論）」 | 每樹 38 次的配置照第 2 批計畫 §10；約 43～50 分（不含建 digest runner） | 08c 文件沒寫各案次數；時間依實測（每次 21 秒，backpressure 13 秒） |
| 2 | 沒寫 downstream-1000ms、gateway-1000ms 的 runner | 用 backpressure runner（timing probe） | D60 |
| 3 | 「斜坡型是新參數……這次只記錄」 | ramp 的下游（late 修正、starvation）只記錄，但 **J5a 對 ramp 也判定** | J5a 判的是資訊流；xhigh 同意，但前提是補強（已依 A 補強） |
| 4 | 「注入前後核對 received＝forwarded」 | 加上 D60 的無效門檻，無效即停；另加 relay 的 forwarded＝Gateway 的 `received_datagrams` | D60；xhigh 檢查建議 |
| 5 | J1「CTest 的兩棵樹比較」 | driver 在第 1 輪之前自己跑比較；base 用 before worktree 建的 runner，branch 用 H 建的 runner，並核對身分 | 讓 J1 有明確的時點與檔案；L1 的 base runner 已不能用（內嵌已移除的路徑）；xhigh 檢查 J1 |
| 6 | J2「條件不成立記成未判定」 | 加：未判定的比例照記；每個（修好的樹、案例）至少 1 次條件成立 | D58 (7)；後者是提議，xhigh 同意 |
| 7 | J3「IG、I0、I2 精確相等」 | 加：`link_shots` 精確且 carried＝lines、`action_drops` 完整；I3 失敗就停（明寫）；status undetermined 在修好的樹上停止 | D59 (11)、D56、D58 (8)；08c「缺 final 判成未判定或停止，絕不當成 0」；xhigh 檢查 J3 |
| 8 | J5a「late_first＋關閉數＋aged＝substituted，而且 never_arrived＝0」 | never_arrived 不含 removed（D58 (2)）；end 只在 EOF 條件下計；加 (a)、(b)；不加 `--injection` 呼叫分析器 | D58 (2)；xhigh 檢查 A（原門檻結構上恆為 0） |
| 9 | J5b 是判定 | 只記錄 | D58 (6) |
| 10 | J6「`analyzers.sha256`」 | `frozen-l2.sha256`（44 檔），開頭、每個跑次之前、結尾都核對 | D60 |
| 11 | J7「Tick 間隔最大值」 | 取 `backpressure.json` 的 `maximum_authority_gap_seconds`（只量故障窗）；各樹 4 次的最大值，4／4 都要有值 | match.log 沒有 Tick 間隔；原文沒定彙總方式；xhigh 檢查 D、F |
| 12 | 停止條件「after 出現 before 沒有的 epoch 重設種類，或比例明顯上升（D44）」 | 即時：故障窗外的非 respawn 重設（從 08b 放寬，拿掉故障案例的 backlog 規則）；事後：新原因在 ≥2 個跑次出現，或每跑次多 ≥1 個事件（每位玩家的事件）；新原因只在 1 個跑次出現的交使用者判斷 | 「明顯上升」沒有數字；開發跑次三棵樹逐案相同；xhigh 檢查 E |
| 13 | 停止條件「乾淨跑次出現權威 Cooldown 拒絕」 | 只對修好的樹停；before 只記錄 | 和 08b 一樣只對 after 停，新舊分開累計（D44） |
| 14 | 沒寫 run_case 的遊戲判定 | 三棵樹都只記錄；backpressure 的判定也只記錄；但工具拋出例外算 runner 錯誤 | D60；xhigh 檢查 D |
| 15 | 預測 P1、P3、P6、P8 | 改寫（原文與理由見第 9 節）；P5 只適用 flow；P4、P7 加註 | D58 (9)、死信評估、D60 的 runner 變更、開發跑次、xhigh 檢查 |
| 16 | 「U4 併入這次」 | U4 定成記錄項 3 | 事故紀錄 U4 |
| 17 | 沒寫 | D58 的記錄項（拿不到內容的丟棄） | D58 |
| 18 | 沒寫判定的彙總條件 | 要 session 完整（114 次）而且沒有 D44 停止旗標，才判通過 | xhigh 檢查 B、C |

### 14. 需要使用者決定的地方（附建議；2026-10-11 全部照建議核准，D61）

1. **J5a 的補強**（(a)、(b)，end 只在 EOF 條件下計，relay 對 Gateway 的核對）與「J5a 也判 ramp」。
   - 建議：照本版。
   - 替代：只用 (b)（最小）；或不補強，把 §1 與 J5a 改寫成「只核對帳本的關閉分類」（xhigh 的 (c)）。
2. **D44 事後規則**：單位是每位玩家的事件；平均多 ≥1 次／跑次就停；新原因要在 ≥2 個跑次出現才停，只出現 1 次的交使用者判斷。
   - 依據是開發跑次三棵樹逐案相同，沒有程式來源。
   - 建議接受。
3. **D44 即時規則從 08b 放寬**（拿掉故障案例的 backlog 規則）。建議接受：x1、ramp 三棵樹都有 backlog，維持原規則會在第 1 個注入跑次就停。
4. **J2 的覆蓋要求**：每個（樹、案例）至少 1 次條件成立。建議接受。
5. **修好的樹上 ingress 是 undetermined、或 I3 失敗，就立即停止**。建議接受：I3 的定義仍是 D58 (8) 的暫定。
6. **Cooldown 停止只對修好的樹**。建議接受。
7. **P1、P8 的新數字**（≤2／8、每位 ≤4、>8 包）是推論，沒有程式來源，只影響預測。P1 的 p 不確定性很大。

### 15. 本版對 xhigh 檢查的修正（對照）

| xhigh 檢查的項目 | 修正 |
|---|---|
| 必修 1（B）：`passed` 沒有納入 D44 事後規則 | `judge_l2.py` 的 `passed` 要求沒有停止旗標；CLI 印出停止旗標與檢視旗標 |
| 必修 2（C）：不完整的 session 也能判通過 | 新增 `session` 項：`completed`、沒有中斷或停止、次數與名稱等於 `plan.schedule`；新增 `mock-session-incomplete/` 負向測試 |
| 必修 3（A）：J5a 結構上恆為 0 | 加 (a)、(b)；end 只在 `eof_before_final` 下計；更正 §1 與 D58 (2) 的描述（附 `bp-1/downstream-250ms` 的證據）；標成建議，待確認 |
| 必修 4：I3 是暗中的停止條件 | J3 與第 11 節明寫 |
| 必修 5（E）：D44 的來源與統計力 | 即時規則寫明「從 08b 放寬」與理由；修正 `command_evidence.py` 的引用；事後規則寫明單位，新原因要 ≥2 個跑次 |
| 必修 6（G 與 J1）：driver | 正式執行強制必要參數；J1 核對 runner 身分、scale、35 情境，由 driver 自己跑比較；子程序用新的 session，逾時先 SIGTERM 再殺 group；`run_l2.py`、`sleeper.py` 的 sha 都核對，`sleeper.py` 與 digest runner 納入 `state()` |
| 順手修 D | gameplay 要求 `result.json` 帶 `artifacts`；backpressure 有 `failure.json` 就是 runner_error；J7 要 4／4；`case_verdict` 讀 `failure.json` |
| 順手修 F | J7 的描述（到期喚醒延遲、分箱上界、間隔只量故障窗） |
| 順手修 J4、「4 行」、佇列 256 | J4 改引 `mock-session` 的 `sums`；改成 carried＝lines；更正佇列的描述 |
| 順手修 null、樹順序、`--dry-run` 結束碼 | before 樹的 ingress 數輸出 null；開頭的樹依案例輪替；`--dry-run` 有任何核對或 runner 身分問題時 exit 1 |
| 順手修 P1、P3、P5 | P1 寫出 p 的不確定性；P3 加 ±1 Tick 的時刻容差；P5 只適用 flow |
| 建議的精確核對 | 注入跑次：relay 的上行 forwarded＝Gateway 全域的 `received_datagrams` |
| 引用 | `backpressure_probe.py:342-343`（xhigh 寫 `:336-338`，實際行號是 342-343）；`run_network.py` 不在凍結清單的註記 |

## L2 的結果（2026-10-11 17:13～17:51，commit H `3e5870c`；**通過**）

證據：`build/target/_build/test/logs/pvp-v7-08c-l2-20261011/`（`declaration.md` 是宣告的副本，`session/` 是 driver 的輸出，`session/judgement.json` 是判定，`summary-main.txt` 是下面數字的整理）。session 期間主 checkout 沒有改動，PR #73 的 Auto-fix 暫停。

**判定**（`judge_l2.py --session`：`passed`＝true）

| 判定 | 結果 |
|---|---|
| session | 114／114 completed，沒有停止、沒有中斷；凍結清單 44 檔開頭與結尾都吻合 |
| J1 | before（`c6e53a31…`）對 H（`ab678e56…`），scale 1、3 各 35 情境 `all_same` |
| J2（I1） | 修好的樹 76 次：74 次精確相等、2 次未判定（2.6％；`x1-r4-flow09a`、`downstream-respawn-1000ms-r2-flow09a`，原因都是 final 前沒有 `ipc closed reason=eof`）；每個（樹、案例）都有條件成立的跑次 |
| J3 | 76 次全部 passed（IG、I0、I2、全域、T3、S1、S2、trace、I3、`link_shots`；新鍵每一行帶齊） |
| J4 | clean-60 溢出＝0、截短＝0；只記錄：gateway-1000ms 的拒絕線溢出 36、228（flow）、24、12（flow+09a） |
| J5a | x1、ramp 20 次全部成立：送出過卻沒到＝0，未到達的紀錄全是 epoch 關閉；late_first 每跑次 x1 138～141、ramp 103～112；relay 轉出＝Gateway 收到（逐次精確相等） |
| J5b（記錄） | probe 的 late 修正（兩位合計）：修好的樹 x1 4～6、ramp 4～6；before 全部 0；network0／20／40、clean、respawn 三棵樹都是 0。（更正，2026-10-11，09R-eval：修好的樹的數字少算約一半。`judge_l2.py:129` 取每一側計數器的最大值，而計數器在 epoch 重設時歸零（例：`x1-r1-flow` 的 action-frames 從 2、3、2 歸零）；依凍結 D48 偵測器的計數器增量，實際每跑次 x1 約 10～14、ramp 約 8～10（合計 x1 flow 69、flow+09a 63；ramp flow 37、flow+09a 40）。J5b 只記錄，判定不變；network／clean／respawn 的 0 仍正確（最大值 0 就是沒有發生）。證據：`build/target/_build/test/logs/pvp-v7-09r-eval-20261011/`的 `detector/`、`late_chain.txt`、`report-from-agent.md` 第 1-b 節。計數方式見下面的已知缺口。） |
| J6 | 44 檔吻合、每次都能解析 |
| J7 | downstream-1000ms：`tick_late_p99_le_us` 最大 before 161、flow 250、flow+09a 170；權威間隔最大 16 797～16 845 µs；差都 ≤1 Tick |

**預測**（推論；推翻不是停止條件）
- P1 成立：clean-60 修好的樹 8 次，副本類 0、late_first 0；`fwd_written_duplicate`／寫出約 1.99。
- P2、P3 成立：注入時修好的樹有 late 修正、before 0；starvation 與 backlog 的次數三棵樹逐案相同。
- **P4 被推翻**：network0／20／40 三棵樹的 late 修正都是 0（修好的樹 late_first 每跑次 2～4，但都是單發的遲到，達不到連續 2 次負樣本的規則）。（補充，2026-10-11，09R-eval：機制。Match 每次發布只帶「自上次發布以來的最小樣本」（`MatchRuntimeHost.cpp:129-134`、`:466-479`）；遺失造成的遲到（該玩家的 Input 在 relay 連續被丟 2～3 包）由下一個倖存的包一次帶到（每次到達是 1 個 Input、1～2 個命令），所以只產生 1 次負樣本的發布；下一次發布帶準時樣本，Client 的 `lateSamples_` 歸零（`LocalPlayerPrediction.cpp:128`），連續 2 次的規則（`:133-137`、`Movement.hpp:58`）因此不觸發。L2 的 32 個 network 跑次共 67 次這樣的到達，相鄰兩個 Tick 的發布都帶負樣本的 0 對；位置固定（遺失用固定種子，見已知缺口）。P4 是預測錯誤，不推翻已得的結論，08c 的判定不依賴它。證據：`build/target/_build/test/logs/pvp-v7-09r-eval-20261011/`的 `p4_late.txt`、`report-from-agent.md` 第 2 節。）
- P5（只對 flow）：`fwd_stale_sequence_input_packets` 中位數 3／15.5／39（預測約 18／32／32）：network0、network20 低於預測的一半，**推翻條件成立**，依預測先核對 relay 的 forwarded 與 Gateway 的資料包數、再判斷重播模型（之後處理，屬記錄）。flow+09a 10.5／19.5／32.5（只記錄）。（更正，2026-10-11，09R-eval：核對完畢，是預測的前提錯了，不是計數或模型錯了。①relay 轉出＝Gateway 收到，32／32 個 network 跑次精確相等；②依 relay 實際的轉出順序重建，`fwd_stale_sequence_input_packets` 和 Gateway 計數 31／32 跑次逐位玩家相等、1 個跑次差 1，舊序號 Actions 32／32 相等，計數與 IG 正確；③重播模型套在 L2 自己的 relay log 上能重現，差額是模型沒算的 Hello 超車；④約 18／32／32 是 v5、v6 與早期 v7 共 66 份 relay log 的合併中位數，借用了其他建置的分布（D45 的型態），只取 v7 約 13／16.5／24.5，而且隨 probe 幀與模擬格點之間的送出相位每跑次變動。不影響任何判定，不需要新的量測。證據：`build/target/_build/test/logs/pvp-v7-09r-eval-20261011/`的 `p5_stale.txt`、`p5_overtaker.txt`、`report-from-agent.md` 第 3 節。）
- P6 成立：downstream-1000ms 修好的樹 late_first 0、副本類 0。
- P7 成立（不為 0）：respawn 案例 `epoch_old` 每跑次 828（flow）、804（flow+09a），`life_old` 0；量比預測的約 147 大（單位與來源之後核對）。
- P8 成立：gateway-1000ms 速率拒絕 4、4（flow）、0、2（flow+09a），T3 成立。
- P11 成立：替代的千分比 after ≤ before（例如 network40 1.74 對 3.74，x1 84.3 對 84.8）。

**記錄項**
- Gateway 拿不到內容的丟棄（D58，pv7 的依據）：只出現在 network 案例的**舊序號 Actions**（每跑次 0～34，各樹案例合計 11～99）與 gateway-1000ms 的速率限制（每跑次 0～4）；其他案例 0。
- D44：三棵樹逐案相同（x1、ramp 每跑次 5 個事件＝starvation 4＋backlog 1；downstream 2、gateway 2、respawn 1；其他 0），全部在故障窗內；停止與檢視旗標都沒有。
- U4：clean-60 的替代三棵樹都是 0。
- 權威 Cooldown 拒絕：只在 network20／40 出現（before 4、flow 1、flow+09a 1），clean 沒有。
- run_case 的遊戲判定（只記錄）：before 的 x1 有 2 次 failed，其餘通過。

**結論**：資訊流修好了。Client→Gateway 的遲到在修好的樹上 Match 都收得到、分得出來，並驅動 late 修正；修之前的樹在同樣的注入下 0 次。權威結果、D44 的重設、Match 的 Tick 都沒有變差。下游（late 修正規則、B2、backlog 餘裕、09a／09b 的範圍）依 D51 ⑤ 在這份資料上重新決定。

**已知缺口（驗收工具；補充，2026-10-11，09R-eval，歸屬依 D63 ④）**
- **probe late 修正的計數方式**：`judge_l2.py:129` 取計數器的最大值，計數器在 epoch 重設時歸零，所以有重設的跑次會少算（J5b 的更正）。之後的 L2 宣告改用凍結 D48 偵測器的計數器增量（P 事件），不用 judge 的最大值。歸屬：之後的 L2 宣告。
- **network 案例的遺失是固定種子**：relay 的亂數用 `random.Random(0x47594F+…)`（`action_probe.py:230`），每次 network 跑次都在同樣位置連續丟 2～3 包（玩家 2 在故障後 +0.36～0.43 s 與 +3.82～3.90 s；network40 另有玩家 1 在 +3.82 s）。所以 network 的「每跑次次數」是同一個情境的重複量測，不是獨立樣本；頻率類的說法不得把跑次當作獨立單位。改種子要動凍結的 `action_probe.py`，屬計畫外。歸屬：驗收工具，第 16 批的回歸要說明。
- 證據：`build/target/_build/test/logs/pvp-v7-09r-eval-20261011/`的 `report-from-agent.md`「另外發現」兩節、`p4_late.txt`。

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
- **D44**：暴露上升，L2 依樹記錄偶發 epoch 重設的頻率。（更正，2026-10-11，09R-eval：只在持續延遲（x1、ramp）時成立；network 型（遺失加 ≤30 ms 抖動）不成立，L2 的 late 修正 0，機制見「L2 的結果」的 P4。）**第 16 批**：整體回歸的基準換成修好之後的資訊流。
- **第 10 批**：觀測缺口第 1、2、4 項大部分在本批完成；剩下 D5、D6、舊序號的 Actions、Client 端的欄位（第 3 項）；log 契約決定沿用或取代 ingress v1。
- **第 11 批**：pv7 的 `GatewayDropReport`（原因列舉一次定完整，dwell 的定義只有一個）；「Gateway 職責」一節；重新檢視 Gateway 的狀態；是否轉送 A3／A5。
- **第 10、11 批共通**：本批的丟棄回報就是 Match 端的死信隊列；ingress v1 與 `GatewayDropReport` 要不要統一成一個死信的 Data Contract，見 HANDOFF 未結事項「死信隊列（命名與概念）」。
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
