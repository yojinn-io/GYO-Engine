# 事前宣告：權威 Module＋回滾 Module（`v8-arch-gyo2-authority-rollback`）

更新：2026-11-20（修訂 0）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：等動工前核准。** 核准之前不實作、不改條文與守衛。

性質：實作段第 4 批的事前宣告（AGENTS §3 的 Architecture Delta、§2 的基盤 Architecture、§14.4 的判定變更、Design Invariants 的變更）。11-17 核准的[權威份](authority-module-declaration.zh-Hant.md)（修訂 5 補，7f9bbb85）與[回滾份](rollback-module-declaration.zh-Hant.md)（修訂 6，9418fcf1）是設計的依據，**本頁不改它們定過的事**；本頁只寫：(1) 兩份寫在第 2、3 批之前，和現在的程式逐條對照的結果；(2) 兩份留給實作批的細節；(3) `World::Summary` 改逐型別雜湊；(4) 拿掉 Kernel 的權威規則；(5) 拆卡、步驟、判定。

用詞：「設計段／實作段」「動工前核准／合併前核准」「第 N 步」照第 3 批。標「**本卡照 L17 判**」的是這張卡的判斷（名字照命名規範、細節照別家原始碼），**不是使用者的話**。本卡新加的詞第一次出現時標「本卡加的詞」。

---

## 0. 根據與讀的版本

**程式**：`gyo2` 59fd4da9（#253 之後）。講現況的句子都來自讀程式的偵察（Opus 5.5／high，主 checkout `build/evidence/gyo2-authority-rollback/`）：
- `recon-a-kernel-removal.md`：Kernel 要拿掉、搬走的程式、測試、守衛、突變（腳本與分類資料在 `recon-a/`）。
- `recon-b-primitives-interfaces.md`：回滾要的原語、唯讀登記表、`World::Summary`、`world_time`、編排層的現況。
- `recon-c-reference-engines.md`：lightyear 736e1695、Bevy 6a911653f、flecs 09790b5f7、bevy_replicon d80c849、Unity Entities 1.5.0（4630ff5）、Unity Netcode 7.2.0（c4cabf4）。

**文件**：

| 文件 | 狀態 | 本頁怎麼用 |
|---|---|---|
| [批次表](rebuild-batch-plan.zh-Hant.md)第 4 列 | 主會話 | 範圍 |
| 權威份（修訂 5 補）、回滾份（修訂 6） | **已核准**（`v8-arch-gyo2-five-declarations-approved`） | 內容不改；第 2 節逐條對照 |
| [統合](kernel-app-boundary-declaration.zh-Hant.md)修訂 38：L1～L30、第 3.1～3.5、7.2、8、9.1、16 節與附錄 | 已核准 | 拿掉的清單、DI-KS 的改寫、線內先後 |
| [游標份](event-cursor-declaration.zh-Hant.md)修訂 6、[讀檔份](restore-entry-declaration.zh-Hant.md)修訂 11 | 已核准 | 訊息不回滾、跨 schedule 的訊息檢查；讀檔與回滾的交界 |
| [第 3 批宣告](time-events-declaration.zh-Hant.md)修訂 4 與 [dev_log](../../../dev_logs/2026_11_20_gyo2_time_events.zh-Hant.md) | 已合併（#253） | `Tick`、`Resimulation`、`RunOneFixedStep`、`RewindFixedSteps`、`TickWriters`、預設骨架、G4、G5 |
| [編排層骨架的宣告](orchestration-skeleton-declaration.zh-Hant.md)與 [O-1～O-11](orchestration.zh-Hant.md) | 已合併（#244） | 登記單位、選項包、`Check` |
| HANDOFF 決定表（`origin/claude/gyo2-time-design` eaec7bbc） | 主會話 | 使用者的決定 |
| [限制頁](../limits.zh-Hant.md) LIM-7、LIM-8 | 架構師 | 第 3.2、8 節 |

**使用者的決定**（原話以 HANDOFF 為準）：`-kernel-is-ecs-no-restrictions`、`-authority-is-game-design`、`-authority-strategy-mechanism`、`-authority-own-registration`、`-authority-no-writer-count`、`-confirm-derived-removals`（含 11～13）、`-rollback-module-strategy`、`-strategy-interface-impl`、`-immediate-change-via-exclusive-world`、`-no-runtime-interception`、`-no-ambiguity-detection`、`-dynamic-insert-per-world`、`-module-deps-in-registers`、`-products-pick-modules`、`-engine-not-rigid`、`-layer-duty-lists`、`-events-two-kinds-like-bevy`、`-event-delivery-rule`、`-authority-guard-gap-later`、`-guards-after-mechanism-stable`、`-scenario-tests`、`-scenario-failure-by-contract`、`-scenario-cross-engine-check`、`-summary-padding-a-then-c`、`-mutation-guards-field`、`-confirm-only-differences`、`-batch3-no-xhigh`、`-net-is-mechanism-first`。

---

## 1. 這一批做什麼（先用遊戲講）

**PvP 射擊**，一個 Product 裡放兩個 World 做測試（之後由網路 Module 分到兩台機器）：

- **Match 的 World**：建 World 時選「伺服器權威」策略、不選回滾策略。作者寫 `authority.Component<Health>`、`authority.Component<Position>`、`authority.Mark<Tick>()`。啟動時策略檢查：World 宣告了不准動態插入；除了時間 Module 的 `RunFixedSteps`，沒有別的 System 寫 `Tick`。執行中不攔任何寫入。
- **Client 的模擬 World**：也選伺服器權威，再選官方的回滾策略。它多登記一個畫面用的 `HitFlash`（一般的 `registrar.Component`），所以兩邊的**一致性指紋**（本卡加的詞，就是權威份的 C 指紋，名字見 4.3）只算判定資料、照權威登記的順序編號，兩邊相同、連得上。
- Client 預測第 100～106 個 tick。測試把 Match 第 100 個 tick 的 `Position` 交給回滾 Module 的入口：和 Client 的歷史不同。回滾的獨占 System（掛在 `Main` 的 `PreUpdate`）立起 `Resimulation`、把 `Health`、`Position` 與存活集合當場放回第 100 個 tick、用時間 Module 的 `RewindFixedSteps` 退 tick、再用 `RunOneFixedStep` 重跑 6 步，放下 `Resimulation`。「播腳步聲」掛了「重跑時不跑」，重跑時不出聲。重跑完 Client 和 Match 的 **digest**（判定資料的雜湊）相同。
- **單機 2D 橫版**：不選權威、回滾 Module（產品登記檔關掉，或 World 不選策略），沒有歷史、沒有任何檢查。
- **讀檔測試**：同一份存檔讀兩次，`World::Summary` 相同——這次在 Windows 上也穩，因為摘要不再雜湊原始位元組：每種型別有自己的雜湊函式，padding 不算、`std::vector` 算內容、`-0.0` 和 `+0.0` 相同。

Kernel 不知道「權威」「回滾」「判定資料」是什麼：它只多給幾個有型別的原語（整欄讀、整欄放回、存活集合放回、讀 stamp），並且每種登記的型別帶一個雜湊函式。

---

## 2. 兩份宣告與現在的程式逐條對照

### 2.1 回滾份要 Kernel 給的原語（回滾份 6.1；`recon-b` 第 1 節）

| # | 原語 | 現在 | 本批 |
|---|---|---|---|
| 1 | 整欄讀出（id、added、changed、值） | `ExclusiveWorld` 裡沒有；Host 側要 `Keys`＋`Get`＋`Stamps` 三次查找（`World.hpp:174-200`）；System 側的 `Changed<T>` 只篩不給 stamp | **做**：`ExclusiveWorld` 裡有型別的整欄走訪，同時給 stamp；`ExclusiveWorld` 也能讀目前的 stamp（第 7.2 節 G5 要用） |
| 2 | 整欄放回（沒變的格不寫） | 只能逐格 `Insert`／`Remove`，每次當場改後句柄的 stamp 會前進（`Params.hpp:1093-1097`），做不到「一次還原標同一個 stamp」 | **做**：一次換整欄，用登記的比較函式跳過沒變的格，有變的格標同一個目前 stamp；成員有變的跑 hook 與 observer |
| 3 | 實體存活集合的讀出與放回 | **比回滾份寫的更窄**：`ExclusiveWorld::SpawnWithId` 把配發器範圍內的 id 丟成 `IdInAllotterRange`，`Insert` 對不存活的實體回 false，所以「第 T 步活著、之後被刪、id 由本 World 發的」實體**現在復活不了** | **做**（必要，不只是效能）：放回存活集合，多的跑 despawn 的路、少的復活；復活不經配發器範圍的檢查 |
| 4 | Resource 的存在、值與 stamp 一起讀寫 | 存在與值有；**讀 Resource 的 stamp 沒有任何 API** | **做**：讀 stamp；寫照原語 2 的規則 |
| 5 | 指定 stamp 寫回 | 沒有 | **不做，只留在 DI-KS 15 第 (13) 項的清單**（本卡照 L17 判）：官方策略不用；回滾份把它留給「作者自寫的時間倒流策略」，那是說明用的例子、不是點名的 Feature；AGENTS §2「實作は薄く始める」 |
| 6 | 以上在 `ExclusiveWorld` 裡當場可用、成員變化跑 hook | 當場改的 7 個函式已有，跑 hook 與 observer（`Executor.cpp:567-769`） | 原語 1～4 照這條路接上 |

