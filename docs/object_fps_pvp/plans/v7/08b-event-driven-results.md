# 第 08b 批：結果改為事件驅動轉送

狀態：**規劃完成（草案），待使用者確認「需要決定的事」**（2026-10-09）。PR 線 P2（PR #73）。依賴第 08 批。
檔位：規劃 ultracode（D45；使用者 2026-10-09「開始第 08b 批，開 ultracode，檔位照建議」）；實作 high，計時器迴圈、期限一致性、lane 的到期判斷局部 xhigh；文件與執行 medium。

用語：本文件把「每 I 重送一次、只帶 retired 的包」稱為**只帶 retired 的重複包**（簡稱重複包），不叫心跳，以免和第 11 批 runtime link 的心跳對時混淆。

## 起因

- D43：第 08 批的評估顯示，結果在路徑上要等兩條固定 30 Hz 節拍的相位（第 08 批文件的節拍清單第 4、5 項）。第 09 批要用這些通道推導 FireGate 常數，所以先改成事件驅動。
- 規格（HANDOFF「P2 以後各批的範圍」；2026-10-09 使用者更正）：Match 的 action lane 與 Gateway 的結果，距上次送出 ≥I 就立即送；不到 I 就設計時器，在「上次送出＋I」送出；不帶 I/2。I＝1/30 秒（`action_delivery.go:16`，33,333,334 ns）。
- 範圍追加（使用者 2026-10-09）：
  - 一起調查第 08 批 L2 的 b 段變長。
  - 加一節「對 movement 佇列與 backlog 的影響」（D44）。

## 現況（規劃時逐行核對，HEAD `a13cfc0`）

**Match 的 action lane**（Match→Gateway，TCP，`IpcHost.cpp`）
- `ActionLane` 只有 `{cursor, nextSend}`（`:172`），在 join 被接受時建立（`:368`）。
- `ScheduleWrite`（`:404`）取所有 lane 中最小的 `nextSend`：到期就等 socket 可寫；還沒到期就在 `nextSend` 設 `laneTimer`（`:410`、`:422-428`）。
- `Write`：`now<nextSend` 就跳過（`:454`）。到期後**先**設 `nextSend=now+I`（`:455`），**之後**才檢查「裁決空且 `retiredThrough==0` 就不送」（`:459`）。
- 所以第一次退休之後，每個到期的 lane 每 I 一定送一包，也就是只帶 retired 的重複包。內容是全部未退休的裁決，每包最多 8 個，cursor 輪轉（`:136-157`）。機制上已經是「到期就送、沒到期設計時器」，但因為永遠有內容，新裁決要等這條自由運轉的 30 Hz 格點。
- 喚醒來源：publish 通知 Pump、socket 讀寫、`laneTimer`。退休在 `MatchRuntimeHost.cpp:224-225`（Tick 內），裁決在 `:268` 的 `match_.Tick`（`PvpMatch.cpp:270-300`）；Step 有 publish 時才通知（`MatchRuntimeHost.cpp:189-193`）。

**Gateway 的結果**（Gateway→Client，UDP）
- `sendUDP` 用固定 ticker（`server.go:585`），每次 tick 呼叫 `actionPackets(time.Now())`，寫出後呼叫 `actionWritten(time.Now())`（`:594`）。`actionWritten` 重設 `nextSend＝寫出完成＋I`，並記錄統計（`action_delivery.go:362-368`）。
- 資格判定帶 I/2 容許（`resultSendTolerance`，`:25`，套用在 `:235`）。
- 只有 `len(ids)==0 && w.retired==0` 才跳過（`:245`），所以第一次退休之後每個 ticker 都送。
- 玩家不是 active 時，Gateway 收到的 Match 結果直接丟掉（`:303`）。

**證據**（第 08 批 L2 第 2 次；評審與對抗式檢查各自重算）
- 下行 kind 7 中帶新內容（新的裁決 ID，或 retired 前進）的只有 9.2～9.4％（clean-30 after n＝11130），其餘都是只帶 retired 的重複包。
- c 段（裁決 Tick 的 snapshot_produced → relay 第一次看到該裁決的下行）：
  - clean-30 after 平均 30.2 ms。以 P＝16.67 ms 分組，c//P 的分布 {0:2, 1:225, 2:52, 3:129}，c mod P 的中位數 0.90 ms。
  - 對抗式檢查的補充：從第 07 批的 Match 起，Gateway 的 30 Hz 送出鎖在 snapshot 轉送之後。kind 7 落在同 session 前一個 kind 4 之後 1 ms 內的比例：第 06 批 L2 約 1％，第 07 批 L2 before（舊 Match）約 1.8％、after（新 Match）約 100％。兩樹用同一個 Gateway，只有 Match 不同。第 08 批 L2 兩樹都約 100％。
  - 所以 0.9 ms 是「snapshot 轉送之後」的偏移，不是裁決路徑本身的成本。為什麼會鎖定是 hypothesis（未驗證）：Go 計時器要等下一次網路喚醒才觸發，可能是 macOS 的計時器合併。獨立程序裡的 Go ticker 只晚約 1.1 ms，沒有鎖定（`evidence/gotimer.out.txt`），所以鎖定來自執行環境，不是 Ticker 型別。
- Gateway 的 `results_min_interval_ms` 最小 16.9 ms（clean-30 after），是 I/2 容許造成的。
- relay 的「結果任一秒」：after 每個案例都是 31；before 的 clean-60 r3 兩個 session 都是 33，超過凍結規則。對抗式檢查證實原因是 relay 端的時間壓縮：33 那個視窗的開頭 3 包在 3.2 ms 內到達，在那之前 relay 有 54.2 ms 沒有任何事件，同一跑次 Gateway 端的最短間隔是 33.2 ms（`adversarial/window33.stdout.txt`）。

## 設計

**核心判斷：「事件」要定義成「有新內容」**
- 只把時機改成「≥I 立即、不到 I 計時器」而保留重複包，兩段都不會改善：通道永遠不閒置，新內容一定落在「上次＋I」的格點上。
- Match lane 本來就有計時器，照字面改幾乎沒有效果。
- 所以 08b 要同時拿掉兩段只帶 retired 的重複包。wire 不變，但語意有變，要使用者先確認（決定 1）。

