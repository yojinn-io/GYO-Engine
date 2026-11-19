# 事前宣告：時間 Module＋事件（`v8-arch-gyo2-time-events`）

更新：2026-11-19（修訂 0）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：待換模型檢查與使用者的動工前核准。核准之前不改程式、條文、守衛。**

性質：實作段第 3 批的事前宣告（AGENTS §3 的 Architecture Delta、§2 的基盤 Architecture、§14.4 的判定變更、Design Invariants 的變更）。

用詞：整條線分「設計段／實作段」；每批的核准分「動工前核准／合併前核准」；一批之內的實作步驟叫「第 N 步」。本頁標「**本卡照 L17 判**」的是這張卡自己的判斷（名字照命名規範、細節照別家原始碼、批次內的先後），**不是使用者的話**；本頁新加的詞在第一次出現時標「本卡加的詞」。

---

## 0. 根據與讀的版本

**程式**：`origin/gyo2` eedc9a00（2026-11-19，#247 之後）。講現況的每一句都來自讀程式的偵察（主 checkout `build/evidence/gyo2-time-events/`）：
- `recon-a-kernel-facts.md`：Kernel 的時鐘、事件、hook、巢狀跑、指紋、測試與突變（Opus／high）。
- `recon-b-orchestration-facts.md`：編排層、`engine/time`、include 順序、守衛、scenario、突變的基礎設施（Opus／high）。
- `recon-c-reference-engines.md`：Bevy 6a911653f、flecs 09790b5f7、lightyear 736e1695、bevy_replicon d80c849 對各機制怎麼發通知、Bevy 時間 Module 的細節（Opus／high）。

**文件**（文件分支 `origin/claude/gyo2-time-design` e2059b5e）：
| 文件 | 狀態 | 本頁怎麼用 |
|---|---|---|
| [批次表](rebuild-batch-plan.zh-Hant.md)第 3 列 | 主會話 | 範圍 |
| [事件游標的宣告](event-cursor-declaration.zh-Hant.md)修訂 6 | **已核准**（`-event-cursor-declaration-approved`、`-five-declarations-approved`） | 內容不改；第 4 節只寫和現況的差與「實作批定」的細節 |
| [T1 宣告](time-types-declaration.zh-Hant.md)修訂 2 | **未核准**；寫在 #240、#242、#244、D2 之前 | 第 2 節逐條對照現況，併進本頁；T1 欠使用者的 (a)～(d) 在第 2.3 節處理 |
| [時間設計](rebuild-time-design.zh-Hant.md)修訂 4 | 第 0 節是使用者決定；其餘是設計稿 | 第 0 節照用；第 2 節以後和新前提衝突的以本頁為準 |
| [統合](kernel-app-boundary-declaration.zh-Hant.md)（分支 `claude/gyo2-kernel-app-boundary-4af68c`）L2、L8、L10、L13、L19、L27，第 3.2、15.1、15.2、16 節 | 已核准 | 時間 Module 的要求清單（第 8 節、第 16 節「時間宣告的要求散在各處」） |
| [回滾份](rollback-module-declaration.zh-Hant.md) 5.7、7.2；[權威份](authority-module-declaration.zh-Hant.md) 5.4；[讀檔份](restore-entry-declaration.zh-Hant.md) 5.5 | 已核准 | 給時間 Module 的要求 |
| [編排層骨架的宣告](orchestration-skeleton-declaration.zh-Hant.md) 7.4、8.3 | 已合併（#244） | 延後的守衛、scenario 第 2 段的接口 |

**使用者的決定**（原話以 HANDOFF 為準）：`-unified-time`、`-fixed-step-in-world`、`-time-plan-two`、`-active-time-name`、`-t1-three-items`、`-clock-out-of-kernel`、`-module-deps-in-registers`、`-products-pick-modules`、`-rollback-module-strategy`、`-layer-duty-lists`、`-events-two-kinds-like-bevy`、`-event-trim-by-cursor`、`-no-separate-eventbus`、`-scenario-tests`、`-guards-after-mechanism-stable`、`-broken-world-ends-product`、`-k1-no-legacy-product-aliases`、`-authority-guard-gap-later`、`-module-vs-plugin`（#240）。

---

## 1. 這一批做什麼（先用遊戲講）

**2D 橫版的單機遊戲**，一個 World，畫面每幀、模擬固定步 64 Hz。

- 每幀開頭，時間 Module 的「推進時間」System 取一次樣，算出這一幀的真實時間與可縮放的時間（暫停時是 0、慢動作時乘 0.5、卡住 3 秒時最多前進 250 ms 或 5 步，多的記下來）。
- 接著輸入的 System 讀手把，寫一筆 `Message`「按下了跳」。
- 時間 Module 的「跑固定步」獨占 System 把這一幀的量加進累加器：夠 2 步就跑 2 次 `FixedUpdate`。每跑一次，`Tick` 加 1，`ActiveTime` 換成固定步的值（一步 15.625 ms、累計＝tick×步長）；跑完換回每幀的值。
- 固定步裡的「跳躍」System 讀「按下了跳」：第一次跑讀到、第二次沒有新的。它在空中撞到金幣，寫一筆 `Message`「拿到金幣了」；金幣實體被拿掉時，hook 先跑、接著 `Observer` 當場（同步點）生成粒子。
- 每幀後段的「播音效」讀「拿到金幣了」。這一幀固定步跑 0 次時，它就沒有新的可讀；跑 3 次時一次讀到三筆。
- 存檔時，作者把 `Tick`、`VirtualTime` 等存進去；讀檔換到新 World 時放回，接著存檔時的時間跑。

**PvP 的 Match**（沒有畫面）：一樣的「跑固定步」，沒有 `TimeControl` 就沒人能暫停；伺服器權威策略（第 4 批）只准時間 Module 公開的驅動者寫 `Tick`。**PvP 的 Client**（第 4 批）：回滾策略把 `Tick` 退回、立起「重跑中」，呼叫時間 Module 的「跑一步」函式重跑 k 步，每一步 System 讀到的 `ActiveTime` 和第一次跑那一步時相同。

Kernel 不知道「每幀」「固定步」「暫停」「tick」是什麼：它只執行 schedule、保存 `Message` 的游標、在觸發點跑 `Observer`。

---

## 2. 範圍

### 2.1 這一批做的

| 代號 | 內容 | 出處 |
|---|---|---|
| E1 | 守衛的整理：從 `kernel_guard.py` 拿掉 `PRODUCT_ALIASES`；第 2 批延後的 6 條綁實作守衛逐條判（第 6.3 節） | `-k1-no-legacy-product-aliases`、骨架宣告 7.4 |
| E2 | 時間的型別與累加器（舊 T1）：`engine/time` 加純資料的型別與純算術 | T1 修訂 2，照第 2.2 節重整 |
| E3 | 保留式事件改名 `Message`＋游標＋上限＋聚合的丟棄記錄；拿掉 `EventTrim` | 游標份 3.2、5.1（已核准） |
| E4 | **拿掉 Kernel 的時鐘**：`Domain`、`ScheduleOptions::domain`／`clock`、Kernel 推 tick、`CurrentTick`、`AccessKind::DomainTick`、tickStamps、D-E12、F-E1、F-9；診斷與丟棄紀錄改帶同步點序號（L8） | 統合 3.2、L2、L8 |
| E5 | **時間 Module**（新的 Engine Module，第 3 節）：時間的 Resource 登記、「推進時間」System、「跑固定步」獨占 System、`Tick`、`ActiveTime` 的切換、「重跑中」、`TimeDiscarded` 訊息 | 批次表第 3 列；統合第 8、16 節；回滾 5.7、7.2；權威 5.4；讀檔 5.5 |
| E6 | 推送式事件 `Event`＋`Observer`＋`Trigger`＋`On<E>`（含元件 Add／Remove 觸發） | 游標份 3.3、5.2（已核准） |
| E7 | scenario 第 2 段（時間與事件） | `-scenario-tests`；骨架宣告 8.3 |
| E8 | 不另做 EventBus：本批碰到的 Engine 機制從 Kernel 的事件發，其餘列出歸屬的批次（第 5 節） | `-no-separate-eventbus` |

### 2.2 舊 T1 逐條對照現況

