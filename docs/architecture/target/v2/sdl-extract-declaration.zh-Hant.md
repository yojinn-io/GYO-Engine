# 事前宣告：Extract 的執行（`v8-arch-gyo2-sdl-extract`，SDL 線子批 R1a）

更新：2026-11-23（修訂 1）。Owner：Kernel＋編排層（GYO2 主會話開的任務卡，SDL 線子批 R1a）。**狀態：交主會話轉使用者動工前核准。核准前不動程式。**

根據：
- SDL 總宣告（`sdl-declaration.zh-Hant.md` 修訂 21，分支 `claude/gyo2-sdl-declaration-7e7a1f`；HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 6.1 節 1～3、5、6、8（只留形）、第 6.6 節、第 10.1～10.3 節的 R1a 列。
- [交換點宣告](exchange-declaration.zh-Hant.md)第 5.4 節（O-12 的 Extract 契約 1～6 與「留給 SDL 那批的」）。
- [Kernel](../kernel.zh-Hant.md) DI-KS 2、10、13、15、26；[編排層](orchestration.zh-Hant.md) O-4、O-5、O-8、O-11、O-12、O-14、O-15。
- 讀的版本：`origin/gyo2` 545b5a67。Bevy：本機 `~/Code/GitHome/bevy` 6a911653f。

經過：
- 修訂 0（5f801e92）：照卡片寫。
- 修訂 1：修訂 0 送敵對式檢查（**Opus／high，作者與檢查者同為 Opus**：Fable 用量不足，主會話轉達使用者「暫時用Opus」）：blocker 1、should 10、nit 8，全部採用（對照在第 11 節）。報告：主 checkout `build/evidence/gyo2-sdl-extract/declaration-r0-adversarial.md`。檢查者沒有建置、沒有跑測試；B-1、S-1、S-4、S-6 卡片對原始碼核對過。最大的兩項：run 條件的 Extract 參數不受 D-E7 擋（B-1，本批補，改 `Registry.cpp`）；J4、J6 改成交使用者（第 10 節 Q1、Q2）。修訂 1 本身沒有再送複查。

---

## 0. 前提表

| # | 前提 | 誰定的 | 掛在上面的決定 |
|---|---|---|---|
| P1 | 渲染一類的 World 只經 Extract 唯讀讀來源，不寫回 | 使用者 11-11（K.3-6、DI-KS 13） | 2.1 的 `const`、2.4、第 3 節第 4、6 項 |
| P2 | Extract 只在同一條執行緒上的兩個 World 之間；兩個 World 都不在 Run 裡 | O-12；總宣告 6.1「執行緒」 | 2.1 的斷言、2.3 第 4 點 |
| P3 | extract schedule 寫在讀者的每幀清單，編排層看 `extract` 旗標改走 Extract 的入口；不在第一張不算錯 | 交換點宣告 5.4-2、總宣告 6.1-3（已核准） | 2.3 第 4 點 |
| P4 | Kernel 的原語是 `WorldExecutor::RunExtract(schedule, const World& source)`；屬 DI-KS 15 第 (3)、(4) 項、不是清單多一項；不加 `StartupCode`；最外層 `Run` 指名 extract schedule 是斷言 | 總宣告 6.1-1（已核准） | 2.1 |
| P5 | 讀者的描述帶 `extractFrom`；來源不存在、排在讀者之後是啟動錯誤 O-E9；編排層在讀者第一次 `BuildPlans` 之前呼叫 `SetExtractSource` | 總宣告 6.1-2（已核准） | 2.3 第 1～3 點 |
| P6 | 來源被替換的做法在 R1b 比、交使用者 | 總宣告 6.1-4 | 2.3 第 5 點、J3 |
| P7 | 錯誤只告訴外面發生了什麼；誤用不加守衛，由設計審查抓；守衛只守寫明的契約 | 使用者 11-20、11-21 | J2 的分界、Q2 |
| P8 | 實作薄起步：只做點名的 Feature 直近要用的，其餘只定形 | AGENTS §2 | 2.2 只做讀、2.4 只留形 |
| P9 | ECS 相關機制以 Bevy 為主參考；偏離寫衝突點與膠水量 | 使用者 11-23 | 第 3 節 |
| P10 | 改 Kernel 的三個子批建議 R1a 先合、KR 跟上、R1b 在 R1a 之後 | 總宣告 10.1（主會話排） | 第 7 節 |

## 1. 這一批做什麼

### 1.1 用遊戲講

KeepTool：文件 World 每幀跑完 ImGui 的 widget System，把 draw list 放進自己的一個 Resource。渲染 World 接著在同一幀跑，它的 extract schedule 裡有一個 System 用 `ExtractRes<DrawList>` 讀文件 World 的那份 draw list，抄成渲染 World 自己的資料（之後由 R2a 的 System 變成 `RenderCommands`）。文件 World 不知道有人讀它，渲染 World 也改不了它。

2D 遊戲（R2b）同理：模擬 World 跑完這一幀，渲染 World 用 `Extract<Read<Sprite>>`、`Extract<Read<GlobalTransform>>` 讀每個實體，在自己這邊畫。

### 1.2 用機制的詞

Kernel 今天只有 Extract 的「宣告那一半」：`Extract<Read<T>>`、`ExtractRes<R>` 參數（`Params.hpp:1318-1340`，寫的版本沒有 `ParamTraits`，`SignatureTests.cpp:110-123` 的 J-10 那一案檢查）、`ScheduleOptions::extract`、`Registry::SetExtractSource`（`Registry.cpp:671-675`，存來源的 `RegistryData*`）、來源沒設或沒建過 Plan 的 D-E14（`Registry.cpp:908-911`）、System 自己帶 Extract 參數只能掛在 extract schedule（D-E7，`Registry.cpp:1065-1067`；hook、observer 另有 D-E7，`:978`、`:1020`）。**執行沒有**：句柄沒有任何讀的方法，建構子在 `call.world` 不是空時斷言（`Params.hpp:1325`、`:1335`，舊的 rollback-extract 隨第 4 批拿掉）；最外層 `Run` 不擋 extract schedule；巢狀跑被 X-15 拒（`Executor.cpp:499-502`）。**還有一個洞**：run 條件帶的 Extract 參數經 `MergeCondition` 只併進 `usesExtract`（`Registry.cpp:834`，給 D-E14），D-E7 看不到——掛在非 extract schedule 的 System 帶一個有 `ExtractRes` 的條件，啟動會通過、執行時才撞句柄的斷言。編排層沒有呼叫 `SetExtractSource` 的地方。

