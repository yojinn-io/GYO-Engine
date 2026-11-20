# 事前宣告：權威 Module＋回滾 Module（`v8-arch-gyo2-authority-rollback`）

更新：2026-11-20（修訂 1；同日記入使用者定的第 12 節第 1～4 件、「不參考 master」、第 5 件的拆卡、規模的處理、A1～A7、A10 的方向、預算的單位與深度的根據）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：等動工前核准。** 核准之前不實作、不改條文與守衛。

經過：修訂 0（6e20758a）送 Fable 5.2／high 敵對式檢查：必改 10、建議改 11、小問題 9（報告：主 checkout `build/evidence/gyo2-authority-rollback/declaration-r0-adversarial-check.md`）。修訂 1 全部照改，處理對照在第 17 節。修訂 1 沒有再送檢查。

性質：實作段第 4 批的事前宣告（AGENTS §3 的 Architecture Delta、§2 的基盤 Architecture、§14.4 的判定變更、Design Invariants 的變更）。11-17 核准的[權威份](authority-module-declaration.zh-Hant.md)（修訂 5 補，7f9bbb85）與[回滾份](rollback-module-declaration.zh-Hant.md)（修訂 6，9418fcf1）是設計的依據，**本頁不改它們定過的事**；和它們字面不同的地方全部列在第 12 節末。本頁只寫：(1) 兩份寫在第 2、3 批之前，和現在的程式逐條對照；(2) 兩份留給實作批的細節；(3) `World::Summary` 改逐型別雜湊；(4) 拿掉 Kernel 的權威規則；(5) 拆卡、步驟、判定。

用詞：「設計段／實作段」「動工前核准／合併前核准」「第 N 步」照第 3 批。標「**本卡照 L17 判**」的是這張卡的判斷（名字照命名規範、細節照別家原始碼），**不是使用者的話**。本卡新加的詞第一次出現時標「本卡加的詞」。

---

## 0. 根據與讀的版本

**程式**：`gyo2` 59fd4da9（#253 之後）。講現況的句子來自讀程式的偵察（Opus 5.5／high，主 checkout `build/evidence/gyo2-authority-rollback/`）：
- `recon-a-kernel-removal.md`：Kernel 要拿掉、搬走的程式、測試、守衛、突變（腳本與分類資料在 `recon-a/`）。
- `recon-b-primitives-interfaces.md`：回滾要的原語、唯讀登記表、`World::Summary`、`world_time`、編排層的現況。
- `recon-c-reference-engines.md`：lightyear 736e1695、Bevy 6a911653f、flecs 09790b5f7、bevy_replicon d80c849、Unity Entities 1.5.0（4630ff5）、Unity Netcode 7.2.0（c4cabf4）。
- 檢查者另外查的（`declaration-r0-adversarial-check.md` 末節）：`has_unique_object_representations` 的行為（本機 clang 實測）、比摘要測試的標註數、`InitialValueTests` 的漏網案。

**文件**：

| 文件 | 狀態 | 本頁怎麼用 |
|---|---|---|
| [批次表](rebuild-batch-plan.zh-Hant.md)第 4 列；「主會話自己判的」Module 的預設 | 主會話 | 範圍；4.1、5.1 |
| 權威份（修訂 5 補）、回滾份（修訂 6） | **已核准**（`v8-arch-gyo2-five-declarations-approved`） | 內容不改；第 2 節逐條對照 |
| [統合](kernel-app-boundary-declaration.zh-Hant.md)修訂 38：L1～L30、第 3.1～3.5、7.2、8、9.1、15、16 節與附錄 | 已核准 | 拿掉的清單、DI-KS 的改寫、線內先後 |
| [游標份](event-cursor-declaration.zh-Hant.md)修訂 6、[讀檔份](restore-entry-declaration.zh-Hant.md)修訂 11 | 已核准 | 訊息不回滾、跨 schedule 的訊息檢查；讀檔與回滾的交界 |
| [第 3 批宣告](time-events-declaration.zh-Hant.md)修訂 4 與 [dev_log](../../../dev_logs/2026_11_20_gyo2_time_events.zh-Hant.md) | 已合併（#253） | `Tick`、`Resimulation`、`RunOneFixedStep`、`RewindFixedSteps`、`TickWriters`、預設骨架、G4、G5 |
| [編排層骨架的宣告](orchestration-skeleton-declaration.zh-Hant.md)與 [O-1～O-11](orchestration.zh-Hant.md) | 已合併（#244） | 登記單位、選項包、`Check` |
| HANDOFF 決定表（`origin/claude/gyo2-time-design`，讀到 57c8dc32） | 主會話 | 使用者的決定。動工時再讀一次（`verify-before-premise`） |
| [限制頁](../limits.zh-Hant.md) LIM-7、LIM-8 | 架構師 | 第 3.4、8 節 |

**使用者的決定**（原話以 HANDOFF 為準）：`-kernel-is-ecs-no-restrictions`、`-authority-is-game-design`、`-authority-strategy-mechanism`、`-authority-own-registration`、`-authority-no-writer-count`、`-confirm-derived-removals`（含 11～13）、`-rollback-module-strategy`、`-strategy-interface-impl`、`-immediate-change-via-exclusive-world`、`-no-runtime-interception`、`-no-ambiguity-detection`、`-dynamic-insert-per-world`、`-module-deps-in-registers`、`-products-pick-modules`、`-engine-not-rigid`、`-layer-duty-lists`、`-events-two-kinds-like-bevy`、`-event-delivery-rule`、`-authority-guard-gap-later`、`-guards-after-mechanism-stable`、`-scenario-tests`、`-scenario-failure-by-contract`、`-scenario-cross-engine-check`、`-summary-padding-a-then-c`、`-mutation-guards-field`、`-confirm-only-differences`、`-batch3-no-xhigh`、`-net-is-mechanism-first`、`-merge-needs-explicit-word`。

**`-summary-padding-a-then-c` 的範圍**（修訂 1 更正）：使用者的原話是「先選A，但是把方案C記下來。這個case表為未完全解決，解決方案用C」。HANDOFF 記「原始位元組雜湊的三種毛病（padding、指標或 heap、+0／-0）」是 **side chat 討論時列的**，使用者沒有另外說。所以本頁把「逐型別雜湊」當使用者定的方向；三種毛病各要不要解、怎麼解，照第 3 節與第 12 節。

---

## 1. 這一批做什麼（先用遊戲講）

**PvP 射擊**，一個 Product 裡放兩個 World 做測試（之後由網路 Module 分到兩台機器）：

- **Match 的 World**：建 World 時選「伺服器權威」策略、不選回滾策略。作者寫 `authority.Component<Health>`、`authority.Component<Position>`、`authority.Mark<Tick>()`。啟動時策略檢查：World 宣告了不准動態插入；除了時間 Module 的 `RunFixedSteps`，沒有別的 System 寫 `Tick`。執行中不攔任何寫入。
- **Client 的模擬 World**：也選伺服器權威，再選官方的回滾策略。它多登記一個畫面用的 `HitFlash`（一般的 `registrar.Component`），所以兩邊的**一致性指紋**（權威份的 C 指紋；名字見 4.3）只算判定資料、照權威登記的順序編號，兩邊相同、連得上。
- Client 預測第 100～106 個 tick。測試把 Match 第 100 個 tick 的 `Position` 交給回滾 Module 的入口：和 Client 的歷史不同。回滾的獨占 System（掛在 `Main` 的 `PreUpdate`）立起 `Resimulation`、把 `Health`、`Position` 與存活集合**一批**放回第 100 個 tick（全部放回之後才跑 hook 與 observer）、用時間 Module 的 `RewindFixedSteps` 退 tick、再用 `RunOneFixedStep` 重跑 6 步，放下 `Resimulation`。「播腳步聲」掛了「重跑時不跑」，重跑時不出聲。重跑完 Client 和 Match 的 **digest**（判定資料的雜湊）相同。
- **單機 2D 橫版**：World 不選權威、回滾策略（兩個 Module 預設帶，但不選策略就沒有作用；要更乾淨可以在產品登記檔關掉），沒有歷史、沒有任何檢查。
- **讀檔測試**：同一份存檔讀兩次，`World::Summary` 相同——這次在 Windows 上也穩，因為摘要不再雜湊原始位元組：每種型別有自己的雜湊函式，padding 不算、`std::vector` 算內容。

Kernel 不知道「權威」「回滾」「判定資料」是什麼：它只多給幾個有型別的原語（整欄讀、一批放回、讀 stamp），並且每種型別可以帶一個雜湊函式。

**用詞：stamp 不是遊戲的 Tick。** 本頁的 stamp 是 ECS 的版本號（Kernel 每次改一格就前進的計數，用來判斷「這格在某個時間點之後改過沒」；同類：Bevy 的 change tick、Unity 的 `GlobalSystemVersion`）。遊戲的 `Tick` 是時間 Module 的固定步計數，兩者無關。第 3 批拿掉的是 `tickStamps`（遊戲 Tick → stamp 的對照表），不是 stamp。

---

## 2. 兩份宣告與現在的程式逐條對照

### 2.1 回滾份要 Kernel 給的原語（回滾份 6.1；`recon-b` 第 1 節）

| # | 原語 | 現在 | 本批 |
|---|---|---|---|
| 1 | 整欄讀出（id、added、changed、值） | `ExclusiveWorld` 裡沒有；Host 側要 `Keys`＋`Get`＋`Stamps` 三次查找（`World.hpp:173-200`）；System 側的 `Changed<T>` 只篩不給 stamp | **做**：`ExclusiveWorld` 裡有型別的整欄走訪，同時給 stamp；`ExclusiveWorld` 也能讀目前的 stamp（第 2.4 節 G5 要用） |
| 2 | 整欄放回（沒變的格不寫） | 只能逐格 `Insert`／`Remove`；每次當場改後句柄的 stamp 會前進（`Params.hpp:1087-1100`），做不到「一次還原標同一個 stamp」 | **做，形狀是「一批放回」**（本卡加的詞）：一次呼叫帶存活集合、多欄、多個 Resource；用登記的比較函式跳過沒變的格，有變的格標同一個目前 stamp；**全部放回之後**才照 DI-KS 9 的順序跑 OnAdd／OnRemove hook 與 `Add<T>`／`Remove<T>` observer，hook 看得到同一個實體已放回的其他欄（照同步點「全部欄 insert 完才跑 OnAdd」的規則；修訂 1，檢查 S6） |
| 3 | 實體存活集合的讀出與放回 | **比回滾份寫的更窄**：`ExclusiveWorld::SpawnWithId` 把配發器範圍內的 id 丟成 `IdInAllotterRange`，`Insert` 對不存活的實體回 false，所以「第 T 步活著、之後被刪、id 由本 World 發的」實體**現在復活不了** | **做**（必要，不只是效能）：放在原語 2 的一批裡；多的實體走 despawn 的路、少的復活；復活不經配發器範圍的檢查 |
| 4 | Resource 的存在、值與 stamp 一起讀寫 | 存在與值有；**讀 Resource 的 stamp 沒有任何 API** | **做**：讀 stamp；寫放在原語 2 的一批裡 |
| 5 | 指定 stamp 寫回 | 沒有 | **不做，只留在 DI-KS 15 第 (13) 項的清單**（本卡照 L17 判）：官方策略不用；回滾份把它留給「作者自寫的時間倒流策略」，那是說明用的例子、不是點名的 Feature；AGENTS §2「實作は薄く始める」 |
| 6 | 以上在 `ExclusiveWorld` 裡當場可用、成員變化跑 hook | 當場改的 7 個函式已有，跑 hook 與 observer（`Executor.cpp:567-769`） | 原語 1～4 照這條路接上 |

**原語是有型別的**（本卡照 L17 判）：回滾 Module 的 `rollback.Component<T>` 在登記時知道 `T`，用它產生型別擦除的「讀、比、放回」函式即可；Kernel 不必提供「不知道 `T` 就複製一欄」。系譜：Bevy 的 `set_last_changed`、`EntityWorldMut` 都是有型別的；Unity Netcode 走原始位元組是因為元件都是 unmanaged（`recon-c` 2）。P2P 佔位要的「存整個狀態」也只存登記過的可回滾型別。

**巢狀跑**：兩層巢狀准跑、沒有數字上限，只有 X-11（同一張 schedule 不能在堆疊上出現兩次，`Executor.cpp:496-501`）。所以**重跑的獨占 System 不能掛在步的 schedule 裡**，要掛在每幀的 schedule（第 5.2 節）。回滾份第 12 節的限界「准不准兩層」由此解答。

### 2.2 唯讀登記表（統合 3.3；`recon-b` 第 2 節）

統合 3.3 列的「缺」（hook、`InitialValue`、schedule 選項與 stage、順序圖、effects、`Writes<…>` 標註）在 DI-KS 25 的條文與 `FrozenRegistry` 的欄位上**第 1 批已補上**（`recon-b` 2.1；檢查者只對了條文，逐欄對程式在第 2 步開頭做）。還缺三樣：

| 缺的 | 誰要 | 本批（本卡照 L17 判） |
|---|---|---|
| 每種 `Message` 的**寫者**名單 | 回滾的跨 schedule 訊息檢查（回滾份 7.1） | 加：和讀者名單同形（System 掛載、hook、observer；每次 `BuildPlans` 重算） |
| `ExclusiveWorld<Writes<…>>` **只收 Resource**，列元件是 RE4（`Registry.cpp:303-305`）；`annotatedWrites` 是 `std::vector<ResourceId>`、進 Registry 指紋（`Fingerprint.cpp:160`） | 權威份 5.8（狀態機的套用 System 用 `Writes<…>` 列出它寫的判定資料，可以是元件）、5.4、5.5 | 擴成元件與 Resource 都收；`annotatedWrites` 的形狀與 Registry 指紋的編碼跟著改；Kernel 照舊不解讀（L7 的範圍內） |
| 順序圖不是遞移閉包（`Scheduler.cpp:499-504`） | 回滾的 Derived 檢查（R-7） | **不加**：回滾 Module 自己算閉包 |

### 2.3 被第 2、3 批換掉或已解決的

