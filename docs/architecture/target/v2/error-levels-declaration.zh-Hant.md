# 事前宣告：異常分級（`v8-arch-gyo2-error-levels`）

修訂 3（2026-11-20）。Owner：架構（GYO2 任務卡「異常分級」，架構師的子會話）。分支 `claude/gyo2-error-levels-170217`，從 `gyo2` 8ec005f2 開。修訂 0 是 641ec8b9；修訂 1（34cd347a）照 Fable 5.2／high 的敵對式檢查（必改 10、建議改 16、小問題 11）改，處理對照在第 16 節；修訂 2（81b1f1de）照主會話照 L17 的判斷併入兩件（`UnknownWorld` 與 schedule 名不一致、`Product::Build` 的例外），見第 17 節；修訂 3 照使用者要的窄複查（Fable 5.2／high，範圍 D3 (b)、D8～D10、第 17 節；必改 7、建議改 12、小問題 8）改，處理對照在第 18 節。

狀態：**等動工前核准**。這一回合只寫宣告，不寫實作。

起因：日誌卡發現 `WriteFailure` 的去重鍵會漏寫；使用者否定「以 Product 為單位去重」：「首先去重不是解決問題的方法，是補丁。第二，核心是不應該產生重複。」「我們把error級別的異常，當成了普通警告來處理，就是這個偏差。」「包進一個普通的 Result 回去也沒問題。呼叫的人也可以不理。核心是不理是錯的！！！！」「返回沒錯，錯誤沒錯，包裝沒錯，錯誤的是對WorldFailure的處理，正常就應該，記日誌，然後退出。不是機制問題，是寫代碼的人寫了一坨垃圾。」「runner對不可修復的錯誤，應該就是記日誌，然後退出。」（HANDOFF `v8-arch-gyo2-logging-no-failure-dedup`；同一列在原話之前，記錄者的摘要寫「O-11、編排層都不動」——這是摘要，不是使用者的原話）。開這張卡的原話：「參考Java 的Exception 建立 異常級別……核心不是退出還是重試。而是沒有分級，現在下面的runner不知道如何處理。……1）整合我們已經有的異常。2）分類。3）給異常處理方案。」「或者參考Go的也可以。」（HANDOFF `v8-arch-gyo2-error-levels`）

**兩次原話的關係**（本卡的讀法，第 13 節第 1 題請使用者確認）：去重那一次說「返回沒錯，錯誤沒錯，包裝沒錯……不是機制問題」；同一天稍後開這張卡時說「沒有分級，runner 不知道如何處理」。本卡讀成：回傳結構化錯誤這個機制保留；缺的是級別——每一級的處理者與規則沒有寫下來，型別上也看不出來，所以 runner 寫錯是可預期的。本批因此會動到當時說「不動」的 O-11 與編排層（第 8.2 節逐條列）。

本卡加的詞（不是使用者的話，名字在合併前核准定稿）：**E0～E3** 四個級別的代號；「處理一次」；「升級／降級」；`ProductEnded`（D3 (b) 的暫名）。

---

## 0. 估時與讀的版本

**估時**（AGENTS §14 冒頭、時間盒）：宣告與盤點 4 h（實際約 4 h）；敵對式檢查與修訂 1 共 3 h；實作 34.5 h，D9 本批做時 37 h（第 11.1 節逐步；修訂 0 的 17 h 偏低，檢查 S14；修訂 2、3 併入與修正的部分照加）。超過 1.5 倍（實作 51.75 h；含 D9 時 55.5 h）或單一問題累計 2 h 就停下回報。實作中另有一個**中途關卡**：第 3 步改 DI 守衛前，把逐條的 §14.4 1～5 交使用者（第 9.1 節 G1、G2、G6、G8）。

| 讀的東西 | 版本 |
|---|---|
| `gyo2` | 8ec005f2（#252 合併）|
| 日誌分支 `claude/gyo2-logging-10dfcd` | 8b83c745（第 4 步後半；宣告修訂 1 是 1d8327a0、動工前核准 df9cd068）|
| 第 4 批宣告 | **修訂 2**：本機 commit 36f8c3f4（未推送；`origin/claude/gyo2-authority-rollback-bda9ba` 1b0c6ef4 是修訂 1）|
| HANDOFF（分支 `claude/gyo2-time-design`） | 4541e71d（修訂 0～2 讀 7122169d；引用的兩列原話沒有變）|
| 共用規則登記頁 | `docs/cross-component-rules.md`（CCR-2、CCR-3）|
| 現行契約 | `docs/architecture/error-handling.md`（Legacy、R0–R6）、`docs/architecture/target/error-handling.md`（DI 1、2 與 Replacement Criteria，2026-11-13 核准）、`target/kernel.zh-Hant.md`（DI-KS 2、17、18、26）、`target/system-contract.zh-Hant.md`、`target/v2/orchestration.zh-Hant.md`（O-1～O-11）|
| 參考 | Java SE、Go、Rust（語言規格的公開知識）；Bevy、flecs、Unity Entities／Netcode、bevy_replicon、lightyear（本機源碼 `~/Code/GitHome`，第 3.2 節）|
| CI log | `gyo2` 8ec005f2 的 L1（run 44677942143），存主 checkout `build/evidence/gyo2-error-levels/ci-44677942143.log`（D5 的量測）|

---

## 1. 這一批做什麼（先用遊戲講）

Match 伺服器上，某個 System 有 bug，回了 `Err`。現在會發生的事（`gyo2` 8ec005f2，照源碼走一遍）：

1. Kernel 的 Executor 標 World `Interrupted`，呼叫預設處理器，**往 stderr 印一段 `kernel error:`**（`engine/kernel/src/Executor.cpp:36-40`、`:96`）。這一段不經 `Base::Log`，日誌 Module 收不到。
2. 同一個錯誤以 `Err` 回到編排層，編排層組 `WorldFailure` 回給 runner（`engine/orchestration/src/Product.cpp:440-451`）。
3. runner 照日誌卡的接法寫一筆 `Fatal`（`WriteFailure`）。

同一件事兩筆，一筆繞過日誌。若這一幀剛好在 `ReplaceWorld`（例：`DropOldFirst` 讀檔，新 World 的遊戲初始化壞了）：新 World 的 Executor 先印一段、編排層再記一筆 **`Warning`**（`Product.cpp:537`）、runner 再寫一筆 `Fatal`——三筆、三種寫法，其中致命的事被標成警告。

玩家讀檔失敗（`BuildNewFirst`，舊 World 照跑）時：編排層記一筆 `Warning`、同時把錯誤回給 runner（`Product.cpp:537`），遊戲多半還要自己記一筆「讀檔失敗」再顯示給玩家——又是兩筆。

這些都不是哪一處寫壞，而是**沒有級別**：誰產生、誰處理、誰記、記成什麼級別，沒有一處寫下來，型別上也看不出來，所以每一層都各自「處理一部分」。這一批做三件事：

1. **整合**：把 `gyo2` 與兩條進行中的線上所有錯誤的表示與回報路徑列成一張表（第 2 節）。
2. **分類**：照「壞到哪一層」分成四級（第 4 節），對照 Java、Go、Rust、Bevy、flecs、Unity（第 3 節）。
3. **處理方案**：每一級寫清楚產生方回什麼、呼叫者必須做什麼、能不能忽略、誰記日誌、只記一次記在哪、什麼級別（第 5 節）；runner 拿到任何錯誤都查得到怎麼做（第 5.3 節），而且建議把 E1／E2 分成不同的型別（D3），讓編譯器幫 runner 分。

做完之後，上面三個例子：bug → 只有 runner 結束處寫的那一筆 `Fatal`；`DropOldFirst` 讀檔壞掉 → 同樣一筆；`BuildNewFirst` 讀檔失敗 → 編排層不記，錯誤（E2 的型別）回給 runner，遊戲自己決定記不記、記成什麼。

---

## 2. 盤點（第一件事）

### 2.1 總表

欄位：誰產生／帶什麼／回到誰／**現在**誰記日誌、記幾次、什麼級別／呼叫者現在被期待怎麼做／分到第 4 節的哪一級。「記」指進 stderr 或 `Base::Log`；行號是 `gyo2` 8ec005f2，日誌分支與第 4 批另標。

**Base**

| # | 項目 | 誰產生 | 帶什麼 | 回到誰 | 現在誰記、幾次、級別 | 呼叫者被期待 | 級 |
|---|---|---|---|---|---|---|---|
| B1 | `GYO_ASSERT`／`GYO_UNREACHABLE`（`engine/base/include/engine/base/Assert.hpp:114-121`；處理器 `:93-104`） | 任何程式 | 條件文字、`source_location` | 不回；斷言處理器，處理器回來就 `abort` | 預設處理器 stderr 一次（`Assert.hpp:63-69`）；日誌分支接手時改走 `WriteFatalRecord`（category `assert`，`Fatal`）再交回前一個處理器 | 不處理，修程式 | E0 |
| B2 | `Base::Result<T, E>`／`Base::Err`、`Base::Error<Code>`、`CodedError`（`Result.hpp`、`Error.hpp`） | 各 Module 的 API | 模組自己的碼、`message`、`detail` | 直接呼叫者 | 不記（`Result` 是純值，target DI 2） | 必須處理（`[[nodiscard]]`；但沒有 `-Werror`，P10） | E2 |
| B3 | `Base::Log`（`Log.hpp:67-70`） | 用它的程式：**現在只有編排層五處** | level（Debug／Info／Warning／Error）、category、文字 | 不回；處理器 | 預設 stderr 一行；日誌分支 `Install` 接手 | 不影響流程 | （記錄的門面，不是錯誤） |
| B4 | `Base::DefaultLogHandler` 的 `catch (...)`（`Log.hpp:54`） | — | 寫日誌時的例外 | 吞掉 | — | — | （日誌的寫出本身不是錯誤通道；Legacy「platform boundary」以外唯一的 `catch (...)`，照「A log line is never worth a failure」留著） |
| B5 | `std::optional`、診斷資料、遊戲規則的結果（`error-handling.md`「These are not errors」） | — | — | — | — | — | 非錯誤 |

**Kernel**（`engine/kernel`）

| # | 項目 | 誰產生 | 帶什麼 | 回到誰 | 現在誰記、幾次、級別 | 呼叫者被期待 | 級 |
|---|---|---|---|---|---|---|---|
| K1 | System 回 `Err(KernelError)` | 作者的 System（或存取驗證的跳板） | `KernelErrorCode`、規則、World 名、上下文鏈、並列清單（`KernelError.hpp:103-131`） | Executor → `Run` 的呼叫者 | **Executor 的處理器 stderr 一次**（`Executor.cpp:36-40`、`:96`），不經 `Base::Log`、無級別；然後 `Run` 回同一個 `Err` | `Run` 的呼叫者不再跑這個 World（再跑是斷言，`Executor.cpp:187`）| E1 |
| K2 | Kernel API 回給 System 的 `Err`：除 K3 外的碼（`AccessViolation`、`StaleHandle`、`ScheduleRunning`、`NoAllotter`……） | Kernel | 同上 | System；World 已標 `Interrupted`（`Executor.cpp:89-97` 的 `Fail`），或違規已記在這次呼叫的錯誤裡、System 回 Ok 也以它結束（`Executor.cpp:363-376`；`KernelError.hpp:85-87`） | 同 K1（最外層 `Run` 記一次） | System 包一層上下文往外傳（DI-KS 2）；**System 處理不了**，World 已經要結束 | E1 |
| K3 | 回給呼叫者、同時記成診斷：`AllottedIdAlive`（`ExclusiveWorld::Spawn`，`Params.hpp:949-969`）、`ExclusiveWorld::SpawnWithId`（`Params.hpp:978`，回 `optional<SpawnRejectReason>`、記 `RejectedSpawn`）、`ExclusiveWorld::Insert<T>`（回 false、計入 `DroppedMissingEntity`，DI-KS 18） | Kernel | 碼或拒絕原因 | **只回 System**；不算 Run 的錯誤、World 不中斷；同時記成 World 的診斷紀錄 | 不記；診斷紀錄由 runner 取走（日誌 ②） | System 自己決定（換 id、放棄） | E2（記的那一半由診斷紀錄完成，第 5.2 節 E2 第 4 點） |
| K4 | 巢狀 run 裡的錯誤（`ExclusiveWorld::RunSchedule`） | Kernel | 同 K1 | **兩條路**：`RunSchedule` 的回傳值（外層 System 可以包了往上回）與 `world.leftError_`（`Executor.cpp:91-94`、`:167-168`）；最外層 `Run` 用 `Listed()` 依四個欄位比內容，避免同一個錯誤在並列清單出現兩次（`Executor.cpp:106-111`、`:364`） | 最外層記一次 | 同 K1；CCR-2：System 把 `RunSchedule` 的 `Err` 往外傳 | E1（P14） |
| K5 | `Trigger` 從 World 外觸發的 `Err`（`WorldExecutor.hpp:99-118`；`TriggerFromHost` 走 `Fail(..., false)`，`Executor.cpp:981`、`:1006`） | observer、hook | 同 K1 | `Trigger` 的呼叫者 | 處理器一次 | 同 K1 | E1 |
| K6 | System 或 hook 丟例外 | 作者程式 | 例外物件 | **不接**：World 標 `Interrupted`，例外往外丟（`WorldExecutor.hpp:29-31`） | 不記 | 呼叫者接或讓它往上 | E1（到了編排層） |
| K7 | `StartupReport`／`StartupError`／`StartupCode`（`KernelError.hpp:133-187`） | `Registry::BuildPlans`、`Finish` | 全部啟動錯誤，一次收齊 | 編排層（收進 `ProductStartupReport`） | 不記 | 不建 World（O-5） | E2 |
| K8 | `DynamicInsertRefused`（`Registry::Register`） | Kernel | 同 K1 | 編排層 `InsertSystems` | 不記；**註解寫「the caller logs the warning」**（`KernelError.hpp:74-75`；CCR-3；DI-KS 26） | 呼叫者記一筆警告 | E2 |
| K9 | `Registry::ExportZoneCSources` 的環境錯誤（`Fingerprint.cpp:523-528`，用 `SystemFailed` 碼） | Kernel | `KernelError` | 呼叫者 | 不記 | 處理 | E2（以 `KernelError` 為型別，P17）|
| K10 | `WorldExecutor::SetErrorHandler`（`WorldExecutor.hpp:119-121`；`Product::SetErrorHandler` 轉交，`Product.hpp:114-115`） | — | 收到 K1／K2／K4／K5 的 `KernelError` | — | 預設＝K1 的 stderr | 測試用來觀察：共用測試台 `tests/common/kernel/WorldTestSupport.hpp:121` 經它記下錯誤，約 49 處判定、9 個 Kernel 測試檔讀它；編排層 `[or-gi-error-once]`、`[or-replace-handler-kept]` | （第二條通道，P2） |
| K11 | 診斷紀錄 `RejectedSpawn`、`DroppedMessages`、作者的 `DiagnosticsWriter<R>`（`World.hpp:101-150`） | World | 被丟掉的那一筆與計數 | runner 在兩次 `Run` 之間取走（游標宣告） | 不記；日誌分支 `TakeKernelDiagnostics` 寫 | 不影響流程 | E3 |
| K12 | `breakOnKernelError` | Kernel | — | 除錯器 | 不記 | 不影響流程（會返回的掛鉤） | （除錯用，不是通道） |

**編排層**（`engine/orchestration`）

| # | 項目 | 誰產生 | 帶什麼 | 回到誰 | 現在誰記、幾次、級別 | 呼叫者被期待 | 級 |
|---|---|---|---|---|---|---|---|
| O1 | `ProductStartupReport`（O-E1～O-E7、各 World 的 `StartupReport`、登記單位的字串碼；`OrchestrationError.hpp:23-97`） | `Product::Build` | 全部啟動錯誤 | `main` | 不記 | `main` 決定（印出、結束） | E2 |
| O2a | `OrchestrationError`：`ScheduleFailed`、`WorldInterrupted`、`WorldMissing` | 入口 | 碼＋`failure`（一定有）＋`kernel` | runner | 不記（O-11）；但 K1 的 stderr 已印過 | runner 決定關程式或回主選單（O-3） | E1 |
| O2b | `ReplaceRegistrationFailed`、`ReplaceCarryOverFailed`、`ReplacePrepareFailed`、`ReplaceScheduleFailed` | `ReplaceWorld` | 碼＋`reporterCode`＋（`DropOldFirst` 或遊戲初始化弄壞第一個 World 時）`failure` | runner | **`Warning` 一次**（`Product.cpp:537`），**不論是不是結束 Product**；`ReplaceScheduleFailed` 另有新 World 處理器的 stderr（`:630`、`:645`） | 有 `failure` 時同 O2a，沒有時舊 World 照跑 | **同一個碼兩種級**（P4）；`BuildNewFirst` 換第一個 World 時另有 P15 |
| O2c | `InsertRefused`、`InsertBuildFailed` | `InsertSystems` | 碼＋`kernel` 或報告 | 呼叫者 | **`Warning` 一次**（`Product.cpp:720`、`:729`；O-9、CCR-3） | 呼叫者處理；World 照跑 | E2 |
| O2d | `UnknownWorld`（`RunSchedule`、`ReplaceWorld`、`InsertSystems`：`Product.cpp:487`、`:529`、`:699`）、`OwnerTaken` | 入口 | 碼 | 呼叫者 | 不記 | 呼叫者處理 | 現在 E2；**修訂 2：`UnknownWorld` 改成 E0**（第 17.1 節）；`OwnerTaken` 照舊 E2（P18）|
| O2e | `PrepareFailed`（準備函式回的碼，作者的碼在 `reporterCode`；`PrepareFn` 的回傳型別就是 `OrchestrationError`，`Setup.hpp:75`） | 作者的準備函式 | 作者的碼；**作者也能放 `failure`**，`Prepare` 只改 `code`（`Product.cpp:337-339`） | 編排層 → `O-E7`（啟動）或 `ReplacePrepareFailed`（替換） | 見 O1／O2b | — | E2（`DropOldFirst` 時 E1；P16）|
| O3 | `WorldFailure`／`WorldFailureKind`（`Interrupted`、`ReplacementFailed`、`ResourceExhausted`；`OrchestrationError.hpp:99-141`） | 編排層＋Kernel，壞掉當下收 | Product、World、怎麼壞、入口、schedule、`syncEpoch`、Kernel 碼與文字、最後成功的 schedule | 掛在每次拒絕上；`Product::Failure()` | 編排層不記（O-11） | runner 處理 | E1 的載體 |
| O4 | `RunnerExit`（`Setup.hpp:77-85`） | runner；預設 runner 是 `Product::Run`（`Product.cpp:495-503`） | `Success`／`Failure`＋文字 | `main` | 不記 | `main` 讀；要寫失敗得另外讀 `Failure()` | （runner 的結果，不是錯誤） |
| O5 | 例外穿過入口 | 作者程式（System、hook、準備函式、`InsertFn`） | 例外物件 | **往外丟**：`RunOn` 先收 `WorldFailure` 再丟（`Product.cpp:426-438`）；`ReplaceWorld` 的 `try`（`:617-678`）裡：`BuildNewFirst` 時 `Warning` 後丟（`:656`）、`DropOldFirst` 時 `Warning`＋收 `WorldFailure` 後丟（`:659-677`）；**`try` 之前**的步驟見 P12；`InsertSystems` 丟掉這一輪再丟（`:709-717`） | 替換時 `Warning` 一次 | runner 用 `catch (...)` 接、再讀 `Failure()`（日誌宣告 5.6 情形 (2)） | 有 `failure` 是 E1，沒有是 E2（P5） |
| O6 | 經 `Product::World(name)` 直接跑 Kernel（`Product.hpp:110-112` 公開；`Product` 沒有 `Trigger` 入口，World 外觸發事件只有這條路） | runner 或持有 Product 的程式 | `KernelError`（**沒有 `failure`**） | 直接呼叫者 | K1 的 stderr；下一個入口的 `Ended()` 才補一份 `entry = "outside the Product"`、Kernel 碼與文字都空的 `WorldFailure`（`Product.cpp:395-414`；守衛 `[or-ended-outside-fields]`，`CoverageTests.cpp:339-341`） | 沒寫 | E1（P13）|
| O7 | `Product::Build` 丟例外（`Configure`、`Build`、`Finish`、`Check`、準備函式；`Product.cpp:80-245` 沒有 `catch`） | 作者程式 | 例外物件 | `main` | 不記 | 沒寫 | 沒有 Product 存在，不是 E1；**修訂 2：和 D4 同一套，在 `Build` 的邊界轉成啟動報告（E2）**（第 17.2 節）|
| O8 | runner 自己丟例外（`Product::Run` 原樣往外，`Product.cpp:498`） | runner | 例外物件 | `main` | 不記 | 沒寫 | Product 有 `Failure()` 時 E1，否則規則 6 |

