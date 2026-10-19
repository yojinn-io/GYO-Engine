# tt2-trace：Engine 的 Trace 與產品寫檔改接（計畫，第 1 版）

- 狀態：計畫第 1 版已核准（D139，第 8 節照建議），實作中。L2 的事前宣告在實作與工具完成後另出一版，宣告前做 D45 的門檻來源對抗式檢查。
- 依據：Engine 計畫 [time-threads-trace](../../../architecture/plans/time-threads-trace/PLAN.md) 的 TT-2（範圍、驗收、Architecture Delta、停止條件）、README 的 `tt2-trace` 列、INVENTORY 的 TT2-B10、F2（`LogFile` 非同步化）。
- 檔位：high；關閉與 flush 的順序、丟棄計數、生產者與關閉的競爭（第 2 節 `trace-core` 的 `Close`／`Push`）局部 xhigh。估 14 h（README 原估 10 h，差額見第 7 節），超過約 21 h 停下回報。
- file:line 以 master `0fabb52` 為準。

## 1. 改什麼、不改什麼

TT-2 的分工照 Engine 計畫：Engine 擁有傳輸（佇列、寫檔、丟棄計數、時間戳）；產品擁有紀錄的種類、內容與 schema。本批只換傳輸，**所有檔案的位元組格式不變**；格式的版本與內容改動屬於下一批 `log-contract`。

**Engine（新）**

| 位置 | 內容 |
|---|---|
| `engine/trace` → `GYO::Trace` | 依賴 `GYO::Threads → GYO::Time → GYO::Base`，不含 SDL。一個 `TraceSink`＝一個檔案：多生產者的有界佇列、一條 `RoleThread` 寫檔執行緒（Waiter 的 Notify 喚醒，不輪詢）、丟棄計數、`Good()`、`Close()` |
| 紀錄 | `{kind, 時間基準的 ns, 預先序列化的內容行}`（Engine 計畫的定義）。序列化在生產者的執行緒；Engine 不解析內容 |
| 喚醒 | 只在佇列由空轉為非空、或達到批次門檻時 Notify（Engine 計畫）。寫檔執行緒每批寫完後 flush 一次 |
| 結束 | `Close(endLine)`：關閉之後的 Push 被拒絕並計數；寫檔執行緒寫完佇列中所有紀錄，再寫呼叫端給的結束行（產品的 `trace_end`，內含 Engine 給的計數），flush，join。結束行一定是最後一行 |
| 立即寫出 | `FlushNow()`：呼叫端阻塞到目前佇列中的紀錄都寫進檔案並 flush（給斷言與 terminate 的路徑，見第 8 節 T-3） |
| `engine/base` 的 `Log.hpp` | 等級、類別、訊息，送到可替換的 atomic handler，做法比照 `Assert.hpp:45-94` 的 `AssertionHandler`；預設輸出到 stderr |
| `SdlGpuRenderDevice` | 慢 acquire 日誌（`SdlGpuRenderDevice.cpp:950` 的 `SDL_Log`）改走 facade |

**產品（改接，位元組不變）**

| 位置 | 現在 | 之後 |
|---|---|---|
| 移動紀錄 `MovementTraceWriter.hpp` | 產品自己的 `MovementTrace`（mutex＋deque，上限 8192）＋`std::jthread` 每 20 ms 輪詢寫檔 | 一個 `TraceSink`；生產者在 `Record` 時序列化成同一行。`trace_end` 的 `events`、`dropped` 改由 Engine 的計數填。Client 與 Match 都用 |
| `LogFile`（Client、Match 的 `--log`） | `Write` 在呼叫端的執行緒寫檔並每行 flush（`LogFile.cpp:134-139`）；Tee 進來的 `std::clog`／`std::cerr` 也一樣，所以 Match 的 IPC 執行緒（`IpcHost.cpp:36`）與 Client 的主迴圈（每秒摘要行）會做檔案 I/O | 時間戳仍在呼叫端取（行的內容與順序不變），寫檔交給 `TraceSink`。檔案沒有 `trace_end`（現在也沒有） |
| 產品的斷言與 terminate 處理 | — | 安裝處理：先 `FlushNow()` 日誌，再照原本的行為結束（T-3） |