| 兩份寫的 | 現在 | 本批 |
|---|---|---|
| 「重跑中」Resource 歸誰（回滾份 5.7、統合 15.2） | 第 3 批已收：`Resimulation` 由 `WorldTimeModule` 登記、恆在、初始 false、沒有寫者（WT-1，`WorldTimeModule.cpp:106`、`:143`） | **歸時間 Module，定了**。回滾策略寫它。小缺口：準備函式用 `PutIfAbsent`，作者對它 `CarryOver` 時舊值會被帶過來——改成每次準備都放 false，WT-1 補一句（11.4） |
| 重跑時每一步 tick 怎麼推（回滾份 7.2 第 2 條） | `RunOneFixedStep(world, schedule)`、`RewindFixedSteps(world, k)`（`FixedStep.hpp:37-68`），和宣告一致 | 官方策略照用 |
| tick 的型別與驅動者名單（L27、權威份 5.4） | `Tick{value}`、`WorldTimeModule::TickWriters()`＝`{"RunFixedSteps"}`（static）、`RunFixedSteps` 帶 `Writes<Tick, FixedTime, ActiveTime>` | 權威的 tick 檢查照用 |
| 可回滾 schedule 的名字（回滾份 5.4 `rollback.StepSchedule(name, recordStage)`） | 編排層給預設骨架 `Main`、`FixedUpdate`（O-4）；時間 Module 的名字可由 `WorldTimeOptions` 換 | 官方策略的預設**跟著時間 Module 的設定走**：步的 schedule＝`WorldTimeOptions::fixedSchedule`（預設 `FixedUpdate`）、記錄掛在它的 `Last`、重跑掛在 `frameSchedule` 的 `PreUpdate`；作者可換 |
| 事件的名字 `Message`、推送式 `Event`＋`Observer` | 第 3 批已做 | 照兩份（已寫 observer） |
| 推送式事件與 observer 帶 zone（第 3 批宣告 4.5 的過渡） | `EventOptions::zone`、`ObserverOptions::zone`；Z6 與寫者數量也涵蓋 observer（`Registry.cpp:1441-1485`） | 隨區規則一起拿掉；Z6 搬權威時含 observer（權威份 5.11 已寫） |
| 策略在建 World 時選定（權威份 4.4、回滾份 5.7） | 選項包 `WorldDescription::options` 在各 Module `Build` 之前就給（`Setup.hpp:26-49`） | 策略經選項包給 |
| `RejectedSpawn::rollbackActive`、`emitterZone` | `rollbackActive` 恆 false、沒有讀者；`emitterZone` 還在 | 都拿掉（統合 3.2、權威份 5.10） |
| DI-KS 23「列出的算寫者」、開局的寫者豁免 | 第 1、2 批已拿掉 | 不再處理 |
| Z5、L3、L7 的執行期攔截 | 第 1 批已拿掉（`engine/` 0 命中） | 只剩守它們「不回來」的反向突變（9.1 A11） |
| `rollbackSafe` | 沒有任何檢查讀它，只進 Registry 指紋（`Fingerprint.cpp:186`） | **拿掉、不搬**（本卡照 L17 判）：沒有對應的公開登記，意思已由「重跑時不跑」涵蓋（第 12 節末「字面不同」） |
| Module 之間怎麼拿到彼此的公開登記；**作者怎麼拿到 `authority.`、`rollback.` 的入口** | **沒有通道**：`WorldSetup`／`WorldCheck` 不交出別的登記單位，產品自己的 `build` 也拿不到；唯一的例子是 static 的 `TickWriters()`（`recon-b` 5.6） | 第 6 節；使用者定 (a) 編排層的共用槽（第 12 節第 3 件） |

### 2.4 第 3 批留下的空窗

- **G4**（「誰都能寫 `Tick`」沒有守衛；(c) 區的 System 讀不到 `Tick`）：現在 (c) 區 System 帶 `Res<Tick>` 是 D-E7 Z1 的啟動錯誤，run 條件讀 `Resimulation` 也一樣（`recon-b` 4）。**本批收掉**：區規則整個拿掉，沒有 (c) 區了；「只准時間 Module 公開的驅動者寫 `Tick`」由伺服器權威策略的專用檢查守（權威份 5.4）。不選權威的 World 照舊誰都能寫——`-engine-not-rigid` 的結果，WT-5 已寫成約定。
- **G5**（沒有「某個 tick 開始時的 stamp」）：**不加 Kernel 原語，由回滾 Module 自己記**（本卡照 L17 判）。官方策略每次記錄時用原語 1 讀目前的 stamp、存進自己的歷史，下次只記 `changed` 大於那個 stamp 的格（回滾份 5.5 第 2 點）。三家都沒有 Kernel 層的 tick→stamp 對照表：lightyear 每個固定步跑記錄 System、用 `is_changed()`；Unity Netcode 備份時存下版本、之後用 `DidChange(備份時版本)`（`recon-c` 4）。GYO 照 Unity 的形（存版本、比版本），理由：一個 System 處理全部可回滾型別。退掉的選項見 15.4（照 Bevy `Ref<T>` 把 stamp 開給一般的 query）。

### 2.5 兩份和程式對不上、要在本批定的小處

| 宣告 | 程式 | 處理（本卡照 L17 判） |
|---|---|---|
| 回滾份 5.5 第 5 點「有變的格標**目前** stamp」 | 逐格當場改每格 stamp 不同 | 原語 2 一批放回、標同一個 stamp |
| 回滾份 7.1「讀 L6 的讀者、寫者名單」 | 只有讀者 | 2.2 加寫者名單 |
| 權威份 5.8「`Writes<…>` 列出寫的判定資料」 | 只收 Resource | 2.2 擴成元件 |
| 權威份 4.3「型別要提供雜湊函式（登記時編譯期檢查）」 | 登記 API 沒有雜湊的位置 | 第 3 節：Kernel 定雜湊的客製點；權威的登記入口照權威份自己做編譯期檢查 |
| 兩份引的行號（`Fingerprint.cpp:338-458`、`Registry.cpp:1440-1490`、`Scheduler.cpp:500-525` 等） | 第 1～3 批之後都移了（`recon-b` 第 6 節、`recon-a` 6 第 11 點） | 本頁用 59fd4da9 的行號；兩份不改（已核准的紀錄） |

---

## 3. `World::Summary` 改逐型別雜湊

### 3.1 現況（`recon-b` 第 3 節；檢查 M8）

`World.cpp:1115-1201`（格式版本 2）：存活實體、每欄每格的 `ValueBytesAt`、每個 Resource 的 `StorageBytes`（配發器也照位元組）、每種訊息的 payload 位元組、游標表，串起來取 SHA-256。side chat 列的三種毛病都在：padding 進雜湊（LIM-8 的來源，Windows restore case 5）；`std::vector`、`std::string` 這類持有堆積指標的型別雜湊到**位址**；`-0.0` 與 `+0.0` 位元組不同。登記 API 與型別擦除的欄都沒有放雜湊函式的位置。

`-summary-padding-a-then-c` 的標註：**測試裡 9 處、10 行**（`WorldTests.cpp:331`、`:516-519`（J-32 golden 自己）、`ExecutorTests.cpp:362`、`:1343`、`RegistrationRoundTests.cpp:191`、`SpawnRejectTests.cpp:411`、`:871`、`:1021`、`orchestration/RestoreFlowTests.cpp:140`），**程式裡 4 處**（`World.cpp:1156`、`World.hpp:351`、`detail/Storage.hpp:246`、`world_time/TimeTypes.hpp:6`）。修訂 0 寫「四處」是數錯了。

### 3.2 形狀（本卡照 L17 判；第 12 節第 1、2 件由使用者定）

- **雜湊函式跟著型別走**：Kernel 定一個客製點（本卡加的詞：作者為自己的型別寫一個函式，Kernel 在需要時找到它），登記元件、Resource、`Message` 時若找得到就存進型別擦除的欄、Resource 槽、訊息緩衝（多一個「雜湊一格」的函式）；找不到時記成「沒有雜湊函式」（呼叫 `Summary` 時才報錯，使用者 2026-11-20 定，第 12 節第 1 件）。名字在實作時照命名規範定（暫名 `HashValue(Hasher&, const T&)`，經 ADL 找）。系譜：lightyear 每個元件型別登記一個 `fn(&C, &mut Hasher)`（`deterministic.rs:10-94`）。
- **同一個函式給兩個人用**：Kernel 的 `World::Summary` 與權威 Module 的 digest。一個概念一個實作。
- **Kernel 附的雜湊**（修訂 1，檢查 M1）：
  - 整數一律換成 64 位元、小端序再寫（`long` 在 Windows 是 4 位元組、在 Linux、macOS 是 8，值相同就相同）；列舉照它的底層整數；`bool`；`EntityId`。
  - 浮點：逐位元（`±0` 不同、NaN 照它的位元；使用者 2026-11-20 定，第 12 節第 2 件）。作者的型別若 `±0` 要算相等，在自己的雜湊函式裡正規化（例 `h(v.x + 0.0f)`），或在寫入時正規化（同第 3 批 `VirtualTime.scale` 的做法）。
  - `std::array`、`std::vector`、`std::string`、`std::optional`（只雜湊「有沒有」與有時的值，不碰空著時的位元組）、`std::pair`：雜湊內容。
  - Kernel 自己的型別（`RangeAllotter`、游標表、訊息紀錄）在 Kernel 裡寫好。
- **照位元組不自動套用**（修訂 1，檢查 M1）：`std::has_unique_object_representations_v` 只看結構不看值——`struct { std::optional<std::uint8_t> a; std::uint16_t b; }` 是 true，但同值的兩個物件 `memcmp` 不同（檢查者本機 clang 實測）；指標、`std::string_view`、`std::span` 也是 true（雜湊到位址）；`long` 的寬度隨平台。所以只在**作者明寫**時照位元組（暫名：一個 `HashAsBytes` 的現成實作，作者為型別選它），作者保證「位元組就是值」。
- **組合型別**：作者寫一個逐欄位呼叫的函式（例：`h(v.x); h(v.y);`），或選「照位元組」。
- **編碼固定**：每個值照固定寬度、小端序寫進雜湊的輸入，三平台相同；J-32 的 golden 三平台同值。最後照舊取 SHA-256（格式版本 2→3）。
- **摘要照順序雜湊**（實體依 id、欄依登記順序），照舊含 id。**digest 照權威份 5.6 用和順序無關的合併**（第 4.4 節），兩者共用的只有每個值的雜湊函式。
- `KernelSettings` 照舊不進摘要。
- **作者的函式漏寫欄位時**，那個欄位的差異摘要看不出來（原始位元組沒有這個面）。這是 9.1 A8 的緩的面之一，也是新的 Replacement Criteria（11.1）。

### 3.3 沒有雜湊函式的型別怎麼辦——**使用者定 (d)**（2026-11-20，第 12 節第 1 件）

登記照過；呼叫 `World::Summary` 時，World 裡有沒有雜湊函式的型別，就回錯誤並列出那些型別。缺函式這件事由作者自己用測試守（使用者原話：「選d就好，執行到報錯，這個問題需用作者自己用測試來守」）。權威的 digest 照權威份 4.3，在 `authority.Component<T>` 登記時編譯期檢查。

### 3.4 LIM-8 留不留（本卡判；第 12 節第 1 件定 (d)、第 2 件定 (i)）

**LIM-8 改寫，不解除**：原始位元組不再是預設，但「作者選照位元組」那條路仍然要作者保證位元組＝值（沒有 padding、沒有 `optional`／union 的殘值、沒有指標與句柄、沒有寬度隨平台的型別）——digest 跨行程比，這條限制現在就適用，不是將來（修訂 0 想另立 LIM-9、「跨行程時才有事」是錯的，檢查 M1）。改寫後的 LIM-8：「`World::Summary` 與 digest 對每個值呼叫型別的雜湊函式；作者選『照位元組』的型別，位元組必須等於值」，守衛：Kernel 附的型別的測試；作者的型別無守衛。第 12 節第 1 件定 (d)，不加 (b)、(c) 那種「略過」「退回位元組」的句子；改寫後的 LIM-8 再加一句「沒有雜湊函式的型別在 `World` 裡時，`Summary` 回錯誤；作者的型別有沒有函式，由作者的測試守」。條文的 owner 是架構師會話，合併前核准時給你看。

---

## 4. 權威 Module：照權威份，加上實作的細節

範圍照權威份第 4、5 節，不改。下面只寫實作時要定、權威份寫「暫名」或「實作批定」的。

### 4.1 位置與登記

- `engine/authority`、`GYO::Engine::Authority`、target `GYO::Authority`、登記單位 `AuthorityModule`（`Orchestration::Module` 的子類）；`modules.csv` 加一列，`depends=base;kernel;orchestration;world_time;rollback`，**`default=yes`**（修訂 1，檢查 S2：照批次表「主會話自己判的」——Module 預設全帶、產品可以關；World 不選策略就沒有作用。修訂 0 寫 `default=no`、引 `-products-pick-modules` 當根據是錯的，那個決定沒有定預設）。代價：用產生的單位表的產品，每個 World 多兩個登記單位，登記單位名單進 Registry 指紋，golden 本來就要在本批重產。
- 登記檔的順序：`world_time` → `rollback` → `authority`（O-6 不重排）。

### 4.2 策略的插口與錯誤的回報

- `AuthorityStrategy`（虛函式的抽象基底類別，L21）、`ServerAuthorityStrategy`、佔位 `PeerRollbackStrategy`（名字實作時照命名規範定）。
- 選法：產品在 `WorldDescription::options` 放一個帶策略物件的選項結構（本卡照 L17 判：時間 Module 已用同一條路）。選項包同一份 `WorldDescription` 在 `ReplaceWorld` 時重用：策略的狀態在 `Build` 時重設。
- **沒選策略時型別照樣進 Kernel**（修訂 1，檢查 S10）：權威的登記入口在沒有策略時，自己用作者的 `Registrar` 登記進 Kernel，只是不代為登記進回滾。否則用到那個型別的 System 是 Kernel 的 R-E4、`Check` 不跑（O-2），作者看不到真正的原因。
- **Build 時才知道的錯誤**（沒選策略、`rollback.Input<T>` 時 `T` 還沒登記、兩筆作者登記 options 不同）：`WorldSetup` 沒有回報錯誤的入口，所以 Module 先存起來，在 `Check` 用自己的字串碼報（O-5；權威 `AU-E…`、回滾 `RB-E…`）。

