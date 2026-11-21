# 事前宣告：trace（`v8-arch-gyo2-trace`）

更新：2026-11-21（修訂 0）。Owner：架構（日誌卡接著做；GYO2 主會話開的任務卡）。**狀態：設計段，等敵對式檢查後交動工前核准。**

性質：實作段第 6 組的第二個 Module 的事前宣告（AGENTS §2 的基盤 Architecture、§3 的 Architecture Delta、新 Component 的 Design Invariants 與 Replacement Criteria、§14 的判定）。日誌（`v8-arch-gyo2-logging`，PR #256 已合併 d3371355）是另一個 Module。

用詞：照日誌宣告（設計段／實作段、動工前核准／合併前核准、第 N 步）。標「**卡片照 L17 判**」的是這張卡自己的判斷，**不是使用者的話**。交使用者的只有第 13 節。

**核心一句**（動工前先寫，每一項都拿來對照）：trace 是**機制**——它告訴外面「每一層做了什麼、在哪一步壞了」，不規定別人怎麼處理（使用者 2026-11-20「異常分級只是系統告訴外部發生了什麼」、2026-11-21「日誌只是看。trace才是追問題。」）。

## 0. 根據與讀的版本

**程式**：`gyo2` bb2497b0（#256 日誌、#257 `engine/` 分層之後）。行號照這個版本。

**決定**（主會話轉來；HANDOFF 由主會話記，代號照轉來時給的）：

| 代號 | 內容 | 本頁怎麼用 |
|---|---|---|
| `v8-arch-gyo2-logging-log-is-for-reading` | 「你用序列化也行，用描述也行。日誌只是看。trace才是追問題。」 | trace 的主要用途＝追問題（第 1、5 節） |
| （本卡 2026-11-21） | 「Log 和 trace是2個Module」 | 兩個 Module（第 6 節 D5） |
| `v8-arch-gyo2-trace-direction`（本卡 2026-11-21「照這個方向做，開始」） | 1 兩個 Module、span 歸 trace；2 入口在最底層（Base 加 trace 掛點，Kernel、編排層只呼叫掛點）；3 共用 `mono_ns`、每執行緒的 span 堆疊帶父 id、標錯誤或 E0 時寫出當下的堆疊；4 以因果為主、量時間附帶、JSON lines；5 Engine 各層發 span，先做薄（Kernel 的 schedule 與 System、編排層的入口）；6 日誌先合併、trace 另寫宣告另開 PR | 本頁的方向；第 6 節各項從它推 |
| `errors-inform-not-prescribe`（使用者 2026-11-20） | 異常分級只告訴外部發生了什麼 | trace 標錯誤＝告訴外面，不改流程（TR-3） |
| （主會話 2026-11-21） | 「可以繼續」：從 bb2497b0 重開，`engine/modules/trace`，登記列寫 `dir` | 第 6 節 D8 |

**查證的證據**（主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-logging/`，git 不追蹤）：`recon-log-vs-trace.md`（Bevy、flecs 的 log 與 trace，加 Bevy #789、#836、tracing 文件、flecs #1326 的原文）、`recon-trace-b0.md`（Unity Entities 1.5.0、Netcode 7.2.0、Bevy 補充、flecs 補充、master 的 `engine/trace`；Opus 5.5／high 偵察）。

## 1. 這一批做什麼（先用遊戲講）

Match World 第 3 幀，System `MoveRacer` 回錯誤，World 中斷。今天只看得到 runner 回的一句原因（`the Product ended: …`）。有 trace 之後，trace 檔裡依序有：

```text
span RunFrame                      （編排層的入口）
  span schedule Main  world=Match  （Kernel 跑一張 schedule）
    span system MoveRacer          （Kernel 呼叫一個 System）  error="MoveRacer: the racer fell"
