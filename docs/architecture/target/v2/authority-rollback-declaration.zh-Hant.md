# 事前宣告：權威 Module＋回滾 Module（`v8-arch-gyo2-authority-rollback`）

更新：2026-11-21（修訂 4：照 `gyo2` f25837b0 更新——#257 的分層換掉舊路徑、異常分級（#254、#255）合併後重對第 10.3 節、trace（#258）留下的「回滾重跑的段」寫成第 10.4 節；差異逐條在第 19 節，都在已核准的範圍內。2026-11-20 修訂 3：使用者重新核准修訂 2 的 5.4、判定 5、5.5（`v8-arch-gyo2-authority-rollback-reapproval`、`-ratio-p99`），換上 Linux 實機重量的基準值、加四分位距比 25% 的事前宣告、記入異常分級卡的交界。修訂 2：5.4、判定 5、5.5、15.2 照研究卡 v2 重推。修訂 1 之後同日記入的：使用者定的第 12 節第 1～4 件、「不參考 master」、第 5 件的拆卡、規模的處理、A1～A11 的方向、突變處置、LIM-7／LIM-8 的方向、預算的單位與深度的根據；5.4 依研究卡整理並照研究卡推薦定案、5.5 入口形狀寫進 Contract、第 11 節 DI 文字的方向）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：動工中（使用者 2026-11-21「可以開始權威卡了。記得讓他更新」，經主會話轉；HANDOFF `v8-arch-gyo2-authority-rollback-start`）。** 本分支已 rebase 到 `gyo2` f25837b0；第 1 步前半（第 3 批延下來的三條守衛，原 afd6e7c5）rebase 後照新路徑改過（第 19 節）。

經過：修訂 0（6e20758a）送 Fable 5.2／high 敵對式檢查：必改 10、建議改 11、小問題 9（報告：主 checkout `build/evidence/gyo2-authority-rollback/declaration-r0-adversarial-check.md`）。修訂 1 全部照改，處理對照在第 17 節。修訂 1 之後的改動與修訂 2 送 Fable 5.2／high 窄複查（結果見第 17 節之後的處理）。

性質：實作段第 4 批的事前宣告（AGENTS §3 的 Architecture Delta、§2 的基盤 Architecture、§14.4 的判定變更、Design Invariants 的變更）。11-17 核准的[權威份](authority-module-declaration.zh-Hant.md)（修訂 5 補，7f9bbb85）與[回滾份](rollback-module-declaration.zh-Hant.md)（修訂 6，9418fcf1）是設計的依據，**除第 12 節末列的之外，本頁不改它們定過的事**。本頁只寫：(1) 兩份寫在第 2、3 批之前，和現在的程式逐條對照；(2) 兩份留給實作批的細節；(3) `World::Summary` 改逐型別雜湊；(4) 拿掉 Kernel 的權威規則；(5) 拆卡、步驟、判定。

用詞：「設計段／實作段」「動工前核准／合併前核准」「第 N 步」照第 3 批。標「**本卡照 L17 判**」的是這張卡的判斷（名字照命名規範、細節照別家原始碼），**不是使用者的話**。本卡新加的詞第一次出現時標「本卡加的詞」。

---

## 0. 根據與讀的版本

**程式**：`gyo2` 59fd4da9（#253 之後）；修訂 4 起以 f25837b0 為準（#254、#255 異常分級，日誌批，#257 分層，#258 trace；差異見第 19 節）。講現況的句子來自讀程式的偵察（Opus 5.5／high，主 checkout `build/evidence/gyo2-authority-rollback/`）：
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
| HANDOFF 決定表（`origin/claude/gyo2-time-design`，修訂 1 讀到 57c8dc32；修訂 2 的窄複查讀到 f944ed53，含 `-baseline-redo` 等） | 主會話 | 使用者的決定。動工時再讀一次（`verify-before-premise`） |
| [限制頁](../limits.zh-Hant.md) LIM-7、LIM-8 | 架構師 | 第 3.4、8 節 |

**使用者的決定**（原話以 HANDOFF 為準）：`-kernel-is-ecs-no-restrictions`、`-authority-is-game-design`、`-authority-strategy-mechanism`、`-authority-own-registration`、`-authority-no-writer-count`、`-confirm-derived-removals`（含 11～13）、`-rollback-module-strategy`、`-strategy-interface-impl`、`-immediate-change-via-exclusive-world`、`-no-runtime-interception`、`-no-ambiguity-detection`、`-dynamic-insert-per-world`、`-module-deps-in-registers`、`-products-pick-modules`、`-engine-not-rigid`、`-layer-duty-lists`、`-events-two-kinds-like-bevy`、`-event-delivery-rule`、`-authority-guard-gap-later`、`-guards-after-mechanism-stable`、`-scenario-tests`、`-scenario-failure-by-contract`、`-scenario-cross-engine-check`、`-summary-padding-a-then-c`、`-mutation-guards-field`、`-confirm-only-differences`、`-batch3-no-xhigh`、`-net-is-mechanism-first`、`-merge-needs-explicit-word`。

**`-summary-padding-a-then-c` 的範圍**（修訂 1 更正）：使用者的原話是「先選A，但是把方案C記下來。這個case表為未完全解決，解決方案用C」。HANDOFF 記「原始位元組雜湊的三種毛病（padding、指標或 heap、+0／-0）」是 **side chat 討論時列的**，使用者沒有另外說。所以本頁把「逐型別雜湊」當使用者定的方向；三種毛病各要不要解、怎麼解，照第 3 節與第 12 節。

---

## 1. 這一批做什麼（先用遊戲講）

**PvP 射擊**，一個 Product 裡放兩個 World 做測試（之後由網路 Module 分到兩台機器）：

- **Match 的 World**：建 World 時選「伺服器權威」策略、不選回滾策略。作者寫 `authority.Component<Health>`、`authority.Component<Position>`、`authority.Mark<Tick>()`。啟動時策略檢查：World 宣告了不准動態插入；除了時間 Module 的 `RunFixedSteps`，沒有別的 System 寫 `Tick`。執行中不攔任何寫入。
- **Client 的模擬 World**：也選伺服器權威，再選官方的回滾策略。它多登記一個畫面用的 `HitFlash`（一般的 `registrar.Component`），所以兩邊的**一致性指紋**（權威份的 C 指紋；名字見 4.3）只算判定資料、照權威登記的順序編號，兩邊相同、連得上。
- Client 預測第 100～106 個 tick。測試把 Match 第 100 個 tick 的整份判定值（`Health`、`Position`、存活集合）交給回滾 Module 的入口（第 5.5 節選 (a) 時再加「第 100 步以前收齊」的標記）：和 Client 的歷史不同。回滾的獨占 System（掛在 `Main` 的 `PreUpdate`）立起 `Resimulation`、把 `Health`、`Position` 與存活集合**一批**放回第 100 個 tick（全部放回之後才跑 hook 與 observer）、用時間 Module 的 `RewindFixedSteps` 退 tick、再用 `RunOneFixedStep` 重跑 6 步，放下 `Resimulation`。「播腳步聲」掛了「重跑時不跑」，重跑時不出聲。重跑完 Client 和 Match 的 **digest**（判定資料的雜湊）相同。
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

本卡照 L17 補的細節（修訂 2 依窄複查 S5 標明）：「World 裡有」定成**有值才報**（欄有格、Resource 有值、訊息緩衝有紀錄）；拿掉那個型別的實體後同一個 `Summary` 會成功。使用者的原話是「執行到報錯」，這個界線是本卡定的。

**Engine 的 Module 放進 World 的型別都附雜湊函式**（修訂 2，依窄複查 S5）：兩個 Module 預設帶（`default=yes`），不選策略時公開登記照樣在 World 裡，沒有函式的話單機 World、讀檔測試一比摘要就報錯。雜湊什麼：回滾的公開登記 Resource 只雜湊每筆的型別序號與選項的種類（函式指標不雜湊）；歷史環、修正狀態的函式**什麼都不寫**（摘要描述模擬狀態，歷史是可重建的快取；讀檔兩次的歷史本來就不同）；權威的登記同公開登記；`Resimulation` 與時間 Module 的 Resource 雜湊各欄位（時間的型別在第 1 步補）。

### 3.4 LIM-8 留不留（本卡判；第 12 節第 1 件定 (d)、第 2 件定 (i)）

**LIM-8 改寫，不解除**：原始位元組不再是預設，但「作者選照位元組」那條路仍然要作者保證位元組＝值（沒有 padding、沒有 `optional`／union 的殘值、沒有指標與句柄、沒有寬度隨平台的型別）——digest 跨行程比，這條限制現在就適用，不是將來（修訂 0 想另立 LIM-9、「跨行程時才有事」是錯的，檢查 M1）。改寫後的 LIM-8：「`World::Summary` 與 digest 對每個值呼叫型別的雜湊函式；作者選『照位元組』的型別，位元組必須等於值」，守衛：Kernel 附的型別的測試；作者的型別無守衛。第 12 節第 1 件定 (d)，不加 (b)、(c) 那種「略過」「退回位元組」的句子；改寫後的 LIM-8 再加一句「沒有雜湊函式的型別在 `World` 裡時，`Summary` 回錯誤；作者的型別有沒有函式，由作者的測試守」。條文的 owner 是架構師會話，合併前核准時給你看。

---

## 4. 權威 Module：照權威份，加上實作的細節

範圍照權威份第 4、5 節，不改。下面只寫實作時要定、權威份寫「暫名」或「實作批定」的。

### 4.1 位置與登記

- `engine/modules/authority`（修訂 4：#257 的分層）、`GYO::Engine::Authority`、target `GYO::Authority`、登記單位 `AuthorityModule`（`Orchestration::Module` 的子類）；頂層 `config/modules.csv` 加一列（`dir=modules/authority`），`depends=base;kernel;orchestration;world_time;rollback`，**`default=yes`**（修訂 1，檢查 S2：照批次表「主會話自己判的」——Module 預設全帶、產品可以關；World 不選策略就沒有作用。修訂 0 寫 `default=no`、引 `-products-pick-modules` 當根據是錯的，那個決定沒有定預設）。代價：用產生的單位表的產品，每個 World 多兩個登記單位，登記單位名單進 Registry 指紋，golden 本來就要在本批重產。
- 登記檔的順序：`world_time` → `rollback` → `authority`（O-6 不重排）。

### 4.2 策略的插口與錯誤的回報

- `AuthorityStrategy`（虛函式的抽象基底類別，L21）、`ServerAuthorityStrategy`、佔位 `PeerRollbackStrategy`（名字實作時照命名規範定）。
- 選法：產品在 `WorldDescription::options` 放一個帶策略物件的選項結構（本卡照 L17 判：時間 Module 已用同一條路）。選項包同一份 `WorldDescription` 在 `ReplaceWorld` 時重用，`WorldSetup::Options<T>()` 回的是 `const`：**策略物件不帶每個 World 的狀態**，狀態放在那個 World 的 Resource（修訂 2，依窄複查 S8；`BuildNewFirst` 失敗時舊 World 照跑，同 5.1 的理由）。
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

- `engine/modules/rollback`、`GYO::Engine::Rollback`、target `GYO::Rollback`、登記單位 `RollbackModule`；`config/modules.csv` 一列（`dir=modules/rollback`），`depends=base;kernel;orchestration;world_time`，`default=yes`（理由同 4.1）。
- 公開登記六種照回滾份 5.4；讀取介面（R-6）給權威讀。
- 公開登記執行時要用（記錄、還原每步都讀）：**登記凍結之後放進那個 World 的一個 Resource**（本卡照 L17 判：Unity 把回滾的設定放在 World 的 singleton；回滾份 5.5 第 1 點的歷史也在 World 的 Resource）。不放在 Module 物件裡以 World 名為鍵的表：`ReplaceWorld` 的 `BuildNewFirst` 失敗時舊 World 照跑，而 Module 裡那份表已在新的一輪 `Build` 重設（O-2；檢查 M5 第 4 點）。

### 5.2 官方策略掛在哪（本卡照 L17 判）

