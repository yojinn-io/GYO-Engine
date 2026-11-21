# 事前宣告：trace（`v8-arch-gyo2-trace`）

更新：2026-11-21（修訂 1）。Owner：架構（日誌卡接著做；GYO2 主會話開的任務卡）。**狀態：設計段；修訂 0（513d1c67）經 Fable 5.2／high 敵對式檢查（必改 6、建議改 7、小問題 7），修訂 1 全部照改（處理對照在第 15 節），交動工前核准。**

性質：實作段第 6 組的第二個 Module 的事前宣告（AGENTS §2 的基盤 Architecture、§3 的 Architecture Delta、新 Component 的 Design Invariants 與 Replacement Criteria、§14 的判定）。日誌（`v8-arch-gyo2-logging`，PR #256 已合併 d3371355）是另一個 Module。

用詞：照日誌宣告（設計段／實作段、動工前核准／合併前核准、第 N 步）。標「**卡片照 L17 判**」的是這張卡自己的判斷，**不是使用者的話**。交使用者的只有第 13 節。

**核心一句**（每一項都拿來對照）：trace 是**機制**——它告訴外面「每一層做了什麼、在哪一步壞了」，不規定別人怎麼處理（使用者 2026-11-20「異常分級只是系統告訴外部發生了什麼」、2026-11-21「日誌只是看。trace才是追問題。」）。

## 0. 根據與讀的版本

**程式**：`gyo2` bb2497b0（#256 日誌、#257 `engine/` 分層之後）。行號照這個版本。

**決定**（主會話轉來；HANDOFF 由主會話記，代號照轉來時給的）：

| 代號 | 內容 | 本頁怎麼用 |
|---|---|---|
| `v8-arch-gyo2-logging-log-is-for-reading` | 「你用序列化也行，用描述也行。日誌只是看。trace才是追問題。」 | trace 的主要用途＝追問題 |
| （本卡 2026-11-21） | 「Log 和 trace是2個Module」——推翻日誌宣告第 13 節第 8 題的「一個系統」 | 兩個 Module；D5 寫明留下的是什麼 |
| `v8-arch-gyo2-trace-direction`（本卡 2026-11-21「照這個方向做，開始」） | 1 兩個 Module、span 歸 trace；2 入口在最底層（Base 加 trace 掛點，Kernel、編排層只呼叫）；3 共用 `mono_ns`、每執行緒的 span 堆疊帶父 id、標錯誤或 E0 時寫出當下的堆疊；4 因果為主、量時間附帶、JSON lines；5 Engine 各層發 span，先做薄；6 日誌先合併、trace 另寫宣告另開 PR | 本頁的方向 |
| `errors-inform-not-prescribe`（使用者 2026-11-20） | 異常分級只告訴外部發生了什麼 | 標錯誤＝告訴，不改流程（TR-3） |
| （主會話 2026-11-21） | 「可以繼續」：從 bb2497b0 重開，`engine/modules/trace`，登記列寫 `dir` | D8 |

**查證的證據**（主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-logging/`，git 不追蹤）：`recon-log-vs-trace.md`、`recon-trace-b0.md`（偵察）、`trace-declaration-r0-adversarial-check.md`（修訂 0 的檢查）。

## 1. 這一批做什麼（先用遊戲講）

relay 遊戲有 Lobby、Match 兩個 World。Match 第 3 幀，System `MoveRacer` 回錯誤，World 中斷。今天只看得到 runner 回的一句原因。有 trace 之後，日誌裡（產品把 `span.` 分到 trace 檔時就在那個檔裡）依序有：

```text
span orchestration RunFrame
  span kernel Main      world=Lobby                     （正常結束）
  span kernel Main      world=Match
    span kernel MoveRacer world=Match   error="MoveRacer: the racer fell"
