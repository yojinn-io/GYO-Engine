# 事前宣告：異常分級（`v8-arch-gyo2-error-levels`）

修訂 0（2026-11-20）。Owner：架構（GYO2 任務卡「異常分級」，架構師的子會話）。分支 `claude/gyo2-error-levels-170217`，從 `gyo2` 8ec005f2 開。

狀態：**等動工前核准**。這一回合只寫宣告，不寫實作。

起因：日誌卡發現 `WriteFailure` 的去重鍵會漏寫；使用者否定「以 Product 為單位去重」：「首先去重不是解決問題的方法，是補丁。第二，核心是不應該產生重複。」「我們把error級別的異常，當成了普通警告來處理，就是這個偏差。」「runner對不可修復的錯誤，應該就是記日誌，然後退出。」（HANDOFF `v8-arch-gyo2-logging-no-failure-dedup`）。開這張卡的原話：「參考Java 的Exception 建立 異常級別……核心不是退出還是重試。而是沒有分級，現在下面的runner不知道如何處理。……1）整合我們已經有的異常。2）分類。3）給異常處理方案。」「或者參考Go的也可以。」（HANDOFF `v8-arch-gyo2-error-levels`）

本卡加的詞（不是使用者的話，名字在合併前核准定稿）：**E0～E3** 四個級別的代號；「處理一次」；「升級」。

---

## 0. 估時與讀的版本

**估時**（AGENTS §14 冒頭、時間盒）：宣告與盤點 4 h；敵對式檢查與修訂 1.5 h；實作 17 h（第 11 節逐步）。超過 1.5 倍（宣告 8.25 h、實作 25.5 h）或單一問題累計 2 h 就停下回報。

| 讀的東西 | 版本 |
|---|---|
| `gyo2` | 8ec005f2（#252 合併）|
| 日誌分支 `claude/gyo2-logging-10dfcd` | 8b83c745（第 4 步後半；宣告修訂 1 是 1d8327a0、動工前核准 df9cd068）|
| 第 4 批分支 `claude/gyo2-authority-rollback-bda9ba` | 1b0c6ef4（宣告修訂 2）|
| HANDOFF（分支 `claude/gyo2-time-design`） | ce75c6cf |
| 現行契約 | `docs/architecture/error-handling.md`（Legacy、R0–R6）、`docs/architecture/target/error-handling.md`（DI 1、2，2026-11-13 核准）|
| 參考 | Java SE（`Throwable`／`Error`／`Exception`／`RuntimeException`）、Go（error 值、`errors.Is`／`As`、`%w`、`panic`／`recover`）、Rust（`Result`、`panic`、`#[must_use]`、`catch_unwind`）——語言規格的公開知識；Bevy、flecs、Unity Entities／Netcode——本機源碼 `~/Code/GitHome`，偵察的結論與行號在第 3 節 |

---

## 1. 這一批做什麼（先用遊戲講）

Match 伺服器上，某個 System 有 bug，回了 `Err`。現在會發生的事（`gyo2` 8ec005f2，照源碼走一遍）：

1. Kernel 的 Executor 標 World `Interrupted`，呼叫預設處理器，**往 stderr 印一段 `kernel error:`**（`engine/kernel/src/Executor.cpp:36-40`、`:96`）。這一段不經 `Base::Log`，日誌 Module 收不到。
2. 同一個錯誤以 `Err` 回到編排層，編排層組 `WorldFailure` 回給 runner（`engine/orchestration/src/Product.cpp:441-451`）。
3. runner 照日誌卡的接法寫一筆 `Fatal`（`WriteFailure`）。

同一件事兩筆，一筆繞過日誌。若這一幀剛好在 `ReplaceWorld`（例：`DropOldFirst` 讀檔，新 World 的遊戲初始化壞了）：新 World 的 Executor 先印一段、編排層再記一筆 **`Warning`**（`Product.cpp:537`）、runner 再寫一筆 `Fatal`——三筆、三種寫法，其中致命的事被標成警告。

玩家讀檔失敗（`BuildNewFirst`，舊 World 照跑）時：編排層記一筆 `Warning`、同時把錯誤回給 runner（`Product.cpp:537`），runner（遊戲）多半還要自己記一筆「讀檔失敗」再顯示給玩家——又是兩筆。

這些都不是哪一處寫壞，而是**沒有級別**：誰產生、誰處理、誰記、記成什麼級別，沒有一處寫下來，所以每一層都各自「處理一部分」。這一批做三件事：

1. **整合**：把 `gyo2` 與兩條進行中的線上所有錯誤的表示與回報路徑列成一張表（第 2 節）。
2. **分類**：照「壞到哪一層」分成四級（第 4 節），對照 Java、Go、Rust、Bevy、flecs、Unity（第 3 節）。
3. **處理方案**：每一級寫清楚產生方回什麼、呼叫者必須做什麼、能不能忽略、誰記日誌、只記一次記在哪、什麼級別（第 5 節）；runner 拿到任何錯誤都查得到怎麼做（第 5.3 節）。

做完之後，上面三個例子：bug → 只有 runner 寫的那一筆 `Fatal`；`DropOldFirst` 讀檔壞掉 → 同樣一筆；`BuildNewFirst` 讀檔失敗 → 編排層不記，錯誤回給 runner，遊戲自己決定記不記、記成什麼。

---

## 2. 盤點（第一件事）

### 2.1 總表

欄位：誰產生／帶什麼／回到誰／**現在**誰記日誌、記幾次、什麼級別／呼叫者現在被期待怎麼做／分到第 4 節的哪一級。「記」指進 stderr 或 `Base::Log`；行號是 `gyo2` 8ec005f2，日誌分支與第 4 批另標。

**Base**

| # | 項目 | 誰產生 | 帶什麼 | 回到誰 | 現在誰記、幾次、級別 | 呼叫者被期待 | 級 |
|---|---|---|---|---|---|---|---|
| B1 | `GYO_ASSERT`／`GYO_UNREACHABLE`（`engine/base/include/engine/base/Assert.hpp:93-105`） | 任何程式 | 條件文字、`source_location` | 不回；斷言處理器，處理器回來就 `abort` | 預設處理器 stderr 一次（`Assert.hpp:63-69`）；日誌分支接手時改走 `WriteFatalRecord`（category `assert`，`Fatal`）再交回前一個處理器 | 不處理，修程式 | E0 |
| B2 | `Base::Result<T, E>`／`Base::Err`、`Base::Error<Code>`、`CodedError`（`Result.hpp`、`Error.hpp`） | 各 Module 的 API | 模組自己的碼、`message`、`detail` | 直接呼叫者 | 不記（`Result` 是純值，target DI 2） | 必須處理（`[[nodiscard]]`；但沒有 `-Werror`，見 P10） | E2 |
| B3 | `Base::Log`（`Log.hpp:63-66`） | 用它的程式：**現在只有編排層五處** | level（Debug／Info／Warning／Error）、category、文字 | 不回；處理器 | 預設 stderr 一行；日誌分支 `Install` 接手 | 不影響流程 | （記錄的門面，不是錯誤） |
| B4 | `std::optional`、診斷資料、遊戲規則的結果（`error-handling.md`「These are not errors」） | — | — | — | — | — | 非錯誤 |

**Kernel**（`engine/kernel`）

| # | 項目 | 誰產生 | 帶什麼 | 回到誰 | 現在誰記、幾次、級別 | 呼叫者被期待 | 級 |
|---|---|---|---|---|---|---|---|
| K1 | System 回 `Err(KernelError)` | 作者的 System（或存取驗證的跳板） | `KernelErrorCode`、規則、World 名、上下文鏈、並列清單（`KernelError.hpp:113-140`） | Executor → `Run` 的呼叫者 | **Executor 的處理器 stderr 一次**（`Executor.cpp:36-40`、`:96`），不經 `Base::Log`、無級別；然後 `Run` 回同一個 `Err` | `Run` 的呼叫者不再跑這個 World（再跑是斷言，`World.cpp:1159`）| E1 |
| K2 | Kernel API 回給 System 的 `Err`：大多數碼（`AccessViolation`、`StaleHandle`、`ScheduleRunning`、`NoAllotter`……） | Kernel | 同上 | System；World 已標 `Interrupted`（`Executor.cpp:89-97` 的 `Fail`）或在 System 回來時結束 | 同 K1（最外層 `Run` 記一次） | System 包一層上下文往外傳（DI-KS 2） | E1 |
| K3 | `AllottedIdAlive`（`ExclusiveWorld::Spawn`，`Params.hpp:949-969`） | Kernel | 同上 | **只回 System**；不算 Run 的錯誤、World 不中斷；**同時**記成 `RejectedSpawn` 診斷紀錄 | 不記；診斷紀錄由 runner 取走（日誌 ②） | System 自己決定（換 id、放棄） | E2（＋E3 紀錄，見 P7） |
| K4 | 巢狀 run 裡的錯誤（`ExclusiveWorld::RunSchedule`） | Kernel | 同上 | 留給最外層 `Run`（`Executor.cpp:91-94`；`WorldExecutor.hpp:33-41`） | 最外層記一次 | 同 K1 | E1 |
| K5 | `Trigger` 從 World 外觸發的 `Err`（`WorldExecutor.hpp:104-118`） | observer、hook | 同上 | `Trigger` 的呼叫者 | 處理器一次 | 同 K1 | E1 |
| K6 | System 或 hook 丟例外 | 作者程式 | 例外物件 | **不接**：World 標 `Interrupted`，例外往外丟（`WorldExecutor.hpp:29-31`） | 不記 | 呼叫者接或讓它往上 | E1（到了編排層） |
| K7 | `StartupReport`／`StartupError`／`StartupCode`（`KernelError.hpp:142-207`） | `Registry::BuildPlans`、`Finish` | 全部啟動錯誤，一次收齊 | 編排層（收進 `ProductStartupReport`） | 不記 | 不建 World（O-5） | E2 |
| K8 | `DynamicInsertRefused`（`Registry::Register`） | Kernel | 同 K1 | 編排層 `InsertSystems` | 不記；**註解寫「the caller logs the warning」**（`KernelError.hpp:74-75`） | 呼叫者記一筆警告 | E2 |
| K9 | `WorldExecutor::SetErrorHandler`（`WorldExecutor.hpp:119-121`；`Product::SetErrorHandler` 轉交，`Product.hpp:114-115`） | — | 收到 K1／K2／K4／K5 的 `KernelError` | — | 預設＝K1 的 stderr | 測試用來計數（12 處） | （第二條通道，見 P2） |
| K10 | 診斷紀錄 `RejectedSpawn`、`DroppedMessages`、作者的 `DiagnosticsWriter<R>`（`World.hpp:101-150`） | World | 被丟掉的那一筆與計數 | runner 在兩次 `Run` 之間取走（游標宣告） | 不記；日誌分支 `TakeKernelDiagnostics` 寫 | 不影響流程 | E3 |
| K11 | `breakOnKernelError` | Kernel | — | 除錯器 | 不記 | 不影響流程（會返回的掛鉤） | （除錯用，不是通道） |