**Module 與執行緒**

| # | 項目 | 誰產生 | 帶什麼 | 回到誰 | 現在誰記 | 呼叫者被期待 | 級 |
|---|---|---|---|---|---|---|---|
| M1 | `world_time`：前置條件的 `GYO_ASSERT`（`TimeTypes.cpp:20-108`、`Sampling.cpp:10-20`）；準備函式的簽名回 `OrchestrationError`（`WorldTimeModule.cpp:129`，目前的實作不回錯誤）；固定步 System 回 `KernelError`（`FixedStep.hpp:41`） | 時間 Module | — | — | 不記 | — | E0／E2／E1 |
| M2 | `world_time`：**把 Kernel API 的 `Err` 用斷言接**（`FixedStep.hpp:26-33` 的 `Present`、`Called`：時間 Resource 不在是 Programmer Error） | 時間 Module | — | — | 斷言處理器 | — | E1 → E0 的升級（第 4.1 節）|
| M3 | `world_time`：`TimeDiscarded`（`TimeTypes.hpp:141`）以 `Message` 發 | 時間 Module | 丟掉的時間 | 讀者 | 不記；日誌分支 `TimeDiscardedLog` 描述器經 `Forward` 寫 | 不影響流程 | E3 |
| M4 | JSON：`JsonError`（`Syntax`……`OutOfMemory`；`JsonError.hpp`）；`bad_alloc` 在公開函式內接住轉成 `OutOfMemory`（`Writer.cpp:115-352`、`YyjsonDocument.cpp:63-69`） | JSON Module | 碼 | 呼叫者 | 不記 | 處理 | E2 |
| M5 | os：`OsError`（`OsError.hpp`，detail 帶原生碼） | os | 碼＋`errno=`／`win32=` | 呼叫者 | 不記 | 處理 | E2 |
| M6 | io、audio、threads（`ThreadError`；`RoleThread.cpp:60` 接 `std::system_error` 轉 `StartFailed`）、time（`TimeError`） | 各 Module | 碼 | 呼叫者 | 不記 | 處理 | E2 |
| M7 | 執行緒本體丟例外（`RoleThread.cpp:54-59` 的 `body(context)` 不接） | 作者程式 | 例外物件 | 沒有人 → `std::terminate` | 日誌分支的 terminate 處理器只 flush、不寫紀錄 | 規則 6 末句：執行緒入口自己接 | E0（行程結束；第 4.1 節）|

**日誌分支**（`claude/gyo2-logging-10dfcd` 8b83c745，未合併）

| # | 項目 | 誰產生 | 帶什麼 | 回到誰 | 誰記 | 呼叫者被期待 | 級 |
|---|---|---|---|---|---|---|---|
| L1 | `LoggingError`（`OpenFailed`、`StartFailed`） | `Install`、檔案輸出端 | 碼 | `main` | 不記 | 處理（沒有檔案也能跑） | E2 |
| L2 | `WriteFatalRecord`／`FatalWriteReport` | 致命路徑 | 一筆 `Fatal` | 呼叫者（回報寫成了沒有） | 它就是寫 | — | E0、E1 的最後一道 |
| L3 | `WriteFailure`／`WithFailureLog`（`WriteFailure.cpp:39-56`：**runner 回來之後**讀 `product.Failure()` 寫一次；例外離開 runner 時同樣讀 `Failure()`） | runner 或 `main` 呼叫 | `WorldFailure` | — | 每次呼叫寫一筆 `Fatal`（category `world`），**不去重**（8b83c745） | runner 退出 | E1 的處理 |
| L4 | 斷言處理器接手、`std::terminate` 處理器（只 flush） | 日誌 | — | 前一個處理器 | 斷言：一筆 `Fatal`；terminate：不寫紀錄，只 flush | — | E0 |
| L5 | `Forward`、`TakeKernelDiagnostics`、`Logging::Write` | 日誌 | — | — | 照 sink 的設定 | — | E3 的寫出（④ 也給作者寫 E2 的處理紀錄） |
| L6 | Sink 寫失敗（LG-11：計數、`Good()` 變 false） | 輸出端 | — | — | 不記 | 不影響流程 | E3 |

**第 4 批宣告**（`authority-rollback-declaration.zh-Hant.md` **修訂 2**，本機 36f8c3f4；未動工、等使用者重新核准）

| # | 項目 | 宣告怎麼寫 | 級 |
|---|---|---|---|
| A1 | `Build` 時才知道的錯誤（沒選策略、`rollback.Input<T>` 時 `T` 未登記、兩筆作者登記 options 不同）：Module 存起來、在 `Check` 用字串碼報（`AU-E…`、`RB-E…`；第 4.2 節） | 進 `ProductStartupReport` | E2 |
| A2 | 共用槽凍結後再寫、凍結後 `Create<T>` 或可寫的取用：斷言 | `GYO_ASSERT` | E0 |
| A3 | `Find<T>` 找不到：**回空**（呼叫者自己判）；Module 在 `Build` 找不到依賴、產品自己的 `build` 找不到：斷言，不新增 O-E 碼（36f8c3f4 第 338 行） | 空值＋斷言 | 非錯誤（沒有值）／E0 |
| A4 | 兩個登記單位 `Create<T>` 同一個型別：斷言（第 339 行） | `GYO_ASSERT` | E0 |
| A5 | `World::Summary` 碰到沒有雜湊函式的型別：回錯誤並列出型別（第 3 節） | 回錯誤 | E2 |
| A6 | 回滾超過深度：放棄這次回滾、記診斷（第 435、666 行） | 診斷紀錄 | E3 |
| A7 | 「還沒實作」錯誤：呼叫時回 `Result` 的錯誤（部分元件的權威值、「這份沒帶」等；第 318 行） | `Result` | E2 |
| A8 | 重複登記的裁決：作者的蓋過代為的、不論先後；兩筆作者或兩筆代為 options 不同是啟動錯誤（第 6 節第 3 點；回滾份 5.4；權威份 4.3、DI-A 11） | 蓋過不報；options 不同進 `Check` | 蓋過＝非錯誤（P6）；options 不同＝E2 |
| A9 | 選了 P2P 佔位策略：啟動錯誤（S3-6） | 進 `Check` | E2 |
| A10 | `Tick` 的寫者不在名單：權威的專用檢查報錯（G4） | 啟動錯誤 | E2 |

### 2.2 觀察到的問題（AGENTS §2 Valid evidence：每條附在哪、什麼、幾次）

主會話查到四處（P1～P3、P6），本卡照源碼再確認，另找到其餘各條（P12～P18 是敵對式檢查找到、本卡照源碼確認的）。

- **P1 致命的事被記成警告（主會話查到，已確認）**：`ReplaceWorld` 的 `fail`（`Product.cpp:536-558`）**先**寫一筆 `Warning` 再看要不要收 `WorldFailure`；`DropOldFirst` 失敗與「替換時跑的遊戲初始化弄壞第一個 World」都是 Product 結束（E1），記成 `Warning`；例外路徑 `:659` 同；`:656` 在第一個 World 已被 `RunGameInit` 弄壞（Product 已結束）時仍寫「the old World goes on」。違反 O-11「編排層不寫日誌」；runner 再寫 `Fatal` 就是兩筆。守衛 `[or-failure-not-logged]`（`tests/common/orchestration/CoverageTests.cpp:305-329`）只走 `RunSchedule`。日誌宣告第 3 節現況表（`logging-declaration.zh-Hant.md` 第 89 行）把這五處當成入口 ③ 的正常用途接下來。
- **P2 Kernel 記一次、runner 又記一次（主會話查到，已確認）**：`Fail` 先呼叫處理器（預設 `fprintf(stderr)`）再回 `Err`（`Executor.cpp:89-97`）——Go 的「log and return」反例。Kernel 單獨用時 Executor 是最外層（2026-11-11 的前提，target DI 1「the Executor is the outermost layer and handles once」），編排層與 runner 疊上去之後它不再是最外層，前提換了、條文沒跟。stderr 那筆不經 `Base::Log`，所以 `[or-failure-not-logged]` 的 `WarningCounter` 數不到它，那個守衛在 K1 的 stderr 照印時照樣通過。`ReplaceWorld` 的新 World 也帶處理器（`Product.cpp:323-328`），`:630`、`:645` 的失敗會多印一段。
- **P3 記一次又回傳（主會話查到，形狀相同、已核准）**：O-9（動態插入被拒、建 Plan 失敗：`Warning` 一次＋回錯誤，`Product.cpp:720`、`:729`）與 `ReplaceWorld` 的 `BuildNewFirst` 失敗（`:537`）。O-9 的條文來自使用者 2026-11-17 的原話（`v8-arch-gyo2-dynamic-insert-per-world`）：「插入就拒絕，返回信息（不退出，但是System無法插入，日誌報警告）。」並登記成共用規則 CCR-3（`docs/cross-component-rules.md:26`：「the caller logs the warning; the kernel only returns the error」）與 DI-KS 26。編排層是 Kernel 的呼叫者，照 CCR-3 記了警告、又把錯誤往上回——到它自己的呼叫者那裡，同一個錯誤被「處理」第二次。O-9 和 O-11「編排層不寫日誌」互相矛盾。
- **P4 同一個碼兩種級**：`ReplacePrepareFailed` 等四個碼在 `BuildNewFirst` 是「這次替換失敗、舊 World 照跑」，在 `DropOldFirst` 是「Product 結束」（`Product.cpp:538-556`）。現在分得出來的方法是看 `failure` 有沒有值，但沒有任何條文說「這就是級別」，而且這個分法有反例（P12、P13、P16）。
- **P5 致命的事從兩條路到 runner**：Kernel 回 `Err` 時以回傳值到；System 丟例外時以例外到（`Product.cpp:426-438` 收好 `WorldFailure` 再丟）。所以 runner 要 `catch (...)`（日誌宣告 5.6 情形 (2)），而 `error-handling.md` 規則 6 除了平台邊界之外不准 `catch (...)`；規則 6 也寫「不要用例外回報 Engine API 的失敗」，編排層的入口把例外往外丟就是用例外回報。
- **P6 第 4 批「重複登記的裁決」（主會話查到，已確認）**：作者的登記蓋過代為的，不報。宣告裡沒有記警告的地方（`警告|Warning` 在修訂 1、修訂 2 與兩份子宣告都是 0 命中）；形狀的問題是「兩筆不一致時默默選一筆」。本卡判為**設計的優先順序，不是錯誤**（第 5.5 節；第 13 節第 8 題）。
- **P7 回給呼叫者又記成診斷**：K3 的三個 API。System 若也寫一筆，日誌的 ② 又寫一筆。需要一條規則（第 5.2 節 E2 第 4 點）。
- **P8 預設 runner 不寫失敗**：`Product::Run` 的預設 runner 回 `RunnerExit{Failure, 文字}`（`Product.cpp:495-503`），`main` 要寫失敗得再讀 `Failure()`；沒讀就沒有任何一筆。這是 O-11 的設計（記不記是日誌的事），但「不理是錯的」沒有地方寫。
- **P9 日誌宣告的 LG-2 還寫「同一份 `WorldFailure` 只寫一次」**（`logging-declaration.zh-Hant.md` 第 8 節），實作已拿掉去重（8b83c745）；HANDOFF 寫了合併前核准時改。本卡只列交界，不改它的檔。
- **P10 `[[nodiscard]]` 只是警告**：`git grep -nE "Werror|/WX|COMPILE_WARNING_AS_ERROR|Wunused-result"` 在 CMake 與 CI 0 命中（終了碼 1；正例見第 9.4 節）。所以「不理 `Result`」在建置上只是一行警告。**現況的命中數**：`gyo2` 8ec005f2 的 L1（run 44677942143）三平台 log 用 `nodiscard|unused-result|ignoring return value|discarding return value|C4834|C4858` 走查 **0 命中**；正例：同一份 log 裡三平台各有其他警告（linux-x64 101 行、macos-arm64 3 行、windows-x64 53 行，例 `Executor.cpp(431): warning C4456`）。CI 用 ccache（快取命中時照樣重放編譯器的 stderr）。
- **P11 條文與註解還寫「Executor 記一次」**：`docs/architecture/error-handling.md`（「Programmer Error detected from runtime data」）、`target/error-handling.md` DI 1、2、Kernel DI-KS 2「Executor 記錄一次」、DI-KS 17「錯誤處理只由最外層呼叫一次」（`target/kernel.zh-Hant.md:93`）、DI-KS 18「錯誤處理只呼叫一次」（`:96`）、`target/system-contract.zh-Hant.md:19`、`target/v2/system-contract-page.zh-Hant.md:337`、`:422`、`:435`、`target/v2/rebuild-vocabulary.zh-Hant.md:125`、CCR-2「reported once」、CCR-3；註解 `KernelError.hpp:58-59`、`:74-75`、`WorldExecutor.hpp:27-28`、`:38-39`、`:105-106`、`:119-121`、`World.hpp:528`、`Executor.cpp:1-4`；`kernel_guard.py` K6 的豁免。
- **P12 `DropOldFirst` 丟掉舊 World 之後、`try` 之前的步驟丟例外：位置空了、沒有 `failure`（既有缺陷）**：`Product.cpp:561-567` 先銷毀舊 World；`:573` `Register`（裡面跑登記單位的 `Build`／`Finish`／`Check` 與產品的 `build`，`:265-293`，都是作者程式）、`:581` `MakeExecutor`、`:606-615` 帶過去的 `put` 都在 `try`（`:617`）之外。這裡丟例外（作者程式或 `bad_alloc`）：`failure_` 沒設、例外直接離開；下一個入口的 `Ended()` 把「空位而沒有記錄」當成「替換進行中」略過（`:402` 的 `continue`），`RunFrame` 走到 `RunOn` 對空指標取值（`:424`）。這是「Product 結束了但沒有 `failure`」的反例。
- **P13 直接跑 Kernel 的路沒有 `failure`**：O6。runner 照「收到就退出」馬上退出時，`WithFailureLog` 讀 `Failure()` 是 null，一筆都不寫；D1 (b) 之後連 Kernel 那一筆也沒有。
- **P14 Kernel 裡已有「第二條通道＋照內容比對」**：K4。`Listed()`（`Executor.cpp:106-111`）與 `SameError`／`ContainsError`（`World.cpp:26-36`，`:508` 的 `CombineCall` 在用）兩處都用 `code`、`rule`、`message`、`detail` 判斷「是不是同一個錯誤」——和使用者否定的去重鍵同形（兩個內容相同的不同錯誤會被當成一個；照源碼造得出，D9）。
- **P15 `BuildNewFirst` 換第一個 World、新 World 上的遊戲初始化失敗：舊的第一個 World 永遠不會跑遊戲初始化**：`Product.cpp:625-637` 先設 `gameInitRan_ = true`，失敗時 `BuildNewFirst` 沒有 `failure`、舊 World 照跑；之後 `RunFrame` 不再跑遊戲初始化（`:471`），明確呼叫 `RunGameInit` 是斷言（`:457`）。O-1（`orchestration.zh-Hant.md:16`）寫「任何一張回錯誤：停下、回傳、記為已跑（Product 就此結束，O-11）」——程式偏離 O-1。
- **P16 作者的準備函式能造出帶 `failure` 的錯誤**：O2e。`BuildNewFirst` 的 `fail` 不清它（`Product.cpp:539-556`）——Product 沒結束而錯誤帶 `failure`。
- **P17 Kernel 裡可恢復的路用 `KernelError`**：K3 的 `AllottedIdAlive`、K8、K9。`target/error-handling.md` 既有的 Replacement Criteria（第 44 行）：「If a kernel error path needs to be recoverable…it is a Runtime Error and belongs to the existing `Result` row with a module error type」——現有程式已經觸發它。
- **P18 `UnknownWorld` 是 E2、同入口的 schedule 名寫錯是斷言**：`RunSchedule` 的 World 名不存在回 `UnknownWorld`（`Product.cpp:486-488`），schedule 名不存在是 Kernel 的斷言（`Executor.cpp:191`）；`Product::World(name)`、`SetErrorHandler` 對不存在的名字也是斷言（`:507`、`:518`）。同一類「照名字指一個東西」的呼叫，兩種級。修訂 2 併入本批（第 17.1 節）。

---

## 3. 別家怎麼分（同一個問題：級別怎麼分、每一級誰處理、能不能被忽略）

### 3.1 語言

| | 級別 | 誰處理 | 能不能忽略 | 記在哪 |
|---|---|---|---|---|
| **Java** | `Throwable` 下分 `Error`（`OutOfMemoryError`、`StackOverflowError`、`AssertionError`：「合理的程式不該接」）與 `Exception`；`Exception` 再分 checked（編譯器要求接或宣告 `throws`）與 `RuntimeException`（unchecked，多半是程式錯誤） | checked：呼叫者；`RuntimeException`、`Error`：一路往上到執行緒頂端（`UncaughtExceptionHandler`）或框架的最外層 | checked 不能默默忽略（編譯錯誤），但能 `catch {}` 吞掉——公認的壞習慣；`Error` 照規範不接 | 慣例「log or rethrow, not both」；頂端處理器記一次 |
| **Go** | error 值（呼叫者處理）與 `panic`（程式錯誤、不可恢復；`recover` 只在少數邊界——例如 HTTP server 每個請求的頂端——轉回 error 或結束那個單位）；error 用 `%w` 包上下文，`errors.Is`（比哨兵值）、`errors.As`（取出某型別）判斷 | error：直接呼叫者，「處理一次」：要嘛包一層往上回，要嘛在這裡處理，不兩者都做；panic：最外層 | 語言允許 `_ = f()`，那是明寫的決定；`errcheck` 等工具抓沒寫的 | 處理的那一層記一次；panic 由 runtime 印堆疊 |
| **Rust** | `Result<T, E>`（可恢復）與 `panic!`（bug；`.unwrap()`／`.expect()` 把 `Err` 明寫升級成 panic）；`catch_unwind` 只給執行緒或 FFI 邊界 | `Result`：呼叫者（`?` 往上傳）；panic：執行緒結束，`JoinHandle::join` 拿到 | `Result` 是 `#[must_use]`：不用就警告，專案常用 `#![deny(unused_must_use)]` 變成錯誤；`let _ =` 是明寫的忽略 | 處理的那一層；panic hook 印一次 |

三家共同的骨架：**(一) 程式本身不可信**（Java `Error`、Go `panic`、Rust `panic`）——不接、最外層記一次、結束；**(二) 這次請求失敗**（checked／error／`Result`）——直接呼叫者處理，往上傳時包上下文、不記，處理的地方記一次；**(三) 明寫的升級**（Java 包成 `RuntimeException`、Go `panic(err)`、Rust `.expect()`）。分歧：Java 用型別階層加編譯器強制，Go 靠慣例與工具，Rust 靠 `#[must_use]`（可設成錯誤）；Go 與 Rust 有「工作單位的邊界」（HTTP 請求、執行緒）把不可恢復的事收成一個值交給上層，Java 靠例外一路往上。

### 3.2 引擎

偵察：Opus 5.5／high，唯讀；版本 bevy 0.20.0-dev 6a911653f、flecs 09790b5f7、com.unity.entities 1.5.0、com.unity.netcode 7.2.0；報告存主 checkout `build/evidence/gyo2-error-levels/recon-reference-engines.md`（本機 macOS，git 不追蹤）。行號相對 `~/Code/GitHome/`；敵對式檢查抽查過 Bevy、flecs、Unity、Netcode 各數處。

