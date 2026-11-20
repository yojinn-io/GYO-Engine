# 事前宣告：日誌機制（含 trace）（`v8-arch-gyo2-logging`）

更新：2026-11-20（修訂 0）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：寫完、等動工前核准。**

性質：實作段第 6 組的事前宣告（AGENTS §2 的基盤 Architecture、§3 的 Architecture Delta、新 Component 的 Design Invariants 與 Replacement Criteria、§14 的判定）。和第 4 批（權威＋回滾）並行。

用詞：整條線分「設計段／實作段」；每批的核准分「動工前核准／合併前核准」；一批之內的實作步驟叫「第 N 步」。標「**卡片照 L17 判**」的是這張卡自己的判斷（名字照命名規範、細節照別家原始碼、批次內的先後），**不是使用者的話**；本卡新加的詞第一次出現時標「本卡加的詞」。交使用者的只有第 13 節。

## 0. 根據與讀的版本

**程式**：`gyo2` 8ec005f2（#244 編排層骨架、#253 時間 Module＋事件、#252 JSON Module 之後）。講現況的句子都來自讀程式（行號照這個版本）。

**決定**（HANDOFF 決定表，文件分支 `origin/claude/gyo2-time-design` 57c8dc32；原話以 HANDOFF 為準）：

| 代號 | 本頁怎麼用 |
|---|---|
| `v8-arch-gyo2-world-events-not-via-event`（含 11-19 補充三點） | 兩個入口、最後一道直接呼叫；本頁第 3 節 |
| `-no-separate-eventbus`（含兩次加註） | 不另做 EventBus；日誌是 World 裡面機制的訂閱者 |
| `-event-delivery-rule` | 警告的送法（`TimeDiscarded` 是 `Message`）；之後各批照原則定，日誌照它訂閱 |
| `-broken-world-ends-product`（D2，含同日更正） | 第 4 點：錯誤回 runner、編排層不寫日誌、日誌是獨立機制；第 5 點：寫日誌不依賴壞掉的 World。商業級崩潰日誌（minidump、呼叫堆疊）不做 |
| `-json-module-declaration-approved` 第 3 題 | JSON `Writer` 對 NaN、無限大回 `NotRepresentable`，表示法由本批定 |
| `-json-adapters-dir`（含推廣） | 碰外部組件的程式放 `src/adapters/<組件>/` |
| `-merge-needs-explicit-word` | 合併只認明確的「合併」 |
| `-module-plugin-owner-definition`（#240） | Module＝自己維護、介面是我們的；日誌是 Module |
| `-infrastructure-before-products`、`-json-early` | 日誌在搬產品之前建、在 JSON 之後 |
| `-confirm-only-differences`、`-batch3-no-xhigh` | 只交分歧；全程 high |

**文件**：[批次表](rebuild-batch-plan.zh-Hant.md)（文件分支）第 6 組「日誌機制」一列（範圍）；[編排層](orchestration.zh-Hant.md) O-3、O-4、O-9、O-11（`WorldFailure`、「編排層不寫日誌」、「World 自己的事不經事件」）；[時間＋事件的宣告](time-events-declaration.zh-Hant.md)第 5 節送法表；[事件游標的宣告](event-cursor-declaration.zh-Hant.md)第 100、173 行（丟棄記錄「World 記、runner 在交換點取走」）；[JSON](json.zh-Hant.md) J-5；[權威份](authority-module-declaration.zh-Hant.md)第 111 行（Client 多登記的一般資料不進 C 指紋）；`rebuild-naming.zh-Hant.md` N11～N22。

**master 的參考（只當參考、不搬回）**：`master:engine/trace/`（`TraceSink`：有上限的佇列＋一條寫者執行緒、`FlushNow`、`Close` 的結尾行、件數守恆；`master:docs/architecture/trace.md` 的 DI 1～8 與 RC 1～3）；`master:engine/base/include/engine/base/Log.hpp`（和 `gyo2` 的相同）；PvP 的 `LogFile`（時間戳、`FlushOnCrash`：斷言失敗寫進檔、`std::terminate` 時先 flush）與 `MovementTraceWriter`（一行一筆 JSON、`schema_version`、`started_ns`＋`time_ns` 表示一段時間）；`master:engine/platform/sdl/src/SdlLogBridge.cpp`（SDL 的日誌轉進 `Base::Log`）。

**證據**（主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-logging/`，git 不追蹤）：`recon-reference-engines.md`（Bevy、flecs、Unity 的對照，Opus 5.5／high 偵察）、`declaration-r0-adversarial-check.md`（修訂 0 的檢查，寫完後補）。

---

## 1. 這一批做什麼（先用遊戲講）

- **平常**：時間 Module 一幀補不完，扣掉 59 步，發一筆 `TimeDiscarded`（`Message`）。日誌不在時間 Module 裡寫字串；接線的人用一行把 `TimeDiscarded` 接到日誌，日誌的 System 在每幀最後讀它，寫成「哪個 World、丟了多少、為什麼」一筆紀錄。訊息緩衝滿了丟最舊的、生成被拒，Kernel 記在 World 上（`DroppedMessages`、`RejectedSpawn`），runner 在兩次 Run 之間交給日誌取走。
- **致命**：Match 的模擬 World 裡某個 System 回錯誤，World 中斷，`Product` 結束（O-11）。runner 拿到帶 `WorldFailure` 的錯誤，直接呼叫日誌的 `WriteFailure`：日誌當場把它寫到主控台並 flush，再把檔案寫到磁碟上才回來。這一路不經 Kernel、不經事件、不經日誌自己的佇列等待，不讀壞掉的 World。
- **trace**：「玩家 3 的移動指令排隊 2 筆」這種給分析工具看的一行一筆 JSON，和日誌走同一套紀錄與輸出端，用 category 分到另一個檔；「這次送出花了多久」用一個 span，結束時寫一筆帶開始時間與長度的紀錄。
- **測試**：日誌的輸出端可以換成記在記憶體裡的，測試直接讀它驗內容；致命路徑有情境測試（World 真的壞掉、runner 真的呼叫）。

## 2. 範圍

### 2.1 這一批做的（L1～L9）

| 代號 | 內容 |
|---|---|
| L1 | 新 Module `engine/logging`（`GYO::Engine::Logging`、`gyo_logging`／`GYO::Logging`）；Module 登記檔加一列（`library_only=yes`） |
| L2 | 紀錄 `Record`、層級、欄位；`Logger`（一個行程一個）與它的安裝／卸下 |
| L3 | 輸出端 `Sink` 介面與三個內建：主控台（文字、同步）、JSON lines 檔（有上限的佇列＋寫者執行緒）、記憶體（測試用） |
| L4 | 紀錄格式：JSON lines 的固定鍵、`schema` 版本、NaN 與無限大的表示法（第 13 節第 3 題）；文字格式 |
| L5 | 平常的入口：`Forward<M>`（World 健康時訂閱 `Message`）、`TakeKernelDiagnostics`（runner 在兩次 Run 之間交給日誌）、`Base::Log` 的處理器 |
| L6 | 致命的入口：`WriteFailure(const WorldFailure&)`（最後一道，直接呼叫）；斷言處理器與 `std::terminate` 的 flush |
| L7 | trace：span（一段時間一筆紀錄）；trace 與日誌的分流（category 篩選） |
| L8 | 測試、情境測試（致命路徑、決定性）、連結守衛、突變清單 |
| L9 | 文件：新頁 `target/v2/logging.zh-Hant.md`（Design Invariants LG-1～LG-10 與 Replacement Criteria）、v2 README 索引、dev_log、合併前核准頁；O-11 的例子更正（第 13 節第 4 題，核准後才改） |

### 2.2 不在這一批

| 項目 | 留到 | 為什麼 |
|---|---|---|
| 每個 System 自動一個 span（Bevy 的 `system` span） | 有產品要量 System 的時間時 | 要在 Kernel 的 Executor 加計時的掛點（改 Kernel；第 4 批正在改 Kernel）；沒有點名的 Feature（AGENTS §2）。RC-L7 |
| Chrome trace／Tracy 的匯出 | 有量測需求時 | 沒有點名的 Feature；JSON lines 的 span 紀錄之後可以由工具轉 |
| 編譯期拿掉某些層級 | 量測顯示呼叫成本是問題時 | 沒有量測；RC-L3 |
| 商業級崩潰日誌（minidump、呼叫堆疊、`SIGSEGV` 處理） | 不排 | 使用者 D2 第 4 點「先不做那種商業級別的崩潰日誌」 |
| SDL 的日誌轉進 `Base::Log` | SDL 那一批 | `gyo2` 現在沒有 SDL；master 的 `SdlLogBridge` 是參考 |
| Engine 各 Module 的警告預設接到日誌 | 第 13 節第 1 題 | 誰接線是使用者的決定 |
| World 建立、替換時往新 World 寫 `Message` | 讀檔那一批 | `-world-events-not-via-event` 補充 (3) |
| 回滾重跑時同一筆 `Message` 被記兩次 | 第 4 批（回滾 Module） | 重跑會不會重發 `Message` 由回滾 Module 定；第 12 節 |
| Kernel 指紋報告手寫 JSON 改用 JSON Module | 主會話另議（JSON 卡第 15 節） | 要動 Kernel 的依賴 |

## 3. 兩個入口與最後一道（使用者已定，這裡只寫細節）

使用者定的（`-world-events-not-via-event` 與補充三點、D2 第 4、5 點）：World 健康時的通知走事件、日誌訂閱；致命錯誤與 World 自己的生滅不走事件，結構化錯誤回 runner；最後把它寫出去的那一道直接呼叫、不經任何機制。

**現況對照**（讀程式得到的，`gyo2` 8ec005f2）：

| World 健康時的警告 | 現在怎麼出來 | 日誌怎麼拿 |
|---|---|---|
| `TimeDiscarded` | `Message`（`engine/world_time/src/WorldTimeModule.cpp:33`、`:43`、`:109`） | 訂閱：`Forward<TimeDiscarded>`（第 5.3 節） |
| 訊息的丟棄記錄 `DroppedMessages` | **不是 `Message`**：Kernel 的診斷紀錄，World 記、兩次 Run 之間用 `World::TakeDiagnostics<DroppedMessages>()` 取走（`engine/kernel/include/engine/kernel/World.hpp:135-150`、`:317`；游標宣告第 100 行「World 記、runner 在交換點取走」，已核准） | runner 在兩次 Run 之間呼叫 `TakeKernelDiagnostics`（第 5.4 節） |
| 生成被拒 `RejectedSpawn` | 同上（`World.hpp:101-131`） | 同上 |
| 作者自己的 `DiagnosticsWriter<R>` 紀錄 | 同上（`Params.hpp:709-733`） | 作者在 runner 取走後自己轉（日誌不認得 `R`） |
| 編排層的警告（O-9 動態插入被拒） | `Engine::Base::Log`（`engine/orchestration/src/Product.cpp:38`） | `Base::Log` 的處理器（第 5.5 節） |

**交界**：O-11 和 `-world-events-not-via-event` 補充 (1) 把「訊息的丟棄記錄」寫成「照常經事件」，和已核准的游標宣告、已合併的實作不一致（丟棄記錄是 runner 取走的診斷紀錄）。補充是 side chat 的看法（使用者說「補過去」），例子不準。怎麼處理交使用者：第 13 節第 4 題。

所以入口的完整清單是：

```text
World 健康時（平常）                         致命時
  Message／Event ──Forward<M>──┐              WorldFailure ──runner 直接呼叫──→ WriteFailure ──同步──→ 主控台＋檔案（flush 完才回）
  Kernel 的診斷紀錄 ──runner 取走──┤                                                          （不經 Kernel、事件、佇列等待；不讀 World）