### 4.3 名字（照命名規範；本卡照 L17 判）

權威份的「C 指紋」是舊 Kernel 的名字（C＝(c) 區），區拿掉之後不再有意思。權威份 4.4 已用 `ConsistencyFingerprint(view)`；本批照它，文件裡叫「一致性指紋」。G-5 的匯出格式 `gyo.zone-c-sources` 改名 `gyo.authority-sources`，`tests/common/zone_scan/zone_scan.py:49-51` 的輸入解析、它的自測與 `mutations.json`（18 個）跟著改；工具目錄的名字在第 5 步照命名規範定（ZS1～ZS7 本身不是區規則，`recon-a` 3）。

### 4.4 一致性指紋、digest、tick 檢查、交叉檢查

照權威份 5.4、5.5、5.6、5.9、5.11。實作細節：
- **收哪些 System**：讀唯讀登記表的 `access.writes`、`annotatedWrites`（2.2 擴成元件之後）、hook 與 observer 的 `access`；經 `InitialValue` 的間接寫（舊漏斗，權威份 5.11 第 1 列的 B3）從 `InitialValueInfo` 重算。Kernel 現在的 `FunnelWrites`（`Registry.cpp:1235-1253`）在拿掉 Z1～Z7 後只剩 Z6 用，跟著搬到權威（`recon-a` 6 第 10 點）；漏斗的走訪性質（鏈、每個被填的元件、原地寫／只讀／只移除請求不算寫）由權威的 Z6 重新觀察（9.1 A4）。
- **digest 的合併**（權威份 5.6「和順序無關」）：每個「實體 × 判定型別」各算一個雜湊、輸入含型別在權威登記中的序號，結果**相加（mod 2^64）**。本卡照 L17 判：lightyear 用 XOR（`checksum.rs:6-7`、`:483-486`），兩個相同的「實體×元件」會互相抵銷（例：兩顆位置相同的球；`recon-c` 1.1 推論），相加不會。這屬官方策略的內部（`Digest` 的形狀不在這次的契約內，權威份 4.4），不寫進 DI。
- **digest 的輸入含不含實體 id：不在本批定**（修訂 1，檢查 M4）。權威份 5.6 與統合 15.3 定「和順序無關」的理由是「兩端實體的迭代順序不保證一致」；兩端 id 是否相同、要不要對映，屬網路 Module（L23，lightyear、replicon、Unity 都做實體對映）。本批的 digest **不含 id**，和 lightyear 相同；交換兩個實體的值看不出來，寫進範圍的限界，網路 Module 的宣告重看。
- **digest 何時比、不一致怎麼辦**：照權威份延到網路 Module；本批只在回歸測試與 scenario 第 3 段用。
- **D-E15 的範圍**：只查經 `authority.Ingress` 登記的入口（權威份 5.11 第 3 列）；現在 Kernel 查全部 `ingress` 的 System——9.1 A4 的緩的面。

### 4.5 和第 3 批的接點

- tick 檢查讀 `WorldTimeModule::TickWriters()`（static，權威依賴 `world_time`）。`fixedStep=false` 的 World 名單照樣列 `RunFixedSteps`：名單是「准寫的人」，沒掛也無害。
- 權威份 5.11 的 Z6 對 `Tick` 例外。

---

## 5. 回滾 Module：照回滾份，加上實作的細節

範圍照回滾份第 5、6、7 節，兩批（`-registrations`、`-official-strategy`）都在本批。

### 5.1 位置與登記

- `engine/rollback`、`GYO::Engine::Rollback`、target `GYO::Rollback`、登記單位 `RollbackModule`；`modules.csv` 一列，`depends=base;kernel;orchestration;world_time`，`default=yes`（理由同 4.1）。
- 公開登記六種照回滾份 5.4；讀取介面（R-6）給權威讀。
- 公開登記執行時要用（記錄、還原每步都讀）：**登記凍結之後放進那個 World 的一個 Resource**（本卡照 L17 判：Unity 把回滾的設定放在 World 的 singleton；回滾份 5.5 第 1 點的歷史也在 World 的 Resource）。不放在 Module 物件裡以 World 名為鍵的表：`ReplaceWorld` 的 `BuildNewFirst` 失敗時舊 World 照跑，而 Module 裡那份表已在新的一輪 `Build` 重設（O-2；檢查 M5 第 4 點）。

### 5.2 官方策略掛在哪（本卡照 L17 判）

- 判斷、還原、重跑是一個不帶 `Writes<…>` 的 `ExclusiveWorld` System，掛在**每幀 schedule 的 `PreUpdate`**（預設 `Main`，在 `FixedMainLoop` 之前）。系譜：lightyear 在 `PreUpdate` 跑 `run_rollback`（回滾份第 4 節表）。不能掛在步的 schedule 裡（X-11，2.1）。
- 記錄是不帶 `Writes<…>` 的 `ExclusiveWorld` System，掛在步的 schedule 的 `recordStage`（預設 `Last`）。回滾份 5.5 第 9 點寫「記錄是只寫歷史 Resource 的一般 System」；改成獨占 System 是因為要讀 stamp（G5），兩種在權威的檢查裡都看不到它寫判定資料，結論不變（第 12 節末「字面不同」）。
- 刪掉的格：記錄時比這一步與上一筆的鍵集合（本卡照 L17 判：lightyear 用 `RemovedComponents`；GYO 的 `Remove<T>` observer 也做得到，但比鍵集合不必在 observer 裡改歷史，少一條重入的路）。

### 5.3 第一版的設定值

最大回滾深度預設 20（照 lightyear，回滾份 5.5 第 1 點）；判斷的模式三種，預設「比對後不同才回滾」；修正預設指數衰減；`rollback.Component<T>` 的比較函式預設 `==`（回滾份 5.4；浮點和雜湊的關係見第 12 節第 2 件）。PvP 移植時另定。

### 5.4 預算（AGENTS §2；回滾份第 11 節要求官方策略那一批寫）——**數字等研究卡的結果，由使用者定**

**處理**（使用者 2026-11-20，side chat）：開一張唯讀研究卡，整理 master 2026-11-01 之後的數據當**基準**（「以前是什麼樣，現在我們要做到什麼」），提出目標候選，數字交使用者定。原話：「預算的規模假設：這個我建議，你專門更具這個問題跑一個研究卡去看看master11/01之後的數據。不是要你參考，而是你需要建立一個基準。以前是什麼樣，現在我們要做到什麼。你覺得如何？」→「但是你要開研究卡，範圍沒錯。GYO很多問題都是到10月底才處理完，11月節點可以。」研究卡：[研究卡] 第 4 批預算基準（`local_3eaf3c3f-2af9-4e8e-947c-a1511cab0350`，Opus 5.5／high，唯讀），報告寫到主 checkout `build/evidence/gyo2-authority-rollback/budget-baseline-research.md`。和「不參考 master 的算法與數據」（第 12 節）的分界：master 11-01 之後的數據只當基準比對「以前到哪、現在要到哪」，不拿它的算法、常數或架構來設計；11-01 之前的數據（含 2026-09-25 的 dev_log）照舊不用。

**已定的**（使用者 2026-11-20，在研究卡）：原話「規模用位元組，深度從 RTT 推。PvP 目標是4人，協議可以看master的 protobuf設計。」
- **規模用位元組**（研究報告的 A1；不用「實體 × 元件」）。
- **深度從目標 RTT 推**（研究報告的 B2）：深度＝ceil(RTT ÷ 15.625 ms)＋餘量。
- **PvP 目標 4 人**。
- **協議可以看 master 的 protobuf 設計**：這是「不參考 master」（第 12 節）的例外，只用來估可回滾狀態的大小。研究卡依 master `apps/object_fps_pvp/protocol/client_v9.proto`（`af2c93ad`，2026-11-01）粗估：每人 `PlayerState` 20 欄＋`CombatState` 14 欄＋`PlayerAnimation` 8 欄，每欄算 4 B，約 168 B；4 人約 0.7 KB。協議裡沒有子彈狀態（命中即時判定），所以不另加子彈實體。輸入走輸入歷史，不算進可回滾狀態。協議裡欄位的常數值（例 `maximum_reference_age_ms`）不引用。

**還沒定的**：位元組規模（候選 S1 4 KB／S2 16 KB／S3 64 KB；修訂 1 的 1,000 × 4 以每格 32 B 算約 128 KB）、目標 RTT 與餘量（RTT 100／150／200／300 ms → 7／10／13／20 步，各加餘量）、門檻要不要跟著收緊、量法（使用者說過要量可以在 Linux 實機原生量）。出處：研究報告第 7 節（主 checkout `build/evidence/gyo2-authority-rollback/budget-baseline-research.md`）。

下面是修訂 1 的假設，**等研究卡與使用者定的數字取代**：

沒有產品給過預算。以 64 Hz 一步（15.625 ms）為基準，規模假設 **1,000 個實體 × 4 種可回滾元件**（舊 PvP 的移植目標沒有寫實體數；1,000 是「一場 PvP 的角色、子彈、道具」的上限估計，沒有出處）：
- **記錄**：每步 ≤ 156 µs（一步的 1%；每步改了全部格的最壞情形）。
- **一次回滾的總開銷**（修訂 1，檢查 S8）：退 20 步、全部格都變的最壞情形，還原＋重跑時 20 次記錄＋`ActiveTime` 與 `Resimulation` 的起落，**不含作者的 System**，≤ 3.125 ms（一步的 20%）。根據：Client 在延遲大時可能每幀都回滾一次，Module 的開銷不能吃掉一步的大半；20% 留 80% 給作者的 System。修訂 0 只設三個各 1% 的門檻，沒有約束到總量（約 3.4 ms＝22%）。
- **量法**：本機（i9-9980HK、macOS x86_64）Release，各 1,000 次，取 p99；結果與平均一起記進 dev_log。超過就停下回報（§14），不放寬、不重量。

---

## 6. Module 與作者的接法——**使用者定 (a)**（2026-11-20，第 12 節第 3 件）

**決定**：編排層給「每次建 World 一份、以型別為鍵」的共用槽；「不算 O-4 職責清單多一項」也經使用者確認。理由（使用者的）：組裝 Module 是編排層的工作（O-4「組裝與流程」、O-2、O-6），buffer 放編排層合理。用詞：使用者覺得它像「Module 的 buffer」（暫時的中轉區：建 World 期間用，內容凍結後交給 World 的 Resource，之後清掉），不像 cache；正式名字實作時照命名規範定，可參考這個說法（下面的暫名 `Shared<T>()` 也在那時重定）。附帶：`Build` 指執行期建 World 時 Module 的登記函式，和 CMake 的建置容易混淆；使用者沒有要求改名。下面保留決定前的比較。

**共用槽的機制**（使用者 2026-11-20 補充：「第 3 件，注意，由於是共用的，需要建立一個機制，或者需要保證順序，邊讀邊寫很容易出問題。」下面四點是 side chat 提的，使用者回「可以」）：

1. **分段，型別上擋住**：`Build` 階段（Module 的 `Build` 與產品自己的 `build`）經 `WorldSetup` 拿到 `T&`；`WorldFinish`、`WorldCheck` 只給 `const T&`。`Build` 階段結束時編排層凍結槽，凍結後再寫是 Programmer Error（斷言）。要看全貌的判斷（Z6、Z8、D-E15 的交叉檢查、重複登記的裁決）一律放在凍結後的 `Finish`／`Check`。系譜（side chat 補，不是使用者的話）：Bevy 的 Plugin 分 `build`／`finish`，`finish` 在所有 `build` 之後才讀別人的登記。
2. **誰建、誰用**：槽裡每種型別只由擁有它的 Module 建（暫名 `Create<T>`）；別人用 `Find<T>`，找不到就回錯誤，不偷偷建空的。先後靠 `modules.csv` 的依賴與既有的 O-E3（依賴排在後面是錯誤）保證；產品自己的 `build` 永遠在最後。
3. **寫入只記錄、不裁決**：`Build` 階段只追加，每筆帶「作者／代為」的來源；凍結後統一裁決（作者的蓋過代為的；兩筆作者的 options 不同是錯誤），所以結果和呼叫順序無關。本卡照 L17 補的細節：編排層不解讀槽裡的型別（O-2），所以裁決由擁有該型別的 Module 在自己的 `Finish` 做，錯誤在 `Check` 回報（`RB-E…`）。這和回滾份 5.4「不論先後，作者的為準」、權威份 DI-A 11「代為登記在登記當下做」都一致：登記當下做的是「記下一筆」，誰為準到凍結後才定。
4. **單執行緒、不加鎖**：建 World 是單執行緒依序跑，不加鎖；寫進 DI（11.5），之後要並行建 World 時碰 RC（11.5 的 RC-O10）。

比過的選項：只靠呼叫順序與約定（不建機制）／建上面的機制（**選這個**）。


**遊戲的例子**：PvP Client 的作者在**產品自己的 `build`** 寫 `authority.Component<Health>()`；伺服器權威策略要在**這一刻**替作者呼叫回滾 Module 的 `rollback.Component<Health>()`（權威份 4.3、4.4：「重跑時不跑」只能在 `AddSystem` 當下給，代為登記不能等到 Plan 建好）。之後權威的 `Check` 要讀回滾的公開登記（Z6、Z8、D-E15）。單機的時間倒流只用回滾：作者在自己的 `build` 寫 `rollback.Component<Position>()`。

**現在缺兩條**（修訂 1，檢查 M5）：(1) 作者拿不到 `authority`、`rollback` 這兩個入口——產品自己的 `build` 只拿到 `WorldSetup&`（`Product.hpp:42`），用產生的單位表的產品手上沒有單位物件；(2) 權威 Module 拿不到同一個 World 的回滾 Module。`WorldSetup`／`WorldCheck` 都不交出別的登記單位（`Setup.hpp`、`detail/RegistrationUnit.hpp:36-47`）。