| | 級別怎麼分 | 每一級誰處理 | 能不能忽略 | 記在哪、幾次 | release 一樣嗎 |
|---|---|---|---|---|---|
| **Bevy** | `BevyError` 帶 `Severity`（`Ignore < Trace < Debug < Info < Warning < Error < Panic`，`bevy_ecs/src/error/bevy_error.rs:297-314`），文件定位為「advisory metadata」，怎麼處理由處理器定（`:12-20`）；`?` 轉來的錯誤**預設 `Panic`**（`:536-545`）；引擎本身幾乎不標級（只有一處 `with_severity`） | 一個處理器：World 的 Resource `FallbackErrorHandler`，預設 `match_severity`（`error/handler.rs:116-138`）；Executor 對 System 的 panic 一律 `catch_unwind`，包成 `Panic` 級的 `BevyError` 交給**同一個**處理器（`schedule/executor/single_threaded.rs:285-295`、`multi_threaded.rs:856-868`）。排程裡 System 的 `Err` 不往上回：處理器是唯一通道 | 可以：`Severity::Ignore`、`ignore` 處理器、`try_*` 命令；參數驗證的 `Skipped` 明文不算錯（`system/system.rs:377-412`）。把處理器換成 `warn` 時連 panic 也吞了 | 處理器一次；但單執行緒 Executor 上 `Panic` 級的錯誤處理器被呼叫**兩次**（第一次 `panic!` 被外層接住、再呼叫一次 `resume_unwind`），panic 改成記錄時 panic hook 與處理器各一次——錯誤本身不去重，只有 backtrace 去重（`bevy_error.rs:596-617`） | 一樣（沒有 `debug_assertions` 分支） |
| **Bevy `AppExit`** | `Success`／`Error(NonZero<u8>)`（`bevy_app/src/app.rs:1550-1557`），System 以 `Message` 寫 | runner 每次 update 後看 `should_exit()`（`schedule_runner.rs:91-106`；`run_once` `app.rs:1516-1527`） | — | — | — |
| **flecs** | 依巨集：`ecs_assert`（內部不變式，必 abort）、`ecs_check`／`ecs_throw`（參數錯，預設 abort，`FLECS_SOFT_ASSERT` 時 `goto error` 回錯誤值）、`ecs_err`＋回傳碼（可恢復）、`ecs_abort`／`ecs_always_assert`（永遠在）（`include/flecs/addons/log.h:464-550`）；約 25 個錯誤碼（`:694-761`） | 發生點自己：abort 交 `ecs_os_api.abort_`（可換）；可恢復的由呼叫者看回傳值 | 回傳值可以不看（C 不強制）；assert 不能；soft 模式之後文件說「應視為 undefined state」 | **發生點記一次**（fatal 或 err），往上傳的呼叫者只傳回傳值、不再記（`src/query/api.c:396-460`、`src/addons/system/system.c:315-319`）；System 回呼是 `void`，沒有錯誤通道（`include/flecs.h:583`、`:592`） | 不一樣：`FLECS_NDEBUG` 編掉 assert／check／throw（`FLECS_KEEP_ASSERT` 時 assert 留下，`log.h:470`） |
| **Unity Entities** | 沒有分級，只有例外與 `[Conditional("ENABLE_UNITY_COLLECTIONS_CHECKS")]` 的安全檢查 | 最內層 `ComponentSystemGroup` 對每個子 System `try { } catch (Exception e) { Debug.LogException(e); }`，**繼續跑下一個 System**（`Unity.Entities/ComponentSystemGroup.cs:713-741`） | 例外被吞掉，World 照跑 | 最內層 group 一次 | 不一樣：檢查在 runtime build 移除，文件說「aren't any guarantees」（`Documentation~/concepts-safety.md:13`） |
| **Unity Netcode** | 日誌 5 級（`NetDebug.LogLevelType`，`Runtime/Debug/NetDebug.cs:338-361`）＋斷線原因列舉（`NetworkStreamDisconnectReason`，原因不是嚴重度） | 偵測點 `LogError` 並在連線實體加 `NetworkStreamRequestDisconnect`（`Runtime/Rpc/RpcSystem.cs:455-474`），收的 System 斷線、發事件給遊戲（`NetworkStreamReceiveSystem.cs:803-808`、`:1070-1120`） | 協定錯誤一定斷線 | 偵測點 `Error` 一次；收尾 `Debug` 一次 | 部分 `LogError` 只在 checks 下記，斷線照做 |
| **bevy_replicon／lightyear** | 沒有分級型別：致命（協定雜湊不符）→ 斷線；單則訊息錯誤 → `error!` 後丟掉、繼續（`bevy_replicon/src/client.rs:283-331`；`lightyear/crates/transport/transport/src/plugin.rs:281-286`）；內部不變式 → `panic!` | 偵測點 | 單則錯誤丟掉繼續 | 偵測點一次 | 查不到差異 |

**和 GYO 的相似點與取捨**（取能力、不取技術債）：

- **處理一次、只在一個地方記**：flecs（發生點記、往上不記）、Unity（最內層 group 記一次）、Netcode（偵測點記一次）都靠結構避免重複，**沒有一家對錯誤去重**（Bevy 只對 backtrace 去重）。取：第 5.1 節的原則。GYO 選「處理的一方記」而不是 flecs 的「發生點記」：GYO 的錯誤往上回、產生方不知道上面會不會處理（flecs 的 `ecs_err` 是發生點記一次再回值——對 GYO 就是「記又回傳」，P3）。
- **Bevy 把 panic 收進同一個處理器**：Executor 在工作單位（System）的邊界 `catch_unwind`、轉成值。取：D4 (b) 的形（在 Product 入口的邊界把 `std::exception` 轉成值）。不取：Bevy 讓處理器決定吞不吞（換成 `warn` 連 panic 都吞）——GYO 的 E1 不能被設定成繼續（O-11）。
- **Bevy 的 `Severity` 是附在錯誤上的欄位、由處理器解讀**：不取（D3 (c)）。Bevy 自己的程式幾乎不標，「未標＝Panic」才是實際的級別；GYO 的級別由邊界定（第 4.1 節），附欄位等於兩處維護同一件事。
- **Bevy 的處理器是排程裡的唯一通道**（System 的 `Err` 不往上回）；GYO 的 `Err` 往上回，所以 Kernel 的處理器是多出來的第二條（D1）。
- **Unity 吞例外、World 照跑**：不取（違反使用者 D2「失敗了還繼續，就是吞錯誤」）。
- **flecs、Unity 的 debug／release 兩套**：不取（GYO 已定一個事件一種性質、release 一樣，`v8-arch-kernel-break-hook`）。
- **Bevy `AppExit` 與錯誤沒有自動連動**：和 GYO 一樣，runner 自己決定怎麼結束（O-3）；GYO 多一條：E1 一定讓 runner 結束那個 Product。
- **網路類（Netcode、replicon、lightyear）**：致命＝斷線帶原因、單則訊息錯誤＝丟掉繼續。網路 Module 回來時，單則訊息錯誤對應 E3、斷線原因是遊戲規則的結果（非錯誤）、協定不一致是 E2——在網路 Module 的宣告照本頁定，本批不做。

**各家分歧**（交使用者的那幾題在第 13 節標出）：未標級時的預設（Bevy panic／Unity 記了繼續／flecs debug abort、release 消失）；panic 與 `Err` 同路（Bevy）或只有例外（Unity）或沒有通道（flecs 的 System）；處理權集中（Bevy 一個處理器）或分散（flecs、Netcode）；debug／release 一致（Bevy）或兩套（flecs、Unity）。

### 3.3 GYO 特有之處

- **一個 World 是一條鏈**（使用者 D2）：World 壞了不能自癒、不能拿替換來救、Product 就結束（O-11）。三家語言都沒有「World」這一層；最接近的是 Go 的「工作單位」（一個 HTTP 請求壞了，伺服器結束那個請求、行程照跑）——GYO 的 Product 是那個單位，runner 是那個邊界。
- **致命的事以值回傳，不靠例外往上**：使用者的理由「c++沒有try catch……有可能Even機制本身已經壞了」（`-world-events-not-via-event`）、「包進一個普通的 Result 回去也沒問題」（`-logging-no-failure-dedup`）。所以 E1 的載體是值（`WorldFailure`），像 Go 的 `recover` 之後回 error，而不是 Java 的 `Error` 一路丟。
- **System 回 `Err`＝宣告這個 World 不能繼續**（`-err-is-bug-world-readable`）：執行時的狀況由 System 自己處理、回 Ok；回 `Err` 就是 bug 等級。這是 GYO 的「明寫的升級」。

---

## 4. 分級（第二件事）

### 4.1 判準：壞到哪一層、相對於誰

級別是**相對於一個單位**說的：行程、Product、World、一次請求。錯誤跨過邊界時，**接手的一方照「壞掉的是不是我持有的東西」重判**；碼與內容不變。

| 級 | 名字（暫） | 還能信任的 | 意思 | Java | Go | Rust |
|---|---|---|---|---|---|---|
| **E0** | 程式錯誤（中止） | 什麼都不能（行程結束） | 前置條件或內部不變式壞了：程式本身錯；沒人接的例外（執行緒本體、`noexcept` 裡丟出）走到 `std::terminate` 也在這一級 | `Error`（`AssertionError`） | `panic`（不 recover） | `panic`（abort） |
| **E1** | 致命：World 壞了、持有它的 Product 結束 | 行程與其他 Product；**不能**信任壞掉的 World 與持有它的 Product | 執行期才看得出的程式錯誤（DI-KS 2 的那一列）、例外離開 World 的 Run、在 World 的 Run 或 `DropOldFirst` 替換裡資源耗盡、`DropOldFirst` 失敗留下空位 | `RuntimeException` 到了框架的最外層 | 工作單位頂端 `recover` 後回的 error | `catch_unwind` 在執行緒邊界收到的 panic |
| **E2** | 失敗：這次請求沒做成 | 所有東西，包括發出請求的一方持有的 World | 外部資料或環境讓一個正確的程式失敗（現行契約的 Runtime Error）；啟動時的組裝錯誤（一次收齊的報告） | checked `Exception` | error 值 | `Result::Err` |
| **E3** | 警告：照設計丟掉或限制了某樣東西 | 所有東西，World 健康 | 訊息被修剪、生成被拒、時間被丟棄、回滾超過深度、輸出端寫失敗：照設計的上限處理掉了，流程不變；以**紀錄**表示，不是錯誤值 | （日誌的 `WARN`） | （`log.Printf`） | （`warn!`） |
| — | 非錯誤 | — | 沒有值（`std::optional`）、遊戲規則的結果、設計的優先順序（第 5.5 節） | — | `v, ok` | `Option` |

**分到哪一級的順序**（擴充 `error-handling.md` 規則 1，不取代它）：

1. 程式本身錯、在呼叫當下就看得出（前置條件、API 誤用）→ **E0**（`GYO_ASSERT`）。規則 1 第 1 步、規則 3 不變。
2. 程式錯、但要到執行期的資料才看得出（DI-KS 2 的那一列），或一個 World 的 Run 不能正常結束（例外、資源耗盡），或一個 World 的位置空了 → 對**持有那個 World 的單位**是 **E1**。
3. 外部資料或環境讓這次請求失敗，而發出請求的一方持有的東西都還健康 → **E2**（`Result`）。規則 1 第 2 步、規則 2、4、5 不變；啟動錯誤的報告也在這一級（還沒有 Product，見下）。
4. 照設計的上限丟掉或限制了某樣東西、World 健康、流程不變 → **E3**（診斷紀錄或 `Message`）。
5. 只是沒有值、遊戲規則的結果、設計的優先順序 → **非錯誤**。規則 1 第 3～5 步不變。

**各層怎麼判**：

- **Kernel 單獨用**（沒有編排層：工具、測試）：`Run`／`Trigger` 回的 `Err` 是 E1（`World::State()` 是 `Interrupted`），也可能以例外到（K6）；Kernel 其他函式回的是 E2（K3、K8、K9）。`Run` 的呼叫者就是 E1 的處理者。
- **編排層**：入口回的錯誤，對 runner 是 E1（這個 Product 有 World 壞了）或 E2（這次請求沒做成，Product 健康）。
- **`Product::Build`**：還沒有 Product，失敗一律是 E2（報告）；`Build` 裡丟出的 `std::exception` 也轉成報告裡的一筆（修訂 2，第 17.2 節）。
- **World 外照名字指一個東西的呼叫**（修訂 2，第 17.1 節；第 13 節第 11 題）：名字不存在是 E0（API 誤用，規則 3）；名字可能來自資料時，呼叫者先用查詢函式（`HasWorld`），查詢的「沒有」是非錯誤（`bool`／`std::optional`，規則 1 第 3 步）。System 裡的同類錯誤（X-12 `UnknownSchedule`）照 DI-KS 2 是 E1，不是這條規則的例外。

**現有的升級與降級**（照源碼列全；DI 3 會寫進「只有這幾條」）：

| 方向 | 在哪 | 為什麼成立 |
|---|---|---|
| 升 E2 → E1 | System 把收到的 E2 包一層回 `Err` | 明寫的升級：宣告「這個 World 不能繼續」（`-err-is-bug-world-readable`） |
| 升 E2 → E1 | `DropOldFirst` 替換的任何一步失敗（重新登記、帶過去、準備函式、新 World 的 schedule） | runner 選了「先丟舊的」：舊 World 已經沒了，失敗就留下空位（O-8；使用者「舊相識 Object obj = new Object失敗了一樣」） |
| 升 E1 → E0 | `world_time` 的 `FixedStep.hpp:26-33`：時間 Resource 被拿掉時 Kernel API 回的 `Err` 用斷言接 | 明寫的升級（等於 Rust 的 `.expect()`）：產品拿掉了時間 Module 自己登記的 Resource，是程式錯 |
| 降 E1 → E2 | `BuildNewFirst` 替換時新 World 的 `Run` 回 `Err` 或丟例外 | 壞掉的是**還沒交給 Product 的新 World**，對 Product 不是「持有的 World 壞了」：新 World 丟掉、舊 World 照跑（O-8）。P15（換第一個 World 時遊戲初始化在新 World 上失敗）照 D10 建議的 (i) 也在這一條：E2、`gameInitRan_` 還原 |

**不准的**：同一個 Product 持有的 World 壞了之後，E1 不會變回 E2（不准拿替換救壞掉的 World，O-11）；E3 不升級（它不是錯誤值）。

### 4.2 和現行契約的對齊

| 現行（`error-handling.md`、`target/error-handling.md`） | 本卡 |
|---|---|
| Assert＝Programmer Error | E0，不變 |
| Result＝Runtime Error | E2，不變 |
| 「Programmer Error detected from runtime data」那一列（Executor 記一次、World `Interrupted`） | E1 的來源之一；**改**：「Executor 記一次」改成「最外層 `Run` 回傳一次，Kernel 不寫日誌，處理的一方記一次」（D1） |
| These are not errors（optional、診斷、遊戲規則） | 非錯誤；**加** E3：照設計的上限丟掉的事，以紀錄表示（現行「Several problems or warnings ... diagnostics data」的延伸，不是新機制） |
| 規則 6（不用例外回報 Engine API 的失敗；邊界只接 `std::exception`、不准吞） | 不變；編排層的入口照它做（D4） |
| 「no global error enum」 | 不變：級別不做成 Base 的列舉或錯誤型別的欄位（D3 (c) 退掉）|
| target RC「kernel error path needs to be recoverable → module error type」 | 現有程式已觸發（P17）。**改**成「Kernel 裡 E2 的路可以用 `KernelError` 當型別，級別由 API 的文件寫明」（第 13 節第 6 題）|

---

## 5. 處理方案（第三件事）

### 5.1 一條原則：處理一次

照 Go 的「handle errors once」與 Java 的「log or rethrow, not both」：**一個錯誤要嘛往上傳（可以包一層上下文），要嘛在這裡處理（決定接下來怎麼做，必要時記一次），不兩者都做**。由此推出：

- **產生的一方不記**（Kernel、編排層、Module）。
- **往上傳的一方不記**，只加上下文（規則 4、5；`KernelError::Wrap`）。
- **處理的一方至多記一次**，記成它判斷的級別（第 5.2 節）。
- **一個錯誤只有一條通道到處理的一方**：不另開回呼、事件、全域處理器把同一個錯誤再送一份（D1）。「通道」指**主動把錯誤送到處理者手上**的路；壞掉的單位身上留著的死因（D8：Executor 上中斷當下收好的錯誤），不送給任何人、只由唯一的記錄者讀一次，不是通道。例外：產生方照設計記的診斷紀錄（E3 的紀錄），規則見第 5.2 節 E2 第 4 點。
- 「同一個錯誤」靠**身分**認，不靠內容比（P14、D9）：同一個錯誤物件的副本身分相同；內容相同的兩個錯誤是兩個。

### 5.2 每一級的處理

| | E0 程式錯誤 | E1 致命 | E2 失敗 | E3 警告 |
|---|---|---|---|---|
| **產生方回什麼** | 不回：`GYO_ASSERT` → 斷言處理器 → `abort` | Kernel：`Run`／`Trigger` 回 `Err(KernelError)`、World `Interrupted`；編排層：入口回 **E1 的型別**（D3 (b)：`ProductEnded`，帶 `WorldFailure`；選 (a) 時是帶 `failure` 的 `OrchestrationError`） | `Result<T, 模組的錯誤>`；啟動時是一次收齊的報告（`StartupReport`、`ProductStartupReport`） | 不回錯誤：World 記診斷紀錄，或發 `Message` |
| **呼叫者必須做** | 無（修程式） | 收到的呼叫點：**不再呼叫這個 Product 的任何入口**，結束 runner（回 `RunnerExit{Failure}`）；之後關程式或另建 Product（回主選單）由 `main` 決定（O-3）。**讀 World 可以**（取診斷紀錄、救未存檔的內容，`-err-is-bug-world-readable`；日誌批「先 `WriteFailure` 再取」）。Kernel 單獨用時：`Run` 的呼叫者不再跑這個 World | 往上傳（包上下文）或在這裡處理（決定、降級、改走別條路、顯示給人），二選一 | 無；日誌照常訂閱或取走 |
| **能不能忽略** | 不能（處理器回來就 `abort`） | **忽略是錯的**。技術上做得到（O-11：之後每次拒絕都帶同一份，不會跑任何東西），但那是 runner 寫錯；Engine 不用斷言或去重擋（使用者否定過「之後的呼叫斷言」與去重，`-logging-no-failure-dedup`）。D3 (b) 讓 runner 在型別上必須面對它；D5 讓「沒讀回傳值」成為編譯錯誤。**D5 擋不到「讀了、沒退出」**——那一段靠第 5.3 節的表與 `WithFailureLog` | **默默忽略是錯的**。`[[nodiscard]]`；D5 之後沒寫的忽略是編譯錯誤；明寫的忽略（`(void)` 加理由）算一種處理，只在 API 的文件說「可以不理」時用 | 可以不看（它不改流程） |
| **誰記** | 斷言處理器（日誌接手時走 `WriteFatalRecord`，category `assert`） | **只有一處：runner 結束的地方**——`WithFailureLog`（包住 runner）或 `main`（用預設 runner、或自己寫 runner 而沒包時）讀 `Product::Failure()` 寫一次（`WriteFailure`）。收到 E1 的各個呼叫點只負責退出、不記。Kernel、編排層不記 | **只有**處理它的那一方，而且可以選擇不記（例：讀檔失敗、遊戲顯示對話框就夠了） | 日誌 Module（① `Forward`、② `TakeKernelDiagnostics`）；產生方不另外記 |
| **記一次記在哪** | 處理器那一次 | runner 結束處那一次。一個 Product 只結束一次，所以由結構保證只有一筆，不靠各呼叫點守規矩（runner 可能有好幾個呼叫點：迴圈、讀檔、工具的插入） | 處理的那一層 | World 記的那一筆，被日誌取走一次 |
| **日誌級別** | `Fatal` | `Fatal`（category `world`） | 處理者定：做不成而且影響使用者 → `Error`；有替代路徑 → `Warning` 或不記 | `Warning`（或 `Info`，照描述器） |

**E2 第 4 點（P7）**：產生方已經照設計記了診斷紀錄的 E2（K3 的三個 API），處理的一方**只決定流程、不再記**；API 的文件寫明「這個錯誤已記成某某診斷」。理由：紀錄與計數一處（F-13，已核准）；處理一次的「記」那一半已經由 World 的紀錄做了。這種 E2 不算「同時是兩級」（RC-EL1 的文字照這點寫）。

**不是 `std::exception` 的例外**（測試的 `AssertionFailure`、第三方的型別）：照規則 6 不接、不吞。穿過編排層時，編排層只做收尾（D4）再原樣往外；離開 runner 時，`WithFailureLog` 的 `catch (...)`（日誌批的程式）在 `Failure()` 有值時寫一筆並回 `Failure`、沒有時再丟——那是 runner 的入口，照 Legacy「Exception at the platform boundary」的同一條理由（入口的 `catch (...)` 要記下並轉成明確的結果、不吞）。建議日誌批保留它（交界，第 11.2 節）。

