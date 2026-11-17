# 事前宣告：Kernel 減法・結構段（`v8-arch-gyo2-kernel-subtract-structure`）

更新：2026-11-17（修訂 0）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：草稿，未核准。使用者核准之前不改程式、不改條文與守衛。**

性質：實作批的事前宣告（AGENTS §3 的 Architecture Delta 的一批、§14.4 的判定變更、Design Invariants 的變更）。七項不重寫，照[統合宣告](kernel-app-boundary-declaration.zh-Hant.md)（修訂 38，已核准）第 4 節；本頁只寫和統合的差分（第 10 節）。

根據：
- [批次表](rebuild-batch-plan.zh-Hant.md)第二段第 1 條。
- 統合宣告修訂 38：關聯表 L3、L5、L6、L7、L9；第 3 節（分層與歸屬）；第 7.2 節（守衛的處置）；第 8 節（本批那一列）；第 9.1 節（DI-KS 的改寫草案）；第 15、16 節。
- HANDOFF 決定表：`v8-arch-gyo2-kernel-world-exec-type`、`-dynamic-insert-per-world`、`-read-only-registry-view`、`-exclusive-writes-optional`、`-kernel-settings-read-write`、`-no-runtime-interception`、`-no-ambiguity-detection`、`-thread-binding-by-runner`、`-system-declares-immediate-structural-change`、`-immediate-change-via-exclusive-world`、`-five-declarations-approved`。
- 程式：`origin/gyo2` a08c16f6。

**標記**：寫「卡片照 L17 判」的，是任務卡照命名規範與別家原始碼自己判的，不是使用者的話（使用者 2026-11-17：「什麼都要我來定，乾脆代碼我也自己寫好了。名字你有命名規範。例子你有源碼。」）。交使用者的只有第 12 節的幾件。

證據（主 checkout `build/evidence/gyo2-kernel-subtract-structure/`）：`scope.md`（給偵察者的範圍）、`test-inventory.md`（測試逐案清單）、`mutant-guard-inventory.md`（突變與守衛腳本清單）。

---

## 1. 這一批做什麼（先用遊戲講）

**單機 2D 橫版。** 主角被打、撿到補血包，兩個 System 都寫主角的血量。現在：沒寫先後時，如果哪一邊是決定性區（zone C）就啟動失敗；作者呼叫過 `RequireExplicitOrder` 時兩邊都是一般區也啟動失敗。這一批之後：Kernel 照登記順序排、列進 `TieBreaks`，不報錯（使用者 11-16「偵測歧義其實是很麻煩的。我個人覺得沒必要。」）。

**開發中打開存取驗證。** 現在 `KernelSettings` 只在第一次 Run 讀一次，之後外面再改是斷言，System 宣告讀它是啟動錯誤（D-E18）。這一批之後：除錯指令的 System 可以寫 `ResMut<KernelSettings>`，**下一次最外層 Run** 開始時生效。

**讀檔與回滾。** 要把一批實體放回去、把 Resource 換掉。現在一般 System 不能增刪 Resource，`ExclusiveWorld` 連一個實體都不能生成（`Spawn()` 一律回 Err）。這一批之後：一般 System 用 `Commands` 記下增刪 Resource，在同步點套用；`ExclusiveWorld` 可以當場生成、插入、拿掉、刪除實體，當場增刪 Resource（使用者 11-17「沿用 ExclusiveWorld」）。回滾的整欄放回等原語不在這一批（權威＋回滾那條線）。

**將來的指令集。** 遊戲跑起來之後，輸入一行指令就加一個 System。這一批給 Kernel 那一半：登記分成「開 Registrar」「建 Plan」兩步、可以重複；World 建立時宣告准不准動態插入，不准時拒絕、回傳錯誤、World 照跑（使用者 11-17「如果World的宣告可以，那就可以動態插入系統…如果宣告不行，那麼就不能動態插入，插入就拒絕，返回信息」）。編排層怎麼把它接到指令（以及登記單位 `Plugin` 搬出 Kernel）在下一批。

**PvP 的 Match。** 現在 Kernel 在執行中攔 untyped `Commands`、`ExclusiveWorld`、`Despawn` 對判定資料的寫入，第一次 Run 之後從外面寫 zone C 也斷言。這一批之後全部拿掉（使用者 11-17「拿掉執行期的攔截，伺服器權威改靠 digest 比對與伺服器覆蓋來抓錯。」）。接手的 digest 比對要等網路 Module，這段期間沒有東西抓（統合第 8 節「空窗」，gyo2 上沒有產品）。

---

## 2. 範圍

### 2.1 這一批做的（S1～S11）

| # | 內容 | 統合的根據 |
|---|---|---|
| S1 | Kernel 的型別 `Product` 改名 `WorldExecutor`（`Product` 這個名字之後給編排層，本批不建編排層） | `kernel-world-exec-type`；3.1 |
| S2 | 登記兩步、可重複：開 Registrar、建 Plan；建好之後可以再開；World 建立時宣告准不准動態插入；owner 由開 Registrar 的人給（`ownerPlugin` 改名 `owner`） | L5 的 Kernel 半；3.5；DI-KS 15 第 (12) 項 |
| S3 | 唯讀、帶版本號的登記表：版本號每次建 Plan 加一；補上 hook、`InitialValue`、schedule 的選項與 stage、`Writes<…>` 標註、World 的宣告 | L6；3.3 |
| S4 | `ExclusiveWorld` 讀寫任何元件與 Resource 的值；`Writes<…>` 留作選填標註：不進存取集合、不算寫者 | L7；`exclusive-writes-optional`；確認清單第 4 件 |
| S5 | `ExclusiveWorld` 可以當場改成員、當場增刪 Resource；DI-KS 18 改寫；hook 的 `ExclusiveWorld<>` 不能當場改 | `immediate-change-via-exclusive-world`；9.1 DI-KS 18 |
| S6 | 一般 System 經 `Commands` 增刪 Resource，在同步點的 resource 階段套用 | 修訂 38；DI-KS 15 第 (5) 項 |
| S7 | `KernelSettings`：拿掉 D-E18；每次最外層 Run 開始時重讀；第一次 Run 之後外面也能寫 | L9 |
| S8 | 拿掉執行期的攔截（L3 的六條）：untyped `Commands` 的檢查、`Despawn` 記錄當下的 Z5、同步點的 Z5 複核、W-6b；`ExclusiveWorld` 的寫入檢查併在 S4 | L3；7.3 |
| S9 | 拿掉歧義的偵測：D-E8、`RequireExplicitOrder`、(c) 歧義；所有沒有先後的衝突對照登記順序平手 | `no-ambiguity-detection`；`strict-order` 退役 |
| S10 | 拿掉執行緒的啟動檢查（H-2、X-3：D-E10、D-E13、X-E2、X-E3、`ScheduleOptions::runner`、`SystemOptions::affinity`、`kMainRole`）；綁執行緒的 Resource 被別的執行緒碰到就斷言 | `thread-binding-by-runner`；3.1 末列 |
| S11 | Registry 與 Plan 指紋改版、golden 重產（第一次） | L10；3.4 |

另外三件附帶的：
- DI-KS 15（Kernel 的職責清單）照統合 9.1 改寫，附「尚未拿掉、隨哪一批拿掉」的過渡表（第 7 節）。
- K0 入口檢查的先後（批次表「主會話自己判的」交到本批）：**照舊，先看呼叫有沒有寫錯，再看 World 是否已經中斷**。卡片照 L17 判：Bevy 的 `World::run_schedule` 只有「schedule 找不到」一種失敗（`world/mod.rs` `try_schedule_scope`），沒有「已中斷」的狀態，先查呼叫是唯一的順序；GYO 現在的程式也是這個順序（`Executor.cpp:444-481`），不用改。
- 批次表寫「`we-j3-spawn-immediate` 隨 `ExclusiveWorld` 可當場改成員退役」——**不退役**。卡片照 L17 判：那個突變改的是一般 System 的 `Spawn` 參數，抓它的是「一般 System 的呼叫期間成員不變」（W-3），這一條本批照留；退役的理由不成立。詳見第 6 節。