這一批補執行：Kernel 的 `RunExtract`、Extract 參數讀得到來源、D-E7 也擋條件；編排層的 `extractFrom`、O-E9、清單與 `RunSchedule` 遇到 extract schedule 走 `RunExtract`。

**不做**：來源被替換（R1b）、渲染 World 的實體對應與「增刪了什麼」的實作（R2b）、`render_world` 把 extract schedule 插在 `Main` 之前（R2a）、渲染 World 的時間（W）。

## 2. 要做的

### 2.1 Kernel：`WorldExecutor::RunExtract`

```cpp
Engine::Base::Result<void, KernelError> RunExtract(std::string_view schedule, const World& source);
```

1. **一次 `RunExtract` 是讀者 World 的一次最外層 Run**，和 `Run` 走同一段：正在 Run 的計數（`RunningScope`）、接上 Registry 的新版本（`Attach`）、讀 `KernelSettings`、清這次 Run 的 `DroppedMessages` 紀錄、Ready 窗的比對、每一步、同步點、hook 輪、結束時修剪讀者自己的訊息、失敗時中斷讀者。差別只有一點：這次 Run 的 **Plan 步驟上**的 System 與它們的 run 條件，Extract 參數讀 `source`。extract schedule 的 System 寫讀者照常（`Commands`、`ResMut` 等），在讀者自己的同步點套用。
2. **來源只經 `const World&` 讀**：不寫來源的資料，也不動來源的 stamp、訊息游標、修剪與診斷紀錄——**除了接版本的簿記**：來源若還沒接上它 Registry 的新版本（DI-KS 26：來源動態插入之後還沒被用過），照 World 既有的 const 讀法接上（`World.cpp:70`），接版本會順帶清掉被拿掉的 System 的游標（`World.cpp:96-100`）。這是 World 自己的簿記，既有的 const 讀法本來就這樣做（Bevy 的 `SystemState::get` 也會推主 World 的 change tick，`function_system.rs:365-373`）。
3. **斷言**（Programmer Error；不加 `StartupCode`、不加 `KernelErrorCode`）：讀者是 Ready；schedule 存在且是 extract schedule；`source` 是由讀者 Registry 設的來源 Registry 建的 World（比 `RegistryData` 的身分），而且不是讀者自己；`source` 是 Ready（不在 Run 裡、沒中斷）。`Run` 指名 extract schedule 也改成在入口斷言（今天會跑到句柄建構子的斷言，訊息還寫著 rollback-extract），訊息指向 `RunExtract`。各有 `GYO_CHECK_ASSERTS` 的案（2.6）。
4. **巢狀**：`ExclusiveWorld::RunSchedule` 指名 extract schedule 照舊被拒（X-15 `ExtractNotNestable`，回 Err、讀者中斷）；只把訊息裡的「rollback-extract」改成「`RunExtract`」，碼與規則名不變。從 extract schedule 裡巢狀跑別的 schedule 時，內層 Plan 的 System 與條件**拿不到來源**（巢狀的 `RunPlan` 給空的來源）；能不能帶 Extract 參數由下一點的 D-E7 在啟動時擋。
5. **D-E7 也擋 run 條件**（B-1）：掛載點的條件或 set 的條件帶 Extract 參數，而那個掛載點的 schedule 不是 extract schedule，是 D-E7（`H-6`），和 System 自己帶的同一個碼與規則。這是收緊既有的啟動錯誤（以前通過、執行時斷言的，改成啟動時擋），不是新碼。既有突變 `rs-adv-extract-outside-extract-schedule` 的 `find` 會跟著改（只收緊，§14.4 事後報告）。
6. **Kernel 不保存來源 World**：來源 World 每次由呼叫的人給；Kernel 只記來源的 Registry（`SetExtractSource`，現有）。這是 R1b 能換來源的前提（DI-KS 37 草案）。
7. **驗證（A.6）**：只比讀者 World。來源在 extract 期間被寫，只可能經保留的指標或另一個 executor（誤用）；經保留的指標的寫入，**在來源開著 `accessVerification` 時**由來源下一次 Run 的 Ready 窗查到（`Executor.cpp:211`；DI-KS 2：release 預設關）。要不要另加 extract 期間的寫入偵測是 Q2。

實作的位置（給審查對照，不是承諾）：`WorldExecutor.hpp`／`Executor.cpp`（`RunExtract`；`Run` 與 `RunExtract` 共用一段；巢狀給空來源；X-15 的訊息）；`detail/Context.hpp`（`SystemContext` 加 `const World* extractSource`；const 版的 `ColumnAt`／`ResourceAt` 宣告）；`World.cpp`（const 版那兩個函式，各幾行，不碰同步點的套用）；`Params.hpp`（Extract 句柄）；`Registry.cpp`（D-E7 延伸到條件：條件併進掛載點時帶出「有 Extract 參數」，在掛載點的檢查一起看）。不改指紋（`ScheduleOptions::extract` 本來就在 Plan 指紋裡）；J-32 的 golden 不受影響。

### 2.2 Extract 參數的讀法

照 `Query<Read<T>>` 與 `Res<T>` 的唯讀那一半：

| 參數 | 方法 | 說明 |
|---|---|---|
| `Extract<Read<T>>` | `Entities()` | 來源裡有 T 的實體，EntityId 升冪（同 `Query`，W-2） |
| | `Count()`、`Contains(id)` | |
| | `Get(id) -> const T*` | 沒有時 `nullptr` |
| | `ForEach(fn(EntityId, const T&))` | 升冪 |
| `ExtractRes<R>` | `Present()`、`operator*`、`operator->` | 沒有值時取值是斷言（同 `Res`） |

- 讀到的是**呼叫 `RunExtract` 當下的來源狀態**：在 `RunFrame` 裡＝來源這一輪跑完、它的交換函式「出」也跑過之後（O-12）；`RunSchedule(讀者, extract)` 讀的是來源目前的狀態（runner 在兩個入口之間寫過的也在內）。
- 句柄只在那一次呼叫裡有效（W-4，現有的檢查照用：檢查讀者那一次呼叫的序號）。
- **不做**（P8）：多個 term 的 join（Bevy 的 `Extract<Query<…>>`）、`Changed`／`Added`、讀來源的 `Message`、讀來源的移除紀錄。join 用多個參數加同一個 id 的 `Get`；其餘見 2.4。

