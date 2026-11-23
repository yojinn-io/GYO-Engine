# 事前宣告：Extract 的執行（`v8-arch-gyo2-sdl-extract`，SDL 線子批 R1a）

更新：2026-11-23（修訂 0）。Owner：Kernel＋編排層（GYO2 主會話開的任務卡，SDL 線子批 R1a）。**狀態：草稿。等敵對式檢查（Opus／high）→ 修訂 → 交主會話轉使用者動工前核准。核准前不動程式。**

根據：
- SDL 總宣告（`sdl-declaration.zh-Hant.md` 修訂 21，分支 `claude/gyo2-sdl-declaration-7e7a1f`；HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 6.1 節 1～3、5、6、8（只留形）、第 6.6 節、第 10.1～10.3 節的 R1a 列。
- [交換點宣告](exchange-declaration.zh-Hant.md)第 5.4 節（O-12 的 Extract 契約 1～6 與「留給 SDL 那批的」）。
- [Kernel](../kernel.zh-Hant.md) DI-KS 10、13、15、26；[編排層](orchestration.zh-Hant.md) O-4、O-5、O-8、O-11、O-12、O-15。
- 讀的版本：`origin/gyo2` 545b5a67。Bevy：本機 `~/Code/GitHome/bevy` 6a911653f。

經過：
- 修訂 0：照卡片寫。

---

## 0. 前提表

| # | 前提 | 誰定的 | 掛在上面的決定 |
|---|---|---|---|
| P1 | 渲染一類的 World 只經 Extract 唯讀讀來源，不寫回 | 使用者 11-11（K.3-6、DI-KS 13） | 2.1 的 `const`、2.4 的增刪讀法、第 3 節第 4、6 項 |
| P2 | Extract 只在同一條執行緒上的兩個 World 之間；兩個 World 都不在 Run 裡 | O-12；總宣告 6.1「執行緒」 | 2.1 的斷言、2.3 第 4 點 |
| P3 | extract schedule 寫在讀者的每幀清單，編排層看 `extract` 旗標改走 Extract 的入口；不在第一張不算錯 | 交換點宣告 5.4-2、總宣告 6.1-3（已核准） | 2.3 第 4 點 |
| P4 | Kernel 的原語是 `WorldExecutor::RunExtract(schedule, const World& source)`；屬 DI-KS 15 第 (3)、(4) 項、不是清單多一項；不加 `StartupCode`；最外層 `Run` 指名 extract schedule 是斷言 | 總宣告 6.1-1（已核准） | 2.1 |
| P5 | 讀者的描述帶 `extractFrom`；來源不存在、排在讀者之後是啟動錯誤 O-E9；編排層在讀者第一次 `BuildPlans` 之前呼叫 `SetExtractSource` | 總宣告 6.1-2（已核准） | 2.3 第 1～3 點 |
| P6 | 來源被替換的做法在 R1b 比、交使用者 | 總宣告 6.1-4 | 2.3 第 5 點、J3 |
| P7 | 錯誤只告訴外面發生了什麼；誤用不加守衛，由設計審查抓 | 使用者 11-20、11-21 | J2 的分界、J6 |
| P8 | 實作薄起步：只做點名的 Feature 直近要用的，其餘只定形 | AGENTS §2 | 2.2 只做讀、2.4 只留形 |
| P9 | ECS 相關機制以 Bevy 為主參考；偏離寫衝突點與膠水量 | 使用者 11-23 | 第 3 節 |
| P10 | 改 Kernel 的三個子批建議 R1a 先合、KR 跟上、R1b 在 R1a 之後 | 總宣告 10.1（主會話排） | 第 7 節 |

## 1. 這一批做什麼

### 1.1 用遊戲講

KeepTool：文件 World 每幀跑完 ImGui 的 widget System，把 draw list 放進自己的一個 Resource。渲染 World 接著在同一幀跑，它的 extract schedule 裡有一個 System 用 `ExtractRes<DrawList>` 讀文件 World 的那份 draw list，抄成渲染 World 自己的資料（之後由 R2a 的 System 變成 `RenderCommands`）。文件 World 不知道有人讀它，渲染 World 也改不了它。

2D 遊戲（R2b）同理：模擬 World 跑完這一幀，渲染 World 用 `Extract<Read<Sprite>>`、`Extract<Read<GlobalTransform>>` 讀每個實體，在自己這邊畫。