**A. Match 的 action lane（只改 `IpcHost.cpp`）**
1. `ActionLane`（`:172`）改成 `{optional lastSend; ActionId sentRetired; 已送的裁決 ID 集合 sent}`。每次選出時刪掉 ≤`retiredThrough` 的 ID，集合最多 32 個（`MaxActionWindow`，`Combat.hpp:14`）。
2. 共用判斷 `LanePending(results, lane)`：有裁決 ID 不在 `sent` 裡，或 `retiredThrough>sentRetired`。`ScheduleWrite` 的到期判斷與 `Write` 的選出用同一個判斷，避免「到期了卻選不出」造成寫出等待空轉。今天不會空轉，是因為 `:455` 會先推進 `nextSend`。
3. `ScheduleWrite`：只計入 pending 的 lane，期限＝`lastSend+I`（沒有 `lastSend` 時為 now）。`GetActionResults` 回 nullopt 的 lane 移除。
4. `Write`：只選「pending 且 now≥lastSend+I」的 lane。未送的裁決依 ID 遞增取前 8 個，加上 `retiredThrough`。選出時才設 `lastSend=now`、併入 `sent`、`sentRetired=retiredThrough`。刪掉 `:455`「沒內容也推進錨點」的行為。
5. 不重送，理由：
   - 走 TCP，每次 accept 都重建 `Session{}`（`:254`、`:282`）。
   - 對 active 玩家，Gateway 不會靜默丟掉 Match 的結果：不是 active 才丟（`action_delivery.go:303`）；驗證失敗會回傳錯誤（`:309-311`），經 `runtimeFailed` 結束連線，Match 隨之重設（`server.go:473-474`、`IpcHost.cpp:274-286`）。link 的 merge 與 session 的 commit 在同一段 `s.mu` 之內（`server.go:360-361`、`action_delivery.go:284-293`、`session.go:80-103`），不會出現「請求只合併了一半」的不一致。
   - **非 active 期間的結果**（使用者 2026-10-09 的問題；分析 1 位＋對抗式檢查 1 位，都用 high）：會被 `:303` 丟掉的情況只有兩種，兩種都不會遺失可補送的內容，所以今天的重複包原本就救不回任何東西，08b 改成只送一次，結果相同。
     - **加入完成之前**（reserved、joining）：Match 不可能有該玩家的裁決或退休。裁決與退休只從 ActionBatch 產生（`IpcHost.cpp:332-349`、`MatchRuntimeHost.cpp:140-173`、`:224-226`）；Gateway 只轉送 active 玩家的動作（`server.go:394`）；Gateway 在處理 JoinResult 時才轉成 active（`:491`，由 link 唯一的讀取 goroutine 在 `s.mu` 下同步處理）。Match 這邊也是同一次 Pump 先放 JoinResult、再建立 lane，寫出時 controls 先於 lane（`IpcHost.cpp:361-370`、`:445`）。
     - **移除之後**（leave、逾時、eviction、join rejected、`runtimeFailed`）：會發生。例如動作已經送出、玩家剛好離開，Match 在讀到 Leave 之前做出裁決送回來，就會被丟掉。但移除是終態：phase 只往前走（只有 `server.go:384`、`:491` 兩處賦值），player id 單調遞增、不重用（`:271-275`；既有測試 `TestPendingJoinTimeoutCannotResurrectSession`），runtime link 也不會重連（`:110`、`:116`、`:131-132`）。Match 讀到 Leave 或 eviction 時，同時刪除 lane 與玩家的動作狀態（`IpcHost.cpp:324`、`:385`、`:457`，`PvpMatch.cpp:45`）。被移除的 Client 在連線世代的邊界清空帳本（`ClientConnection.cpp:163-165`），重新加入拿到新的 id，動作窗不會卡住。
     - 既有日誌：1087 份 gateway.log 依程序切段後，同一程序內 id 再次 active、id 重用、移除後又出現，都是 0 次。被丟掉的結果不寫 log，所以「離開時正好有結果在路上」發生過幾次數不出來，這一點靠程式論證。
     - **不變式**（寫進程式註解，`action_delivery.go:303`）：phase 只往前走、id 不重用、只轉送 active 玩家的動作。將來如果加入「沿用同一個 id 重連」或 link 重連，必須同時加上「玩家 active 時由 Match 補送全部未退休的狀態」，否則只送一次的 lane 會遺失結果。
     - 不採用的修法：把 `:303` 改成錯誤或 `runtimeFailed`（正常的離開會讓整個 Match 重設）；Gateway 替非 active 玩家暫存結果（沒有人會再取用）；在統計行加丟棄計數（會改統計行的格式）。
   - Match 對已裁決的重複請求不產生新內容（`MatchRuntimeHost.cpp:163-164`）。
6. 寫出優先序 controls > lanes > snapshot 不變（`:441-468`）。
7. 依賴「每個 Tick 都 publish」（`SnapshotIntervalTicks==1`，`MatchRuntimeHost.cpp:293`）：裁決在有 publish 的 Tick 才會被 Pump 看到。用 `static_assert` 或註解寫明。
8. 每次 `ScheduleWrite` 會對每個 lane 呼叫 `GetActionResults`（取 host mutex，複製 ≤32 筆），會和 Step 競爭。影響可忽略是 hypothesis（未驗證）；開發跑次與 L2 記錄 `tick_late` 與 `ipc_iterations_per_s`。

**B. Gateway 的結果（`action_delivery.go`、`server.go`）**
1. 刪掉 `resultSendTolerance`（`:18-25`），`:235` 改為嚴格判定 `now.Before(w.nextSend)`。錨點仍是寫出完成＋I（`actionWritten`），保留「被擋住的寫出放行後不補送」。
2. session 端的 window 加 `retiredSent`、`retirementAsked`，只屬於 session 方向，不影響 link 的 `actionBatch`。
3. pending 的條件，三者任一：
   - (a) 有裁決 ID>`w.acknowledged`：照現在每 I 重送一次。UDP 需要它，也保留「重送直到連續 ACK」（`actions_test.go`）。
   - (b) `w.retired>retiredSent`：退休前進時送一次。
   - (c) `retirementAsked`。