- 判斷、還原、重跑是一個不帶 `Writes<…>` 的 `ExclusiveWorld` System，掛在**每幀 schedule 的 `PreUpdate`**（預設 `Main`，在 `FixedMainLoop` 之前）。系譜：lightyear 在 `PreUpdate` 跑 `run_rollback`（回滾份第 4 節表）。不能掛在步的 schedule 裡（X-11，2.1）。
- 記錄是不帶 `Writes<…>` 的 `ExclusiveWorld` System，掛在步的 schedule 的 `recordStage`（預設 `Last`）。回滾份 5.5 第 9 點寫「記錄是只寫歷史 Resource 的一般 System」；改成獨占 System 是因為要讀 stamp（G5），兩種在權威的檢查裡都看不到它寫判定資料，結論不變（第 12 節末「字面不同」）。
- 刪掉的格：記錄時比這一步與上一筆的鍵集合（本卡照 L17 判：lightyear 用 `RemovedComponents`；GYO 的 `Remove<T>` observer 也做得到，但比鍵集合不必在 observer 裡改歷史，少一條重入的路）。

### 5.3 第一版的設定值

最大回滾深度預設 20（照 lightyear，回滾份 5.5 第 1 點；修訂 2 的根據換成區網實測的往返延遲，5.4.4；產品依自己量到的線上延遲另設）；判斷的模式三種，預設「比對後不同才回滾」；修正預設指數衰減；`rollback.Component<T>` 的比較函式預設 `==`（回滾份 5.4；浮點和雜湊的關係見第 12 節第 2 件）。PvP 移植時另定。

### 5.4 預算（AGENTS §2；回滾份第 11 節要求官方策略那一批寫）——**修訂 2 照研究卡 v2 重推；使用者 2026-11-20 重新核准（修訂 3）**

**重新核准**（使用者 2026-11-20，主會話，`v8-arch-gyo2-authority-rollback-reapproval`）：原話「第 4 批 A、B 照建議，門檻 25%」。A 照建議：推翻 `…-budget-and-entry-contract` 的數字，改成 P1 32 KB／深度 20、P2 128 KB／深度 32、P3 2 KB／深度 20；門檻 3 改成「≤ 1.5 × 同一次量測的機器底比值」；每人 424 B（`pvp_baseline`）可以用。新的事前宣告：分母兩列的四分位距比任一超過 25% 就不判定（5.4.5）。比過的選項：維持已核准的 16 KB × 20 等數字／照 v2 改（選）；門檻由使用者直接定 25%（主會話建議值）。理由：使用者沒有另外說。

**門檻 3 的統計量**（使用者 2026-11-20，研究卡 v2 轉，`v8-arch-gyo2-authority-rollback-ratio-p99`）：分母維持 p99，宣告字面不變；先降雜訊再重量。原話：先說「選 b，分子分母都改用 p50」，之後改成「我想了還是維持p99」；降雜訊時使用者關掉 Linux 實機的 Chrome，governor 不改：「不用這樣改，你重跑一次就行了」。比過的選項：(a) 維持 p99、先降雜訊（選）；(b) 都用 p50（研究卡與本卡原本的建議）；(c) 都用平均。理由：使用者沒有另外說。

**處理**（使用者 2026-11-20，side chat）：開一張唯讀研究卡，整理 master 2026-11-01 之後的數據當**基準**（「以前是什麼樣，現在我們要做到什麼」），提出目標候選，數字交使用者定。原話：「預算的規模假設：這個我建議，你專門更具這個問題跑一個研究卡去看看master11/01之後的數據。不是要你參考，而是你需要建立一個基準。以前是什麼樣，現在我們要做到什麼。你覺得如何？」→「但是你要開研究卡，範圍沒錯。GYO很多問題都是到10月底才處理完，11月節點可以。」研究卡：[研究卡] 第 4 批預算基準（`local_3eaf3c3f-2af9-4e8e-947c-a1511cab0350`，唯讀；開卡時說 Opus 5.5／high，**App 記錄的檔位是 low**（主會話 2026-11-20 查到）——報告第 12、13 節進動工前的敵對式檢查），報告寫到主 checkout `build/evidence/gyo2-authority-rollback/budget-baseline-research.md`。和「不參考 master 的算法與數據」（第 12 節）的分界：master 11-01 之後的數據只當基準比對「以前到哪、現在要到哪」，不拿它的算法、常數或架構來設計；11-01 之前的數據（含 2026-09-25 的 dev_log）照舊不用。

**重做**（使用者 2026-11-20，主會話：「重開，然後如果任務卡那邊用了錯誤的推論，重來。」）：第一張研究卡的檔位是 low、沒有逐份讀 dev_log，每人位元組用 protobuf 欄數 × 4 B 推估（168 B）。主會話另開研究卡 v2（`local_9474f6e2-883c-429f-8238-abd4e0432df1`，Opus 5.5／high），報告：主 checkout `build/evidence/gyo2-authority-rollback/budget-baseline-research-v2.md`（第 7 節逐條判本頁）。修訂 1 的 5.4.6（使用者 2026-11-20 核准的 P1 16 KB 等）凡出自舊推算的，本修訂照 v2 重推，**重新交使用者核准**；v2 判「成立」的（門檻 1、2、量法、前提「策略由作者選」）照舊。修訂 1 的 5.4.1～5.4.7 留在 git 歷史（fcae13f1 之後、本修訂之前的提交），本頁不再保留比過的候選表。

#### 5.4.1 基準線：以前（master 11-01 之後；v2 第 2 節）

| 項目 | 值 | 出處（v2） |
|---|---|---|
| 回滾、預測重跑、修正的時間 | master 之前**沒有量測**。master 的「回滾」是 Client 只預測本地玩家：`Reconcile` 把它設成權威值後重跑待確認的輸入（≤ 12 筆），沒有狀態歷史。研究卡 v2 在 Linux 實機補量：最壞一次（biome、重跑 12 筆）p99 **13.95 µs**（`reconcile_k12_biome`，10 個行程 p99 的中位數；修訂 3） | 2.2、2.6、結論 2；資料 `baseline-v2/linux-20261120/run3-quiet/` |
| 規模 | 4 人、60 Hz（原始碼常數，只當對照） | 2.1 |
| 每個角色的記憶體大小 | `PlayerState` 128＋`CombatState` 112＋`AnimationTrack` 184＝**424 B**（實測；4 人＝1,696 B，和 World 欄的實測相同） | 2.1 M1（`pvp-baseline-report.zh-Hant.md:58`）。這是 `pvp_baseline` 的數字：使用者的 `-no-master-reference` 點名不用，之後 `-budget-baseline-research` 把 11-01 之後的數據開放成基準；本卡把它當「估狀態大小」，**使用者 2026-11-20 准用**（`-reapproval`） |
| 一個玩家的移動一步 p99（broadphase 之後） | 約 0.0067 ms（Intel Mac、RelWithDebInfo）；4 人一 tick 約 0.032 ms | 2.1 M3（`v8-product-pa-broadphase-declaration.md:56、61`） |
| 區網的往返延遲（時鐘同步的樣本） | 每 10 秒視窗最小值的中位數約 2.2 ms（不到 1 步）；視窗內最大值的中位數 10～13 ms（約 1 步）、其 p95 41～99 ms（3～7 步）；全場最大 231～354 ms（15～23 步）；逐樣本的 p95 沒有記錄。snapshot age p99 23～29 ms。master 的 Match＋Gateway 跑在 Mac 上的一場（11-09，探索值） | 2.1 M7、M8（修訂 2 依窄複查 M3 更正：修訂 2 初稿把「視窗最大值的 p95」寫成延遲的 p95） |
| 量測穩定度 | Intel Mac 原生的 µs 級 10 次全距比 4.5～13.8%（Mac 上的 Docker 10～48%）；Linux 實機約 1%、能分辨 2～3% | 2.1 M12 |

排除：11-01 之前的數據（含 2026-09-25 的 dev_log）；master 的算法與常數不當設計根據（`MaxPendingCommands`、`MaxPlayers`、hitbox 的 R＝7 只當「以前」的對照；v2 第 5 節）。

#### 5.4.2 已定的（使用者 2026-11-20，照舊）

原話（在第一張研究卡）：「規模用位元組，深度從 RTT 推。PvP 目標是4人，協議可以看master的 protobuf設計。」同日補：「你還得去看願景計劃，master不單單是4人PVP」「200mx200m 是測試；目前的地圖都是 500x500，還有地面和地下 2 個部分」「所以還有分段加載」「這些問題master上有基礎數據」；以及「你是研究卡，這些東西都是需要你來推的。而且你說的這些策略，其實都是和產品有關，得看作者如何選。」
- 規模用位元組；深度從延遲推；PvP 4 人（願景之後可能 8 人）；策略（哪些實體可回滾、深度、MC-2）由作者選，預算寫成「Module 的成本對可回滾位元組 B 與深度 D 的關係」，用涵蓋已點名 Feature 的點判定。
- 估每人大小：protobuf 欄數 × 4 B 的估法由實測的記憶體大小取代（v2 第 7 節 5.4.2）。
- 地圖 500 m × 500 m 兩層：repo 沒有出處，和 big-map 的約 1 km² 衝突（v2 1.2）；**不影響 Client 的判定點**（角色數是全圖總數），只影響 Match 的實體總數（只記錄、不判定）。

#### 5.4.3 規模（v2 3.1）

| 項 | 值 | 來源種類 |
|---|---|---|
| 每個角色 | 424 B | 實測（master 的元件配置；GYO2 的元件可能不同，量級可用） |
| 角色上限 | 8 玩家＋AI 30＋Boss 1＝39（文件之間 AI 20／30、玩家 4／8 衝突，取上限，v2 1.2） | 規劃 |
| 角色全部可回滾 | 39 × 424 ≈ **16.5 KB** | 推算；**AI、Boss 照玩家的 424 B 算是假設**（v2 缺口 G3：非玩家實體的位元組沒有實測） |
| 事件類實體（作者選了「事件改到的狀態也回滾」時） | 326 個 × 16～32 B ≈ 5～10 KB | 規劃的數量 × 推算的大小（沒有實測） |
| 私人（背包 40 件） | 約 0.6 KB | 推算 |
| 合計上限 | 約 **27 KB** | 推算 |
| 4 人 PvP 全部可回滾 | 1.7 KB | 實測 × 規劃 |

- 角色數是全圖的總數，不隨地圖大小變；interest management 只會讓 Client 收到的更少。修訂 1 的「Client 看得到約 315 個」外推刪除。
- 通常 AI 在 Client 走內插、不預測；27 KB 是作者全選時的上限，不是預期值。
- Match 的實體總數：規劃約 400（200 m 問卷）；大地圖沒有規劃值（v2 缺口 G6）。digest 與 `World::Summary` 量測用一個大的合成數（例 5,000），標明不是需求。
- 旁證：broadphase 之後一個玩家的移動一步 p99 約 0.0067 ms；作者的重跑約 39 角色 × 20 步 × 6.7 µs ≈ 5.2 ms（推算；AI、Boss 和玩家同成本是假設），大於 Module 的總開銷上限，Module 不是瓶頸（修訂 1 用的「一步約 2.2 ms」是 broadphase 之前的數字，撤回）。

#### 5.4.4 深度（v2 3.3）

- 公式：深度 ≥ ceil((RTT＋抖動餘量) ÷ 15.625 ms)。
- 區網實測（探索值，5.4.1）：往返延遲的典型值 2～13 ms（1 步以內）；視窗最大值的 p95 41～99 ms → 3～7 步；全場最大 231～354 ms → 15～23 步。回滾深度是「現在 − 確認點」，確認點會落後最新收到的 tick（第 5.5 節），所以公式的 RTT 要加確認點的落後。修訂 1 用的「view_lag p99 5 tick＋2 → 16 步」把 view_lag 當成 RTT（它含內插與排隊），撤回。
- **預設 20**（312 ms）：涵蓋區網視窗最大值的 p95 約 3 倍、典型值很多倍；區網的 max（23 步）會超過，超過深度時照第 5.5 節處理，不靠加深度解決。根據種類：實測（區網）＋參考實作（lightyear `max_rollback_ticks` 預設 20，`manager.rs:46-56`）。
- **WAN 無法判定**（沒有目標 RTT、沒有量測）：產品依自己量到的線上延遲另設；預算按 P2 的 32 步記錄。
- 對照：master hitbox 回溯的歷史 8 Tick、預算「≤ 64 KB、每發 p99 ≤ 0.2 ms」（master 自己的寫法，只當對照）。

#### 5.4.5 判定點與門檻（v2 3.2、3.4、3.5；**使用者 2026-11-20 重新核准**）