### 1.2 用機制的詞

Kernel 今天只有 Extract 的「宣告那一半」：`Extract<Read<T>>`、`ExtractRes<R>` 參數（`Params.hpp:1318-1340`，寫的版本編譯不過）、`ScheduleOptions::extract`、`Registry::SetExtractSource`（`Registry.cpp:671-675`）、來源沒設或沒建過 Plan 的 D-E14（`Registry.cpp:908-911`）、Extract 參數只能在 extract schedule（D-E7，`Registry.cpp:1065-1067`）。**執行沒有**：句柄沒有任何讀的方法，建構子在 `call.world` 不是空時斷言（`Params.hpp:1325`、`:1335`，舊的 rollback-extract 隨第 4 批拿掉）；最外層 `Run` 不擋 extract schedule；巢狀跑被 X-15 拒（`Executor.cpp:499-502`）。編排層沒有呼叫 `SetExtractSource` 的地方。

這一批補執行：Kernel 的 `RunExtract`、Extract 參數讀得到來源；編排層的 `extractFrom`、O-E9、清單與 `RunSchedule` 遇到 extract schedule 走 `RunExtract`。

**不做**：來源被替換（R1b）、渲染 World 的實體對應與「增刪了什麼」的實作（R2b）、`render_world` 把 extract schedule 插在 `Main` 之前（R2a）、渲染 World 的時間（W）。

## 2. 要做的

### 2.1 Kernel：`WorldExecutor::RunExtract`

```cpp
Engine::Base::Result<void, KernelError> RunExtract(std::string_view schedule, const World& source);
```

1. **一次 `RunExtract` 是讀者 World 的一次最外層 Run**，和 `Run` 走同一段：讀 `KernelSettings`、清這次 Run 的 `DroppedMessages` 紀錄、Ready 窗的比對、每一步、同步點、hook 輪、結束時修剪讀者自己的訊息、失敗時中斷讀者。差別只有一點：這次 Run 裡 System 與 run 條件的 Extract 參數讀 `source`。extract schedule 的 System 寫讀者照常（`Commands`、`ResMut` 等），在讀者自己的同步點套用。
2. **來源只經 `const World&` 讀**：不寫來源的資料，也不動來源的 stamp、訊息游標、修剪與診斷紀錄。讀之前來源若還沒接上它 Registry 的新版本（DI-KS 26：來源動態插入之後還沒被用過），照 World 既有的 const 讀法接上（`World.cpp:70`；接版本是 World 自己的簿記，不是資料的寫入，既有的 const 讀法本來就這樣做）。
3. **斷言**（Programmer Error；不加 `StartupCode`、不加 `KernelErrorCode`）：讀者是 Ready；schedule 存在且是 extract schedule；`source` 是由讀者 Registry 設的來源 Registry 建的 World（比 Registry 的身分），而且不是讀者自己；`source` 是 Ready（不在 Run 裡、沒中斷）。`Run` 指名 extract schedule 也改成在入口斷言（今天會跑到句柄建構子的斷言，訊息還寫著 rollback-extract），訊息指向 `RunExtract`。
4. **巢狀**：`ExclusiveWorld::RunSchedule` 指名 extract schedule 照舊被拒（X-15 `ExtractNotNestable`，回 Err、讀者中斷）；只把訊息裡的「rollback-extract」改成「`RunExtract`」，碼與規則名不變。extract schedule 的 System 巢狀跑別的 schedule 照 K0；被跑的 schedule 不能有 Extract 參數（D-E7 已擋）。
5. **Kernel 不保存來源 World**：來源 World 每次由呼叫的人給；Kernel 只記來源的 Registry（`SetExtractSource`，現有）。這是 R1b 能換來源的前提（DI-KS 37 草案）。
6. **驗證（A.6）**：只比讀者 World。來源在 extract 期間被寫，只可能經保留的指標或另一個 executor（誤用）；經保留的指標的寫入由來源下一次 Run 的 Ready 窗查到（現有），不另加偵測（P7、J6）。

實作的位置（給審查對照，不是承諾）：`WorldExecutor.hpp`／`Executor.cpp`（`RunExtract`；`Run` 與 `RunExtract` 共用一段；X-15 的訊息）；`detail/Context.hpp`（`SystemContext` 加 `const World* extractSource`；const 版的 `ColumnAt`／`ResourceAt` 宣告）；`World.cpp`（const 版那兩個函式，各幾行，不碰同步點的套用）；`Params.hpp`（Extract 句柄）。不改 `Registry.cpp` 的登記與檢查（D-E7、D-E14、R-E4 照舊）；不改指紋（`ScheduleOptions::extract` 本來就在 Plan 指紋裡）；J-32 的 golden 不受影響。