4. (c) 的觸發（poke）：`receiveActions` 在 `CommitSequence` 與 merge 成功之後（`:290-293`），若 `len(in.Shots)==0` 且 `0<AcknowledgedThrough≤w.retired`，就設 `retirementAsked` 並喚醒送出迴圈。
   - 依據：Client 只有在 ack 大於自己知道的 retired 時，才送只帶 ACK 的批次（`ClientConnection.cpp:413`），每 I 一次（`:420`）。所以這個條件成立，就證明 Client 缺了那次退休。
   - 最後一次退休遺失時，poke 是唯一的補救；沒有 poke，Client 的動作窗會停在 retired＋32（`:818`）。
   - 帶 shots 的批次不觸發，因為無法判斷 Client 的 retired。
5. `actionPackets` 同時回報要送的封包與下一個期限：取 active、`s.available` 且 pending 的玩家中最早的 `nextSend`。選出時一律先設 `nextSend=now+I` 再編碼，這樣編碼失敗的 continue（`:253-264`）才不會讓計時器以 0 等待空轉。選出時清掉 (b)、(c) 的旗標。
6. `sendUDP`（`server.go:584-595`）：ticker 換成 `time.Timer` 加上 `resultWake`（容量 1，在 `New` 建立，以不阻塞的方式送出）。
   - `receiveActionResults` 合併後，只有出現新 ID 或 retired 前進時才喚醒；poke 也喚醒。
   - 寫法照 `runtime_link.go:242-279`：wake 或 timer 觸發 → `actionPackets` → 寫出 → `actionWritten` → 在 `s.mu` 下取期限並 `Reset(max(time.Until(d),0))`；沒有期限時 `Reset(I)`，作為閒置時的保險，那次喚醒不送任何東西。
   - go.mod 是 go 1.23，`Reset` 不必先 drain（link 已依賴這點）。
7. 不改：runtime link（第 08 批的嚴格間隔與期限計時器）、wire、統計行的格式、Client、probe。

**C. 「結果每秒 ≤31」的論證（由程式推導）**
- 同一玩家下一次選出 ≥ 上一次寫出完成＋I，戳記是單調時鐘。
- 在半開的 1 秒視窗內（`action_probe.py:319-327`），(n−1)·I<1 s，所以 n≤30。kind 7 只從 `actionPackets` 寫出，這個上限涵蓋所有結果包：新裁決、重送、退休、poke 回應。
- relay 端的 ≤31 無法由產品保證：relay 端的時間壓縮會讓間隔變小（上面的 33）。08b 拿掉約 90％的重複包之後，持續 30 Hz 只會出現在「未 ACK 的裁決留超過 1 秒」的期間（下行阻塞、drain-stall）。
- Match lane 同樣論證：選出到選出 ≥I。

**D. 預期效果（hypothesis，未驗證，不放進門檻）**
- 合法動作之間的間隔遠大於 I：同 session 前一個上行 kind 6 到新動作的最小間隔，clean 案例 93～98 ms。但計入被拒的動作時，最小間隔在 clean-30 是 33.4 ms、clean-60 是 44～47 ms：動作計畫裡有約 I 間隔的連續動作，所以「不到 I 設計時器」的路徑在 L2 一定會被走到。
- 新裁決到達時，兩段多半已閒置 ≥I，c 應該縮短。但「不到 I 的期限」、退休 (b)、重送 (a)、poke 回應都走計時器，依上面的鎖定現象，可能被延到下一次 snapshot 轉送（最多約 16.7 ms）；Match 的 `laneTimer`（asio）也可能一樣。所以 after 的 c 可能是雙峰。
- 下行結果包預期大幅減少（hypothesis）。clean 中 (a) 的重送很少：after 中「裁決首次下行 → 上行 ACK」超過 I−2 ms 的只有 0.6～0.8％（`adversarial/ack_delay.stdout.txt`）。但 poke 和 (b) 會互相競爭：Client 下一個只帶 ACK 的批次，可能在 Gateway 送出 (b) 之後、Client 收到之前抵達，觸發 poke，多送 1 包退休。downstream-250ms 中，整個 RTT 期間每 I 會觸發一次 poke，約 8 包。兩者都受 ≥I 約束，無害。

**E. 架構 Delta**
- 只改產品自有的 `IpcHost.cpp` 與 gateway 套件；不新增子系統，不改 `MatchRuntimeHost`／`PvpMatch` 的介面。
- 行為上的 Delta：伺服器端「無條件週期送 retired」改為「退休送一次，遺失時由 Client 的 ACK-only 觸發補送（poke）」。可靠性責任有一部分轉到 Client 既有的 ACK-only 迴圈（`ClientConnection.cpp:413`、`:420`）。

## b 段變長的調查（第 08 批 L2 第 2 次）

**結論：在 Tick 解析度下沒有退步；Tick 以下的路徑延遲，現有紀錄量不到。** b 變長來自量測上的耦合：D36 讓 a 縮短的時間轉移到 b，加上 probe 幀格點相對 Match Tick 提早。產品程式不需要為 b 修改。

- k＝裁決 Tick −「relay 上行之後第一個 snapshot_produced 的 Tick」，也就是 relay 收到之後漏掉幾個 Tick：
  - clean-30 after 408 筆全是 0；before 是 {0:403, 1:5}。
  - clean-60、gateway-250ms 兩邊全是 0；upstream-250ms 兩邊都是 {0:198, 1:6}。
  - after 沒有任何動作因路徑延遲而晚一個 Tick。k＝0 時，b 就等於「relay 上行到下一個 Tick 的剩餘時間」。這是定義上的恆等式，是推論，不是另一項觀測。
- 各假設：
  - H1（link 的嚴格間隔，新請求碰上 ACK 批次重新錨定）：就 b 而言否定。k＝0 時，link 內的任何等待都不會改變 b。合法動作到達時，前一個上行 kind 6 距今至少 93～98 ms（clean），nextSend 早已過去。
  - H3／H4（Match IPC、Gateway 收包路徑）：在 Tick 解析度下否定，理由同上。
  - H2＋H5（抵達點在 Tick 格點中的相位、probe 相位）：成立，而且是系統性的。
    - probe 起點距前一個 Tick 的中位數：clean-30 3.91→1.85 ms、clean-60 3.54→1.63、gateway-250ms 3.66→2.04、upstream-250ms 3.38→1.85。
    - 機制：probe 在 join 的 Wait 看到兩位玩家的 snapshot 之後才定起點（`gameplay_action.hpp:109`；Wait 每 2 ms 輪詢，`action_main.cpp:79-82`）。第 08 批的 asio worker 收 snapshot 比較快：Match 產生→worker 收到，P50 1.31→0.41 ms。
    - 之後幀格點還會在超時時重新錨定（`gameplay_action.hpp:170`）。60 個跑次全部在第 0～2 幀重新錨定一次，延後 0.5～4.96 ms；另有 9 個跑次在中途出現 ≥5 ms 的重新錨定（例如 clean-30 before r1 第 7 幀 ＋35 ms）。所以單一跑次的幀相位不能由起點推得；彙總上，「幀相位減起點」的中位數兩邊相近（clean-30 after 4.0、before 4.2）。
    - 起點的位移約 0.9 ms 可由 snapshot 收包變快解釋；其餘約 0.4～1 ms 的來源未確定。
  - H6（D36 讓 a 縮短的時間移到 b）：成立。同一個送出相位組（1 ms 一組）內，送出→裁決 Tick（a＋b）兩樹的差距只有 −0.21～＋0.43 ms。Match 反正在下一個 Tick 才裁決，早到只有在剛好跨過 Tick 邊界時才有用。