### 2.2 不在這一批（留到哪一批）

| 留下的 | 現在的位置 | 隨哪一批拿掉或搬走 |
|---|---|---|
| 開局的排程（K0）：`ScheduleOptions::startup`、D-E21、`StartupNotNestable`、`StartupRunning`、DI-KS 22、J-58 | `Executor.cpp`、`Registry.cpp` | 第 2 條 編排層的骨架（J-58 改寫搬到編排層） |
| 登記單位 `Plugin`／`Requires`／`Plugin::Finish`／`FinishContext::InstallRunner`；R-E1、R-E3、R-E6；`Registry(std::vector<Plugin*>)` 與 `Registry::Finish()` | `Registry.hpp`、`Registry.cpp` | 第 2 條（本批把 `Finish()` 改成建立在兩步之上，行為不變；見 3.2） |
| 每個 World 准不准動態插入的**編排半**（World 描述裡的選項、轉交、記警告） | — | 第 2 條（本批只做 Kernel 半：宣告、拒絕、回傳錯誤） |
| 時鐘：`Domain`、`ScheduleOptions::clock`、tick、`CurrentTick`、`AccessKind::DomainTick`、tickStamps、`EventTrim`、`EventOptions::domain`、D-E12、F-E1、J-30 的 tick 半、J-36、J-37 | 多處 | 第 3 條 時間 Module＋事件 |
| 事件改名 `Message`、推送式 `Event`＋`Observer`、游標 | — | 第 3 條 |
| 寫者數量 D-E2 本身（只有「`Writes<…>` 列的算寫者」在本批拿掉）、`WritePolicy`／`Shared`、DI-KS 6 | `Registry.cpp` `CheckExclusiveWriters` | 第 4 條 權威＋回滾 |
| 區：`Zone`、Z1～Z8、S-6～S-9、S-17、`RequireExplicitZones`、`emitterZone`（含 `RejectedSpawn::emitterZone`）、D-E9、X-2／D-E11／X-E1、digest、`DigestSet`、`CFingerprint`、`ExportZoneCSources` | 多處 | 第 4 條 |
| 回滾的宣告（`Rollback`、`rollbackSafe`、`notInRollback`、D-E15、D-E16）、回滾的原語 | 多處 | 第 4 條 |
| `DiagnosticsBuffer::Entry::rollbackActive` | `detail/Storage.hpp` | 第 4 條 |

---

## 3. 設計

### 3.1 S1：`Product` → `WorldExecutor`

只改名：類別、標頭（`Product.hpp` → `WorldExecutor.hpp`，命名規範 N16「檔名等於它主要的型別名」）、註解、測試輔助。`Executor.cpp` 不改名：守衛 `kernel_guard.py:292` 的 K6 豁免以檔名比對，改名要動守衛；這個檔現在就實作 `Product`，不是新的不一致（卡片照 L17 判）。行為不變。`Product` 這個識別字本批之後在 `engine/kernel` 不再出現（判定 G-3）。

### 3.2 S2：登記兩步、可重複

```cpp
struct RegistrationOwner { std::string name; };   // Kernel 只存、只比相等、只拿來印（統合 3.5）

class Registry {
public:
    Registry();                                    // 新：沒有 Plugin 清單
    explicit Registry(std::vector<Plugin*>);      // 照舊，到下一批
    // 第一步。Ok：一個 Registrar；用它登記的內容在下一次 BuildPlans 生效。
    // Err（執行時錯誤，不改 Registry）：用這份 Registry 建的 World 裡有一個宣告了不准動態插入。
    Engine::Base::Result<Registrar, KernelError> OpenRegistrar(RegistrationOwner owner);
    // 第二步。Ok：版本號加一、每張 schedule 的 Plan 重建、指紋重算。
    // Err：所有啟動錯誤一次收齊；第一次失敗時 Registry 不能用（照舊），之後的失敗退回上一個版本、World 照跑。
    Engine::Base::Result<void, StartupReport> BuildPlans();
    Engine::Base::Result<void, StartupReport> Finish();   // 照舊：Plugin 的 Build → BuildPlans 前的檢查 → Plugin::Finish → 建 Plan
};
struct WorldOptions { bool dynamicInsert = true; };      // World 建立時的宣告（L5）
```

規則：
1. **可重複**：`BuildPlans` 之後可以再 `OpenRegistrar`。兩次 `BuildPlans` 之間可以開好幾個 Registrar（編排層依登記檔順序 build 各 Module）。上一次 `BuildPlans` 之前開的 Registrar 作廢：再用它登記是斷言（Registrar 帶世代號；取代現在的「不能複製、不能移動」，改成只能移動，才放得進 `Result`）。
2. **id 只往後加**：已發的 ComponentId、ResourceId、EventTypeId、registrationSeq 在之後的登記與重建中不變；新的往後加（DI-KS 4 改寫）。
3. **拿掉 System**：`bool Registrar::RemoveSystem(std::string_view name)`，下一次 `BuildPlans` 生效；名字不存在回 false、不記任何東西（不加新的啟動錯誤碼）。ECS 元件、Resource、事件、schedule 不能拿掉（Bevy 也不能取消登記元件）。
4. **准不准動態插入**：`World(const Registry&, std::string name, WorldOptions)`。只要有一個用這份 Registry 建、還活著的 World 宣告 `dynamicInsert = false`，`OpenRegistrar` 回 `Err`（新碼 `DynamicInsertRefused`），Registry 與 World 都不變。**記警告由呼叫的人做**（編排層；Kernel 的錯誤只在一處記，error-handling DI 2）——卡片照 L17 判。
5. **World 接上新版本**：`WorldExecutor::Run`（最外層）開始時，Registry 的版本和 World 記的不同，就先把新登記的欄、Resource、事件緩衝往後加、照 registrationSeq 重建每個 System 的執行狀態（留下的 System 保留 `lastRun` 與 `Local`；拿掉的丟掉），再跑。開 Registrar 之後、`BuildPlans` 之前跑 `Run` 是斷言（Programmer Error：登記到一半）。
6. **重建失敗**：之後的 `BuildPlans` 失敗時，Registry 退回上一個版本（失敗前存一份），World 照用上一個版本跑——「不改 World」。
7. **`Finish()`**：照舊把 Plugin 清單走完；內部改成「每個 Plugin 開一個 owner 為它的 Registrar → 解析 → Plugin::Finish → 建 Plan」，順序與結果不變（下一批搬到編排層時改成 build → 建 Plan → finish）。

系譜：Bevy `Schedule::initialize` 在圖有改動時重建（`schedule.rs:637-650`）、元件可隨時登記、id 只往後加；flecs `pipeline.c:269-277` 有改動才重建；Unity 執行中可以建 System。「每個 World 宣告」四家都沒有，是使用者的設計（L5）。**卡片照 L17 判的細節**：拒絕時不記 log（3 的第 4 點）；`RemoveSystem` 用 bool 而不是新的啟動錯誤；重建失敗退回上一個版本（Bevy 建圖失敗是 panic；GYO 的「不改 World」是使用者的原話）。

### 3.3 S3：唯讀、帶版本號的登記表