| T1 的內容 | 現況或後來的決定 | 本頁的處理 |
|---|---|---|
| 型別放 `engine/time`，命名空間 `Engine::Time` | `time-plan-two` (7) 照舊；DI 10 規定 `gyo_time` 只連 `GYO::Base`＋`Threads`（`time.md:89-90`，守衛 `tests/common/time/CMakeLists.txt:37`） | **照舊**。型別是純資料，不引用 Kernel |
| `FixedTime` 沒有 tick、累加器不記 tick（「tick 只在 World」） | `-clock-out-of-kernel`：tick 移到時間 Module | `Tick` 是時間 Module 登記的 Resource（型別也放 `engine/time`，純資料）；`FixedTime`、`TickAccumulator` 照舊不記 tick（第 3.3 節） |
| 「通用的時間在固定步裡的累計，T3 定」（T1 第 3 節第 7 條） | — | **本卡照 L17 判**：固定步裡 `ActiveTime.elapsed`＝`Tick`×步長、`delta`＝步長（lightyear 重跑時把 `Time<Fixed>` 換成回滾用的值，`prediction/rollback.rs:1186-1291`）。這樣重跑時 System 讀到的值天然和第一次相同（回滾 7.2 第 2 條） |
| 「兩個寫者，要不要標成可多寫」（T1 第 8 節） | `-authority-no-writer-count`：Kernel 不數寫者，`WritePolicy` 拿掉 | **不再是問題**；時間設計 DI 2「各只有一個寫者」失去守衛，改寫成時間 Module 的約定（DI WT-5） |
| 放進 World 之後摘要與 padding（T1 第 3 節第 9 條） | World 摘要照 Resource 的位元組比；digest 的內容屬權威（第 4 批） | 型別照 T1 沒有 padding、沒有指標、可直接複製（編譯期檢查守）；`RealTime.now` 每次不同的處理留給權威 Module 的 digest 規則，本批的測試用手動的取樣（第 3.4 節） |
| 「時間的 System 與驅動固定步的 System，T3」 | 「Module」已重新定義（#240），編排層已有登記單位（#244） | 本批做（第 3 節） |
| 「這一批不碰 Kernel」 | — | 本批同一條線還要拿掉 Kernel 的時鐘（E4） |

### 2.3 T1 欠使用者的四件

| T1 的待決 | 處理 | 依據 |
|---|---|---|
| (a) 用頻率設定時，頻率是整數還是小數 | **本卡照 L17 判：照 Bevy 收小數**（`double`）。換算一次成整數奈秒（四捨五入），之後只用整數；頻率不是有限的正數是斷言。整數字面值照樣能寫（`64` 轉成 `64.0`）。 | 批次表「頻率的型別照 Bevy `Time<Fixed>`／Unity 的做法判」。Bevy `from_hz(f64)`（`bevy_time/src/fixed.rs:76-109`）；Unity Netcode 的 `SimulationTickRate` 是整數。兩者存進去都是整數奈秒、之後行為相同，差別只在能不能寫 59.94——不改任何使用者的決定，所以不交使用者。浮點除法是 IEEE 正確捨入，三平台換算出同一個整數；判定 1 的「步長的設定」案例在 L1 三平台跑 |
| (b) 主會話對設計的四處調整 | **本卡照 L17 判，四處都留**：(1) 扣整步的算術是 `engine/time` 的純函式 `AdvanceVirtual`（Bevy 的截斷也在 `Time<Virtual>` 自己的函式，`virt.rs:249-266`）；(2) `FixedTime` 裡裝累加器（獨占 System 不能有自己的狀態，放兩份就有兩個來源）；(3) `Add` 多一個上限參數，用來斷言；(4) `Reset()` 回傳清掉的量。四處都沒有推翻使用者的決定；「誰呼叫 `Reset`、記不記進 `discarded`」本批定：時間 Module 只在 `FixedTime` 換步長時呼叫，清掉的量記進 `discarded`（使用者「丟了多少有記錄」） | 時間設計第 547、552 行「扣整步是時間的 System 的事」在合併前改寫成「`AdvanceVirtual` 一處」 |
| (c) 修訂 2 要不要再送檢查 | 被本頁取代：本頁整份送換模型檢查（第 12 節） | — |
| (d) 核准整份 T1 | 被本頁取代：T1 的內容併進本頁，核准本頁即核准它；T1 檔案留作紀錄，開頭加一行「由 time-events 宣告取代」 | — |

### 2.4 不在這一批

| 事 | 歸哪一批 |
|---|---|
| runner 的取樣與交換點（「進」寫取樣的時間點）、可注入的等待、對準 tick 格點的等待 | 編排層第 5 條（交換點、執行緒的安排）。本批的取樣先照 Bevy 的 `TimeUpdateStrategy` 放在時間 Module 的 Resource（第 3.4 節） |
| tick 的跳變（回滾退 tick、Client 對時）通知 | 第 4 批（回滾）與網路；第 5 節 |
| 「tick 只有一個來源」的檢查、tick 的 Z6 例外、`authority.Mark<Tick>()` | 第 4 批（權威）。本批公開 tick 的型別與驅動者名單（L27） |
| 狀態機切換、讀檔完成、存檔版本的通知 | 第 5 條各批；第 5 節寫建議的送法 |
| World 壞掉的通知、日誌 Module | 日誌還沒排批次；**第 11 節第 1 件要你定** |
| Loading（`Domain::Loading` 沒有任何行為，`recon-a` §7） | 隨 `Domain` 拿掉；Loading 的流程由狀態機那一批或生成世界那一批定 |
| PvP 改成 64 Hz | PvP 移植（`-t1-three-items`） |
| 命名空間根 `Engine::` → `GYO::Engine::` 的改名 | 另一批（第 3.1 節的 include 順序在本批處理） |

---

## 3. 時間 Module

### 3.1 位置、名字、include 順序（本卡照 L17 判）

| 事 | 判定 | 理由與系譜 |
|---|---|---|
| 型別 | `engine/time`（`GYO::Time`，命名空間 `Engine::Time`）：`RealTime`、`VirtualTime`、`FixedTime`、`ActiveTime`、`TimeControl`、`TickAccumulator`、`AdvanceVirtual`、`Tick`、`Resimulation`、`TimeSampling` 與兩個上限、步長的設定 | `time-plan-two` (7)；DI 10 不變（純資料、純算術，不引用 Kernel） |
| 登記單位與 System | **新的 Engine Module** `engine/world_time`（命名空間 `GYO::Engine::WorldTime`、target `GYO::WorldTime`、登記單位 `WorldTimeModule`，是編排層 `Module` 的子類）。**本卡加的詞** | `time-plan-two` (7)「兩個 System 放別的 Module」；DI 10 不准 `gyo_time` 連 Kernel。N17 要目錄、命名空間、target 一致，而 `GYO::Engine::Time` 會遮住現有的 `::Engine::Time`（在 `GYO::Engine::*` 裡寫 `Time::` 會指到新的），所以不能叫 `Time`。比過的名字：`Clock`（和 `MonotonicClock`、舊稱「時鐘」＝`Domain` 撞）、`Timeline`（Unity 的動畫時間軸）、`GameTime`（工具的預覽也用）；`WorldTime` 說的是「放進 World 的時間」。系譜：Bevy 的 `bevy_time` 把型別與 `TimePlugin` 放同一個 crate；GYO 因 DI 10 拆成兩個 target，型別那一半照 Bevy |
| 登記檔 | `engine/config/modules.csv` 加一列 `world_time`：`default=yes`、`depends=kernel,orchestration,time` | `-module-deps-in-registers`；Module 預設全帶、產品可關（骨架宣告、O-6）。**這是第一個真的進 World 的 Engine Module**（現在十列都是 `library_only=yes`，`recon-b` §1） |
| include 順序 | `engine/world_time` 的程式一律寫 `::Engine::Base::`、`::Engine::Time::`（全限定）；`gyo_orchestration_include_order` 的掃描範圍擴到 `engine/world_time` 的公開標頭 × Kernel 標頭 × 編排層標頭 | 第 2 批的問題：`namespace GYO::…` 裡未限定的 `Engine::` 在 `GYO::Engine` 已開過時找錯（dev_log `2026_11_18_gyo2_orchestration_skeleton.zh-Hant.md:45`、a8065853）。時間 Module 就在 `GYO::Engine` 底下，所以一開始就照修好的寫法；守衛擴範圍是「只緊不緩」 |

