# trace（Base 的門面、`engine/modules/trace`）

Owner：架構。來源：事前宣告 [trace-declaration.zh-Hant.md](trace-declaration.zh-Hant.md)（修訂 1，2026-11-21 動工前核准「照建議，全部核准」）。最終文字在 `v8-arch-gyo2-trace` 的合併前核准定稿（2026-11-21「照建議，全部核准，合併」，[trace-merge-approval.zh-Hant.md](trace-merge-approval.zh-Hant.md)）。

**核心**：trace 是機制——告訴外面「每一層做了什麼、在哪一步壞了」，不規定別人怎麼處理（使用者 2026-11-21「日誌只是看。trace才是追問題。」）。日誌和 trace 是兩個 Module：職責分開，傳輸共用日誌的那一套（宣告 D5）。

| 部分 | 內容 | 依賴（公開；私有） |
|---|---|---|
| 門面 `engine/core/base/include/engine/base/Trace.hpp`（`GYO::Engine::Base`） | `TraceSpan`（可延後開始的 RAII、例外展開時告訴接收者）、`TraceMarkCurrent`、`TraceCurrentPath`、`TraceActive`、`SetTraceReceiver`、`TraceReceiver`、`TraceToken`；一個原子指標指向接收者 | 只有 C++ 標準庫（Base 是 header-only） |
| `trace`（`GYO::Engine::Trace`，`engine/modules/trace`） | 接收者：每執行緒固定容量的段堆疊（世代）、行程內唯一的 id、父、計時、寫成日誌紀錄、路徑紀錄；`Install` 回 `TraceHandle` | `GYO::Base`；私有 `GYO::Logging`、`GYO::Time` |

`trace` 是 `library_only=yes`、`default=no`：產品的 `main` 裝。

**誰發段**（宣告 D6）：Kernel——`WorldExecutor::RunPlan` 一張 schedule 一段（含巢狀跑）、一步的 System 一段（跑的條件成立才開始）、`TriggerFromHost` 一段，`Fail` 標當下那一段；編排層——五個入口（`RunGameInit`、`RunFrame`、`RunSchedule`、`ReplaceWorld`、`InsertSystems`）各一段；產品與 Module 自己的段。

**紀錄**（經日誌寫出，level `Trace`）：

```text
一段一筆   category span.<category>  text＝name  world  欄位 span_id parent_id begin_ns duration_ns [error] [exception] [unbalanced]
路徑一筆   category span.path        text＝錯誤   world  欄位 path span_id
E0 時      日誌的那一筆 Fatal 帶 trace_path（當下的路徑）
```

路徑的寫法：`category:name@world > …`，最外層在前；放不下時保留最深的一端，前面標 `...`。

## Design Invariants

守衛欄：行為的測試和機制同時寫；綁實作方式的含入掃描在情境測試第 4 段通過之後才立（AGENTS §3）。突變清單 `tests/common/trace/mutations.json`（前綴 `tr-`，`guards` 寫條號）。

<a id="tr-1"></a>
**TR-1 入口。** 發段的路只有 Base 的門面；Kernel、編排層、Module 不依賴 trace Module。trace Module 只含入 Base、Logging、Time 與自己。
守衛：`trace.guard`（`tests/common/trace/trace_guard.py`：一條含入規則、正例、目錄不存在時失敗）與 `trace.guard_selftest`；Kernel 一側由 `kernel_guard` K5、編排層由 `orchestration_guard` O4（都不准 `engine/trace/`）；`gyo_expect_links(gyo_trace GYO::Base GYO::Logging GYO::Time)`；`kernel_symbols` 的准許清單只多門面的接收者指標（`GYO::Engine::Base::Detail::traceReceiver`）。突變 `tr-guard-*`。

<a id="tr-2"></a>
**TR-2 不改模擬的值。** trace 不讀、不寫 World 的值，不改 Kernel 的步驟；開不開 trace，模擬的值逐位元相同。Kernel 不讀時鐘（計時在 trace Module 的接收者裡）。
守衛：情境 S4-4（`scenario.segment4`）；`kernel_guard` K3；審查。

<a id="tr-3"></a>
**TR-3 標錯誤只是告訴。** `MarkError`、`TraceMarkCurrent`、例外展開的標記不改回傳值、不改流程、不接也不吞例外。路徑在**錯誤產生的地方**寫一筆（`origin`）：Kernel 的 `Fail` 在這一層產生的錯誤是 origin，巢狀跑的錯誤往外經過外層 System 時不是；編排層自己的錯誤是 origin，Kernel 的 schedule 錯誤（`ScheduleFailed`、`ReplaceScheduleFailed`）與壞掉的 World（帶 `failure`）只是經過；例外的路上，第一個被展開的段寫路徑（「an exception left the span」）；子段的例外被接住後，父段之後開始了新的子段或被標記，父段自己的例外再寫路徑，兩件都沒有就自己丟例外時，父段只標 `exception`、不寫路徑（看起來像經過；審查 2026-11-21 第 2 項）。往外傳的那幾層只標「這段以失敗結束」。（宣告 D4 (i)，使用者 2026-11-21 核准：不靠狀態壓掉，不是去重。）
守衛：`trace.module` 各案；`kernel.*` 的「kernel trace:」各案（`[ks-trace-err]`、`[ks-trace-nested]`、`[ks-trace-exception]`、`[ks-trace-host-trigger-once]`、`[ks-trace-system-trigger-once]`）；`orchestration.trace-spans`（`[or-trace-origin]`、`[or-trace-break-through]`、`[or-trace-ended-through]`）；情境 S4-1、S4-2。突變 `tr-origin-*`、`tr-unwound-every-span`、`tr-child-flag-kept`、`tr-mark-keeps-child-flag`、`tr-kernel-origin-always`、`tr-kernel-*-trigger-origin`、`tr-kernel-fail-unmarked`、`tr-orch-*`。