| 點 | B | D | 用途 |
|---|---|---|---|
| **P1 判定點** | **32 KB** | 20 | 5.4.3 的上限 27 KB 進位到 2 的冪 |
| P2 壓力點 | **128 KB** | 32 | 只用在門檻 3 的比值，其餘只記錄；B＝P1 × 4（讓第 3 條的比值好讀）；32 步：Unity 規定 Client 最多領先 500 ms（`NetworkTimeSystem.cs:220、242-243`），64 Hz 是 32 步（參考實作，只回答「做到哪」） |
| P3 小點 | **2 KB** | 20 | 只記錄；4 人 PvP 全部可回滾 1.7 KB；看固定開銷 |

以前→現在：master 的回滾狀態是 1 人 × 128 B、重跑最多 12 步；P1 是它的 256 倍位元組、1.7 倍步數。

**Module 這一側的配置**（修訂 2 依窄複查 M2 寫死；回滾成本主要跟格數走，只給位元組會讓格數差到 16 倍）：一個 archetype、16 種元件、每格 64 B（和機器底的 `colsT16s64` 同形）；P1＝32 個實體（512 格）、P2＝128 個實體（2,048 格）、P3＝2 個實體（32 格）。「每步所有格都改」由量測程式在計時之外把全部格標成改過（直接寫值並推 stamp），不經作者的 System。另列一組只記錄的「實際組成」：39 個 424 B 的角色（3 種元件）＋326 個 32 B 的小實體。

門檻：
1. 記錄每步（P1，每步所有格都改的最壞情形）≤ **156 µs**（一步的 1%）。規則，照舊。對 32 KB 很寬，只擋得住大退化。
2. 一次回滾總開銷（P1：退 20 步、全部格都變；還原＋20 次記錄＋`ActiveTime`／`Resimulation` 起落，不含作者的 System）≤ **3.125 ms**（一步的 20%）。規則，照舊。量法：重跑時步的 schedule 只放一個「把全部格標成改過」的量測用 System；判定值＝每行程（一次回滾的總時間 − 20 × 同一行程量的那個 System 單獨一步的中位數）的 p99，取 10 個行程的中位數。
3. **線性**（改寫）：同一次量測裡，`(P2 的記錄每步 ÷ P1 的記錄每步)` ≤ **1.5 ×** `(機器底 hot_record_colsT16s64_B128K_D32 ÷ hot_record_colsT16s64_B32K_D20)`；分子、分母都是「10 個行程各取 p99，再取中位數」之後相除（`-ratio-p99`）；兩邊都 hot、都不 batch（一次一個樣本），分子分母的大小與深度相同。理由：P1 的歷史約 640 KB、P2 約 4 MB，落在不同的快取層（Ryzen 7 6800H 每核 L2 512 KB、L3 16 MB），機器本身的比值就可能超過 4；修訂 1 的「≤ 6」的 6 是猜的。1.5 擋得住「每格多做了 O(n) 的事」，擋不住 O(n log n)（這個規模只多約 1.2 倍），是已知的限界。**基準值**（修訂 3；研究卡 v2 在 Linux 實機重量，2026-11-20 20:52～21:05）：分母＝2.4795 ÷ 0.5960＝**4.160**，Module 的比值上限＝1.5 × 4.160＝**6.240**；判定時用同一次跑出來的分母，不沿用這個數。
   - **分母穩不穩**（使用者 2026-11-20 定，`-reapproval`；事前宣告）：分母的兩列（`hot_record_colsT16s64_B32K_D20`、`hot_record_colsT16s64_B128K_D32`）的四分位距比（10 個行程的 p99）**任一超過 25% 就不判定第 3 條**，停下回報（§14），由使用者決定降雜訊或重量的做法；不改門檻。參考：基準那次 4.2%／9.0%；桌面開著 Chrome 那次 136%／65%（留作紀錄，不當基準）。
4. 只記錄：P1 的每 KB 記錄 ns 與機器底的比值、P2、P3、冷快取、「實際組成」；P1 確認點前進一次的全掃比對（第 5.5 節 (a) 每次確認點前進都要掃，機器底有 `hot_cmpelem`）；記憶體（P1 的環約 640 KB、P2 約 4 MB）。下一批可以用它收緊。

量法：判定在使用者的 Linux 實機原生（**量的條件和基準那次相同**：關掉桌面程式（Chrome 0 個、開跑前負載接近 0），governor 照舊 `powersave`、EPP `balance_performance`；`taskset`、先暖機並記頻率；10 個行程 × 各 1,000 次，取每行程 p99 的中位數，四分位距比與平均一起記）；**每次判定都在同一次跑機器底**（交付包的 `floor_bench`，第 3 條的分母）。Intel Mac 只當開發參考。超過就停下回報（§14），不放寬、不重量。量的情境：P1 的穩定每步與一次回滾到 20；P2、P3 的記錄每步；P1 加一組「部分元件、部分實體」（第 5.5 節的形狀，只記錄）；digest 與 `World::Summary` 在大的合成實體數時的時間（只記錄）。

**機器底的基準值**（修訂 3；`run3-quiet/out/summary.md`，10 個行程 p99 的中位數，配置 `colsT16s64`）：P1 32 KiB／D20 記錄每步 0.596 µs、回滾 14.68 µs；P2 128 KiB／D32 記錄每步 2.480 µs、回滾 81.31 µs；P3 2 KiB／D20 記錄每步 0.146 µs（比桌面開著那次高約 25%，兩次各自穩定；只記錄）。門檻 1、2 分別約是機器底的 262 倍、213 倍，只擋得住大退化（和預期一致）。

**量測程式與交付包**（修訂 2 依窄複查 M1、S9；修訂 3 註：(1) 研究卡 v2 已做完並在 Linux 實機跑過，見 HANDOFF 2026-11-20「第 4 批 Linux 實機量測完成」）：v2 備好的交付包（主 checkout `build/evidence/gyo2-authority-rollback/baseline-v2/bundle/run-linux.sh`，約 25 分）只量「以前」（master 的 Reconcile）與機器底 1／4／16／64／256 KiB、`hot_record` 只在深度 20、32 KB 一邊會 batch；**判定 5 第 3 條的分母（32 KiB／D20、128 KiB／D32）現在的包量不出來**。另外包沒有實跑過（只做 `bash -n`，C++ 沒編過）、`analyze.py` 沒有彙整平均。所以：(1) 在使用者去跑之前，`floor_bench` 要加 2／32／128 KiB、兩個深度都量、不 batch、冷快取改 32／128，`analyze.py` 加平均；(2) 回滾 Module 的量測程式與離線腳本在第 4 步做（第 14 節），照 `offline-session-scripts`：一台一支腳本、交出前每條命令實際跑過，和機器底在同一次跑。「以前」與機器底已量（上面的基準值）；回滾 Module 本身未量。判定點與門檻是規則或同一次量測的比值，不等 Module 的量測結果就能宣告（§14.1）。

### 5.5 多通道、interest management、分段加載要留的入口形狀——**修訂 2 照源碼重核；使用者 2026-11-20 重新核准（修訂 3）**

**重新核准**（使用者 2026-11-20，主會話，`v8-arch-gyo2-authority-rollback-reapproval`；原話「第 4 批 A、B 照建議，門檻 25%」）：B 照建議——MC-5 依回滾份 5.3 已有的「判斷」掛點寫，官方用 (a) 全域完成點，作者覆寫可做 (b)、(c)，不新增掛點；GYO 特有的 MC-1（事件不可靜默丟）與 MC-3＋SL-1＋SL-2（三態＋有效期間由呼叫者明說）留在 Contract；MC-4 的差分規則寫成給網路 Module 的需求。比過的選項：MC-5 固定一種／策略、官方 (a)（選）；GYO 特有兩條留（選）／不留。理由：使用者沒有另外說。MC-1 的落實方式見下面第 2 件（本卡照 L17 判）。

**經過**：修訂 1 依第一張研究卡寫的形狀，使用者 2026-11-20 核准寫進 Contract（原話「預算照研究卡推薦，入口形狀寫進 Contract」）。之後查到第一張研究卡把 MC-5 寫反（lightyear 不是逐實體記確認 tick），研究卡 v2 照源碼逐條重核（v2 第 4 節；版本 lightyear `736e1695`、bevy_replicon `d80c849`、Unity Netcode 7.2.0 `c4cabf4`）。照使用者「如果任務卡那邊用了錯誤的推論，重來」，下面的形狀重新交使用者，**2026-11-20 核准**（見上）；回滾份 5.5 第 2～5 點照第 12 節末的字面不同第 7 點改寫。

根據：AGENTS §2 的 1（使用者點名：「master之前有規劃多通道。但是還沒開始實作。我估計你需要先知道，需要留設計。」、interest management（願景）、「所以還有分段加載」）；2（下表的參考實作）。多通道的需求改由 11-07 之後的文件給（`pv10-requirements.md`、`scene/README.md`、`kernel/networking.zh-Hant.md`；v2 第 5 節）：不可靠的 snapshot，加每個收件者一條可靠有序的事件串流。

| # | 情況 | 回滾份 5.5 現在的寫法 | 別家（v2 第 4 節） | 判斷 |
|---|---|---|---|---|
| MC-1 | 可靠事件晚到、指向舊 tick | 第 4 點：超過深度就丟 | lightyear：權威值不論新舊都進確認歷史，在確認點 C 用「C 當時或之前最後的值」比，舊的可靠值會被帶到 C（`registry.rs:553、633-695`）；超過最大深度就放棄這次回滾並 warn（`rollback.rs:404-412`）。replicon：過舊的 mutation 丟掉。Unity：RPC 不綁 tick、不進預測 | **GYO 特有**（「事件不可靜默丟」寫進 Contract），交使用者 |
| MC-2 | 事件改到可回滾的狀態 | 沒寫 | lightyear 有手動先例：`ConfirmedHistory::insert_explicit`＋`request_forced_rollback` | **成立**：作者選，Module 只給機制（使用者「策略…和產品有關，得看作者如何選」） |
| MC-3 | interest management：只送看得到的 | 第 5 點：存活集合整份放回 | lightyear 只有 Updated／Removed 兩態，「沒帶」靠全域確認點推成「沒變」；replicon、Unity 把失去相關性當成刪除（Unity 文件要使用者另外區分） | **GYO 特有**（API 上分「確定刪除」與「這份沒帶」），交使用者 |
| MC-4 | 低頻、只帶部分元件 | 第 4 點：整份比 | lightyear 以（實體、元件、tick）比對，預設 `PartialEq::ne`（`registry.rs:519-613`）。另外 lightyear 的值 delta 壓縮（`Diffable`）有「差分沒解開的 tick P 以後都不能當確認點」（`rollback.rs:358-361`、`499-538`） | **成立**：比對與還原以（實體、元件、tick）為單位，留在回滾。「確認點不得晚於還沒解開的差分」是給確認點的一方（網路 Module）的要求，回滾不認得網路、守不了（R-3），列進給網路 Module 的需求，不進回滾的 DI 與 9.3（修訂 2 依窄複查 M8） |
| MC-5 | 選哪一步當回滾點 | 第 3、4 點：一次一個「第 T 步」 | lightyear：**一個全域的完成確認點**（`rollback.rs:365` `last_confirmed_tick()`），在那一步逐實體逐元件比；註解說舊的實體層級做法會漏掉同實體上沒變但不一致的元件（`:350-357`）。Unity 預設逐 ghost（只回滾收到新 snapshot 的，`GhostUpdateSystem.cs:1038-1043`）；可選 `AlwaysRollbackAllPredictedGhosts`：全部回到各 ghost 新 tick 的最小值（`ClientServerTickRate.cs:655-672`）。replicon 兩種原語都給、都不實作 | **各家分歧**，交使用者。修訂 1 的「入口的鍵各帶 tick、判斷取最早」＝Unity 的可選模式 |
| MC-6 | 私人資料 | — | 各家都是每個 World 各自跑 | **成立**：不另設計；入口不假設每個 Client 的實體集合相同 |
| SL-1 | 段卸載 | 刪除與卸載不分 | 沒有一家有一般化的入口 | **GYO 特有**（屬有效期間），和 MC-3 一起交使用者 |
| SL-2 | 段載入，實體在第 T 步出現 | 回滾到 T 之前會刪掉它 | lightyear 依來源決定：伺服器複製來的保留現值、用最舊的權威值當種子（`rollback.rs:1054-1068`），預生成的刪掉；Unity `PredictionStartTick` 之前不模擬 | 行為**成立**（和 lightyear 一致）；「由呼叫者明說」是 GYO 特有，和 MC-3 一起交使用者 |
| SL-3 | 載入那一步大量寫入 | — | — | **成立**：等分段加載真的做了再加量測與門檻 |
| SL-4 | Match | — | — | **成立**：不回滾，不受影響 |

