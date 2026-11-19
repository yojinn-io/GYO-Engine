# 事前宣告：時間 Module＋事件（`v8-arch-gyo2-time-events`）

更新：2026-11-19（修訂 4：收第 11 節五件與「一起確認」第 1～3 條的使用者決定；第 3 件推翻本卡建議、範圍擴到編排層的預設骨架；「一起確認」第 3 條推翻 `time-plan-two` (7)，遊戲時間全部放 `engine/world_time`）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：動工前核准（使用者 2026-11-19，本卡：「照建議，全部核准」，`v8-arch-gyo2-time-events-declaration-approved`）。照第 12 節實作（第 1 步 → 第 7 步），合併前做合併前核准。**

> **核准的範圍**：第 11 節五件與各條的結果；第 10 節的 DI 文字（最終文字在合併前核准）；第 10.2 節 O-4 補句**不算編排層職責清單多一項**的判斷；第 6.1 節 G1～G8 的方向（逐組的最終核准在合併前帶突變結果與檢證者報告）；第 12 節的模型與檔位。比過的選項：逐項核准或退回某項／全部核准 ←選。

經過：修訂 0（f7ad16a2）送 Fable 5.2／high 敵對式檢查：必改 8、建議改 14、小問題 12（報告 主 checkout `build/evidence/gyo2-time-events/declaration-r0-adversarial-check.md`）。修訂 1 全部照改，並收主會話轉來的使用者決定 `v8-arch-gyo2-summary-padding-a-then-c`；處理對照在第 15 節。修訂 1 沒有再送檢查。

性質：實作段第 3 批的事前宣告（AGENTS §3 的 Architecture Delta、§2 的基盤 Architecture、§14.4 的判定變更、Design Invariants 的變更）。

用詞：整條線分「設計段／實作段」；每批的核准分「動工前核准／合併前核准」；一批之內的實作步驟叫「第 N 步」。標「**本卡照 L17 判**」的是這張卡自己的判斷（名字照命名規範、細節照別家原始碼、批次內的先後），**不是使用者的話**；本卡新加的詞第一次出現時標「本卡加的詞」。

---

## 0. 根據與讀的版本

**程式**：`origin/gyo2` eedc9a00（#247 之後）。講現況的句子都來自讀程式的偵察（主 checkout `build/evidence/gyo2-time-events/`）：`recon-a-kernel-facts.md`（Kernel）、`recon-b-orchestration-facts.md`（編排層、`engine/time`、守衛、scenario）、`recon-c-reference-engines.md`（Bevy 6a911653f、flecs 09790b5f7、lightyear 736e1695、bevy_replicon d80c849、Unity Netcode）；三份都是 Opus／high。

**文件**（文件分支 `origin/claude/gyo2-time-design` 62c9d8ee）：

| 文件 | 狀態 | 本頁怎麼用 |
|---|---|---|
| [批次表](rebuild-batch-plan.zh-Hant.md)第 3 列 | 主會話 | 範圍 |
| [事件游標的宣告](event-cursor-declaration.zh-Hant.md)修訂 6 | **已核准** | 內容不改；第 4 節只寫和現況的差、「實作批定」的細節、和第 3／4 批交界的過渡 |
| [T1 宣告](time-types-declaration.zh-Hant.md)修訂 2 | 未核准；寫在 #240、#242、#244、D2 之前 | 第 2.2、2.3 節逐條對照，併進本頁 |
| [時間設計](rebuild-time-design.zh-Hant.md)修訂 4 | 第 0 節是使用者決定；其餘是設計稿 | 第 0 節照用；其餘和新前提衝突的以本頁為準 |
| [統合](kernel-app-boundary-declaration.zh-Hant.md)（分支 `claude/gyo2-kernel-app-boundary-4af68c`）L2、L8、L10、L13、L19、L27；3.2、15.1、15.2、16、第 7、8 節 | 已核准 | 給時間 Module 的要求 |
| [回滾份](rollback-module-declaration.zh-Hant.md) 5.7、7.2；[權威份](authority-module-declaration.zh-Hant.md) 5.4；[讀檔份](restore-entry-declaration.zh-Hant.md) 5.5 | 已核准 | 給時間 Module 的要求 |
| [編排層骨架的宣告](orchestration-skeleton-declaration.zh-Hant.md) 7.4、8.3、8.4；[編排層](orchestration.zh-Hant.md) O-1～O-11 | 已合併（#244） | 延後的守衛、scenario 的接口、Module 的接法 |

**使用者的決定**（原話以 HANDOFF 為準）：`-unified-time`、`-fixed-step-in-world`、`-time-plan-two`、`-same-input-same-result`、`-active-time-name`、`-t1-three-items`、`-clock-out-of-kernel`、`-module-deps-in-registers`、`-products-pick-modules`、`-rollback-module-strategy`、`-layer-duty-lists`、`-events-two-kinds-like-bevy`、`-event-trim-by-cursor`、`-cross-clock-events-open`、`-no-separate-eventbus`、`-scenario-tests`、`-scenario-failure-by-contract`、`-guards-after-mechanism-stable`、`-broken-world-ends-product`、`-k1-no-legacy-product-aliases`、`-authority-guard-gap-later`、`-summary-padding-a-then-c`、Module／Plugin 的定義（#240）。

---

## 1. 這一批做什麼（先用遊戲講）

**2D 橫版的單機遊戲**，一個 World，畫面每幀、模擬固定步 64 Hz。

- 每幀開頭，時間 Module 的「推進時間」System 取一次樣，算出這一幀的真實時間與可縮放的時間：暫停時是 0、慢動作時乘 0.5；卡住 3 秒時固定步最多補 5 步、多的整步扣掉並記下來（沒有固定步的 World 改用 250 ms 的上限，兩個上限不同時作用）。
- 輸入的 System 讀手把，寫一筆 `Message`「按下了跳」。
- 時間 Module 的「跑固定步」獨占 System 把這一幀的量加進累加器：夠 2 步就跑 2 次固定步的 schedule。每跑一步，`Tick` 加 1，`ActiveTime` 換成固定步的值；跑完換回每幀的值。
- 固定步裡的「跳躍」System 讀「按下了跳」：第一步讀到、第二步沒有新的。撞到金幣時寫一筆 `Message`「拿到金幣了」，並在同步點拿掉金幣實體：**先跑 `Remove<Coin>` 的 `Observer`（生成粒子），再跑 OnRemove hook，然後才銷毀**。
- 每幀後段的「播音效」讀「拿到金幣了」。這一幀固定步跑 0 次時沒有新的；跑 3 次時一次讀到三筆。
- 存檔時，作者把 `Tick`、`VirtualTime` 等存進去；讀檔換到新 World 時放回，時間接著存檔時的值跑。

**PvP 的 Match**（沒有畫面）：一樣的「跑固定步」；不登記暫停，就沒人能暫停。伺服器權威策略（第 4 批）只准時間 Module 公開的驅動者寫 `Tick`。**PvP 的 Client**（第 4 批）：回滾策略用時間 Module 的「退回 k 步」函式把 `Tick` 與固定步的累計一起退回、立起「重跑中」，再用「跑一步」函式重跑 k 步；每一步 System 讀到的時間和第一次跑那一步時相同。

Kernel 不知道「每幀」「固定步」「暫停」「tick」是什麼：它只執行 schedule、保存 `Message` 的游標、在觸發點跑 `Observer`。

---

## 2. 範圍

### 2.1 這一批做的

| 代號 | 內容 | 出處 |
|---|---|---|
| E1 | 守衛的整理：從 `kernel_guard.py` 拿掉 `PRODUCT_ALIASES`；第 2 批延後的 6 條綁實作守衛逐條判（第 6.3 節） | `-k1-no-legacy-product-aliases`、骨架宣告 7.4 |
| E2 | 時間的型別與累加器（舊 T1）：`engine/world_time` 的純資料型別與純算術（`engine/time` 不動） | T1 修訂 2，照第 2.2 節重整 |
| E3 | 保留式事件改名 `Message`＋游標＋上限＋聚合的丟棄記錄；拿掉 `EventTrim` | 游標份 3.2、5.1（已核准） |
| E4 | 拿掉 Kernel 的時鐘：`Domain`、`ScheduleOptions::domain`／`clock`、Kernel 推 tick、`CurrentTick`、`AccessKind::DomainTick`、tickStamps、`World::Tick`／`TickStamp`、D-E12、F-E1、F-9；診斷與丟棄紀錄改帶同步點序號（L8） | 統合 3.2、L2、L8 |
| E5 | 時間 Module（新的 Engine Module，第 3 節） | 批次表第 3 列；統合第 8、16 節；回滾 5.7、7.2；權威 5.4；讀檔 5.5 |
| E6 | 推送式事件 `Event`＋`Observer`＋`Trigger`＋`On<E>`（含元件 Add／Remove 觸發） | 游標份 3.3、5.2（已核准） |
| E7 | scenario 第 2 段（時間與事件） | `-scenario-tests`；骨架宣告 8.3 |
| E8 | 不另做 EventBus：本批碰到的 Engine 機制從 Kernel 的事件發（第 5 節） | `-no-separate-eventbus`、`-event-delivery-rule` |
| E9 | **編排層的預設骨架**：編排層給每幀一張 `Main` 與固定步一張 `FixedUpdate`，預設帶、作者可換或關（第 3.6 節）。改第 2 批已合併的編排層 | 使用者 2026-11-19（第 11 節第 3 件，`v8-arch-gyo2-orchestration-gives-schedule-skeleton`） |

### 2.2 舊 T1 逐條對照現況

| T1 的內容 | 現況或後來的決定 | 本頁的處理 |
|---|---|---|
| 型別放 `engine/time`，命名空間 `Engine::Time` | `time-plan-two` (7)「時間的型別留在 `engine/time`」；DI 10 規定 `gyo_time` 只連 `GYO::Base`＋`Threads`（`time.md:89-90`，守衛 `tests/common/time/CMakeLists.txt:37`） | **改了（使用者 2026-11-19，「一起確認」第 3 條，`v8-arch-gyo2-game-time-in-world-time`，推翻 `time-plan-two` (7)）**：遊戲時間全部放 `engine/world_time`，`engine/time` 照舊只有時間基準（`MonotonicClock`、`Waiter`、`LateWakeStats`），DI 10、D29、名字都不動（第 3.1 節） |
| `FixedTime` 沒有累計、沒有 tick（T1 第 2.1、3 節第 7 條：「固定步裡的累計只能是 tick×步長」） | `-clock-out-of-kernel`：tick 移到時間 Module；回滾策略會退 tick（回滾 7.2） | **本卡照 L17 判，改 T1：`FixedTime` 有累計 `elapsed`**，每跑一步加一個步長（Bevy `Time<Fixed>` 每步 `advance_by`，`fixed.rs:216-227`；lightyear 回滾時從現在的累計減掉 k 步，`prediction/rollback.rs:1153-1183`；Unity Netcode `m_ElapsedTime - m_FixedTimeStep*tickAge`，`NetcodeClientPredictionRateManager.cs:288`——三家都是累計值）。T1 擔心「回滾時 tick 被還原、累計不會」：時間 Module 給「退回 k 步」的函式同時退 `Tick` 與累計（第 3.3 節），一處維持。修訂 0 的「`elapsed`＝`Tick`×步長」退掉：沒有一家這樣做，而且之後 Client 對時改 `Tick` 時累計會跟著跳。**使用者 2026-11-19 確認照建議**（`v8-arch-gyo2-fixed-time-accumulates`） |
| 「兩個寫者要不要標成可多寫」（T1 第 8 節） | `-authority-no-writer-count`：Kernel 不數寫者 | 不再是問題；時間設計 DI 2「各只有一個寫者」改寫成約定（WT-5） |
| 放進 World 之後摘要與 padding（T1 第 3 節第 9 條） | 使用者 `-summary-padding-a-then-c`（2026-11-19）：`World::Summary` 照原始位元組雜湊會算進 padding；先由編排層骨架卡補 Kernel 自己的型別、契約補「進摘要的型別不能有 padding」；正式解法（逐型別的雜湊）在第 4 批；比摘要的測試標「未完全解決」 | **第 3 批不改摘要的方式**。時間的型別、`TimeDiscarded`、本批新增進 World 的 Kernel 內部紀錄（游標表、`{seq, payload}`）都照契約沒有 padding，用 `static_assert`（`sizeof` 等於逐欄大小的加總；不用 `has_unique_object_representations_v`，它對浮點一律回 false）守。浮點的 +0／-0 與 NaN 位元組不同（使用者那次列的第三種毛病）：本批不處理，`VirtualTime.scale` 只會被寫成套用的倍率（不會產生 -0），寫進範圍的限界；逐型別雜湊在第 4 批解 |
| 「時間的 System 與驅動固定步的 System，T3」 | 「Module」重新定義（#240），編排層有登記單位（#244） | 本批做（第 3 節） |
| 「這一批不碰 Kernel」 | — | 本批同一條線拿掉 Kernel 的時鐘（E4） |

### 2.3 T1 欠使用者的四件