<a id="tr-4"></a>
**TR-4 父子與生命期。** 父＝這條執行緒堆疊頂上那一段；每段有行程內唯一的 id。`TraceSpan` 在 `Begin` 才開始（宣告了而沒開始的段不呼叫接收者，例：跑的條件不成立的步），在解構時結束；`Begin` 之後才開始的例外展開，結束時標 `exception`；段在它開始時的接收者那裡結束。token 帶世代（每次 `Install` 加一），世代不符就略過，前一次 `Install` 留下的堆疊在下一次碰到時丟掉。結束的段不在頂上時照樣結束、從堆疊拿掉、標 `unbalanced`。堆疊固定 64 層，更深的段只計數、不記錄，標在它們上面的錯誤也丟掉（沒有路徑紀錄；審查第 8 項）。段在別的執行緒結束時不寫、也不從原執行緒的堆疊拿掉（只約定同一條執行緒，見 RC-T1）。
守衛：`gyo_base_tests` 的「trace facade:」各案；`trace.module`；Kernel 的 `[ks-trace-spans]`、`[ks-trace-skipped]`；情境 S4-1 的父子。突變 `tr-facade-*`、`tr-parent-none`、`tr-stale-stack-kept`、`tr-unbalanced-*`、`tr-kernel-*-span*`。

<a id="tr-5"></a>
**TR-5 紀錄。** 一段一筆：category `span.<category>`、level `Trace`、text＝name、`world`、欄位 `span_id`、`parent_id`、`begin_ns`、`duration_ns`（`GYO::Engine::Time`，和日誌的 `mono_ns` 同一個時間基準）、有時 `error`（最多 256 位元組）、`exception`、`unbalanced`；路徑紀錄 category `span.path`、欄位 `path`、`span_id`。category 最多 64 位元組，超過截斷（和 `error` 一樣；機制不因名字太長而停下行程）。只在 Logger 裝了（`Logging::Installed()`）且有輸出端收這個 category 時寫；段結束時再問一次（段開著時 Logger 卸下就不寫）。紀錄經日誌的傳輸，所以和日誌共用有上限的佇列（LG-6），滿了會丟。
守衛：`trace.module`（巢狀、錯誤、路徑、只收路徑、沒有 Logger）；情境 S4-1。突變 `tr-duration-wrong`、`tr-no-logger-taken`、`tr-path-cut-unmarked`、`tr-category-asserted`、`tr-end-not-rechecked`、`lg-installed-always`。

<a id="tr-6"></a>
**TR-6 沒人收時。** 沒裝接收者：`TraceSpan` 只讀一個原子指標。裝了而沒人收：只維持堆疊（給路徑用），不讀時鐘、不配置。
守衛：`trace.module`（「with only span.path taken」「with no Logger installed」）；判定 7 的量測。突變 `tr-timed-always`。

<a id="tr-7"></a>
**TR-7 一個行程一個。** 同時只裝一個（第二個是斷言）；`TraceHandle` 卸下時還原之前的接收者。前提：`Install` 在別的執行緒開始發段之前、卸下在它們停了之後（時鐘與之前的接收者不是原子；審查第 7 項）。
守衛：`trace.module`。突變 `tr-second-install`、`tr-close-keeps-receiver`。

**動到的別的 Component**（宣告第 7 節，使用者 2026-11-21 核准）：日誌 LG-2、LG-5（致命紀錄帶 `trace_path`）、Kernel DI-KS 15（呼叫 Base 的門面不是 Kernel 的職責，清單不加項）、`target/error-handling.md` DI 2、DI 4（標在 trace 段上的錯誤不是日誌行）——各頁已補。

### Replacement Criteria

- **RC-T1 Parallel**：Kernel 解除 LIM-2 時，停下重看父子（明寫父）與 worker 上的路徑；也重看段在別的執行緒結束的情形（現在不寫，原執行緒的堆疊留著那一層，之後的路徑讀到它的名字）。
- **RC-T2 路徑不準**：觀察到錯誤被標時堆疊已經不含出錯的那一段——重看標記的位置。
- **RC-T3 成本**：判定 7 量到掛點是熱路徑的主要成本，或某個點名的 Feature 超出預算——登記 id 或編譯期開關。本批的量（Intel i9-9980HK、Release）：一段，沒裝 43 ns、裝了沒人收 83 ns、進佇列 2.4 µs；一幀 20 個 System，trace 關 6.7 µs、沒人收 6.7 µs、全收 78 µs。
- **RC-T4 日誌要帶路徑**：有 Feature 要每一行日誌知道自己在哪一段——重看 `TraceCurrentPath` 是否給一般紀錄用。
- **RC-T5 輸出要分家**：trace 需要和日誌不同的丟棄政策（例：一筆都不能丟）——重看宣告 D5 (b)。
