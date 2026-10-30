# pv8-design 第 2 輪：協議壓力點的通道設計

更新：2026-10-30。Owner：`object_fps_pvp`。**狀態：6 項已決定（全部照建議，`pv8-round2-picks`）；已寫進 protocol-v8 並定稿（`pv8-final-approve-round2`）。下一步是 pv8-line 的 A 層。** 起因：`pv8-scope-reopen`、`pv8-design-round2-ultracode`（[HANDOFF](HANDOFF.md)）。需求用典型值（[pv8-requirements](pv8-requirements.md)）。

## 過程

- ultracode（run `wf_0b9d1bf5-4aa`，約 23 分，7／7 個 agent 回傳）：2 位唯讀偵察（Sonnet 5.5，medium：現有通道與送達、欄位的實際值域）→ 3 個方案（Opus 5.5：擴充現有、通道分離、風險與權威優先）→ 1 位審查（Opus 5.5）→ 1 次對抗式檢查（Sonnet 5.5）。
- 評分（滿分 30）：風險與權威優先 27、擴充現有 23、通道分離 16。以「風險與權威優先」為基底，移植「擴充現有」的事件與動作形狀，以及「通道分離」的 stale 分 Type 與守衛形狀。
- **全部是設計，沒有實作；大小都是 varint 算術的推論**，要等實測。
- agent 的原始輸出留在會話的工作檔，不放進 repo。

## 結論：分三層

### A 層：現在上 wire（和 `v8-product-crouch` 同一條 pv8-line、同一個 PR）

- 第 1 輪的蹲下與衝刺欄位（號碼與語意不變）。
- **流水號縮窄**：tick、player_id、epoch、life、命令序號、action_id 等約 69 個欄位，uint64 → uint32。
  - varint 的實際值不變，實際流量不變，`AuthorityDigest` 不變；只有帳面最大值變小。
  - 4 人 snapshot：契約最大約 1101 → 740 B，型別最大約 1175 → 822 B，**餘裕約 378 B**（推論）。
  - 保留 u64：`view_tick_q16`（×65536 之後 u32 只到約 18 分鐘）、時鐘樣本的微秒值、`arena_digest`、只增不減的診斷計數。
  - 用盡上限統一成一個產品常數 `WireCounterMax`；C++ 內部型別維持 uint64，在編碼邊界加 Assert。
- 刪除沒有讀者的 `Welcome.match_id`（Gateway 寫死 1，Client 不讀），號碼保留為 reserved。
- B／C 層的欄位號在 proto 裡以 reserved 預留，防止之後撞號。
- Gateway 不加狀態；Engine 與 `services/gyo_gateway` 都不改。

### B 層：設計定稿，等支撐的功能批開工才上 wire

- **可靠事件串流**（Match→Client）：搭在 ActionResults 框架上，多一個 `repeated events`（事件號 u32，每位玩家一條，由 Match 配發與重送）；ACK 搭在 `ActionBatch.events_acknowledged_through`。
  - 外送箱放在 `MatchRuntimeHost`（sim 與 IPC 兩條執行緒的交會點，和現有的動作 ACK 同一處），重送排程併進 `IpcHost::ActionLane`；Gateway 用既有的 results slot，不加第二個槽。
  - 溢位時以新的逐出原因 `EVICTION_EVENT_BACKLOG` 逐出（上限 256，要實測）。
- **對局狀態**：`MatchStatus`（階段、計時、分數）與 `PlayerState.team` 放進 snapshot（低頻、latest-wins）；`RoomRules` 經 Ready→Welcome 下發。
- **動作種類**：ShotRequest 加 `oneof detail`（撿起、丟下、使用道具、技能、互動、整理背包）；非射擊動作在交接後的下一個 Tick 裁決，避免視窗 32 的行頭阻塞。
- 支撐的功能：事件與對局狀態是 ⑤ 的 `v8-product-match-rules`（擊殺訊息、計分、隊伍）；動作種類是第一個非射擊動作的功能。

### C 層：只記形狀，列決策點