**編排層**（`engine/orchestration`）

| # | 項目 | 誰產生 | 帶什麼 | 回到誰 | 現在誰記、幾次、級別 | 呼叫者被期待 | 級 |
|---|---|---|---|---|---|---|---|
| O1 | `ProductStartupReport`（O-E1～O-E7、各 World 的 `StartupReport`、登記單位的字串碼；`OrchestrationError.hpp:44-91`） | `Product::Build` | 全部啟動錯誤 | `main` | 不記 | `main` 決定（印出、結束） | E2 |
| O2a | `OrchestrationError`：`ScheduleFailed`、`WorldInterrupted`、`WorldMissing` | 入口 | 碼＋`failure`（一定有）＋`kernel` | runner | 不記（O-11）；但 K1 的 stderr 已印過 | runner 決定關程式或回主選單（O-3） | E1 |
| O2b | `ReplaceRegistrationFailed`、`ReplaceCarryOverFailed`、`ReplacePrepareFailed`、`ReplaceScheduleFailed` | `ReplaceWorld` | 碼＋`reporterCode`＋（`DropOldFirst` 或遊戲初始化弄壞第一個 World 時）`failure` | runner | **`Warning` 一次**（`Product.cpp:537`），**不論是不是結束 Product**；`ReplaceScheduleFailed` 另有新 World 處理器的 stderr | 有 `failure` 時同 O2a，沒有時舊 World 照跑 | **同一個碼兩種級：**有 `failure` 是 E1，沒有是 E2（P4） |
| O2c | `InsertRefused`、`InsertBuildFailed` | `InsertSystems` | 碼＋`kernel` 或報告 | 呼叫者 | **`Warning` 一次**（`Product.cpp:720`、`:729`；O-9） | 呼叫者處理；World 照跑 | E2 |
| O2d | `UnknownWorld`、`OwnerTaken` | 入口 | 碼 | 呼叫者 | 不記 | 呼叫者處理 | E2 |
| O2e | `PrepareFailed`（準備函式回的碼，作者的碼在 `reporterCode`） | 作者的準備函式 | 作者的碼 | 編排層 → `O-E7`（啟動）或 `ReplacePrepareFailed`（替換） | 見 O1／O2b | — | E2（`DropOldFirst` 時 E1） |
| O3 | `WorldFailure`／`WorldFailureKind`（`Interrupted`、`ReplacementFailed`、`ResourceExhausted`；`OrchestrationError.hpp:94-150`） | 編排層＋Kernel，壞掉當下收 | Product、World、怎麼壞、入口、schedule、`syncEpoch`、Kernel 碼與文字、最後成功的 schedule | 掛在每次拒絕上；`Product::Failure()` | 編排層不記（O-11） | runner 處理 | E1 的載體 |
| O4 | `RunnerExit`（`Setup.hpp:77-85`） | runner；預設 runner 是 `Product::Run`（`Product.cpp:495-503`） | `Success`／`Failure`＋文字 | `main` | 不記 | `main` 讀；要寫失敗得另外讀 `Failure()` | （runner 的結果，不是錯誤） |
| O5 | 例外穿過入口 | 作者程式（System、hook、準備函式、`InsertFn`） | 例外物件 | **往外丟**：`RunOn` 先收 `WorldFailure` 再丟（`Product.cpp:426-438`）；`ReplaceWorld` 在 `BuildNewFirst` 時 `Warning` 後丟（`:656`）、`DropOldFirst` 時 `Warning`＋收 `WorldFailure` 後丟（`:659-677`）；`InsertSystems` 丟掉這一輪再丟（`:709-717`） | 替換時 `Warning` 一次 | runner 用 `catch (...)` 接、再讀 `Failure()`（日誌宣告 5.6 情形 (2)） | 有 `failure` 是 E1，沒有是 E2（P5） |

**Module**

| # | 項目 | 誰產生 | 帶什麼 | 回到誰 | 現在誰記 | 呼叫者被期待 | 級 |
|---|---|---|---|---|---|---|---|
| M1 | `world_time`：前置條件的 `GYO_ASSERT`（`TimeTypes.cpp:20-108`、`Sampling.cpp:10-20`、`FixedStep.hpp:27-98`）；準備函式回 `OrchestrationError`（`WorldTimeModule.cpp:129`）；固定步 System 回 `KernelError`（`FixedStep.hpp:41`） | 時間 Module | — | — | 不記 | — | E0／E2／E1 |
| M2 | `world_time`：`TimeDiscarded`（`TimeTypes.hpp:141`）以 `Message` 發 | 時間 Module | 丟掉的時間 | 讀者 | 不記；日誌分支 `TimeDiscardedLog` 描述器經 `Forward` 寫 | 不影響流程 | E3 |
| M3 | JSON：`JsonError`（`Syntax`……`OutOfMemory`；`JsonError.hpp`）；`bad_alloc` 在公開函式內接住轉成 `OutOfMemory`（`Writer.cpp:115-352`、`YyjsonDocument.cpp:63-69`） | JSON Module | 碼 | 呼叫者 | 不記 | 處理 | E2 |
| M4 | os：`OsError`（`OsError.hpp`，detail 帶原生碼） | os | 碼＋`errno=`／`win32=` | 呼叫者 | 不記 | 處理 | E2 |
| M5 | io、audio、threads（`ThreadError`；`RoleThread.cpp:60` 接 `std::system_error` 轉 `StartFailed`）、time（`TimeError`） | 各 Module | 碼 | 呼叫者 | 不記 | 處理 | E2 |

**日誌分支**（`claude/gyo2-logging-10dfcd` 8b83c745，未合併）

| # | 項目 | 誰產生 | 帶什麼 | 回到誰 | 誰記 | 呼叫者被期待 | 級 |
|---|---|---|---|---|---|---|---|
| L1 | `LoggingError`（`OpenFailed`、`StartFailed`） | `Install`、檔案輸出端 | 碼 | `main` | 不記 | 處理（沒有檔案也能跑） | E2 |
| L2 | `WriteFatalRecord`／`FatalWriteReport` | 致命路徑 | 一筆 `Fatal` | 呼叫者（回報寫成了沒有） | 它就是寫 | — | E0、E1 的最後一道 |
| L3 | `WriteFailure`／`WithFailureLog` | runner 或 `main` 呼叫 | `WorldFailure` | — | 每次呼叫寫一筆 `Fatal`（category `world`），**不去重**（8b83c745） | runner 寫一次然後退出 | E1 的處理 |
| L4 | 斷言處理器接手、`std::terminate` 處理器（只 flush） | 日誌 | — | 前一個處理器 | 斷言：一筆 `Fatal`；terminate：不寫紀錄，只 flush | — | E0 |
| L5 | `Forward`、`TakeKernelDiagnostics`、`Logging::Write` | 日誌 | — | — | 照 sink 的設定 | — | E3 的寫出（④ 也給作者寫 E2 的處理紀錄） |
| L6 | Sink 寫失敗（LG-11：計數、`Good()` 變 false） | 輸出端 | — | — | 不記 | 不影響流程 | E3 |

**第 4 批宣告**（`authority-rollback-declaration.zh-Hant.md` 修訂 2，1b0c6ef4；未動工）

| # | 項目 | 宣告怎麼寫 | 級 |
|---|---|---|---|
| A1 | `Build` 時才知道的錯誤（沒選策略、`rollback.Input<T>` 時 `T` 未登記、兩筆作者登記 options 不同）：Module 存起來、在 `Check` 用字串碼報（`AU-E…`、`RB-E…`；第 4.2 節） | 進 `ProductStartupReport` | E2 |
| A2 | 共用槽凍結後再寫：斷言（第 6 節第 1 點） | `GYO_ASSERT` | E0 |
| A3 | `Find<T>` 沒人 `Create<T>`：回錯誤、不代建（第 6 節第 2 點） | 回錯誤（型別未定） | E2（在 `Build` 階段，最後進啟動報告） |
| A4 | `World::Summary` 碰到沒有雜湊函式的型別：回錯誤並列出型別（第 3 節） | 回錯誤 | E2 |
| A5 | snapshot 來源超過深度：丟掉並記診斷（第 9 節） | 診斷紀錄 | E3 |
| A6 | 重複登記的裁決：作者的蓋過代為的、不論先後；兩筆作者或兩筆代為 options 不同是啟動錯誤（第 6 節第 3 點；回滾份 5.4；權威份 4.3、DI-A 11） | 蓋過不報；options 不同進 `Check` | 蓋過＝非錯誤（P6）；options 不同＝E2 |
| A7 | 選了 P2P 佔位策略：啟動錯誤（S3-6） | 進 `Check` | E2 |
| A8 | `Tick` 的寫者不在名單：權威的專用檢查報錯（G4） | 啟動錯誤 | E2 |

### 2.2 觀察到的問題（AGENTS §2 Valid evidence：每條附在哪、什麼、幾次）

主會話查到四處（P1～P3、P6），本卡照源碼再確認，另找到 P4、P5、P7～P11。