**別家**：Bevy 有兩種，都在 App 層。(1) `App::get_added_plugins::<T>()` 回傳已加入的 Plugin 參照，**唯讀**，文件寫「read the settings」（`bevy_app/src/app.rs:578-600`）；(2) lightyear 的登記存在 App 上的 Resource `ComponentRegistry`，預測套件與作者都在 build 時改它（`lightyear/src/protocol.rs:102`、`prediction/src/registry.rs:741-748`）。GYO 在 `Build` 時還沒有 World；(2) 的對應是「每次建 World 一份、以型別為鍵的共用槽」。

**比過的選項**：
- **(a) 編排層給每次建 World 一份的共用槽**（**選這個**；修訂 1 新增）：`WorldSetup` 加一個以型別為鍵的槽（修訂 0、1 的暫名 `Shared<T>()`「沒有就建一份、有就回同一份」已由上面的機制取代：擁有者 `Create<T>`、別人 `Find<T>`；`WorldFinish`、`WorldCheck` 只拿 `const T&`），每次 `Build` 那個 World 時新建，跟著那一次建 World 走。回滾 Module 在自己的 `Build` 開頭建它的登記表放進槽；權威 Module、作者都從槽拿到 `rollback`、`authority` 的入口。系譜：lightyear 的 App Resource。代價：改第 2 批已合併的編排層介面；編排層不認得槽裡的型別（和選項包同形，O-2「只轉交、不解讀」）。本卡判**不算 O-4 職責清單多一項**（屬「登記單位與生命週期」），不碰 RC-O1（不改迴圈）、RC-O4（編排層不 include 任何 Module）。
- **(b) 編排層給「依賴的登記單位」的查詢**：`WorldSetup`、`WorldCheck` 加 `Unit<T>()`，查登記檔寫了依賴、排在前面的單位；產品自己的 `build` 也要能查它帶的單位。代價：比 Bevy 的前例多一步（回傳可改的參照、呼叫會改它狀態的登記）；Module 物件以 World 名為鍵的資料遇上 `BuildNewFirst` 失敗的問題（5.1）要另外處理。系譜：Bevy `get_added_plugins`。
- **(c) 產品經選項包傳共用物件**：產品建一個登記物件，同時放進兩個 Module 的選項，作者也拿得到。編排層不改；代價：每個用權威或回滾的產品都要自己接線，忘了接就是代為登記失效；回滾的公開登記變成「產品的東西」，和 R-6 不一致。
- **(d) static、以 World 名為鍵的表**（`TickWriters` 的延伸）：編排層不改；代價：行程內全域的可變狀態，同一行程兩個 Product（測試常見）同名 World 會互相蓋掉。

選 (a) 或 (b) 都改編排層的公開介面，「不算職責清單多一項」原是本卡的判斷，所以交使用者；使用者選 (a) 並確認不算多一項。side chat 的補充（不是使用者的話）：網路三家（lightyear、replicon、Unity Netcode）都把登記放在共同容器；(b) 的同類還有 Unity `GetExistingSystemManaged`、Unreal `GetSubsystem`；GYO 在 `Build` 時還沒有 World，所以由編排層在建 World 期間給；建好後凍結的登記放進 World 的 Resource，換 World 失敗時舊 World 不受影響。

---

## 7. 拿掉 Kernel 的權威規則（統合 3.2、7.2、附錄；`recon-a`）

### 7.1 程式

| 組 | 拿掉的（位置見 `recon-a` 1） | 去處 | 步 |
|---|---|---|---|
| 區 | `Zone`、各 Options 的 `zone`、`external`、`RequireExplicitZones`、`ApplyDefaultZones`、`CheckZoneAccess` 與輔助（`ZoneOf`、`ZoneName` 等）、S-6、S-7、S-8 的 (c) 半、S-9、**S-13**（統合沒點名，但它是 D-E7 的區規則，`recon-a` 6 第 5 點）、S-17、Z1、Z2、Z3、Z7、`ingress`、`ingressStage`、X-2／D-E11／X-E1、`parallelGuardTest`、D-E9、`InitialValue` 的 Z1、`emitterZone`、`SystemContext::zone` | 不由機制守（P14、確認清單） | 第 2 步（沒有新家） |
| 寫者 | `WritePolicy`、`CheckExclusiveWriters`（D-E2 S-11） | 不由機制守（`-authority-no-writer-count`） | 第 2 步 |
| digest | `DigestSet`、`MarkDigestSet`（R-E4 的 digest 部分）、S-12、`NamedSetRecord::digest`、`digestMarks` | 權威 Module 的 digest | 第 2 步 |
| 輸入 | `Role`（`State`／`Input`） | 權威的 `.input` 標記 | 第 6 步（Z8 還在用） |
| 交叉 | Z6（System、hook、observer）、Z8、D-E15、`FunnelWrites` | 權威 Module 的 5.11 交叉檢查 | 第 6 步 |
| 回滾 | `Rollback`（Yes／No／Derived）、`rollbackEnabled`、`notInRollback`、D-E16（`Scheduler.cpp:438-462`）、`rollbackActive`、Replay 的註解；`rollbackSafe` | 回滾 Module 的公開登記與 R-7；`rollbackSafe` 拿掉 | 第 6 步 |
| 指紋 | `CFingerprintOf`、`kCFingerprintVersion`、`FrozenRegistry::CFingerprint()`、`ExportZoneCSources`、`ZoneCSourcesOptions` | 權威 Module（一致性指紋、G-5 匯出） | 第 6 步 |
| 錯誤碼 | `StartupCode` 的 `DE2`、`DE9`、`DE11`、`XE1`（第 2 步）；`DE15` → 權威、`DE16` → 回滾（第 6 步，字串碼） | 列舉值等於統合 3.1 的清單 | 第 2、6 步 |
| 指紋格式 | Registry 指紋去掉 16 個欄位、`annotatedWrites` 收元件；Plan export 去掉 `registry_c`、`rollback=`、`zone=`；Plan 指紋不變（`recon-a` 1.4） | golden 重產：第 2 步一次、第 6 步一次（修訂 1，檢查 S3） | 第 2、6 步 |
| `world_time` | `WorldTimeModule.cpp:79`、`:100-109` 的 `.zone = Zone::B` 與 G4 的註解 | 拿掉 | 第 2 步 |
| 具名集合 | `SetRead<…>::ColumnView::ValueBytesAt`（`Params.hpp:748-750`）：值交成原始位元組 | **留**（本卡照 L17 判）：具名集合是 Kernel 的一般機制；`DigestSet` 拿掉後沒有用它做比較的 Feature。第一個用它比較值的 Feature 出現時改走逐型別雜湊（範圍的限界） | — |

### 7.2 先後：沒有新家的先拿，有新家的等接手測試（修訂 1，檢查 S3）

統合第 8 節的原則是「不需要新家的先拿，要新家的和新家同一條線」。修訂 0 把全部壓到最後，會讓權威與回滾在還有寫者數量與區規則的 Kernel 上開發（例：DI-A 8「不數寫者」的測試——兩個 System 寫 `Health`——在 Kernel 還有 D-E2 時過不了）。改成：
- **第 2 步（做兩個 Module 之前）**：拿掉沒有新家的——寫者數量、區規則、`DigestSet`、D-E9、X-2（9.1 A1、A2、A3、A10）。
- **第 6 步（第 4、5 步的接手測試寫好之後）**：拿掉有新家的——Z6、Z8、D-E15、D-E16、`Rollback`、`Role`、C 指紋、G-5（A4～A7）。這段期間 Z6 等規則在 Kernel 與新 Module 各有一份；Kernel 那份的對象寫法要配合第 2 步拿掉 `Zone`（Z6 現在以「zone C 的 System」為對象，第 2 步改成以 `Rollback::Yes` 為準、照舊擋），在第 6 步整份拿掉。
- 代價：golden 重產兩次（第 2、6 步）。比過：全部壓到最後（golden 一次，但兩個 Module 的測試在舊規則上過不了、最後才發現對不上）。

---

## 8. LIM-7 重看（批次表「LIM-7 隨區規則移出，在權威＋回滾那條線的宣告裡重看」）

LIM-7 的寫法建立在區上。區拿掉之後，Kernel 的行為是：**生成撞上活著的實體就丟掉並記 `RejectedSpawn`，不論是誰的資料**（F-13，照舊）。改寫（本卡照 L17 判，條文合併前核准時給你看）：
- 限制：生成被活著的實體擋住時照樣丟掉，Kernel 不擋。
- PvP（有權威端）的形式：編號只由權威端給，只在本機存在的實體不和權威共用編號空間（怎麼分開由產品決定）——照舊。
- 回滾這一側（回滾份 7.4 已答）：還原時存活集合與配發器一起退回，重跑的 `Spawn` 拿到和第一次相同的編號；會撞到的只有「本機專用的實體佔了同一個編號」，前一點已排除。
- 解除條件：第一個有權威端的產品證明本機的東西必須和權威共用編號空間。

---

## 9. 守衛、測試、突變

### 9.1 判定的變更（AGENTS §14.4；**方向要你核准**；最終在合併前帶突變結果與檢證者報告再看一次）

**方向的核准**（使用者，2026-11-20，side chat）：A1、A2、A3、A10 照建議刪（原話「照建議就行。」）；含檢查補進 A2 的五案與第 2 步開頭的恆真案掃描（9.2）。A4～A7（交叉檢查、一致性指紋、G-5 清單搬權威；Derived 檢查搬回滾）照建議（原話「可以。」，2026-11-20，side chat）：四組都變成「只在選了那個策略的 World 才查」，緩的面與緊的面照下表。A8、A9、A11 的方向還沒核准。

| 組 | 守衛 | 守的性質 | 處置 | 緩的面 | 緊的面 | 之後由誰守 |
|---|---|---|---|---|---|---|
| A1 | J-15、`ZoneRuleTests:151`、`:191`、`CoverageTests:302`、`InitialValueTests:170`、`SkeletonTests:365`、`ObserverTests:871` 的第二寫者半（`:896`）、編排層 `CoverageTests:538`（O-1 的 D-E2） | 一欄一個寫者 | 刪 | 第二個寫者通過 | 無 | 不由機制守（`-authority-no-writer-count`） |
| A2 | J-14（除手寫 Diagnostics 的 S-8 半）、J-38、J-39、J-40（除 Z8 半）、J-41、J-47 的 Z1 案、J-24 的 S-13、**J-45 的 S-13（`ExecutorTests:804` 的「zone C holding it: Finish fails」）**、D-E9（`SchedulerTests:590`、`WorldTests:940` 後半）、X-E1（`CoverageTests:385`、`SchedulerTests:477` 的 X-E1 半）、`InitialValueTests` 的 Z1／Z2／Z3 案（`:624`、`:635`、`:825`、`:915`、`:1096`、`:309` 的兩個 SUBCASE、`:1032` 的 Z1／Z2 半）、`NestedRunTests:1569`、`:1233` 的 S-17 半、**`ObserverTests:1001`（zone C observer 寫 zone B 是啟動錯誤）**、`ObserverTests:815`、`:871` 的區半、`RegistrationRoundTests:404` 的兩種、`SpawnRejectTests:291`、`:580` 的 `emitterZone` 判定；**只判「區規則允許的組合會通過」、拿掉規則後恆真的三案**（`InitialValueTests:666`、`:835`、`:1129`） | 區規則、S-6～S-17、Z1～Z3、Z7、X-2、D-E9、`emitterZone` | 刪 | 任何 System 讀寫任何資料都通過 | 無 | 不由機制守（P14、確認清單第 1～13 件） |
| A3 | J-16（`ZoneRuleTests:206`）、`RegistrationRoundTests:404` 的 `DigestSet` 半 | digest 集合禁 Shared、R-E4 的 digest | 刪 | 具名集合不再有 digest 的身分 | 無 | 權威 Module 的 digest |
| A4 | Z6／B3 九案（`InitialValueTests:645`、`:762`、`:773`、`:795`、`:1001`、`:1012`、`:1078`、`:1205`、`:1218`）；**漏斗的反方向與走訪**（`:986` 只讀或篩選不算寫、`:996` 原地寫請求不算寫、`:1164` 只移除請求不算寫、`:1066`、`:1117` 每個被填的元件都查、`:1180` 鏈穿過原地寫的元件）；Z8（`CoverageTests:254` 的 Z8 半、`ZoneRuleTests:305` 的 Z8 半）；D-E15（`SchedulerTests:626` 的半）；Z6 的混合案（`CoverageTests:254`、`InitialValueTests:1032` 的 Z6 半） | 判定資料 × 回滾；漏斗把哪些算成寫 | **搬到權威、判定改**：對象從「zone C 的 System」改成「寫了判定資料的 System、hook、observer」；從 Kernel 的 `Rollback::Yes` 改讀回滾的公開登記；`Tick` 例外；漏斗的走訪性質改由權威的 Z6 觀察 | 沒選權威策略的 World 不查；`Tick` 不必可回滾；D-E15 只查經 `authority.Ingress` 登記的入口（現在查全部 `ingress`）；Z6 的對象從全部 zone C 資料縮成經權威登記的資料 | 不分「判定的 System」：凡寫判定資料的都查；`Mark`／`MarkResource` 標記的型別不代為登記進回滾，作者沒自己登記時 Z6 會報 | 權威 Module |
| A5 | 一致性指紋（舊 C 指紋）12 案的 C 半（`recon-a` 2.2） | 兩端判定部分相同 | **搬到權威、判定改**（權威份 5.5 的內容表）：編號改權威登記順序；同步點只記有無；before／after、`rollbackEnabled`、zone 不進；`Runs`、策略名與版本、訊息上限進 | 一般資料的登記改了不再影響指紋；同步點改名不影響 | 策略、`Runs`、observer 存取進指紋；兩方向的守衛 | 權威 Module |
| A6 | J-17（`ZoneCExportTests` 兩案、`InitialValueTests:1246`、`:1269`）、`zone_c_export_check.py`、`kernel.J-17_zone_scan` | G-5 的來源檔清單 | **搬到權威、判定改**：範圍從「zone C 的 System」改成「寫判定資料的 System、hook、observer、`InitialValue`」；格式改名 | 讀判定資料、不寫的 System 不再列 | observer 列入 | 權威 Module＋產品的掃描 |
| A7 | D-E16 三處（`CoverageTests:113` 的半、`:312`、`SchedulerTests:626` 的半） | Derived 在讀者之前由立即寫者重算 | **搬到回滾**（R-7）：判定照搬；加巢狀 schedule 也查 | **沒帶回滾 Module、World 沒選回滾策略、或選了不檢查的自寫策略時不查**（回滾份 5.1 第 3 項：策略自己的檢查；統合 7.2 也寫了這個放寬面）。修訂 0 寫「無」是錯的 | 巢狀 schedule 也查 | 回滾 Module（官方策略） |
| A8 | `World::Summary` 的比較 12 組，含 J-32 golden（`WorldTests:541`）、測試裡 9 處「未完全解決」 | 同一個 World 狀態得到同一個摘要 | **改**：照第 3 節逐型別雜湊；J-32 golden 重產；9 處標註拿掉 | 只差在 padding 的兩個狀態（第 12 節第 2 件定逐位元，`±0`、NaN 的位元差照樣看得出），摘要從「可能不同」變成相同；**作者的函式漏寫欄位時，那個欄位的差異看不出來** | 持有堆積的型別改內容就會不同（以前雜湊位址）；沒有雜湊函式的型別不再能悄悄進摘要（第 12 節第 1 件定 (d)：呼叫時回錯誤） | Kernel |
| A9 | Registry 指紋 golden 6 處、Plan export 格式（`SchedulerTests:674`、`CoverageTests:411`）、`RegistryTests:223` 的「轉換中」名單 | 格式與錯誤碼清單 | 改：版本升、golden 重產；「轉換中」名單清空 | 無 | 錯誤碼清單只剩保留的 | Kernel |
| A10 | `WorldTimeModuleTests:176` 的 `[wt-time-zone-b]` 兩個 CHECK | 時間型別是 zone B（第 3 批的過渡） | 刪 | 無區可言 | 無 | —（G4 收掉，2.4） |
| A11 | 反向守衛的突變（`ks-s4-*` 9、`ks-s8-*` 5、`sr-zone-c-own-cells`、Z5 的 2）與它們殺的行為案（J-52「no Z5」、J-45 的 zone C／Exclusive 寫入、J-57 的 listed zone C resource 等） | 拿掉的限制不回來 | 突變退役：replace 用到拿掉的欄位、編譯不過；「不回來」改由型別保證（欄位不存在）。案例改名留下（成為一般行為）。**第 2 步開頭逐個列出「退役後會失去唯一突變的判定」，照 `tests/common/mutation/README.md` 交你，不等合併前**（修訂 1，檢查 S7） | 那些判定可能暫時沒有突變 | 無 | Kernel |