### 2.3 編排層

1. **描述欄位**：`WorldDescription::extractFrom`（`std::string`，空＝沒有來源）。
2. **O-E9**（新碼，列舉值 `OE9 = 9`，`ToString` 補；O-E8 不用，理由見總宣告 6.1-2）：
   - (a) `extractFrom` 指名的名字不在描述裡、是自己、或排在讀者之後（World 照描述的順序跑，來源要先跑完這一輪）。在 `Build` 第 1 步對 `description.worlds`（描述本身，不是已收下的 World）查。
   - (b) 一個 World 的每幀清單裡有 extract schedule，但那個 World 沒有 `extractFrom`；第一個 World 的開局清單裡有 extract schedule（開局清單跑在第一個 World，它前面沒有 World 可以當來源）。清單是單位在 `Build` 裡插的、`extract` 旗標要 Plan 建好才讀得到，所以 (b) 放在登記之後的清單檢查（和 O-E5 同一處）；Kernel 有錯時那一段整個跳過（現況）。替換時 (b) 經 `ReplaceRegistrationFailed` 帶出。
   - (b) 是描述的契約檢查（和 O-E5「清單裡的名字不是那個 World 的 schedule」同類：沒有來源時那一張 schedule 跑不了），不是對誤用的守衛（J2）。
   - **不算錯**：有 `extractFrom` 但清單裡沒有 extract schedule（Bevy 的 sub app 也可以有 extract 函式而不抽東西）；一個來源有多個讀者；讀者再當別人的來源（鏈）——Extract 只讀，沒有理由擋。
3. **登記時接來源**：讀者的登記在它的 `BuildPlans` 之前呼叫 `SetExtractSource(來源的 Registry)`。啟動時，來源的登記已在前面做完（來源排在前面；它的 Registry 還在登記結果裡、沒移到槽）；替換讀者時，用來源槽裡目前的 Registry。
   - **讀者不登記、只報一次的情形**（Q1 的建議 (a)）：(a) 不成立（O-E9 已報）；來源被 O-E6 丟掉（O-E6 已報，不另報 O-E9）；來源的 Kernel 登記失敗（來源的 `StartupReport` 已報）。三者讀者都 `notChecked`，不另報讀者的 D-E14。O-E3 只記錯、World 照樣登記（`Product.cpp:186-207`），所以來源有 O-E3 時讀者照常接來源。
   - 有 Extract 參數卻沒有 `extractFrom` 的 World：Kernel 的 D-E14（現有），編排層不另報。
4. **清單、`RunSchedule`、`beforeSwap`**：編排層跑一張 schedule 的地方共用一段：schedule 的 `extract` 旗標是真就 `RunExtract(schedule, 來源的 World)`，否則 `Run`。旗標從**那次要跑的 executor 的 Registry** 查（`beforeSwap` 時是新的 Registry，新的 executor 還不在槽裡）。
   - 讀者的交換函式「進」本來就在清單之前（O-12），不必改。
   - `RunSchedule(讀者, extract schedule)` 和清單走同一條路（J1；總宣告 6.1-3 留給 R1a 定）。`RunSchedule` 或 `beforeSwap` 指名 extract schedule 而那個 World 沒有來源：斷言（同「名字不存在」今天是斷言）。
   - 入口先看所有 World 健康（O-11，現有），所以跑到 `RunExtract` 時來源一定健康、不在 Run 裡。
5. **替換**：
   - 替換讀者：新的 Registry 照第 3 點接上來源目前的 Registry；`beforeSwap` 裡的 extract schedule 照第 4 點。
   - **替換有讀者的來源**：R1b 之前是斷言（J3）。「有讀者」＝描述裡有任何 World 的 `extractFrom` 指名它（鏈的中間那個也算）。斷言放在替換的最前面，`DropOldFirst` 丟掉舊 World（`Product.cpp:746`）之前。理由：`SetExtractSource` 存的是來源的 `RegistryData*`，換上新來源時舊的 Registry 銷毀、讀者的指標懸空；怎麼做（讀者一起重建／准許換來源）是 R1b 要比、交使用者的題，R1a 不先選。R1b 合併時拿掉這個斷言。R1a 到 R1b 之間沒有點名的產品會替換有讀者的來源（KeepTool 不替換 World，總宣告 6.1-4）。
   - 動態插入（`InsertSystems`）不換 Registry 物件（`Adopt` 在同一個物件裡換表，`Registry.cpp:1231-1243`），讀者的指標照舊有效，不必擋。
6. **銷毀**：讀者先於來源（O-15 的反序，現有）；讀者的 Registry 也先於來源的 Registry 銷毀（`~Product` 每個槽依序丟 World、Registry，照描述的反序）。`Build` 失敗時，還在登記結果裡的 Registry 改成明確照反序丟（今天交給 `std::vector` 的解構，順序依標準函式庫而定；`~Registry` 不碰來源指標，所以今天不出事，但條文要在每條路上成立，O-15「`Build` 失敗的路徑也一樣」）。
7. **生成世界（O-14）**：不加規則。來源在生成中時讀者照樣抽，讀到的是生成中的狀態；要等的話，渲染側用 `ExtractRes<WorldGeneration>` 讀來源的生成狀態自己判（`WorldGeneration` 每個 World 都登記，O-2）。

### 2.4 「上次 Extract 之後來源改了什麼」的形（只留形；R2b 定與實作）

總宣告 6.1-8 要 R1a 留這個讀法的形。三件事分開；R1a 只寫候選與限制，不替 R2b 選：