- **P1 致命的事被記成警告（主會話查到，已確認）**：`ReplaceWorld` 的 `fail`（`Product.cpp:536-558`）**先**寫一筆 `Warning` 再看要不要收 `WorldFailure`；`DropOldFirst` 失敗與「替換時跑的遊戲初始化弄壞第一個 World」都是 Product 結束（E1），記成 `Warning`；例外路徑 `:659` 同。違反 O-11「編排層不寫日誌」；runner 再寫 `Fatal` 就是兩筆。守衛 `[or-failure-not-logged]`（`tests/common/orchestration/CoverageTests.cpp:305-329`）只走 `RunSchedule`。日誌宣告第 3 節的現況表（`logging-declaration.zh-Hant.md` 第 89 行）把這五處當成入口 ③ 的正常用途接下來。
- **P2 Kernel 記一次、runner 又記一次（主會話查到，已確認）**：`Fail` 先呼叫處理器（預設 `fprintf(stderr)`）再回 `Err`（`Executor.cpp:89-97`）。Kernel 本身是「記一次並回傳」——Go 的「log and return」反例。Kernel 單獨用時 Executor 是最外層（2026-11-11 的前提，target DI 1「the Executor is the outermost layer and handles once」），編排層與 runner 疊上去之後它不再是最外層，前提換了、條文沒跟。stderr 那筆不經 `Base::Log`，所以 `[or-failure-not-logged]` 的 `WarningCounter` 數不到它，那個守衛在 K1 的 stderr 照印時照樣通過。`ReplaceWorld` 的新 World 也帶處理器（`Product.cpp:323-328`），`:630`、`:645` 的失敗會多印一段。
- **P3 記一次又回傳（主會話查到，形狀相同、已核准）**：O-9（動態插入被拒、建 Plan 失敗：`Warning` 一次＋回錯誤，`Product.cpp:720`、`:729`）與 `ReplaceWorld` 的 `BuildNewFirst` 失敗（`:537`）。錯誤回給了呼叫者，編排層又自己記——同一個錯誤「處理」兩次；呼叫者若也記（它該決定要不要記），就是兩筆。O-9 的條文是使用者核准的（編排層骨架），和 O-11「編排層不寫日誌」互相矛盾。
- **P4 同一個碼兩種級**：`ReplacePrepareFailed` 等四個碼在 `BuildNewFirst` 是「這次替換失敗、舊 World 照跑」，在 `DropOldFirst` 是「Product 結束」（`Product.cpp:538-556`）。現在分得出來的唯一方法是看 `failure` 有沒有值，但沒有任何條文說「這就是級別」，runner 的作者得讀 `Product.cpp` 才知道。
- **P5 致命的事從兩條路到 runner**：Kernel 回 `Err` 時以回傳值到；System 丟例外時以例外到（`Product.cpp:426-438` 收好 `WorldFailure` 再丟）。所以 runner 要 `catch (...)`（日誌宣告 5.6 情形 (2)、`WithFailureLog`），而 `error-handling.md` 規則 6 除了平台邊界之外不准 `catch (...)`；規則 6 也寫「不要用例外回報 Engine API 的失敗」，編排層的入口把例外往外丟就是用例外回報。
- **P6 第 4 批「重複登記的裁決」（主會話查到，已確認）**：作者的登記蓋過代為的，不報。宣告裡沒有記警告的地方（`警告|Warning` 在宣告與兩份子宣告都是 0 命中）；形狀的問題是「兩筆不一致時默默選一筆」。本卡判為**設計的優先順序，不是錯誤**（第 5.5 節；交使用者確認，第 13 節第 6 題）。
- **P7 回給 System 又記成診斷**：`AllottedIdAlive` 回給 System，同時記成 `RejectedSpawn`（F-13：紀錄與計數一處）。System 若也寫一筆，日誌的 ② 又寫一筆。需要一條規則（第 5.2 節 E2 第 4 點）。
- **P8 預設 runner 不寫失敗**：`Product::Run` 的預設 runner 回 `RunnerExit{Failure, 文字}`（`Product.cpp:495-503`），`main` 要寫失敗得再讀 `Failure()`；沒讀就沒有任何一筆（編排層不記）。這是 O-11 的設計（記不記是日誌的事），但「不理是錯的」沒有地方寫。
- **P9 日誌宣告的 LG-2 還寫「同一份 `WorldFailure` 只寫一次」**（`logging-declaration.zh-Hant.md` 第 8 節），實作已拿掉去重（8b83c745）；HANDOFF 寫了合併前核准時改。本卡只列交界，不改它的檔。
- **P10 `[[nodiscard]]` 只是警告**：`git grep -nE "Werror|/WX|COMPILE_WARNING_AS_ERROR|Wunused-result"` 在 CMake 與 CI 0 命中（走查先以已知命中驗過走得通，第 9.4 節）。所以「不理 `Result`」在建置上只是一行警告。
- **P11 條文與註解還寫「Executor 記一次」**：`docs/architecture/error-handling.md`（「Programmer Error detected from runtime data」）、`target/error-handling.md` DI 1、2、Kernel DI-KS 2「Executor 記錄一次」、`KernelError.hpp:74-75`、`WorldExecutor.hpp:119-121`、`Executor.cpp:1-4`、`kernel_guard.py` K6 的豁免。

---

## 3. 別家怎麼分（同一個問題：級別怎麼分、每一級誰處理、能不能被忽略）

（偵察結果補進本節；見下面各表。）

### 3.1 語言

| | 級別 | 誰處理 | 能不能忽略 | 記在哪 |
|---|---|---|---|---|
| **Java** | `Throwable` 下分 `Error`（`OutOfMemoryError`、`StackOverflowError`、`AssertionError`：「合理的程式不該接」）與 `Exception`；`Exception` 再分 checked（編譯器要求接或宣告 `throws`）與 `RuntimeException`（unchecked，多半是程式錯誤：`NullPointerException`、`IllegalArgumentException`） | checked：呼叫者；`RuntimeException`、`Error`：一路往上到執行緒頂端（`UncaughtExceptionHandler`）或框架的最外層 | checked 不能默默忽略（編譯錯誤），但能 `catch {}` 吞掉——Java 社群公認的壞習慣；`Error` 照規範不接 | 慣例「log or rethrow, not both」；頂端處理器記一次 |
| **Go** | error 值（呼叫者處理）與 `panic`（程式錯誤、不可恢復；`recover` 只在少數邊界——例如 HTTP server 每個請求的頂端——轉回 error 或結束那個單位）；error 用 `%w` 包上下文，`errors.Is`（比哨兵值）、`errors.As`（取出某型別）判斷 | error：直接呼叫者，「處理一次」：要嘛包一層往上回，要嘛在這裡處理（記、降級、重試），不兩者都做；panic：最外層 | 語言允許 `_ = f()`，但那是明寫的決定；`errcheck` 等工具抓沒寫的 | 處理的那一層記一次；panic 由 runtime 印堆疊 |
| **Rust** | `Result<T, E>`（可恢復）與 `panic!`（bug；`.unwrap()`／`.expect()` 把 `Err` 明寫升級成 panic）；`panic = "abort"` 時不能接，`unwind` 時 `catch_unwind` 只給執行緒或 FFI 邊界 | `Result`：呼叫者（`?` 往上傳）；panic：執行緒結束，`JoinHandle::join` 拿到 | `Result` 是 `#[must_use]`：不用就警告，專案常用 `#![deny(unused_must_use)]` 變成錯誤；`let _ =` 是明寫的忽略 | 處理的那一層；panic hook 印一次 |

三家共同的骨架：**(一) 程式本身不可信**（Java `Error`、Go `panic`、Rust `panic`）——不接、最外層記一次、結束；**(二) 這次請求失敗**（checked／error／`Result`）——直接呼叫者處理，往上傳時包上下文、不記，處理的地方記一次；**(三) 明寫的升級**（Java 包成 `RuntimeException`、Go `panic(err)`、Rust `.expect()`）。分歧：Java 用型別階層加編譯器強制，Go 靠慣例與工具，Rust 靠 `#[must_use]`（可設成錯誤）；Go 與 Rust 有「工作單位的邊界」（HTTP 請求、執行緒）把不可恢復的事收成一個值交給上層，Java 靠例外一路往上。

### 3.2 引擎

偵察：Opus 5.5／high，唯讀；版本 bevy 0.20.0-dev 6a911653f、flecs 09790b5f7、com.unity.entities 1.5.0、com.unity.netcode 7.2.0；報告存主 checkout `build/evidence/gyo2-error-levels/recon-reference-engines.md`（本機 macOS，git 不追蹤）。行號相對 `~/Code/GitHome/`。