DI 的文字變更（第 11 節）、9.4 的退役、LIM-7、LIM-8 的條文同樣在這次給方向、合併前逐條核准。

### 9.2 只換寫法的（沒有緩的面；事後報告 1～5）

- **只拿掉參數的案**（`recon-a` 2.5 的 151 案扣掉 9.1 補進 A2、A4 的）與測試的共用支援檔（`KernelTestSupport.hpp:69-114`、`WorldTestSupport.hpp:27-85`、`FinishRecord.cpp`、`WorldExecRecord.cpp`、`ThreadBoundProbe.cpp`）：拿掉 `Zone::`、`rollbackEnabled` 等參數。**逐案確認判定仍有作用**（修訂 1，檢查 M6）：`recon-a` 用關鍵字分類，漏了 helper 藏起 `Zone::C` 的案與「只判通過」的案。第 2 步開頭另跑一次——把區規則、寫者數量的檢查關掉（讓它們一律通過）後仍通過的案，就是拿掉規則後恆真的案，逐個判「刪（列進 A2）」或「改成觀察別的性質」；結果列進 dev_log，新發現的刪除照 A2 一起在合併前核准。
- 案名或訊息裡寫「zone C」「Shared」而性質留下的（J-22、J-57、J-32b、J-45、J-52、J-55 的幾案），改名，殺突變的 `cases` 同步改。
- J-37、J-47 的留下兩案（`ZoneRuleTests:224`、`:358`）與 J-14 的手寫 Diagnostics 半搬到別的 Kernel 測試檔；`ZoneRuleTests.cpp` 刪檔。
- `GYO_KERNEL_J_ENTRIES` 拿掉在 Kernel 變成 0 案的 J-15、J-16、J-17、J-38、J-39、J-40、J-41（`tests/common/kernel/CMakeLists.txt:68-71`；0 案的 ctest 會因 `"test cases: +0"` 失敗）。
- `td-despawn-cells-kept`、`td-adv-despawn-dead-id-check-weakened` 期待的 W-1 是結構檢查、不是寫者（`recon-a` 4.5）：拿掉參數後重跑，確認仍 killed。

### 9.3 新增的行為案例

- **Kernel**：原語 1～4（有型別的整欄走訪給 stamp；一批放回：沒變的格不寫、有變的標同一個 stamp、全部放回後才跑 hook 與 observer、hook 看得到同一個實體已放回的其他欄、復活配發器範圍內的 id；Resource 的 stamp；`ExclusiveWorld` 讀目前 stamp）；`Writes<…>` 收元件；訊息寫者名單（System、hook、observer、`BuildPlans` 重算）；逐型別雜湊（padding 不同摘要相同、`optional` 空著時的殘值不影響、`std::vector` 內容不同摘要不同、內容相同而位址不同時相同、`long` 等寬度隨平台的整數照 64 位元、作者選照位元組的型別、Kernel 附的型別、浮點逐位元（`-0.0f` 與 `+0.0f` 摘要不同、作者在函式裡正規化時相同）、沒有函式的型別在 World 裡時 `Summary` 回錯誤並列出型別、拿掉那個型別的實體後同一個 `Summary` 成功）。
- **權威**：權威份第 7 節 DI-A 與第 5 節各條；一致性指紋兩方向；digest（同狀態相同、改一格不同、兩個相同的「實體×元件」不抵銷、迭代順序無關）；tick 檢查（名單外寫 `Tick` 報錯、`Writes<Tick>` 標註也算、時間 Module 的驅動者通過）；佔位策略與沒選策略的啟動錯誤；沒選策略時型別照樣進 Kernel；代為登記在登記當下、作者的為準；Build 時的錯誤在 `Check` 報；A4 的漏斗走訪性質。
- **編排層**（共用槽，第 6 節的機制）：`Build` 拿 `T&`、`Finish`／`Check` 拿 `const T&`（編譯期）；凍結後寫入觸發斷言；`Find<T>` 在沒人 `Create<T>` 時回錯誤、不代建；兩個 Module 對同一型別 `Create` 是錯誤；作者／代為兩筆登記照兩種呼叫順序跑，裁決結果相同；兩筆作者 options 不同報錯；換 World 時新的一次建 World 拿到空槽。
- **回滾**：回滾份第 9 節 R-1～R-7 的守衛（含 R-2 等價測試與它列的七個突變）；跨 schedule 的訊息兩方向；沒選策略的 World 沒有歷史（R-4）；換 World 歷史不跨世代（R-5）；`BuildNewFirst` 失敗時舊 World 的回滾照常（5.1）；超過深度丟掉並記診斷；Plan 版本變了歷史作廢（回滾份 7.5）。
- **scenario 第 3 段**（`tests/common/scenario/`，ctest `scenario.segment3`，標籤 `cpu`）：

| 步 | 做什麼 | 檢查 |
|---|---|---|
| S3-1 | 一個 Product 兩個 World：Match（伺服器權威、不回滾）、Client（伺服器權威＋官方回滾），Client 多登記 `HitFlash` | 兩邊一致性指紋相同；Client 改 `HitFlash` 的登記指紋不變、改判定登記就不同 |
| S3-2 | Client 先跑 6 步（預測），Match 用不同的輸入跑同樣 6 步 | 兩邊 digest 不同 |
| S3-3 | 把 Match 第 1 步的判定值交給 Client 的回滾入口 | Client 重跑 5 步；重跑後 digest 等於 Match；「播腳步聲」只在第一次跑時數到 |
| S3-4 | 同 S3-3，但交的值和歷史相同 | 不回滾（預設模式） |
| S3-5 | 第 3 步刪掉一個實體、第 1 步的權威值要它還活著 | 實體復活（配發器範圍內的 id）、OnAdd 與 `Add<T>` observer 在全部放回之後跑 |
| S3-6 | 選 P2P 佔位策略的 World | 啟動錯誤，訊息寫明還沒實作 |
| S3-7 | 單機 World（不選策略） | 沒有歷史、沒有權威檢查，第 1、2 段照過 |
| S3-8 | 同一份存檔讀兩次，存檔型別含 `float` 與 `std::vector`（作者寫雜湊函式） | 摘要相同 |

`-scenario-cross-engine-check`（修訂 1 分開寫）：S3-1（協議雜湊，replicon、lightyear）、S3-2～S3-5（預測與回滾、`is_in_rollback`、checksum，lightyear）有同技術的對照；S3-6 的 P2P 在 lightyear 有實作（GYO 只佔位）；S3-7 的「不選就沒有」三家都是（不加 Plugin 就沒有）；S3-8 只有 lightyear 的 checksum 可比（它略過沒登記的元件）。

### 9.4 突變（`recon-a` 第 4 節；D184）

| 分組 | 數 | 處置 |
|---|---|---|
| 退役・寫者數量 | 12 | 退役（`-authority-no-writer-count`） |
| 退役・digest | 2 | 退役 |
| 退役・Z5（第 1 批 L3 的反向守衛） | 2 | 退役（A11） |
| 退役・區規則 | 47 | 退役（含第 1 批 L3／L7 的 7 個反向守衛，A11；`wt-tick-zone-c`） |
| 搬權威・Z6／B3（含漏斗內部） | 15 | 搬到新清單 `authority_mutations.json`，find 隨新實作重寫；`rs-iv-fx16`、`rs-iv-fd9` 等殺它們的案歸 A4（修訂 1） |
| 搬權威・Z8／D-E15 | 3 | 同上 |
| 搬權威・一致性指紋 | 20 | 同上；`rs-b6-cfp-version-1` 照 README 第 2 條（版本常數一個） |
| 搬權威・G-5 匯出 | 5 | 同上（含 `rs-j17-zone-b-listed`） |
| 搬回滾 | 3 | 搬到 `rollback_mutations.json`；`rs-adv-fp-rollback-enabled-dropped` 若回滾的登記不進任何指紋就退役 |
| 留 Kernel・改 find／cases／expect | 19 | 改寫（含 `we-j36-hook-writerpos-zero`、`we-j32-spawn-before-despawn`：只改 find） |
| 合計 | 128 | |

- 碰到的突變照 README 第 9 條補 `guards`（現在 128 個裡只有 18 個有）；退役照第 7 條寫理由，dev_log 一行。
- 「搬」＝在新清單立同一個錯誤的突變、在 Kernel 清單退役；新清單每個都要 `what` 與 `guards`。
- 新突變：Kernel（原語、逐型別雜湊、寫者名單、`Writes` 收元件）、權威、回滾、scenario 第 3 段、`zone_scan` 改格式後的自測。照 README 第 3 條；本批改的 DI 條數 × 5 是上限的參考，超過寫理由。
- 新的三份清單（Kernel 的新增部分、`authority_mutations.json`、`rollback_mutations.json`）各交 Fable 5.2／high 敵對式檢查一次（§14.2）。
- 完整跑的指令（`run_mutations` 的清單參數或包裝的腳本）加上兩份新清單，由統合在第 3 步接好（I17）。
- 每步只跑相關突變；合併前背景完整跑一次（Kernel＋編排層＋`world_time`＋權威＋回滾＋`zone_scan`）。只有預期的案例失敗才算殺。

### 9.5 綁實作的守衛（`-guards-after-mechanism-stable`）

- 第 3 批延到本批開頭的三條（6 號加時鐘的識別字；`naming_guard` 加 `engine/world_time`；`world_time` 的 `gyo_expect_links`）：條件「scenario 第 2 段通過、合併前核准之後」已成立，**第 1 步立**。時鐘的識別字用限定程式的寫法（`engine/kernel` 還有 11 處「Kernel 不認得時鐘」的說明註解含 `clock`，`recon-a` 5；修訂 1，檢查小 4）。
- 本批新的（Kernel 的掃描加「拿掉的名字不回來」、`naming_guard` 加 `engine/authority`、`engine/rollback`、兩個 Module 的 `gyo_expect_links`、回滾 R-3 與權威 DI-A 2 的方向掃描）：**延到第 5 批開頭**，條件是 scenario 第 3 段通過、本批合併前核准之後（和第 2、3 批同一個處理）。這段期間靠文件、審查與判定 4 的一次性掃描（含方向的掃描）。

---

## 10. 不拆卡：本卡一張串著做（使用者 2026-11-20 定，第 12 節第 5 件）

**決定**：權威與回滾不拆，由本卡一張串著做。原話：「要分開弄嗎？權威和回滾？」（side chat 先建議改成統合＋回滾子卡兩張）→「一起做。」→ side chat 確認「不拆、一張卡做？」→「對的不拆」。比過：拆兩張子卡（修訂 1，本卡當統合）／統合＋回滾子卡（side chat 建議；關鍵路徑和兩張一樣）／不拆（**選這個**）。理由：使用者沒有另外說。

**順序照依賴**：回滾的公開登記與讀取介面 → 回滾的策略與檢查 → 權威（權威的代為登記、5.11 的 Z6／Z8／D-E15 都讀回滾的公開登記；統合第 8 節的線內先後也是「回滾的公開登記 → 權威」）。拿掉 Kernel 規則照 7.2 分兩段：沒有新家的在第 2 步，有新家的在第 4、5 步的接手測試寫好之後（第 6 步）。

### 10.1 步與步之間的交界

拆卡時的關聯表留作「先做的步定形狀、後做的步照用」的清單；後面的步碰到要改前面定好的形狀時，照 §14 冒頭停下回報，不順手改。