| 要知道的 | 候選的形 | Bevy 怎麼做 | GYO 的限制 |
|---|---|---|---|
| 新增、改過（`Added`／`Changed`） | 讀者的每個 extract 掛載點記「上次 extract 時來源的 stamp」（來源的 stamp 空間），比來源欄的 stamp；記在讀者這邊，不寫來源 | `Extract<P>` 內有自己對 main world 的 `SystemState`（`bevy_extract/src/extract_param.rs:58-62`），每次取參數時更新 `last_run`（`bevy_ecs/src/system/function_system.rs:456`），`Changed` 以上一次 extract 為準 | GYO 的 stamp 每個 World 各一套，讀者掛載點的 `lastRun` 在讀者的空間，不能直接拿來比來源 |
| 拿掉（實體刪了、元件拿掉） | (i) 讀者這邊以來源實體為鍵的表，每次比對；(ii) 來源保留移除紀錄、讀者在自己這邊記讀到哪裡（像 Bevy）；(iii) 來源側的 `Remove<T>` observer 寫進來源的 Resource（作者或 Module 的膠水）——**R2b 定** | `Extract<RemovedComponents<Mesh3d>>` 等（`bevy_pbr/src/render/mesh.rs:2002-2018`）：主 World 的移除紀錄要到所有 sub app extract 完才清（`clear_trackers`，`bevy_app/src/sub_app.rs:591`），讀的游標在 Extract 自己的 `SystemState`（讀者這邊）；實體對應另由 sync 寫回來源（`sync_world.rs`，和 DI-KS 13 不相容，總宣告 6.6） | GYO 沒有保留的移除紀錄；移除只有推送式的 `Remove<T>` observer，而且只在來源裡跑。(ii) 要改 Kernel |
| 來源的 `Message` | Extract 不讀；要給讀者的資料放成元件或 Resource | `Extract<MessageReader<…>>` 可以：訊息雙緩衝、活過兩次 update | 來源在自己最外層 Run 結束時修剪所有讀者都讀過的紀錄（DI-KS 10），Extract 在那之後才跑；跨 World 的讀者要拉住保留，就得在來源記游標＝寫來源 |

R1a 不實作任何一項。DI-KS 36 草案寫「沒有 `Changed`／`Added`、不讀來源的 `Message` 與移除紀錄」；R2b 要加時改那一條（RC 28 (b) 是重評的觸發）。

### 2.5 條文（草案；最終文字合併前核准）

**DI-KS 35.** **Extract 的執行。** `WorldExecutor::RunExtract(schedule, const World& source)` 是讀者 World 的一次最外層 Run、只跑 extract schedule：正在 Run 的計數、接版本、步驟、同步點、hook 輪、讀者訊息的修剪、失敗時的中斷都和 `Run` 一樣，差別只有這次 Run 的 Plan 步驟上的 System 與它們的 run 條件，Extract 參數讀 `source`；巢狀跑的內層拿不到來源。斷言：`Run` 指名 extract schedule；`RunExtract` 指名不是 extract 的 schedule；`source` 不是由讀者 Registry 設的來源 Registry 建的 World，或就是讀者；`source` 不是 Ready。巢狀跑 extract schedule 照舊被拒（X-15）。帶 Extract 參數的 System 或 run 條件掛在非 extract schedule 是 D-E7。不加啟動錯誤與執行時錯誤碼；屬第 (3)、(4) 項、不是清單多一項。守衛：XK-1、XK-2、XK-4、XK-6、XK-7 與斷言各案；J-56；突變 `ex-`。（`v8-arch-gyo2-sdl-extract`；系譜 Bevy `SubApp::extract`、`bevy_extract::extract`）

**DI-KS 36.** **Extract 參數讀呼叫當下的來源狀態。** `Extract<Read<T>>`（`Entities` 升冪、`Count`、`Contains`、`Get`、`ForEach`）與 `ExtractRes<R>`（`Present`、取值）經 `const World&` 讀；不寫來源的資料，也不動來源的 stamp、訊息游標、修剪與診斷紀錄——接版本的簿記除外（來源接上自己 Registry 的新版本，含清掉被拿掉的讀者的游標，照既有的 const 讀法）。沒有 `Changed`／`Added`、不讀來源的 `Message` 與移除紀錄（來源在自己的 Run 結束時已修剪，DI-KS 10）；要加時改這一條（候選在[事前宣告](sdl-extract-declaration.zh-Hant.md) 2.4）。守衛：XK-1、XK-2、XK-3、XK-5；J-10 的 `Extract<Write<T>>` 不合法（`SignatureTests.cpp`）。

**DI-KS 37.** **Kernel 不保存來源 World。** 讀者的 Registry 只記來源的 Registry（`SetExtractSource`，在第一次 `BuildPlans` 之前）；來源 World 每次由呼叫的人給 `RunExtract`。呼叫的人的規則（Kernel 看不到）：讀者的 Registry 在登記、建 Plan 與 `RunExtract` 期間，來源的 Registry 要活著；讀者的 Registry 不比來源的晚銷毀；`RunExtract` 期間不寫來源（同一條執行緒、來源不在 Run 裡）。編排層照 O-12、O-15 做（`Build` 失敗的路徑也照反序丟）。守衛：XO-6（銷毀先後）；審查。

**RC 28.** **Extract 要寫回、要動兩個 World，或參數不夠**（草案）：(a) 某個點名的 Feature 需要 Extract 寫回來源（GPU 讀回進 World、照 Bevy 把對應實體寫回來源），或同步那一步要同時改兩個 World（R2b 的實體對應）——DI-KS 13 要改，停下交使用者；(b) 兩個以上的 Module 或產品為了在 extract 裡過濾、join、知道移除、或記「上次抽到哪」，在渲染 World 抄整份來源或各自寫比對——重評 `Extract<Query<…>>` 與 2.4 的候選。

**DI-KS 13 的守衛欄**：今天寫「J-43」，規格的 J-43 是「來源的位元組與 stamps 不變；寫入斷言；來源未凍結先建目標＝啟動錯誤；`Extract<Write>` 編譯失敗」（`plans/kernel/spec.zh-Hant.md:1116`），但 gyo2 上沒有叫 J-43 的測試。照 Q2 改：(a) 改成「XK-2（來源不被動）、D-E14 的案、J-10 的 `Extract<Write<T>>` 不合法」，拿掉「寫入斷言」那一半；(b) 照 J-43 加寫入斷言與它的案。條文本身不變。

**DI-KS 26**：「正在 Run 的 World……由 `WorldExecutor::Run` 進入時加」「下一次被用到時（World API、`WorldExecutor::Run`）自己接上」與斷言裡的 `WorldExecutor::Run`，都改成「`Run`／`RunExtract`」。行為不變（共用同一段）。