`FrozenRegistry`（名字照統合 L6 留著）加：
- `std::uint64_t Version()`：第一次 `BuildPlans` 成功是 1，之後每次成功加一。
- `Hooks()`：每個 hook 的名字、觸發、元件、存取、登記處、owner。
- `InitialValues()`：Target、Request、宣告者、登記處。
- `Schedules()`：每張 schedule 的選項與 stage 順序。
- `SystemInfo::access` 的 `annotatedWrites`（`ExclusiveWorld<Writes<R…>>` 列的 Resource；S4）。
- World 的宣告由 `World::Options()` 交出（宣告屬於 World，不屬於 Registry）。

約定（寫進 DI-KS 25）：交出去的參照與 span 只在當前版本有效，`BuildPlans` 之後作廢；修改只走 `Registrar`。順序圖不另交：每個 mount 的 before／after 已在 `SystemInfo::mounts`，D-E16 要的圖由回滾 Module 自己從這裡建（卡片照 L17 判：同一個事實一處，AGENTS §9）。事件的上限與讀者名單隨游標那一批。

系譜：Bevy `Schedule::graph()` 回傳 `&ScheduleGraph`（`schedule.rs:677`）、`ComponentInfo::hooks()`（`component/info.rs:135`）；版本號 flecs `rebuild_count`（`pipeline.c:277`）。

### 3.4 S4：`ExclusiveWorld` 讀寫任何值，`Writes<…>` 選填

- `Modify<T>(entity, f)` 寫任何元件；`Read<T>(f)`、`Modify<T>(f)` 讀寫任何 Resource，含 `KernelSettings`。拿掉 `CheckExclusiveWrite`、`CheckExclusiveResourceWrite`、`CheckExclusiveRead` 與錯誤碼 `ExclusiveWorldWrite`、`UndeclaredResourceWrite`、`UnreadableResource`（S-13 的寫入半、S-14、S-15、S-16）。
- 存取集合：`ExclusiveWorld<…>` 的 Contribution 是「整個 World 的值的讀寫」。`Writes<R…>` 不再是 Resource 的寫，只記進 `annotatedWrites`：不算 D-E2 的寫者、不受 Z2／D-E18／親和的檢查、不改變衝突。
- 存取驗證：獨占 System 宣告了整個 World 的寫，第二層的邊界比對不會再把它的寫入算成未宣告（J-56「驗證照樣抓得到沒列出的單一寫者 Resource」三案隨之改變，第 5 節）。
- S-18（hook 不得用 `ExclusiveWorld<Writes<…>>`）拿掉；改成 S5 的「hook 的 `ExclusiveWorld<>` 不能當場改成員」（執行期錯誤，統合修訂 35：從簽名判不出來）。
- S-17（獨占 System 兼 ingress）是區規則，留到第 4 條。

系譜：Bevy 獨占系統 `&mut World` 不列寫入（`multi_threaded.rs:173`、`:506`）；flecs `immediate`；`Writes<…>` 留作選填是使用者的決定（「嗯，可以留選填」）。

### 3.5 S5：`ExclusiveWorld` 當場改成員、當場增刪 Resource

新增的成員函式（卡片照 L17 判名字：照 GYO 已有的 `World::Insert`／`Remove`／`Despawn`／`SetResource`／`RemoveResource` 與 `Spawn::New`、`SpawnWithId::New`，不照 Bevy 的 `insert_resource`，同一個專案裡同一件事一個名字）：

| 函式 | 回傳 | 說明 |
|---|---|---|
| `Spawn<C…>(C… values)` | `Result<EntityId, KernelError>` | id 由配發器發（同 `Spawn<Bundle<C…>>::New`；配發器用完是 `AllotterExhausted`）；取代現在一律回 Err 的 `Spawn()` |
| `SpawnWithId<C…>(EntityId, C…)` | `Result<std::optional<SpawnRejectReason>, KernelError>` | 不能生成時不生成、回傳理由，並照 F-13 記一筆 `RejectedSpawn`（記錄的地方與計數只有一處） |
| `Insert<T>(EntityId, T)` | `Result<bool, KernelError>` | 實體不在時不插入、回 false，照 F.2 第 5 步計入 `DroppedMissingEntity` |
| `Remove<T>(EntityId)` | `Result<bool, KernelError>` | — |
| `Despawn(EntityId)` | `Result<bool, KernelError>` | 先跑每個元件的 OnRemove，再依 ComponentId 升冪拿掉（同同步點第 2 步，`despawn-notify-all-then-destroy`） |
| `SetResource<T>(T)`、`RemoveResource<T>()` | `Result<void, KernelError>`、`Result<bool, KernelError>` | — |

統合留給本批定的三件（第 10 節第 12 件），卡片照 L17 判：

1. **hook 什麼時候跑**：**當場**。插入新的格時先填 `InitialValue`、再跑 OnAdd；拿掉時先跑 OnRemove 再拿掉；順序同同步點的各步。這些 hook 記下的變更，**在函式回傳之前當場套用到空**：每一輪是一次完整的同步點套用（跑 hook、填初始值），一輪一輪套到空，上限是 `KernelSettings` 的 hook 清空輪上限，超過照 F-12 回 `HookRoundLimit`。不進這張 schedule 的 hook 佇列。
   系譜：Bevy 每個 `EntityWorldMut::insert` 結束時 `world.flush()`（`world/entity_access/world_mut.rs:1097`）把 hook 記的命令當場套用；flecs `immediate`「Operations passed the iterator's world execute immediately」（`docs/Systems.md:1505`）。GYO 自己的先例：統合第 16 節「外面在 Ready 時觸發 observer，延後變更觸發回來之前當場套用…一輪一輪套到空，上限用 hook 清空輪上限」。不採「進 hook 佇列、等同步點」：回滾的還原之後緊接著巢狀跑固定步，裡面那張 schedule 不套用外層 schedule 的 hook 佇列（DI-KS 19），hook 衍生的變更會在重跑之後才出現。
2. **stamp 與存取驗證的快照**：變更本身用句柄目前的 stamp；每一輪 hook 套用取新的 stamp、同步點序號加一（F-6）；函式回傳時句柄照 `RunSchedule` 回來時的做法更新自己的 stamp 與同步點序號（`Params.hpp` 的 `Renew`），之後的寫入排在這些 stamp 之後。存取驗證：當場套用時 hook 照同步點的規則驗（第一層）；套用完重取第二層的快照，獨占 System 自己的邊界比對與下一步的「呼叫前」比對都不會把它們算成別人的。
3. **限制**：
   - 只在序列執行（v1 只有序列，LIM-2）。
   - 在 `Read`／`Modify` 的閉包裡呼叫是 Err（新碼 `ClosureChangesMembership`，比照 X-14：閉包拿著的參考會因為成員變更而失效），World 中斷。
   - hook 拿到的 `ExclusiveWorld<>` 呼叫是 Err（新碼 `HookChangesMembership`，比照 X-13：hook 在套用中跑），World 中斷。
   - **繞過句柄改成員仍然擋**：W-3 的檢查（「呼叫期間成員不變」）照留——一般 System 照舊；獨占 System 的每個當場改的函式，**先比對**目前的成員記號等於這一層的基準（不等就是之前有繞路，斷言），變更與當場套用完**再把基準更新**成新的值。所以經 World API 或儲存本身繞路改成員，不論在當場改之前或之後，仍然斷言（偵察者指出：只在變更後把基準設成目前的值，會把同一次呼叫裡更早的繞路吞掉）。這是第 12 節第 1 件，要使用者定（記錄裡「兩案退役」與這個做法不同）。World API 的結構變更在 Running 時照舊斷言（`World.cpp:145`；`NestedRunTests` :198、:2003 守它，兩個選項都留）。
   - 巢狀不留舊參照：閉包與回傳值本來就不能指回 World（DI-KS 21），句柄的 stamp 在每次變更後更新；不另加規則。