- 給 08b 的 L2 與第 09 批：b、a＋b、`legal_match_p95_ms`、d 都會因建置不同、經由 probe 相位而變化，不做 before／after 判定，只記錄，並附上 k 與起點相位。改用和相位無關的指標：c、k 率、同一送出相位組內的 a＋b。

## 對 movement 佇列與 backlog 的影響（D44）

使用者的問題：08b 會不會改變 movement 輸入抵達 Match 的時機或集中度，會不會讓 backlog（30 Tick 內排隊命令合計 ≥105，`PvpMatch.cpp:410-452`）更容易觸發。分析 1 位（high）＋對抗式檢查 1 位（xhigh），只用既有證據與程式；以下是對抗式檢查修正後的結論。

**結論：08b 不會直接改變 movement 輸入抵達 Match 的時機，也不會直接讓 backlog 重設變容易。D44 不需要在 08b 實作前調查，維持第 16 批整體回歸的排程。** 但分析找到了 D44 的可能機制（見下），它和第 09 批的相位常數有關。

1. **08b 不改輸入路徑上的程式。**
   - 不改的部分：Client 的送出規則（`ClientConnection.cpp:590-628`）；Gateway 的 `receivePacket`、`link.input` 與 link 的寫出迴圈（`server.go:340-441`、`runtime_link.go:100-165`、`:208-281`）；Match 的 IPC 讀取、`SubmitInput` 與 Tick 交接（`IpcHost.cpp:288-331`、`MatchRuntimeHost.cpp:70-131`、`:237-266`）；`PvpMatch` 的 backlog；`TrackSlack`；相位追蹤。
   - 和輸入共用的有三處：
     - Gateway 的 `s.mu` 與 `sendUDP`。`sendUDP` 也負責快照轉送，`snapshotOut` 的容量只有 1（`server.go:118`、`:546-556`、`:584-610`）。快照遺失會改變 Client 看到的 slack 樣本序列。
     - Match 的 IPC io 執行緒與 host mutex。08b 會讓 `ScheduleWrite` 對每個 lane 多取一次鎖（設計 A.8），而每次 input 讀取後都會跑 Pump（`IpcHost.cpp:296`）。
     - Client worker：期限在 2 ms 以內時，io 執行緒會阻塞在 `WaitUntil`（`ClientConnection.cpp:698-707`），`SendInput` 的 post 最多延後 2 ms。08b 改變退休送達的時刻，就會改變 Client 持有 action 期限的時間。
   - 依程式，這些地方對單一命令的延遲上限是「一個臨界區或 handler 的長度」，再加上 Client 端 2 ms 以內。量級在 1 ms 以下只是 hypothesis（未驗證），沒有量測。
2. **延遲本身不會讓排隊命令變多，但超過 slack 就會。**
   - 排隊命令的上限是 Client 已經產生的領先：晚到的命令會被替代，已解析過的序號會被丟掉（`PvpMatch.cpp:75-82`、`:77-79`，`MatchRuntimeHost.cpp:88`）。所以單純的延遲只會讓它變少。
   - 例外：延遲超過 slack（第 08 批 L2 第 2 次 clean：host→執行的 P01 約 26.6 ms、P50 約 37.2 ms），而且兩個遲到命令的負樣本分別落在連續兩次發布。這時 Client 會立刻把相位往前修正（`LocalPlayerPrediction.cpp:128-137`），幅度幾乎都被夾在上限 2 Tick（33.3 ms，`:97-98`、`Movement.hpp:59`）。領先從 2 變 4，約 23 個 Tick 後 30 Tick 合計 ≥105，epoch 重設。
   - 脆弱的停頓長度大約在 30～150 ms：比 slack 長，又短到不會讓權威越過 Client 的末端而 reseed（視窗上限 12 個命令，`Movement.hpp:34`；reseed 在 `LocalPlayerPrediction.cpp:196-200`）。這正是主機噪音的量級。gateway-250ms、upstream-250ms 這類較長的停頓會 reseed，所以沒有 backlog。
   - 所以真正的風險不是合計的餘裕（穩態 60、L2 最大 70，最大值的分布是雙峰：不是 60～70，就是 ≥105），而是「遲到修正」多常發生。以不依賴 generation 間隔的方法獨立偵測（gen→exec 中位數跳升 >20 ms）：
     - 第 06、07、08 批 L2 共 180 次：0 次。
     - 第 08 批開發的 63 個 trace：3 次。D44 那次到 108 並重設；downstream-death-1000ms 到 89；network20 之後到 105 並重設。
   - 歷史上 network20 的 3 次 backlog（v6 batch09、v6 batch14b、第 08 批開發）也都是「成對遲到 → 相位前移約 30 ms」的同一條鏈，不只是抖動造成的結構性領先，要計入 D44 機制的統計。