### 2.2 Extract 參數的讀法

照 `Query<Read<T>>` 與 `Res<T>` 的唯讀那一半：

| 參數 | 方法 | 說明 |
|---|---|---|
| `Extract<Read<T>>` | `Entities()` | 來源裡有 T 的實體，EntityId 升冪（同 `Query`，W-2） |
| | `Count()`、`Contains(id)` | |
| | `Get(id) -> const T*` | 沒有時 `nullptr` |
| | `ForEach(fn(EntityId, const T&))` | 升冪 |
| `ExtractRes<R>` | `Present()`、`operator*`、`operator->` | 沒有值時取值是斷言（同 `Res`） |

- 值是來源這一輪跑完（同步點與 hook 輪都套用了）的狀態。
- 句柄只在那一次呼叫裡有效（W-4，現有的檢查照用：檢查讀者那一次呼叫的序號）。
- **不做**（P8）：多個 term 的 join（Bevy 的 `Extract<Query<…>>`）、`Changed`／`Added`、讀來源的 `Message`。join 用多個參數加同一個 id 的 `Get`；後兩者見 2.4。

### 2.3 編排層

1. **描述欄位**：`WorldDescription::extractFrom`（`std::string`，空＝沒有來源）。
2. **O-E9**（新碼；O-E8 不用，理由見總宣告 6.1-2）：
   - (a) `extractFrom` 指名的 World 不在描述裡、是自己、或排在讀者之後（World 照描述的順序跑，來源要先跑完這一輪）；
   - (b) 一個 World 的每幀清單裡有 extract schedule，但那個 World 沒有 `extractFrom`；開局清單裡有 extract schedule（開局清單跑在第一個 World，它前面沒有 World 可以當來源）。
   - (b) 是描述的契約檢查（和 O-E5「清單裡的名字不是那個 World 的 schedule」同類：沒有來源時那一張 schedule 跑不了），不是對誤用的守衛（J2）。
   - **不算錯**：有 `extractFrom` 但清單裡沒有 extract schedule（Bevy 的 sub app 也可以有 extract 函式而不抽東西）；一個來源有多個讀者；讀者再當別人的來源（鏈）——Extract 只讀，沒有理由擋。
3. **登記時接來源**：讀者的登記在它的 `BuildPlans` 之前呼叫 `SetExtractSource(來源的 Registry)`。啟動時，來源的登記已在前面做完（來源排在前面）；替換讀者時，用來源目前的 Registry。
   - 來源的登記失敗（Kernel 的 `StartupReport`、或來源本身被 O-E3／O-E6 擋下沒有登記）時，讀者不登記（`WorldStartupResult::notChecked`），只報來源的錯，不另報讀者的 D-E14（J4）。
   - 有 Extract 參數卻沒有 `extractFrom` 的 World：Kernel 的 D-E14（現有），編排層不另報。
4. **清單與 `RunSchedule`**：編排層跑一張 schedule 的地方（每幀清單、`RunSchedule`、替換時的 `beforeSwap`）共用一段：schedule 的 `extract` 旗標是真就 `RunExtract(schedule, 來源的 World)`，否則 `Run`。
   - 讀者的交換函式「進」本來就在清單之前（O-12），不必改。
   - `RunSchedule(讀者, extract schedule)` 和清單走同一條路（J1；總宣告 6.1-3 留給 R1a 定）。`RunSchedule` 指名 extract schedule 而那個 World 沒有來源：斷言（同「名字不存在」今天是斷言）。
   - 入口先看所有 World 健康（O-11，現有），所以跑到 `RunExtract` 時來源一定健康、不在 Run 裡。
5. **替換**：
   - 替換讀者：新的 Registry 照第 3 點接上來源目前的 Registry；`beforeSwap` 裡的 extract schedule 照第 4 點。
   - **替換有讀者的來源**：R1b 之前是斷言（J3）。今天 `SetExtractSource` 存的是來源 Registry 的指標，換上新來源時舊的 Registry 銷毀、讀者的指標懸空；怎麼做（讀者一起重建／准許換來源）是 R1b 要比、交使用者的題，R1a 不先選。R1b 合併時拿掉這個斷言。R1a 到 R1b 之間沒有點名的產品會替換有讀者的來源（KeepTool 不替換 World，總宣告 6.1-4）。