**不改**

- `match-shots.jsonl`、`match-ingress.jsonl` 的寫出：主執行緒每個統計視窗（1 秒）寫一次，紀錄在 host 的鎖下暫存（權威側的狀態）。它們不在熱路徑上，改接會碰到 host 的鎖與視窗語意；留給 `log-contract` 一起決定（第 6 節）。
- 任何檔案的格式、欄位、檔名與 schema 版本；凍結分析器。
- Go 的 Gateway（不連結 Engine）。
- `-fexperimental-library` 的移除（`cxx-flag-removal`；本批會少掉一個 `std::jthread` 的使用者，但 `std::osyncstream` 還在）。

## 2. 實作切片

| 切片 | 內容 | 檔位 |
|---|---|---|
| `log-facade` | `engine/base/include/engine/base/Log.hpp`、handler 的替換與預設 stderr；SdlGpu 改走 facade | high |
| `trace-core` | `engine/trace`：`TraceSink`、佇列、寫檔執行緒、`Close`、`FlushNow`、丟棄計數；CMake 與依賴檢查（只新增 `GYO::Trace → GYO::Threads`） | high；`Close`／`Push` 的競爭、結束行的順序、計數局部 xhigh |
| `trace-tests` | `tests/common/trace`：第 4 節的 L1 與突變 | high |
| `product-movement` | `MovementTraceWriter` 改用 `TraceSink`，刪除產品的 `MovementTrace` 佇列與輪詢 | high |
| `product-log` | `LogFile` 改用 `TraceSink`；斷言與 terminate 處理 | high |
| `docs` | Engine 的 TT 計畫（README、PLAN、HANDOFF、dev_log、`inactive_products.md` 的 TT 節，依 D12 匿名）、Trace 的 `Design Invariants`（含 `Replacement Criteria`）、本文件、HANDOFF | medium |

## 3. Trace 的 Design Invariants（草案，實作時寫進 Engine 的架構文件）

依 AGENTS.md §3，新 Component 在積上功能之前寫下約束與置換條件。放在新的 `docs/architecture/trace.md`（Engine 的架構文件；本文件不決定最終檔名，實作時若有更適合的位置再提出）。

- 生產者的 `Push` 不做檔案 I/O、不等待寫檔執行緒；佇列滿時丟棄並計數，不阻塞。
- 寫檔執行緒只由 Notify 喚醒，沒有輪詢計時器。
- 每一筆被接受的紀錄，在 `Close` 之後都已寫出；每一筆被拒絕的紀錄（佇列滿、關閉之後）都有計數。寫出數＋丟棄數＝Push 的次數。
- 結束行是檔案的最後一行；它之後不寫任何東西。
- 同一個生產者的紀錄照 Push 的順序寫出；不同生產者之間的順序是進入佇列的順序。
- Engine 不解析、不改寫內容行；不知道任何產品的 schema、事件種類或檔名。
- 時間戳用 `GYO::Time` 的時間基準。
- Replacement Criteria：(1) 有消費端需要「不丟棄」的保證（例如重播用的紀錄），佇列模型必須改成阻塞或溢出到磁碟；(2) 每加一個消費端，都要重新驗證關閉順序或計數；(3) 單一 sink 的鎖在量測中成為熱路徑的主要成本。出現任一項時，停下交使用者決定修還是重建。

## 4. 測試清單（L1）與突變

L1（CTest，CI 四平台）：