World 外的程式                    ├──→ Logger ──→ Sink（主控台／JSON lines 檔／記憶體）
  Engine::Base::Log ──處理器────┘
斷言失敗、std::terminate ──處理器──→ 致命那一路
```

`Base::Log` 不是新入口：它早就在 Base，編排層 O-9 已經用它。日誌 Module 只是在安裝時接手它的處理器。使用者說的兩個入口是 World 的兩條路；`Base::Log` 是 World 外的程式（編排層、Module 的組裝期、之後的 SDL 轉接）用的。這一點也在第 13 節第 4 題一起確認。

## 4. 別家怎麼做（同一種技術比）

本機原始碼：Bevy 6a911653f、flecs 09790b5f7、Unity Entities 4630ff5、Netcode c4cabf4。完整的行號在 `recon-reference-engines.md`。**本機沒有 `tracing`、`tracing-subscriber`、`serde_json`、`Unity.Logging` 的原始碼**；它們的內部行為只寫「沒查證」，不當任何一條判法的唯一根據。

### 4.1 對照

| 技術 | Bevy（`bevy_log`＝`tracing` 0.1） | flecs（`log.c`、`os_api`） | Unity（Entities、Netcode） | 一致／分歧 | GYO 本批 |
|---|---|---|---|---|---|
| 層級 | 5 級（`lib.rs:157-169`） | 整數，含 `fatal`=-4（`os_api.h:368-374`） | Netcode 5 級含 Exception（`NetDebug.cs:338-362`） | 大致一致 | 6 級＝5 級＋`Fatal`（5.2） |
| 篩選何時定 | 啟動時（`LogPlugin` 的 `EnvFilter`，`lib.rs:173-224`）；執行中改：沒找到（`lib.rs:325`、`:422`） | 編譯期 `FLECS_LOG_0..3`（`log.h:336-452`）＋執行中一個全域整數（`log.c:957-963`） | Netcode：每個 World 的 singleton，System 每幀寫回（`NetDebug.cs:189`、`:322-332`） | **分歧** | 啟動時定輸出端與 category，執行中只改層級（5.8；第 13 節第 6 題一起確認） |
| 按 category 篩 | 有（target） | 沒有 | 沒有 | 分歧 | 有（輸出端各自的前綴清單） |
| 預設輸出 | stderr（`lib.rs:363-368`；註解寫 stdout，和程式不一致 `:75-76`） | stdout（`os_api.c:153-162`） | Unity 的標準日誌（`NetDebug.cs:367-414`） | 分歧 | stderr（照 `Base::Log`） |
| 多個輸出端 | 可疊多個 layer（`lib.rs:226-249`） | 一個 callback（`os_api.h:229-234`） | 引擎日誌＋每條連線一個封包檔（`PacketDumpLogger.cs:54-59`） | 分歧 | 多個 `Sink`（5.7） |
| JSON 日誌 | 沒有內建（只有 Chrome trace JSON 檔，`lib.rs:332-354`） | 沒有 | 沒有 | 一致（都沒有） | JSON lines（F3 的根據是 master 的 PvP trace） |
| 誰寫、緩衝 | 呼叫的執行緒；Tracy 緩衝**無上限**（`lib.rs:251-255`）；Chrome trace 在 `FlushGuard`（Resource）被丟掉時才寫（`:63-71`） | 呼叫的執行緒直接 `fputs`、沒有鎖、沒有 `fflush`（`os_api.c:153-281`、`log.c` grep 0 件） | 封包檔 32 KB 緩衝、滿了當場同步寫（`PacketDumpLogger.cs:61-71`） | 一致：**沒有一家有上限的佇列** | 主控台同步；檔案用有上限的佇列＋寫者（系譜是 master `TraceSink`，不是這三家；理由：master 的 producer 在 Match 的權威鎖下，不能等磁碟） |
| 致命時 | 錯誤處理器**直接**呼叫 `log::error!`（`error/handler.rs:119-199`）；panic hook 用 `eprintln!` 繞過 subscriber（`lib.rs:307-317`）；致命時 flush：沒找到 | `ecs_abort` 直接經 `log_` 印再 `abort`（`log.c:682-703`）；level -4 印 backtrace（`os_api.c:278-280`）；flush：沒有 | System 的例外 `Debug.LogException` 後繼續下一個 System（`ComponentSystemGroup.cs:715-741`） | 一致：**直接呼叫、不經 ECS 事件**；**沒有一家保證 flush** | 直接呼叫（使用者已定、和三家一致）；**flush 完才回是 GYO 自己加的**（D2「我們如果可以的話，也要記日誌」；master `LogFile::FlushOnCrash`） |
| span／trace | 日誌與 span 同一套（`info_span!`）；內建 App、幀、schedule、System 的 span（`sub_app.rs:577-586`、`schedule.rs:447`）；輸出 Chrome JSON 或 Tracy | `perf_trace_push/pop` 兩個 callback，自己不計時（`os_api.c:515-533`）；`ecs_log_push/pop` 只改縮排 | `ProfilerMarker` 每個 System 一個、和日誌不相連（`SystemState.cs:289-292`） | **分歧** | 一筆一個 span、和日誌同一套輸出端（第 13 節第 2 題） |
| 結構化欄位 | event 帶 level、target、file、line 與 key=value（`tracing` 的模型，沒查證） | 只有 level、file、line、msg；時間戳可選、秒（`os_api.h:229-234`、`os_api.c:166-207`） | 只有字串（`NetDebug.cs:367-414`） | 分歧 | 有序的鍵值欄位＋固定鍵（5.12） |
| 標出哪個 World、哪一幀 | 沒有；`FrameCount` 不自動進日誌（`frame_count.rs:21-38`） | 沒有（callback 沒有 world 參數，`os_api.h:229-234`） | 日誌沒有，Netcode 手寫 `[{WorldName}]` 前綴（`GhostReceiveSystem.cs:1386-1387`）；`ProfilerMarker` 自動帶 World 名（`SystemState.cs:290`） | 一致：**都沒有內建** | `Forward` 與取走的紀錄自動帶 `world`；不帶 tick（5.9）。自動帶 World 名比別家多：GYO 一個 `Product` 多個 World，Netcode 的手寫前綴是同一個需要 |
| NaN、±Inf 進 JSON | 沒開 json；沒查證 | 字串 `"NaN"`、`"Inf"`，**−Inf 的負號丟了**（`strbuf.c:81-95`） | 沒找到 | 資料不足 | 第 13 節第 3 題 |
| 日誌在不在 ECS 裡 | 日誌在 World 外；但 `FallbackErrorHandler` 是 Resource、參與排程衝突（`handler.rs:116`、`combinator.rs:236-243`）；`LogDiagnosticsPlugin` 是 System、寫自己的 Resource（`log_diagnostics_plugin.rs:100-111`） | 完全在 ECS 外（`os_api.c:26`） | Netcode 的 `NetDebug` 是 singleton，51 處取用、產生 job 依賴（`NetDebugSystem.cs:30-66`） | 分歧；**三家都沒有讓日誌改模擬狀態的路** | 日誌的 System 只讀（LG-3） |
| ECS 裡的警告怎麼到日誌 | 直接呼叫巨集（`virt.rs:255-260` 的 `debug!`）；**`Messages` 丟棄不記**（`message_cursor.rs:120-129` 的 TODO） | 直接呼叫巨集（`src/` 內 317 處） | 直接呼叫 | 一致：**都是直接呼叫** | 發事件、日誌訂閱（使用者已定 `-no-separate-eventbus`；所以多出第 13 節第 1 題） |
| 外部庫 | `tracing` 0.1＋`tracing-subscriber` 0.3.20 | 自己寫 | `UnityEngine.Debug`；`Unity.Logging` 是選用、本機沒開 | 分歧；三家都沒寫選型理由 | 第 13 節第 5 題 |

「World 外的程式每幀把 ECS 的診斷搬出來」：三家都沒有這個形狀。最接近的是 Bevy 的 `LogDiagnosticsPlugin`（World 裡的 System 隔一段時間讀 `DiagnosticsStore` 再呼叫 `info!`）。GYO 的「World 記、runner 取走」是游標宣告已核准的形狀（照 GYO 自己的 `RejectedSpawn`），本批照它。

### 4.2 外部庫的比較（第 13 節第 5 題）

版本、授權、大小來自知識，**沒有查證、沒有下載**。

| 選項 | 授權 | 帶來什麼 | 和 `gyo2` 現有的重疊 | 判斷 |
|---|---|---|---|---|
| spdlog | MIT | 多個輸出端、非同步佇列（執行緒池）、檔案輪替、`fmt` 格式 | 佇列與寫者＝master `TraceSink`；格式＝JSON `Writer`；門面＝`Base::Log` | 不用：它的數字格式是 `fmt` 的，JSON lines 會有第二份數字格式（違反 J-5「數字格式只有一份實作」的用意）；非同步佇列的丟棄計數、結尾行、致命 flush 的語義要在它外面再包一層 |
| fmt（或 C++20 `std::format`） | MIT | 格式化字串 | 文字格式只給人看；JSON 經 `Writer` | 不用：文字格式只是幾個鍵值的串接，用不到格式字串 |
| quill | MIT | 低延遲（producer 只複製參數、背景格式化） | 同 spdlog | 不用：判定 7 沒量到問題之前沒有理由；RC-L3 重評 |
| 自己寫（建議） | — | 照 master `TraceSink`（約 300 行、已有測試與突變的設計）＋JSON `Writer` | — | 用：不下載 |

## 5. 設計（Contract）

### 5.1 位置與名字（卡片照 L17 判，照 N11～N17、N20）

- 目錄 `engine/logging/`，namespace `GYO::Engine::Logging`，target `gyo_logging`／`GYO::Logging`。名字用 `Logging` 不用 `Log`：`Engine::Base::Log` 已是 Base 的函式名，`GYO::Engine::Log::Log…` 讀起來混（Bevy 是 `bevy_log`；Unity 是 `Unity.Logging`）。
- **一個 Module**（`library_only=yes`、`default=no`）。不做登記單位（`Module` 子類）：`Logger` 一個行程一個、由 `main` 在建 `Product` 之前裝；訂閱是接線的人在自己的 `build` 裡呼叫 `Forward<M>`（第 13 節第 1 題）；沒有東西要在每個 World 的 `Build`／`Finish` 做。照 JSON Module 的形（library_only）。之後要「預設接線」時才可能變成登記單位（RC-L2）。
- 依賴：`GYO::Base`、`GYO::Json`（JSON lines 的寫出，J-5 的單一數字格式）、`GYO::Kernel`（`MessageReader`、`Registrar`、`World::TakeDiagnostics`）、`GYO::Orchestration`（`WorldSetup`、`WorldFailure`）、`GYO::Os`（檔案寫出）、`GYO::Threads`（寫者執行緒）。沒有外部組件，所以本批沒有 `src/adapters/`；之後若接外部日誌庫，照 `-json-adapters-dir` 放 `src/adapters/<組件>/`。
- 檔名＝主要型別名（N16）：`Logger.hpp`、`Record.hpp`、`Sink.hpp`、`ConsoleSink.hpp`、`JsonLinesFileSink.hpp`、`MemorySink.hpp`、`Forward.hpp`、`Span.hpp`、`WriteFailure.hpp`。

### 5.2 紀錄、層級、Logger

- `Record`：`level`、`category`（字串，例 `"world_time"`、`"trace.movement"`）、`text`、時間（牆上時鐘的毫秒與 UTC 偏移、單調時鐘的奈秒——照 master `LogFile` 的兩個時鐘）、執行緒名（`Threads` 的角色名，沒有時省略）、`world`（有 World 時）、`fields`（有序的鍵值，值是 `int64`／`uint64`／`double`／`bool`／字串）、`kind`（`log` 或 `span`）。
- 層級：`Trace`、`Debug`、`Info`、`Warning`、`Error`、`Fatal`。`Base::LogLevel` 的四級照對應；`Trace` 是給 trace 紀錄的最低級（Bevy／tracing 的五級加上致命；flecs 有 `fatal`）。**卡片照 L17 判**。
- `Logger`：一個行程一個，`Logging::Install(LoggerOptions)` 回一個擁有者物件，解構時卸下並還原它接手的處理器（`Base::Log`、斷言、`std::terminate`）。一個行程一個照別家（Bevy／tracing 的全域 subscriber、flecs 的 `ecs_os_api`、Unity 的全域 Logger 都是一個行程一個）——**卡片照 L17 判**。沒裝 `Logger` 時，所有入口照 `Base::Log` 的預設（一行到 `std::cerr`）。
- `Logger` 在第一個 `Product` 之前建、在最後一個 `Product` 之後毀（`main` 擁有）；這樣 `Product` 結束後回主選單（另建 `Product`，O-3）時日誌照常。

### 5.3 平常的入口：訂閱 `Message`

```cpp
// 在 WorldDescription::build（產品自己的）或某個登記單位的 Build 裡：
Logging::Forward<TimeDiscarded>(setup, {.category = "world_time", .level = Logging::Level::Debug,
                                        .schedule = DefaultSchedules::frame, .stage = "Last"},
                                &DescribeTimeDiscarded);  // void(const TimeDiscarded&, Logging::Fields&)