### 5.3 runner 拿到什麼、怎麼做（查表）

runner 從入口（`RunGameInit`、`RunFrame`、`RunSchedule`、`ReplaceWorld`、`InsertSystems`）拿到下面幾種結果。D3 選 (b) 時，E1 與 E2 在型別上就分開（`RunGameInit`、`RunFrame`、`RunSchedule` 只會回 E1 的型別）。

| 拿到 | 判法 | 做什麼 |
|---|---|---|
| `Ok` | — | 繼續 |
| E1（(b)：`ProductEnded`；(a)：`error.failure` 有值） | 型別（或 `failure`）| 不再呼叫入口，回 `RunnerExit{Failure, …}`。**不在這裡記**：`WithFailureLog` 或 `main` 在 runner 結束時讀 `Failure()` 記一次。要告訴人原因（例：讀檔被存檔版本拒絕）時讀 `cause` |
| E2（`OrchestrationError`，沒有 `failure`） | 型別 | 這是 runner 自己發的請求失敗了（讀檔、動態插入、owner 名被占用）：照 `code` 與 `reporterCode` 決定（顯示、重試、放棄），要記就記一次。World 名不存在不在這一列（修訂 2 起是 E0；名字來自資料時先 `HasWorld`）|
| 經 `Product::World(name)` 直接 `Run`／`Trigger` 回 `Err(KernelError)` 或丟例外 | 直接用 Kernel＝繞過編排層（O-1） | 同 E1 那一列：不再呼叫入口、結束 runner。runner 結束處讀 `Failure()`：D8 (b) 之下有值，回 `Err` 的路帶 Kernel 的錯誤、例外的路不帶 |
| 例外離開入口 | D4 (b) 之下只剩不是 `std::exception` 的例外 | 不接（規則 6）；runner 的入口（`WithFailureLog`）照第 5.2 節最後一段 |
| `bad_alloc` 之後 | D4：`WorldFailure` 在冒險的那一步之前組好、`Break` 是指標的指定 | E1 的路不配置（只有附加 `what()` 會試，失敗就留預設文字），照 E1 那一列；`main` 在 `Product::Build` 時仍可能收到 `std::bad_alloc`（第 17.2 節第 6 點） |
| runner 自己丟例外 | O8 | 同「例外離開入口」 |
| 斷言 | E0 | 不會回到 runner（`abort`） |

`main` 拿到的：`Product::Build` 的 `ProductStartupReport`（E2：印出、結束或退回；修訂 2 起 `Build` 裡丟出的 `std::exception` 也在這份報告裡，只有不是 `std::exception` 的照規則 6 原樣往外）；日誌 `Install` 的 `LoggingError`（E2：沒有檔案也能跑，或結束）；`Product::Run` 的 `RunnerExit`——沒有包 `WithFailureLog` 時，`main` 是 E1 的記錄者：讀 `Failure()` 寫一次（P8、D6）。

### 5.4 作者的 System 怎麼做

- 收到 Module 的 E2（`Result`）或 K3 的 E2：在 System 裡處理、回 Ok。要記，用日誌的 ④（作者自己的 System 可以用，日誌宣告第 13 節第 5 題已核准）記一次；K3 的不記（第 5.2 節 E2 第 4 點）。
- 收到 Kernel API 的 E1 碼（K2：World 已經要結束）：包一層上下文、回 `Err`。不記、不吞（CCR-2）。
- 自己判斷「不能繼續」：回 `Err`（升級，第 4.1 節）。這是宣告 bug。
- Engine 的 Module 在 System 裡報 E3：發 `Message`（LG-1），不呼叫 `Base::Log`、不直接寫日誌。
- Module 自己的前置條件被破壞（如 `FixedStep.hpp:26-33`）：可以明寫升級成 E0（斷言），寫明理由。

### 5.5 盤點逐項的處置

| # | 級 | 現在的偏差 | 處置 |
|---|---|---|---|
| B1 | E0 | 無 | 不變 |
| B2 | E2 | 不理只是警告（P10） | D5 |
| B3 | 門面 | 編排層五處在用 | 編排層不再用（D2）；`Base::Log` 留給 World 外的程式（日誌批決定入口 ③ 怎麼寫，交界） |
| B4 | — | 無 | 不變 |
| K1、K2、K5 | E1 | Executor 記一次又回傳（P2、P11） | D1：Kernel 不記，最外層 `Run`／`Trigger` 回傳一次 |
| K3 | E2（＋診斷紀錄） | 兩條通道（P7） | 第 5.2 節 E2 第 4 點寫進三個 API 的文件；行為不變 |
| K4 | E1 | 內容比對兩處（P14） | D9 |
| K6 | E1 | — | Kernel 不變；編排層接（D4） |
| K7 | E2 | 無 | 不變 |
| K8 | E2 | 註解與 CCR-3 要呼叫者記警告 | D2：CCR-3 與註解改成「回給呼叫者，處理的一方決定記不記」 |
| K9 | E2 | 型別是 `KernelError`（P17） | target RC 的文字（第 4.2 節）；行為不變 |
| K10 | 第二條通道 | 預設印 stderr | D1 |
| K11、K12 | E3／— | 無 | 不變 |
| O1 | E2 | 無 | 不變 |
| O2a | E1 | 前面已有 K1 的 stderr | D1 之後只剩 runner 結束處那一筆；D3 (b) 的型別 |
| O2b | E1／E2 | 記 `Warning`（P1、P3）；同碼兩級（P4）；P15 | D2 拿掉記錄；D3 型別分開（`DropOldFirst` 的那一步以 `cause` 帶）；D10 |
| O2c | E2 | 記 `Warning`（P3） | D2 |
| O2d | `UnknownWorld`：E0；`OwnerTaken`：E2 | P18 | 修訂 2：`UnknownWorld` 改成斷言、碼拿掉（第 17.1 節）；`OwnerTaken` 照舊（理由在第 17.1 節）|
| O2e | E2 | 作者能放 `failure`（P16） | D3 (b) 之下 `OrchestrationError` 沒有 `failure` 欄位，型別上做不到；選 (a) 時 `Prepare` 要清掉它 |
| O3 | E1 載體 | 無 | 成為 `ProductEnded` 的內容（(b)）或照舊（(a)） |
| O4 | — | 預設 runner 不寫（P8） | D6 |
| O5 | E1／E2 | 例外往外丟（P5）；`try` 前的步驟（P12） | D4（含 P12 的修正） |
| O6 | E1 | 沒有 `failure`（P13） | D8 |
| O7 | E2 | 沒人接 | 修訂 2：`Build` 的邊界接 `std::exception`、轉成報告的一筆（第 17.2 節）|
| O8 | 規則 6 | 沒寫 | 第 5.3 節寫明；不改行為 |
| M1、M3～M6 | 各級 | 無 | 不變（M6 的舊 Module 照現行契約，回到 `gyo2` 的批照本頁） |
| M2 | E1 → E0 | 沒寫成升級 | 第 4.1 節列為現有的升級；行為不變 |
| M7 | E0 | 沒寫 | 第 4.1 節；行為不變（執行緒入口自己接是規則 6 的既有要求） |
| L1、L2、L4、L5、L6 | 各級 | 無 | 不變 |
| L3 | E1 的處理 | 宣告 LG-2 的文字還寫去重（P9） | 日誌批合併前核准時改（交界）；本頁第 5.2 節 E1 的「記在哪」就是 `WithFailureLog` 現在的寫法 |
| A1、A5、A7、A9、A10 | E2 | 無 | 照宣告 |
| A2、A4 | E0 | 無 | 照宣告 |
| A3 | 非錯誤／E0 | 無 | 照宣告（修訂 2 已改成回空＋斷言）|
| A6 | E3 | 無 | 照宣告 |
| A8 | 非錯誤（蓋過）／E2（不一致） | 蓋過不報（P6） | 判為設計的優先順序、不記；第 4 批不改（第 13 節第 8 題確認） |

**A8 的理由**（P6）：「代為」的定義就是「可以被作者蓋過」（回滾份 5.4：「代為不是特權……只代表可以被作者蓋過」）。蓋過是登記規則照設計得到的結果，不是有東西失敗或被丟掉；要報的是真的不一致（兩筆作者、兩筆代為 options 不同），宣告已經讓它成為啟動錯誤。若記成 `Warning`，每個選了伺服器權威又自己登記的產品每次啟動都有一筆不需要處理的警告——這正是「把非錯誤當警告」的同一種偏差。（同一種技術沒有找到完全同形的別家：Bevy 的 `DefaultPlugins.set(...)` 是使用者明寫的覆寫、不報，`bevy_app/src/plugin_group.rs:261`，和「不論先後自動以作者為準」不完全同形。）

---

## 6. 設計決定與選項（Contract → Architecture）

每一項寫系譜、比過的選項、決定的理由、支撐的 Feature；**決定的理由不比改動量**（AGENTS §1、§4；`target/README.md:53`「這是不是在為舊形狀找位置？是的就不採，即使它最省工」），每項另答「是不是在為舊形狀找位置」。費用只列、不當理由。

Feature（第 7.2 節）：F1 一個 World 壞了留下恰好一筆致命紀錄；F2 讀檔失敗時遊戲繼續、由遊戲決定怎麼告訴玩家；F3 工具動態插入失敗時工具繼續；F4 Match 伺服器一場對局壞了、行程開下一場；F5 日誌的致命入口。

### D1 Kernel 的錯誤處理器

**問題**：P2、P11。

| 選項 | 系譜 | 內容 | 失去的 |
|---|---|---|---|
| **(b) 拿掉處理器**（建議） | 新導出：Go「處理一次」；Rust 的 `Result` 只有回傳一條路 | `SetErrorHandler`（Kernel 與 `Product` 的）拿掉；`Run`／`Trigger` 回的 `Err` 是唯一的通道；Kernel 不寫任何東西到 stderr。除錯時停在當下仍用 `breakOnKernelError`。巢狀錯誤照舊由最外層回傳一次（DI-KS 17 的「只報一次」改成「只回傳一次」）| **「不靜默」不再由 Kernel 自己保證**：現在不論呼叫者理不理，`Fail` 都會印一次；拿掉之後，直接用 `WorldExecutor` 而丟掉回傳值的人（工具、測試、O6 的路）完全沒有聲音。補法：D5（沒讀回傳值是編譯錯誤），所以 **D1 (b) 依賴 D5 (a)**；D5 的現況命中數已量是 0（P10）。測試失去處理器這個觀察點，改看回傳值 |
| (a) 留處理器、預設什麼都不做 | 舊的形（11-11 的 Executor 處理器），改預設 | 測試照舊用它觀察 | 作者在處理器裡記、runner 又記，就又是兩筆——第二條通道還在 |
| (c) 編排層建 Executor 時裝一個不做事的處理器 | 補丁 | Kernel 單獨用時照印 | 編排層替 Kernel 關掉它的行為——退掉 |
| (d) 留預設印、runner 不寫 Kernel 那段 | 舊的形 | — | 致命紀錄缺上下文、繞過日誌——退掉 |

**決定的理由**：處理器是在「Executor 是最外層」的前提下立的（target DI 1、11-11）；編排層與 runner 疊上去之後，最外層是 runner。`Run` 已經把同一個 `Err` 回傳，處理器是第二條通道；(b) 讓「一個錯誤只有一條通道」由結構保證。使用者 11-11 立這一列的理由——「原因不是可恢復，而是記日誌」「不靜默處理也是我們的原則之一」——在 (b) 之下由「Err 一定回到呼叫者、呼叫者不能沒寫就丟掉（D5）、處理者記一次（DI 4、5）」三件合起來保證，記日誌的地方從 Kernel 移到處理者。這是推翻 11-11 時的形，交使用者（第 13 節第 5 題）。

**舊形狀？** (a) 是：它留著 11-11 的處理器、只改預設，等於為舊的「Executor 記」找位置。(b) 不是。

**費用**（只列）：共用測試台與約 49 處 Kernel 判定、編排層 2 處改看回傳值；DI-KS 2、17、18 的守衛要照 §14.4 交核准（第 9.1 節）；條文與註解（P11）；`kernel_guard.py` K6 拿掉 Executor 的豁免。

**支撐的 Feature**：F1。

### D2 編排層的五處 `Warning`

**問題**：P1、P3。

| 選項 | 系譜 | 內容 |
|---|---|---|
| **(a) 全部拿掉；警告由處理的一方記**（建議） | 處理一次（Go）；CCR-3 的「呼叫者記」往上推一層 | 編排層不呼叫 `Base::Log`；O-9 改成「把錯誤回給呼叫的人、World 照跑；要不要記警告由處理它的一方決定」；`ReplaceWorld` 的四個失敗與例外路徑不記；O-4 職責清單拿掉「編排層自己的警告」；CCR-3、DI-KS 26 的「the caller logs the warning」改成「the caller handles it once (and logs it if it decides to)」 |
| (b) 只拿掉 Product 結束的那三處（`:537` 的 E1 分支、`:656`、`:659`），O-9 與 `BuildNewFirst` 照舊記 | 只修 P1 | P3 的「記又回傳」留著 |
| (d) 拒絕與失敗改記成診斷紀錄（E3 的形），由日誌的 ② 寫出；錯誤照樣回傳 | 照 K3 的形 | 使用者的「日誌報警告」由 Engine 保證；但同一件事走兩條路（回傳＋紀錄），就是 P7 的形再多一處；而且 K3 是「World 裡照設計丟掉」，這裡是「呼叫者的請求失敗」，性質不同 |
| (c) 照舊、runner 不記 | — | 致命記成警告——退掉 |

**決定的理由**：O-11 已寫「編排層不寫日誌」；(a) 讓 O-9 與 O-11 不再互相矛盾，也讓 F2、F3 的處理者（遊戲、工具）自己決定怎麼告訴人。**使用者 11-17 的原話「日誌報警告」在 (a) 之下仍然做得到**——由處理的一方（工具、遊戲的 runner）記；變的是「誰記」，從編排層移到它的呼叫者，和 CCR-3 當初把它從 Kernel 移到呼叫者是同一個方向。(b) 留下的兩處是同一種偏差。這推翻使用者 11-17 的原話、CCR-3、DI-KS 26、O-9、O-4，交使用者（第 13 節第 2 題）。

**舊形狀？** (b) 是（為 O-9 的「編排層記」留位置）。(a) 不是。

**支撐的 Feature**：F2、F3。

### D3 runner 怎麼看出級別

**問題**：P4、P12、P13、P16；使用者要的是「參考 Java 的 Exception 建立異常級別」。

| 選項 | 系譜 | 內容 |
|---|---|---|
| **(b) E1 與 E2 分成不同的型別**（建議） | Java 的型別階層（`Error` 與 `Exception` 是不同的類別）、Rust 的 `enum`、Go 的 `errors.As`（以型別認級）與 `%w`（包住原因）| 見下面「(b) 的內容」 |
| (a) `failure` 有值＝E1，寫成 O-11 的不變式並立守衛 | Go 的 `errors.As` 用在一個 optional 欄位 | 碼照舊說「哪一步」，`failure` 說「Product 結束了沒」；P12、P13、P16 要各自修；簽名看不出 `RunFrame` 只會回 E1；作者的準備函式要靠 `Prepare` 清掉 `failure` |
| (c) 錯誤型別加一個級別欄位，或 Base 加級別列舉 | Bevy 的 `Severity` | 違反「no global error enum」；和型別或 `failure` 說同一件事，就是兩處維護同一個事實（AGENTS §9）|
| (d) 改碼：Product 結束時碼一律是 `WorldInterrupted`／`WorldMissing` | 碼＝級別 | 「哪一步失敗」從碼搬到文字，runner 要分辨讀檔失敗的原因就得讀文字（存檔版本 Module 要靠 `reporterCode` 分辨，O-5）|

**(b) 的內容**（名字是暫名，實作時照命名規範定）：

- **E1 的型別 `ProductEnded`**，三個欄位：
  - `failure`：`std::shared_ptr<const WorldFailure>`，和 `Product` 持有的是**同一個物件**（`Failure()` 回的就是它）。「之後每次拒絕都帶同一份」（O-11）不複製、不配置，也不多一處存同一個事實（D4 的 `bad_alloc` 段靠這一點）。
  - `cause`：`std::optional<OrchestrationError>`——**升級成 E1 的那個 E2**（`DropOldFirst` 某一步失敗時的碼、`reporterCode`、`detail`；Go 的 `%w`）。存檔版本 Module 的拒絕照舊靠 `cause->reporterCode` 分辨（O-5「原樣帶出」不變）；`[or-ended-drop-code]`（`CoverageTests.cpp:272`）改讀 `cause->code`。
  - `kernel`：`std::optional<Kernel::KernelError>`——中斷那個 World 的 Kernel 錯誤（有的話）。
- **E2 的型別 `OrchestrationError` 拿掉 `failure` 欄位**；只屬於 E1 的三個碼 `ScheduleFailed`、`WorldInterrupted`、`WorldMissing` 離開 `OrchestrationErrorCode`。它們說的事改由 `WorldFailure::kind`（中斷或空位）與 `entry` 說；讀這三個碼的判定（`[or-ended-no-replace]`、`[or-drop-old-missing]`、`[or-ended-gameinit]` 等）改讀 `kind`（G5）。
- **入口的簽名照它能回的級**：`RunGameInit`、`RunFrame`、`RunSchedule`（修訂 2 之後 World 名錯是斷言，只剩 E1：`Product.cpp:482-493` 的四個出口）→ `Result<void, ProductEnded>`；`ReplaceWorld`、`InsertSystems` 兩級都可能 → 回一個只有兩個選項的和型別（`std::variant<ProductEnded, OrchestrationError>` 包一層）。
- `PrepareFn` 照舊回 `OrchestrationError`——沒有 `failure` 欄位，作者造不出 E1（P16）；`DropOldFirst` 時由編排層把它包進 `ProductEnded::cause`。

**決定的理由**（照 Contract 與 Feature 比）：
1. **F4 的 runner 要一眼分出 E1／E2**：(b) 由型別與編譯器分（`std::visit` 兩個選項都要寫；`RunFrame`、`RunSchedule` 的簽名直接說「只會是 E1」）；(a) 靠 runner 的作者記得看一個 optional 欄位。
2. **Contract：E1 只能由編排層發**：(b) 之下作者的準備函式型別上做不到（P16）；(a) 要靠執行期清掉。
3. **使用者要的是 Java 式的級別**：(b) 之後 E1、E2 在程式裡有名字（`ProductEnded`、`OrchestrationError`）；(a) 之下 E0～E3 只存在文件裡。這一點要讓使用者在知道的情況下選。
4. **哪一步失敗、作者的碼不丟**：`cause` 照帶，F2 的「玩家看到讀檔被拒的原因」在 `DropOldFirst` 時也成立。
5. 使用者在去重那一次說「包裝沒錯」：(b) 不改「結構化錯誤回給 runner」，改的是 E1 與 E2 不再共用一個錯誤型別、E1 改成包住原因——這和那句話是否衝突，交使用者（第 13 節第 3 題）。

**舊形狀？** (a) 是：把級別塞進現有 `OrchestrationError` 的一個 optional 欄位，等於為舊的「一個錯誤型別包兩種級」找位置。(b) 不是。

**費用**（只列）：五個入口與錯誤型別的簽名；編排層測試讀 `error().failure`、`error().code` 的各處；日誌批 `WriteFailure` 的範例註解。

**支撐的 Feature**：F1、F2、F4。

### D4 例外離開入口

**問題**：P5、P12。

| 選項 | 系譜 | 內容 |
|---|---|---|
| **(b) 編排層的入口在邊界接 `std::exception`，轉成回傳值；`catch (...)` 只收尾再原樣丟**（建議） | Go 的 `recover` 在工作單位頂端轉回值；Bevy 在 System 邊界 `catch_unwind`；`error-handling.md` 規則 6（「A module may throw internally ... only if it catches at its public boundary and returns a Result. Boundary catches name `std::exception`」；規則 6 禁的是吞，不是收尾後再丟） | 見下面「範圍與各路回什麼」 |
| (a) 照舊往外丟、runner 接 | 現在的形 | E1 從兩條路到；runner 要 `catch (...)`；P12 照樣要另修 |
| (e) Kernel 的 `Run` 在邊界接 | Bevy 的位置 | Kernel 已定不接例外、World 標 `Interrupted` 後往外（`WorldExecutor.hpp:29-31`，spec E.5；DI-KS 17、18 的巢狀與當場變更也照它定）；Kernel 不認得產品的例外政策。編排層是 Engine 對產品程式的邊界（runner 只呼叫它的入口，O-3），在這裡接一次就涵蓋 Kernel 往上的全部路——退掉 (e) |