### 3.2 Feature → Contract → Architecture

| Feature（Product） | Contract | Architecture |
|---|---|---|
| 固定頻率的模擬不被畫面拖慢（①PvP Match、②單機） | 每幀跑 0～N 次固定步；步長整數奈秒、預設 64 Hz；一幀最多 N 步（預設 5），超過的整步從 `VirtualTime` 扣、記進 `discarded` | 「跑固定步」獨占 System＋`FixedTime`（裝累加器）＋`AdvanceVirtual` |
| 暫停、慢動作，UI 照常動（②、③工具的預覽） | `TimeControl` 寫想要的；`VirtualTime` 是套用後的；`RealTime` 不受影響 | 「推進時間」System 讀 `TimeControl` 寫 `VirtualTime`、`RealTime` |
| 作者不必選讀哪個時間（②） | `ActiveTime` 平常＝`VirtualTime`，固定步裡＝（步長、`Tick`×步長） | 「跑一步」函式換入換出 |
| PvP 的權威與回滾、存檔（①、②） | `Tick` 是公開型別的 Resource；驅動者名單公開；重跑時同一個「跑一步」函式推 tick；「重跑中」恆在；時間的 Resource 能被讀檔放回 | 第 3.3、3.5 節 |
| 測試與重放不靠真實的時間基準（①②③） | 取樣可以換成手動 | `TimeSampling` Resource（第 3.4 節） |
| 追不上時有記錄（①②） | 扣掉的量累計在 `VirtualTime.discarded`，並發一筆 `TimeDiscarded` 訊息 | 第 5 節 |

### 3.3 Resource 與 System

| Resource（`engine/time` 的型別，時間 Module 登記） | 欄位（整數奈秒） | 寫者 |
|---|---|---|
| `RealTime` | `now`、`delta`、`elapsed` | 推進時間 |
| `VirtualTime` | `delta`、`elapsed`、`scale`（這一輪乘上去的）、`paused`、`discarded`（累計扣掉的）、倍率乘完的零頭 | 推進時間 |
| `FixedTime` | 步長、累加器（餘數）；`Overstep()`、`UntilNextStep()` | 跑固定步（`Add`）；換步長的入口 |
| `ActiveTime` | `delta`、`elapsed` | 推進時間（每幀的值）、跑一步函式（固定步的值，跑完換回） |
| `Tick` | `value`（`uint64`，已跑完的固定步數；第 n 步執行中是 n） | 跑一步函式；回滾策略用 `ExclusiveWorld` 退回（回滾 7.2） |
| `TimeControl` | 想要的 `paused`、`scale` | 產品；可以不登記（Match 不登記就沒人能暫停：時間 Module 的設定 `pausable=false` 時不登記） |
| `TimeSampling` | 模式：讀時間基準（預設）／手動（下一個時間點或下一段經過時間） | 產品、測試；第 3.4 節 |
| `Resimulation`（「重跑中」） | `active`（恆為 false，除非回滾策略寫） | 回滾策略（第 4 批）；時間 Module 只登記、放初始值 |

| System（`WorldTimeModule` 登記） | 種類 | 做什麼 |
|---|---|---|
| `AdvanceTime`（推進時間） | 一般 System | 依 `TimeSampling` 取一次樣 → 更新 `RealTime` → 讀 `TimeControl`、`FixedTime` → `AdvanceVirtual` 算出這一輪的量與扣掉的量 → 寫 `VirtualTime`、`ActiveTime`（每幀的值）；扣掉的量大於 0 時寫一筆 `TimeDiscarded` 訊息 |
| `RunFixedSteps`（跑固定步） | `ExclusiveWorld<Writes<Tick, FixedTime, ActiveTime>>` | 把 `VirtualTime.delta` `Add` 進 `FixedTime`，得 k 步（`AdvanceVirtual` 已保證 k ≤ 上限）；呼叫 k 次 `RunOneFixedStep` |
| `RunOneFixedStep(ExclusiveWorld&, schedule)`（公開函式，不是 System） | — | `Tick`+1 → `ActiveTime`＝（步長、`Tick`×步長）→ 巢狀跑固定步的 schedule → `ActiveTime` 換回每幀的值。回滾策略重跑時先把 `Tick` 退到 T，再呼叫它 k 次（回滾 7.2 第 2 條，「時間 Module 給推進一步的函式」那一種） |

**掛在哪**（本卡照 L17 判）：Kernel 不內建 schedule 與 stage 名（DI-KS 15），schedule 由作者宣告。`WorldTimeModule` 的設定給四個名字，預設照 Bevy 的形：每幀的 schedule `Update`、推進時間的 stage `First`、跑固定步的 stage `FixedMainLoop`、固定步的 schedule `FixedUpdate`（N22b 用階段名）；作者可以全部改。設定裡的 schedule 或 stage 不存在，是 `WorldTimeModule::Check` 回報的啟動錯誤（編排層 O-5 收齊）。系譜：Bevy `First` 的 `time_system`、`RunFixedMainLoop` 的 `run_fixed_main_schedule`（`bevy_time/src/lib.rs:68-108`）。

**`Tick` 的先後**：跑之前加 1，第一步執行中讀到 1——和現在 Kernel 推 tick 的語意相同（`Executor.cpp:212-219`，`RunPlan` 開頭 `++ticks_`），也和 lightyear 在 `FixedFirst` 加 1 相同（`timeline.rs:211-278`）。所以現有測試「第 N 個 tick」的期待值換觀察方式後不變。

**公開給權威的**（L27）：`Tick` 的型別；驅動者名單 `WorldTimeModule::TickWriters()`＝`{"RunFixedSteps"}`（對時 System 在 Client 那一批加）；`RunFixedSteps` 帶 `Writes<Tick>` 標註。

**時間 Module 的職責清單**（新 Component，本卡照 L17 寫）：(1) 把時間的 Resource 登記進它被建進的每個 World；(2) 取樣（照 `TimeSampling`）；(3) 真實時間與可縮放時間的推進（暫停、倍率、上限、扣掉的記錄）；(4) 固定步的累加與驅動、`Tick`；(5) `ActiveTime` 的切換；(6) 登記「重跑中」並放初始值（回滾 5.7 已核准由時間 Module 收）；(7) 發 `TimeDiscarded`。不在清單上的：tick 的跳變與回滾（回滾 Module）、tick 只有一個來源的檢查（權威）、runner 的等待與交換點（編排層）、狀態（狀態機）。

### 3.4 取樣（本卡照 L17 判）

時間設計第 3.1、5.4 節的形狀是「runner 在交換點寫取樣的時間點、時間的 System 讀它」，但交換點在編排層第 5 條，本批沒有。本批照 Bevy `TimeUpdateStrategy`（`bevy_time/src/lib.rs:115-131`、`:179-192`）：`TimeSampling` Resource 選「讀時間基準」（`AdvanceTime` 呼叫 `::Engine::Time::MonotonicClock`）或「手動」（測試、重放、scenario 寫下一個時間點或下一段經過時間）。交換點做出來之後，runner 改寫手動的值即可，Resource 的形狀不變。

和既有規則的關係：「System 不自己讀時間基準」是時間設計第 3.1 節的設計稿；使用者定的「(c) 區的時間只來自 tick」已歸伺服器權威策略（`-clock-out-of-kernel`，權威 5.4：靠 G-5 的來源掃描與 digest）。時間 Module 的 `AdvanceTime` 不是判定的 System，讀時間基準不違反任何使用者的決定。

### 3.5 讀檔、回滾、權威的要求逐條對照