**編排層**（`orchestration.zh-Hant.md`）：
- O-5：「`OrchestrationCode`（O-E1～O-E7）」→「（O-E1～O-E7、O-E9；O-E8 不用）」；若 Q1 選 (a)，補一句「來源沒登記成的讀者標沒檢查，和 O-15 的 `Configure` 回報同類」。
- O-8 末句「做法在 SDL 那批定」→「R1b 定做法之前，替換有讀者的來源是斷言（`v8-arch-gyo2-sdl-extract`）」。
- O-12 的 Extract 那一項，照 2.3 寫成：讀者的描述 `extractFrom` 指名一個排在前面的 World（O-E9）；編排層在讀者第一次 `BuildPlans` 之前接上來源的 Registry；清單、`RunSchedule`、`beforeSwap` 遇到 extract schedule 經 Kernel 的 `RunExtract` 跑（DI-KS 35）；讀者的交換函式「進」在它之前；extract schedule 不在清單最前面不算錯，在它之前跑的 schedule 讀到上一輪抽的資料。守衛欄加 `orchestration.extract` 各案與突變 `or-ex-`。
- O-15：「誰比誰活得久」補一句讀者的 Registry 不比來源的晚銷毀（`Build` 失敗的路徑也一樣）。
- O-4 不改（「extract 的時機」已在職責清單）。

**說明頁**（`system-contract-page.zh-Hant.md` 第 62 列附近）：Extract 參數的讀法、讀到的是呼叫當下的來源狀態、run 條件也可以帶（只在 extract schedule）、不讀 `Message` 與移除紀錄、沒有 `Changed` 的後果、在 extract 之前跑的 schedule 讀到上一輪抽的資料。

### 2.6 測試（驗收的骨幹）與突變

測試代號用 `XK-`（Kernel）、`XO-`（編排層），避開編排層條文 O-n 與 KR 的 Kn。

**Kernel**（新檔 `tests/common/kernel/ExtractTests.cpp`，CPU 標籤，照 kernel 既有的分組）：
- **XK-1 讀到來源的值**：來源與讀者都登記 T 與 R（避免讀錯 World 時先撞「沒登記」的斷言），兩邊放不同的值；來源的 Run 寫值，讀者 `RunExtract` 的 System 讀到來源的值。來源再跑、改值，再 extract 讀到新值。
- **XK-2 來源不被動**：`RunExtract` 前後，來源的 `World::Summary`、讀到的欄的 stamp、來源的訊息游標與紀錄筆數都相同；讀者 extract System 經 `Commands`／`ResMut` 寫讀者，在讀者的同步點套用。
- **XK-3 讀法**：`Entities` 升冪（來源 spawn 的順序打亂）、`Get` 沒有時 `nullptr`、`Count`、`Contains`、`ForEach`；`ExtractRes::Present` 在來源沒有值時是 false。
- **XK-4 run 條件帶 Extract 參數**（在 extract schedule 上）：條件讀來源決定跑不跑。
- **XK-5 來源接上新版本**：來源動態插入一種新元件、還沒再跑過；讀者再登記一輪加讀它的 extract System；`RunExtract` 讀得到（空的欄，`Count() == 0`），不讀到不存在的欄。接續 `RegistrationRoundTests.cpp` 的「DI-KS 13: an extract source registers again …」（那一案只驗登記）。
- **XK-6 和 `Run` 共用的那一段**：`RunExtract` 結束時修剪讀者自己的訊息（讀者有讀過的紀錄被修剪）；`KernelSettings` 在 `RunExtract` 開始時讀（System 改的值下一次才算）；`DroppedMessages` 每次 `RunExtract` 自己一份。
- **XK-7 D-E7 擋條件**：非 extract schedule 上的 System 帶一個有 `ExtractRes` 的掛載點條件、或 set 條件 → D-E7（`H-6`）；同樣的條件在 extract schedule 上通過（J-24 的形式）。
- **斷言**（`GYO_CHECK_ASSERTS`，`tests/common/support/AssertTestSupport.hpp`）：`Run` 指名 extract schedule；`RunExtract` 指名非 extract schedule；`source` 是別的 Registry 建的 World；`source` 就是讀者；`source` 不是 Ready（中斷的來源）。
- 現有照留：J-56（X-15）、J-10 的 `Extract<Write<…>>` 不合法、J-24、D-E14 的突變。

**編排層**（新檔 `tests/common/orchestration/ExtractTests.cpp`）：
- **XO-1 每幀**：來源先跑、讀者的 extract 讀到來源這一幀（含它的「出」之後）的值；讀者的交換函式「進」在 extract 之前（記錄順序）。
- **XO-2 O-E9**：指名不存在的 World、指名自己、指名排在後面的 World、每幀清單有 extract schedule 卻沒有來源、開局清單有 extract schedule，各一案。
- **XO-3 讀者不登記、只報一次**：來源的 Kernel 登記失敗；來源被 O-E6 丟掉；O-E9 (a)。三案都只報那一個錯，讀者 `notChecked`、沒有 D-E14、沒有多一個 O-E9（照 Q1 的選擇改）。
- **XO-4 `RunSchedule(讀者, extract)`**：讀到來源目前的值（runner 在入口之間寫過的也讀到）。
- **XO-5 替換讀者**：`BuildNewFirst`、`DropOldFirst` 之後照讀；`beforeSwap` 指名 extract schedule 讀得到來源。
- **XO-6 銷毀**：讀者的 World 先於來源的 World 銷毀（兩邊各放一個解構時記錄的 Resource）；`Build` 失敗的路徑同。
- **XO-7 一個來源兩個讀者、鏈**：都讀得到。
- **斷言**（`GYO_CHECK_ASSERTS`）：替換有讀者的來源（`BuildNewFirst`、`DropOldFirst` 各一，`DropOldFirst` 時舊 World 還在；鏈的中間那個也擋）；`RunSchedule` 指名 extract schedule 而 World 沒有來源。

**突變**（D184：新的全跑；既有突變中 `find` 落在改動範圍的也跑：`ns-extract-nestable`（同一個 `if` 的訊息改了）、`rs-adv-extract-outside-extract-schedule`（`find` 跟著 B-1 改）、`ks-s2-extract-source-after-build`）。草案，實作時照 §14.5 收斂（每個只捕一種別的突變捕不到的錯），清單送敵對式檢查（DI-KS 13 的守衛，§14.2）：