| T1 的待決 | 處理 | 依據 |
|---|---|---|
| (a) 頻率是整數還是小數 | **本卡照 L17 判：收小數**（`double`），換算一次成整數奈秒、**四捨五入**（本卡自己的規則；Bevy `from_hz` 經 `Duration::from_secs_f64`，它怎麼捨入沒有查），之後只用整數；不是有限的正數是斷言。**這改了主會話在 T1 4b 的建議（整數）**，在第 11 節「一起確認」點明 | 批次表「頻率的型別照 Bevy `Time<Fixed>`／Unity 的做法判」。Bevy `from_hz(f64)`（`fixed.rs:76-109`）；Unity Netcode `SimulationTickRate` 是整數。存進去都是整數奈秒、之後行為相同，差別只在能不能寫 59.94，不改使用者的決定。IEEE 除法正確捨入，三平台換算相同；判定 2 的「步長的設定」案例在 L1 三平台跑。**跨平台的浮點**（使用者問後 side chat 的回答，2026-11-19）：IEEE 754 的加減乘除與開根號正確捨入，同樣輸入在符合標準的 CPU 上得到同一個位元結果；差異來自 FMA 合併、fast-math 重排、x87 的 80 位元、libm（sin、cos、atan2 等）、denormal。GYO 已對 Clang／GCC 加 `-ffp-contract=off`（`build/cmake/GyoBuild.cmake:34-40`；MSVC 的 `/fp:precise` 本來不合併），沒有 fast-math，三平台都是 64 位元。本批的兩處浮點（頻率換算的一次除法、每幀乘倍率的一次乘法與零頭）都是基本運算、不經 libm。PvP 的 Match 與 Client 比對的是換算後的整數奈秒，Match 不用倍率。不採的替代：整數頻率走純整數換算、小數才走浮點（多一條路徑與一組測試）。**使用者 2026-11-19 確認照判定**（`v8-arch-gyo2-t1-closed-by-time-events`） |
| (b) 主會話對設計的四處調整 | **本卡照 L17 判，四處都留**：(1) 扣整步是純函式 `AdvanceVirtual`（在 `engine/world_time`）（Bevy 的截斷也在 `Time<Virtual>` 自己的函式，`virt.rs:249-266`）；(2) `FixedTime` 裡裝累加器（獨占 System 不能有自己的狀態）；(3) `Add` 多一個上限參數做斷言；(4) `Reset()` 回傳清掉的量。`Reset` 本批**只留在型別裡、不接線**：執行中換步長沒有點名的 Feature（AGENTS §2），步長只在建 World 時由設定給（第 3.3 節） | 時間設計第 547、552 行「扣整步是時間的 System 的事」在合併前改寫成「`AdvanceVirtual` 一處」 |
| (c) 修訂 2 要不要再送檢查 | 被本頁取代：本頁修訂 0 送了換模型檢查 | — |
| (d) 核准整份 T1 | 被本頁取代：核准本頁即核准併進來的內容；T1 檔案留作紀錄，開頭加一行「由 time-events 宣告取代（`v8-arch-gyo2-t1-closed-by-time-events`）」——在本批的實作 PR 裡改（T1 在 `gyo2` 與文件分支各有一份，改在併進 `gyo2` 的那一份） | `v8-arch-gyo2-t1-closed-by-time-events` |

### 2.4 不在這一批

| 事 | 歸哪一批 |
|---|---|
| runner 在交換點寫取樣、可注入的等待、對準 tick 格點的等待 | 編排層第 5 條（交換點）。本批的取樣照 Bevy `TimeUpdateStrategy` 放在 Resource（第 3.4 節） |
| 執行中換步長 | 沒有點名的 Feature；需要時是 Replacement Criteria（WT RC (e)） |
| tick 跳變的通知、Client 對時 | 第 4 批（回滾）、網路 |
| 「tick 只有一個來源」的檢查、tick 的 Z6 例外 | 第 4 批（權威）；本批公開 tick 的型別與驅動者名單（L27） |
| `World::Summary` 改逐型別雜湊 | 第 4 批（`-summary-padding-a-then-c`） |
| 區規則（含 `MessageOptions::zone`） | 第 4 批（第 4.5 節的過渡） |
| 狀態機切換、讀檔完成、存檔版本的通知 | 第 5 條各批（第 5 節寫建議） |
| World 壞掉的通知、日誌 Module | 日誌還沒排批次；第 11 節第 1 件 |
| Loading（`Domain::Loading` 沒有任何行為，`recon-a` §7） | 隨 `Domain` 拿掉；流程由狀態機或生成世界那一批定 |
| PvP 改 64 Hz；命名空間根的改名 | PvP 移植；另一批 |

---

## 3. 時間 Module

### 3.1 位置、名字、include 順序（本卡照 L17 判）

| 事 | 判定 | 理由與系譜 |
|---|---|---|
| `engine/time` | **不動**：照舊只有時間基準（`MonotonicClock`、`Waiter`、`LateWakeStats`），DI 10（只連 Base＋Threads）、D29、名字都不動 | 使用者 2026-11-19，「一起確認」第 3 條，`v8-arch-gyo2-game-time-in-world-time`。三家都把「現在幾點、睡到幾點」放在 OS 介面那一層、把遊戲時間放在另一處並和 World 綁在一起：Bevy 的基本時間是 `Instant`（`bevy_platform`），遊戲時間全在 `bevy_time`；flecs 基本時間在 OS API（`ecs_os_now`、`ecs_os_sleep`），遊戲時間在 World 資訊與 timer addon；Unity 基本時間是 `Stopwatch`、`realtimeSinceStartup`，遊戲時間是 World 的 `TimeData` 與更新它的 System |
| 遊戲時間（全部） | **新的 Engine Module** `engine/world_time`（命名空間 `GYO::Engine::WorldTime`、target `GYO::WorldTime`、登記單位 `WorldTimeModule`，編排層 `Module` 的子類）：`RealTime`、`VirtualTime`、`FixedTime`、`ActiveTime`、`TimeControl`、`TickAccumulator`、`AdvanceVirtual`、步長與兩個上限的設定、`Tick`、`Resimulation`、`TimeSampling`、`TimeDiscarded`、兩個 System、跑一步／退回 k 步。純型別與算術的標頭（`TimeTypes.hpp`、`TickAccumulator.hpp`）**不 include `MonotonicClock.hpp`**（zone_scan 的 ZS1，`tests/common/zone_scan/zone_scan_rules.json:6`）；只有 `AdvanceTime` 的實作讀它。**本卡加的詞** | **使用者 2026-11-19，「一起確認」第 3 條，`v8-arch-gyo2-game-time-in-world-time`**。原話：「一個概念不要分為2個實作。看著太噁心了。有這種一個概念開始分裂的情況，就是壞味道的開始。」「不不不，你看看別家的實作，基本時間有什麼，而遊戲時間有什麼。…」「可以。不頭鐵」。比過：遊戲時間全部放 `world_time`、`engine/time` 只留時間基準 ←選／全部放 `engine/time` 並讓它連 Kernel（DI 10 要改，audio、threads 被迫連 Kernel）／維持分兩處（修訂 1～3 的寫法，使用者判為壞味道）。理由（使用者的）：一個概念不要分成兩個實作；看別家的分法、不頭鐵。side chat 的補充（不是使用者的話）：`time-plan-two` (7) 當時把「型別」與「時間基準」當成同一個概念，照別家的分界它們是兩個概念；現在 `engine/time` 裡沒有遊戲時間，分裂是 T1 與本宣告的計畫要讓它發生。名字：N17 要目錄、命名空間、target 一致；叫 `GYO::Engine::Time` 會遮住現有的 `::Engine::Time`。比過：`Clock`、`Timeline`、`GameTime`。系譜：Bevy `bevy_time` |
| 登記檔 | `engine/config/modules.csv` 加 `world_time,GYO::WorldTime,…,no,yes,base;kernel;orchestration;time,,WorldTimeModule,engine/world_time/WorldTimeModule.hpp`（`library_only=no`、`default=yes`；預設帶、產品可關，O-6） | `-module-deps-in-registers`。**第一個真的進 World 的 Engine Module**（現在十列都是 `library_only=yes`） |
| include 順序 | `engine/world_time` 的程式一律寫 `::Engine::Base::`、`::Engine::Time::`；`gyo_orchestration_include_order` 的範圍本批就擴到 `world_time` 的公開標頭（含 `RunOneFixedStep` 那個樣板的標頭）。它是「各種順序都編得過」的行為檢查，不是綁實作的掃描，所以不延後 | 第 2 批的問題（dev_log `2026_11_18_gyo2_orchestration_skeleton.zh-Hant.md:45`、a8065853）；時間 Module 在 `GYO::Engine` 底下 |

### 3.2 Feature → Contract → Architecture

| Feature（Product） | Contract | Architecture |
|---|---|---|
| 固定頻率的模擬不被畫面拖慢（①PvP Match、②單機） | 每幀跑 0～N 次固定步；步長整數奈秒、預設 64 Hz；一幀最多 N 步（預設 5），超過的整步從 `VirtualTime` 扣、記進 `discarded` | 「跑固定步」獨占 System＋`FixedTime`＋`AdvanceVirtual` |
| 暫停、慢動作，UI 照常動（②、③工具的預覽） | `TimeControl` 寫想要的；`VirtualTime` 是套用後的；`RealTime` 不受影響 | 「推進時間」System |
| 作者不必選讀哪個時間（②） | `ActiveTime` 平常＝`VirtualTime`，固定步裡＝（步長、`FixedTime.elapsed`） | 「跑一步」函式換入換出 |
| PvP 的權威與回滾、存檔（①、②） | `Tick` 是公開型別；驅動者名單公開；重跑走同一個「跑一步」；「退回 k 步」同時退 `Tick` 與累計；「重跑中」恆在；可存可放回的 Resource 有清單 | 第 3.3、3.5 節 |
| 測試與重放不靠真實的時間基準（①②③） | 取樣可以換成手動 | `TimeSampling`（第 3.4 節） |
| 追不上、或固定步沒被驅動時有記錄（①②；使用者「丟了多少有記錄」） | 扣掉的量累計在 `VirtualTime.discarded`，並發一筆 `TimeDiscarded` | 第 3.3 節「沒被取走的量」；第 5 節 |

### 3.3 Resource、System、函式

| Resource | 欄位（整數奈秒） | 寫者 |
|---|---|---|
| `RealTime` | `now`、`delta`、`elapsed`、是否已有第一次取樣 | `AdvanceTime` |
| `VirtualTime` | `delta`、`elapsed`、`scale`（這一輪乘上去的）、`paused`、`discarded`（累計扣掉的）、倍率乘完的零頭 | `AdvanceTime` |
| `FixedTime` | 步長、累加器（餘數）、`elapsed`（已跑的固定步累計）、`pending`（`AdvanceTime` 交給 `RunFixedSteps` 的這一輪的量；本卡加的欄位）；`Overstep()`、`UntilNextStep()` | `AdvanceTime`（寫 `pending`）、`RunFixedSteps`（取走 `pending`、`Add`）、跑一步／退回 k 步（`elapsed`） |
| `ActiveTime` | `delta`、`elapsed` | `AdvanceTime`（每幀的值）、跑一步函式（固定步的值，跑完換回） |
| `Tick` | `value`（`uint64`）：已經開始的固定步數——第 n 步執行中與跑完之後都是 n | 跑一步函式；退回 k 步函式；回滾策略 |
| `TimeControl` | 想要的 `paused`、`scale` | 產品。設定 `pausable=false` 時不登記 |
| `TimeSampling` | 模式：讀時間基準（預設）／手動「每幀經過時間」（沿用到被改，照 Bevy `ManualDuration`，`bevy_time/src/lib.rs:122-128`）／手動「下一個時間點」 | 產品、測試 |
| `Resimulation`（「重跑中」） | `active`，初始 false | 只有回滾策略寫（第 4 批）；時間 Module 只登記 |

| System 與函式（`WorldTimeModule` 提供） | 種類 | 做什麼 |
|---|---|---|
| `AdvanceTime`（推進時間） | 一般 System。`pausable` 時讀 `TimeControl`，否則用不讀它的另一個簽名（沒登記的型別當參數是啟動錯誤 RE4，`Registry.cpp:327-336`） | 依 `TimeSampling` 取一次樣 → 更新 `RealTime`（建好 World 後第一次取樣 `delta`＝0） → 上一輪的 `pending` 若沒被取走，記進 `discarded` 並發 `TimeDiscarded`（原因：固定步沒有被驅動） → `AdvanceVirtual` 算這一輪的量與扣掉的量 → 寫 `VirtualTime`、`ActiveTime`（每幀的值）、`FixedTime.pending`；扣掉的量大於 0 時發 `TimeDiscarded` |
| `RunFixedSteps`（跑固定步） | `ExclusiveWorld<Writes<Tick, FixedTime, ActiveTime>>`，自己佔一個 stage（`-same-input-same-result`） | 取走 `pending`、`Add` 進累加器得 k 步（`AdvanceVirtual` 已保證 k ≤ 上限）；呼叫 k 次 `RunOneFixedStep` |
| `RunOneFixedStep(world, schedule)`（公開的函式樣板，參數是任何一種 `ExclusiveWorld<…>`——`ExclusiveWorld<Writes<…>>` 是不同的 `final` 型別，`Params.hpp:838-839`） | — | `Tick`+1、`FixedTime.elapsed`+步長 → `ActiveTime`＝（步長、`FixedTime.elapsed`）→ 巢狀跑固定步的 schedule → 回 Ok 時把 `ActiveTime` 換回每幀的值；**回 Err 或例外時直接往上傳、不換回**（World 已中斷，`Params.hpp:1006-1024`、`Executor.cpp:139-168`；D2 之後 Product 結束） |
| `RewindFixedSteps(world, k)`（公開的函式樣板；本卡加的） | — | `Tick`−k、`FixedTime.elapsed`−k×步長；累加器不動。回滾策略重跑前呼叫，之後呼叫 k 次 `RunOneFixedStep`（回滾 7.2 第 2 條，「時間 Module 給推進一步的函式」那一種）。系譜：lightyear 回滾時 `apply_delta(-n)` 並把 `Time<Fixed>` 換成回滾用的值（`rollback.rs:1153-1291`） |

**schedule 與 stage 的名字**：時間 Module **不宣告任何 schedule**，只往編排層的預設骨架（第 3.6 節）上掛兩格：`AdvanceTime` 在 `Main` 的 `First`、`RunFixedSteps` 在 `Main` 的 `FixedMainLoop`，巢狀跑 `FixedUpdate`（使用者 2026-11-19，第 11 節第 3 件，`v8-arch-gyo2-orchestration-gives-schedule-skeleton`）。作者關掉預設骨架、自己宣告時，`WorldTimeModule` 的設定可以換這四個名字。那時：掛載的目標（每幀那一張與兩個 stage）不存在是 Kernel 既有的 D-E1（`Registry.cpp:1292-1304`；Kernel 有啟動錯誤時 `Finish`、`Check` 不跑，`Product.cpp:244-249`）；固定步那一張（巢狀跑的目標、不是掛載）不存在由 `WorldTimeModule::Check` 回報，否則執行時才是 X-12。