| 要求（出處） | 本頁的回答 |
|---|---|
| 時間的 Resource 能被讀檔放回（讀檔 5.5） | 全部是可直接複製、沒有指標的 Resource，作者照一般 Resource 存與放回（`WorldSetup` 的準備時機，O-7）。scenario S2 的讀檔案例驗它 |
| `Tick` 是回滾策略能寫的 Resource、重跑者用 `ExclusiveWorld` 退（回滾 7.2 第 1 條） | `Tick` 是公開型別；不擋任何寫者 |
| 重跑時每一步 tick 怎麼推、System 讀到的固定步時間等於第一次（回滾 7.2 第 2 條） | `RunOneFixedStep`；`ActiveTime.elapsed`＝`Tick`×步長 |
| 「重跑中」由時間 Module 登記、恆在（回滾 5.7、7.2 第 3 條） | `Resimulation`，建進 World 就登記、值 false。沒有時間 Module 的 World 沒有它 |
| 公開 tick 型別、驅動者、對時（L27、權威 5.4） | 第 3.3 節 |
| 時間設計 DI 2 的守法（權威份修訂 4 回報） | 不數寫者之後，改成約定（DI WT-5），沒有啟動檢查；理由：Kernel 不疊加限制（`-kernel-is-ecs-no-restrictions`），作者自己寫驅動也該做得到（`engine-not-rigid`） |
| tick 的 Z6 例外（統合 16） | 第 4 批（權威）。本批沒有 Z6 的變更 |
| 訊息要 tick 的自己在內容帶（游標份 3.2「時間標記」） | 寫的 System 讀 `Res<Tick>` |

---

## 4. 事件：和已核准的游標份的差

游標份的內容（修訂 6）不改。下面只寫 (1) 現況的事實，(2) 游標份寫「實作批定」「實作批對照」的細節，本卡照 L17 判。

### 4.1 現況（`recon-a` §2、§3）

- 讀者沒有游標：掃整個緩衝，取「上一個 tick 中 writerPos 大於自己 Plan 位置」加上「本 tick 中 writerPos 小於自己位置」的紀錄，依 (tick, writerPos, seq) 排序（`Params.hpp:419-437`）。紀錄是 `{tick, seq, writerPos, payload}`，沒有上限、沒有丟棄計數（`detail/Storage.hpp:189-214`）。
- `StepKind::EventTrim` 在 `Plan.hpp:36`、`Scheduler.cpp:152`、`:201-260`、`Executor.cpp:437-441`、`Registry.cpp:909-912`（D-E1）、`Fingerprint.cpp` 五處。
- `ExclusiveWorld` 不含事件（DI-KS 1），必須是唯一參數（S-10，`SystemTraits.hpp:137-138`）。World 外面在 Ready 時能寫元件與 Resource 但不跑 hook，沒有事件寫入的 API；Ready 時沒有 `SystemContext`。
- 「最外層 Run 結束、清空輪之後」在 `Executor.cpp:444-448` 的 `!nested` 分支。巢狀跑沒有獨立的深度計數，`runStack_` 就是層級。

### 4.2 游標份留給實作批的細節（本卡照 L17 判）

| 細節（游標份出處） | 判定 | 系譜 |
|---|---|---|
| hook 帶 `MessageReader` 被拒：編譯失敗還是啟動錯誤（第 8 節） | **編譯失敗**（hook 參數的 traits 裡 `static_assert`，訊息帶專屬字串），用現有的編譯失敗測試機制（`expect_build_failure`） | Bevy 的 hook 只拿 `DeferredWorld`，型別上就拿不到讀者（`lifecycle.rs:84`）；GYO 的 Extract 寫入也是編譯失敗（DI-KS 13） |
| observer 的延後變更：和 hook 共用佇列還是分開（3.3 末） | **共用**：observer 的 `Commands` 記進同一條 hook 佇列（每個 schedule 一條，巢狀層各自），照 F-12 一輪一輪套用 | flecs 的 hook 與 observer 在延後模式下落在同一個同步點（`ObserversManual.md:2598`）；Bevy 的 hook 與 observer 都用 World 的命令佇列 |
| 丟棄記錄要標時間時用哪個計數（L8：stamp 或同步點序號） | **同步點序號**（現有的 `syncEpoch`，World 每次同步點加 1）。`RejectedSpawn::tick`、`DiagnosticsWriter` 的 tick、`HookCall::tick` 一起換成它 | 「第幾個同步點」對作者可讀、每個 World 單調；stamp 是每筆寫入的版本，同一個同步點有很多個 |
| 新增時 hook 在 observer 之前、移除時 hook 在 observer 之後；GYO 現行 OnRemove 的先後（3.3） | 現行：despawn 時所有 OnRemove hook 跑完才銷毀（`World.cpp:530-803`，F.2）。改成：每個（實體、元件）先跑 Remove 的 observer、再跑 OnRemove hook，全部跑完才銷毀；新增時先 OnAdd hook、再 Add 的 observer | flecs `ObserversManual.md:128-142`；Bevy `world/entity_access/mod.rs:1344-1345` |
| World 外面在 Ready 時觸發，observer 的延後變更歸屬（3.3 末；Ready 時沒有 `SystemContext`） | 觸發時開一條暫時的佇列，回來之前照第 3.3 節末的規則套到空（上限＝`KernelSettings::hookRoundLimit`） | 游標份修訂 6（已核准）；本卡只定「暫時的佇列」這個實作 |
| `ExclusiveWorld` 觸發 `Event` | `ExclusiveWorld` 加 `Trigger<E>(…)`（當場跑 observer）；DI-KS 1 的「不含事件」改成「不含 `Message` 的讀寫；可以當場觸發 `Event`」 | 游標份 3.3「`ExclusiveWorld` 與 World 外面觸發，observer 當場跑」（已核准）；Bevy `World::trigger` |
| observer 的登記放哪 | World 的 Registry 裡（不放全域；`kernel_symbols.py` 與 K4 不准新的可寫全域） | — |

### 4.3 拿掉時鐘後要換的欄位（本卡照 L17 判）

| 欄位 | 現在 | 改成 |
|---|---|---|
| `RejectedSpawn::tick`、`DiagnosticsWriter` 的 tick、hook 的 `call.tick` | 那個 schedule 的 domain tick | 同步點序號 |
| 編排層 `WorldFailure` 的 `domain`、`tick`（`OrchestrationError.hpp:126-128`、`Product.cpp:389-392`） | domain 與 tick | `domain` 拿掉；`tick` 換成那個 World 的同步點序號（欄名 `syncEpoch`）。編排層不認得時鐘（O-4） |
| `World::Tick`、`World::TickStamp` | Kernel 的 tick 表 | 拿掉 |
| scenario 的 `Schedule(name, clock)`（`WalkerGame.hpp:212-221`） | 為 D-E12 宣告 `Domain::Frame` | 拿掉 domain 宣告（骨架宣告 8.3 已預告） |

### 4.4 指紋、摘要、golden（L10）

Registry、Plan、C 三種指紋的版本號都升（現在 6／4／8），export 標頭 `gyo.kernel.plans 4` 升；golden 重產：`SchedulerTests:138-145`（6 個）、`RegistryTests:40-41`、`InitialValueTests:904-906`、`HookFlushTests:937-941`、export 文字 `SchedulerTests:711-714`、`HookFlushTests:550`；World 摘要（J-32，`WorldTests:504`）照游標份含游標表與寫入序號、版本升。observer 的登記進 Registry 指紋。重產的值在合併前核准時列出新舊對照。

---

## 5. 不另做 EventBus：Engine 機制逐個定送法

原則（本卡照 L17 從別家歸納，`recon-c`）：**World 層級的廣播、讀者節奏不同 → 保留式 `Message`；針對某個實體或某個結構變更、要當下反應 → 推送式 `Event`；連續的狀態（時間前進、暫停、是否重跑中）→ 不發事件，讀 Resource**。三條在 Bevy 一致：`StateTransitionEvent`、`AppExit`、`AssetEvent` 是 Message；生命週期與 `bevy_scene::Ready` 是 Event；時間前進、暫停、`Rollback` 是狀態。flecs 只有推送式，節奏不同時建議用 System 輪詢（`ObserversManual.md:104-126`）。