| 測試 | 斷言 |
|---|---|
| 8 個生產者同時 Push | 佇列有上限；寫出數＋丟棄數＝Push 數，丟棄數精確 |
| 喚醒 | 寫檔執行緒由紀錄喚醒（測試用的計數：等待只以 Notified 醒來，沒有 Deadline） |
| 結束 | `Close` 後檔案最後一行是結束行；`Close` 之後的 Push 被拒絕並計數 |
| 關閉競爭 | 生產者與 `Close` 同時進行時，守恆式仍成立，結束行仍是最後一行 |
| `FlushNow` | 返回時，之前 Push 的紀錄都已在檔案中 |
| 寫入失敗 | `Good()` 變成 false |
| facade | handler 的替換在多執行緒下安全；`nullptr` 回到預設 |
| 依賴圖 | 只新增 `GYO::Trace → GYO::Threads`；`GYO::Trace` 沒有 SDL 邊 |
| 產品：位元組 | 移動紀錄與 `LogFile` 對固定的事件序列，寫出的位元組和改接前的格式化結果相同（以改接前的序列化函式產生的 golden 比較） |
| 產品：既有測試 | CTest `cpu\|shader` 全部通過，期待值不改；權威 digest 與 golden 0 差異 |

突變（`scripts/run_mutations.py`，只有預期的斷言觸發才算 killed）：丟棄不計數、關閉後仍接受 Push、結束行之前不寫完佇列、喚醒改成不 Notify（等待逾時才寫，測試應以結構條件發現）、`FlushNow` 不等待、產品的移動紀錄少寫一個欄位。

## 5. L2（方向，事前宣告另出）

README 的驗收：寫檔不進熱路徑，射擊的 P99 增量 ≤1 ms。照 `a1-shot-step` 的框架（同機兩棵樹交錯、閒置閘門、sleeper、凍結清單）：

- B＝master `0fabb52`，A＝本批。兩邊都開 `--log` 與移動紀錄（改接的兩個寫檔器都在運作）。
- 主指標：合法射擊的延遲 P99（Match 端與 Client 端，沿用 `a1-shot-step` L2 的量法），A−B ≤ +1 ms。輔助：Match 的 Tick 間隔 P99、`trace_end` 的 dropped（期待 0）、凍結分析器能讀 A 的所有檔案。
- 情境與次數、停止規則、門檻來源在宣告中寫明；宣告前做 D45 檢查。
- 平台：本機 macOS（Match 與 Client）；Windows 與 Linux 的實機不在本批（CI 的 L1 涵蓋功能）。

## 6. 發現與範圍外

- `MovementTraceWriter::Finish` 先 `SetMovementTrace({})` 再停寫檔執行緒；已經取得舊指標的生產者若在最後一次 Drain 之後 Record，那筆紀錄不會寫出也不會計數（`MovementTraceWriter.hpp:39-42`、`MovementTrace.hpp:51-56`）。本批改接後由 `Close` 的拒絕計數涵蓋；屬於本批的範圍（同一段程式），不另開項目。
- 三種寫出方式（`TraceSink`、主執行緒的視窗寫出、Gateway 的 Go 寫出）在本批之後仍並存；`match-shots.jsonl`、`match-ingress.jsonl` 是否改接，在 `log-contract` 和格式一起決定。
- ~~facade 的預設輸出到 stderr，SdlGpu 的慢 acquire 行會多出現在 Client 的日誌檔~~（實作時更正：Client 原本就把 SDL 日誌轉進日誌檔，這一行已經是 `[info] SDL_GPU slow swapchain acquire …`。為了位元組不變，Client 安裝 facade 的 handler，照原樣轉給 `SDL_LogMessage`，見第 10 節）。

## 7. 工時

| 項目 | 估計 |
|---|---|
| `log-facade`＋SdlGpu | 1 h |
| `trace-core`＋`trace-tests`＋突變 | 5 h |
| `product-movement`＋`product-log`＋位元組 golden | 3 h |
| L2：工具（沿用 A1 的框架）、開發跑次、乾跑、宣告、D45 檢查、session | 3.5 h |
| `docs`（Engine 的 TT 文件、Design Invariants、本文件、HANDOFF） | 1.5 h |
| 合計 | 14 h |

README 原估 10 h 沒有包含產品寫檔器的改接與 L2 的工具；這兩項是 README 的驗收（「寫檔不進熱路徑」）所需要的。

## 8. 等你決定