3. **唯一有完整 trace 的 D44（backpressure host-ipc-250ms）**：
   - 注入的 IPC 停頓在 4.45～4.70 秒，重設在 9.78 秒，晚了 5.08 秒，超出 1.5 秒的歸因窗，所以被判為「Unexplained」。
   - 重設前約 0.5 秒（9.20～9.27 秒）：Client worker 送出晚了 12～22 ms，Match 的 Tick 晚約 15 ms，快照 588 晚了 38.6 ms 才到 Client，589 沒有送到。也就是 Client、Gateway、Match IPC 之中至少有一處停頓了約 30 ms。停在哪裡無法確定：589 也可能是在 Match host 內被覆蓋（`MatchRuntimeHost.cpp:307`，這條路徑不寫 trace）。
   - 之後兩個命令遲到，Client 的 generation 間隔出現 13.3 ms 的斜坡，合計約 −33.4 ms，等於 2 Tick 的上限；排隊命令 2→3→4，合計 53→104→108，Tick 622 重設。「連續 2 個負樣本觸發遲到修正」是推論，因為 trace 沒有記錄 Client 的相位決策。
   - 當時沒有任何動作流量（results=0、link_actions=0），08b 要改的程式只是在空轉。
   - 另一次（`run_network.py`）：Match 啟動時沒有 `--movement-trace`（`run_network.py:161`），重設原因沒有紀錄，也可能是 starvation（6 秒應用程式停頓的測試，任何重設都判失敗，`network_main.cpp:280-297`）。停頓測試的玩家 11／12 在那個視窗沒有動作，但較早的玩家 8／9 有射擊流量。
4. **還沒排除的：間接的主機噪音**（CPU、喚醒次數、鎖競爭，以及計時器寫錯造成的空轉）。在沒有動作的跑次，08b 只會減少 Match 的空轉喚醒，Gateway 的閒置計時器（`Reset(I)`）保留不變。要在開發跑次與 L2 記錄確認。
5. **檢定力的限制**：L2 過去 180 次為 0 次，95％上限約每次 1.7％。照這個比例，after 36 次中出現至少 1 次的機率約 45％。所以 after 出現 1 次不能歸因於 08b；沒出現也不能證明 08b 沒有影響。
6. **給第 09 批的輸入**：D44 的可能機制落在相位追蹤（遲到修正的 2 Tick 上限、相位目標 4 ms、領先 2）與 backlog 門檻 105 的交互作用上。門檻 105 與領先 2 從 `bfeb047` 起沒變，遲到修正的規則從 `a4ccaa5` 起。第 09 批推導 FireGate 常數前，要在規劃中回答 D44 是否需要先查清楚。

**08b 要記錄的項目**（只記錄，不改產品；開發跑次與 L2 都做，before／after 比較）
- D44 的主要指標：每個跑次、每位玩家的「遲到修正／領先跳升」次數，偵測方法在 L2 宣告中寫明（例如：epoch 開始 1 秒之後，gen→exec 中位數比前 60 Tick 高出 20 ms 以上）。合計的最大值與 `reset_reason` 只用來確認結果。network20／40 另加「受損連線」標記，但計入 D44 機制的統計。
- 用來定位停頓的既有欄位：Gateway 的 `coalesced_input_windows`、`coalesced_snapshots`、每個視窗的 `snapshot_interval` 最大值；Match 的 `snapshot_overwrites`、`tick_late_bins`、`ipc_iterations_per_s`、`cpu_s`；Client 端遺失的快照數；worker 的喚醒次數與 CPU。
- 08b 自己的風險：Gateway 的 CPU 與 `sendUDP` 的喚醒次數（偵測空轉）；Match 的 `tick_late`（`ScheduleWrite` 多取的鎖）。
- 新舊版本的失敗次數分別累計（D44）。
- 分析的腳本與輸出：`build/target/_build/test/logs/pvp-v7-08b-plan-20261009/d44-analysis/`、`d44-adversarial/`（`commands.txt`、`sha256.txt`）。

## 驗收

**L1：Go**（`apps/object_fps_pvp/gateway`；用 `result_cadence_test.go` 的手動時刻法，由 `actionPackets` 回報的期限推進虛擬時鐘，不用固定 tick，避免重現第 06 批的「每隔一次」）
- G1 閒置時立即送：退休之後沒有封包（沒有重複包）；新裁決在 ≥L+I 到達，同一個 now 就送出。
- G2 不到 I 時等到期限：寫出完成在 t0，t0+10 ms 來了新裁決 → 回報的期限＝t0+I；t0+I−1 ns 時 0 包，t0+I 時 1 包。
- G3 不爆量：固定 seed 的隨機事件時刻，寫出延遲 0～30 ms，加一次 250 ms 的阻塞寫出。斷言相鄰間隔 ≥I、任一半開 1 秒視窗 ≤30、放行後不補送。
- G4 退休只送一次：Client ACK、Match 退休 → 送 1 包帶 retired；之後 1 秒內，計時器與 wake 都送 0 包。
- G5 poke：只帶 ACK 且 ACK≤retired → 到期時送 1 包。ACK>retired、帶 shots 的批次、被 `CommitSequence` 拒絕的重複 sequence 都不觸發。
- G6 未 ACK 的裁決每 I 重送，ACK 之後停止；超過 8 筆時輪替。
- G7 一致性的性質測試：隨機的帳本、phase、available、時刻。回報的期限 ≤now 時，`actionPackets(now)` 一定會為那位玩家送出；反過來，送出的玩家一定有期限。涵蓋 joining、`!available`、Leave、evict。
- G8 真實時間的接線（`sendUDP` 搭配 fake runtime）：
  - 在上次寫出後 30 ms 送新裁決：正確的實作約在 t0+I 送出，固定 I 的計時器約在 t0+63 ms。上限設 t0+I+15 ms，這樣抓得到 `results-fixed-timer` 突變。或把「下一次等待時間」抽成純函式，用虛擬時鐘測（實作時選一個，寫進文件）。
  - 新裁決的 wake 要喚醒迴圈；1 kHz 的多餘 wake 打 200 ms，送出數不變。
- G9 計時器等待中發生 `runtimeFailed`、Leave、evict、ctx 取消：沒有封包、不空轉，迴圈結束。
- G10 非 active 的結果不起作用，身分不會回來（`server_test.go`，用既有的 `newTestServer`、fake runtime）：
  - (a) joining 的玩家送 Actions：runtime 沒收到 ActionBatch；之後 runtime 送來該玩家的 ActionResults：房間仍可用、peer 沒收到 kind 7、沒有建立動作帳本。
  - (b) active 之後經 `/rooms/1/leave` 離開（runtime 收到 Leave），之後 runtime 送來該 id 的 ActionResults：房間仍可用、沒有 Failure、link 的動作窗沒有這個 id。
  - (c) 用同一個 request_id 再 reserve：拿到新的 player id。