**`Tick` 的先後**：跑之前加 1，第一步執行中讀到 1——和現在 Kernel 推 tick 的語意相同（`Executor.cpp:212-219`，`RunPlan` 開頭 `++ticks_`），也和 lightyear 在 `FixedFirst` 加 1 相同（`timeline.rs:211-278`）。

**公開給權威的**（L27）：`Tick` 的型別；驅動者名單 `WorldTimeModule::TickWriters()`＝`{"RunFixedSteps"}`（對時 System 在 Client 那一批加）；`RunFixedSteps` 帶 `Writes<Tick>`。

**步長**：只在建 World 時由 `WorldTimeModule` 的設定給（頻率或時間長度，預設 64 Hz）。執行中換步長本批不做。

**和「遊戲時間全部放 `world_time`」對過**（side chat 逐點對、使用者 2026-11-19「同意」，`v8-arch-gyo2-world-time-surface`）：(1) 行為、名字、規則都不受影響；(2) 依賴更單純：`world_time` 只在取樣時用 `engine/time` 的 `MonotonicClock`，方向照舊 `world_time` → `time`；(3) **新增：`world_time` 內型別與取樣分標頭**——型別（含 `Tick`）一個標頭、不 include `MonotonicClock.hpp`，取樣（`AdvanceTime` 讀時鐘的部分）另一個標頭；理由：zone_scan 的 ZS1 禁 (c) 區的來源 include 時鐘，第 4 批判定的 System 要讀 `Tick`；以前靠型別在 `engine/time` 自然隔開，現在在 `world_time` 裡切開——這是檔案的切法，不是兩個實作；(4) T1 調整 (b) 第 1 處（`AdvanceVirtual` 是純函式）照舊留：「不經 Kernel 單獨測」這個理由變弱（測試會連到 Kernel），但它仍是純函式、測試只餵數字不建 World，「Bevy 把截斷放在時間型別自己的函式」照舊成立。

**時間 Module 的職責清單**（新 Component，本卡照 L17 寫）：(1) 把時間的 Resource 登記進它被建進的每個 World；(2) 取樣（照 `TimeSampling`；交換點做出來後，「取樣」這一項移給 runner，Resource 的形狀不變）；(3) 真實時間與可縮放時間的推進（暫停、倍率、上限、扣掉的記錄）；(4) 固定步的累加與驅動、`Tick`、退回 k 步；(5) `ActiveTime` 的切換；(6) 登記「重跑中」並放初始值（回滾 5.7 已核准由時間 Module 收）；(7) 發 `TimeDiscarded`。不在清單上的：回滾的判斷、還原與重跑（回滾 Module）；tick 只有一個來源的檢查（權威）；runner 的等待與交換點（編排層）；狀態（狀態機）。

### 3.4 取樣（本卡照 L17 判）

時間設計第 3.1、5.4 節的形狀是「runner 在交換點寫取樣的時間點」，但交換點在編排層第 5 條。本批照 Bevy `TimeUpdateStrategy`（`bevy_time/src/lib.rs:115-131`、`:179-192`）放在 `TimeSampling`：預設讀時間基準（`AdvanceTime` 呼叫 `::Engine::Time::MonotonicClock`），測試、重放、scenario 用手動。

三個後果：(1) 多個 World 的產品各自取樣，同一幀不同 World 的 `RealTime.now` 不同——時間設計 DI 1「一次醒來只取樣一次」在交換點做出來之前不成立，WT-2 的「每幀只取一次樣」是每個 World；(2) Ownership 和時間設計第 537 行「取樣歸 runner」不同，是過渡（職責清單第 (2) 項註明）；(3) 手動「每幀經過時間」沿用到被改。

和既有規則：「System 不自己讀時間基準」是時間設計的設計稿；使用者定的 K.3 第 11 項只禁 (c) 區的 System 自己讀時鐘（`docs/architecture/plans/kernel/spec.zh-Hant.md:1219`），已歸伺服器權威策略。`AdvanceTime` 是 (b) 區、不是判定的 System。

### 3.5 讀檔、回滾、權威的要求逐條對照

| 要求（出處） | 本頁的回答 |
|---|---|
| 時間的 Resource 能被讀檔放回（讀檔 5.5） | **可以存與放回**：`Tick`、`VirtualTime`（`elapsed`、`discarded`、零頭；`delta` 下一幀會被蓋）、`FixedTime` 的 `elapsed` 與累加器的餘數、`TimeControl`。**不放回**：`RealTime`（時間基準的時間點跨行程沒有意義；新 World 的 `RealTime` 從頭開始，第一次取樣 `delta`＝0）；`FixedTime` 的步長以這次的設定為準（作者只放回 `elapsed` 與餘數，時間 Module 給一個「放回固定步狀態」的函式，不收步長）；`Resimulation`（恆 false）。存檔結構要沒有 padding（`-summary-padding-a-then-c`；`VirtualTime` 有 bool 與浮點，`static_assert` 守）。scenario S2-8 驗 |
| `Tick` 是回滾策略能寫的 Resource、用 `ExclusiveWorld` 退（回滾 7.2 第 1 條） | 公開型別；不擋任何寫者；`RewindFixedSteps` 是建議的退法 |
| 重跑時每一步怎麼推、System 讀到的固定步時間等於第一次（7.2 第 2 條） | `RewindFixedSteps`＋`RunOneFixedStep`：退回後重跑第 T+1 步時 `elapsed` 與第一次相同（步長不變，本批不准執行中換步長） |
| 「重跑中」由時間 Module 登記、恆在（5.7、7.2 第 3 條） | `Resimulation`；沒有時間 Module 的 World 沒有它 |
| 公開 tick 型別、驅動者、對時（L27、權威 5.4） | 第 3.3 節 |
| 時間設計 DI 2 的守法（權威份修訂 4 回報） | 改成約定（WT-5），沒有啟動檢查；理由：Kernel 不疊加限制（`-kernel-is-ecs-no-restrictions`），作者自己寫驅動也做得到（`engine-not-rigid`） |
| tick 的 Z6 例外（統合 16） | 第 4 批 |
| 訊息要 tick 的自己在內容帶（游標份 3.2） | 寫的 System 讀 `Res<Tick>` |

### 3.6 編排層的預設骨架（使用者 2026-11-19，第 11 節第 3 件，`v8-arch-gyo2-orchestration-gives-schedule-skeleton`；細節本卡照 L17 判）

- **編排層給**：每個 World 在登記單位 `Build` 之前，由編排層宣告兩張 schedule——每幀一張 `Main`（stage 依序 `First`、`PreUpdate`、`FixedMainLoop`、`Update`、`PostUpdate`、`Last`）與固定步一張 `FixedUpdate`（stage 依序 `First`、`PreUpdate`、`Update`、`PostUpdate`、`Last`；**本卡照 L17 判**，Bevy 的 FixedMain 是 `FixedFirst`…`FixedLast` 五段，GYO 的 stage 名屬於各自的 schedule，不必加前綴）；每幀的清單預設跑 `Main`。
- **作者可換或關**（`engine-not-rigid`）：`WorldDescription` 加一個骨架的選項（預設＝上面那一套；關掉＝編排層不宣告，作者自己宣告）。作者在預設骨架之外另加 schedule 照舊。
- **界線**（O-4 補一句；side chat 的措辭，使用者的意思是「不認識時間，但是會執行Module」）：編排層給 schedule 的骨架（名字與段落順序）並經 Kernel 執行它；不認得時間，也不認得任何 Module 掛進去的是什麼。Module 在登記時把 System 掛進去——對上 O-3（runner 不呼叫 System、不排先後）與 RC-O8。**本卡判：不算編排層職責清單多一項**，O-4 已列「遊戲初始化與每幀的清單」，預設骨架是那一項的預設值；核准時請你一併看這個判斷。
- **既有的測試**：第 2 批的編排層測試與 scenario 第 1 段宣告自己的 schedule，判定照舊；它們的 World 若因預設骨架多了兩張空的 schedule 而改到期待值（Registry 指紋、每幀清單），改成明寫「關掉預設骨架」，判定不變（第 6.2 節）。
- **系譜**：Bevy 由 `bevy_app` 給 `Main`（`main_schedule.rs:221-235`），`bevy_time` 只往上掛（`bevy_time/src/lib.rs:84-94`）；flecs 由 pipeline addon 給內建 phase；Unity Entities 給初始化／模擬／呈現三組（side chat 憑知識、沒讀本地原始碼）；Unreal Mass 對到引擎的 tick group。四家都是引擎給骨架、作者可換，沒有一家放在時間模組。退掉的：時間 Module 自己宣告（本卡修訂 1 的建議；退的理由見第 11 節第 3 件）。

---

## 4. 事件：和已核准的游標份的差

游標份（修訂 6）不改。下面寫 (1) 現況，(2) 游標份留給實作批的細節，(3) 和第 4 批交界的過渡。標「本卡照 L17 判」。

### 4.1 現況（`recon-a` §2、§3）

- 讀者沒有游標：掃整個緩衝，取「上一個 tick 中 writerPos 大於自己 Plan 位置」加上「本 tick 中 writerPos 小於自己位置」，依 (tick, writerPos, seq) 排序（`Params.hpp:419-437`）。紀錄 `{tick, seq, writerPos, payload}`，沒有上限、沒有丟棄計數（`detail/Storage.hpp:189-214`）。
- `StepKind::EventTrim`：`Plan.hpp:36`、`Scheduler.cpp:152`、`:201-260`、`Executor.cpp:437-441`（trim 時重拍存取驗證的第 2 層快照）、`Registry.cpp:909-912`（D-E1）、`Fingerprint.cpp` 五處。
- `ExclusiveWorld` 不含事件（DI-KS 1），必須是唯一參數（S-10，`SystemTraits.hpp:137-138`）。World 外面在 Ready 時能寫元件與 Resource、不跑 hook，沒有事件寫入的 API；Ready 時沒有 `SystemContext`。
- 同步點序號 `syncEpoch`：每次同步點套用加 1，hook 清空輪每一輪也加 1（`World.cpp:801`），當場改成員的清空輪也加（`Executor.cpp:749`），當場改成員本身不加。
- 「最外層 Run 結束、清空輪之後」在 `Executor.cpp:444-448` 的 `!nested` 分支。

### 4.2 游標份留給實作批的細節（本卡照 L17 判）

| 細節（游標份出處） | 判定 | 系譜 |
|---|---|---|
| hook 帶 `MessageReader` 被拒：編譯失敗還是啟動錯誤（第 8 節） | **編譯失敗**：hook 參數 traits 的 `static_assert`，訊息帶專屬字串；測試加進 `gyo_kernel_compile_fail_samples`（`tests/common/kernel/CMakeLists.txt:169-230`，已有 `HOOK_PARAMETER` 一型） | Bevy 的 hook 只拿 `DeferredWorld`（`lifecycle.rs:84`） |
| observer 的延後變更和 hook 共用佇列還是分開（3.3 末） | **共用**：一般觸發的 observer 記進該 schedule 的 hook 佇列（巢狀層各自），照 F-12 一輪一輪套用 | flecs 延後模式的 hook 與 observer 落在同一個同步點（`ObserversManual.md:2598`）；Bevy 都用 World 的命令佇列 |
| `ExclusiveWorld::Trigger` 當場跑的 observer，它的延後變更 | 走當場改成員的同一條路（`hookTarget_`＋它自己的清空輪，`Executor.cpp:686-751`） | 和 `ExclusiveWorld` 當場改成員時 hook 的處理一致（DI-KS 18） |
| 一般 System 經 `Commands` 觸發的自訂事件在同步點的哪一步 | **F.2 的最後一步**（Resource 那一步之後、`syncEpoch`+1 之前），依記錄的先後送達；DI-KS 9 的順序表加這一步 | flecs 自訂事件依發出順序（`ObserversManual.md:2711-2714`）；放最後是讓 observer 看得到這個同步點的全部結構變更 |
| 新增、移除時 hook 與 observer 的粒度 | 逐（實體、元件）：新增時 OnAdd hook → Add 的 observer；移除時 Remove 的 observer → OnRemove hook；despawn 的實體全部跑完才銷毀（現行 F.2 的「全部 OnRemove 跑完才銷毀」保留） | flecs `ObserversManual.md:128-142`；Bevy `world/entity_access/mod.rs:1344-1345` |
| World 外面在 Ready 時觸發（Ready 時沒有 `SystemContext`） | 觸發時開一條暫時的佇列，回來之前照游標份 3.3 末套到空（上限＝`KernelSettings::hookRoundLimit`） | 游標份修訂 6；本卡只定「暫時的佇列」 |
| 修剪與存取驗證 | 最外層 Run 結束的修剪改了訊息緩衝與游標表，**修剪之後重拍存取驗證的快照**（現行 `EventTrim` 步 `Executor.cpp:439` 的做法搬過去），下一次 Run 的 Ready 視窗比對（`:226-241`）才不會誤報 | 現行程式 |
| 丟棄記錄要標時間時用哪個計數（L8） | **同步點序號**；`RejectedSpawn::tick`、`DiagnosticsWriter` 的 tick、hook 的 `call.tick` 一起換 | 「第幾個同步點」每個 World 單調、對作者可讀；stamp 是每筆寫入的版本 |
| observer 的登記放哪 | World 的 Registry 裡（K4、`kernel_symbols.py` 不准新的可寫全域） | — |
| `ExclusiveWorld` 觸發 `Event` | `ExclusiveWorld` 加 `Trigger<E>(…)`；DI-KS 1 改成「不含 `Message` 的讀寫；可以當場觸發 `Event`」 | 游標份 3.3（已核准）；Bevy `World::trigger` |

### 4.3 拿掉時鐘後要換的欄位