**交使用者的兩件與本卡的建議**（v2 第 6 節第 2、3 件；修訂 2 依窄複查 M7、S1 改寫）：

1. **MC-5 選哪一步當回滾點**：(a) lightyear：網路那一側告訴回滾「第 C 步以前都收齊了」，在 C 逐實體逐元件比；(b) Unity 可選模式：有不一致就全部回到各實體新權威值裡最早的那一步（修訂 1 的字面）；(c) Unity 預設：只回滾收到新值的實體（Unity 自己說會讓實體之間的時間線不一致）。
   - 回滾份 5.3 已核准的策略掛點本來就有「判斷要不要回滾、決定退回哪一步」（作者可只覆寫它），所以不新增掛點。要定的是兩件：(i) **入口帶什麼**；(ii) **官方實作的預設規則**。使用者的原則「這些策略…和產品有關，得看作者如何選」照用：作者覆寫「判斷」就能做別的規則。
   - **本卡與研究卡 v2 一致的建議**：(i) 入口帶（實體、元件、tick）的權威值，與「第 C 步以前收齊」的標記——兩樣都帶，作者的「判斷」做 (b)、(c) 時資料也夠用；(ii) 官方實作的「判斷」用 (a)。理由：(a) 在同一步逐元件比，接 MC-4，也避開 lightyear 註解說的漏比；Unity 把它做成專案選項是因為 CPU 成本，GYO 由作者覆寫「判斷」達到同樣的選擇。
   - 入口是**回滾 Module 層、所有策略共用的入口**（不是官方策略自己的入口）：回滾份 5.3 寫「外面送資料進來的入口不在 interface 裡，官方的狀態觸發策略定義它自己的入口」，這裡字面不同，列在第 12 節末。
   - 選 (a) 時 Contract 要再寫清楚四件（窄複查 S1；本卡照 lightyear 寫成提案）：
     - **「收齊」只算 snapshot 通道**；可靠事件串流不算進確認點（一條卡住的可靠串流不會讓確認點停住），晚到的可靠值照下面第 2 件 MC-1 的規則帶到確認點。
     - **tick 大於 C、已經收到的權威值**：留在確認歷史，回滾時不蓋掉，重跑到那一步時照它的值比（lightyear `rollback.rs:27`）。
     - **「這份沒帶」**：在 C 這一步不參加比對，**不推成「沒變」**（和 lightyear 不同；interest management 下沒帶的可能是離開了視野）；實體離開這個 World 的歷史由明說的「退出」表示（有效期間），兩者分工。
     - **成本與深度**：每次確認點前進都要把全部可回滾的（實體、元件）掃一遍比對，列為判定 5 只記錄的一項（5.4.5 第 4 條）；深度公式加確認點的落後（5.4.4）。
2. **GYO 特有的要不要留在 Contract**：
   - **三種權威值（有值／確定刪除／這份沒帶）＋由呼叫者明說的有效期間（MC-3、SL-1、SL-2）**：研究卡 v2 與本卡都建議**留**。interest management 與分段加載是點名的 Feature，會讓「離開」和「刪除」在同一個 World 裡同時出現；別家的做法都要產品另外繞（Unity 文件明說）。起點與終點由呼叫者明說、沒明說的一律當遊戲裡的生成與刪除（Kernel 裡卸載和刪除都是同一個 despawn，回滾 Module 分不出來）。
   - **MC-1「事件不可靜默丟」**：**使用者定留在 Contract**（`-reapproval`）。落實方式（**本卡照 L17 判**：照 lightyear 源碼；性質不變，只定做法）是改寫成 lightyear 的規則、拿掉「來源種類」這一格：晚到的可靠權威值照樣記進確認歷史，在確認點 C 用「C 當時或之前最後的值」比，所以不會丟；選了 (a) 這條規則就涵蓋了。超過最大深度時放棄這次回滾、記診斷（照 lightyear）。

**本批只定形狀、行為做薄**（AGENTS §2，成立）：interface 與 Contract 照核准的形狀寫；行為只做「整份 snapshot、一個 tick 的權威值（選 (a) 時加收齊標記）、沒明說的一律當遊戲生成與刪除」。本批回「還沒實作」錯誤（呼叫時回 Result 的錯誤，和 P2P 佔位的啟動錯誤不同形）的入口：部分元件的權威值、「這份沒帶」、明說的進入與退出、事件來源的權威值寫進歷史（MC-2）。多通道、分段加載的行為在網路 Module 與地圖線那一批補。代價：第 4 步多 6～10 h（修訂 3 估：入口的資料結構與收齊標記、官方 (a) 的「判斷」、四個「還沒實作」入口的錯誤與測試；修訂 1 的 4～6 h 沒有收齊標記與三態）。給網路 Module 的需求（不在本批）：確認點不得晚於還沒解開的差分（MC-4）。

---

## 6. Module 與作者的接法——**使用者定 (a)**（2026-11-20，第 12 節第 3 件）

**決定**：編排層給「每次建 World 一份、以型別為鍵」的共用槽；「不算 O-4 職責清單多一項」也經使用者確認。理由（使用者的）：組裝 Module 是編排層的工作（O-4「組裝與流程」、O-2、O-6），buffer 放編排層合理。用詞：使用者覺得它像「Module 的 buffer」（暫時的中轉區：建 World 期間用，內容凍結後交給 World 的 Resource，之後清掉），不像 cache；正式名字實作時照命名規範定，可參考這個說法（下面的暫名 `Shared<T>()` 也在那時重定）。附帶：`Build` 指執行期建 World 時 Module 的登記函式，和 CMake 的建置容易混淆；使用者沒有要求改名。下面保留決定前的比較。

**共用槽的機制**（使用者 2026-11-20 補充：「第 3 件，注意，由於是共用的，需要建立一個機制，或者需要保證順序，邊讀邊寫很容易出問題。」下面四點是 side chat 提的，使用者回「可以」）：

1. **分段，型別上擋住**：`Build` 階段（Module 的 `Build` 與產品自己的 `build`）經 `WorldSetup` 拿到 `T&`；`WorldFinish`、`WorldCheck` 只給 `const T&`。`Build` 階段結束時編排層凍結槽，凍結後再寫是 Programmer Error（斷言；實際擋的是凍結後呼叫槽的 `Create<T>` 或可寫的取用，見下面的錯誤形式）。要看全貌的判斷（Z6、Z8、D-E15 的交叉檢查、重複登記的裁決）一律放在凍結後的 `Finish`／`Check`。系譜（side chat 補，不是使用者的話）：Bevy 的 Plugin 分 `build`／`finish`，`finish` 在所有 `build` 之後才讀別人的登記。
2. **誰建、誰用**：槽裡每種型別只由擁有它的 Module 建（暫名 `Create<T>`）；別人用 `Find<T>`，找不到就回錯誤，不偷偷建空的。先後靠 `config/modules.csv` 的依賴與既有的 O-E3（依賴排在後面是錯誤）保證；產品自己的 `build` 永遠在最後。
3. **寫入只記錄、不裁決**：`Build` 階段只追加，每筆帶「作者／代為」的來源；凍結後統一裁決（作者的蓋過代為的；兩筆作者的 options 不同是錯誤），所以結果和呼叫順序無關。本卡照 L17 補的細節（修訂 2 依窄複查 M5、M6 改寫）：
   - **裁決是對追加紀錄的純函式**：擁有者的公開讀取介面拿 `const` 的紀錄、回裁決後的結果與錯誤清單；誰讀（擁有者自己的 `Finish`、權威的 `Check`）都經這個介面，所以不需要寫回槽（編排層不解讀槽裡的型別，O-2）。擁有者在 `Check` 用自己的字串碼報錯（`RB-E…`）。
   - **登記當下做的**：進 Kernel 的全部副作用照 DI-A 11 當下做（`AddSystem` 與「重跑時不跑」的執行條件、型別進 Kernel、`rollback.Input<T>` 的歷史 Resource 等），再在槽裡追加一筆帶來源的紀錄。**延後到凍結後的只有**：同一個型別多筆登記時哪一筆的 options 為準、兩筆作者 options 不同的錯誤。進 Kernel 的副作用不得取決於裁決結果：回滾的每型別選項是資料，由回滾的 System 在執行時讀凍結後的登記（5.1），不是每型別一筆 Kernel 登記。「重跑時不跑」不在裁決的範圍（一個 System 只 `AddSystem` 一次）。
   - 這和回滾份 5.4「不論先後，作者的為準」一致；和權威份 4.3「作者先登記，策略就不代登記」字面不同（現在一律追加、之後裁決，結果相同），列在第 12 節末。
4. **單執行緒、不加鎖**：照 CCR-5（`docs/cross-component-rules.md`：同一個 Registry 的 `Register`、`BuildPlans` 與 World 的建立在一條執行緒上），槽不加鎖；CCR-5 放寬時碰 RC（11.5 的 RC-O10）。

**生命週期與錯誤的形式**（本卡照 L17 補，修訂 2 依窄複查 S3、S4）：
- 槽活到那個 World 的登記單位的準備函式跑完（`Product.cpp` 的 `Register` 之後還有 `Prepare`；`PrepareFn` 只拿到 `World&`）；Module 要留到執行時的內容，在準備函式裡凍結進 World 的 Resource（5.1）。槽裡放的物件由槽持有，準備函式跑完後和槽一起釋放。
- `Find<T>` 找不到：回空（呼叫者自己判）。Module 在 `Build` 找不到依賴的登記是 Programmer Error（依賴由 O-E3 保證，走到這裡是程式錯）；產品自己的 `build` 找不到（例：關掉了回滾 Module 卻用 `rollback.`）也是 Programmer Error（斷言），不新增 O-E 碼。
- 兩個登記單位 `Create<T>` 同一個型別：Programmer Error（斷言）。
- 凍結後呼叫 `Create<T>` 或可寫的取用：斷言。經 `Build` 時留下的 `T&` 寫入，編排層看不到（不解讀型別），由擁有者的介面在凍結後拒絕追加（擁有者自己的斷言）。

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
| 區 | `Zone`、各 Options 的 `zone`、`external`、`RequireExplicitZones`、`ApplyDefaultZones`、`CheckZoneAccess` 與輔助（`ZoneOf`、`ZoneName` 等）、S-6、S-7、S-8 的 (c) 半、S-9、**S-13**（統合沒點名，但它是 D-E7 的區規則，`recon-a` 6 第 5 點）、S-17、Z1、Z2、Z3、Z7、`ingressStage`、X-2／D-E11／X-E1、`parallelGuardTest`、D-E9、`InitialValue` 的 Z1、`emitterZone`、`SystemContext::zone` | 不由機制守（P14、確認清單） | 第 2 步（沒有新家） |
| 寫者 | `WritePolicy`、`CheckExclusiveWriters`（D-E2 S-11） | 不由機制守（`-authority-no-writer-count`） | 第 2 步 |
| digest | `DigestSet`、`MarkDigestSet`（R-E4 的 digest 部分）、S-12、`NamedSetRecord::digest`、`digestMarks` | 權威 Module 的 digest | 第 2 步 |
| 輸入 | `Role`（`State`／`Input`） | 權威的 `.input` 標記 | 第 6 步（Z8 還在用） |
| 交叉 | Z6（System、hook、observer）、Z8、D-E15 與它的對象 `ingress` 旗標（修訂 2，依窄複查 M9：第 2 步只拿掉 `ingress` 的區規則 Z3、S-17、D-E7 的部分，旗標留到第 6 步）、`FunnelWrites` | 權威 Module 的 5.11 交叉檢查 | 第 6 步 |
| 回滾 | `Rollback`（Yes／No／Derived）、`rollbackEnabled`、`notInRollback`、D-E16（`Scheduler.cpp:438-462`）、`rollbackActive`、Replay 的註解；`rollbackSafe` | 回滾 Module 的公開登記與 R-7；`rollbackSafe` 拿掉 | 第 6 步 |
| 指紋 | `CFingerprintOf`、`kCFingerprintVersion`、`FrozenRegistry::CFingerprint()`、`ExportZoneCSources`、`ZoneCSourcesOptions` | 權威 Module（一致性指紋、G-5 匯出） | 第 6 步 |
| 錯誤碼 | `StartupCode` 的 `DE2`、`DE9`、`DE11`、`XE1`（第 2 步）；`DE15` → 權威、`DE16` → 回滾（第 6 步，字串碼） | 列舉值等於統合 3.1 的清單 | 第 2、6 步 |
| 指紋格式 | Registry 指紋去掉 16 個欄位、`annotatedWrites` 收元件；Plan export 去掉 `registry_c`、`rollback=`、`zone=`；Plan 指紋不變（`recon-a` 1.4） | golden 重產：第 2 步一次、第 6 步一次（修訂 1，檢查 S3） | 第 2、6 步 |
| `world_time` | `WorldTimeModule.cpp:79`、`:100-109` 的 `.zone = Zone::B` 與 G4 的註解 | 拿掉 | 第 2 步 |
| 具名集合 | `SetRead<…>::ColumnView::ValueBytesAt`（`Params.hpp:748-750`）：值交成原始位元組 | **留**（本卡照 L17 判）：具名集合是 Kernel 的一般機制；`DigestSet` 拿掉後沒有用它做比較的 Feature。第一個用它比較值的 Feature 出現時改走逐型別雜湊（範圍的限界） | — |

