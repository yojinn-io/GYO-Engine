# 事前宣告：日誌機制（含 trace）（`v8-arch-gyo2-logging`）

更新：2026-11-20（修訂 1）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：等動工前核准。**

經過：修訂 0（2d275db9）送 Fable 5.2／high 敵對式檢查（general-purpose、只讀），必改 9、建議改 10、小問題 10（證據 `declaration-r0-adversarial-check.md`）。修訂 1 全部照改，處理對照在第 16 節；修訂 1 沒有再送檢查。修訂 1 的主要變化：致命那一行改成「先寫、不等任何東西」，由預先開好的第二個寫出口寫（第 5.6 節）；`Forward` 改成做得出來的形狀（第 5.3 節）；拆成兩個 Module，照 `time`／`world_time` 的先例（第 5.1 節）；直接寫一筆的 API 列進入口清單（第 3 節）；第 13 節從 6 題改成 8 題，加了「GYO 自己加的、和別家不同的」與回滾的交界。

性質：實作段第 6 組的事前宣告（AGENTS §2 的基盤 Architecture、§3 的 Architecture Delta、新 Component 的 Design Invariants 與 Replacement Criteria、§14 的判定）。和第 4 批（權威＋回滾）並行。

用詞：整條線分「設計段／實作段」；每批的核准分「動工前核准／合併前核准」；一批之內的實作步驟叫「第 N 步」。標「**卡片照 L17 判**」的是這張卡自己的判斷（名字照命名規範、細節照別家原始碼或 `gyo2` 的先例、批次內的先後），**不是使用者的話**；本卡新加的詞第一次出現時標「本卡加的詞」。交使用者的只有第 13 節。

## 0. 根據與讀的版本

**程式**：`gyo2` 8ec005f2（#244 編排層骨架、#253 時間 Module＋事件、#252 JSON Module 之後）。講現況的句子都來自讀程式（行號照這個版本）。

**決定**（HANDOFF 決定表，文件分支 `origin/claude/gyo2-time-design` 57c8dc32；原話以 HANDOFF 為準）：

| 代號 | 本頁怎麼用 |
|---|---|
| `v8-arch-gyo2-world-events-not-via-event`（含 11-19 補充三點） | 兩個入口、最後一道直接呼叫；第 3 節 |
| `-no-separate-eventbus`（含兩次加註） | 不另做 EventBus；日誌是 World 裡面機制的訂閱者 |
| `-event-delivery-rule` | 警告的送法（`TimeDiscarded` 是 `Message`） |
| `-broken-world-ends-product`（D2，含同日更正） | 第 4 點：錯誤回 runner、編排層不寫日誌、日誌是獨立機制、「日誌是否使用是作者控制參數開啟的」；第 5 點：寫日誌不依賴壞掉的 World；不做商業級崩潰日誌 |
| `-json-module-declaration-approved` 第 3 題 | JSON `Writer` 對 NaN、無限大回 `NotRepresentable`，表示法由本批定 |
| `-json-adapters-dir`（含推廣） | 碰外部組件的程式放 `src/adapters/<組件>/` |
| `-merge-needs-explicit-word` | 合併只認明確的「合併」 |
| `-module-plugin-owner-definition`（#240） | Module＝自己維護、介面是我們的 |
| `-infrastructure-before-products`、`-json-early` | 日誌在搬產品之前、在 JSON 之後 |
| `-confirm-only-differences`、`-batch3-no-xhigh` | 只交分歧；全程 high |

**文件**：[批次表](rebuild-batch-plan.zh-Hant.md)（文件分支）第 6 組「日誌機制」一列（範圍）；[編排層](orchestration.zh-Hant.md) O-3、O-4、O-9、O-11；[時間＋事件的宣告](time-events-declaration.zh-Hant.md)第 5 節；[事件游標的宣告](event-cursor-declaration.zh-Hant.md)第 100、173 行（丟棄記錄「World 記、runner 在交換點取走」）、「從沒跑過的讀者」一列；[回滾份](rollback-module-declaration.zh-Hant.md) 7.1（第 254～260 行，跨 schedule 的訊息）；[權威份](authority-module-declaration.zh-Hant.md)第 111 行；[JSON](json.zh-Hant.md) J-2、J-5；`rebuild-naming.zh-Hant.md` N11～N22。

**master 的參考（只當參考、不搬回）**：`master:engine/trace/`（`TraceSink`：有上限的佇列＋一條寫者執行緒、`FlushNow` 只 try-lock、寫者執行緒上回 false、`Close` 的結尾行、件數守恆；`master:docs/architecture/trace.md` 的 DI 1～8 與 RC 1～3）；PvP 的 `LogFile`（兩個時鐘的時間戳、容量 65536、每行 flush、`FlushOnCrash`）與 `MovementTraceWriter`（一行一筆 JSON、`schema_version`、`started_ns`＋`time_ns` 表示一段時間）；`master:engine/platform/sdl/src/SdlLogBridge.cpp`。

**證據**（主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-logging/`，git 不追蹤）：`recon-reference-engines.md`（Bevy、flecs、Unity，Opus 5.5／high 偵察）、`declaration-r0-adversarial-check.md`（修訂 0 的檢查，Fable 5.2／high）。

---

## 1. 這一批做什麼（先用遊戲講）

- **平常**：時間 Module 一幀補不完，扣掉 59 步，發一筆 `TimeDiscarded`（`Message`）。時間 Module 不寫字串；日誌的 System 在每幀最後讀它，寫成「哪個 World、丟了多少、為什麼」一筆紀錄（誰接線是第 13 節第 1 題）。訊息緩衝滿了丟最舊的、生成被拒，Kernel 記在 World 上（`DroppedMessages`、`RejectedSpawn`），runner 在兩次 Run 之間交給日誌取走。
- **致命**：Match 的模擬 World 裡某個 System 回錯誤或丟例外，World 中斷，`Product` 結束（O-11）。runner 拿到 `WorldFailure`，直接呼叫日誌的 `WriteFailure`：日誌在呼叫的執行緒上，先把它寫到主控台，再用一個開機時就開好的寫出口寫進檔，**不等任何東西**；寫完才回頭請寫者把之前排隊的行也寫出去（最多等一段時間）。這一路不經 Kernel、不經事件、不讀壞掉的 World。
- **trace**：「玩家 3 的移動指令排隊 2 筆」這種給分析工具看的一行一筆 JSON，和日誌走同一套紀錄與輸出端，用 category 分到另一個檔；「這次送出花了多久」用一個 span，結束時寫一筆帶開始時間與長度的紀錄。
- **測試**：輸出端可以換成記在記憶體裡的，測試直接讀；致命路徑有情境測試（World 真的壞掉、runner 真的呼叫）。

## 2. 範圍

### 2.1 這一批做的

| 代號 | 內容 |
|---|---|
| L1 | 兩個新 Module（第 5.1 節）：`engine/logging`（不認得 ECS：紀錄、`Logger`、輸出端、格式、直接寫一筆、span、致命時寫一行的底層）與 `engine/world_logging`（World 的部分：`Forward`、`TakeKernelDiagnostics`、`WriteFailure`）。Module 登記檔各一列（都 `library_only=yes`） |
| L2 | `Record`、層級、欄位；`Logger`（一個行程一個）的安裝（回 `Result`）與卸下 |
| L3 | 輸出端 `Sink` 介面與三個內建：主控台（文字、同步）、JSON lines 檔（有上限的佇列＋寫者執行緒＋預先開好的致命寫出口）、記憶體（測試用） |
| L4 | 紀錄格式：JSON lines 的封套（固定鍵、`schema` 版本、結尾行）、NaN 與無限大的表示法（第 13 節第 3 題）、重複鍵與寫出失敗的處理；文字格式 |
| L5 | 入口（第 3 節的清單）：`Forward`、`TakeKernelDiagnostics`、`Base::Log` 的處理器、直接寫一筆（`Logging::Write`） |
| L6 | 致命：`WriteFailure`；斷言處理器與 `std::terminate` |
| L7 | trace：span；trace 與日誌的分流（category 篩選） |
| L8 | 測試、情境測試（致命路徑、決定性）、連結守衛、突變清單 |
| L9 | 文件：新頁 `target/v2/logging.zh-Hant.md`（Design Invariants LG-1～LG-11 與 Replacement Criteria）、v2 README 索引、dev_log、合併前核准頁；O-11 的例子更正（第 13 節第 4 題，核准後才改） |

### 2.2 不在這一批

| 項目 | 留到 | 為什麼 |
|---|---|---|
| 每個 System 自動一個 span（Bevy 的 `system` span、Unity 的 `ProfilerMarker`） | 有產品要量 System 的時間時 | 要在 Kernel 的 Executor 加掛點（改 Kernel；第 4 批正在改）；沒有點名的 Feature。RC-L7 |
| Chrome trace／Tracy 的匯出 | 有量測需求時 | 沒有點名的 Feature |
| 編譯期拿掉某些層級 | 量測顯示呼叫成本是問題時 | RC-L3 |
| 商業級崩潰日誌（minidump、呼叫堆疊、`SIGSEGV`） | 不排 | D2 第 4 點 |
| 「寫到磁碟上」（fsync） | 不排 | `Os::Files::FileWriter` 只交給 OS、不 fsync（`Files.hpp:78-83`）；行程死了內容還在，斷電不保證。要 fsync 是 `engine/os` 的變更 |
| 紀錄帶執行緒名 | 有 Feature 要時 | 寫紀錄的那一刻沒有 API 問「我是哪個角色執行緒」（`engine/os/Thread.hpp` 只有 `SetCurrentName`）；要的話是 `engine/threads` 的變更 |
| SDL 的日誌轉進 `Base::Log` | SDL 那一批 | master `SdlLogBridge` 是參考 |
| 動態插入（`InsertSystems`）時用 `Forward` | 有需要時 | `InsertSystems` 只給 `Registrar`、沒有 `WorldSetup`（`Product.hpp:127`） |
| World 建立、替換時往新 World 寫 `Message` | 讀檔那一批 | `-world-events-not-via-event` 補充 (3) |
| 回滾 World 裡固定步發的 `Message` 怎麼記 | 第 4 批對齊（第 12、14 節、第 13 節第 7 題） | 回滾份 7.1 的跨 schedule 規則 |
| Kernel 指紋報告手寫 JSON 改用 JSON Module | 主會話另議 | 要動 Kernel 的依賴 |

## 3. 入口（使用者已定兩個入口與最後一道；這裡列實際的全部）

使用者定的（`-world-events-not-via-event` 與補充三點、D2 第 4、5 點）：World 健康時的通知走事件、日誌訂閱；致命錯誤與 World 自己的生滅不走事件，結構化錯誤回 runner；最後把它寫出去的那一道直接呼叫、不經任何機制。

**現況對照**（`gyo2` 8ec005f2）：

| 會進日誌的東西 | 現在怎麼出來 | 日誌怎麼拿 |
|---|---|---|
| `TimeDiscarded` | `Message`（`engine/world_time/src/WorldTimeModule.cpp:33`、`:43`、`:109`） | 訂閱：`Forward`（第 5.3 節） |
| 訊息的丟棄記錄 `DroppedMessages` | **不是 `Message`**：Kernel 的診斷紀錄，World 記、兩次 Run 之間 `World::TakeDiagnostics<DroppedMessages>()` 取走（`engine/kernel/include/engine/kernel/World.hpp:135-150`、`:317`；游標宣告第 100 行，已核准）；World 中斷後仍讀得到（`World.hpp:296-298`） | runner 在兩次 Run 之間呼叫 `TakeKernelDiagnostics`（第 5.4 節） |
| 生成被拒 `RejectedSpawn` | 同上（`World.hpp:101-131`） | 同上 |
| 作者自己的 `DiagnosticsWriter<R>` 紀錄 | 同上（`Params.hpp:709-733`） | 作者取走後自己用 `Logging::Write` 寫 |
| 編排層的警告 | `Engine::Base::Log` 五處：O-9 動態插入（`engine/orchestration/src/Product.cpp:720`、`:729`）、替換失敗（`:537`）、替換時的例外（`:656`、`:659`）。`DropOldFirst` 失敗時，編排層先記這一筆 `Warning`，再回 `WorldFailure` | `Base::Log` 的處理器（第 5.5 節） |
| `WorldFailure` | `RunGameInit`／`RunFrame`／`RunSchedule`／`ReplaceWorld` 回的錯誤上（`OrchestrationError::failure`）；**System 丟例外時不回錯誤，例外往外丟**（`Product.cpp:426-438`、`WorldExecutor.hpp:29-30`），之後 `Product::Failure()` 讀得到（`Product.hpp:132`）；`Product::Run()` 只回 `RunnerExit`（`Product.cpp:495-503`）；結束後每次拒絕都帶同一份（`Product.cpp:385-393`） | runner 或 `main` 直接呼叫 `WriteFailure`（第 5.6 節） |
| 產品的 trace、span、作者自己的紀錄 | （沒有） | 直接寫一筆：`Logging::Write`、`Logging::Span`（第 5.5 節） |

**交界**：O-11 和 `-world-events-not-via-event` 補充 (1) 把「訊息的丟棄記錄」寫成「照常經事件」，和已核准的游標宣告、已合併的實作不一致。第 13 節第 4 題。

**入口的完整清單**（修訂 0 只列了四條，漏了直接寫一筆；這一張是全部）：

```text
World 健康時（平常）
  ① Message ──Forward──→ ┐                              （訂閱：使用者定的入口一）
  ② Kernel 的診斷紀錄 ──runner 在兩次 Run 之間取走──→ ┤   （游標宣告已定的「runner 取走」）