| 欄位 | 現在 | 改成 |
|---|---|---|
| `RejectedSpawn::tick`、`DiagnosticsWriter` 的 tick、hook 的 `call.tick` | 那個 schedule 所屬 domain 的 tick | 同步點序號（失去「屬於哪個 domain」，見 G8） |
| 編排層 `WorldFailure` 的 `domain`、`tick` | domain 與 tick | `domain` 拿掉；`tick` 換成那個 World 的同步點序號（欄名 `syncEpoch`）。編排層不認得時鐘（O-4）。**少掉的**：錯誤內容看不出「遊戲時間上壞在哪」；作者要的話在自己的錯誤訊息帶 `Tick`（見 G6） |
| `World::Tick`、`World::TickStamp`、tickStamps | Kernel 的 tick 表 | 拿掉（G5） |
| scenario 的 `Schedule(name, clock)`（`WalkerGame.hpp:212-221`） | 為 D-E12 宣告 `Domain::Frame` | 拿掉 domain 宣告 |

### 4.4 指紋、摘要、golden（L10）

Registry、Plan、C 三種指紋的版本號都升（6／4／8），export 標頭 `gyo.kernel.plans 4` 升；golden 重產：`SchedulerTests:138-145`、`RegistryTests:40-41`、`InitialValueTests:904-906`、`HookFlushTests:937-941`、export 文字 `SchedulerTests:711-714`、`HookFlushTests:550`。World 摘要照游標份含游標表與寫入序號、版本升；J-32（`WorldTests:504`）的新 golden **標「未完全解決」**（`-summary-padding-a-then-c`：原始位元組雜湊的正式解法在第 4 批）。observer 的登記進 Registry 指紋。新舊對照在合併前核准時列出。

### 4.5 和第 4 批交界的過渡：區規則還在

區規則第 4 批才拿掉（DI-KS 8 現行「每個 System、元件、Resource、事件宣告 zone」）。本批：

- `MessageOptions::zone` **留著，隨區規則在第 4 批拿掉**。游標份 5.1 寫「`zone` 拿掉（統合 3.2）」，統合 3.2 的去處就是區規則那一批，所以只是時點對齊，內容不變。J-14（`ZoneRuleTests.cpp:53-67`，期待 `DE7==4`）與 C 指紋的事件段照舊。
- 推送式 `Event` 的型別照 `Message` 宣告 zone（`RequireExplicitZones` 時必填，否則 B）；`Observer` 的 zone 照 hook 的規則（`HookOptions::zone`，zone C 的限制照 `Registry.cpp:1086-1092`）。
- 時間 Module 登記的全部是 (b) 區（`Tick` 在內）；(c) 區的 System 讀不到 `Tick`，直到第 4 批（G4）。`gyo2` 上沒有用 (c) 區的產品。

---

## 5. 不另做 EventBus：Engine 機制的送法（**已定：照建議**，使用者 2026-11-19，第 11 節第 2 件，`v8-arch-gyo2-event-delivery-rule`）

從別家歸納的原則（`recon-c`；本卡的歸納，**使用者 2026-11-19 定：照建議，之後各批照它定送法、不再逐批問**）：**World 層級的廣播、讀者節奏不同 → 保留式 `Message`；針對某個實體或結構變更、要當下反應 → 推送式 `Event`；連續的狀態（時間前進、暫停、是否重跑中）→ 不發事件，讀 Resource**。在 Bevy 一致：`StateTransitionEvent`、`AppExit`、`AssetEvent` 是 Message；生命週期與 `bevy_scene::Ready` 是 Event；時間前進、暫停、lightyear 的 `Rollback` 是狀態。flecs 只有推送式，節奏不同時建議用 System 輪詢（`ObserversManual.md:104-126`）。

**兩種事件都不指定收件人**（side chat 對使用者的更正，使用者同意轉交）：發送者都不知道誰處理。差別在 (a) 時間：保留式是讀者自己挑時間拉，推送式是發的那一刻推；(b) 保留式留到讀過或碰到上限，推送式當下沒人訂就沒了。推送式可以帶目標實體（Bevy `EntityEvent`），但只限定「關於哪個實體」，不指定處理者。真正「知道誰、一定送到」的是直接呼叫（例：D2 把錯誤交給 runner）。

| 機制 | 你在 `-no-separate-eventbus` 列了嗎 | Bevy（位置） | flecs（位置） | 本卡的建議 | 和你列舉的差別 | 批次 |
|---|---|---|---|---|---|---|
| 固定步跑了一步（「時間 tick」） | **列了** | 不通知（`fixed.rs:243-258`） | 不通知；系統輪詢 tick source（`timer.c:12-48`） | **不發**：訂閱者就是固定步 schedule 裡的 System | **縮小**：你列的「時間 tick」不發事件 | — |
| 時間被扣掉、固定步沒被驅動 | （「時間」那一類） | debug 日誌＋`effective_speed`（`virt.rs:255-266`） | 沒有截斷 | **`Message` `TimeDiscarded`**（量、原因：整步上限／時間長度上限／沒被驅動、這一輪的真實經過時間）；累計照舊在 `VirtualTime.discarded` | 新增 | **本批** |
| 暫停、恢復、改倍率 | — | 不通知（`virt.rs:213-239`） | 沒有暫停 API | 不發；讀 `VirtualTime`、`Changed<…>` | — | — |
| 元件新增、移除 | 列了（生命週期） | hook 先、observer 後（`bundle/insert.rs:458-499`） | OnAdd／OnRemove observer（`flecs.h:1873-1889`） | `Event` `Add<T>`、`Remove<T>`（已核准） | 一致 | **本批** |
| tick 跳變（回滾退 tick、對時跳 tick） | — | —；lightyear 推送式 `LocalTimelineShift`（`timeline.rs:321-330`） | — | `Event`（以 tick 為索引的緩衝要同時平移，lightyear 的理由）；「重跑中」是 Resource、不發 | — | 第 4 批、網路 |
| 狀態機切換 | 列了 | `Message` `StateTransitionEvent`（`transitions.rs:236-291`） | 核心沒有 | `Message` | 一致 | 第 5 條 |
| 讀檔完成 | 列了 | 推送式 `Ready`（`resolved_scene.rs:315`） | 同步函式 | 讀檔＝新 World；由新 World 的準備或生成世界的 System 發 `Message`，定案在讀檔的實作 | 一致（送法待定） | 第 5 條 |
| 存檔版本被拒 | — | — | — | `Result` 的錯誤碼（O-5） | — | 第 5 條 |
| World 壞掉（D2） | **列了** | 錯誤回呼 `FallbackErrorHandler`（`error/handler.rs:116`） | 直接呼叫日誌（`log.c:451`） | **已定（第 11 節第 1 件，`v8-arch-gyo2-world-events-not-via-event`）**：不發事件；D2 的結構化錯誤回給 runner，日誌 Module 從 runner 拿 | 縮小（使用者已定） | 日誌那一批 |
| World 建立、替換 | — | 沒有事件（`app.rs:1235-1247`） | 沒有；`ecs_atfini` 回呼 | **已定**：同上，不發事件 | — | 日誌那一批 |

### 5.1 逐項說明（side chat 寫、使用者看完說「嗯寫進去」，2026-11-19；**不是使用者的原話**）

1. **固定步跑了一步 → 不發**：時間 Module 每跑一步 `Tick`+1；模擬邏輯放在固定步的 schedule 裡每步自動跑，「被跑到」就是通知，要知道第幾步就讀 `Tick`。
2. **時間被扣掉 → 保留式 `TimeDiscarded`（本批）**：卡住 1 秒醒來累積 64 步，一幀最多補 5 步、其餘 59 步扣掉；或這一幀固定步沒被驅動。讀者是日誌、除錯畫面、效能監控，節奏和時間 Module 不同。內容：丟掉的量、原因（整步上限／時間長度上限／沒被驅動）、這一幀的真實經過時間；累計另留在 `VirtualTime.discarded`。
3. **暫停、恢復、改倍率 → 不發**：作者寫 `TimeControl`，下一幀套到 `VirtualTime`；這是一直存在的狀態，讀 `VirtualTime.paused`，要抓變化用 `Changed<…>`。Bevy 同。
4. **元件新增、移除 → 推送式（本批，已核准）**：例：角色加上「著火」時生成粒子、金幣被拿掉時放音效；針對實體、當下反應。新增時 hook 先、observer 後；移除時 observer 先、hook 後，最後銷毀。
5. **狀態機切換 → 保留式（第 5 條）**：遊戲中 → 暫停選單 → 結算；音樂、UI、存檔提示、統計各自依自己的節奏讀。Bevy 同，並在切換點跑離開／進入的 schedule。
6. **讀檔完成 → 保留式，由新 World 發（讀檔那一批）**：讀檔＝建新 World、放回存檔；舊 World 正要被丟掉，所以新 World 準備好之後在它自己裡面寫 `Message`（那時它健康，接第 11 節第 1 件的界線）。
7. **tick 跳變 → 推送式（第 4 批）**：回滾從 100 退到 94 重跑、或對時跳 tick；所有以 tick 為索引的歷史（輸入、預測）要當下同時平移，不能有人晚讀（lightyear 同）。「是否重跑中」是狀態，讀「重跑中」Resource。
8. **存檔版本過舊被拒 → 不是事件，回錯誤碼**：讀檔是作者呼叫的，結果直接回給呼叫者，舊 World 照跑；呼叫者本來就在等結果。

**總結**：「被跑到」與「狀態」直接讀、不發；大家各自看的用保留式；要當下同步處理的用推送式；有人在等結果的直接回傳。

---

## 6. 守衛、測試、突變

### 6.1 判定的變更（AGENTS §14.4；**方向已核准**，使用者 2026-11-19，`v8-arch-gyo2-batch3-guard-changes-direction`；最終在合併前帶突變結果與檢證者報告逐組再看一次）

| 組 | 守衛 | 守的性質 | 處置 | 緩的面 | 緊的面 | 之後由誰守 |
|---|---|---|---|---|---|---|
| G1 | J-37（`ZoneRuleTests:223`）、J-24 的 F-E1（`:400`）、`rs-j37-event-domain-unchecked`、`rs-j37-nonclock-allowed` | 事件只在它 domain 的 clock schedule 用 | 刪 | 任何 schedule 都能讀寫任何訊息（使用者 `-cross-clock-events-open`） | 無 | 游標保證不漏讀（G3） |
| G2 | J-24 的 D-E12（`SchedulerTests:605`）、`RegistryTests:223-244` 的轉換清單、編排層 `[or-gi-domain]`、`rs-j24-de12-removed`、`or-gi-domain-unchecked` | 每個 schedule 有 domain、每個 domain 一個 clock；遊戲初始化的 schedule 屬於一個 domain（O-1 的過渡句） | 刪 | schedule 不必屬於任何時鐘 | 無 | 掛載目標不存在：Kernel 的 D-E1；固定步那一張不存在：`WorldTimeModule::Check` |
| G3 | J-36（`WorldTests:996`）、J-54 的修剪案（`HookFlushTests:591`、`:615`、`:772`）、J-56 的內層修剪（`NestedRunTests:919`）、J-24 的 eventTrim（`CoverageTests:378`）、J-20 的修剪位置（`:425`）、`we-j36-*`、EventTrim 的 7 個突變 | 讀者依位置與 tick 讀、隔兩 tick 讀不到、修剪在 Plan 裡的位置 | 照游標份第 8 節改寫 | 「隔兩 tick 讀不到」不再成立；修剪不在 Plan 裡；巢狀跑不修剪 | 跳過的讀者下次補讀；恰好一次擴大到全部跑過的讀者（上限內）；上限與聚合的丟棄記錄 | Kernel |
| G4 | J-39 的 tick 部分（`CoverageTests:302`；DI-KS 8 的兩句）、`rs-adv-domain-tick-is-zone-b`、`rs-j12-tick-one-mount` | tick 只有一個來源、(c) 區讀得到 tick | 刪 | System 可以寫 `Tick`；(c) 區讀不到 `Tick` 直到第 4 批 | 無 | 第 4 批權威的專用檢查（權威 5.4）；空窗是使用者已接受的（`-authority-guard-gap-later`） |
| G5 | J-12 的 `CurrentTick` 案（`SignatureTests:147`、`:260`）、J-30 的 tickStamps（`ExecutorTests:478`）、`we-adv-tickstamp-never-recorded`、J-56 的 tick 前進（`NestedRunTests:148`、`:889`）、`ns-nested-tick-not-advanced` | `CurrentTick` 的存取推導、tickStamps（每個 tick 的第一個 stamp）、巢狀跑推 tick | tick 前進的性質**搬到**時間 Module 的測試（`RunOneFixedStep` 推 tick、巢狀跑 k 次 tick 加 k）；`CurrentTick` 的推導與 **tickStamps 刪、不搬** | Kernel 不推 tick；「某個 tick 開始時的 stamp」沒有任何地方保證 | 無 | tick 前進：時間 Module。tickStamps 不搬的理由：Kernel 內沒有任何判斷讀它（`recon-a` 1.6），只有 J-30 一個測試；需要「某個 tick 以來改過什麼」的作者用 `Changed<…>` 或自己記 |
| G6 | 編排層 `[or-ended-failure-tick]`（`tests/common/orchestration/CoverageTests.cpp:209` 判 `f.domain == Frame && f.tick == 1`、`:313` 文字含 `"tick 1"`）——**O-11 的 Design Invariant 守衛**；`or-failure-no-domain`、`or-failure-no-tick`、`or-failure-text-no-tick` | `WorldFailure` 帶壞掉那張 schedule 的 domain 與 tick | `domain` 的判定刪、突變退役；`tick` 改判同步點序號，兩個突變改寫 | 錯誤內容不再帶 domain 與遊戲時間 | 無 | 編排層（同步點序號） |
| G7 | **由 PR #249 處理**（突變清點卡，使用者 2026-11-19「拿掉 PRODUCT_ALIASES 你直接處理」；#249 已併入 `gyo2`（4347ddea，2026-11-19；同日 #250 也拿掉 entity_store W1 的舊產品名），本批 rebase 時收 #249 的版本）。K1 的舊產品別名（`kernel_guard.py:89-94`、`:299`；`test_kernel_guard.py:125-127`、`:433-435`；`ks-k1-roles-dropped` 的 find） | Engine 不出現 RetroFPS、`fps::pvp`、`Pvp`、`Gyo::Tools` | 拿掉清單與兩個案例；突變改 find | K1 不再擋這四個名字 | 無 | 「Engine 不出現產品名」由角色詞 `ROLE_WORDS` 照舊守。使用者已定方向（`-k1-no-legacy-product-aliases`） |
| G8 | J-55 的 tick（`SpawnRejectTests:591`、`:957`）、`sr-tick-fixed-domain`、`sr-tick-one` | 記錄帶的是丟棄它那張 schedule **所屬 domain** 的 tick | 改成同步點序號 | 「屬於哪個 domain」消失（World 只有一個同步點序號） | 無 | 「記錄來自丟棄它的那次、一筆、不設上限、不取 stamp」照舊（`sr-no-record`、`sr-records-capped`、`sr-drop-takes-stamp` 照舊殺） |