| 機制 | Bevy（位置） | flecs（位置） | GYO 的送法 | 批次 |
|---|---|---|---|---|
| 固定步跑了一步 | 不通知（`fixed.rs:243-258`） | 不通知；系統輪詢 tick source（`timer.c:12-48`） | 不發。訂閱者就是固定步 schedule 裡的 System | — |
| 時間被扣掉（追不上） | 一行 debug 日誌＋`effective_speed`（`virt.rs:255-266`） | 沒有截斷 | **`Message` `TimeDiscarded`**（扣掉的量、這一輪的真實經過時間、是固定步的整步還是時間長度的上限）；累計照舊在 `VirtualTime.discarded`。理由：使用者「丟了多少有記錄」；日誌 Module 將來讀它；World 層級、讀者節奏不同 → 保留式。和 Bevy 不同的地方是 Bevy 只寫日誌（Bevy 的被丟時間不需要人反應；GYO 使用者要記錄） | **本批** |
| 暫停、恢復、改倍率 | 不通知：`paused` 旗標、run condition（`virt.rs:213-239`） | 沒有暫停 API | 不發。讀 `VirtualTime.paused`、`scale`，或用 `Changed<…>` | — |
| 元件新增、移除 | hook 先、observer 後（`bundle/insert.rs:458-499`） | OnAdd／OnRemove observer（`flecs.h:1873-1889`） | **`Event`（`Add<T>`、`Remove<T>`）**，已核准 | **本批** |
| tick 跳變（回滾退 tick、對時跳 tick） | —；lightyear 推送式 `LocalTimelineShift`（`timeline.rs:321-330`） | — | 建議 `Event`（以 tick 為索引的緩衝要同時平移，lightyear 的理由）；「重跑中」本身是 Resource，不發 | 第 4 批、網路 |
| 狀態機切換 | `Message` `StateTransitionEvent`（`bevy_state transitions.rs:236-291`） | 核心沒有 | 建議 `Message` | 第 5 條（狀態機） |
| 讀檔完成、場景就緒 | 推送式 `Ready`（`resolved_scene.rs:315`）、`WorldInstanceReady` | 同步函式，沒有通知 | 建議：讀檔＝新 World（讀檔份），由新 World 的準備或生成世界的 System 發 `Message`；定案在讀檔的實作 | 第 5 條（讀檔） |
| 存檔版本被拒 | — | — | 不是事件：`Result` 的錯誤碼（O-5、存檔版本 Module） | 第 5 條 |
| World 壞掉（D2） | 錯誤回呼 `FallbackErrorHandler`（每個 World 一個 Resource，`error/handler.rs:116`），不是事件 | 直接呼叫日誌（`log.c:451`） | **第 11 節第 1 件要你定** | 日誌那一批 |
| World 建立、替換 | 沒有事件（`app.rs:1235-1247`） | 沒有；收尾用 `ecs_atfini` 回呼 | 同上，第 11 節第 1 件 | 日誌那一批 |
| 日誌 | 直接呼叫 tracing | 直接呼叫 | 日誌 Module 訂閱上面的 `Message`／`Event`（使用者 `-no-separate-eventbus`） | 還沒排 |

---

## 6. 守衛、測試、突變

### 6.1 判定的變更（AGENTS §14.4；**有緩的面的要你逐組核准**，最終在合併前核准帶突變結果再看一次）

| 組 | 守衛 | 守的性質 | 處置 | 緩的面 | 緊的面 | 之後由誰守 |
|---|---|---|---|---|---|---|
| G1 | J-37（`ZoneRuleTests:223`）、J-24 的 F-E1（`:400`）、`rs-j37-*` | 事件只在它 domain 的 clock schedule 用 | 刪 | 任何 schedule 都能讀寫任何訊息（使用者 `-cross-clock-events-open`） | 無 | 游標保證不漏讀（G3） |
| G2 | J-24 的 D-E12（`SchedulerTests:605`）、`RegistryTests:223-244` 的轉換清單、`or-gi-domain`、`rs-j24-de12-removed`、`or-gi-domain-unchecked` | 每個 schedule 有 domain、每個 domain 一個 clock | 刪 | schedule 不必屬於任何時鐘 | 無 | 時間 Module 的 `Check`（設定的 schedule 要存在） |
| G3 | J-36（`WorldTests:996`）、J-54 的修剪案（`HookFlushTests:591`、`:615`、`:772`）、J-56 的內層修剪（`NestedRunTests:919`）、J-24 的 eventTrim（`CoverageTests:378`）、J-20 的修剪位置（`:425`）、`we-j36-*`、B 表 7 個 EventTrim 突變 | 讀者依位置與 tick 讀、隔兩 tick 讀不到、修剪在 Plan 裡的位置 | 照游標份第 8 節改寫 | 「隔兩 tick 讀不到」不再成立；修剪不在 Plan 裡；巢狀跑不修剪 | 被 run 條件跳過、schedule 沒跑的讀者下次補讀；恰好一次擴大到全部跑過的讀者（上限內）；上限與聚合的丟棄記錄 | Kernel（G3 的新案例） |
| G4 | J-39 的 tick 部分（`CoverageTests:302`；DI-KS 8「domain tick 只由 clock schedule 推進、System 宣告寫它是啟動錯誤」）、`rs-adv-domain-tick-is-zone-b`、`rs-j12-tick-one-mount` | tick 只有一個來源、(c) 區讀得到 tick | 刪 | System 可以寫 `Tick`、(c) 區讀不到 `Tick`（它是 (b) 區的 Resource）直到第 4 批拿掉區規則 | 無 | 第 4 批：權威的專用檢查（權威 5.4）。空窗是使用者已接受的（`-authority-guard-gap-later`）；`gyo2` 上沒有用 (c) 區的產品 |
| G5 | J-12 的 `CurrentTick` 案（`SignatureTests:147`、`:260`）、J-30 的 tickStamps（`ExecutorTests:478`）、`we-adv-tickstamp-never-recorded`、J-56 的 tick 前進（`NestedRunTests:148`、`:889`）、`ns-nested-tick-not-advanced` | `CurrentTick` 的存取推導、tickStamps、巢狀跑推 tick | 刪 Kernel 這一半；tick 前進的性質搬到時間 Module 的測試（`RunOneFixedStep` 推 tick、巢狀跑 k 次 tick 加 k） | Kernel 不再推 tick | 無（搬過去的同一性質） | 時間 Module |

### 6.2 判定不變、觀察方式換的（沒有緩的面；事後報告 1～5）

- 約 60 個用 `CurrentTick` 當「第 N 次才動作」的閘門（`InitialValue` 19、`World` 17、`SpawnReject` 13、`HookFlush` 9 等，`recon-a` §5）：改用測試自己的計數 Resource（每次 Run 由一個測試 System 加 1）。Kernel 的測試不連時間 Module。「緩的面沒有」的證明照 §14.4：舊判定殺的突變，新判定也全殺。
- J-55 的 tick（`SpawnRejectTests:591`、`:957`）、`sr-tick-*`、`sr-no-record`、`sr-records-capped`、`sr-drop-takes-stamp`：tick 換成同步點序號，判的性質（記錄來自丟棄它的那次、一筆、不設上限、不取 stamp）不變。
- 改名：`EventReader`→`MessageReader` 等（`SignatureTests`、`RegistryTests:43,68` 的事件編號、`HookFlushTests`、`NestedRunTests` 用事件記錄的案例）；C 表 find 含 `EntryKind::Event`（9 個）、`AccessKind::Event`（4 個）的突變同步改 find。
- golden 重產（第 4.4 節）。
- `ks-diks15-code-added-without-text`（find `FE1,\n};`）改 find 到新的 enum 尾；`hf-startup-skipped`、`hf-rounds-follow-trim`、`hf-trim-before-rounds` 的 replace 用到要拿掉的符號，改寫成不靠它們、仍殺同一個案例，或退役並寫明理由。
- `kernel_guard.py` 的 `PRODUCT_ALIASES` 拿掉（使用者已定 `-k1-no-legacy-product-aliases`；緩的面：K1 不再擋 RetroFPS、`fps::pvp`、`Pvp`、`Gyo::Tools` 四個舊名字；守的性質「Engine 不出現產品名」由角色詞 `ROLE_WORDS` 照舊守）；`test_kernel_guard.py:125`、`:433` 與突變 `ks-k1-roles-dropped` 的 find 跟著改。
- 編排層突變 `or-failure-no-domain` 退役（欄位拿掉）、`or-failure-no-tick`／`or-failure-text-no-tick` 改成同步點序號。

### 6.3 第 2 批延後的綁實作守衛（`-guards-after-mechanism-stable`；條件＝涵蓋它的 scenario 段通過，第 1 段已通過）