```

標錯誤的那一刻，trace 另寫一筆「當下的路徑」（RunFrame › Main › MoveRacer），所以就算 World 之後被丟掉、行程之後被 `std::terminate` 結束，壞在哪一步已經寫出去了。產品自己也能開 span（「這次送出花了多久」），它會接在當下那條路徑底下。

trace 沒有裝的時候，這些掛點只讀一個原子指標就回，遊戲的模擬值完全不變。

## 2. 範圍

### 2.1 這一批做的

| 代號 | 內容 |
|---|---|
| T1 | Base 的 trace 掛點（第 6 節 D1、D2）：開始一段、結束一段、在一段上標錯誤；沒有接收者時只讀一個原子指標 |
| T2 | `engine/modules/trace`（`GYO::Engine::Trace`）：接上掛點；每執行緒的 span 堆疊（span id、父 id）；計時（`Engine::Time`）；標錯誤時寫出當下路徑；輸出成紀錄（D5） |
| T3 | Engine 各層發 span（D6）：Kernel 跑一張 schedule、呼叫一個 System（錯誤在 span 結束前標上）；編排層的入口（`RunGameInit`、`RunFrame`、`RunSchedule`、`ReplaceWorld`、`InsertSystems`） |
| T4 | E0 時寫出當下路徑（D4） |
| T5 | 測試、情境、守衛、突變；文件（新頁 `target/v2/trace.zh-Hant.md`、README、dev_log、合併前核准頁） |
| T6 | 日誌批的兩件收尾（主會話 2026-11-21 轉來）：logging 的測試暫存檔改寫到建置目錄（不在 repo 根留下 `logging-test-files/`）；合併前核准頁補使用者的核准紀錄 |

### 2.2 不在這一批

| 項目 | 留到 | 為什麼 |
|---|---|---|
| 日誌每一行帶當下的 span 路徑（Bevy #25911 的目的） | 有點名的 Feature 時 | 沒有點名；兩個 Module 之間要多一條關係（RC-T4） |
| Chrome trace／Tracy／Perfetto 匯出 | 有量測需求時 | 沒有點名；JSON lines 可以之後轉 |
| 每個 stage、sync point、hook round 各一個 span | 有 Feature 要時 | 先做薄（方向第 5 點） |
| Parallel executor 下的父子（建立時明寫父） | Kernel 實作 Parallel 時（現在 LIM-2，`Declarations.hpp:119`） | 現在只有 Sequential；掛點的形狀先留位置（D3） |
| 編譯期拿掉 trace | 量到掛點是熱路徑成本時 | 三家都是編譯期移除，GYO 先用執行期開關（D7、RC-T3） |
| span 帶任意欄位（屬性） | 有 Feature 要時 | 方向是「因果為主」；先只有名字、category、錯誤文字 |
| 產品的分析用紀錄行（master 的移動 trace，F3） | — | 屬於日誌：一個 category 自己一個檔（日誌情境 S3-4），不是 span |

## 3. 查證（假設→查證→對比→推斷→總結）

使用者 2026-11-21 教的流程。前兩步已做（`recon-log-vs-trace.md`、`recon-trace-b0.md`），這裡寫結論。

- **假設**：trace 和 log 分開；Engine 的核心不依賴 trace 的實作也能發 span；壞掉時能取得「在哪一步」。
- **查證與對比**：

| 項目 | Bevy | flecs | Unity | GYO master |
|---|---|---|---|---|
| log 與 trace | 同一個門面 `tracing`，輸出端分層 | 兩組回呼（`log_`、`perf_trace_push_/pop_`） | 三條分開：ProfilerMarker、Debug、Netcode tracing | `Base::Log` 與 `TraceSink`（產品紀錄檔，不是 span）分開 |
| 核心依賴什麼 | 只依賴門面（`bevy_ecs` 的 optional `tracing`） | 只呼叫 `os_api` 回呼 | 核心直接用 `ProfilerMarker` | — |
| 父子 | 建立時明寫父（`parent: None` 或 schedule span，#25911） | 回呼沒有父、沒有執行緒 | marker 沒有父參數，由同執行緒巢狀推出（知識，未查證） | 無 |
| 錯誤與 span | 錯誤帶 System 名；panic hook 印 `SpanTrace` | 無 | `Debug.LogException`，不帶 marker；例外路徑另補 `Begin()` 維持配對 | 無 |
| 關掉的成本 | 編譯期（`cfg(feature="trace")`） | 編譯期（`FLECS_PERF_TRACE`），開了之後一次空指標判斷 | 編譯期（`ENABLE_PROFILER`） | — |
| System 身分 | span 建立一次、存起來，每次 `enter` | 每次 push 傳名字 | marker 名在 System 建立時組好 | — |

- **推斷**：
  1. 兩家（Bevy、flecs）的核心都只碰最底層的入口；Unity 的核心直接用 profiler，是因為 profiler 是引擎內建的 Plugin 等級服務（推的）。
  2. 三家的 trace 主要拿來**量效能**；「追問題」對上的是 Bevy 的 span 樹（因果）與 panic 時的 `SpanTrace`（當下路徑）。
  3. 錯誤的「在哪一步」有兩種取法：錯誤發生時 span 還在（Bevy 的 panic hook）→ 取當下路徑；span 已經結束（Bevy 的 error handler 在 span 釋放後才跑）→ 只能靠名字。GYO 的 Kernel 在 `Executor.cpp:362-366` 拿到 System 的錯誤之後、`Fail` 之前，span 還沒結束——可以取當下路徑。
  4. 父子：單執行緒時「每執行緒堆疊」就推得出父；多執行緒（Parallel）時要建立時明寫父（Bevy #25911 的教訓）。
- **總結**：假設成立。分開的 Module、Base 的掛點、標錯誤時取當下路徑，都有先例；要補的是兩點：Parallel 時父要能明寫（D3 留位置），關掉時是執行期而不是編譯期（D7 量）。

## 4. 機制還是功能

**機制**（卡片照 L17 判）：Engine 的各層與每個產品都會用到；它不認得任何產品、不改任何流程，只把「發生了什麼」交出去。所以照「想通用性」：掛點不帶產品的詞，名字與 category 由呼叫的人給；trace 不決定錯誤要不要處理、怎麼處理。

## 5. 基盤 Architecture 的根據（AGENTS §2）

### 5.1 為什麼算基盤

Base 的掛點會被 Kernel、編排層與之後每個 Module、產品呼叫；形狀改了，上面每個呼叫點都要改。trace 的紀錄格式是工具讀的資料契約。屬於基盤（「再利用可能な層の共通構造」「Data Contract の Format」）。

### 5.2 予見の根據 → 要求（Feature → Contract → Architecture）

| # | Feature（Product） | 根據 | Contract | Architecture | 本批 |
|---|---|---|---|---|---|
| F8 | 追問題：壞掉時看得出每一層做了什麼、在哪一步壞了（Game、Tool） | 1：使用者 2026-11-21 兩句（第 0 節）；2：Bevy span 樹與 `SpanTrace` | span 帶父；可在 span 上標錯誤；標錯誤與 E0 時寫出當下路徑；Engine 各層發 span | D1～D6 | 實作 |
| F4 | 量一段時間（PvP 的「這次送出花了多久」） | 1：master PvP `MovementTraceEvent::startedNs`（日誌宣告 F4）；2：Bevy span、Unity ProfilerMarker | span 帶開始與長度，單調時鐘（`Engine::Time`），和日誌共用 `mono_ns` | D5 | 實作（附帶） |
| F9 | 之後的 Parallel executor 仍能接成路徑 | 1：Kernel 規格的 Parallel（LIM-2，`Declarations.hpp:119`，計畫中）；2：Bevy #25911 | 掛點能明寫父 | D3 | 只定形狀 |

導不出的要求：每秒幾個 span、可接受的成本——沒有量測根據；判定 7 本批只量（D7）。

## 6. 設計（Contract → Architecture）

每項寫系譜、退掉的選項、決定的理由（AGENTS §4）。

### D1 掛點放哪裡

| 選項 | 系譜 | 內容 | 退掉的理由 |
|---|---|---|---|
| **(a) Base 的 trace 門面**（建議） | flecs `os_api` 的回呼；Bevy 核心只依賴門面；GYO 既有的 `Base::Log`（`engine/core/base/include/engine/base/Log.hpp:40-69`，原子函式指標） | `engine/core/base/include/engine/base/Trace.hpp`：一組原子函式指標（接收者），trace Module 裝上；Kernel、編排層、Module、產品只呼叫門面 | — |
| (b) 各層各給 hook（Kernel 給「System 前後」、編排層給「入口前後」） | Unity `ComponentSystemGroup::OnUpdateBefore/After`（`ComponentSystemGroup.cs:94-98`） | trace Module 依賴 Kernel、編排層，各自去掛 | 每加一層就多一組 hook；產品自己的 span 沒有入口；trace Module 要依賴 Kernel（底層的機制被上層綁住） |
| (c) Kernel 直接依賴 trace Module | Unity 核心直接用 profiler | — | 依賴方向反了（Kernel 在 `engine/ecs`，trace 在 `engine/modules`） |

**決定的理由**：方向第 2 點；(a) 讓 Kernel 不依賴任何 Module（和 `Base::Log` 同一個形），產品也用同一個入口。**舊形狀？** 不是（`Base::Log` 是同層的先例，不是要搬的舊東西）。

### D2 門面的形狀

- `Engine::Base::TraceSpan`（RAII）：建立時「開始一段」，解構時「結束一段」；`MarkError(text)` 在結束前標錯誤。
- 參數：`category`（例：`kernel`、`orchestration`、產品自己的）、`name`（例：schedule 名、System 名、`RunFrame`）；都是 `std::string_view`，**呼叫的人保證它活到這一段結束**（Kernel 的 schedule 名、System 名存在登記裡，跑的時候不變）。不格式化、不配置。
- 接收者（trace Module 裝）是三個函式指標：`begin(category, name, parent) → token`、`end(token)`、`error(token, text)`；全部 `noexcept`。沒有接收者時：讀一個原子指標、回一個空 token，什麼都不做。
- **卡片照 L17 判**：每次傳字串，不先登記 id。理由：Unity、Bevy 的名字是建立時組好、之後重用——GYO 的 Kernel 名字本來就在登記裡，傳 view 不複製，等價；登記 id 會讓產品多一道手續。量到字串比較是成本時改（RC-T3）。

| 選項 | 系譜 | 退掉的理由 |
|---|---|---|
| **RAII 物件＋三個回呼**（建議） | Bevy `Span::entered()` 的 guard；Unity `ProfilerMarker.Auto()` | — |
| push／pop 兩個函式 | flecs `perf_trace_push/pop` | 提前 return、例外的路上要自己補 pop（Unity 在例外路徑另補 `Begin()` 就是這個問題，`WorldUnmanaged.cs:924-946`）；RAII 讓結束一定發生 |

### D3 父子怎麼接

| 選項 | 系譜 | 內容 | 代價 |
|---|---|---|---|
| **(a) trace Module 每執行緒一個堆疊，門面的 `begin` 多一個可選的父 token**（建議） | 單執行緒：tracing 的 current span；明寫父：Bevy #25911（`function_system.rs:909-916`） | 沒給父時，用這條執行緒堆疊頂上的那一段；給了就用給的。現在 Kernel 不給（Sequential） | 門面多一個參數 |
| (b) 只用每執行緒堆疊 | Unity marker 的巢狀 | — | Kernel 實作 Parallel 時，worker 上的 System 找不到 schedule 父，要改門面（基盤的形狀變更，上面每個呼叫點都要動） |
| (c) 只用明寫的父 | Bevy 的 System span | 每個呼叫點都要拿到父 token | 產品的 span 要層層傳 token，太重 |

**決定的理由**：F9 的根據是 Kernel 規格裡計畫中的 Parallel；(a) 現在不多做事，之後不必改門面（`gyo2-mechanism-not-conservative`：建機制這一輪別選會被上層蓋住的保守形狀）。

### D4 錯誤與「在哪一步」

- **在 span 上標錯誤**：Kernel 在 `Executor.cpp:362-366`（拿到 System 的錯誤、`Fail` 之前）對 System 的 span 呼叫 `MarkError`；schedule 的 span 也標（錯誤往外傳時，外層每一段各標一次，文字是那一層的 `KernelError` 文字）。編排層的入口回錯誤時標它的 span。
- **標錯誤時寫出當下路徑**：trace Module 收到第一個 `error` 時，寫一筆「路徑」紀錄：這條執行緒堆疊從底到頂的 `category:name`，加上錯誤文字。同一條路徑往外傳時各層再標，只寫各自那一段的錯誤，不再寫整條路徑（卡片照 L17 判：路徑寫一次就夠，處理一次的精神；不是去重——是只在錯誤**第一次被看到**的地方取路徑，因為那時堆疊最深）。
  - 這不是「處理」錯誤：不改回傳值、不改流程、不吞例外（TR-3）。
- **E0（斷言、`std::terminate`）時**：

| 選項 | 系譜 | 內容 | 代價 |
|---|---|---|---|
| **(a) 門面提供「取當下路徑」，日誌的致命紀錄帶一個 `trace_path` 欄位**（建議） | Bevy 的 panic hook 印 `SpanTrace`（`bevy_log/src/lib.rs:307-316`，路徑由門面那一層持有） | Base 門面多一個接收者 `path(buffer) → 長度`（trace Module 裝；不配置，寫進呼叫的人給的緩衝）；日誌的 `WriteFatalRecord`／斷言、terminate 處理器呼叫它，有內容就加一個欄位 | 日誌多讀一個 Base 的掛點（日誌不依賴 trace Module）；E0 仍恰好一筆 `Fatal` |
| (b) trace Module 也接手斷言與 terminate 處理器，自己寫一筆路徑 | 各家 profiler 自己的 crash hook（知識） | 處理器串接 | 處理器的先後看安裝順序；兩個 Module 都接手同一組處理器，容易一筆 `Fatal` 變兩筆或漏掉 |
| (c) E0 時不寫路徑 | — | — | 斷言發生在 System 裡時看不出是哪個 System |

**決定的理由**：(a) 讓「E0 恰好一筆 `Fatal`」（日誌判定 2）不變，路徑附在那一筆上；處理器不重複接手。這要改日誌（已合併的 Module）的致命路徑——加一個欄位，日誌的 Design Invariants LG-2、LG-5 要加一句（第 8 節）。

### D5 輸出：trace 的紀錄怎麼寫出去

| 選項 | 系譜 | 內容 | 代價 |
|---|---|---|---|
| **(a) trace Module 把每段寫成日誌的一筆紀錄**（建議） | Bevy：span 與 event 進同一個 Registry，由輸出層分（fmt／chrome／tracy）；GYO 日誌已有「一個 category 自己一個檔」（日誌情境 S3-4） | 一段結束時寫一筆：category `trace.<呼叫的 category>`、text＝name、欄位 `span_id`、`parent_id`、`begin_ns`、`duration_ns`、（有時）`error`；路徑紀錄另一筆。要分檔就在日誌的 `Install` 用 `include={"trace."}` 給一個檔 | trace 依賴 logging（`trace` → `logging`，方向向下）；日誌的資料契約不變（只是 category 與欄位） |
| (b) trace 自己一套傳輸（佇列＋寫者＋檔） | master `engine/trace` 的 `TraceSink` | 和日誌的 `JsonLinesFileSink` 幾乎一樣 | 同一個傳輸寫兩份（AGENTS §9「同じ事実を手動で維持する場所は一か所」的精神）；致命路徑要再做一次 |
| (c) 把日誌的檔案寫出抽到更底層的共用 Module | — | logging 與 trace 都用它 | 動剛合併的 logging 的結構，只為一個新的使用者；沒有觀測到第三個使用者 |

**決定的理由**：使用者定的是「兩個 Module」（職責：日誌給人看、trace 追問題），不是「兩套傳輸」；(a) 讓 trace 只做自己的事（span、父子、計時、路徑），寫出交給已驗證過的日誌傳輸。分不分檔是產品的設定（`Install` 的篩選），不是 trace 的規定。**舊形狀？** (b) 是（master 的 `TraceSink`）。

**沒有裝 Logger 時**：trace 的紀錄照日誌的規則（沒裝時寫標準錯誤）；trace 裝了而 Logger 沒裝，是產品的組合，不是錯誤。

### D6 Engine 各層發哪些 span（先做薄）

| 層 | 位置（bb2497b0） | span |
|---|---|---|
| Kernel | `WorldExecutor::Run` 的一張 schedule（`Executor.cpp:173` 起，`LevelGuard` 的範圍 `:248`） | category `kernel`、name＝schedule 名；巢狀的 `RunSchedule`（K0）自然接在 System 底下 |
| Kernel | 每個 System 呼叫（`Executor.cpp:345` 的 `system.run(call)` 那一步；跳過的不開） | category `kernel`、name＝System 名；錯誤在 `:362-366` 標上 |
| 編排層 | `RunGameInit`、`RunFrame`、`RunSchedule`、`ReplaceWorld`、`InsertSystems`（`Product.cpp:464`、`:476`、`:491`、`:534`、`:737`） | category `orchestration`、name＝入口名；回錯誤時標 |

- Kernel 不讀時鐘：計時在 trace Module 的接收者裡（`kernel_guard` K3 照舊）。Kernel 不寫日誌：呼叫的是 trace 門面，不是 `Base::Log`（K6 照舊；門面的 include 要加進 K6 的准許，或另立一條規則，第 8 節）。
- 決定性：trace 不讀、不寫 World 的任何值；開不開 trace，模擬的值逐位元相同（TR-2、判定 4）。

### D7 關掉時的成本

| 選項 | 系譜 | 代價 |
|---|---|---|
| **(a) 執行期開關：沒裝接收者時讀一個原子指標**（建議） | GYO `Base::Log`；flecs 開了 `FLECS_PERF_TRACE` 之後的形 | 每個 span 一次 acquire load＋分支；建立 RAII 物件 |
| (b) 編譯期開關（巨集或 CMake 選項）＋(a) | Bevy `cfg(feature="trace")`、flecs、Unity | 多一個建置組合要測；關掉時沒有 trace 可以追——正是要追問題的時候 |

**決定的理由**：(a)。卡片的質疑（AGENTS 的「自分の見解を出す」）：三家都編譯期移除，是因為它們的 trace 是量效能的工具、可以在發行版拿掉；GYO 的 trace 主要是追問題，發行版正需要它，所以執行期開關比較對；成本由判定 7 量，超過再做 (b)（RC-T3）。

### D8 位置與名字（卡片照 L17 判：照 `logging` 的先例與命名規範）

| 項目 | 名字 |
|---|---|
| 門面 | `engine/core/base/include/engine/base/Trace.hpp`：`Engine::Base::TraceSpan`、`TraceReceiver`、`SetTraceReceiver` |
| Module | `engine/modules/trace/`、`GYO::Engine::Trace`、`gyo_trace`／`GYO::Trace`；登記列 `trace,modules/trace,GYO::Trace,…,yes,no,base;logging,time,,` |
| 安裝 | `Trace::Install(options) → Result<TraceHandle, …>`：一個行程一個，`TraceHandle` 卸下時還原接收者（同 `Logging::Install`） |

## 7. Architecture Delta（AGENTS §3 的七項）

1. **Feature**：F8（追問題）、F4（量一段時間）；F9 只定形狀。
2. **現在的問題**：World 壞掉時只有 runner 的一句原因（`WorldFailure::Text()`），看不出外層做了什麼；斷言發生在 System 裡時，致命紀錄不知道是哪個 System；產品沒有量時間的工具（span 已從 logging 拿出）。
3. **變的邊界**：Base 多一個門面（`Trace.hpp`）；Kernel 的 `Executor.cpp`、編排層的 `Product.cpp` 開始呼叫它；新 Module `engine/modules/trace`；日誌的致命路徑多讀一個 Base 掛點、多一個欄位。
4. **影響的 Module／產品／工具**：Base、Kernel、編排層、logging、新的 trace；沒有產品（gyo2 還沒有產品）；`kernel_guard`、`orchestration` 的守衛。
5. **依賴方向**：Kernel → Base（已有）；編排層 → Base（已有）；logging → Base（已有）；**新邊** trace → Base、trace → logging、trace → time。沒有向上的邊。
6. **Ownership**：「span 的父子、計時、路徑」歸 trace Module；「當下有沒有接收者」歸 Base 門面（和 `Base::Log` 同）；Kernel、編排層只擁有「我現在在做哪一步」的名字。
7. **為什麼更小的變更不行**：不動 Kernel 就沒有 Engine 這一側的路徑（方向第 5 點）；不動日誌就得讓 E0 寫兩筆或不寫路徑（D4）。

**和第 4 批（權威＋回滾）的交界**：第 4 批也改 `Executor.cpp`、`WorldExecutor.hpp`、`Product.cpp`、`Setup.hpp`（異常分級宣告第 11.2 節的列表）。本批在那裡只加 span 的呼叫（每處一兩行），後合的那一批 rebase；第 4 批新加的入口或 schedule 若要 span，照本頁的門面加。

## 8. Design Invariants（新頁 `target/v2/trace.zh-Hant.md`，見出し `Design Invariants`；最終文字在合併前核准定稿）

| 條 | 內容 | 守衛 |
|---|---|---|
| **TR-1 入口** | 發 span 的路只有 Base 的門面（`TraceSpan`）；Kernel、編排層、Module 不依賴 trace Module。 | 含入掃描（`kernel_guard`、`orchestration.guard`、trace 自己的守衛，情境測試通過後立）；`gyo_expect_links` |
| **TR-2 不改模擬的值** | trace 不讀、不寫 World 的值，不改 Kernel 的步驟；開不開 trace，模擬的值逐位元相同。Kernel 不讀時鐘（計時在 trace Module）。 | 情境的決定性案；`kernel_guard` K3 |
| **TR-3 標錯誤只是告訴** | `MarkError` 不改回傳值、不改流程、不吞例外；錯誤第一次被標的地方寫出當下路徑，往外傳時各層只標自己那一段。 | 測試 |
| **TR-4 父子** | 沒給父時，父是這條執行緒堆疊頂上的那一段；給了用給的。每段有行程內唯一的 id。結束的順序和開始相反（RAII）；例外的路上也結束。 | 測試（巢狀、例外、明寫父） |
| **TR-5 紀錄** | 每段結束寫一筆日誌紀錄：category `trace.<category>`、text＝name、欄位 `span_id`、`parent_id`、`begin_ns`、`duration_ns`（`Engine::Time`）、有錯誤時 `error`；路徑紀錄另一筆（欄位 `path`、`error`）。 | 往返測試、golden |
| **TR-6 關掉時** | 沒裝接收者時，`TraceSpan` 只讀一個原子指標、不配置、不讀時鐘。 | 配置計數測試；判定 7 |
| **TR-7 一個行程一個** | 同時只裝一個；卸下時還原接收者。 | 測試 |
| 日誌 **LG-2、LG-5 的補充** | 致命紀錄在有 trace 接收者時帶一個 `trace_path` 欄位（E0 時當下的路徑）；仍恰好一筆 `Fatal`。 | `logging.terminate.*` 與斷言案加檢查 |

### Replacement Criteria

- **RC-T1 Parallel**：Kernel 實作 Parallel 時，worker 上的 System 由 Kernel 明寫父；若門面的父參數不夠（例：要跨執行緒的 flow），停下重看 D3。
- **RC-T2 路徑不準**：觀察到錯誤被標時堆疊已經不含出錯的那一段（例：錯誤在 span 結束後才被看到）——重看 D4 的標記位置。
- **RC-T3 成本**：判定 7 量到掛點是熱路徑的主要成本，或某個點名的 Feature 超出預算——考慮登記 id 或編譯期開關。
- **RC-T4 日誌要帶路徑**：有 Feature 要每一行日誌知道自己在哪一段（Bevy #25911）——重看 D4 (a) 的掛點是否要給一般的紀錄用。
- **RC-T5 輸出要分家**：trace 需要和日誌不同的丟棄政策（例：一筆都不能丟）而日誌的傳輸做不到——重看 D5 (b)。

## 9. 測試、守衛、突變

- **測試先行**：每一步先 Red 再 Green；新測試在 `tests/common/trace/`（`gyo_trace_tests`），Kernel、編排層的 span 在各自的測試加案（只看 trace 收到的事件，用測試的接收者）。
- **情境**：`tests/common/scenario/` 加一段（段號和第 4 批對齊）：情境第 3 段的 relay 遊戲，Match 第 3 幀 System 回錯誤——trace 收到 RunFrame › schedule › System 的三段、System 段標了錯誤、路徑紀錄一筆；System 裡斷言（測試的處理器）——日誌的致命紀錄帶 `trace_path`；trace 開關兩次，遊戲的值逐位元相同。
- **守衛**（情境通過後立，AGENTS §3）：trace 的含入掃描；`kernel_guard` 的規則調整（K6 准許門面的 include，或另立「Kernel 只用 trace 門面」）——這是守衛規則的變更，照 AGENTS §14.4 交核准。
- **突變**：清單 `tests/common/trace/mutations.json`（前綴 `tr-`），每條決定至少一個（README 基準 3）；合併前完整跑一次；清單送 Fable 5.2／high 敵對式檢查。

## 10. 判定（先宣告，結果出來之後不放寬）

| # | 判定 | 怎麼算過 |
|---|---|---|
| 1 | L1 三平台 ctest 全綠 | 含 `gyo_trace_tests`、情境新段、守衛 |
| 2 | 追問題 | 情境：System 回錯誤時，trace 的紀錄有三段（入口、schedule、System）且父子相連、System 段有 `error`、路徑紀錄一筆而且只有一筆；斷言時日誌的那一筆 `Fatal` 帶 `trace_path`，仍恰好一筆 `Fatal` |
| 3 | 父子與配對 | 巢狀、例外、提前 return、明寫父的單元案：每段恰好一筆、`parent_id` 對、結束順序和開始相反 |
| 4 | 決定性 | trace 開關，情境的遊戲值逐位元相同 |
| 5 | 紀錄格式 | 每筆 `Parse` 讀回；欄位照 TR-5 |
| 6 | 突變 | 全部 killed（只算預期的失敗）；敵對式檢查加的也 killed |
| 7 | 成本（只量、不設門檻） | 本機 Release：沒裝接收者時 `TraceSpan` 一次的成本；裝了時一段（開始＋結束＋一筆紀錄進佇列）的成本；Kernel 跑一張有 N 個 System 的 schedule，trace 關與開的差。方法照日誌判定 7（暖機、5 輪、中位數的中位數） |

## 11. 實作的步驟（動工前核准之後）

上段（公開介面、資料格式、Design Invariants）。全程 high、不排 xhigh；碰到 §14 停止條件先停下回報。

| 步 | 內容 | 實作 | 審查 | 估計 |
|---|---|---|---|---|
| 第 1 步 | Base 門面 `Trace.hpp`（RAII、三個接收者、沒裝時的路）；單元測試 | Opus 5.5／high | — | 2～3 h |
| 第 2 步 | trace Module：安裝與卸下、每執行緒堆疊、id、計時、寫成日誌紀錄、路徑紀錄、明寫父 | Opus 5.5／high | — | 4～6 h |
| 第 3 步 | Kernel、編排層發 span（D6）；守衛規則的調整 | Opus 5.5／high | — | 3～4 h |
| 第 4 步 | 日誌的致命紀錄帶 `trace_path`（D4 (a)）；日誌批收尾（T6） | Opus 5.5／high | — | 2～3 h |
| 第 5 步 | 情境、判定 7 的量測、含入掃描守衛、突變清單 | Opus 5.5／high | 敵對式檢查 Fable 5.2／high | 4～6 h |
| 第 6 步 | 審查、文件、完整跑突變 | Opus 5.5／high | 審查 Fable 5.2／high | 3～4 h |

**時間估計**：設計段 3～5 h；實作段 18～26 h。超過 1.5 倍或單一問題累計超過 2 小時，停下回報。

## 12. 範圍的限界

- 沒有產品：情境用測試的遊戲。
- 只有 Sequential executor：Parallel 下的父子只定形狀（F9）。
- 只量成本、不設門檻（判定 7）。

## 13. 交使用者的（動工前核准）

1. **D1 掛點在 Base**（建議 (a)）：和 `Base::Log` 同一個形，Kernel 不依賴任何 Module。
2. **D3 門面多一個可選的父參數**（建議 (a)）：為之後的 Parallel 留位置，現在不多做事。
3. **D4 E0 時的路徑**（建議 (a)）：日誌的致命紀錄帶 `trace_path`，要改已合併的日誌（LG-2、LG-5 加一句）。
4. **D5 輸出走日誌**（建議 (a)）：trace → logging 的依賴；分不分檔是產品在日誌 `Install` 的設定。和「兩個 Module」的關係：職責分兩個 Module，傳輸共用一套。
5. **D7 執行期開關、成本只量**（建議 (a)）：和三家的「編譯期拿掉」不同，理由是 GYO 的 trace 在發行版也要能追問題。
6. **`kernel_guard` 的規則調整**（第 9 節）：讓 Kernel 能 include 門面。

## 14. 停止條件

- AGENTS §14 冒頭的三條。
- 實作中發現 D4 的標記位置拿不到出錯的那一段（RC-T2），或 Kernel 的 span 會改變步驟或決定性（TR-2）——停下回報。
- 第 4 批先合併而 `Executor.cpp`、`Product.cpp` 的結構大改——停下重看 D6 的位置。