### 7.2 先後：沒有新家的先拿，有新家的等接手測試（修訂 1，檢查 S3）

統合第 8 節的原則是「不需要新家的先拿，要新家的和新家同一條線」。修訂 0 把全部壓到最後，會讓權威與回滾在還有寫者數量與區規則的 Kernel 上開發（例：DI-A 8「不數寫者」的測試——兩個 System 寫 `Health`——在 Kernel 還有 D-E2 時過不了）。改成：
- **第 2 步（做兩個 Module 之前）**：拿掉沒有新家的——寫者數量、區規則、`DigestSet`、D-E9、X-2（9.1 A1、A2、A3、A10）。
- **第 6 步（第 4、5 步的接手測試寫好之後）**：拿掉有新家的——Z6、Z8、D-E15、D-E16、`Rollback`、`Role`、C 指紋、G-5（A4～A7）。這段期間 Z6 等規則在 Kernel 與新 Module 各有一份；Kernel 那份的對象寫法要配合第 2 步拿掉 `Zone`（Z6 現在以「zone C 的 System」為對象，第 2 步改成以 `Rollback::Yes` 為準、照舊擋），在第 6 步整份拿掉；D-E15 以 `options.ingress` 為對象（`Registry.cpp:1387-1392`），所以 `ingress` 旗標留到第 6 步和 D-E15 一起拿（修訂 2，依窄複查 M9）。
- 代價：golden 重產兩次（第 2、6 步）。比過：全部壓到最後（golden 一次，但兩個 Module 的測試在舊規則上過不了、最後才發現對不上）。

---

## 8. LIM-7 重看（批次表「LIM-7 隨區規則移出，在權威＋回滾那條線的宣告裡重看」）

**方向已核准**（使用者 2026-11-20，side chat，原話「可以」）：LIM-7 去掉區的措辭；LIM-8 改寫、不解除（3.4）。兩條的條文在合併前核准時再看。

LIM-7 的寫法建立在區上。區拿掉之後，Kernel 的行為是：**生成撞上活著的實體就丟掉並記 `RejectedSpawn`，不論是誰的資料**（F-13，照舊）。改寫（本卡照 L17 判，條文合併前核准時給你看）：
- 限制：生成被活著的實體擋住時照樣丟掉，Kernel 不擋。
- PvP（有權威端）的形式：編號只由權威端給，只在本機存在的實體不和權威共用編號空間（怎麼分開由產品決定）——照舊。
- 回滾這一側（回滾份 7.4 已答）：還原時存活集合與配發器一起退回，重跑的 `Spawn` 拿到和第一次相同的編號；會撞到的只有「本機專用的實體佔了同一個編號」，前一點已排除。
- 解除條件：第一個有權威端的產品證明本機的東西必須和權威共用編號空間。

---

## 9. 守衛、測試、突變

### 9.1 判定的變更（AGENTS §14.4；方向已核准，見下；最終在合併前帶突變結果與檢證者報告再看一次）

**方向的核准**（使用者，2026-11-20，side chat）：A1、A2、A3、A10 照建議刪（原話「照建議就行。」）；含檢查補進 A2 的五案與第 2 步開頭的恆真案掃描（9.2）。A4～A7（交叉檢查、一致性指紋、G-5 清單搬權威；Derived 檢查搬回滾）照建議（原話「可以。」，2026-11-20，side chat）：四組都變成「只在選了那個策略的 World 才查」，緩的面與緊的面照下表。A8（摘要逐型別雜湊；緩的面含「作者的函式漏欄位看不出」）、A9（指紋格式與 golden）、A11（反向守衛突變退役；第 2 步開頭列出失去唯一突變的判定交使用者）照建議（使用者 2026-11-20，side chat，原話「可以」）。A1～A11 的方向到此都已核准；逐組的最終核准在合併前。

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
- **回滾**：回滾份第 9 節 R-1～R-7 的守衛（含 R-2 等價測試與它列的七個突變）；跨 schedule 的訊息兩方向；沒選策略的 World 沒有歷史（R-4）；換 World 歷史不跨世代（R-5）；`BuildNewFirst` 失敗時舊 World 的回滾照常（5.1）；超過深度放棄這次回滾並記診斷；Plan 版本變了歷史作廢（回滾份 7.5）；**入口形狀（5.5，照使用者重新核准的形狀寫案例）**：沒明說進入或退出的生成與刪除照遊戲處理、回滾到生成之前會刪掉預測錯的實體；比對以（實體、元件、tick）為單位，只差一個元件時只寫回那一格；第 5.5 節列的本批「還沒實作」的入口（部分元件的權威值、這份沒帶、明說的進入與退出、事件來源的值）各回「還沒實作」錯誤、不靜默丟掉。（修訂 4 加：超過深度時發 `RollbackAbandoned` 訊息、不寫日誌；重跑的段的父鏈含 `span.rollback`、第一次跑的不含，第 10.4 節）
- **scenario 第 3 段**（`tests/common/scenario/`，ctest `scenario.segment3`，標籤 `cpu`）：

| 步 | 做什麼 | 檢查 |
|---|---|---|
| S3-1 | 一個 Product 兩個 World：Match（伺服器權威、不回滾）、Client（伺服器權威＋官方回滾），Client 多登記 `HitFlash` | 兩邊一致性指紋相同；Client 改 `HitFlash` 的登記指紋不變、改判定登記就不同 |
| S3-2 | Client 先跑 6 步（預測），Match 用不同的輸入跑同樣 6 步 | 兩邊 digest 不同 |
| S3-3 | 把 Match 第 1 步的整份判定值交給 Client 的回滾入口（形狀照第 5.5 節核准的） | Client 重跑 5 步；重跑後 digest 等於 Match；「播腳步聲」只在第一次跑時數到 |
| S3-4 | 同 S3-3，但交的值和歷史相同 | 不回滾（預設模式） |
| S3-5 | 第 3 步刪掉一個實體、第 1 步的權威值要它還活著 | 實體復活（配發器範圍內的 id）、OnAdd 與 `Add<T>` observer 在全部放回之後跑 |
| S3-6 | 選 P2P 佔位策略的 World | 啟動錯誤，訊息寫明還沒實作 |
| S3-7 | 單機 World（不選策略） | 沒有歷史、沒有權威檢查，第 1、2 段照過 |
| S3-8 | 同一份存檔讀兩次，存檔型別含 `float` 與 `std::vector`（作者寫雜湊函式） | 摘要相同 |

`-scenario-cross-engine-check`（修訂 1 分開寫）：S3-1（協議雜湊，replicon、lightyear）、S3-2～S3-5（預測與回滾、`is_in_rollback`、checksum，lightyear）有同技術的對照；S3-6 的 P2P 在 lightyear 有實作（GYO 只佔位）；S3-7 的「不選就沒有」三家都是（不加 Plugin 就沒有）；S3-8 只有 lightyear 的 checksum 可比（它略過沒登記的元件）。

### 9.4 突變（`recon-a` 第 4 節；D184）

**方向已核准**（使用者 2026-11-20，side chat，原話「可以」）：128 個照下面處置（退役 63、搬權威 43、搬回滾 3、改寫 19）。

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
- 完整跑的指令（`run_mutations` 的清單參數或包裝的腳本）加上兩份新清單，在第 3 步接好（I17）。
- 每步只跑相關突變；合併前背景完整跑一次（Kernel＋編排層＋`world_time`＋權威＋回滾＋`zone_scan`）。只有預期的案例失敗才算殺。

### 9.5 綁實作的守衛（`-guards-after-mechanism-stable`）

- 第 3 批延到本批開頭的三條（6 號加時鐘的識別字；`naming_guard` 加 `engine/modules/world_time`；`world_time` 的 `gyo_expect_links`，連結集照 #257 之後的登記列，多 `GYO::Logging`）：條件「scenario 第 2 段通過、合併前核准之後」已成立，**第 1 步立**。時鐘的識別字用限定程式的寫法（`engine/ecs/kernel` 還有 11 處「Kernel 不認得時鐘」的說明註解含 `clock`，`recon-a` 5；修訂 1，檢查小 4）。
- 本批新的（Kernel 的掃描加「拿掉的名字不回來」、`naming_guard` 加 `engine/modules/authority`、`engine/modules/rollback`、兩個 Module 的 `gyo_expect_links`、回滾 R-3 與權威 DI-A 2 的方向掃描）：**延到第 5 批開頭**，條件是 scenario 第 3 段通過、本批合併前核准之後（和第 2、3 批同一個處理）。這段期間靠文件、審查與判定 4 的一次性掃描（含方向的掃描）。

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
| I9 | 兩個 Module 的 `config/modules.csv` 列（順序 `world_time` → `rollback` → `authority`）與目錄骨架（含 `tests/common/{authority,rollback}` 與 `tests/common/CMakeLists.txt` 的兩行） | 第 3 步 | 第 4、5 步 |
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

### 10.3 和異常分級卡的交界（修訂 3 記入；**修訂 4 照合併後的結果重寫**）

異常分級已合併（#254、#255）。合併後以它的宣告第 0.1 節（核心原則）與第 19 節（修訂 7 收窄範圍）為準：錯誤是資訊，告訴外面發生了什麼、不替外面決定怎麼處理；E0～E3 級別與命名規則留下；D3（E1／E2 拆型別與既有型別改名）、D5（沒讀的 `Result` 變編譯錯誤）、D8、D9、17.1、17.2 撤回；D4 改回往外拋。修訂 3 照它當時的動工前版本寫，下面逐條改正。

- **級別**：照修訂 3 的表不變（它是資訊）——`Check` 時報的 `AU-E…`／`RB-E…`、「還沒實作」的 `Result`、`Summary` 缺雜湊函式（第 3.3 節）、P2P 佔位、`Tick` 寫者檢查＝E2；凍結後寫入、`Create<T>` 重複、`Find<T>` 在 `Build` 找不到依賴（第 6 節）＝E0；回滾超過深度＝E3；重複登記「作者蓋過代為」＝非錯誤、不記。
- **型別名**（修訂 4 改正）：既有型別**不改名**——Kernel 照舊 `KernelError`、編排層 `OrchestrationError`、`WorldFailure`（修訂 3 寫的 `KernelFailure`、`OrchestrationFailure`、`Base::Failure<Code>`、`WorldError` 是 D3 的名字，已撤回）。命名規則（`…Error`＝E1、`…Failure`＝E2）只用在**本批新增的型別**。本批預計不新增錯誤型別：第 3.3 節 `Summary` 缺雜湊函式回 `KernelError` 的新碼，權威與回滾的 `Check` 報告用編排層既有的報告形狀加 `AU-E…`／`RB-E…` 碼；真的要新型別時照後綴取名，在合併前核准給使用者看。
- **不在回傳之外再記一筆**（它的 D1、D2）：本批的 Module 回錯誤時不另寫 `Warning`／`Log`，處理由上層決定。
- **E3 的告知方式**（修訂 4，本卡照 L17 判：照 `world_time` 的 `TimeDiscarded`＋`TimeDiscardedLog`）：回滾超過深度時放棄這次回滾，在 World 裡發一則訊息（暫名 `RollbackAbandoned`，帶請求的 tick、確認點、深度），不直接寫日誌；要寫成日誌的產品用 `WorldLogging::Forward` 自己接。第 12 節末第 7 條的「記診斷」就是這則訊息。
- **上游丟例外**（修訂 4 改正）：它的 17.2「`Build` 接例外、之後不再呼叫」已撤回，`Product::Build` 裡丟出的例外照常往外到 `main`，沒有 Product。所以第 6 節「Module 在 `Build` 找不到依賴：斷言」仍不會因上游丟例外被觸發，結論不變。
- **同檔**：它已合併，rebase 時沒有衝突（`naming_guard.py` 的衝突來自 #257，第 19 節）。