- **T-1 產品改接的範圍**：(a) 本批改接移動紀錄與 `LogFile`（位元組不變），`match-shots.jsonl`、`match-ingress.jsonl` 留給 `log-contract`（建議：這兩個是熱路徑上的寫檔與輪詢，L2 的「寫檔不進熱路徑」量的就是它們）；(b) 本批只做 Engine 與 SdlGpu，產品全部在 `log-contract` 改接（L2 移到 `log-contract`）；(c) 四個寫出器全部改接。
- **T-2 紀錄的形式**：照 Engine 計畫，生產者預先序列化成一行（Engine 不知道 schema；代價是格式化在生產者的執行緒，移動紀錄每筆約 1～2 µs〔推論〕，由 L2 的射擊 P99 量）。另一個做法是 Engine 存固定大小的二進位內容加格式化函式，在寫檔執行緒格式化，介面較複雜。建議照 Engine 計畫。
- **T-3 日誌在當機時的完整性**：現在 `LogFile` 每行 flush，程序異常結束前寫的行都在檔案中；改成背景寫檔後，佇列中還沒寫出的行會遺失。建議：斷言處理與 `std::set_terminate` 先 `FlushNow()`；被訊號殺掉（SIGKILL、SIGSEGV）時遺失最後一批，接受。另一個做法：錯誤等級的行同步寫。
- **T-4 審查**：Engine 的公開介面（`TraceSink`、`Log.hpp`）與關閉順序，交 1 位 xhigh 審查 agent 檢查一次（比照 TT-1，高於主對話的檔位，需要你同意）。

## 10. 實作紀錄（2026-10-20，D139 之後）

- Engine：`GYO::Trace`（`engine/trace`）、`engine/base/include/engine/base/Log.hpp`、SdlGpu 改走 facade（等級 Info、文字不變）。Design Invariants 與 Replacement Criteria 寫在 Engine 的 [docs/architecture/trace.md](../../../architecture/trace.md)（第 3 節的草案，英文）。和 Engine 計畫的差異（紀錄只有內容行、沒有批次門檻、加 `FlushNow`、結束行可以不給）記在 Engine 的 PLAN。
- 產品：
  - `MovementTrace.hpp`：新增介面 `MovementTraceSink`；記憶體的 `MovementTrace`（測試與 probe 讀回用）實作它；時間戳改在 `TraceMovement` 取（原本在 `MovementTrace::Record`）。
  - `MovementTraceWriter.hpp`：`MovementTraceLine`（序列化移到生產者的執行緒）、`MovementTraceEndLine`（`dropped`＝佇列滿＋`Finish` 之後才到的事件）；寫檔交給 `TraceSink`，刪除 `std::jthread` 與 20 ms 輪詢。`Good()`＝沒有寫入失敗且沒有遺失。
  - `LogFile`：時間戳仍在呼叫端取，寫檔交給 `TraceSink`（上限 65536 行）；有丟棄時，檔案最後多一行 `[log] dropped_lines=N`（沒有丟棄時位元組不變）。`FlushOnCrash()`：斷言處理先把失敗寫進日誌並 `FlushNow()`，再交給原本的處理；terminate 處理先 `FlushNow()`。Client 與 Match（有 `--log` 時）都安裝。
  - Client：`Engine::Base::SetLogHandler` 轉給 `SDL_LogMessage`（`"<category> <text>"`），所以慢 acquire 行在主控台與日誌檔都和改接前相同。
  - CMake：產品的 domain 函式庫與權威 digest 的 -O0 目標連結 `GYO::Trace`。