| | 級別怎麼分 | 每一級誰處理 | 能不能忽略 | 記在哪、幾次 | release 一樣嗎 |
|---|---|---|---|---|---|
| **Bevy** | `BevyError` 帶 `Severity`（`Ignore < Trace < Debug < Info < Warning < Error < Panic`，`bevy_ecs/src/error/bevy_error.rs:297-314`），文件定位為「advisory metadata」，怎麼處理由處理器定（`:12-20`）；`?` 轉來的錯誤**預設 `Panic`**（`:536-545`）；引擎本身幾乎不標級（只有一處 `with_severity`） | 一個處理器：World 的 Resource `FallbackErrorHandler`，預設 `match_severity`（`error/handler.rs:116-138`）；Executor 對 System 的 panic 一律 `catch_unwind`，包成 `Panic` 級的 `BevyError` 交給**同一個**處理器（`schedule/executor/single_threaded.rs:285-295`、`multi_threaded.rs:856-868`）。System 的 `Err` 不往上回：處理器是唯一通道 | 可以：`Severity::Ignore`、`ignore` 處理器、`try_*` 命令；參數驗證的 `Skipped` 明文不算錯（`system/system.rs:377-412`）。把處理器換成 `warn` 時連 panic 也吞了 | 處理器一次；但單執行緒 Executor 上 `Panic` 級的錯誤處理器被呼叫**兩次**（第一次 `panic!` 被外層接住、再呼叫一次 `resume_unwind`），panic 改成記錄時 panic hook 與處理器各一次——錯誤本身不去重，只有 backtrace 去重（`bevy_error.rs:596-617`） | 一樣（沒有 `debug_assertions` 分支） |
| **Bevy `AppExit`** | `Success`／`Error(NonZero<u8>)`（`bevy_app/src/app.rs:1550-1557`），System 以 `Message` 寫 | runner 每次 update 後看 `should_exit()`（`schedule_runner.rs:91-106`；`run_once` `app.rs:1516-1527`） | — | — | — |
| **flecs** | 依巨集：`ecs_assert`（內部不變式，必 abort）、`ecs_check`／`ecs_throw`（參數錯，預設 abort，`FLECS_SOFT_ASSERT` 時 `goto error` 回錯誤值）、`ecs_err`＋回傳碼（可恢復）、`ecs_abort`／`ecs_always_assert`（永遠在）（`include/flecs/addons/log.h:464-550`）；約 25 個錯誤碼（`:694-761`） | 發生點自己：abort 交 `ecs_os_api.abort_`（可換）；可恢復的由呼叫者看回傳值 | 回傳值可以不看（C 不強制）；assert 不能；soft 模式之後文件說「應視為 undefined state」 | **發生點記一次**（fatal 或 err），往上傳的呼叫者只傳回傳值、不再記（`src/query/api.c:396-460`、`src/addons/system/system.c:315-319`）；System 回呼是 `void`，沒有錯誤通道（`include/flecs.h:583`、`:592`） | 不一樣：`FLECS_NDEBUG` 編掉 assert／check／throw |
| **Unity Entities** | 沒有分級，只有例外與 `[Conditional("ENABLE_UNITY_COLLECTIONS_CHECKS")]` 的安全檢查 | 最內層 `ComponentSystemGroup` 對每個子 System `try { } catch (Exception e) { Debug.LogException(e); }`，**繼續跑下一個 System**（`Unity.Entities/ComponentSystemGroup.cs:713-741`） | 例外被吞掉，World 照跑 | 最內層 group 一次 | 不一樣：檢查在 runtime build 移除，文件說「aren't any guarantees」（`Documentation~/concepts-safety.md:13`） |
| **Unity Netcode** | 日誌 5 級（`NetDebug.LogLevelType`，`Runtime/Debug/NetDebug.cs:338-361`）＋斷線原因列舉（`NetworkStreamDisconnectReason`，原因不是嚴重度） | 偵測點 `LogError` 並在連線實體加 `NetworkStreamRequestDisconnect`（`Runtime/Rpc/RpcSystem.cs:455-474`），收的 System 斷線、發事件給遊戲（`NetworkStreamReceiveSystem.cs:803-808`、`:1070-1120`） | 協定錯誤一定斷線 | 偵測點 `Error` 一次；收尾 `Debug` 一次 | 部分 `LogError` 只在 checks 下記，斷線照做 |
| **bevy_replicon／lightyear** | 沒有分級型別：致命（協定雜湊不符）→ 斷線；單則訊息錯誤 → `error!` 後丟掉、繼續（`bevy_replicon/src/client.rs:283-331`；`lightyear/crates/transport/transport/src/plugin.rs:281-286`）；內部不變式 → `panic!` | 偵測點 | 單則錯誤丟掉繼續 | 偵測點一次 | 查不到差異 |

**和 GYO 的相似點與取捨**（取能力、不取技術債）：

- **處理一次、發生點或處理點記一次**：flecs（發生點記、往上不記）、Unity（最內層 group 記一次）、Netcode（偵測點記一次）都靠「只在一個地方記」的結構避免重複，**沒有一家去重**（Bevy 只對 backtrace 去重）。取：第 5.1 節的原則。GYO 選「處理的一方記」而不是 flecs 的「發生點記」，因為 GYO 的錯誤往上回、產生方不知道上面會不會處理（flecs 的可恢復錯誤也是發生點 `ecs_err` 一次後才回值——對 GYO 就是「記又回傳」，P3）。
- **Bevy 把 panic 收進同一個處理器**：Executor 在工作單位（System）的邊界 `catch_unwind`、轉成值。取：D4 (b) 的形（在 Product 入口的邊界把 `std::exception` 轉成值）。不取：Bevy 讓處理器決定吞不吞（換成 `warn` 連 panic 都吞）——GYO 的 E1 不能被設定成繼續（O-11）。
- **Bevy 的 `Severity` 是附在錯誤上的欄位、由處理器解讀**：不取（D3 (c)）。Bevy 自己的程式幾乎不標，於是「未標＝Panic」才是實際的級別；GYO 的級別由邊界定（第 4.1 節），附欄位等於兩處維護同一件事。
- **Bevy 的處理器是唯一通道**（System 的 `Err` 不往上回）；GYO 的 `Err` 往上回，所以 Kernel 的處理器是多出來的第二條（D1）。
- **Unity 吞例外、World 照跑**：不取（違反使用者 D2「失敗了還繼續，就是吞錯誤」）。
- **flecs、Unity 的 debug／release 兩套**：不取（GYO 已定一個事件一種性質、release 一樣，`v8-arch-kernel-break-hook`）。
- **Bevy `AppExit` 與錯誤沒有自動連動**：和 GYO 一樣，runner 自己決定怎麼結束（O-3）；GYO 多一條：E1 一定讓 runner 結束那個 Product。
- **網路類（Netcode、replicon、lightyear）**：致命＝斷線帶原因、單則訊息錯誤＝丟掉繼續。網路 Module 回來時，單則訊息錯誤對應 E3（照設計丟掉、計數），斷線原因是遊戲規則的結果（非錯誤，交給遊戲），協定不一致是 E2（連線這個請求失敗）——在網路 Module 的宣告照本頁定，本批不做。

**各家分歧**（交使用者的那幾題在第 13 節標出）：未標級時的預設（Bevy panic／Unity 記了繼續／flecs debug abort、release 消失）；panic 與 `Err` 同路（Bevy）或只有例外（Unity）或沒有通道（flecs 的 System）；處理權集中（Bevy 一個處理器）或分散（flecs、Netcode）；debug／release 一致（Bevy）或兩套（flecs、Unity）。

### 3.3 GYO 特有之處

- **一個 World 是一條鏈**（使用者 D2）：World 壞了不能自癒、不能拿替換來救、Product 就結束（O-11）。三家語言都沒有「World」這一層；最接近的是 Go 的「工作單位」（一個 HTTP 請求壞了，伺服器結束那個請求、行程照跑）——GYO 的 Product 是那個單位，runner 是那個邊界。
- **致命的事以值回傳，不靠例外往上**：使用者的理由「c++沒有try catch……有可能Even機制本身已經壞了」（`-world-events-not-via-event`）、「包進一個普通的 Result 回去也沒問題」（`-logging-no-failure-dedup`）。所以 E1 的載體是值（`WorldFailure`），像 Go 的 `recover` 之後回 error，而不是 Java 的 `Error` 一路丟。
- **System 回 `Err`＝宣告這個 World 不能繼續**（`-err-is-bug-world-readable`）：執行時的狀況由 System 自己處理、回 Ok；回 `Err` 就是 bug 等級。這是 GYO 的「明寫的升級」。

---

## 4. 分級（第二件事）

### 4.1 判準：壞到哪一層

級別照「**錯誤之後還能信任到哪一層**」分，不照錯誤從哪裡來、也不照錯誤碼分。同一個 `KernelError` 回給 System 時是 E2（System 可以處理），被 System 回到 Executor 時就成了 E1——級別在**跨過邊界的那一刻**由接手的一方定，碼不變。

| 級 | 名字（暫） | 還能信任的 | 意思 | Java | Go | Rust |
|---|---|---|---|---|---|---|
| **E0** | 程式錯誤（中止） | 什麼都不能 | 前置條件或內部不變式壞了：程式本身錯 | `Error`（`AssertionError`） | `panic`（不 recover） | `panic`（abort） |
| **E1** | 致命：World 壞了、Product 結束 | 行程與其他 Product；**不能**信任壞掉的 World 與它所屬的 Product | 執行期才看得出的程式錯誤（DI-KS 2 的那一列）、例外離開 World 的 Run、資源耗盡、`DropOldFirst` 失敗留下空位 | `RuntimeException` 到了框架的最外層 | 工作單位頂端 `recover` 後回的 error | `catch_unwind` 在執行緒邊界收到的 panic |
| **E2** | 失敗：這次請求沒做成 | 所有東西，包括發出請求的 World | 外部資料或環境讓一個正確的程式失敗（現行契約的 Runtime Error）；啟動時的組裝錯誤（一次收齊的報告） | checked `Exception` | error 值 | `Result::Err` |
| **E3** | 警告：照設計丟掉或限制了某樣東西 | 所有東西，World 健康 | 訊息被修剪、生成被拒、時間被丟棄、輸出端寫失敗：照設計的上限處理掉了，流程不變 | （日誌的 `WARN`） | （`log.Printf`） | （`warn!`） |
| — | 非錯誤 | — | 沒有值（`std::optional`）、遊戲規則的結果、設計的優先順序（第 5.5 節） | — | `v, ok` | `Option` |

**分到哪一級的順序**（擴充 `error-handling.md` 規則 1，不取代它）：

1. 程式本身錯、在呼叫當下就看得出（前置條件、API 誤用）→ **E0**（`GYO_ASSERT`）。規則 1 第 1 步、規則 3 不變。
2. 程式錯、但要到執行期的資料才看得出（DI-KS 2 的那一列），或一個 World 的 Run 不能正常結束（例外、資源耗盡），或一個 World 的位置空了 → **E1**。
3. 外部資料或環境讓這次請求失敗，而發出請求的東西還健康 → **E2**（`Result`）。規則 1 第 2 步、規則 2、4、5 不變；啟動錯誤的報告也在這一級。
4. 照設計的上限丟掉或限制了某樣東西、World 健康、流程不變 → **E3**（診斷紀錄或 `Message`，不是錯誤值）。
5. 只是沒有值、遊戲規則的結果、設計的優先順序 → **非錯誤**。規則 1 第 3～5 步不變。

**升級**（只有明寫的一條）：System 把收到的 E2 包一層回 `Err`，就是宣告「這個 World 不能繼續」，變成 E1。這和 `-err-is-bug-world-readable` 一致：執行時的狀況由 System 自己處理、回 Ok；回 `Err` 是 bug 等級。**沒有降級**：E1 不會變回 E2（不准拿替換救壞掉的 World，O-11）。

### 4.2 和現行契約的對齊

