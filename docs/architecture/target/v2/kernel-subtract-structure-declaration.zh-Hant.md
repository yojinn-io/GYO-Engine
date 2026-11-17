# 事前宣告：Kernel 減法・結構段（`v8-arch-gyo2-kernel-subtract-structure`）

更新：2026-11-17（修訂 1）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：草稿，未核准。使用者核准之前不改程式、不改條文與守衛。**

經過：修訂 0（41086c8f）送 Fable／high 敵對式檢查，必改 9、建議改 15、小問題 8（報告 `declaration-r0-adversarial-check.md`）。修訂 1 照它改：S2 的介面改成呼叫範圍內的 Registrar 加暫存副本（M7、M8、S15）；當場改的 hook 時機交使用者（M6，第 12 節第 2 件）；登記表照核准的 L6 補上順序圖與 effects（M5）；第 5.1 節補漏列與錯列（M2～M4）；第 7 節補齊並加守衛欄（S6）；時間重估、加檢查點（S14）；其餘建議改與小問題逐條處理（第 13 節對照表）。修訂 1 之後送了窄複查（第 13 節末）。

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

**將來的指令集。** 遊戲跑起來之後，輸入一行指令就加一個 System。這一批給 Kernel 那一半：登記分成「開 Registrar 登記」「建 Plan」兩步、可以重複；World 建立時宣告准不准動態插入，不准時拒絕、回傳錯誤、World 照跑（使用者 11-17：「如果World的宣告可以，那就可以動態插入系統，但是，寫明不保證指紋一致性。如果宣告不行，那麼就不能動態插入，插入就拒絕，返回信息（不退出，但是System無法插入，日誌報警告）。」）。「日誌報警告」由呼叫的人記（編排層），本批 Kernel 只回傳錯誤；下一批之前沒有人記這個警告（第 11 節）。編排層怎麼把它接到指令（以及登記單位 `Plugin` 搬出 Kernel）在下一批。

**PvP 的 Match。** 現在 Kernel 在執行中攔 untyped `Commands`、`ExclusiveWorld`、`Despawn` 對判定資料的寫入，第一次 Run 之後從外面寫 zone C 也斷言。這一批之後全部拿掉（使用者 11-17「拿掉執行期的攔截，伺服器權威改靠 digest 比對與伺服器覆蓋來抓錯。」）。接手的 digest 比對要等網路 Module，這段期間沒有東西抓（統合第 8 節「空窗」，gyo2 上沒有產品）。

---

## 2. 範圍

### 2.1 這一批做的（S1～S11）

| # | 內容 | 統合的根據 |
|---|---|---|
| S1 | Kernel 的型別 `Product` 改名 `WorldExecutor`（`Product` 這個名字之後給編排層，本批不建編排層） | `kernel-world-exec-type`；3.1 |
| S2 | 登記兩步、可重複：開 Registrar、建 Plan；建好之後可以再開；World 建立時宣告准不准動態插入；owner 由開 Registrar 的人給（`ownerPlugin` 改名 `owner`） | L5 的 Kernel 半；3.5；DI-KS 15 第 (12) 項 |
| S3 | 唯讀、帶版本號的登記表：版本號每次建 Plan 加一；補上 hook、`InitialValue`、schedule 的選項與 stage、具名集合的選項、每張 schedule 解析後的順序圖、`SystemInfo` 的 effects、`Writes<…>` 標註；World 的宣告由 World 交出 | L6；3.3 |
| S4 | `ExclusiveWorld` 讀寫任何元件與 Resource 的值；`Writes<…>` 留作選填標註：不進存取集合、不算寫者 | L7；`exclusive-writes-optional`；確認清單第 4 件 |
| S5 | `ExclusiveWorld` 可以當場改成員、當場增刪 Resource；DI-KS 18 改寫；hook 的 `ExclusiveWorld<>` 不能當場改 | `immediate-change-via-exclusive-world`；9.1 DI-KS 18 |
| S6 | 一般 System 經 `Commands` 增刪 Resource，在同步點的 resource 階段套用 | 修訂 38；DI-KS 15 第 (5) 項。批次表與統合第 8 節的本批那一列沒有寫它，是主會話開卡時加進本批的（讀檔要用）；列為和統合的差分（第 10 節） |
| S7 | `KernelSettings`：拿掉 D-E18；每次最外層 Run 開始時重讀；第一次 Run 之後外面也能寫 | L9 |
| S8 | 拿掉執行期的攔截（L3 的六條）：untyped `Commands` 的檢查、`Despawn` 記錄當下的 Z5、同步點的 Z5 複核、W-6b；`ExclusiveWorld` 的寫入檢查併在 S4 | L3；7.3 |
| S9 | 拿掉歧義的偵測：D-E8、`RequireExplicitOrder`、(c) 歧義；所有沒有先後的衝突對照登記順序平手 | `no-ambiguity-detection`；`strict-order` 退役 |
| S10 | 拿掉執行緒的啟動檢查（H-2、X-3：D-E10、D-E13、X-E2、X-E3、`ScheduleOptions::runner`、`SystemOptions::affinity`、`kMainRole`）；綁執行緒的 Resource 被別的執行緒碰到就斷言 | `thread-binding-by-runner`；3.1 末列 |
| S11 | Registry 與 Plan 指紋改版、golden 重產（第一次） | L10；3.4 |

另外三件附帶的：
- DI-KS 15（Kernel 的職責清單）照統合 9.1 改寫，附「尚未拿掉、隨哪一批拿掉」的過渡表（第 7 節）。
- K0 入口檢查的先後（批次表「主會話自己判的」交到本批）：**照舊，先看呼叫有沒有寫錯，再看 World 是否已經中斷**。卡片照 L17 判：理由是現有程式就是這個順序（`Executor.cpp:444-481`），而且使用者沒有改它的理由；別家沒有可比的——Bevy 的 `World::run_schedule` 只有「schedule 找不到」一種失敗（`world/mod.rs` `try_schedule_scope`），沒有「已中斷」的狀態，說明不了兩種檢查的先後。不用改程式。
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
struct RegistrationOwner { std::string name; std::uint32_t seq; };  // 編排層給；Kernel 只存、只比相等、只拿來印（統合 3.5）