**原語是有型別的**（本卡照 L17 判）：回滾 Module 的 `rollback.Component<T>` 在登記時知道 `T`，用它產生型別擦除的「讀、比、放回」函式即可；Kernel 不必提供「不知道 `T` 就複製一欄」。系譜：Bevy 的 `set_last_changed`、`EntityWorldMut` 都是有型別的；Unity Netcode 走原始位元組是因為元件都是 unmanaged（`recon-c` 2）。P2P 佔位要的「存整個狀態」也只存登記過的可回滾型別，同樣有型別。

**巢狀跑**：兩層巢狀准跑、沒有數字上限，只有 X-11（同一張 schedule 不能在堆疊上出現兩次，`Executor.cpp:496-501`）。所以**重跑的獨占 System 不能掛在步的 schedule 裡**，要掛在 `Main`（第 5.2 節）。回滾份第 12 節的限界「准不准兩層」由此解答。

### 2.2 唯讀登記表（統合 3.3；`recon-b` 第 2 節）

統合 3.3 列的「缺」（hook、`InitialValue`、schedule 選項與 stage、順序圖、effects、`Writes<…>` 標註）**第 1 批已全部補上**。真正還缺三樣：

| 缺的 | 誰要 | 本批（本卡照 L17 判） |
|---|---|---|
| 每種 `Message` 的**寫者**名單 | 回滾的跨 schedule 訊息檢查（回滾份 7.1） | 加：和讀者名單同形（System 掛載、hook、observer；每次 `BuildPlans` 重算）。現在要從存取自己推，兩份程式重算同一件事 |
| `ExclusiveWorld<Writes<…>>` **只收 Resource**，列元件是 RE4（`Registry.cpp:303-305`） | 權威份 5.8（狀態機的套用 System 用 `Writes<…>` 列出它寫的判定資料，判定資料可以是元件）、5.4、5.5 | 擴成元件與 Resource 都收；Kernel 照舊不解讀（L7）。這是 L7「選填標註」的範圍內 |
| 順序圖不是遞移閉包（`Scheduler.cpp:499-504`） | 回滾的 Derived 檢查（R-7） | **不加**：回滾 Module 自己算閉包（簡單；Kernel 交出邊就夠了） |

### 2.3 被第 2、3 批換掉或已解決的

| 兩份寫的 | 現在 | 本批 |
|---|---|---|
| 「重跑中」Resource 歸誰（回滾份 5.7、統合 15.2：時間不收時退回回滾 Module） | 第 3 批已收：`Resimulation` 由 `WorldTimeModule` 登記、恆在、初始 false、沒有寫者（WT-1，`WorldTimeModule.cpp:106`、`:143`） | **歸時間 Module，定了**。回滾策略寫它。小缺口：準備函式用 `PutIfAbsent`，作者對它 `CarryOver` 時舊值會被帶過來（`recon-b` 6 第 15 列）——改成每次準備都放 false（WT-1 文字不變） |
| 重跑時每一步 tick 怎麼推（回滾份 7.2 第 2 條） | 第 3 批給了 `RunOneFixedStep(world, schedule)` 與 `RewindFixedSteps(world, k)`（`FixedStep.hpp:37-68`），和宣告一致 | 官方策略照用 |
| tick 的型別與驅動者名單（L27、權威份 5.4） | `Tick{value}`、`WorldTimeModule::TickWriters()`＝`{"RunFixedSteps"}`（static）、`RunFixedSteps` 帶 `Writes<Tick, FixedTime, ActiveTime>` | 權威的 tick 檢查照用 |
| 可回滾 schedule 的名字（回滾份 5.4 `rollback.StepSchedule(name, recordStage)`） | 編排層給預設骨架 `Main`、`FixedUpdate`（O-4） | 官方策略的預設：步的 schedule＝`FixedUpdate`、記錄掛在它的 `Last`；作者可換 |
| 事件的名字 `Message`、推送式 `Event`＋`Observer` | 第 3 批已做 | 權威的 `authority.Message<M>`、C 指紋收 observer 的存取，照兩份（已寫 observer） |
| 推送式事件與 observer 帶 zone（第 3 批宣告 4.5 的過渡） | `EventOptions::zone`、`ObserverOptions::zone`、Z6 與寫者數量也涵蓋 observer（`Registry.cpp:1441-1485`） | 隨區規則一起拿掉；Z6 搬權威時含 observer（權威份 5.11 已寫） |
| 策略在建 World 時選定（權威份 4.4、回滾份 5.7） | 編排層的選項包 `WorldDescription::options` 在各 Module `Build` 之前就給（`Setup.hpp:26-49`），時間 Module 用同一條路 | 策略經選項包給，編排層不必為策略改（第 6 節） |
| `RejectedSpawn::rollbackActive`、`emitterZone` | `rollbackActive` 恆 false、沒有讀者；`emitterZone` 還在 | 都拿掉（統合 3.2、權威份 5.10） |
| DI-KS 23「列出的算寫者」、開局的寫者豁免 | 第 1、2 批已拿掉 | 不再處理 |
| Z5、L3、L7 的執行期攔截 | 第 1 批已拿掉（`engine/` 0 命中） | 只剩守它們「不回來」的反向突變（第 9.4 節） |
| `rollbackSafe` | 沒有任何檢查讀它，只進 Registry 指紋（`Fingerprint.cpp:186`） | **拿掉、不搬**（本卡照 L17 判）：回滾份 6.3 寫「移到回滾 Module」，但它沒有對應的公開登記，意思已由「重跑時不跑」涵蓋。這是和回滾份字面不同的地方，第 12 節末列出 |
| Module 之間怎麼拿到彼此的公開登記（權威的 `OnRegister` 代為呼叫回滾、權威的 `Check` 讀回滾與時間） | **沒有通道**：`WorldSetup`／`WorldCheck` 不交出別的登記單位，唯一的例子是 static 的 `TickWriters()`（`recon-b` 5.6） | 第 6 節；**交你定**（第 12 節第 2 件） |

### 2.4 第 3 批留下的空窗

- **G4**（「誰都能寫 `Tick`」沒有守衛；(c) 區的 System 讀不到 `Tick`）：現在 (c) 區 System 帶 `Res<Tick>` 是 D-E7 Z1 的啟動錯誤，run 條件讀 `Resimulation` 也一樣（`recon-b` 4）。**本批收掉**：區規則整個拿掉，沒有 (c) 區了；「只准時間 Module 公開的驅動者寫 `Tick`」由伺服器權威策略的專用檢查守（權威份 5.4）。單機或不選權威的 World 照舊誰都能寫——那是 `-engine-not-rigid` 的結果，寫進 WT-5 的約定（已寫）。
- **G5**（沒有「某個 tick 開始時的 stamp」）：**不加 Kernel 原語，由回滾 Module 自己記**（本卡照 L17 判）。官方策略每次記錄時，用原語 1 讀目前的 stamp，存進自己的歷史；下次只記 `changed` 大於那個 stamp 的格（回滾份 5.5 第 2 點）。三家都沒有 tick→stamp 的對照表：lightyear 每個固定步跑記錄 System、用 `is_changed()`（「這個 System 上次跑之後改過」）；Unity Netcode 備份時存下版本、之後用 `DidChange(備份時版本)`（`recon-c` 4）。GYO 照 Unity 的形（存版本、比版本），理由：記錄是一個獨占 System 處理全部可回滾型別，不是每型別一個 System。

### 2.5 兩份和程式對不上、要在本批定的小處

| 宣告 | 程式 | 處理（本卡照 L17 判） |
|---|---|---|
| 回滾份 5.5 第 5 點「有變的格標**目前** stamp」 | 逐格當場改每格 stamp 不同 | 原語 2 一次放回、標同一個 stamp |
| 回滾份 7.1「讀 L6 的讀者、寫者名單」 | 只有讀者 | 2.2 加寫者名單 |
| 權威份 5.8「`Writes<…>` 列出寫的判定資料」 | 只收 Resource | 2.2 擴成元件 |
| 權威份 4.3「型別要提供雜湊函式（登記時編譯期檢查）」 | 登記 API 沒有雜湊的位置 | 第 3 節：Kernel 的每個登記型別都帶雜湊函式，權威的檢查自動成立 |
| 兩份引的行號（`Fingerprint.cpp:338-458`、`Registry.cpp:1440-1490`、`Scheduler.cpp:500-525` 等） | 第 1～3 批之後都移了（`recon-b` 第 6 節 17 項、`recon-a` 6 第 11 點） | 本頁用 59fd4da9 的行號；兩份不改（已核准的紀錄） |
| 統合附錄「時鐘」組的 `\bclock\b` | 還有 11 處，全是第 3 批寫的「Kernel 不認得時鐘」說明註解 | 不在本批判定 4 的範圍；那一組照第 3 批的判定 4（限定程式的寫法）已過 |

---

## 3. `World::Summary` 改逐型別雜湊

### 3.1 現況（`recon-b` 第 3 節）