地上物品與容器、背包與裝備、Boss 與小怪、技能與狀態效果、投射物、任務、防透視、遠處降頻、8 人房。這些需要每位玩家一份的相關集合或私人內容，v8 沒有排定對應的功能。形狀記在 agent 的輸出中，開工時再拿出來。

## 分批（草案）

| 代號 | 內容 | 檔位｜引擎 | 估時 | 依賴 |
|---|---|---|---|---|
| `pv8-design-round2-approve` | 你決定下面的事，主會話寫進 HANDOFF 與 protocol-v8 | medium｜Opus 5.5 | 1～2 h | 本文件 |
| `pv8-obs-bytes` | 不改 wire 的診斷：每種訊息的送出位元組分布；Client 的 stale 丟棄依 Type 分開計數（量 ActionResults 被 snapshot 超車的比例） | medium｜Opus 5.5 | 3～5 h | 無，可並行 |
| `pv8-rename` | proto、Go 套件、CMake target 改名為 v8，版本值仍是 7 | low｜Sonnet 5.5 | 1～2 h | 核准 |
| `pv8-seq-ceiling` | 不改 wire：`WireCounterMax` 取代所有 uint64 上限檢查；各用盡邊界的測試；事前宣告 35 個情境 0 差分 | high｜Opus 5.5 | 4～6 h | 核准；`v8-product-crouch` 的 commit 1 之後（同一批檔案） |
| `pv8-seq-narrow` | wire：縮窄型別、刪 match_id、預留號碼、重釘大小、protoreflect 覆蓋、凍結探針在 L1 實跑 | high（大小釘局部 xhigh）｜Opus 5.5 | 6～10 h | `pv8-rename`、`pv8-seq-ceiling` |
| `v8-product-crouch`（wire 接入） | 第 1 輪的 4 個 bool、4 個參數、版本 7→8、DigestVersion 2 | high（局部 xhigh）｜Opus 5.5 | 既有估時 | `pv8-rename`、commit 1 |
| `pv8-contract-round2` | 改寫 protocol-v8：序號表、§6 新大小、B 層完整條文與 C 層形狀的擴充附錄；修正 protocol-v7 §3 的 Hello 間隔（文件寫 1000 ms，程式 `ClientConnection.cpp:429` 是 250 ms） | medium＋xhigh（送達語意）｜Opus 5.5，對抗 Sonnet 5.5 | 4～6 h | 核准 |
| `pv8-event-stream`（B，⑤ 開工時） | 事件串流本體與 Design Invariants、Replacement Criteria | xhigh｜Opus 5.5，契約 ultracode（先問） | 20～30 h | pv8-line 合併、⑤ 開工 |
| `pv8-match-status`（B，⑤） | MatchStatus、team、RoomRules（Match 的規則資料） | high｜Opus 5.5 | 10～14 h | 事件串流的契約核准 |
| `pv8-action-body`（B，第一個非射擊動作） | oneof detail、新的 ActionKind 與拒收原因、Gateway 的單則檢查 | high（契約 ultracode，先問）｜Opus 5.5 | 12～16 h | 那個功能排進計畫 |

## 對抗式檢查的處置（主會話的判讀）