| # | 守衛 | 判定 | 理由 |
|---|---|---|---|
| 1 | `naming_guard.py` 的 `ROOTS` 加 `engine/orchestration` | **第 1 步立** | 本批只換 `WorldFailure` 的兩個欄位，不動編排層的命名 |
| 2 | 編排層的 include 掃描（O-4） | **第 1 步立** | 時間 Module 依賴編排層，編排層不 include 它；方向不變 |
| 3 | 編排層不讀 Plan 的步驟、不呼叫 System（O-3） | **第 1 步立** | 本批不改 runner 的形狀 |
| 4 | 引擎各 CMakeLists 不手寫引擎內的 `GYO::` link（O-6） | **第 1 步立** | `world_time` 照登記檔產生 link，正好受它檢查 |
| 5 | `gyo_expect_links(gyo_orchestration GYO::Base GYO::Kernel)` | **第 1 步立** | 本批不改編排層的依賴 |
| 6 | Kernel 不出現 `Plugin`、`FinishContext`、開局識別字的常駐掃描（DI-KS 15） | **第 1 步立**（現有的三類） | 第 1、2 批的設計已穩 |
| 新 | 6 號加上時鐘的識別字（`Domain`、`CurrentTick`、`DomainTick`、`EventTrim`）；`naming_guard` 加 `engine/world_time`；`world_time` 的 `gyo_expect_links` | **延到第 4 批開頭** | 本批剛改的設計，涵蓋它的 scenario 第 2 段要先通過、合併前核准之後才立（和第 2 批同一個處理） |

### 6.4 新增的行為案例

- **`engine/time`**（`tests/common/time/TimeTypesTests.cpp`）：T1 第 5 節判定 1 的全部案例照用（累加、餘數、不丟時間、沒有 tick、扣整步、步數不超過上限、上限的斷言、量與步數、舊測試的期待值、和舊 `double` 版逐點比、暫停、倍率、零頭、沒有固定步的 250 ms、步長的設定〔改成：`FromHz(60.0)`→16,666,667；`FromHz(0)` 與非有限值斷言〕、`RealTime`、前置條件），加：`Reset` 回傳清掉的量。
- **時間 Module**（`tests/common/world_time/`）：登記了全部 Resource；`Resimulation` 恆在、初始 false；`pausable=false` 時沒有 `TimeControl`；手動取樣下一幀的 `RealTime`／`VirtualTime`；固定步跑 k 次、`Tick` 加 k、第 n 步讀到 `Tick`＝n；固定步裡 `ActiveTime`＝（步長、`Tick`×步長）、跑完換回；追不上時 `discarded` 與 `TimeDiscarded` 一筆；`RunOneFixedStep` 從退回的 `Tick` 重跑時讀到的 `ActiveTime` 與第一次相同；設定的 schedule 或 stage 不存在時 `Check` 報錯；換步長時 `Reset` 清掉的量記進 `discarded`；`TickWriters()` 的內容。
- **保留式、推送式**：游標份第 8 節「新：保留式」「新：推送式」兩列全部。
- **include 順序**：`world_time` 的標頭先於 Kernel、先於編排層各種順序都編得過。

### 6.5 突變（D184）

- 現在落在範圍內的：Kernel 40 個（時鐘 14、EventTrim 7、事件 19）＋編排層 4 個；處置照 6.1、6.2，每個寫明退役、改 find、還是改寫。
- 新的突變清單：`docs/architecture/target/v2/scripts/world_time_mutations.json`（時間型別與時間 Module）；保留式與推送式的新突變加進 Kernel 清單。清單在各步實作前由實作者定案，**交 Fable／high 敵對式檢查一次**（§14.2；新的守衛 Test 與受入判定）。T1 判定 2 的 22 個突變全部列入。
- 每步只跑相關的（新的、find 在改動範圍的、殺它的測試被改的）；合併前背景完整跑一次（Kernel 清單＋編排層清單＋`world_time` 清單）。只有預期的案例失敗才算殺，建置失敗不算。

---

## 7. scenario 第 2 段（`tests/common/scenario/`，ctest `scenario.segment2`，標籤 `cpu`）

接在第 1 段之後（骨架宣告 8.3 的接口），用手動取樣：

| 步 | 做什麼 | 檢查 |
|---|---|---|
| S2-1 | 建 Product，產品帶 `WorldTimeModule`（64 Hz）；`Walk` 改成固定步裡每步 +1 | 時間的 Resource 都在、`Tick`＝0 |
| S2-2 | 手動取樣 10 幀、每幀 15.625 ms | `Tick`＝10、玩家在 (10,0) |
| S2-3 | 一幀 3 步的量 | 這一幀固定步跑 3 次、位置 (13,0) |
| S2-4 | 加分改成 `Message` `ScoreGained`（固定步寫、每幀讀） | 每幀的讀者讀到的筆數＝這一幀跑的步數；0 步那一幀讀到 0 筆 |
| S2-5 | 暫停（`TimeControl`）2 幀 | `Tick` 不動、每幀的 System 照跑 |
| S2-6 | 卡住：一幀 1 秒 | 跑 5 步、`TimeDiscarded` 一筆、`discarded` 增加 59 步的量 |
| S2-7 | 金幣實體被拿掉 | `Remove<Coin>` 的 observer 在同步點跑一次、在 OnRemove hook 之前 |
| S2-8 | 存檔（`Tick`、`VirtualTime`、`FixedTime`、玩家、分數）→ 換新 World 讀檔 | 新 World 的 `Tick` 接著存檔時的值；再跑 5 步位置接著加 |
| S2-9 | 拿掉為 D-E12 加的 domain 宣告 | 第 1 段照樣通過 |

失敗的分類照 `-scenario-failure-by-contract`（骨架宣告 8.4）。

---

## 8. 要改的程式與文件

| 位置 | 內容 |
|---|---|
| `engine/time/include/engine/time/TimeTypes.hpp`、`TickAccumulator.hpp`（新） | 第 3.3 節的型別、`AdvanceVirtual`、更新 `RealTime` 的函式、步長與上限的設定 |
| `engine/world_time/`（新） | `WorldTimeModule`、`AdvanceTime`、`RunFixedSteps`、`RunOneFixedStep`、`TickWriters`；`CMakeLists.txt` 照登記檔 |
| `engine/config/modules.csv` | 加 `world_time` 一列 |
| `engine/kernel`（`Declarations.hpp`、`Params.hpp`、`Access.hpp`、`SystemTraits.hpp`、`Plan.hpp`、`detail/Storage.hpp`、`detail/Context.hpp`、`World.*`、`Scheduler.cpp`、`Executor.cpp`、`Registry.*`、`Fingerprint.cpp`、`KernelError.hpp`） | E3、E4、E6 |
| `engine/orchestration`（`OrchestrationError.*`、`Product.cpp`） | `WorldFailure` 的兩個欄位 |
| `tests/common/{time,world_time,kernel,orchestration,scenario}` | 第 6、7 節 |
| `docs/architecture/plans/kernel/scripts/kernel_guard.py`、`test_kernel_guard.py`、`naming_guard.py`、編排層的掃描 | 第 6.2、6.3 節 |
| `docs/architecture/target/kernel.zh-Hant.md` | DI-KS 1、8、10、11、12、14、15 的變更（第 10 節） |
| `docs/architecture/time.md`（英文） | 新型別的說明與 Design Invariants（第 10 節） |
| `docs/architecture/target/v2/world-time.zh-Hant.md`（新） | 時間 Module 的 Design Invariants、職責清單、Replacement Criteria |
| `docs/architecture/target/v2/orchestration.zh-Hant.md` | O-11 的 `WorldFailure` 欄位一句 |
| `rebuild-time-design.zh-Hant.md`、`time-types-declaration.zh-Hant.md` | 第 547、552 行改寫；T1 開頭標「由本宣告取代」 |
| `docs/dev_logs/2026_11_xx_gyo2_time_events.zh-Hant.md` | dev_log，和本批同一變更 |

**不碰**：`MonotonicClock.hpp`、`Waiter`、`LateWakeStats`；權威、回滾、區規則（第 4 批）；`.github/`。

---

## 9. 基盤 Architecture 的根據與 Architecture Delta

### 9.1 為什麼算基盤