6. **銷毀**：讀者先於來源（O-15 的反序，現有）；讀者的 Registry 也先於來源的 Registry 銷毀（`~Product` 每個槽依序丟 World、Registry，照描述的反序）。R1a 加測試釘住 World 的先後（Registry 的先後由審查對照 `~Product` 的步驟）。
7. **生成世界（O-14）**：不加規則。來源在生成中時讀者照樣抽，讀到的是生成中的狀態；要等的話，渲染側用 `ExtractRes<WorldGeneration>` 讀來源的生成狀態自己判（`WorldGeneration` 每個 World 都登記，O-2）。

### 2.4 「上次 Extract 之後來源改了什麼」的形（只留形；R2b 定與實作）

總宣告 6.1-8 要 R1a 留這個讀法的形。三件事分開：

| 要知道的 | 形 | 理由 |
|---|---|---|
| 新增、改過（`Added`／`Changed`） | 讀者的每個 extract 掛載點記「上次 extract 時來源的 stamp」（來源的 stamp 空間），拿來比來源欄的 stamp；記在讀者這邊，不寫來源 | Bevy 的 `Extract<P>` 內有自己對 main world 的 `SystemState`（`bevy_extract/src/extract_param.rs:58-62`），每次取參數時更新 `last_run`（`bevy_ecs/src/system/function_system.rs:456`），所以 `Changed` 以上一次 extract 為準。GYO 的 stamp 每個 World 各一套，讀者掛載點的 `lastRun` 在讀者的空間，不能直接拿來比來源 |
| 拿掉（實體刪了、元件拿掉） | Kernel 不給；讀者這邊以來源實體為鍵的表，每次比對（Bevy `RenderMeshInstances` 的形） | Bevy 的同步把 `RenderEntity` 寫回來源、由讀者清來源的待同步清單（`bevy_extract/src/sync_world.rs`），和 DI-KS 13 不相容（總宣告 6.1-8、6.6） |
| 來源的 `Message` | Extract 不讀；要給讀者的資料放成元件或 Resource | 來源在自己最外層 Run 結束時修剪所有讀者都讀過的紀錄（DI-KS 10），Extract 在那之後才跑；跨 World 的讀者要拉住保留，就得在來源記游標＝寫來源。Bevy 的 `Extract<MessageReader<…>>` 靠訊息的雙緩衝活過兩次 update，GYO 沒有 |

R1a 不實作任何一項。DI-KS 36 草案寫「沒有 `Changed`／`Added`、不讀來源的 `Message`」；R2b 要加時改那一條（RC 28 (b) 是重評的觸發）。

### 2.5 條文（草案；最終文字合併前核准）

**DI-KS 35.** **Extract 的執行。** `WorldExecutor::RunExtract(schedule, const World& source)` 是讀者 World 的一次最外層 Run、只跑 extract schedule：步驟、同步點、hook 輪、讀者訊息的修剪、失敗時的中斷都和 `Run` 一樣，差別只有這次 Run 的 System 與 run 條件的 Extract 參數讀 `source`。斷言：`Run` 指名 extract schedule；`RunExtract` 指名不是 extract 的 schedule；`source` 不是由讀者 Registry 設的來源 Registry 建的 World，或就是讀者；`source` 不是 Ready。巢狀跑 extract schedule 照舊被拒（X-15）。不加啟動錯誤與執行時錯誤碼；屬第 (3)、(4) 項、不是清單多一項。守衛：第 2.6 節 K-1、K-2、K-4；J-56；突變 `ex-`。（`v8-arch-gyo2-sdl-extract`；系譜 Bevy `SubApp::extract`、`bevy_extract::extract`）

**DI-KS 36.** **Extract 參數只讀來源這一輪跑完的狀態。** `Extract<Read<T>>`（`Entities` 升冪、`Count`、`Contains`、`Get`、`ForEach`）與 `ExtractRes<R>`（`Present`、取值）經 `const World&` 讀；不寫來源的資料，也不動來源的 stamp、訊息游標、修剪與診斷紀錄（來源接上自己 Registry 的新版本是 World 的簿記，照既有的 const 讀法）。沒有 `Changed`／`Added`、不讀來源的 `Message`（來源在自己的 Run 結束時已修剪，DI-KS 10）；要加時改這一條（形在[事前宣告](sdl-extract-declaration.zh-Hant.md) 2.4）。守衛：K-1、K-2、K-3、K-5；`Extract<Write<T>>` 不合法（`SignatureTests.cpp`，J-43）。