| 現行（`error-handling.md`） | 本卡 |
|---|---|
| Assert＝Programmer Error | E0，不變 |
| Result＝Runtime Error | E2，不變 |
| 「Programmer Error detected from runtime data」那一列（Executor 記一次、World `Interrupted`） | E1 的來源之一；**改**：「Executor 記一次」改成「最外層 `Run` 回傳一次，Kernel 不寫日誌，處理的一方記一次」（第 6 節 D1） |
| These are not errors（optional、診斷、遊戲規則） | 非錯誤；**加** E3：照設計的上限丟掉的事，以診斷紀錄或 `Message` 表示，不是錯誤值（現行「Several problems or warnings ... diagnostics data」的延伸，不是新機制） |
| 規則 6（不用例外回報 Engine API 的失敗；邊界只接 `std::exception`） | 不變；編排層的入口照它做（第 6 節 D4） |
| 「no global error enum」 | 不變：級別不做成 Base 的列舉或錯誤型別的欄位（第 6 節 D3） |

---

## 5. 處理方案（第三件事）

### 5.1 一條原則：處理一次

照 Go 的「handle errors once」與 Java 的「log or rethrow, not both」：**一個錯誤要嘛往上傳（可以包一層上下文），要嘛在這裡處理（決定接下來怎麼做，必要時記一次），不兩者都做**。由此推出：

- **產生的一方不記**（Kernel、編排層、Module）。
- **往上傳的一方不記**，只加上下文（規則 4、5；`KernelError::Wrap`）。
- **處理的一方至多記一次**，記成它判斷的級別（第 5.4 節）。
- **一個錯誤只有一條通道到處理的一方**：不另開回呼、事件、全域處理器把同一個錯誤再送一份（第 6 節 D1 的理由）。

### 5.2 每一級的處理

| | E0 程式錯誤 | E1 致命 | E2 失敗 | E3 警告 |
|---|---|---|---|---|
| **產生方回什麼** | 不回：`GYO_ASSERT` → 斷言處理器 → `abort` | Kernel：`Run`／`Trigger` 回 `Err(KernelError)`、World `Interrupted`；編排層：入口回 `Err(OrchestrationError)`，**`failure` 有值**（`WorldFailure`） | `Result<T, 模組的錯誤>`；啟動時是一次收齊的報告（`StartupReport`、`ProductStartupReport`） | 不回錯誤：World 記診斷紀錄，或發 `Message` |
| **呼叫者必須做** | 無（修程式） | runner（或沒有 runner 時的 `main`）：**記一次，然後退出**——不再呼叫這個 Product 的任何入口；之後關程式或另建 Product（回主選單）由它決定（O-3）。Kernel 單獨用時：`Run` 的呼叫者不再跑這個 World | 往上傳（包上下文）或在這裡處理（決定、降級、改走別條路、顯示給人），二選一 | 無；日誌照常訂閱或取走 |
| **能不能忽略** | 不能（處理器回來就 `abort`） | **忽略是錯的**。技術上做得到（O-11：之後每次拒絕都帶同一份，不會跑任何東西），但那是 runner 寫錯；Engine 不用斷言或去重擋（使用者否定過「之後的呼叫斷言」與去重，`-logging-no-failure-dedup`） | **默默忽略是錯的**。`[[nodiscard]]`；明寫的忽略（`(void)` 加理由）算一種處理，只在 API 的文件說「可以不理」時用。第 6 節 D5 提議把沒寫的忽略變成編譯錯誤 | 可以不看（它不改流程） |
| **誰記** | 斷言處理器（日誌接手時走 `WriteFatalRecord`，category `assert`） | **只有** runner 或 `main`（`WriteFailure`）。Kernel、編排層不記 | **只有**處理它的那一方，而且可以選擇不記（例：讀檔失敗、遊戲顯示對話框就夠了） | 日誌 Module（① `Forward`、② `TakeKernelDiagnostics`）；產生方不另外記 |
| **記一次記在哪** | 處理器那一次 | runner 收到 `failure` 的那一刻；同一個 Product 只會收到一次，因為收到就退出 | 處理的那一層 | World 記的那一筆，被日誌取走一次 |
| **日誌級別** | `Fatal` | `Fatal`（category `world`） | 處理者定：做不成而且影響使用者 → `Error`；有替代路徑 → `Warning` 或不記 | `Warning`（或 `Info`，照描述器） |

**E2 的第 4 點（P7）**：產生方已經照設計記了診斷紀錄的 E2（現在只有 `AllottedIdAlive` → `RejectedSpawn`），處理的一方**只決定流程、不再記**；API 的文件寫明「這個錯誤已記成某某診斷」。理由：紀錄與計數一處（F-13，已核准）；處理一次的「記」那一半已經由 World 的紀錄做了。

### 5.3 runner 拿到什麼、怎麼做（查表）

runner 只會從入口（`RunGameInit`、`RunFrame`、`RunSchedule`、`ReplaceWorld`、`InsertSystems`）拿到兩種結果。

| 拿到 | 判法 | 做什麼 |
|---|---|---|
| `Ok` | — | 繼續 |
| `Err`，`error.failure` 有值 | E1 | `WriteFailure(*error.failure)`（或產品自己的致命紀錄）一次 → 回 `RunnerExit{Failure, …}`，不再呼叫入口。`WithFailureLog` 就是把這一列包好 |
| `Err`，`error.failure` 沒有值 | E2 | 這是 runner 自己發的請求失敗了（讀檔、動態插入、名字錯）：照 `error.code` 與 `reporterCode` 決定（顯示、重試、放棄），要記就記一次 |
| 例外離開入口 | 照第 6 節 D4 的選擇：選 (b) 時不會發生（`std::exception` 已轉成上兩列之一）；只剩不是 `std::exception` 的例外（測試的 `AssertionFailure`、外來型別），照規則 6 不接 | — |
| 斷言 | E0 | 不會回到 runner（`abort`） |

`main` 拿到的：`Product::Build` 的 `ProductStartupReport`（E2：印出、結束或退回）；日誌 `Install` 的 `LoggingError`（E2：沒有檔案也能跑，或結束）；`Product::Run` 的 `RunnerExit`（runner 已經處理過 E1；用預設 runner 時 `main` 自己讀 `Failure()` 並寫一次，P8——第 6 節 D6 的選項）。

### 5.4 作者的 System 怎麼做

- 收到 E2（Module 的 `Result`、`AllottedIdAlive`）：在 System 裡處理、回 Ok。要記，用日誌的 ④（作者自己的 System 可以用，日誌宣告第 13 節第 5 題已核准）記一次；`AllottedIdAlive` 不記（第 5.2 節第 4 點）。
- 收到 Kernel API 的 E1 碼（World 已經 `Interrupted`）：包一層上下文、回 `Err`。不記、不吞。
- 自己判斷「不能繼續」：回 `Err`（升級，第 4.1 節）。這是宣告 bug。
- Engine 的 Module 在 System 裡報 E3：發 `Message`（LG-1），不呼叫 `Base::Log`、不直接寫日誌。

### 5.5 盤點逐項的處置

| # | 級 | 現在的偏差 | 處置 |
|---|---|---|---|
| B1 | E0 | 無 | 不變 |
| B2 | E2 | 不理只是警告（P10） | D5 |
| B3 | 門面 | 編排層五處在用 | 編排層不再用（D2）；`Base::Log` 留給 World 外的程式 |
| K1、K2、K4、K5 | E1 | Executor 記一次又回傳（P2、P11） | D1：Kernel 不記，最外層 `Run` 回傳一次 |
| K3 | E2＋E3 | 兩條通道（P7） | 第 5.2 節第 4 點寫進 API 文件；行為不變 |
| K6 | E1 | — | Kernel 不變；編排層接（D4） |
| K7 | E2 | 無 | 不變 |
| K8 | E2 | 註解要呼叫者記警告 | 註解改成「回給呼叫者」（D2） |
| K9 | 第二條通道 | 預設印 stderr | D1 |
| K10 | E3 | 無 | 不變 |
| O1 | E2 | 無 | 不變 |
| O2a | E1 | 前面已有 K1 的 stderr | D1 之後只剩 runner 那一筆 |
| O2b | E1／E2 | 記 `Warning`（P1、P3）；同碼兩級（P4） | D2 拿掉記錄；D3 寫明「`failure` 有值＝E1」並立守衛 |
| O2c | E2 | 記 `Warning`（P3） | D2 |
| O2d、O2e | E2 | 無 | 不變 |
| O3 | E1 載體 | 無 | O-11 補「有 `failure` 就是 Product 結束、而且只在這時有」 |
| O4 | — | 預設 runner 不寫（P8） | D6 |
| O5 | E1／E2 | 例外往外丟（P5） | D4 |
| M1～M5 | 各級 | 無 | 不變（M5 的舊 Module 照現行契約，回到 `gyo2` 的批照本頁） |
| L1、L2、L4、L5、L6 | 各級 | 無 | 不變 |
| L3 | E1 的處理 | 宣告 LG-2 的文字還寫去重（P9） | 日誌批合併前核准時改（交界，第 11.2 節）；D4 選 (b) 時 `WithFailureLog` 的 `catch (...)` 那一支可以拿掉 |
| A1、A3、A4、A7、A8 | E2 | 無 | 照宣告；A3、A4 的錯誤型別在第 4 批照本頁定 |
| A2 | E0 | 無 | 照宣告 |
| A5 | E3 | 無 | 照宣告 |
| A6 | 非錯誤（蓋過）／E2（不一致） | 蓋過不報（P6） | 判為設計的優先順序、不記（第 13 節第 6 題確認）；第 4 批不改 |

**A6 的理由**（P6）：「代為」的定義就是「可以被作者蓋過」（回滾份 5.4：「代為不是特權……只代表可以被作者蓋過」）。蓋過是登記規則照設計得到的結果，不是有東西失敗或被丟掉；要報的是真的不一致（兩筆作者、兩筆代為 options 不同），宣告已經讓它成為啟動錯誤。同一種技術的別家：Bevy 的 Plugin 設定由使用者覆寫預設（`DefaultPlugins.set(...)`），不報警告。若記成 `Warning`，每個選了伺服器權威又自己登記的產品每次啟動都有一筆不需要處理的警告——這正是「把非錯誤當警告」的同一種偏差。

---

## 6. 設計決定與選項（Contract → Architecture）

每一項寫系譜、比過的選項、決定的理由、支撐的 Feature（AGENTS §4）。

