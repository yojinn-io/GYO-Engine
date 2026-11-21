# 日誌（`engine/modules/logging`、`engine/modules/world_logging`）

Owner：架構。來源：事前宣告 [logging-declaration.zh-Hant.md](logging-declaration.zh-Hant.md)（修訂 1，2026-11-20 動工前核准；第 8 節）與修訂 3（2026-11-21，宣告第 18 節：World 壞掉時記不記由產品的 main 決定、要記就經 ③ 或 ④，致命路徑只給 E0；日誌和 trace 是兩個 Module，span 歸 trace；使用者「日誌只是看。trace才是追問題。」「Log 和 trace是2個Module」）。**最終文字在 `v8-arch-gyo2-logging` 的合併前核准定稿。**

兩個 Module，照 `time`／`world_time` 的拆法：

| Module | 內容 | 依賴（公開；私有） |
|---|---|---|
| `logging`（`GYO::Engine::Logging`） | `Record`、`Level`、`FieldValue`、`RecordBuilder`；一個行程一個 `Logger`（`Install` 回 `Result`、`LoggerHandle` 卸下）；輸出端 `Sink` 與三個內建（`ConsoleSink`、`JsonLinesFileSink`、`MemorySink`）；JSON lines 與文字格式；`Write`（直接寫一筆）；`WriteFatalRecord`（致命時的最後一道）；斷言與 `std::terminate` 的處理器。不認得 ECS | `GYO::Base`；私有 `GYO::Json`、`GYO::Os`、`GYO::Threads`、`GYO::Time` |
| `world_logging`（`GYO::Engine::WorldLogging`） | `Forward<D>`（把健康 World 的 `Message` 寫成紀錄）、`LogWorldLabel<D>`、`TakeKernelDiagnostics` | `GYO::Logging`、`GYO::Kernel`、`GYO::Orchestration` |

兩個都 `library_only=yes`、`default=no`：`Logger` 由 `main` 裝；`Forward` 由接線的人在自己的 `build` 裡呼叫；描述器（例：`WorldTime::TimeDiscardedLog`）由發出訊息的 Module 寫在訊息旁邊（宣告第 13 節第 1 題 D）。沒有外部組件，所以沒有 `src/adapters/`。

**入口**（宣告第 3 節）：

```text
World 健康時
  ① Message ──Forward──→ ┐
  ② Kernel 的診斷紀錄 ──runner 在兩次 Run 之間取走（TakeKernelDiagnostics）──→ ┤
World 外的程式                                                                   ├──→ Logger ──→ Sink
  ③ Engine::Base::Log ──處理器──→ ┤   （World 壞掉時，產品的 main 要記那一行，通常走這裡）
  ④ Logging::Write（直接寫一筆）──→ ┘
致命時（E0）
  ⑥ 斷言失敗、std::terminate ──處理器──→ WriteFatalRecord（主控台＋各輸出端的致命寫出口，不等任何東西；之後 flush，有期限）
（⑤ 是修訂 2 以前的 WriteFailure，修訂 3 拿掉；號碼不重排）
```

## Design Invariants

守衛欄：行為的測試和機制同時寫；綁實作方式的含入掃描在情境測試第 3 段通過之後才立（AGENTS §3「ガードを立てる時期」）。突變清單 `tests/common/logging/mutations.json`（前綴 `lg-`，`guards` 寫條號）。

<a id="lg-1"></a>
**LG-1 入口。** 進日誌的路只有上面的 ①②③④⑥。Engine 的 Module 在 System 裡報警告走 `Message`（①），不走 ③④；作者自己的 System 可以直接呼叫 ④（宣告第 13 節第 5 題）。致命錯誤與 World 自己的壞掉、建立、替換不經事件。World 壞掉（E1）時記不記、記什麼，由決定接下來怎麼辦的那一方（產品的 main）定，要記就經 ③ 或 ④（宣告 5.6）；日誌不另設 E1 的入口，也不規定誰寫。Kernel 的診斷紀錄（`DroppedMessages`、`RejectedSpawn`）由 runner 取走，一個 World 一個取走的人；中斷的 World 也取，紀錄標 `world_interrupted`；和 E1 那一行沒有規定的先後（讀的人用 `mono_ns` 排），取診斷紀錄失敗時，E1 那一行照寫（使用者 2026-11-20）。`Forward` 在沒有輸出端收的時候只推游標（之後不補記）。不新增別的入口（回呼登記、全域事件匯流排）。**不保證**：讀者第一次跑之前，別張最外層 schedule 寫的、那次 Run 結束就被修剪的訊息；回滾 World 裡固定步寫的訊息（回滾份 7.1，和第 4 批對齊，宣告第 13 節第 7 題）。
守衛：`world_logging.forward`、`world_logging.diagnostics`、`logging.logger`（`Base::Log`）、情境測試 `scenario.segment3`（S3-1 的 ③、S3-2 的 ①②）；突變 `lg-diag-*`、`lg-forward-filtered-cursor-kept`。