**範圍與各路回什麼**：

- `RunOn`：接 `std::exception`（含 `std::bad_alloc`），回 E1（`kind` `Interrupted` 或 `ResourceExhausted`，`where` 盡量帶 `what()`）。
- `ReplaceWorld`：**整個函式**都在接的範圍內（N4：不只現在的 `try`，也含丟掉舊 World 之前的 `take`、`BuildNewFirst` 的 `take`／`put`）。各段回什麼：
  - `DropOldFirst`、丟掉舊 World **之前**（`take` 迴圈，`Product.cpp:562-564`）失敗：把已移出的值放回舊 World、回 E2（什麼都沒換）；放回也失敗時舊 World 少了值、不再完整 → E1（`kind` 的值照 D10 同一個問題在實作時照命名規範定，現在的三個值都不對）。
  - `DropOldFirst`、丟掉之後（含 P12 的 `Register`、`MakeExecutor`、帶過去）：E1，`cause` 帶那一步的 E2。
  - `BuildNewFirst`：E2（舊 World 照跑；P15 見 D10）。
  - **在 `Register()` 裡丟的例外**（登記單位的 `Build`／`Finish`／`Check`、產品的 `build`）：照第 17.2 節在呼叫點接成 O-E8，所以這種例外以 `ReplaceRegistrationFailed`（報告在 `detail`）回，`BuildNewFirst` 是 E2、`DropOldFirst` 是 E1 的 `cause`（S8）。D4 新增的 E2 碼（暫名「作者程式丟了例外」，`reporterCode` 帶 `what()`）只給 `Register()` 以外的路：準備函式、`beforeSwap` 的 schedule、帶過去的搬移。
- `InsertSystems`：丟掉這一輪（既有的 `DiscardRegistrations`）、回 E2（同一個新碼）。
- `Product::Build`：第 17.2 節。
- 不是 `std::exception` 的：`catch (...)` 照現在做收尾（收 `WorldFailure`、`giveBack`、`DiscardRegistrations`），然後原樣往外丟——測試的 `AssertionFailure` 穿過時也不會留下沒有 `failure` 的空位。

**`bad_alloc` 與 P12 的源頭**（修訂 1 的「盡力」不精確，窄複查 M3）：

- 現在 E1 的路在 `catch` 裡配置：`Break` 複製 Product 名（`Product.cpp:380`）、`RunOn` 的 `catch` 指定 32 字的文字（`:430`）、`ReplaceWorld` 的例外路徑在 `catch` 裡組整份 `WorldFailure`（`:661-673`）。那裡再失敗時，`DropOldFirst` 又是「空位而沒有 `failure`」。
- 做法：**`WorldFailure` 在冒險的那一步之前組好**——`RunOn` 在跑 schedule 之前（`:419-424` 已經做一半）、`DropOldFirst` 在丟掉舊 World 之前——以 `shared_ptr` 持有，Product 名、例外時的預設文字都先放好。`catch` 裡只填列舉（`kind`），`what()` 用一個內層 `try` 盡量附加（失敗就留預設文字）；`Break` 改成指標的指定（不配置）；回傳的 `ProductEnded` 共用同一個物件（D3 (b)）。所以例外之後的 E1 路除了附加 `what()` 以外不配置。
- **P12 的源頭**：`Ended()` 把「空位而沒有 `failure`」讀成「替換進行中」（`:397-402`）。改成由結構分辨：替換進行中在 slot 上有明確的狀態；`Ended()` 碰到「空位、沒有 `failure`、也不在替換中」是內部不變式壞了（`GYO_ASSERT`，E0）。照上一點組好的 `WorldFailure`，這個狀態不會出現。

**決定的理由**：runner 拿到 E1 只剩一條路（回傳值），第 5.3 節的表不需要 `catch (...)`；而且規則 6 本來就要求 Engine API 在公開邊界把例外轉成 `Result`，(b) 是照現行規則補上編排層的缺口；P12 的缺陷在同一處、從源頭修。

**推翻的已定決定**：O-9「`build` 丟例外時先丟掉這一輪再往外傳」；O-11 守衛的例外案；日誌宣告 5.6 的情形 (2)；去重那一次「返回沒錯」的原話（例外改成回傳是不是動到「返回」）——交使用者（第 13 節第 4 題）。

**舊形狀？** (a) 是。(b) 不是。

**支撐的 Feature**：F1（例外造成的 World 壞掉也只留一筆、而且一定有 `Failure()`）。

### D5 不理 `Result` 變成編譯錯誤

**問題**：P10；D1 (b) 依賴它。

| 選項 | 系譜 | 內容 |
|---|---|---|
| **(a) GYO 的程式（engine、products、tools、tests；不含 `third_party`）把「沒用 `[[nodiscard]]` 的回傳值」設成錯誤**（建議） | Rust `#![deny(unused_must_use)]`；Java checked 的編譯期強制 | GCC／Clang `-Werror=unused-result`（GCC 的 `[[nodiscard]]` 歸在這一類；Clang 的類別層級 `[[nodiscard]]` 是 `-Wunused-result`）、MSVC `/we4834`（函式）與類別層級 `[[nodiscard]]` 的對應警告號——**實作第 1 步在三平台 L1 實測確認**（`Base::Result` 是類別層級，`Result.hpp:60`）。放在 `build/cmake/GyoBuild.cmake` 已經給 GYO 程式加 `/Zc:preprocessor` 的同一處。明寫的忽略用 `(void)` 並加註理由 |
| (b) 照舊只是警告 | 現在的形 | 「不理是錯的」只靠審查；D1 (b) 之後直接用 Kernel 而丟掉回傳值就完全沒聲音 |

**理由**：使用者「不理是錯的！！！！」——能由建置擋的就不靠約定（這是機制，明寫 `(void)` 仍然可以，不把規則釘死）。現況命中數 0（P10），代價小。**界線**：D5 只擋「沒讀回傳值」，擋不到「讀了、沒退出」（E1 的原案）；那一段靠 D3 (b) 的型別、第 5.3 節的表與 `WithFailureLog`。

**支撐的 Feature**：所有產品（E2 不被默默丟掉）；D1 (b) 的「不靜默」。

### D6 預設 runner 與 `main`

**問題**：P8。

| 選項 | 系譜 | 內容 |
|---|---|---|
| **(a) 照舊：沒包 `WithFailureLog` 時 `main` 讀 `Failure()` 寫一次；在 O-3 與第 5.3 節寫明**（建議） | Bevy `App::run` 回 `AppExit`、由 `main` 決定（`app.rs:185-195`） | 編排層不依賴日誌（LG-4：`world_logging` 依賴編排層，不是反過來） |
| (b) `Product` 銷毀時，結束過而沒人讀過 `Failure()` 就斷言 | `std::thread` 解構時還 joinable 就 terminate | 使用者否定過「之後的呼叫斷言」（`-logging-no-failure-dedup`），同一種強制——退掉 |
| (c) `RunnerExit` 帶 `WorldFailure` | — | `Failure()` 已經有，兩處同一個事實——退掉 |

**舊形狀？** 不適用（不改形）。**支撐的 Feature**：F1。

### D7 級別的條文放哪

**建議**：`docs/architecture/target/error-handling.md`（英文，Engine 錯誤處理的目標頁，已有 DI 1、2）加「Error levels」一節與 DI 3～6（第 8.2 節）；`docs/architecture/error-handling.md`（Legacy）在相關處加指向標記，正文等遷移批取代。理由：錯誤處理的 owner 頁只有一處（AGENTS §9）。主會話 2026-11-20 判：先照這個提案放，合併前由主會話轉圖書管理員確認，不擋動工。

### D8 直接跑 Kernel 弄壞 World 時，`Failure()` 看不看得到（新，P13）

| 選項 | 系譜 | 內容 |
|---|---|---|
| **(b) Kernel 在中斷的那一刻把那個錯誤留在 `WorldExecutor` 上，`Product::Failure()` 讀時檢查**（建議） | 壞掉的單位留著自己的死因、由唯一的等待者去讀：Rust `JoinHandle::join` 拿到執行緒的 panic 內容、C++ `std::future` 留著 `exception_ptr`、Go `context.Cause`。引擎裡沒有同形（Bevy、Unity 沒有；flecs 的 `ecs_log_last_error` 是 errno 形的全域碼，不同） | `Fail(…, nested=false)`（`Executor.cpp:87-98`）在回傳之前另存一份「中斷這個 World 的錯誤」，存在 **Executor**（不在 World 的儲存裡），是中斷當下收好的獨立的值。`Product::Failure()` 在讀的時候做現在 `Ended()` 做的檢查，補上 `kernelCode`、`where` 與 `ProductEnded::kernel` |
| (a) `Product` 加 `Trigger` 入口 | 新的公開介面 | 只補 `Trigger`；直接 `Run` 仍是繞過 |
| (d) 不交出能跑的 `WorldExecutor`：`Product::World(name)` 只給讀的面，加 `Trigger` 入口 | 修源頭 | 「outside the Product」這條路、`Ended()` 的掃描、(b) 的存放與讀時檢查一起消失；`engine/`、`tests/` 裡直接經它跑的只有一處（`CoverageTests.cpp:333`）。但 O-1 明寫容許繞過編排層（作者的自由，`-engine-not-rigid-for-authors`），而且公開介面變得最大、沒有名指しされた Product 的 Feature 要求它 |
| (c) 只寫文件：繞過的一方自己是處理者 | — | runner 結束處的 `WithFailureLog` 一筆都不寫（P13 照舊） |

**(b) 的細節**：
- **簽名**：`Failure()` 現在是 `const noexcept`（`Product.hpp:132`）；讀時檢查會改 `failure_`（第一次讀到外面弄壞的 World 時 Product 就此結束，`Break`），所以拿掉 `const`，`noexcept` 也拿掉（那一次要組 `WorldFailure`、可能配置）。日誌批的 `WithFailureLog` 在 `catch` 裡讀 `Failure()`，那裡可能再丟 `bad_alloc`——交界（第 11.2 節）。
- **例外的路存不到**：例外離開最外層 `Run`（`LevelGuard` 的解構子只標 `Interrupted`，`Executor.cpp:162-171`）與 `Trigger`（`:989-996`）不經 `Fail`，沒有 Kernel 錯誤。這時 `Failure()` 有值、沒有 `kernelCode` 與 `where`（例外本身已經直接到呼叫者手上）。回 `Err` 的各路（含巢狀留下的 `leftError_`、hook round 上限、`Trigger`）都經 `Fail(…, false)`，都存得到（窄複查逐條查過）。
- **不是第二條通道**：第 5.1 節的「通道」指主動把錯誤送到處理者手上的路；(b) 存的是壞掉的單位身上的死因，不送給任何人，只由唯一的記錄者（runner 結束處）讀一次。第 5.1 節、DI 4 照這個區分寫。

**決定的理由**：讓「E1 只記在 runner 結束處一處」對所有路都成立（F1）。(b) 碰到 O-11 與 D2 第 5 點「錯誤內容不依賴壞掉的 World 來組、不在事後讀壞掉的 World」——本卡讀成：那句的用意是「World 可能已經壞了，不能從它的資料組錯誤」；(b) 存的是 Kernel 在中斷**那一刻**收好、放在 Executor 上的值，不讀 World 的資料。這個讀法、以及要不要改採修源頭的 (d)，交使用者（第 13 節第 7 題）。

**舊形狀？** 不是。**支撐的 Feature**：F1、F4。

### D9 照內容比對「是不是同一個錯誤」（新，P14）

**現在有兩處**：`Executor.cpp:106-111` 的 `Listed()`（巢狀錯誤的 `leftError_` 與這次呼叫回過的錯誤）、`World.cpp:26-36` 的 `SameError`／`ContainsError`（`:508` 的 `CombineCall`：System 回的 `Err` 是不是已經含有 Kernel API 回過的那一個）。兩處都比 `code`、`rule`、`message`、`detail` 四個欄位。

照源碼造得出「兩個內容相同的不同錯誤」：`Spawn<…>::New` 在 allotter 用完時回的 `AllotterExhausted` 是固定的文字（`Params.hpp:617`，rule W-9，detail 空）。
- `Listed()`：外層 System 先撞到一次（進 `call.returned`）並丟掉，再巢狀跑一張 schedule，裡面的 System 也撞到 → `leftError_` 和那一筆四個欄位全同，`Executor.cpp:364` 判成已列，巢狀那一筆不見。
- `ContainsError`：同一個 System 一次呼叫裡撞到兩次、把第一筆包了回 `Err` → `World.cpp:508` 把第二筆判成已含。

| 選項 | 內容 |
|---|---|
| **(b) 比身分**（建議） | `KernelError` 加一個身分欄位，**Kernel 第一次經手時配**（`Detail::CallError`，`World.cpp:438-447`；`Fail`；`CombineCall` 新組的 `Siblings`）：用 Base 既有的 `IdIssuer`（Kernel 已經用它，`World.cpp:11`），行程內唯一，不會跨 World 撞號。作者自己 `Err(KernelError(...))` 而 Kernel 還沒經手的值沒有身分，**沒有身分的值和任何值都不相等**。`Wrap`、複製、`leftError_`、並列清單都帶著它（窄複查查過保得住）。不經 World 的 `KernelError`（K8、K9）沒有身分，也不被比。身分不進 `World::Summary`、任何指紋或匯出，回滾不還原它（`KernelError` 現在本來就不進任何指紋或匯出格式） |
| (a) 照舊 | 照內容比對留著，和使用者否定的去重鍵同形 |
| (c) 巢狀錯誤只走一條路（拿掉 `leftError_`） | 外層 System 吞掉 `Err` 時 World 已中斷卻沒有錯誤可報，違反 DI-KS 17「一定報」——退掉 |

**這是不是「換一個去重鍵」**：不是。兩條路是 DI-KS 17 的設計（外層 System 可能吞掉內層的 `Err`），比的是**同一個錯誤物件的兩份副本**；身分相同＝同一個物件，不是「內容相同就當成一個」。RC-EL3 的文字照這點寫（「內容比對或去重鍵」觸發，比同一個物件的身分不觸發）。

**理由**：使用者「去重是補丁」；這裡要做的是讓「是不是同一個」有正確的判法。**範圍**：Kernel，和 D1 同檔；估時另列（第 11.1 節）。交使用者決定本批做或另開（第 13 節第 9 題）。

### D10 `BuildNewFirst` 換第一個 World 時遊戲初始化在新 World 上失敗（新，P15）

`Product.cpp:625-637`：新 World 跑遊戲初始化失敗後丟掉；舊的第一個 World 還在、`Ready`；`gameInitRan_` 已設，所以舊 World 永遠不會跑遊戲初始化。O-11 的結束條件只有「World 中斷或位置空了」，這裡兩種都不是——修訂 1「照 O-1 判 E1、不是推翻」不成立（窄複查 M5）。

| 選項 | 內容 | 代價 |
|---|---|---|
| **(i) E2、`gameInitRan_` 還原**（建議） | 新 World 丟掉、舊的第一個 World 照跑；遊戲初始化視為還沒跑，下一個入口照 O-1「在第一幀之前跑完」在舊的第一個 World 上跑 | 落在第 4.1 節的降級（壞掉的是還沒交出去的新 World），不需要新的結束原因、不改 O-11。遊戲初始化在丟掉的新 World 上跑過一部分；它只影響那個 World，World 外的副作用照「`BuildNewFirst` 失敗」的一般規則由作者負責 |
| (ii) 先在**舊的**第一個 World 上跑遊戲初始化再替換（和換別的 World 時 `:638` 一樣） | 失敗就是舊 World 中斷，落在 O-11 現有的條件 | 推翻編排層骨架定的「替換第一個 World、而遊戲初始化還沒跑時，在新 World 上跑」 |
| (iii) E1，O-11 加第三種結束原因 | Product 結束 | 改 O-11 的條文、`WorldFailureKind` 加一個值、`EndedError()` 的文字；第 9.2 節 `[el-e1-*]` 的右邊要加這種情形 |

**理由**：(i) 照第 4.1 節的判準推得出（相對於 Product，壞掉的不是它持有的 World），不新增結束原因；O-1 的「任何一張回錯誤……記為已跑（Product 就此結束，O-11）」是在講跑在 Product 的第一個 World 上的情形，括號引用 O-11 當理由，這條路 O-11 不成立，所以「記為已跑」也不套用。O-1 加一句寫明這條路。交使用者（第 13 節第 10 題）；照 AGENTS §14.3 先固定失敗的測試。

---

## 7. 基盤 Architecture 的根據（AGENTS §2）

### 7.1 為什麼算基盤

錯誤的級別與「誰處理、誰記」是每個 Module、runner、產品、工具的程式都要照著寫的契約；改它時，上面每一處接錯誤的程式都要改，而且量隨產品與 Module 增加。照 AGENTS §2 的判準是基盤（「Engine の公開 Interface」「再利用可能な層の共通構造」）。

### 7.2 予見の根據 → 要求

| 根據 | 種類 | 導出的要求 |
|---|---|---|
| F1 一個 World 壞了，Product 結束，留下恰好一筆致命紀錄（使用者 D2「我們如果可以的話，也要記日誌」；`-logging-no-failure-dedup`「記日誌，然後退出」） | 1 名指しされた Feature | E1 只有一個記錄處、只有一條通道（D1、D2、D4、D8） |
| F2 讀檔：`BuildNewFirst` 失敗時遊戲繼續、玩家看到原因（O-8；存檔版本 Module「被拒是 `Result`」，`-event-delivery-rule`；讀檔批） | 1 | 這次請求的 E2 不被編排層記；`reporterCode` 照帶（D2、D3） |
| F3 工具與編輯器：動態插入失敗時工具繼續（O-9；使用者 11-17「不退出，但是System無法插入」；KeepTool、地圖編輯器） | 1 | E2 回呼叫者、由它記（D2） |
| F4 Match 伺服器：一場對局壞了，行程結束那個 Product、開下一場（O-3「回主選單（另建一個 Product）」） | 1 | runner 在型別上分得出 E1／E2（D3） |
| F5 日誌的致命入口 ⑤⑥（日誌批，已核准） | 1 | E0、E1 各一個寫出的地方（第 5.2 節） |
| Java／Go／Rust 的分級；Bevy、flecs、Unity 的處理器與日誌（第 3 節） | 2 成熟した参考実装 | 「處理一次」、工作單位的邊界、`#[must_use]` 可設成錯誤、型別上分級 |

---

## 8. Architecture Delta 與 Design Invariants

### 8.1 七項（AGENTS §3）

1. **要它的目的**：使用者明示的分級任務（`v8-arch-gyo2-error-levels`）；Feature F1～F5。
2. **現在的問題**：第 2.2 節 P1～P18。
3. **變的邊界**：Kernel 的公開介面（拿掉 `SetErrorHandler`；D8 加「中斷的錯誤」的讀取；D9 `KernelError` 的身分）；編排層的公開介面（D3 (b) 入口的簽名、E1 型別 `ProductEnded`、`OrchestrationError` 拿掉 `failure`、三個只屬於 E1 的碼搬走；拿掉 `Product::SetErrorHandler`；`Failure()` 拿掉 `const noexcept`；入口與 `Product::Build` 不再丟作者的 `std::exception`；新增一個 E2 碼與 O-E8；拿掉 `UnknownWorld`、World 名錯改斷言）；錯誤處理的契約（級別一節）；建置的警告設定（D5）。
4. **影響的 Module／Product／Tool**：`engine/kernel`、`engine/orchestration`、`build/cmake`（D5）、`tests/common/kernel`（共用測試台＋9 個檔）、`tests/common/orchestration`、`plans/kernel/scripts/mutations.json`、`orchestration_mutations.json`、`kernel_guard.py`；`docs/cross-component-rules.md`（CCR-2、CCR-3）；日誌分支（`WriteFailure` 的範例、5.6、LG-1 ③、LG-2，交界）；第 4 批（不改它的決定；照本頁分級）。產品還沒有回到 `gyo2`，不受影響。
5. **依賴方向**：不變。不加任何依賴邊（編排層照舊不依賴日誌）。
6. **Ownership**：「記錄 Kernel 錯誤」從 Kernel 的 Executor 移到處理它的一方；「記錄編排層與 Kernel 動態插入的警告」從編排層移到它的呼叫者；E1 的記錄固定在 runner 結束處。這是責任的移動，所以列為 Delta。
7. **為什麼不能更小**：只修 P1 留下 P2（Kernel 再印一次）、P3（記又回傳）、P5（兩條路）、P12、P13（沒有 `failure` 的 E1）；每一處都會讓 runner 的作者照樣不知道該不該記、記幾次。使用者要的是分級，不是修兩處（`-error-levels`「不不不……開一個任務卡來做這個事」）。