`World.cpp:1115-1201`（格式版本 2）：存活實體、每欄每格的 `ValueBytesAt`、每個 Resource 的 `StorageBytes`（配發器也照位元組）、每種訊息的 payload 位元組、游標表，串起來取 SHA-256。三種毛病都在：padding 進雜湊（LIM-8 的來源，Windows restore case 5）；`std::vector`、`std::string` 這類持有堆積指標的型別雜湊到**位址**；`-0.0` 與 `+0.0` 位元組不同。登記 API 與型別擦除的欄都沒有放雜湊函式的位置。比摘要的測試 12 組，其中 4 處標「未完全解決」（`WorldTests.cpp:329`、`ExecutorTests.cpp:362`、`:1342`、`RestoreFlowTests.cpp:140`），golden 只有 J-32（`WorldTests.cpp:541`）。

### 3.2 形狀（本卡照 L17 判，除了 3.3 交你定的那一件）

- **雜湊函式跟著型別走，不跟著呼叫的人走**：Kernel 定一個客製點（本卡加的詞：作者為自己的型別寫一個函式，Kernel 在登記時找到它），登記元件、Resource、`Message` 時取得它，存進型別擦除的欄、Resource 槽、訊息緩衝（多一個「雜湊一格」的函式）。名字在實作時照命名規範定（暫名 `HashValue(Hasher&, const T&)`，經 ADL 找）。系譜：lightyear 每個元件型別登記一個 `fn(&C, &mut Hasher)`（`deterministic.rs:10-94`）；Bevy 用 `#[reflect(Hash)]` 或自訂函式（`bevy_reflect/derive/src/lib.rs:195-198`）。
- **同一個函式給兩個人用**：Kernel 的 `World::Summary` 與權威 Module 的 digest。一個概念一個實作（使用者 11-19 對時間的原話「一個概念不要分為2個實作」同理）。權威份 4.3「登記時編譯期檢查」因此自動成立。
- **Kernel 附的雜湊**：整數、列舉、`bool`、`EntityId`；**浮點把 `-0.0` 當 `+0.0`、所有 NaN 當同一個值**（照 Bevy `FloatOrd` 的 Hash，`bevy_math/src/float_ord.rs:81-93`；理由：使用者 11-19 把「+0／-0 位元組不同」列為要解的毛病）；`std::array`、`std::vector`、`std::string`、`std::optional`、`std::pair` 雜湊內容；Kernel 自己的型別（`RangeAllotter`、游標表、訊息紀錄）在 Kernel 裡寫好。
- **沒有 padding 的型別**（`std::has_unique_object_representations_v<T>` 為真）**預設照位元組雜湊**——位元組就是它的值，沒有 padding、沒有浮點、沒有堆積指標。作者為這種型別什麼都不用寫。
- **編碼固定**：每個值照固定寬度、小端序寫進雜湊的輸入，三平台相同；最後照舊取 SHA-256（格式版本 2→3）。
- **摘要照順序雜湊**（實體依 id、欄依登記順序），照舊含 id；它是同一個行程裡比兩個 World 用的。**digest 照權威份 5.6 用和順序無關的合併**（第 4.3 節），兩者共用的只有每個值的雜湊函式。
- `KernelSettings` 照舊不進摘要。

### 3.3 沒有雜湊函式、也不是「沒有 padding」的型別怎麼辦——**交你定**（第 12 節第 1 件）

例：`struct Velocity { float x, y; }`（有浮點，`has_unique_object_representations` 為假）。各家做法不同（`recon-c` 1）：lightyear 靜默略過；Bevy 回 `None`（不支援）；flecs 沒有 `cmp`／`equals` 時呼叫就斷言；Unity 的 World 比較直接 `MemCmp`（padding 照算）。

### 3.4 LIM-8 留不留

**看第 12 節第 1 件的結果**（本卡判）：
- 選 (a)（建議）：LIM-8 的三種毛病都解了，LIM-8 標「已解除」。剩一個較窄的限界：沒有 padding 的型別若含**裸指標或句柄**，照位元組雜湊會算到位址值（同一個行程內穩定、跨行程不同）——寫成新的 LIM-9，解除條件「出現需要跨行程比摘要、又有這種型別的 Feature」。
- 選 (b)：LIM-8 改寫成「沒給雜湊函式的型別不進摘要」。
- 選 (c)：LIM-8 照留，只對給了函式的型別解除。

限制頁的 owner 是架構師會話；條文在本批合併前核准時一起給你看。

---

## 4. 權威 Module：照權威份，加上實作的細節

範圍照權威份第 4、5 節，不改。下面只寫實作時要定、而權威份寫「暫名」或「實作批定」的。

### 4.1 位置與登記

- `engine/authority`、`GYO::Engine::Authority`、target `GYO::Authority`、登記單位 `AuthorityModule`（`Orchestration::Module` 的子類）；`modules.csv` 加一列，`depends=base;kernel;orchestration;world_time;rollback`，`default=no`（本卡照 L17 判：單機不帶它；`-products-pick-modules`。Bevy 的網路套件、lightyear 都不在 `DefaultPlugins` 裡）。
- 登記檔的順序：`world_time` → `rollback` → `authority`（O-6 不重排，權威要排在依賴之後）。

### 4.2 策略的插口

- `AuthorityStrategy`（虛函式的抽象基底類別，L21）、`ServerAuthorityStrategy`、佔位 `PeerRollbackStrategy`（名字實作時照命名規範定）。
- 選法：產品在 `WorldDescription::options` 放一個帶策略物件的選項結構（本卡照 L17 判：時間 Module 已用同一條路；Unity 建 World 時帶旗標）。World 有權威登記卻沒放策略：`AuthorityModule::Check` 報權威自己的錯誤碼（O-5，字串碼 `AU-E…`）。
- 選項包同一份 `WorldDescription` 在 `ReplaceWorld` 時重用：策略的狀態在 `Build` 時重設（O-2「以 World 名為鍵記的資料在 Build 時重設」）。

### 4.3 名字（照命名規範；本卡照 L17 判）

權威份的「C 指紋」是舊 Kernel 的名字（`CFingerprint`，C＝(c) 區），區拿掉之後不再有意思。權威份 4.4 已用 `ConsistencyFingerprint(view)`；本批照它，文件裡叫「一致性指紋」。G-5 的匯出格式 `gyo.zone-c-sources` 改名 `gyo.authority-sources`；掃描工具 `tests/common/zone_scan/` 的位置與名字在權威那份實作定（輸入的產生者從 Kernel 換成權威 Module；ZS1～ZS7 本身不是區規則，`recon-a` 3）。

### 4.4 一致性指紋、digest、tick 檢查、交叉檢查

照權威份 5.4、5.5、5.6、5.9、5.11。實作細節：
- **收哪些 System**：讀唯讀登記表的 `access.writes`、`annotatedWrites`（2.2 擴成元件之後）、hook 與 observer 的 `access`；經 `InitialValue` 的間接寫（舊漏斗，權威份 5.11 第 1 列的 B3）從 `InitialValueInfo` 重算——Kernel 現在的 `FunnelWrites`（`Registry.cpp:1235-1253`）在 Kernel 拿掉 Z1～Z7 後只剩 Z6 用，跟著搬到權威（`recon-a` 6 第 10 點）。
- **digest 的合併**（權威份 5.6「和順序無關」）：每個「實體 × 判定型別」各算一個雜湊，**輸入含實體 id 與型別在權威登記中的序號**，結果**相加（mod 2^64）**。本卡照 L17 判，和 lightyear 不同的兩處與理由：(1) lightyear 用 XOR 且不含 id（`checksum.rs:6-7`、`:483-486`），兩個相同的「實體×元件」會互相抵銷、兩個實體交換值看不出來（`recon-c` 1.1 推論）；相加不會讓相同的兩項抵銷。(2) lightyear 不含 id 是因為 Bevy 的實體 id 在兩端不同；GYO 有權威端時編號只由權威端給（LIM-7 的 PvP 形式），兩端相同。Resource 進 digest 是權威份已定的擴充。
- **digest 何時比、不一致怎麼辦**：照權威份延到網路 Module；本批只在回歸測試與 scenario 第 3 段用。

### 4.5 和第 3 批的接點

- tick 檢查讀 `WorldTimeModule::TickWriters()`（static，權威依賴 `world_time`，可以直接呼叫）。`fixedStep=false` 的 World 名單照樣列 `RunFixedSteps`：名單是「准寫的人」，沒掛也無害。
- 權威份 5.11 的 Z6 對 `Tick` 例外。

---

## 5. 回滾 Module：照回滾份，加上實作的細節

範圍照回滾份第 5、6、7 節，兩批（`-registrations`、`-official-strategy`）都在本批。

### 5.1 位置與登記

- `engine/rollback`、`GYO::Engine::Rollback`、target `GYO::Rollback`、登記單位 `RollbackModule`；`modules.csv` 一列，`depends=base;kernel;orchestration;world_time`，`default=no`（本卡照 L17 判，理由同 4.1）。
- 公開登記六種照回滾份 5.4；讀取介面（R-6）給權威讀。

### 5.2 官方策略掛在哪（本卡照 L17 判）

- 判斷、還原、重跑是一個不帶 `Writes<…>` 的 `ExclusiveWorld` System，掛在 **`Main` 的 `PreUpdate`**（在 `FixedMainLoop` 之前）。系譜：lightyear 在 `PreUpdate` 跑 `run_rollback`（回滾份第 4 節表）。不能掛在 `FixedUpdate` 裡（X-11，2.1）。
- 記錄是不帶 `Writes<…>` 的 `ExclusiveWorld` System，掛在步的 schedule 的 `recordStage`（預設 `FixedUpdate` 的 `Last`）。回滾份 5.5 第 9 點寫「記錄是只寫歷史 Resource 的一般 System」；改成獨占 System 是因為要讀 stamp（G5，2.4），兩種在權威的檢查裡都看不到它寫判定資料，結論不變。
- 刪掉的格：記錄時比這一步與上一筆的鍵集合（本卡照 L17 判：lightyear 用 `RemovedComponents`；GYO 的 `Remove<T>` observer 也做得到，但比鍵集合不必在 observer 裡改歷史，少一條重入的路）。