- 改寫舊語意的測試（比照第 06 批，在本文件列出原文與理由）：`result_cadence_test.go:40`、`:52-88`（每個 tick 都送、I/2；`:64`、`:69` 用到 `resultSendTolerance`），`actions_test.go:367-368`。
- 照常通過：`TestActionsRealUDPAndTCPResendUntilContiguousClientACK`、`TestLinkResendsAnUndecidedRequestAtTheActionRateDespiteOtherWakes`。
- 執行：`go vet`、`go test`、`go test -race -count=3`；G1～G7 另跑 `-count=20`。

**L1：C++**（`tests/object_fps_pvp/IpcHostTests.cpp`；目前 4 個案例，沒有 lane 的測試）
- M1 閒置不寫：動作、ACK、退休之後，500 ms 內沒有 action_results frame；同一個 ID 在 ACK 前只出現在 1 個 frame。
- M2 立即送：閒置 ≥100 ms 後送一個動作，帶它裁決的 frame 要先於 tick>resolved_tick 的 snapshot。只需要有裁決（沒帶已發布的 observed tick 時會是 InvalidReference，`PvpMatch.cpp:281-291`），不要求 MagazineFull。
- M3 間隔：每個 Tick 都送新動作並 ACK，3 秒內的 action_results frame ≤ ceil(3 s/I)+1＝91（不節流的突變約 180）。接收端是每 1 ms 輪詢、每次讀 4096 bytes（`IpcHostTests.cpp:96-111`），負載下兩個 frame 會在同一次讀取到達，所以最小間隔只記錄，不斷言。
- M4 不空轉：閒置 500 ms 的 `Iterations()` 增量 < 校準上限。上限在 08b 之前的頭、用同一個測試量現況再定，寫明來源跑次與次數；這個上限只用來偵測 0 等待的空轉（重複包會把基準拉高，上限偏鬆）。
- M5 lane 計時器等待中發生 Leave、evict、Stop：不當掉、沒有那位玩家的 frame、Stop 及時結束。
- M6 換連線：新連線的 lane 是空的，不沿用 `sent`。
- M1～M5 是真實時間：用計數與順序斷言，連跑 30 次。

**突變**（`tests/object_fps_pvp/mutations.json`，batch `v7-08b`；只有出現預期的失敗訊息才算 killed）
- `results-half-tolerance`（`:235` 改回 I/2）→ G2、G3。
- `results-repeat`（pending 改回 `:245` 的條件）→ G4。
- `results-no-poke` → G5。
- `results-no-wake` → G8。
- `results-no-resend`（pending 拿掉 (a)）→ 既有的 `ResendUntilContiguousClientACK`。
- `results-deadline-ignores-phase` → G7。
- `results-anchor-at-selection-only`（`actionWritten` 不重新錨定）→ G3。
- `results-fixed-timer`（計時器用固定 I）→ G8。
- `nonactive-drop-is-error`（`:303` 的丟棄改成錯誤）→ G10 (b)；`nonactive-actions-forwarded`（拿掉 `server.go:394` 的 active 檢查）→ G10 (a)；`player-id-reused`（重用最小的空閒 id）→ G10 (c)。
- `lane-repeat` → M1；`lane-unpaced`（拿掉 L+I）→ M3；`lane-anchor-every-pump`（沒有內容也推進 `lastSend`）→ M2；`lane-due-ignores-content` → M4；`lane-no-sent-tracking` → M1。
- 只會機率性被 kill 的突變，加大 trials 並寫出存活機率；kill 不了的不列入。
- 同一變更移除 `v7-06-results-exact-deadline`、`v7-06-results-full-tolerance`（`mutations.json:728-746`）：它們依賴的 `resultSendTolerance` 會消失，`run_mutations.py:80` 會判為 stale。指向同一批檔案的 `v7-07-ipc-snapshot-before-controls`、`v7-08-link-half-tolerance` 也要確認沒有變成 stale。

**其他**：全量 CTest，權威 digest 不變；TSan 跑 `object_fps_pvp.ipc` 3 次；`worker_main` 驗收連跑 30 次。

**開發跑次**：25 案矩陣、`backpressure_probe.py`、`run_network.py`。
- 記錄 relay 的「結果任一秒」、Gateway 的最短間隔、c、D44 新舊的 epoch 重設次數（`reset_reason≠life_respawn`）。
- 「結果 ≤31」在 L2 與矩陣跑次走的 gameplay v5 路徑中，凍結的分析器**不執行**：`action_probe.py:336-338` 把分析交給 `gameplay_evidence.analyze`，而 `gameplay_evidence.py` 只在 `:207` 檢查 kind 6。所以開發跑次與 L2 都要由 judge 以唯讀匯入的 `maximum_window` 計算。

## L2 的事前宣告（草案，待使用者核准；依 D45 已做 1 次門檻來源的對抗式檢查，修正已併入）

- **目的**：
  1. 確認兩段都改為事件驅動：c（裁決 Tick 的 snapshot_produced → relay 第一次看到該裁決的下行 kind 7）縮短，不再落在兩條 30 Hz 格點上。
  2. 確認不爆量：Gateway 寫出端是嚴格的 I；relay 量到的「結果任一秒」≤31。
  3. 確認 gameplay（含 Client 的 action／ACK ≤31）沒有變差。
  4. 記錄第 09 批推導 FireGate 常數要用的通道分布。
- **產物**（雜湊寫進 `artifacts.sha256`，跑次前後各核對一次；宣告寫進 `declaration.sha256`）：
  - before：沿用第 08 批 L2 第 2 次 after 的 gateway（`f670fbe1…`，第 08 批程式 `730984e`）與 match（`13d70680…`，第 07 批 L2 的 match-after）。跑次前確認「`10003b9` 到 08b 基準之間，Match 連結的原始碼沒有變動」（規劃時：`git diff --stat 10003b9..HEAD` 在 Match 範圍只有 `ClientConnection` 與 client_network 的連結）；有變動就從第 08 批頭重建。
  - after：08b 頭建置的 Gateway 與 Match。
  - probe：兩邊共用 `d45a1f0b0808b7fc25a9f31500b9007854f9bc893917f9706c8b539226607abf`。08b 不改 Client 與 acceptance，跑次前以 git diff 確認。