World 外的程式                                          ├──→ Logger ──→ Sink（主控台／JSON lines 檔／記憶體）
  ③ Engine::Base::Log ──處理器──→ ┤                    （既有的門面；編排層五處在用）
  ④ Logging::Write／Span（直接寫一筆）──→ ┘            （trace、span、作者自己的紀錄）
致命時
  ⑤ WorldFailure ──runner／main 直接呼叫──→ WriteFailure ──→ 主控台＋致命寫出口（不等任何東西）
  ⑥ 斷言失敗 ──處理器──→ 同 ⑤ 的寫法；std::terminate ──處理器──→ 只請寫者 flush（有期限）
```

使用者說的「兩個入口」是 ①（平常）與 ⑤（致命）。②是「平常」的第二條路（第 13 節第 4 題）。③④是 World 外、或作者自己要寫一筆時用的。**Engine 的 Module 在 System 裡報警告，不用 ③④，用 `Message`**（`-no-separate-eventbus`）；作者自己的 System 用不用 ④ 是第 13 節第 5 題。

## 4. 別家怎麼做（同一種技術比）

本機原始碼：Bevy 6a911653f、flecs 09790b5f7、Unity Entities 4630ff5、Netcode c4cabf4。完整的行號在 `recon-reference-engines.md`。**本機沒有 `tracing`、`tracing-subscriber`、`serde_json`、`Unity.Logging`、spdlog、quill 的原始碼**；它們的內部行為只寫「沒查證」，不當任何一條判法的唯一根據。

### 4.1 ECS 引擎的對照

| 技術 | Bevy（`bevy_log`＝`tracing` 0.1） | flecs（`log.c`、`os_api`） | Unity（Entities、Netcode） | 一致／分歧 | GYO 本批 |
|---|---|---|---|---|---|
| 層級 | 5 級（`lib.rs:157-169`） | 整數，含 `fatal`=-4（`os_api.h:368-374`） | Netcode 5 級含 Exception（`NetDebug.cs:338-362`） | 大致一致 | 6 級＝5 級＋`Fatal`（5.2） |
| 篩選何時定 | 啟動時（`EnvFilter`，`lib.rs:173-224`）；執行中改：沒找到（`lib.rs:325`、`:422`） | 編譯期 `FLECS_LOG_0..3`（`log.h:336-452`）＋執行中一個全域整數（`log.c:957-963`） | Netcode：每個 World 的 singleton，System 每幀寫回（`NetDebug.cs:189`、`:322-332`） | **分歧** | 第 13 節第 6 題 |
| 按 category 篩 | 有（target） | 沒有 | 沒有 | 分歧 | 有（輸出端各自的前綴清單） |
| 預設輸出 | stderr（`lib.rs:363-368`；註解寫 stdout，和程式不一致 `:75-76`） | stdout（`os_api.c:153-162`） | Unity 的標準日誌（`NetDebug.cs:367-414`） | 分歧 | stderr（照 `Base::Log`） |
| 多個輸出端 | 可疊多個 layer（`lib.rs:226-249`） | 一個 callback（`os_api.h:229-234`） | 引擎日誌＋每條連線一個封包檔（`PacketDumpLogger.cs:54-59`） | 分歧 | 多個 `Sink`（5.7） |
| JSON 日誌 | 沒有內建（只有 Chrome trace JSON，`lib.rs:332-354`） | 沒有 | 沒有 | 一致（都沒有） | JSON lines（F3 的根據是 master 的 PvP trace） |
| 誰寫、緩衝 | 呼叫的執行緒；Tracy 緩衝**無上限**（`lib.rs:251-255`）；Chrome trace 在 `FlushGuard`（Resource）被丟掉時才寫（`:63-71`） | 呼叫的執行緒直接 `fputs`、沒有鎖、沒有 `fflush`（`os_api.c:153-281`） | 封包檔 32 KB 緩衝、滿了當場同步寫、不丟（`PacketDumpLogger.cs:61-71`） | 一致：**沒有一家有上限、會丟的佇列** | 主控台同步；檔案用有上限、會丟的佇列（系譜是 master `TraceSink`；第 13 節第 2 題 (a)） |
| 致命時 | 錯誤處理器**直接**呼叫 `log::error!`（`error/handler.rs:119-199`）；panic hook 用 `eprintln!` 繞過 subscriber（`lib.rs:307-317`）；致命時 flush：沒找到 | `ecs_abort` 直接經 `log_` 印再 `abort`（`log.c:682-703`）；flush：沒有 | System 的例外 `Debug.LogException` 後繼續下一個 System（`ComponentSystemGroup.cs:715-741`） | 一致：**直接呼叫、不經 ECS 事件**；**沒有一家保證 flush** | 直接呼叫（使用者已定，和三家一致）；先寫致命那一行再等寫者、預先開好的寫出口是 GYO 加的（第 13 節第 2 題 (b)） |
| span／trace | 日誌與 span 同一套；內建 App、幀、schedule、System 的 span（`sub_app.rs:577-586`、`schedule.rs:447`）；Chrome JSON、Tracy | `perf_trace_push/pop` 兩個 callback，自己不計時（`os_api.c:515-533`） | `ProfilerMarker` 每個 System 一個、和日誌不相連（`SystemState.cs:289-292`） | **分歧** | 第 13 節第 8 題 |
| 結構化欄位 | event 帶 level、target、file、line 與 key=value（`tracing` 的模型，沒查證） | 只有 level、file、line、msg（`os_api.h:229-234`） | 只有字串（`NetDebug.cs:367-414`） | 分歧 | 有序的鍵值欄位＋固定鍵（5.12） |
| 標出哪個 World、哪一幀 | 沒有；`FrameCount` 不自動進日誌 | 沒有（callback 沒有 world 參數） | 日誌沒有，Netcode 手寫 `[{WorldName}]` 前綴（`GhostReceiveSystem.cs:1386-1387`）；`ProfilerMarker` 自動帶 World 名（`SystemState.cs:290`） | 一致：**都沒有內建** | `Forward` 與取走的紀錄自動帶 `world`；不帶 tick（5.9）。比別家多一欄，卡片照 L17 判：GYO 一個 `Product` 多個 World，Netcode 的手寫前綴是同一個需要 |
| NaN、±Inf 進 JSON | 沒開 json；沒查證 | 字串 `"NaN"`、`"Inf"`，**−Inf 的負號丟了**（`strbuf.c:81-95`） | 沒找到 | 資料不足 | 第 13 節第 3 題 |
| 日誌在不在 ECS 裡 | 日誌在 World 外；`FallbackErrorHandler` 是 Resource、參與排程衝突（`handler.rs:116`）；`LogDiagnosticsPlugin` 是 System、寫自己的 Resource | 完全在 ECS 外（`os_api.c:26`） | Netcode 的 `NetDebug` 是 singleton、產生 job 依賴（`NetDebugSystem.cs:30-66`） | 分歧；**三家都沒有讓日誌改模擬狀態的路** | 日誌的 System 只讀（LG-3） |
| ECS 裡的警告怎麼到日誌 | 直接呼叫巨集（`virt.rs:255-260`）；**`Messages` 丟棄不記**（`message_cursor.rs:120-129` 的 TODO） | 直接呼叫巨集（`src/` 內 317 處） | 直接呼叫 | 一致：**都是直接呼叫** | 發事件、日誌訂閱（使用者已定；所以多出第 13 節第 1、5 題） |
| 外部庫 | `tracing` 0.1＋`tracing-subscriber` 0.3.20 | 自己寫 | `UnityEngine.Debug`；`Unity.Logging` 選用、本機沒開 | 分歧 | 第 13 節第 8 題 |

「World 外的程式每幀把 ECS 的診斷搬出來」：三家都沒有這個形狀。最接近的是 Bevy 的 `LogDiagnosticsPlugin`（World 裡的 System 隔一段時間讀 `DiagnosticsStore` 再呼叫 `info!`）。GYO 的「World 記、runner 取走」是游標宣告已核准的形狀，本批照它。

### 4.2 日誌庫的對照（有佇列、背景寫者、致命 flush 這幾種技術的，才是同一種技術）

**全部來自知識（公開文件），本機沒有原始碼、沒查證**；只用來說明分歧，不當唯一根據。要查證就要下載原始碼（第 13 節第 8 題）。

| 技術 | spdlog（MIT） | quill（MIT） | master `TraceSink`＋`LogFile` | GYO 本批 |
|---|---|---|---|---|
| 佇列滿了 | 非同步模式可選：擋住呼叫者、蓋掉最舊的、丟掉新的 | 可選：擋住、或丟掉並計數 | 丟並計數（trace DI 1）；`LogFile` 把容量開到 65536 | 丟並計數；`Warning` 以上要不要不丟，第 13 節第 2 題 (a) |
| flush 的時機 | `flush_on(level)`：某層級以上每筆 flush；定時 flush | 背景執行緒定時 | 每批寫完；`LogFile` 每行 | 每批寫完 |
| 當機時 | 結束時 `shutdown` 寫完佇列；訊號處理要自己裝 | 有當機處理器，收到訊號時把佇列寫完 | `FlushOnCrash`：斷言寫進檔、`std::terminate` 先 flush | 致命那一行另走預先開好的寫出口，不靠佇列（5.6） |

### 4.3 外部庫用不用（第 13 節第 8 題）

| 選項 | 授權 | 帶來什麼 | 和 `gyo2` 現有的重疊 | 判斷 |
|---|---|---|---|---|
| spdlog | MIT | 多個輸出端、非同步佇列、溢出政策、檔案輪替、`fmt` 格式 | 佇列與寫者＝master `TraceSink`；格式＝JSON `Writer`；門面＝`Base::Log` | 不用：要的丟棄計數、結尾行、致命那一行另走一個寫出口，都得在它外面再包一層；它的佇列與我們的件數守恆（LG-6）要對齊。（只把組好的行交給它，數字格式不會重複——修訂 0 的這個理由偏弱，拿掉） |
| fmt（或 C++20 `std::format`） | MIT | 格式化字串 | 文字格式只是幾個鍵值的串接 | 不用 |
| quill | MIT | 低延遲（producer 只複製參數、背景格式化） | 同 spdlog | 不用：沒量到問題之前沒有理由；RC-L3 重評 |
| 自己寫（建議） | — | 照 master `TraceSink`（約 300 行、有測試與突變的設計）＋JSON `Writer` | — | 不下載 |

## 5. 設計（Contract）

### 5.1 位置與名字（卡片照 L17 判：照 `time`／`world_time` 的先例與 N11～N17、N20）

- **兩個 Module**，照 `gyo2` 已有的拆法（`engine/config/modules.csv`：`time`＝不進 World 的函式庫、`world_time`＝World 的部分）：

| Module | 目錄、namespace、target | 內容 | 依賴（公開；私有） |
|---|---|---|---|
| `logging` | `engine/logging/`、`GYO::Engine::Logging`、`gyo_logging`／`GYO::Logging` | `Record`、`Level`、`Fields`、`Logger`、`Sink` 與三個內建、JSON lines 與文字格式、`Write`、`Span`、`WriteFatalRecord`（致命時寫一行的底層）、斷言與 `std::terminate` 的處理器 | `GYO::Base`；私有 `GYO::Json`、`GYO::Os`、`GYO::Threads`、`GYO::Time` |
| `world_logging` | `engine/world_logging/`、`GYO::Engine::WorldLogging`、`gyo_world_logging`／`GYO::WorldLogging` | `Forward`、`LogWorldLabel`、`TakeKernelDiagnostics`、`WriteFailure`、`WithFailureLog` | `GYO::Logging`、`GYO::Kernel`、`GYO::Orchestration`；私有無 |

- 理由：致命時寫一行、輸出端、格式都不需要 Kernel；不用 World 的程式（工具、之後的伺服器程序）只連 `logging`。名字用 `Logging` 不用 `Log`：`Engine::Base::Log` 已是 Base 的函式名。兩個都 `library_only=yes`、`default=no`、不做登記單位（`Logger` 一個行程一個、由 `main` 裝；`Forward` 是接線的人在自己的 `build` 裡呼叫）。之後要「預設接線」時才可能變成登記單位（RC-L2）。
- 沒有外部組件，所以本批沒有 `src/adapters/`；之後若接外部日誌庫，照 `-json-adapters-dir`。
- 檔名＝主要型別名（N16）。

### 5.2 紀錄、層級、Logger

- `Record`：`level`、`category`、`text`、`wall`（牆上時鐘的毫秒與 UTC 偏移）、`monoNs`（`Engine::Time::NowNs()`，engine 的時間基準，`engine/time/include/engine/time/MonotonicClock.hpp:8-9`）、`world`（有 World 時）、`fields`、`kind`（`log`、`span`）。不帶執行緒名（第 2.2 節）。
- 層級：`Trace`、`Debug`、`Info`、`Warning`、`Error`、`Fatal`。`Trace` 只是「最詳細的層級」（Bevy／tracing 的用法），和 category 的 `trace.` 前綴（分流）無關；兩者獨立（5.13）。
- `Fields`：有序的鍵值（值是 `int64`／`uint64`／`double`／`bool`／字串）。**同一筆裡寫同一個鍵兩次，那一筆改成一筆 `Error` 紀錄**（同 5.12 的寫出失敗），不讓一行讀不回（J-2 的重複鍵）。一筆的欄位不多，逐一比對鍵的成本小。
- `Logger`：一個行程一個（Bevy 的全域 subscriber 不能裝兩次 `lib.rs:199-203`、flecs 的 `ecs_os_api` 全域 `os_api.c:26`；Unity 本機沒有原始碼、沒查證）。`Logging::Install(LoggerOptions)` 回 `Result<LoggerHandle, LoggingError>`：檔案開不了（`OpenFailed`）、寫者起不來（`StartFailed`）是 Runtime Error（照 master `TraceSink::Open`）。`LoggerHandle` 解構時卸下並還原接手的處理器。
- 先後：`Install` 要在其他執行緒起來之前（`SetAssertionHandler` 的規定，`Assert.hpp:79`；`Install` 自己起的寫者執行緒在處理器裝好之後才起）；卸下要在其他執行緒都停了、不會再進處理器之後（`Log.hpp:59-62` 把這個責任交給呼叫的人）。同時裝第二個是斷言。
- 沒裝 `Logger` 時：③④ 照 `Base::Log` 的預設（一行到 `std::cerr`）；⑤ 只寫主控台。
- `Logger` 在第一個 `Product` 之前裝、在最後一個 `Product` 之後卸（`main` 擁有），回主選單（另建 `Product`，O-3）時日誌照常。

### 5.3 ① 訂閱 `Message`：`Forward`

Kernel 的 System 沒有狀態：`AddSystem<auto Fn>` 是編譯期的函式指標，`AddSystem<class F>` 用 `F{}` 建（`engine/kernel/include/engine/kernel/SystemTraits.hpp:357-360`）。所以格式、category、層級都要是型別的一部分，執行期的值只有 World 名（放 Resource）：

```cpp
// 描述器（本卡加的詞）：一個無狀態的型別，寫在哪裡是第 13 節第 1 題。
struct TimeDiscardedLog {
    using Message = GYO::Engine::WorldTime::TimeDiscarded;
    static constexpr std::string_view category = "world_time";
    static constexpr Logging::Level level = Logging::Level::Debug;
    static void Describe(const Message& m, Logging::RecordBuilder& out);  // text 與 fields
};