**DI-KS 37.** **Kernel 不保存來源 World。** 讀者的 Registry 只記來源的 Registry（`SetExtractSource`，在第一次 `BuildPlans` 之前）；來源 World 每次由呼叫的人給 `RunExtract`。呼叫的人的規則（Kernel 看不到）：來源的 Registry 比讀者的 Registry 活得久；`RunExtract` 期間不寫來源（同一條執行緒、來源不在 Run 裡）。編排層照 O-12、O-15 做。守衛：編排層的 O-6 測試（銷毀先後）；審查。

**RC 28.** **Extract 要寫回、要動兩個 World，或參數不夠**（草案）：(a) 某個點名的 Feature 需要 Extract 寫回來源（GPU 讀回進 World、照 Bevy 把對應實體寫回來源），或同步那一步要同時改兩個 World（R2b 的實體對應）——DI-KS 13 要改，停下交使用者；(b) 兩個以上的 Module 或產品為了在 extract 裡過濾、join、或記「上次抽到哪」，在渲染 World 抄整份來源或各自寫比對——重評 `Extract<Query<…>>` 與 2.4 的形。

**DI-KS 13 的守衛欄**：補 K-1、K-2 與 `ex-` 突變；條文不變。

**編排層**（`orchestration.zh-Hant.md`）：
- O-5：「`OrchestrationCode`（O-E1～O-E7）」→「（O-E1～O-E7、O-E9；O-E8 不用）」。
- O-8 末句「做法在 SDL 那批定」→「R1b 定做法之前，替換有讀者的來源是斷言（`v8-arch-gyo2-sdl-extract`）」。
- O-12 的 Extract 那一項，照 2.3 寫成：讀者的描述 `extractFrom` 指名一個排在前面的 World（O-E9）；編排層在讀者第一次 `BuildPlans` 之前接上來源的 Registry；清單、`RunSchedule`、`beforeSwap` 遇到 extract schedule 經 Kernel 的 `RunExtract` 跑（DI-KS 35）；讀者的交換函式「進」在它之前；extract schedule 不在清單最前面不算錯，在它之前跑的 schedule 讀到上一輪抽的資料。守衛欄加 `orchestration.extract` 各案與突變 `or-ex-`。
- O-4 不改（「extract 的時機」已在職責清單）。

**說明頁**（`system-contract-page.zh-Hant.md` 第 62 列附近）：Extract 參數的讀法、值是來源這一輪跑完的狀態、不讀 `Message` 與沒有 `Changed` 的後果、在 extract 之前跑的 schedule 讀到上一輪抽的資料。

### 2.6 測試（驗收的骨幹）與突變

**Kernel**（新檔 `tests/common/kernel/ExtractTests.cpp`，CPU 標籤，照 kernel 既有的分組）：
- **K-1 讀到來源這一輪的值**：來源的 Run 寫 T 與 R；讀者 `RunExtract` 的 System 讀到同值。來源再跑、改值，再 extract 讀到新值。兩個 World 的同一種元件放不同的值（避免讀錯 World 也通過）。
- **K-2 來源不被動**：`RunExtract` 前後，來源的 `World::Summary`、讀到的欄的 stamp、來源的訊息游標與紀錄筆數都相同；讀者 extract System 經 `Commands`／`ResMut` 寫讀者，在讀者的同步點套用。
- **K-3 讀法**：`Entities` 升冪（來源 spawn 的順序打亂）、`Get` 沒有時 `nullptr`、`Count`、`Contains`、`ForEach`；`ExtractRes::Present` 在來源沒有值時是 false。
- **K-4 run 條件帶 Extract 參數**：條件讀來源決定跑不跑。
- **K-5 來源接上新版本**：來源動態插入一種新元件、還沒再跑過；讀者再登記一輪加讀它的 extract System；`RunExtract` 讀得到（空的欄，`Count() == 0`），不讀到不存在的欄。接續 `RegistrationRoundTests.cpp` 的「DI-KS 13: an extract source registers again …」（那一案只驗登記）。
- 現有照留：J-56（X-15）、`SignatureTests.cpp` 的 `Extract<Write<…>>` 不合法、J-24、D-E14 的突變。
- 斷言不寫測試：Kernel 的測試沒有 death test 的機制（既有慣例）；由審查對照 2.1 第 3 點。