<a id="lg-2"></a>
**LG-2 致命路徑只給 E0。** `WriteFatalRecord` 是斷言與 `std::terminate` 的最後一道（E0；級別的定義在 `docs/architecture/target/error-handling.md` 的「Error levels」）。在呼叫的執行緒上：先寫主控台（鎖只 try），再寫每個收這個 category 的輸出端的致命寫出口（不看層級；檔案輸出端用預先開好的第二個附加寫出口、不經佇列），**之後**才盡力請全部輸出端 flush，全部共用一個期限（`fatalFlushTimeout`，預設 1 秒）；在輸出端自己的寫者執行緒上，flush 立刻略過。行用 `Install` 預先保留的容量組，不配置（超過容量才配置）。同一執行緒再入只寫主控台。不丟例外。「寫出」＝交給 OS，不是到磁碟。斷言失敗時（`takeAssertions`）先寫一筆 category `assert` 的 `Fatal` 紀錄，再交給之前的處理器（abort 仍由 Base 做）；`std::terminate` 時（`takeTerminate`）先寫一筆 category `terminate` 的 `Fatal` 紀錄（飛行中例外的 `what()`；不是 `std::exception`、或沒有例外時各寫一句固定的話），再交給之前的處理器（使用者 2026-11-20：日誌機制自己記「出了什麼事」）。World 壞掉（E1）不走這條路：產品的 main 要記，就是經 ③ 或 ④ 的一筆紀錄，給人看；壞在哪一步由 trace 追（使用者 2026-11-21「日誌只是看。trace才是追問題。」）。作者經 ④ 寫的 `Fatal` 不受限制。致命紀錄在 trace 有接收者時帶一個 `trace_path` 欄位：E0 那一刻這條執行緒的段路徑，從 Base 的門面讀進固定大小的緩衝（不配置、不上鎖；給的欄位超過 32 個時不加）；仍恰好一筆 `Fatal`（[trace 宣告](trace-declaration.zh-Hant.md) D4 (a)，使用者 2026-11-21 核准）。**不保證**：記憶體已壞、堆疊用盡、訊號、寫者卡在 OS 時的時限（同一個檔的第二個 handle 可能一起卡）；Windows 上 `std::set_terminate` 是每條執行緒各一份（UCRT），`Install` 只裝在呼叫它的執行緒，別的執行緒走到 `std::terminate` 時沒有那一筆（要的話由起執行緒的地方另裝，`engine/core/threads` 的事）。
守衛：`logging.fatal`（含重複鍵的致命紀錄）、`logging.alloc`（致命路徑不配置）、`logging.file`（寫者被擋住時致命那一行照樣先寫）、`logging.terminate.std`／`.other`／`.none`（行程真的走到 `std::terminate`，檔裡恰好一筆）、`logging.append`（兩個附加寫出口各 10 萬行不交錯，L1 三平台）；突變 `lg-fatal-*`、`lg-assert-*`、`lg-terminate-*`。

<a id="lg-3"></a>
**LG-3 不改模擬的值。** `Forward` 登記的 System 參數只有 `MessageReader<D::Message>` 與 `Res<LogWorldLabel<D>>`（編譯期確認都是唯讀），永遠回成功（描述器與輸出端的例外在 System 裡接住，改寫成一筆 category `logging` 的 `Error` 紀錄並計數，同一幀之後的訊息照記）。日誌不寫 ECS 元件、`Message`、`Event`、`Commands`；唯一放進 World 的是準備時放的 `LogWorldLabel<D>`（自己的 Resource，之後不變）。開不開日誌，遊戲自己的元件與 Resource、遊戲的讀者讀到的訊息逐位元相同。登記、保留、指紋、stamp 會變（宣告 5.11），那是寫明的。
守衛：`world_logging.forward`（遊戲的讀者讀到的相同、描述器丟例外）、編譯失敗樣本（`tests/common/world_logging/compile_fail/ForwardSample.cpp`，`GYO_WORLD_LOGGING_REJECT_WRITE`／`_DESCRIPTOR`）、`scenario.segment3`（S3-3）；突變 `lg-forward-*`。