| # | 交界 | 在哪一步定、實作 | 哪一步用 |
|---|---|---|---|
| I1 | 逐型別雜湊的客製點、`Hasher`、Kernel 附的函式、「照位元組」的寫法 | 第 1 步 | Kernel 的摘要；第 5 步的 digest |
| I2 | 原語 1～4（含一批放回的 hook 時機）、`ExclusiveWorld` 讀目前 stamp | 第 2 步 | 第 4 步 |
| I3 | `Writes<…>` 收元件、`annotatedWrites` 的形狀與指紋編碼 | 第 2 步 | 第 5 步（指紋、Z6、tick 檢查） |
| I4 | 訊息寫者名單 | 第 2 步 | 第 4 步（跨 schedule 訊息） |
| I5 | Module 與作者的接法（第 6 節：共用槽與它的四點機制） | 第 3 步 | 第 4、5 步、作者 |
| I6 | 回滾的公開登記與讀取介面（六種，R-6）、重複登記的規則、凍結後放進 World 的 Resource | 第 3 步 | 第 4 步（策略、檢查）、第 5 步（5.11、代為登記） |
| I7 | `AuthorityStrategy`、`RollbackStrategy` 的掛點（兩份的契約部分） | 第 3 步（標頭）；第 4、5 步實作 | 作者 |
| I8 | `Tick`、`TickWriters()`、`Resimulation`、`RunOneFixedStep`、`RewindFixedSteps`、`WorldTimeOptions` 的 schedule 名 | 第 3 批（已有） | 第 4、5 步 |
| I9 | 兩個 Module 的 `modules.csv` 列（順序 `world_time` → `rollback` → `authority`）與目錄骨架（含 `tests/common/{authority,rollback}` 與 `tests/common/CMakeLists.txt` 的兩行） | 第 3 步 | 第 4、5 步 |
| I10 | 拿掉 Kernel 規則的先後（7.2） | 第 2、6 步 | — |
| I11 | 突變清單：Kernel、編排層、權威、回滾各一份；搬走的突變在第 6 步從 Kernel 退役、在新清單立 | 各步 | — |
| I12 | 文件：Kernel 的 DI、限制頁、`target/v2/README.md` 與 `docs/architecture/README.md`〈Design Invariants〉的索引、本頁、新頁 `target/v2/authority.zh-Hant.md`、`target/v2/rollback.zh-Hant.md` | 第 4、5、7 步 | — |
| I13 | J-17 的搬移：`tests/common/kernel/ZoneCExportTests.cpp`、`zone_c_fixture/`、`zone_c_export_check.py`、`tests/common/kernel/CMakeLists.txt:88-89`、`:117-123` 的兩個 ctest | 第 5 步在 `tests/common/authority/` 立新的；第 6 步從 Kernel 拿掉 | — |
| I14 | G-5 格式改名與 `zone_scan` 的輸入解析、自測、`mutations.json` | 第 5 步 | 產品的掃描 |
| I15 | 測試用的 Product 組法（帶 `world_time`＋`rollback`＋`authority` 的單位表、策略的選項） | 第 3 步 | 第 4、5 步、scenario 第 3 段 |
| I16 | dev_log 與 HANDOFF：一條線一份 dev_log，本卡寫；HANDOFF 由主會話寫 | 第 7 步 | — |
| I17 | 完整跑的指令加兩份新清單 | 第 3 步 | 第 8 步 |

### 10.2 分支與合併

- 全部在本卡的分支上做，不開子卡、不切別的工作樹。
- 每步完成時推送本分支；審查在第 4、5 步完成時與整條線完成時各做一次（第 14 節），新突變清單的敵對式檢查在第 8 步。
- 整條線**一個 PR**（開到 `gyo2`，`fewer-prs`），**一次合併前核准**；`gh pr merge` 只在使用者明確說「合併」之後（`v8-arch-gyo2-merge-needs-explicit-word`：「好了」「OK」不算）。

---

## 11. Design Invariants 的變更（方向要你核准；最終文字在合併前核准）

### 11.1 Kernel（`target/kernel.zh-Hant.md`）

- **DI-KS 2**：刪 digest 的空窗句。
- **DI-KS 5**：刪「不論區」與 J-44 的「(c) System」措辭。
- **DI-KS 6**：拿掉（寫者數量；統合 9.1）。
- **DI-KS 7**：拿掉（X-2；確認清單第 13 件）。
- **DI-KS 8**：整條拿掉。去處：Z6、Z8 → 權威的交叉檢查；「tick 只有一個來源」→ 權威的專用檢查；「(c) 只從 tick 取時間」→ 不再由啟動檢查守（確認清單第 11 件）；其餘區規則拿掉。
- **DI-KS 9**：刪「(c) 區」的措辭（統合 9.1）。
- **DI-KS 10**（修訂 1，檢查 M10）：「World 摘要（版本 2）…紀錄逐欄雜湊（序號、內容的位元組）」改成版本 3、內容用型別的雜湊函式；〔範圍的限界〕(2) 講 observer 的區規則那段刪。
- **DI-KS 11**：刪「寫入計入獨占」「(c) 的 hook 衝突是啟動錯誤」；守衛 J-34 的前半。
- **DI-KS 12**：改寫成「Kernel 提供回滾用的原語（清單見 DI-KS 15 第 (13) 項），不提供 Rewind／Replay／Restore 與『哪些資料要回滾』的判斷；回滾在回滾 Module」。
- **DI-KS 13、14** 與 **RC 1**：守衛的引用（J-16、J-15、「J-17 掃描」）改指新的位置或刪。
- **DI-KS 15**：第 (1) 項補「World 摘要逐型別雜湊（型別可以帶雜湊函式）」；第 (13) 項照統合 9.1 的清單，標明本批實作 1～4 與 `ExclusiveWorld` 讀目前 stamp、第 5 項只有清單；「轉換中」表裡權威與回滾那幾列刪。這兩處補句**不算職責清單多一項**（使用者 2026-11-20 同意，第 12 節第 4 件）：第 (13) 項的「`ExclusiveWorld` 能讀目前的 stamp」屬「整欄讀出」；第 (1) 項的「型別可以帶雜湊函式、摘要逐型別雜湊、沒有函式時呼叫摘要才報錯」屬「World 摘要」。DI 文字寫 stamp 時加註「ECS 的版本號，不是遊戲的 `Tick`」。
- **DI-KS 19**：Z5 那一句若還在，拿掉（統合 9.1）。
- **DI-KS 25**：唯讀登記表加每種訊息的寫者名單；`Writes<…>` 標註含元件；observer 的「區」欄位拿掉。
- **新**：**摘要逐型別雜湊**：`World::Summary` 對每個值呼叫該型別的雜湊函式，不雜湊原始位元組；整數照 64 位元、小端序；作者選「照位元組」的型別由作者保證位元組＝值；浮點逐位元，作者的型別要 `±0` 相等時自己正規化；沒有雜湊函式的型別在 World 裡時 `Summary` 回錯誤（第 3 節；第 12 節第 1、2 件）。
- **RC**：RC 10（為繞過 DI-KS 6 改 Shared）、RC 12（X-2）拿掉；RC 11（回滾等價破裂）移回滾 Module 的 RC（統合 9.1）；RC 5、RC 9 的 (c) 措辭改寫。**新 RC**（修訂 1，檢查 S9）：逐型別雜湊——作者的函式漏欄位造成的漏報在比摘要或 digest 的測試裡出現兩次以上；或作者為了比摘要而必須為大部分型別寫函式（產品裡要寫函式的型別過半）→ 停下，提「照位元組加檢查」或「反射」的方案。

### 11.2 權威 Module（新頁 `target/v2/authority.zh-Hant.md`）

權威份第 7 節 DI-A 1～11 與 Replacement Criteria 1～5 照搬；DI-A 5 的「C 指紋」改「一致性指紋」；加 **DI-A 12（本卡加）**：digest 的每個值用 Kernel 定的同一個雜湊客製點，合併和順序無關（修訂 1：不寫「含實體 id」，檢查 M4）。

### 11.3 回滾 Module（新頁 `target/v2/rollback.zh-Hant.md`）

回滾份第 9 節 R-1～R-7 與 RC 照搬；R-7 的守衛「Kernel 現有的三處 D-E16 案搬來」照 9.1 A7；加一句：公開登記凍結後放進 World 的 Resource（5.1）；RC 加「記錄改成一般 System 才能並行時（獨占 System 成為並行的牆）」（15.4）。

### 11.4 時間 Module（`target/v2/world-time.zh-Hant.md`）

- WT-1：補「`Resimulation` 每次準備都放成 false，`CarryOver` 不帶它」。
- WT-5：補「伺服器權威策略檢查 `TickWriters()` 名單外的寫者（權威 5.4）」；第 3 批的 G4 註記刪。

### 11.5 編排層（`target/v2/orchestration.zh-Hant.md`；第 12 節第 3 件定 (a)）

- **O-2** 補「每次建 World 有一份以型別為鍵的共用槽。槽裡每種型別只由擁有它的登記單位建（`Create<T>`），別人用 `Find<T>`，找不到是錯誤、不代建；先後由登記檔的依賴與 O-E3 保證，產品自己的 `build` 在最後。登記單位與產品自己的 `build` 在 `Build` 拿 `T&`，`Build` 階段結束時編排層凍結槽，之後 `Finish`、`Check` 只拿 `const T&`，凍結後寫是 Programmer Error。`Build` 階段的寫入只追加、帶來源，裁決在凍結後由擁有者做，結果和呼叫順序無關。編排層不解讀槽裡的型別。建 World 單執行緒依序跑，槽不加鎖。槽在那一次建 World 結束時清掉，要留下的內容由 Module 凍結進 World 的 Resource」（「Create／Find」「凍結」是暫名，實作時照命名規範定；「結束時清掉」「凍結進 Resource」「裁決由擁有者做」是本卡的文字）。守衛：`Finish`／`Check` 拿到的是 `const`（編譯期，`requires` 檢查寫入不成立）；凍結後寫入的斷言；`Find` 找不到回錯誤、不代建；同一組登記兩種呼叫順序裁決結果相同；兩筆作者 options 不同報錯。（(b) 的補句不採。）
- **RC-O10**（新）：建 World 要改成並行（多執行緒同時建，或一個 World 的登記單位並行 `Build`），共用槽需要加鎖或改成別的同步方式。
- **O-4** 不改（不算多一項，使用者 2026-11-20 確認）。

---

## 12. 要你定的

### 第 1 件：沒有雜湊函式的型別怎麼辦（第 3 節）——**已定 (d)**

**決定**（使用者，2026-11-20，side chat）：選 (d)。原話：「選d就好，執行到報錯，這個問題需用作者自己用測試來守」。理由：執行到才報錯；缺函式這件事由作者自己用測試守。下面保留決定前的比較。

**遊戲的例子**：作者登記 `struct Velocity { float x, y; }` 當元件，沒寫雜湊函式，也沒選「照位元組」。之後某個測試比 `World::Summary`。

**別家**（同一種技術＝執行期的狀態 checksum；修訂 1 更正，檢查 M3）：只有 lightyear 有，作者自願登記，**沒登記的元件靜默略過**（`archetypes.rs:65-74`）＝下面的 (b)。不同技術的參考：flecs 的比較 hook，沒有就標成不合法、**呼叫時才斷言**（`type_info.c:588-599`）＝下面的 (d) 的形；Bevy 反射回 `None`；Unity 的 World diff 直接 `MemCmp`。

**比過的選項**：
- **(d) 登記照過，呼叫 `World::Summary` 時才擋**（建議；修訂 1 新增）：`Summary` 遇到 World 裡有沒有雜湊函式的型別，回錯誤並列出那些型別。不比摘要的單機遊戲與工具不用寫任何函式；比摘要的測試第一次跑就知道缺哪幾個。權威的 digest 照權威份 4.3 在 `authority.Component<T>` 登記時編譯期檢查（判定資料一定要有）。代價：錯誤到呼叫時才出現（比 (a) 晚）。系譜：flecs 的「沒有就標不合法、用到才斷言」。
- **(a) 登記時編譯失敗**：除非作者寫了函式或選了照位元組。代價：**回到 11-19 退掉的方向**——那時比過「再加登記時檢查（太硬，浮點與不能平凡複製的型別會被擋）」沒採，LIM-8 也寫「Kernel 不在登記時檢查作者的型別」；(a) 只是多了「寫函式」這個出口。每個產品的每個登記型別都要有函式，包括不比摘要的單機遊戲與工具（`-engine-not-rigid`「限制只放作者自選處」）。
- **(b) 略過、`FrozenRegistry` 交出「沒進摘要的型別」名單**：代價：比摘要的測試對這些型別是盲的，只看結果看不出來。系譜：lightyear（同技術唯一的一家）。
- **(c) 退回原始位元組**：代價：使用者定的正式解法 C 只對寫了函式的型別成立；restore case 5 那種問題在作者的型別上照樣發生；LIM-8 照留。

各選項都一樣的：作者的函式漏寫欄位時，那個欄位的差異看不出來（9.1 A8）。

這件交使用者，因為 (a) 會推翻 11-19 的取捨，而且同技術的唯一參考（lightyear）是 (b)。

### 第 2 件：浮點怎麼雜湊（修訂 1 新增，檢查 M2）——**已定 (i)，加作者正規化的出口**

**決定**（使用者，2026-11-20，side chat）：Kernel 附的 float 雜湊逐位元（NaN 也照位元）；作者的型別若 `±0` 要算相等，在自己的雜湊函式裡正規化（例 `h(v.x + 0.0f)`），或在寫入時正規化（同第 3 批 `VirtualTime.scale` 的做法）。經過：使用者問「這個情況可以避免掉嗎？其實我擔心的是-0.0 和 +0.0在數學上是等價的。依照演算法，不同還真可以能有這種情況。你如何看。」side chat 說明（不是使用者的話）：同一份程式、同樣輸入，IEEE 算出的位元相同，兩端出現 `±0` 之差表示已經分岔；`±0` 只在 `==` 下等價，`1/x`、`atan2`、`signbit` 的結果都不同，會讓後面的計算分岔。使用者回「可以。」理由：使用者同意 side chat 的看法，沒有另外說。下面保留決定前的比較。

**遊戲的例子**：`Velocity{-0.0f, 0}` 與 `Velocity{+0.0f, 0}`。`==` 說相等，位元組不同。

**三種「相等」要一致**：摘要與 digest 用的雜湊（共用一個函式）、回滾份 R-2 的「逐位元相同」、`rollback.Component<T>` 的比較函式（預設 `==`：`±0` 相等、NaN 永不相等）。