DI-KS 的文字變更（第 10.1 節）同樣在這次逐條核准。

### 6.2 判定不變、觀察方式換的（沒有緩的面；事後報告 1～5，證明：舊判定殺的突變新判定也全殺）

- 用 `CurrentTick` 當「第 N 次才動作」的閘門（約 60 處：`InitialValue` 19、`World` 17、`SpawnReject` 13、`HookFlush` 9 等）與用 `World::Tick`／`TickStamp` 當觀察（約 79 處）：**改由測試在兩次 Run 之間從外面寫一個計數 Resource**（World 是 Ready 時 `SetResource`），不加 System——加 System 會多一個 Plan 步、多取 stamp，動到寫死 stamp 與步序的期待（例：`we-j30-skip-no-stamp` 的 `{0, 6, 3, 1, 12, 9, 1}`、`HookFlushTests:286-297` 的 `StampsPerRun`）。(c) 區的 rig（`HookFlushTests:147`、`:740` 的 `ZoneCSpawnsSeed`、`ExecutorTests:1019`、`:1081`、`InitialValueTests` 的 `SimulationSpawnsSpark` 五處）用 zone C、role=Input 的 boundary Resource 當計數。巢狀跑要逐次前進的案例歸 G5。`SkeletonTests.cpp:196`（八個 tick 的手算）同樣處理。逐案試不出「同值」的，改列進 6.1 另請核准。
- 改名：`EventReader`→`MessageReader` 等（`SignatureTests`、`RegistryTests:43,68`、`HookFlushTests`、`NestedRunTests` 用事件記錄的案例）；find 含 `EntryKind::Event`（9 個）、`AccessKind::Event`（4 個）的突變同步改 find。`naming_guard.py`、`kernel_symbols.py` 現在沒有列舉事件的名字（`recon-a` 6.1），不用改。
- golden 重產（第 4.4 節）；寫死版本號的 3 個突變（`sr-plan-version-not-bumped`、`rs-iv-registry-version-1`、`rs-b6-cfp-version-1`）改成新版本號。
- `ks-diks15-code-added-without-text`（find `FE1,\n};`）改 find 到新的 enum 尾；`hf-startup-skipped`、`hf-rounds-follow-trim`、`hf-trim-before-rounds` 的 replace 用到要拿掉的符號：改寫成仍殺同一個案例，做不到的退役並寫明理由（退役＝緩的面，屆時列進 6.1）。
- 預設骨架（第 3.6 節）讓既有的編排層測試與 scenario 第 1 段的 World 多兩張空的 schedule 時：改成明寫「關掉預設骨架」，判定不變；做不到同值的改列 6.1。
- scenario 第 1 段：S1-1～S1-10、S1-7b 的判定不變。第 2 段用**自己的一份產品描述**（帶 `WorldTimeModule`、`Walk` 在固定步裡），第 1 段的登記只拿掉 domain 宣告（S2-9），`RunFrame`×5 走 5 格的前提不變。編排層清單裡 `target: gyo_scenario_tests` 的 22 個突變照跑。

### 6.3 第 2 批延後的綁實作守衛（`-guards-after-mechanism-stable`；條件＝涵蓋它的 scenario 段通過，第 1 段已通過、已合併）

| # | 守衛（含守它的突變） | 判定 | 理由 |
|---|---|---|---|
| 1 | `naming_guard.py` 的 `ROOTS` 加 `engine/orchestration` | **第 1 步立** | 本批只換 `WorldFailure` 的兩個欄位，不動編排層的命名 |
| 2 | 編排層的 include 掃描（O-4） | **第 1 步立** | 時間 Module 依賴編排層，編排層不 include 它 |
| 3 | 編排層不讀 Plan 的步驟、不呼叫 System（O-3） | **第 1 步立** | 本批不改 runner |
| 4 | 引擎各 CMakeLists 不手寫引擎內的 `GYO::` link（O-6） | **第 1 步立** | `world_time` 照登記檔產生 link，正好受它檢查 |
| 5 | `gyo_expect_links(gyo_orchestration GYO::Base GYO::Kernel)` | **第 1 步立** | 不改編排層的依賴 |
| 6 | Kernel 不出現 `Plugin`、`FinishContext`、開局識別字的常駐掃描（DI-KS 15） | **第 1 步立**（現有三類） | 第 1、2 批的設計已穩 |
| 新 | 6 號加時鐘的識別字；`naming_guard` 加 `engine/world_time`；`world_time` 的 `gyo_expect_links` | **延到第 4 批開頭** | 本批剛改的設計，scenario 第 2 段通過、合併前核准之後才立（和第 2 批同一個處理） |

1～6 的突變進編排層清單（1、2、3、5）與 Kernel 清單（4、6），定案後和本批其他新突變一起受 Fable／high 的敵對式檢查（第 6.5 節）。

### 6.4 新增的行為案例

- **時間的型別**（`tests/common/world_time/TimeTypesTests.cpp`，連 `GYO::WorldTime`、因而連到 Kernel；測試內容和 T1 判定 1 相同）：T1 判定 1 的全部案例（累加、餘數、不丟時間、沒有 tick、扣整步、步數不超過上限、上限的斷言、量與步數、舊測試的期待值、和舊 `double` 版逐點比、暫停、倍率、零頭、沒有固定步的 250 ms、步長的設定〔`FromHz(60.0)`→16,666,667；`FromHz(0)` 與非有限值斷言〕、`RealTime`、前置條件），加 `Reset` 回傳清掉的量、型別沒有 padding 的 `static_assert`。
- **時間 Module**（`tests/common/world_time/`）：全部 Resource 已登記；`Resimulation` 恆在、初始 false；`pausable=false` 時沒有 `TimeControl` 而 `AdvanceTime` 照跑；手動取樣兩種模式；建好 World 後第一次取樣 `delta`＝0；固定步跑 k 次、`Tick` 加 k、第 n 步讀到 `Tick`＝n；固定步裡 `ActiveTime`＝（步長、`FixedTime.elapsed`）、跑完換回；追不上時 `discarded` 與 `TimeDiscarded` 一筆（原因＝整步上限）；`RunFixedSteps` 被 run 條件跳過的那一幀，下一幀 `AdvanceTime` 把沒取走的量記進 `discarded`、發 `TimeDiscarded`（原因＝沒被驅動）；`RewindFixedSteps` 後重跑讀到的 `Tick`、`ActiveTime` 和第一次相同；k 步的第 j 步回 Err：之後不跑、Err 傳出、World 中斷；固定步那一張不存在時 `Check` 報錯；掛載目標不存在時 Kernel 報 D-E1；`TickWriters()` 的內容；讀檔放回 `Tick`、`VirtualTime`、`FixedTime` 狀態後接著跑，步長以設定為準。
- **保留式、推送式**：游標份第 8 節「新：保留式」「新：推送式」兩列全部；加 4.2 定的：`Commands` 觸發在 F.2 最後一步、依記錄先後；新增與移除的逐格先後；`ExclusiveWorld::Trigger` 的延後變更走當場改成員的清空輪；修剪後存取驗證不誤報；hook 帶 `MessageReader` 編譯失敗。
- **include 順序**：`world_time` 的標頭與 Kernel、編排層的標頭各種順序都編得過。

### 6.5 突變（D184）

- 現在落在範圍內、要逐個處置的（退役、改 find、改寫，各寫明）：find 落在範圍的 Kernel 40 個＋編排層 4 個；寫死版本號的 3 個；expect 依賴 tick 測試的 5 個（`recon-a` 6.3 D 組）。
- 殺它的案例會被改寫的：J-55 63 個、J-54 60 個、J-56 41 個、J-32 26 個、J-36 5 個、J-30 4 個（檢查者用 Python 掃 `cases`），加 scenario 22 個；每步照 D184 重跑，實際名單在執行時列進 dev_log。
- 新的清單：`docs/architecture/target/v2/scripts/world_time_mutations.json`（時間型別與時間 Module；T1 判定 2 的 22 個全部列入）；保留式、推送式的新突變與 6.3 的 4、6 號加進 Kernel 清單；6.3 的 1、2、3、5 號加進編排層清單。各步實作前定案，**交 Fable／high 敵對式檢查一次**（§14.2）。
- 合併前背景完整跑一次（Kernel＋編排層＋`world_time`）。只有預期的案例失敗才算殺，建置失敗不算。

---

## 7. scenario 第 2 段（`tests/common/scenario/`，ctest `scenario.segment2`，標籤 `cpu`）

自己的一份產品描述（帶 `WorldTimeModule`、64 Hz、`TimeSampling` 用手動「每幀經過時間」）；`Walk` 在固定步裡每步 +1。前提：有固定步時 250 ms 的上限不作用（`-t1-three-items` 第 2 條）。

| 步 | 做什麼 | 檢查 |
|---|---|---|
| S2-1 | 建 Product | 時間的 Resource 都在、`Tick`＝0 |
| S2-2 | 每幀經過 15.625 ms，跑 10 幀 | `Tick`＝10、玩家在 (10,0) |
| S2-3 | 一幀經過 3 步的量 | 這一幀固定步跑 3 次、位置 (13,0) |
| S2-4 | 加分是 `Message` `ScoreGained`（固定步寫、每幀讀） | 每幀的讀者讀到的筆數＝這一幀跑的步數；經過 0 的那一幀讀到 0 筆 |
| S2-5 | 暫停（`TimeControl`）2 幀 | `Tick` 不動、每幀的 System 照跑 |
| S2-6 | 卡住：一幀經過 1 秒（餘數 0） | 跑 5 步、`TimeDiscarded` 一筆、`discarded` 增加 59 步的量 |
| S2-7 | 金幣實體被拿掉 | `Remove<Coin>` 的 observer 在同步點跑一次、在 OnRemove hook 之前 |
| S2-8 | 存檔（`Tick`、`VirtualTime`、`FixedTime` 的狀態、玩家、分數；結構沒有 padding）→ 換新 World 讀檔 | 新 World 的 `Tick` 接著存檔時的值；再跑 5 步位置接著加；第 1 幀 `RealTime.delta`＝0 |
| S2-9 | 第 1 段拿掉為 D-E12 加的 domain 宣告 | 第 1 段照樣通過 |

失敗的分類照 `-scenario-failure-by-contract`（骨架宣告 8.4）。

---

## 8. 要改的程式與文件

| 位置 | 內容 |
|---|---|
| `engine/world_time/include/engine/world_time/TimeTypes.hpp`、`TickAccumulator.hpp`（新） | 純型別、`AdvanceVirtual`、更新 `RealTime` 的函式、步長與上限的設定（`engine/time` 不動） |
| `engine/world_time/`（新） | `WorldTimeModule`、`Tick`、`Resimulation`、`TimeSampling`、`TimeDiscarded`、`AdvanceTime`、`RunFixedSteps`、`RunOneFixedStep`、`RewindFixedSteps`、放回固定步狀態的函式、`TickWriters`；`CMakeLists.txt` 照登記檔 |
| `engine/config/modules.csv` | `world_time` 一列 |
| `engine/kernel`（`Declarations.hpp`、`Params.hpp`、`Access.hpp`、`SystemTraits.hpp`、`Plan.hpp`、`detail/Storage.hpp`、`detail/Context.hpp`、`World.*`、`Scheduler.cpp`、`Executor.cpp`、`Registry.*`、`Fingerprint.cpp`、`KernelError.hpp`） | E3、E4、E6 |
| `engine/orchestration`（`OrchestrationError.*`、`Product.cpp`、`Setup.hpp`） | `WorldFailure` 的兩個欄位；預設骨架（第 3.6 節） |
| `tests/common/{time,world_time,kernel,orchestration,scenario}` | 第 6、7 節 |
| `tests/common/kernel/kernel_guard.py`、`test_kernel_guard.py`、`naming_guard.py`；編排層的掃描 | 第 6.1 G7、6.3 |
| `docs/architecture/target/kernel.zh-Hant.md` | DI-KS 1、8、9、10、11、12、14、15（含「轉換中」表那一列，`:77`）、25 的變更（第 10.1 節） |
| `docs/architecture/target/v2/orchestration.zh-Hant.md` | O-1 的過渡句（遊戲初始化的 schedule 要屬於一個 domain）拿掉；O-11 的 `WorldFailure` 欄位 |
| `docs/architecture/target/determinism.md`、`target/world.zh-Hant.md`（4 處）、`target/v2/system-contract-page.zh-Hant.md`（`:54`、`:61`） | domain tick、clock schedule、`EventReader`、`CurrentTick` 的文字照新形改 |
| `docs/architecture/target/v2/world-time.zh-Hant.md`（新） | 時間 Module 的 Design Invariants、職責清單、Replacement Criteria |
| `rebuild-time-design.zh-Hant.md`、`time-types-declaration.zh-Hant.md` | 第 547、552 行改寫；T1 開頭標「由本宣告取代」 |
| `docs/dev_logs/2026_11_xx_gyo2_time_events.zh-Hant.md` | dev_log，和本批同一變更 |

**不碰**：`MonotonicClock.hpp`、`Waiter`、`LateWakeStats`；權威、回滾、區規則（第 4 批）；摘要的雜湊方式（第 4 批）；`.github/`。

**和編排層骨架卡那條 PR 的先後**：骨架卡要另開 PR 補 Kernel 自己的型別（`RangeAllotter` 等）與摘要契約（`-summary-padding-a-then-c`），會改 `detail/Storage.hpp` 與 `World.cpp` 的摘要段。本批第 3～5 步也改這兩個檔。**那條 PR 先併進 `gyo2`，本批第 3 步開始前 rebase 上去**；那條還沒併時，第 1、2 步（不碰這兩個檔）照做。