| id（草案） | 改動 | 預期的失敗 | 守的（guards） |
|---|---|---|---|
| `ex-reads-reader` | Extract 句柄讀讀者 World 而不是來源 | XK-1 那條比值的 CHECK | DI-KS 36 |
| `ex-trims-source` | 共用段在結束時把修剪套到來源上 | XK-2 那條比游標與筆數的 CHECK | DI-KS 13、36 |
| `ex-entities-unsorted` | `Entities` 照欄的存放順序反過來 | XK-3 | DI-KS 36 |
| `ex-res-present-inverted` | `ExtractRes::Present` 反過來 | XK-3 | DI-KS 36 |
| `ex-source-after-conditions` | `extractSource` 在條件評估之後才設 | XK-4（句柄的斷言訊息） | DI-KS 35 |
| `ex-extract-not-trimmed` | `RunExtract` 不走結束時的修剪 | XK-6 | DI-KS 35 |
| `ks-de7-condition-skipped` | D-E7 不看條件 | XK-7 | DI-KS 35（H-6） |
| `ex-run-accepts-extract` | `Run` 入口不斷言 extract schedule | 斷言案（`expected GYO_ASSERT to fail`） | DI-KS 35 |
| `ex-source-identity-unchecked` | 不比來源的身分 | 斷言案 | DI-KS 35 |
| `or-ex-source-not-set` | 登記時不呼叫 `SetExtractSource` | XO-1（啟動失敗，D-E14） | O-12 |
| `or-ex-oe9-unknown`、`-self`、`-later`、`-list`、`-gameinit` | O-E9 的五個條件各拿掉一個 | XO-2 那一案 | O-12（O-E9） |
| `or-ex-runon-plain-run` | 跑 schedule 時不看 `extract` 旗標 | XO-1（`Run` 入口的斷言訊息） | O-12 |
| `or-ex-reader-registered-anyway` | 來源沒登記成時照樣登記讀者 | XO-3（多一個 D-E14） | O-5（Q1） |
| `or-ex-replace-source-allowed` | 不擋替換有讀者的來源 | XO 斷言案 | O-8 |
| `or-ex-replace-chain-middle` | 只擋鏈的末端讀者的來源 | XO 斷言案（鏈的中間） | O-8 |

預期的失敗是斷言時，`expect` 用那條測試的固定字串（§14.2：可識別）；照 `tests/common/mutation/README.md` 第 9 點填 `guards`。

## 3. 和 Bevy 的對照（P9；逐項）

| # | 項 | Bevy | GYO（R1a） | 衝突點 | 膠水量 | 處置 |
|---|---|---|---|---|---|---|
| 1 | 讀者與來源怎麼宣告 | `SubApp::set_extract(fn)`（`bevy_app/src/sub_app.rs:174`），`ExtractPlugin` 設成 entity sync＋跑 `ExtractSchedule`（`bevy_extract/src/extract_plugin.rs:83-94`） | 描述欄位 `extractFrom`＋O-E9 | GYO 的 World 由描述組（O-4），沒有 sub app 物件 | 一個欄位、一個錯誤碼 | 照 GYO 的描述；接合點：一個 Bevy sub app＝一個有 `extractFrom` 的 World |
| 2 | 時機 | `SubApps::update`：main 跑完 → 每個 sub app 先 `extract` 再 `update`（`sub_app.rs:575-590`） | World 照描述的順序跑、來源在前；extract schedule 在讀者的清單裡，預設由 `render_world` 插在 `Main` 之前（R2a） | 位置可變（P3）；來源的交換函式「出」在讀者的 extract 之前 | 0 | 預設排法同效果 |
| 3 | Extract 參數 | `Extract<P>` 包任何唯讀 `SystemParam`，內有自己對 main world 的 `SystemState`（`extract_param.rs:51-62`） | 只有 `Extract<Read<T>>`、`ExtractRes<R>` | **偏離**（薄起步，P8）：沒有多 term 的 join、沒有 `Changed` | join 用多個參數＋`Get`；`Changed` 見 2.4 | RC 28 (b) 是重評的觸發 |
| 4 | 來源怎麼交給讀者 | 把 main world `mem::replace` 成 `MainWorld` Resource 放進 sub world，跑完換回（`extract_plugin.rs:136-147`）；`Extract` 經 `Res<MainWorld>` 讀，只讀靠型別；`SystemState::get` 會推 main 的 change tick（`function_system.rs:365-373`） | `const World&` 參數；Kernel 不保存（DI-KS 37）；接版本的簿記照 const 讀法 | 無 | 0 | 同方向（只讀靠型別），不必搬 World |
| 5 | extract schedule 的延後變更 | 關掉自動 `apply_deferred` 與最後一次套用（`extract_plugin.rs:60-66`），留到 sub world 的 schedule 裡 `apply_extract_commands`（`:113-121`），為了 pipelined 時讓 main 先走 | 在 extract schedule 自己的同步點套用（一般的 schedule） | **偏離** | 0 | 不開 render 執行緒（master D27、總宣告 6.1「執行緒」）；日後開時再看 |
| 6 | 寫回來源 | `MainWorld` 可以 `ResMut` 寫回；entity sync 在 extract 前寫 main（`extract_plugin.rs:86-90`） | 不寫回（P1） | 實體對應的寫回（總宣告 6.1-8、6.6） | 見 2.4 | RC 28 (a) |
| 7 | 讀來源的 `Message` | 可以（訊息雙緩衝，活過兩次 update） | 不讀（2.4） | **偏離**：來源在自己的 Run 結束時修剪（DI-KS 10） | 要給讀者的放成元件或 Resource | 告知（J5） |
| 8 | 讀來源的移除紀錄 | `Extract<RemovedComponents<T>>`；main 的 `clear_trackers` 排在所有 sub app 的 extract 之後（`sub_app.rs:591`），游標在讀者那一邊 | 不讀；GYO 沒有保留的移除紀錄，只有來源裡的推送式 `Remove<T>` observer | **偏離**（機制不同，不是只缺 API） | R2b 定（2.4 的三個候選） | 告知；R2b 的宣告比 |
| 9 | 只跑 extract | `update_subapp_by_label`＝extract＋update（`sub_app.rs:604-609`）；直接 `run_schedule(ExtractSchedule)` 會因沒有 `MainWorld` 而 panic | `RunSchedule(讀者, extract)` 單跑 extract schedule、自動給來源 | 小偏離 | 0 | `RunSchedule` 是單一 schedule 的入口（O-1）（J1） |
| 10 | extract 期間來源被寫的偵測 | 靠借用規則（`&mut World` 被搬走） | const 型別擋 API 的寫；經保留的指標的寫，在來源開著驗證時由它下一次 Run 的 Ready 窗查 | 規格的 J-43 寫了「寫入斷言」 | 見 Q2 | Q2 |