每個讀時間的 System 照 `ActiveTime` 等的欄位寫；每個讀寫事件的 System 照 `Message`／`Event` 的 API 寫；權威、回滾、狀態機、讀檔、音效、網路都積在這兩樣上面（統合 5.1、游標份 7.1）。

### 9.2 予見的根據 → 要求

| 根據 | 要求 |
|---|---|
| 名指的 Feature（第 3.2 節表、游標份 7.2） | 第 3.2 節的 Contract 欄 |
| 參考實作：Bevy `bevy_time`、lightyear 的 timeline 與回滾、Unity `NetworkTime`（`recon-c` 二） | 型別的切法、`ActiveTime` 的換入換出、tick 在固定步開頭加 1、「重跑中」在時間那一側 |
| 規模與預算：64 Hz、一幀最多 5 步；訊息預設 1024 筆 | 時間 Module 每幀的成本：一個一般 System＋一個獨占 System＋每步一次巢狀跑；本批在本機量一次「每幀 0／1／5 步」時 `RunFrame` 的額外時間，記進 dev_log（不設判定門檻；記憶體照游標份 7.3） |

### 9.3 Architecture Delta（七項）

1. **Feature**：第 3.2 節；游標份第 6 節第 1 項。
2. **現在的問題**：時鐘與 tick 在 Kernel（使用者判「遊戲需求入侵了 Engine 設計」）；World 裡沒有任何時間的 Resource；事件依 Plan 位置與 tick 讀、修剪是 Plan 裡的一步、跨時鐘是啟動錯誤；沒有推送式。
3. **變化的邊界**：Kernel 少了時鐘、事件 API 改名加游標、多了推送式；`engine/time` 多了型別與算術；**新的 Engine Module `engine/world_time`**（新的 Subsystem）；編排層的 `WorldFailure` 少兩個欄位。
4. **影響**：第 8 節。
5. **依賴方向**：`world_time` → `orchestration`、`kernel`、`time`、`base`；`time` 照舊只連 `base`＋`Threads`；Kernel 不依賴任何一個（K5）。權威、回滾將來依賴 `world_time`（登記檔）。
6. **Ownership**：tick 從 Kernel 移到 `world_time`；「事件留多久」從「時鐘＋Plan 裡的修剪步」變成 Kernel 的游標與上限；「這一輪跑幾次固定步」是 World 裡的 System（`fixed-step-in-world`）；RejectedSpawn 等的時間標記從 tick 變成 Kernel 自己的同步點序號。
7. **為什麼不能更小**：拿掉時鐘而不同時提供時間 Module，scenario 與將來的權威、回滾沒有 tick；只改名不加游標，拿掉 tick 後讀者就沒有讀取依據（游標份 6.7）；推送式是使用者定的兩種之一，而且生命週期 observer 是 Engine 機制送通知的入口（第 5 節）。

### 9.4 系譜與退掉的選項（AGENTS §4）

| 決定 | 系譜 | 退掉的 | 決定的理由 |
|---|---|---|---|
| 型別與 System 分兩個 target | Bevy 同一個 crate；GYO 因 DI 10 拆 | 型別也搬進 `world_time`（`engine/time` 只剩時間基準） | `time-plan-two` (7) 使用者定型別留 `engine/time`；拆開後純算術能不連 Kernel 單獨測 |
| 累加器 `Add` 回傳步數 | 舊 `FixedTickRuntime` 的形（使用者指定帶過來、改名）；Bevy 是一次 `expend` 一步 | 照 Bevy `expend` | 結果相同；`Add` 之後餘數一定不足一步，內插不讀到中間狀態（T1 第 6 節） |
| 一幀步數上限（預設 5） | 舊的；Bevy 沒有步數上限，只靠 250 ms | 只用時間長度的上限 | 使用者 `time-plan-two` (4)「超過上限的整步扣掉」 |
| `RunOneFixedStep` 公開給回滾 | lightyear 回滾時手動跑 n 次 FixedMain（`rollback.rs:1186-1291`） | 步的 schedule 自己推 tick | 推 tick 與換 `ActiveTime` 只在一處，重跑與第一次跑走同一條路 |
| 取樣用 `TimeSampling` Resource | Bevy `TimeUpdateStrategy` | runner 在交換點寫（時間設計的形） | 交換點本批沒有；Resource 的形狀和之後 runner 寫的形狀相容 |
| 丟棄與診斷用同步點序號 | GYO 自己的 `syncEpoch` | stamp | 第 4.2 節 |

既有的 Test 與守衛能不能不改，**沒有**當作任何一項的決定理由。

---

## 10. Design Invariants 的變更（逐條要你核准；最終文字在合併前核准）

### 10.1 Kernel（`target/kernel.zh-Hant.md`）

- **DI-KS 1**：「`ExclusiveWorld` 不含事件」→「不含 `Message` 的讀寫；可以當場 `Trigger` 推送式事件」。
- **DI-KS 8**：拿掉 tick 的兩句（「(c) 只從 domain tick…取時間」的 tick 部分、「domain tick 只由該 domain 的 clock schedule 推進…」）；區規則本身第 4 批拿掉。
- **DI-KS 10**：照游標份第 9 節改寫（兩種事件）。
- **DI-KS 11**：補「和 observer 的先後見 DI-KS 10」。
- **DI-KS 12**：拿掉「domain tick 只有一個來源且在快照內」「Fixed domain 事件緩衝」；其餘第 4 批改寫。
- **DI-KS 14**：拿掉「每個 domain 一個 clock schedule、每個 domain 的事件只在它的 clock schedule 內」與「時鐘的部分隨時間 Module 那一批移出」。
- **DI-KS 15**：第 (2) 項拿掉「（推送式隨事件那一批加入）」；「不認得時鐘」照舊。
- **RC**：加游標份第 9 節的四條。

### 10.2 時間的型別（`docs/architecture/time.md`，英文；T1 第 6 節的三條）

1. 累加器只做整數的算術：不讀時間基準、不記 tick、不丟時間；從上一次 `Reset` 起，加進去的總量恆等於步數×步長＋餘數；`Add` 之後餘數不足一步。
2. 超過上限的量只在 `AdvanceVirtual` 一處扣；有固定步時扣整步；扣掉的一定累計進 `discarded`。
3. 這些型別不引用 Kernel（DI 10 的延伸）。
**Replacement Criteria**：時間 Module 要正確必須知道累加器的內部狀態；或 `ActiveTime` 需要第三種來源。

### 10.3 時間 Module（新頁 `target/v2/world-time.zh-Hant.md`；本卡加的代號 WT）

- **WT-1 登記**：建進一個 World，就登記第 3.3 節的 Resource（`TimeControl` 照設定）；`Resimulation` 恆在、初始 false，時間 Module 自己不寫它。
- **WT-2 取樣與推進**：每幀只取一次樣（`AdvanceTime`）；`RealTime` 不受暫停、倍率、上限影響；扣掉只在 `AdvanceVirtual`，扣掉的累計進 `discarded` 並發一筆 `TimeDiscarded`。
- **WT-3 tick 與固定步**：`Tick` 只經 `RunOneFixedStep` 前進（驅動與重跑同一條路）；固定步執行中 `ActiveTime`＝（步長、`Tick`×步長），跑完換回每幀的值；累加器不因重跑倒退。
- **WT-4 依賴**：`world_time` 只依賴 `orchestration`、`kernel`、`time`、`base`；Kernel、編排層不認得它；程式裡的 `Engine::` 一律全限定。
- **WT-5 寫者的約定**：`RealTime`、`VirtualTime`、`FixedTime`、`ActiveTime` 只由時間 Module 寫，`TimeControl`、`TimeSampling` 由產品寫，`Tick` 另外可由回滾策略寫——**這是約定，不是啟動檢查**；`TickWriters()` 公開驅動者名單供權威策略檢查。
- **WT-6 職責清單**：第 3.3 節末的七項。
- **Replacement Criteria**：(a) 時間 Module 要正確，必須讓 Kernel 或編排層認得時間；(b) 回滾或讀檔需要時間 Module 以外的地方推 tick 或換 `ActiveTime`；(c) 某個產品的時間組法（例：固定步在別的執行緒的 World）用這些 Resource 與函式拼不出來；(d) 職責清單要多一項，或判不出屬時間 Module 還是別的 Module。