| # | 嚴重度 | 指出的問題 | 主會話的判讀 |
|---|---|---|---|
| 1 | must | u32 縮窄後，protobuf 對超過 32 位元的 varint 會無聲截斷，接收端（Gateway、Match）沒有保護 | 降為 should。惡意或錯誤的 Client 本來就能直接送任何 u32 值，截斷後得到的也只是一個 u32 值，不會多出新的攻擊面；Match 對這些值本來就做範圍與一致性的檢查。混版由版本值 7→8 嚴格拒收。仍建議加一個測試：送入 2^32＋1 時行為和送入截斷後的值一樣（不 crash、照規則拒收或接受） |
| 2 | must | B 層「Gateway 不加狀態」對事件的可靠性過於樂觀：results slot 是 latest-wins，下行共用傳輸序號，stale 一律丟 | 接受，列為 B 層上線的前置條件：stale 依 Type 區分、槽被取代時事件由 Match 下一輪重送，並寫出最壞延遲。A 層不受影響 |
| 3 | must | 事件串流在 Resync、epoch、link 重建、短線重連時的語意沒有定義（Client 的 next_expected 何時重置） | 接受，列為 B 層的前置條件：Welcome 帶串流起點或 generation，兩條路徑各有測試；做不到就維持只留形狀 |
| 4 | must | 事件 ACK 的驗證（不超過已配發值、不倒退、和動作退役互不阻塞）沒有逐條寫出 | 接受，寫進 `pv8-event-stream` 的契約 |
| 5 | should | 型別最大 822 B 的推導（減 8 個長度前綴）沒有證據 | 接受：`pv8-seq-narrow` 用 `proto.Size` 實測釘住，文件改用實測值 |
| 6 | should | 凍結探針的 sha256 與版本值 7→8：`backpressure_probe.py` 檢查開頭位元組，版本變了就要改 | 接受：`pv8-seq-narrow` 前先查清楚哪些探針屬凍結群組；凍結工具要改時照 AGENTS §14.4 說明 |
| 7 | should | `WireCounterMax` 統一了常數，但各處的用盡行為（拋例外、重設、報錯、凍結）不同 | 接受：只統一常數，行為照原樣，各自有測試 |
| 8 | should | 先在 proto 預留 B／C 的號碼，等於把沒有功能支撐的設計寫進契約 | 列為你的決定（`pv8-reserve-numbers`）：預留只防撞號，不產生行為；不預留就等功能批再配號 |
| 9 | should | 現有 CombatState（彈匣、換彈）已對全員廣播；B 層的新事件沒有規定可見性 | 接受：B 層每種事件都要寫可見性（全員、本隊、本人）並有測試 |
| 10 | should | 非射擊動作「下一個 Tick 裁決」和射擊判定的 Design Invariants（E、S）的關係沒釐清 | 接受，寫進 `pv8-action-body` 的契約 |
| 11 | note | `pv8-seq-narrow` 與 crouch 的 wire 接入會在同三處大小釘上衝突 | 接受：同一個 PR 內依序做，後落地的一批重釘 |

## 等你決定

| # | 問題 | 選項 | 建議 |
|---|---|---|---|
| `pv8-version-split` | pv8-line 只上 A 層；B 層在 ⑤ 時上 wire，算 pv8 的延續還是另升 pv9？ | (a) A 現在合併，B 在 ⑤ 時升 pv9；(b) A 現在合併，B 在 ⑤ 時以 pv8 追加，pv8 到那時才收口；(c) A＋B 一起在 pv8-line 合併，crouch 的 wire 接入要等事件串流做完 | (a)。注意：這等於部分收回 `pv8-scope-reopen`（「一起評估」做完了，但上 wire 的只有 A）；(b) 會讓同一個版本號對應兩種 wire |
| `pv8-req-v8-scope` | v8 一定要做的，是否只有 ⑤ 的對局規則？撿物品、背包、Boss、技能、任務是否只留形狀？（問卷 10-2） | (a) 照假設；(b) 指定幾項進 v8；(c) 連 ⑤ 也只留形狀 | (a) |
| `pv8-seq-cpp-types` | C++ 內部的 PlayerId、ActionId、tick 要不要一起改成 uint32？ | (a) 一起改，範圍大，會和 crouch 衝突；(b) 維持 u64，用 `WireCounterMax` 加編碼 Assert | (b) |
| `pv8-reserve-numbers` | B／C 層的欄位號現在要不要在 proto 預留（reserved）？ | (a) 預留；(b) 不預留，功能批再配號 | (a)：只防撞號，不產生行為 |
| `pv8-obs-bytes-now` | 要不要現在就做 `pv8-obs-bytes`（實際位元組與超車比例的觀測，不改 wire）？ | (a) 現在做，和 pv8-line 並行；(b) 等 ⑤ | (a)：B 層的設計要用實測值 |
| `pv8-hello-doc-mismatch` | protocol-v7 §3 的 Hello 間隔以哪個為準？ | (a) 以程式的 250 ms 為準，修正文件；(b) 先查清楚是哪一版改的 | (a) |

B 層與 C 層的細節決定（事件外送箱的位置、溢位處理、私人狀態的模型、受擊欄位要不要移走、ShotRequest 的改名、防透視的時機），到對應的批次開工時再問。