**編排層**（新檔 `tests/common/orchestration/ExtractTests.cpp`）：
- **O-1 每幀**：來源先跑、讀者的 extract 讀到來源這一幀的值；讀者的交換函式「進」在 extract 之前（記錄順序）。
- **O-2 O-E9**：指名不存在的 World、指名自己、指名排在後面的 World、每幀清單有 extract schedule 卻沒有來源、開局清單有 extract schedule，各一案。
- **O-3 來源的登記失敗**：只報來源的錯；讀者 `notChecked`、沒有 D-E14。
- **O-4 `RunSchedule(讀者, extract)`**：讀到來源目前的值。
- **O-5 替換讀者**：`BuildNewFirst`、`DropOldFirst` 之後照讀；`beforeSwap` 指名 extract schedule 讀得到來源。
- **O-6 銷毀**：讀者的 World 先於來源的 World 銷毀（兩邊各放一個解構時記錄的 Resource）。
- **O-7 一個來源兩個讀者、鏈**：都讀得到。

**突變**（D184：新的全跑；既有突變中 `find` 落在改動範圍的也跑——`ns-extract-nestable` 的那一行不改，但同一個 `if` 的訊息改了，照跑一次）。草案，實作時照 §14.5 收斂，清單送敵對式檢查（DI-KS 13 的守衛，§14.2）：

| id（草案） | 改動 | 預期的失敗 |
|---|---|---|
| `ex-reads-reader` | Extract 句柄讀讀者 World 而不是來源 | K-1（兩個 World 的值不同） |
| `ex-entities-unsorted` | `Entities` 照欄的存放順序反過來 | K-3 |
| `ex-res-present-inverted` | `ExtractRes::Present` 反過來 | K-3 |
| `ex-condition-no-source` | run 條件建句柄時不給來源 | K-4（句柄的斷言訊息） |
| `or-ex-source-not-set` | 登記時不呼叫 `SetExtractSource` | O-1（啟動失敗，D-E14） |
| `or-ex-oe9-later-accepted` | 不查「排在後面」 | O-2 那一案 |
| `or-ex-oe9-list-unchecked` | 不查「清單有 extract 沒來源」 | O-2 那一案 |
| `or-ex-runon-plain-run` | 跑 schedule 時不看 `extract` 旗標 | O-1（`Run` 入口的斷言訊息） |
| `or-ex-failed-source-registers-reader` | 來源登記失敗時照樣登記讀者 | O-3（多一個 D-E14） |

預期的失敗是斷言訊息時，`expect` 用那條斷言專用的固定字串（§14.2：可識別）。

## 3. 和 Bevy 的對照（P9；逐項）

