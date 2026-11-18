# 事前宣告：Kernel 減法・結構段（`v8-arch-gyo2-kernel-subtract-structure`）

更新：2026-11-17（修訂 2）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：第一段核准（2026-11-17）；三段實作完成（2026-11-18，至 ac55aa12）；S10 停在 AGENTS §14 的停止條件、等使用者選修法；合併前的第二段核准未做。**

> **核准**：使用者 2026-11-17 在主會話對第 12 節四件回「照建議」（主會話轉達；HANDOFF `v8-arch-gyo2-kernel-subtract-structure-declaration-approved`）：(1) J-56 兩案保留（(a)）；(2) 當場改成員觸發的 hook 記下的變更在呼叫回傳前當場套用到空（(a)）；(3) 核准修訂 2（cd553b2d）的方向與設計——這是第一段核准，第二段照第 9 節在合併前帶檢證、§14.2 的檢查與突變結果再請核准；(4) 當場套用與換上新版本兩段做逐行複核，改用 **Fable／xhigh**（實作者是 Opus，照 navigator 換模型；檔位高於主會話，使用者已同意）。修訂 2 不再送檢查。

經過：修訂 0（41086c8f）送 Fable／high 敵對式檢查，必改 9、建議改 15、小問題 8（報告 `declaration-r0-adversarial-check.md`）。修訂 1 照它改：S2 的介面改成呼叫範圍內的 Registrar 加暫存副本（M7、M8、S15）；當場改的 hook 時機交使用者（M6，第 12 節第 2 件）；登記表照核准的 L6 補上順序圖與 effects（M5）；第 5.1 節補漏列與錯列（M2～M4）；第 7 節補齊並加守衛欄（S6）；時間重估、加檢查點（S14）；其餘建議改與小問題逐條處理（第 13 節對照表）。修訂 1（67423f49）送 Fable／high 窄複查：修訂 0 的必改 9 條全部解決、建議改 12 解決 3 部分；新的必改 2、建議改 8、小問題 14（報告 `declaration-r1-narrow-check.md`）。修訂 2 照它改：S2 改成「表物件位址不變、交換內容；World 下一次被用到時自己接上；Registry 只記兩個計數」（NM1、NS3、NS5）；之後一輪准登記什麼（NS1，新碼 R-E7）；衍生欄位每次重算（NS2）；DI 的守衛測試也進第二段核准、§14.2 的檢查排進每段（NS7）；新增 RC 17、RC 18（NS8）。處理對照在第 13 節末。修訂 2 沒有再送檢查。

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
    // Ok：登記記下來，下一次 BuildPlans 成功才生效。
    // Err（執行時錯誤，Registry 與 World 都不變）：用這份 Registry 建、還活著的 World 裡有一個宣告了不准動態插入。
    template <class Build>   // Build：可以用 Registrar& 呼叫的可呼叫物
    Engine::Base::Result<void, KernelError> Register(RegistrationOwner owner, Build&& build);
    // 第二步。Ok：版本號加一、Plan 與指紋重算；用它建的 World 下一次被用到時接上。沒有登記待建時什麼都不做、版本不變。
    // Err：所有啟動錯誤一次收齊；這一輪丟掉，Registry 與 World 都還是上一個版本（第一次失敗時 Registry 不能用，照舊）。
    Engine::Base::Result<void, StartupReport> BuildPlans();
    // 放棄還沒建 Plan 的那一輪。
    void DiscardRegistrations();
    Engine::Base::Result<void, StartupReport> Finish();   // 照舊（只用在第一次）
};
struct WorldOptions { bool dynamicInsert = true; };      // World 建立時的宣告（L5）；WorldExecutor 的建構子轉交
```

規則：
1. **第一次與之後**：第一次建 Plan 之前沒有 World，登記直接寫進表（同現在；`Plugin::Finish` 讀到的就是這張表，版本號 0）。第一次成功之後的每一輪，第一個 `Register` 把現在的表複製成**暫存副本**，這一輪的登記寫進副本；`BuildPlans` 成功時把副本的**內容換進**原來的表物件（物件位址不變：`FrozenRegistry`、World 拿著的表指標、依賴這份 Registry 的 Extract 來源指標都照樣有效；它們先前交出的 span 與指標作廢，正好是版本號變的那一刻，DI-KS 25）；失敗或 `DiscardRegistrations` 時丟掉副本。所以：之後一輪登記到一半的內容外面看不到；失敗的那一輪發出的 id 作廢，下一輪從上一個版本接著發（DI-KS 4）。代價：之後每一輪複製一次表（登記是少有的事）。
2. **衍生的欄位每次重算**：每次 `BuildPlans` 從登記的輸入重算所有衍生的東西（存取集合、effects 的 id、`SystemInfo`、平手、Plan、順序圖、指紋）——現在有幾處是往後加、沒有清（`Registry.cpp:1087` 的 effects、`Scheduler.cpp:495` 的平手、`:559` 的 Plan），重建前要清掉。守衛：新案「第二輪只加一個無關的 System，`TieBreaks`、每個 System 的 effects、`OrderGraph`、沒動到的 schedule 的 Plan 指紋都不變」。
3. **呼叫範圍內的 Registrar**：Registrar 只在 `build` 回呼裡有效，照舊不能複製、不能移動——J-19 的編譯期守衛（`CoverageTests` :85）照留、沒有緩的面。在 `build` 回呼裡再呼叫 `Register`、`BuildPlans` 或 `DiscardRegistrations` 是斷言；`build` 丟出例外時，這一輪已記下的登記留在暫存副本裡，由呼叫的人決定丟掉或繼續。
4. **前提**：
   - 斷言：用這份 Registry 建的 World 在 `WorldExecutor::Run` 裡（最外層的 Run 開始到它回來，例外離開也算；巢狀的 `RunSchedule` 不另算）時，不能呼叫 `Register`／`BuildPlans`／`DiscardRegistrations`——Run 呼叫到的都算：它的 System、hook、同步點上的初始值函式、它的 System 跑的另一個 World（例：指令的 System 在自己的呼叫裡重建，會讓 `Executor.cpp:177`、`:255` 拿著的 Plan 與 System 懸空）。Registry 為此記第三個計數「正在 Run 的 World 數」（規則 5）。別的 Registry 不受限。（第二段逐行檢查 M1、S1 後由「這條執行緒上正在跑的 System 或 hook 屬於這份 Registry 的 World」改成這樣：只看最內層的呼叫，漏了經別的 World 繞回來與初始值函式。）
   - 呼叫的人的規則（Kernel 看不到：沒有鎖與原子操作，守衛 K3）：同一份 Registry 的 `Register`／`BuildPlans`／`DiscardRegistrations`，和用它建的 World 的建立、銷毀、執行，在同一條執行緒上做，或由呼叫的人排好不重疊。登記進共用規則頁（第 4 節）。
   - HANDOFF `dynamic-insert-per-world` 的「定下的」寫「World 在 Ready 時可再登記或拿掉 System」——那是卡片的整理（使用者回「對的。」），不是使用者的原話。
5. **World 下一次被用到時接上**：World 記著自己接上的版本；它的每個入口（World API、`WorldExecutor::Run`）先比對 Registry 的版本，不同就先接上：新登記的欄、Resource、事件緩衝、驗證的排除表往後加；hook 的存取與佇列、每個 System 的存取**全部重算**（之後一輪不會改到既有的，見規則 6，但照樣重算，不靠推論）；每個 System 的執行狀態照 registrationSeq 重建（留下的保留 `lastRun` 與 `Local`，拿掉的丟掉）。所以建好之後、下一次 Run 之前，World API 就能用新的型別（下一批「建 World 後、第一次 Run 前放 Resource 的預設值」要用到）。Registry 不記 World 的名單，只記三個計數（活著的 World 數、宣告不准的 World 數：World 的建構與解構改這兩個，所以規則 4 的「同一條執行緒或排好」也包含 World 的建立與銷毀；正在 Run 的 World 數：`WorldExecutor::Run` 進入時加、每一條離開的路上減，規則 4 的斷言看它）。中斷的 World 不算在 Run 裡，不觸發規則 4 的斷言；它不會再跑，接不接上看不出差別。宣告不准的 World 中斷之後照樣擋（它還活著，規則 8 的計數只在建構與解構時變）。
6. **之後一輪准登記什麼**（「Plan 的重建先薄」）：

   | 准 | 不准（建 Plan 失敗，新碼 R-E7：這一輪丟掉、World 照跑） |
   |---|---|
   | 新的元件、Resource、事件、具名集合、schedule（開局的除外）、stage、System 的集合、effect；新的 System；`RemoveSystem`；在既有的 schedule 上掛新的 System | 對**上一個版本已有的元件**加 hook 或 `InitialValue`（`InitialValue` 的 Target 或 Request 是已有的元件都算：已有 Request 的實體永遠拿不到 Target；hook 是既有的格沒跑過 OnAdd、之後卻會跑 OnRemove；Bevy 同樣拒絕：`register_component_hooks` 在元件已存在時 panic，`world/mod.rs:345-349`）；在**上一個版本已有的具名集合**加成員（會改既有 System 與 hook 的存取）；開局的 schedule、在開局的 schedule 上掛 System（開局只跑一次、已經跑過）；配發器；`DigestSet`、`RequireExplicitZones`（區那一套，第 4 條拿掉） |

   R-E7 用建 Plan 的錯誤而不是斷言：指令打錯不應該讓遊戲停。R-E7 是 Kernel 職責 (12)「登記」之內的啟動錯誤，不是職責清單多一項；統合第 3.1 節的保留清單因此多一個碼（第 10 節的差分）。
7. **拿掉 System**：`bool Registrar::RemoveSystem(std::string_view name)`，下一次 `BuildPlans` 生效；名字不在目前的表回 false、不記任何東西。拿掉的 System 留一筆墓碑（registrationSeq 是 `data.systems` 的索引，`Registry.cpp:562`、`Scheduler.cpp:328`、`:338`、`Executor.cpp:255-256`；不重發），`FrozenRegistry::Systems()` 照樣列出它、`SystemInfo::removed` 為真；同一個名字之後可以再登記（R-E2 只比還在的）；同一輪先拿掉再登記同名也可以；別的 System 的 before／after 指著被拿掉的，建 Plan 失敗（D-E4）。ECS 元件、Resource、事件、schedule 不能拿掉（Bevy 也不能取消登記元件）。
8. **准不准動態插入**：`World(const Registry&, std::string name, WorldOptions)`、`WorldExecutor(const Registry&, std::string name, WorldOptions)`。只要有一個用這份 Registry 建、還活著的 World 宣告 `dynamicInsert = false`，`Register` 回 `Err`（新碼 `DynamicInsertRefused`），什麼都不改——同一份 Registry 上宣告允許的別的 World 也一起被擋（登記與 Plan 屬於 Registry）。有暫存副本時建 World 是斷言。「日誌報警告」由呼叫的人記（Kernel 的錯誤只在一處記，error-handling DI 2）；登記進共用規則頁。
9. **`Finish()` 與兩步混用**：`Finish()` 只能是第一次（照舊走完 Plugin 清單、照舊的順序：每個 Plugin 一次 `Register` → 解析 → `Plugin::Finish` → 建 Plan；`Finish()` 要用 `BuildPlans` 內部的分段，不只是兩個公開的步驟）；之後可以用 `Register`／`BuildPlans` 再登記，`Plugin::Finish` 不會再被呼叫（下一批搬到編排層時改成 build → 建 Plan → finish）。第一次失敗時 Registry 不能用（照舊），不能再 `Register`。
10. **Extract**：`SetExtractSource` 照舊只能在第一次建 Plan 之前呼叫；D-E14 的「來源已凍結」改成「來源至少建好過一次 Plan」。來源之後再登記：表物件位址不變（規則 1），id 只往後加，讀它的 Registry 下一次建 Plan 讀到新的內容。守衛：新案「來源重建之後、依賴它的再重建」。

系譜：Bevy `Schedule::initialize` 在圖有改動時重建（`schedule.rs:637-650`）、元件可以隨時登記、id 只往後加；flecs 有改動才重建 pipeline（`src/addons/pipeline/pipeline.c:297-311`）；Unity 執行中可以建 System。「每個 World 宣告」四家都沒有，是使用者的設計（L5）。

**退掉的選項與理由**（AGENTS §4；卡片照 L17 判）：
- 介面：(a) 開 Registrar 回傳句柄、可移動（修訂 0）——退掉：移動得出去的 Registrar 會比 Registry 活得久而懸空；(b) **呼叫範圍內的 Registrar** ← 採用：和既有的 `Plugin::Build(Registrar&)` 同形，編排層依登記檔順序對每個 Module 呼叫一次 `Register`，再呼叫一次 `BuildPlans`。系譜：既有的 `Plugin::Build`、Bevy `Plugin::build(&mut App)`。
- 重建失敗：(a) 斷言或中斷（Bevy 建圖失敗是 panic）；(b) 這一輪丟掉、留在上一個版本 ← 採用。理由（卡片的推導，不是使用者的話）：使用者對拒絕的情形說「不退出，但是System無法插入」，指令打錯不應該讓遊戲停；暫存副本讓「丟掉」不用另外還原。
- 何時看得到：(a) 登記當下就寫進現在的表（現況）；(b) 暫存副本、建好才換內容 ← 採用（(a) 會讓 World API 看到一半的型別、讓失敗後的 id 重發）。
- World 怎麼接上：(a) Registry 記 World 的名單、建好時逐一接上（修訂 1）——退掉：World 的建立與銷毀要寫名單，多執行緒時有競爭與懸空，Kernel 又不能加鎖；(b) **每個 World 下一次被用到時自己接上** ← 採用；(c) 每個 World 釘住一個不可變的版本（多版本並存）——退掉：要保留舊版本的表直到最後一個 World 換版，現在沒有需要它的 Feature；它的好處（准與不准的 World 可以共用一份 Registry）列進替換條件（第 7 節 RC 17）。
- 拒絕時不在 Kernel 記 log；`RemoveSystem` 用 bool，不加新的啟動錯誤碼。

### 3.3 S3：唯讀、帶版本號的登記表

`FrozenRegistry`（名字照統合 L6 留著）加：
- `std::uint64_t Version()`：還沒建好過 Plan 是 0（`Plugin::Finish` 裡讀到的就是 0）；第一次 `BuildPlans` 成功是 1，之後每次成功加一，失敗不加。
- `Hooks()`：每個 hook 的名字、觸發、元件、存取、登記處、owner。
- `InitialValues()`：Target、Request、宣告者、登記處。
- `Schedules()`：每張 schedule 的選項與 stage 順序。
- `Sets()`：具名的 System 集合的選項（含集合層級的 before／after）。
- `OrderGraph(schedule)`：Scheduler 解析之後的順序邊（集合與同步點都展開成 System 對 System，含 stage 的先後與照登記順序平手加的邊，每條標來源）。D-E16 讀的就是這張圖（`Scheduler.cpp:503-521`）；回滾 Module 讀它，不在 Kernel 外重寫一次解析規則。
- `SystemInfo::effects`；`SystemInfo::removed`（拿掉的 System 留墓碑，第 3.2 節規則 7）。
- `SystemInfo::access.annotatedWrites`（`ExclusiveWorld<Writes<R…>>` 列的 Resource；S4）。
- World 的宣告由 `World::Options()` 交出（宣告屬於 World，不屬於 Registry）。

約定（DI-KS 25）：`FrozenRegistry` 物件本身一直有效（`World::Types()` 交出的參照可以留著）；它交出的 span、指標（`Systems()`、`PlanOf`、`SystemNamed` 等）只在當前版本有效，`BuildPlans` 成功之後作廢；修改只走 `Registrar`。事件的上限與讀者名單隨游標那一批。修訂 0 寫「順序圖不另交」，檢查指出理由不成立（集合層級的先後不在 mount 裡、解析是 Scheduler 的規則），而且是拿掉使用者核准的 L6 項目；修訂 1 照 L6 交出。

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
| `Spawn<C…>(C… values)` | `Result<EntityId, KernelError>` | id 由配發器發（同 `Spawn<Bundle<C…>>::New`；用完是 `AllotterExhausted`）；**沒有宣告配發器是 Err（新碼 `NoAllotter`）**——D-E3 在啟動時看不到 `ExclusiveWorld` 會生成（現在 `Detail::Allot` 是斷言，`World.cpp:427`）。取代現在一律回 Err 的 `Spawn()`。（實作補充：配發器發的 id 還活著時回 Err `AllottedIdAlive`，只交給呼叫的 System，見第 14.2 節） |
| `SpawnWithId<C…>(EntityId, C…)` | `Result<std::optional<SpawnRejectReason>, KernelError>` | 不能生成時不生成、回傳理由，並照 F-13 記一筆 `RejectedSpawn`（記錄的地方與計數只有一處）。這一層記下、還沒套用的同一個 id 的生成，之後在同步點照 F-13 被丟成 `IdAlive` |
| `Insert<T>(EntityId, T)` | `Result<bool, KernelError>` | 實體不在時不插入、回 false，同時照 F.2 第 5 步計入 `DroppedMissingEntity`（計數只有一處，所以兩邊都有，是刻意的） |
| `Remove<T>(EntityId)` | `Result<bool, KernelError>` | — |
| `Despawn(EntityId)` | `Result<bool, KernelError>` | 先跑每個元件的 OnRemove，再依 ComponentId 升冪拿掉（同同步點第 2 步，`despawn-notify-all-then-destroy`） |
| `SetResource<T>(T)`、`RemoveResource<T>()` | `Result<void, KernelError>`、`Result<bool, KernelError>` | — |

統合留給本批定的三件（統合第 10 節第 12 件）：

1. **hook 什麼時候跑：交使用者（第 12 節第 2 件）**。卡片的建議：**當場**。插入新的格時先填 `InitialValue`、再跑 OnAdd；拿掉時先跑 OnRemove 再拿掉；順序同同步點的各步。這些 hook 記下的變更，在函式回傳之前當場套用到空：一輪一輪套用，**每一輪只套用這次當場變更觸發的 hook 所記的變更**——不套用這一層還沒到同步點的紀錄（DI-KS 19、`nested-apply-per-level`），也不碰這張 schedule 的 hook 佇列；上限是這張 schedule 宣告的清空輪上限（沒宣告就用 `KernelSettings` 的），超過照 F-12 回 `HookRoundLimit`、World 中斷（之前各輪已套用的留著，中斷的 World 不會再跑）。
   要交使用者的原因：使用者 11-15 對同步點選了「(b) 每張跑完時清空。加一個上限。」（`hook-queue-flush-at-run-end`），RC 14 記著「每個同步點把連鎖做完（Bevy 的做法，使用者 2026-11-15 不採）」；當場改這條路要採的正是 Bevy 那一種（只限這條路）。
   系譜：Bevy 每個 `EntityWorldMut::insert` 結束時 `world.flush()`（`world/entity_access/world_mut.rs:1097`）把 hook 記的命令當場套用；flecs `immediate`「Operations passed the iterator's world execute immediately」（`docs/Systems.md:1505`）。同形的先例：統合第 16 節對「外面在 Ready 時觸發 observer」判的「一輪一輪套到空，上限用 hook 清空輪上限」——那是統合照 L17 判的、不是使用者的話，也還沒實作。另一個選項「進這張 schedule 的 hook 佇列，Run 結束時清」的代價：回滾的還原之後緊接著巢狀跑固定步，裡面那張 schedule 不套用外層的 hook 佇列（DI-KS 19），hook 衍生的變更會在重跑之後才出現。
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
- `ResourceOptions::affinity` 改成 `bool threadBound`（卡片照 L17 判名字：Bevy 叫 `NonSend`，GYO 的登記選項是形容詞或名詞的成員，例 `external`、`rollbackSafe`）：值放進去時記下那條執行緒（`World::SetResource`、`ExclusiveWorld::SetResource`、同步點套用的 `Commands::SetResource`）。之後在別的執行緒**碰到就斷言**，碰到包括：`Res`／`ResMut`、`ExclusiveWorld` 的讀寫與增刪、World API 的讀寫、`SetResource` 蓋過既有的值、`RemoveResource`、`World::Summary()`，以及**有值時銷毀 World**；Kernel 自己在 Run 裡取快照與邊界比對時讀它的位元組不算（那是 Run 所在的執行緒做的內部複製，碰錯的話 System 的存取已經先斷言）（Bevy 也查銷毀，`non_send.rs:34-46`）。拿掉值之後忘掉記號，換執行緒再放可以。
- 執行緒的記號用 Kernel 既有的 `thread_local` 變數的位址（每條執行緒不同），不引入 `<thread>`——守衛 K3 禁止 Kernel 用它（卡片照 L17 判）。限界：執行緒結束後，新的執行緒可能拿到同一個位址，這時抓不到（第 11 節）。**實作時這個做法不可靠而停下，2026-11-18 使用者改定為 Base 的執行緒序號，見第 14.1 節。**
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
| `docs/cross-component-rules.md` | 新增三列（AGENTS §1 MUST；owner：Engine 的 Kernel）：(1) `Register` 回 `DynamicInsertRefused` 時呼叫的人記警告；(2) 拿到的 `FrozenRegistry` 參照在版本號變了之後重新讀；(3) 同一份 Registry 的 `Register`／`BuildPlans`／`DiscardRegistrations`，和用它建的 World 的建立、銷毀、執行，在同一條執行緒上做，或由呼叫的人排好不重疊。這頁由圖書管理員維護，同一個變更加列並通知 |
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

### 5.2 沒有緩的面的（同一個性質換觀察方式，或期待值隨定義變）

§14.4：一般測試由實作者改、事後報告；**Design Invariants 的守衛測試不論有沒有緩的面都要你核准**——下表的 J-24（DI-KS 5 的守衛：`CoverageTests` :99、:109、`SchedulerTests` :597）、J-32b 的兩段（DI-KS 2）、指紋的 golden（DI-KS 3）都在第二段核准裡（第 9 節）。

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
| S2 | 建 Plan 之後 `Register` 加一個 System、再 `BuildPlans`：版本號加一、下一次 Run 跑到新 System、舊 System 的 `Changed` 與 `Local` 保留；建好之後、下一次 Run 之前，World API 就能用新登記的元件與 Resource（World 在入口接上）；`Register` 之後、`BuildPlans` 之前外面看不到新型別（World API 與 `FrozenRegistry` 都還是舊的）；`BuildPlans` 失敗：版本號不變、World 照跑舊 Plan、失敗那一輪的 id 作廢、下一輪從上一版接著發；`DiscardRegistrations` 之後同上；沒有登記待建時 `BuildPlans` 什麼都不做；第二輪只加一個無關的 System，`TieBreaks`、既有的 effects、`OrderGraph`、沒動到的 schedule 的 Plan 指紋都不變；`RemoveSystem`：不再跑、留墓碑（`Systems()` 列出、`removed` 為真）、名字不存在回 false、同名可以再登記、被別人的 before／after 指著時建 Plan 失敗（D-E4）；R-E7 的每一種（對舊元件加 hook／`InitialValue`（Target 或 Request 是舊的）、對舊集合加成員、開局的 schedule 與它上面的掛載、配發器、`DigestSet`、`RequireExplicitZones`）建 Plan 失敗、World 照跑；不允許動態插入的 World 在時 `Register` 回 `DynamicInsertRefused`、什麼都不改（同一份 Registry 上允許的 World 也被擋）；中斷的 World 不算在 Run 裡、宣告不准的中斷之後照樣擋；這份 Registry 的 World 在 Run 裡時（它的 System、hook、初始值函式、它跑的別的 World）呼叫 `Register`／`BuildPlans`／`DiscardRegistrations` 是斷言，別的 Registry 不受限；`build` 回呼裡再呼叫 `Register` 是斷言；有暫存副本時建 World 是斷言；`Finish()` 之後可以 `Register`、`Plugin::Finish` 不再被呼叫；Extract 的來源重建之後、依賴它的再重建 |
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

### 5.6 檢證（G-7）之後的更正（`g7-verification-fable-high.md`，Fable／high，讀 3bf480a8）

G-7 的結論：第 5.1 節 C、D2、E、F、G、H 成立；A、B、D、I 成立但要更正下列文字；第 5.2 節各列成立（`:99` 要更正）。第二次修正（ac55aa12）處理了它指出的測試缺陷與指紋缺口。

| 位置 | 更正 |
|---|---|
| 組 A `SchedulerTests` :216 | 「Plan 照登記順序」只在四組裡的兩組斷言，不是四組 |
| 組 A 的緩的面 | 舊的案斷言「這一對在哪一筆資料上衝突」；`TieBreak` 沒有這個欄位，所以**衝突在哪一筆資料上不再有守衛**（只守哪兩個 System 平手） |
| 組 B `ExecutorTests` :296 | 拿掉的是 UntypedWrite 的三段，不是五段；`:316`、`:331` 兩段換了觸發留下（第 5.2 節） |
| 組 B 第 4 欄 | 列的三個守衛存在、也抓得到「沒計數」的突變，但沒有一個是原本 Z5 的那個輸入本身；第二次修正另加了正面案（Host 在第一次 Run 後寫 zone C、zone B 刪除帶 zone C 欄的實體、untyped `Commands` 寫 zone C 欄都照做） |
| 組 D `NestedRunTests` :1758 | 「閉包都被呼叫」是一個共用的旗標，實際證明的是五個寫進去的值 |
| 組 I | 新案用的是句柄的 `Insert`，不是 `Spawn` |
| 第 5.2 節 `CoverageTests` :99 | D-E4 與 D-E5 出自同一個分析步驟，`:99` 殺不了 `rs-fix-b2-stop-after-declared-edges`；殺它的是 `:109`（D-E4＋D-E16），第二次修正把那一句檢查加了標記、改了 `:98-100` 的註解 |
| 測試缺陷（已修） | `SchedulerTests.cpp:292` 等五處把 `const char*` 標籤印成位址，對應的突變永遠對不上預期；ac55aa12 改成印文字 |
| 指紋缺口（已修） | `threadBound`、`annotatedWrites` 寫成常數也沒有測試失敗；ac55aa12 加了指紋的守衛與 Plan 匯出那一行的整行比對 |
| 沒列在第 5 節的案 | `AccessCheckTests.cpp:253`（J-5 加了 Mode 與 CRes 的寫，第二次修正）；`HookFlushTests.cpp:436-441`（「兩次 Run 之間上限 0」照舊，另有專屬突變 `ks-s7-zero-limit-first-run-only`）；舊 `SchedulerTests` :250 的反向登記順序照舊 |

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

**新增**：第 5.3 節每一項至少一個突變，預期失敗寫成能辨識的訊息或案例名（§14.2）；當場改的部分另加「hook 的紀錄沒有當場套用」「句柄的 stamp 沒有更新」「基準直接設成目前的值（吞掉之前的繞路）」「閉包裡沒有拒絕」「hook 裡沒有拒絕」；S2 加「衍生欄位沒有清就重建」「R-E7 沒有檢查」「墓碑被重發」「World 沒有在入口接上」「失敗的那一輪沒有丟掉」「拒絕時仍改了 Registry」「暫存的登記在建好前就看得到」「System 的呼叫裡沒有擋 `Register`」「拿掉的 System 留下執行狀態」；S5 加「當場套用也套了這一層的紀錄」「第二層沒有先比對」；S7 加「巢狀也重讀」「開關翻轉時沒有作廢快照」；S10 加「不比對執行緒」「銷毀時不查」。

**跑哪些**（D184）：新增的全部、改寫的全部、find 落在本批改動範圍的、殺它的測試被改到的；PR 線合併前背景完整跑一次。

**守衛腳本**：`kernel_guard.py:292` 的 K6 豁免以「Executor」開頭的檔名比對。**`Executor.cpp` 不改名**（只改標頭 `Product.hpp` → `WorldExecutor.hpp`；卡片照 L17 判）。比較：改名的代價是改一個守衛的比對式並重驗它（守衛判定的變更要走 §14.4）；不改名的代價是一個檔名不完全等於型別名（N16），但 `Executor` 仍是 `WorldExecutor` 的一部分、現在就是這樣。第 5.4 節不採「寫空字串」是因為那會在指紋裡留下假欄位、改變資料的意思；這裡只是檔名，不影響任何契約。K3 禁止 Kernel 用 `<thread>`／`std::this_thread`：綁執行緒的 Resource 用既有的 `thread_local` 變數（`tlCurrent`）的位址當執行緒的記號，不碰 K3、K4、`kernel_symbols.py`（卡片照 L17 判）。`naming_guard.py`、K1 裡的「Product」指遊戲產品，不改。

---

## 7. Design Invariants 的變更（`target/kernel.zh-Hant.md`；逐條要你核准）

| 條 | 處置 | 改寫後 | 守衛（之後） |
|---|---|---|---|
| DI-KS 1 | 改寫 | `ExclusiveWorld<…>` 的 Contribution 是所有元件與 Resource 的值的讀寫（事件不在內）；`Writes<R…>` 是選填的標註，不改變它的存取、不算寫者，記進登記表的 `annotatedWrites` | J-10～J-12；新案「`Writes<R>` 不是 R 的寫」 |
| DI-KS 2 | 前半留；後半拿掉；粗體標題改成「經 World 的繞路由存取驗證（可關）攔，違規以 Err 回傳」（拿掉「untyped 路徑」） | 留：驗證開著時，經 World 的未宣告存取與 System 邊界的淨變化記為違規、以 Err 回傳；驗證是開發工具。拿掉：untyped `Commands`、`Despawn`、`ExclusiveWorld` 對 zone C／Exclusive／沒列出的單一寫者 Resource 的寫入回 Err | J-5、J-6 的 W-6 那半、J-57 的閉包案；J-32b 留下的那段 |
| DI-KS 3 | 改寫 | 順序只來自 id 與 Plan；Plan 在兩次成功的 `BuildPlans` 之間不變，同樣的登記得到同樣的 Plan | J-1、J-2、J-9、J-20；S2 新案（第二輪加無關 System，沒動到的 Plan 不變） |
| DI-KS 4 | 改寫 | 「凍結後不變」改成「建 Plan 成功的版本發出的 id 在之後的登記與重建中不變，新的往後加；失敗或放棄的那一輪發出的 id 作廢」；其餘照舊 | J-8、J-18、J-19；新案（失敗那一輪的 id 作廢、留下的 id 不變） |
| DI-KS 5 | 改寫（含併入的段落） | 衝突與順序分開；沒有先後的衝突對照登記順序平手、列在 `TieBreaks`，不是錯誤；Kernel 不保證結果對登記順序不敏感。併入段落裡「J-44＝…要嘛全部 D-E8」改成「有先後的那一組 20 種排列結果都不變」 | J-20～J-25、J-44 有先後的那半；搬來的 :113、:163、:189 |
| DI-KS 6 | 只改三處 | 寫者清單拿掉「獨占的 System 在 `ExclusiveWorld<Writes<…>>` 列出來的 Resource」；「沒有函式體內決定型別的寫入路徑能繞過計數（untyped 路徑寫不到 Exclusive 欄，記錄當下以 Err 回傳）」改成「計數只數簽名宣告的寫入；untyped `Commands`（含 `SetResource`／`RemoveResource`）與 `ExclusiveWorld` 的寫入不計」；守衛清單拿掉 J-32b；整條隨第 4 條拿掉 | J-15、J-16（照舊） |
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
| DI-KS 25（新） | 新增 | **登記表唯讀、帶版本**：Kernel 交出的登記資訊唯讀、帶版本號；`FrozenRegistry` 物件一直有效，它交出的 span 與指標只在當前版本有效；修改只走 `Registrar`；第一次建 Plan 之後，登記到一半的內容在建好之前看不到（第一次之前沒有 World，`Plugin::Finish` 讀的就是正在建的表） | S2、S3 新案 |
| DI-KS 26（新） | 新增 | **動態插入照 World 的宣告**：World 建立時宣告准不准在 Ready 時再登記或拿掉 System；不准時 `Register` 回傳執行時錯誤、不改 Registry 與 World；一個不准就擋整份 Registry；准時 `BuildPlans` 成功後用它建的 World 下一次被用到時接上，指紋跟著重算、不保證兩端一致；失敗的那一輪丟掉；之後一輪只准第 3.2 節規則 6 的那些（其餘是 R-E7）；有暫存副本時不能建 World；這份 Registry 的 System 裡不能呼叫 `Register`／`BuildPlans`；同一份 Registry 的登記與 World 的建立、銷毀、執行不重疊（呼叫的人的規則） | J-19 改過的案；S2 新案 |

Replacement Criteria：
- RC 3（觸發條件是 D-E8）拿掉。
- 新增 RC 16「**Kernel 又長出限制**：Kernel 的職責清單（DI-KS 15）要多加一項，或判不出某樣東西屬不屬 ECS——不數次數，出現就停下來問（L17）」。
- RC 14 不改字；第 12 節第 2 件選「當場」時，在 RC 14 末加一句「`ExclusiveWorld` 的當場套用是另一條路，不算把清空輪改成別的時機」。
- 新增 RC 17（草案）「**動態登記的接上不夠**：World 接上新版本時漏了某張表、因此出缺陷兩次以上；或每多一種准登記的東西都要另寫接上的程式；或產品需要准與不准動態插入的 World 共用一份 Registry。→ 停下來比較『每個 World 釘住一個不可變的版本』（第 3.2 節退掉的選項 (c)）」。「兩次以上」沒有根據，是提案的數字。
- 新增 RC 18（草案）「**當場套用要第三個例外**：`ExclusiveWorld` 的當場套用為了某個 Feature，要再破 DI-KS 19 的『每一層的紀錄只在自己的同步點套用』→ 停下來重看當場改的模型」。只在第 12 節第 2 件選 (a) 時加。
- RC 7 的主詞改寫（Host → 編排層）留到第 2 條（編排層那時才存在）。

---

## 8. 判定（先宣告，結果出來之後不放寬）

| # | 判定 | 怎麼量 |
|---|---|---|
| G-1 | 本機 `ctest -L "cpu|shader"` 全過（macOS arm64，Debug） | ctest 的輸出 |
| G-2 | 第 5 節的每一案照表處置；新案照第 5.3 節的 Red 做法（空實作上先失敗、補實作後通過），並各有專屬突變 | 測試的提交順序與輸出；Red 的輸出存主 checkout `build/evidence/gyo2-kernel-subtract-structure/` |
| G-3 | 掃描 0 命中：`git grep -n -P "<式子>" -- engine/kernel`，式子：`class\s+Product\b`、`\bProduct(::|\s*[&*]|\()`、`Product\.hpp`、`\bRequireExplicitOrder\b`、`\bexplicitOrderOwners\b`、`\bDE(8|10|13|18)\b`、`\bXE[23]\b`、`\bkMainRole\b`、`\bAffinity\b`、`\.runner\b`、`\bUntypedWrite\b`、`\bZoneCDespawn\b`、`\bExclusiveWorldWrite\b`、`\bUndeclaredResourceWrite\b`、`\bUnreadableResource\b`、`\bCheckDespawn\b`、`\bCheckUntypedCommand\b`、`\bCheckExclusive(Write|ResourceWrite|Read)\b`、`"Z5"`、`"S-1[4568]"`、`"F-10"`、`"M3b"`、`\bownerPlugin\b`、`\baffinity\b`、`"H-2"`、`"X-3"`、`W-6b` | 每個式子先在 a08c16f6 用已知例命中一次；終了碼區分 0 命中（1）與失敗（其他）。`Product` 指遊戲產品的註解（例 `Registry.cpp:132`）不在式子裡，照舊 |
| G-4 | 突變：第 6 節「要跑」的全部照 D184 跑，每一個都在預期的失敗下 killed；改寫與新增的突變的預期失敗寫在清單裡 | `tests/common/mutation/run_mutations.py` 的輸出 |
| G-5 | 守衛腳本的 Python 測試全過（`test_kernel_guard.py`、`test_naming_guard.py`、`test_kernel_symbols.py`、`test_store_stamp_guard.py`） | unittest 的輸出 |
| G-6 | CI 的 L1 三平台通過（PR 上） | GitHub Actions |
| G-7 | 第 5.1 節有緩的面的各組與第 5.2 節的 DI 守衛各列，由檢證者（Fable／high）用實際的測試與程式確認 1～5 的記述（§14.4） | 檢證報告 |
| G-8 | 新的守衛測試（DI-KS 24～26 與改寫的 DI 的新案）與它們的突變清單，每段一次由和作者不同的模型（Fable／high）做敵對式檢查（§14.2）；報告寫檢查者的模型、找到的處數、加進去之後的結果（killed／總數） | 檢查報告 |

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
| 當場套用（S5 第 1、2 點）與換上新版本（S2 規則 1、5）的逐行複核 | Fable／xhigh（使用者 2026-11-17 同意） | 時序、stamp、懸空的參照容易錯；ギアボックス「xhigh 局所的にだけ」；和實作者（Opus）換模型 |

**兩段核准**（照 K0 的先例，`k0-guard-changes-approved`）：這一次請你核准的是宣告（方向、設計、要動哪些判定）。這一次的根據是偵察者的逐案清單與兩次抽查（敵對式檢查、窄複查），還沒有逐組做 §14.4 的檢證。§14.4 要求「緩的面先做突變測試與別的檢證者確認，再請使用者核准」——突變要等實作完才跑得了，所以實作完、合併前，帶著檢證報告（G-7）、§14.2 的檢查（G-8）與突變結果，再請你核准第 5.1 節各組、第 5.2 節的 DI 守衛各列與第 7 節。第二段被你退回的組，合併前把那一組的改動撤回。

**時間重估**（檢查 S14：統合的 10～14 h 是 S5、S6 與 S2 的細節定下之前估的）：64 案的處置、約 80 個突變、7 個當場改的函式、暫存副本與 World 的接上。重估 **20～28 h**。分三段，每段結束時 ctest 通過、提交、回報：
- **段 1 拿掉**（S1、S4 的值的部分、S7、S8、S9、S10、S11）：8～11 h。
- **段 2 登記**（S2、S3）：6～8 h。
- **段 3 當場改**（S5、S6）：6～9 h。
每段結束時另做 G-8 的 §14.2 檢查（那一段新加的守衛與突變）。
超過 1.5 倍（42 h）或單一問題累計超過 2 h 就停下回報。

---

## 10. Architecture Delta（和統合第 4 節的差分）

- 新增的公開介面：`RegistrationOwner`、`Registry()`、`Registry::Register`／`BuildPlans`／`DiscardRegistrations`、`Registrar::RemoveSystem`、`WorldOptions`（`World` 與 `WorldExecutor` 的建構子）、`World::Options()`、`FrozenRegistry::Version`／`Hooks`／`InitialValues`／`Schedules`／`Sets`／`OrderGraph`、`SystemInfo::effects`、`AccessSet::annotatedWrites`、`ExclusiveWorld` 的七個成員函式、`Commands::SetResource`／`RemoveResource`、`ResourceOptions::threadBound`、四個 `KernelErrorCode`、一個 `StartupCode`（R-E7）。改掉的：`Commands::Insert`／`Remove`、`Despawn::Entity` 的回傳型別改 `void`。`World(const Registry&)`／`WorldExecutor(const Registry&)` 的建構與解構現在改 Registry 裡的兩個計數、`WorldExecutor::Run` 改第三個（`RegistryData` 經指標可改，`Registry` 照舊以 `const&` 傳），所以同一份 Registry 的 World 建立、銷毀與登記要在同一條執行緒或排好（第 3.2 節規則 4）——既有建構子的執行緒契約變了。接上由 World 自己做（World → Registry 的方向照舊），Registry 不認得 World。都在統合第 4 節第 3 點列的介面範圍內；沒有新的 Module、沒有新的依賴邊。
- 和統合（與批次表）不同的：
  1. J-56 兩案的去留（第 12 節第 1 件）。
  2. 當場改的 hook 時機（第 12 節第 2 件）。
  3. `we-j3-spawn-immediate` 不退役（第 2.1 節）。
  4. C 指紋與 Plan 匯出標頭在這一批就升版（統合 L10 寫 C 指紋隨權威 Module；但它讀的 System 與 Resource 位元組在本批就變，第 3.11 節）。
  5. 統合 7.2 寫「留」的 J-22 兩案、J-44 有先後的那半、`NestedRunTests` :605 與 :646、`StrictOrderTests` :113、:163、:189 要換觀察方式才守得住（第 5.1、5.2 節）。
  6. S6 是主會話開卡時加進本批的，批次表與統合第 8 節的本批那一列沒有（第 2.1 節）。
  7. 登記的介面用呼叫範圍內的 Registrar（統合 L5 寫「開 Registrar」「建 Plan」兩步，沒有定形狀；第 3.2 節）。
  8. 新的啟動錯誤碼 R-E7（之後一輪登記了只准第一次的東西）：統合第 3.1 節的保留清單多一個碼；屬職責 (12)「登記」之內，不是職責清單多一項（第 3.2 節規則 6）。

---

## 11. 範圍的限界

- 宣告寫在偵察之後、實作之前；時間是讀程式重估的，沒有量測。
- 別家的系譜：Bevy 讀本地原始碼（6a911653f）的指定行；flecs 讀本地 checkout（09790b5f7）的 `docs/Systems.md` 與 `pipeline.c`；其餘照統合的證據，沒有重讀。
- 當場改成員之後的當場套用、暫存副本換上時 World 的接上，是設計推導，沒有試做；實作時若發現和巢狀的紀錄（DI-KS 19）、存取驗證或既有的參照衝突，照 AGENTS §14 停下回報。
- 第 5、6 節的逐案清單由偵察者（Opus／high）讀程式寫成，本卡讀了主表、敵對式檢查者（Fable／high）抽查。
- 空窗：(1) 執行期攔截拿掉後到網路 Module 之前，判定資料被 untyped 路徑寫錯沒有東西抓（統合第 8 節）；(2) `DynamicInsertRefused` 的警告在編排層之前沒有人記；(3) 別的執行緒上的 World 在跑時呼叫 `Register`／`BuildPlans`，Kernel 抓不到（共用規則頁的一列）；(4) 綁執行緒的記號在執行緒結束後可能被新執行緒重用，這時抓不到（已由第 14.1 節的決定解掉：記號改成 Base 的執行緒序號）。
- 依 L5 的順序「build → 建 Plan → finish → 權威檢查」，`BuildPlans` 成功之後才跑的策略檢查若失敗，Kernel 沒辦法撤回已換上的版本（型別留著，只能 `RemoveSystem`）；由編排層那一批定怎麼處理。
- `NestedRunTests` :1881 退役後，「裡面的同步點在第 0 步之後失敗、沒有 hook 跑過」的程式路徑沒有觸發方法、留著沒有測試。

---

## 12. 要你定的

1. **J-56「獨占 System 自己繞路改成員」兩案**（`NestedRunTests.cpp:260`、`:273`：獨占 System 經儲存本身拿掉一格，在跑 schedule 之前或之後；現在由 W-3 的斷言擋）。HANDOFF `immediate-change-via-exclusive-world` 的「定下的」寫兩案退役（原話只有「沿用 ExclusiveWorld」，退役是卡片推出來的）。
   - **(a) 保留（建議）**：句柄的當場變更先比對、再更新這一層的基準，繞過句柄的變更照樣斷言。兩案留著、判定不變；DI-KS 18 保留「繞路由常開的檢查擋下」。代價：每次當場變更多一次 O(欄數) 的加總（和現在每次呼叫前後做的一樣）。
   - **(b) 退役**：獨占 System 的呼叫不查 W-3。兩案拿掉，緩的面：獨占 System 經 World API 或儲存本身繞路改成員不再被抓；突變 `ns-epoch-check-skipped-for-exclusive` 一起退役。
   - 卡片的理由：Bevy 的 `&mut World` 由 Rust 的借用規則保證只有一條路，C++ 沒有；W-3 是 GYO 用來補這個差的。(a) 不影響當場改的能力。存取驗證打開時另有代價（兩個選項都有）：每次當場改先比對、再重取第二層的快照，快照是整份的位元組複製，所以一次放回 N 個實體的成本是 N 倍的 World 大小（開發版才有）。
2. **`ExclusiveWorld` 當場改成員時，觸發的 hook 記下的變更什麼時候套用**（第 3.5 節第 1 點）。
   - **(a) 當場套用到空（建議）**：在那次呼叫回傳前一輪一輪套用，只套用這次觸發的，不碰這一層的紀錄與 hook 佇列；上限照 F-12。系譜：Bevy 每次插入後 `world.flush()`、flecs `immediate`；統合第 16 節對「外面觸發 observer」也判了這個做法（統合照 L17 判、還沒實作）。
   - **(b) 進這張 schedule 的 hook 佇列，照 11-15 的規則在 Run 結束時清**：和同步點一致。代價：回滾的還原之後巢狀跑固定步時，hook 衍生的變更還沒套用（裡面那張不套用外層的佇列，DI-KS 19）。
   - 為什麼問你：11-15 你對同步點選了「(b) 每張跑完時清空。加一個上限。」（`hook-queue-flush-at-run-end`），RC 14 記著「每個同步點把連鎖做完（Bevy 的做法）不採」。(a) 只用在當場改這條路，不改同步點。
3. **宣告的核准**：第 5 節各組的方向與第 7 節 Design Invariants 的改寫。這一次的根據是偵察的逐案清單與兩次抽查，還沒有逐組檢證；照第 9 節的兩段核准，實作完、合併前再帶檢證報告、§14.2 的檢查與突變結果，請你核准緩的面、DI 的守衛與 DI 的最終文字。
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

### 修訂 1 窄複查的處理（`declaration-r1-narrow-check.md`）

| 項 | 處理 |
|---|---|
| NM1 World 的建立與銷毀寫 Registry | Registry 不記 World 名單、只記兩個計數；呼叫的人的規則包含 World 的建立與銷毀；第 10 節列出建構子的執行緒契約變了（第 3.2 節規則 4、5） |
| NM2 「World 在 Ready 時」不是原話 | 第 3.2 節規則 4 改成「HANDOFF『定下的』的整理」 |
| NS1 之後一輪准登記什麼 | 第 3.2 節規則 6（新碼 R-E7）；開局改成 R-E7，不是斷言 |
| NS2 衍生欄位重複累加、空號 | 第 3.2 節規則 2（每次重算）、規則 7（墓碑、`removed`）；沒有待建時不做事 |
| NS3 換表後誰還有效、Extract 來源指標 | 第 3.2 節規則 1（物件位址不變、換內容）、規則 10；第 3.3 節（`FrozenRegistry` 物件一直有效） |
| NS4 `Plugin::Finish` 要看到正在建的表 | 第一次直接寫表、不用副本（第 3.2 節規則 1、9；DI-KS 25） |
| NS5 中斷的 World 擋登記 | 不再逐一檢查 World 的狀態；中斷的 World 不擋（規則 5） |
| NS6 DI-KS 6 的假句子 | 第 7 節 DI-KS 6 改三處 |
| NS7 DI 守衛的核准、§14.2 的檢查 | 第 5.2 節開頭、第 9 節、G-7、G-8、第 12 節第 3 件 |
| NS8 新機制沒有替換條件 | RC 17、RC 18（第 7 節） |
| S6、S7、S14 的部分 | DI-KS 6、G-3 補式子、第 12 節第 3 件寫明第一次核准的根據 |
| NN1、NN2 | G-3 的式子 |
| NN3 | 斷言只限這份 Registry 的 World（規則 4） |
| NN4 | `build` 回呼裡的重入是斷言、例外時留在副本（規則 3） |
| NN5 | 第一次失敗不能再 `Register`（規則 9） |
| NN6、NN7 | 第 3.5 節、第 12 節第 2 件：標明統合第 16 節是卡片判的；引用使用者的原話 |
| NN8 | 第 12 節第 1 件補驗證打開時的成本 |
| NN9 | 第 7 節 DI-KS 2 的標題、DI-KS 3 的守衛、DI-KS 26 的內容 |
| NN10 | 第 10 節：接上由 World 做，Registry 不認得 World |
| NN11 | 本節 |
| NN12 | 第 3.10 節：Kernel 內部的快照不算碰到 |
| NN13 | 第 11 節 |
| NN14 | G-2 的證據位置、第 9 節的撤回 |

---

## 14. 實作中的偏差與補充（卡片照 L17 判；第二段核准時看）

三段的實作者都是 Opus／high；換表與接上（段 2）、當場套用（段 3）各由 Fable／xhigh 逐行複核；G-7（§14.4 檢證）與段 1 的 G-8（§14.2）由 Fable／high 做。報告都在主 checkout `build/evidence/gyo2-kernel-subtract-structure/`。下面是宣告沒寫、實作時補上或改掉的；都在已核准的方向內，但沒有逐條給你看過。

### 14.1 停下的一件（2026-11-18 使用者已定，見文末「決定」）

**S10 綁執行緒的 Resource 的執行緒記號**：第 3.10 節「用 `thread_local` 的位址當記號」不可靠——macOS 的 `thread_local` 用到才配置，第一次碰 Kernel 的執行緒可能拿到剛結束的別條執行緒的同一塊記憶體。探針 `kernel.J-6_thread_bound_destroy` 壓力跑 300 次漏 20 次（`segment1/s10-probe-stress.log`）；G-8 另跑一次漏 9 次，主執行緒先讀過記號時漏 0 次。選項：(a) 放寬 K3、只准 `std::this_thread::get_id()`（建議；Bevy 的 `NonSend` 這樣取）；(b) 由 `engine/base` 給執行緒 id；(c) 保留位址記號、寫明會漏抓、拿掉銷毀的檢查與探針。修正沒有提交；探針留著，所以 ctest 會偶發失敗。

**決定（使用者，side chat 2026-11-18 轉交）**：選 (b)，並改語義。原話：「修改語義，不是做執行緒序號，而是做一個基礎發號器，只不過 thread 用來做一個系統中不會重複執行緒序號。」「這個是基礎發號器，可以由多個系統使用，就像 Sha256 本身就是算 hash 而已，也不會說這個就是 pvp 專用的。」對「`CurrentThreadSerial()` 也放 Base」答「ok」。理由：發號器是不帶領域的通用機制，和 `Sha256` 同類，合乎 Base「只靠標準庫、和領域無關」的定位（Base 已用 atomic／`thread_local`）；號碼跨時間唯一，解決 macOS 位址重用；Kernel 不碰 `<thread>`，K3 不變。

**做法**：
- `engine/base/include/engine/base/IdIssuer.hpp`（header-only）：`IdIssuer::Next()`（`atomic` 的 `fetch_add`，從 1 起、不給 0、同一個發號器不重複不回收；用完 2^64 個是斷言）；`CurrentThreadSerial()`（執行緒第一次要時從 `Detail::threadSerials` 拿一個號存在 `thread_local`，之後固定）。名字照轉交的形狀（`IdIssuer`、`CurrentThreadSerial`）。
- Kernel：`ResourceBase::thread` 改成 `std::uint64_t`（0＝沒有值）；拿掉 `Detail::CurrentThreadToken()`；`World::CheckThread`／`BindThread` 用 `Engine::Base::CurrentThreadSerial()`。
- 測試：`tests/common/base/IdIssuerTests.cpp`（5 案，含「一條執行緒結束後才開始的執行緒拿不到它的號」200 輪）；`gyo_base_tests` 連 `Threads::Threads`。
- 守衛：`kernel_symbols.py` 的 FOREIGN 加 Base 的兩個名字（`Engine::Base::Detail::threadSerials`、`Engine::Base::Detail::threadSerial`），自測試跟著改成「四個 FOREIGN」並加三個長得像的名字。**這是 DI 守衛的判定變更、有緩的面，要你核准**（第 14.5 節）。
- 結果：探針壓力跑 300 次、斷言 300 次、漏 0 次（修正前漏 20 次；`s10-fix/probe-stress-300.log`）；ctest `cpu|shader` 98/98；突變：S10 相關改寫 6、新增 6（Base 5、記號守衛 1），連同記號守衛與 Base 的全部 20 個殺掉（`s10-fix/s10-mutants.log`、`symbols-mutants.log`）；`bs-id-issuer-not-atomic` 是機率性的（8 執行緒×10000），另跑兩次都殺掉。
- DI-KS 14 的條文（**使用者 2026-11-18「照建議」核准，已改**；另加一句「Kernel 只用 `CurrentThreadSerial()`」）：「執行緒的記號是位址、會漏抓（限界）」改成「Base 的執行緒序號（同一次執行裡不重複、不回收、不為 0；0 表示沒有值），結束的執行緒的號碼不會落到新的執行緒手上；記號不是 World 的資料，不進值的位元組、摘要、快照、指紋」，守衛清單加 `gyo_base_tests` 的 `IdIssuer`／`CurrentThreadSerial` 各案。限界拿掉、只收緊。
- 2026-11-18 曾轉交「整個 Base 改成有 `.cpp` 的函式庫、狀態改成 `.cpp` 的 static」，實作到一半時使用者撤回（side chat：「我改想法了，只處理IdIssuer，base裡面只有hpp。」「不不不，IdIssuer也不用cpp。」）。已還原，Base 與 `IdIssuer` 都只有標頭；上面 FOREIGN 加兩個名字的做法照舊、等核准。撤回前的改動另存在會話暫存區，沒有進 repo。
- 敵對式檢查（Fable 5.2／high，`s10-fix/adversarial-check-fable-high.md`）：候選 28 個、殺 14；建議補的 11 處照做——Base 第 5 案每輪查不為 0；J-6 加三案（反方向與「綁的執行緒結束後、新執行緒去碰」、沒標 `threadBound` 的 Resource 任何執行緒可用、兩條執行緒放進同樣的值摘要相同）；J-56 新案加「Ping 真的寫進去」的正例；記號守衛自測試加 5 個長得像的名字與兩種格式的正例；過時的註解改掉。取進清單的候選 21 個（`bs-*`、`ks-s10-*`、`ns-rebase-no-retake-*`）。沒補的：拿掉發完 2^64 的斷言（測不到，標頭註解寫明）、「只有例外時才重取快照」（讀程式判斷為等價，未證明）。
- **已定（使用者 2026-11-18「照建議」：不准）**：Kernel 原始碼能不能直接用 `Engine::Base::IdIssuer`（一個 atomic 計數器）。K3 只認 `std::atomic`／`<atomic>`，現在經 `IdIssuer` 可以繞過（`adv-k3-kernel-uses-id-issuer` 活著）。做法：`kernel_guard.py` 加 K8「engine/kernel 裡出現 `IdIssuer` 或 Base 的 `threadSerials`／`threadSerial` 是 finding，只准 `CurrentThreadSerial`」（理由，主會話給的：K3 不讓 Kernel 自己持有跨執行緒的狀態，S10 只要「現在是哪條執行緒」）。

### 14.2 設計上的補充

| 項 | 做法 | 理由 |
|---|---|---|
| `Register`／`BuildPlans`／`DiscardRegistrations` 的前提斷言 | 改成「這份 Registry 有 World 正在跑（計數不是 0）就斷言」，取代「這條執行緒上正在跑的 System 屬於這份 Registry 的 World」 | 段 2 的逐行複核找到原本的檢查只看最內層呼叫：World A 的 System 跑另一份 Registry 的 World B、B 的 System 回頭重建 A 的 Registry，正在跑的 Plan 被釋放（ASan 證實）；`InitialValue` 的函式在同步點裡重建也一樣。計數同時抓到兩者。它沒有收窄已核准的能力：規則 4 本來就規定同一份 Registry 的登記和它的 World 的執行不重疊（主會話問過，見對話；計數不是原子的，跨執行緒只是盡力抓到違規）。要支援「多個 World 在不同執行緒、共用一份 Registry、跑著時登記」得換成 RC 17 的「每個 World 釘住一個版本」 |
| 中斷的 World 與拒絕 | 宣告不准動態插入、已中斷的 World 照樣拒絕（它還活著）；「中斷的 World 不擋登記」只指它不觸發「正在跑」的前提。規則 5 與 DI-KS 26 的措辭照此改 | 段 2 複核 S4 |
| R-E7 對 `InitialValue` | Target 或 Request 任一是舊版本的就是 R-E7；R-E7 的訊息開頭寫是哪一種 | 段 2 複核 S2、S6 |
| 當場改之前發現宣告外的改動 | 回 `BoundaryChange`、不做變更、World 中斷（比照 `RunNestedFrom` 的拒絕） | 段 3 實作者的判斷 |
| 當場套用時 hook 回 Err | World 中斷（同同步點） | 同上 |
| 已中斷的 World 上當場改 | 不改、回傳留下的錯誤（同 `RunSchedule`） | 同上 |
| 例外離開當場改（hook 或 `InitialValue` 的函式丟出、被獨占 System 接住） | World 立刻中斷，留一個錯誤（新碼 `ImmediateChangeAborted`）給最外層的 Run 回報一次；句子加進 DI-KS 18 | 段 3 複核的必改：原本會帶著改到一半的狀態繼續跑、hook 已記的變更靜靜丟掉。比照 K0 已核准的 X-16（DI-KS 17） |
| 當場的 `Spawn<C…>` 拿到配發器給的、仍然活著的 id | 回傳型別維持 `Result<EntityId, KernelError>`：不生成、回錯誤碼 `AllottedIdAlive`（只交給呼叫的 System，不記進這次呼叫的錯誤、Run 不以它結束、World 不中斷），照 F-13 記一筆 `RejectedSpawn` | 段 3 複核 S1：原本回傳那個 id，呼叫的人會拿著別的實體的 id。實作時先改成 `Result<std::optional<EntityId>, …>`、回傳空；**使用者 2026-11-18 定「參考Go的成功和錯誤回傳，不要回空」**（主會話轉交），改回錯誤碼。錯誤碼名字、不中斷、`RejectedSpawn` 照舊記，卡片照 L17 判（比照同步點丟掉的生成：F-13，不是 Run 的錯誤） |
| 當場改的 `Insert` | 實體活著就回 true（蓋寫也算） | 段 3 實作者的判斷 |
| 當場改觸發的 hook 出錯時的訊息 | 寫「step N（當場改）」，不寫「sync point step N」；`SystemContext` 加一個旗標 | 段 3 複核 S3 |
| 第一輪的 `DiscardRegistrations` | 表重設成只剩 Kernel 自己的登記；Plugin 清單與 Extract 來源留著 | 段 2 實作者 |
| 拿掉的 System 的墓碑 | 不算進 Registry 與 C 指紋（只數還在的）；`SystemNamed` 先找還在的、沒有再找最新的墓碑 | 段 2 實作者；golden 不用為墓碑重產 |
| 指紋裡的 owner | 只雜湊名字，不含序號 | 段 2 實作者 |
| `OrderGraph` | 經所有非 System 的節點展開成 System 對 System、不重複的（first, second, 來源）；會跨 stage、每張 schedule 最多 O(n²)、同一對可以有幾種來源；「同步點」來源指經過同步點的宣告邊 | 段 2 實作者 |
| `SystemInfo::effects` | 放 `EffectId`，不放名字 | 段 2 實作者 |
| 接上新版本 | World 在 Running／Applying 時接上是斷言；接上後斷言四張表（事件緩衝、驗證的排除表、hook 的存取、hook 佇列）的大小 | 段 2 實作者、段 2 複核 S3 |
| `StartupCode` 沒有文字 | 變成建置錯誤（`ToString(StartupCode)` 的 switch 開 `-Wswitch`／C4062）；對應的突變改成預期建置失敗 | 第二次修正（G-8 指出原本抓不到） |
| `Commands::Insert`／`Remove`、`Despawn::Entity` 改回傳 `void` | 呼叫處 25 處跟著改 | 段 1（第 3.6 節已宣告） |
| D-E14 的訊息 | 改成「還沒建好過 Plan」 | 段 2 |

### 14.3 測試與突變的補充

- `SpawnRejectTests` :993 第 2 段：要讓 finish 通過，生成的元件改成 zone C 的 Shared（原本是 Exclusive、兩個生成者會碰到還在的 D-E2）。判定照第 5.1 節組 A。
- `CoverageTests` :109 的第二個錯誤用 D-E4（第 5.2 節只寫了 :99）。
- 綁執行緒的 Resource 在別的執行緒銷毀的檢查：解構子不能把例外丟給 doctest，另做探針執行檔 `gyo_kernel_thread_bound_probe`、ctest `kernel.J-6_thread_bound_destroy`（宣告沒列）。
- 換觀察方式留下或搬家的案（J-23 三案、`NestedRunTests` :605／:646、J-22 的「沒有平手」）改前改後都通過，沒有 Red；檢出力由各自的突變證明。
- 新案：`RegistrationRoundTests.cpp`（段 2）、`ImmediateChangeTests.cpp`（段 3）、兩次修正補的守衛；`kernel.zh-Hant.md` 的 DI-KS 3、14、15、24 的守衛清單加了新案的名字（只是守衛清單；條文本身只有 DI-KS 18 多一句）。
- 突變清單 624 → 759：段 1 退役 48、改寫 26、新增 22；段 2 新增 45、重新找錨點 8；第一次修正退役 1、改寫 12、新增 18；段 3 新增 36；第二次修正新增 63、改寫 13。等價而沒加的：`ks-s2-effects-not-cleared`（排序去重把重複吃掉）；抓不到的：段 3 複核 N3、N6。`ns-c-version-not-bumped` 和 `hf-c-version-kept` 重複，沒有退役（留給整理）。
- 跳過的小問題（段 2 複核）：N1（`~Registry` 斷言活著的 World 數：用丟例外的斷言處理器測不了、拿掉計數又要改 DI 的字）、N2（空的 `Register` 算不算一輪，是定義問題）、N3（等價突變）、N5（段 2 之前就有的死欄位）、N7、N8（行為或設計的改變，見 `segment2-line-review-fable-xhigh.md`）。

### 14.4 檢查的結果

| 檢查 | 結果 |
|---|---|
| 段 2 逐行複核（Fable／xhigh，3f721dda） | 必改 1、建議改 6、小問題 9 → 第一次修正（e172fa86～3bf480a8）處理 |
| 段 3 逐行複核（Fable／xhigh，98e1bc38） | 必改 1、建議改 3、小問題 8；§14.2：找到 19 處、提 16 個突變 → 第二次修正（ac55aa12）處理 |
| G-7 §14.4 檢證（Fable／high，3bf480a8） | 沒有不成立的組；6 處文字更正（第 5.6 節）、1 個測試缺陷、1 個指紋缺口 → 已修 |
| G-8 段 1 的 §14.2（Fable／high，3bf480a8） | 檢查者 Fable 5.2／high；找到 45 處；加進去時殺掉 22／45、18 處當時抓不到、5 處錯的失敗 → 第二次修正補了 18 處的測試與 5 處的訊息；第二次修正跑的 180 個突變全部在預期的失敗下被殺掉 |

### 14.5 S10 修正的守衛變更（AGENTS §14.4；DI 守衛；使用者 2026-11-18「照建議」核准）

`tests/common/kernel/kernel_symbols.py`（kernel 函式庫的可寫資料記號；registry-schedule 宣告判定 9、spec J-9）與它的自測試 `test_kernel_symbols.py`：

1. **守的性質**：kernel 自己沒有可寫的全域狀態，只准 TypeKey 的錨與 `tlCurrent`；別的模組的 inline 變數因為 include 而出現在 kernel 的物件檔裡時，各自列名、單獨一行回報，不靜靜放過。
2. **緩的面**：以前出現 `Engine::Base::Detail::threadSerials`（發號器）或 `Engine::Base::Detail::threadSerial`（執行緒的號碼）就失敗，現在當成 FOREIGN 通過。只有這兩個完整的名字；`GYO::Kernel` 裡同名的變數、名字多一截的、`Engine::Base` 外層同名的都仍然失敗。
3. **緊的面**：自測試多八個長得像的名字（`GYO::Kernel::Detail::threadSerial`／`threadSerials`、`Engine::Base::threadSerial`／`threadSerials`、`Engine::Base::Detail::threadSerialsCopy`、`Engine::Base::Detail::threadSerialX`、`Engine::Base::Detail::Inner::threadSerial`、`GYO::Base::Detail::threadSerial`）要被抓到；SYSV 與 Mach-O 兩種格式各有這兩個名字被判成 foreign 的正例（Windows 沒有真函式庫那一跑，靠它）；FOREIGN 清單固定成這四個。
4. **緩的面守的性質由誰繼續守**：這兩個變數是 Base 的（`IdIssuer.hpp`），由 `gyo_base_tests` 的 `IdIssuer`／`CurrentThreadSerial` 各案與突變 `bs-*` 守行為；kernel 自己另開一個同名的會被突變 `ks-s10-kernel-own-thread-serial` 證明仍然擋下；`kernel_guard.py` K4 仍擋 kernel 原始碼裡的 `thread_local`／`static`。
5. **根據與證據**：使用者 2026-11-18 選 (b)「Base 提供、Kernel 用」，Base 是 header-only，Kernel 呼叫 `CurrentThreadSerial()` 必然把這兩個 inline 變數帶進 kernel 的物件檔（和既有的 assertion handler 同一種情形）。舊版守衛對現在的函式庫回報這兩個「不允許」、新版只把它們列為 foreign（`s10-fix/symbols-old-vs-new.log`）；記號守衛與 Base 相關的突變 20 個全部殺掉（`s10-fix/symbols-mutants.log`）。

### 14.6 完整突變跑出的補充（3c333d4b、aa6e65c0）

- 在 ac55aa12 完整跑一次：758 個殺 747；沒殺的 11 個在基準 a08c16f6 重跑分類。基準就沒殺的 3 個（`ks-real-product-name`、`rs-sample-capture-accepted`、`rs-sample-return-accepted`）不是這批造成，沒動。這批改名讓 6 個編不過，改 find／replace 後殺掉。
- 這批設計讓 2 個活下來，使用者（同一次 side chat）定「都補測試」：`hf-product-limit-static` 補「另一個 World 在這個 World 的 Run 裡面跑，上限仍是各自的」（跨 World 巢狀 Run 可行）；`ns-rebase-no-retake` 補「內層排程寫事件後丟出、外層接住，事件的寫入不算到外層頭上」。兩案是新增（只收緊），兩個突變的 cases／expect 指向新案，在預期的失敗下殺掉（`survivor-tests-2-mutants.log`）。

### 14.7 S10 三件的決定（使用者 2026-11-18，主會話轉交）

主會話列出三件與建議，使用者答「照建議」：(1) 核准 `kernel_symbols.py` 的 FOREIGN 加 `Engine::Base::Detail::threadSerials`、`threadSerial`（第 14.5 節；Base 維持只有標頭，撤回的那則照舊有效）；(2) 核准 DI-KS 14 改寫（已改 `target/kernel.zh-Hant.md`）；(3) Kernel 不准直接用 `IdIssuer`，`kernel_guard.py` 加 K8。比過的選項：(1) 核准／不核准；(3) 不准、加守衛／准、寫進 K3 的說明。理由：使用者沒有另外說；(3) 的理由是主會話的建議。

**K8 的做法**：`kernel_guard.py` 的 K8：engine/kernel 裡出現識別字 `IdIssuer`、`threadSerials`、`threadSerial` 是 finding（註解與字串抹掉之後讀；Executor 的檔也讀；CMakeLists.txt 在 `#` 註解之外也讀；include 那一行不讀）。限界寫進說明：`##` 貼字、接行、asm 標籤與 `__atomic_*` 內建；K2～K8 共用的抹除函式讀錯有前綴的字元字面量與 raw string（J-9 既有的缺口）。

**K8 的 §14.2 檢查**（Fable 5.2／high，`k8/adversarial-check-fable-high.md`）：找到 19 處（繞路 7、誤報 4、守衛改壞而自測試抓不到 8）。照建議：自測試補 8 案（含一個斷言寫錯的修正：`"threadSerial"` 是 `"K8 threadSerials"` 的子字串）、CMakeLists 也讀、include 行不讀、說明寫限界。突變：K8 的 4＋取進的 9＋補的 3＝16 個，連同 kernel_guard 既有的突變共 71 個全部殺掉（`k8/mutants-after-check.log`）。沒修、列成後續：抹除函式的既有缺口（動到 K2～K8 全部的判定，屬 DI 守衛變更）；`engine/entity_store` 持有 `IdIssuer` 時兩個守衛都放行（別的 owner 的守衛 W3，要使用者或架構師定）；K3 不認 `__atomic_*`／`__sync_*`／`_Interlocked*`／`_Atomic`。

---

## 15. 第二段核准的材料（合併前；照第 9 節）

這一次請你核准三樣：第 5.1 節九組的緩的面、第 5.2 節裡 Design Invariants 的守衛各列、第 7 節的最終文字（現在 `target/kernel.zh-Hant.md` 的樣子，含第 14 節的偏差）。已經另外定過、不再問的：第 12 節四件（2026-11-17）、S10 三件（第 14.7 節）、兩個活下來的突變補測試（第 14.6 節）、當場 `Spawn<C…>` 遇到活著的 id 回錯誤碼（2026-11-18，15.5）。被退回的組，合併前把那一組的改動撤回。

### 15.1 先宣告的判定

| # | 狀態 |
|---|---|
| G-1 ctest | 本機（macOS arm64）`cpu|shader` 98/98 |
| G-2 照表處置與 Red | 各段的 Red／Green 在證據的 `segment1/`～`segment3/`、`segment2-fix/`、`fix2/` |
| G-3 掃描 0 命中 | 27 個式子 0 命中（每個先在 a08c16f6 命中過；最後一次在 K8 之後，`k8/g3-scan.log`） |
| G-4 突變 | ac55aa12 完整跑一次 758 個殺 747；沒殺的 11 個在基準上分類：3 個基準就沒殺（不是這批造成，留作後續）、6 個改名讓它編不過（已修、殺掉）、2 個這批的設計讓它活下來（已補測試、殺掉）。之後的 S10、K8 相關突變全部殺掉。清單 624 → 805 |
| G-5 守衛腳本 | 全過（144 項） |
| G-6 L1 三平台 | 等 PR |
| G-7 §14.4 檢證 | Fable／high：沒有不成立的組，6 處文字更正（第 5.6 節）、1 個測試缺陷與 1 個指紋缺口已修 |
| G-8 §14.2 檢查 | 段 1（Fable／high，找到 45 處，已補）、段 2 與段 3 的逐行複核（Fable／xhigh，各一個必改，已修）、S10（Fable 5.2／high，28 個候選，建議的 11 處已補）、K8（Fable 5.2／high，19 處，建議的已補；第 14.7 節） |

### 15.2 第 5.1 節各組（緩的面：以前會失敗、現在通過的）

| 組 | 用遊戲講 | 緩的面 | G-7 |
|---|---|---|---|
| A 歧義 | 兩個 System 都寫主角血量、沒寫先後：不再報錯，照登記順序排、列進 `TieBreaks` | 沒有先後的衝突對都通過；**衝突在哪一筆資料上不再有守衛**（只守哪兩個 System 平手） | 成立（文字更正 2 處） |
| B 執行期攔截 | untyped `Commands` 寫 zone C、zone B 刪掉帶 zone C 欄的實體：照做，同一窗的 zone C 插入丟掉並計數 | 這些寫入與刪除都通過；網路 Module 之前沒有東西抓 | 成立（更正 2 處；另加正面案） |
| C 外面寫 zone C | 第一次 Run 之後 Host 改 zone C 的狀態：照做 | W-6b 的斷言拿掉 | 成立 |
| D `ExclusiveWorld` 寫入 | 讀檔與回滾的獨占 System 寫任何元件與 Resource | 寫入都成功、驗證不報 | 成立（更正 1 處） |
| D2 生成 | 獨占 System 可以當場生成實體 | 以前一律 Err | 成立 |
| E `Writes<…>` | 列出的 Resource 不算寫者 | Z2、D-E18、S-18 都通過啟動 | 成立 |
| F `KernelSettings` | 除錯指令改設定，下一次 Run 生效 | 宣告、讀、寫都准 | 成立 |
| G 執行緒啟動檢查 | 不在啟動時擋，改成碰到時斷言 | 啟動時不再擋 | 成立 |
| H 再登記 | 准動態插入的 World 在 Ready 時可以再登記或拿掉 System | 以前 finish 之後拿不到 Registrar | 成立 |
| I 繞路改成員 | 選 (a) 保留：繞過句柄改成員照樣斷言 | 無 | 成立（更正 1 處） |

### 15.3 第 5.2 節裡 Design Invariants 的守衛

| 列 | 守的 DI | 改什麼 | G-7 |
|---|---|---|---|
| `CoverageTests` :99、:109、`SchedulerTests` :597（J-24） | DI-KS 5 | 第二個錯誤換成留下的碼 | 成立；`:99` 殺不了 `rs-fix-b2-stop-after-declared-edges`，殺它的是 `:109`（已標記） |
| `ExecutorTests` :316、:331（J-32b） | DI-KS 2 | 觸發換成 `AllotterExhausted` | 成立 |
| 指紋 golden 各處 | DI-KS 3 | Registry 2→3、Plan 4→5、C 6→7、匯出標頭 2→3 | 成立；`threadBound`、`annotatedWrites` 的指紋缺口已補守衛 |

### 15.4 第 7 節的最終文字

照第 7 節的表改寫，另有實作中加的（第 14.2 節）：DI-KS 18 加「例外離開當場改時 World 中斷（`ImmediateChangeAborted`）」；規則 5 與 DI-KS 26 寫明「已中斷的 World 照樣拒絕」；DI-KS 14 照 S10 的決定改寫並加「Kernel 只用 `CurrentThreadSerial()`」（K8）。RC 3 拿掉；RC 16 新增；RC 17、RC 18 以草案新增（第 12 節第 2 件選了 (a)）。

### 15.5 實作中新出現、請一起看的

| 項 | 做法 | 會不會改到已核准的能力 |
|---|---|---|
| 登記的前提 | 這份 Registry 有 World 正在跑（計數不是 0）時 `Register`／`BuildPlans`／`DiscardRegistrations` 是斷言 | 不會：規則 4 本來就規定登記和它的 World 的執行不重疊；計數不是原子的，跨執行緒只是盡力抓。要「多個 World 在不同執行緒共用一份 Registry、跑著時登記」得走 RC 17 |
| 當場的 `Spawn<C…>` 拿到還活著的 id | 回錯誤碼 `AllottedIdAlive`、不生成、記一筆 `RejectedSpawn`；錯誤只交給呼叫的 System（Run 不以它結束、World 不中斷） | 使用者 2026-11-18 已定（不要回空）；回傳型別照第 3.5 節原樣；DI-KS 18 的那一句照此改寫 |
| 例外離開當場改 | World 立刻中斷，留一個錯誤給最外層的 Run | 比照 K0 已核准的 X-16 |
| `StartupCode` 沒有文字 | 變成建置錯誤（`-Wswitch`／C4062） | 只收緊 |
| 拿掉的 System | 留墓碑，不算進指紋 | 不會 |

### 15.6 合併前還要做的

L1 三平台（PR 上）；Windows 與 Linux 本機沒跑過。不在這批範圍、建議另開的：基準就沒殺掉的 3 個突變（`ks-real-product-name`、`rs-sample-capture-accepted`、`rs-sample-return-accepted`）；K2～K8 共用的抹除函式讀錯有前綴的字面量；`engine/entity_store` 能不能持有 `IdIssuer`（W3 不認它，要定）；K3 不認編譯器的原子內建。