<a id="lg-4"></a>
**LG-4 不認得產品與其他 Module。** `engine/modules/logging` 只含入 Base、Json、Os、Threads、Time 與自己；公開標頭只含入 Base 與自己。`engine/modules/world_logging` 只含入 Base、Logging、Kernel、Orchestration 與自己。兩個都不認得任何訊息型別、產品的詞（描述器在發出訊息的 Module 那邊）。
守衛：ctest `logging.guard`（`tests/common/logging/logging_guard.py`：三條含入規則、正例、目錄不存在時失敗）與 `logging.guard_selftest`；突變 `lg-guard-*`。

<a id="lg-5"></a>
**LG-5 JSON lines 是資料契約。** 一筆一行，每行是 JSON Module `Parse` 讀得回的嚴格 JSON。封套的固定鍵依序：`schema`（本版 `1`）、`kind`（紀錄是 `log`）、`level`、`category`、`wall`、`mono_ns`、（有時）`world`、（為真時）`world_interrupted`、`text`、`fields`（物件，鍵照寫入順序）。`wall` 照 `strftime` 的 `%z`：`2026-11-20T10:40:11.007+0900`。非有限數寫成字串 `"NaN"`、`"Infinity"`、`"-Infinity"`，負零照 `-0.0`。結尾行 `kind=end`，鍵 `schema`、`kind`、`mono_ns`、`written`、`dropped_full`、`dropped_closed`、`fatal_direct`，是檔案的最後一行。同一筆寫同一個鍵兩次、或 JSON 表示不了（不合法 UTF-8），那一筆改成一筆 category `logging` 的 `Error` 紀錄（同 `world`；`text` 帶 JSON 錯誤碼，不說哪個欄位）。加鍵不升版，改鍵的意義或型別才升版；產品自己的內容版本放在自己的 `fields`。致命紀錄可帶 `trace_path`（LG-2）；trace Module 的紀錄是一般的紀錄，category `span.` 開頭（[trace.zh-Hant.md](trace.zh-Hant.md) TR-5），不改封套。文字格式（主控台）不是契約。檔案用 `LineEndings::AsWritten`，三平台位元組相同。例外：致命路徑在握著主控台的鎖時寫主控台，可能和另一行交錯（只限主控台）。
守衛：`logging.format`（golden、NaN／±Inf／負零、跳脫、不合法 UTF-8、結尾行）、`logging.file`、`scenario.segment3`（S3-4：每行 `Parse` 讀回、逐欄比對）；突變 `lg-json-*`、`lg-dup-key-not-replaced`、`lg-dupkey-replacement-not-to-taker`、`lg-fatal-dupkey-kept`。

<a id="lg-6"></a>
**LG-6 檔案輸出端的傳輸。** 寫紀錄的執行緒只在短鎖下把組好的行推進有上限的佇列（預設 65536 行），不做檔案 I/O、不等寫者；佇列滿了丟並計數。一條寫者執行緒（`Threads::RoleThread`）只在「推進空佇列」、flush、關閉時被通知喚醒，不用計時器；每批或每 64 KiB 交給 OS 一次，邊界都在行尾。件數守恆：交給它的＝`written`＋`dropped_full`＋`dropped_closed`（關閉之後；寫失敗後取走的行仍算 `written`，失敗由 `Good()` 表示）；致命那一行不經佇列，另計 `fatal_direct`、不進這個式子。同一個寫的人經佇列的行照推入順序；致命那一行可能在它之前推入的行之前（讀的人用 `mono_ns` 排）。`Close` 寫完全部已收的行，再寫結尾行。
守衛：`logging.file`、`logging.append`、`logging.alloc`（寫者執行緒配置失敗時照樣寫出、不結束行程）、`scenario.segment3`（S3-4 的件數）；突變 `lg-file-*`、`lg-writer-oom-terminates`。

<a id="lg-7"></a>
**LG-7 篩選。** 每個輸出端各自的篩選：收哪些 category（`include`、`exclude` 前綴）在 `Install` 定、之後不變；最低層級（預設與每個前綴，最長的前綴勝）在 `Install` 給初值，執行中可用 `SetLevel` 改，下一筆起生效（宣告第 13 節第 6 題）。④ 被篩掉的紀錄不組、不配置、不讀時鐘；① 被篩掉時要推游標，不承諾不配置。
守衛：`logging.logger`、`logging.alloc`；突變 `lg-filter-*`、`lg-setlevel-*`、`lg-alloc-refused-reads-clock`。