| # | 項 | Bevy | GYO（R1a） | 衝突點 | 膠水量 | 處置 |
|---|---|---|---|---|---|---|
| 1 | 讀者與來源怎麼宣告 | `SubApp::set_extract(fn)`（`bevy_app/src/sub_app.rs:174`），`ExtractPlugin` 設成 entity sync＋跑 `ExtractSchedule`（`bevy_extract/src/extract_plugin.rs:83-94`） | 描述欄位 `extractFrom`＋O-E9 | GYO 的 World 由描述組（O-4），沒有 sub app 物件 | 一個欄位、一個錯誤碼 | 照 GYO 的描述；接合點：一個 Bevy sub app＝一個有 `extractFrom` 的 World |
| 2 | 時機 | `SubApps::update`：main 跑完 → 每個 sub app 先 `extract` 再 `update`（`sub_app.rs:575-590`） | World 照描述的順序跑、來源在前；extract schedule 在讀者的清單裡，預設由 `render_world` 插在 `Main` 之前（R2a） | 位置可變（P3） | 0 | 預設排法同效果 |
| 3 | Extract 參數 | `Extract<P>` 包任何唯讀 `SystemParam`，內有自己對 main world 的 `SystemState`（`extract_param.rs:51-62`） | 只有 `Extract<Read<T>>`、`ExtractRes<R>` | **偏離**（薄起步，P8）：沒有多 term 的 join、沒有 `Changed` | join 用多個參數＋`Get`；`Changed` 見 2.4 | RC 28 (b) 是重評的觸發 |
| 4 | 來源怎麼交給讀者 | 把 main world `mem::replace` 成 `MainWorld` Resource 放進 sub world，跑完換回（`extract_plugin.rs:136-147`）；`Extract` 經 `Res<MainWorld>` 讀，只讀靠型別 | `const World&` 參數；Kernel 不保存（DI-KS 37） | 無 | 0 | 同方向（只讀靠型別），不必搬 World |
| 5 | extract schedule 的延後變更 | 關掉自動 `apply_deferred` 與最後一次套用（`extract_plugin.rs:60-66`），留到 sub world 的 schedule 裡 `apply_extract_commands`（`:113-121`），為了 pipelined 時讓 main 先走 | 在 extract schedule 自己的同步點套用（一般的 schedule） | **偏離** | 0 | 不開 render 執行緒（master D27、總宣告 6.1「執行緒」）；日後開時再看 |
| 6 | 寫回來源 | `MainWorld` 可以 `ResMut` 寫回；entity sync 在 extract 前寫 main（`extract_plugin.rs:86-90`） | 不寫回（P1） | 實體對應的寫回（總宣告 6.1-8、6.6） | 見 2.4 | RC 28 (a) |
| 7 | 讀來源的 `Message` | 可以（訊息雙緩衝，活過兩次 update） | 不讀（2.4） | **偏離**：來源在自己的 Run 結束時修剪（DI-KS 10） | 要給讀者的放成元件或 Resource | 告知（J5） |
| 8 | 只跑 extract | `update_subapp_by_label`＝extract＋update（`sub_app.rs:604-609`）；直接 `run_schedule(ExtractSchedule)` 會因沒有 `MainWorld` 而 panic | `RunSchedule(讀者, extract)` 單跑 extract schedule、自動給來源 | 小偏離 | 0 | `RunSchedule` 是單一 schedule 的入口（O-1）（J1） |
| 9 | extract 期間來源被寫的偵測 | 靠借用規則（`&mut World` 被搬走） | const 型別擋 API 的寫；經保留的指標的寫由來源下一次 Run 的 Ready 窗查（現有）；不另加 | 舊規格 H-6 寫過「驗證開著時記為違規」，現行 DI-KS 13 沒有這句 | 0 | J6 |

## 4. 卡片照 L17 判的（可以推翻）

| # | 判的 | 比過的 | 理由 |
|---|---|---|---|
| J1 | `RunSchedule(讀者, extract schedule)` 和清單走同一條路（有來源就 `RunExtract`） | 斷言（只准在清單裡跑）；回 `OrchestrationError` | 總宣告 6.1-3 留給 R1a。同一張 schedule 換個入口行為不同是陷阱；測試與工具要單跑一張 schedule 時也要有來源。Bevy 單跑會 panic，那是因為 `MainWorld` 只在 extract 函式裡存在，不是設計上要禁 |
| J2 | O-E9 (b)：清單有 extract schedule 但 World 沒有來源＝啟動錯誤 | 跑到時才斷言 | 描述自相矛盾，啟動時就看得出來，和 O-E5 同類；不是對誤用加守衛（沒有來源，那張 schedule 根本跑不了） |
| J3 | 替換有讀者的來源：R1b 之前是斷言 | 回新的 `OrchestrationError` 碼（R1b 會拿掉，留下只活一批的碼）；R1b 併進 R1a（R1b 有交使用者的題、要改 DI-KS 13、26 與 RC-O13，會拖慢 K 的路） | R1a 不先選 R1b 的做法；斷言最小、不留公開契約；R1a 到 R1b 之間沒有產品會走到 |
| J4 | 來源的登記失敗時讀者不登記（`notChecked`） | 照樣登記、多報一個 D-E14 | 一個錯只報一次；讀者的 D-E14 是來源失敗的連帶，不是讀者自己的錯 |
| J5 | 沒有 `Changed`／`Added`、不讀來源的 `Message` | 現在就做 2.4 的第一列 | 點名的 Feature 直近不用（K 的 draw list 每幀重生）；形已留（2.4），R2b 照它做 |
| J6 | 不加「extract 期間來源被寫」的執行期偵測 | 照舊規格 H-6 在驗證開著時偵測 | 誤用不加守衛（P7）；const 擋 API，保留的指標由現有的 Ready 窗查到。舊規格 H-6 這句不在現行 DI-KS 13 裡，不是推翻已核准的條文 |
| J7 | Kernel RC 用 28 | — | 總宣告 10.1 原分配是 KR 25～27、R1a／R1b 28～29；25 被摘要那一批用掉，KR 改用 26、27，不影響 28。**請主會話確認** |