```

- `Forward<M>` 往這個 World 登記一個 System：參數只有 `MessageReader<M>` 和日誌自己的唯讀 World 標籤；每筆 `M` 變一筆紀錄，`world` 欄位＝`setup.WorldName()`。格式函式由接線的人給（日誌不認得 `M`）。
- 預設掛在 `Main` 的 `Last`（每幀最後，讀這一幀所有寫者寫的）。接線的人可以改。
- `Event` 的訂閱同形（`ForwardEvent<E>`，登記一個 `Observer`）——**本批只做 `Message`**：`gyo2` 現在沒有要記的 `Event`（元件 `Add`／`Remove` 每筆都記沒有意義；tick 跳變在第 4 批）。介面形狀寫在本頁、到第一個要記的 `Event` 再實作（AGENTS §2「実装は薄く始める」）。
- 誰呼叫 `Forward`（產品、發出的 Module、還是日誌自己）是第 13 節第 1 題。
- World 標籤怎麼給 System：System 是無狀態的函式或 lambda 型別（`Registry.hpp:272-296`），拿不到登記時的 World 名。做法：`Forward` 第一次在一個 World 呼叫時，登記一個日誌自己的唯讀 Resource（本卡加的詞「World 標籤」`LogWorldLabel`，內容只有 World 名），System 讀它。它不是判定資料：不經權威 Module 登記，所以不進 C 指紋與 digest（權威份第 111 行）。**卡片照 L17 判**；第 1 步實作時若 Kernel 有更輕的取法（例：已有的 per-mount 狀態），照實作改並在合併前核准報。
- **tick 不自動帶**：日誌不認得時間 Module（LG-4）。要 tick 的，格式函式從訊息內容寫（`TimeDiscarded` 現在不帶 tick；游標宣告「紀錄只帶序號；要 tick 的訊息自己在內容裡帶」、`WorldFailure` 的 G6 同一條線）。**卡片照 L17 判**。

### 5.4 平常的入口：Kernel 的診斷紀錄

- `Logging::TakeKernelDiagnostics(Kernel::World&)`：取走（`TakeDiagnostics`）這個 World 的 `DroppedMessages`、`RejectedSpawn`，各寫成一筆 `Warning` 紀錄（category `"kernel"`）。由 runner 在兩次 Run 之間呼叫（游標宣告「runner 在交換點取走」）。
- 只對健康的 World 呼叫。**致命路徑不呼叫它**：D2 第 5 點「不在事後讀壞掉的 World」。函式在 World 是 `Interrupted` 時斷言（Programmer Error）。
- 取走會清空：同一個 World 只能有一個取走的人。runner 要自己也看這些紀錄時，就自己取、自己轉，不呼叫它。
- 交換點在第 5 條（多個 World、交換點）才由編排層排；那時這個呼叫可以移到交換點，介面不變。

### 5.5 World 外的程式：`Base::Log`

- `Install` 接手 `Base::SetLogHandler`；`Base::Log(level, category, text)` 變成一筆紀錄。`Base::Log` 的契約不變（不改流程、不丟例外、任何執行緒）。
- Kernel 照舊不寫日誌（`kernel_guard.py` K6）；編排層照舊只在 O-9 那一處用 `Base::Log`。

### 5.6 致命的入口：`WriteFailure`（最後一道）

```cpp
auto ran = product->RunFrame();
if (!ran && ran.error().failure) Logging::WriteFailure(*ran.error().failure);   // runner 或 main 直接呼叫
```

- 誰呼叫：拿到錯誤的那一層（runner，或 `Product::Run()` 回來後的 `main`）。編排層不呼叫（O-11「編排層不寫日誌」）。
- 寫什麼：一筆 `Fatal` 紀錄，category `"world"`，欄位照 `WorldFailure` 的欄位（`product`、`world`、`kind`、`entry`、`schedule`、`syncEpoch`、`kernelCode`、`where`、`lastSchedule`），`text`＝`WorldFailure::Text()`。只讀 `WorldFailure` 的值，不讀 World、不讀 `Product`。
- 怎麼寫（**直接呼叫**的細節，卡片照 L17 判，系譜見第 4 節）：
  1. 主控台：在呼叫的執行緒上同步寫、`fflush`。不經佇列。
  2. 每個檔案輸出端：先請寫者把已收的行寫完（`FlushNow`，上限 `fatalFlushTimeout`，預設 1 秒）；寫者在期限內完成，再由呼叫的執行緒把致命那一行直接寫進檔、flush；寫者沒在期限內完成（卡住或已失敗），仍由呼叫的執行緒直接寫致命那一行（可能和寫者正在寫的行交錯——寧可交錯也不丟），並在主控台補一行說明。
  3. 記憶體輸出端（測試）：直接加。
  4. 回傳 `FatalWriteReport`：每個輸出端寫成了沒有。不丟例外。
- 「直接」的意思：不經 Kernel、編排層、`Event`／`Message`；也不只是推進日誌自己的佇列然後等（佇列是日誌自己的機制，寫者執行緒可能就是壞掉的那一部分）。
- 斷言處理器：`Install` 可選（`LoggerOptions::takeAssertions`，預設開）接手 `SetAssertionHandler`：斷言失敗時走同一條致命路徑寫一筆（category `"assert"`），再交回前一個處理器（它會 abort）。`std::terminate`：先對每個檔案輸出端 `FlushNow`，再交回前一個處理器。照 master `LogFile::FlushOnCrash`。
- **不保證的**（範圍的限界，LG-2 寫明）：記憶體已被寫壞、堆疊用盡、`SIGSEGV`。C++ 接不住所有錯誤（使用者的理由）；致命路徑盡量不配置（預先保留格式化用的字串容量），但不承諾 async-signal-safe。

### 5.7 輸出端（Sink）

- 介面 `Sink`：`Accepts(level, category)`、`Write(const Record&)`、`Flush()`、`WriteFatal(const Record&)`。作者可以自己實作。
- 內建三個：
  - `ConsoleSink`：文字格式，寫 `stderr`（照 `Base::Log` 的預設），同步、一行一次寫出（`Base::Log` 預設處理器的做法：組成一個字串再寫）。
  - `JsonLinesFileSink`：JSON lines，寫者執行緒＋有上限的佇列；照 master `TraceSink` 的契約：producer 只在短鎖下推進佇列、不做檔案 I/O、不等寫者；佇列滿了丟並計數；`Close` 寫完全部已收的行、再寫一行結尾（含件數）；件數守恆：推入＝寫出＋滿了丟的＋關了才來的。檔案經 `engine/os` 的 `FileWriter`。
  - `MemorySink`：記在記憶體裡的 `Record` 清單，測試讀它。
- 篩選（第 5.8 節）每個輸出端各自一份：同一筆紀錄可以到主控台（`Info` 以上）也到 trace 檔（只收 `trace.` 開頭的 category）。trace 和日誌的分流就靠這個。

### 5.8 層級與篩選：哪些是編排期、哪些是執行期（別家分歧，第 4.1 節；第 13 節第 6 題一起確認）

| 設定 | 何時定 | 可改嗎 |
|---|---|---|
| 有哪些輸出端、每個輸出端的格式與檔名 | `Install` 時（`main`，第一個 `Product` 之前） | 不可（執行中增減輸出端要和寫者執行緒、致命路徑同步，沒有 Feature 要它） |
| 每個輸出端收哪些 category（前綴清單） | `Install` 時 | 不可 |
| 每個輸出端、每個 category 的最低層級 | `Install` 時給初值 | **執行中可改**（`Logger::SetLevel`，原子變數；任何執行緒；下一筆起生效） |
| `Forward<M>` 的層級與 category | 登記時（World 的 `build`） | 不可（跟著 World；替換 World 時照描述重登記） |
| 編譯期拿掉某些層級 | 不做 | RC-L3 |

篩選在組紀錄之前判（`Logger::Enabled(level, category)`），被篩掉的不格式化、不配置。

### 5.9 多個 World

- 每筆經 `Forward` 或 `TakeKernelDiagnostics` 來的紀錄帶 `world`（World 名，`Product` 裡唯一，O-E6）。致命紀錄帶 `product` 與 `world`。
- **不自動帶 tick、幀號**：日誌不認得時間（LG-4）；Kernel 的同步點序號 System 拿不到（`Params.hpp:1057` 是內部的）。致命紀錄帶 `syncEpoch`（`WorldFailure` 已有）。要 tick 的由格式函式從訊息帶。
- 多個 `Product` 先後存在（回主選單）時，`product` 欄位分得開（致命紀錄）；平常的紀錄只帶 `world`——兩個 `Product` 的 World 同名時分不開。要分開的話 World 名自己帶產品前綴。**卡片照 L17 判**（別家都沒有內建的 World 標籤，見第 4 節）。

### 5.10 執行緒

- `Logger` 的所有入口任何執行緒都可以呼叫，可同時呼叫（`Base::Log` 的契約）。
- 誰寫：`ConsoleSink` 由呼叫的執行緒寫（短鎖保一行完整）；`JsonLinesFileSink` 由它自己的寫者執行緒寫（`Threads::RoleThread`），只被通知喚醒、不用計時器輪詢（master trace DI 2）；`MemorySink` 由呼叫的執行緒加（鎖）。
- flush：檔案輸出端每批寫完 flush 一次（master 的預設）；`Logger::Flush()` 等全部寫完（有期限）；`Close` 寫完全部；**致命時一定 flush**（第 5.6 節）。
- `Forward` 的 System 在 Kernel 排的執行緒上跑（`gyo2` 的 Kernel 在呼叫的執行緒上跑，CCR-5）；它只做格式化與推入佇列。

### 5.11 和決定性的關係

- 日誌的 System 只讀：參數只准 `MessageReader<M>` 與 `Res<LogWorldLabel>`（編譯期檢查，`static_assert` 帶專屬字串）。不寫 ECS 元件、Resource、`Message`、`Event`、不發 `Commands`；永遠回成功（日誌寫失敗只計數，不讓 World 中斷）。
- 推讀者游標不算寫入（游標宣告）。日誌的 System、讀者游標、`LogWorldLabel` 會出現在 Kernel 的 World 摘要與登記指紋裡，**不會**出現在權威的 C 指紋與 digest 裡（它們只看經權威 Module 登記的；權威份第 111 行：Client 比 Match 多登記的一般資料不進 C 指紋）。所以 Match 開日誌、Client 不開，判定資料的比對不受影響。
- 時間戳、執行緒名只進輸出，不回到 World。
- 判定 4 用情境測試證明：同一個遊戲、同樣的輸入，開日誌和不開日誌，遊戲自己的 ECS 元件與 Resource 逐位元相同。

### 5.12 紀錄格式

- **JSON lines**（`JsonLinesFileSink`）：一筆一行、每行是 JSON Module `Parse` 讀得回的嚴格 JSON（J-2、J-5）。固定鍵依序：`schema`（版本，本批 `1`）、`kind`、`level`、`category`、`wall`（字串，例 `2026-11-20T10:40:11.007+0900`）、`mono_ns`（整數）、`thread`（有時）、`product`（致命時）、`world`（有時）、`text`、`fields`（物件，鍵照寫入順序）；span 另有 `begin_ns`、`duration_ns`。鍵名用小寫加底線（照 master `MovementTraceWriter` 與 Bevy／tracing JSON 的慣例）。`schema` 改版的規則：加鍵不升版、改既有鍵的意義或型別才升版。
- **NaN、無限大**：JSON `Writer` 對它們回錯誤，所以欄位值是非有限數時，日誌在交給 `Writer` 之前換成約定的表示——**第 13 節第 3 題**（建議：字串 `"NaN"`、`"Infinity"`、`"-Infinity"`）。負零照 `Writer` 寫 `-0.0`。
- 寫出失敗（例：字串不是合法 UTF-8，`Writer::Finish` 回 `NotRepresentable`）：那一筆改寫成一筆 `Error` 紀錄（category `"logging"`，`text` 說明哪個欄位不合法，原文以跳脫後的十六進位帶出），計數加一；不丟紀錄、不中斷呼叫的人。**卡片照 L17 判**。
- **文字格式**（`ConsoleSink`）：`[category] level: text key=value …`，沿用 `Base::Log` 預設處理器的開頭；給人看，不承諾格式（不當資料契約）。

### 5.13 trace

- **和日誌的關係**：trace 紀錄就是 category 以 `trace.` 開頭、層級 `Trace` 的紀錄，走同一個 `Logger`、同一套輸出端；用輸出端的篩選分到自己的檔（例：`trace.movement` 只進 `movement.jsonl`）。一個 Module 還是兩個是第 13 節第 2 題。
- **span**：`auto span = Logging::Span("trace.net", "send");` 結束（解構或 `End()`）時寫**一筆**紀錄：`kind=span`、`begin_ns`（單調時鐘）、`duration_ns`、欄位。不寫「開始」「結束」兩筆（master 的 `started_ns`＋`time_ns` 同形；Chrome trace 的 complete event `ph:"X"` 同形）。被篩掉的 span 不讀時鐘。
- 不記父子關係（沒有 Feature 要它；RC-L4）。巢狀的 span 各自一筆，看時間區間就知道包含關係。
- 計時用單調時鐘（`std::chrono::steady_clock`，和 master `MovementTraceNowNs` 相同）。牆上時鐘只用在 `wall` 欄。

### 5.14 外部庫（spdlog、fmt 等）

比較在第 4.2 節。**建議不用**（第 13 節第 5 題）：要的東西 `gyo2` 已經有或 master 已經做過（格式：JSON Module 的 `Writer`，數字格式只有一份實作 J-5；傳輸：master `TraceSink` 的設計與測試；門面：`Base::Log`），外部庫會帶來第二份數字格式（fmt）與第二套非同步佇列。量測顯示自己的實作是熱路徑的主要成本時再評估（RC-L3）。不用的話，本批沒有下載。

## 6. 基盤 Architecture 的根據（AGENTS §2）

### 6.1 為什麼算基盤

日誌的入口與紀錄格式上會積：每個 Module 的警告、每個產品的診斷與 trace、分析工具讀的 JSON lines、致命時的最後一筆。入口的形狀（訂閱、取走、直接呼叫）一旦被 Module 用上，改它就要改每個 Module；紀錄格式被工具讀，改了工具要跟著改（資料契約，AGENTS §8）；致命路徑的保證（flush 完才回）被 runner 依賴。所以照基盤：予見的要求寫進 Contract 與 Design Invariants，只實作直近要用的部分。

### 6.2 予見的根據 → 要求（Feature → Contract → Architecture）

根據：1＝名指的 Feature、2＝參考實作、3＝規模與預算。

| # | Feature（Product） | 根據 | Contract | Architecture | 本批 |
|---|---|---|---|---|---|
| F1 | World 壞掉時留一筆完整的紀錄（Game、Tool） | 1：D2 第 4、5 點、`-world-events-not-via-event`；2：master `LogFile::FlushOnCrash`、Bevy／flecs（第 4 節） | `WriteFailure` 同步寫、flush 完才回；不讀 World | 第 5.6 節 | 實作 |
| F2 | World 健康時的警告進日誌（時間被扣、訊息被丟、生成被拒） | 1：`-event-delivery-rule` 表、游標宣告「丟了有記錄」 | 訂閱 `Message`；runner 取走 Kernel 紀錄 | 第 5.3、5.4 節 | 實作 |
| F3 | 產品的 trace（一行一筆 JSON 給分析工具；PvP 的移動 trace 是先例） | 1：批次表第 6 組「trace 一起看」；2：master `MovementTraceWriter`（約 420 位元組一行、`schema_version`）、`LogFile`；3：master 的 trace 在 producer 執行緒（Match 的權威鎖下）組行，不能等磁碟 | JSON lines 固定鍵與 `schema`；producer 不做 I/O、不等寫者；丟了有計數 | 第 5.7、5.12 節 | 實作 |
| F4 | 量一段時間（送出、讀檔、一幀的某段）來和計畫的預算比（AGENTS §2「一つの host で費用を計測し、計画の予算と比べる」） | 1：AGENTS §2；2：master `started_ns`、Bevy span、Unity `ProfilerMarker`（第 4 節） | span 一筆、單調時鐘 | 第 5.13 節 | 實作（手動 span）；System 自動 span 延後 |
| F5 | 測試驗日誌的內容（Engine 與產品） | 1：卡片 brief「用可注入的 sink 驗輸出」 | `MemorySink` | 第 5.7 節 | 實作 |
| F6 | 第三方的日誌轉進來（SDL） | 2：master `SdlLogBridge` | `Base::Log` 門面不變 | 不另做 | SDL 那一批 |
| F7 | 執行中調日誌的詳細度（除錯時開 `Debug`） | 2：第 4 節（各家都能在執行中或啟動時改） | `SetLevel` 原子變數 | 第 5.8 節 | 實作 |

導不出的要求：每秒幾筆、幾個執行緒同時寫——沒有量測根據；判定 7 定一個預算，量出來超過就停（RC-L3）。

## 7. Architecture Delta（AGENTS §3 的七項）

1. **哪個 Feature**：F1～F5、F7；使用者決定 `-world-events-not-via-event`、D2、批次表第 6 組。
2. **現在的構造有什麼問題**：`gyo2` 只有 `Base::Log`（一個處理器、預設寫 `std::cerr`）；沒有東西接 `WorldFailure`（O-11 寫「日誌那一批定」）；`TimeDiscarded`、`DroppedMessages`、`RejectedSpawn` 發出來了但沒有人讀；沒有檔案輸出、沒有結構化格式、致命時沒有 flush。
3. **哪個邊界改變**：新 Module `engine/logging`（新 Subsystem）；`Base::Log` 與 `GYO_ASSERT` 的處理器由它接手（處理器的契約不變）。
4. **影響的 Module／Product／Tool**：新增 `engine/logging`、`tests/common/logging`、`engine/config/modules.csv` 一列、情境測試多一段。Kernel、編排層、時間、JSON 的程式**都不改**。O-11 的文字更正（第 13 節第 4 題，核准後）。
5. **依賴方向**：`GYO::Logging → Base、Json、Kernel、Orchestration、Os、Threads`。沒有 Module 依賴日誌（第 13 節第 1 題選 B 時；選 A 時 `world_time → logging`）。沒有循環。
6. **Ownership**：紀錄格式、輸出端、致命時的寫出歸日誌 Module；「要不要記、記成什麼」歸接線的人（產品或發出的 Module，第 13 節第 1 題）；`WorldFailure` 的內容歸編排層（不變）；Kernel 診斷紀錄的內容歸 Kernel（不變）。
7. **為什麼不能更小**：只用 `Base::Log`、不建 Module＝沒有檔案、沒有結構化格式、致命時沒有 flush，D2 第 4 點「我們如果可以的話，也要記日誌」做不到；只做致命那一路＝平常的警告沒人讀（`TimeDiscarded` 現在發了沒人收）；把 trace 留給產品自己做＝重複 master 的 `TraceSink`＋手寫 JSON（AGENTS §2「同じ Conversion の繰り返し」，master 有兩處）。

**Batch declaration checks**（AGENTS §4）：

| 選擇 | 系譜 | 退掉的選項與決定理由 | 需要它的 Feature／Product |
|---|---|---|---|
| 新 Module `engine/logging`（不放 Base） | 新導出；master 是 `Base::Log`＋`engine/trace`（傳輸）＋產品的 `LogFile`／`MovementTraceWriter`（格式） | 放 Base：Base 只依賴標準庫，要依賴 JSON、Os、Threads 就不能放；照 master 分三處：格式在產品、傳輸在 Engine——每個產品重寫格式與致命 flush（master 的 PvP 就是），是在為舊形狀找位置，不採 | F1、F3；Game、Tool |
| 致命時直接呼叫、同步寫 | 使用者的決定；別家（第 4 節） | 推進佇列再等：佇列與寫者執行緒是日誌自己的機制，可能就是壞的那一部分 | F1 |
| 有上限、會丟的佇列（檔案輸出端） | master `TraceSink`（DI 1～5、RC 1） | 會擋的佇列：producer 在 Match 的權威鎖下（master），等磁碟會拖慢模擬；不丟、溢出到磁碟：沒有 Feature 要「一筆都不能丟」（RC-L1） | F3 |
| 篩選在執行中只能改層級 | 第 4 節 | 執行中增減輸出端：要和寫者、致命路徑同步，沒有 Feature | F7 |
| 一筆一個 span | master `started_ns`、Chrome trace `X` | 開始、結束兩筆：兩倍的量，讀的人要配對；父子關係：沒有 Feature | F4 |

既有的測試、守衛不用改——這不是選擇的理由（AGENTS §4），只是事實：本批不改既有程式。

## 8. Design Invariants（新頁 `target/v2/logging.zh-Hant.md`，見出し `Design Invariants`；最終文字在合併前核准定稿）

守衛欄：行為的測試和機制同時寫；綁實作方式的掃描在情境測試通過之後才立（AGENTS §3「ガードを立てる時期」），本批第 6 步。

| 條 | 內容 | 守衛 |
|---|---|---|
| **LG-1 入口** | World 健康時的警告只從兩條路進日誌：訂閱 `Message`／`Event`（`Forward`）、runner 在兩次 Run 之間交出 Kernel 的診斷紀錄（`TakeKernelDiagnostics`）。致命錯誤與 World 自己的壞掉、建立、替換不經事件；`WorldFailure` 只經 `WriteFailure`。World 外的程式經 `Base::Log`。日誌不新增別的入口（回呼登記、全域事件匯流排）。 | 測試：三條路各一；致命路徑的情境測試（判定 2）；審查 |
| **LG-2 最後一道直接呼叫** | `WriteFailure` 只讀傳進來的 `WorldFailure` 的值：不讀 World、`Product`、Kernel；不經 Kernel、編排層、事件。回傳之前，主控台已寫並 flush，每個檔案輸出端已寫進那一行並 flush，或在回報裡標失敗；寫者執行緒卡住時在 `fatalFlushTimeout`＋寫一行的時間內回來。不丟例外。不保證：記憶體已壞、堆疊用盡、訊號。 | 情境測試（World 真的中斷）；卡住的寫者的測試；檔案在 `Close` 之前就讀得到那一行 |
| **LG-3 不影響模擬** | 日誌登記的 System 只讀（`MessageReader<M>`、`Res<LogWorldLabel>`），永遠成功；日誌不寫任何 ECS 元件、Resource、`Message`、`Event`、`Commands`。開不開日誌，遊戲資料逐位元相同。日誌的登記不經權威 Module。 | `static_assert`（編譯失敗樣本）；判定 4 的情境測試 |
| **LG-4 不認得產品與其他 Module** | `engine/logging` 不 include 其他 Module（`world_time` 等）的標頭，不認得任何訊息型別、產品的詞（N5、N6）；格式由接線的人給。 | 連結守衛（`gyo_expect_links`）；含入掃描（第 6 步）；審查 |
| **LG-5 JSON lines 是資料契約** | 每行是嚴格 JSON（`Parse` 讀得回）；固定鍵、順序與 `schema` 版本照第 5.12 節；非有限數照約定的表示；寫出失敗的那一筆改成一筆錯誤紀錄、不丟。加鍵不升版，改鍵的意義或型別升版。文字格式不是契約。 | 往返測試（每行 `Parse`）；NaN／±Inf／負零／不合法 UTF-8 各一；golden（一筆固定內容的紀錄） |
| **LG-6 檔案輸出端的傳輸** | producer 不做檔案 I/O、不等寫者；佇列滿了丟並計數；寫者只被通知喚醒、不輪詢；件數守恆（推入＝寫出＋滿了丟＋關了才來）；結尾行是最後一行；同一個 producer 的行照推入順序。（master trace DI 1～5） | 單元測試（照 master `TraceSinkTests` 的案例重寫）；件數守恆 |
| **LG-7 篩選** | 輸出端與它的 category 篩選在 `Install` 定、之後不變；層級在執行中可改（任何執行緒、下一筆起）；被篩掉的紀錄不格式化、不配置、不讀時鐘。 | 測試（改層級前後）；配置計數測試 |
| **LG-8 一個行程一個 Logger** | 同時只裝一個（第二個是斷言）；卸下時還原它接手的 `Base::Log`、斷言、`std::terminate` 處理器；沒裝時照 `Base::Log` 的預設。 | 測試 |
| **LG-9 span** | 一段時間一筆紀錄（`begin_ns`、`duration_ns`，單調時鐘）；被篩掉的 span 不讀時鐘；不記父子關係。 | 測試（可注入的時鐘） |
| **LG-10 依賴** | `gyo_logging` 只連 `GYO::Base`、`GYO::Json`、`GYO::Kernel`、`GYO::Orchestration`、`GYO::Os`、`GYO::Threads`；沒有外部組件（之後有的話放 `src/adapters/<組件>/`）。 | `gyo_expect_links(gyo_logging …)`（和測試同時立） |

### Replacement Criteria（新 Component，寫在積上去之前）

- **RC-L1 一筆都不能丟**：有 Feature 要日誌或 trace 不丟（例：拿 trace 重播）——會丟的佇列不夠，停下交使用者（改成會擋或溢出到磁碟是 LG-6 的變更）。（master trace RC 1）
- **RC-L2 接線重複**：三個以上的產品重複同樣的 `Forward` 接線（同一個 Engine 訊息、同樣的格式）——重看第 13 節第 1 題（預設接線、登記單位）。
- **RC-L3 成本**：判定 7 的預算在本機量測超過，或之後的量測顯示日誌的格式化或佇列鎖是熱路徑的主要成本——停下重看（含外部庫、編譯期拿掉層級、per-thread 緩衝）。（master trace RC 3）
- **RC-L4 trace 分家**：trace 需要和日誌不同的丟棄政策、父子關係或 Chrome trace／Tracy 匯出，而同一套輸出端做不到——提出把 trace 分成自己的 Module。
- **RC-L5 致命路徑不夠**：觀察到致命紀錄在 `WriteFailure` 回傳後仍沒進檔，或需要在記憶體已壞時也寫得出——重做致命路徑（預先開好的檔案描述符、不配置的寫出）。
- **RC-L6 沒有 World 的程式**：有產品或工具要日誌但不用 Kernel，因連結 `GYO::Logging` 被迫連 Kernel——照 `time`／`world_time` 拆成函式庫與 World 的部分。
- **RC-L7 System 的自動 span**：有 Feature 要每個 System 的時間——要 Kernel 的 Executor 給計時的掛點（改 Kernel），交使用者。
- **RC-L8 入口長出來**：要第三個「World 健康時」的入口（例：某個 Module 要日誌提供回呼）——停下，LG-1 的變更。

## 9. 測試、守衛、突變

### 9.1 測試先行

每一步先寫會失敗的測試（Red，記下失敗的輸出），再實作到通過（Green）。新測試在 `tests/common/logging/`，執行檔 `gyo_logging_tests`，標籤 `cpu`。時間（牆上、單調）經可注入的時鐘，測試不讀真的時鐘。

### 9.2 情境測試（第 5 步；`tests/common/scenario/` 加一段，段號和第 4 批對齊，看哪批先合併）

- **致命路徑**（F1，判定 2）：一個兩個 World 的 `Product`，第二個 World 的 System 在第 3 幀回錯誤；自訂 runner 拿到錯誤後呼叫 `WriteFailure`；`MemorySink` 和真的 `JsonLinesFileSink`（建置目錄裡的暫存檔）都有那一筆，欄位等於 `WorldFailure`；檔案在 `Close` 之前就讀得到；之後的 `RunFrame` 被拒（O-11）且日誌沒有多寫。另一案：System 丟例外。另一案：`DropOldFirst` 替換失敗（`WorldMissing`）。
- **平常**（F2）：時間 Module 照預設、一幀給 1 秒真實經過時間，`Forward<TimeDiscarded>` 收到一筆、`world` 對；訊息上限設 2、寫 5 筆，runner 呼叫 `TakeKernelDiagnostics`，收到一筆 `DroppedMessages`。
- **決定性**（判定 4）：同一個情境跑兩次（日誌開、關；開的時候全部 `Debug` 以上、兩個輸出端），結束時遊戲自己的 ECS 元件與 Resource 逐位元相同。
- **trace**（F3、F4）：寫 1,000 筆 trace 紀錄（帶跳脫字串、NaN、±Inf、負零）與 100 個 span，`trace.` 只進 trace 檔、其餘只進主控台；每行 `Parse` 讀回、逐欄比對。

### 9.3 守衛

- 和測試同時立（現有形式）：`gyo_expect_links(gyo_logging GYO::Base GYO::Json GYO::Kernel GYO::Orchestration GYO::Os GYO::Threads)`；`cmake.UnitRegistry`（登記檔多一列）；LG-3 的編譯失敗樣本（`Forward` 的 System 帶寫入參數要編不過，訊息帶專屬字串；照 `tests/common/kernel/CMakeLists.txt` 的 `gyo_kernel_compile_fail_samples`）。
- 情境測試通過之後立（第 6 步）：LG-4 的含入掃描（Python，照 `kernel_guard.py`；`engine/logging/` 不准 include `engine/<其他 Module>/`，Base、Json、Kernel、Orchestration、Os、Threads 除外）。自我測試三件（規則全關、實際樹的正例、根不存在時失敗）。

### 9.4 突變（AGENTS §14.2、§14.5；`tests/common/mutation/README.md`）

- 清單 `tests/common/logging/mutations.json`，前綴 `lg-`。每個突變寫 `what`（只有它抓的誤）與 `guards`（LG-n 或判定 n）（README 第 9 條）。
- 下限：每條 DI 裡獨立會錯的決定一個（README 第 3 條）。估計：LG-1 3、LG-2 6（不 flush、等寫者不設期限、讀 World、經佇列、丟例外、漏欄位）、LG-3 3、LG-5 6（NaN、+Inf、−Inf、鍵順序、schema、錯誤紀錄）、LG-6 6（滿了不丟、不計數、結尾行、守恆、順序、輪詢）、LG-7 3、LG-8 2、LG-9 2、LG-10 1；掃描守衛每條規則約 3 個（README 第 4 條）。合計約 35～45。超過 45（約前兩批每條 DI 平均的 ×5 以內）就在 dev_log 寫理由。
- 每步只跑相關的突變；合併前完整跑一次（`mutation-rerun-scope`）。
- 新清單在第 6 步後送 Fable 5.2／high 敵對式檢查（AGENTS §14.2）。

## 10. 判定（先宣告，結果出來之後不放寬）

| # | 判定 | 怎麼算過 |
|---|---|---|
| 1 | L1 三平台（linux-x64、windows-x64、macos-arm64）ctest 全綠 | 含 `gyo_logging_tests`、情境的新段、守衛 |
| 2 | 致命路徑（第 9.2 節三案） | `WriteFailure` 回傳時：記憶體輸出端與檔案都有那一筆、欄位等於 `WorldFailure`（逐欄）；卡住的寫者（測試用、永不完成的輸出端）案：在 `fatalFlushTimeout`＋100 ms 內回傳，主控台有那一筆 |
| 3 | 件數守恆 | 每個檔案輸出端 `Close` 後：推入＝寫出＋滿了丟＋關了才來；結尾行的件數等於 `Counts()` |
| 4 | 決定性 | 第 9.2 節：開與不開日誌，遊戲資料的位元組比較相等 |
| 5 | 紀錄格式 | 每行 `Parse` 讀得回；NaN、±Inf 照第 13 節第 3 題的表示；golden 一筆三平台位元組相同 |
| 6 | 突變 | 全部 killed（只算預期的失敗）；敵對式檢查加的也 killed |
| 7 | 預算（本機 Release，Intel Mac；卡片定的、沒有量測根據，請一起核准） | 一筆 8 欄、約 400 位元組的紀錄經 `Forward` 或直接呼叫到推入檔案輸出端的佇列：中位數 ≤ 2 µs、p99 ≤ 20 µs；被篩掉的紀錄 ≤ 20 ns、不配置。超過 → RC-L3，停下回報，不放寬 |

## 11. 實作的步驟（動工前核准之後）

上段（公開介面、資料格式、Design Invariants）。全程 high、不排 xhigh（`-batch3-no-xhigh`）；碰到 §14 停止條件或原因不明的失敗，先停下回報（xhigh 只在要調查時、經同意）。

| 步 | 內容 | 實作 | 審查 | 估計 |
|---|---|---|---|---|
| 第 1 步 | 骨架：目錄、target、登記檔一列、連結守衛；`Record`、層級、`Logger` 的安裝與卸下、`MemorySink`、`Base::Log` 處理器 | Opus 5.5／high（本卡） | — | 3～5 h |
| 第 2 步 | `ConsoleSink`、JSON lines 格式（NaN 表示、錯誤紀錄、golden）、`JsonLinesFileSink`（佇列、寫者、件數、結尾行） | Opus 5.5／high | — | 6～9 h |
| 第 3 步 | `Forward<M>`、`LogWorldLabel`、`TakeKernelDiagnostics`、LG-3 的編譯失敗樣本；篩選與 `SetLevel` | Opus 5.5／high | — | 4～6 h |
| 第 4 步 | 致命路徑：`WriteFailure`、斷言與 `std::terminate` 處理器、卡住的寫者 | Opus 5.5／high | — | 4～6 h |
| 第 5 步 | span；情境測試（第 9.2 節）；判定 7 的量測 | Opus 5.5／high | — | 4～6 h |
| 第 6 步 | 含入掃描守衛；突變清單；突變清單的敵對式檢查 | Opus 5.5／high | 敵對式檢查 Fable 5.2／high | 4～6 h |
| 第 7 步 | 審查（全部的變更）；文件（新頁、README 索引、dev_log、合併前核准頁、O-11 更正）；完整跑突變 | Opus 5.5／high | 審查 Fable 5.2／high | 4～6 h |

§14.4 的檢證者：本批不改既有的判定，沒有；若實作中出現判定變更，先停下，檢證者 Fable 5.2／high。

**時間估計**：設計段 4～6 h；實作段 29～44 h。超過 1.5 倍（設計段 9 h、實作段 66 h）或單一問題累計超過 2 小時，停下回報。

**並行**：本批不改 Kernel、編排層的程式。會和第 4 批碰到的檔：`engine/config/modules.csv`（各加一列，合併時的文字衝突）、`docs/architecture/target/v2/README.md`（索引）、`tests/common/scenario/`（段號）。O-11 的文字更正（第 13 節第 4 題）先和主會話對齊再改。

## 12. 範圍的限界

- **致命路徑不是崩潰處理**：記憶體被寫壞、堆疊用盡、`SIGSEGV`、`abort` 之外的強制結束，致命那一筆可能寫不出來（LG-2 寫明）。
- **ThreadSanitizer 不在 L1**：多執行緒的測試只證明結果，不證明沒有資料競爭。
- **回滾重跑**：作者在固定步裡發的 `Message`，回滾重跑時若再發一次，日誌會記兩次。重跑時要不要發、要不要標「重跑中」由回滾 Module 定（第 4 批）；本批的 `Forward` 掛在 `Main` 的 `Last`，`TimeDiscarded` 不在固定步裡發，不受影響。
- **讀者游標進 Kernel 的 World 摘要**：開不開日誌，Kernel 的完整 World 摘要不同（游標表、`LogWorldLabel`），判定資料的比對（C 指紋、digest）不受影響（第 5.11 節）。若第 4 批的回滾快照或 digest 改成含全部 Resource 與游標，要重看——和第 4 批卡對齊（第 14 節）。
- **Windows 主控台的編碼**：`ConsoleSink` 寫 UTF-8 位元組；主控台顯示亂碼不在本批。
- **判定 7 只在一台機器量**：Intel Mac 本機；其他平台不量（`platform-priority`）。

## 13. 交使用者的

**第 1 題：Engine 各 Module 的警告 `Message`（例：`TimeDiscarded`），誰把它接到日誌？**（GYO 特有：別家都是發出的程式直接呼叫日誌巨集，GYO 定了「發事件、日誌訂閱」，所以多出「誰接線」這一題）

| 選項 | 做法 | 好處 | 代價 |
|---|---|---|---|
| **A 發出的 Module 自己接** | `world_time` 在自己的 `Build` 呼叫 `Logging::Forward<TimeDiscarded>` | 預設就記；新 Module 自己帶格式 | `world_time → logging` 依賴；日誌變成多數 Module 的依賴、產品關不掉；日誌嵌進功能（D2 更正：「日誌不要嵌入到功能中呀」） |
| **B 產品接（建議）** | 日誌 Module 只給 `Forward<M>`；產品在自己的 `build` 一行一種，格式也由產品給 | 依賴最乾淨（沒有 Module 依賴日誌）；和「日誌是機制、不嵌進功能」一致；要不要記是作者選的（`engine-not-rigid`） | Engine 的訊息預設不記；每個產品重複接線（RC-L2：三個以上產品重複就重看） |
| C 日誌認得各 Module 的訊息 | 日誌 Module 裡寫 `TimeDiscarded` 等的格式 | 預設就記、產品不用寫 | 依賴倒過來（`logging → world_time`），每加一個 Module 就改日誌；違反 LG-4 與 Product 可刪除性 |

建議 B。理由：使用者說過「日誌不要嵌入到功能中呀，日誌是機制」；`gyo2` 現在沒有產品，B 的代價要等產品回來才出現，那時用 RC-L2 判。

**第 2 題：trace 和日誌是一個 Module 還是兩個？**（別家分歧）

| 選項 | 系譜 | 好處 | 代價 |
|---|---|---|---|
| **一個 Module（建議）** | Bevy：`tracing` 一套同時是日誌與 span（第 4 節） | 輸出端、寫者執行緒、件數、致命 flush 只有一份（難的部分）；trace 只是 category 與篩選 | trace 要不同政策時要分（RC-L4） |
| 兩個 Module | Unity：Logging 與 Profiler 分開；flecs：`log` 與 `perf_trace` 兩個掛點；master：`Base::Log` 門面＋`engine/trace` 傳輸 | trace 的政策可以獨立 | 傳輸與致命 flush 兩份，或 trace 依賴日誌；master 的 PvP 自己在兩者之上又寫了格式與 flush |

**第 3 題：JSON lines 裡 NaN 與無限大怎麼寫？**（別家分歧；JSON Module 的 `Writer` 回錯誤，留給本批定）

| 選項 | 誰這樣做 | 好處 | 代價 |
|---|---|---|---|
| **字串 `"NaN"`、`"Infinity"`、`"-Infinity"`（建議）** | flecs 寫字串 `"NaN"`、`"Inf"`，但 −Inf 的負號丟了（`strbuf.c:81-95`）；本建議分開正負、拼法照 JavaScript／Python `json` 的名字 | 不丟資訊；每行仍是嚴格 JSON | 同一欄位可能是數字也可能是字串，讀的人要處理 |
| `null` | serde_json（知識，本機沒有原始碼、沒查證） | 型別簡單（數字或 null） | 分不出 NaN、+Inf、−Inf，也分不出「沒有值」 |
| 字面 `NaN`、`Infinity` | Python `json` 的預設輸出（非標準） | 讀的人最自然 | 不是 JSON：JSON Module 的 `Parse`（J-2）讀不回，工具要寬鬆解析 |

**第 4 題：O-11 的例子更正，以及入口的完整清單**（會改已核准的 DI 文字）

O-11（已合併 #253）與 `-world-events-not-via-event` 補充 (1) 寫「World 健康時的警告（訊息的丟棄記錄、`TimeDiscarded` 等）照常經事件」。實際上丟棄記錄（`DroppedMessages`）和生成被拒（`RejectedSpawn`）是 Kernel 的診斷紀錄，已核准的游標宣告定的是「World 記、runner 在交換點取走」（第 3 節表）。

| 選項 | 做法 | 代價 |
|---|---|---|
| **(a) 照已核准的取走、更正例子（建議）** | O-11 的例子只留 `TimeDiscarded`，加一句「Kernel 的診斷紀錄由 runner 在兩次 Run 之間取走交給日誌」；日誌給 `TakeKernelDiagnostics`。另外確認：World 外的程式（編排層 O-9、組裝期、之後的 SDL 轉接）經既有的 `Base::Log`，日誌接手它的處理器——這不是第三個入口，是 World 外的門面 | 改 O-11 的一句（DI 文字，要你同意）；「平常」那個入口其實是兩條路（訂閱、取走） |
| (b) Kernel 把丟棄也發成 `Message` | Kernel 加一個內建的訊息型別 | 改 Kernel（第 4 批正在改）；丟棄的訊息本身也可能被丟（緩衝滿了再發一筆）；推翻游標宣告的「World 記、runner 取走」 |

**第 5 題：外部日誌庫（spdlog、fmt、quill）用不用？**（別家分歧：Bevy 用外部的 `tracing`；flecs、Unity 自己寫）

比較在第 4.2 節。建議不用，理由在第 5.14 節；不用就沒有下載。若你想用，我先報來源、版本、大小、授權，等你同意才下載。

**第 6 題（一起核准）**：
- 判定 7 的預算數字（卡片定的、沒有量測根據）。
- 篩選何時定（別家分歧，第 4.1 節）：輸出端與 category 在啟動時定、執行中只改層級（第 5.8 節）。Bevy 啟動時定、執行中不能改；flecs 編譯期＋執行中一個全域層級；Unity Netcode 每個 World 一份、執行中改。建議的理由：F7（除錯時開 `Debug`）只要改層級；增減輸出端要和寫者、致命路徑同步，沒有 Feature 要它。

## 14. 和其他卡、主會話對齊的

- **主會話**：O-11 的文字更正（第 13 節第 4 題，核准後才改，改在本批的分支）；`engine/config/modules.csv` 與第 4 批同時加列。
- **第 4 批（權威＋回滾）**：(1) 日誌的 System 與 `LogWorldLabel` 不經權威 Module、不進 C 指紋與 digest——第 4 批的 digest 若改成含全部 Resource 或游標，要告訴本卡；(2) 回滾重跑時 `Message` 會不會重發（第 12 節）；(3) 情境測試的段號。

## 15. 停止條件

- AGENTS §14 冒頭：同一問題換兩次測法沒結論、原因的說明會改計畫的前提、發現回歸或超出範圍——停下、不 commit 修正、報告。
- 實作中發現要改 Kernel 或編排層的程式（例：拿不到 World 名、`WorldFailure` 欄位不夠）——停下，先和主會話對齊。
- 判定 7 超過預算——RC-L3，停下。
- 時間超過第 11 節估計的 1.5 倍。