// 在某個 build 裡（WorldSetup）：
WorldLogging::Forward<TimeDiscardedLog>(setup, {.schedule = DefaultSchedules::frame, .stage = "Last"});
```

- `Forward<D>` 往這個 World 登記一個 System（名字 `Log` ＋ 描述器的型別名，N18 的動詞片語；同一個 World 對同一個 `D` 呼叫兩次是 Programmer Error）。System 的參數只有 `MessageReader<D::Message>` 與 `Res<LogWorldLabel>`（`static_assert`，訊息帶專屬字串；`ParamTraits<P>::readOnly` 是公開的，`Params.hpp:237-239`）。
- 先 `Logger::Enabled(D::level, D::category)`，關著就只推游標、不格式化。游標一定要推：`MessageReader::Read()` 有紀錄時會配置一個 `vector`（`Params.hpp:463-474`），所以這條路**不保證不配置**（LG-7 只對 ④ 承諾）。
- 掛點必填：沒有預設值。預設骨架開著時建議 `Main`／`Last`（每幀最後）；`ScheduleSkeleton::Off` 時沒有 `Main`，要接線的人自己指定。
- **`LogWorldLabel`**（本卡加的詞）：日誌自己的唯讀 Resource，內容是 `char name[64]`，World 名補 0（固定長度、沒有指標、沒有 padding；照 `TimeConfig::fixedSchedule` 的先例，`WorldTimeModule.cpp:97`、`:126`）。World 名超過 63 位元組是 Programmer Error（World 名是作者寫的常數）。一個 World 第一次 `Forward` 時登記（zone 不寫、用預設，第 4 批會拿掉 `zone`），值在 `setup.Prepare` 裡 `PutIfAbsent` 放（O-7）。它不是判定資料：不經權威 Module 登記。第 4 批把 `World::Summary` 改成逐型別雜湊後，它要有雜湊函式（第 14 節）。
- **讀者跑過一次才拉住保留**（游標宣告「從沒跑過的讀者」）：日誌的 System 第一次跑之前，別張最外層 schedule（例：遊戲初始化）寫的訊息，那次 Run 結束就被修剪，日誌讀不到。寫進 LG-1 的「不保證」。
- 描述器或自訂 `Sink` 丟例外：`Forward` 的 System 自己 `catch`，那一筆改成一筆 `Error` 紀錄（category `"logging"`）、計數加一，System 照常回成功——例外離開 System 會讓 World 中斷（`WorldExecutor.hpp:29-30`），日誌不可以讓 World 中斷。`Sink` 的介面另規定不丟例外。
- `Event` 的訂閱同形（`ForwardEvent<D>`，登記一個 `Observer`）——**本批只定形狀、不實作**：`gyo2` 現在沒有要記的 `Event`。
- **tick 不自動帶**：日誌不認得時間 Module（LG-4）。要 tick 的，描述器從訊息內容寫（游標宣告「要 tick 的訊息自己在內容裡帶」、`WorldFailure` 的 G6 同一條線）。

### 5.4 ② Kernel 的診斷紀錄：`TakeKernelDiagnostics`

- `WorldLogging::TakeKernelDiagnostics(Kernel::World&)`：取走這個 World 的 `DroppedMessages`、`RejectedSpawn`，各寫成一筆 `Warning`（category `"kernel"`，`world` 欄位＝`World::Name()`）。runner 在兩次 Run 之間呼叫。
- 取走會清空：同一個 World 只能有一個取走的人。runner 要自己也看，就自己取、用 ④ 寫。
- **World 中斷之後**：Kernel 特意讓診斷紀錄在中斷後仍讀得到（`World.hpp:296-298`、`:314-315`）。修訂 0 寫成斷言，Fable 指出那樣壞掉那一幀的紀錄（可能正是原因）就永遠進不了日誌。修訂 1 的建議：准許，紀錄加 `world_interrupted: true`；**順序規則**：先 `WriteFailure`，再取診斷。這是讀壞掉的 World，和 D2 第 5 點的界線要使用者看——第 13 節第 2 題 (c)。
- 交換點在第 5 條才由編排層排；那時這個呼叫可以移到交換點，介面不變。

### 5.5 ③ `Base::Log` 與 ④ 直接寫一筆

- ③：`Install` 接手 `Base::SetLogHandler`；`Base::Log(level, category, text)` 變成一筆紀錄。契約不變（不改流程、不丟例外、任何執行緒）。Kernel 照舊不寫日誌（`kernel_guard.py` K6）；編排層照舊（五處）。
- ④：`Logging::Write(level, category, text, fields)`（或用 `RecordBuilder` 組），任何執行緒；`Logging::Span(...)` 見 5.13。給 trace、span、World 外的程式、作者取走 Kernel 紀錄後自己寫。關著的 category 不組紀錄、不配置、不讀時鐘（LG-7）。
- **誰可以在 System 裡呼叫 ④**：Engine 的 Module 報警告不用它（用 `Message`，`-no-separate-eventbus`）。作者自己的 System 用不用，是第 13 節第 5 題。

### 5.6 ⑤ 致命：`WriteFailure`（最後一道）

**誰呼叫、怎麼接**（三種情形；`WorldFailure` 由 runner 或 `main` 直接交給日誌，編排層不呼叫）：

```cpp
// (1) 入口回錯誤：
auto ran = product.RunFrame();
if (!ran && ran.error().failure) WorldLogging::WriteFailure(*ran.error().failure);
// (2) System 丟例外：RunFrame 不回錯誤、例外往外丟。
try { ran = product.RunFrame(); } catch (...) {
    if (const auto* f = product.Failure()) WorldLogging::WriteFailure(*f);
    throw;  // 或結束程式，由 runner 決定
}
// (3) 用預設 runner：Product::Run() 只回 RunnerExit，main 再看 Failure()。
const auto exit = product->Run();
if (const auto* f = product->Failure()) WorldLogging::WriteFailure(*f);
```

- `world_logging` 另給一個包住 runner 的輔助 `WorldLogging::WithFailureLog(Runner)`：回一個 runner，裡面照 (1)(2) 寫。產品裝 runner 時自己選要不要包（編排層不知道）。每個產品要記得接——這是第 13 節第 2 題 (d)。
- **不重複**：O-11 結束後每次拒絕都帶同一份 `WorldFailure`。`WriteFailure` 記住寫過的（`product`＋`world`＋`entry`＋`syncEpoch`＋`kind`），同一份第二次回 `AlreadyWritten`、不寫。
- **寫什麼**：一筆 `Fatal` 紀錄，category `"world"`，欄位照 `WorldFailure`（`product`、`world`、`kind`、`entry`、`schedule`、`sync_epoch`、`kernel_code`、`where`、`last_schedule`），`text`＝`WorldFailure::Text()`。**只讀傳進來的 `WorldFailure` 的值**，不讀 World、Kernel（情形 (2)(3) 讀的是 `Product::Failure()`：編排層在壞掉當下收好的值）。

**怎麼寫**（`logging` 的 `WriteFatalRecord`；`WriteFailure` 與斷言處理器都走它）：

1. 在呼叫的執行緒上組好那一行（文字一份、JSON 一份），用 `Install` 時預先保留的字串容量（不配置；超過容量才配置）。
2. **主控台**：同步寫 `stderr`、`fflush`。不等任何東西。
3. **每個 JSON lines 檔**：用 `Install` 時就開好的**致命寫出口**（本卡加的詞：同一個檔的第二個附加模式 `FileWriter`，只有致命路徑用；檔案輸出端一律用附加模式開，`Truncate` 的選項由 `Install` 先截斷再以附加模式開）把那一行**一次寫出**。不等寫者、不碰佇列、不碰寫者的 `FileWriter`。
   - 為什麼不交錯：寫者每次寫出的都是整行（master 的寫法：湊到 64 KiB 或一批結束，邊界都在行尾）；兩個附加模式的寫出口各自一次寫出整行，落點都在檔尾。**這是知識（POSIX `O_APPEND`、Windows 只附加的 handle），沒有在本機驗證**——第 2 步在 L1 三平台用「兩條執行緒、兩個附加 handle、各寫 10 萬行」的測試驗：每行 `Parse` 讀得回。驗不過就停下（第 15 節），改成另一個致命檔。
4. 寫完之後，**盡力**請各檔案輸出端把之前收的行寫出（`FlushNow`，只 try-lock、在寫者執行緒上直接略過、上限 `fatalFlushTimeout` 預設 1 秒、全部輸出端共用這一個期限，不是每個各等一次）。所以檔裡的順序可能是「致命那一行在前、它之前排隊的行在後」；每行有 `mono_ns`，讀的人用它排。
5. 回 `FatalWriteReport`：主控台、每個致命寫出口寫成了沒有、步驟 4 有沒有在期限內完成。不丟例外。

**「flush」在這裡的意思**：`FileWriter::Write` 回來＝交給 OS（`Files.hpp:78-83`、`:96-98` 不緩衝、不 fsync）。行程結束後內容還在；斷電不保證。

**「卡住」的幾種與承諾**：

| 情形 | 承諾 |
|---|---|
| 寫者落後（佇列很長） | 致命那一行不受影響，已寫；步驟 4 等到期限 |
| 寫者已失敗（寫入錯誤） | 致命寫出口是另一個 handle，照寫；失敗就在回報標出 |
| 寫者卡在 OS 的寫入裡（磁碟滿、網路磁碟） | **不保證時限**：另一個 handle 寫同一個檔，在 OS 層可能一起卡。改成另一個致命檔才避得開（RC-L5） |
| 致命路徑跑在寫者執行緒自己身上（斷言發生在寫者裡） | 步驟 4 略過；步驟 2、3 照做 |
| 致命路徑重入（寫致命那一行時又斷言） | `thread_local` 的重入旗標：第二次只寫主控台、不碰任何鎖，然後回 |
| 呼叫的執行緒握著主控台的鎖或佇列的鎖時斷言 | 致命路徑對主控台的鎖、佇列的鎖只 try-lock；拿不到就不用鎖直接寫（可能和另一行交錯，寧可交錯也不卡死） |

**斷言處理器**（`LoggerOptions::takeAssertions`，預設開）：斷言失敗時走 `WriteFatalRecord`（category `"assert"`），再把失敗交給前一個處理器（前一個是 `nullptr` 時呼叫 `DefaultAssertionHandler`）；abort 由 `ReportAssertionFailure` 做（`Assert.hpp:96`），不是處理器。測試裝的處理器會丟 `AssertionFailure`，日誌的處理器讓它穿過（不 `catch`）。**`std::terminate`**：步驟 4（只 flush，期限同上），再交回前一個處理器。照 master `LogFile::FlushOnCrash`。

**不保證的**（LG-2 寫明）：記憶體已被寫壞、堆疊用盡、訊號（`SIGSEGV` 等）、上表「卡在 OS」的時限。C++ 接不住所有錯誤（使用者的理由）；不承諾 async-signal-safe。

### 5.7 輸出端（Sink）

- 介面 `Sink`：`Accepts(level, category)`、`Write(const Record&)`、`Flush(timeout)`、`WriteFatal(std::string_view jsonLine, std::string_view textLine)`。**全部不丟例外**（`noexcept`）。作者可以自己實作。
- 內建三個：
  - `ConsoleSink`：文字格式，寫 `stderr`，同步、一行一次寫出（`Base::Log` 預設處理器的做法）。**經 ① 時是在跑模擬的執行緒上做 I/O**：管線塞住或主控台暫停時 World 會停住（第 12 節）。
  - `JsonLinesFileSink`：JSON lines。寫者執行緒（`Threads::RoleThread`，只被通知喚醒、不輪詢）＋有上限的佇列（預設 65536 行，照 master `LogFile`）：producer 只在短鎖下推進佇列、不做檔案 I/O、不等寫者；滿了丟並計數；`Close` 寫完全部已收的行、再寫結尾行；件數守恆。檔案經 `engine/os` 的 `FileWriter`（附加模式）；另開一個致命寫出口（5.6）。寫出走一個內部的「行的寫出」介面，預設是 `FileWriter`，**測試可以注入**（例：永遠不回來的、會失敗的），判定 2 用它。
  - `MemorySink`：記憶體裡的 `Record` 與行的清單，測試讀它。
- 篩選每個輸出端各自一份（5.8）。trace 和日誌的分流靠它。

### 5.8 層級與篩選

| 設定 | 何時定 | 可改嗎 |
|---|---|---|
| 有哪些輸出端、格式、檔名 | `Install` 時（`main`） | 不可 |
| 每個輸出端收哪些 category（前綴清單） | `Install` 時 | 不可 |
| 每個輸出端、每個 category 前綴的最低層級 | `Install` 時給初值 | **執行中可改還是不可，第 13 節第 6 題**（建議可改：`Logger::SetLevel`，原子變數、下一筆起） |
| `Forward` 的層級與 category | 描述器的型別（編譯期） | 不可；它經上面兩列篩 |
| 編譯期拿掉某些層級 | 不做 | RC-L3 |

### 5.9 多個 World

- 經 ① ② 來的紀錄帶 `world`（`Product` 裡唯一，O-E6）。致命紀錄帶 `product` 與 `world`。
- **不自動帶 tick、幀號**：日誌不認得時間（LG-4）；Kernel 的同步點序號 System 拿不到（`Params.hpp:1057` 是內部的）。致命紀錄帶 `sync_epoch`。
- 兩個 `Product` 先後存在、World 同名時，平常的紀錄分不開；要分開的話 World 名自己帶前綴。卡片照 L17 判（別家都沒有內建）。

### 5.10 執行緒

- `Logger` 的入口任何執行緒都可以呼叫、可同時呼叫。
- `ConsoleSink`：呼叫的執行緒寫（短鎖保一行完整）；`JsonLinesFileSink`：寫者執行緒寫，致命那一行由呼叫的執行緒經致命寫出口寫；`MemorySink`：呼叫的執行緒加（鎖）。
- flush：檔案輸出端每批寫完寫出一次；`Logger::Flush(timeout)` 等全部寫完（有期限）；`Close` 寫完全部；致命：5.6。
- `Forward` 的 System 在 Kernel 的執行緒上跑（`gyo2` 的 Kernel 在呼叫的執行緒上跑，CCR-5、`Product.hpp:25`）。

### 5.11 和決定性的關係

- **日誌不改模擬的值**：日誌的 System 只讀（`MessageReader<M>`、`Res<LogWorldLabel>`，`static_assert`），永遠回成功（5.3 的 `catch`）。日誌不寫任何 ECS 元件、`Message`、`Event`、不發 `Commands`；唯一寫進 World 的是登記時放的 `LogWorldLabel`（自己的 Resource，之後不變）。
- **但日誌的登記會改變 World 的其他東西**（修訂 0 說得太滿，修訂 1 照實寫）：
  1. **登記順序的編號**：現在的 C 指紋是 Kernel 的 `CFingerprintOf`，它把 zone C 的元件、Resource、訊息連同登記編號一起雜湊（`engine/kernel/src/Fingerprint.cpp:384`、`:390`、`:396`、`:415`），System 的存取清單也帶編號（`:189`）。`Forward` 在 zone C 的東西之前登記，後面的編號全部位移，C 指紋就變。**現在**：Match 與 Client 的日誌接線不同時，C 指紋不同；`gyo2` 上還沒有產品比 C 指紋。**第 4 批之後**：一致性指紋改看權威登記（權威份第 111 行：Client 多登記的一般資料不進 C 指紋），日誌的登記不再影響。這個前提寫進第 12、14 節。
  2. **保留與丟棄記錄**：修剪看所有跑過的讀者的游標（`World.cpp:1042-1054`）。日誌的讀者落後時（例：寫者在別張最外層 schedule），訊息緩衝在 Run 的邊界多留紀錄，可能多出一筆 `DroppedMessages`（讀者名單是日誌的 System）。遊戲的讀者讀到的紀錄不變（Fable 推過一次，第 3 步用測試確認）。
  3. **stamp 與摘要**：多一個 System，stamp 每步進一次就位移；Kernel 的 `World::Summary`、Registry 指紋、游標表都不同。
- **判定 4 比什麼**（先宣告）：「日誌關」＝不呼叫 `Forward`、不登記 `LogWorldLabel`（不只是不裝 `Logger`）。比：情境測試列出的遊戲自己的 ECS 元件與 Resource 的值（逐位元）、遊戲自己的讀者讀到的訊息內容與順序。不比：Registry 指紋、C 指紋、`World::Summary`、stamp、訊息緩衝、游標表、`DroppedMessages`（理由：上面 1～3，日誌會改它們，這是已知、寫明的）。
- 時間戳只進輸出，不回到 World。

### 5.12 紀錄格式

- **JSON lines**：一筆一行、每行是 JSON Module `Parse` 讀得回的嚴格 JSON（J-2、J-5）。固定鍵依序：`schema`（封套的版本，本批 `1`）、`kind`、`level`、`category`、`wall`、`mono_ns`、`product`（致命時）、`world`（有時）、`world_interrupted`（有時）、`text`、`fields`（物件，鍵照寫入順序）；`kind=span` 另有 `begin_ns`、`duration_ns`。鍵名小寫加底線（照 master `MovementTraceWriter`）。
- `wall` 的寫法照 `strftime` 的 `%z`：`2026-11-20T10:40:11.007+0900`（日期時間是 ISO 8601 擴充式、偏移是基本式；照 master `LogFile`，不是嚴格的 ISO 8601）。本地時間經 `Os::Process::ToLocalTime`（`engine/os/include/engine/os/Process.hpp:36-51`）。
- **結尾行**：`Close` 寫的最後一行，`kind=end`，鍵 `schema`、`kind`、`mono_ns`、`written`、`dropped_full`、`dropped_closed`、`fatal_direct`。
- **NaN、無限大**：`Writer` 對它們回錯誤，所以欄位值是非有限數時，日誌在交給 `Writer` 之前換成約定的表示——第 13 節第 3 題。負零照 `Writer` 寫 `-0.0`。
- **寫出失敗**（字串不是合法 UTF-8 時 `Writer::Finish` 回 `NotRepresentable`；或同一筆重複鍵，5.2）：那一筆改寫成一筆 `Error` 紀錄（category `"logging"`、同一個 `kind`、`world`），`text` 帶 `JsonError` 的碼與訊息，**不說是哪個欄位**（`JsonError` 只有碼與訊息，`JsonError.hpp:32`；要逐欄就得自己再驗一次 UTF-8，是 J-5 之外的第二份檢查，不做），計數加一；不丟、不中斷呼叫的人。
- **封套和內容的版本分開**：`schema` 是 Engine 的封套（固定鍵）的版本；產品自己的內容（例：`trace.movement` 的欄位）改版時，版本放在自己的 `fields` 裡（例 `fields.schema_version`，照 master）。封套歸 Engine 是 GYO 這次的選擇，master trace DI 6 是「Engine 不知道行的意思」——第 13 節第 2 題 (e)。
- 加鍵不升版，改既有鍵的意義或型別才升版。
- **文字格式**（`ConsoleSink`）：`[category] level: text key=value …`，沿用 `Base::Log` 預設處理器的開頭；給人看，不是資料契約。
- golden：檔案寫出用 `LineEndings::AsWritten`（`Native` 在 Windows 是 `\r\n`，三平台位元組不同）；golden 測試注入時鐘時連 UTC 偏移一起注入（CI 是 UTC，本機不是）。

### 5.13 trace

- **三個標記各管一件事**：`kind`（`log`／`span`／`end`）是紀錄的形狀；`level` 是詳細度；category 的 `trace.` 前綴只是分流的慣例（輸出端用前綴清單分到 trace 檔）。`trace.x` 配 `Info`、`Trace` 配別的 category 都合法；「trace」在本頁指「給分析工具看的結構化紀錄」這個用途，不是一個型別。
- **span**：`auto span = Logging::Span("trace.net", "send");` 結束（解構或 `End()`）時寫**一筆**：`kind=span`、`begin_ns`、`duration_ns`（`Engine::Time::NowNs()`）、欄位。被篩掉的 span 不讀時鐘。不記父子關係。系譜：master 的 `started_ns`＋`time_ns`、Chrome trace 的 complete event `ph:"X"`。
- trace 和日誌一個 Module 還是兩個、span 本批實作還是只定形狀：第 13 節第 6、8 題。

## 6. 基盤 Architecture 的根據（AGENTS §2）

### 6.1 為什麼算基盤

日誌的入口與紀錄格式上會積：每個 Module 的警告、每個產品的診斷與 trace、分析工具讀的 JSON lines、致命時的最後一筆。入口的形狀被 Module 用上，改它就要改每個 Module；紀錄格式被工具讀（資料契約，AGENTS §8）；致命路徑的保證被 runner 依賴。所以照基盤：予見的要求寫進 Contract 與 Design Invariants，只實作直近要用的部分。

### 6.2 予見的根據 → 要求（Feature → Contract → Architecture）

根據：1＝名指的 Feature、2＝參考實作、3＝規模與預算。AGENTS §2：1 或 3 至少一個，2 只回答「怎麼做」。

| # | Feature（Product） | 根據 | Contract | Architecture | 本批 |
|---|---|---|---|---|---|
| F1 | World 壞掉時留一筆完整的紀錄（Game、Tool） | 1：D2 第 4、5 點、`-world-events-not-via-event`；2：master `FlushOnCrash`、三家（4.1） | `WriteFailure` 先寫、不等；不讀 World | 5.6 | 實作 |
| F2 | World 健康時的警告進日誌 | 1：`-event-delivery-rule` 表、游標宣告「丟了有記錄」 | 訂閱 `Message`；runner 取走 Kernel 紀錄 | 5.3、5.4 | 實作 |
| F3 | 產品的 trace（一行一筆 JSON 給分析工具） | 1：master PvP 的移動 trace（使用者「有一個完整遊戲作為對照」，`-infrastructure-before-products`，產品會回來）；3：master 每行約 420 位元組、在 Match 的權威鎖下組行，不能等磁碟 | JSON lines 封套；producer 不做 I/O、不等寫者；丟了有計數 | 5.7、5.12 | 實作 |
| F4 | 量一段時間（PvP 的「這次送出花了多久」） | 1：master PvP `MovementTraceEvent::startedNs`（「Sent events span the nonblocking send call」）；2：Bevy span、Unity `ProfilerMarker` | span 一筆、單調時鐘 | 5.13 | 第 13 節第 6 題（建議實作手動 span；System 自動 span 延後） |
| F5 | 測試驗日誌的內容（Engine 與產品） | 1：卡片 brief「用可注入的 sink 驗輸出」 | `MemorySink`；注入的行寫出 | 5.7 | 實作 |
| F6 | 第三方的日誌轉進來（SDL） | 1：SDL 那一批；2：master `SdlLogBridge` | `Base::Log` 門面不變 | — | SDL 那一批 |
| F7 | 執行中調日誌的詳細度 | **只有 2**（三家分歧） | `SetLevel` | 5.8 | **第 13 節第 6 題：使用者點名才實作，否則只在 `Install` 定** |

導不出的要求：每秒幾筆、幾個執行緒同時寫——沒有量測根據。判定 7 本批只量、不設門檻（第 13 節第 6 題）。

## 7. Architecture Delta（AGENTS §3 的七項）

1. **哪個 Feature**：F1～F5（F4、F7 看第 13 節）；使用者決定 `-world-events-not-via-event`、D2、批次表第 6 組。
2. **現在的構造有什麼問題**：`gyo2` 只有 `Base::Log`（一個處理器、預設寫 `std::cerr`）；沒有東西接 `WorldFailure`（O-11 寫「日誌那一批定」）；`TimeDiscarded`、`DroppedMessages`、`RejectedSpawn` 發出來了沒有人讀；沒有檔案輸出、沒有結構化格式、致命時沒有任何寫出的保證。
3. **哪個邊界改變**：兩個新 Module（新 Subsystem）；`Base::Log`、`GYO_ASSERT`、`std::terminate` 的處理器由 `logging` 接手（處理器的契約不變）。
4. **影響的 Module／Product／Tool**：新增 `engine/logging`、`engine/world_logging`、`tests/common/logging`、`tests/common/world_logging`、`engine/config/modules.csv` 兩列、情境測試多一段。Kernel、編排層、時間、JSON、os 的程式**都不改**。第 13 節第 1 題選 D 時，`world_time` 加描述器（見該題）。O-11 的文字更正（第 13 節第 4 題）。
5. **依賴方向**：`GYO::Logging → Base`（私有 `Json`、`Os`、`Threads`、`Time`）；`GYO::WorldLogging → Logging、Kernel、Orchestration`。第 13 節第 1 題選 B 時沒有 Module 依賴日誌；選 D 時 `world_time → logging`（只是紀錄的型別）；選 A 時 `world_time → world_logging`。沒有循環。
6. **Ownership**：紀錄的封套、輸出端、致命寫出歸 `logging`；World 的接法歸 `world_logging`；「要不要記」歸接線的人；「記成什麼」歸描述器的作者（第 13 節第 1 題）；`WorldFailure` 的內容歸編排層、Kernel 診斷紀錄的內容歸 Kernel（都不變）。
7. **為什麼不能更小**：只用 `Base::Log`＝沒有檔案、沒有結構化格式、致命時沒有保證，D2 第 4 點「我們如果可以的話，也要記日誌」做不到；只做致命那一路＝`TimeDiscarded` 發了沒人收；把 trace 留給產品自己做＝重複 master 的 `TraceSink`＋手寫 JSON（AGENTS §2「同じ Conversion の繰り返し」）。

**Batch declaration checks**（AGENTS §4）：

| 選擇 | 系譜 | 退掉的選項與決定理由 | 需要它的 Feature／Product |
|---|---|---|---|
| 兩個 Module（`logging`＋`world_logging`） | `gyo2` 的 `time`／`world_time` | 一個 Module（修訂 0）：不用 World 的程式也得連 Kernel、EntityStore、編排層；致命時寫一行的底層也掛在 Kernel 的連結上。放 Base：Base 只依賴標準庫。照 master 分三處（格式在產品、傳輸在 Engine）：每個產品重寫格式與致命寫出——在為舊形狀找位置，不採 | F1、F3、F5；Game、Tool |
| 致命那一行先寫、用預先開好的第二個寫出口 | **新導出**（master 是 `Push`＋`FlushNow`，三家都不 flush） | `Push` 再等寫者（master）：寫者可能就是壞的那一部分，違反「不經任何機制」；呼叫者直接用寫者的 `FileWriter`（修訂 0）：同一個 `FileWriter` 兩條執行緒，`engine/os` 的契約沒說可以；另一個致命檔：讀的人要合兩個檔（留作 RC-L5 的退路） | F1 |
| 有上限、會丟的佇列（檔案輸出端） | master `TraceSink`（DI 1～5、RC 1） | 會擋的佇列：producer 在 Match 的權威鎖下（master），等磁碟會拖慢模擬；不丟、溢出到磁碟：沒有 Feature 要「一筆都不能丟」（RC-L1）。`Warning` 以上要不要不丟：第 13 節第 2 題 (a) | F3 |
| 描述器是型別、World 名放 Resource | Kernel 的 System 無狀態（`SystemTraits.hpp:357-360`） | 執行期的格式函式（修訂 0）：做不出來；全部放 Resource：Resource 變成格式表、`World::Summary` 要雜湊函式指標 | F2 |
| 一筆一個 span | master `started_ns`、Chrome trace `X` | 開始、結束兩筆：兩倍的量、讀的人要配對；父子關係：沒有 Feature | F4 |

既有的測試、守衛不用改——這不是選擇的理由（AGENTS §4），只是事實。

## 8. Design Invariants（`logging` 與 `world_logging` 共用新頁 `target/v2/logging.zh-Hant.md`，見出し `Design Invariants`；最終文字在合併前核准定稿）

守衛欄：行為的測試和機制同時寫；綁實作方式的掃描在情境測試通過之後才立（AGENTS §3「ガードを立てる時期」），本批第 6 步。

| 條 | 內容 | 守衛 |
|---|---|---|
| **LG-1 入口** | 進日誌的路只有第 3 節的 ①～⑥。Engine 的 Module 在 System 裡報警告走 `Message`（①），不走 ③④。致命錯誤與 World 自己的壞掉、建立、替換不經事件；`WorldFailure` 只經 `WriteFailure`。不新增別的入口（回呼登記、全域事件匯流排）。不保證：讀者第一次跑之前被修剪的訊息（5.3）。 | 測試：每條路各一；致命路徑的情境測試；審查 |
| **LG-2 最後一道直接呼叫** | `WriteFailure` 只讀傳進來的 `WorldFailure`（或 `Product::Failure()` 的值），不讀 World、Kernel；不經 Kernel、編排層、事件、日誌自己的佇列。先寫主控台與每個致命寫出口，**之後**才盡力等寫者（期限一個、全部輸出端共用）。回傳時致命那一行已交給 OS 或在回報標失敗。同一份 `WorldFailure` 只寫一次。不丟例外。重入只寫主控台。不保證：記憶體已壞、堆疊用盡、訊號、寫者卡在 OS 時的時限。 | 情境測試（World 真的中斷，三種情形）；注入「永不回來的寫出」：`WriteFailure` 回來、致命那一行在、回報標步驟 4 逾時；寫者執行緒上的斷言；重入；同一份寫兩次 |
| **LG-3 不改模擬的值** | 日誌登記的 System 只讀（`MessageReader<M>`、`Res<LogWorldLabel>`），永遠成功（描述器與輸出端的例外在 System 裡接住）；日誌不寫 ECS 元件、`Message`、`Event`、`Commands`，只在登記時放 `LogWorldLabel`。開不開日誌，判定 4 列的遊戲資料逐位元相同。登記、保留、指紋會變（5.11），那是寫明的。 | `static_assert`（編譯失敗樣本）；判定 4 的情境測試；描述器丟例外的測試 |
| **LG-4 不認得產品與其他 Module** | `engine/logging` 只 include Base、Json、Os、Threads、Time；`engine/world_logging` 另加 Logging、Kernel、Orchestration。都不認得任何訊息型別、產品的詞（N5、N6）。 | `gyo_expect_links`；含入掃描（第 6 步）；審查 |
| **LG-5 JSON lines 是資料契約** | 每行是嚴格 JSON（`Parse` 讀得回）；封套的固定鍵、順序、`schema` 版本、結尾行照 5.12；非有限數照約定；重複鍵與寫出失敗的那一筆改成錯誤紀錄。加鍵不升版，改鍵的意義或型別升版。文字格式不是契約。例外：5.6 表中「握鎖時斷言」那一格允許交錯。 | 往返測試；NaN／±Inf／負零／不合法 UTF-8／重複鍵各一；golden；兩個附加 handle 的並行寫測試（L1 三平台） |
| **LG-6 檔案輸出端的傳輸** | producer 不做檔案 I/O、不等寫者；佇列滿了丟並計數；寫者只被通知喚醒；件數守恆（推入＝寫出＋滿了丟＋關了才來）——致命那一行不經佇列，另計 `fatal_direct`、不進這個式子；結尾行是最後一行；同一個 producer 經佇列的行照推入順序（致命那一行可能在它之前推入的行之前，5.6）。 | 單元測試（照 master `TraceSinkTests` 的案例重寫）；件數守恆 |
| **LG-7 篩選** | 輸出端與 category 篩選在 `Install` 定、之後不變；層級何時可改照第 13 節第 6 題；④ 被篩掉的紀錄不組、不配置、不讀時鐘（① 要推游標，不承諾不配置）。 | 測試；配置計數測試（④） |
| **LG-8 一個行程一個 Logger** | 同時只裝一個（第二個是斷言）；`Install` 失敗回 `Result`；卸下時還原接手的處理器；沒裝時照 `Base::Log` 的預設。 | 測試 |
| **LG-9 span** | 一段時間一筆紀錄（`begin_ns`、`duration_ns`，`Engine::Time`）；被篩掉的 span 不讀時鐘；不記父子關係。 | 測試（注入的時鐘） |
| **LG-10 依賴** | `gyo_logging` 公開只連 `GYO::Base`、私有 `GYO::Json`、`GYO::Os`、`GYO::Threads`、`GYO::Time`；`gyo_world_logging` 只連 `GYO::Logging`、`GYO::Kernel`、`GYO::Orchestration`；沒有外部組件（之後有的話放 `src/adapters/<組件>/`）。 | `gyo_expect_links`（和測試同時立） |
| **LG-11 Sink 不丟例外** | `Sink` 的函式都是 `noexcept`；內建的輸出端寫失敗只計數、`Good()` 變 false。 | 編譯期（`noexcept` 檢查）；寫失敗的測試 |

### Replacement Criteria（新 Component，寫在積上去之前）

- **RC-L1 一筆都不能丟**：有 Feature 要日誌或 trace 不丟（例：拿 trace 重播）——停下交使用者（LG-6 的變更）。（master trace RC 1）
- **RC-L2 接線重複**：三個以上的產品重複同樣的 `Forward` 接線——重看第 13 節第 1 題（預設接線、登記單位）。
- **RC-L3 成本**：判定 7 量到的成本讓某個點名的 Feature 超出它的預算，或量測顯示日誌的格式化或佇列鎖是熱路徑的主要成本——停下重看（含外部庫、編譯期拿掉層級、per-thread 緩衝）。
- **RC-L4 trace 分家**：trace 需要和日誌不同的丟棄政策、父子關係或 Chrome trace／Tracy 匯出，而同一套輸出端做不到——提出把 trace 分出去。
- **RC-L5 致命路徑不夠**：L1 的並行附加測試不過、觀察到致命紀錄在 `WriteFailure` 回傳後沒進檔、或寫者卡在 OS 讓致命路徑也卡住——改成另一個致命檔、預先配置的寫出。
- **RC-L6 描述器的形狀不夠**：有訊息需要執行期才知道的格式（例：每個 World 不同的 category）而型別做不到——重看 5.3（可能要 Kernel 給 System 帶狀態的方法，交使用者）。
- **RC-L7 System 的自動 span**：有 Feature 要每個 System 的時間——要 Kernel 的 Executor 給計時的掛點，交使用者。
- **RC-L8 入口長出來**：要第七種進日誌的路——停下，LG-1 的變更。

## 9. 測試、守衛、突變

### 9.1 測試先行

每一步先寫會失敗的測試（Red，記下失敗的輸出），再實作到通過（Green）。新測試在 `tests/common/logging/`（`gyo_logging_tests`）與 `tests/common/world_logging/`（`gyo_world_logging_tests`），標籤 `cpu`。時間（牆上、單調、UTC 偏移）經可注入的時鐘，測試不讀真的時鐘。

### 9.2 情境測試（第 5 步；`tests/common/scenario/` 加一段，段號和第 4 批對齊，看哪批先合併）

- **致命路徑**（F1，判定 2）：兩個 World 的 `Product`，第二個 World 的 System 在第 3 幀 (1) 回錯誤、(2) 丟例外、(3) `DropOldFirst` 替換失敗（`WorldMissing`）。runner 用 `WithFailureLog`（(3) 由 `main` 接）。預期：`MemorySink` 和真的 `JsonLinesFileSink`（建置目錄裡的暫存檔）都有那一筆 `Fatal`、欄位等於 `WorldFailure`；檔案在 `Close` 之前就讀得到；之後的 `RunFrame` 被拒（O-11），runner 再呼叫 `WriteFailure` 回 `AlreadyWritten`、日誌沒有多一筆 `Fatal`。(3) 另有編排層經 `Base::Log` 記的那一筆 `Warning`（`Product.cpp:537`），在 `Fatal` 之前。
- **平常**（F2）：時間 Module 照預設、一幀給 1 秒真實經過時間，`Forward<TimeDiscardedLog>` 收到一筆、`world` 對；訊息上限設 2、寫 5 筆，runner 呼叫 `TakeKernelDiagnostics`，收到一筆 `DroppedMessages`。
- **決定性**（判定 4）：同一個情境跑兩次（日誌關＝不呼叫 `Forward`、不登記 `LogWorldLabel`；開＝全部 `Debug` 以上、兩個輸出端），比 5.11 列的對象。
- **trace**（F3、F4）：寫 1,000 筆 trace 紀錄（帶跳脫字串、NaN、±Inf、負零、重複鍵一筆）與 100 個 span，`trace.` 只進 trace 檔、其餘只進主控台；每行 `Parse` 讀回、逐欄比對；重複鍵那一筆變成錯誤紀錄。

### 9.3 守衛

- 和測試同時立（現有形式）：`gyo_expect_links(gyo_logging …)`、`gyo_expect_links(gyo_world_logging …)`；`cmake.UnitRegistry`（登記檔多兩列）；LG-3 的編譯失敗樣本（`Forward` 的 System 帶寫入參數要編不過，訊息帶專屬字串；照 `tests/common/kernel/CMakeLists.txt` 的 `gyo_kernel_compile_fail_samples`）。
- 情境測試通過之後立（第 6 步）：LG-4 的含入掃描（Python，照 `kernel_guard.py`；兩個目錄各自的准許清單）。自我測試三件（規則全關、實際樹的正例、根不存在時失敗）。

### 9.4 突變（AGENTS §14.2、§14.5；`tests/common/mutation/README.md`）

- 清單 `tests/common/logging/mutations.json`，前綴 `lg-`。每個突變寫 `what` 與 `guards`（README 第 9 條）。
- 只立測試觀察得到的決定（修訂 0 的「讀 World」「不 flush」觀察不到，拿掉）。估計：LG-1 3（`TakeKernelDiagnostics` 漏一種紀錄、`world` 欄位、`world_interrupted`）、LG-2 6（致命那一行等寫者之後才寫、不寫致命寫出口、不去重、重入時碰鎖、逾時後不回、寫者執行緒上等自己）、LG-3 3（不 `catch`、`static_assert` 拿掉、描述器的例外讓 System 回錯誤）、LG-5 7（NaN、+Inf、−Inf、鍵順序、`schema`、重複鍵、寫出失敗）、LG-6 6（滿了不丟、不計數、結尾行、守恆、順序、`fatal_direct` 進了守恆式）、LG-7 3、LG-8 2、LG-9 2、LG-10 1、LG-11 1；掃描守衛每條規則約 3 個（README 第 4 條）。合計約 40～50。超過「DI 條數 × 5」（11 × 5＝55）時，在合併前核准說明理由（README 第 8 條）。
- 每步只跑相關的突變；合併前完整跑一次（`mutation-rerun-scope`）。
- 新清單在第 6 步後送 Fable 5.2／high 敵對式檢查（AGENTS §14.2）。

## 10. 判定（先宣告，結果出來之後不放寬）

| # | 判定 | 怎麼算過 |
|---|---|---|
| 1 | L1 三平台（linux-x64、windows-x64、macos-arm64）ctest 全綠 | 含 `gyo_logging_tests`、`gyo_world_logging_tests`、情境的新段、守衛、兩個附加 handle 的並行寫測試 |
| 2 | 致命路徑（第 9.2 節三種情形＋LG-2 的單元案） | `WriteFailure` 回傳時：記憶體輸出端與檔案都有那一筆、欄位等於 `WorldFailure`（逐欄）；注入「永不回來的寫出」時，`WriteFailure` 有回來、致命那一行在致命寫出口、回報標步驟 4 逾時（期限用注入的值 50 ms；**不判真實時間的上限**，靠 ctest 的 TIMEOUT 擋住真的卡死）；同一份第二次回 `AlreadyWritten` |
| 3 | 件數守恆 | 每個檔案輸出端 `Close` 後：推入＝寫出＋滿了丟＋關了才來；結尾行的件數等於 `Counts()`；`fatal_direct` 等於致命寫出口寫的行數 |
| 4 | 決定性 | 5.11 列的比較對象相等 |
| 5 | 紀錄格式 | 每行 `Parse` 讀得回；NaN、±Inf 照第 13 節第 3 題；golden 一筆三平台位元組相同 |
| 6 | 突變 | 全部 killed（只算預期的失敗）；敵對式檢查加的也 killed |
| 7 | 成本（**只量、不設門檻**，第 13 節第 6 題） | 本機 Release（Intel Mac）量兩條路，分開記：(a) ④ 直接寫一筆（8 欄、約 400 位元組）到推入佇列；(b) ① 經 `Forward`（同樣的內容，一個 System 讀一筆）；另 (c) ④ 被篩掉的一筆。方法：暖機 10,000 次、每輪 100,000 次、5 輪，記每輪的中位數與 p99，報 5 輪中位數的中位數；不重試、不丟輪。結果寫進 dev_log；門檻等有產品的量再定（RC-L3） |

計測的件數突き合わせ（AGENTS §14 冒頭）：判定 3 是本批的件數突き合わせ，報告的第一行照格式寫。

## 11. 實作的步驟（動工前核准之後）

上段（公開介面、資料格式、Design Invariants）。全程 high、不排 xhigh（`-batch3-no-xhigh`）；碰到 §14 停止條件或原因不明的失敗，先停下回報。

| 步 | 內容 | 實作 | 審查 | 估計 |
|---|---|---|---|---|
| 第 1 步 | 骨架：兩個目錄、target、登記檔兩列、連結守衛；`Record`、`Level`、`Fields`、`Logger` 的安裝（`Result`）與卸下、`MemorySink`、`Base::Log` 處理器、④ `Write` | Opus 5.5／high（本卡） | — | 4～6 h |
| 第 2 步 | `ConsoleSink`、JSON lines（封套、NaN、重複鍵、寫出失敗、結尾行、golden）、`JsonLinesFileSink`（佇列、寫者、件數、注入的行寫出、致命寫出口）、兩個附加 handle 的並行寫測試（先在 L1 三平台 dispatch 驗，驗不過就停） | Opus 5.5／high | — | 7～10 h |
| 第 3 步 | `Forward`、描述器、`LogWorldLabel`、`TakeKernelDiagnostics`、LG-3 的編譯失敗樣本；篩選（與第 6 題的結果）；讀者對保留的影響的測試 | Opus 5.5／high | — | 5～7 h |
| 第 4 步 | 致命：`WriteFatalRecord`、`WriteFailure`、`WithFailureLog`、斷言與 `std::terminate` 處理器、重入、寫者執行緒上 | Opus 5.5／high | — | 5～7 h |
| 第 5 步 | span（第 6 題的結果）；情境測試（第 9.2 節）；判定 7 的量測 | Opus 5.5／high | — | 4～6 h |
| 第 6 步 | 含入掃描守衛；突變清單；突變清單的敵對式檢查 | Opus 5.5／high | 敵對式檢查 Fable 5.2／high | 4～6 h |
| 第 7 步 | 審查（全部的變更）；文件（新頁、README 索引、dev_log、合併前核准頁、O-11 更正）；完整跑突變 | Opus 5.5／high | 審查 Fable 5.2／high | 4～6 h |

§14.4 的檢證者：本批不改既有的判定，沒有；若實作中出現判定變更，先停下，檢證者 Fable 5.2／high。

**時間估計**：設計段 5～7 h（到修訂 1 約 5 h）；實作段 33～48 h。超過 1.5 倍（設計段 10 h、實作段 72 h）或單一問題累計超過 2 小時，停下回報。

**並行**：本批不改 Kernel、編排層、os 的程式。會和第 4 批碰到的見第 14 節。

## 12. 範圍的限界

- **致命路徑不是崩潰處理**：記憶體被寫壞、堆疊用盡、訊號、`abort` 之外的強制結束，致命那一筆可能寫不出來；寫者卡在 OS 時沒有時限（5.6 表、LG-2）。
- **「交給 OS」不是「到磁碟」**：斷電不保證（第 2.2 節）。
- **兩個附加 handle 不交錯**是知識、靠第 2 步的 L1 測試驗；網路檔案系統上不保證（不測）。
- **主控台在模擬的執行緒上寫**：經 ① 的紀錄到 `ConsoleSink` 是同步 I/O；主控台卡住時 World 停住。產品要避免就不讓 `ConsoleSink` 收 ① 的 category。
- **ThreadSanitizer 不在 L1**：多執行緒的測試只證明結果。
- **回滾 World 與固定步的 `Message`**：回滾份 7.1（第 4 批實作）規定固定步與巢狀 schedule 寫的 `Message` 不能有在它們之外的讀者，官方策略違反是**啟動錯誤**。所以選了回滾策略的 World（PvP 的 Client），`Forward` 掛在 `Main`／`Last` 記不了固定步發的任何 `Message`；掛進固定步，重跑時會重複記，而 LG-3、LG-4 讓日誌讀不到「重跑中」。本批不定，第 13 節第 7 題。`TimeDiscarded` 不在固定步裡發，不受影響。
- **登記影響現在的 C 指紋**（5.11 第 1 點）：第 4 批第 6 步合併之前，Match 與 Client 的日誌接線要一致才比得過 C 指紋；`gyo2` 上還沒有產品比。
- **讀者第一次跑之前的訊息**讀不到（5.3）。
- **Windows 主控台的編碼**：寫 UTF-8 位元組；顯示亂碼不在本批。
- **判定 7 只在一台機器量**；其他平台不量（`platform-priority`）。

## 13. 交使用者的

八題：第 1、3、4、5、7、8 題是多選一；第 2 題是 GYO 自己加的五件（逐件）；第 6 題是一起確認的三件小事。和別家相同、或照 `gyo2` 先例判得出的，沒有放進來（標「卡片照 L17 判」的那些）。

**第 1 題：Engine 各 Module 的警告 `Message`（例：`TimeDiscarded`），誰寫格式、誰決定要記？**（GYO 特有：三家都是發出的程式直接呼叫日誌巨集，GYO 定了「發事件、日誌訂閱」，才多出這一題）

修訂 0 把「誰寫格式」和「誰決定要記」綁在一起，Fable 指出漏了一個選項（D）。

| 選項 | 誰寫格式 | 誰決定要記 | 好處 | 代價 |
|---|---|---|---|---|
| A | 發出的 Module（`world_time`） | 發出的 Module（自己的 `Build` 呼叫 `Forward`） | 預設就記 | `world_time → world_logging` 依賴，日誌變成多數 Module 的依賴、產品關不掉 |
| B | 產品 | 產品 | 依賴最乾淨 | 每個產品重寫 Engine 訊息的格式；預設不記 |
| C | 日誌 Module | 日誌 Module | 預設就記 | 依賴倒過來（`world_logging → world_time`），每加一個 Module 就改日誌；違反 LG-4 |
| **D（建議）** | 發出的 Module 在自己的標頭給描述器（5.3 的 `TimeDiscardedLog`），只用 `logging` 的 `RecordBuilder`（不認得 ECS 的那一個） | 產品一行 `Forward<WorldTime::TimeDiscardedLog>(setup, …)` | 格式寫在最懂資料的地方、只寫一次；記不記是作者選的（D2「日誌是否使用是作者控制參數開啟的」）；`world_time` 不碰 Kernel 的訂閱 | `world_time → logging`（只是紀錄的型別，不碰 World 的部分）；預設不記，產品要一行 |

建議 D。卡片的讀法（**不是使用者的話**）：使用者說「日誌不要嵌入到功能中呀，日誌是機制」，原本是針對「編排層內建日誌＋參數」；卡片延伸成「發出的 Module 不決定記不記」。D 讓 `world_time` 只描述資料、不決定記不記。若你的意思是「發出的 Module 連日誌的型別都不該碰」，就選 B。選 D 的話，`world_time` 加描述器是本批的範圍（動 `engine/world_time`，第 4 批若也改它，見第 14 節）。

**第 2 題：GYO 自己加的、和別家都不同的五件**（逐件，各一句；都有建議，可以一起回「照建議」）

| 件 | 和別家的差 | 建議 |
|---|---|---|
| (a) 一般日誌也會被丟 | 三家都沒有會丟的佇列（呼叫者同步寫）；master 的丟棄是給 trace 的，`LogFile` 把容量開到 65536 | 全部會丟、容量 65536、丟了有計數與結尾行；**另一個選項**：`Warning` 以上佇列滿了改由呼叫者經致命寫出口同步寫（不丟，代價是那一刻呼叫者等磁碟） |
| (b) 致命那一行先寫、再等寫者最多 1 秒 | 三家都沒有保證 flush | 照 5.6：先寫、不等；之後盡力等 1 秒 |
| (c) 中斷的 World 還取診斷紀錄 | Kernel 特意留著（`World.hpp:296-298`）；D2 第 5 點「不在事後讀壞掉的 World」是說錯誤的內容 | 准許，標 `world_interrupted`；順序：先 `WriteFailure` 再取。另一個選項：中斷的 World 一律不取（壞掉那一幀的紀錄就沒有了） |
| (d) 誰呼叫 `WriteFailure` | 三家由錯誤處理器自動呼叫；GYO 的 runner 由產品或 Module 裝，日誌碰不到 | 每個產品的 runner 或 `main` 自己接，日誌給 `WithFailureLog` 輔助。這是「誰接線」的同一種問題（第 1 題） |
| (e) JSON lines 的封套歸 Engine | master trace DI 6 是「Engine 不知道行的意思，schema 歸使用者」（master 不帶回，不算推翻） | 封套（固定鍵）歸 Engine、內容（`fields`）歸寫的人，兩個版本分開（5.12） |

**第 3 題：JSON lines 裡 NaN 與無限大怎麼寫？**（別家資料不足、做法不同；JSON Module 的 `Writer` 回錯誤，留給本批定）

| 選項 | 誰這樣做 | 好處 | 代價 |
|---|---|---|---|
| **字串 `"NaN"`、`"Infinity"`、`"-Infinity"`（建議）** | flecs 寫字串 `"NaN"`、`"Inf"`，但 −Inf 的負號丟了（`strbuf.c:81-95`）；本建議分開正負、拼法照 JavaScript／Python `json` 的名字 | 不丟資訊；每行仍是嚴格 JSON | 同一欄位可能是數字也可能是字串，讀的人要處理 |
| `null` | serde_json（知識，沒查證） | 型別簡單 | 分不出 NaN、+Inf、−Inf，也分不出「沒有值」 |
| 字面 `NaN`、`Infinity` | Python `json` 的預設輸出（非標準） | 讀的人最自然 | 不是 JSON：JSON Module 的 `Parse`（J-2）讀不回 |

**第 4 題：O-11 的例子更正**（會改已核准的 DI 文字）

O-11（已合併 #253）與 `-world-events-not-via-event` 補充 (1) 寫「World 健康時的警告（訊息的丟棄記錄、`TimeDiscarded` 等）照常經事件」。實際上丟棄記錄（`DroppedMessages`）和生成被拒（`RejectedSpawn`）是 Kernel 的診斷紀錄，已核准的游標宣告定的是「World 記、runner 在交換點取走」（第 3 節表）。

| 選項 | 做法 | 代價 |
|---|---|---|
| **(a) 照已核准的取走、更正例子（建議）** | O-11 的例子只留 `TimeDiscarded`，加一句「Kernel 的診斷紀錄由 runner 在兩次 Run 之間取走交給日誌」（第 3 節的 ②） | 改 O-11 的一句（DI 文字，要你同意）；「平常」那個入口其實是兩條路 |
| (b) Kernel 把丟棄也發成 `Message` | Kernel 加一個內建的訊息型別 | 改 Kernel（第 4 批正在改）；丟棄的訊息本身也可能被丟；推翻游標宣告的「World 記、runner 取走」 |

**第 5 題：作者自己的 System 可以直接呼叫日誌（④ `Logging::Write`）嗎？**（GYO 特有：三家的 System 都直接呼叫；GYO 對 Engine 的 Module 定了走 `Message`）

| 選項 | 做法 | 好處 | 代價 |
|---|---|---|---|
| **准許（建議）** | Engine 的 Module 報警告照舊走 `Message`；作者自己的 System 可以直接呼叫 ④（trace、span、自己的除錯行） | 和三家相同、作者方便（`engine-not-rigid`「預設方便、不禁止別的做法」）；trace 與 span 本來就要 ④ | 「警告走事件」只約束 Engine，不約束作者 |
| 只准 `trace.` 開頭 | 其他的要走 `Message` | 作者的警告也統一走事件 | 每個作者的警告都要定一個 `Message` 型別與描述器；檢查要靠審查（執行期分辨不出呼叫者） |

**第 6 題：一起確認的三件**

1. **判定 7 只量、不設門檻**（修訂 0 是「中位數 ≤ 2 µs、p99 ≤ 20 µs，超過就停」，但沒有量測根據，Fable 指出那等於預先埋一個停止條件）。量的方法已先宣告（第 10 節）；門檻等有產品的量再定。
2. **層級在執行中可不可以改**（三家分歧：Bevy 不能改、flecs 一個全域整數可改、Unity Netcode 每個 World 可改）。F7 只有參考實作的根據（AGENTS §2 要你點名）。建議：可改（`SetLevel`，原子變數），因為除錯時開 `Debug` 不用重啟；輸出端與 category 在 `Install` 定、不可改。選「不可改」也可以，那就只在 `Install` 定。
3. **span 本批實作**（只有 master PvP 的 `started_ns` 這個根據，`gyo2` 上還沒有使用者）。建議：實作手動的 span（約 3 小時，形狀定了不實作的話，第一個產品回來時要補）。選「只定形狀」也可以。

**第 7 題：回滾 World 裡固定步發的 `Message` 怎麼記？**（和已核准的回滾份 7.1 的交界，第 12 節）

| 選項 | 做法 | 代價 |
|---|---|---|
| **(a) 本批不定、交第 4 批與主會話對齊（建議）** | 本批的 `Forward` 照 7.1：在回滾 World 裡記固定步的訊息，接線的人要把 `Forward` 掛進固定步，並接受重跑時重複；日誌之後要不要分辨「重跑中」，等第 4 批的「重跑中」Resource 定了再看 | 回滾 World 的固定步訊息，本批會重複記 |
| (b) 現在就讓日誌的讀者不受 7.1 約束 | 7.1 的檢查豁免日誌的讀者（它只讀、不影響判定） | 改已核准的回滾份與第 4 批正在寫的檢查；豁免要有辨別日誌讀者的方法（又是一個 Kernel 或回滾 Module 認得日誌的地方） |
| (c) 現在就讓日誌讀「重跑中」 | `Forward` 可選讀 `Resimulation`，重跑時不記 | `world_logging` 依賴 `world_time`（LG-4 的變更） |

**第 8 題：trace 與日誌是一個 Module 還是兩個；外部日誌庫用不用**（別家分歧）

- trace 與日誌：**同一套（建議）**——Bevy 的 `tracing` 同時是日誌與 span；輸出端、寫者、件數、致命寫出只有一份。分開——Unity（Logging 與 Profiler）、flecs（`log` 與 `perf_trace`）、master（`Base::Log`＋`engine/trace`）；trace 的政策可以獨立，但傳輸與致命寫出兩份。分家的條件寫在 RC-L4。（這裡的「一個 Module」是 trace 和日誌同一套；`logging`／`world_logging` 的拆法是另一件事，照 `time`／`world_time` 的先例，5.1。）
- 外部庫：**不用（建議）**，比較在第 4.3 節，不用就沒有下載。若你要用，或要先下載 spdlog、quill 的原始碼來查證第 4.2 節（只讀設計），我先報來源、版本、大小、授權，等你同意才下載。

## 14. 和其他卡、主會話對齊的

先合併的不用改；後合併的照下表改。

| 對象 | 內容 | 誰改 |
|---|---|---|
| 主會話 | O-11 的文字更正（第 13 節第 4 題，核准後才改，改在本批的分支） | 本卡 |
| 第 4 批 | `engine/config/modules.csv`：各加列，**列有順序**（依賴在前） | 後合併的 |
| 第 4 批 | 拿掉 `Zone` 與各 Options 的 `zone`：本批的登記不寫 `.zone`（用預設），所以不衝突 | — |
| 第 4 批 | `World::Summary` 逐型別雜湊：`LogWorldLabel` 要有雜湊函式（逐位元組雜湊 `name[64]`） | 後合併的 |
| 第 4 批 | C 指紋改成權威登記之後，5.11 第 1 點的「現在」那一段改寫 | 本卡（若第 4 批先合併，合併前核准時改；後合併則交第 4 批卡） |
| 第 4 批 | 回滾份 7.1 的跨 schedule 訊息檢查（第 13 節第 7 題） | 照第 7 題的結果 |
| 第 4 批 | 若改 `Setup.hpp`、`Product.cpp`（例：`Failure()`、runner 的型別），5.6 的三種情形與 `WithFailureLog` 跟著改 | 後合併的 |
| 第 4 批 | 若改 `engine/world_time`（第 1 題選 D 時本批也改它） | 後合併的 |
| 第 4 批 | `tests/common/scenario/` 的段號與 `CMakeLists.txt` 的連結清單 | 後合併的 |
| 第 4 批 | `docs/architecture/target/v2/README.md` 的索引 | 後合併的 |

## 15. 停止條件

- AGENTS §14 冒頭：同一問題換兩次測法沒結論、原因的說明會改計畫的前提、發現回歸或超出範圍——停下、不 commit 修正、報告。
- 第 2 步的兩個附加 handle 並行寫測試在任一平台不過——停下（RC-L5），不改測試。
- 實作中發現要改 Kernel、編排層或 os 的程式（例：拿不到 World 名、`WorldFailure` 欄位不夠、`FileWriter` 不能兩個 handle 同時附加）——停下，先和主會話對齊。
- 時間超過第 11 節估計的 1.5 倍。

## 16. 修訂 0 的敵對式檢查：處理對照

報告：主 checkout `build/evidence/gyo2-logging/declaration-r0-adversarial-check.md`（Fable 5.2／high）。

| 指摘 | 處理 | 改在 |
|---|---|---|
| M1 直接寫入的 API 是沒列出的入口 | 改：入口清單列 ①～⑥ 全部；④ 命名 `Logging::Write`；作者的 System 用不用交使用者 | 第 3 節、5.5、LG-1、第 13 節第 5 題 |
| M2 致命路徑自相矛盾、「寫到磁碟」做不到、致命那一行不在件數裡 | 改：先寫、不等，之後才盡力等；「flush」定義成交給 OS；`fatal_direct` 另計；交錯只在握鎖時的那一格 | 第 1 節、5.6、5.12、LG-2、LG-5、LG-6 |
| M3 時限做不到、測不到、重入與鎖 | 改：預先開好的第二個附加寫出口（不和寫者共用 `FileWriter`）；「卡住」分種類各寫承諾；卡在 OS 不保證時限；注入的行寫出；不判真實時間；期限全部共用一個；try-lock、重入旗標、寫者執行緒上略過 | 5.6、5.7、LG-2、判定 2、第 7 節表 |
| M4 `Forward` 做不出來、`LogWorldLabel` 的表示 | 改：描述器是型別；`LogWorldLabel` 是 `char[64]`、超長是 Programmer Error、`Prepare` 放；第 4 批的雜湊函式列進第 14 節 | 5.3、第 14 節 |
| M5 決定性的敘述不符現況、判定 4 沒定義 | 改：分「現在／第 4 批之後」；寫出保留與丟棄記錄、stamp 的影響；判定 4 列比與不比、定義「關」 | 5.11、判定 4、第 12 節 |
| M6 和回滾份 7.1 衝突 | 改：寫進範圍的限界與對齊表；交使用者 | 第 12、14 節、第 13 節第 7 題 |
| M7 現況描述錯（五處 `Base::Log`、例外、`RunnerExit`、重複的 failure） | 改：表照實寫；5.6 給三種情形；`WriteFailure` 去重；9.2 的預期算進編排層那一筆 | 第 3 節、5.6、9.2 |
| M8 重複鍵、結尾行 | 改：重複鍵那一筆改成錯誤紀錄；結尾行 `kind=end` 與鍵 | 5.2、5.12、LG-5 |
| M9 判定 7 不符 §14.1 | 改：只量、不設門檻；先宣告方法；分開兩條路；交使用者確認 | 判定 7、第 13 節第 6 題 |
| S1 五件 GYO 特有標成 L17 | 改：列成第 13 節第 2 題；`TakeKernelDiagnostics` 改成准許中斷的 World＋順序規則（建議） | 第 13 節第 2 題、5.4 |
| S2 第 1 題漏選項、借了使用者的話 | 改：加 D（建議）；標明「卡片的讀法」；寫出 B 與 D2「作者控制參數開啟」的距離 | 第 13 節第 1 題 |
| S3 沒和 `time`／`world_time` 的拆法比 | 改：採兩個 Module，表加一列 | 5.1、第 7 節表 |
| S4 F7 只有根據 2、F4 的根據 | 改：F7 交使用者點名；F4 的根據改成 master PvP 的 `startedNs`（產品會回來），span 實作與否交使用者 | 6.2、第 13 節第 6 題 |
| S5 該和日誌庫比 | 改：加第 4.2 節（全部標沒查證）；要查證就要下載，併進第 8 題 | 4.2、第 13 節第 8 題 |
| S6 `Install` 沒有錯誤的路、生命週期 | 改：回 `Result`；先後與卸下的條件 | 5.2、LG-8 |
| S7 時鐘、UTC 偏移、執行緒名 | 改：用 `Engine::Time`、依賴加 `GYO::Time`；UTC 偏移用 `Os::Process::ToLocalTime`；執行緒名本批拿掉 | 5.2、5.12、LG-10、第 2.2 節 |
| S8 `Forward` 裡的例外、同步 I/O、配置、第一次跑之前 | 改：System 裡 `catch`、`Sink` `noexcept`（LG-11）；主控台同步 I/O 寫進限界；LG-7 只對 ④ 承諾；第一次跑之前讀不到寫進 LG-1 | 5.3、5.7、LG-1、LG-7、LG-11、第 12 節 |
| S9 和第 4 批的衝突不完整 | 改：列全、寫誰後合併誰改 | 第 14 節 |
| S10 突變估計 | 改：拿掉觀察不到的；門檻照 README 第 8 條（條數 × 5，在合併前核准說明） | 9.4 |
| 小 1 斷言處理器的交回 | 改 | 5.6 |
| 小 2 golden 的換行與 UTC 偏移 | 改 | 5.12 |
| 小 3 第 4 題綁兩件 | 改：`Base::Log` 不算新入口那一件移到第 3 節的清單（現況，不需要決定）；第 4 題只問 O-11 | 第 3 節、第 13 節第 4 題 |
| 小 4 三個重疊的標記 | 改：寫明各管一件事 | 5.2、5.13 |
| 小 5 `Forward` 的名字、骨架關掉、動態插入 | 改：System 名、掛點必填、動態插入不支援 | 5.3、第 2.2 節 |
| 小 6 spdlog 的理由、Unity 沒查證 | 改 | 4.3、5.2 |
| 小 7 `wall` 的寫法 | 改：寫明照 `%z` | 5.12 |
| 小 8 寫出失敗說不出哪個欄位 | 改：寫明不說是哪個欄位與理由 | 5.12 |
| 小 9 產品自己的紀錄版本 | 改：封套與內容的版本分開 | 5.12 |
| 小 10 依賴的公開與私有、行號 | 改 | 5.1、5.3 |