### 3.6 S6：一般 System 經 `Commands` 增刪 Resource

`Commands::SetResource<T>(T)`、`Commands::RemoveResource<T>()`：記一筆 `ResourceInsert`／`ResourceRemove`，在同步點第 7 步依 (ResourceId, planPos, recSeq) 套用（同步點的這一步已經存在，只是沒有記錄者，`World.cpp:847-863`）。untyped `Commands` 沒有存取宣告，Resource 的增刪不進衝突圖（同 Bevy `Commands::insert_resource`，`system/commands/mod.rs:816`）。hook 也可以用（記進 hook 佇列）。

一般 System 的 `Res<T>`／`ResMut<T>` 碰到不存在的 Resource 照舊是斷言（`Params.hpp:393`）。可能不存在的 Resource 的參數（Bevy 的 `Option<Res<T>>`）不在本批（沒有點名的 Feature，AGENTS §1）。

### 3.7 S7：`KernelSettings`

- 拿掉 D-E18：System、hook 可以宣告 `Res`／`ResMut<KernelSettings>`。
- `WorldExecutor` 每次最外層 `Run` 開始時讀（取代「第一次 Run 時快取」）；`hookRoundLimit == 0` 在每次 Run 開始時斷言。Run 中途改的值，下一次最外層 Run 才生效（巢狀的 `RunSchedule` 不重讀）。
- `World::SetResource<KernelSettings>` 在第一次 Run 之後不再斷言。
- 系譜：Bevy `FallbackErrorHandler` 是 Resource，「schedule 正在跑時改，要等跑完才生效」（`bevy_ecs/src/error/handler.rs:110-117`）。

### 3.8 S8：拿掉執行期的攔截

| 拿掉 | 位置 |
|---|---|
| `CheckUntypedCommand`、`UntypedWrite`：untyped `Commands` 只寫 zone B 的 Shared 欄 | `Params.hpp:483`、`:495`；`World.cpp:345` |
| `CheckDespawn`：zone B 的 `Despawn` 碰帶 zone C 欄的實體 | `Params.hpp:605`；`World.cpp:355` |
| 同步點的 Z5 複核與外層的檢查、`ZoneCDespawn` | `World.cpp:576-640` |
| W-6b：第一次 Run 之後從外面寫 zone C | `World.cpp:196-202` |
| `ExclusiveWorld` 的寫入檢查 | 併在 S4 |

`Record::emitterZone` 欄位本身與 `RejectedSpawn::emitterZone` 留到第 4 條（區移出時一起拿）。DI-KS 19 的「規則 Z5 的重查」一句拿掉。

### 3.9 S9：拿掉歧義的偵測

拿掉 `Registrar::RequireExplicitOrder`、`StartupCode::DE8` 與 (c) 歧義的檢查；Scheduler 對所有沒有先後的衝突對照 registrationSeq 平手，全部列進 `TieBreaks`（含 zone C）。`StrictOrderTests.cpp`（8 案）退役。

### 3.10 S10：執行緒

- 拿掉 `ScheduleOptions::runner`、`SystemOptions::affinity`、`Affinity`、`kMainRole`；啟動錯誤 D-E10、D-E13、X-E2、X-E3。
- `ResourceOptions::affinity` 改成 `bool threadBound`（卡片照 L17 判名字：Bevy 叫 `NonSend`，GYO 的登記選項都是形容詞或名詞的成員，例 `external`、`rollbackSafe`）：值放進去時記下那條執行緒（`World::SetResource`、`ExclusiveWorld::SetResource`、同步點套用的 `Commands::SetResource`），之後在別的執行緒經 `Res`／`ResMut`、`ExclusiveWorld`、World API 碰到就斷言；拿掉值時忘掉。執行緒的記號用 Kernel 既有的 `thread_local` 變數的位址（每條執行緒不同），不引入 `<thread>`——守衛 K3 禁止 Kernel 用它（卡片照 L17 判）。
- 系譜：Bevy `bevy_ecs/src/storage/non_send.rs:74-84`（插入時記執行緒、別的執行緒存取就 panic）。使用者 11-16「別人掉過的坑，我沒必要在跳一次。」

### 3.11 S11：指紋

拿掉的欄位（`runner`、System 與 Resource 的 `affinity`、`Writes<…>` 從寫入移到 `annotatedWrites`）與新增的（`threadBound`、owner 的改名）改變雜湊的位元組：Registry、Plan、C 三種指紋的格式版本各加一，Plan 匯出的標頭也加一，所有 golden 重產（逐處清單在第 5.4 節；C 指紋共用 System 與 Resource 的位元組，所以也動到）。

---

## 4. 要改的程式

| 檔 | 改什麼 |
|---|---|
| `engine/kernel/include/engine/kernel/Product.hpp` → `WorldExecutor.hpp`；`src/Executor.cpp`（不改名） | S1；S2 的第 5 點（接上新版本）；S7 每次 Run 重讀 |
| `Registry.hpp`、`src/Registry.cpp`、`detail/RegistryData.hpp` | S2（兩步、世代號、`RemoveSystem`、退回上一版、owner）；S3；S4（`Writes` 不計寫者）；S7（D-E18）；S9（`RequireExplicitOrder`）；S10 |
| `src/Scheduler.cpp` | S9（D-E8、(c) 歧義）；S10（H-2 的檢查若在這裡） |
| `Params.hpp` | S4；S5（新成員函式、閉包與 hook 的拒絕）；S6；S8（`Commands`、`Despawn` 的檢查） |
| `World.hpp`、`src/World.cpp`、`detail/Context.hpp`、`detail/Storage.hpp` | S2（`WorldOptions`、接上新版本）；S5（當場套用）；S8；S10（記執行緒、斷言） |
| `Declarations.hpp` | S10（`Affinity`、`runner`、`kMainRole`） |
| `KernelError.hpp` | 拿掉 `UntypedWrite`、`ZoneCDespawn`、`ExclusiveWorldWrite`、`UndeclaredResourceWrite`、`UnreadableResource`、`DE8`、`DE10`、`DE13`、`DE18`、`XE2`、`XE3`；新增 `DynamicInsertRefused`、`ClosureChangesMembership`、`HookChangesMembership` |
| `src/Fingerprint.cpp` | S11 |
| `tests/common/kernel/*` | 第 5 節 |
| `docs/architecture/plans/kernel/scripts/mutations.json` | 第 6 節 |
| `docs/architecture/target/kernel.zh-Hant.md` | 第 7 節 |
| `docs/architecture/target/v2/system-contract-page.zh-Hant.md` | 作者看到的改變：`ExclusiveWorld` 的新函式、`Commands` 增刪 Resource、`KernelSettings` 可寫、歧義不報錯 |

---

## 5. 守衛與測試的變更（AGENTS §14.4）

全部 15 個測試檔 332 案與編譯失敗的樣本逐案讀過（偵察者 Opus／high，`test-inventory.md`）：主表 64 案（改 16、換觀察方式 8、拿掉 22、拆開 16、看第 12 節第 1 件 2），只要改名或改準備資料的 13 案另列（5.5）。下面依「守的性質」分組，每組寫 §14.4 的 1～5；同一組的案例共用同一個性質。**緩的面**＝以前判失敗、之後判通過的情形；**緊的面**＝以前判通過、之後判失敗的情形。

### 5.1 有緩的面的（要你逐組核准）