- **主機**：沿用第 08 批（每輪前後 5 秒 sleeper、記錄背景的高 CPU 程序、TimerBaseline 的主機狀態）。
- **案例**（待決定 5）：第 08 批的 4 案保留可比性（clean-30 ×12，clean-60、upstream-250ms、gateway-250ms 各 ×6），加 downstream-250ms ×6（下行阻塞，涵蓋重送與 poke）。每輪 before、after 各 1 次，奇數輪先跑 before。
- **判定**（全部成立才通過）：
  1. after 的所有跑次，gameplay 判定通過（凍結的 `gameplay_evidence.py`，含 action／ACK ≤31）。
  2. after 每個有樣本的 Gateway 統計視窗，`results_min_interval_ms` ≥33.3。judge 解析字串，和字面值 `33.3` 比較：統計行以 `%.1f` 印出（`send_statistics.go:41`），嚴格的 I＝33.333334 ms 印成 `33.3`，用浮點的 1000/30 比較會誤判失敗。值為「-」的視窗略過。小於 0.05 ms 的違反看不到，交給 L1 的 G3。最後一個統計視窗之後的尾段只有判定 3 的計數保護，沒有間隔保護。
  3. after 每個跑次、每個 session，relay 的下行 kind 7 以 `action_probe.maximum_window`（唯讀匯入）計算 ≤31。凍結的分析器在這條路徑不執行這條規則，所以由 judge 套用。超過時停下，先分辨是 relay 端壓縮（之前有 ≥I 的 relay 空白、同時段 snapshot 也有空白，比照 `adversarial/window33.py`）還是寫出端，再交使用者決定（決定 2）。
  4. clean-30、clean-60：after 的 c 中位數 < 同一 session 中 before 的 c 中位數。只比方向，不設數值；判定或只記錄，待決定 4。
  5. after 每個有樣本的視窗，`link_action_min_interval_ms` ≥ 門檻（待決定 3：33.3 或 32.3）。
- **只記錄**：
  - c 的 P50／P95、c//P 的分布；c 依「距該玩家上次結果寫出 <I 或 ≥I」分組的分布，以及 kind 7 相對前一個 kind 4 的偏移（確認計時器路徑的實際觸發與鎖定）。
  - a、b、d、k 率、probe 起點相位、同一送出相位組內的 a＋b。b、a＋b、`legal_match_p95_ms`、d 不跨 tree 解讀。
  - 產生→worker 收到；relay 每 session 每秒的結果包數與其中沒有新內容的比例；Gateway 每 10 秒的 results 數與間隔；Client 的 ACK-only 批次數；Match 的 `ipc_iterations_per_s` 與 `tick_late`；故障案例的恢復秒數。
  - D44：上一節「08b 要記錄的項目」，before／after 分開。
- **停止條件**：
  - 跑次錯誤時停下，不自行重跑；after 的 gameplay 失敗，或判定 2～5 任一不成立（判定 4 只在被列為判定時）；產物雜湊不符。
  - D44：clean 案例的 backlog／starvation 重設已由 `gameplay_evidence.py:289-291` 判為 gameplay 失敗，落在判定 1，不另外放寬。故障案例中出現 backlog 重設，或在故障結束 1.5 秒之後出現非 respawn 的重設（和 `command_evidence.py:413-422` 同一規則），停下回報；故障窗內的 starvation 只記錄。故障窗的時刻要從該跑次自己的故障紀錄取得，有沒有這個欄位要在定稿宣告時核對（未驗證）。依上一節的檢定力限制，after 出現 1 次只代表「停下來回報」，不代表 08b 是原因。

### 判定門檻的來源（D45 的核對）

| 判定 | 門檻 | 來源建置 | 指標 | 混入的流量 |
|---|---|---|---|---|
| 2 | `results_min_interval_ms` ≥33.3（字面值比較） | 由程式推導，不取自任何跑次：I（`action_delivery.go:16`）、寫出完成＋I（`:362-368`）、嚴格判定取代 `:235`；戳記是寫出後的 `time.Now()`（`server.go:594`）。第 08 批 L2（帶 I/2）最小 16.9 ms，能分辨兩種建置 | gateway.log 統計行的 `results_min_interval_ms`（同一玩家相鄰兩次 `actionWritten` 的最小差） | 新裁決、重送、退休、poke 回應全部共用同一個玩家期限，混合不會讓真實的最小值變小；用最小值，不用 P50 |
| 3 | relay 結果任一秒 ≤31 | 凍結規則的文字 `action_probe.py:410-412`（D42 不改）；視窗演算法 `:319-327`。寫出端上限推導為 30，31 容許 1 個邊界包。第 08 批 before 的 33 只當背景 | relay.json 中 `upstream==false`、`received`、kind 7、同一 session，套用 `maximum_window` | relay 收到的所有結果包，和凍結規則的原意相同；不含 kind 6 與 snapshot。relay 端停頓會壓縮戳記，超過時不自動歸因 |
| 1 的一部分 | Client action／ACK 任一秒 ≤31 | 凍結規則 `gameplay_evidence.py:207`。08b 不改 Client（`ClientConnection.cpp:394-420` 的單一期限） | relay.json 上行 kind 6，依 session 計 `maximum_window` | 新請求、Client 重送、ACK-only 都受同一個 Client 期限約束；08b 只可能減少 ACK-only |
| 4 | after 的 c 中位數 < before（只比方向） | 同一個 L2 session 交錯執行的 before，不借用其他建置；預期幅度只是 hypothesis，不放進門檻 | `segments.py` 的 tick_to_down（每個合法裁決只取第一次出現） | 重送、只帶退休的包、poke 回應都不進入；起點是 Match Tick，不受 probe 幀相位影響。L2 沒有逐跳戳記，不成立時分不出是 Match 還是 Gateway |
| 5 | `link_action_min_interval_ms` ≥33.3 或 32.3（決定 3） | 33.3：由程式推導（寫出完成的戳記 `runtime_link.go:265-266`、`action_delivery.go:352`，嚴格判定 `:188`）。32.3：沿用第 08 批 L2 判定 2（`08-client-worker-asio.md:171`），I−1 ms 的餘裕沒有推導依據 | gateway.log 的 `link_action_min_interval_ms` | 請求、重送、ACK-only 批次都經過同一個 `nextSend` 閘門，所以只能用最小值；P50 不能當成動作間隔 |
| L1 G3 | 相鄰寫出 ≥I；任一半開 1 秒 ≤30；放行後不補送 | 由程式推導：30＝floor((1 s−1 ns)/I)+1 | 測試中 `actionWritten` 的時刻 | 測試帳本混合新裁決、重送、退休、poke |
| L1 M3 | 3 秒內 action_results frame ≤91 | 由程式推導：選出到選出 ≥I；ceil(3 s/I)+1 | 測試 Gateway 收到的 action_results frame 數 | 只算 action_results frame，不含同一條 TCP 上的 snapshot 與 control |
| L1 M4 | 閒置 500 ms 的 `Iterations()` 增量 < 校準上限 | 待 L1 在 08b 之前的頭量測，寫明來源跑次；目前沒有數字，不得先寫進宣告 | `IpcHost::Iterations()` | 含每個 Tick 的 snapshot 寫出與 publish 的 pump，所以要用同一個測試校準，不能用 match.log 的 `ipc_iterations_per_s` |