### D1 Kernel 的錯誤處理器

**問題**：P2、P11。Kernel 記一次又回傳。

| 選項 | 系譜 | 內容 |
|---|---|---|
| **(b) 拿掉處理器**（建議） | 新導出：照 Go「處理一次」；Rust 的 `Result` 只有回傳一條路 | `SetErrorHandler`（Kernel 與 `Product` 的）拿掉；`Run`／`Trigger` 回的 `Err` 是唯一的通道；Kernel 不寫任何東西到 stderr。除錯時停在當下仍用 `breakOnKernelError`（會返回的掛鉤，不是通道）|
| (a) 留處理器、預設什麼都不做 | 舊的形（11-11 的 Executor 處理器），改預設 | 測試照舊用它計數；但作者在處理器裡記、runner 又記 `WorldFailure`，就又是兩筆——留著第二條通道等於留著重複的可能 |
| (c) 編排層建 Executor 時裝一個不做事的處理器 | 補丁 | Kernel 單獨用時照印；編排層替 Kernel 關掉它的行為——退掉 |
| (d) 留預設印、runner 不寫 Kernel 那段 | 舊的形 | 致命紀錄缺上下文、繞過日誌——退掉 |

**決定的理由**：處理器是在「Executor 是最外層」的前提下立的（target DI 1、11-11）；編排層與 runner 疊上去之後，最外層是 runner。`Run` 已經把同一個 `Err` 回傳，處理器是第二條通道；(b) 讓「一個錯誤只有一條通道」由結構保證，不靠「請不要在處理器裡記」的約定。Bevy 的處理器是唯一通道（System 的 `Err` 不往上回），這點和 GYO 相反；GYO 往上回，所以處理器多餘（第 3.2 節）。退掉 (a) 的理由不是測試要改（測試的費用不是決定的理由，AGENTS §4），是重複的可能還在。

**費用**（只列、不當理由）：12 處測試改成看回傳的 `Err`；Kernel DI-KS 2、target DI 1、2 的條文；`kernel_guard.py` K6 拿掉 Executor 的豁免（全 Kernel 不寫日誌）。

**支撐的 Feature**：F1（一個 World 壞了，留下恰好一筆致命紀錄）。

### D2 編排層的五處 `Warning`

**問題**：P1、P3。

| 選項 | 內容 |
|---|---|
| **(a) 全部拿掉**（建議） | 編排層不呼叫 `Base::Log`；O-9 改成「把錯誤回給呼叫的人、World 照跑」；`ReplaceWorld` 的四個失敗與例外路徑不記；O-4 職責清單拿掉「編排層自己的警告」 |
| (b) 只拿掉 Product 結束的那三處（`:537` 的 E1 分支、`:659`），O-9 與 `BuildNewFirst` 照舊記 | 只修 P1，P3 的「記又回傳」留著 |
| (c) 照舊、runner 不記 | 致命記成警告——退掉 |

**決定的理由**：(b) 留下的兩處是同一種偏差（處理兩次）；O-11 已寫「編排層不寫日誌」，(a) 讓 O-9 與 O-11 不再互相矛盾。拿掉之後職責清單少一項，不是多一項（RC-O5 不觸發），但動了使用者核准的 O-9 與 O-4 文字，交使用者（第 13 節第 2 題）。

**支撐的 Feature**：F2（讀檔失敗時遊戲繼續、由遊戲決定怎麼告訴玩家）、F3（工具動態插入失敗時工具繼續）。

### D3 runner 怎麼看出級別

**問題**：P4。現在只有 `failure` 有沒有值分得出，但沒寫成契約。

| 選項 | 系譜 | 內容 |
|---|---|---|
| **(a) `failure` 有值＝E1，寫成 O-11 的不變式並立守衛**（建議） | Go 的 `errors.As`：級別由「包著哪一種型別」定，外層的碼說的是哪一步 | 碼照舊說「哪一步失敗」（`ReplacePrepareFailed` 等），`failure` 說「Product 結束了沒」；不變式：`failure` 有值 ⟺ 這個 Product 有 World 壞了。守衛逐條走每個入口的每條失敗路徑 |
| (b) 入口的錯誤型別分成兩種（像 Rust 的 `enum { Ended(WorldFailure), Refused(OrchestrationError) }`） | Rust | 型別上分得最清楚；但五個入口的簽名都要改、`std::variant` 在 C++ 的寫法冗長，和 `Base::Result` 的單一錯誤型別不合 |
| (c) 錯誤型別加一個級別欄位，或 Base 加級別列舉 | Java 的型別階層壓成一個欄位 | 違反「no global error enum」；一個欄位和 `failure` 說同一件事，就是兩處維護同一個事實（AGENTS §9）|
| (d) 改碼：Product 結束時碼一律是 `WorldInterrupted`／`WorldMissing`，哪一步放進 `failure.where` | 碼＝級別 | 碼和級別一一對應；但「哪一步失敗」從碼搬到文字，runner 要分辨讀檔失敗的原因就得讀文字（`-save-version` 要靠 `reporterCode` 分辨，O-5）|

**決定的理由**：(a) 不加新概念，把已經存在、而且已經正確的分法（照源碼逐條查過：`failure` 只在 `RunOn`、`Ended`、`ReplaceWorld` 的 `fail` 收 `WorldFailure` 時設）寫成契約，讓 runner 不必讀 `Product.cpp`。(b) 是型別最嚴的形，但改五個公開簽名換到的只是 (a) 的守衛已經保證的事。

**支撐的 Feature**：F1、F2、F4（Match 伺服器：一場對局的 Product 結束、行程開下一場）。

### D4 例外離開入口

**問題**：P5。

| 選項 | 系譜 | 內容 |
|---|---|---|
| **(b) 編排層的入口在邊界接 `std::exception`，轉成回傳值**（建議） | Go 的 `recover` 在工作單位頂端轉回 error；`error-handling.md` 規則 6（「A module may throw internally ... only if it catches at its public boundary and returns a Result. Boundary catches name `std::exception`」） | `RunOn`：接 `std::exception`（含 `std::bad_alloc`），照現在收 `WorldFailure`，**回** `Err`（碼新增一個，例：`ScheduleThrew`；名字實作時照命名規範）而不是再丟。`ReplaceWorld`：`BuildNewFirst` 時回 E2（舊 World 照跑）、`DropOldFirst` 時回 E1。`InsertSystems`：丟掉這一輪、回 E2。**不是** `std::exception` 的照舊往外丟（測試的 `AssertionFailure` 刻意不繼承 `std::exception`，規則 6 不准 `catch (...)`）。Kernel 不變（它的呼叫者是編排層）|
| (a) 照舊往外丟、runner 接 | 現在的形 | E1 從兩條路到；runner 要 `catch (...)`，違反規則 6 |

**決定的理由**：runner 拿到 E1 只剩一條路（回傳值），第 5.3 節的表只有兩列要查；而且規則 6 本來就要求 Engine API 在公開邊界把例外轉成 `Result`，(b) 是照現行規則補上編排層的缺口，不是新規則。`bad_alloc` 之後組 `WorldFailure` 本來就在 `catch` 裡做（`Product.cpp:428-431`），轉成回傳值不多配置。

**推翻的已定決定**：O-9「`build` 丟例外時先丟掉這一輪再往外傳」與 O-11 守衛的「bad_alloc」案、`WorldExecutor.hpp:29-31` 對編排層的說法——交使用者（第 13 節第 3 題）。

**支撐的 Feature**：F1（例外造成的 World 壞掉也留下恰好一筆）。

### D5 不理 `Result` 變成編譯錯誤

**問題**：P10。

| 選項 | 內容 |
|---|---|
| **(a) GYO 的程式（engine、products、tools、tests；不含 `third_party`）把「沒用 `[[nodiscard]]` 的回傳值」設成錯誤**（建議） | GCC／Clang `-Werror=unused-result`（含 `[[nodiscard]]` 的那一類）、MSVC `/we4834`；放在 `build/cmake/GyoBuild.cmake` 已經給 GYO 程式加 `/Zc:preprocessor` 的同一處。明寫的忽略用 `(void)` 並加註理由。照 Rust `#![deny(unused_must_use)]` 的形 |
| (b) 照舊只是警告 | 「不理是錯的」只靠審查 |

**理由**：使用者「不理是錯的！！！！」——能由建置擋的就不靠約定（Engine 只提供機制：這是機制，不是把規則釘死，明寫 `(void)` 仍然可以）。**風險**：現在若已有沒寫的忽略，三平台 L1 會紅；第 11 節第 1 步先在本機三種編譯器之一量命中數，**命中數不是 0 就停下回報**，不在這一批順手修。

**支撐的 Feature**：所有產品（E2 不被默默丟掉）。

### D6 預設 runner 與 `main`

**問題**：P8。

| 選項 | 內容 |
|---|---|
| **(a) 照舊：`main` 讀 `Failure()` 寫一次；在 O-3 與第 5.3 節寫明「沒有 runner 時 `main` 是處理 E1 的一方」**（建議） | 編排層不依賴日誌（LG-4：`world_logging` 依賴編排層，不是反過來） |
| (b) `Product` 銷毀時，結束過而沒人讀過 `Failure()` 就斷言 | 像 `std::thread` 解構時還 joinable 就 terminate；但使用者否定過「之後的呼叫斷言」（`-logging-no-failure-dedup`），同一種強制，退掉 |
| (c) `RunnerExit` 帶 `WorldFailure` | `main` 少一次讀取；但 `Failure()` 已經有，兩處同一個事實 |

### D7 級別的條文放哪

**建議**：`docs/architecture/target/error-handling.md`（英文，Engine 的錯誤處理目標頁，已有 DI 1、2）加「Error levels」一節與 DI 3～6（第 8 節）；`docs/architecture/error-handling.md`（Legacy）在「Programmer Error detected from runtime data」那一列與規則 1 加指向標記，正文等遷移批取代（照它頁首的規則：Legacy 只收現況與守衛還在的 DI）。不另開 v2 新頁：錯誤處理的 owner 頁只有一處（AGENTS §9「同じ事実を手動で維持する場所は、一か所に限定する」）。

---

## 7. 基盤 Architecture 的根據（AGENTS §2）

### 7.1 為什麼算基盤