## 5. Architecture Delta（差分；七項在總宣告第 11 節）

- **公開介面**：Kernel 新增 `WorldExecutor::RunExtract`；`Extract<Read<T>>`、`ExtractRes<R>` 有了讀的方法。編排層新增 `WorldDescription::extractFrom`、`OrchestrationCode::OE9`。
- **Dependency**：沒有新的邊（編排層本來就依賴 Kernel）。
- **Ownership**：不變。Kernel 的執行原語屬 DI-KS 15 第 (3)、(4) 項；編排層的 extract 時機屬 O-4 已列的「extract 的時機」。
- **Data Contract**：沒有。
- **Design Invariants**：DI-KS 35～37、RC 28 新增；DI-KS 13 的守衛欄補；O-5、O-8、O-12 改句（第 2.5 節）。
- **和總宣告的差分**：沒有超出計畫的 Module、Product 或依賴。

## 6. 範圍外與範圍的限界

- R1b（來源替換）、R2a（`render_world` 插 extract）、R2b（實體對應、2.4 的實作）、W（渲染 World 的時間）。
- 斷言不寫測試（沒有 death test 的機制）。
- 不量測、沒有預算判定（Extract 的幀時間在 R2a、K 量）。不是計測的批，沒有件數的突き合わせ。
- R1a 合併到 R2a 之間，Extract 只有測試在用，沒有產品用。

## 7. 和其他卡的協調（經主會話）

- **KR**（`local_e2d05fe7`，分支 `claude/gyo2-kernel-hierarchy-06b6eb`）：都改 Kernel。R1a 改 `Executor.cpp`（`Run` 的入口分出共用段、X-15 的訊息）、`World.cpp`（const 版 `ColumnAt`／`ResourceAt`，不碰同步點的套用）、`Params.hpp`、`detail/Context.hpp`、`WorldExecutor.hpp`、`kernel.zh-Hant.md`（DI-KS 35～37、RC 28、DI-KS 13 的守衛欄、更新行）。KR 改同步點的套用（`World.cpp`）與 `Executor.cpp`。條號不重疊（KR：DI-KS 29～33、34 保留、RC 26、27）。照總宣告 R1a 先合；誰後合併誰 fetch＋merge gyo2（不 rebase）。
- **R1b**：拿掉 J3 的斷言；以 DI-KS 37 為前提。
- **K、R2b**：用 2.2 的 API；R2b 照 2.4 的形做增刪。
- **O-E9**：號已在總宣告保留；通知並行的卡由主會話。

## 8. 分工、檢查與估時

- **上段**（Kernel 與編排層的契約，navigator）。
- **實作者** Opus／high；**審查者** Opus／high（另一個 agent；Fable 用量不足期間作者與審查者同模型，報告寫明）；本批不改既有判定（§14.4 沒有緩的面），不需要驗證者。
- **本宣告的敵對式檢查**：Opus／high（作者同模型，報告寫明）。**突變清單的敵對式檢查**：Opus／high。
- **步驟**：(1) Kernel 的 `RunExtract`、Extract 句柄、K-1～K-5 → (2) 編排層、O-1～O-7 → (3) 文件（第 2.5 節）→ (4) 突變（D184）與清單的敵對式檢查 → (5) 審查 → dev_log、draft PR 到 `gyo2`、手動 dispatch L1。合併等使用者明說「合併」。
- **估時（推定）**：Kernel 5～9 h、編排層 4～8 h、文件 2～3 h、突變與檢查 2～4 h、審查與修正 2～4 h，合計 **15～28 h**。超過估計 1.5 倍就停下回報。

## 9. 動工前核准清單

1. 範圍（第 2 節）與不做的（2.2「不做」、2.4、第 6 節）。
2. 卡片判的 J1～J6（第 4 節）。
3. 條文草案的方向：DI-KS 35～37、RC 28、DI-KS 13 守衛欄、O-5／O-8／O-12 改句（最終文字在合併前核准）。
4. 條號：RC 28（J7，主會話確認）。