---

## 9. 基盤 Architecture 的根據與 Architecture Delta

### 9.1 為什麼算基盤

每個讀時間的 System 照 `ActiveTime` 等的欄位寫；每個讀寫事件的 System 照 `Message`／`Event` 的 API 寫；權威、回滾、狀態機、讀檔、音效、網路都積在這兩樣上面（統合 5.1、游標份 7.1）。

### 9.2 予見的根據 → 要求、預算

| 根據 | 要求 |
|---|---|
| 名指的 Feature（第 3.2 節、游標份 7.2） | 第 3.2 節的 Contract 欄 |
| 參考實作：Bevy `bevy_time`、lightyear 的 timeline 與回滾、Unity `NetworkTime`（`recon-c` 二） | 型別的切法、`ActiveTime` 的換入換出、tick 在固定步開頭加 1、累計值與退回、「重跑中」在時間那一側 |
| 規模：64 Hz、一幀最多 5 步；訊息預設 1024 筆 | 下面的預算 |

**預算**（判定 5）：
- **時間**：時間 Module 每幀的額外成本（`AdvanceTime`＋`RunFixedSteps` 的固定開銷，不含固定步裡作者的 System），在本機 Release 版每幀 0／1／5 步各量一次，**不超過 64 Hz 一步（15.625 ms）的 1%（156 µs）**。根據：沒有產品給過預算；1% 是讓時間 Module 不成為固定步預算的一項。超過就停下回報（§14），不放寬。
- **記憶體**：游標份 7.3 算的（預設 1024 筆、內容 32 位元組約 40 KB／種）與實測比：scenario 第 2 段與一個「100 種訊息全部頂滿」的測試量訊息緩衝的峰值，記進 dev_log；超過計算值 2 倍就停下回報。

### 9.3 Architecture Delta（七項）

1. **Feature**：第 3.2 節；游標份第 6 節第 1 項。
2. **現在的問題**：時鐘與 tick 在 Kernel（使用者判「遊戲需求入侵了 Engine 設計」）；World 裡沒有任何時間的 Resource；事件依 Plan 位置與 tick 讀、修剪是 Plan 裡的一步、跨時鐘是啟動錯誤；沒有推送式。
3. **變化的邊界**：Kernel 少了時鐘、事件 API 改名加游標、多了推送式；`engine/time` 不動；**新的 Engine Module `engine/world_time`**（新 Subsystem）；編排層 `WorldFailure` 少一個欄位、一個換義；**編排層多了預設骨架**（改第 2 批已合併的部分，使用者 2026-11-19）。
4. **影響**：第 8 節。
5. **依賴方向**：`world_time` → `orchestration`、`kernel`、`time`、`base`；`time` 照舊只連 `base`＋`Threads`；Kernel 與編排層不依賴它（K5、O-4）。權威、回滾將來依賴 `world_time`（登記檔）。
6. **Ownership**：tick 從 Kernel 移到 `world_time`；「事件留多久」從「時鐘＋Plan 的修剪步」變成 Kernel 的游標與上限；「這一輪跑幾次固定步」是 World 裡的 System；丟棄與診斷紀錄的時間標記變成 Kernel 自己的同步點序號；取樣暫時在 `world_time`（交換點之後移給 runner）。
7. **為什麼不能更小**：拿掉時鐘而不同時提供時間 Module，scenario 與將來的權威、回滾沒有 tick；只改名不加游標，拿掉 tick 後讀者沒有讀取依據（游標份 6.7）；推送式是使用者定的兩種之一。

**對編排層 RC 的判斷**：時間 Module 是第一個接進編排層的真實 Module。它只用登記單位、`modules.csv`、準備時機與預設骨架，不改編排層的迴圈（`WorldFailure` 的兩個欄位是拿掉 Kernel 時鐘的後果；預設骨架是使用者定的、四家都由引擎給，不認得時間，也不是為時間 Module 開的特例）——RC-O1 不成立；scenario 第 2 段不需要編排層認得時間——RC-O8 不成立。實作中若出現其一，照 RC 停下。

### 9.4 系譜、退掉的選項、需要它的 Feature（AGENTS §4）

| 決定 | 系譜 | 退掉的 | 決定的理由 | 需要它的 Feature（Product） |
|---|---|---|---|---|
| 遊戲時間全部在 `world_time`、`engine/time` 只留時間基準 | Bevy、flecs、Unity 都把基本時間放在 OS 介面層、遊戲時間另成一處 | 型別放 `engine/time`、System 放 `world_time`（修訂 1～3）；全部放 `engine/time` 並連 Kernel | 使用者 2026-11-19，「一起確認」第 3 條，`v8-arch-gyo2-game-time-in-world-time`：一個概念不分成兩個實作 | 固定步、暫停（①②③） |
| `FixedTime` 帶累計、退回用函式 | Bevy、lightyear、Unity 都是累計值 | `Tick`×步長（修訂 0；GYO 新導出，Client 對時改 `Tick` 時累計會跳） | 照三家一致的做法；退回在 `RewindFixedSteps` 一處 | 回滾（① Client）、作者讀固定步時間（②） |
| 累加器 `Add` 回傳步數 | 舊 `FixedTickRuntime`；Bevy 一次 `expend` 一步 | 照 Bevy | 結果相同；`Add` 後餘數一定不足一步（T1 第 6 節） | 固定步（①②） |
| 一幀步數上限（預設 5） | 舊的；Bevy 只靠 250 ms | 只用時間長度上限 | 使用者 `time-plan-two` (4) | 追不上時（①②） |
| `RunOneFixedStep`、`RewindFixedSteps` 公開 | lightyear 回滾時手動跑 n 次（`rollback.rs:1186-1291`） | 步的 schedule 自己推 tick | 推 tick 與換 `ActiveTime` 只在一處 | 回滾（① Client） |
| 取樣用 `TimeSampling` | Bevy `TimeUpdateStrategy` | runner 在交換點寫 | 交換點本批沒有；形狀相容 | 測試與重放（①②③） |
| 沒被取走的量記成 `discarded` | GYO 新導出（T1 2.2 留下的洞） | 寫成前提、不檢查 | 使用者「丟了多少有記錄」 | 追不上、Loading（①②） |
| 編排層給預設骨架、時間 Module 只往上掛 | Bevy `bevy_app` 的 `Main`；flecs pipeline；Unity、Mass | 時間 Module 自己宣告；作者宣告；`default=no` | 使用者 2026-11-19（第 11 節第 3 件，`v8-arch-gyo2-orchestration-gives-schedule-skeleton`） | 單機作者只寫遊戲的 System（②）；Match（①） |
| `Module` 預設帶（`default=yes`） | Bevy `DefaultPlugins`、Unity 內建模組 | `default=no` | 骨架宣告已定「Module 預設全帶、產品可以關」 | 單機不必另外列（②） |
| hook 與 observer 共用佇列 | flecs、Bevy | 分開 | 同一個 F-12 上限、作者只記一套 | 元件新增時當場反應（②） |
| `TimeDiscarded` 用保留式 | Bevy 的 World 層級通知用 Message | 推送式 | 讀者（日誌）的節奏和時間 System 不同 | 追不上的記錄（①②） |
| 丟棄與診斷用同步點序號 | GYO 的 `syncEpoch` | stamp | 第 4.2 節 | 讀檔、診斷（①②③） |

既有的 Test 與守衛能不能不改，**沒有**當作任何一項的決定理由。

---

## 10. Design Invariants 的變更（逐條要你核准；最終文字在合併前核准）

### 10.1 Kernel（`target/kernel.zh-Hant.md`）

- **DI-KS 1**：「`ExclusiveWorld` 不含事件」→「不含 `Message` 的讀寫；可以當場 `Trigger` 推送式事件」。
- **DI-KS 8**：拿掉 tick 的兩句；區規則本身第 4 批拿掉；事件的 zone 照舊。
- **DI-KS 9**：同步點的順序表加最後一步「`Commands` 觸發的自訂事件，依記錄先後」；新增、移除時 hook 與 observer 的逐格先後。
- **DI-KS 10**：照游標份第 9 節改寫（兩種事件）。
- **DI-KS 11**：補「和 observer 的先後見 DI-KS 10」。
- **DI-KS 12**：拿掉「domain tick 只有一個來源且在快照內」「Fixed domain 事件緩衝」；其餘第 4 批改寫。
- **DI-KS 14**：拿掉「每個 domain 一個 clock schedule、每個 domain 的事件只在它的 clock schedule 內」與「時鐘的部分隨時間 Module 那一批移出」。
- **DI-KS 15**：第 (2) 項拿掉「（推送式隨事件那一批加入）」；「轉換中」表裡時鐘那一列刪；「不認得時鐘」照舊。
- **DI-KS 25**：唯讀登記表加 observer（觸發、型別、存取、登記者）、每種訊息的上限與讀者名單（游標份 5.1、5.2）。
- **RC**：加游標份第 9 節的四條。

### 10.2 編排層（`target/v2/orchestration.zh-Hant.md`）

- **O-1**：拿掉「在第 3 條之前，遊戲初始化的 schedule 要屬於一個 domain」。
- **O-4**：補「編排層給 schedule 的預設骨架（名字與段落順序；作者可換或關）並經 Kernel 執行它；不認得時間，也不認得任何 Module 掛進去的是什麼」（第 3.6 節，`v8-arch-gyo2-orchestration-gives-schedule-skeleton`；本卡判不算職責清單多一項，**動工前核准時請你一併看**）。
- **O-11**：`WorldFailure` 帶同步點序號、不帶 domain 與 tick（G6）；加一段（第 11 節第 1 件，使用者 2026-11-19，`v8-arch-gyo2-world-events-not-via-event`，含使用者「補過去」的三點）：「通知只在發出它的 World 健康時經 Kernel 的事件發；致命錯誤與 World 自己的壞掉、建立、替換不經事件，結構化錯誤回給 runner，最後一道把它寫出去的路直接呼叫、不經任何機制。World 健康時的警告（丟棄記錄、`TimeDiscarded` 等）照常經事件。World 的建立與替換由編排層知道；要讓新 World 裡的 System 知道，在它準備好之後往裡面寫 `Message`（讀檔那一批定）。」

### 10.3 時間的型別（併進新頁 `target/v2/world-time.zh-Hant.md`；`time.md` 不加，使用者 2026-11-19，「一起確認」第 3 條，`v8-arch-gyo2-game-time-in-world-time`）

1. 累加器只做整數的算術：不讀時間基準、不記 tick、不丟時間；從上一次 `Reset` 起，加進去的總量恆等於步數×步長＋餘數；`Add` 之後餘數不足一步。
2. 超過上限的量只在 `AdvanceVirtual` 一處算；有固定步時扣整步。
3. 純型別與算術的標頭不引用 Kernel、不 include `MonotonicClock.hpp`（同一個 Module 裡的 System 才引用 Kernel）；沒有 padding。
**Replacement Criteria**：時間 Module 要正確必須知道累加器的內部狀態；或 `ActiveTime` 需要第三種來源。

### 10.4 時間 Module（新頁 `target/v2/world-time.zh-Hant.md`；WT 是本卡加的代號）

- **WT-1 登記**：建進一個 World，就登記第 3.3 節的 Resource（`TimeControl` 照設定）；`Resimulation` 恆在、初始 false，時間 Module 自己不寫它。
- **WT-2 取樣與推進**：每個 World 每幀只取一次樣（`AdvanceTime`）；`RealTime` 不受暫停、倍率、上限影響；扣掉只在 `AdvanceVirtual` 算；扣掉的量與沒被取走的量都累計進 `discarded` 並發一筆 `TimeDiscarded`，`discarded` 只由 `AdvanceTime` 寫。
- **WT-3 tick 與固定步**：`Tick` 與 `FixedTime.elapsed` 只經 `RunOneFixedStep`（前進）與 `RewindFixedSteps`（退回）一起改；固定步執行中 `ActiveTime`＝（步長、`FixedTime.elapsed`），跑完換回；累加器不因退回而倒退；步長只在建 World 時給。
- **WT-4 依賴**：`world_time` 只依賴 `orchestration`、`kernel`、`time`、`base`；Kernel、編排層不認得它；程式裡的 `Engine::` 一律全限定。
- **WT-5 寫者的約定**：`RealTime`、`VirtualTime`、`FixedTime`、`ActiveTime` 只由時間 Module 寫，`TimeControl`、`TimeSampling` 由產品寫，`Tick` 另外可由回滾策略寫——**約定，不是啟動檢查**；`TickWriters()` 公開驅動者名單供權威策略檢查。
- **WT-6 職責清單**：第 3.3 節末的七項。
- **Replacement Criteria**：(a) 要正確必須讓 Kernel 或編排層認得時間；(b) 回滾或讀檔需要時間 Module 以外的地方推 tick、退 tick 或換 `ActiveTime`；(c) 某個產品的時間組法（例：固定步在別的執行緒的 World）用這些 Resource 與函式拼不出來；(d) 職責清單要多一項，或判不出屬時間 Module 還是別的 Module；(e) 需要執行中換步長。

守衛：行為測試（第 6.4 節）照常；綁實作的掃描延到第 4 批開頭（第 6.3 節）。

---

## 11. 要你定的

### 第 1 件：World 自己的事（壞掉、建立、替換）不能從那個 World 的 `Event` 發——怎麼辦？——**已定：錯誤照 D2 回給 runner，日誌 Module 從 runner 拿**（使用者 2026-11-19，side chat 轉達，`v8-arch-gyo2-world-events-not-via-event`）