錯誤的級別與「誰處理、誰記」是每個 Module、runner、產品、工具的程式都要照著寫的契約；改它時，上面每一處接錯誤的程式都要改，而且量隨產品與 Module 增加。照 AGENTS §2 的判準是基盤（「Engine の公開 Interface」「再利用可能な層の共通構造」）。

### 7.2 予見の根據 → 要求

| 根據 | 種類 | 導出的要求 |
|---|---|---|
| F1 一個 World 壞了，Product 結束，留下恰好一筆致命紀錄（使用者 D2「我們如果可以的話，也要記日誌」；`-logging-no-failure-dedup`「記日誌，然後退出」） | 1 名指しされた Feature | E1 只有一個處理者、只有一條通道（D1、D2、D4） |
| F2 讀檔：`BuildNewFirst` 失敗時遊戲繼續、玩家看到原因（O-8；存檔版本 Module「被拒是 `Result`」，`-event-delivery-rule`；讀檔批） | 1 | 同一個碼的 E2 不被編排層記；`reporterCode` 照帶（D2、D3） |
| F3 工具與編輯器：動態插入失敗時工具繼續（O-9；KeepTool、地圖編輯器） | 1 | E2 回呼叫者、不記（D2） |
| F4 Match 伺服器：一場對局壞了，行程結束那個 Product、開下一場（O-3「回主選單（另建一個 Product）」） | 1 | runner 一眼分得出 E1／E2（D3） |
| F5 日誌的致命入口 ⑤⑥（日誌批，已核准） | 1 | E0、E1 各一個寫出的地方（第 5.2 節） |
| Java／Go／Rust 的分級；Bevy、flecs、Unity 的處理器與日誌（第 3 節） | 2 成熟した参考実装 | 「處理一次」、工作單位的邊界、`#[must_use]` 可設成錯誤 |

---

## 8. Architecture Delta 與 Design Invariants

### 8.1 七項（AGENTS §3）

1. **要它的目的**：使用者明示的分級任務（`v8-arch-gyo2-error-levels`）；Feature F1～F5。
2. **現在的問題**：第 2.2 節 P1～P11（各附在哪、什麼、幾次）。
3. **變的邊界**：Kernel 的公開介面（拿掉 `SetErrorHandler`）；編排層的公開介面（拿掉 `Product::SetErrorHandler`、入口不再丟 `std::exception`、新增一個碼）；錯誤處理的契約（新的級別一節）；建置的警告設定（D5）。
4. **影響的 Module／Product／Tool**：`engine/kernel`、`engine/orchestration`、`build/cmake`（D5）、`tests/common/kernel`、`tests/common/orchestration`；日誌分支（`WithFailureLog` 與 LG-2 的文字，交界）；第 4 批（照本頁定 A3、A4 的錯誤型別；不改它的決定）。產品還沒有回到 `gyo2`，不受影響。
5. **依賴方向**：不變。不加任何依賴邊（編排層照舊不依賴日誌）。
6. **Ownership**：「記錄 Kernel 錯誤」從 Kernel 的 Executor 移到處理它的一方（runner 或 Kernel 的直接呼叫者）；「記錄編排層的警告」從編排層移到呼叫者。這是責任的移動，所以列為 Delta。
7. **為什麼不能更小**：只修 P1（拿掉 Product 結束時的 `Warning`）留下 P2（Kernel 再印一次）、P3（記又回傳）、P5（兩條路）；每一處都會讓 runner 的作者照樣不知道該不該記、記幾次。使用者要的是分級，不是修兩處（`-error-levels`「不不不……開一個任務卡來做這個事」）。

### 8.2 要改的 Design Invariants（都要使用者同意；最終文字在合併前核准定稿）

| 在哪 | 現在 | 改成 |
|---|---|---|
| `target/error-handling.md` DI 1 | 「returned as `Result<void, KernelError>`, logged once by the Executor, and never silent」 | 「returned once by the outermost `Run` (or `Trigger`) and never silent; the kernel never logs; the caller handles it once (level E1)」 |
| `target/error-handling.md` DI 2 | 「the Executor's handler is the only place a kernel error is logged」 | 「`Base::Result` stays a pure value; the kernel has no error handler and no log call」 |
| `target/error-handling.md` 新 DI 3 | — | **級別**：E0～E3 與非錯誤的定義、判準順序、升級只有一條（第 4.1 節） |
| 新 DI 4 | — | **處理一次**：產生與往上傳的一方不記；處理的一方至多記一次；一個錯誤只有一條通道（第 5.1 節） |
| 新 DI 5 | — | **E1 的處理者**：runner（沒有 runner 時 `main`）記一次然後退出；Kernel、編排層、Module 不記（第 5.2 節） |
| 新 DI 6 | — | **E3 不是錯誤值**：以診斷紀錄或 `Message` 表示，不改流程（第 4.1 節） |
| Kernel DI-KS 2（`target/kernel.zh-Hant.md`） | 「Executor 記錄一次並 Interrupted」 | 「最外層 `Run` 回傳一次並 Interrupted，Kernel 不寫日誌」 |
| 編排層 O-3 | runner 決定關程式或回主選單 | 補：「收到帶 `failure` 的錯誤時記一次、不再呼叫入口（E1，target error-handling DI 5）；沒有 runner 時 `main` 是處理者」 |
| 編排層 O-4 | 職責清單含「編排層自己的警告」 | 拿掉這一項（D2）|
| 編排層 O-9 | 「記一次 `Warning`、把錯誤回給呼叫的人……`build` 丟例外時先丟掉這一輪再往外傳」 | 「把錯誤回給呼叫的人、World 照跑；`build` 丟 `std::exception` 時丟掉這一輪、回錯誤」（D2、D4）|
| 編排層 O-11 | 「任何一個 World 中斷（Kernel 回 Err 或例外離開它的 Run）……」 | 補：「錯誤帶 `failure` ⟺ 這個 Product 有 World 壞了（E1）；例外（`std::exception`）離開 Run 時也以回傳值交給 runner」（D3、D4） |
| `docs/architecture/error-handling.md`（Legacy） | Executor 記一次 | 加指向 target 的標記，正文不改（D7） |
| 日誌 LG-1 ③、LG-2（日誌批的頁，未合併） | ③ 寫「編排層五處在用」；LG-2「同一份只寫一次」 | **不在本批改**；交界（第 11.2 節）|

### 8.3 Replacement Criteria（這一套級別的）

- **RC-EL1**：出現一種錯誤，照第 4.1 節的判準分不進任何一級，或同時是兩級（不是「跨邊界時升級」那種）——停下重看級別。
- **RC-EL2**：某個 Feature 需要 E1 之後繼續跑那個 Product（例：需要自癒）——那表示 O-11 或級別錯了，交使用者，不在功能裡繞過。
- **RC-EL3**：為了不重複，某處需要去重、「已寫過」旗標、或第二條通道——表示處理一次被破壞，找出哪一層多處理了一次。
- **RC-EL4**：E2 需要第二種「處理者」之外的人記（例：Module 要求每次失敗一定進日誌）——重看第 5.1 節。

---

## 9. 測試、守衛、突變

### 9.1 判定要變的守衛（AGENTS §14.4；緩的面要使用者核准）

| 守衛 | 守的性質 | 緩的面 | 締的面 | 性質還在哪守 |
|---|---|---|---|---|
| `orchestration.O-9` 各案的「記一次 `Warning`」 | 動態插入失敗被告知 | 以前「沒記 `Warning`」是失敗，之後通過 | 以前「記了 `Warning`」通過，之後失敗 | 「錯誤回給呼叫者、碼與 `kernel`／報告對」照舊由同一組案守 |
| `ReplaceTests` 等看 `Warning` 次數的案（實作時逐條列） | 替換失敗被告知 | 同上 | 同上 | 回傳的錯誤與 `failure` |
| Kernel 12 處以處理器計數的案（「記一次」） | Err 只報一次 | 處理器不存在 | — | 改成「`Run` 回一次 `Err`、Kernel 不寫 stderr」（新案，§14.2 的命中驗證）|
| O-8 「bad_alloc」「exception」案（D4 選 (b) 時） | 例外弄壞的 World 結束 Product | 以前「例外沒丟出來」是失敗 | 以前「例外丟出來、沒回錯誤」通過，之後失敗 | `failure` 的內容照舊檢查 |

選 D1 (b)、D2 (a)、D4 (b) 時才有這些；選別的照實際的列。

### 9.2 新的守衛（行為測試，和機制同時寫）

- **`[el-e1-iff-failure]`**：逐個入口、逐條失敗路徑（`RunGameInit`、`RunFrame`、`RunSchedule` 的 Kernel `Err` 與例外；`ReplaceWorld` 兩種先後 × 四個步驟 × 回錯誤／例外；`InsertSystems` 的三種失敗；`UnknownWorld`、`OwnerTaken`），檢查「`failure` 有值 ⟺ `Product::Failure()` 不是 null」。
- **`[el-orchestration-silent]`**：上面每一條路徑，`Base::Log` 的計數是 0（擴充 `[or-failure-not-logged]` 到全部路徑）。
- **`[el-kernel-silent]`**：Kernel 的 `Err` 路徑（System、巢狀、`Trigger`、hook round 上限）不寫 stderr——用子行程或把 stderr 導到檔案的 CTest（照 `gyo_assert_abort_probe` 的 `cmake -P` 形）；不是 `Base::Log` 的計數能看到的。
- **`[el-exception-returned]`**（D4 (b)）：`std::runtime_error`、`std::bad_alloc` 離開 System → 入口回 `Err`、不丟；`AssertionFailure` 照舊穿過。
- **D5**：一個編譯失敗樣本（不理 `Result` 的一行）在三平台的 L1 編譯失敗（照 `tests/common/world_logging/compile_fail/` 的形）。
- 掃描守衛（K6 拿掉 Executor 豁免、編排層不 include `Log.hpp`）：K6 已經存在，改範圍不是新立；編排層的含入掃描（`orchestration.guard` 的 O4）加 `engine/base/Log.hpp`——O4 已經存在。新的掃描不立（AGENTS §3「ガードを立てる時期」）。

### 9.3 突變（§14.5：只加別的突變不捕的誤）