- 測試：`tests/object_fps_pvp/DiagnosticFileTests.cpp`（移動紀錄的 3 筆事件與整個檔案，對照改接前的寫檔器在 master `0fabb52` 產生的位元組；`LogFile` 的時間戳格式、順序、append、`FlushNow`）。
- 本機：CTest `cpu|shader` 73／73（權威 digest 0 差異）；Engine 突變 5／5、產品突變 3／3（`tt2-movement-line-field`、`tt2-movement-end-uncounted`、`tt2-log-timestamp-space`）；Trace 測試重複 20 次全過、TSan 3 輪乾淨。
- 偏差：第 4 節的「寫入失敗」測試只在 Linux 跑（`/dev/full`），macOS 與 Windows 沒有可攜的做法。
- xhigh 審查（T-4，1 位）：沒有 MUST-FIX；SHOULD 6 項、NIT 6 項，處理與未處理的項目記在 Engine 的 [TT HANDOFF](../../../architecture/plans/time-threads-trace/HANDOFF.md)「TT-2 紀錄」。和產品有關的處理：測試的 UB（被 Tee 的 stream 宣告順序）；`LogFile` 每行 flush（`flushEachLine`，多個 Client 同時啟動、寫到同名日誌檔時不切開一行，和舊版相同）；移動紀錄改用 `std::string`＋`%.17g` 組行（不在生產者上建 stream，MSVC 會取全域鎖）；當機路徑、丟棄行、`Finish` 之後才到的事件補測試。修正後產品突變 7／7（新增 `tt2-log-crash-no-flush`、`tt2-log-handler-not-restored`、`tt2-log-drops-unreported`、`tt2-movement-late-uncounted`）、Engine 突變 6／6（新增 `tt2-timer-wake`）、CTest `cpu|shader` 73／73。

## 11. L2 事前宣告（第 2 版，等你決定 11.7）

第 1 版草稿經 D45 的門檻來源對抗式檢查（1 位 xhigh，2026-10-20）：MUST-FIX 5、SHOULD-FIX 4；對應見第 12 節。本版只改宣告內容；工具、開發跑次與乾跑在 11.6 決定之後做，雜湊在第 3 版寫入。

### 11.1 要回答的問題與門檻的來源

README 的驗收「寫檔不進熱路徑（射擊 P99 增量 ≤1 ms）」不能照字面量：

- 射擊延遲受 probe 幀相對 Match Tick 的相位影響，建置不同就會系統性移動，不能跨建置比較（v7 D46⑦：`plans/v7/HANDOFF.md:91`、`:441`；`pv7-checkpoint-l2.md:23`；`plans/v7/STABLE_BASELINE.md:199`）。B 與 A 是兩個建置。
- 這次多出來的成本在生產者執行緒上（每筆事件的組行＋佇列＋佇列由空變非空時的一次 Notify）。Match 上，在 host 鎖內記錄的有 Reset（`PvpMatch.cpp:435`）、Resolved（`:491`）、HostAccepted（`MatchRuntimeHost.cpp:245`）、LifecycleCancelled（`:411`）、SnapshotProduced（`:462`）、RuntimeGap（`:492`）。
- Match 統計行的 `host_lock_*_wait` 太粗：A1 session（本批同樣的 8 個情境、兩棵樹共 44 項）的 P99 全部落在第一格（250 µs），≥250 µs 的等待 0 筆；每次 step 多持鎖數十 µs 時，輸入撞上的機率約 0.24%，P99 不會動。它只能當「≥1% 的鎖等待 ≥1 ms」的粗閘門。

所以本版把「1 ms」換算到每筆事件的成本：A1 session 的移動紀錄裡，4 人房（quad clean-60）每個 Tick 在鎖內記錄的事件最多 8 筆、2 人房最多 4 筆（`pvp-v71-a1-l2-20261019/runs/quad-clean-60-r1-A/clean-60-3/match-commands.jsonl` 等）。step 因紀錄多持鎖的時間 ≤ 8 × 每筆成本；要 ≤1 ms，每筆的 P99 要 ≤125 µs。這個換算只對「每 Tick 一次持鎖」成立；1 ms 本身沿用 README，沒有另外的推導。

### 11.2 兩棵樹與項目