## 4. 卡片照 L17 判的（可以推翻）

| # | 判的 | 比過的 | 理由 |
|---|---|---|---|
| J1 | `RunSchedule(讀者, extract schedule)`、`beforeSwap` 和清單走同一條路（有來源就 `RunExtract`） | 斷言（只准在清單裡跑）；回 `OrchestrationError` | 總宣告 6.1-3 留給 R1a。同一張 schedule 換個入口行為不同是陷阱；測試與工具要單跑一張 schedule 時也要有來源。Bevy 單跑會 panic，那是因為 `MainWorld` 只在 extract 函式裡存在，不是設計上要禁 |
| J2 | O-E9 (b)：清單有 extract schedule 但 World 沒有來源＝啟動錯誤 | 跑到時才斷言 | 描述自相矛盾，啟動時就看得出來，和 O-E5 同類；不是對誤用加守衛（沒有來源，那張 schedule 根本跑不了） |
| J3 | 替換有讀者的來源：R1b 之前是斷言 | 回新的 `OrchestrationError` 碼（R1b 會拿掉，留下只活一批的碼）；R1b 併進 R1a（R1b 有交使用者的題、要改 DI-KS 13、26 與 RC-O13，會拖慢 K 的路） | R1a 不先選 R1b 的做法；斷言最小、不留公開契約；R1a 到 R1b 之間沒有產品會走到 |
| J5 | 沒有 `Changed`／`Added`、不讀來源的 `Message` 與移除紀錄 | 現在就做 2.4 的某一列 | 點名的 Feature 直近不用（K 的 draw list 每幀重生）；候選已留（2.4），R2b 選 |
| J7 | Kernel RC 用 28 | — | 總宣告 10.1 原分配是 KR 25～27、R1a／R1b 28～29；25 被摘要那一批用掉，KR 改用 26、27，不影響 28。**請主會話確認** |
| J8 | D-E7 延伸到 run 條件（B-1） | 不改、讓條件在執行時撞斷言 | H-6「Extract 參數只在 extract schedule」本來就是啟動錯誤，條件漏掉是現有的洞；只收緊、不加碼 |

（修訂 0 的 J4、J6 改成第 10 節的 Q1、Q2。）

## 5. Architecture Delta（差分；七項在總宣告第 11 節）

- **公開介面**：Kernel 新增 `WorldExecutor::RunExtract`；`Extract<Read<T>>`、`ExtractRes<R>` 有了讀的方法；D-E7 多擋 run 條件（收緊既有的啟動錯誤）。編排層新增 `WorldDescription::extractFrom`、`OrchestrationCode::OE9`。
- **Dependency**：沒有新的邊（編排層本來就依賴 Kernel）。
- **Ownership**：不變。Kernel 的執行原語屬 DI-KS 15 第 (3)、(4) 項；編排層的 extract 時機屬 O-4 已列的「extract 的時機」。
- **Data Contract**：沒有。
- **Design Invariants**：DI-KS 35～37、RC 28 新增；DI-KS 13 的守衛欄、DI-KS 26 的「Run」改字；O-5、O-8、O-12、O-15 改句（第 2.5 節）。
- **既有判定**：`rs-adv-extract-outside-extract-schedule` 的 `find` 改（只收緊，沒有緩的面；§14.4 事後報告 1～5）；DI-KS 13 守衛欄照 Q2 改。
- **和總宣告的差分**：多改 `Registry.cpp`（D-E7）；沒有超出計畫的 Module、Product 或依賴。

## 6. 範圍外與範圍的限界

- R1b（來源替換）、R2a（`render_world` 插 extract）、R2b（實體對應、2.4 的實作）、W（渲染 World 的時間）。
- 不量測、沒有預算判定（Extract 的幀時間在 R2a、K 量）。不是計測的批，沒有件數的突き合わせ。
- R1a 合併到 R2a 之間，Extract 只有測試在用，沒有產品用。

## 7. 和其他卡的協調（經主會話）

- **KR**（`local_e2d05fe7`，分支 `claude/gyo2-kernel-hierarchy-06b6eb`）：都改 Kernel。R1a 改 `Executor.cpp`（`Run` 的入口分出共用段、巢狀給空來源、X-15 的訊息）、`World.cpp`（const 版 `ColumnAt`／`ResourceAt`，不碰同步點的套用）、`Registry.cpp`（D-E7 看條件）、`Params.hpp`、`detail/Context.hpp`、`WorldExecutor.hpp`、`kernel.zh-Hant.md`（DI-KS 35～37、RC 28、DI-KS 13 的守衛欄、DI-KS 26 的改字、更新行）、`mutations.json`。KR 改同步點的套用（`World.cpp`）、`Executor.cpp`、DI-KS 9、12、15 等。條號不重疊（KR：DI-KS 29～33、34 保留、RC 26、27）。照總宣告 R1a 先合；誰後合併誰 fetch＋merge gyo2（不 rebase）。
- **R1b**：拿掉 J3 的斷言；以 DI-KS 37 為前提。
- **K、R2b**：用 2.2 的 API；R2b 從 2.4 的候選選。
- **O-E9**：號已在總宣告保留；通知並行的卡由主會話。

## 8. 分工、檢查與估時

