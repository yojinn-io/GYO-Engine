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

## 11. L2 事前宣告（第 1 版草稿，D45 檢查前）

### 11.1 要回答的問題與門檻的來源

README 的驗收寫「寫檔不進熱路徑（射擊 P99 增量 ≤1 ms）」。實作後確認兩件事，所以本宣告不直接用射擊延遲判定：

- 合法射擊的延遲受 probe 幀相對 Match Tick 的相位影響，建置不同時相位會系統性移動，不能跨建置比較（v7 D46⑦，`pv7-checkpoint-l2.md` 第 2 節）。B 與 A 是兩個建置。
- 改接後多出來的成本在生產者的執行緒上：移動紀錄的格式化（原本在寫檔執行緒）與佇列的 Notify。Match 上，`Resolved`、`Reset`（`PvpMatch.cpp:435`、`:491`）在 step 持有 host 鎖時記錄，`HostAccepted`（`MatchRuntimeHost.cpp:245`）在輸入路徑持鎖時記錄；所以「熱路徑變慢」在 Match 上直接表現為 host 鎖的等待（`host_lock_input_wait`：IPC 送輸入時等 step 放鎖；`host_lock_step_wait`：step 等輸入放鎖），這兩個量在 Match 的統計行裡，和 probe 相位無關。
- 「1 ms」沿用 README 的增量上限；比較規則沿用 `a1-shot-step` 的 C1、C2：median A − median B ≤ max(range B, 1 ms)。

### 11.2 兩棵樹與項目

- B＝master `0fabb52`（`../GYO-Engine-a1-before` 切到 `0fabb52` 從零建置；pv7、A1 之後）。A＝主 checkout（本批最後一個改程式的 commit；之後只改文件）。兩棵樹的 Match、Gateway、probe、arena 寫進 `artifacts.sha256`；`build/acceptance` 兩棵樹相同，以凍結清單核對。
- 交錯（奇數輪 B 先、偶數輪 A 先）、閒置閘門、sleeper、受干擾規則沿用 `a1-shot-step`。
- 項目（每棵樹各 3 輪）：clean-30、clean-60、clean-144、soak-short、quad clean-60、backpressure、rapid-edge-60、rapid-edge-144。所有項目的 Match 都開移動紀錄；action、quad、rapid 的 probe 也寫自己的移動紀錄（`clients-commands.jsonl`）。估 48 項、約 45 分鐘（工具完成後以 `--list-only` 實算）。

### 11.3 每項的停止條件

| 代號 | 條件 |
|---|---|
| R1 等 | 沿用 `a1-shot-step` 的 R1、R1s、R3、R4、T1、H1、D（runner 判定、執行檔身分、受干擾）；兩棵樹都判 |
| T2-1 | 每個移動紀錄檔（Match 的 `match-commands.jsonl`、probe 的 `clients-commands.jsonl`）都以 `trace_end` 結尾，`dropped`＝0，`events`＝檔案中事件行的數目；兩棵樹都判 |
| T2-2 | A 的移動紀錄可由凍結分析器讀取（runner 的判定本來就讀它；判定失敗就是 R1） |

### 11.4 結束時的判定（只在完整 session 上判）

| 代號 | 指標 | 規則 |
|---|---|---|
| T2-3 | Match 的 `host_lock_input_wait` P99（每項把所有統計視窗的 bins 加總後取 P99 的上界） | 每個情境：median A − median B ≤ max(range B, 1 ms) |
| T2-4 | Match 的 `host_lock_step_wait` P99（同上） | 同 T2-3 |
| T2-5 | 移動「產生→Actual」P95、P50（`a1-shot-step` 的 C1、C2） | 同 C1、C2 |
| T2-6 | Match 的 Tick 晚醒 P99（`tick_late` 的 bins 加總） | 只記錄（Waiter 量的是醒來時刻，對 step 的工作量不敏感） |
| T2-7 | 合法射擊的 Match 端與 Client 端延遲 P95、P99 | 只記錄（D46⑦） |

- 解析度：鎖等待與晚醒的 bins 是 `LateWakeStats` 的固定邊界（250 µs、500 µs、1 ms、2 ms、4 ms…），P99 只能落在這些值上。T2-3、T2-4 的「1 ms」因此等於「P99 最多往上跳一格（在 1 ms 以下的格子之間，或 1 ms→2 ms）」；另外只記錄每個情境 ≥250 µs 的等待比例，作為較細的參考。

不成立時照 AGENTS.md §14.1 停下回報，不重跑、不放寬。