## 停止條件

- 實作與 L1：需要改 wire；權威 digest 改變；`worker_main` 的斷言需要放寬；需要修改凍結的分析器；需要改 `MatchRuntimeHost`／`PvpMatch` 的公開介面，或需要改 Client、probe（範圍擴大，先停下回報）；有突變無法被 kill，或 v7-06 兩條以外出現 stale；改寫舊測試時，放寬了「本批要改的語意本身」以外的斷言；開發跑次中 D44 的重設比例升高，或舊版也出現。
- 決定 1（拿掉重複包）沒有得到使用者確認，就不開始實作。
- 實作開始前，先完成第 06、07、08 批（含 P2-log）結論的對抗式核對（使用者 2026-10-09 的補充）。

## 需要決定的事

1. **（必須先決定）「事件」的定義**
   - 建議 Q'：拿掉兩段只帶 retired 的重複包。Match lane 走 TCP，每個裁決與每次退休只送一次，不重送。Gateway 對 Client：未 ACK 的裁決照現在每 I 重送；退休前進時送一次；Client 送來只帶 ACK 且 ACK≤Gateway 已知的 retired 時，補送退休（poke）。
   - H：保留重複包，只拿掉 I/2。由程式推導，c 幾乎不會改善。
   - R：Gateway 也不主動重送未 ACK 的裁決，完全靠 Client 的重送請求。會改變現有「重送直到連續 ACK」的語意，不建議。
2. **relay 的「結果任一秒 ≤31」**：建議列為判定（判定 3），超過就停下分辨原因再交你決定。另一個選項：照 D42 只記錄，只以 Gateway 端的最短間隔判定。
3. **最短間隔的門檻**：建議結果與 link 都用由程式推導的 33.3（字面值比較）。另一個選項：link 沿用第 08 批的 32.3，並在宣告中註明「沿用的回歸門檻，不是推導值」。
4. **c 變短**：建議列為判定，clean-30 與 clean-60 只比方向。另一個選項：只記錄。
5. **L2 規模**：建議第 08 批的 4 案加 downstream-250ms ×6，共 72 次，約 36 分鐘。另一個選項：再加 network20 ×6（84 次）；network20 歷史上會出現 backlog 重設、跑次照樣通過（`adversarial/nonrespawn_resets.txt`），只能記錄次數。
6. **診斷欄位**：Gateway 統計行要不要加「收到 Match 結果的最短間隔」。建議不加：TT-2／第 10 批的日誌格式會成為 Data Contract（D45），08b 不先擴充；Match lane 的間隔由 L1 的 M3 釘住。
7. **b 的結論**：第 08 批文件的「更正」已由已完成批次的對抗式核對加上（2026-10-09）。剩下的建議：在「未結事項」記錄 probe 幀格點的相位問題（起點綁在 join Wait 之後收到 snapshot 的時刻，`gameplay_action.hpp:109`、`action_main.cpp:79-82`；超時時重新錨定，`:170`）；probe 起點隨機化留到第 09 批規劃時決定。
8. **`run_network.py` 加 `--movement-trace`**：目前 D44 型的失敗在這個驗收中無法歸因（`run_network.py:161`）。加上要改受追蹤的驗收腳本。建議在 08b 一起加（只多寫 trace，不改判定）；另一個選項是另外處理，加之前只保留 match.log 與 gateway.log。
9. **計時器鎖定**（對抗式檢查的發現）：從第 07 批的 Match 起，Gateway 的計時器只在 snapshot 轉送喚醒時觸發。建議只記錄（判定外），並列為第 09 批推導常數的輸入；原因不在 08b 調查。

## 規劃的過程與證據

- workflow（ultracode）：方案 3 位（high；最小改動、證據優先、風險優先）、評審 1 位（high）、對抗式檢查 1 位（xhigh）。以風險優先的方案為主體，取最小改動方案的「Match 走 TCP 不重送」、送出迴圈的寫法與 v7-06 突變的處理，取證據優先方案的 c 量化分析。
- 非 active 期間的結果：分析 1 位、對抗式檢查 1 位（都用 high）。對抗式檢查的提示漏傳了分析原文（workflow 腳本的疏失），所以它是從頭獨立推導；兩邊的結論一致，對抗式檢查另外補上「移除之後會發生、但沒有影響」的具體順序與「改成錯誤」不可採用的理由。證據在 `pvp-v7-08b-plan-20261009/nonactive-analysis/`、`nonactive-adversarial/`。
- D44 一節：分析 1 位（high）＋對抗式檢查 1 位（xhigh），使用者 2026-10-09 同意（「xhigh 可以，照建議配置」）。對抗式檢查撤回了分析中「Gateway 嫌疑最大」與「延遲只會讓排隊命令變少」兩句，改寫為上面的說法，並補上快照轉送、host mutex 的取鎖頻率、Client worker 的 2 ms 阻塞三個共用點，以及 network20 屬於同一條鏈。
- 規劃的對抗式檢查結論是「可行，但要先更正」。已併入本草案的修正：計時器鎖定改為記錄並列為第 09 批的輸入；D44 的停止條件改為明確定義；M3 改用長視窗計數；G8 指定上限與情境；判定 2 用字面值比較；寫明判定 3 由 judge 套用；判定 5 的門檻改為待決定；probe 相位機制加上 `gameplay_action.hpp:170`；probe 的雜湊與兩處行號；b 的結論限定在 Tick 解析度。
- 證據：`build/target/_build/test/logs/pvp-v7-08b-plan-20261009/`（git 忽略）：`minimal/`、`evidence/`、`risk/`、`judge/`、`adversarial/`，各自的 `commands.txt` 與 `sha256.txt`。