| 組 | 1. 守的性質 | 2. 緩的面 | 3. 緊的面 | 4. 性質之後由誰守 | 案例（處置） | 5. 根據 |
|---|---|---|---|---|---|---|
| A 歧義 | 沒有先後的衝突對（任一方 zone C，或呼叫了 `RequireExplicitOrder`）在 finish 失敗（D-E8） | 這些對都通過，照登記順序排、列進 `TieBreaks` | 無 | 「要寫先後」：不由機制守（使用者的決定）。「哪些對算衝突」：改用 `TieBreaks` 的內容斷言，照樣守 | `StrictOrderTests` :92、:140、:149、:163、:189、:200、:220（拿掉；:140、:149、:189 的性質在 `SchedulerTests` :197、:265、:287 已有）；`SchedulerTests` :216（改：斷言四組各列出哪一對、Plan 照登記順序）、:313 J-25（換觀察：`TieBreaks` 有 {WithB, WithoutB}）、:432 J-44（拆：有歧義的那半 :456-460 拿掉，有先後的那半留）；`CoverageTests` :212（換觀察：`TieBreaks` 有那一對）；`SpawnRejectTests` :993 第 2 段（改：finish 通過、`TieBreaks` 有那一對） | `no-ambiguity-detection`；統合 7.2 J-21～J-25、J-44 |
| B 執行期的攔截 | untyped `Commands` 只寫 zone B 的 Shared 欄（F-10）；zone B 的刪除碰到帶 zone C 欄的實體（記錄當下與同步點，含巢狀各層、hook 佇列、清空輪）回 Err | 這些寫入與刪除都照做：刪除先套用，同一窗對同一實體的 zone C 插入因實體不在被丟掉並計數 | 無 | 不由機制守（伺服器權威靠 digest 比對與伺服器覆蓋；網路 Module 之前有空窗，統合第 8 節）。「丟掉並計數」的結果由 `NestedRunTests` :395、`WorldTests` :484 第 3 段、`ExecutorTests` :710 第 3 段守 | `ExecutorTests` :296（拆：五段 UntypedWrite 與 Z5 那段拿掉；「zone B Shared 准」留）、:404 第 3 段（拿掉）、:710（拆：四段 Z5 拿掉；三段留）；`HookFlushTests` :608 第 1 段（拿掉）；`SpawnRejectTests` :758（改：Run 通過、先刪後生成，`RejectedSpawns()==0`）；`NestedRunTests` :409、:420、:433、:445、:1681、:1902（拿掉）、:1694（拿掉：沒有 Z5 就沒有意義的反例）、:1881（拿掉：「裡面的同步點在第 0 步之後失敗」除了 Z5 沒有別的觸發） | `no-runtime-interception`；統合 7.2 J-32b、J-52、J-56 |
| C 第一次 Run 之後從外面寫 zone C | W-6b：第一次 Run 之後 Host 寫 zone C 的狀態（非 boundary、非 external）是斷言 | 這些寫入照做 | 無 | 不由機制守（同 B） | `ExecutorTests` :55（拆：三個 W-6b 的斷言拿掉；W-6 與其他留） | 同 B；統合 7.2 J-6 |
| D `ExclusiveWorld` 的寫入限制 | `ExclusiveWorld` 只寫 zone B 的 Shared 資料；沒列在 `Writes<…>` 的單一寫者 Resource 寫不進去（S-13、S-14）；驗證的兩層把它們當成未宣告 | 寫任何元件與 Resource 都成功、驗證不報 | 寫的值確實寫進去（新的正面斷言，見 :1758） | 不由機制守（`Writes<…>` 標註留給權威策略讀） | `ExecutorTests` :545（拆：zone C、Exclusive、zone C Shared 三段改成「通過、值寫進去」；啟動時的 zone C 那段是區規則、留）；`NestedRunTests` :1304 第 3 段（改：通過、Flag==14）、:622、:634（拿掉）、:646（換觀察：改用事件緩衝經存下的位址寫，留住「巢狀跑之後重取快照」的守衛）、:605（換觀察：同 :646，留住 `Executor.cpp:486-510` 唯一的測試）、:1758（改：六種存取都通過、閉包都被呼叫、值都是 100） | `exclusive-writes-optional`；統合 7.2 J-45、J-56、J-57 |
| E `Writes<…>` 列的算寫者 | 列出來的 Resource 算 D-E2 的寫者；列 zone C 是 Z2、列 `KernelSettings` 是 D-E18；hook 用 `Writes<…>` 是 S-18 | 這些都通過啟動 | hook 的 `ExclusiveWorld<>` 當場改成員回 Err（新案，5.3） | 不由機制守（確認清單第 4 件） | `NestedRunTests` :1373（拿掉）、:2045 第 2 段（拿掉；第 1 段是開局的、留到下一批）、:1338（拆：Z2、D-E18、S-18 三段改成「通過」；S-17 兩段留到第 4 條） | 確認清單第 4 件；統合 9.1 DI-KS 23 |
| F `KernelSettings` | System、hook、run 條件不能宣告它（D-E18）；`ExclusiveWorld` 讀寫它是 Err（S-15）；第一次 Run 之後外面寫是斷言；只在第一次 Run 讀 | 宣告、讀、寫都准；第一次 Run 之後外面寫也准 | 改了的值在**下一次最外層 Run** 生效（新斷言） | 「下一次 Run 生效」成為守的性質 | `CoverageTests` :186（改成正面：finish 通過）、:277（拆：D-E18 那半拿掉）；`SchedulerTests` :477（拆：D-E18 兩段拿掉）；`HookFlushTests` :430 第 1 段（改：外面寫的值下一次 Run 生效）；`ExecutorTests` :590（拆：D-E18 與「寫了斷言」拿掉、換成下一次 Run 生效；其餘留）；`NestedRunTests` :1389（改：讀到值、寫了下一次 Run 生效） | `kernel-settings-read-write`；統合 7.2 J-50 |
| G 執行緒的啟動檢查 | System 或 Resource 要的執行緒角色和 schedule 的 runner 不合，在 finish 失敗（X-E2、X-E3、D-E10）；一個 World 的 schedule 在兩條 runner 上（D-E13）；`ExclusiveWorld` 讀親和不合的 Resource 是 Err（S-16） | 啟動時不再擋 | 綁執行緒的 Resource 在別的執行緒被碰到就斷言（新案，5.3） | 部分由新的斷言守（碰到時才抓）；runner 的安排在編排層 | `SchedulerTests` :355 J-28（拿掉）、:522（拆：D-E13 那段拿掉；D-E12 留到第 3 條）；`NestedRunTests` :1287（拿掉） | `thread-binding-by-runner`；統合 7.2 J-28 |
| H 登記之後不能再登記 | finish 之後拿不到 Registrar（R-2） | 允許動態插入的 World 可以在 Ready 時再登記或拿掉 System | 不允許的 World：`OpenRegistrar` 回 Err、不改 World（新）；用作廢的 Registrar 登記是斷言（新） | 由 DI-KS 26 與新案守 | `RegistryTests` :176（改：Registrar 只從開 Registrar 那一步來；加新案）；`CoverageTests` :85（改：`Registrar` 變成只能移動，`is_move_constructible` 的斷言改成「可移動、不可複製」，壽命改由世代號守） | `dynamic-insert-per-world`；統合 7.2 J-19 |
| I 繞路改成員（**看第 12 節第 1 件**） | 獨占 System 經儲存本身改成員，在跑 schedule 之前或之後，都由 W-3 斷言 | (a) 無；(b) 這兩種繞路不再被抓 | (a) 經句柄改成員之後再繞路也抓（新案，5.3） | (a) 本身；(b) 不由機制守 | `NestedRunTests` :260、:273（(a) 留、(b) 拿掉） | `immediate-change-via-exclusive-world` |