path  orchestration:RunFrame › kernel:Main@Match › kernel:MoveRacer@Match   error="…"
```

錯誤在 Kernel 拿到的那一刻標上，trace 把當下的路徑交給日誌——交給日誌的佇列，不是已經到磁碟（第 12 節）。System 丟例外時也一樣：被例外展開而結束的那幾段自己標「例外離開」。斷言發生在 System 裡時，日誌的那一筆 `Fatal` 帶著當下的路徑。

trace 沒有裝、或沒有輸出端收 `span.` 的時候，掛點只維持路徑、不讀時鐘、不寫紀錄；遊戲的模擬值完全不變。

## 2. 範圍

### 2.1 這一批做的

| 代號 | 內容 |
|---|---|
| T1 | Base 的 trace 門面（D1、D2）：`TraceSpan`（可延後開始的 RAII）、標錯誤、標「當下那一段」、取當下路徑；沒裝時讀一個原子指標 |
| T2 | `engine/modules/trace`：裝上接收者；每執行緒的 span 堆疊（id、父 id、世代）；計時；寫成日誌紀錄；路徑紀錄（D3～D5） |
| T3 | Engine 各層發 span（D6）：Kernel 的 schedule（`RunPlan`）、System、`TriggerFromHost`；`Fail` 標當下那一段；編排層五個入口 |
| T4 | E0 時日誌的致命紀錄帶 `trace_path`（D4，改已合併的 logging） |
| T5 | 測試、情境、守衛（含 `kernel_symbols` 的准許清單）、突變；文件 |
| T6 | 日誌批的收尾：logging 的測試暫存檔改寫到建置目錄（`FileSinkTests.cpp:64`、`AppendTests.cpp:26` 現在寫在工作目錄）；合併前核准頁補使用者的核准紀錄 |

### 2.2 不在這一批

| 項目 | 留到 | 為什麼 |
|---|---|---|
| 日誌每一行帶當下路徑（Bevy #25911 的目的） | 有點名的 Feature 時 | 沒點名（RC-T4） |
| Chrome trace／Tracy／Perfetto 匯出 | 有量測需求時 | 沒點名 |
| 每個 stage、sync point、hook round 各一段 | 有 Feature 要時 | 先做薄；它們的錯誤落在 schedule 那一段（D6） |
| Parallel executor 下的明寫父 | Kernel 解除 LIM-2 時 | 只有根據 2（D3）；之後加一個有預設值的參數，不改既有呼叫 |
| 編譯期拿掉 trace | 量到掛點是熱路徑成本時 | D7、RC-T3 |
| span 帶任意欄位 | 有 Feature 要時 | 只有名字、category、World 名、錯誤文字 |
| 回滾重跑的段和第一次分開 | 第 4 批 | 第 4 批的事（第 7 節交界） |
| 產品的分析用紀錄行（master 的移動 trace，F3） | — | 屬於日誌（日誌情境 S3-4）；用別的 category，不和 `span.` 撞 |

## 3. 查證（假設→查證→對比→推斷→總結）

- **假設**：trace 和 log 分開；Engine 的核心不依賴 trace 的實作也能發 span；壞掉時能取得「在哪一步」。
- **查證與對比**：

| 項目 | Bevy | flecs | Unity | GYO master |
|---|---|---|---|---|
| log 與 trace | 同一個門面 `tracing`，兩種輸出層並排（fmt／chrome、tracy） | 兩組回呼 | 三條分開：ProfilerMarker、Debug、Netcode tracing | `Base::Log` 與 `TraceSink`（產品紀錄檔，不是 span） |
| 核心依賴什麼 | 只依賴門面 | 只呼叫 `os_api` 回呼 | 核心直接用 `ProfilerMarker` | — |
| 父子 | 建立時明寫（`parent: None` 或 schedule span，#25911） | 回呼沒有父、沒有執行緒 | marker 沒有父參數（知識，未查證） | 無 |
| 錯誤與 span | 錯誤帶 System 名；panic hook 印 `SpanTrace` | 無 | `Debug.LogException`；例外路徑另補 `Begin()`（偵察標「推的」：為了讓外層的 Begin／End 對上） | 無 |
| 關掉的成本 | 編譯期 | 編譯期；開了之後一次空指標判斷 | 編譯期 | — |
| System 身分 | span 建立一次、存起來 | 每次 push 傳名字 | marker 名在建立時組好 | — |

- **推斷**：
  1. Bevy、flecs 的核心都只碰最底層的入口。
  2. 三家的 trace 主要量效能；「追問題」對上的是 Bevy 的 span 樹與 panic 時的 `SpanTrace`。
  3. 「在哪一步」要在 span 還沒結束時取。GYO 的 Kernel 把一次執行裡的錯誤都交給 `WorldExecutor::Fail`（`Executor.cpp:79`）；System 那一步回 `Fail` 時（`:366`）那一步的範圍還沒結束——只要 System 的段的生命期涵蓋到 `return Fail(...)`，就取得到（D2 的「延後開始」）。例外離開時，段會被展開結束，要自己標（D4）。
  4. 單執行緒時「每執行緒堆疊」推得出父；多執行緒時要明寫父（#25911）。
- **總結**：假設成立。要注意兩點：例外的路（段被展開結束），和 Parallel 時的父（現在只有 Sequential，D3 照 AGENTS §2 延後）。

## 4. 機制還是功能

**機制**（卡片照 L17 判）：Engine 的各層與每個產品都會用到；它不認得任何產品、不改任何流程，只把「發生了什麼」交出去。掛點不帶產品的詞，名字、category、World 名由呼叫的人給；trace 不決定錯誤要不要處理、怎麼處理。

## 5. 基盤 Architecture 的根據（AGENTS §2）

### 5.1 為什麼算基盤

Base 的門面會被 Kernel、編排層與之後每個 Module、產品呼叫；形狀改了，上面每個呼叫點都要改。trace 的紀錄格式是工具讀的資料契約。

### 5.2 予見の根據 → 要求

| # | Feature（Product） | 根據 | Contract | Architecture | 本批 |
|---|---|---|---|---|---|
| F8 | 追問題：壞掉時看得出每一層做了什麼、在哪一步壞了（Game、Tool） | 1：使用者 2026-11-21 兩句；2：Bevy span 樹與 `SpanTrace` | span 帶父；標錯誤；錯誤與 E0 時交出當下路徑；Engine 各層發 span；例外的路也標 | D1～D6 | 實作 |
| F4 | 量一段時間（PvP 的「這次送出花了多久」） | 1：master PvP `MovementTraceEvent::startedNs`（日誌宣告 F4） | span 帶開始與長度，`Engine::Time`，和日誌共用 `mono_ns` | D5 | 實作（附帶） |

**規模的估計**（根據 3，S4 的指正：修訂 0 說「導不出」不對）：一幀的段數 ≈（1＋System 數）×（一幀的步數）×（1＋回滾重跑次數 k）＋入口數。例：20 個 System、固定步一幀 4 次（`WorldTimeModule.cpp:70` 一幀可多次）、k＝8（回滾份 `rollback-module-declaration.zh-Hant.md:107`、`:152`：一個獨占 System 巢狀跑 k 次）→ 約 21×4×9＝756 段。日誌一筆進佇列中位數 3.2 µs（RC-L3）→ 全部寫出約 2.4 ms／幀；沒有輸出端收 `span.` 時，每段只剩原子讀取、堆疊推入推出（估數十 ns）→ 約 40 µs／幀。判定 7 量真實的數字。

**不再列**：修訂 0 的 F9（Parallel）。LIM-2 是限制，解除條件是量到超預算＋逐位元等價的守衛（`limits.zh-Hant.md:12`、`scheduler.zh-Hant.md:52`），不是計畫中的 Feature；只剩根據 2，照 AGENTS §2 延後（D3）。

## 6. 設計（Contract → Architecture）

每項寫系譜、退掉的選項、決定的理由（AGENTS §4）。

### D1 掛點放哪裡

| 選項 | 系譜 | 內容 | 退掉的理由 |
|---|---|---|---|
| **(a) Base 的 trace 門面**（建議） | flecs `os_api` 回呼；Bevy 核心只依賴門面；GYO `Base::Log`（`Log.hpp:40-69`） | `engine/core/base/include/engine/base/Trace.hpp`：**一個**原子指標，指向接收者（一組函式指標）；trace Module 裝上 | — |
| (b) 各層各給 hook | Unity `OnUpdateBefore/After`（`ComponentSystemGroup.cs:94-98`） | trace Module 依賴 Kernel、編排層 | 每加一層多一組 hook；產品自己的段沒有入口；trace 綁在 Kernel 上 |
| (c) Kernel 直接依賴 trace Module | Unity 核心直接用 profiler | — | 依賴方向反了（`engine/ecs` → `engine/modules`） |

**決定的理由**：方向第 2 點；Kernel 不依賴任何 Module，產品用同一個入口。**舊形狀？** 不是。

### D2 門面的形狀

- **`Engine::Base::TraceSpan`**：預設建構是「沒開始」；`Begin(category, name, world = {})` 開始；解構時結束；`MarkError(text)` 在結束前標錯誤。**可以延後開始**（M1）：Kernel 在一步的範圍開頭宣告，跑的條件成立之後（`Executor.cpp:344`）才 `Begin`，所以跳過的步不開，而段的生命期涵蓋到 `return Fail(...)`（`:366`）。
- **例外離開時**：段在解構時比對 `std::uncaught_exceptions()`，比開始時多就標「例外離開」再結束（先例 `LevelGuard`，`Executor.cpp:150-161`）。只是告訴：不接、不吞、不改例外。
- **`Engine::Base::TraceMarkCurrent(text)`**：標**這條執行緒堆疊頂上那一段**。Kernel 在 `Fail`（`Executor.cpp:79`，一次執行裡所有錯誤的出口）呼叫它一次——System 那一步的錯誤標在 System 段，同步點、hook 清空輪、Ready window 的錯誤標在 schedule 段（S7：不必動十處 `return Fail`）。
- **`Engine::Base::TraceCurrentPath(buffer)`**：把這條執行緒當下的路徑寫進呼叫的人給的緩衝，回長度；不配置、不上鎖；放不下時保留**最深的一端**、開頭標 `…`。給 E0 用（D4）。
- 參數都是 `std::string_view`，**呼叫的人保證它活到這一段結束**（Kernel 的 schedule 名、System 名、World 名存在登記與 World 裡，跑的時候不變）。不格式化、不配置。
- **接收者**（trace Module 裝；全部 `noexcept`）：`begin(category, name, world) → token`、`end(token, ended_by_exception)`、`error(token, text, origin)`、`markCurrent(text, origin)`、`path(buffer)`。`origin` 見 D4。沒裝時：讀一個原子指標、回空 token、什麼都不做；`TraceCurrentPath` 回 0。

| 選項 | 系譜 | 退掉的理由 |
|---|---|---|
| **可延後開始的 RAII＋「標當下那一段」**（建議） | Bevy `Span::entered()` 的 guard；Unity `ProfilerMarker.Auto()` | — |
| push／pop 兩個函式 | flecs `perf_trace_push/pop` | 提前 return、例外的路上要自己補 pop；RAII 讓結束一定發生 |
| 只有 RAII、沒有「標當下那一段」 | — | Kernel 要把十多個 `return Fail` 旁邊各加一句（S7） |

**卡片照 L17 判**：每次傳字串，不先登記 id（GYO 的名字本來就在登記裡，傳 view 不複製；量到是成本時改，RC-T3）。

### D3 父子怎麼接

| 選項 | 系譜 | 內容 | 代價 |
|---|---|---|---|
| **(a) 每執行緒一個堆疊；父＝堆疊頂上那一段**（建議，本批） | tracing 的 current span（單執行緒） | Kernel、編排層、產品都不傳父 | Parallel 時 worker 上的段找不到 schedule 父 |
| (b) (a)＋門面的 `Begin` 多一個可選的父 token | Bevy #25911（`function_system.rs:909-916`） | — | 只有根據 2（AGENTS §2：只有 2 不成立）；而且明寫父只救 `parent_id`，路徑（每執行緒堆疊）在 worker 上仍缺 schedule 以上（S2） |

**決定的理由**：照 AGENTS §2 延後 (b)。加一個有預設值的參數不改既有呼叫，延後的代價小；Kernel 解除 LIM-2 時重看（RC-T1），那時路徑也要一起重想。

**堆疊的生命期**（S3）：
- token 帶「世代」（每次 `Install` 加一）與堆疊深度。世代不符（Trace 卸下又重裝）的 `end`／`error` 略過；卸下時各執行緒堆疊不必清，下一次碰到時照世代丟掉。
- `end` 的段不在頂上時（不該發生，RAII 保證相反的順序）：照樣結束、從堆疊拿掉，紀錄帶 `unbalanced`——告訴，不斷言。
- 裝之前開始的段拿到空 token，結束時什麼都不做。
- 路徑只讀**這條執行緒**自己的堆疊；堆疊裡的 view 指向還活著的段（RAII），所以 E0 時讀得到。

### D4 錯誤與「在哪一步」

- **標**：Kernel 在 `Fail` 呼叫 `TraceMarkCurrent(錯誤文字, origin)`；編排層的入口回錯誤時標它的段；例外的路由段自己標。
- **路徑寫幾次**（M5：修訂 0 的「只寫一次」沒有規則，照寫只能靠狀態壓掉，實質是去重——使用者退過去重，所以交使用者選）：

| 選項 | 內容 | 代價 |
|---|---|---|
| **(i) 由知道的那一層分兩種標法**（建議） | 產生錯誤的地方標 `origin=true`：標那一段並寫路徑；往外傳的地方標 `origin=false`：只標「這段以失敗結束」。Kernel 分得出：`Fail` 的錯誤是這一步自己產生的，還是巢狀留下的 `leftError_` 往上帶（`Executor.cpp:355-363`）；編排層自己的錯誤碼是 origin，Kernel 帶上來的是往外傳；例外的路：最深那一段（第一個被展開的）是 origin | 一個錯誤一筆路徑；沒有狀態；Kernel 多傳一個布林 |
| (ii) 每次標都寫路徑 | 沒有狀態；外層的路徑是內層的前綴 | 一個錯誤三到四筆路徑，讀的人取最長的 |
| (iii) 只寫第一次，之後壓掉 | 修訂 0 的寫法 | 要記「底下標過了」（每執行緒的狀態、何時重置），就是去重 |

  理由：(i) 照「處理一次」的精神放在**結構**上——產生的那一層知道自己是源頭，不靠事後比對。
- **E0（斷言、`std::terminate`）**：

| 選項 | 系譜 | 內容 | 代價 |
|---|---|---|---|
| **(a) 日誌的致命紀錄帶 `trace_path` 欄位**（建議） | Bevy panic hook 印 `SpanTrace`（`bevy_log/src/lib.rs:307-316`，路徑由門面那一層持有） | 日誌的 `WriteFatalRecord` 呼叫 `TraceCurrentPath`（固定大小的緩衝，不配置），有內容就加一個欄位 | 改已合併的日誌（LG-2、LG-5 各加一句）；日誌讀一個 Base 掛點，不依賴 trace Module |
| (b) trace Module 也接手斷言與 terminate 處理器，自己寫一筆 | 各家 profiler 的 crash hook（知識） | 處理器串接 | 兩個 Module 接手同一組處理器，先後看安裝順序 |
| (c) E0 時不給路徑 | — | — | 斷言發生在 System 裡時看不出是哪個 System |

  理由：(a) 保住 LG-2 的性質——E0 恰好一筆 `Fatal`、處理器只被接手一次——路徑附在那一筆上。

### D5 輸出：trace 的紀錄怎麼寫出去

**系譜要先說清楚**（S1）：(a) 的紀錄形狀，和日誌宣告修訂 3 退役的「logging 5.13 的 span」幾乎一樣（`begin_ns`、`duration_ns`、category 前綴分檔）。使用者的「兩個 Module」推翻的是「一個系統」——留下的是什麼，請看下表的「職責」與「傳輸」兩欄。

| 選項 | 系譜 | 職責 | 傳輸 | 代價 |
|---|---|---|---|---|
| **(a) trace 把每段寫成日誌的一筆紀錄**（建議） | 退役的 logging 5.13（最近的祖先）；Bevy：同一個門面下兩種輸出層並排 | trace Module 擁有 span、父子、計時、路徑；logging 不知道 span | 共用日誌的傳輸（佇列、寫者、檔、致命路徑） | trace → logging 的依賴；trace 的紀錄和日誌共用有上限的佇列，滿了路徑紀錄也會丟（LG-6） |
| (b) trace 自己一套傳輸 | master `engine/trace` 的 `TraceSink` | 同上 | 另一套佇列＋寫者＋檔 | 和 `JsonLinesFileSink` 幾乎一樣的東西寫兩份；致命路徑要再做一次 |
| (c) 把日誌的檔案寫出抽成更底層的共用 Module | — | 同上 | 共用的寫出 Module | 為了 (a) 已經能共用的東西多一層抽象 |

**決定的理由**：(a)。職責分在兩個 Module（日誌給人看、trace 追問題），傳輸共用一套已經驗證過的。分不分檔是產品在日誌 `Install` 的設定。**舊形狀？** (a) 的紀錄形狀和退役的 5.13 同形——差別在職責歸 trace Module；這一點交使用者確認（第 13 節第 3 題）。(b) 是 master 的形。

**紀錄的樣子**（S6、M4 後半）：
- 一段結束時一筆：category `span.<呼叫的 category>`（不用 `trace.`：日誌的例子與產品的分析紀錄用 `trace.movement`，`LoggingBudget.cpp:55`）、level `Trace`、text＝name、`world`（有時）、欄位 `span_id`、`parent_id`、`begin_ns`、`duration_ns`、（有時）`error`、`exception`、`unbalanced`。
- 路徑紀錄：category `span.path`、level `Trace`、text＝錯誤文字、欄位 `path`、`span_id`。
- level 是 `Trace`，不是 `Error`：trace 不「記錯誤」，錯誤在欄位裡；產品可以只留路徑紀錄（`include={"span.path"}`）。
- **只在有人收時寫**：沒有裝 Logger，或沒有輸出端收這個 category 時，trace 不讀時鐘、不組紀錄，只維持堆疊（和 LG-7 對等）。為此日誌加一個查詢 `Logging::Installed()`（`Logging::Enabled` 在沒裝時恆真，`Write.hpp:33-35`，不能拿來判斷）。

### D6 Engine 各層發哪些段（先做薄）

| 層 | 位置（bb2497b0） | 段 |
|---|---|---|
| Kernel | `WorldExecutor::RunPlan`（`Executor.cpp:208` 起；涵蓋 Ready window 的檢查 `:223-235`、同步點、hook 清空輪、巢狀跑） | category `kernel`、name＝schedule 名、world＝World 名 |
| Kernel | 一步的 System 呼叫：一步的範圍開頭宣告、`:344`（跑的條件成立）才開始；涵蓋到 `CombineCall` 與 `return Fail`（`:361-366`） | category `kernel`、name＝System 名、world。**長度含 Kernel 的存取驗證（`CompareAfter`，`:353`，開發版才開）** |
| Kernel | `WorldExecutor::TriggerFromHost`（`:938`，另一個最外層入口） | category `kernel`、name `Trigger`、world |
| Kernel | `Fail`（`:79`） | `TraceMarkCurrent(text, origin)` 一行 |
| 編排層 | `RunGameInit`、`RunFrame`、`RunSchedule`、`ReplaceWorld`、`InsertSystems`（`Product.cpp:464`、`:476`、`:491`、`:534`、`:737`） | category `orchestration`、name＝入口名；回錯誤時標 |

- Kernel 不讀時鐘（計時在 trace Module）；Kernel 不寫日誌（呼叫的是 trace 門面）。
- 決定性：trace 不讀、不寫 World 的任何值，不改步驟；開不開 trace，模擬的值逐位元相同（TR-2）。

### D7 關掉時的成本

| 選項 | 系譜 | 代價 |
|---|---|---|
| **(a) 執行期：沒裝接收者時讀一個原子指標；裝了而沒人收 `span.` 時只維持堆疊、不讀時鐘**（建議） | GYO `Base::Log`；flecs 開了 `FLECS_PERF_TRACE` 之後的形 | 每段一次原子讀取＋（裝了時）堆疊推入推出 |
| (b) 另加編譯期開關（巨集或 CMake 選項） | Bevy `cfg(feature="trace")`、flecs、Unity | 多一個建置組合要測；編進來的那一版行為和測過的那一版不同 |

**決定的理由**：(a)。卡片的質疑：三家編譯期移除，是因為它們的 trace 是量效能的工具、發行版可以拿掉；GYO 的 trace 是追問題，發行版正需要它。代價按第 5.2 節的估計：沒人收時約 40 µs／幀，全部寫出約 2.4 ms／幀——所以「發行版也開」指的是門面與堆疊常開、寫不寫由產品的篩選決定。判定 7 量；超過再做 (b)（RC-T3）。

### D8 位置與名字（卡片照 L17 判：照 `logging` 的先例與命名規範）

| 項目 | 名字 |
|---|---|
| 門面 | `engine/core/base/include/engine/base/Trace.hpp`：`Engine::Base::TraceSpan`、`TraceReceiver`、`SetTraceReceiver`、`TraceMarkCurrent`、`TraceCurrentPath` |
| Module | `engine/modules/trace/`、`GYO::Engine::Trace`、`gyo_trace`／`GYO::Trace`；登記列 `trace,modules/trace,GYO::Trace,"Spans: the per-thread stack, parents, timing and the path, written as logging records",yes,no,base;logging,time,,` |
| 安裝 | `Trace::Install(options) → Result<TraceHandle, …>`：一個行程一個（第二個是斷言）；`TraceHandle` 卸下時還原接收者；`options.clock` 可注入（golden 用） |

## 7. Architecture Delta（AGENTS §3 的七項）

1. **Feature**：F8（追問題）、F4（量一段時間）。
2. **現在的問題**：World 壞掉時只有 runner 的一句原因，看不出外層做了什麼；斷言發生在 System 裡時，致命紀錄不知道是哪個 System；產品沒有量時間的工具（span 已從 logging 拿出）。
3. **變的邊界**：Base 多一個門面；Kernel 的 `Executor.cpp`（`RunPlan`、一步、`TriggerFromHost`、`Fail`）與編排層的 `Product.cpp`（五個入口）開始呼叫它；新 Module；日誌的致命路徑多一個欄位，日誌多一個查詢 `Installed()`。
4. **影響的 Component**：Base、Kernel、編排層、logging、新的 trace；守衛 `kernel_symbols`（准許清單）；沒有產品。
5. **依賴方向**：新邊 trace → base、trace → logging、trace → time；沒有向上的邊。
6. **Ownership**：span 的父子、計時、路徑歸 trace；「有沒有接收者」歸 Base 門面；Kernel、編排層只擁有「我現在在做哪一步」的名字。
7. **為什麼更小的變更不行**：不動 Kernel 就沒有 Engine 這一側的路徑（方向第 5 點）；不動日誌就得讓 E0 寫兩筆或不給路徑（D4）。

**動到的別的 Component 的 Design Invariants**（M4：修訂 0 漏了；都要使用者同意，第 13 節）：

| 在哪 | 現在 | 本批的補充 |
|---|---|---|
| Kernel DI-KS 15（`target/kernel.zh-Hant.md:76`）職責清單 13 項 | 不在清單上的不放進 Kernel；RC 16：清單要多一項就停下來問 | 照 RC 16 交使用者：選 (1) 清單加第 14 項「告訴 trace 門面現在在哪一步（呼叫 Base 的門面，不擁有 trace）」，或 (2) 不加、寫明「呼叫 Base 的門面（像 `GYO_ASSERT`）不是 Kernel 的職責」。卡片建議 (2)：Kernel 不擁有任何 trace 的狀態，和用 `GYO_ASSERT` 同類 |
| `target/error-handling.md` DI 2「the kernel has no error handler and no log call」、DI 4「returns and does not also log」 | 理由：同一個錯誤處理兩次（D1） | 加一句：「標在 trace 段上的錯誤不是日誌行：它是 `Trace` 層級的診斷，不改回傳值、不改流程；錯誤照舊由回傳交給呼叫的人」。理由：Go／OpenTelemetry 的 span 上 `RecordError` 是同一個區分（主會話轉來 side chat 的說明，不是使用者的話） |
| 日誌 LG-2、LG-5（`target/v2/logging.zh-Hant.md`） | E0 恰好一筆 `Fatal`；固定鍵 | 致命紀錄在 trace 有路徑時帶 `trace_path` 欄位；仍恰好一筆 |
| `kernel_symbols` 守衛（`tests/common/kernel/kernel_symbols.py:44-49`、自我測試 `test_kernel_symbols.py:139-142`） | Kernel 函式庫的外來可寫符號恰好四個 | 加門面的接收者指標（M3），見第 9 節的 §14.4 1～5 |

**和第 4 批（權威＋回滾）的交界**（S4）：
- 同檔：第 4 批也改 `Executor.cpp`、`WorldExecutor.hpp`、`Product.cpp`、`Setup.hpp`。本批在 Kernel 加四處（`RunPlan` 一段、一步一段、`TriggerFromHost` 一段、`Fail` 一行），編排層五處；後合的那一批 rebase。
- 量：回滾的重跑讓段數乘上（1＋k）（第 5.2 節）；重跑的段和第一次分不出——要分是第 4 批的事（例：回滾的 System 開一段 `Resimulate`），本批不做。
- 決定性：trace 不碰 World，回滾的逐位元等價不受影響（TR-2、判定 4）。

## 8. Design Invariants（新頁 `target/v2/trace.zh-Hant.md`，見出し `Design Invariants`；最終文字在合併前核准定稿）

| 條 | 內容 | 守衛 |
|---|---|---|
| **TR-1 入口** | 發段的路只有 Base 的門面；Kernel、編排層、Module 不依賴 trace Module。 | 含入掃描（情境通過後立）；`gyo_expect_links` |
| **TR-2 不改模擬的值** | trace 不讀、不寫 World 的值，不改 Kernel 的步驟；開不開 trace，模擬的值逐位元相同。Kernel 不讀時鐘。 | 情境的決定性案；`kernel_guard` K3 |
| **TR-3 標錯誤只是告訴** | `MarkError`、`TraceMarkCurrent`、例外離開的標記不改回傳值、不改流程、不接也不吞例外。路徑照 D4 選定的規則寫。 | 測試 |
| **TR-4 父子與生命期** | 父＝這條執行緒堆疊頂上那一段；每段有行程內唯一的 id；token 帶世代，不符就略過；不在頂上的 `end` 照樣結束並標 `unbalanced`；例外的路也結束並標 `exception`。 | 單元案（巢狀、例外、提前 return、重裝、不平衡） |
| **TR-5 紀錄** | 一段一筆：category `span.<category>`、level `Trace`、text＝name、`world`、欄位 `span_id`、`parent_id`、`begin_ns`、`duration_ns`、（有時）`error`、`exception`、`unbalanced`；路徑紀錄 category `span.path`。只在 Logger 裝了且有輸出端收時寫。 | 往返、golden（注入的時鐘） |
| **TR-6 沒人收時** | 沒裝接收者：只讀一個原子指標。裝了而沒人收：只維持堆疊，不讀時鐘、不配置。 | 配置計數測試；判定 7 |
| **TR-7 一個行程一個** | 同時只裝一個（第二個是斷言）；卸下時還原接收者。 | 測試 |

### Replacement Criteria

- **RC-T1 Parallel**：Kernel 解除 LIM-2 時，停下重看 D3（明寫父、worker 上的路徑）。
- **RC-T2 路徑不準**：觀察到錯誤被標時堆疊已經不含出錯的那一段——重看 D2、D4 的標記位置。
- **RC-T3 成本**：判定 7 量到掛點是熱路徑的主要成本，或某個點名的 Feature 超出預算——登記 id 或編譯期開關。
- **RC-T4 日誌要帶路徑**：有 Feature 要每一行日誌知道自己在哪一段——重看 `TraceCurrentPath` 是否給一般紀錄用。
- **RC-T5 輸出要分家**：trace 需要和日誌不同的丟棄政策（例：一筆都不能丟）——重看 D5 (b)。

## 9. 測試、守衛、突變

- **測試先行**：每一步先 Red 再 Green；新測試在 `tests/common/trace/`；Kernel、編排層的段在各自的測試加案，用測試的接收者看收到的事件。**測試寫的檔放在建置目錄**（`CMAKE_CURRENT_BINARY_DIR` 傳給測試），不在工作目錄；logging 的兩處一併改（T6）。
- **情境**：`tests/common/scenario/` 加一段（段號和第 4 批對齊）：relay 遊戲，(1) Match 第 3 幀 System 回錯誤；(2) System 丟例外；(3) System 裡斷言（測試的處理器）；(4) trace 開關兩次比遊戲的值。
- **守衛**：
  - trace 的含入掃描（情境通過後立）。
  - **`kernel_symbols` 的准許清單加門面的接收者指標**（M3：修訂 0 說要改 `kernel_guard` K6，事實寫錯——K6 是禁止清單，門面本來就過；會壞的是 `kernel_symbols`）。第 3 步就要用。照 AGENTS §14.4：
    1. **守的性質**：Kernel 函式庫裡沒有自己的可寫全域狀態（決定性），外來的只准列名的 Base 符號。
    2. **緩める面**：多准一個 Base 符號（`Engine::Base::Detail::traceReceiver`，原子指標）。
    3. **締める面**：無。
    4. **緩める面守的性質由誰守**：這個符號是 Base 的、Kernel 只讀不寫（`Trace.hpp` 的門面）；和既有的 `assertionHandler` 同類（`Assert.hpp:49`）；Kernel 自己的可寫符號仍由 `KERNEL_ALLOWED` 守。
    5. **根據**：D1（使用者核准的方向第 2 點）；動工前核准時一起定（第 13 節第 6 題）。
- **突變**：清單 `tests/common/trace/mutations.json`（前綴 `tr-`），每條決定至少一個（README 基準 3）；合併前完整跑一次；清單送 Fable 5.2／high 敵對式檢查。

## 10. 判定（先宣告，結果出來之後不放寬）

| # | 判定 | 怎麼算過 |
|---|---|---|
| 1 | L1 三平台 ctest 全綠 | 含 `gyo_trace_tests`、情境新段、守衛 |
| 2 | 追問題 | 情境 (1)：三段（入口、`Main@Match`、`MoveRacer@Match`）父子相連、System 段有 `error`；路徑紀錄照 D4 選定的規則（(i)：一筆）；情境 (2)：例外展開的段帶 `exception`、最深那段寫路徑；情境 (3)：日誌那一筆 `Fatal` 帶 `trace_path`、仍恰好一筆 `Fatal` |
| 3 | 父子與配對 | 單元案：巢狀、例外、提前 return、延後開始、跳過的步不開、重裝（世代）、不平衡：每段恰好一筆、`parent_id` 對 |
| 4 | 決定性 | 情境 (4)：trace 開關，遊戲值逐位元相同 |
| 5 | 紀錄格式 | 每筆 `Parse` 讀回；欄位照 TR-5；沒人收時沒有紀錄、不讀時鐘 |
| 6 | 突變 | 全部 killed（只算預期的失敗）；敵對式檢查加的也 killed |
| 7 | 成本（只量、不設門檻） | 本機 Release：沒裝時一段；裝了沒人收時一段；裝了有人收時一段（含進佇列）；Kernel 跑一張 20 個 System 的 schedule，三種情形的差。方法照日誌判定 7 |
| 8 | 日誌批收尾（T6） | 在 repo 根跑完 ctest 與突變之後，repo 根沒有 `logging-test-files/`、`scenario-segment3-files/` 等測試留下的東西 |

## 11. 實作的步驟（動工前核准之後）

上段（公開介面、資料格式、Design Invariants）。全程 high、不排 xhigh；碰到 §14 停止條件先停下回報。

| 步 | 內容 | 實作 | 審查 | 估計 |
|---|---|---|---|---|
| 第 1 步 | Base 門面 `Trace.hpp`（延後開始、例外的標記、標當下那一段、取路徑、沒裝時的路）；`kernel_symbols` 准許清單（§14.4，第 9 節） | Opus 5.5／high | — | 2～3 h |
| 第 2 步 | trace Module：安裝、堆疊與世代、id、計時、寫成日誌紀錄、路徑紀錄、只在有人收時寫；日誌加 `Installed()` | Opus 5.5／high | — | 4～6 h |
| 第 3 步 | Kernel、編排層發段（D6） | Opus 5.5／high | — | 3～4 h |
| 第 4 步 | 日誌的致命紀錄帶 `trace_path`；T6 | Opus 5.5／high | — | 2～3 h |
| 第 5 步 | 情境、判定 7、含入掃描守衛、突變清單 | Opus 5.5／high | 敵對式檢查 Fable 5.2／high | 4～6 h |
| 第 6 步 | 審查、文件（含別的 Component 的條文補充）、完整跑突變 | Opus 5.5／high | 審查 Fable 5.2／high | 3～4 h |

**時間估計**：設計段 4～6 h（到修訂 1 約 4 h）；實作段 18～26 h。超過 1.5 倍或單一問題累計超過 2 小時，停下回報。

## 12. 範圍的限界

- 沒有產品：情境用測試的遊戲。
- 只有 Sequential executor（LIM-2）。
- 路徑紀錄進的是日誌的佇列，不是已經到磁碟；佇列滿了會丟（LG-6）。E0 時的 `trace_path` 走致命路徑，不經佇列。
- Windows 上非 `Install` 執行緒的 `std::terminate` 沒有致命紀錄（LG-2 的不保證），也就沒有 `trace_path`。
- 回滾重跑的段和第一次分不出（第 4 批）。
- 只量成本、不設門檻（判定 7）。

## 13. 交使用者的（動工前核准）

只交和別家不同、推翻已定的事、或規則要求問的（`lessons-ratchet-both-ways`）。其餘照第 6 節的建議做。

1. **D4 路徑寫幾次**（M5，和你退過的去重有關）：建議 (i) 由知道的那一層分兩種標法——產生錯誤的地方寫路徑，往外傳的地方只標「這段以失敗結束」；或 (ii) 每次都寫、讀的人取最長；或 (iii) 只寫第一次（要記狀態，就是去重）。
2. **D4 E0 時的路徑**：建議 (a) 日誌的致命紀錄帶 `trace_path`——要改已合併的日誌（LG-2、LG-5 各加一句）。
3. **D5 輸出走日誌**：建議 (a)。它的紀錄形狀和日誌修訂 3 退役的 span 同形；留下的差別是「職責歸 trace Module、傳輸共用日誌」。如果你說的「兩個 Module」也包括「兩套傳輸」，就選 (b)。
4. **D7 執行期開關、成本只量**：和 Bevy、flecs、Unity 的「編譯期拿掉」不同；理由是 GYO 的 trace 在發行版也要能追問題。估計：沒人收時約 40 µs／幀、全部寫出約 2.4 ms／幀（第 5.2 節）。
5. **Kernel 職責清單（DI-KS 15）與錯誤條文（error-handling DI 2、DI 4）**（RC 16 規定要問）：建議清單不加項、寫明「呼叫 Base 的門面不是 Kernel 的職責」；錯誤條文加一句「標在 trace 段上的錯誤不是日誌行」。
6. **`kernel_symbols` 准許清單多一個 Base 符號**（守衛的放寬，§14.4 的 1～5 在第 9 節）。

## 14. 停止條件

- AGENTS §14 冒頭的三條。
- 實作中發現標記拿不到出錯的那一段（RC-T2），或 Kernel 的段會改變步驟或決定性（TR-2）。
- 第 4 批先合併而 `Executor.cpp`、`Product.cpp` 的結構大改——停下重看 D6 的位置。

## 15. 修訂 0 的敵對式檢查：處理對照

檢查者 Fable 5.2／high，報告 `trace-declaration-r0-adversarial-check.md`。行號與三家引文它都開檔對過、全部屬實。

| # | 指出 | 處理 |
|---|---|---|
| M1 | System 段放在 `:345` 會在 `:346` 結束，早於錯誤成形 | D2 加「可延後開始」：一步的範圍開頭宣告、`:344` 才開始；長度含 `CompareAfter` 寫明（D6） |
| M2 | 例外離開時段被展開結束，沒有 `error`、沒有路徑 | D2：展開而結束的段自己標 `exception`；D4 (i)：最深那段寫路徑；判定 2 加例外案 |
| M3 | 守衛的事實寫錯；會壞的是 `kernel_symbols` | 第 9 節改成 `kernel_symbols` 准許清單加一個符號，附 §14.4 的 1～5；第 13 節第 6 題改 |
| M4 | 沒對照 Kernel DI-KS 15／RC 16、error-handling DI 2／4；紀錄的 level 沒定 | 第 7 節加「動到的別的 Component 的 Design Invariants」表；第 13 節第 5 題；TR-5 定 level `Trace` |
| M5 | 「路徑只寫一次」沒有規則，實質是去重 | D4 改成三個結構上的選項，交使用者（第 13 節第 1 題） |
| M6 | 門面沒有 World，例子寫不出來 | `Begin` 加 `world`；Kernel 的段帶 World 名；例子與判定 2 改成 `Main@Match` |
| S1 | D5 (a) 少了最近的祖先（退役的 5.13） | D5 加系譜與「職責／傳輸」兩欄；第 13 節第 3 題 |
| S2 | F9 的根據寫過頭 | F9 拿掉；D3 照 AGENTS §2 延後明寫父 |
| S3 | 堆疊的生命期沒定 | D3 加「堆疊的生命期」（世代、不平衡、裝之前的段）；D1／D2 統一成一個原子指標 |
| S4 | 量與第 4 批的交界 | 第 5.2 節的估計；D7 用它；第 7 節交界加量與決定性；第 12 節加佇列會丟 |
| S5 | 「已經寫出去了」不成立 | 第 1 節改寫；第 12 節加兩條限界 |
| S6 | `trace.` 和產品的分析紀錄撞名；路徑紀錄沒定 | category 改 `span.`；路徑紀錄 `span.path` 與欄位定了 |
| S7 | 沒有 System 段的錯誤與入口；改動量 | 段放 `RunPlan`；加 `TriggerFromHost`；`Fail` 標當下那一段（不必動十處） |
| 小 | D4、D5 (c) 的理由寫法 | 改成性質（LG-2）；(c) 改成「為已能共用的東西多一層抽象」 |
| 小 | D2 的 Unity 說法、D7 (b) 的代價 | 照偵察的「推的」改寫；(b) 的代價改寫 |
| 小 | `trace_path` 的緩衝、截斷、不配置、不上鎖 | D2 的 `TraceCurrentPath` 寫明 |
| 小 | Trace 裝了而 Logger 沒裝時每段一行 stderr | D5：只在 Logger 裝了且有人收時寫；日誌加 `Installed()` |
| 小 | 沒人收時不讀時鐘 | TR-6、D5 |
| 小 | T6 沒有判定；trace 測試的寫檔位置 | 判定 8；第 9 節 |
| 小 | TR-7 第二次 `Install`、golden 的時鐘 | TR-7 斷言；D8 `options.clock` |