### 5.3 第一版的設定值

最大回滾深度預設 20（照 lightyear，回滾份 5.5 第 1 點）；判斷的模式三種，預設「比對後不同才回滾」；修正預設指數衰減。PvP 移植時另定。

### 5.4 預算（AGENTS §2；回滾份第 11 節要求官方策略那一批寫）

沒有產品給過預算。照第 3 批的做法，以 64 Hz 一步（15.625 ms）的 1% 為門檻，規模用本卡的假設（**本卡加的假設，核准時請你看**）：1,000 個實體 × 4 種可回滾元件（舊 PvP 的移植目標沒有寫實體數；1,000 是「一場 PvP 的角色、子彈、道具」的上限估計）。
- 記錄：每步 ≤ 156 µs（每步改了全部格的最壞情形）。
- 還原（不含重跑）：退回 20 步、全部格都變的最壞情形 ≤ 156 µs。
- 重跑：成本＝k 步作者的 System，不算 Module 的開銷；只量「重跑 k 步的額外開銷（換 `ActiveTime`、`Resimulation` 的起落）」≤ 156 µs。
本機 Release 量，超過就停下回報（§14），不放寬。

---

## 6. Module 之間的接法——**交你定**（第 12 節第 2 件）

**遊戲的例子**：PvP Client 的作者寫 `authority.Component<Health>()`。伺服器權威策略要在**這一刻**替作者呼叫回滾 Module 的 `rollback.Component<Health>()`（權威份 4.3、4.4：「重跑時不跑」只能在 `AddSystem` 當下給，所以代為登記不能等到 Plan 建好）。之後權威的 `Check` 要讀回滾的公開登記（Z6、Z8、D-E15）。可是現在的權威 Module 拿不到同一個 Product 裡的回滾 Module：登記單位由產生的表建立、Product 持有，`WorldSetup`／`WorldCheck` 不交出別的登記單位。

**別家**：Bevy 有兩種，都在 App 層。(1) `App::get_added_plugins::<T>()` 回傳已加入的 Plugin 參照，文件寫明「讀別的 Plugin 的設定」（`bevy_app/src/app.rs:578-600`）；(2) lightyear 的登記存在 App 的 Resource `ComponentRegistry`，預測套件在 build 時改它（`lightyear/src/protocol.rs:102`、`prediction/src/registry.rs:741-748`）。GYO 在 `Build` 時還沒有 World，(2) 沒有對應；(1) 對應「查同一個 World 裡、登記檔寫了依賴的登記單位」。

**比過的選項**：
- **(a) 編排層給「依賴的登記單位」的查詢**（建議）：`WorldSetup` 與 `WorldCheck` 加 `Unit<T>()`，只准查 `modules.csv` 寫了依賴、而且在這個 World 裡排在前面的單位（否則是 Programmer Error）。權威 `Build` 時拿到 `RollbackModule&`，呼叫它以 World 名為鍵的公開登記。代價：改第 2 批已合併的編排層介面；本卡判**不算 O-4 職責清單多一項**（屬「登記單位與生命週期」「依賴的讀取與檢查」），也不碰 RC-O1（不改迴圈）、RC-O4（編排層不 include 任何 Module）。系譜：Bevy `get_added_plugins`。
- **(b) 產品經選項包傳一個共用物件**：產品建一個回滾登記物件，同時放進兩個 Module 的選項。編排層不改；代價：每個用權威＋回滾的產品都要自己接線，忘了接就是代為登記失效，而且回滾的公開登記變成「產品的東西」，和 R-6「回滾 Module 的公開介面」不一致。
- **(c) 回滾 Module 用 static、以 World 名為鍵的表**（`TickWriters` 的延伸）：編排層不改；代價：行程內全域的可變狀態，同一行程兩個 Product（測試常見）同名 World 會互相蓋掉。

這件是 L17 必須交你的那一類：(a) 動編排層的公開介面，「不算職責清單多一項」是本卡的判斷。

---

## 7. 拿掉 Kernel 的權威規則（統合 3.2、7.2、附錄；`recon-a`）

### 7.1 程式

| 組 | 拿掉的（位置見 `recon-a` 1） | 去處 |
|---|---|---|
| 區 | `Zone`、`ComponentOptions`／`ResourceOptions`／`MessageOptions`／`EventOptions`／`ObserverOptions`／`SystemOptions`／`HookOptions` 的 `zone`、`external`、`RequireExplicitZones`、`ApplyDefaultZones`、`CheckZoneAccess` 與輔助、S-6、S-7、S-8 的 (c) 半、S-9、**S-13**（統合沒點名，但它是 D-E7 的區規則，`recon-a` 6 第 5 點）、S-17、Z1、Z2、Z3、Z7、`ingress`、`ingressStage`、X-2／D-E11／X-E1、`parallelGuardTest`、D-E9、`InitialValue` 的 Z1、`emitterZone`、`SystemContext::zone` | 不由機制守（P14、確認清單） |
| 寫者 | `WritePolicy`、`CheckExclusiveWriters`（D-E2 S-11） | 不由機制守（`-authority-no-writer-count`） |
| digest | `DigestSet`、`MarkDigestSet`（R-E4 的 digest 部分）、S-12、`NamedSetRecord::digest` | 權威 Module 的 digest（第 4.4 節） |
| 輸入 | `Role`（`State`／`Input`） | 權威的 `.input` 標記（權威份 4.3） |
| 交叉 | Z6（System、hook、observer）、Z8、D-E15、`FunnelWrites` | 權威 Module 的 5.11 交叉檢查 |
| 回滾 | `Rollback`（Yes／No／Derived）、`rollbackEnabled`、`notInRollback`、D-E16（`Scheduler.cpp:438-462`）、`rollbackActive`、Replay 的註解 | 回滾 Module 的公開登記與 R-7 |
| 回滾 | `rollbackSafe` | 拿掉（2.3） |
| 指紋 | `CFingerprintOf`、`FrozenRegistry::CFingerprint()`、`ExportZoneCSources`、`ZoneCSourcesOptions` | 權威 Module（一致性指紋、G-5 匯出） |
| 錯誤碼 | `StartupCode` 的 `DE2`、`DE9`、`DE11`、`XE1`；`DE15` → 權威（字串碼）、`DE16` → 回滾（字串碼） | 列舉值等於統合 3.1 的清單（`RegistryTests.cpp:223` 的「轉換中」名單清空） |
| 指紋格式 | Registry 指紋去掉 16 個欄位（格式 7→8）；Plan export 去掉 `registry_c`、`rollback=`、`zone=`（`gyo.kernel.plans` 6→7）；Plan 指紋不變（`recon-a` 1.4） | golden 重產（第三次，統合第 8 節） |
| `world_time` | `WorldTimeModule.cpp:79`、`:100-109` 的 `.zone = Zone::B` 與 G4 的註解 | 拿掉 |

### 7.2 先後：守衛不留空窗

統合第 8 節的線內先後照做：Kernel 給原語 → 回滾的公開登記 → 權威的骨架 → 權威的交叉檢查 → 拿掉 Kernel 的權威與回滾規則。拆卡之後（第 10 節），**拿掉的提交在權威與回滾兩張卡的接手測試都併進本線之後才併**；拿掉的工作可以和兩張卡並行做，但不先併。

---

## 8. LIM-7 重看（批次表「LIM-7 隨區規則移出，在權威＋回滾那條線的宣告裡重看」）

LIM-7 的寫法建立在區上（「(c) 區的生成被不是 (c) 區的東西擋住」）。區拿掉之後，Kernel 的行為是：**生成撞上活著的實體就丟掉並記 `RejectedSpawn`，不論是誰的資料**（F-13，照舊）。改寫（本卡照 L17 判，條文合併前核准時給你看）：
- 限制：生成被活著的實體擋住時照樣丟掉，Kernel 不擋。
- PvP（有權威端）的形式：編號只由權威端給，只在本機存在的實體不和權威共用編號空間（怎麼分開由產品決定）——照舊。
- 回滾這一側（回滾份 7.4 已答）：還原時存活集合與配發器一起退回，重跑的 `Spawn` 拿到和第一次相同的編號；會撞到的只有「本機專用的實體佔了同一個編號」，前一點已排除。
- 解除條件：第一個有權威端的產品證明本機的東西必須和權威共用編號空間。

---

## 9. 守衛、測試、突變

### 9.1 判定的變更（AGENTS §14.4；**逐組要你核准**；最終在合併前帶突變結果與檢證者報告再看一次）