<a id="lg-8"></a>
**LG-8 一個行程一個 Logger。** 同時只裝一個（第二個是斷言）；`Install` 打不開檔案或起不了寫者時回 `OpenFailed`／`StartFailed`，並先關掉已開的輸出端；卸下時先還原接手的處理器（`Base::Log`、斷言、`std::terminate`），再依序關掉每個輸出端。沒裝時 ③④ 寫一行到標準錯誤（③ 是 `Base::Log` 自己的預設處理器）。`Logging::Installed()` 回答有沒有裝（`Enabled` 在沒裝時恆真，不能拿來判斷；trace Module 用它決定寫不寫）。
守衛：`logging.logger`、`logging.file`（開不了檔）、`logging.fatal`（處理器的還原）；突變 `lg-install-*`、`lg-baselog-not-restored`、`lg-assert-not-restored`、`lg-terminate-not-restored`。

<a id="lg-9"></a>
**LG-9（修訂 3 退役）。** 原本是 logging 的 span。使用者 2026-11-21 定日誌和 trace 是兩個 Module，span 歸 trace Module（另寫宣告）；logging 不再有 span，條號不重用。

<a id="lg-10"></a>
**LG-10 依賴。** `gyo_logging` 公開只連 `GYO::Base`、私有只連 `GYO::Json`、`GYO::Os`、`GYO::Threads`、`GYO::Time`（不交給使用者）；`gyo_world_logging` 只連 `GYO::Logging`、`GYO::Kernel`、`GYO::Orchestration`；沒有外部組件（之後有的話放 `src/adapters/<組件>/`）。
守衛：`gyo_expect_links`（`tests/common/logging/CMakeLists.txt`、`tests/common/world_logging/CMakeLists.txt`）；突變 `lg-link-kernel`。

<a id="lg-11"></a>
**LG-11 Sink 不丟例外。** `Sink` 的函式都是 `noexcept`；內建的輸出端寫失敗只計數（`JsonLinesFileSink::Good()` 變 false、`ConsoleSink::Failed()`、`MemorySink::Lost()`）。寫失敗只在輸出端自己身上表示，不另送標準錯誤或別的紀錄；佇列滿了、關了之後丟的行是照設計的上限丟掉（E3），計數並寫進結尾行（LG-6）。
守衛：`tests/common/logging/LoggerTests.cpp` 的 `static_assert`；突變 `lg-sink-write-may-throw`。

### Replacement Criteria

- **RC-L1 一筆都不能丟**：有 Feature 要日誌或 trace 不丟（例：拿 trace 重播）——停下交使用者（LG-6 的變更）。
- **RC-L2 接線重複**：三個以上的產品重複同樣的 `Forward` 接線——重看預設接線（登記單位）。
- **RC-L3 成本**：判定 7 量到的成本讓某個點名的 Feature 超出它的預算，或量測顯示格式化、佇列鎖或取本地時間是熱路徑的主要成本——停下重看（含外部庫、編譯期拿掉層級、每執行緒緩衝、快取 UTC 偏移）。本批的量：直接寫一筆進佇列中位數 3.2 µs、p99 23 µs；經 `Forward` 一筆約 6.8 µs；被篩掉 47 ns（Intel Mac，Release）。
- **RC-L4（修訂 3 已發生）**：trace 已分成自己的 Module（使用者 2026-11-21），本條退役。
- **RC-L5 致命路徑不夠**：兩個附加寫出口的並行寫測試在某平台不過、觀察到致命紀錄在 `WriteFatalRecord` 回傳後沒進檔、或寫者卡在 OS 讓致命路徑也卡住——改成另一個致命檔、預先配置的寫出。
- **RC-L6 描述器的形狀不夠**：有訊息需要執行期才知道的格式而型別做不到——重看 `Forward`（可能要 Kernel 給 System 帶狀態的方法，交使用者）。
- **RC-L7（修訂 3 移到 trace）**：System 的 span 屬於 trace Module 的宣告，本條退役。
- **RC-L8 入口長出來**：要第六種進日誌的路（現在是 ①②③④⑥ 五條）——停下，LG-1 的變更。