### 8.2 要改的 Design Invariants 與登記的規則（都要使用者同意；最終文字在合併前核准定稿）

| 在哪 | 現在 | 改成 |
|---|---|---|
| `target/error-handling.md` DI 1 | 「returned as `Result<void, KernelError>`, logged once by the Executor, and never silent」 | 「returned once by the outermost `Run` (or `Trigger`); the kernel never logs; not silent because the `Err` always reaches the caller, ignoring it unread is a compile error in GYO code (D5), and its handler logs it once (DI 4, 5)」 |
| `target/error-handling.md` DI 2 | 「the Executor's handler is the only place a kernel error is logged」 | 「`Base::Result` stays a pure value; the kernel has no error handler and no log call」 |
| `target/error-handling.md` Replacement Criteria | kernel 的可恢復路徑要改用模組的錯誤型別 | 「kernel E2 paths may use `KernelError` as the type; the API's documentation states the level」（P17）|
| 新 DI 3 | — | **級別**：E0～E3 與非錯誤的定義、相對於單位、判準順序、升降只有第 4.1 節表列的幾條 |
| 新 DI 4 | — | **處理一次**：產生與往上傳的一方不記；處理的一方至多記一次；一個錯誤只有一條通道；同一個錯誤以身分認 |
| 新 DI 5 | — | **E1 的記錄處**：runner 結束處（`WithFailureLog` 或 `main`）讀 `Failure()` 記一次；收到 E1 的呼叫點只退出；Kernel、編排層、Module 不記 |
| 新 DI 6 | — | **E3 是紀錄、不是錯誤值**：以診斷紀錄或 `Message` 表示，不改流程；產生方已記診斷的 E2，處理者不再記 |
| Kernel DI-KS 2（`target/kernel.zh-Hant.md`） | 「Executor 記錄一次並 Interrupted」 | 「最外層 `Run` 回傳一次並 Interrupted，Kernel 不寫日誌」 |
| Kernel DI-KS 17（`:93`） | 「錯誤處理只由最外層呼叫一次」 | 「錯誤只由最外層 `Run` 回傳一次」 |
| Kernel DI-KS 18（`:96`） | 「錯誤處理只呼叫一次」 | 「錯誤只回傳一次」 |
| Kernel DI-KS 26（`:120`） | 「記警告是呼叫的人的事，共用規則頁」 | 「回給呼叫的人、由處理它的一方決定記不記」（D2） |
| `target/system-contract.zh-Hant.md:19`；`target/v2/system-contract-page.zh-Hant.md:337`、`:422`、`:435`；`target/v2/rebuild-vocabulary.zh-Hant.md:125` | Executor 是最外層、記錄一次 | 照 DI 1、4 改寫（v2 兩頁是設計段的紀錄，只加標記不改正文——由圖書管理員判） |
| `docs/cross-component-rules.md` CCR-2 | 「reported once」 | 「returned once」 |
| `docs/cross-component-rules.md` CCR-3 | 「the caller logs the warning; the kernel only returns the error」 | 「the caller handles it once (logs it if it decides to); neither the kernel nor the orchestration layer logs」 |
| 編排層 O-1 | 「任何一張回錯誤：……記為已跑（Product 就此結束，O-11）」 | D10 (i)：加一句「替換第一個 World 時在新 World 上跑的遊戲初始化失敗，不算跑過、Product 不結束（E2）」；選 (iii) 時改 O-11（見下） |
| 編排層 O-2 | 「出錯的登記單位不用、其餘照做」 | 補丟過例外之後的規則（第 17.2 節第 3 點）；`notChecked` 的意思擴充 |
| 編排層 O-3 | runner 決定關程式或回主選單 | 補：「收到 E1 時不再呼叫入口、結束；E1 的記錄在 runner 結束處（DI 5）；沒有包 `WithFailureLog` 時 `main` 是記錄者」 |
| 編排層 O-4 | 職責清單含「編排層自己的警告」 | 拿掉這一項（D2）|
| 編排層 O-9 | 「記一次 `Warning`、把錯誤回給呼叫的人……`build` 丟例外時先丟掉這一輪再往外傳」 | 「把錯誤回給呼叫的人、World 照跑；`build` 丟 `std::exception` 時丟掉這一輪、回錯誤」（D2、D4）|
| 編排層 O-11 | 「任何一個 World 中斷（Kernel 回 Err 或例外離開它的 Run）……回 `WorldInterrupted` 或 `WorldMissing`……錯誤回給 runner」 | 改：回 E1 的型別 `ProductEnded`（中斷或空位由 `WorldFailure::kind` 說、升級的原因在 `cause`；`WorldInterrupted`、`WorldMissing`、`ScheduleFailed` 三個碼搬走，D3）；例外（`std::exception`）離開 Run 時也以回傳值交給 runner（D4）；直接跑 Kernel 弄壞時 `Failure()` 帶 Kernel 的錯誤、`Failure()` 讀時檢查（D8）；D10 選 (iii) 時加第三種結束原因 |
| 編排層 O-5 | 「準備函式的錯誤（O-E7）在建好的 World 上才會出現，另成一份」 | 補：`Build` 裡丟出的 `std::exception` 是報告的一筆（新的 O-E 碼，第 17.2 節） |
| 編排層 O-8（`UnknownWorld` 的各案）、`Product.hpp` 的入口說明 | World 名不存在回 `UnknownWorld` | World 名不存在是斷言；名字來自資料時先 `HasWorld`（第 17.1 節）|
| `docs/architecture/error-handling.md`（Legacy） | Executor 記一次 | 加指向 target 的標記，正文不改（D7） |
| 註解（P11 列的各處） | Executor 記一次、呼叫者記警告 | 照上面改 |
| 日誌 LG-1 ③、LG-2、5.6（日誌批的頁，未合併） | ③ 寫「編排層五處在用」；LG-2「同一份只寫一次」；5.6 三種接法 | **不在本批改**；交界（第 11.2 節）|

### 8.3 Replacement Criteria（這一套級別的）

- **RC-EL1**：出現一種錯誤，照第 4.1 節的判準分不進任何一級，或對同一個單位同時是兩級（「跨邊界時重判」與「產生方已記診斷的 E2」不算）——停下重看級別。
- **RC-EL2**：某個 Feature 需要 E1 之後繼續跑那個 Product（例：需要自癒）——表示 O-11 或級別錯了，交使用者，不在功能裡繞過。
- **RC-EL3**：為了不重複，某處需要去重、「已寫過」旗標、內容比對或去重鍵、或第二條通道——表示處理一次被破壞，找出哪一層多處理了一次。比同一個錯誤物件的身分（D9）不觸發；現有的 `Listed()` 與 `SameError` 照內容比對，由 D9 處理，不處理時它們就是觸發這一條的現有程式。
- **RC-EL4**：E2 需要處理者之外的人記（例：Module 要求每次失敗一定進日誌）——重看第 5.1 節。
- **RC-EL5**：第 4.1 節的升降表要加第五條——停下交使用者。

---

## 9. 測試、守衛、突變

### 9.1 判定要變的守衛（AGENTS §14.4）

**中途關卡**：下面 G1、G2、G6、G8 是 DI（DI-KS 17、18、O-8、O-11）的守衛，**不論有沒有緩的面都要 1～5 交使用者核准**。第 3 步動手前，照各組逐條（判定的位置、守的性質、緩的面、締的面、性質還在哪守）列表交使用者，核准後才改；本宣告先列組與性質。

| 組 | 守衛（現在列得出的） | 守的性質 | 緩的面 | 締的面 | 性質之後在哪守 |
|---|---|---|---|---|---|
| G1 Kernel 處理器的觀察 | 共用測試台 `WorldTestSupport.hpp:121` 的 `rig->recorded`（約 49 處、9 檔：`ImmediateChangeTests`、`NestedRunTests`、`ExecutorTests`、`ObserverTests`、`HookFlushTests`、`SkeletonTests`、`AccessCheckTests`、`WorldTests`、`RegistrationRoundTests`）；J-54「the handler ran N times」、J-56 `[ns-err-once]`、`[ks-s5-throw-reported-once]`；`HookFlushTests.cpp:423`、`:444`；`RegistrationRoundTests.cpp:152`；`ExecutorTests.cpp:742` | Err 只報一次、報的是對的那個 | 處理器不存在：「處理器被叫 N 次」不再能失敗 | — | 新案：`Run`／`Trigger` 只回一次 `Err`、內容對；外層 System 吞掉內層 `Err` 時 `Run` 仍回 `Err`；並列清單裡同一個錯誤只出現一次（D9 之後以身分）；Kernel 不寫 stderr（`[el-silent-*]`） |
| G2 編排層處理器 | `[or-gi-error-once]`（`GameInitTests.cpp:151`）、`[or-replace-handler-kept]`（`ReplaceTests.cpp:111`） | 遊戲初始化的錯誤報一次；替換後沿用處理器 | 處理器不存在 | — | 前者改成回傳值只一次；後者守的性質整個拿掉（處理器不存在） |
| G3 看 `Warning` 次數 | `[or-beforeswap-warn]`（`CoverageTests.cpp:476`）、`[or-d3-throw-warns]`（`DefectTests.cpp:38`）、`[or-insert-warn-once]`（`InsertTests.cpp:52`）、`[or-insert-build-warn]`（`:68`）、`[or-replace-check-warn]`（`ReplaceTests.cpp:91`）、`[or-restore-fail-warn]`（`RestoreFlowTests.cpp:112`）、`[or-insert-warning-text]` | 失敗被告知 | 以前「沒記 `Warning`」是失敗，之後通過 | 以前「記了 `Warning`」通過，之後失敗 | 「錯誤回給呼叫者、型別與碼對、`kernel`／報告對」照舊由同一組案守；`DefectTests.cpp:25-39`「D3: an exception from extra logs one warning」是當時認定的缺陷回歸（檔頭 AGENTS 14.3），D2 把那次修正反過來——理由單獨寫：當時的缺陷是「例外穿過時沒有任何告知」，D4 之後改成「回傳錯誤」告知 |
| G4 例外往外丟 | `[or-run-throw]`（`CoverageTests.cpp:363`）、`[or-ended-throw]`（`:371-391`）、`InsertTests.cpp:81-83`、`ReplaceTests.cpp:194-196`、`DefectTests.cpp:32-34` | 例外弄壞的 World 結束 Product；替換與插入在例外時收尾 | 以前「例外沒丟出來」是失敗 | 以前「例外丟出來、沒回錯誤」通過，之後失敗 | `failure` 的內容與收尾（`giveBack`、`DiscardRegistrations`）照舊檢查；`AssertionFailure` 照舊穿過 |
| G5 E1 的讀法（D3 (b)） | 讀 `error().failure` 的各處；讀 `WorldInterrupted`／`WorldMissing`／`ScheduleFailed` 碼的 `[or-ended-no-replace]`（`CoverageTests.cpp:259`）、`[or-drop-old-missing]`（`RestoreFlowTests.cpp:133`）、`[or-ended-gameinit]`（`CoverageTests.cpp:336`）等；讀「哪一步」碼的 `[or-ended-drop-code]`（`:272`） | E1 帶 `WorldFailure`；中斷與空位分得出；哪一步失敗分得出 | 無：三個碼說的「中斷／空位」由 `kind` 說（同一件事、`EndedError()` 現在就是照 `kind` 選碼，`Product.cpp:384-389`）；「哪一步」由 `cause->code` 說 | 型別上分開 | 同一組案改讀 `kind`、`cause->code`；**「緩的面：無」以「舊判定失敗的入口與突變，新判定全部也失敗」驗證（§14.4），驗不過就改列緩的面交核准** |
| G6 `UnknownWorld`（修訂 2） | `[or-unknown-world-run]`、`[or-unknown-world-replace]`、`[or-unknown-world-insert]`（`CoverageTests.cpp:394-403`）；突變 `g8-runschedule-unknown-world-code`、`g8-replace-unknown-world-code`、`g8-insert-unknown-world-code`（`orchestration_mutations.json:539`、`:549`、`:559`）與 `or-ended-before-name`（`:2046`） | 名字錯不會跑任何東西、呼叫者被告知 | **有**：「名字錯就斷言」的實作以前讓三條判定失敗、之後通過 | 以前「回 `UnknownWorld`、程式照跑」通過，之後失敗 | 「不會跑任何東西」：新案加「斷言之後 `GameInitRan()` 仍是 false、World 沒有被動到」；「以回傳值告知、程式照跑」這一半**有意拿掉**，之後沒有判定守（§14.4 第 4 點照實寫；對使用者的意義：工具把面板上打的 World 名直接傳進來，以前拿到可以顯示的錯誤、之後是行程中止，除非先 `HasWorld`） |
| G7 `Build` 的例外（修訂 2） | 現在沒有守衛（O7 沒人接） | — | — | 新案 | `[el-build-throw-*]` |
| G8 外面弄壞的 World（D8） | `[or-ended-outside-fields]`（`CoverageTests.cpp:339-341`，要求 `!kernelCode.has_value()`） | 「outside the Product」的 `WorldFailure` 的欄位 | 以前「帶了 `kernelCode`」是失敗，之後通過 | 以前「沒帶 `kernelCode`」通過，之後失敗（回 `Err` 的路） | 改寫成：回 `Err` 的路帶 `kernelCode` 與 `where`；例外的路不帶（`[el-d8-direct-throw]`） |

### 9.2 新的守衛（行為測試，和機制同時寫）

標籤一律帶路徑，讓突變的預期失敗認得出是哪一條（§14.2）。

- **`[el-e1-<路徑>]`**：逐個入口、逐條失敗路徑（`RunGameInit`、`RunFrame`、`RunSchedule` 的 Kernel `Err` 與例外；`ReplaceWorld` 兩種先後 × 各步驟 × 回錯誤／例外，**含 P12 的 `try` 前步驟與丟掉之前的 `take`**；`InsertSystems` 的失敗；`OwnerTaken`；O6 直接跑 Kernel），判定：**回 E1 的型別 ⟺ 獨立觀測到 Product 結束**（**任一** World 是 `Interrupted` 或某個位置 `!HasWorld`，**而且**下一次 `RunFrame` 回 E1）。右邊不讀 `failure_`。D10 選 (i) 時那條路是 E2、右邊照常；選 (iii) 時右邊加那一種結束原因。
- **`[el-p12-dropold-throw-before-try]`**、**`[el-p15-buildnew-first-gameinit]`**：缺陷的回歸測試，先 Red 再修（§14.3）；後者的期待照 D10 的選項。
- **`[el-p12-ended-invariant]`**：「空位、沒有 `failure`、不在替換中」是斷言（D4 的源頭修正；用測試專用的掛點造出這個狀態，或省略並寫明靠審查——實作時判）。
- **`[el-prepare-cannot-end]`**（D3 (b)）：編譯期——`OrchestrationError` 沒有 `failure` 成員（`requires` 檢查）。
- **`[el-drop-cause-<步驟>]`**（D3 (b)）：`DropOldFirst` 各步失敗時 `ProductEnded::cause` 的碼與 `reporterCode` 對（存檔版本被拒的讀檔案例）。
- **`[el-silent-<路徑>]`**：探針執行檔跑代表性的路（Kernel 單獨的 `Run` `Err`、巢狀、`Trigger`、hook round 上限；編排層的 `RunFrame` `Err`、`DropOldFirst` 失敗、`BuildNewFirst` 失敗、插入被拒、例外、`Build` 的例外），由 `cmake -P` 的 `execute_process` 收 stderr（照 `tests/common/base/AssertAbortProbe.cmake` 的形，三平台可攜），判定：**stderr 全空**。正例：探針的「故意印一行」模式，判定要看到那一行。`Base::Log` 的預設處理器寫 `std::cerr`，所以一個探針同時量到 `Base::Log`、`fprintf`、`std::cerr` 三種寫法。
- **`[el-exception-returned-<路徑>]`**（D4 (b)）：`std::runtime_error`、`std::bad_alloc`（人為丟出）離開 System、準備函式、`InsertFn`、`beforeSwap`、帶過去的搬移 → 入口回對應級的錯誤、不丟；`AssertionFailure` 穿過、而且穿過 `DropOldFirst` 時 `Failure()` 有值（收尾照做）。
- **`[el-d8-direct-<Run|Trigger>]`**、**`[el-d8-direct-throw]`**（D8 (b)）：經 `World(name)` 直接弄壞後，回 `Err` 的路 `Failure()` 帶 `kernelCode` 與 `where`；例外的路有值、不帶。
- **`[el-d9-listed]`**、**`[el-d9-contains]`**（D9）：第 6 節 D9 的兩個情境（`AllotterExhausted`），兩個內容相同的不同錯誤各出現一次。
- **`[el-unknown-world-<入口>]`**（修訂 2）：`RunSchedule`、`ReplaceWorld`、`InsertSystems` 指不存在的 World → `GYO_CHECK_ASSERTS` 成立，**而且之後 `GameInitRan()` 仍是 false、World 沒有被動到**；Product 已結束時指不存在的 World → 回 E1、不斷言。
- **`[el-build-throw-<步驟>]`**（修訂 2）：`Configure`、`Build`、`Finish`、`Check`、準備函式各丟一次 `std::runtime_error` → `Product::Build` 回報告、含 O-E8 一筆、`what()` 在訊息裡、其餘照第 17.2 節的規則收齊；**不在作者呼叫點的例外**（測試掛點造一個 `bad_alloc`）由最外層的保底收成一筆；`AssertionFailure` 照舊穿過。
- **D5 的編譯失敗樣本**（照 `gyo2` 現有的 `tests/common/kernel/compile_fail/` 的形）：不理 `Base::Result` 的一行；預期的診斷：GCC「ignoring return value」、Clang「ignoring return value of function declared with 'nodiscard' attribute」、MSVC 實測得到的警告號（第 1 步定）——不接受「編譯失敗」本身。
- **不立新的掃描守衛**（AGENTS §3「ガードを立てる時期」）：K6 拿掉 Executor 豁免是收緊既有掃描的範圍，照 §14.4（締的面）事後報告；「編排層不 include `Log.hpp`」是新規則（O4 是允許清單、允許 `engine/base/`，`orchestration_guard.py:6-8`），**不立**，由 `[el-silent-*]` 的行為測試與審查守。

### 9.3 突變（§14.5：只加別的突變不捕的誤）

| 突變 | 只有它捕的誤 | 守的對象 | 預期的失敗 |
|---|---|---|---|
| `el-replace-fail-warn-back` | 在 `ReplaceWorld` 的 `fail` 裡寫回一筆 `Base::Log` | DI 4、O-9 | `[el-silent-dropold]` 與 `[el-silent-buildnew]` |
| `el-insert-warn-back` | `InsertSystems` 被拒時寫回 `Base::Log` | DI 4、O-9 | `[el-silent-insert-refused]` |
| `el-replace-catch-warn-back` | 例外路徑寫回 `Base::Log` | DI 4 | `[el-silent-replace-throw]` |
| `el-kernel-print-back` | `Fail` 裡寫回 `fprintf(stderr)` | DI 2 | `[el-silent-kernel-run]` |
| `el-p12-unwrapped` | `DropOldFirst` 的接法不涵蓋 `Register` 以後的步驟 | O-11、D4 | `[el-p12-dropold-throw-before-try]` |
| `el-p15-old-behaviour` | 換第一個 World 的遊戲初始化失敗時照舊（`gameInitRan_` 留著、舊 World 不跑遊戲初始化） | O-1、D10 | `[el-p15-buildnew-first-gameinit]` |
| `el-drop-cause-lost` | `DropOldFirst` 失敗時不帶 `cause` | D3 (b)、O-5 | `[el-drop-cause-prepare]` |
| `el-rethrow-std` | `RunOn` 接到 `std::exception` 後照舊丟 | O-11（D4） | `[el-exception-returned-runframe]` |
| `el-catch-all-swallow` | `catch (...)` 收尾後不再丟（吞掉 `AssertionFailure`） | 規則 6 | `[el-exception-returned-assertion-passes]` |
| `el-d8-no-stored-error` | `Fail` 不另存中斷的錯誤 | D8 | `[el-d8-direct-trigger]` |
| `el-d9-listed-content` | `Listed()` 改回比內容 | D9 | `[el-d9-listed]` |
| `el-d9-contains-content` | `SameError` 改回比內容 | D9 | `[el-d9-contains]` |
| `el-unknown-world-returns` | 某個入口的名字檢查改回回錯誤（三個入口各一個變體） | 規則 3、第 17.1 節 | `[el-unknown-world-<該入口>]` |
| `el-unknown-world-after-gameinit` | `RunSchedule` 的名字檢查挪到遊戲初始化之後 | 第 17.1 節 | `[el-unknown-world-runschedule]` 的 `GameInitRan` 判定 |
| `el-build-rethrow` | `Build` 的接法（收成一個小函式，七處呼叫點共用）接到 `std::exception` 後照舊丟 | O-5、第 17.2 節 | `[el-build-throw-configure]` |
| `el-build-no-backstop` | 拿掉 `Build` 最外層的保底 | 第 17.2 節 | `[el-build-throw-backstop]` |