| 組 | 守衛 | 守的性質 | 處置 | 緩的面 | 緊的面 | 之後由誰守 |
|---|---|---|---|---|---|---|
| A1 | J-15、`ZoneRuleTests:151`、`:191`、`CoverageTests:302`、`InitialValueTests:170`、`SkeletonTests:365`、`ObserverTests:871` 的第二寫者半、編排層 `CoverageTests:538`（O-1 的 D-E2） | 一欄一個寫者 | 刪 | 第二個寫者通過 | 無 | 不由機制守（`-authority-no-writer-count`） |
| A2 | J-14（除手寫 Diagnostics 的 S-8 半）、J-38、J-39、J-40（除 Z8 半）、J-41、J-47 的 Z1 案、J-24 的 S-13、D-E9（`SchedulerTests:590`、`WorldTests:940` 後半）、X-E1（`CoverageTests:385`、`SchedulerTests:477` 的 X-E1 半）、`InitialValueTests` 的 Z1／Z2／Z3 九案、`NestedRunTests:1569`、`:1233` 的 S-17 半、`ObserverTests:815`、`:871` 的區半、`RegistrationRoundTests:404` 的兩種、`SpawnRejectTests:291`、`:580` 的 `emitterZone` 判定 | 區規則、S-6～S-17、Z1～Z3、Z7、X-2、D-E9、`emitterZone` | 刪 | 任何 System 讀寫任何資料都通過 | 無 | 不由機制守（P14、確認清單第 1～13 件） |
| A3 | J-16（`ZoneRuleTests:206`）、`RegistrationRoundTests:404` 的 `DigestSet` 半 | digest 集合禁 Shared、R-E4 的 digest | 刪 | 具名集合不再有 digest 的身分 | 無 | 權威 Module 的 digest（每個判定型別都進，作者可排除） |
| A4 | Z6／B3 九案（`InitialValueTests:645`…`:1218`）、Z8（`CoverageTests:254` 的 Z8 半、`ZoneRuleTests:305` 的 Z8 半）、D-E15（`SchedulerTests:626` 的半）、Z6 的混合案（`CoverageTests:254`、`InitialValueTests:1032`） | 判定資料 × 回滾 | **搬到權威、判定改**：對象從「zone C 的 System」改成「寫了判定資料的 System、hook、observer」；從 Kernel 的 `Rollback::Yes` 改讀回滾的公開登記；`Tick` 例外 | 沒選權威的 World 不查；`Tick` 不必可回滾 | 不分「判定的 System」：凡寫判定資料的都查；observer 也查（Z6 已含） | 權威 Module |
| A5 | 一致性指紋（舊 C 指紋）12 案的 C 半（`recon-a` 2.2） | 兩端判定部分相同 | **搬到權威、判定改**（權威份 5.5 的內容表）：編號改權威登記順序；同步點只記有無；before／after、`rollbackEnabled`、zone 不進；`Runs`、策略名與版本、訊息上限進 | 一般資料的登記改了不再影響指紋；同步點改名不影響 | 策略、`Runs`、observer 存取進指紋；兩方向的守衛 | 權威 Module |
| A6 | J-17（`ZoneCExportTests` 兩案、`InitialValueTests:1246`、`:1269`）、`zone_c_export_check.py` | G-5 的來源檔清單 | **搬到權威、判定改**：範圍從「zone C 的 System」改成「寫判定資料的 System、hook、observer、`InitialValue`」 | 讀判定資料、不寫的 System 不再列 | observer 列入 | 權威 Module＋產品的掃描 |
| A7 | D-E16 三處（`CoverageTests:113` 的半、`:312`、`SchedulerTests:626` 的半） | Derived 在讀者之前由立即寫者重算 | **搬到回滾**（R-7）：判定一字不改的部分照搬；加巢狀 schedule 也查 | 無 | 巢狀 schedule 也查 | 回滾 Module |
| A8 | `World::Summary` 的比較 12 組，含 J-32 golden（`WorldTests:541`）、四處「未完全解決」 | 同一個 World 狀態得到同一個摘要 | **改**：照第 3 節逐型別雜湊；J-32 golden 重產；四處「未完全解決」解除 | 只差在 padding、`-0.0`／`+0.0`、NaN 位元的兩個狀態，摘要從「可能不同」變成相同 | 持有堆積的型別改內容就會不同（以前雜湊位址） | Kernel |
| A9 | Registry 指紋 golden 6 處、Plan export 格式（`SchedulerTests:674`、`CoverageTests:411`）、`RegistryTests:223` 的「轉換中」名單 | 格式與錯誤碼清單 | 改：版本升、golden 重產；「轉換中」名單清空 | 無 | 錯誤碼清單只剩保留的 | Kernel |
| A10 | `WorldTimeModuleTests:176` 的 `[wt-time-zone-b]` 兩個 CHECK | 時間型別是 zone B（第 3 批的過渡） | 刪 | 無區可言 | 無 | —（G4 收掉，2.4） |

DI 的文字變更（第 11 節）同樣在這次逐條核准。

### 9.2 判定不變、只換寫法的（沒有緩的面；事後報告 1～5）

- **只拿掉參數的 151 案**（`recon-a` 2.5）與測試的共用支援檔（`KernelTestSupport.hpp:69-114` 等）：拿掉 `Zone::`、`rollbackEnabled` 等參數，判定不動。案名或訊息裡寫「zone C」「Shared」而性質留下的（J-22、J-57、J-32b、J-45、J-52、J-55 的幾案），改名，殺突變的 `cases` 同步改。
- J-37、J-47 的留下兩案（`ZoneRuleTests:224`、`:358`）與 J-14 的手寫 Diagnostics 半搬到別的 Kernel 測試檔；`ZoneRuleTests.cpp` 刪檔。
- `GYO_KERNEL_J_ENTRIES` 拿掉在 Kernel 變成 0 案的 J-15、J-16、J-17、J-38、J-39、J-40、J-41（`CMakeLists.txt:68-71`；0 案的 ctest 會因 `"test cases: +0"` 失敗）。
- `td-despawn-cells-kept`、`td-adv-despawn-dead-id-check-weakened` 期待的 W-1 是結構檢查、不是寫者（`recon-a` 4.5）：拿掉參數後重跑，確認仍 killed。

### 9.3 新增的行為案例

- **Kernel**：原語 1～4（有型別的整欄走訪給 stamp；整欄放回沒變的格不寫、有變的標同一個 stamp、成員變化跑 hook 與 observer、復活配發器範圍內的 id；Resource 的 stamp；`ExclusiveWorld` 讀目前 stamp）；`Writes<…>` 收元件；訊息寫者名單（System、hook、observer、`BuildPlans` 重算）；逐型別雜湊（padding 不同摘要相同、`-0.0`／`+0.0` 相同、NaN 相同、`std::vector` 內容不同摘要不同、內容相同而位址不同時相同、沒有 padding 的型別照位元組、Kernel 附的型別；第 12 節第 1 件的結果）。
- **權威**：權威份第 7 節 DI-A 與第 5 節各條；一致性指紋兩方向（判定登記改了指紋變、一般資料改了不變）；digest（同狀態相同、改一格不同、兩實體交換值不同、迭代順序無關）；tick 檢查（名單外寫 `Tick` 報錯、`Writes<Tick>` 標註也算、時間 Module 的驅動者通過）；佔位策略與沒選策略的啟動錯誤；代為登記在登記當下、作者的為準。
- **回滾**：回滾份第 9 節 R-1～R-7 的守衛（含 R-2 等價測試與它列的七個突變）；跨 schedule 的訊息兩方向；沒選策略的 World 沒有歷史（R-4）；換 World 歷史不跨世代（R-5）；超過深度丟掉並記診斷；Plan 版本變了歷史作廢（回滾份 7.5）。
- **scenario 第 3 段**（`tests/common/scenario/`，ctest `scenario.segment3`，標籤 `cpu`）：

| 步 | 做什麼 | 檢查 |
|---|---|---|
| S3-1 | 一個 Product 兩個 World：Match（伺服器權威、不回滾）、Client（伺服器權威＋官方回滾），Client 多登記 `HitFlash` | 兩邊一致性指紋相同；Client 改 `HitFlash` 的登記指紋不變、改判定登記就不同 |
| S3-2 | Client 先跑 6 步（預測），Match 用不同的輸入跑同樣 6 步 | 兩邊 digest 不同 |
| S3-3 | 把 Match 第 1 步的判定值交給 Client 的回滾入口 | Client 重跑 5 步；重跑後 digest 等於 Match；「播腳步聲」只在第一次跑時數到 |
| S3-4 | 同 S3-3，但交的值和歷史相同 | 不回滾（預設模式） |
| S3-5 | 第 3 步刪掉一個實體、第 1 步的權威值要它還活著 | 實體復活（配發器範圍內的 id）、OnAdd 與 `Add<T>` observer 跑 |
| S3-6 | 選 P2P 佔位策略的 World | 啟動錯誤，訊息寫明還沒實作 |
| S3-7 | 單機 World（不帶權威、回滾 Module） | 沒有歷史、沒有權威檢查，第 1、2 段照過 |
| S3-8 | 同一份存檔讀兩次，存檔型別含 `float` 與 `std::vector` | 摘要相同 |

`-scenario-cross-engine-check`：S3-1～S3-6 lightyear 都支援（協議雜湊、預測與回滾、`is_in_rollback`、checksum；`recon-c`）；S3-8 只有 lightyear 的 checksum 可比（它略過沒登記的元件）。

### 9.4 突變（`recon-a` 第 4 節；D184）