| 雜湊的規則 | 和 R-2 | 和 `==` 的比較 | 別家（同技術） |
|---|---|---|---|
| **(i) 逐位元**（建議） | 一致 | 不一致的邊角：伺服器 `-0.0`、Client `+0.0` 時比較說「相同、不回滾」，digest 說「不同」 | lightyear 的自訂函式 `to_bits()`／`to_le_bytes()`、flecs `equals`、Unity `MemCmp` 都是逐位元 |
| **(ii) `±0` 相同、NaN 都相同**（修訂 0 的寫法） | digest 比 R-2 鬆 | NaN 的邊角：比較說「NaN 不等於 NaN、一直回滾」，digest 說相同 | Bevy `FloatOrd`（排序鍵與雜湊表用的包裝，不是狀態雜湊） |

**建議 (i)**：同技術的參考都是逐位元；同一份確定性程式在兩端算出的位元本來就相同，`±0` 不同正是值得抓的分岔；邊角由作者的比較函式處理（要的話用逐位元比）。修訂 0 寫「使用者把 ±0 列為毛病」是錯的：三種毛病是 side chat 列的（第 0 節）。

這件交使用者，因為和同技術的別家是否一致、三種相等要不要對齊，會影響摘要、digest、回滾三處。

### 第 3 件：Module 與作者的接法（第 6 節）——**已定 (a)，並加共用槽的機制**

**補充決定**（使用者，2026-11-20，side chat）：原話「第 3 件，注意，由於是共用的，需要建立一個機制，或者需要保證順序，邊讀邊寫很容易出問題。」side chat 提出四點機制（分段型別上擋、擁有者建別人找、只記錄凍結後裁決、單執行緒不加鎖），使用者回「可以」。比過：只靠呼叫順序與約定／建機制（選）。細節在第 6 節。

**決定**（使用者，2026-11-20，side chat）：(a)，並確認不算 O-4 多一項。原話（依序）：「這個稍微展開一下。」→「給我一個其他家的交叉比較。」→「Build 時哪個build ？ 遊戲編譯時？」→「類似於Module 的 cache？」→「Module的buff，應該更準確。我問一下，組裝Module是誰的工作？」→「buffer 放編排層很合理。可以選（a）」。詳見第 6 節。下面保留決定前的選項。

(a) 每次建 World 一份的共用槽（選這個）／(b) 依賴單位的查詢／(c) 選項包傳共用物件／(d) static 表。選 (a)、(b) 時「不算 O-4 職責清單多一項」是本卡的判斷，請一併看。

### 第 4 件：DI-KS 15（Kernel 職責清單）的兩處補句算不算多一項（修訂 1 新增，檢查 S9）——**已定：兩句都不算**

**決定**（使用者，2026-11-20，side chat）：原話「同意。」之前使用者問了「其他家有這個問題嗎？」「第 3 批拿掉了？當時的理由是什麼？」「stamp 這個東西，Kernel到底要不要？」，side chat 一一說明後同意。比過：兩句都不算多一項（選）／算多一項（停下照 RC 16 處理）。理由：使用者沒有另外說。side chat 的說明（不是使用者的話）：別家的版本號都在 ECS 核心（Bevy change tick、Unity `GlobalSystemVersion`）；摘要本來就在 Kernel，只換算法；函式由作者寫，Kernel 只找、只呼叫。用詞：stamp 是 ECS 的版本號，不是遊戲的 `Tick`；第 3 批拿掉的是 `tickStamps` 對照表，不是 stamp（第 1 節末）。下面保留決定前的判斷。

統合 9.1 的 DI-KS 15 第 (13) 項寫「只限下列……多一種就是清單多一項」。本卡判兩處都**不算**：
- 第 (13) 項加「`ExclusiveWorld` 讀目前 stamp」：判它屬「整欄讀出」（讀出本來就帶 stamp；記錄要知道「現在是哪個 stamp」才能下次比）。
- 第 (1) 項加「型別可以帶雜湊函式、摘要逐型別雜湊」：判它屬「World 摘要」那一項（換摘要的算法，不是新職責）。

這是 L17 必須交使用者的那一類（職責清單）。

### 第 5 件：第 9.1 節 A1～A11 的方向、第 11 節的 DI 文字、9.4 的退役、LIM-7／LIM-8 的方向、第 5.4 節的規模假設、第 10 節的拆卡

- 判定與 DI：方向在動工前核准；逐組的最終核准在合併前帶突變結果與檢證者（Fable 5.2／high）報告再看一次（和第 2、3 批同一個做法）。A1～A3、A10 是已定決定的直接後果，**方向已核准（照建議刪，2026-11-20）**；A4～A7（搬過去時判定的對象變了、A7 的緩的面）**方向已核准（照建議，2026-11-20）**；要特別看、A8（作者的函式漏欄位）、A11（反向守衛退役後可能沒有突變的判定，第 2 步開頭列給你）。
- 規模（1,000 實體 × 4 種）與「一次回滾 ≤ 一步的 20%」是本卡的假設，沒有出處。
- 拆卡：**已定不拆**（使用者 2026-11-20，第 10 節）。
- 規模與預算：**另開唯讀研究卡**（使用者 2026-11-20，第 5.4 節），數字等研究卡的結果由使用者定。

### 本卡照 L17 判的（各家一致或細節；不逐條問）

**不參考 master 的算法與數據**（使用者 2026-11-20 定，side chat）：權威與回滾 Module 從頭照 GYO2 的新架構與開源參考（lightyear、bevy_replicon、Unity Netcode、Bevy、flecs）做；不用 master 的算法、常數與量測數據（含 `LocalPlayerPrediction`、`AuthorityDigest` 的情境、`pvp_baseline` 的數字）。2026-09-25 的 dev_log 是 10/10 事故前的數據，不准引用。side chat 提過的「用 master 數據改 5.4 的規模與預算、修正照 master 的 0.1 秒、digest 情境照 `AuthorityDigest`、估計下修」全部不採。原話：「算了，重做吧，我怕你把master的問題帶到新架構裡來。（特別你還踩空了09_25 的dev_log，那個是10/10事故前的數據。不准）」本頁修訂 1 沒有引用 master（`git grep -i -E "master|09_25|pvp_baseline|LocalPlayerPrediction|AuthorityDigest"` 0 命中，同一指令以 `lightyear` 正例命中 25 行）。5.4 的規模與預算另開唯讀研究卡：master 2026-11-01 之後的數據只當**基準**（以前到哪、現在要到哪），不拿它的算法、常數當設計的根據；數字由使用者定（第 5.4 節，使用者 2026-11-20 補）。


- 原語有型別、放在 `ExclusiveWorld`、一批放回後才跑 hook（2.1）；原語 5 只留清單（2.1）；順序圖的閉包由回滾自己算（2.2）；訊息寫者名單、`Writes` 收元件（2.2）。
- 「重跑中」歸時間 Module（第 3 批已定）；`Resimulation` 每次準備放 false（2.3）。
- G4 隨區規則拿掉與權威的 tick 檢查收掉；G5 由回滾 Module 存版本、比版本（照 Unity）（2.4）。
- 整數照 64 位元、照位元組不自動套用、容器雜湊內容（3.2）；摘要照順序、digest 和順序無關、相加（3.2、4.4）。
- 權威、回滾 Module `default=yes`（照主會話的判斷，4.1、5.1）；策略經選項包；沒選策略時型別照樣進 Kernel；Build 時的錯誤在 `Check` 報（4.2）；「C 指紋」改名「一致性指紋」、G-5 格式改名（4.3）。
- 回滾的公開登記凍結後放進 World 的 Resource（5.1）；重跑與記錄的位置跟著時間 Module 的設定、刪掉的格比鍵集合（5.2）；預設值照 lightyear（5.3）。
- `rollbackSafe` 拿掉不搬（2.3）；S-13 隨區規則拿掉；具名集合的位元組介面留（7.1）；LIM-7 改寫（第 8 節）；LIM-8 改寫不解除（3.4）。
- 沒有新家的規則先拿、有新家的等接手測試（7.2）。
- 綁實作的守衛：第 3 批延下來的三條第 1 步立，本批新的延到第 5 批開頭（9.5）。

**和兩份已核准宣告字面不同的地方**（請你知道，不另外問；修訂 1 補齊，檢查 M4、M7、S2）：
1. `rollbackSafe` 不搬（回滾份 6.3 寫搬）。
2. 記錄 System 是獨占 System（回滾份 5.5 第 9 點寫一般 System，結論不變）。
3. 原語 5 不實作（統合 9.1「留」，清單照留）。
4. digest 的合併用相加（權威份 5.6 只寫「和順序無關」，相加符合；輸入不含 id 和權威份的前提一致）。
5. D-E16 搬過去後是策略自己的檢查，沒選回滾策略就不查（回滾份 5.1 本來就這樣寫；統合 7.2 寫了放寬面）——不是字面不同，是修訂 0 寫錯，這裡列出提醒。
6. 兩個 Module `default=yes`（兩份沒寫預設；照主會話的判斷）。

---

## 13. 判定（先宣告，結果出來之後不放寬）

- **判定 1：通過**。本機完整 ctest 全過（含 `scenario.segment1`～`segment3`）；L1 三平台（linux-x64、windows-x64、macos-arm64）全過，含 toolchain windows-x64 兩次建置的 `orchestration.restore`（`-summary-padding-a-then-c` 的來源）；`git grep -n -i "summary-padding-a-then-c" -- tests engine` **0 命中**（標註全部拿掉；先在 59fd4da9 命中 14 行當正例，終了碼區分「沒有」與「失敗」）。
- **判定 2：行為**。第 9.3 節的案例全過；S3-1～S3-8 全過。
- **判定 3：檢出力**。三份新突變清單經 Fable 5.2／high 檢查補齊後全殺；第 9.4 節的 128 個照處置後，留下與搬走的全殺；完整跑結果記進 dev_log，活著的逐個說明；分母以執行時的實際清單為準。
- **判定 4：一次性的掃描**（`git grep -n -P`，`engine/kernel`、`engine/orchestration`、`engine/world_time`；每個樣式先在 59fd4da9 命中一次當正例——正例都在 `engine/kernel`，`engine/world_time` 只中第一條——終了碼區分「沒有」與「失敗」）。
  - 照統合附錄的：`\b(Zone|WritePolicy|Rollback)::`、`\bRole::Input\b`、`\bRequireExplicitZones\b`、`\bemitterZone\b`、`\brollbackEnabled\b`、`\bnotInRollback\b`、`\brollbackSafe\b`、`\bingress(Stage)?\b`、`\bDigestSet\b`、`\bCFingerprint\b`、`\bExportZoneCSources\b`、`\bparallelGuardTest\b`。
  - 本卡加的（修訂 1 補，檢查 S1）：`\bRole::State\b`、`\brollbackActive\b`、`\bFunnelWrites\b`、`\bDE(2|9|11|15|16)\b`、`\bXE1\b`、`\bzone\b`、`\bwritePolicy\b`、`\bCFingerprintOf\b`、`\bkCFingerprintVersion\b`、`\bZoneCSourcesOptions\b`、`\bMarkDigestSet\b`、`\bdigestMarks\b`、`\bZoneName\b`、`\bexternal\b`、`"Z[0-9]"`、`\bRollback::Derived\b`、`enum class (Zone|WritePolicy|Role|Rollback)\b`。
  - 以上全部零命中；`StartupCode` 的列舉值等於統合 3.1 的清單（`RegistryTests.cpp:223`）。
  - **方向**：三個目錄裡沒有 `engine/authority`、`engine/rollback` 的 include，也沒有 `Authority::`、`Rollback::` 的名字（R-3、DI-A 2 的常設守衛延到第 5 批，這段期間由這條守）。
- **判定 5：預算**（第 5.4 節）：**數字未定**，等預算基準研究卡的結果由使用者定，定了才算宣告（§14.1：結果出來之前宣告）。修訂 1 的假設是：記錄每步 ≤ 156 µs；一次回滾的總開銷 ≤ 3.125 ms（1,000 實體 × 4 種，本機 Release，各 1,000 次取 p99）。`World::Summary` 不設預算（除錯與測試用），dev_log 記改前改後的時間。超過就停下回報。

---

## 14. 誰做、用什麼、怎麼分步

上段（公開介面、Design Invariants、資料形式）。全部 high、不用 xhigh（`v8-arch-gyo2-batch3-no-xhigh`；碰到 §14 停止或原因不明的失敗，停下回報、提議 xhigh，不自己升檔）。

| 步 | 內容 | 實作者（模型／檔位） | 估計（agent 時間） |
|---|---|---|---|
| 第 1 步 | 第 3 批延下來的三條守衛；逐型別雜湊（I1）與摘要格式 3；9 處「未完全解決」解除；比摘要的現有測試的型別補函式（第 12 節第 1 件定 (d)：不比摘要的測試型別不必補） | Opus 5.5／high | 8～12 h |
| 第 2 步 | A11 的清單交使用者；拿掉沒有新家的規則（A1、A2、A3、A10；9.2 的恆真案掃描）、golden 第一次重產；原語 1～4 與讀 stamp（I2）、`Writes` 收元件（I3）、訊息寫者名單（I4）、`Resimulation` 每次放 false | Opus 5.5／high | 16～22 h |
| 第 3 步 | 第 6 節的接法（I5）；`engine/authority`、`engine/rollback` 骨架、`modules.csv`、I6 回滾的公開登記與讀取介面、I7 的標頭、I15 測試支援、I17 | Opus 5.5／high | 8～12 h |
| 第 4 步 | 回滾：檢查（R-7 Derived、跨 schedule 訊息、巢狀 schedule 的所屬）；官方策略（記錄、判斷、還原、重跑、修正、輸入歷史；R-1、R-2、R-5）、P2P 佔位；新頁 `target/v2/rollback.zh-Hant.md`；預算量測（5.4，數字定了才量） | Opus 5.5／high | 22～34 h |
| 第 5 步 | 權威（權威份第 10 節步 3、4）：登記入口、代為登記、策略、佔位、tick 檢查；一致性指紋、digest、G-5 匯出與 `zone_scan` 的接手（J-17 的搬移）、5.11 交叉檢查；新頁 `target/v2/authority.zh-Hant.md` | Opus 5.5／high | 12～20 h |
| 第 6 步 | 拿掉有新家的規則（A4～A7）、`Role`、C 指紋、golden 第二次重產；突變的處置（9.4） | Opus 5.5／high | 8～12 h |
| 第 7 步 | scenario 第 3 段；Kernel 的 DI、限制頁、world-time、編排層、索引；dev_log | Opus 5.5／high | 5～8 h |
| 第 8 步 | 完整突變、三份新清單的敵對式檢查、§14.4 檢證、審查 | 見下 | 8～12 h |