- B＝master `0fabb52`、A＝本批最後一個改程式的 commit（之後只改文件）。兩棵樹都先刪掉 `build/target` 再從零建置；Gateway 建置資訊 B＝`0fabb52`、A＝A commit，modified＝false；之後才寫 `artifacts.sha256`（D45 M4）。`build/acceptance` 兩棵樹相同，以凍結清單核對。B 用 `../GYO-Engine-a1-before`（已切到 `0fabb52`，要再從零建一次）。
- 交錯、閒置閘門、sleeper、受干擾規則沿用 `a1-shot-step`。
- 項目（每棵樹各 3 輪）：clean-30、clean-60、clean-144、soak-short、quad clean-60、backpressure、rapid-edge-60、rapid-edge-144。Match 每項都寫移動紀錄；probe 寫移動紀錄的是 clean-30／60／144（`gameplay_action.hpp:87`）、soak、quad（`quad_main.cpp:65`）、backpressure（`timing_main.cpp:54`）；rapid 只有 Match 的（D45 S1）。
- `LogFile` 不在 L2：凍結的 runner 不傳 `--log`，probe 不用 `LogFile`。它由 L1（`DiagnosticFileTests`）與 L3（Client 預設就寫日誌，`main.cpp`）涵蓋；README 的「寫檔不進熱路徑」對 `LogFile` 這一部分，L2 不驗證（D45 M3）。

### 11.3 每項的停止條件

| 代號 | 條件 |
|---|---|
| R1 等 | 沿用 `a1-shot-step` 的 R1、R1s、R3、R4、T1、H1、D；兩棵樹都判。Match 與 probe 在移動紀錄 `!Good()` 時本來就以失敗結束（`match_main.cpp:128`；`gameplay_action.hpp:195` 等），所以「有遺失」由 R1 停下。A 的 R1 若是遺失，回報時註明可能是停止順序（`Finish` 之後才到的事件在 A 會計數，在 B 是靜默遺失，第 6 節），不自動當成熱路徑退步（D45 S2） |
| T2-1 | 每個移動紀錄檔都以 `trace_end` 結尾，`events`＝檔中事件行數；兩棵樹都判 |

### 11.4 結束時的判定（只在完整 session 上判）

彙總單位：quad 每輪（5 份 `match.log`）、backpressure 每個 case 各取一個值，不跨輪或跨 case 合併（同 `pv7-checkpoint-l2.md` 第 5 節，D45 S3）。

| 代號 | 指標 | 規則 |
|---|---|---|
| T2-3、T2-4 | Match 的 `host_lock_input_wait`、`host_lock_step_wait` 的 P99（每個值把該單位所有統計視窗的 bins 加總） | 粗閘門：median A − median B ≤ max(range B, 1 ms)。B 的歷史值全部是 250 µs（A1 session），所以實際上是「A 的中位數 ≤ 1000 µs」；只抓得到 ≥1% 的鎖等待 ≥1 ms 這種退步 |
| T2-5 | 移動「產生→Actual」P95、P50（`a1-shot-step` 的 C1、C2） | 同 C1、C2。A1 的 range B 0.13～0.56 ms |
| T2-9 | 每筆事件的生產者成本 P99 × 8 | ≤ 1 ms（等於每筆 P99 ≤ 125 µs）。量法：A 的建置上，一支開發用程式在 sink 存活、寫檔執行緒閒置的狀態下，對 Resolved 形狀的事件呼叫 `TraceMovement` 10 萬次（每次都讓佇列由空變非空，所以每次都含 Notify），取每次耗時的 P99。只在本機 macOS 量（Windows 見 11.5） |
| T2-8 | 移動「產生→送出」P95（`send_p95_ms`，`gameplay_evidence.py:294`；clean-30／60／144、soak） | 只記錄。預測：median A − median B ≤ 0.05 ms（A1 的 B 值 0.176～0.223 ms、跑次間差 ≤0.05 ms）；超過時回報（D45 M2） |
| T2-6 | Match 的 Tick 晚醒 P99、`cpu_s`、鎖等待的 max 與 ≥250 µs 的筆數 | 只記錄。預測：≥250 µs 的筆數 A＝0 |
| T2-7 | 合法射擊的 Match 端與 Client 端延遲 P95、P99 | 只記錄（D46⑦） |

不成立時照 AGENTS.md §14.1 停下回報，不重跑、不放寬。

### 11.5 量不到的部分

- Windows（主要平台）：本機無法量；`std::ostringstream` 已從生產者路徑移除（審查 S6），每筆的 Notify 是 `SetEvent`。Windows 上的成本留給 `lan15-windows` 或之後的 LAN 場次（只記錄）。
- Client 端熱路徑：只由 T2-8（只記錄）與 L3 的手感涵蓋。