| 分組 | 數 | 處置 |
|---|---|---|
| 退役・寫者數量 | 12 | 退役（`-authority-no-writer-count`） |
| 退役・digest | 2 | 退役 |
| 退役・Z5（第 1 批 L3 的反向守衛） | 2 | 退役：replace 用到拿掉的欄位、編譯不過；「不回來」改由型別保證（欄位不存在） |
| 退役・區規則 | 47 | 退役（含第 1 批 L3／L7 的 7 個反向守衛，理由同上；`wt-tick-zone-c`） |
| 搬權威・Z6／B3（含漏斗內部） | 15 | 搬到新清單 `authority_mutations.json`，find 隨新實作重寫 |
| 搬權威・Z8／D-E15 | 3 | 同上 |
| 搬權威・一致性指紋 | 20 | 同上；`rs-b6-cfp-version-1` 照 README 第 2 條（版本常數一個） |
| 搬權威・G-5 匯出 | 5 | 同上（含 `rs-j17-zone-b-listed`） |
| 搬回滾 | 3 | 搬到 `rollback_mutations.json`；`rs-adv-fp-rollback-enabled-dropped` 若回滾的登記不進任何指紋就退役 |
| 留 Kernel・改 find／cases／expect | 19 | 改寫（含 `we-j36-hook-writerpos-zero`、`we-j32-spawn-before-despawn`：只改 find） |
| 合計 | 128 | |

- 碰到的突變照 README 第 9 條補 `guards`（現在 128 個裡只有 18 個有）；退役照第 7 條寫理由，dev_log 一行。
- 「搬」＝在新清單立同一個錯誤的突變、在 Kernel 清單退役；新清單每個都要 `what` 與 `guards`。
- 新突變：Kernel（原語、逐型別雜湊、寫者名單、`Writes` 收元件）、權威、回滾、scenario 第 3 段。照 README 第 3 條（每個獨立的決定一個）；本批改的 DI 條數 × 5 是上限的參考，超過寫理由。
- 新的三份清單（Kernel 的新增部分、`authority_mutations.json`、`rollback_mutations.json`）各交 Fable 5.2／high 敵對式檢查一次（§14.2）。
- 每步只跑相關突變；合併前背景完整跑一次（Kernel＋編排層＋`world_time`＋權威＋回滾）。只有預期的案例失敗才算殺。

### 9.5 綁實作的守衛（`-guards-after-mechanism-stable`）

- 第 3 批延到本批開頭的三條（6 號加時鐘的識別字；`naming_guard` 加 `engine/world_time`；`world_time` 的 `gyo_expect_links`）：條件「scenario 第 2 段通過、合併前核准之後」已成立，**第 1 步立**。
- 本批新的（Kernel 的掃描加「拿掉的名字不回來」的式子、`naming_guard` 加 `engine/authority`、`engine/rollback`、兩個 Module 的 `gyo_expect_links`、回滾 R-3 的 include 掃描）：**延到第 5 批開頭**，條件是 scenario 第 3 段通過、本批合併前核准之後（和第 2、3 批同一個處理）。這段期間靠文件、審查與判定 4 的一次性掃描。

---

## 10. 拆卡：先立統合，再分頭寫（`v8-arch-gyo2-boundary-split-integrator-first`；本卡判）

**判斷：拆成兩張子卡，本卡當統合。** 理由：
- 規模：本卡估 81～122 h（第 14 節）。一張卡串著做，關鍵路徑上的權威（12～20 h）與回滾（24～36 h）可以並行。
- 兩個 Module 的程式在新的目錄（`engine/authority`、`engine/rollback`、各自的測試與突變清單），彼此不搶檔案；搶檔案的都是基盤（Kernel、編排層、Kernel 的突變清單、`modules.csv`），留在統合。
- 拆開後關聯容易被推給別份（記憶 `split-large-task-integrator-first`），所以先立關聯表（10.2），改基盤介面的全部留在統合。

### 10.1 三張卡做什麼

| 卡 | 做什麼 | 檔案 |
|---|---|---|
| **本卡（統合）** | 第 1～3 步：基盤（逐型別雜湊、原語、登記表兩樣、第 6 節的接法、兩個 Module 的骨架與 interface 的形狀、關聯表）；第 4 步：拿掉 Kernel 的權威規則（和子卡並行做、最後併）；第 5、6 步：scenario 第 3 段、文件、驗證 | `engine/kernel`、`engine/orchestration`、`engine/world_time`、`modules.csv`、`tests/common/{kernel,orchestration,world_time,scenario}`、Kernel 與編排層的突變清單、`target/kernel.zh-Hant.md`、`limits.zh-Hant.md`、本頁 |
| **卡 R（回滾）** | 回滾份第 11 節兩批：公開登記與檢查（R-3、R-4、R-6、R-7、跨 schedule 訊息）、官方策略（記錄、判斷、還原、重跑、修正、輸入歷史；R-1、R-2、R-5）、P2P 佔位 | `engine/rollback/`、`tests/common/rollback/`、`rollback_mutations.json`、新頁 `target/v2/rollback.zh-Hant.md` |
| **卡 A（權威）** | 權威份第 10 節步 3、4：登記入口、代為登記、策略、佔位、tick 檢查、一致性指紋、digest、G-5 匯出與掃描工具的接手、5.11 交叉檢查 | `engine/authority/`、`tests/common/authority/`、`authority_mutations.json`、`tests/common/zone_scan/` 的接手、新頁 `target/v2/authority.zh-Hant.md` |

### 10.2 關聯表（統合定、子卡引用；改這張表只由統合）

| # | 交界 | 誰定形狀 | 誰實作 | 誰用 |
|---|---|---|---|---|
| I1 | 逐型別雜湊的客製點與 `Hasher` | 統合 | 統合 | Kernel 的摘要；A 的 digest |
| I2 | 原語 1～4、`ExclusiveWorld` 讀目前 stamp | 統合 | 統合 | R |
| I3 | `Writes<…>` 收元件 | 統合 | 統合 | A（指紋、Z6、tick 檢查） |
| I4 | 訊息寫者名單 | 統合 | 統合 | R（跨 schedule 訊息） |
| I5 | Module 之間的接法（第 6 節的結果） | 統合 | 統合 | A（代為登記、`Check` 讀回滾） |
| I6 | 回滾的公開登記與讀取介面（六種，R-6） | 統合（標頭與語意） | R | A（5.11、代為登記） |
| I7 | `AuthorityStrategy`、`RollbackStrategy` 的掛點（兩份的契約部分） | 統合（標頭） | A、R | 作者 |
| I8 | `Tick`、`TickWriters()`、`Resimulation`、`RunOneFixedStep`、`RewindFixedSteps` | 第 3 批（已有） | — | A、R |
| I9 | 兩個 Module 的 `modules.csv` 列與目錄骨架 | 統合 | 統合 | A、R |
| I10 | 拿掉 Kernel 規則的先後 | 統合 | 統合 | 在 A、R 的接手測試併進本線之後才併（7.2） |
| I11 | 突變清單 | Kernel 與編排層的清單只由統合改；搬走的突變由統合退役、A／R 在自己的清單立 | 各自 | — |
| I12 | 文件 | 統合寫 Kernel 的 DI、限制頁、本頁；A、R 各寫自己的頁 | 各自 | — |

子卡碰到要改 I1～I7 的地方：停下、回報統合，不自己改。

### 10.3 分支與合併

- 統合的分支就是本卡的分支。第 3 步完成、推送之後開兩張子卡，各自從那個提交切工作樹。
- 子卡完成時各自做審查、新突變清單的敵對式檢查，把分支交回；統合在使用者同意後把兩條分支併進本線（不 push 子卡的分支到別處）。
- 整條線**一個 PR**（開到 `gyo2`，`fewer-prs`），**一次合併前核准**。

---

## 11. Design Invariants 的變更（逐條要你核准；最終文字在合併前核准）

### 11.1 Kernel（`target/kernel.zh-Hant.md`）

- **DI-KS 6**：拿掉（寫者數量；統合 9.1）。
- **DI-KS 7**：拿掉（X-2；確認清單第 13 件）。
- **DI-KS 8**：整條拿掉。去處：Z6、Z8 → 權威的交叉檢查；「tick 只有一個來源」→ 權威的專用檢查；「(c) 只從 tick 取時間」→ 不再由啟動檢查守（確認清單第 11 件）；其餘區規則拿掉。
- **DI-KS 9**：刪「(c) 區」的措辭（統合 9.1）。
- **DI-KS 11**：刪「寫入計入獨占」「(c) 的 hook 衝突是啟動錯誤」；守衛 J-34 的前半。
- **DI-KS 12**：改寫成「Kernel 提供回滾用的原語（清單見 DI-KS 15 第 (13) 項），不提供 Rewind／Replay／Restore 與『哪些資料要回滾』的判斷；回滾在回滾 Module」。
- **DI-KS 15**：第 (1) 項補「World 摘要逐型別雜湊（每個登記的型別帶雜湊函式）」；第 (13) 項照統合 9.1 的清單，標明本批實作 1～4 與 `ExclusiveWorld` 讀目前 stamp（屬「整欄讀出」的 stamp），第 5 項只有清單；「轉換中」表裡權威與回滾那幾列刪。
- **DI-KS 19**：Z5 那一句若還在，拿掉（統合 9.1）。
- **DI-KS 25**：唯讀登記表加每種訊息的寫者名單；`Writes<…>` 標註含元件。
- **新**：**摘要逐型別雜湊**：`World::Summary` 對每個值呼叫該型別的雜湊函式，不雜湊原始位元組；浮點的 `-0.0`／`+0.0` 相同、NaN 相同；編碼固定寬度、小端序（第 3 節；第 12 節第 1 件的結果寫進這條）。
- **RC**：RC 10（為繞過 DI-KS 6 改 Shared）、RC 12（X-2）拿掉；RC 11（回滾等價破裂）移回滾 Module 的 RC（統合 9.1）；RC 5、RC 9 的 (c) 措辭改寫。