**原話**：「選A，為何因為c++沒有try catch。錯誤你不知道會有多廣，到什麼程度，有可能Even機制本身已經壞了。所以你沒有辦法做Event的。如果Event不成功，就會連日誌都丟了。」（「Even」照原文，指 Event。）**理由**（使用者的）：錯誤的範圍與程度不知道，Event 機制本身可能已經壞了；經 Event 發，失敗時連日誌都會丟。side chat 把「c++沒有try catch」讀成「C++ 不能像託管語言那樣保證接住所有錯誤（例如記憶體被寫壞）」，沒有向使用者逐字確認。**定下的**：`-no-separate-eventbus` 的範圍收成「World 裡面的機制」；World 自己的壞掉、建立、替換是例外，走 D2 的結構化錯誤回給 runner；日誌 Module 怎麼接 runner 在日誌那一批定。合併前寫進編排層 O-11 一句（第 10.2 節）。

**補充三點**（side chat 的看法，使用者聽完說「補過去」，同意轉交；**不是使用者的原話**；2026-11-19）：
1. **例外的界線**：發通知的那個 World 是健康的，才走 `Event`／`Message`；致命錯誤與 World 自己的生滅不走。World 健康時的警告照常走事件（例：訊息的丟棄記錄、`TimeDiscarded`），日誌照常訂閱。所以日誌 Module 有兩個入口：平常訂閱事件，致命時從 runner 拿結構化錯誤。目的：不讓這條例外被讀成「凡是錯誤都不走事件」。
2. **最後一道直接呼叫**：照使用者的理由（報錯的路不能依賴可能壞掉的東西）往下推，runner 也可能受影響，最後一道（把結構化錯誤寫出去）直接呼叫、不經任何機制；和 D2 第 5 點「錯誤資訊在壞掉當下收集、不依賴壞掉的 World」同一條線。細節在日誌那一批定。
3. **World 建立、替換本身不是錯誤**：由編排層知道；新 World 裡的 System 想知道（例：讀檔來的），可以在新 World 準備好之後往它裡面寫 `Message`（那時它健康）。留到讀檔那一批定，不影響本件。

以下是交你定時的說明，留作紀錄。

**遊戲的例子**：PvP Client 的模擬 World 跑到第 3000 個 tick，某個 System 回了錯誤。照 D2（`-broken-world-ends-product`），World 中斷、Product 結束，`WorldFailure` 回給 runner。你在 `-no-separate-eventbus` 說「D2 的 World 壞掉」也從 Kernel 的 Event／Observer 發、日誌 Module 當訂閱者。但 `Event` 與 `Observer` 住在 World 裡：壞掉的 World 不能再跑 observer（D2：無法分辨是否健康）；「World 被建立、被換掉」時，那個 World 還不存在或正要被丟掉。

**別家**（`recon-c`）：沒有一家把這類事當 ECS 事件。Bevy 的錯誤走回呼（每個 World 一個 `FallbackErrorHandler` Resource，`error/handler.rs:116`），World／SubApp 的建立與拿掉沒有事件，日誌直接呼叫 tracing；flecs 的錯誤直接呼叫日誌函式（`log.c:451`），World 收尾用 `ecs_atfini` 回呼。理由一致：World 是事件的容器，它自己的生滅沒有讀者；日誌要在任何時點都能用。

**比過的選項**：
- **錯誤照 D2 回給 runner，日誌 Module 從 runner 拿到它**（建議）：`-no-separate-eventbus` 的範圍收成「World 裡面的機制」，World 自己的生滅與壞掉是例外；日誌 Module 怎麼接 runner 在日誌那一批定。代價：一條例外，寫進 DI。系譜：Bevy、flecs。
- **另立一個「Product 層」的事件佇列**：等於在 Kernel 外再做一套事件，就是你否決的 EventBus。
- **發到另一個健康的 World**：只有多 World 的產品有；單一 World 的產品沒有地方發。

這會改你列舉的範圍，所以交你定。

### 第 2 件：Engine 機制的送法表（第 5 節）——**已定：照建議**（使用者 2026-11-19，side chat 轉達，`v8-arch-gyo2-event-delivery-rule`）

**原話**：「嗯，這個照建議，你的歸納邏輯很清晰。」**定下的**：(1) 原則：整個 World 關心、讀的人節奏不同 → 保留式 `Message`；針對某個實體、要當下反應 → 推送式 `Event`；一直存在的狀態（暫停、第幾 tick、重跑中）→ 不發事件，讀 Resource。(2) 第 5 節的表照建議，含「時間 tick 不發事件」（縮小 `-no-separate-eventbus` 的列舉）。之後各批照這條原則定送法，不再逐批問。**比過的選項**：照原則定、時間 tick 不發 ←選／照原列舉每跑一步發一個事件（和固定步裡的 System 重複、每步多一份成本）／只採表、不立原則。**理由**：使用者說歸納邏輯清晰，沒有另外說。附帶：使用者原本以為 `Message` 是指定給某個 System 處理，side chat 更正後使用者說「那是我理解錯誤了，我還以為Message會to OOOOSystem來做的。」更正的內容寫在第 5 節原則下。

以下是交你定時的說明，留作紀錄。

你交代「逐個機制對照 Bevy、flecs，定推送式或保留式，交使用者看」。第 5 節的表是本卡的建議。**和你列舉的差別**：
- **「時間 tick」不發事件**（縮小你的列舉）：訂閱者就是固定步 schedule 裡的 System；Bevy、flecs 都不通知固定步跑了一步（`fixed.rs:243-258`、`timer.c:12-48`）。比過：每跑一步發一筆 `Event`（observer 每步跑一次，和放進固定步的 System 重複）／發 `Message`（讀者每幀讀到 k 筆，和讀 `Tick` 的差得到同樣的資訊）。
- 暫停、恢復不發（讀 `VirtualTime`）；存檔版本被拒是 `Result`。
- 新增 `TimeDiscarded`（本批做）。
- 原則本身（廣播→保留式、針對實體→推送式、連續的狀態→不發）也請你定；之後的批次照它定送法。

### 第 3 件：每幀的 schedule 骨架由誰宣告（時間設計第 0 節列的「請你定」）——**已定：編排層給骨架**（使用者 2026-11-19，side chat 轉達，`v8-arch-gyo2-orchestration-gives-schedule-skeleton`）。**推翻了本卡的建議（時間 Module 自己宣告）**

**原話**（依序）：(1) 看完時間 Module 自己宣告的說明後：「其他家呢？」(2) side chat 比過四家後：「能否展開下A。我覺得怪怪的為何由時間Module來展開？而不是由編排層給。」(3)「D可以，不認識時間，但是會執行Module，我是這麼看的，你認為呢？」**理由**（使用者的）：由時間 Module 展開很怪，應由編排層給；編排層不認識時間，但是會執行 Module。**退掉本卡建議的理由**（side chat 整理）：骨架不是時間的事；關掉時間 Module 的 World 就沒有骨架；多個 Module 各自宣告會打架、取決於登記順序；四家都不這樣放；本卡建議它的來源其實是「不碰 O-4」，正是 AGENTS §4 提醒的「替舊的形狀找位置」。**定下的**：第 3.6 節。

以下是交你定時的說明，留作紀錄（「編排層給骨架」那一項原本寫「違反 O-4、不建議」，是本卡把 O-4 讀成「不認得時鐘＝不能給骨架」，讀錯了）。

**遊戲的例子**：單機遊戲的作者只想寫「讀手把」「跳躍」「播音效」三個 System。時間 Module 要掛在「每幀那一張的開頭」與「固定步之前的那一段」，還要巢狀跑「固定步那一張」。這幾張 schedule 與 stage 誰宣告？`WorldTimeModule` 預設帶（`default=yes`），而產品不寫 `units` 時每個 World 都帶它——包括只做畫面的 World。

**別家**：Bevy 的 `Main`（`First`→`PreUpdate`→`RunFixedMainLoop`→`Update`→…）由 `bevy_app` 給，`TimePlugin` 只往上掛（`bevy_time/src/lib.rs:84-94`）；Unity 給預設的 system group；flecs 給內建的 phase——三家都是引擎給骨架。GYO 的 Kernel 不內建 schedule 名（DI-KS 15），但那條只管 Kernel。

**比過的選項**：
- **時間 Module 在名字不存在時自己宣告預設的骨架**（建議）：每幀一張 `Main`（stage `First`、`PreUpdate`、`FixedMainLoop`、`Update`、`PostUpdate`、`Last`）與固定步一張 `FixedUpdate`；作者已宣告同名的就用作者的（stage 要齊，否則 D-E1）；作者也可以在設定裡換名字。代價：時間 Module 多宣告 schedule；只做畫面的 World 若帶著它也會多兩張（不想要就在那個 World 關掉時間 Module）。系譜：Bevy（由 `bevy_app` 給、這裡由時間 Module 給，因為 GYO 的編排層不認得時鐘，O-4）。
- **作者宣告**（修訂 0 的寫法）：每個帶時間 Module 的 World 都要宣告四個名字，不然 D-E1。代價：作者負擔變大，和「預設方便」（`engine-not-rigid`）有張力。
- **`default=no`**：要時間的產品自己列。代價：和「Module 預設全帶」（骨架宣告）不一致，單機也要另外列。
- **編排層給骨架**：違反 O-4（編排層不認得時鐘）——列出來，不建議。

### 第 4 件：第 6.1 節 G1～G8 八組判定的變更——**已定：方向照建議**（使用者 2026-11-19，side chat 轉達，`v8-arch-gyo2-batch3-guard-changes-direction`）

**原話**：「這個照建議」。**定下的**：G1～G8 八組的方向都核准（動工前）；逐組的最終核准在合併前帶突變結果與 §14.4 檢證者（Fable／high）報告再看一次。**第 10 節的 DI 文字變更不在這次的範圍**（side chat 只講了 G1～G8），動工前核准時另外請你看。**比過的選項**：八組方向都核准、最終在合併前逐組再看 ←選／某幾組先不核准（例：不接受 G4 的空窗，就要把權威的「tick 只有一個來源」檢查提前到本批，範圍擴大）／G6 保留 tick（要讓編排層讀 `Tick`，違反編排層不認得時間；或給 `WorldFailure` 加作者附加的欄位，多一個機制）。**理由**：使用者沒有另外說。side chat 的講法：G1、G2、G3、G5、G7、G8 是已定決定的直接後果；要特別看的是 G4（第 3～4 批之間沒人擋寫 tick、(c) 區讀不到 tick；使用者接受過 `-authority-guard-gap-later`）與 G6（O-11 的 DI 守衛；錯誤看不出遊戲時間上壞在哪，作者要的話在自己的錯誤訊息帶 `Tick`）。

以下是交你定時的說明，留作紀錄。


每組的「守的性質、緩的面、緊的面、之後由誰守」在表裡。G1、G2、G3、G5、G8 是你已定的決定（放開跨時鐘、時鐘移出、照游標修剪、L8）的直接後果；G4 的空窗是你接受過的（`-authority-guard-gap-later`）；G6 是 O-11 的守衛（DI 的守衛一律要你核准）；G7 是你已定的方向。**方向在動工前核准；逐組的最終核准在合併前帶突變結果與檢證者（Fable／high）報告再看一次**（和第 2 批同一個做法）。第 10 節的 DI 文字同樣。

### 第 5 件：第 12 節的模型與檔位——**已定：不用 xhigh**（使用者 2026-11-19，side chat 轉達，`v8-arch-gyo2-batch3-no-xhigh`）

**原話**（依序）：(1)「敵對式審查 Fable用 hight 就夠了。新突變清單的敵對式檢查 用 Opus/high，xhight 太頻繁了，沒必要。」(2) side chat 指出 Opus 檢查 Opus 定的清單違反 AGENTS §14.2 與 navigator（檢查者要和定清單的人不同模型），列三個做法（檢查者維持 Fable／high／清單由 Fable 定、Opus 檢查／例外照 Opus 檢查），使用者回「a」（維持 Fable／high）。**定下的**：第 3、4、5 步的三段 xhigh 取消，全照 high（side chat 把「xhigh 太頻繁了，沒必要」讀成連這三段一起取消並請使用者更正，使用者沒有更正）；第 5 步的敵對式審查 Fable／high；新突變清單的敵對式檢查 Fable／high；實作者 Opus／high、審查者與 §14.4 檢證者 Fable／high 照原案。**比過的選項**：三段 xhigh＋第 5 步 Fable／xhigh 審查（原建議）／只留第 5 步的審查／都用 high ←接近此項，審查者 Fable／high。**理由**（使用者的）：Fable 用 high 就夠；xhigh 太頻繁、沒必要。這是檔位的下修，不低於 navigator 的下限（上段不用小模型、檢查者和作者不同模型）。

**補充：使用者的通用原則**（2026-11-19，side chat 轉達）。原話：「xhight 如果不是什麼需要調查的情況可以不需要的。」side chat 的讀法（向使用者講明了，使用者還沒回）：平常的實作與審查用 high；xhigh 只在需要調查時用（原因不明的錯誤、量不到的現象）；邏輯難本身不構成用 xhigh 的理由。這比 `agents/effort-gearbox.md` 現行的 xhigh 定義（「局部用在時序、協議、決定性這類容易誤判的邏輯」）窄。**本批照這條執行**（和第 5 件一致）：實作中碰到需要調查的情形（§14 的停止條件、原因不明的失敗）時，停下回報、提議用 xhigh，不自己升檔。條文要不要改由主會話或風紀委員給使用者看，本卡不改。

以下是原本的標題，留作紀錄：第 12 節的模型與檔位（四處 xhigh、Fable 的四個角色）

### 卡片照 L17 判的（使用者 2026-11-19 看過下面前 4 條；其餘照別家，未逐條看）

使用者 2026-11-19（side chat）：「等等，我發現你又回去了，很多決議其實開源Engine裡面都有，我們不是可以參考的麼。」所以照別家答得出的各條由本卡照 L17 判、不逐條問；只有各家做法不同或有分歧、或 GYO 有特殊 Feature 時才交你（本節末）。