- **上段**（Kernel 與編排層的契約，navigator）。
- **實作者** Opus／high；**審查者** Opus／high（另一個 agent；Fable 用量不足期間作者與審查者同模型，報告寫明）；本批沒有緩的面（§14.4），不需要驗證者。
- **本宣告的敵對式檢查**：修訂 0 做過（Opus／high，同模型）。**突變清單的敵對式檢查**：Opus／high。
- **步驟**：(1) Kernel 的 `RunExtract`、Extract 句柄、D-E7、XK 各案 → (2) 編排層、XO 各案 → (3) 文件（第 2.5 節）→ (4) 突變（D184）與清單的敵對式檢查 → (5) 審查 → dev_log、draft PR 到 `gyo2`、手動 dispatch L1。合併等使用者明說「合併」。
- **估時（推定）**：Kernel 6～11 h（含 D-E7）、編排層 5～9 h、文件 2～3 h、突變與檢查 3～5 h、審查與修正 2～4 h，合計 **18～32 h**。超過估計 1.5 倍就停下回報。

## 9. 動工前核准清單

1. 範圍（第 2 節）與不做的（2.2「不做」、2.4、第 6 節）。
2. 卡片判的 J1～J3、J5、J7、J8（第 4 節）。
3. 第 10 節 Q1、Q2。
4. 條文草案的方向：DI-KS 35～37、RC 28、DI-KS 13 守衛欄、DI-KS 26 的改字、O-5／O-8／O-12／O-15 改句（最終文字在合併前核准）。
5. 條號：RC 28（J7，主會話確認）。

## 10. 要使用者定的

### Q1：來源沒登記成時，讀者怎麼辦（修訂 0 的 J4）

**例子**：渲染 World 的 `extractFrom` 指名模擬 World，模擬 World 有一個 System 少登記了元件（Kernel 的 R-E4）。

| 選項 | 做法 | 好處 | 代價 |
|---|---|---|---|
| **(a)** 讀者不登記，標沒檢查（建議） | 只報來源的錯（或 O-E6、O-E9 (a)）；讀者 `notChecked` | 一個錯只報一次；不出現「來源沒設」這種連帶的 D-E14 | 讀者自己和來源無關的錯（例：讀者也有 R-E4），要等來源修好、下一次啟動才看得到——是 O-5「一次收齊」的例外 |
| (b) 讀者照樣登記、不接來源 | 讀者照常登記；它若有 Extract 參數，Kernel 報 D-E14 | O-5 照字面收齊，讀者自己的錯這一次就看得到 | 多一個連帶的 D-E14，讀的人要自己分辨哪個是根 |

建議 (a)：前例是 O-15「有任何 `Configure` 回報就不跑任何 World 的 `Build`～`Check`，每個 World 標沒檢查」——同樣是「前面壞了，後面的不檢查」。要寫進 O-5 一句（第 2.5 節）。

### Q2：extract 期間來源被寫，要不要偵測（修訂 0 的 J6）

**例子**：渲染 World 的 extract System 經作者留著的指標，呼叫了模擬 World 的 `World::Insert`。

**事實**：DI-KS 13 的守衛欄寫「J-43」；規格的 J-43 是「來源的位元組與 stamps 不變；**寫入斷言**；來源未凍結先建目標＝啟動錯誤；`Extract<Write>` 編譯失敗」（`plans/kernel/spec.zh-Hant.md:1116`）。gyo2 上沒有叫 J-43 的測試；`Extract<Write>` 的檢查在 J-10。

| 選項 | 做法 | 好處 | 代價 |
|---|---|---|---|
| **(a)** 不加執行期偵測（建議） | DI-KS 13 守衛欄改成「XK-2（來源不被動）、D-E14 的案、J-10 的 `Extract<Write<T>>` 不合法」，拿掉「寫入斷言」那一半 | 合法的路已由 `const` 在編譯時擋住；剩下的只有經保留指標或別的 executor 的誤用，照「誤用不加守衛、由審查抓」；經保留指標的直接寫入，來源開著驗證時下一次 Run 的 Ready 窗查得到 | 經來源 World API 的寫（例子那種）查不到 |
| (b) 照 J-43 加寫入斷言 | `RunExtract` 期間把來源標成「被讀中」，來源的 World API 寫入與 `Run` 斷言；加 `GYO_CHECK_ASSERTS` 案與突變 | 守住 DI-KS 13 字面上的「無反饋」，連經 API 的寫都擋 | World 多一個狀態與每個寫入口的檢查（接版本的簿記要排除）；約多 1～2 h；是對誤用加的守衛 |

建議 (a)。理由：DI-KS 13 是寫明的契約，但它的合法路徑已由型別守住；(b) 擋的是只有繞過型別才走得到的路（P7）。

## 11. 修訂 0 的敵對式檢查：處理對照

| # | 要點 | 處理 |
|---|---|---|
| B-1 | D-E7 擋不到 run 條件；巢狀時條件拿不拿來源未定 | 採用：2.1 第 4、5 點、J8、XK-7、突變 `ks-de7-condition-skipped`、第 7 節加 `Registry.cpp` |
| S-1 | 有 `GYO_CHECK_ASSERTS`，「沒有 death test」錯 | 採用：2.6 斷言各案、突變 `ex-run-accepts-extract` 等；第 6 節拿掉那句 |
| S-2 | O-E3／O-E6 的描述錯；O-E9 在哪查 | 採用：2.3 第 2、3 點 |
| S-3 | J4 和 O-5 收齊有張力 | 採用：改成 Q1，引 O-15 前例 |
| S-4 | J6 其實拿掉 J-43 的一半 | 採用：改成 Q2 |
| S-5 | 「這一輪跑完」不準 | 採用：2.2、DI-KS 36 標題 |
| S-6 | Bevy 的移除靠 `RemovedComponents`；先替 R2b 下了結論 | 採用：2.4 改成候選、第 3 節第 8 列 |
| S-7 | 漏改 DI-KS 26 | 採用：2.5 |
| S-8 | DI-KS 37 在 `Build` 失敗路徑不成立 | 採用：條文改寫＋`Build` 失敗時照反序丟（2.3 第 6 點） |
| S-9 | 突變與測試的缺口 | 採用：XK-6、`ex-trims-source`、`ex-source-after-conditions`、O-E9 五個突變、J3 的位置與鏈、`guards` 欄、XK-1 兩邊都登記 |
| S-10 | 測試代號撞名 | 採用：`XK-`、`XO-` |
| N-1～N-8 | 游標的簿記、驗證預設關、`RegistryData*`、J-10、`OE9 = 9`、Bevy 的 change tick、`beforeSwap`、§14.4 | 全部採用（2.1、2.3、2.5、第 3 節、第 5 節） |