class Registry {
public:
    Registry();                                    // 新：沒有 Plugin 清單
    explicit Registry(std::vector<Plugin*>);      // 照舊，到下一批
    // 第一步：開一個 Registrar、交給 build、build 回來就關掉（和 Plugin::Build(Registrar&) 同形）。
    // Ok：登記的內容記在暫存的副本裡，下一次 BuildPlans 成功才生效。
    // Err（執行時錯誤，Registry 與 World 都不變）：用這份 Registry 建、還活著的 World 裡有一個宣告了不准動態插入。
    template <class Build>   // Build：可以用 Registrar& 呼叫的可呼叫物
    Engine::Base::Result<void, KernelError> Register(RegistrationOwner owner, Build&& build);
    // 第二步。Ok：暫存的副本換上去、版本號加一、Plan 與指紋重算、用它建的 World 當場接上。
    // Err：所有啟動錯誤一次收齊；暫存的副本丟掉，Registry 與 World 都還是上一個版本（第一次失敗時 Registry 不能用，照舊）。
    Engine::Base::Result<void, StartupReport> BuildPlans();
    // 放棄還沒建 Plan 的登記：暫存的副本丟掉。
    void DiscardRegistrations();
    Engine::Base::Result<void, StartupReport> Finish();   // 照舊（只用在第一次）：每個 Plugin 一次 Register → Plugin::Finish → 建 Plan
};
struct WorldOptions { bool dynamicInsert = true; };      // World 建立時的宣告（L5）；WorldExecutor 的建構子轉交
```

規則：
1. **暫存的副本**：一次 `BuildPlans` 之前的第一個 `Register` 把現在的表複製一份，之後的登記都寫進副本；`BuildPlans` 成功時換上去（舊的表連同交出去的參照一起作廢，正好是版本號變的那一刻，DI-KS 25），失敗或 `DiscardRegistrations` 時丟掉副本。所以：登記到一半的內容外面看不到；失敗的那一輪發出的 id 作廢，下一輪從上一個版本接著發（DI-KS 4 改寫成「**建 Plan 成功的版本**發出的 id 在之後不變」）。代價：每一輪複製一次表（登記是少有的事；第一次登記時表是空的）。
2. **呼叫範圍內的 Registrar**：Registrar 只在 `build` 回呼裡有效，照舊不能複製、不能移動——J-19 的編譯期守衛（`CoverageTests` :85）照留、沒有緩的面。修訂 0 的「只能移動、帶世代號」退掉：檢查指出移動得出去的 Registrar 會比 Registry 活得久而懸空，世代號蓋不到（S15）。
3. **前提（斷言）**：`Register`、`BuildPlans`、`DiscardRegistrations` 呼叫時，用這份 Registry 建的每個 World 都在 Ready，而且這條執行緒上沒有正在跑的 System 或 hook。斷言抓得到同一條執行緒的情形（例：指令的 System 在自己的呼叫裡重建，會讓 `Executor.cpp:177`、`:255` 拿著的 Plan 與 System 懸空）；別的執行緒上的 World 在跑時，Kernel 看不到（Kernel 沒有原子操作，守衛 K3），這一半是呼叫的人的規則，登記進共用規則頁（第 4 節）。使用者的原話是「World 在 Ready 時」。
4. **World 當場接上**：`BuildPlans` 成功時，Registry 記得的每個 World（與它的 `WorldExecutor`）當場接上新版本：新登記的欄、Resource、事件緩衝、hook 的存取與佇列、驗證的排除表往後加；每個 System 的執行狀態照 registrationSeq 重建（留下的保留 `lastRun` 與 `Local`，拿掉的丟掉）。所以建好之後、下一次 Run 之前，World API 就能用新的型別（下一批「建 World 後、第一次 Run 前放 Resource 的預設值」要用到）。
5. **拿掉 System**：`bool Registrar::RemoveSystem(std::string_view name)`，下一次 `BuildPlans` 生效；名字不在目前的表回 false、不記任何東西。拿掉之後 registrationSeq 留空號（不重發）；同一個名字之後可以再登記（R-E2 只比還在的）；同一輪先拿掉再登記同名也可以；別的 System 的 before／after 指著被拿掉的，建 Plan 失敗（D-E4）。ECS 元件、Resource、事件、schedule 不能拿掉（Bevy 也不能取消登記元件）。
6. **准不准動態插入**：`World(const Registry&, std::string name, WorldOptions)`、`WorldExecutor(const Registry&, std::string name, WorldOptions)`。只要有一個用這份 Registry 建、還活著的 World 宣告 `dynamicInsert = false`，`Register` 回 `Err`（新碼 `DynamicInsertRefused`），什麼都不改——同一份 Registry 上宣告允許的別的 World 也一起被擋（登記與 Plan 屬於 Registry）。登記到一半（有暫存的副本）時建 World 是斷言。「日誌報警告」由呼叫的人記（Kernel 的錯誤只在一處記，error-handling DI 2）；登記進共用規則頁。
7. **`Finish()` 與兩步混用**：`Finish()` 只能是第一次建 Plan（照舊走完 Plugin 清單，內部改成每個 Plugin 一次 `Register`，`Plugin::Finish` 照舊在建 Plan 之前、拿到的 `Version()` 是 0）；之後可以用 `Register`／`BuildPlans` 再登記，`Plugin::Finish` 不會再被呼叫（下一批搬到編排層時改成 build → 建 Plan → finish）。
8. **開局**：在第一次之後的一輪登記帶開局的 schedule（`ScheduleOptions::startup`）是斷言（開局在下一批搬到編排層；這之前不讓它「晚跑」或「不跑」，違反 DI-KS 22）。
9. **Extract**：D-E14 的「來源已凍結」改成「來源至少建好過一次 Plan」；來源之後再登記，id 只往後加，讀它的 Registry 不受影響。

系譜：Bevy `Schedule::initialize` 在圖有改動時重建（`schedule.rs:637-650`）、元件可以隨時登記、id 只往後加；flecs 有改動才重建 pipeline（`src/addons/pipeline/pipeline.c:297-311`）；Unity 執行中可以建 System。「每個 World 宣告」四家都沒有，是使用者的設計（L5）。

**退掉的選項與理由**（AGENTS §4；卡片照 L17 判）：
- 介面：(a) 開 Registrar 回傳句柄、可移動（修訂 0）——退掉，見規則 2；(b) **呼叫範圍內的 Registrar** ← 採用：和既有的 `Plugin::Build(Registrar&)` 同形，編排層依登記檔順序對每個 Module 呼叫一次 `Register`，再呼叫一次 `BuildPlans`。系譜：既有的 `Plugin::Build`、Bevy `Plugin::build(&mut App)`。
- 重建失敗：(a) 斷言或中斷（Bevy 建圖失敗是 panic）；(b) 失敗的那一輪丟掉、留在上一個版本 ← 採用。理由（卡片的，不是使用者的話）：使用者對拒絕的情形說「不退出，但是System無法插入」，指令打錯不應該讓遊戲停；暫存副本的設計讓「丟掉」不用另外還原。
- 何時看得到：(a) 登記當下就寫進現在的表（現況）；(b) 暫存副本、建好才換 ← 採用（規則 1；(a) 會讓 World API 看到一半的型別、讓失敗後的 id 重發）。
- 拒絕時不在 Kernel 記 log；`RemoveSystem` 用 bool，不加新的啟動錯誤碼。

### 3.3 S3：唯讀、帶版本號的登記表

`FrozenRegistry`（名字照統合 L6 留著）加：
- `std::uint64_t Version()`：還沒建好過 Plan 是 0（`Plugin::Finish` 裡讀到的就是 0）；第一次 `BuildPlans` 成功是 1，之後每次成功加一，失敗不加。
- `Hooks()`：每個 hook 的名字、觸發、元件、存取、登記處、owner。
- `InitialValues()`：Target、Request、宣告者、登記處。
- `Schedules()`：每張 schedule 的選項與 stage 順序。
- `Sets()`：具名的 System 集合的選項（含集合層級的 before／after）。
- `OrderGraph(schedule)`：Scheduler 解析之後的順序邊（集合與同步點都展開成 System 對 System，含 stage 的先後與照登記順序平手加的邊，每條標來源）。D-E16 讀的就是這張圖（`Scheduler.cpp:503-521`）；回滾 Module 讀它，不在 Kernel 外重寫一次解析規則。
- `SystemInfo::effects`。
- `SystemInfo::access.annotatedWrites`（`ExclusiveWorld<Writes<R…>>` 列的 Resource；S4）。
- World 的宣告由 `World::Options()` 交出（宣告屬於 World，不屬於 Registry）。

約定（DI-KS 25）：交出去的參照與 span 只在當前版本有效，`BuildPlans` 成功之後作廢；修改只走 `Registrar`。事件的上限與讀者名單隨游標那一批。修訂 0 寫「順序圖不另交」，檢查指出理由不成立（集合層級的先後不在 mount 裡、解析是 Scheduler 的規則），而且是拿掉使用者核准的 L6 項目；修訂 1 照 L6 交出。

系譜：Bevy `Schedule::graph()` 回傳 `&ScheduleGraph`（`schedule.rs:678`）、`ComponentInfo::hooks()`（`component/info.rs:135`）；版本號 flecs `rebuild_count`（`src/addons/pipeline/pipeline.c:311`）。

### 3.4 S4：`ExclusiveWorld` 讀寫任何值，`Writes<…>` 選填

- `Modify<T>(entity, f)` 寫任何元件；`Read<T>(f)`、`Modify<T>(f)` 讀寫任何 Resource，含 `KernelSettings`。拿掉 `CheckExclusiveWrite`、`CheckExclusiveResourceWrite`、`CheckExclusiveRead` 與錯誤碼 `ExclusiveWorldWrite`、`UndeclaredResourceWrite`、`UnreadableResource`（S-13 的寫入半、S-14、S-15、S-16）。
- 存取集合：`ExclusiveWorld<…>` 的 Contribution 是「**所有元件與 Resource 的值**的讀寫」；**事件不在內**（現在兩層驗證都把事件放在外面，`World.cpp:149`、`:980-987`，照舊）。`Writes<R…>` 不再是 Resource 的寫，只記進 `annotatedWrites`：不算 D-E2 的寫者、不受 Z2／D-E18 的檢查、不改變衝突；`annotatedWrites` 進 Registry 指紋（它是登記的內容）。
- 存取驗證：獨占 System 宣告了所有元件與 Resource 的寫，第二層的邊界比對不會再把它對它們的寫入算成未宣告（第 5.1 節組 D）。
- S-18（hook 不得用 `ExclusiveWorld<Writes<…>>`）拿掉；改成 S5 的「hook 的 `ExclusiveWorld<>` 不能當場改成員」（執行期錯誤，統合修訂 35：從簽名判不出來）。
- S-17（獨占 System 兼 ingress）是區規則，留到第 4 條。

系譜：Bevy 獨占系統 `&mut World` 不列寫入（`multi_threaded.rs:173`、`:506`）；flecs `immediate`；`Writes<…>` 留作選填是使用者的決定（「嗯，可以留選填」）。

### 3.5 S5：`ExclusiveWorld` 當場改成員、當場增刪 Resource

新增的成員函式（卡片照 L17 判名字：照 GYO 已有的 `World::Insert`／`Remove`／`Despawn`／`SetResource`／`RemoveResource` 與 `Spawn::New`、`SpawnWithId::New`，不照 Bevy 的 `insert_resource`，同一個專案裡同一件事一個名字）：

| 函式 | 回傳 | 說明 |
|---|---|---|
| `Spawn<C…>(C… values)` | `Result<EntityId, KernelError>` | id 由配發器發（同 `Spawn<Bundle<C…>>::New`；用完是 `AllotterExhausted`）；**沒有宣告配發器是 Err（新碼 `NoAllotter`）**——D-E3 在啟動時看不到 `ExclusiveWorld` 會生成（現在 `Detail::Allot` 是斷言，`World.cpp:427`）。取代現在一律回 Err 的 `Spawn()` |
| `SpawnWithId<C…>(EntityId, C…)` | `Result<std::optional<SpawnRejectReason>, KernelError>` | 不能生成時不生成、回傳理由，並照 F-13 記一筆 `RejectedSpawn`（記錄的地方與計數只有一處）。這一層記下、還沒套用的同一個 id 的生成，之後在同步點照 F-13 被丟成 `IdAlive` |
| `Insert<T>(EntityId, T)` | `Result<bool, KernelError>` | 實體不在時不插入、回 false，同時照 F.2 第 5 步計入 `DroppedMissingEntity`（計數只有一處，所以兩邊都有，是刻意的） |
| `Remove<T>(EntityId)` | `Result<bool, KernelError>` | — |
| `Despawn(EntityId)` | `Result<bool, KernelError>` | 先跑每個元件的 OnRemove，再依 ComponentId 升冪拿掉（同同步點第 2 步，`despawn-notify-all-then-destroy`） |
| `SetResource<T>(T)`、`RemoveResource<T>()` | `Result<void, KernelError>`、`Result<bool, KernelError>` | — |

統合留給本批定的三件（統合第 10 節第 12 件）：

1. **hook 什麼時候跑：交使用者（第 12 節第 2 件）**。卡片的建議：**當場**。插入新的格時先填 `InitialValue`、再跑 OnAdd；拿掉時先跑 OnRemove 再拿掉；順序同同步點的各步。這些 hook 記下的變更，在函式回傳之前當場套用到空：一輪一輪套用，**每一輪只套用這次當場變更觸發的 hook 所記的變更**——不套用這一層還沒到同步點的紀錄（DI-KS 19、`nested-apply-per-level`），也不碰這張 schedule 的 hook 佇列；上限是這張 schedule 宣告的清空輪上限（沒宣告就用 `KernelSettings` 的），超過照 F-12 回 `HookRoundLimit`、World 中斷（之前各輪已套用的留著，中斷的 World 不會再跑）。
   要交使用者的原因：使用者 11-15 對同步點定了「hook 記的變更在 schedule 的 Run 結束時一輪一輪清空」（`hook-queue-flush-at-run-end`），RC 14 記著「每個同步點把連鎖做完（Bevy 的做法，使用者 2026-11-15 不採）」；當場改這條路要採的正是 Bevy 那一種（只限這條路）。
   系譜：Bevy 每個 `EntityWorldMut::insert` 結束時 `world.flush()`（`world/entity_access/world_mut.rs:1097`）把 hook 記的命令當場套用；flecs `immediate`「Operations passed the iterator's world execute immediately」（`docs/Systems.md:1505`）。GYO 自己的先例：統合第 16 節「外面在 Ready 時觸發 observer…一輪一輪套到空，上限用 hook 清空輪上限」。另一個選項「進這張 schedule 的 hook 佇列，Run 結束時清」的代價：回滾的還原之後緊接著巢狀跑固定步，裡面那張 schedule 不套用外層的 hook 佇列（DI-KS 19），hook 衍生的變更會在重跑之後才出現。
2. **stamp 與存取驗證的快照**（卡片照 L17 判）：變更本身用句柄目前的 stamp；每一輪 hook 套用取新的 stamp、同步點序號加一（F-6）；函式回傳時句柄照 `RunSchedule` 回來時的做法更新自己的 stamp 與同步點序號（`Params.hpp` 的 `Renew`），之後的寫入排在這些 stamp 之後。存取驗證打開時，**先**把獨占 System 到目前為止自己的改動照它自己的帳比對一次（同 `RunNestedFrom` 的做法，`Executor.cpp:488-512`），**再**做變更與當場套用（hook 照同步點的規則驗、第一層），套用完重取第二層的快照。先比對是為了不讓「先在宣告外改、再當場改」把前面的改動吞掉（檢查 S3）。
3. **限制**（卡片照 L17 判）：
   - 只在序列執行（v1 只有序列，LIM-2）。
   - 在 `Read`／`Modify` 的閉包裡呼叫是 Err（新碼 `ClosureChangesMembership`，比照 X-14：閉包拿著的參考會因為成員變更而失效），World 中斷。
   - hook 拿到的 `ExclusiveWorld<>` 呼叫是 Err（新碼 `HookChangesMembership`，比照 X-13：hook 在套用中跑），World 中斷。
   - **繞過句柄改成員仍然擋**（第 12 節第 1 件）：W-3 的檢查（「呼叫期間成員不變」）照留——一般 System 照舊；獨占 System 的每個當場改的函式，**先比對**目前的成員記號等於這一層的基準（不等就是之前有繞路，斷言），變更與當場套用完**再把基準更新**成新的值。經 World API 或儲存本身繞路改成員，不論在當場改之前或之後，仍然斷言。World API 的結構變更在 Running 時照舊斷言（`World.cpp:145`；`NestedRunTests` :198、:2003 守它，兩個選項都留）。
   - 巢狀不留舊參照：閉包與回傳值本來就不能指回 World（DI-KS 21），句柄的 stamp 在每次變更後更新；不另加規則。

### 3.6 S6：一般 System 經 `Commands` 增刪 Resource

`Commands::SetResource<T>(T)`、`Commands::RemoveResource<T>()`：記一筆 `ResourceInsert`／`ResourceRemove`，在同步點第 7 步依 (ResourceId, planPos, recSeq) 套用（同步點的這一步已經存在，只是沒有記錄者，`World.cpp:847-863`）。untyped `Commands` 沒有存取宣告，Resource 的增刪不進衝突圖（同 Bevy `Commands::insert_resource`，`system/commands/mod.rs:816`）。hook 也可以用（記進 hook 佇列）。

一般 System 的 `Res<T>`／`ResMut<T>` 碰到不存在的 Resource 照舊是斷言（`Params.hpp:393`）；先問在不在用 `ResMut::Present()`（已有，`Params.hpp:381-384`）。可能不存在的 Resource 的參數（Bevy 的 `Option<Res<T>>`）不在本批（沒有點名的 Feature，AGENTS §1）。

S8 之後 `Commands::Insert`、`Commands::Remove`、`Despawn::Entity` 不再有失敗的情形，回傳型別改成 `void`（和 `CommandsFor` 一致；Bevy 的 commands 也不回傳）。卡片照 L17 判；留著不會失敗的 `Result` 等於契約說謊。呼叫處的 `if (auto r = …; !r) return r;` 一起改（第 5.5 節）。

### 3.7 S7：`KernelSettings`

- 拿掉 D-E18：System、hook、run 條件可以宣告 `Res`／`ResMut<KernelSettings>`。
- 每次 `WorldExecutor::Run` 呼叫開始時讀**一次**（取代「第一次 Run 時快取」）；開局在同一次 `Run` 裡先跑時也不再讀（開局的 System 改的值下一次 `Run` 生效）。巢狀的 `RunSchedule` 不重讀。
- 讀到的驗證開關和上一次不同（開→關或關→開）時，第二層的快照全部作廢重取——不然「開、關幾次 Run、再開」會拿第一次的舊快照比，誤報 `PreCallChange`（檢查 S1）。
- `hookRoundLimit == 0` 在每次 Run 開始時斷言（System 寫 0 是 Programmer Error）。
- `KernelSettings` 被拿掉（`RemoveResource`，現在就是允許的狀態）時：Kernel 用預設值；System 宣告 `Res`／`ResMut<KernelSettings>` 而它不在，照一般 Resource 斷言。
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

`Record::emitterZone` 欄位本身與 `RejectedSpawn::emitterZone` 留到第 4 條（區移出時一起拿）。DI-KS 19 的「規則 Z5 的重查」一句拿掉。「裡面的同步點在第 0 步之後失敗、沒有 hook 跑過」的處理（`Executor.cpp` 失敗時重取快照的那段）在拿掉 Z5 之後沒有觸發的方法；程式留著（防禦用），`NestedRunTests` :1881 退役，記在第 11 節。

### 3.9 S9：拿掉歧義的偵測

拿掉 `Registrar::RequireExplicitOrder`（與 `RegistryData::explicitOrderOwners`）、`StartupCode::DE8` 與 (c) 歧義的檢查；Scheduler 對所有沒有先後的衝突對照 registrationSeq 平手，全部列進 `TieBreaks`（含 zone C）。`StrictOrderTests.cpp` 8 案：5 案退役，3 案換觀察方式搬到 `SchedulerTests`（第 5.1 節組 A）。

### 3.10 S10：執行緒

- 拿掉 `ScheduleOptions::runner`、`SystemOptions::affinity`、`Affinity`、`kMainRole`；啟動錯誤 D-E10、D-E13、X-E2、X-E3。
- `ResourceOptions::affinity` 改成 `bool threadBound`（卡片照 L17 判名字：Bevy 叫 `NonSend`，GYO 的登記選項是形容詞或名詞的成員，例 `external`、`rollbackSafe`）：值放進去時記下那條執行緒（`World::SetResource`、`ExclusiveWorld::SetResource`、同步點套用的 `Commands::SetResource`）。之後在別的執行緒**碰到就斷言**，碰到包括：`Res`／`ResMut`、`ExclusiveWorld` 的讀寫與增刪、World API 的讀寫、`SetResource` 蓋過既有的值、`RemoveResource`、`World::Summary()`，以及**有值時銷毀 World**（Bevy 也查銷毀，`non_send.rs:34-46`）。拿掉值之後忘掉記號，換執行緒再放可以。
- 執行緒的記號用 Kernel 既有的 `thread_local` 變數的位址（每條執行緒不同），不引入 `<thread>`——守衛 K3 禁止 Kernel 用它（卡片照 L17 判）。限界：執行緒結束後，新的執行緒可能拿到同一個位址，這時抓不到（第 11 節）。
- 系譜：Bevy `bevy_ecs/src/storage/non_send.rs:74-84`（插入時記執行緒、別的執行緒存取就 panic）。使用者 11-16「別人掉過的坑，我沒必要在跳一次。」

### 3.11 S11：指紋

- **Registry 指紋**：拿掉 `runner`、System 與 Resource 的 `affinity`；加 `threadBound`、`annotatedWrites`；`ownerPlugin` 改名 `owner`。登記單位名單改成「各 owner 第一次 `Register` 的順序」——`Finish()` 照 Plugin 清單的順序呼叫，所以和現在的名單同義，下一批搬到編排層時不用再重產（編排層照登記檔的順序呼叫）。格式版本 2 → 3。
- **C 指紋**：共用 System 與 Resource 的位元組（`Fingerprint.cpp:151`、`:178`），所以也變：6 → 7。
- **Plan 指紋**：雜湊的欄位沒有一個被拿掉（`Fingerprint.cpp:203-212`）；版本 4 → 5 的理由是使用者的慣例：行為改變而不是 Plan 的步驟時，Plan 與 C 的版本各加一（`hook-flush-not-in-plan-bump-versions`）。這一批改了「Plan 何時重建」與平手的範圍。
- **Plan 匯出**：`ExportPlans` 印 `runner=`（`:422`），標頭 `gyo.kernel.plans 2` → 3。
- 所有 golden 重產（第 5.4 節）。

---

## 4. 要改的程式與文件

| 檔 | 改什麼 |
|---|---|
| `engine/kernel/include/engine/kernel/Product.hpp` → `WorldExecutor.hpp`；`src/Executor.cpp`（不改名） | S1；S2 規則 4（接上新版本）、`WorldOptions` 的轉交；S7 每次 Run 讀一次 |
| `Registry.hpp`、`src/Registry.cpp`、`detail/RegistryData.hpp` | S2（`Register`、暫存副本、`BuildPlans`、`DiscardRegistrations`、`RemoveSystem`、World 的名單、owner）；S3；S4（`Writes` 不計寫者）；S7（D-E18）；S9（`RequireExplicitOrder`）；S10 |
| `src/Scheduler.cpp` | S3（交出順序圖）；S9（D-E8、(c) 歧義）；S10（H-2 的檢查若在這裡） |
| `Params.hpp` | S4；S5（新成員函式、閉包與 hook 的拒絕）；S6（含回傳型別改 `void`）；S8（`Commands`、`Despawn` 的檢查） |
| `World.hpp`、`src/World.cpp`、`detail/Context.hpp`、`detail/Storage.hpp` | S2（`WorldOptions`、接上新版本）；S5（當場套用）；S7（快照作廢）；S8；S10（記執行緒、斷言） |
| `Declarations.hpp` | S10（`Affinity`、`runner`、`kMainRole`） |
| `KernelError.hpp` | 拿掉 `UntypedWrite`、`ZoneCDespawn`、`ExclusiveWorldWrite`、`UndeclaredResourceWrite`、`UnreadableResource`、`DE8`、`DE10`、`DE13`、`DE18`、`XE2`、`XE3`；新增 `DynamicInsertRefused`、`NoAllotter`、`ClosureChangesMembership`、`HookChangesMembership` |
| `src/Fingerprint.cpp` | S11 |
| `tests/common/kernel/*`、`tests/common/kernel/CMakeLists.txt` | 第 5 節 |
| `docs/architecture/plans/kernel/scripts/mutations.json` | 第 6 節 |
| `docs/architecture/target/kernel.zh-Hant.md` | 第 7 節 |
| `docs/architecture/target/v2/system-contract-page.zh-Hant.md` | 作者看到的改變：`ExclusiveWorld` 的新函式、`Commands` 增刪 Resource、`KernelSettings` 可寫、歧義不報錯、兩步登記 |
| `docs/cross-component-rules.md` | 新增三列（AGENTS §1 MUST；owner：Engine 的 Kernel）：(1) `Register` 回 `DynamicInsertRefused` 時呼叫的人記警告；(2) 拿到的 `FrozenRegistry` 參照在版本號變了之後重新讀；(3) `Register`／`BuildPlans` 只在用這份 Registry 建的每個 World 都停著（Ready、沒有別的執行緒在跑它們）時呼叫。這頁由圖書管理員維護，同一個變更加列並通知 |
| `docs/dev_logs/2026_11_xx_gyo2_kernel_subtract_structure.zh-Hant.md` 與 v2 的 README 索引 | AGENTS §15（同一個變更） |
| `docs/architecture/target/v2/HANDOFF.md` | 不改（主會話記；決定照四樣轉給主會話） |

**刻意不改、會過時的 v2 之前的文件**（`pre-v2-target-docs-later`）：`docs/architecture/plans/kernel/spec.zh-Hant.md` 的 R-2、F-10、S-13～S-18、H-2、X-3、D-E8、D-E18 與 J 表；`target/limits.zh-Hant.md` 的 LIM-6；`target/runtime-composition`、`world`、`scheduler`、`system-contract` 的相關句子。統一處理時以本頁與 `target/kernel.zh-Hant.md` 為準。

---

## 5. 守衛與測試的變更（AGENTS §14.4）

全部 15 個測試檔 332 案與編譯失敗的樣本逐案讀過（偵察者 Opus／high，`test-inventory.md`）：主表 64 案（改 16、換觀察方式 8、拿掉 22、拆開 16、看第 12 節第 1 件 2），只要改名或改準備資料的 13 案另列（5.5）。下面依「守的性質」分組，每組寫 §14.4 的 1～5；同一組的案例共用同一個性質。**緩的面**＝以前判失敗、之後判通過的情形；**緊的面**＝以前判通過、之後判失敗的情形。

### 5.1 有緩的面的（要你逐組核准）

| 組 | 1. 守的性質 | 2. 緩的面 | 3. 緊的面 | 4. 性質之後由誰守 | 案例（處置） | 5. 根據 |
|---|---|---|---|---|---|---|
| A 歧義 | 沒有先後的衝突對（任一方 zone C，或呼叫了 `RequireExplicitOrder`）在 finish 失敗（D-E8） | 這些對都通過，照登記順序排、列進 `TieBreaks` | 無 | 「要寫先後」：不由機制守（使用者的決定）。「哪些對算衝突」「每張 schedule 的每一對都列出、各帶 schedule 名」：改用 `TieBreaks` 的內容斷言，照樣守（:163、:189 換觀察方式留下，見右欄） | `StrictOrderTests` :92、:140、:149、:200、:220（拿掉；:140、:149 的性質在 `SchedulerTests` :197、:265、:287 已有）；:163（換觀察：不呼叫 `RequireExplicitOrder`，斷言兩張 schedule 的平手全部列出、依 schedule 再依登記順序、各帶 schedule 名）、:189（換觀察：同上，`TieBreak::schedule` 現在只有這一案斷言）——兩案搬到 `SchedulerTests` J-23 旁；`SchedulerTests` :216（改：斷言四組各列出哪一對、Plan 照登記順序）、:313 J-25（換觀察：`TieBreaks` 有 {WithB, WithoutB}）、:432 J-44（拆：有歧義的那半 :456-460 拿掉，有先後的那半留）；`CoverageTests` :212（換觀察：`TieBreaks` 有那一對）；`SpawnRejectTests` :993 第 2 段（改：finish 通過、`TieBreaks` 有那一對） | `no-ambiguity-detection`；統合 7.2 J-21～J-25、J-44 |
| B 執行期的攔截 | untyped `Commands` 只寫 zone B 的 Shared 欄（F-10）；zone B 的刪除碰到帶 zone C 欄的實體（記錄當下與同步點，含巢狀各層、hook 佇列、清空輪）回 Err | 這些寫入與刪除都照做：刪除先套用，同一窗對同一實體的 zone C 插入因實體不在被丟掉並計數 | 無 | 不由機制守（伺服器權威靠 digest 比對與伺服器覆蓋；網路 Module 之前有空窗，統合第 8 節）。「丟掉並計數」的結果由 `NestedRunTests` :395、`WorldTests` :484 第 3 段、`ExecutorTests` :710 第 3 段守 | `ExecutorTests` :296（拆：五段 UntypedWrite 與 Z5 那段拿掉；「zone B Shared 准」留）、:404 第 3 段（拿掉）、:710（拆：四段 Z5 拿掉；三段留）；`HookFlushTests` :608 第 1 段（拿掉）；`SpawnRejectTests` :758（改：Run 通過、先刪後生成，`RejectedSpawns()==0`）；`NestedRunTests` :409、:420、:433、:445、:1681、:1902（拿掉）、:1694（拿掉：沒有 Z5 就沒有意義的反例）、:1881（拿掉：「裡面的同步點在第 0 步之後失敗」除了 Z5 沒有別的觸發） | `no-runtime-interception`；統合 7.2 J-32b、J-52、J-56 |
| C 第一次 Run 之後從外面寫 zone C | W-6b：第一次 Run 之後 Host 寫 zone C 的狀態（非 boundary、非 external）是斷言 | 這些寫入照做 | 無 | 不由機制守（同 B） | `ExecutorTests` :55（拆：三個 W-6b 的斷言拿掉；W-6 與其他留） | 同 B；統合 7.2 J-6 |
| D `ExclusiveWorld` 的寫入限制 | `ExclusiveWorld` 只寫 zone B 的 Shared 資料；沒列在 `Writes<…>` 的單一寫者 Resource 寫不進去（S-13、S-14）；驗證的兩層把它們當成未宣告 | 寫任何元件與 Resource 都成功、驗證不報 | 寫的值確實寫進去（新的正面斷言，見 :1758） | 不由機制守（`Writes<…>` 標註留給權威策略讀） | `ExecutorTests` :545（拆：zone C、Exclusive、zone C Shared 三段改成「通過、值寫進去」；啟動時的 zone C 那段是區規則、留）；`NestedRunTests` :1304 第 3 段（改：通過、Flag==14）、:622、:634（拿掉）、:646（換觀察：改用事件緩衝經存下的位址寫，留住「巢狀跑之後重取快照」的守衛）、:605（換觀察：同 :646，留住 `Executor.cpp:486-510` 唯一的測試）、:1758（改：六種存取都通過、閉包都被呼叫、值都是 100） | `exclusive-writes-optional`；統合 7.2 J-45、J-56、J-57 |
| E `Writes<…>` 列的算寫者 | 列出來的 Resource 算 D-E2 的寫者；列 zone C 是 Z2、列 `KernelSettings` 是 D-E18；hook 用 `Writes<…>` 是 S-18 | 這些都通過啟動 | hook 的 `ExclusiveWorld<>` 當場改成員回 Err（新案，5.3） | 不由機制守（確認清單第 4 件） | `NestedRunTests` :1373（拿掉）、:2045 第 2 段（拿掉；第 1 段是開局的、留到下一批）、:1338（拆：Z2、D-E18、S-18 三段改成「通過」；S-17 兩段留到第 4 條） | 確認清單第 4 件；統合 9.1 DI-KS 23 |
| F `KernelSettings` | System、hook、run 條件不能宣告它（D-E18）；`ExclusiveWorld` 讀寫它是 Err（S-15）；第一次 Run 之後外面寫是斷言；只在第一次 Run 讀 | 宣告、讀、寫都准；第一次 Run 之後外面寫也准 | 改了的值在**下一次最外層 Run** 生效（新斷言） | 「下一次 Run 生效」成為守的性質 | `CoverageTests` :186（改成正面：finish 通過）、:277（拆：D-E18 那半拿掉）；`SchedulerTests` :477（拆：D-E18 兩段拿掉）；`HookFlushTests` :430 第 1 段（改：外面寫的值下一次 Run 生效）；`ExecutorTests` :590（拆：D-E18 與「寫了斷言」拿掉、換成下一次 Run 生效；其餘留）；`NestedRunTests` :1389（改：讀到值、寫了下一次 Run 生效） | `kernel-settings-read-write`；統合 7.2 J-50 |
| G 執行緒的啟動檢查 | System 或 Resource 要的執行緒角色和 schedule 的 runner 不合，在 finish 失敗（X-E2、X-E3、D-E10）；一個 World 的 schedule 在兩條 runner 上（D-E13）；`ExclusiveWorld` 讀親和不合的 Resource 是 Err（S-16） | 啟動時不再擋 | 綁執行緒的 Resource 在別的執行緒被碰到就斷言（新案，5.3） | 部分由新的斷言守（碰到時才抓）；runner 的安排在編排層 | `SchedulerTests` :355 J-28（拿掉）、:522（拆：D-E13 那段拿掉；D-E12 留到第 3 條）；`NestedRunTests` :1287（拿掉） | `thread-binding-by-runner`；統合 7.2 J-28 |
| D2 `ExclusiveWorld` 不能生成 | `ExclusiveWorld::Spawn()` 一律回 Err `ExclusiveWorldWrite`（S-13：沒有 untyped 的生成） | 獨占 System 能生成實體（有型別的 `Spawn<C…>`） | 生成的實體在呼叫裡就看得到（新） | 不由機制守（使用者的決定）；新行為由 5.3 的 S5 各案守 | `ExecutorTests` :567-570「spawn」那段（改） | `immediate-change-via-exclusive-world` |
| H 登記之後不能再登記 | finish 之後拿不到 Registrar（R-2） | 允許動態插入的 World 可以在 Ready 時再登記或拿掉 System | 不允許的 World：`Register` 回 Err、什麼都不改（新）；在 World 跑著時或 System 裡呼叫是斷言（新） | 由 DI-KS 26 與新案守 | `RegistryTests` :176（改：「Registrar 只在 `Register` 與 `Plugin::Build` 的回呼裡拿得到」，編譯期斷言留；加新案）。`CoverageTests` :85 **不變**：Registrar 照舊不能複製、不能移動（第 3.2 節規則 2） | `dynamic-insert-per-world`；統合 7.2 J-19 |
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

### 5.3 新增的案例

Red 的做法（§14.2、§14.3）：新介面先加宣告與空的實作（建置得過、行為沒有），新案在它上面跑出失敗，再補實作看到通過；另外每個新案至少被一個專屬的突變在預期的訊息下殺掉（第 6 節、G-4）。

| 項 | 案例 |
|---|---|
| S2 | 建 Plan 之後 `Register` 加一個 System、再 `BuildPlans`：版本號加一、下一次 Run 跑到新 System、舊 System 的 `Changed` 與 `Local` 保留；建好之後、下一次 Run 之前，World API 就能用新登記的元件與 Resource；`Register` 之後、`BuildPlans` 之前外面看不到新型別（World API 與 `FrozenRegistry` 都還是舊的）；`BuildPlans` 失敗：版本號不變、World 照跑舊 Plan、失敗那一輪的 id 作廢、下一輪從上一版接著發；`DiscardRegistrations` 之後同上；`RemoveSystem`：不再跑、留空號、名字不存在回 false、同名可以再登記、被別人的 before／after 指著時建 Plan 失敗（D-E4）；不允許動態插入的 World 在時 `Register` 回 `DynamicInsertRefused`、什麼都不改（同一份 Registry 上允許的 World 也被擋）；在 System 的呼叫裡呼叫 `Register`／`BuildPlans` 是斷言；登記到一半建 World 是斷言；第一次之後的一輪帶開局的 schedule 是斷言；`Finish()` 之後可以 `Register`、`Plugin::Finish` 不再被呼叫 |
| S3 | 版本號（建好前 0、`Plugin::Finish` 裡 0、第一次 1、成功加一、失敗不加）；`Hooks()`、`InitialValues()`、`Schedules()`、`Sets()`、`OrderGraph()`（含集合、同步點、stage 的邊與平手邊）、`effects`、`annotatedWrites` 的內容 |
| S4 | `ExclusiveWorld<Writes<R>>` 的存取集合沒有 R 的寫、`annotatedWrites` 有 R（J-12 旁，偵察發現現在沒人守） |
| S5 | 七個函式各一案（值、stamp、`Added`）；沒有配發器時 `Spawn` 是 `NoAllotter`；OnAdd／OnRemove 當場跑、`InitialValue` 當場填；（第 12 節第 2 件選當場時）hook 記的變更回傳前套用到空、**這一層還沒到同步點的紀錄不被套用**、這張 schedule 的 hook 佇列不被動到、超過清空輪上限是 `HookRoundLimit`（schedule 宣告的上限優先）；`SpawnWithId` 不能生成時回傳理由並記 `RejectedSpawn`；這一層記下的同一個 id 的生成之後被丟成 `IdAlive`；閉包裡呼叫是 `ClosureChangesMembership`；hook 的 `ExclusiveWorld<>` 呼叫是 `HookChangesMembership`；（第 12 節第 1 件選 (a) 時）先繞路改成員、再經句柄生成是斷言，經句柄生成之後再繞路也是斷言；當場改之後再跑巢狀 schedule，裡面看得到；驗證打開時當場改不被報成違規，但**當場改之前在宣告外的改動照樣被報**（事件緩衝） |
| S6 | `Commands::SetResource`／`RemoveResource` 在同步點第 7 步套用、依 (ResourceId, planPos, recSeq) 的順序、stamp；hook 用它記進下一窗 |
| S7 | Run 中途 System 改 `KernelSettings`，這一次 Run 不變、下一次 `Run` 生效；開局的 System 改的值在同一次 `Run` 裡不生效；巢狀的 `RunSchedule` 不重讀；驗證「開、關兩次 Run 且資料有變、再開」不誤報；`KernelSettings` 被拿掉時用預設值 |
| S9 | 「集合的先後消掉平手」（`StrictOrderTests` :113 搬來）；兩張 schedule 的平手全部列出、帶 schedule 名（:163、:189 搬來） |
| S10 | 綁執行緒的 Resource 在記下的執行緒以外被 `Res`、`ExclusiveWorld`、World API、`SetResource` 蓋寫、`RemoveResource`、`Summary()` 碰到是斷言；有值時在別的執行緒銷毀 World 是斷言；同一條執行緒照常；拿掉值之後換執行緒放回照常 |
| DI-KS 15 | `StartupCode` 的列舉值等於「統合第 3.1 節的清單＋過渡表裡還沒拿掉的碼」（統合 9.1 承諾的守衛；之後每一批拿掉碼時一起改這張表） |

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

`RegistryTests` :103、:125（`IsFrozen` 的語意：兩步之後「建好過 Plan」）；`HookFlushTests` :408、`ExecutorTests` :446、`NestedRunTests` :285（`Product` → `WorldExecutor`）；`HookFlushTests` :674（標題的「no Z5」）；`NestedRunTests` :574、:1167、:1368、:1850、:2018（`Writes<Flag>` 變成標註，判定不變）、:1265（準備資料的 `.affinity` 改成 `.threadBound`、標題過時的字拿掉）；輔助：`WorldTestSupport.hpp` 的 `Rig::product`、`WorldExecRecord.cpp:149`。CMake：`kernel.strict_order` 的 ctest 拿掉；`GYO_KERNEL_J_ENTRIES` 只拿掉 J-28（`SchedulerTests` :355 是它唯一的案例；比對 0 案的項目會讓 ctest 失敗）。J-32b 與 J-45 **留著**：`ExecutorTests` :296、:545 是拆開不是拿掉，留下的斷言只經 `kernel.J-32b`、`kernel.J-45` 跑（`kernel.others` 排除 `J-*`，`CMakeLists.txt:60-69`）。回傳型別改 `void`（第 3.6 節）：`Commands::Insert`／`Remove`、`Despawn::Entity` 的呼叫處把 `if (auto r = …; !r) return r;` 改成直接呼叫（判定不變；處數在實作時列進 dev_log）。

---

## 6. 突變的處置

624 個逐一分類（偵察者 Opus／high，`mutant-guard-inventory.md`）：find 落在拿掉或改寫的程式 63（退役 47、改寫 16；`ns-plan-version-not-bumped` 改寫後重複、改列退役）；find 還在、預期失敗的案例或錯誤碼變了 25（改清單 14、其中退役 1；只要重跑 11）；只改名 5；不受影響 531。

**退役（48，含下面「改寫」段落裡的 `ns-plan-version-not-bumped`）**，理由都是「守的性質已不由機制守」（第 5.1 節的組）：
- 組 D、E（S4）：`ns-modify-checks-after-call`、`ns-modify-resource-no-read-check`、`ns-modify-resource-checks-after-call`、`ns-listed-not-counted`、`ns-res-layer1-unlisted-declared`、`ns-res-layer2-unlisted-declared`、`we-j45-debug-only`、`we-j45-exclusive-unchecked`、`we-adv-exclusive-zone-half`、`ns-j45-zone-c-writable`、`ns-j45-exclusive-writable`、`ns-res-unlisted-writable`、`ns-res-shared-needs-listing`、`ns-res-zone-c-writable`、`ns-res-settings-readable`、`ns-res-affinity-readable`、`ns-decl-hook-writes-allowed`、`ns-startup-listed-counted`。
- S5：`ns-j45-spawn-ok`（守的是「`ExclusiveWorld` 不能生成」）。
- 組 F（S7）：`rs-j24-de18-resmut-only`、`rs-adv-condition-settings-dropped`、`we-j50-settings-writable`。
- 組 B、C（S8）：`we-j6b-host-zone-c`、`we-adv-j6b-host-zone-c-resource`、`we-j32b-debug-only`、`we-j32b-untyped-unchecked`、`we-adv-untyped-zone-half`、`we-j32b-z5-record-off`、`ns-z5-*` 九個、`hf-round-skips-z5`、`we-j52-recheck-pending-only`、`we-adv-z5-ignores-spawnwithid`、`sr-z5-skips-spawn`、`we-j52-step0-unchecked`。
- 組 A（S9）：`rs-j3-2-ambiguity-tie-j22`、`rs-j3-2-ambiguity-tie-j44`。
- 組 G（S10）：`rs-j24-de13-removed`、`rs-j28-affinity-unchecked`、`rs-j28-main-is-named`。

**改寫（16＋13）**：指紋版本的七個（`hf-plan-version-kept` 等，find 跟著新的常數；`ns-plan-version-not-bumped` 改寫後和 `hf-plan-version-kept` 重複，退役）；`ns-st-settings-after-startup`（find 改成每次 Run 重讀的條件）；`hf-registry-schedule-field`、`hf-registry-fp-schedule-field-when-declared`（`runner` 拿掉後換錨點）；`hf-export-version-kept`（標頭 3）；`rs-j18-plugins-reversed`、`rs-j19-duplicate-built`、`rs-fix-b4-finish-before-resolve`、`rs-j24-stop-at-first`（`Finish` 改建在兩步之上，換錨點、預期不變）；`we-j34-hook-immediate`（拿掉 Z5 的 `pending` 參數時換 find）；B 類 14 個的預期改成 `TieBreaks` 的內容或新的觸發（`we-j3-16-executor-swallows`、`we-adv-dropped-err-beside-own-err-lost`、`we-j32b-dropped-err-lost`、`rs-adv-exclusive-no-conflict`、`rs-j3-3-deferred-not-counted`、`rs-j3-3-event-writes-not-counted`、`rs-j22-deferred-vs-read`、`rs-fix-b2-stop-after-declared-edges` 等，全表在證據檔）。

**不退役**：`we-j3-spawn-immediate`（第 2.1 節）——偵察者確認：改的是一般 System 的 `Spawn`，沒有東西會更新基準，`Executor.cpp:328` 的 W-3 斷言照樣觸發。第 12 節選 (b) 時也一樣活不下來；選 (b) 的話要退役的是 `ns-epoch-check-skipped-for-exclusive`。

**新增**：第 5.3 節每一項至少一個突變，預期失敗寫成能辨識的訊息或案例名（§14.2）；當場改的部分另加「hook 的紀錄沒有當場套用」「句柄的 stamp 沒有更新」「基準直接設成目前的值（吞掉之前的繞路）」「閉包裡沒有拒絕」「hook 裡沒有拒絕」；S2 加「失敗的那一輪沒有丟掉」「拒絕時仍改了 Registry」「World 沒有當場接上新版本」「暫存的登記在建好前就看得到」「System 的呼叫裡沒有擋 `Register`」「拿掉的 System 留下執行狀態」；S5 加「當場套用也套了這一層的紀錄」「第二層沒有先比對」；S7 加「巢狀也重讀」「開關翻轉時沒有作廢快照」；S10 加「不比對執行緒」「銷毀時不查」。

**跑哪些**（D184）：新增的全部、改寫的全部、find 落在本批改動範圍的、殺它的測試被改到的；PR 線合併前背景完整跑一次。

**守衛腳本**：`kernel_guard.py:292` 的 K6 豁免以「Executor」開頭的檔名比對。**`Executor.cpp` 不改名**（只改標頭 `Product.hpp` → `WorldExecutor.hpp`；卡片照 L17 判）。比較：改名的代價是改一個守衛的比對式並重驗它（守衛判定的變更要走 §14.4）；不改名的代價是一個檔名不完全等於型別名（N16），但 `Executor` 仍是 `WorldExecutor` 的一部分、現在就是這樣。第 5.4 節不採「寫空字串」是因為那會在指紋裡留下假欄位、改變資料的意思；這裡只是檔名，不影響任何契約。K3 禁止 Kernel 用 `<thread>`／`std::this_thread`：綁執行緒的 Resource 用既有的 `thread_local` 變數（`tlCurrent`）的位址當執行緒的記號，不碰 K3、K4、`kernel_symbols.py`（卡片照 L17 判）。`naming_guard.py`、K1 裡的「Product」指遊戲產品，不改。

---

## 7. Design Invariants 的變更（`target/kernel.zh-Hant.md`；逐條要你核准）

| 條 | 處置 | 改寫後 | 守衛（之後） |
|---|---|---|---|
| DI-KS 1 | 改寫 | `ExclusiveWorld<…>` 的 Contribution 是所有元件與 Resource 的值的讀寫（事件不在內）；`Writes<R…>` 是選填的標註，不改變它的存取、不算寫者，記進登記表的 `annotatedWrites` | J-10～J-12；新案「`Writes<R>` 不是 R 的寫」 |
| DI-KS 2 | 前半留；後半拿掉 | 留：驗證開著時，經 World 的未宣告存取與 System 邊界的淨變化記為違規、以 Err 回傳；驗證是開發工具。拿掉：untyped `Commands`、`Despawn`、`ExclusiveWorld` 對 zone C／Exclusive／沒列出的單一寫者 Resource 的寫入回 Err | J-5、J-6 的 W-6 那半、J-57 的閉包案；J-32b 留下的那段 |
| DI-KS 3 | 改寫 | 順序只來自 id 與 Plan；Plan 在兩次成功的 `BuildPlans` 之間不變，同樣的登記得到同樣的 Plan | J-1、J-2、J-9、J-20 |
| DI-KS 4 | 改寫 | 「凍結後不變」改成「建 Plan 成功的版本發出的 id 在之後的登記與重建中不變，新的往後加；失敗或放棄的那一輪發出的 id 作廢」；其餘照舊 | J-8、J-18、J-19；新案（失敗那一輪的 id 作廢、留下的 id 不變） |
| DI-KS 5 | 改寫（含併入的段落） | 衝突與順序分開；沒有先後的衝突對照登記順序平手、列在 `TieBreaks`，不是錯誤；Kernel 不保證結果對登記順序不敏感。併入段落裡「J-44＝…要嘛全部 D-E8」改成「有先後的那一組 20 種排列結果都不變」 | J-20～J-25、J-44 有先後的那半；搬來的 :113、:163、:189 |
| DI-KS 6 | 只改兩處 | 寫者清單拿掉「獨占的 System 在 `ExclusiveWorld<Writes<…>>` 列出來的 Resource」與「untyped 路徑寫不到 Exclusive 欄，記錄當下以 Err 回傳」；守衛清單拿掉 J-32b；整條隨第 4 條拿掉 | J-15、J-16（照舊） |
| DI-KS 9 | 改寫 | 「獨占的 System 只在它跑的 schedule 的同步點變（DI-KS 18）」改成「`ExclusiveWorld` 可以當場改（DI-KS 18）」；「hook 的變更屬下一窗」加例外：「`ExclusiveWorld` 當場改觸發的 hook 所記的變更，在那次呼叫回傳前當場套用到空（第 12 節第 2 件選當場時；選進佇列時這一句改成『進這張 schedule 的 hook 佇列』）」；併入段落裡「(c) 資料的保護由 Z5」與守衛 J-32b 拿掉；其餘照舊 | J-3、J-31～J-34、J-54、J-55、J-56；S5 新案 |
| DI-KS 13 | 改一句 | 「來源 World 的 Registry 必須先凍結」改成「來源的 Registry 至少建好過一次 Plan」 | J-43、D-E14 的案 |
| DI-KS 14 | 改寫 | 拿掉「一 World 一執行緒角色」的啟動檢查（H-2）與守衛「finish 檢查 H-1、H-2」，改成「綁執行緒的 Resource 被別的執行緒碰到（含銷毀）就斷言」；「同一個 World 同一時間只被一條執行緒跑」與時鐘的部分照舊（時鐘的部分隨第 3 條） | J-6；S10 新案 |
| DI-KS 15 | 改寫 | 照統合 9.1 的職責清單 (1)～(13)；另附過渡表：開局（第 2 條）、時鐘與事件的 domain（第 3 條）、區與寫者數量、digest、回滾的宣告（第 4 條）此刻仍在 Kernel，隨各批拿掉；第 (13) 項的原語隨第 4 條加入；第 (2) 項的推送式隨第 3 條加入 | J-9；H-7 的掃描；新案「`StartupCode` 的列舉值等於清單＋過渡表」 |
| DI-KS 18 | 改寫（`immediate-change-via-exclusive-world`） | 一般的 System 執行期間成員不變，結構變更只經同步點；`ExclusiveWorld` 可以當場改成員與增刪 Resource；hook 的 `ExclusiveWorld<>` 與閉包裡不能當場改；**繞過句柄改成員仍由常開的檢查擋下**（第 12 節第 1 件；選退役的話這一句拿掉、守衛的兩案也拿掉） | J-3；J-56「獨占 System 自己繞路改成員」兩案（選 (a)）；S5 新案 |
| DI-KS 19 | 刪一句 | 「規則 Z5 的重查也看外面每一層留著的紀錄…」與守衛「外層 (c) 區的插入碰上裡面的刪除」拿掉；加「`ExclusiveWorld` 的當場套用也不套用這一層還沒到同步點的紀錄」 | J-56 其餘各案；S5 新案 |
| DI-KS 20 | 改名 | `Product::Run` → `WorldExecutor::Run` | J-56（照舊） |
| DI-KS 21 | 加一句 | 閉包裡不能當場改成員（`ClosureChangesMembership`） | J-57；S5 新案 |
| DI-KS 23 | 拿掉 | 「列出來的算寫者、沒列的寫不進去」（確認清單第 4 件）；`Writes<…>` 留作選填標註（DI-KS 1） | — |
| DI-KS 24（新） | 新增 | **`KernelSettings` 下一次 `Run` 生效**：只放開發與調校的設定，System 與外面都可以讀寫，每次 `WorldExecutor::Run` 呼叫開始時讀一次；驗證開關翻轉時重取快照；會改變 World 規則的開關（例：准不准動態插入）在建 World 時的選項，執行中不能改 | J-50、J-54 改過的案；S7 新案 |
| DI-KS 25（新） | 新增 | **登記表唯讀、帶版本**：Kernel 交出的登記資訊唯讀、帶版本號；交出去的參照只在當前版本有效；修改只走 `Registrar`；登記到一半的內容在建好之前看不到 | S2、S3 新案 |
| DI-KS 26（新） | 新增 | **動態插入照 World 的宣告**：World 建立時宣告准不准在 Ready 時再登記或拿掉 System；不准時 `Register` 回傳執行時錯誤、不改 Registry 與 World；准時 `BuildPlans` 成功後用它建的 World 當場接上，指紋跟著重算、不保證兩端一致；失敗的那一輪丟掉；`Register`／`BuildPlans` 只在每個 World 都 Ready、沒有 System 在跑時呼叫 | J-19 改過的案；S2 新案 |

Replacement Criteria：
- RC 3（觸發條件是 D-E8）拿掉。
- 新增 RC 16「**Kernel 又長出限制**：Kernel 的職責清單（DI-KS 15）要多加一項，或判不出某樣東西屬不屬 ECS——不數次數，出現就停下來問（L17）」。
- RC 14 不改字；第 12 節第 2 件選「當場」時，在 RC 14 末加一句「`ExclusiveWorld` 的當場套用是另一條路，不算把清空輪改成別的時機」。
- RC 7 的主詞改寫（Host → 編排層）留到第 2 條（編排層那時才存在）。

---

## 8. 判定（先宣告，結果出來之後不放寬）

| # | 判定 | 怎麼量 |
|---|---|---|
| G-1 | 本機 `ctest -L "cpu|shader"` 全過（macOS arm64，Debug） | ctest 的輸出 |
| G-2 | 第 5 節的每一案照表處置；新案照第 5.3 節的 Red 做法（空實作上先失敗、補實作後通過），並各有專屬突變 | 測試的提交順序與輸出 |
| G-3 | 掃描 0 命中：`git grep -n -P "<式子>" -- engine/kernel`，式子：`class\s+Product\b`、`\bProduct(::|\s*[&*(])`、`Product\.hpp`、`\bRequireExplicitOrder\b`、`\bexplicitOrderOwners\b`、`\bDE(8|10|13|18)\b`、`\bXE[23]\b`、`\bkMainRole\b`、`\bAffinity\b`、`\.runner\b`、`\bUntypedWrite\b`、`\bZoneCDespawn\b`、`\bExclusiveWorldWrite\b`、`\bUndeclaredResourceWrite\b`、`\bUnreadableResource\b`、`\bCheckDespawn\b`、`\bCheckUntypedCommand\b`、`\bCheckExclusive(Write|ResourceWrite|Read)\b`、`"Z5"`、`"S-1[4568]"`、`"F-10"`、`"M3b"`、`\bownerPlugin\b` | 每個式子先在 a08c16f6 用已知例命中一次；終了碼區分 0 命中（1）與失敗（其他）。`Product` 指遊戲產品的註解（例 `Registry.cpp:132`）不在式子裡，照舊 |
| G-4 | 突變：第 6 節「要跑」的全部照 D184 跑，每一個都在預期的失敗下 killed；改寫與新增的突變的預期失敗寫在清單裡 | `tests/common/mutation/run_mutations.py` 的輸出 |
| G-5 | 守衛腳本的 Python 測試全過（`test_kernel_guard.py`、`test_naming_guard.py`、`test_kernel_symbols.py`、`test_store_stamp_guard.py`） | unittest 的輸出 |
| G-6 | CI 的 L1 三平台通過（PR 上） | GitHub Actions |
| G-7 | 第 5.1 節有緩的面的各組，由檢證者（Fable／high）用實際的測試與程式確認 1～5 的記述（§14.4） | 檢證報告 |

---

## 9. 誰做、用什麼、怎麼分段

上段（公開介面與 Design Invariants；navigator「上段」第一條）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 事前宣告的作者 | Opus／high（本卡） | — |
| 事前宣告與守衛、突變清單的敵對式檢查 | Fable／high | 和作者換模型（§4、§14.2）；修訂 0 做過一次、修訂 1 做窄複查 |
| 實作者 | Opus／high | 統合第 8 節 |
| 審查者 | Fable／high | 和實作者換模型 |
| §14.4 的檢證者（緩的面的 1～5 用實際資料確認） | Fable／high | 統合第 8 節；navigator「檢證者確認 4 時不用小模型」 |
| 當場套用（S5 第 1、2 點）與暫存副本換上（S2 規則 1、4）的逐行複核 | Opus／xhigh（要你同意才用） | 時序、stamp、懸空的參照容易錯；ギアボックス「xhigh 局所的にだけ」 |

**兩段核准**（照 K0 的先例，`k0-guard-changes-approved`）：這一次請你核准的是宣告（方向、設計、要動哪些判定）。§14.4 要求「緩的面先做突變測試與別的檢證者確認，再請使用者核准」——這要等實作完才做得到，所以實作完、合併前，帶著檢證報告與突變結果，再請你核准第 5.1 節各組與第 7 節。

**時間重估**（檢查 S14：統合的 10～14 h 是 S5、S6 與 S2 的細節定下之前估的）：64 案的處置、約 80 個突變、7 個當場改的函式、暫存副本與 World 的接上。重估 **20～28 h**。分三段，每段結束時 ctest 通過、提交、回報：
- **段 1 拿掉**（S1、S4 的值的部分、S7、S8、S9、S10、S11）：8～11 h。
- **段 2 登記**（S2、S3）：6～8 h。
- **段 3 當場改**（S5、S6）：6～9 h。
超過 1.5 倍（42 h）或單一問題累計超過 2 h 就停下回報。

---

## 10. Architecture Delta（和統合第 4 節的差分）

- 新增的公開介面：`RegistrationOwner`、`Registry()`、`Registry::Register`／`BuildPlans`／`DiscardRegistrations`、`Registrar::RemoveSystem`、`WorldOptions`（`World` 與 `WorldExecutor` 的建構子）、`World::Options()`、`FrozenRegistry::Version`／`Hooks`／`InitialValues`／`Schedules`／`Sets`／`OrderGraph`、`SystemInfo::effects`、`AccessSet::annotatedWrites`、`ExclusiveWorld` 的七個成員函式、`Commands::SetResource`／`RemoveResource`、`ResourceOptions::threadBound`、四個 `KernelErrorCode`。改掉的：`Commands::Insert`／`Remove`、`Despawn::Entity` 的回傳型別改 `void`。都在統合第 4 節第 3 點列的介面範圍內；沒有新的 Module、沒有新的依賴邊。
- 和統合（與批次表）不同的：
  1. J-56 兩案的去留（第 12 節第 1 件）。
  2. 當場改的 hook 時機（第 12 節第 2 件）。
  3. `we-j3-spawn-immediate` 不退役（第 2.1 節）。
  4. C 指紋與 Plan 匯出標頭在這一批就升版（統合 L10 寫 C 指紋隨權威 Module；但它讀的 System 與 Resource 位元組在本批就變，第 3.11 節）。
  5. 統合 7.2 寫「留」的 J-22 兩案、J-44 有先後的那半、`NestedRunTests` :605 與 :646、`StrictOrderTests` :113、:163、:189 要換觀察方式才守得住（第 5.1、5.2 節）。
  6. S6 是主會話開卡時加進本批的，批次表與統合第 8 節的本批那一列沒有（第 2.1 節）。
  7. 登記的介面用呼叫範圍內的 Registrar（統合 L5 寫「開 Registrar」「建 Plan」兩步，沒有定形狀；第 3.2 節）。

---

## 11. 範圍的限界

- 宣告寫在偵察之後、實作之前；時間是讀程式重估的，沒有量測。
- 別家的系譜：Bevy 讀本地原始碼（6a911653f）的指定行；flecs 讀本地 checkout（09790b5f7）的 `docs/Systems.md` 與 `pipeline.c`；其餘照統合的證據，沒有重讀。
- 當場改成員之後的當場套用、暫存副本換上時 World 的接上，是設計推導，沒有試做；實作時若發現和巢狀的紀錄（DI-KS 19）、存取驗證或既有的參照衝突，照 AGENTS §14 停下回報。
- 第 5、6 節的逐案清單由偵察者（Opus／high）讀程式寫成，本卡讀了主表、敵對式檢查者（Fable／high）抽查。
- 空窗：(1) 執行期攔截拿掉後到網路 Module 之前，判定資料被 untyped 路徑寫錯沒有東西抓（統合第 8 節）；(2) `DynamicInsertRefused` 的警告在編排層之前沒有人記；(3) 別的執行緒上的 World 在跑時呼叫 `Register`／`BuildPlans`，Kernel 抓不到（共用規則頁的一列）；(4) 綁執行緒的記號在執行緒結束後可能被新執行緒重用，這時抓不到。
- `NestedRunTests` :1881 退役後，「裡面的同步點在第 0 步之後失敗、沒有 hook 跑過」的程式路徑沒有觸發方法、留著沒有測試。

---

## 12. 要你定的

1. **J-56「獨占 System 自己繞路改成員」兩案**（`NestedRunTests.cpp:260`、`:273`：獨占 System 經儲存本身拿掉一格，在跑 schedule 之前或之後；現在由 W-3 的斷言擋）。HANDOFF `immediate-change-via-exclusive-world` 的「定下的」寫兩案退役（原話只有「沿用 ExclusiveWorld」，退役是卡片推出來的）。
   - **(a) 保留（建議）**：句柄的當場變更先比對、再更新這一層的基準，繞過句柄的變更照樣斷言。兩案留著、判定不變；DI-KS 18 保留「繞路由常開的檢查擋下」。代價：每次當場變更多一次 O(欄數) 的加總（和現在每次呼叫前後做的一樣）。
   - **(b) 退役**：獨占 System 的呼叫不查 W-3。兩案拿掉，緩的面：獨占 System 經 World API 或儲存本身繞路改成員不再被抓；突變 `ns-epoch-check-skipped-for-exclusive` 一起退役。
   - 卡片的理由：Bevy 的 `&mut World` 由 Rust 的借用規則保證只有一條路，C++ 沒有；W-3 是 GYO 用來補這個差的。(a) 不影響當場改的能力。
2. **`ExclusiveWorld` 當場改成員時，觸發的 hook 記下的變更什麼時候套用**（第 3.5 節第 1 點）。
   - **(a) 當場套用到空（建議）**：在那次呼叫回傳前一輪一輪套用，只套用這次觸發的，不碰這一層的紀錄與 hook 佇列；上限照 F-12。系譜：Bevy 每次插入後 `world.flush()`、flecs `immediate`；GYO 統合第 16 節對「外面觸發 observer」也是這個做法。
   - **(b) 進這張 schedule 的 hook 佇列，照 11-15 的規則在 Run 結束時清**：和同步點一致。代價：回滾的還原之後巢狀跑固定步時，hook 衍生的變更還沒套用（裡面那張不套用外層的佇列，DI-KS 19）。
   - 為什麼問你：11-15 你對同步點定了 (b) 的模型（`hook-queue-flush-at-run-end`），RC 14 記著「每個同步點把連鎖做完（Bevy 的做法）不採」。(a) 只用在當場改這條路，不改同步點。
3. **宣告的核准**：第 5 節各組的方向與第 7 節 Design Invariants 的改寫。照第 9 節的兩段核准：實作完、合併前再帶檢證報告與突變結果請你核准緩的面與 DI 的最終文字。
4. **xhigh 的逐行複核**（第 9 節最後一列）要不要用。

---

## 13. 修訂 0 檢查的處理（`declaration-r0-adversarial-check.md`）

| 項 | 處理 |
|---|---|
| M1 掃描式吃到留下的 `CheckExclusiveWriters` | G-3 改成 `\bCheckExclusive(Write|ResourceWrite|Read)\b` |
| M2 拿掉 J-32b 的 ctest 項目會讓留下的案不跑 | J-32b、J-45 留著，只拿掉 J-28（第 5.5 節） |
| M3 `ExclusiveWorld::Spawn()` 那段漏列 | 第 5.1 節加組 D2 |
| M4 組 A 的「別處守著」不實 | :163、:189 換觀察方式留下（第 5.1 節組 A、5.3） |
| M5 順序圖被拿掉、理由不成立 | 照 L6 交出 `OrderGraph`、`Sets`、`effects`（第 3.3 節） |
| M6 當場套用和 DI-KS 9、11-15 的決定衝突 | DI-KS 9 加例外（第 7 節）；交使用者（第 12 節第 2 件） |
| M7 World 在跑時重建 | 前提與斷言（第 3.2 節規則 3）、共用規則頁一列、DI-KS 26 |
| M8 新登記何時看得到 | 暫存副本、成功才換上、World 當場接上、失敗那一輪的 id 作廢（第 3.2 節規則 1、4；DI-KS 4、25） |
| M9 「不改 World」不是使用者的原話 | 改成卡片的推導並列退掉的選項（第 3.2 節） |
| S1 驗證開關翻轉的誤報、開局與重讀、`KernelSettings` 被拿掉 | 第 3.7 節 |
| S2 當場套用不得套這一層的紀錄 | 第 3.5 節第 1 點、5.3、DI-KS 19 |
| S3 第二層驗證被當場改繞過 | 第 3.5 節第 2 點（先比對） |
| S4 沒有配發器、上限、中斷、計數 | 第 3.5 節的表與第 1 點（`NoAllotter`） |
| S5 DI-KS 1 的措辭 | 「元件與 Resource，事件不在內」（第 3.4 節、DI-KS 1） |
| S6 第 7 節不完整、沒有守衛欄 | 第 7 節重寫、加守衛欄、DI-KS 15 的守衛排進 5.3；:1881 的路徑記在第 11 節 |
| S7 G-3 的 `Product` 吃到遊戲產品的註解、缺式子 | G-3 改窄、補式子 |
| S8 新介面的 Red 做不到 | 第 5.3 節開頭的 Red 做法 |
| S9 S2 沒回答的情形 | 第 3.2 節規則 5～9 |
| S10 綁執行緒：拿掉、蓋寫、銷毀、記號的限界 | 第 3.10 節、第 11 節 |
| S11 S6 是範圍外加的；`Commands` 回傳型別 | 第 2.1 節、第 10 節；回傳改 `void`（第 3.6 節） |
| S12 dev_log、共用規則頁、過時的舊文件、原話截斷 | 第 4 節；第 1 節引全句、第 11 節記空窗 |
| S13 指紋的理由與內容 | 第 3.11 節 |
| S14 檢證時機、時間、退掉的選項 | 第 9 節（兩段核准、20～28 h、三段）；第 3.2 節退掉的選項 |
| S15 可移動的 Registrar 懸空 | 改成呼叫範圍內，不移動（第 3.2 節規則 2） |
| N1、N2 行號與理由 | 已改 |
| N3 `Present()` | 第 3.6 節 |
| N4 計數 | 第 6 節（退役 48、改寫 16） |
| N5 `Executor.cpp` 的理由 | 第 6 節「守衛腳本」 |
| N6 上限 0 | 第 3.7 節 |
| N7 owner 的序號 | `RegistrationOwner{name, seq}`（第 3.2 節） |
| N8 `Plugin::Finish` 裡的版本號 | 0（第 3.2 節規則 7、第 3.3 節） |