### 11.2 權威 Module（新頁 `target/v2/authority.zh-Hant.md`）

權威份第 7 節 DI-A 1～11 與 Replacement Criteria 1～5 照搬；DI-A 5 的「C 指紋」改「一致性指紋」；加一條 **DI-A 12（本卡加）**：digest 的每個值用 Kernel 登記的同一個雜湊函式，合併和順序無關、含實體 id 與權威登記序號（第 4.4 節）。

### 11.3 回滾 Module（新頁 `target/v2/rollback.zh-Hant.md`）

回滾份第 9 節 R-1～R-7 與 RC 照搬；R-7 的守衛「Kernel 現有的三處 D-E16 案搬來」照 9.1 A7。

### 11.4 時間 Module（`target/v2/world-time.zh-Hant.md`）

- WT-1：補「`Resimulation` 每次準備都放成 false，`CarryOver` 不帶它」。
- WT-5：補「伺服器權威策略檢查 `TickWriters()` 名單外的寫者（權威 5.4）」；第 3 批的 G4 註記刪。

### 11.5 編排層（`target/v2/orchestration.zh-Hant.md`；只在第 12 節第 2 件選 (a) 時）

- **O-2** 補「登記單位在 `Build`、`Finish`、`Check` 可以查登記檔寫了依賴、在同一個 World 裡排在前面的登記單位（`Unit<T>()`）；查別的是 Programmer Error」。
- **O-4** 不改（判不算多一項）。

---

## 12. 要你定的

### 第 1 件：沒有雜湊函式、也不是「沒有 padding」的型別怎麼辦（第 3.3 節）

**遊戲的例子**：作者登記 `struct Velocity { float x, y; }` 當元件，沒寫雜湊函式。它有浮點，Kernel 不能照位元組雜湊（`-0.0` 與 `+0.0` 會不同）。

**別家**（`recon-c` 1）：lightyear 只雜湊登記了函式的元件，沒登記的靜默略過（`archetypes.rs:65-74`）；Bevy 回 `None`、代表不支援（`reflect.rs:341-353`）；flecs 沒有比較函式時呼叫就斷言（`type_info.c:588-599`）；Unity 直接 `MemCmp`。四家都不同。

**比過的選項**：
- **(a) 登記時編譯失敗**（建議）：除非型別沒有 padding（照位元組）或作者寫了函式。Kernel 附整數、浮點、`EntityId`、常用容器的函式，作者為含浮點的結構寫一個逐欄位呼叫的函式（例：`h(v.x); h(v.y);`）。三種毛病都解，LIM-8 解除（3.4）。代價：每個含浮點或容器的元件、Resource、訊息都要寫一個函式，現有測試與時間型別也要補（時間型別大多是整數奈秒，`VirtualTime.scale` 等少數要寫）。系譜：flecs（沒有就擋）。
- **(b) 略過、啟動時列出**：沒有函式的型別不進摘要與 digest，`FrozenRegistry` 交出「沒進摘要的型別」名單。代價：比摘要的測試對這些型別是盲的，而且只看結果看不出來。系譜：lightyear。
- **(c) 退回原始位元組、LIM-8 照留**：只有寫了函式的型別解掉三種毛病。代價：使用者 11-19 定的正式解法只解一半；restore case 5 那種問題在作者的型別上照樣會發生。

這件交你，因為各家做法不同（L17）。

### 第 2 件：Module 之間的接法（第 6 節）

選項與代價在第 6 節：(a) 編排層給「依賴的登記單位」查詢（建議；本卡判不算 O-4 職責清單多一項，請你一併看這個判斷）／(b) 產品經選項包傳共用物件／(c) static 表。

### 第 3 件：第 9.1 節 A1～A10 十組判定的變更、第 11 節的 DI 文字

方向在動工前核准；逐組的最終核准在合併前帶突變結果與檢證者（Fable 5.2／high）報告再看一次（和第 2、3 批同一個做法）。A1～A3、A10 是已定決定的直接後果（`-authority-no-writer-count`、確認清單、P14）；要特別看的是 A4～A6（搬到權威時判定的對象變了）、A8（摘要的緩的面：只差在 padding 與 `±0` 的狀態變成相同）。

### 第 4 件：第 5.4 節預算的規模假設與第 10 節的拆卡

規模（1,000 實體 × 4 種可回滾元件）是本卡的假設，沒有出處；拆卡（本卡當統合、兩張子卡）是本卡的判斷，影響你要看幾張卡。

### 本卡照 L17 判的（各家一致或細節；不逐條問）

- 原語有型別、放在 `ExclusiveWorld`（2.1）；原語 5 只留清單不實作（2.1）；順序圖的閉包由回滾自己算（2.2）；訊息寫者名單、`Writes` 收元件（2.2）。
- 「重跑中」歸時間 Module（已由第 3 批定）；`Resimulation` 每次準備放 false（2.3）。
- G4 隨區規則拿掉與權威的 tick 檢查收掉；G5 由回滾 Module 存版本、比版本（照 Unity）（2.4）。
- 浮點的雜湊照 Bevy `FloatOrd`（3.2）；沒有 padding 的型別照位元組（3.2）；摘要照順序、digest 和順序無關（3.2、4.4）。
- digest 用相加、含實體 id 與型別序號（和 lightyear 不同，理由在 4.4）。
- 權威、回滾 Module `default=no`（4.1、5.1）；策略經選項包（4.2）；「C 指紋」改名「一致性指紋」、G-5 格式改名（4.3）。
- 重跑掛 `Main/PreUpdate`、記錄掛步的 schedule 的 `Last`、刪掉的格比鍵集合（5.2）；預設值照 lightyear（5.3）。
- `rollbackSafe` 拿掉不搬（2.3）；S-13 隨區規則拿掉（7.1）；LIM-7 改寫（第 8 節）。
- 綁實作的守衛：第 3 批延下來的三條第 1 步立，本批新的延到第 5 批開頭（9.5）。

**和兩份已核准宣告字面不同的地方**（請你知道，不另外問）：`rollbackSafe` 不搬（回滾份 6.3 寫搬）；記錄 System 是獨占 System（回滾份 5.5 第 9 點寫一般 System，結論不變）；原語 5 不實作（統合 9.1「留」，清單照留）。

---

## 13. 判定（先宣告，結果出來之後不放寬）

- **判定 1：通過**。本機完整 ctest 全過（含 `scenario.segment1`～`segment3`）；L1 三平台（linux-x64、windows-x64、macos-arm64）全過；比摘要的測試四處「未完全解決」標註拿掉後全過，含 toolchain windows-x64 兩次建置的 `orchestration.restore`（`-summary-padding-a-then-c` 的來源）。
- **判定 2：行為**。第 9.3 節的案例全過；S3-1～S3-8 全過。
- **判定 3：檢出力**。三份新突變清單經 Fable 5.2／high 檢查補齊後全殺；第 9.4 節的 128 個照處置後，留下與搬走的全殺；完整跑（Kernel＋編排層＋`world_time`＋權威＋回滾）結果記進 dev_log，活著的逐個說明；分母以執行時的實際清單為準。
- **判定 4：一次性的掃描**（`git grep -n -P`，`engine/kernel`、`engine/orchestration`、`engine/world_time`；每個樣式先在 59fd4da9 命中一次當正例，終了碼區分「沒有」與「失敗」）：`\b(Zone|WritePolicy|Rollback)::`、`\bRole::(Input|State)\b`、`\bRequireExplicitZones\b`、`\bemitterZone\b`、`\brollbackEnabled\b`、`\bnotInRollback\b`、`\brollbackSafe\b`、`\brollbackActive\b`、`\bingress(Stage)?\b`、`\bDigestSet\b`、`\bCFingerprint\b`、`\bExportZoneCSources\b`、`\bparallelGuardTest\b`、`\bFunnelWrites\b`、`\bDE(2|9|11|15|16)\b`、`\bXE1\b` 零命中；`StartupCode` 的列舉值等於統合 3.1 的清單（`RegistryTests.cpp:223`）。
- **判定 5：預算**（第 5.4 節）：記錄、還原、重跑額外開銷各 ≤ 156 µs（1,000 實體 × 4 種，本機 Release）；`World::Summary` 不設預算（除錯與測試用），在 dev_log 記改前改後的時間。超過就停下回報。

---

## 14. 誰做、用什麼、怎麼分步

上段（公開介面、Design Invariants、資料形式）。全部 high、不用 xhigh（`v8-arch-gyo2-batch3-no-xhigh`；碰到 §14 停止或原因不明的失敗，停下回報、提議 xhigh，不自己升檔）。