### 5.2 沒有緩的面的（同一個性質換觀察方式，或期待值隨定義變；照 §14.4 由實作者改、事後報告）

| 案例 | 改什麼 | 為什麼沒有緩的面 |
|---|---|---|
| `SchedulerTests` :250、:265；`SpawnRejectTests` :993 第 1 段 | 加 `TieBreaks().empty()` 與宣告的先後 | 拿掉 D-E8 之後只剩「finish 通過」，什麼都抓不到；加上斷言才守得住原來的性質（偵察發現統合 7.2 寫「留」不夠） |
| `StrictOrderTests` :113 | 拿掉 `RequireExplicitOrder`，搬到 `SchedulerTests`（J-21／J-23 旁） | 「集合的先後消掉平手」別處沒有守 |
| `CoverageTests` :99、:109（J-24「同一張 schedule 的錯誤都收齊」） | 第二個錯誤從 D-E8 換成同一張 schedule 裡留下的錯誤（:99 用 D-E5；:109 的 D-E16 那半照留） | 只刪掉的話，「分析到第一個錯就停」的突變會活下來 |
| `SchedulerTests` :597（J-24「不同碼一起回報」） | 期待的碼改成 D-E3、D-E4、D-E7 再加一個留下的碼（D-E19），維持 `size() >= 4` | 同一個性質，換掉被拿掉的碼 |
| `ExecutorTests` :316、:331（被丟掉的 kernel Err 照樣回報、和 System 自己的 Err 並列） | 觸發從 UntypedWrite 換成 `Spawn::New` 的 `AllotterExhausted` | 守的是 `CallError` → `call.returned` 的一般規則，不是 F-10 |
| `RegistryTests` :43；`SchedulerTests` :152、:618；`HookFlushTests` :449、:865；`SpawnRejectTests` :789；`InitialValueTests` :684、:897；`CoverageTests` :501 | 指紋 golden 與版本（5.4） | 格式改版，判定（跨平台一致、決定性、不等式）不變 |

### 5.3 新增的案例（先寫、先看到失敗，§14.3）

| 項 | 案例 |
|---|---|
| S2 | 建 Plan 之後再開 Registrar、加一個 System、再建 Plan：版本號加一、下一次 Run 跑到新 System、舊 System 的 `Changed` 與 `Local` 保留；`RemoveSystem` 之後不再跑、名字不存在回 false；不允許動態插入的 World 在時 `OpenRegistrar` 回 `DynamicInsertRefused`、Registry 版本與 World 不變；重建失敗退回上一版、World 照跑舊 Plan；作廢的 Registrar 登記是斷言；開了 Registrar、還沒建 Plan 就 Run 是斷言；新登記的元件與 Resource 在已建好的 World 裡可以用 |
| S3 | 版本號（1、每次成功加一、失敗不加）；`Hooks()`、`InitialValues()`、`Schedules()` 的內容；`annotatedWrites` |
| S4 | `ExclusiveWorld<Writes<R>>` 的存取集合沒有 R 的寫（J-12 旁，偵察發現現在沒人守） |
| S5 | 七個函式各一案（值、stamp、`Added`）；OnAdd／OnRemove 當場跑、hook 記的變更回傳前套用到空、超過清空輪上限是 `HookRoundLimit`；`InitialValue` 當場填；`SpawnWithId` 不能生成時回傳理由並記 `RejectedSpawn`；閉包裡呼叫是 `ClosureChangesMembership`；hook 的 `ExclusiveWorld<>` 呼叫是 `HookChangesMembership`；（第 12 節選 (a) 時）先繞路改成員、再經句柄生成，斷言；當場改之後再跑巢狀 schedule，裡面看得到；驗證打開時當場改不被報成違規 |
| S6 | `Commands::SetResource`／`RemoveResource` 在同步點第 7 步套用、依 (ResourceId, planPos, recSeq) 的順序、stamp；hook 用它記進下一窗 |
| S7 | Run 中途 System 改 `KernelSettings`，這一次 Run 不變、下一次最外層 Run 生效；巢狀的 `RunSchedule` 不重讀 |
| S10 | 綁執行緒的 Resource 在記下的執行緒以外被 `Res`、`ExclusiveWorld`、World API 碰到是斷言；同一條執行緒照常；拿掉值之後換執行緒放回照常 |

### 5.4 寫死的指紋版本與固定值

卡片照 L17 判：**C 指紋與 Plan 匯出的標頭也在這一批升版**（偵察發現：C 指紋共用 `SystemBytes`／`ResourceBytes`，`Fingerprint.cpp:151`、`:178` 雜湊 `affinity`；`ExportPlans` 印 `runner=`，`:422`）。不採「寫空字串保持位元組不變」：那是為了不動守衛而留下假欄位，AGENTS §4 不准以「不用改測試」當決定的理由。

| 項 | 現在 | 之後 | 位置 |
|---|---|---|---|
| Plan 指紋版本 | 4 | 5 | `Fingerprint.cpp:63` |
| Registry 指紋版本 | 2 | 3 | `:66` |
| C 指紋版本 | 6 | 7 | `:74` |
| Plan 匯出標頭 | `gyo.kernel.plans 2` | `gyo.kernel.plans 3` | `:413` |
| golden（重產） | — | — | `RegistryTests.cpp:41`；`SchedulerTests.cpp:133-138`；`HookFlushTests.cpp:882`、`:884`、`:886`；`InitialValueTests.cpp:904`、`:906` |
| 版本的斷言 | — | — | `CoverageTests.cpp:504`、`HookFlushTests.cpp:483-487`、`InitialValueTests.cpp:717`、`SpawnRejectTests.cpp:795-796`、`SchedulerTests.cpp:636` 等（全表在 `mutant-guard-inventory.md` Task 3） |

### 5.5 只改名或改準備資料的（判定不變）

`RegistryTests` :103、:125（`IsFrozen` 的語意：兩步之後「建好過 Plan」）；`HookFlushTests` :408、`ExecutorTests` :446、`NestedRunTests` :285（`Product` → `WorldExecutor`）；`HookFlushTests` :674（標題的「no Z5」）；`NestedRunTests` :574、:1167、:1368、:1850、:2018（`Writes<Flag>` 變成標註，判定不變）、:1265（準備資料的 `.affinity` 改成 `.threadBound`、標題過時的字拿掉）；輔助：`WorldTestSupport.hpp` 的 `Rig::product`、`WorldExecRecord.cpp:149`。CMake：`kernel.strict_order` 的 ctest 拿掉；`GYO_KERNEL_J_ENTRIES` 拿掉 J-28、J-32b、J-45（它們的案例全部拿掉之後，比對 0 案的項目會讓 ctest 失敗）——J-45 若 :545 還留著啟動時那段就保留。

---

## 6. 突變的處置

624 個逐一分類（偵察者 Opus／high，`mutant-guard-inventory.md`）：find 落在拿掉或改寫的程式 63（退役 46、改寫 17）；find 還在、預期失敗的案例或錯誤碼變了 25（改清單 14、其中退役 1；只要重跑 11）；只改名 5；不受影響 531。