### 10.4 和 trace 的交界（修訂 4 新增；trace 合併前核准留給本批）

trace（#258）合併時留下一件給本批：回滾重跑時開出來的段，和第一次跑的分不出來（[trace-merge-approval](trace-merge-approval.zh-Hant.md)第 79 行、trace 宣告第 7 節）。現況：Kernel 的 `RunPlan` 每張 schedule 開一段（含巢狀跑）、每步的 System 一段（TR-4）；回滾的獨占 System 巢狀跑 `RunOneFixedStep` 時，那些段的父是回滾 System 的段，名字和第一次跑的相同。

**本批處理**（本卡照 L17 判；trace 宣告第 7 節舉的例子就是這個做法）：回滾 Module 在每次重跑一步時經 Base 的門面開一段（`TraceSpan`，category `rollback`、name `resimulate`），把那一步的巢狀跑包起來；第一次跑沒有這一層。讀紀錄的人照父鏈分辨：祖先裡有 `span.rollback` 的是重跑。

- 只用 Base 的門面，`rollback` 不依賴 trace Module（TR-1）；不讀不寫 World 的值（TR-2），等價測試（R-2）與判定 4 不受影響。
- tick 不放進段名（段名是固定字串，放 tick 要在熱路徑上組字串）；要知道哪一步，照段的先後與 `RollbackAbandoned`／歷史對照。之後有 Feature 需要時再看 TR-5 的欄位。
- 測試：第 9.3 節加一案——重跑的段的父鏈含 `span.rollback`、第一次跑的不含；突變一個（拿掉這一段時那一案失敗）。
- 段數：每次回滾多 k 段（k＝重跑的步數），在 trace 宣告第 5.2 節的估計（×（1＋k））之內。判定 5 的量測不裝 trace 接收者（沒裝時只讀一個原子指標，TR-6），所以門檻不變。

---

---

## 11. Design Invariants 的變更（方向已核准，見下；最終文字在合併前核准）

**方向已核准**（使用者 2026-11-20，本卡，原話「DI 文字方向照建議」）：11.1～11.5 照下面的方向；最終文字在合併前核准。

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

回滾份第 9 節 R-1～R-7 與 RC 照搬；R-7 的守衛「Kernel 現有的三處 D-E16 案搬來」照 9.1 A7；加一句：公開登記凍結後放進 World 的 Resource（5.1）；加入口形狀（5.5；使用者 2026-11-20 重新核准，條文照核准的形狀寫）：比對與還原以（實體、元件、tick）為單位、事件來源的權威值可寫進歷史（作者選），以及使用者核准的回滾點規則（MC-5）與 GYO 特有的形狀（MC-1、MC-3、SL-1、SL-2）；RC 加「記錄改成一般 System 才能並行時（獨占 System 成為並行的牆）」（15.4）。

### 11.4 時間 Module（`target/v2/world-time.zh-Hant.md`）

- WT-1：補「`Resimulation` 每次準備都放成 false，`CarryOver` 不帶它」。
- WT-5：補「伺服器權威策略檢查 `TickWriters()` 名單外的寫者（權威 5.4）」；第 3 批的 G4 註記刪。

### 11.5 編排層（`target/v2/orchestration.zh-Hant.md`；第 12 節第 3 件定 (a)）

- **O-2** 補「每次建 World 有一份以型別為鍵的共用槽。槽裡每種型別只由擁有它的登記單位建（`Create<T>`），別人用 `Find<T>`（找不到回空、不代建）；先後由登記檔的依賴與 O-E3 保證，產品自己的 `build` 在最後。登記單位與產品自己的 `build` 在 `Build` 拿 `T&`，`Build` 階段結束時編排層凍結槽，之後 `Finish`、`Check` 只拿 `const T&`。`Build` 階段的寫入只追加、帶來源；進 Kernel 的副作用在登記當下做、不取決於裁決；裁決是擁有者對紀錄的純函式，在凍結後經擁有者的讀取介面取得，結果和呼叫順序無關。編排層不解讀槽裡的型別。照 CCR-5 在一條執行緒上，槽不加鎖。槽活到那個 World 的登記單位的準備函式跑完，要留到執行時的內容由 Module 在準備函式裡凍結進 World 的 Resource」（「Create／Find」「凍結」是暫名，實作時照命名規範定；「準備函式跑完」「凍結進 Resource」「裁決是純函式」是本卡的文字，修訂 2 依窄複查 M5、M6、S3、S6 改寫）。錯誤的形式：Module 的 `Build` 或產品自己的 `build` 找不到依賴的登記、兩個登記單位 `Create` 同一型別、凍結後呼叫 `Create` 或可寫的取用，都是 Programmer Error（斷言）。守衛：`Finish`／`Check` 拿到的是 `const`（編譯期，`requires` 檢查寫入不成立）；凍結後 `Create` 的斷言；`Find` 找不到回空、不代建；同一組登記兩種呼叫順序裁決結果相同；兩筆作者 options 不同報錯；準備函式讀得到槽的內容。（(b) 的補句不採。）
- **RC-O10**（新）：CCR-5 放寬時（建 World 要並行，或一個 World 的登記單位並行 `Build`），共用槽需要加鎖或改成別的同步方式。
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

- 判定與 DI：方向在動工前核准；逐組的最終核准在合併前帶突變結果與檢證者（Fable 5.2／high）報告再看一次（和第 2、3 批同一個做法）。A1～A3、A10 是已定決定的直接後果，**方向已核准（照建議刪，2026-11-20）**；A4～A7（搬過去時判定的對象變了、A7 的緩的面）**方向已核准（照建議，2026-11-20）**；A8（作者的函式漏欄位）、A11（反向守衛退役後可能沒有突變的判定，第 2 步開頭列給你）**方向已核准（照建議，2026-11-20）**；A9、9.4 的突變處置、LIM-7／LIM-8 的方向也照建議核准。
- （修訂 1 時寫「規模 1,000 實體 × 4 種是本卡的假設」；修訂 2 由 5.4.5 取代。）
- 拆卡：**已定不拆**（使用者 2026-11-20，第 10 節）。
- 規模與預算：另開唯讀研究卡（使用者 2026-11-20）；修訂 2 照研究卡 v2 重推，重新交使用者（本節開頭 A）。

### 重新核准（修訂 2 交出，2026-11-20 使用者定；修訂 3 記入）

**決定**（使用者 2026-11-20，主會話，`v8-arch-gyo2-authority-rollback-reapproval`）：原話「第 4 批 A、B 照建議，門檻 25%」。A、B 照下面本卡的建議定；424 B 准用；新加的事前宣告「分母兩列的四分位距比任一超過 25% 就不判定」（5.4.5）。門檻 3 的統計量另由研究卡 v2 轉：維持 p99（`-ratio-p99`，5.4 開頭）。C 已由研究卡 v2 做完（改包並在 Linux 實機跑，5.4.5）。比過的選項、理由見 5.4、5.5 開頭。下面保留交出時的寫法。

照使用者「如果任務卡那邊用了錯誤的推論，重來」，下面兩件原本已核准（使用者 2026-11-20「預算照研究卡推薦，入口形狀寫進 Contract」），因為出自第一張研究卡的錯誤推論，重新交使用者：

A. **5.4 的數字**（推翻已核准的）：P1 16 KB→**32 KB**（深度 20）、P2 64 KB→**128 KB**（深度 32）、P3 1 KB→**2 KB**；門檻 3「P2÷P1 ≤ 6」→「≤ 1.5 × 同一次量測的機器底比值」（分子分母同大小、同深度、同統計量，5.4.5），並加「同一次跑機器底」；Module 側的配置寫死（16 種元件 × 64 B，P1 32 個實體、P2 128 個、P3 2 個）。根據：每人 424 B 的實測取代 168 B 的推估；快取層級。門檻 1、2、量法、深度預設 20 照舊（深度的根據換成區網的往返延遲）。本卡建議照改。
   - **請確認的一點**（窄複查 M4）：424 B 出自 `pvp_baseline`（使用者的 `-no-master-reference` 點名不用；之後 `-budget-baseline-research` 把 11-01 之後的數據開放成基準）。本卡把它當「估狀態大小」（同使用者准的看 protobuf）。不准用時退回 168 B 的推估：角色 6.6 KB＋事件類 10 KB ≈ 17 KB，進位後 P1 仍是 32 KB。另外 AI、Boss 照玩家的 424 B 算是假設。
B. **5.5 的形狀**：(1) MC-5：入口帶（實體、元件、tick）的權威值與「第 C 步以前收齊」的標記，官方實作的「判斷」用 (a) lightyear 的全域確認點，作者覆寫「判斷」可以做 (b) Unity 可選（取最早）、(c) Unity 預設（只回滾收到新值的）——不新增掛點（回滾份 5.3 已有）；本卡與研究卡 v2 一致建議這樣定。選 (a) 時再寫清楚四件（收齊只算 snapshot 通道、tick 大於 C 的值留著、「這份沒帶」不推成沒變、全掃比對的成本只記錄）。(2) GYO 特有的要不要留：三種權威值＋由呼叫者明說的有效期間（MC-3、SL-1、SL-2；本卡與研究卡都建議留）、MC-1「事件不可靜默丟」（研究卡建議留；本卡建議改寫成 lightyear 的「帶到確認點」、拿掉來源種類）。
C. **Linux 實機的交付包**（使用者的操作，不是決定）：主 checkout `build/evidence/gyo2-authority-rollback/baseline-v2/bundle/run-linux.sh`，約 25 分；量到之前「以前的回滾成本」與機器底是空的，判定 5 的量測結果標「未量」。**現在這個包量不出判定 5 第 3 條的分母**（沒有 32／128 KiB、`hot_record` 只在深度 20），去跑之前要先改包（5.4.5 末段）；誰改（研究卡 v2 或本卡）請主會話或使用者指定。

以下是修訂 1 時的狀態，留作經過：


1. ~~第 5.5 節的入口形狀~~ **已定：寫進 Contract**（2026-11-20）。原題：MC-3、MC-5、SL-1、SL-2 合成「實體的有效期間」並和 MC-1、MC-2（機制）、MC-4 一起寫進回滾的 Contract（本卡建議要；改回滾份 5.5 第 2～5 點字面）。
2. ~~第 5.4 節的數字~~ **已定：照研究卡推薦**（2026-11-20）。原題：研究卡的推薦（5.4.6：P1 16 KB／20 步判定、P2／P3 只記、三條門檻含新增的線性比值、Linux 實機原生、預設深度 20）要不要採（本卡建議採）。
3. ~~第 11 節 DI 文字的方向~~ **已定：照建議**（2026-11-20，本卡）。

### 使用者的界線：不參考 master 的算法與數據

**不參考 master 的算法與數據**（使用者 2026-11-20 定，side chat）：權威與回滾 Module 從頭照 GYO2 的新架構與開源參考（lightyear、bevy_replicon、Unity Netcode、Bevy、flecs）做；不用 master 的算法、常數與量測數據（含 `LocalPlayerPrediction`、`AuthorityDigest` 的情境、`pvp_baseline` 的數字）。2026-09-25 的 dev_log 是 10/10 事故前的數據，不准引用。side chat 提過的「用 master 數據改 5.4 的規模與預算、修正照 master 的 0.1 秒、digest 情境照 `AuthorityDigest`、估計下修」全部不採。原話：「算了，重做吧，我怕你把master的問題帶到新架構裡來。（特別你還踩空了09_25 的dev_log，那個是10/10事故前的數據。不准）」本頁修訂 1 沒有引用 master（`git grep -i -E "master|09_25|pvp_baseline|LocalPlayerPrediction|AuthorityDigest"` 0 命中，同一指令以 `lightyear` 正例命中 25 行）。5.4 的規模與預算另開唯讀研究卡：master 2026-11-01 之後的數據只當**基準**（以前到哪、現在要到哪），不拿它的算法、常數當設計的根據；數字由使用者定（第 5.4 節，使用者 2026-11-20 補）。（修訂 2 註：上面「本頁修訂 1 沒有引用 master、0 命中」是修訂 1 當時的事；之後 `-budget-baseline-research`、`-baseline-redo` 把 11-01 之後的數據開放成基準，5.4.1 起引用 master 的檔與 `pvp-baseline-report`，界線見 5.4 開頭與第 12 節 A。）