`el-failure-on-buildnew`、`el-no-failure-on-dropold`（修訂 0）不立：同一個誤由既有的 `or-drop-fail-unrecorded`（find 正是 `} else if (dropOldFirst) {`）與 `[or-build-new-fail-goes-on]` 捕，照 §14.5 在既有突變的預期失敗加上 `[el-e1-*]` 的標籤。

**要退役或改寫的既有突變**（`orchestration_mutations.json`、`plans/kernel/scripts/mutations.json`）：
- 退役：`g8-runschedule-unknown-world-code`、`g8-replace-unknown-world-code`、`g8-insert-unknown-world-code`（把碼換成 `WorldMissing` 的三個；碼拿掉後由 `el-unknown-world-returns` 取代）；`or-insert-no-warning`、`or-insert-two-warnings`、`g8-insert-build-no-warning`、`g8-insert-warning-text`、`g8-beforeswap-failure-no-warning`、`or-d3-throw-no-warn`、`or-handler-not-kept`、`g8-handler-set-late`；Kernel 的 `hf-limit-handler-twice` 與預期失敗含「the handler」的各個。
- 改寫：`or-ended-before-name`（預期失敗改成 E1 與斷言的先後）、`or-run-throw-unrecorded`、`or-throw-unrecorded`、`g8-throw-no-giveback`（例外改成回傳後的預期）、`we-adv-dropped-err-beside-own-err-lost`（D9 之後的寫法）。
- 守的對象拿掉的退役（§14.5）；**是某個判定唯一的突變而退役時，先問**。

新突變清單由別的模型做一次敵對式檢查（§14.2；Fable 5.2／high，因為作者是 Opus）。突變只跑相關的（新的、find 在改動範圍、殺它的測試被改的；HANDOFF D184 的規則），PR 線合併前背景完整跑一次。

### 9.4 走查的驗證（§14.2）

第 2.2 節的「0 命中」走查用 `git grep`（不丟 stderr、看終了碼），先以已知命中驗過走得通：同一個式子 `警告|Warning` 在 `origin/gyo2` 的 `orchestration.zh-Hant.md` 命中（第 9、25、40、48 行）；建置設定那一條的同一種走法 `git grep -n "Zc:preprocessor" -- build/cmake` 命中 `GyoBuild.cmake:47-48`；`git grep -n "Warn(" engine/orchestration` 命中 6 行（定義 1、呼叫 5）；CI log 的 nodiscard 走查，正例是同一份 log 裡三平台的其他警告（P10）。實作時照同樣的方法重跑並記在 dev_log；不用 `rg`。

---

## 10. 判定（先宣告，結果出來之後不放寬）

1. 第 9.2 節的新守衛全過，三平台 L1。
2. 第 2.1 節每一項在 dev_log 有「級／處置／守衛」三欄，沒有空格。
3. **Engine 寫 0 筆**：一次 System 的 `Err`、一次 `std::exception`、一次 `DropOldFirst` 失敗、一次 `BuildNewFirst` 失敗、一次插入被拒：`[el-silent-*]` 的探針 stderr 全空、`Base::Log` 計數 0。**照第 5.3 節寫的測試用 runner**（不用日誌 Module）在 Product 結束時記錄的次數＝1。「行程裡恰好一筆 `Fatal`」要 `WithFailureLog` 才量得到，**列為日誌批的判定**（交界，第 11.2 節）。
4. 第 9.3 節的突變全殺，新突變經 Fable 5.2／high 檢查補齊；退役的逐個寫理由。
5. D5：三平台 L1 開了 `-Werror` 類設定後全綠，編譯失敗樣本照預期的診斷失敗。
6. `bad_alloc` 的回傳路徑：人為丟出時回 E1、`Failure()` 有值；E1 的路在 `catch` 之後不配置（附加 `what()` 除外），由審查與 `[el-p12-ended-invariant]` 守，不另做配置失敗的注入測試。

---

## 11. 實作的步驟與估時、和其他批的交界

### 11.1 步驟（動工前核准之後；實作者 Opus 5.5／high，審查者 Fable 5.2／high，§14.4 的檢驗者 Opus 5.5／high——上段：觸 DI 與公開介面）

| 步 | 做什麼 | 估時 |
|---|---|---|
| 1 | D5：設定開在 `GyoBuild.cmake`、編譯失敗樣本；三平台 L1 實測 MSVC 的類別層級警告號；任一平台紅就停 | 2 h |
| 2 | 條文草稿：`target/error-handling.md` 級別一節與 DI 1～6、RC；Legacy 標記；DI-KS 2、17、18、26；CCR-2、CCR-3；O-1（D10）、O-2、O-3、O-4、O-5、O-8、O-9、O-11 | 4 h |
| 3a | **中途關卡**：G1、G2、G6、G8 的逐條 1～5 表交使用者 | 1.5 h |
| 3b | Kernel：拿掉處理器（D1）、D8 的中斷錯誤；註解、K6 豁免；共用測試台與約 49 處判定改看回傳值；`[el-silent-kernel-*]` 探針 | 6 h |
| 3c | D9（核准本批做時）：兩處改比身分、`IdIssuer` 配身分、`[el-d9-*]` | 2.5 h |
| 4 | 編排層：D3 的型別與簽名（`ProductEnded` 三欄、碼的搬家）、D2 拿掉五處、D4（整個 `ReplaceWorld`、先組好的 `WorldFailure`、替換中的狀態）、D8 的 `Failure()`、D10、`Product::SetErrorHandler` 拿掉；P12、P15 先 Red；`[el-e1-*]`、`[el-exception-returned-*]`、`[el-drop-cause-*]`、`[el-silent-*]`；G3～G5、G8 | 9 h |
| 4b | 第 17 節兩件：`UnknownWorld` 改斷言（G6，三案改寫、三個突變退役與變體、`or-ended-before-name` 改寫）；`Build` 的逐處接與最外層保底、O-E8（列舉、`ToString`、報告文字）、`notChecked` 的意思擴充；`[el-build-throw-*]`、`[el-unknown-world-*]` | 4.5 h |
| 5 | 突變：新增、改寫、退役；相關的 Kernel（清單 737 個中受影響的）與編排層（227 個中受影響的）重跑；Fable 檢查 | 5 h |
| 6 | 三平台 L1、dev_log（盤點表的三欄）、HANDOFF | 2.5 h |
| | 合計（不含 3c） | 34.5 h |
| | 合計（含 3c） | 37 h |

### 11.2 和日誌批、第 4 批的交界

**先後**（主會話排的事，列出來讓使用者知道）：**本批先合**，日誌批接著合，第 4 批的 Kernel 與編排層步驟在本批之後。理由：日誌批已經暫停等本批（批次表「同日暫停，等異常分級完成」；HANDOFF 另記「使用者說繼續後」才恢復）；本批不需要日誌的程式（判定 3）；第 4 批新增的錯誤照本頁的級別寫（第 5.5 節已逐項分好，不必改它的決定）。

| 交界 | 誰改誰的檔 |
|---|---|
| 日誌宣告第 3 節現況表「編排層的警告：`Base::Log` 五處」、LG-1 的 ③ | 日誌批改：本批合併後 `gyo2` 的 Engine 裡沒有任何 `Base::Log` 的使用者，入口 ③ 留不留是日誌批的事 |
| 日誌 LG-2「同一份只寫一次」 | 日誌批合併前核准時改（HANDOFF 已記）；對齊本頁 DI 5 |
| 日誌宣告 5.6 的三種接法、`WriteFailure.hpp` 的範例註解 | D3、D4 之後：(1) 入口回 E1 的型別；(2) 例外的情形只剩非 `std::exception`；(3) 預設 runner 由 `main` 讀 `Failure()`。日誌批改；本頁 DI 5 的「記在 runner 結束處」就是 `WithFailureLog` 現在的寫法 |
| `WithFailureLog` 的 `catch (...)` | 建議日誌批**保留**（第 5.2 節最後一段）|
| 判定「行程裡恰好一筆 `Fatal`」 | 日誌批的判定（本頁判定 3）|
| `docs/architecture/target/v2/orchestration.zh-Hant.md` | **同檔**：日誌分支改了 O-11 後面那一段（df9cd068）、本批改 O-3／O-4／O-9／O-11。本批先合時日誌批 rebase；條文以本批為準、日誌批只改它那一段的例子 |
| D8 (b)（直接跑 Kernel 弄壞時 `Failure()` 看得到；`Failure()` 拿掉 `const noexcept`） | 本批合併後通知日誌卡：`WithFailureLog` 對這條路不再寫 0 筆；它在 `catch` 裡讀 `Failure()`，那裡可能再丟 `bad_alloc`，怎麼處理是日誌批的事 |
| 第 4 批：`Product.cpp`、`Setup.hpp`（共用槽） | 同檔；後合的那一批 rebase。本批在 `Product.cpp` 的改動：五處 `Warn`、`catch` 與 `try` 的範圍、入口簽名、`SetErrorHandler` |
| 第 4 批：Kernel 的 `Executor.cpp`、`WorldExecutor.hpp`、共用測試台與 9 個測試檔、`plans/kernel/scripts/mutations.json`、`kernel_guard.py`、DI-KS 2／17／18 | 同檔：第 4 批拿掉區規則時動同一批（本機 afd6e7c5 已改 `kernel_guard.py` 與 `mutations.json`）。後合的 rebase；本批先合時通知第 4 批卡 |
| 第 4 批 A5、A7 的錯誤型別（E2） | 第 4 批照本頁定；本批不碰第 4 批的檔 |
| `tests/common/scenario/` 的段號 | 本批不加情境段（判定 3 用行為測試）；日誌批的致命情境段照本頁 DI 5 寫 |
| HANDOFF、批次表 | 主會話記（本卡把決定轉過去） |

---

## 12. 範圍的限界

- 不改日誌 Module 的程式與文件（交界列給日誌批）。
- 不改第 4 批的宣告；只給級別對照（第 5.5 節）。
- 不改 Legacy 的 io、audio、threads、time 的錯誤（它們已照現行契約是 E2）。
- 不改 `GYO_ASSERT`、`Base::Result`、`Base::Error`、`Base::Log` 的介面。
- 不加級別的列舉或欄位（D3 (c) 退掉）；D3 (b) 加的是編排層的 E1 型別。
- 不碰產品的 runner（產品還沒回到 `gyo2`）；第 5.3 節是它們回來時照的表。
- 不加「照名字指」的 `Try…` 版入口（第 17.1 節：查詢用既有的 `HasWorld`）。
- 訊號（`SIGSEGV`）、記憶體已被寫壞、堆疊用盡：不分級（日誌 LG-2 的「不保證」照舊）。

---

## 13. 交使用者的

照「各家分歧、GYO 特有、推翻已定決定、DI 守衛的判定變更」四類。每題附建議與理由；名字與照源碼答得出的細節本卡已自己判（標在各節）。

1. **級別本身與兩次原話的讀法**（第 4 節、文首）：E0～E3＋非錯誤，相對於單位分；升降只有第 4.1 節表列的幾條。請確認本卡的讀法：去重那一次的原話「返回沒錯，錯誤沒錯，包裝沒錯……不是機制問題」（HANDOFF 同一列的摘要另記「O-11、編排層都不動」），被同一天開這張卡的「沒有分級，runner不知道如何處理」擴大了範圍，所以本批會動 O-1／O-2／O-3／O-4／O-5／O-8／O-9／O-11 與編排層的程式。**GYO 特有**。建議：照第 4 節。
2. **D2 拿掉編排層的五處 `Warning`，警告改由處理的一方記**。**推翻**：使用者 2026-11-17 原話「插入就拒絕，返回信息（不退出，但是System無法插入，日誌報警告）」（`v8-arch-gyo2-dynamic-insert-per-world`）——警告照樣記得到，記的人從編排層移到工具或遊戲；CCR-3、DI-KS 26、O-9、O-4（拿掉「編排層自己的警告」，清單少一項）。比過 (b) 只修致命那三處、(d) 改記診斷紀錄。建議 (a)。
3. **D3 E1 與 E2 分成不同的型別**：`ProductEnded`（共用 Product 持有的 `WorldFailure`、包住升級成 E1 的那個 E2、帶 Kernel 錯誤）／`OrchestrationError`（拿掉 `failure`；只屬於 E1 的三個碼搬走）；`RunGameInit`、`RunFrame`、`RunSchedule` 只回 E1。**碰到**去重那一次的「包裝沒錯」——(b) 保留「結構化錯誤回給 runner」，改的是 E1 不再和 E2 共用一個型別。**各家分歧**：級別放型別（Java、Rust）或靠一個欄位（Go 的 `errors.As` 用在 optional 上，D3 (a)）。照建議做完，E1、E2 在程式裡有名字；選 (a) 的話級別只存在文件裡。建議 (b)。
4. **D4 入口在邊界接 `std::exception` 轉成回傳值**（含 P12 從源頭修：先組好 `WorldFailure`、替換中有明確的狀態）。**推翻**：O-9「`build` 丟例外時往外傳」；O-11 的例外守衛（G4）；日誌宣告 5.6 的情形 (2)；也碰到「返回沒錯」。**各家分歧**：例外在工作單位邊界轉成值（Go、Bevy）或一路丟（Java）或接住繼續（Unity）。建議 (b)。
5. **D1 拿掉 Kernel 的錯誤處理器**。**推翻**：target DI 1、2、DI-KS 2、17、18 的「Executor 記一次」（2026-11-13 核准）；它的來歷是使用者 2026-11-11「這裡其實我想用 Result，原因不是可恢復，而是記日誌」「不靜默處理也是我們的原則之一」。**失去的**：Kernel 不再自己保證留下痕跡；「不靜默」改由「Err 一定回到呼叫者＋沒讀是編譯錯誤（D5）＋處理者記一次」三件保證，所以 D1 (b) 要和 D5 (a) 一起核准。**各家分歧**：Bevy 的處理器是唯一通道、GYO 的 `Err` 往上回。建議 (b)＋D5 (a)。
6. **D5 不理 `Result` 變成編譯錯誤**（現況命中 0）；以及 target 的 Replacement Criteria 改寫（Kernel 的 E2 路可以用 `KernelError`，P17）。**各家分歧**：Java 強制、Go 靠工具、Rust 可設成錯誤。建議 (a)；RC 照第 8.2 節改。
7. **D8 Kernel 在中斷那一刻把錯誤留在 Executor 上，`Failure()` 讀時看得到**（`Failure()` 拿掉 `const noexcept`；例外的路沒有 Kernel 錯誤）。要確認：(1) O-11／D2 第 5 點「不在事後讀壞掉的 World」指的是「不從壞掉的 World 的資料組錯誤」，所以讀 Executor 上中斷當下收好的值不違反；(2) 要不要改採修源頭的 (d)：不交出能跑的 `WorldExecutor`、加 `Trigger` 入口——它讓這整條路消失，但收回 O-1 容許的繞過、公開介面變得最大。建議 (b)。
8. **第 4 批「重複登記的裁決」判為非錯誤、不記**（P6、第 5.5 節 A8）。主會話提出它和 O-9 形狀相同；本卡判：蓋過是登記規則的設計結果，不一致已是啟動錯誤。建議：照判、第 4 批不改。
9. **D9 照內容比對改成比身分**（兩處：`Listed()`、`SameError`），本批做或另開。照源碼造得出兩個內容相同的不同錯誤被當成一個。建議本批做（同檔，+2.5 h）。
10. **D10 `BuildNewFirst` 換第一個 World、遊戲初始化在新 World 上失敗**：(i) E2、`gameInitRan_` 還原、下一個入口在舊的第一個 World 上跑遊戲初始化（建議；不新增結束原因，O-1 加一句寫明這條路）／(ii) 先在舊 World 上跑遊戲初始化再替換（推翻編排層骨架的做法）／(iii) E1、O-11 加第三種結束原因。修訂 1 寫「照 O-1 判 E1、不是推翻」不對，已撤回。
11. **World 名不存在改成斷言**（第 17.1 節，主會話併入）。這是 O-8 守衛的判定變更、**有緩的面**（G6），而且「各家沒有分歧」只成立一半：Bevy 在「用」的那一支兩種都給（`run_schedule` panic、`try_run_schedule` 回 `Result`），flecs 的斷言 release 會編掉。本卡只給會中止的那一支、查詢用 `HasWorld` 的理由在第 17.1 節。對使用者的意義：工具把面板上打的 World 名直接傳進來，以前拿到可以顯示的錯誤、之後是行程中止，除非先 `HasWorld`。建議：照第 17.1 節。

**主會話照 L17 判了、不交使用者的**（2026-11-20 主會話轉來）：P18 與 O7 併入本批（第 17 節；第 17.1 節的判定變更與 Bevy 的兩支照上面第 11 題交使用者，因為窄複查指出有緩的面）；D7 條文先照本宣告的提案放，合併前由主會話轉圖書管理員確認，不擋動工；批次先後照第 11.2 節（本批 → 日誌批 → 第 4 批的 Kernel 與編排層步驟）。

## 14. 和主會話、其他卡對齊的

- 使用者的決定（原話、比過的選項、理由、出處）轉主會話 `local_cb825af5-cff5-45d9-9c01-e92ad7a9e912` 記 HANDOFF。
- 動工前核准之後，通知日誌卡（`local_ba94d25b`）第 11.2 節的交界；第 4 批卡（`local_f8a8066c`）第 5.5 節的對照與同檔的範圍。
- D7 的位置經主會話轉圖書管理員。

## 15. 停止條件

- 第 11.1 節第 1 步任一平台紅（D5）。
- 第 3a 步的中途關卡（等使用者核准 G1、G2）。
- 任何一步超過估時 1.5 倍，或單一問題累計 2 h。
- 實作時發現某個錯誤分不進第 4 節的任何一級（RC-EL1），或升降表要加一條（RC-EL5）。
- 要改日誌批或第 4 批的檔才做得下去。
- 實作中發現第 2.2 節以外的既有缺陷（照 §14 冒頭：回歸或超出範圍，停下）。
- AGENTS §14 冒頭的三條。

---

## 16. 修訂 0 的敵對式檢查：處理對照

檢查者 Fable 5.2／high（唯讀；作者 Opus 5.5）。報告：主 checkout `build/evidence/gyo2-error-levels/declaration-check-r0-fable.md`（本機 macOS，git 不追蹤）。必改 10、建議改 16、小問題 11；本卡逐條照源碼確認後處理，**全部採納**（下表「處理」欄）。