| 步 | 卡 | 內容 | 實作者（模型／檔位） | 估計（agent 時間） |
|---|---|---|---|---|
| 第 1 步 | 統合 | 第 3 批延下來的三條守衛；逐型別雜湊（I1）與摘要格式 3；四處「未完全解決」解除；現有測試型別補函式（依第 12 節第 1 件） | Opus 5.5／high | 8～12 h |
| 第 2 步 | 統合 | 原語 1～4 與讀 stamp（I2）、`Writes` 收元件（I3）、訊息寫者名單（I4）、`Resimulation` 每次放 false | Opus 5.5／high | 8～12 h |
| 第 3 步 | 統合 | 第 6 節的接法（I5）；`engine/authority`、`engine/rollback` 骨架、`modules.csv`、I6、I7 的標頭；推送後開卡 A、卡 R | Opus 5.5／high | 4～6 h |
| 第 4 步 | 統合（和 A、R 並行） | 拿掉 Kernel 的權威規則（第 7 節）、測試與突變的處置（9.1、9.2、9.4）、golden 重產；**等 A、R 的接手測試併進本線之後才併** | Opus 5.5／high | 12～16 h |
| R-1、R-2 | 卡 R | 公開登記與檢查；官方策略、P2P 佔位；新頁；預算量測 | Opus 5.5／high | 8～12 h；16～24 h |
| A-1、A-2 | 卡 A | 登記、代為登記、策略、tick 檢查；一致性指紋、digest、G-5、交叉檢查；新頁 | Opus 5.5／high | 6～10 h；6～10 h |
| 第 5 步 | 統合 | 併 A、R；scenario 第 3 段；Kernel 的 DI、限制頁、world-time、編排層的文字；dev_log | Opus 5.5／high | 5～8 h |
| 第 6 步 | 統合 | 完整突變、三份新清單的敵對式檢查、§14.4 檢證、審查 | 見下 | 8～12 h |

合計 81～122 h；並行時關鍵路徑約 57～86 h（第 1～3 步 → 卡 R → 第 5、6 步）。超過估計 1.5 倍或單一問題累計超過 2 小時就停下回報。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者（三張卡） | Opus 5.5／high | 上段，不用小模型 |
| 審查者（各卡完成時、整條線） | Fable 5.2／high | 和實作者不同模型 |
| 新突變清單的敵對式檢查（Kernel 新增、權威、回滾） | Fable 5.2／high | §14.2；和定清單的人不同模型 |
| §14.4 檢證者（A1～A10） | Fable 5.2／high | 第 2、3 批同一個指定 |
| 本宣告的敵對式檢查 | Fable 5.2／high | 任務卡的指定 |

---

## 15. 基盤 Architecture 的根據與 Architecture Delta

### 15.1 為什麼算基盤

權威的一致性指紋是兩端握手的內容，一改就不相容；回滾的公開登記與 `RollbackStrategy` 是作者寫策略的插口；Kernel 的原語與逐型別雜湊是每個型別、每個回滾策略都會用的。網路 Module、狀態機的 (c) 區、讀檔都積在上面（統合 5.1、權威份 9.1、回滾份）。

### 15.2 予見的根據 → 要求

| 根據 | 要求 |
|---|---|
| 名指的 Feature：PvP Match／Client（伺服器權威、預測與回滾）、格鬥遊戲 P2P（佔位）、玩家存檔（讀檔兩次相同）、單機不增加複雜度 | 第 4、5 節；原語 1～4；P2P 用得到的原語在清單裡；逐型別雜湊；兩個 Module `default=no` |
| 參考實作：lightyear（checksum、預測、`Rollback` Resource）、bevy_replicon（協議雜湊）、Unity Netcode（`GhostUpdateSystem`、`IsFirstTimeFullyPredictingTick`）、Bevy（`set_last_changed`、`get_added_plugins`、`FloatOrd`）、flecs（type hooks 的 `cmp`／`equals`） | 原語的形、雜湊的客製點與浮點規則、重跑的位置、Module 之間的查詢 |
| 規模：64 Hz、回滾深度 20、1,000 實體 × 4 種（本卡的假設） | 判定 5 |

### 15.3 Architecture Delta（七項；和統合第 4 節、兩份第 9 節的差分）

1. **Feature**：第 15.2 節第 1 列。
2. **現在的問題**：區、寫者、digest、C 指紋、Z6／Z8／D-E15、D-E16、`Rollback` 列舉都還在 Kernel（`recon-a` 1），Kernel 對單機也執行這些檢查；回滾要的原語 1～4 不在；摘要照原始位元組雜湊（LIM-8）；Module 之間沒有通道。
3. **變化的邊界**：新 Subsystem `engine/authority`、`engine/rollback`；Kernel 少了區、寫者、digest、回滾的宣告與 C 指紋，多了原語 1～4、讀 stamp、每個登記型別的雜湊函式、訊息寫者名單，`Writes<…>` 收元件；編排層多了依賴單位的查詢（第 12 節第 2 件選 (a) 時）。
4. **影響**：Kernel（第 7 節的檔案）、編排層（`Setup.hpp`、`Product.cpp`）、`world_time`（區參數、`Resimulation`）、`modules.csv`、`tests/common/{kernel,orchestration,world_time,scenario,zone_scan}`、新 `tests/common/{authority,rollback}`、Kernel 與編排層的突變清單、新兩份清單；文件見第 11 節與限制頁。
5. **依賴方向**：新增 權威→Kernel、編排層、`world_time`、回滾；回滾→Kernel、編排層、`world_time`；產品→權威、回滾。Kernel、編排層、`world_time` 不依賴兩者。沒有反向。
6. **Ownership**：區、寫者數量、判定資料的讀寫：Kernel → 不由機制守；digest、一致性指紋、G-5、Z6／Z8／D-E15、tick 的單一來源：Kernel → 權威 Module；`Rollback`、D-E16、「重跑時不跑」：Kernel → 回滾 Module；摘要的雜湊：照原始位元組 → 每個型別的函式（型別的作者）。
7. **為什麼不能更小**：只拿掉 Kernel 的規則不給新家，伺服器權威與回滾沒有任何機制（統合第 8 節：守衛不留空窗）；只建新家不拿掉，Kernel 對單機也執行權威規則，違反 `-kernel-is-ecs-no-restrictions`；摘要只補 padding 是已做過的 A，使用者定正式解法是 C。

### 15.4 系譜、退掉的選項、需要它的 Feature（AGENTS §4）

| 決定 | 系譜 | 退掉的 | 決定的理由 | 需要它的 Feature |
|---|---|---|---|---|
| 雜湊函式跟著型別、摘要與 digest 共用 | lightyear 逐元件登記；Bevy `#[reflect(Hash)]` | 摘要與 digest 各一套；只補 padding（已做的 A） | 一個概念一個實作；使用者定 C | 存檔（讀兩次相同）、PvP digest |
| 浮點 `±0`、NaN 正規化 | Bevy `FloatOrd` | 逐位元（lightyear 的自訂函式、flecs `equals`） | 使用者把 `±0` 列為毛病 | 存檔、PvP digest |
| 原語有型別、在 `ExclusiveWorld` | Bevy；統合 9.1 | 型別擦除的整欄複製（Unity 的原始位元組） | 回滾的登記知道 `T`；GYO 的元件不限 unmanaged | PvP Client 回滾、P2P（佔位） |
| G5 由回滾存版本 | Unity Netcode `DidChange(版本)` | Kernel 加 tick→stamp 原語；每型別一個 `Changed<T>` System（lightyear） | 三家都沒有 Kernel 層的對照；一個獨占 System 處理全部型別 | PvP Client 回滾 |
| 重跑掛 `Main/PreUpdate` | lightyear `PreUpdate` 的 `run_rollback` | 掛在 `FixedUpdate`（X-11 不准） | 巢狀規則 | PvP Client 回滾 |
| digest 相加、含 id | lightyear 的 XOR 是起點 | XOR、不含 id | 相同的兩項不抵銷；GYO 的 id 由權威端給 | PvP digest |
| 依賴單位的查詢（第 12 節第 2 件） | Bevy `get_added_plugins` | 選項包共用物件；static 表 | 交你定 | PvP（權威＋回滾） |
| 兩個 Module `default=no` | lightyear、Unity Netcode 都不在預設集合 | `default=yes` | 單機不增加複雜度 | 單機 |

既有的 Test 與守衛能不能不改，**沒有**當作任何一項的決定理由。

---

## 16. 範圍的限界

- 這是宣告，沒有試做；第 14 節的時間是讀程式量估的。
- 行號照三份偵察（59fd4da9），卡片抽查了 `recon-b` 第 1、3、5 節與 `recon-a` 第 1、4 節的一部分；Unity 讀的是非官方鏡像；bevy_ggrs、GGPO 本機沒有。
- 規模假設（5.4）沒有出處。
- 網路 Module 的設計不在本批：digest 何時比、伺服器值怎麼搬、對時、tick 跳變的通知（`LocalTimelineShift` 那一種，第 3 批第 5 節表列在「第 4 批、網路」）——本批只做到回滾 Module 退 tick 時用 `RewindFixedSteps`，**tick 跳變的推送式事件不做**：以 tick 為索引的歷史只有回滾 Module 自己的，退回時它自己處理；別的讀者出現時（網路 Module 的輸入緩衝）再加（AGENTS §2）。
- 狀態機的 (c) 區接法（權威份 5.8）只做到 `Writes<…>` 收元件與 `authority.Runs`、`authority.MarkResource`；狀態機本身在第 5 批。
- 「誰實際寫的」除錯工具（L30）不在本批。