**退役（47）**，理由都是「守的性質已不由機制守」（第 5.1 節的組）：
- 組 D、E（S4）：`ns-modify-checks-after-call`、`ns-modify-resource-no-read-check`、`ns-modify-resource-checks-after-call`、`ns-listed-not-counted`、`ns-res-layer1-unlisted-declared`、`ns-res-layer2-unlisted-declared`、`we-j45-debug-only`、`we-j45-exclusive-unchecked`、`we-adv-exclusive-zone-half`、`ns-j45-zone-c-writable`、`ns-j45-exclusive-writable`、`ns-res-unlisted-writable`、`ns-res-shared-needs-listing`、`ns-res-zone-c-writable`、`ns-res-settings-readable`、`ns-res-affinity-readable`、`ns-decl-hook-writes-allowed`、`ns-startup-listed-counted`。
- S5：`ns-j45-spawn-ok`（守的是「`ExclusiveWorld` 不能生成」）。
- 組 F（S7）：`rs-j24-de18-resmut-only`、`rs-adv-condition-settings-dropped`、`we-j50-settings-writable`。
- 組 B、C（S8）：`we-j6b-host-zone-c`、`we-adv-j6b-host-zone-c-resource`、`we-j32b-debug-only`、`we-j32b-untyped-unchecked`、`we-adv-untyped-zone-half`、`we-j32b-z5-record-off`、`ns-z5-*` 九個、`hf-round-skips-z5`、`we-j52-recheck-pending-only`、`we-adv-z5-ignores-spawnwithid`、`sr-z5-skips-spawn`、`we-j52-step0-unchecked`。
- 組 A（S9）：`rs-j3-2-ambiguity-tie-j22`、`rs-j3-2-ambiguity-tie-j44`。
- 組 G（S10）：`rs-j24-de13-removed`、`rs-j28-affinity-unchecked`、`rs-j28-main-is-named`。

**改寫（17＋14）**：指紋版本的七個（`hf-plan-version-kept` 等，find 跟著新的常數；`ns-plan-version-not-bumped` 改寫後和 `hf-plan-version-kept` 重複，退役）；`ns-st-settings-after-startup`（find 改成每次 Run 重讀的條件）；`hf-registry-schedule-field`、`hf-registry-fp-schedule-field-when-declared`（`runner` 拿掉後換錨點）；`hf-export-version-kept`（標頭 3）；`rs-j18-plugins-reversed`、`rs-j19-duplicate-built`、`rs-fix-b4-finish-before-resolve`、`rs-j24-stop-at-first`（`Finish` 改建在兩步之上，換錨點、預期不變）；`we-j34-hook-immediate`（拿掉 Z5 的 `pending` 參數時換 find）；B 類 14 個的預期改成 `TieBreaks` 的內容或新的觸發（`we-j3-16-executor-swallows`、`we-adv-dropped-err-beside-own-err-lost`、`we-j32b-dropped-err-lost`、`rs-adv-exclusive-no-conflict`、`rs-j3-3-deferred-not-counted`、`rs-j3-3-event-writes-not-counted`、`rs-j22-deferred-vs-read`、`rs-fix-b2-stop-after-declared-edges` 等，全表在證據檔）。

**不退役**：`we-j3-spawn-immediate`（第 2.1 節）——偵察者確認：改的是一般 System 的 `Spawn`，沒有東西會更新基準，`Executor.cpp:328` 的 W-3 斷言照樣觸發。第 12 節選 (b) 時也一樣活不下來；選 (b) 的話要退役的是 `ns-epoch-check-skipped-for-exclusive`。

**新增**：第 5.3 節每一項至少一個突變，預期失敗寫成能辨識的訊息或案例名（§14.2）；當場改的部分另加「hook 的紀錄沒有當場套用」「句柄的 stamp 沒有更新」「基準直接設成目前的值（吞掉之前的繞路）」「閉包裡沒有拒絕」「hook 裡沒有拒絕」；S2 加「重建失敗沒有退回」「拒絕時仍改了 Registry」「World 沒有接上新版本」；S7 加「巢狀也重讀」；S10 加「不比對執行緒」。

**跑哪些**（D184）：新增的全部、改寫的全部、find 落在本批改動範圍的、殺它的測試被改到的；PR 線合併前背景完整跑一次。

**守衛腳本**：`kernel_guard.py:292` 的 K6 豁免以「Executor」開頭的檔名比對——所以 **`Executor.cpp` 不改名**（只改標頭 `Product.hpp` → `WorldExecutor.hpp`；卡片照 L17 判，第 3.1 節照此修正）。K3 禁止 Kernel 用 `<thread>`／`std::this_thread`：綁執行緒的 Resource 用既有的 `thread_local` 變數（`tlCurrent`）的位址當執行緒的記號，不碰 K3、K4、`kernel_symbols.py`（卡片照 L17 判）。`naming_guard.py`、K1 裡的「Product」指遊戲產品，不改。

---

## 7. Design Invariants 的變更（`target/kernel.zh-Hant.md`；逐條要你核准）

| 條 | 處置 | 改寫後 |
|---|---|---|
| DI-KS 1 | 改寫 | `ExclusiveWorld<…>` 的 Contribution 是整個 World 的值的讀寫；`Writes<R…>` 是選填的標註，不改變它的存取、不算寫者（記進登記表的 `annotatedWrites`） |
| DI-KS 2 | 前半留；後半拿掉 | 留：驗證開著時，經 World 的未宣告存取與 System 邊界的淨變化記為違規、以 Err 回傳；驗證是開發工具。拿掉：untyped `Commands`、`Despawn`、`ExclusiveWorld` 對 zone C／Exclusive／沒列出的單一寫者 Resource 的寫入回 Err |
| DI-KS 3 | 改寫 | 順序只來自 id 與 Plan；Plan 在兩次 `BuildPlans` 之間不變，同樣的登記得到同樣的 Plan |
| DI-KS 4 | 改寫 | 「凍結後不變」改成「已發的 id 在之後的登記與重建中不變，新的往後加」；其餘照舊 |
| DI-KS 5 | 改寫 | 衝突與順序分開；沒有先後的衝突對照登記順序平手、列在 `TieBreaks`，不是錯誤；Kernel 不保證結果對登記順序不敏感 |
| DI-KS 6 | 只改一處 | 寫者清單拿掉「獨占的 System 在 `ExclusiveWorld<Writes<…>>` 列出來的 Resource」與「untyped 路徑寫不到 Exclusive 欄，記錄當下以 Err 回傳」；整條隨第 4 條拿掉 |
| DI-KS 9 | 改寫一句 | 「獨占的 System 只在它跑的 schedule 的同步點變（DI-KS 18）」改成「`ExclusiveWorld` 可以當場改（DI-KS 18）」；其餘照舊 |
| DI-KS 14 | 改寫 | 拿掉「一 World 一執行緒角色」的啟動檢查（H-2），改成「綁執行緒的 Resource 被別的執行緒碰到就斷言」；「同一個 World 同一時間只被一條執行緒跑」與時鐘的部分照舊（時鐘的部分隨第 3 條） |
| DI-KS 15 | 改寫 | 照統合 9.1 的職責清單 (1)～(13)；另附過渡表：開局（第 2 條）、時鐘與事件的 domain（第 3 條）、區與寫者數量、digest、回滾的宣告（第 4 條）此刻仍在 Kernel，隨各批拿掉；第 (13) 項的原語隨第 4 條加入 |
| DI-KS 18 | 改寫（`immediate-change-via-exclusive-world`） | 一般的 System 執行期間成員不變，結構變更只經同步點；`ExclusiveWorld` 可以當場改成員與增刪 Resource，變更觸發的 hook 當場跑、它們記的變更在回傳前當場套用到空；hook 的 `ExclusiveWorld<>` 與閉包裡不能當場改；**繞過句柄改成員仍由常開的檢查擋下**（第 12 節第 1 件；選退役的話這一句拿掉） |
| DI-KS 19 | 刪一句 | 「規則 Z5 的重查也看外面每一層留著的紀錄…」拿掉 |
| DI-KS 20 | 改名 | `Product::Run` → `WorldExecutor::Run` |
| DI-KS 21 | 加一句 | 閉包裡不能當場改成員（`ClosureChangesMembership`） |
| DI-KS 23 | 拿掉 | 「列出來的算寫者、沒列的寫不進去」（確認清單第 4 件）；`Writes<…>` 留作選填標註（DI-KS 1） |
| DI-KS 24（新） | 新增 | **`KernelSettings` 下一次最外層 Run 生效**：只放開發與調校的設定，System 與外面都可以讀寫，每次最外層 Run 開始時重讀；會改變 World 規則的開關（例：准不准動態插入）在建 World 時的選項，執行中不能改 |
| DI-KS 25（新） | 新增 | **登記表唯讀、帶版本**：Kernel 交出的登記資訊唯讀、帶版本號；交出去的參照只在當前版本有效；修改只走 `Registrar` |
| DI-KS 26（新） | 新增 | **動態插入照 World 的宣告**：World 建立時宣告准不准在 Ready 時再登記或拿掉 System；不准時 `OpenRegistrar` 回傳執行時錯誤、不改 Registry 與 World；准時下一次最外層 Run 前接上新版本，指紋跟著重算、不保證兩端一致；重建失敗退回上一個版本 |