### 本卡照 L17 判的（各家一致或細節；不逐條問）


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
7. （修訂 2，窄複查 S2；第 5.5 節 2026-11-20 重新核准，**生效**）回滾份 5.5 第 3 點「第 T 步的權威值」→（實體、元件、tick）的權威值＋確認點；第 4 點「超過最大深度的請求丟掉」→「放棄這次回滾、記診斷」；第 5 點「存活集合整份放回」→ 有效期間內的放回；5.3 的註「入口不在 interface 裡，官方策略定義自己的入口」→ 回滾 Module 層、所有策略共用的入口。
8. （修訂 2，窄複查 S2）權威份 4.3「作者先登記，策略就不代登記」→ 一律追加、凍結後裁決（第 6 節第 3 點；結果相同：作者的為準）。

---

## 13. 判定（先宣告，結果出來之後不放寬）

- **判定 1：通過**。本機完整 ctest 全過（含 `scenario.segment1`～`segment3`）；L1 三平台（linux-x64、windows-x64、macos-arm64）全過，含 toolchain windows-x64 兩次建置的 `orchestration.restore`（`-summary-padding-a-then-c` 的來源）；`git grep -n -i "summary-padding-a-then-c" -- tests engine` **0 命中**（標註全部拿掉；先在 59fd4da9 命中 14 行當正例，終了碼區分「沒有」與「失敗」）。
- **判定 2：行為**。第 9.3 節的案例全過；S3-1～S3-8 全過。
- **判定 3：檢出力**。三份新突變清單經 Fable 5.2／high 檢查補齊後全殺；第 9.4 節的 128 個照處置後，留下與搬走的全殺；完整跑結果記進 dev_log，活著的逐個說明；分母以執行時的實際清單為準。
- **判定 4：一次性的掃描**（`git grep -n -P`，`engine/ecs/kernel`、`engine/orchestration`、`engine/modules/world_time`（修訂 4：#257 的路徑；目錄與樣式不變）；每個樣式先在動工時的 `gyo2`（f25837b0）命中一次當正例——正例都在 `engine/ecs/kernel`，`engine/modules/world_time` 只中第一條——終了碼區分「沒有」與「失敗」）。
  - 照統合附錄的：`\b(Zone|WritePolicy|Rollback)::`、`\bRole::Input\b`、`\bRequireExplicitZones\b`、`\bemitterZone\b`、`\brollbackEnabled\b`、`\bnotInRollback\b`、`\brollbackSafe\b`、`\bingress(Stage)?\b`、`\bDigestSet\b`、`\bCFingerprint\b`、`\bExportZoneCSources\b`、`\bparallelGuardTest\b`。
  - 本卡加的（修訂 1 補，檢查 S1）：`\bRole::State\b`、`\brollbackActive\b`、`\bFunnelWrites\b`、`\bDE(2|9|11|15|16)\b`、`\bXE1\b`、`\bzone\b`、`\bwritePolicy\b`、`\bCFingerprintOf\b`、`\bkCFingerprintVersion\b`、`\bZoneCSourcesOptions\b`、`\bMarkDigestSet\b`、`\bdigestMarks\b`、`\bZoneName\b`、`\bexternal\b`、`"Z[0-9]"`、`\bRollback::Derived\b`、`enum class (Zone|WritePolicy|Role|Rollback)\b`。
  - 以上全部零命中；`StartupCode` 的列舉值等於統合 3.1 的清單（`RegistryTests.cpp:223`）。
  - **方向**：三個目錄裡沒有 `engine/authority/`、`engine/rollback/` 的 include（含入路徑不隨 #257 改），也沒有 `Authority::`、`Rollback::` 的名字（R-3、DI-A 2 的常設守衛延到第 5 批，這段期間由這條守）。
- **判定 5：預算**（第 5.4.5 節；使用者 2026-11-20 重新核准；回滾 Module 的量測結果：未量）：在 P1（可回滾狀態 32 KB、深度 20）判 (1) 記錄每步（每步所有格都改）≤ 156 µs；(2) 一次回滾總開銷（退 20 步、全部格都變；還原＋20 次記錄＋`ActiveTime`／`Resimulation` 起落，不含作者的 System）≤ 3.125 ms；(3) 同一次量測裡，(P2（128 KB、深度 32）的記錄每步 ÷ P1 的記錄每步) ≤ 1.5 × (機器底在同樣兩個大小、配置 `colsT16s64` 的記錄比值)（p99；基準值：分母 4.160、上限 6.240；判定用同一次的分母）；分母兩列的四分位距比任一超過 25% 就不判定第 3 條、停下回報。P2、P3（2 KB、深度 20）其餘只記錄。量法：使用者的 Linux 實機原生，條件和基準那次相同（關桌面程式、governor `powersave`），`taskset`、先暖機並記頻率，10 個行程 × 各 1,000 次，取每行程 p99 的中位數判定，四分位距比與平均一起記；每次判定在同一次跑機器底（`floor_bench`）；Intel Mac 只當開發參考。量之前先講影響、取得使用者同意（`warn-before-heavy-local-runs`）。`World::Summary` 不設預算（除錯與測試用），dev_log 記改前改後的時間。超過就停下回報。修訂 1 的數字（P1 16 KB、P2 64 KB、P3 1 KB、比值 ≤ 6）出自第一張研究卡的推估，撤回。

---

## 14. 誰做、用什麼、怎麼分步

上段（公開介面、Design Invariants、資料形式）。全部 high、不用 xhigh（`v8-arch-gyo2-batch3-no-xhigh`；碰到 §14 停止或原因不明的失敗，停下回報、提議 xhigh，不自己升檔）。

| 步 | 內容 | 實作者（模型／檔位） | 估計（agent 時間） |
|---|---|---|---|
| 第 1 步 | 第 3 批延下來的三條守衛；逐型別雜湊（I1）與摘要格式 3；9 處「未完全解決」解除；比摘要的現有測試的型別補函式（第 12 節第 1 件定 (d)：不比摘要的測試型別不必補） | Opus 5.5／high | 8～12 h |
| 第 2 步 | A11 的清單交使用者；拿掉沒有新家的規則（A1、A2、A3、A10；9.2 的恆真案掃描）、golden 第一次重產；原語 1～4 與讀 stamp（I2）、`Writes` 收元件（I3）、訊息寫者名單（I4）、`Resimulation` 每次放 false | Opus 5.5／high | 16～22 h |
| 第 3 步 | 第 6 節的接法（I5）；`engine/modules/authority`、`engine/modules/rollback` 骨架、`config/modules.csv`、I6 回滾的公開登記與讀取介面、I7 的標頭、I15 測試支援、I17 | Opus 5.5／high | 8～12 h |
| 第 4 步 | 回滾：檢查（R-7 Derived、跨 schedule 訊息、巢狀 schedule 的所屬）；官方策略（記錄、判斷、還原、重跑、修正、輸入歷史；R-1、R-2、R-5）、P2P 佔位；入口形狀（5.5，照 2026-11-20 重新核准的形狀）；新頁 `target/v2/rollback.zh-Hant.md`；預算量測（5.4.5）：回滾 Module 的量測程式與離線腳本（一台一支、交出前每條命令實際跑過，和機器底同一次跑；量測要使用者在 Linux 實機跑） | Opus 5.5／high | 22～34 h＋入口形狀 6～10 h＋量測程式與腳本 4～6 h |
| 第 5 步 | 權威（權威份第 10 節步 3、4）：登記入口、代為登記、策略、佔位、tick 檢查；一致性指紋、digest、G-5 匯出與 `zone_scan` 的接手（J-17 的搬移）、5.11 交叉檢查；新頁 `target/v2/authority.zh-Hant.md` | Opus 5.5／high | 12～20 h |
| 第 6 步 | 拿掉有新家的規則（A4～A7）、`Role`、C 指紋、golden 第二次重產；突變的處置（9.4） | Opus 5.5／high | 8～12 h |
| 第 7 步 | scenario 第 3 段；Kernel 的 DI、限制頁、world-time、編排層、索引；dev_log | Opus 5.5／high | 5～8 h |
| 第 8 步 | 完整突變、三份新清單的敵對式檢查、§14.4 檢證、審查 | 見下 | 8～12 h |

合計 97～148 h（含量測程式與腳本 4～6 h、5.5 入口形狀 6～10 h；修訂 1 估入口 4～6 h，修訂 3 照核准的形狀重估；不拆後串著做，關鍵路徑＝合計；少了交接與併分支的費用沒有另外扣）。超過估計 1.5 倍或單一問題累計超過 2 小時就停下回報。

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
| 規模：64 Hz；可回滾狀態上限約 27 KB（39 角色 × 424 B（玩家實測；AI、Boss 同大小是假設）＋事件類＋私人，5.4.3）；深度預設 20（區網的往返延遲＋lightyear 預設，5.4.4）；判定點 P1 32 KB／20、P2 128 KB／32、P3 2 KB／20（使用者 2026-11-20 重新核准） | 判定 5 |

### 15.3 Architecture Delta（七項；和統合第 4 節、兩份第 9 節的差分）

1. **Feature**：第 15.2 節第 1 列。
2. **現在的問題**：區、寫者、digest、C 指紋、Z6／Z8／D-E15、D-E16、`Rollback` 列舉都還在 Kernel（`recon-a` 1），Kernel 對單機也執行這些檢查；回滾要的原語 1～4 不在；摘要照原始位元組雜湊（LIM-8）；Module 之間、作者與 Module 之間沒有通道。
3. **變化的邊界**：新 Subsystem `engine/modules/authority`、`engine/modules/rollback`；Kernel 少了區、寫者、digest、回滾的宣告與 C 指紋，多了原語 1～4、讀 stamp、雜湊的客製點、訊息寫者名單，`Writes<…>` 收元件；編排層多了每次建 World 一份的共用槽（第 12 節第 3 件定 (a)）。
4. **影響**：Kernel（第 7 節的檔案）、編排層（`Setup.hpp`、`Product.cpp`）、`world_time`（區參數、`Resimulation`）、`config/modules.csv`、`tests/common/{kernel,orchestration,world_time,scenario,zone_scan}`、新 `tests/common/{authority,rollback}`、`tests/common/CMakeLists.txt`、Kernel 與編排層的突變清單、新兩份清單；文件見第 11 節、限制頁與索引。
5. **依賴方向**：新增 權威→Kernel、編排層、`world_time`、回滾；回滾→Kernel、編排層、`world_time`；產品→權威、回滾。Kernel、編排層、`world_time` 不依賴兩者。沒有反向。
6. **Ownership**：區、寫者數量、判定資料的讀寫：Kernel → 不由機制守；digest、一致性指紋、G-5、Z6／Z8／D-E15、tick 的單一來源：Kernel → 權威 Module；`Rollback`、D-E16、「重跑時不跑」：Kernel → 回滾 Module；摘要的雜湊：照原始位元組 → 每個型別的函式（型別的作者）。
7. **為什麼不能更小**：只拿掉 Kernel 的規則不給新家，伺服器權威與回滾沒有任何機制（統合第 8 節：守衛不留空窗）；只建新家不拿掉，Kernel 對單機也執行權威規則，違反 `-kernel-is-ecs-no-restrictions`；摘要只補 padding 是已做過的 A，使用者定正式解法是 C；沒有接法，作者用不到兩個 Module。

### 15.4 系譜、退掉的選項、需要它的 Feature（AGENTS §4）