| 突變 | 只有它捕的誤 | 守的對象 | 預期的失敗 |
|---|---|---|---|
| `el-replace-warn-back` | 在 `fail` 裡寫回一筆 `Base::Log` | DI 4、O-9 | `[el-orchestration-silent]` |
| `el-kernel-print-back` | `Fail` 裡寫回 `fprintf(stderr)` | DI 2 | `[el-kernel-silent]` |
| `el-failure-on-buildnew` | `BuildNewFirst` 失敗也設 `failure` | O-11、DI 3 | `[el-e1-iff-failure]` |
| `el-no-failure-on-dropold` | `DropOldFirst` 失敗不設 `failure` | 同上 | `[el-e1-iff-failure]` |
| `el-rethrow-std` | `RunOn` 接到 `std::exception` 後照舊丟 | O-11（D4） | `[el-exception-returned]` |
| `el-catch-all` | 接 `catch (...)`（把 `AssertionFailure` 也吞掉） | 規則 6 | `[el-exception-returned]` 的 `AssertionFailure` 案 |

既有突變：`or-…` 裡看 `Warning` 次數的（實作時 `rg` 找出、逐個判：守的對象拿掉就退役，§14.5）。突變清單由別的模型做一次敵對式檢查（§14.2；Fable 5.2／high，因為作者是 Opus）。

### 9.4 走查的驗證（§14.2）

第 2.2 節的「0 命中」走查（`警告|Warning` 在第 4 批三份宣告、`Werror|/WX|…` 在建置設定）用 `git grep`（不丟 stderr、看終了碼），先以已知命中驗過走得通：同一個式子 `警告|Warning` 在 `origin/gyo2` 的 `orchestration.zh-Hant.md` 命中（第 9、25 行）；建置設定那一條的同一種走法 `git grep -n "Zc:preprocessor" -- build/cmake` 命中 `GyoBuild.cmake:47-48`；`git grep -n "Warn(" engine/orchestration` 命中 6 行（定義 1、呼叫 5）。實作時照同樣的方法重跑並記在 dev_log。

---

## 10. 判定（先宣告，結果出來之後不放寬）

1. 第 9.2 節的新守衛全過，三平台 L1。
2. 第 2.1 節每一項在 dev_log 有「級／處置／守衛」三欄，沒有空格。
3. 一次 System 的 `Err` 與一次例外造成 Product 結束：整個行程的 stderr 與 `Base::Log` 只有 runner 寫的那一筆（用 `WithFailureLog`，在日誌批合併之後的情境測試裡數；日誌批還沒合併時，用 `Base::Log` 計數＋stderr 子行程兩條判，數 0）。
4. 第 9.3 節的突變全殺，新突變經 Fable 5.2／high 檢查補齊。
5. D5 選 (a) 時：本機量命中數是 0；不是 0 就停（第 15 節）。

---

## 11. 實作的步驟與估時、和其他批的交界

### 11.1 步驟（動工前核准之後；實作者 Opus 5.5／high，審查者 Fable 5.2／high，§14.4 的檢驗者 Opus 5.5／high——上段：觸 DI 與公開介面）

| 步 | 做什麼 | 估時 |
|---|---|---|
| 1 | D5 先量：本機 Clang 開 `-Werror=unused-result` 建全部 GYO target，記命中數；不是 0 就停 | 1.5 h |
| 2 | 條文：`target/error-handling.md` 級別一節與 DI 1～6、Legacy 標記、Kernel DI-KS 2、O-3／O-4／O-9／O-11（草稿，合併前核准定稿） | 2.5 h |
| 3 | Kernel：拿掉處理器（D1）、`KernelError.hpp:74-75` 與 `Executor.cpp` 頭註解、K6 豁免；Kernel 測試改看回傳的 `Err`；`[el-kernel-silent]` | 3 h |
| 4 | 編排層：拿掉五處 `Warn`（D2）、入口接 `std::exception`（D4）、`Product::SetErrorHandler` 拿掉；`[el-e1-iff-failure]`、`[el-orchestration-silent]`、`[el-exception-returned]`；改 O-9 等守衛（第 9.1 節） | 4.5 h |
| 5 | D5 的建置設定與編譯失敗樣本（第 1 步是 0 才做） | 1 h |
| 6 | 突變（第 9.3 節）與 Fable 檢查、退役的 `or-…` | 2 h |
| 7 | 三平台 L1、dev_log（盤點表的三欄）、HANDOFF | 2.5 h |
| | 合計 | 17 h |

### 11.2 和日誌批、第 4 批的交界

**誰先合**（建議）：**本批先合**，日誌批接著合，第 4 批的編排層那一步在本批之後。理由：日誌批已經暫停等本批（批次表「同日暫停，等異常分級完成」）；本批不需要日誌的程式（判定 3 在日誌合併前用 `Base::Log` 計數＋stderr 判）；第 4 批新增的錯誤照本頁的級別寫（第 5.5 節已逐項分好，不必改它的決定）。

| 交界 | 誰改誰的檔 |
|---|---|
| 日誌宣告第 3 節現況表的「編排層的警告：`Base::Log` 五處」、LG-1 的 ③ 說明 | 日誌批改（本批合併後那五處不存在）；本批不碰日誌的檔 |
| 日誌 LG-2「同一份只寫一次」 | 日誌批合併前核准時改（HANDOFF 已記）；本批第 5.2 節的 E1 一列是它要對齊的文字 |
| `WithFailureLog` 的 `catch (...)` 那一支（D4 選 (b) 時不再需要） | 日誌批改（它的程式）；本批合併後通知日誌卡 |
| 日誌宣告 5.6 的三種接法 | D4 選 (b) 時剩兩種（入口回錯誤、預設 runner 由 `main` 讀 `Failure()`）；日誌批改 |
| 第 4 批 A3（`Find<T>` 找不到）、A4（`Summary` 缺雜湊）的錯誤型別 | 第 4 批照本頁 E2 定；本批不碰第 4 批的檔 |
| 第 4 批編排層的共用槽（`Product.cpp`、`Setup.hpp`） | 和本批第 4 步同檔；後合的那一批 rebase。本批在 `Product.cpp` 的改動集中在 `Warn` 五處、三個 `catch` 與 `SetErrorHandler` |
| `tests/common/scenario/` 的段號 | 本批不加情境段（判定 3 用行為測試）；日誌批的致命情境段照本頁的 E1 一列寫 |
| HANDOFF、批次表 | 主會話記（本卡把決定轉過去） |

---

## 12. 範圍的限界

- 不改日誌 Module 的程式與文件（交界列給日誌批）。
- 不改第 4 批的宣告；只給級別對照（第 5.5 節）。
- 不改 Legacy 的 io、audio、threads、time 的錯誤（它們已照現行契約是 E2）。
- 不改 `GYO_ASSERT`、`Base::Result`、`Base::Error`、`Base::Log` 的介面。
- 不加級別的型別、列舉或欄位（D3）。
- 不碰產品的 runner（產品還沒回到 `gyo2`）；第 5.3 節是它們回來時照的表。
- 訊號（`SIGSEGV`）、記憶體已被寫壞、堆疊用盡：不分級（日誌 LG-2 的「不保證」照舊）。

---

## 13. 交使用者的

照「各家分歧、GYO 特有、推翻已定決定、職責清單多一項」四類。每題附建議與理由；名字與照源碼答得出的細節本卡已自己判（標在各節）。

1. **級別本身**（第 4 節）：E0～E3＋非錯誤，照「壞到哪一層」分；升級只有「System 回 `Err`」一條。**GYO 特有**（World／Product 這一層、致命以值回傳）。建議：照第 4 節。
2. **D2 拿掉編排層的五處 `Warning`**，O-9 改成只回錯誤，O-4 拿掉「編排層自己的警告」。**推翻已定決定**（O-9、O-4，編排層骨架核准）。建議 (a)。
3. **D4 入口在邊界接 `std::exception` 轉成回傳值**。**推翻已定決定**（O-9 的「往外傳」、O-11 守衛的例外案）；**各家分歧**（Go `recover` 在工作單位頂端轉回值、Bevy 在 System 邊界 `catch_unwind` 轉成值交處理器／Java 一路丟到頂端／Unity 在 group 接住、記了繼續）。建議 (b)。
4. **D1 拿掉 Kernel 的錯誤處理器**。**推翻已定決定**（target DI 1、2、DI-KS 2 的「Executor 記一次」，2026-11-13 核准）；**各家分歧**（Bevy 的處理器是唯一通道、GYO 往上回）。建議 (b)。
5. **D5 不理 `Result` 變成編譯錯誤**。**各家分歧**（Java 強制、Go 靠工具、Rust 可設成錯誤）。建議 (a)，先量、命中不是 0 就停。
6. **第 4 批「重複登記的裁決」判為非錯誤、不記**（P6、第 5.5 節）。主會話提出它和 O-9 形狀相同；本卡判：蓋過是登記規則的設計結果，不一致已是啟動錯誤。建議：照判、第 4 批不改。
7. **D3 runner 以 `failure` 有沒有值分 E1／E2**（不加級別欄位、不改簽名）。**各家分歧**（Go `errors.As`／Rust 分型別／Java 型別階層）。建議 (a)。
8. **誰先合**：本批 → 日誌批 → 第 4 批的編排層那一步（第 11.2 節）。批次先後本來由主會話排；因為日誌批在等本批，列出來讓使用者知道。

---

## 14. 和主會話、其他卡對齊的

- 使用者的決定（原話、比過的選項、理由、出處）轉主會話 `local_cb825af5-cff5-45d9-9c01-e92ad7a9e912` 記 HANDOFF。
- 動工前核准之後，通知日誌卡（`local_ba94d25b`）第 11.2 節的交界；第 4 批卡（`local_f8a8066c`）第 5.5 節的對照。

## 15. 停止條件

- 第 11.1 節第 1 步的命中數不是 0（D5）。
- 任何一步超過估時 1.5 倍，或單一問題累計 2 h。
- 實作時發現某個錯誤分不進第 4 節的任何一級（RC-EL1）。
- 要改日誌批或第 4 批的檔才做得下去。
- AGENTS §14 冒頭的三條。

## 16. 修訂 0 的敵對式檢查：處理對照

（檢查後補。）