| # | 摘要 | 處理 |
|---|---|---|
| M1 | `DropOldFirst` 在 `try` 前丟例外：空位、沒有 `failure` | 確認（`Product.cpp:561-617`、`:402`）。P12；D4 涵蓋丟掉舊 World 之後全部步驟；`[el-p12-*]` 先 Red；停止條件加「第 2.2 節以外的既有缺陷」|
| M2 | 經 `World(name)` 直接跑 Kernel 的路 | 確認（`Product.hpp:110-112`、`Executor.cpp:981`、`:1006`）。O6、P13；第 5.3 節加列；新選項 D8 交使用者 |
| M3 | `[el-e1-iff-failure]` 是恆真式 | 右邊改成獨立觀測；兩個突變併入既有的 |
| M4 | 「升級只有一條」「沒有降級」不符源碼 | 判準改成相對於單位、跨邊界重判；升降表列四條（含 `FixedStep.hpp:26-33`）；RC-EL5 |
| M5 | 第 13 節漏列推翻的決定 | 確認（HANDOFF 第 147 行的原話、CCR-3、DI-KS 26、去重那一列的「不動」）。第 13 節第 1～3 題、第 8.2 節補；D2 加選項 (d) |
| M6 | D1 (b) 沒寫失去的 | D1 表加「失去的」欄；D1 依賴 D5；DI 1 新文字寫明由誰保證；第 13 節第 4 題補 11-11 原話；D5 移到第 1 步 |
| M7 | DI 改動表與 §14.4 不完整；「12 處」數錯 | 第 8.2 節補 DI-KS 17、18、26、system-contract、CCR-2／3、註解；第 9.1 節分 G1～G5 並加中途關卡；計數改成實際（共用測試台與約 49 處）|
| M8 | 第 4 批讀的是修訂 1 | 確認（36f8c3f4 第 3、318、338、339、435 行）。照修訂 2 重做 A1～A10 |
| M9 | D3 的理由是改得少；E0～E3 在程式裡沒有名字 | D3 改照 Contract 與 Feature 比，建議改成 (b) 型別分開；每項加「舊形狀？」；第 13 節第 3 題明講 |
| M10 | `bad_alloc` 轉回傳值要配置；`catch (...)` 的收尾沒寫 | D4 寫明 `catch (...)` 收尾後原樣丟、`bad_alloc` 盡力而為；第 5.3 節、判定 6 照寫 |
| S1 | Kernel 的 `Listed()` 內容比對 | P14、K4；D9 |
| S2 | E1 記在 runner 結束處一處 | 採納：第 5.2 節、DI 5 |
| S3 | K3 的兩級與 RC-EL1 矛盾；漏 API；既有 RC | K3 改成 E2＋診斷紀錄、加 `SpawnWithId`、`Insert<T>`；RC-EL1 改寫；P17、K9、第 4.2 節 |
| S4 | 例句與 K2 矛盾；Kernel 單獨用；D4 沒比 Kernel 接 | 例句換掉；第 4.1 節「各層怎麼判」；D4 加 (e) 與退掉的理由 |
| S5 | `UnknownWorld` 的理由 | P18；O2d 的處置寫理由，偏差轉主會話 |
| S6 | 盤點漏項 | 加 B4、O7、O8、M2、M7、K9 |
| S7 | `BuildNewFirst` 換第一個 World 的遊戲初始化 | P15；D10 |
| S8 | 準備函式帶 `failure` | P16；D3 (b) 型別上擋；`[el-prepare-cannot-end]` |
| S9 | 守衛與突變的寫法 | `[el-silent-*]` 探針同時量三種寫法並有正例；標籤帶路徑；五處各有突變；不立新掃描；退役清單；不用 `rg` |
| S10 | §14.4 緩的面沒列全 | 第 9.1 節 G3、G4 照列；`DefectTests` 的反轉單獨寫理由 |
| S11 | 判定 3 不在範圍 | 判定 3 改成 Engine 0 筆＋測試用 runner 1 次；「恰好一筆」交日誌批 |
| S12 | D5 的量法 | 從 L1 log 量到 0（P10）；MSVC 的類別層級警告號第 1 步實測；寫明 D5 擋不到「讀了沒退出」|
| S13 | 交界漏項 | 第 11.2 節加同檔 `orchestration.zh-Hant.md`、入口 ③ 沒有生產者、Kernel 同檔、讀 World 可以、D8 通知 |
| S14 | 估時偏低 | 17 h → 28 h；突變重跑單列 |
| S15 | 選項寫法 | 各 D 加系譜、Feature、「舊形狀？」；D2 加 (d) |
| S16 | 非 `std::exception` 會變 0 筆 | 第 5.2 節最後一段：建議日誌批保留 `WithFailureLog` 的 `catch (...)` |
| N1 | 行號錯 7 處 | 全改 |
| N2 | HANDOFF 版本 | 7122169d |
| N3 | `compile_fail` 的路徑 | 改成 `tests/common/kernel/compile_fail/` |
| N4 | Bevy `DefaultPlugins.set` 的類比 | A8 的理由改寫，註明不完全同形 |
| N5 | flecs `KEEP_ASSERT`；Bevy「唯一通道」限排程 | 第 3.2 節改 |
| N6 | D7 的位置 | 轉主會話、圖書管理員 |
| N7 | `:656` 在 Product 已結束時仍寫「old World goes on」 | P1 補 |
| N8 | `WorldTimeModule.cpp:129` 只是簽名 | M1 改寫 |
| N9 | D4 的碼與 `what()` | D4 寫明 |
| N10 | 「資源耗盡」直接寫成 E1 | 第 4.1 節改成看邊界 |
| N11 | 誰先合不必當成一題 | 移到「轉主會話的」 |

---

## 17. 修訂 2：主會話併入的兩件（2026-11-20；修訂 3 照窄複查改）

主會話照 L17 判（原話：「併進本批，照你的分級統一（照別家同類 API 判是 E0 還是 E2），寫進宣告、不另交使用者，除非各家分歧」「和 D4 同一套處理」）。窄複查指出 17.1 是 O-8 守衛的判定變更、有緩的面，而且別家只有一半一致，所以修訂 3 把 17.1 列成第 13 節第 11 題；17.2 照主會話的判斷不交使用者。

### 17.1 照名字指一個東西：名字不存在是 E0

**問題**：P18。同一個入口 `RunSchedule`，World 名錯回 `UnknownWorld`（E2）、schedule 名錯是斷言（E0）；`Product::World(name)` 名字錯也是斷言。

**別家**（同一種 API：照名字或標籤取用一個東西）：

| | 「用」：照名字跑或取用 | 「查」：問有沒有 |
|---|---|---|
| Bevy | `World::run_schedule` 名字不存在 **panic**（`bevy_ecs/src/world/mod.rs:3995-3998`，文件「# Panics If the requested schedule does not exist」）；**同時給** `try_run_schedule` 回 `Result`（`:3980-3985`）；`App::sub_app` 不存在 panic（`bevy_app/src/app.rs:1204-1209`） | `get_sub_app` 回 `Option`（`app.rs:1225`） |
| flecs | `ecs_run` 對不是 System 的實體是 `ecs_assert(…, ECS_INVALID_PARAMETER)`（abort；`src/addons/system/system.c:195-204`）——**release 會編掉**（第 3.2 節） | `ecs_lookup` 回 0（`include/flecs.h:3742`） |
| Unity Entities | （沒有照名字跑的同形 API） | `World.GetExistingSystem<T>()` 回空的 handle（`Unity.Entities/World.cs:909-910`） |

可比的只有 Bevy 與 flecs 的 debug 建置：兩家都有「用的時候名字不存在就中止」的一支，也都有「查」；**Bevy 另外給一支會回錯誤的「用」**。「只給會中止的那一支」是 GYO 的選擇，不是各家共同的形（第 13 節第 11 題）。

**建議**：
- `RunSchedule`、`ReplaceWorld`、`InsertSystems` 的 World 名不存在改成 `GYO_ASSERT`（E0，規則 3 API 誤用），和 schedule 名、`World(name)` 一致；`OrchestrationErrorCode::UnknownWorld` 拿掉。
- 名字來自資料（工具的面板、設定檔）時，呼叫者先用既有的 `HasWorld(name)`（「查」的那一支，`bool`＝非錯誤）。修訂 1 曾以「World 名常來自工具或資料（規則 2 的寬契約），所以是 E2」為理由；回應：寬契約要的是「資料錯了能被告知、程式不停」，`HasWorld` 已經給了這條路，只是改成先查再用。不另加 `Try…` 版的入口（Bevy 的 `try_run_schedule`）：多一支同義的入口，就要維持兩支的行為一致，沒有 Feature 要它。
- `HasWorld` 回的是「有這個名字而且位置不空」（`Product.cpp:511-514`），名字檢查看的是「有沒有這個名字」（`:485-486`）。兩者只在位置空的時候不同，那時 Product 已經結束、`Ended()` 先回 E1，所以「先 `HasWorld` 再呼叫」不會誤觸斷言；斷言照「沒有這個名字」寫，不寫成 `HasWorld` 的反面。
- 檢查的先後不變：先 `Ended()`（Product 已結束時回 E1，不斷言），再檢查名字，名字檢查在遊戲初始化之前（`[el-unknown-world-*]` 守「斷言之後沒有跑任何東西」）。
- **規則的範圍**：「照名字指」的規則只管 **World 外的呼叫**（編排層的入口、World 外呼叫 Kernel）。System 裡 `ExclusiveWorld::RunSchedule` 指不存在的 schedule 回 `UnknownSchedule`、World 中斷（X-12，`Executor.cpp:488-491`）是 E1——System 裡的程式錯照 DI-KS 2 以 `Err` 表示，不是這條規則的例外。
- `OwnerTaken` 照舊 E2。理由（修訂 3 換掉）：照同一種方法看別家，「占用一個已被占用的名字」Bevy 是中止＋查詢（`add_plugins` 重複時 panic，`bevy_app/src/app.rs:525-533`；`is_plugin_added`，`:570`），Unity 是丟例外（`World.cs:897`）——照 17.1 的判法會落在 E0；但 GYO **沒有**「這個 owner 名被占了沒」的查詢，回傳值是呼叫者唯一知道的方法，所以留 E2。補一個查詢、一併改 E0 沒有 Feature 要求，不在本批做。

**系譜**：現行規則 3（API misuse is a Programmer Error）與規則 1 第 3 步（沒有值用 `optional`）；Bevy 與 flecs 的「用／查」兩支。**舊形狀？** 不是（`UnknownWorld` 是舊形，拿掉）。**支撐的 Feature**：F4（runner 的 E2 只剩它自己發的請求真的失敗的情形，查表不混入程式錯）。

### 17.2 `Product::Build` 裡丟出的例外

**問題**：O7。`Product::Build`（`Product.cpp:80-245`）沒有任何 `catch`；登記單位的 `Configure`、`Build`、`Finish`、`Check` 與產品自己的 `build`、準備函式丟例外時直接到 `main`；不是作者程式的地方也會丟 `bad_alloc`（`:82` `new Product()`、`:98-117`、`:139-178`、`:211`、`:219`／`:243` 組報告、`:230` `MakeExecutor`）。

**決定**（和 D4 同一套；兩層）：
1. **逐處接**：在呼叫作者程式的每一處（步驟 2 的 `Configure`、步驟 3～4 每個 World 的 `Build`／`Finish`／`Check` 與產品的 `build`、步驟 6 的準備函式）接 `std::exception`，記成報告的一筆：新的 O-E 碼（暫名 O-E8「作者程式丟了例外」，名字照命名規範定），`subjects` 帶登記單位與 World 名，訊息帶 `what()`。接法收成一個小函式、各處共用。級別 E2（還沒有 Product）。
2. **最外層的保底**：`Build` 的最外層也接 `std::exception`；`errors`、`unitErrors`、`results` 宣告在 `try` 之外，接到時回「到那一刻為止收到的全部＋這一筆」——不丟掉已收齊的錯誤（修訂 2 退掉這一層的理由不成立，窄複查 M7）。這一層收的是不在作者呼叫點的例外（多半是 `bad_alloc`）。
3. **丟過例外之後各步怎麼走**（O-2「出錯的登記單位不用、其餘照做」原本管的是第 1 步、作者程式還沒跑的錯誤；這裡補成明確的規則）：
   - 丟過例外的登記單位，**之後不再呼叫**（它的實例跨 World 共用，`Product.cpp:265-270`、`:287-293`，狀態不明）。
   - 用到它的 World，以及用到「依賴它的登記單位」的 World，**從那一步起不再登記**，標 `notChecked`（不跑 `Finish`、`Check` 與清單檢查）。這也避開第 4 批 A3「Module 在 `Build` 找不到依賴：斷言」——依賴它的單位根本不會被呼叫。
   - 沒用到它的 World 照做。
   - `Configure` 丟例外時，共用的遊戲初始化清單可能只插了一半（`:182-187`）：那次 `Build` 不做遊戲初始化清單的 O-E5 檢查，報告寫明「因為 O-E8 而沒檢查」。其餘單位的 `Configure` 照做（各有自己的 `ProductSetup`，`:185`）。
   - `notChecked` 的意思從「Kernel 失敗了」（`OrchestrationError.hpp:67-69`）擴充成「Kernel 失敗了，或用到的登記單位丟過例外」；`kernel` 是空的而 `notChecked` 是 true 的組合，報告的文字照寫。O-2、O-5 的條文照改（第 8.2 節）。
   - 半輪的 Registry：`Registrar` 在任何出路都會關上，`~Registry()` 沒有斷言（`Registry.cpp:713`），`Register()` 失敗時 Registry 是區域變數、直接銷毀（`Product.cpp:252`、`:284`）——可行；反正有錯誤就不建任何 World。
4. **替換時**：`Register()` 是啟動與替換共用（`Product.cpp:211`、`:573`），第 1 點的接法寫在它裡面，所以替換時登記單位丟的例外以 O-E8 進報告、再以 `ReplaceRegistrationFailed` 回（D4 的「範圍與各路回什麼」）。
5. 不是 `std::exception` 的：不接、原樣往外（規則 6；測試的 `AssertionFailure`）。還沒有 Product、沒有要收尾的狀態（`Build` 回之前什麼都不交出去）。
6. `bad_alloc`：記那一筆時再失敗就從 `catch` 裡往外（`main` 可能收到 `std::bad_alloc`，第 5.3 節照寫）。

**系譜**：規則 6（公開邊界接 `std::exception` 轉成 `Result`）；D4；O-2、O-5 的收齊。**比過的**：照舊讓例外到 `main`（規則 6 的缺口）／只接最外層一次（丟掉「其餘照做」：一個單位丟例外，其他單位的錯誤就收不到）／只逐處接（不在作者呼叫點的例外照樣到 `main`）——退掉，兩層並用。**舊形狀？** 不是。**支撐的 Feature**：F3（工具載入的外掛在 `Configure` 丟例外時，工具拿到報告、決定怎麼顯示，而不是整個程式被例外帶走）。

**推翻的已定決定**：無；O-2「其餘照做」、O-5「一次收齊」照用，補明確的規則與一個碼，`notChecked` 的意思擴充（第 8.2 節）。

---

## 18. 修訂 2 的窄複查：處理對照

檢查者 Fable 5.2／high（唯讀；作者 Opus 5.5）；範圍照使用者的指定：D3 (b)、D8～D10、第 17 節兩件，以及它們牽動的各節。報告：主 checkout `build/evidence/gyo2-error-levels/declaration-check-r2-narrow-fable.md`（本機 macOS，git 不追蹤）。必改 7、建議改 12、小問題 8；本卡照源碼確認（`Product.cpp:376-392` 的 `Break` 會複製、`World.cpp:24-37` 的 `SameError`、`Product.hpp:132` 的 `const noexcept`、Kernel 已用 `IdIssuer`）後**全部採納**。

| # | 摘要 | 處理 |
|---|---|---|
| M1 | 修訂 2 之後 `RunSchedule` 只回 E1 | D3 (b) 把它移到只回 E1 的一組；`[el-e1-*]` 的路徑清單拿掉 `UnknownWorld`；第 5.3 節照改 |
| M2 | `ProductEnded` 帶什麼沒定；哪一步與 `reporterCode` 會丟 | D3 (b) 定三欄：共用的 `failure`、`cause`（升級的那個 E2）、`kernel`；三個只屬於 E1 的碼搬走、改讀 `kind`；G5 列明並以 §14.4 的方法驗「緩的面：無」；O-11 的條文改動進第 8.2 節；`[el-drop-cause-*]` |
| M3 | D4 的 `bad_alloc` 段自相矛盾；`Break` 會配置；P12 的形還在 | D4 改成「`WorldFailure` 在冒險之前組好、`shared_ptr` 持有、`Break` 是指標指定、回傳共用同一個物件」；P12 從源頭修：替換中有明確的狀態、`Ended()` 碰到不可能的狀態是斷言；判定 6、第 5.3 節照改 |
| M4 | D9 漏 `SameError`；身分給不到作者造的錯誤；沒寫情境；RC-EL3 自己觸發 | D9 兩處都改；身分在 Kernel 第一次經手時用 `IdIssuer` 配、沒有身分的值不相等；兩個 `AllotterExhausted` 情境寫進 D9 與 `[el-d9-*]`；突變各一；RC-EL3 改字並說明為什麼比身分不是換去重鍵 |
| M5 | D10 判 E1 時被記成壞掉的 World 是健康的；要改 O-11 | D10 列三個選項，建議 (i) E2＋`gameInitRan_` 還原（落在降級、不改 O-11）；修訂 1 的「不是推翻」撤回；第 13 節第 10 題 |
| M6 | G6 有緩的面；「各家沒有分歧」只成立一半 | G6 照實列緩的面與第 4 點；第 17.1 節的別家表與結論照實寫（Bevy 兩支都給、flecs release 編掉）；第 13 節第 11 題交使用者 |
| M7 | 只逐處接，`Build` 仍會丟；退掉最外層的理由不成立 | 第 17.2 節改成兩層並用（逐處接＋最外層保底，保底回已收齊的全部＋這一筆）；`[el-build-throw-backstop]` 與突變 |
| S1 | `Failure()` 的簽名 | D8 寫明拿掉 `const noexcept`；第 8.1、11.2 節照寫 |
| S2 | 例外的路存不到 | D8 寫明例外的路 `Failure()` 有值、不帶 Kernel 錯誤；`[el-d8-direct-throw]` |
| S3 | D8 與「唯一通道」對不上；系譜；修源頭的選項 | 第 5.1 節定義「通道」與「死因」；系譜換成 `JoinHandle::join`／`std::future`／`context.Cause`、寫明引擎沒有同形；存在 Executor；加選項 (d) 並列進第 13 節第 7 題 |
| S4 | 新規則和 X-12 不一致 | 規則的範圍收成 World 外的呼叫；X-12 寫明照 DI-KS 2 |
| S5 | `OwnerTaken` 的理由 | 第 17.1 節照同一方法查 Bevy、Unity；理由換成「GYO 沒有 owner 名的查詢」 |
| S6 | `[el-unknown-world-*]` 只判有斷言 | 加「`GameInitRan()` 仍是 false、World 沒動」；突變 `el-unknown-world-after-gameinit` |
| S7 | 丟例外之後各步怎麼走 | 第 17.2 節第 3 點寫成明確的規則；`notChecked` 的意思擴充進 O-2、O-5 |
| S8 | `Register()` 共用，替換時回哪個碼 | D4 與第 17.2 節第 4 點寫明：`Register()` 裡的例外以 O-E8 → `ReplaceRegistrationFailed` 回；D4 的新碼只給 `Register()` 以外 |
| S9 | `[or-ended-outside-fields]` 沒進表；`el-build-rethrow` 只蓋一處 | 第 9.1 節加 G8；接法收成一個小函式，一個突變蓋七處 |
| S10 | 第 13 節的寫法 | D3、D4 拆成兩題；「O-11、編排層都不動」標明是摘要；D10 的定性改正 |
| S11 | 估時合計對不上、偏低 | 第 11.1 節重算：34.5 h（含 D9 37 h）；第 2 步的條文清單補 O-1、O-2、O-5、O-8 |
| S12 | `[el-e1-*]` 的「該 World」 | 改成「任一 World」 |
| N1 | 三個突變的說明與 id | 第 9.3 節寫 id 與「把碼換成 `WorldMissing`」 |
| N2 | 拿 `SetErrorHandler` 當對照會過期 | 第 17.1 節拿掉 |
| N3 | 第 8.1 節第 3 點沒跟修訂 2 | 照補 |
| N4 | D4 的範圍漏了丟掉之前的 `take` 與 `BuildNewFirst` 的 `take`／`put` | D4 寫明整個函式都在範圍內、丟掉之前失敗時放回並回 E2 |
| N5 | 身分跨 World 撞號、進不進指紋 | D9 用 `IdIssuer`（行程內唯一）、寫明不進 `Summary`、指紋、匯出、回滾 |
| N6 | HANDOFF 的版本 | 第 0 節照寫 4541e71d |
| N7 | 別家表的結論只靠 Bevy | 第 17.1 節照實寫「可比的只有 Bevy 與 flecs 的 debug 建置」 |
| N8 | `HasWorld` 與名字檢查的差別 | 第 17.1 節寫明，斷言不寫成 `HasWorld` 的反面 |