| 決定 | 系譜 | 退掉的 | 決定的理由 | 需要它的 Feature |
|---|---|---|---|---|
| 雜湊函式跟著型別、摘要與 digest 共用 | lightyear 逐元件登記 | 摘要與 digest 各一套；只補 padding（已做的 A） | 一個概念一個實作；使用者定 C | 存檔（讀兩次相同）、PvP digest |
| 照位元組不自動、作者明寫才用 | — | `has_unique_object_representations` 為真就自動（修訂 0） | 自動那條有 `optional`、指標、平台寬度三個漏洞（檢查者實測） | 存檔、PvP digest |
| 沒有函式時（第 12 節第 1 件） | flecs 呼叫時擋／lightyear 略過 | (a) 登記時編譯失敗、(b) 略過、(c) 退回位元組 | 使用者定 (d)（flecs 的形），2026-11-20 | 存檔、單機不增加複雜度 |
| 浮點（第 12 節第 2 件） | lightyear、flecs、Unity 逐位元 | (ii) `±0` 相同、NaN 都相同（Bevy `FloatOrd`） | 使用者定 (i) 逐位元＋作者自己正規化的出口，2026-11-20 | PvP digest、回滾 |
| 原語有型別、在 `ExclusiveWorld`、一批放回 | Bevy；統合 9.1；同步點的 hook 規則 | 型別擦除的整欄複製（Unity 的原始位元組）；逐欄放回、每欄當場跑 hook | 回滾的登記知道 `T`；hook 不看到半還原的 World | PvP Client 回滾、P2P（佔位） |
| G5 由回滾存版本、記錄是獨占 System | Unity Netcode `DidChange(版本)` | Kernel 加 tick→stamp 原語；每型別一個 `Changed<T>` System（lightyear）；**照 Bevy `Ref<T>` 把 stamp 開給一般的 query**（Kernel 的參數多一種；v1 只有 Serial，獨占 System 沒有並行的代價；之後要並行時這是一道牆，寫進回滾的 RC） | 三家都沒有 Kernel 層的對照；一個 System 處理全部型別 | PvP Client 回滾 |
| 重跑掛每幀 schedule 的 `PreUpdate` | lightyear `PreUpdate` 的 `run_rollback` | 掛在步的 schedule（X-11 不准） | 巢狀規則 | PvP Client 回滾 |
| 回滾的登記凍結後放 World 的 Resource | Unity 的 World singleton | Module 物件以 World 名為鍵的表 | `BuildNewFirst` 失敗時舊 World 要照常 | PvP Client 回滾、讀檔 |
| digest 相加、不含 id | lightyear 的 XOR、不含 id | XOR；含 id（修訂 0） | 相同的兩項不抵銷；id 的對映屬網路 Module | PvP digest |
| Module 與作者的接法（第 12 節第 3 件） | lightyear App Resource／Bevy `get_added_plugins` | (b) 查依賴的 Module 物件、(c) 產品經選項包接線、(d) static 表 | 使用者定 (a) 編排層的共用槽（lightyear App Resource 的形），2026-11-20 | PvP（權威＋回滾）、單機時間倒流 |
| 兩個 Module `default=yes` | Bevy `DefaultPlugins`、Unity 內建模組（主會話的判斷） | `default=no`（修訂 0） | 照主會話已定的預設；不選策略就沒有作用 | 單機（不選策略） |
| 沒有新家的規則先拿 | 統合第 8 節的原則 | 全部壓到最後（修訂 0） | 兩個 Module 不在舊規則上開發 | — |

既有的 Test 與守衛能不能不改，**沒有**當作任何一項的決定理由。

---

## 16. 範圍的限界

- 這是宣告，沒有試做；第 14 節的時間是讀程式量估的。
- 行號照三份偵察（59fd4da9），卡片與檢查者抽查了一部分（檢查報告末節）；`recon-b` 2.1「第 1 批已補上」只對了條文，逐欄對程式在第 2 步開頭做；Unity 讀的是非官方鏡像；bevy_ggrs、GGPO 本機沒有。
- 規模與預算（5.4、判定 5）：修訂 2 照研究卡 v2 重推，使用者 2026-11-20 重新核准；「以前」與機器底已在 Linux 實機量（基準值），回滾 Module 本身未量；不拿 master 的算法、常數當設計的根據。
- 不參考 master 的算法、常數與量測數據；2026-09-25 的 dev_log（10/10 事故前）不准引用（使用者 2026-11-20）。實作也照這條：參考只用 GYO2 的新架構與開源參考。
- 網路 Module 的設計不在本批：digest 何時比、含不含實體 id、伺服器值怎麼搬、對時、tick 跳變的通知（`LocalTimelineShift` 那一種）。本批只做到回滾 Module 退 tick 時用 `RewindFixedSteps`；**tick 跳變的推送式事件不做**：以 tick 為索引的歷史只有回滾 Module 自己的，退回時它自己處理；別的讀者出現時（網路 Module 的輸入緩衝）再加（AGENTS §2）。
- digest 不含實體 id：兩個實體交換判定值時看不出來（4.4）。
- 具名集合的原始位元組介面留著（7.1），沒有用它做比較的 Feature。
- 狀態機的 (c) 區接法（權威份 5.8）只做到 `Writes<…>` 收元件與 `authority.Runs`、`authority.MarkResource`；狀態機本身在第 5 批。
- 「誰實際寫的」除錯工具（L30）不在本批。
- 修訂 1 之後的改動與修訂 2 經 Fable 5.2／high 窄複查一次（第 17 節末）；修訂 2 依指摘改完後、修訂 3 記入使用者的決定與基準值，都沒有再送檢查（修訂 3 不改設計，只記決定、數字與交界）。

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
| M9 A 依賴 R | 回滾的公開登記與讀取介面在第 3 步做；I6、估計跟著改 |
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

## 18. 修訂 1 之後與修訂 2 的窄複查的處理（`declaration-r2-delta-adversarial-check.md`）

Fable 5.2／high，唯讀；範圍 fcae13f1 之後的全部差分（含修訂 2）與研究卡 v2 的出處、推算。結果：必改 9、建議改 10、小問題 9（報告：主 checkout `build/evidence/gyo2-authority-rollback/declaration-r2-delta-adversarial-check.md`）。查過、成立的：master 的出處與日期、參考引擎的引用行、算術、(a) 不需要 Kernel 新原語、使用者的決定與 HANDOFF 一致、「卡 A／卡 R」0 命中。

| 指摘 | 處理 |
|---|---|
| M1 門檻 3 的分母量不出來、定義不唯一 | 5.4.5 第 3 條寫死分子分母（同大小、同深度、hot、不 batch、10 行程 p99 的中位數後相除）；交付包要先改（加 2／32／128 KiB、兩個深度）；第 12 節 C 註明 |
| M2 Module 側的配置沒定 | 5.4.5 寫死 16 種元件 × 64 B、P1 32 個實體／P2 128／P3 2；「每步所有格都改」在計時外做；門檻 2 的扣除法寫明；另列「實際組成」只記錄 |
| M3 「時鐘同步延遲 p95」標錯 | 5.4.1、5.4.4、5.3、15.2、第 12 節 A 改成視窗最小值／最大值的寫法 |
| M4 424 B 是 `pvp_baseline` | 5.4.1 標明；第 12 節 A 加「請確認」與退路（168 B 時 P1 仍 32 KB）；AI、Boss 的大小標成假設 |
| M5 裁決沒有地方放 | 第 6 節第 3 點：裁決是擁有者對紀錄的純函式，經 `const` 的讀取介面取得；O-2 補句同改 |
| M6 「登記當下只記下一筆」和 DI-A 11 衝突 | 第 6 節第 3 點：進 Kernel 的副作用當下做、不取決於裁決；延後的只有 options 誰為準；「重跑時不跑」不在裁決範圍 |
| M7 MC-5 的建議沒對上回滾份 5.3 | 5.5 改成：入口帶（實體、元件、tick）的值與收齊標記；官方「判斷」用 (a)；作者覆寫「判斷」做 (b)、(c)；不新增掛點；入口是 Module 層共用的（列字面不同） |
| M8 差分規則搬錯、驗不了 | MC-4 分開：比對單位留在回滾；「確認點不得晚於還沒解開的差分」改成給網路 Module 的需求；9.3、11.3 拿掉 |
| M9 D-E15 第 2～5 步沒有對象 | `ingress` 旗標留到第 6 步和 D-E15 一起拿（7.1、7.2） |
| S1 選 (a) 還缺四件 | 5.5 第 1 件補四件（收齊只算 snapshot、tick 大於 C 的留著、「這份沒帶」不推成沒變、全掃比對只記錄）；5.4.4 加確認點的落後 |
| S2 字面不同沒列 | 第 12 節末加第 7、8 項；文首改成「除第 12 節末列的之外不改」 |
| S3 槽的生命週期 | 第 6 節與 O-2：槽活到準備函式跑完 |
| S4 錯誤的形式 | 第 6 節與 O-2：找不到回空、`Build` 中找不到依賴與重複 `Create`、凍結後 `Create` 都是斷言 |
| S5 Module 自己的 Resource 與 (d) | 3.3 加「Engine 的 Module 放進 World 的型別都附雜湊函式」與各雜湊什麼；「有值才報」標成本卡 |
| S6 引用 CCR-5 | 第 6 節第 4 點、O-2、RC-O10 |
| S7 0.0067 ms 的標籤 | 改成「一個玩家的移動一步」 |
| S8 策略的狀態 | 4.2：策略物件不帶每個 World 的狀態，狀態放 World 的 Resource |
| S9 量測怎麼交到 Linux 實機 | 第 14 節第 4 步加量測程式與離線腳本 4～6 h；5.4.5 末段寫明 |
| S10 例子預設了 (a) | 第 1 節、S3-3 改成整份判定值；做薄段標「選 (a) 時」 |
| m1～m8 | 第 12 節第 5 件、第 16 節、9.4／第 17 節的「統合」、9.1／第 11 節的標題、15.4 的欄、L17 段的標題與 0 命中的註、文首與第 0 節、P2 的「只記錄」與「同 P2P 佔位」 |
| m9（研究卡 v2 的小錯） | 宣告用 256 倍、Mac 原生 4.5～13.8%；`ICommandData.cs:169` 不再寫成歷史環上限（宣告只引 `NetworkTimeSystem.cs`）；交付包的量測點照 M1 改 |

## 19. 修訂 4：照 `gyo2` f25837b0 更新（2026-11-21）

起因：主會話 2026-11-21 轉來使用者的話「可以開始權威卡了。記得讓他更新」，要本卡先 rebase 到 f25837b0，照 #257 改路徑，寫明 trace 留下的件，對齊異常分級。下面每一條都是**路徑或實作做法**的改動，不改使用者定過的事、判定的點與門檻、Module 的職責，所以照已核准的範圍往下做；合併前核准時一起給使用者看。

| # | 哪裡 | 修訂 3 | 修訂 4 | 根據 |
|---|---|---|---|---|
| 1 | 4.1、5.1、6、I9、第 14 節第 3 步、15.3 | `engine/authority`、`engine/rollback`、`modules.csv` | `engine/modules/authority`、`engine/modules/rollback`；頂層 `config/modules.csv`（多 `dir` 欄） | #257；主會話「新的 Module 放 `engine/modules/`」 |
| 2 | 9.5、判定 4 | 掃描與守衛的目錄 `engine/kernel`、`engine/world_time`；正例在 59fd4da9 命中 | `engine/ecs/kernel`、`engine/modules/world_time`；正例在 f25837b0 命中；樣式不變；含入路徑（`engine/kernel/…`）不隨 #257 改 | #257 |
| 3 | 第 1 步前半（原 afd6e7c5，rebase 後 a8bb6a7e） | `naming_guard` 的第三列 `engine/world_time`；`gyo_expect_links(gyo_world_time GYO::Base GYO::Kernel GYO::Orchestration GYO::Time)`；突變 `wt-links-more` 改 `engine/config/modules.csv` | `engine/modules/world_time`；連結集多 `GYO::Logging`（日誌批讓 `world_time` 依賴 `logging`，登記列已是 `base;kernel;logging;orchestration;time`）；突變改 `config/modules.csv` 的新列；自測與 `wt-guard-*` 的路徑一起改 | #257、日誌批；rebase 時 `naming_guard.py` 一處衝突 |
| 4 | 10.3 | 照異常分級的動工前版本：用改名後的 `KernelFailure` 等；上游例外「之後不再呼叫」 | 既有型別不改名，後綴規則只用在新型別；`Build` 的例外往外拋；不在回傳之外再記一筆；E3 用 World 的訊息告知 | 異常分級第 0.1、19 節（D3、D4、17.2 撤回） |
| 5 | 10.4（新） | 沒寫 | 回滾重跑每步開一段 `rollback`／`resimulate`；加一案與一個突變 | trace 合併前核准第 79 行 |

不受影響的：判定 1～5 的點與門檻、5.4 的預算與基準值（判定 5 的量測不裝 trace 接收者）、第 12 節定過的各件、DI 的方向。第 14 節的估時不變（10.4 約 1 h，在第 5 步的範圍內吸收）。