合計 87～132 h（拆卡時的同一組估計，不拆後串著做，關鍵路徑＝合計；少了交接與併分支的費用沒有另外扣）。超過估計 1.5 倍或單一問題累計超過 2 小時就停下回報。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | Opus 5.5／high | 上段，不用小模型 |
| 審查者（第 4、5 步完成時、整條線） | Fable 5.2／high | 和實作者不同模型 |
| 新突變清單的敵對式檢查（Kernel 新增、權威、回滾） | Fable 5.2／high | §14.2；和定清單的人不同模型 |
| §14.4 檢證者（A1～A11） | Fable 5.2／high | 第 2、3 批同一個指定 |
| 本宣告的敵對式檢查 | Fable 5.2／high | 任務卡的指定 |

---

## 15. 基盤 Architecture 的根據與 Architecture Delta

### 15.1 為什麼算基盤

權威的一致性指紋是兩端握手的內容，一改就不相容；回滾的公開登記與 `RollbackStrategy` 是作者寫策略的插口；Kernel 的原語與逐型別雜湊是每個型別、每個回滾策略都會用的；Module 與作者的接法是之後每個 Module（網路、狀態機、audio）都會用的。網路 Module、狀態機的 (c) 區、讀檔都積在上面（統合 5.1、權威份 9.1、回滾份）。

### 15.2 予見的根據 → 要求

| 根據 | 要求 |
|---|---|
| 名指的 Feature：PvP Match／Client（伺服器權威、預測與回滾）、格鬥遊戲 P2P（佔位）、玩家存檔（讀檔兩次相同）、單機不增加複雜度 | 第 4、5 節；原語 1～4；P2P 用得到的原語在清單裡；逐型別雜湊；不選策略就沒有作用 |
| 參考實作：lightyear（checksum、預測、`Rollback` Resource、App 上的 `ComponentRegistry`）、bevy_replicon（協議雜湊）、Unity Netcode（`GhostUpdateSystem`、`DidChange`、World 的 singleton）、Bevy（`set_last_changed`、`get_added_plugins`、`Ref<T>`）、flecs（type hooks 的 `cmp`／`equals`、不合法時呼叫才斷言） | 原語的形、雜湊的客製點、重跑的位置、Module 與作者的接法 |
| 規模：64 Hz、回滾深度 20、1,000 實體 × 4 種（本卡的假設） | 判定 5 |

### 15.3 Architecture Delta（七項；和統合第 4 節、兩份第 9 節的差分）

1. **Feature**：第 15.2 節第 1 列。
2. **現在的問題**：區、寫者、digest、C 指紋、Z6／Z8／D-E15、D-E16、`Rollback` 列舉都還在 Kernel（`recon-a` 1），Kernel 對單機也執行這些檢查；回滾要的原語 1～4 不在；摘要照原始位元組雜湊（LIM-8）；Module 之間、作者與 Module 之間沒有通道。
3. **變化的邊界**：新 Subsystem `engine/authority`、`engine/rollback`；Kernel 少了區、寫者、digest、回滾的宣告與 C 指紋，多了原語 1～4、讀 stamp、雜湊的客製點、訊息寫者名單，`Writes<…>` 收元件；編排層多了每次建 World 一份的共用槽（第 12 節第 3 件定 (a)）。
4. **影響**：Kernel（第 7 節的檔案）、編排層（`Setup.hpp`、`Product.cpp`）、`world_time`（區參數、`Resimulation`）、`modules.csv`、`tests/common/{kernel,orchestration,world_time,scenario,zone_scan}`、新 `tests/common/{authority,rollback}`、`tests/common/CMakeLists.txt`、Kernel 與編排層的突變清單、新兩份清單；文件見第 11 節、限制頁與索引。
5. **依賴方向**：新增 權威→Kernel、編排層、`world_time`、回滾；回滾→Kernel、編排層、`world_time`；產品→權威、回滾。Kernel、編排層、`world_time` 不依賴兩者。沒有反向。
6. **Ownership**：區、寫者數量、判定資料的讀寫：Kernel → 不由機制守；digest、一致性指紋、G-5、Z6／Z8／D-E15、tick 的單一來源：Kernel → 權威 Module；`Rollback`、D-E16、「重跑時不跑」：Kernel → 回滾 Module；摘要的雜湊：照原始位元組 → 每個型別的函式（型別的作者）。
7. **為什麼不能更小**：只拿掉 Kernel 的規則不給新家，伺服器權威與回滾沒有任何機制（統合第 8 節：守衛不留空窗）；只建新家不拿掉，Kernel 對單機也執行權威規則，違反 `-kernel-is-ecs-no-restrictions`；摘要只補 padding 是已做過的 A，使用者定正式解法是 C；沒有接法，作者用不到兩個 Module。

### 15.4 系譜、退掉的選項、需要它的 Feature（AGENTS §4）

| 決定 | 系譜 | 退掉的 | 決定的理由 | 需要它的 Feature |
|---|---|---|---|---|
| 雜湊函式跟著型別、摘要與 digest 共用 | lightyear 逐元件登記 | 摘要與 digest 各一套；只補 padding（已做的 A） | 一個概念一個實作；使用者定 C | 存檔（讀兩次相同）、PvP digest |
| 照位元組不自動、作者明寫才用 | — | `has_unique_object_representations` 為真就自動（修訂 0） | 自動那條有 `optional`、指標、平台寬度三個漏洞（檢查者實測） | 存檔、PvP digest |
| 沒有函式時（第 12 節第 1 件） | flecs 呼叫時擋／lightyear 略過 | 使用者定 (d)（flecs 的形），2026-11-20 | (a) 登記時編譯失敗、(b) 略過、(c) 退回位元組 | 存檔、單機不增加複雜度 |
| 浮點（第 12 節第 2 件） | lightyear、flecs、Unity 逐位元 | 使用者定 (i) 逐位元＋作者自己正規化的出口，2026-11-20 | (ii) `±0` 相同、NaN 都相同（Bevy `FloatOrd`） | PvP digest、回滾 |
| 原語有型別、在 `ExclusiveWorld`、一批放回 | Bevy；統合 9.1；同步點的 hook 規則 | 型別擦除的整欄複製（Unity 的原始位元組）；逐欄放回、每欄當場跑 hook | 回滾的登記知道 `T`；hook 不看到半還原的 World | PvP Client 回滾、P2P（佔位） |
| G5 由回滾存版本、記錄是獨占 System | Unity Netcode `DidChange(版本)` | Kernel 加 tick→stamp 原語；每型別一個 `Changed<T>` System（lightyear）；**照 Bevy `Ref<T>` 把 stamp 開給一般的 query**（Kernel 的參數多一種；v1 只有 Serial，獨占 System 沒有並行的代價；之後要並行時這是一道牆，寫進回滾的 RC） | 三家都沒有 Kernel 層的對照；一個 System 處理全部型別 | PvP Client 回滾 |
| 重跑掛每幀 schedule 的 `PreUpdate` | lightyear `PreUpdate` 的 `run_rollback` | 掛在步的 schedule（X-11 不准） | 巢狀規則 | PvP Client 回滾 |
| 回滾的登記凍結後放 World 的 Resource | Unity 的 World singleton | Module 物件以 World 名為鍵的表 | `BuildNewFirst` 失敗時舊 World 要照常 | PvP Client 回滾、讀檔 |
| digest 相加、不含 id | lightyear 的 XOR、不含 id | XOR；含 id（修訂 0） | 相同的兩項不抵銷；id 的對映屬網路 Module | PvP digest |
| Module 與作者的接法（第 12 節第 3 件） | lightyear App Resource／Bevy `get_added_plugins` | 使用者定 (a) 編排層的共用槽（lightyear App Resource 的形），2026-11-20 | (b) 查依賴的 Module 物件、(c) 產品經選項包接線、(d) static 表 | PvP（權威＋回滾）、單機時間倒流 |
| 兩個 Module `default=yes` | Bevy `DefaultPlugins`、Unity 內建模組（主會話的判斷） | `default=no`（修訂 0） | 照主會話已定的預設；不選策略就沒有作用 | 單機（不選策略） |
| 沒有新家的規則先拿 | 統合第 8 節的原則 | 全部壓到最後（修訂 0） | 兩個 Module 不在舊規則上開發 | — |

既有的 Test 與守衛能不能不改，**沒有**當作任何一項的決定理由。

---

## 16. 範圍的限界

- 這是宣告，沒有試做；第 14 節的時間是讀程式量估的。
- 行號照三份偵察（59fd4da9），卡片與檢查者抽查了一部分（檢查報告末節）；`recon-b` 2.1「第 1 批已補上」只對了條文，逐欄對程式在第 2 步開頭做；Unity 讀的是非官方鏡像；bevy_ggrs、GGPO 本機沒有。
- 規模與預算（5.4、判定 5）未定：等預算基準研究卡（master 11-01 之後的數據只當基準）與使用者定；不拿 master 的算法、常數當設計的根據。
- 不參考 master 的算法、常數與量測數據；2026-09-25 的 dev_log（10/10 事故前）不准引用（使用者 2026-11-20）。實作也照這條：參考只用 GYO2 的新架構與開源參考。
- 網路 Module 的設計不在本批：digest 何時比、含不含實體 id、伺服器值怎麼搬、對時、tick 跳變的通知（`LocalTimelineShift` 那一種）。本批只做到回滾 Module 退 tick 時用 `RewindFixedSteps`；**tick 跳變的推送式事件不做**：以 tick 為索引的歷史只有回滾 Module 自己的，退回時它自己處理；別的讀者出現時（網路 Module 的輸入緩衝）再加（AGENTS §2）。
- digest 不含實體 id：兩個實體交換判定值時看不出來（4.4）。
- 具名集合的原始位元組介面留著（7.1），沒有用它做比較的 Feature。
- 狀態機的 (c) 區接法（權威份 5.8）只做到 `Writes<…>` 收元件與 `authority.Runs`、`authority.MarkResource`；狀態機本身在第 5 批。
- 「誰實際寫的」除錯工具（L30）不在本批。
- 修訂 1 沒有再送檢查。

---

## 17. 修訂 0 檢查的處理（`declaration-r0-adversarial-check.md`）

| 檢查 | 處理 |
|---|---|
| M1 照位元組的三個漏洞、LIM-8 | 3.2：照位元組不自動、整數照 64 位元、`optional` 雜湊內容；3.4：LIM-8 改寫不解除、不另立 LIM-9 |
| M2 浮點正規化的出處與分類 | 第 0 節更正出處；第 12 節第 2 件交你（三種相等的對照） |
| M3 第 1 件的選項、系譜、11-19 | 補 (d) 並改為建議；(a) 寫明回到 11-19 退掉的方向與代價；別家照同技術重寫 |
| M4 digest 含 id | 4.4：不含 id、延到網路 Module；DI-A 12 不寫 id；第 12 節末列字面不同 |
| M5 作者的入口、選項 | 第 6 節重寫：兩條缺口、共用槽選項（建議）、`get_added_plugins` 是唯讀、`BuildNewFirst` 的風險（5.1） |
| M6 A1～A10 不完整 | A2 補 `ObserverTests:1001`、`ExecutorTests:804`、恆真三案；A4 補漏斗反方向與走訪六案；9.2 改成逐案確認＋關掉規則再掃 |
| M7 A7 的緩的面 | 改寫；第 12 節末列出 |
| M8 未完全解決的數目 | 3.1 改為測試 9 處、程式 4 處；判定 1 改成 `git grep` 0 命中 |
| M9 A 依賴 R | 回滾的公開登記與讀取介面由統合在第 3 步做；I6、估計跟著改 |
| M10 DI 清單漏列 | 11.1 補 DI-KS 2、5、10、13、14、25、RC 1 |
| S1 判定 4 的樣式 | 補 17 條、方向掃描、標明哪些照附錄 |
| S2 `default=no` | 改 `default=yes`，照主會話的判斷 |
| S3 拿掉的先後 | 7.2：沒有新家的第 2 步先拿，有新家的第 6 步 |
| S4 關聯表漏的交界 | I12～I17 新增、I9 補測試目錄與 CMake、I3 補指紋編碼 |
| S5 A4 少兩項 | A4 的緩的面與緊的面補 |
| S6 多欄還原的 hook 時機 | 2.1 原語 2：一批放回、全部放回後才跑 |
| S7 反向守衛退役 | 9.1 A11：第 2 步開頭列給你 |
| S8 預算總量與量法 | 5.4：一次回滾的總量門檻、量法 |
| S9 職責清單的兩個判斷、RC、15.4 | 第 12 節第 4 件；11.1 新 RC；11.3 回滾 RC；15.4 補三列（含 Bevy `Ref<T>` 退掉的理由） |
| S10 Build 時的錯誤 | 4.2：沒選策略也進 Kernel、存起來在 `Check` 報 |
| S11 具名集合的位元組介面 | 7.1：留，寫進範圍的限界 |
| 小 1 | 2.3 與 11.4 一致（WT-1 補一句） |
| 小 2 | 3.2：編碼跨平台、J-32 三平台同值 |
| 小 3 | 4.4 改寫：不再用「因為 Bevy 的 id 兩端不同」 |
| 小 4 | 9.5：時鐘識別字用限定程式的寫法 |
| 小 5 | 2.3、5.2：預設跟著時間 Module 的設定 |
| 小 6 | 9.3 的對照分開寫 |
| 小 7 | 第 0 節改讀到 57c8dc32、動工時再讀 |
| 小 8 | 判定 4 標明照附錄與本卡加的 |
| 小 9 | 第 12 節第 5 件點到 9.4 的退役與 LIM 條文 |