- ~~第 2.3 節 T1 的四件~~——**已確認：照判定**（使用者 2026-11-19，side chat 轉達，`v8-arch-gyo2-t1-closed-by-time-events`）。原話：先問「如何處理不同平台，不同cpu的浮點數誤差？」，side chat 回答後「OK這個照你建議就行。」(a) 收小數改了主會話的「整數」建議，使用者知道並同意；(b) 四處都留、`Reset` 不接線；(c)(d) 由本宣告取代。比過：同意／(a) 改回整數／(b) 某一處不要。浮點的回答寫在第 2.3 節 (a) 列。
- ~~第 2.2 節 `FixedTime` 帶累計（改 T1 的「沒有累計」）~~——**已確認：照建議**（使用者 2026-11-19，side chat 轉達，`v8-arch-gyo2-fixed-time-accumulates`）。原話：「照建議」。比過：同意（照 Bevy、lightyear、Unity 累加）←選／改回 T1 的沒有累計（作者自己用 tick×步長算）／tick×步長（修訂 0；Client 對時改 tick 時累計會跳）。
- ~~第 3.1 節名字 `world_time`／`WorldTimeModule`、型別的分放~~——**已改定**（使用者 2026-11-19，side chat 轉達，`v8-arch-gyo2-game-time-in-world-time`）：遊戲時間全部放 `engine/world_time`，`engine/time` 只留時間基準，**推翻 `time-plan-two` (7)**。原話與比過的選項見第 3.1 節表。名字 `world_time`／`WorldTimeModule` 照用。
- ~~第 3.3 節的 Resource、System、函式、`Tick` 的先後、`pending` 與「沒被取走記成 `discarded`」~~——**已確認：照判定**（使用者 2026-11-19，side chat 轉達，`v8-arch-gyo2-world-time-surface`）。原話：先問「上一個問題有些變化，這個決議還成立嗎？」，side chat 逐點對過後「同意。」比過：同意／某處不同意（例：tick 從 0 開始、沒被取走的時間不記）。對過的結果寫在第 3.3 節末。
- 第 3.4 節的取樣、第 3.5 節的讀檔：**卡片照 L17 判**。取樣照 Bevy `TimeUpdateStrategy`；讀檔不放回 `RealTime`（Bevy `Time<Real>` 同樣是行程內的時間點）。
- 第 4.2、4.3 節的實作細節、第 4.5 節區規則的過渡：**卡片照 L17 判**。照 flecs、Bevy（hook 帶 `MessageReader` 編譯失敗、observer 與 hook 共用佇列、新增 hook 先／移除 observer 先、依記錄先後送達、修剪後重拍驗證快照、同步點序號）；`MessageOptions::zone` 留到第 4 批是 GYO 的批次安排。
- 第 6.3 節延後守衛、第 8 節和骨架卡摘要 PR 的先後、第 9.2 節的預算：**卡片照 L17 判**，照第 2 批的先例，沒有新的取捨。
- 第 3.6 節預設骨架的細節（`FixedUpdate` 的 stage 名、關掉骨架的選項的形）：**卡片照 L17 判**，照 Bevy 的 `Main` 與 FixedMain 的段落（side chat 那則沒有列這條，本卡照同一原則歸進來）。

**用同類的技術比過**（使用者 2026-11-19 指正比較方式：「？？？？你這個，用同樣的技術比較呀，observer 和 Bevy、flecs的observer比，message和bevy的message比。」交使用者的標準：「我是說，個家的做法不同，或者做法有分歧。或者我們有特殊feature才要我做決策的。」）——沒有要交你的：
- **Observer** 三家一致：當場觸發當場跑（Bevy `World::trigger`、flecs、GYO）；延後觸發延到套用時（Bevy `Commands::trigger`、flecs 延後模式到同步點、GYO 到同步點）；生命週期 hook 與 observer 的先後一致；同一點內 observer 的先後都不保證；observer 拿到事件內容與目標實體 id。**Message**：Bevy 的 `MessageWriter` 當場寫入緩衝，GYO 同；保留規則照游標（已核准）。剩下的差別只在「延後指令怎麼套用」：Bevy、flecs 照記錄先後，GYO 的同步點照種類分批——這是 DI-KS 9 原有的結構，不是 observer 的設計造成的，也不是本批要定的。`Commands` 觸發的自訂事件放在 F.2 的最後一步，是既有結構下的實作細節（卡片照 L17 判；理由：observer 看得到那個同步點的全部結構變更；被刪實體的元件在 Bevy、flecs 也不保證讀得到，observer 只拿 id，要用的資料放事件內容）。DI-KS 9 加這一步的文字照列在第 10.1 節。
- **步長不隨存檔放回**：Bevy 整個 `Time<Fixed>` 放回會帶步長；Unity 的固定步長是設定、不在存檔內容；flecs 沒有固定步長。各家差別小，卡片照「步長是設定」判。
- **取樣**：Bevy、Unity 由時間的 System 自己讀時鐘；flecs 由呼叫者傳 delta（`ecs_progress(world, delta)`）。GYO 先照 Bevy，交換點做出來後回到時間設計的 runner 取樣，Resource 的形狀相容，屬過渡。
- **`FixedUpdate` 的 stage 名不加 `Fixed` 前綴**：Bevy 的 FixedMain 是五張 schedule，GYO 是一張 schedule 裡的五個 stage，stage 名屬於各自的 schedule；名字的細節，卡片照命名規範判。

**仍要你看的一項（職責清單）**：第 10.2 節 O-4 補句「編排層給 schedule 的預設骨架」**不算編排層職責清單多一項**（O-4 已列「遊戲初始化與每幀的清單」）——職責清單多不多一項是 L17 必須交你的那一類，所以不歸「照別家」。

---

## 12. 誰做、用什麼、怎麼分步

上段（公開介面、Design Invariants、資料形式）。全部照 high，不用 xhigh（`v8-arch-gyo2-batch3-no-xhigh`；需要調查時停下提議）。

| 步 | 內容 | 實作者（模型／檔位） | 估計（agent 時間） |
|---|---|---|---|
| 第 1 步 | E1：`PRODUCT_ALIASES`、延後守衛 1～6 與它們的突變 | 本卡 Opus／high | 3～5 h |
| 第 2 步 | E2：時間的型別與累加器（測試先行） | 本卡 Opus／high | 3～4 h |
| 第 3 步 | E3：`Message` 改名、游標、上限、丟棄記錄、拿掉 `EventTrim`（讀者先不靠 tick） | 本卡 Opus／high | 8～12 h |
| 第 4 步 | E4＋E5＋E9：拿掉 Kernel 的時鐘、同步點序號、測試的閘門改寫、編排層的預設骨架（先做）、時間 Module、`modules.csv`、`WorldFailure` | 本卡 Opus／high | 15～21 h |
| 第 5 步 | E6：`Event`、`Observer`、`Trigger`、`On<E>`、元件 Add／Remove | 本卡 Opus／high | 8～12 h |
| 第 6 步 | E7：scenario 第 2 段、`TimeDiscarded`、golden、DI 與文件、dev_log、預算量測 | 本卡 Opus／high | 3～5 h |
| 第 7 步 | 完整突變（兩百多個被改到案例的突變的重跑在各步內）、新突變清單的敵對式檢查、§14.4 檢證、審查 | 見下 | 6～10 h |

合計 46～69 h（修訂 3 加預設骨架 3～5 h）。第 2 步不依賴 Kernel，可以先做；第 3 步排在第 4 步之前，讀者先改成游標、不靠 tick，拿掉時鐘時事件就沒有牽連；第 4 步一次做完拿掉與時間 Module，中間不留沒有 tick 的狀態；第 3 步開始前 rebase 到骨架卡那條摘要 PR 之後（第 8 節）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | Opus／high（本卡） | 上段；不用小模型 |
| 第 5 步的敵對式審查 | Fable／high（使用者 2026-11-19：不用 xhigh，`v8-arch-gyo2-batch3-no-xhigh`） | observer 的重入與先後；和實作者不同模型 |
| 審查者 | Fable／high | 和實作者不同模型 |
| §14.4 檢證者（G1～G8） | Fable／high | 第 2 批同一個指定 |
| 新突變清單的敵對式檢查 | Fable／high | §14.2；和定清單的人不同模型 |

超過估計 1.5 倍（約 104 h）或單一問題累計超過 2 小時就停下回報。

---

## 13. 判定（先宣告，結果出來之後不放寬）

- **判定 1：通過**。本機完整 ctest 全過（含 `scenario.segment1`、`segment2`）；L1 三平台（linux-x64、windows-x64、macos-arm64）全過；`gyo_time` 的連結檢查照過（`engine/time` 沒變）；include 順序守衛照過。比摘要的測試照 `-summary-padding-a-then-c` 標「未完全解決」，它們在 L1 的偶發失敗照那個決定處理，不算本批的回歸——但本批新加的摘要內容（游標表、紀錄）不得是新的不穩定來源（`static_assert` 守）。
- **判定 2：行為**。第 6.4 節的案例全過；第 7 節 S2-1～S2-9 全過。
- **判定 3：檢出力**。新突變清單經 Fable／high 檢查補齊後全殺；第 6.5 節列的舊突變照 6.1、6.2 處置後，留下的全殺；分母以執行時的實際清單為準；完整跑（Kernel＋編排層＋`world_time`）結果記進 dev_log，活著的逐個說明。
- **判定 4：一次性的掃描**（git grep，`engine/` 與 `tests/`，文件與 dev_log 除外；每個樣式先在現在的程式命中一次當正例，終了碼區分「沒有」與「失敗」）：`\bDomain::(None|Fixed|Frame|Loading)\b`、`\bCurrentTick\b`、`\bDomainTick\b`、`[eE]ventTrim`、`\bEventReader\b`、`\bEventWriter\b`、`\bEventOptions\b`、`\bEventPayloads\b`、`\bTickStamp\b`、`\.clock\b`（正例：`schedule.clock`）零命中；`tests/common/kernel/kernel_guard.py` 裡 `PRODUCT_ALIASES` 零命中。
- **判定 5：預算**（第 9.2 節）：時間 Module 每幀額外成本 ≤ 156 µs（0／1／5 步各一次，本機 Release）；訊息緩衝峰值 ≤ 游標份 7.3 計算值的 2 倍。超過就停下回報。

---

## 14. 範圍的限界

- 這是宣告，沒有試做；第 12 節的時間是讀程式量估的。
- 系譜照三份偵察與檢查者重對的行號；Unreal 不在本地；Bevy `Duration::mul_f64`、`from_secs_f64` 怎麼捨入沒有查（GYO 的倍率零頭與頻率換算是自己的規則）；flecs 的行號檢查者沒有重對。
- 浮點 +0／-0、NaN 在原始位元組雜湊下不同：本批不處理，第 4 批的逐型別雜湊解。
- `TimeSampling` 讀時間基準的模式在 L1 上只驗「時間往前、第一次 `delta` 是 0」，不驗確切的值。
- 「本卡照 L17 判」的各條沒有經使用者逐條看，第 11 節「一起確認」交使用者一次看。
- 修訂 1 沒有再送檢查。

---

## 15. 修訂 0 檢查的處理（`declaration-r0-adversarial-check.md`）

| 檢查 | 處理 |
|---|---|
| B1 「時間 tick」被判成不發卻沒上報；送法表該交使用者 | 第 5 節加「和你列舉的差別」欄；第 11 節第 2 件 |
| B2 第 1 節移除時的先後寫反 | 改成 observer 先、hook 後、再銷毀 |
| B3 `-summary-padding-a-then-c` 沒反映完 | 第 0 節版本、2.2、3.5（存檔無 padding）、4.4（J-32 標未完全解決）、第 8 節（和骨架卡 PR 的先後）、判定 1、限界（±0） |
| B4 §14.4 歸類錯 | 加 G6（O-11）、G7（`PRODUCT_ALIASES`）、G8（J-55 的 domain）；G5 寫明 tickStamps 刪、不搬與理由；scenario 第 1 段寫明不變（第 2 段用自己的產品描述） |
| B5 區規則的過渡 | 第 4.5 節 |
| B6 `Check` 報不到掛載目標 | 第 3.3 節分開寫：掛載目標＝Kernel D-E1、固定步那一張＝`Check`；G2、6.4 照改 |
| B7 判定 3、4 照字面做不到 | 判定 3 分母改成實際清單、第 6.5 節補齊；判定 4 改成帶邊界的樣式、各有正例、補小寫與其他符號 |
| B8 `Tick`×步長的系譜錯、換步長倒退 | 改成照三家的累計值＋`RewindFixedSteps`；本批不做執行中換步長、`Reset` 不接線、RC (e) |
| S1 骨架由誰宣告 | 第 11 節第 3 件 |
| S2 沒被驅動的那一幀 | `FixedTime.pending`；沒被取走記成 `discarded`＋`TimeDiscarded`；6.4 案例 |
| S3 `TimeControl` 不登記時的簽名 | 第 3.3 節：兩個簽名 |
| S4 讀檔放回 `RealTime`、步長 | 第 3.5 節的清單 |
| S5 錯誤路徑 | 第 3.3 節、6.4 案例 |
| S6 函式樣板 | 第 3.3 節 |
| S7 閘門改寫會動期待值 | 第 6.2 節：從外面寫計數、(c) 用 boundary Resource、逐案試不出同值的改列 6.1 |
| S8 推送式四個交界 | 第 4.2 節四列；DI-KS 9 |
| S9 `WorldFailure` 少了 tick | 4.3、G6 |
| S10 取樣的三個後果 | 第 3.4 節 |
| S11 預算 | 第 9.2 節、判定 5 |
| S12 檔位與估計 | 第 3、4 步加 xhigh 段；估計加大 |
| S13 DI 與文件清單 | 10.1 加 DI-KS 9、15 的表、25；10.2 O-1、O-11；第 8 節加三份文件；9.3 寫 RC-O1、RC-O8 |
| S14 §4 表 | 9.4 加「需要它的 Feature」欄與五列 |
| （修訂 2、3）使用者決定 | 第 11 節第 1～3 件；第 3 件推翻本卡建議，加第 3.6 節、E9、第 10.2 節 O-4、估計 |
| nit 1～12 | 全部照改（`modules.csv` 的欄、路徑、編譯失敗測試的名字、`syncEpoch` 的語意、`Main` 與 `Update`、`Tick` 的定義、World 專用型別放 `world_time`、不 include `MonotonicClock.hpp`、引 `-same-input-same-result`、頻率捨入不寫成照 Bevy、S2-2 用經過時間模式與 250 ms 前提、`SkeletonTests:196` 與名字清單） |