### 11.6 開發量測（不算判定，2026-10-20）

T2-9 的量法先在本機跑過一次（Intel Mac、`-O2`、`producer_bench.cpp` 在 `build/target/_build/test/logs/engine-tt2-20261020/`；同一支程式分別對 B 的標頭與 A 的標頭編譯，B、A 交錯各 2 次，兩次結果相近）：

| 每筆 `TraceMovement` 的耗時 | B（mutex＋deque，寫檔執行緒 20 ms 輪詢） | A（組行＋sink） |
|---|---|---|
| 寫檔執行緒閒置（每筆都 Notify），P50／P99／max | 0.30／1.55／51 µs | 3.9／25～28／54～63 µs |
| 8 筆一組（像 4 人房的一個 Tick），P50／P99／max | 0.17～0.20／1.4～1.6／80～84 µs | 1.8／20／66～102 µs |

- 換算：8 × P99 ≈ 0.2 ms，在 1 ms 內（每筆的上限 125 µs，A 約 25 µs）。T2-9 照這個量法會成立。
- 但生產者的成本是原本的約 10 倍：組行（字串＋三個 `%.17g`）約 1.5 µs，佇列由空變非空時的 Notify（macOS 是 `kevent`）約 2 µs，P99 的 20 µs 多半是寫檔執行緒醒來時和生產者搶 CPU 與佇列鎖。改成在寫檔執行緒組行（計畫 T-2 的另一個做法）可以把組行的 1.5 µs 移出熱路徑，Notify 仍在。
- 這不是判定（宣告還沒核准），只供 11.7 參考。

### 11.7 等你決定

- **L2-2 生產者成本**（11.6）：(a) 接受：在 1 ms 的預算內，照現在的做法（建議；量得到的影響由 L2 的 T2-5、T2-8 觀察）；(b) 改回「寫檔執行緒組行」（T-2 的另一個做法：Engine 存固定大小的內容＋格式化函式），預估約 2 h，L1 重做一輪。
- **L2-1 驗收的換算**：README 的「射擊 P99 增量 ≤1 ms」改成本節的判定（T2-1、T2-3／4 粗閘門、T2-5、T2-9，其餘只記錄），README 的 `tt2-trace` 列同步改寫（AGENTS.md §14.4：換驗收判定要你核准）。(a) 照本版（建議）；(b) 另外在 Match 的統計行加「step 持鎖時間」的直方圖，讓熱路徑有一個靈敏的判定量（統計行是日誌格式，要動 log-contract 的範圍，本批會延後約 2 h）；(c) 不做 L2，只靠 L1、T2-9 的本機量測與 L3。

## 12. D45 檢查的對應（第 1 版 → 第 2 版）

| 項目 | 指出的問題 | 對應 |
|---|---|---|
| M1 | T2-3／4 卡在第一格，對這次的成本不敏感；「最多跳一格」的說法錯 | 改成粗閘門並寫明解析度；新增 T2-9（換算到每筆成本）；鎖等待的 max 與 ≥250 µs 筆數只記錄 |
| M2 | Client 與 IPC 執行緒的生產者沒人量 | 新增 T2-8（`send_p95_ms`，只記錄，附預測）；11.5 寫明 |
| M3 | `LogFile` 在 L2 沒有被執行 | 11.2 寫明不在 L2，由 L1 與 L3 涵蓋 |
| M4 | 兩棵樹不是從零建置、Gateway 建置資訊不符、A commit 未定 | 11.2：兩棵樹都刪 `build/target` 從零建置，之後才寫 artifacts |
| M5 | 換驗收判定要你核准；1 ms 套到鎖等待沒有推導 | 11.1 寫明 1 ms 的來源與換算；11.6 列為等你決定 |
| S1 | probe 寫移動紀錄的項目列錯（rapid 沒有） | 11.2 更正 |
| S2 | dropped＝0 已由 R1 涵蓋；A 獨有的停止原因 | T2-1 改判 events；R1 的回報註明 |
| S3 | 彙總單位沒寫 | 11.4 開頭 |
| S4 | 引用寫反、漏列 | 11.1 更正 |