守衛：行為測試（第 6.4 節）照常；綁實作的掃描延到第 4 批開頭（第 6.3 節）。

---

## 11. 要你定的

### 第 1 件：World 自己的事（壞掉、建立、替換）不能從那個 World 的 `Event` 發——怎麼辦？（**不擋本批**，日誌那一批之前定就好）

**遊戲的例子**：PvP Client 的模擬 World 在第 3000 個 tick 的某個 System 回了錯誤。照 D2（`-broken-world-ends-product`），這個 World 中斷、Product 結束，錯誤（`WorldFailure`：哪個 World、怎麼壞、壞在哪）回給 runner。你在 `-no-separate-eventbus` 說「D2 的 World 壞掉」也從 Kernel 的 Event／Observer 發、日誌 Module 當訂閱者。但 `Event` 與 `Observer` 住在 World 裡：壞掉的 World 不能再跑 observer（D2 自己說無法分辨是否健康），而「World 被建立、被換掉」發生時，那個 World 還不存在或正要被丟掉。

**別家**（`recon-c`）：沒有一家把這類事當 ECS 事件。Bevy 的錯誤走回呼（每個 World 一個 `FallbackErrorHandler` Resource，`error/handler.rs:116`），World／SubApp 的建立與拿掉沒有事件，日誌直接呼叫 tracing；flecs 的錯誤直接呼叫日誌函式，World 收尾用 `ecs_atfini` 回呼。理由一致：World 是事件的容器，它自己的生滅沒有讀者可通知；日誌要在任何時點都能用。

**比過的選項**：
- **錯誤照 D2 回給 runner，日誌 Module 從 runner 拿到它**（建議）：`-no-separate-eventbus` 的範圍收成「World 裡面的機制」，World 自己的生滅與壞掉是例外；日誌 Module 怎麼接 runner 在日誌那一批定（一個 runner 呼叫的介面，或 runner 把錯誤寫進另一個健康的 World 當 `Message`）。代價：例外一條，要寫進 DI。系譜：Bevy、flecs。
- **另立一個「Product 層」的事件佇列**，World 的生滅與壞掉發到那裡：等於在 Kernel 外再做一套事件，就是你否決的 EventBus。
- **發到另一個健康的 World**（例如畫面那個 World）：只有多 World 的產品有；單一 World 的產品沒有地方發。

**我的建議**：第一個。這會改你 `-no-separate-eventbus` 列舉的範圍（「D2 的 World 壞掉」那一項），所以交你定。

### 第 2 件：第 6.1 節 G1～G5 五組有緩的面的判定變更

每組的「守的性質、緩的面、緊的面、之後由誰守」在表裡。G1、G2、G3 是你已定的決定（放開跨時鐘、時鐘移出、照游標修剪）的直接後果；G4 的空窗是你接受過的（`-authority-guard-gap-later`）；G5 是同一性質搬到時間 Module。**方向在動工前核准；逐組的最終核准在合併前帶突變結果與 §14.4 的檢證者（Fable／high）報告再看一次**（和第 2 批同一個做法）。

### 第 3 件：第 12 節的模型與檔位（含兩處 xhigh）

### 一起確認（照 L17 自己判的，不同意請指出）

第 2.3 節 T1 的四件；第 3.1 節名字 `world_time`／`WorldTimeModule`；第 3.3 節的 Resource、System、掛載的預設名、`Tick` 的先後；第 3.4 節的取樣；第 4.2、4.3 節的實作細節；第 5 節各機制的送法（本批只做 `TimeDiscarded`、元件 Add／Remove）；第 6.3 節延後守衛的判定。

---

## 12. 誰做、用什麼、怎麼分步

上段（公開介面、Design Invariants、資料形式）。

| 步 | 內容 | 實作者 | 估計（agent 時間） |
|---|---|---|---|
| 第 1 步 | E1：`PRODUCT_ALIASES`、延後守衛 1～6 | 本卡（Opus／high） | 2～3 h |
| 第 2 步 | E2：時間的型別與累加器（測試先行） | 本卡（Opus／high） | 3～4 h |
| 第 3 步 | E3：`Message` 改名、游標、上限、丟棄記錄、拿掉 `EventTrim`（讀者先不靠 tick） | 本卡（Opus／high） | 8～12 h |
| 第 4 步 | E4＋E5：拿掉 Kernel 的時鐘、同步點序號、時間 Module、`modules.csv`、`WorldFailure` | 本卡（Opus／high） | 10～14 h |
| 第 5 步 | E6：`Event`、`Observer`、`Trigger`、`On<E>`、元件 Add／Remove | 本卡（Opus／high；觸發點、同步點、巢狀跑的交界 xhigh） | 8～12 h |
| 第 6 步 | E7：scenario 第 2 段、`TimeDiscarded`、golden、DI 文字、dev_log、本機量測 | 本卡（Opus／high） | 3～4 h |
| 第 7 步 | 完整突變、新突變清單的敵對式檢查、§14.4 檢證、審查 | 見下 | 4～6 h |

合計 38～55 h。第 3 步排在第 4 步之前：讀者先改成游標、不靠 tick，拿掉時鐘時事件就沒有牽連（批次表「拿掉某條限制和接手它的新家放在同一條線」）。第 4 步一次做完拿掉與時間 Module，中間不留沒有 tick 的狀態。第 2 步不依賴 Kernel，可以先做。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | Opus／high（本卡） | 上段；不用小模型 |
| 第 5 步的觀察點 | 本卡照 xhigh 處理該段（**比本卡的 high 高，要你同意**） | 觀察者在同步點、巢狀跑、Ready 時觸發的交界（ギアボックス：タイミング、決定性） |
| 審查者 | Fable／high | 和實作者不同模型 |
| §14.4 檢證者（G1～G5） | Fable／high | 第 2 批同一個指定；不用小模型 |
| 新突變清單的敵對式檢查 | Fable／high | §14.2；和定清單的人不同模型 |
| 第 5 步的敵對式審查 | xhigh-reviewer（Fable／xhigh；**要你同意**） | 觀察者的重入與先後 |

超過估計 1.5 倍（約 80 h）或單一問題累計超過 2 小時就停下回報。

---

## 13. 判定（先宣告，結果出來之後不放寬）

- **判定 1：通過**。本機完整 ctest 全過（含 `scenario.segment1`、`segment2`）；L1 三平台（linux-x64、windows-x64、macos-arm64）全過；`gyo_time` 的連結檢查照過；include 順序守衛照過。
- **判定 2：行為**。第 6.4 節的案例全過；第 7 節 S2-1～S2-9 全過。
- **判定 3：檢出力**。新突變清單經 Fable／high 檢查補齊後全殺；落在範圍內的 44 個舊突變照 6.1、6.2 處置後，留下的全殺；完整跑（Kernel＋編排層＋`world_time`）結果記進 dev_log，活著的逐個說明。
- **判定 4：一次性的掃描**（git grep，先用正例命中一次，終了碼區分「沒有」與「失敗」）：`engine/` 與 `tests/` 裡 `Domain::`、`CurrentTick`、`DomainTick`、`EventTrim`、`EventReader`、`EventWriter`、`ScheduleOptions::clock` 零命中（文件與 dev_log 除外）；`kernel_guard.py` 裡 `PRODUCT_ALIASES` 零命中。
- **判定 5：量測**：每幀 0／1／5 步的 `RunFrame` 額外時間記一次（只記錄，不設門檻）。

---

## 14. 範圍的限界

- 這是宣告，沒有試做；第 12 節的時間是讀程式量估的。
- 系譜照三份偵察（子 agent 讀本地原始碼）；Unreal 不在本地；Bevy 的 `Duration::mul_f64` 怎麼捨入沒有查（GYO 的倍率零頭是自己的規則，不照它）。
- 文件分支 `claude/gyo2-time-design` 比 `gyo2` 新 28 個 commit，本頁讀的是文件分支；`gyo2` 上的副本較舊。
- 「本卡照 L17 判」的各條沒有經使用者逐條看，第 11 節「一起確認」交使用者一次看。
- `TimeSampling` 讀時間基準的模式在 L1 上不驗確切的值（只驗手動模式）；讀時間基準那一條只驗「時間往前、第一幀 delta 是 0」。