Replacement Criteria：
- RC 3（觸發條件是 D-E8）拿掉。
- 新增 RC 16「**Kernel 又長出限制**：Kernel 的職責清單（DI-KS 15）要多加一項，或判不出某樣東西屬不屬 ECS——不數次數，出現就停下來問（L17）」。
- RC 7 的主詞改寫（Host → 編排層）留到第 2 條（編排層那時才存在）。

---

## 8. 判定（先宣告，結果出來之後不放寬）

| # | 判定 | 怎麼量 |
|---|---|---|
| G-1 | 本機 `ctest -L "cpu|shader"` 全過（macOS arm64，Debug） | ctest 的輸出 |
| G-2 | 第 5 節的每一案照表處置；新增的案例（第 5.3 節）先在改程式之前寫好、看到失敗（Red），再改程式看到通過（Green）——新行為的案例照 §14.3 的形式 | 測試的提交順序與輸出 |
| G-3 | 掃描 0 命中：`git grep -n -P "<式子>" -- engine/kernel`，式子：`\bProduct\b`、`\bRequireExplicitOrder\b`、`\bDE(8|10|13|18)\b`、`\bXE[23]\b`、`\bkMainRole\b`、`\bAffinity\b`、`\bUntypedWrite\b`、`\bZoneCDespawn\b`、`\bExclusiveWorldWrite\b`、`\bUndeclaredResourceWrite\b`、`\bUnreadableResource\b`、`\bCheckDespawn\b`、`\bCheckUntypedCommand\b`、`\bCheckExclusive\w*\b`、`"Z5"`、`\bownerPlugin\b` | 每個式子先在 a08c16f6 用已知例命中一次；終了碼區分 0 命中（1）與失敗（其他） |
| G-4 | 突變：第 6 節「要跑」的全部照 D184 跑，每一個都在預期的失敗下 killed；改寫的突變的預期失敗寫在清單裡 | `tests/common/mutation/run_mutations.py` 的輸出 |
| G-5 | 守衛腳本的 Python 測試全過（`test_kernel_guard.py`、`test_naming_guard.py`、`test_kernel_symbols.py`、`test_store_stamp_guard.py`） | unittest 的輸出 |
| G-6 | CI 的 L1 三平台通過（PR 上） | GitHub Actions |

---

## 9. 誰做、用什麼

上段（公開介面與 Design Invariants；navigator「上段」第一條）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 事前宣告的作者 | Opus／high（本卡） | — |
| 事前宣告與守衛、突變清單的敵對式檢查 | Fable／high | 和作者換模型（§4、§14.2） |
| 實作者 | Opus／high | 統合第 8 節 |
| 審查者 | Fable／high | 和實作者換模型 |
| §14.4 的檢證者（緩的面的 1～5 用實際資料確認） | Fable／high | 統合第 8 節；navigator「檢證者確認 4 時不用小模型」 |
| 當場套用（S5 第 1、2 點）的逐行複核 | Opus／xhigh（要你同意才用） | 時序與 stamp 容易錯；ギアボックス「xhigh 局所的にだけ」 |

時間估計：10～14 h（統合第 8 節）。超過 1.5 倍（21 h）或單一問題累計超過 2 h 就停下回報。

---

## 10. Architecture Delta（和統合第 4 節的差分）

- 新增的公開介面：`RegistrationOwner`、`Registry()`、`Registry::OpenRegistrar`、`Registry::BuildPlans`、`Registrar::RemoveSystem`、`WorldOptions`、`World::Options()`、`FrozenRegistry::Version`／`Hooks`／`InitialValues`／`Schedules`、`AccessSet::annotatedWrites`、`ExclusiveWorld` 的七個成員函式、`Commands::SetResource`／`RemoveResource`、`ResourceOptions::threadBound`、三個 `KernelErrorCode`。統合第 4 節第 3 點列的介面範圍內，沒有新的 Module、沒有新的依賴邊。
- 和統合不同的四處：J-56 兩案的去留（第 12 節交使用者）；`we-j3-spawn-immediate` 不退役（第 2.1 節）；C 指紋與 Plan 匯出標頭在這一批就升版（統合 L10 寫 C 指紋隨權威 Module；但它讀的 System 與 Resource 位元組在本批就變，第 5.4 節）；統合 7.2 寫「留」的 J-22 兩案、J-44 有先後的那半、`NestedRunTests` :605 與 :646、`StrictOrderTests` :113 要換觀察方式才守得住（第 5.2 節，偵察發現）。
- `Registrar` 從「不能複製、不能移動」改成「只能移動、帶世代號」：防止留到 Build 之後再用的保證改由世代號給（斷言）。

---

## 11. 範圍的限界

- 宣告寫在偵察之後、實作之前；時間估計照統合，沒有重估。
- 別家的系譜：Bevy 讀本地原始碼（6a911653f）的指定行；flecs 讀 `docs/Systems.md`；其餘照統合的證據，沒有重讀。
- 當場改成員之後「hook 當場跑、記的變更當場套用到空」是設計推導，沒有試做；實作時若發現和巢狀的紀錄（DI-KS 19）或存取驗證衝突，照 AGENTS §14 停下回報。
- 第 5、6 節的逐案清單由偵察者（Opus／high）讀程式寫成，本卡抽查。

---

## 12. 要你定的

1. **J-56「獨占 System 自己繞路改成員」兩案**（`NestedRunTests.cpp:260`、`:273`：獨占 System 經儲存本身拿掉一格，在跑 schedule 之前或之後；現在由 W-3 的斷言擋）。HANDOFF `immediate-change-via-exclusive-world` 的「定下的」寫兩案退役（原話只有「沿用 ExclusiveWorld」，退役是卡片推出來的）。
   - **(a) 保留（建議）**：句柄的當場變更更新這一層的基準，繞過句柄的變更照樣斷言。兩案留著、判定不變；DI-KS 18 保留「繞路由常開的檢查擋下」。代價：句柄每次變更後多一次基準更新（O(欄數) 的加總，和現在每次呼叫前後做的一樣）。
   - **(b) 退役**：獨占 System 的呼叫不查 W-3。兩案拿掉，緩的面：獨占 System 經 World API 或儲存本身繞路改成員不再被抓。
   - 卡片的理由：Bevy 的 `&mut World` 由 Rust 的借用規則保證只有一條路，C++ 沒有；W-3 是 GYO 用來補這個差的。(a) 不影響當場改的能力。
2. **第 5 節逐案的判定變更**（AGENTS §14.4）與**第 7 節的 Design Invariants 變更**：逐條核准。
3. **xhigh 的逐行複核**（第 9 節最後一列）要不要用。
