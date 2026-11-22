# 事前宣告：生成世界（重做）＋預設的實體配發器（`v8-arch-gyo2-world-gen`）

更新：2026-11-22（修訂 1）。Owner：架構（GYO2 主會話開的任務卡 `gyo2-world-gen`）。**狀態：等動工前核准。實作要等使用者核准、說開始。**

**經過**：
- 修訂 0（c8b5faf4）建議「生成中跑另一份清單」（本頁第 8 節的 A）。送 Fable 5.2／high 敵對式檢查：blocker 4、should 12、nit 9（主 checkout `build/evidence/gyo2-world-gen/declaration-r0-adversarial-check.md`），全部採用，處理在第 17 節。
- **修訂 1 改了建議**：檢查指出 A 的代價少算（單一 World 的讀取畫面要讓輸入、畫面、日誌都在生成那張再掛一次；生成那張結束後永不再跑，掛在上面的訊息讀者會永久拉住保留）、B 的代價多算（時間 Module 現有的暫停就做得到「不 tick、不丟時間」）。重算之後建議 **B：生成期間主迴圈照跑，編排層只立「生成中」的狀態，作者用 run 條件擋**——和 Bevy、Unity、Unreal 相同。第 14 節第 1 件交你定。
- 交換點卡的交界已對齊（5.12）。

**性質**：實作段第 5 批的事前宣告。涉及 AGENTS §2 的基盤 Architecture（編排層的流程、Kernel 的公開型別）、§3 的 Architecture Delta（新 Module、Kernel 公開型別搬家、編排層 Design Invariants 的增改）、§4 的系譜與決勝理由、§14.4 的守衛搬家。

**用詞**：設計段／實作段、動工前核准／合併前核准、第 N 步。標「**卡片照 L17 判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。要你定與確認的在第 14 節。

**核心一句**（每一項都拿來對照）：**生成世界是機制——編排層認得「這個 World 還在生成」：每次建 World 都從生成開始，作者說「還沒好」就繼續，沒人說就結束；生成什麼、分幾批、等什麼才算好、生成期間要不要 tick，全是作者的。** 預設的實體配發器同理：**Kernel 只認得「有一個配發器」，策略是一個可換的 Resource；預設帶一個，作者不必宣告就能 `Spawn`。**

## 0. 根據與讀的版本

- 批次表 `rebuild-batch-plan.zh-Hant.md`（文件分支 `claude/gyo2-time-design` c65548d9）第 5 批「生成世界（重做）」那一列與開頭的〈每批的設計審查〉；HANDOFF `v8-arch-gyo2-batch5-start`。
- [回收清單](recovered-from-stale-cards.zh-Hant.md)第 2 節；主 checkout `build/evidence/gyo2-build-world-phase/inventory-and-premises.md`（使用者 11-11 原話、盤點、前提表、沒答的五題）。
- HANDOFF 決定（文件分支 c65548d9）：`v8-arch-gyo2-build-world-phase-is-mechanism`、`-save-unaware-means-format-only`、`-game-init-list`、`-init-values-like-bevy`、`-two-layers-like-bevy`、`-restore-entry-new-world-default`、`-restore-entry-ids-author-choice`、`-replace-order-author-choice`、`-save-version-module`、`-default-allotter-module`（主會話照 L17 判）、`-asset-visible-on-arrival`、`-broken-world-ends-product`、`-five-declarations-approved`。
- [統合宣告](kernel-app-boundary-declaration.zh-Hant.md)修訂 38：L4、L14、L16、L17；第 240 行（11-11 的讀取畫面與分批載入→生成世界）。
- [讀檔宣告](restore-entry-declaration.zh-Hant.md)修訂 11：4、5.1、5.8、第 8 節第 198 行（給本批的要求）、第 13 節判定 1、2、第 15 節。
- asset 宣告修訂 3（分支 `claude/gyo2-asset-585196` 1b120a86，已核准、未合併）：5.6、5.7、5.13（G-1～G-3）、5.14；決定 9。
- 交換點宣告修訂 1（分支 `claude/gyo2-exchange-81ea9f` 840cb86c）：5.1、5.3、5.4、第 10 節末、第 13 節 W-1～W-9。
- 第 4 批（已合併）：[權威頁](authority.zh-Hant.md) DI-A 5、第 4 節；[回滾頁](rollback.zh-Hant.md) R-2 前提 6；Kernel DI-KS 12、DI-KS 15 第 (7)、(13) 項、DI-KS 27。
- 已合併的程式：`origin/gyo2` 45f13757——`engine/orchestration`（`Product.hpp`、`Setup.hpp`、`src/Product.cpp`）、`engine/ecs/kernel/include/engine/kernel/detail/Storage.hpp:409-462`（`EntityAllotter`、`RangeAllotter`）、`Registry.hpp:238-249`、`src/Registry.cpp:693-711`（兩個不同的配發器是 RE2）、`World.cpp:1042-1049`（訊息修剪取所有游標的最小值）、`engine/modules/world_time/src/TimeTypes.cpp:64-67`（暫停時這一幀的量 0、不累加、不扣）、[orchestration.zh-Hant.md](orchestration.zh-Hant.md) O-1～O-11、[world-time.zh-Hant.md](world-time.zh-Hant.md) WT-2、WT-3。
- 別家原始碼：主 checkout `build/evidence/gyo2-world-gen/other-engines.md`（偵察者 Opus／high；路徑:行號都在裡面）。

---

## 1. 這一批做什麼（先用遊戲講）

**單機 RPG 開新遊戲。** 玩家在主選單按「開始」。World 建好、準備函式跑完，World 從「生成中」開始。每一幀照常跑 `Main`（輸入、畫面、日誌、資產的套用都照跑），只是作者把生成用的 System 放在一個帶條件「生成中才跑」的 set 裡：這一幀生成地圖的前 2000 格、下一幀場景物件、再下一幀敵人、最後放玩家（使用者 11-11 原話的順序）。還沒做完的 System 呼叫一次 `Hold()`（「還沒好」）。哪一幀跑完沒有人說「還沒好」，生成就結束，下一幀起「生成中才跑」的 set 不再跑、「生成完才跑」的遊戲邏輯開始跑。作者讓時間在生成期間暫停（`TimeControl.paused`），所以 `Tick` 在生成期間一直是 0；讀取畫面的動畫照常有真實時間可用。

**遊戲中讀另一個存檔。** 玩家在第 3 關按「讀取存檔 2」。作者先在現在的 World 驗證存檔（版本、資產 id 全部載好；不對就不發替換）；再 `ReplaceWorld`，在 `extra` 準備函式裡放好暫存的存檔資料、把配發器範圍的下界移到存檔記的下一號之後。新 World 換上之後從生成中開始：作者的讀檔 System（在「生成中才跑」的 set 裡）照存檔編號放回物品、任務、觸發條件，存檔大就分幾幀、`Hold()`；做完，遊戲邏輯開始跑。遊戲初始化不再跑。

**等資產。** 地圖用的模型還在背景解碼。asset Module 的套用 System 照常掛在 `Main/First`，每一幀開頭 World 都看到最新的載入結果；作者的 System 用 asset 的就緒查詢看「全部好了沒有」，沒好就 `Hold()`。判定要用的內容在生成期間就載完，之後才開始 tick（asset 決定 9）。

**作者什麼都不宣告就 `Spawn`。** 卡牌遊戲的作者只登記元件、寫一個生成牌的 System。預設帶的 `entity_ids` Module 已經在每個 World 登記好範圍配發器、放好預設範圍 `[1, 2^32−1]`；`Spawn` 直接拿得到編號。PvP 的作者要「單調不重用」——預設的就是；要別的策略，就在那個 World 不選這個 Module、登記自己的配發器型別。

**不在意生成的作者。** 什麼條件都不寫：第一幀照樣是「生成中」，沒人 `Hold()`，第一幀結束就結束生成；沒有任何 System 受影響，和現在相同。

## 2. 範圍

### 2.1 這一批做的（W1～W7）

| # | 內容 | 歸屬 |
|---|---|---|
| W1 | **生成中**：每個 World 建好（啟動、每次替換）且準備函式跑完後從生成中開始；每一幀照常跑每幀清單；那一幀跑完沒人 `Hold()` 就結束生成 | 編排層 |
| W2 | 編排層自己的 Resource `WorldGeneration`（`Generating()`、`Hold()`），每個 World 都登記；每幀開頭由編排層寫入 | 編排層 |
| W3 | run 條件 `WhileGenerating`、`AfterGenerating`（讀 `WorldGeneration`），給作者的 set 用 | 編排層 |
| W4 | `Product::Generating(world)` 查詢 | 編排層 |
| W5 | **預設的實體配發器** `entity_ids` Module：每個 World 登記範圍配發器、準備函式放預設範圍（不蓋已在的值）、範圍由 `EntityIdsOptions` 給；登記檔 `default=yes` | 新 Module |
| W6 | `RangeAllotter` 從 Kernel 搬進 `entity_ids`（Kernel 只留 `EntityAllotter` 這個概念與配發的機制） | Kernel、新 Module |
| W7 | 生成的行為案例、讀檔掛在生成世界的案例、等外部結果的案例、多 World 的案例；scenario 第 6 段 | 測試 |

### 2.2 不在這一批

- **在已經跑起來的 World 裡重新進生成**（`Regenerate`）：只定形狀（5.11），不實作。
- **新 World 生成期間舊 World 照跑、生成完才換上**（無縫轉場）：不做（第 8 節選項 C；RC-O11）。
- 讀取畫面的畫法、轉場、淡出；生成期間要不要 tick（作者；asset 決定 9）。
- 讀檔的實作、存檔版本 Module、狀態機重做（各自那一批；本頁只寫交界，5.6～5.9）。
- 交換點、World 之間的資料、Product 層 Resource、把 World 交給別的執行緒（交換點卡；交界 5.12）。
- 回滾的代為登記配發器（R-2 前提 6 照舊是作者的責任；5.8）。
- 回收編號的配發策略（Bevy 的 free list＋世代）：沒有點名的 Feature（RC-EI1）。

## 3. 基盤 Architecture 的根據（AGENTS §2）

### 3.1 為什麼算基盤

- 生成世界是每個產品建 World 時都會走的流程；讀檔、asset、狀態機、網路的初始同步都會掛上來。
- `RangeAllotter` 是 Kernel 的公開型別，十幾個測試檔與兩個測試用的遊戲在用；搬家越晚越貴（`rename-early-with-rules`）。

### 3.2 予見的根據 → 要求（Feature → Contract → Architecture）

| 根據（出處） | 種類 | 推出的要求 |
|---|---|---|
| 使用者 11-11：地圖→場景→物件→敵人→玩家、物件太多可以分批、Client 要有讀取頁面、「現在物件少可以算 Startup，將來越來越多呢，先把契約訂好」（`inventory-and-premises.md` 開頭）；統合第 240 行把它交給生成世界 | 1 名指しの Feature | 生成可以跨多幀分批；分批期間能畫讀取畫面；順序由作者排 |
| 使用者 11-16：「開局、Loading、讀檔都是遊戲邏輯的叫法，不是機制」、「Loading 有沒有是作者決定」、「世界生成階段 Engine 不認識，那 Engine 如何排列流程？」（`-build-world-phase-is-mechanism`、`-save-unaware-means-format-only`） | 1 | Engine（編排層）認得「生成」這個階段；不認得 Loading、讀檔這些名字 |
| 使用者 11-16：遊戲初始化與生成世界分開；生成世界每個 World 各自、可以多次、可以分批（`-game-init-list`） | 1 | 每個 World 一份狀態；每次建 World 都生成；遊戲初始化在前 |
| 使用者 11-17：照 Bevy 兩層（`-two-layers-like-bevy`）；初始值在建立當下給、生成只是一般的 schedule（`-init-values-like-bevy`） | 1 | Kernel 不加任何階段的概念 |
| 讀檔宣告修訂 11：讀檔 System 掛在生成世界；分批時照存檔編號放回的作者要在第一批之前設好配發器；判定要有讀檔的案例 | 1（玩家存檔） | 第一批之前有準備時機（已有：O-7）；讀檔的案例 |
| asset 決定 9、G-1～G-3 | 1 | 生成的每一幀之間 World 看得到新的載入結果；結束條件與「何時開始 tick」由作者給 |
| `-default-allotter-module` | 1（主會話照 L17 判） | 預設 Module 登記配發器並放範圍 |
| 別家：Bevy 載入期間主迴圈照跑、每幀輪詢（`examples/showcase/loading_screen.rs:26-29`、`:208-221`；`multi_asset_sync.rs:30-35`）；Unity 場景載入一律非同步、幾幀後才好（`streaming-overview.md:3`）；Unreal `WaitingToStart` 期間世界 tick、每 tick 輪詢 `ReadyToStartMatch`（一般知識） | 2 參考實作 | 怎麼做（第 4 節） |
| 規模：BigMap 約 1 km²、區塊化地圖 242 MB（`-replace-order-author-choice` 引用 master 的計畫）；一幀 16.7 ms | 3 規模 | 一次生成不能塞在一幀 |

導不出、也不放進來的：「生成完自動跑某件事」的通知事件（狀態機要時由那一批提）、批數（作者自己記）、進度百分比。

## 4. 別家怎麼做（出處：`other-engines.md`）

| 問題 | Bevy | Unity Entities | flecs | Unreal（一般知識） |
|---|---|---|---|---|
| 「開始前跑一次」在哪一層 | `bevy_app` 的 `MainScheduleOrder` 兩張清單，`run_main` 第一次先跑 startup 清單（`main_schedule.rs:213-233`、`:290-304`）；`bevy_ecs` 沒有 | 沒有 World 層級的 | pipeline addon 的 `OnStart`，只在第一次 `progress`（`pipeline.c:711-736`、`:773-776`） | `BeginPlay` 在世界載好之後 |
| 跨多幀的載入 | 沒有內建；例子用 Resource 或 `States`＋`run_if`，**主迴圈照跑、每幀輪詢**（`loading_screen.rs:26-29`、`:208-221`；`multi_asset_sync.rs:30-35`） | SubScene 一律非同步，先載進 streaming World 再 `MoveEntitiesFrom`，每次 update 最多搬一個（`SceneSectionStreamingSystem.cs:227`、`:343`、`:71`） | 沒有 | `WaitingToStart` 期間世界 tick，每 tick 輪詢 `ReadyToStartMatch`；level streaming 非同步 |
| 「好了」的信號 | 輪詢的 System 自己判、明說好了（`loading_screen.rs:217-221` 設 `LevelReady`；`multi_asset_sync.rs:265` `NextState::set`） | 作業每次回報 Completed/Busy/…（`:542-548`） | — | `ReadyToStartMatch()` 回 true |
| 讀取畫面畫在哪 | 同一個 main World（`loading_screen.rs:238-246`） | — | — | — |
| 資產還沒到 | 場景生成推回佇列、下一幀再試（`world_asset_spawner.rs:461-463`） | Busy | — | — |
| 實體配發 | 核心內、游標＋free list，不能換、不能設範圍；可繞過（`entity/mod.rs:775-850`、`world/mod.rs:1076`） | 核心內；1.2 起整個行程一份（`EntityComponentStoreEntities.cs:14`） | 核心內；`entity_ranges` addon 預設開、可在編譯期關，**刪過實體之後不能建範圍**（`entity_ranges.c:151-153`） | — |
| 讀檔的編號 | 重新配號＋對照表（`dynamic_world.rs:95-106`） | 重新配號＋remap（`SerializeUtility.cs:125-176`） | `ecs_make_alive` 可指定 | — |

**共通**：(1) 「開始前」的階段在 ECS 之上那一層；(2) 跨多幀的載入**主迴圈照跑、每幀輪詢、由作者的程式判斷好了沒有**；(3) 配發器三家都在核心內、都不能換策略。

**本頁的位置**（建議 B）：兩層照 Bevy（使用者已定）；生成期間主迴圈照跑，照三家；和三家不同的只有兩處——(a) 「生成中」由編排層擁有、每次建 World 都從它開始（三家由作者自己用狀態做）；(b) 「好了」用「沒人 `Hold()` 就結束」而不是「有人明說好了」，這是新導出的（第 14 節第 2 件）。配發器：GYO 已由使用者定「策略是產品的 Resource」（K.3-1，2026-11-11），和三家不同；本頁不改這一點，只把預設的那一份放進預設帶的 Module。

## 5. 設計（Contract；照建議 B 寫，選 A 時改的地方見 8.1 末段）

### 5.1 「生成中」（W1）

- **進入**：World 建好、準備函式全部跑完（啟動時在 `Product::Build` 的第 6 步之後；替換時在換上之後）時，那個 World 從生成中開始。一律如此，沒有開關：不在意的作者什麼都不寫，第一幀結束就結束（1 節最後一例）。
- **一幀**：`RunFrame` 對生成中的 World，跑每幀清單之前把它的 `WorldGeneration` 寫成「生成中、沒人 `Hold()`」；每幀清單照常跑；跑完讀 `WorldGeneration`：
  - 有人 `Hold()`：下一幀仍是生成中。
  - 沒有人：生成結束，編排層把 `WorldGeneration` 寫成「不在生成中」；下一幀起 `AfterGenerating` 的 set 開始跑。
- **每個 World 各自**：每個 World 看自己的狀態——A 在生成、B 早已結束，互不等待（`-game-init-list`）。
- **作者決定一切內容**：生成用的 System 放在作者自己的 set、掛 `WhileGenerating`；遊戲邏輯要等生成完才跑的，set 掛 `AfterGenerating`。跨多幀的先後（「地圖做完才放敵人」）由作者用自己的游標或條件排；Engine 不提供「逐項做完才下一項」（三家都沒有；RC-O14）。
- **「一輪」**：和交換點卡共用的定義是「一個 World 在一次 `RunFrame` 裡跑的全部」；在 B 之下就是那個 World 的每幀清單，生成中也一樣。
- **編排層不認得**生成用的 System 是什麼，也不認得 Loading、讀檔、資產這些名字。

### 5.2 `WorldGeneration` 與條件（W2、W3）

```cpp
// engine/orchestration/include/engine/orchestration/WorldGeneration.hpp
struct WorldGeneration {
    // Whether this World is generating this frame (written by the orchestration layer before the frame).
    [[nodiscard]] bool Generating() const noexcept { return generating; }
    // This frame is not the last generating one. No effect after generation has ended.
    void Hold() noexcept { held = true; }
    bool generating = false;
    bool held = false;
};
void HashValue(Kernel::Hasher&, const WorldGeneration&);
// Run conditions for an author's set (wrapped with Kernel::Condition<&WhileGenerating>("WhileGenerating")).
bool WhileGenerating(Kernel::Res<WorldGeneration>);
bool AfterGenerating(Kernel::Res<WorldGeneration>);
```

- 編排層在每個 World（骨架開關都一樣）、任何登記單位的 `Build` 之前登記它（owner `orchestration`），放初始值。生成中的每一幀開頭，由編排層在兩次 Run 之間寫成 `{generating = true, held = false}`；生成結束時寫成 `{false, false}`。
- System 用 `ResMut<WorldGeneration>` 呼叫 `Hold()`。多個 System 寫它時 Kernel 只排先後（不數寫者）。
- **不放批數**：放了的話，分 3 幀與分 1 幀生成出來的 World 摘要會不同（DI-KS 27 每個 Resource 都進摘要），「分批數不同、結果相同」就沒法比。生成結束後兩欄一律是 false。批數要用的 System 自己記；`Product` 也不記（修訂 0 的 `GenerationPasses` 拿掉：沒有 Feature 用，檢查 S10）。
- 生成結束後 `Hold()` 沒有作用；不加守衛（`no-guards-against-misuse`）。
- **為什麼用 Resource、不用 Message**（卡片照 L17 判）：要的是「這一幀有沒有人反對」與「現在是不是生成中」——兩個每幀重寫的狀態；用 Message 的話編排層要在 World 外持讀者游標並處理修剪。系譜：Bevy `NextState` 是 Resource（`resources.rs:175-181`）；讀的時機照 `AppExit`（World 外的一方在跑完之後讀，`app.rs:1406-1409`）。
- **條件為什麼由編排層給**（卡片照 L17 判）：Resource 是編排層的，條件只是讀它；照 Bevy `in_state` 由 `bevy_state` 和狀態一起給。作者也可以自己寫條件讀 `WorldGeneration`。

### 5.3 `Product` 的入口與查詢（W4）

- `bool Generating(std::string_view world) const`：那個 World 是否在生成中（`GYO_ASSERT`：沒有那個 World，同 `World()`）。runner 用它決定要不要在別的 World 畫讀取畫面（交換點卡形 (2)）。
- `RunFrame`：遊戲初始化沒跑時先跑它（O-1），再逐 World 照 5.1。
- `RunSchedule(world, schedule)`：照舊跑那一張，**不寫也不讀生成的狀態**（不清 `held`、不判結束）。只用 `RunSchedule` 推一個 World 的 runner 推不動生成：要推，用交換點卡定的「一個 World 的一輪」入口（5.12），那個入口和 `RunFrame` 共用同一段生成的處理（實作上一個私有步驟）。
- `RunGameInit`、`InsertSystems`、`World()`：照舊；生成中照樣可以動態插入（O-2）。
- **預設 runner**（只跑一次 `RunFrame`）：有人 `Hold()` 時照樣回 Success，World 仍在生成中——預設 runner 本來就只跑一幀（Bevy `run_once`）。寫進 O-3 的說明與案例。

### 5.4 和遊戲初始化、替換的先後

- **遊戲初始化在第一個 World 的生成第一幀之前**跑完（O-1 本來就在「所有 World 的第一幀之前」；生成的第一幀就是第一幀，條文不改，只補說明）。理由：遊戲初始化是整個遊戲的設定，生成是這個 World 的內容（`-game-init-list` 原話「整個遊戲初始化，還是進入遊戲後生成世界」）。
- **替換**：`ReplaceWorld` 的步驟照 O-8 不變（帶過去 → 準備 → 必要時遊戲初始化 → `beforeSwap` → 換上）；**換上之後新 World 從生成中開始**，下一個 `RunFrame` 是它的生成第一幀。生成中的 World 再被替換：新 World 重新從生成中開始；`BuildNewFirst` 失敗時舊 World 的生成狀態不變。
- **`beforeSwap` 留著**（卡片照 L17 判）：它是「換上之前、在新 World 上同步跑、失敗就保留舊 World」的關（O-8、存檔版本 Module「拒絕走讀檔失敗的路」）；生成是換上之後、跨多幀的內容建造。兩者做不同的事。
- **讀檔失敗的安全範圍收窄（要你確認，第 14 節第 3 件）**：使用者定 `-replace-order-author-choice` 時說「壞檔是最嚴重的 Bug」。讀檔的放回移到換上之後的生成，**生成期間任何 Err（作者回 Err、`AllotterExhausted`）都是 Product 結束**（O-11）；`BuildNewFirst` 保護的只剩登記、準備、`beforeSwap`。所以 O-8 那句改成「**在 `beforeSwap` 與之前失敗是安全的；生成期間失敗 Product 結束**」；要驗證的外部資料（版本、能不能解析、資產 id 在不在）放在發替換之前或 `beforeSwap`，生成裡只放已經驗證過的資料的放回。保住「放回途中失敗也安全」的做法是選項 C（新 World 在旁邊生成完才換上），本頁退掉（8.1）。

### 5.5 錯誤

- 生成期間的一張 schedule 回 Err 或丟例外：和平常一樣，World 中斷、Product 結束（O-11）。`WorldFailure` 照舊（schedule 是那一張、entry 是 `RunFrame`）。
- 作者的資料錯（讀檔 System 發現存檔壞了）不該回 Err：作者把錯寫進自己的 Resource（例：`LoadProblem`），runner 在兩次入口之間讀它、改發替換回主選單。**不要用 `Message`**：兩次 Run 之間沒有讀者游標拉住時，訊息在那次 Run 結束就被修剪（`World.cpp:1042-1049`；修訂 0 的例子錯，檢查 S1）。
- 新增錯誤碼：沒有。

### 5.6 生成期間的時間（asset 決定 9 的回答）

- 生成期間主迴圈照跑，時間 Module 也照跑。**要不要 tick 是作者的**（決定 9「等哪些才開始 tick 歸作者」），兩種寫法都用現有的機制，**時間 Module 不用改**：
  - **暫停**（`pausable=true` 的 World）：作者在生成中把 `TimeControl.paused` 設 true、生成結束設回。暫停時這一幀的可縮放量 0、不累加、不扣（`TimeTypes.cpp:64-67`），`RealTime` 照走（讀取畫面的動畫有時間用）。`Tick` 在生成期間是 0。
  - **條件**（`pausable=false` 的 World，例 PvP 的 Match）：`WorldTimeOptions::fixedStepConditions = {AfterGenerating}`。生成期間每幀那一份由下一次 `AdvanceTime` 記成 `NotDriven`、發一筆 `TimeDiscarded`（WT-2），`Tick` 0。
- 兩種寫法寫進說明頁的「生成世界」一節。
- 生成結束後的第一個固定步：暫停的寫法下照常累加（暫停期間沒有累加），不會補步。

### 5.7 讀檔掛在生成世界（讀檔宣告的要求）

| 讀檔宣告的要求 | 回答 |
|---|---|
| 讀檔 System 掛在生成世界的方式 | 讀檔 System 放在作者的 set、掛 `WhileGenerating`（任何 stage，例 `Main/Update`）；存檔資料由準備函式放進 World 的暫存 Resource（描述的 `prepare`、或 `ReplaceRequest::extra`）。存檔大時分幾幀放回、沒做完就 `Hold()` |
| 分批時照存檔編號放回的作者要在第一批之前設好配發器 | 準備函式在生成第一幀之前跑（O-7 的時機不變）。順序：帶過去 → 登記單位的準備（`entity_ids` 放預設範圍，**已經在就不蓋**）→ 描述的準備 → `extra`。讀檔在 `extra` 用 `SetResource(RangeAllotter::Of(存檔的下一號, 上限))` 蓋掉預設值。和 flecs「刪過實體之後不能建範圍」（`entity_ranges.c:151-153`）同理：下界只在 World 還沒有任何實體時移 |
| 判定要有讀檔的案例 | 判定 1 的 C1～C3、C8 |
| 重新配號的寫法 | 照讀檔宣告 5.1：`Spawn`＋作者自己的對照表；三家讀檔都這樣做（`other-engines.md` B11、B13） |
| 用 `ExclusiveWorld::Restore` 放回 | Kernel 不知道放回的用途（DI-KS 12），讀檔也可以用。差別：不補 `InitialValue`、全部放回後才跑 hook；範圍內的 id 會被復活，所以配發器也要一起放回或移下界，否則之後的 `Spawn` 會撞到（`AllottedIdAlive`）。寫進說明頁的讀檔一節，由讀檔的實作那一批決定要不要用 |
| 讀檔宣告判定 2 的四個 `RangeAllotter` 突變 | 見 5.10 末與第 14 節第 4 件 |

### 5.8 權威與回滾（第 4 批）

- **建立時的值不算寫者**（`-init-values-like-bevy`）：生成就是一般的 schedule，Kernel 不為它做任何特別處理；本頁不改。
- **一致性指紋**（DI-A 5）：收「看得到寫判定資料的 System」，`Spawn` 的 bundle 算在延後寫裡（`engine/modules/authority/src/Consistency.cpp:83-85`）。生成判定實體的 System 也進指紋，它掛的 run 條件也進（DI-A 5「執行條件與所屬 set 的執行條件」）。**兩端生成邏輯相同**時指紋相同；**只在一端生成**的（Match 生成玩家、Client 等網路送來）用已有的 `OneEndOnly` 標記。本批不改權威；要不要讓指紋自動排除生成用的 System，交網路 Module 的宣告（第 16 節）。AU-E6 只管可回滾的 schedule，生成用的 System 不在那裡就不受影響。
- **回滾 R-2 前提 6**（步的 schedule 上有生成時配發器登記成可回滾）：照舊是作者的責任。`entity_ids` 不依賴回滾 Module（方向：回滾可選、配發器預設帶）；作者寫 `rollback.Resource<EntityIds::RangeAllotter>()`。官方回滾策略要不要代為登記 World 的配發器，交回滾的後續（第 16 節）。

### 5.9 asset、存檔版本 Module、狀態機

- **asset G-1**（每一批之間 world_asset 的套用 System 會跑）：套用 System 照 asset 5.6 掛在 `Main/First`，生成中的每一幀都跑。**不用掛兩處**。
- **G-2**（結束條件由作者給）：作者的 System 用 `AssetView::Ready(...)`，沒好就 `Hold()`。
- **G-3**（`beforeSwap` 期間沒有套用點）：照舊（5.4 不改 `beforeSwap`）。
- **存檔版本 Module**（`-save-version-module`）：放在發替換之前或 `beforeSwap`（5.4），不放生成期間。
- **狀態機（重做）**：初始狀態要在生成之前、之後由那一批定。本批給它的：`WorldGeneration` 與兩個條件、`Product::Generating`。生成是「每次建 World、編排層擁有」的那一段；遊戲的狀態（主選單、關卡中、暫停）是狀態機的，兩者不重疊。

### 5.10 預設的實體配發器 `entity_ids`（W5、W6）

- **位置**：`engine/modules/entity_ids`，namespace `GYO::Engine::EntityIds`，target `gyo_entity_ids`／`GYO::EntityIds`，登記單位 `EntityIdsModule`（N11～N17、N20；`world_time` 的先例）。名字不用 `EntityAllotter`：那是 Kernel 的 concept（`Storage.hpp:414`），這個 Module 只是其中一個實現（AGENTS §6 名字是概念的 tag；檢查 N5）。`config/modules.csv` 一列：`library_only=no`、`default=yes`、`depends` 空（core、ecs、orchestration 不登記，link 寫在 CMakeLists）。
- **`Build`**：`Resource<RangeAllotter>({.name = "RangeAllotter"})`、`Allotter<RangeAllotter>()`。
- **準備函式**：World 裡沒有值時放 `RangeAllotter::Of(options.first, options.last)`；已經有（`CarryOver` 帶過去的）就不蓋（O-7）。描述的準備與 `extra` 在它之後跑，本來就能蓋掉。
- **設定**：`EntityIdsOptions{EntityId first = 1; EntityId last = 0xFFFF'FFFF;}`，經 `WorldDescription::options`（`OptionBag`）給；沒給用預設。
  - `first = 1`：0 留給作者當「沒有實體」（Unity `Entity.Null` 是 index 0、flecs 的 0 是無效 id——一般知識；本 repo 的測試遊戲也這樣用：`tests/common/orchestration/SaveGame.hpp:71` 的 `chase = 0`）。
  - `last = 2^32−1`：Kernel DI-KS 4 的鍵格式「低 32 index、高 32 generation」，預設策略世代恆 0。
- **換策略**：在那個 World 不選這個 Module，登記自己的型別。`WorldDescription::units` 空＝全選（`Product.hpp:35`），要在一個 World 去掉它得把其他單位全列出來；Product 層由產品的登記單位選單（`gyo_product_units`）去掉。另加「排除」的寫法沒有點名的 Feature，不加（RC-O6 監看）。同時選這個 Module 又登記另一種配發器，是 Kernel 既有的 RE2（`Registry.cpp:704-708`），不另加檢查。
- **不和 Base 的 `IdIssuer` 共用**（`-default-allotter-module` (1)）。
- **不做**：回收編號、回滾的登記、網路的編號對映、存檔。
- **為什麼放 Module、不放 Kernel**：K.3-1（使用者 2026-11-11）「配發策略是產品的 Resource」與 `-kernel-is-ecs-no-restrictions`。三家都把配發放核心內（`other-engines.md` B14），但 GYO 的「策略可換」是使用者已定的，本頁不重開；預設值放在預設帶、可關的 Module（`-module-defaults`）。最接近的先例是 flecs 的 `entity_ranges` 做成可關的 addon（`flecs.h:231`、`addon_defines.h:24-25`）。
- **`RangeAllotter` 搬家**：從 `engine/ecs/kernel/include/engine/kernel/detail/Storage.hpp:419-453` 搬到 `engine/modules/entity_ids/include/engine/entity_ids/RangeAllotter.hpp`（`GYO::Engine::EntityIds::RangeAllotter`），連同逐欄雜湊；行為不變。Kernel 留下 `EntityAllotter` concept、`Registrar::Allotter<T>()`、`AllotterExhausted`、`NoAllotter` 等機制；Kernel 的程式本來就不用它（`git grep` 只命中這個標頭與測試）。理由（卡片照 L17 判）：
  1. 〈每批的設計審查〉第 1 條「一個公開出口＋各實現注入」：公開出口是 Kernel 的 concept；預設的實現若型別在 Kernel、登記與範圍在 Module，「預設策略」就分在兩處。
  2. 批次表「Kernel 不內建預設策略」；[全局審計](global-audit.zh-Hant.md)第 55 行已記「`RangeAllotter`、`Owns`：與『策略由產品定』矛盾」。
  3. 搬了之後 Kernel 不附任何策略型別，DI-KS 4「配發策略由產品宣告」的字面與實際一致。
- **測試怎麼改**（不是決勝理由，§4；逐筆在第 10 節）：Kernel 的測試不 link Module，用 `tests/common/kernel/WorldTestSupport.hpp` 裡的測試用配發器（合成資料，AGENTS §7）——語意同 `RangeAllotter` 的副本，守的不是它的性質，只是讓 Kernel 的機制有東西可配。守 `RangeAllotter` 性質的測試與突變搬到 `tests/common/entity_ids/`。
- **讀檔宣告判定 2 的四個突變**（`restore-entry-declaration.zh-Hant.md:251`、`:270-274`：`Owns` 恆真／恆假、`Of` 忽略 `first`、`Allot` 不前進；讀檔還沒實作）：它們改的是 `RangeAllotter`，原定由 Kernel 的 `RestoreEntryTests.cpp` 抓。搬家後 Kernel 測試用替身，抓不到。改成：四個突變改 `entity_ids` 的 `RangeAllotter.hpp`，由用 `entity_ids` 的讀檔案例抓（讀檔的實作那一批放在 `tests/common/entity_ids/` 或 scenario）；讀檔宣告判定 1 的其餘案例照舊在 Kernel。這是改已核准判定的位置，第 14 節第 4 件請你確認。

### 5.11 只定形狀、不實作：`Regenerate`

`Product::Regenerate(world)`：兩次 Run 之間，把一個健康、生成已結束的 World 放回生成中（`WorldGeneration` 寫回生成中）。實體與 Resource 不動，清掉舊內容是作者的事。用途：在同一個 World 裡換關卡（Bevy 用狀態、Unity 把場景載進同一個 World、Unreal 的 level streaming，`other-engines.md` A2、A7）。在 B 之下實作只是寫一個旗標，但沒有直近的 Feature：讀檔一律在新的 World；「從主選單進關卡」用新 World 還是 World 內，是舊卡問過你、沒答的第 (3) 題，交狀態機重做那一批問。在那之前，「可以多次」照「每次建 World 都生成」讀（卡片照 L17 判，第 15 節）。

### 5.12 和交換點卡的交界（`gyo2-exchange`，`local_7876c9c8`）

**已對齊**（2026-11-22，兩張卡以 SendMessage 往返；修訂 1 改建議 B 之後的差異見表末）。交換點卡的宣告：`exchange-declaration.zh-Hant.md`（分支 `claude/gyo2-exchange-81ea9f`，修訂 1 840cb86c）——交換點第 5.1 節、執行緒第 5.3 節、Extract 第 5.4 節、交界第 13 節 W-1～W-9（它引用本頁修訂 0 的節號 5.1～5.5、5.14；本頁修訂 1 的對應是 5.1～5.4、5.12，已通知對方）。

**「一輪」**（兩卡共用的定義）：一個 World 在一次 `RunFrame` 裡跑的全部。

| 交界 | 本卡負責 | 交換點卡負責 |
|---|---|---|
| 資料進出 World 的三種形 | 形 (1) 套用點（World 裡的 System：asset、網路的收件送件、World 之間的郵箱）：生成中照常跑（B 之下掛 `Main` 一處即可） | 形 (2) runner 在兩次入口之間直接讀寫 World：和生成無關；形 (3) Extract：在讀者的「一輪」之前跑。**交換函式照已核准的時間設計 5.3 登記**（交換點卡修訂 1；Kernel 診斷紀錄的取走與 11-11 的郵箱需要它）：「進」「出」包住「一輪」，生成結束那一批也只各一次；只由 `RunFrame` 呼叫，`RunGameInit`、`RunSchedule` 不呼叫；有 World 壞了不呼叫。本卡不需要提供「這一輪是不是生成」 |
| 別的 World 要知道某個 World 在生成 | 提供 `Product::Generating(world)` 與 World 內的 `WorldGeneration` | 不加新機制：runner 在兩次入口之間讀 `Generating` 寫進別的 World（形 (2)），或 Module 的服務＋套用 System（形 (1)） |
| 一個 World 的一輪的入口（分頻率、分執行緒的 runner） | 生成的處理是一個私有步驟，`RunFrame` 與交換點卡的入口共用（5.3） | 入口的形狀（暫名 `Product::Lend`→`WorldLease`，本批不實作）；交出期間由持有的執行緒推它的一輪，生成的狀態跟著交出（交換點卡 5.3） |
| 產品把設定交給 Module | `entity_ids` 用每個 World 的 `WorldDescription::options` | Product 一份的設定走 `ProductDescription::options`（Product 層 Resource）及其形狀 |
| `beforeSwap` 期間 | 不改（5.4） | 寫明沒有套用點、沒有交換點（照 asset G-3） |
| 兩批合起來的案例（交換函式的進出包住生成中的一輪） | — | 後合併的那一批補測試（交換點卡第 10 節末） |
| 網路的初始同步 | 作者的 System 等到了才停止 `Hold()` | 屬網路 Module（之後）；lightyear 的 `CatchUpSnapshotReady`／`HasCaughtUp`（`other-engines.md` A9）是參考 |

修訂 1 改建議 B 之後：「一輪」的定義不變（B 之下一輪就是每幀清單）；修訂 0 要求「套用點掛 `Main/First` 與 `Generate/First` 兩處」的規則在 B 之下不需要。已通知交換點卡；你選 A 的話恢復修訂 0 的規則。

---

## 6. Architecture Delta（AGENTS §3 的七項）

1. **Feature**：讀取畫面與分批載入（使用者 11-11）；玩家存檔的讀檔在生成世界（讀檔宣告）；資產在生成期間載完再開始 tick（asset 決定 9）；作者不宣告也能 `Spawn`（`-default-allotter-module`）。
2. **現在的問題**：編排層只有「遊戲初始化（行程一次）」與「每幀」；`beforeSwap` 一次同步跑完，不能跨幀、不能畫讀取畫面；作者要 `Spawn` 得自己登記配發器並給範圍；預設策略的型別在 Kernel、沒有人替作者裝。
3. **變的邊界**：編排層多一個流程階段（生成中）、一個自己的 Resource 與兩個條件；新 Module `entity_ids`；Kernel 少一個公開型別（`RangeAllotter`）。
4. **影響**：`engine/orchestration`、`engine/ecs/kernel`（一個標頭）、新的 `engine/modules/entity_ids`、`config/modules.csv`；測試：kernel（約 10 檔換成測試用配發器）、orchestration、rollback、scenario、新的 entity_ids；文件：[orchestration.zh-Hant.md](orchestration.zh-Hant.md)、[kernel.zh-Hant.md](../kernel.zh-Hant.md) DI-KS 4、15、新頁 `entity-ids.zh-Hant.md`、[說明頁](system-contract-page.zh-Hant.md)、`docs/architecture/README.md`（Module 清單）；之後掛上來的 asset、讀檔、狀態機、網路。
5. **依賴方向**：`entity_ids` → orchestration、kernel、base；Kernel、編排層不認得它。編排層不新增依賴。
6. **Ownership**：生成的流程歸編排層（O-4 已列的職責）；預設策略歸 `entity_ids`；配發的機制仍歸 Kernel。
7. **為什麼不能更小**：只用 `beforeSwap` 不能跨幀，啟動時的 World 也沒有 `beforeSwap`；讓作者自己用狀態機做生成——狀態機還沒有，而且使用者要 Engine 認得這個階段（3.2 第二列）；不搬 `RangeAllotter` 的話預設策略分在兩處（5.10）。

## 7. Design Invariants 的變更（逐條要你核准；最終文字在合併前核准定稿）

### 7.1 編排層（[orchestration.zh-Hant.md](orchestration.zh-Hant.md)）

- **O-12（新）生成世界。** 每個 World 建好、準備函式跑完時從生成中開始。`RunFrame` 對生成中的 World，跑每幀清單之前把 `WorldGeneration` 寫成「生成中、沒人 `Hold()`」；跑完沒人 `Hold()` 就結束生成、寫成「不在生成中」。每幀清單在生成中照常跑。每個 World 各自；`RunSchedule` 不讀也不寫生成的狀態；一個 World 的一輪的入口（交換點卡）與 `RunFrame` 共用同一段處理。批數不進 World、`Product` 不記。生成期間的錯誤同平常（O-11）。編排層給條件 `WhileGenerating`、`AfterGenerating`；不認得生成用的 System 是什麼，也不認得 Loading、讀檔、資產這些名字。守衛：`orchestration.O-12` 各案（第 11 節 C1～C12）、scenario 第 6 段。
- **O-2（改）**：「每個 World 在任何登記單位的 `Build` 之前，編排層登記 `WorldGeneration`（骨架開關都一樣）」。
- **O-3（改）**：預設 runner 只跑一次 `RunFrame`，有人 `Hold()` 時照樣回 Success、World 仍在生成中。
- **O-4（改）**：職責清單「生成世界的流程（第 5 條）」去掉「（第 5 條）」。骨架不變；「`Off`（編排層不宣告）」補「schedule」兩字（`WorldGeneration` 照登記，檢查 N7）。不算職責清單多一項。
- **O-8（改）**：「換上之後，新 World 從生成中開始（O-12）；`beforeSwap` 照舊在換上之前、同步」；「`BuildNewFirst` 的讀檔失敗是安全的」改成「**在 `beforeSwap` 與之前失敗是安全的；生成期間失敗 Product 結束**；要驗證的外部資料放在發替換之前或 `beforeSwap`」（第 14 節第 3 件）。
- **Replacement Criteria（新）**：
  - **RC-O11**：有點名的 Feature 需要「新 World 生成期間舊 World 照跑、生成完才換上」（無縫轉場），或需要「讀檔放回途中失敗也保留舊 World」（第 8 節選項 C）。
  - **RC-O12**：兩個以上的產品為了「生成中」另寫自己的旗標或狀態，因為 `WorldGeneration` 與兩個條件不夠用。
  - **RC-O13**：生成要做得出來，必須讓 Kernel 認得生成（任何階段的欄位、分支）。
  - **RC-O14**：兩個以上的產品各自寫「逐項做完才下一項」的游標，考慮由編排層提供。

### 7.2 Kernel（[kernel.zh-Hant.md](../kernel.zh-Hant.md)）

- **DI-KS 4（改）**：「配發器是產品的 Resource……配發策略由產品宣告」後補「**Kernel 不附任何配發器型別**；預設的範圍配發器在 `entity_ids` Module（[entity-ids.zh-Hant.md](entity-ids.zh-Hant.md)）」。「〔併自 world DI 2〕……J-8 含突變『用盡時回繞』『加 Reset 歸零』」改成「World 內沒有配發、回收、歸零的操作；預設配發器的單調與不回繞由 EI-2 守」。
- **DI-KS 15 第 (7) 項（改字）**：「配發器（含在 World 是 Ready 時設定配發器、含範圍的下界：讀檔……）」改成「配發器（含在 World 是 Ready 時設定配發器的值；例：讀檔照存檔編號放回前，把預設範圍配發器的下界移到存檔記的下一號之後）」——機制不變，「範圍的下界」是搬出去那個型別的概念，改成例子（檢查 N8）。不算清單多一項或少一項。

### 7.3 新頁 `entity-ids.zh-Hant.md`（見出し `Design Invariants`）

- **EI-1 職責。** 每個選了它的 World：登記 `RangeAllotter` 為那個 World 的配發器；準備函式在沒有值時放 `EntityIdsOptions` 的範圍（預設 `[1, 2^32−1]`），已經有就不蓋。不做回收、回滾登記、網路對映、存檔。Kernel 與編排層不認得它。守衛：`entity_ids.module` 各案（C13～C16）。
- **EI-2 範圍配發器。** `RangeAllotter`：一個範圍 `[first, last]`，單調、不重用、用完回 `nullopt`（Kernel 報 `AllotterExhausted`）、不回繞、沒有 `Reset`；`Owns` 是範圍內。逐欄雜湊（padding 不進摘要）。守衛：第 10 節搬來的測試與突變。
- **EI-3 依賴。** 只依賴 orchestration、kernel、base；不依賴回滾、權威、時間。守衛：`gyo_expect_links`，**第 4 步、scenario 第 6 段通過之後才立**（AGENTS §3「ガードを立てる時期」；檢查 S9）；在那之前靠本頁與審查。
- **Replacement Criteria**：**RC-EI1** 有點名的產品在一個 World 的生命內用完 2^32 個編號（要回收的策略：Bevy 的 free list＋世代，`entity/mod.rs:775-850`）；**RC-EI2** Kernel 要正確必須知道配發器的型別；**RC-EI3** 兩個以上的產品各自寫同一種自訂配發器；**RC-EI4** 職責清單要多一項。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

### 8.1 生成的形狀

| 選項 | 系譜 | 支撐的 Feature | 結論 |
|---|---|---|---|
| **B：生成期間主迴圈照跑；編排層擁有「生成中」、每次建 World 都從它開始；作者用條件擋（本頁）** | 參考實作：Bevy 讀取畫面例子與 `States`＋`run_if`（`loading_screen.rs:26-29`、`multi_asset_sync.rs:30-35`）、Unity（主迴圈照跑、場景非同步）、Unreal（`WaitingToStart` 期間世界 tick）；「生成中歸編排層」是新導出（三家由作者自己做） | 分批與讀取畫面、讀檔、資產等完才 tick | **建議**（第 14 節第 1 件交你定 A 或 B） |
| A：生成中跑另一份清單（`Generate`），不跑每幀清單（修訂 0 的建議） | 新導出：Bevy `startup_labels` 延伸成可多批 | 同上 | 退：(1) 每批都要跑的東西（asset 套用、網路收件、輸入、畫面、日誌的 Forward）都要在 `Generate` 再掛一次——單一 World 的讀取畫面一出現就是好幾個單位，掛載散在每個實現（設計審查第 1 條）；(2) `Generate` 結束後永不再跑，掛在上面的訊息讀者永久拉住保留、每次 Run 記一筆丟棄（`World.cpp:1042-1049`）；`Generate` 送給只在 `Main` 的讀者的訊息在那次 Run 結束被修剪；(3) 作者把 `Main` 放進生成清單時最後一幀會跑兩次 `Main`；(4) 生成期間 `RealTime` 不前進；(5) O-4 的骨架要改，`SkeletonTests.cpp:83` 的 DI 守衛要改。好處：生成期間不 tick 是結構的結果、作者不用寫條件 |
| A'：A 加共用外框（兩份清單共用一張每批都跑的 schedule，例 `First`／`Last` 拆成獨立 schedule） | 新導出（檢查 B2 建議並列） | 同上 | 退：解掉 A 的 (1)，但 O-4 的骨架與已核准的時間 Module 掛點（`Main/First`）都要拆；(2) 照舊；比 B 多一套東西而得到的只有「不用寫條件」 |
| C：替換時新 World 在旁邊生成、舊 World 照跑，生成完才換上（無縫） | 參考實作：Unity streaming World（`SceneSectionStreamingSystem.cs:227`、`:343`）；Unreal seamless travel（一般知識） | 好處：讀檔放回途中失敗也能保留舊 World（使用者「壞檔最嚴重」）、轉場無縫 | 退：沒有點名的 Feature 要無縫（BigMap 用 `DropOldFirst`，兩張放不下）；一個 World 槽同時有兩個 World，`World()`、`InsertSystems`、失敗時的收拾都要重定；RC-O11 |
| D：Engine 提供「逐項做完才下一項」的階段序列 | 新導出（三家都沒有） | 使用者 11-11 的順序 | 退：作者用條件與自己的游標排得出；RC-O14 |
| E：把 `beforeSwap` 擴成生成（拿掉 `beforeSwap`） | 舊的形狀 | — | 退：`beforeSwap` 是「失敗就保留舊 World」的關，生成在換上之後（5.4）；啟動時的 World 沒有 `beforeSwap` |

**決勝理由（B）**：每批都跑的東西一律掛一處、照三家；生成用的 System 與遊戲邏輯各放一個帶條件的 set（Kernel 已有，`Registry.hpp:249`），條件集中在作者的兩個 set 上，不散在每個 System；時間 Module、asset、交換點都不用改；沒有永不再跑的 schedule。代價：作者要寫兩個條件、選一種「生成期間不 tick」的寫法（5.6）；「生成中」是一個兩個值的狀態，和之後的狀態機分工要寫清楚（5.9）。

**選 A 時本頁要改的**：5.1～5.3 換成修訂 0 的「生成清單＋`Generate` 骨架」；5.9、5.12 恢復「套用點掛兩處」；5.6 改成「結構上不 tick」並補 S2、S3 的例外；第 10 節加 O-4 骨架守衛（`SkeletonTests.cpp:83`）的 §14.4 核准；第 13 節加訊息游標的兩條限界。

**旧い形に置き場所を探していないか**（〈Batch declaration checks〉）：B 不是把舊的 `Domain::Loading`、runtime-composition 3a 的 stage 序或 K0 的「Startup 由 Kernel 跑一次」搬到編排層——Kernel 那邊已拿掉；B 的形狀從三家「主迴圈照跑、作者擋」導出，編排層只多「每次建 World 都從生成開始」這一個狀態。

### 8.2 「好了」的信號

| 選項 | 系譜 | 結論 |
|---|---|---|
| **每幀沒人 `Hold()` 就結束（本頁）** | **新導出**（別家的先例都是「有人明說好了」，檢查 S4） | 建議（第 14 節第 2 件交你定） |
| 要有人呼叫 `Finish()` 才結束 | Bevy `loading_screen.rs:217-221`、`NextState::set`；Unreal `ReadyToStartMatch` 回 true | 退（若你選它，本頁改用它）：多個獨立的單位（讀檔、等資產、網路）誰來說好了要由作者寫一個總判斷；不在意生成的作者也得呼叫 |
| 用 `Message` | Bevy `AppExit` | 退：編排層要在 World 外持讀者游標與修剪（5.2） |

取捨：`Hold()` 失敗時**放行**——生成用的 System 被別的條件跳過、或提早回 Ok 忘了 `Hold()`，World 就以半成品結束生成，沒有任何訊號；`Finish()` 失敗時**卡住**（永遠在生成中），看得見。好處是 `Hold()` 讓各單位互不認得就能組在一起（asset 可以給一個「這組沒好就 `Hold()`」的現成 System）、不在意生成的作者什麼都不寫。

### 8.3 預設配發器

| 選項 | 系譜 | 結論 |
|---|---|---|
| **預設帶、可關的 Module 登記並放範圍；`RangeAllotter` 搬進 Module（本頁）** | `-default-allotter-module`；flecs `entity_ranges` addon（可關） | 採 |
| Module 登記、`RangeAllotter` 留在 Kernel | 現在的位置 | 退：預設策略分在兩處（5.10 理由 1） |
| Kernel 沒有配發器時自己配 | Bevy、flecs、Unity（核心內） | 退：推翻 K.3-1 與 DI-KS 4（使用者的決定），Kernel 職責清單第 (7) 項要改 |
| 和 Base 的 `IdIssuer` 共用 | 使用者的提問 | 退：`-default-allotter-module` (1) |

**決勝理由**：使用者已定「策略可換」，預設帶可關的 Module 是在這個前提下讓作者不必宣告的最小做法；型別跟著預設的那一份走，公開出口只留 Kernel 的 concept。既有測試要改不是理由（§4）。

## 9. thin start 與實作的步驟

**直近要用的才實作**：生成中、`WorldGeneration`、兩個條件、`Product::Generating`；`entity_ids` 的登記與預設範圍；`RangeAllotter` 搬家。**只定形狀**：`Regenerate`（5.11）；一個 World 的一輪的入口（交換點卡）。**不做**：無縫轉場、逐項序列、回收編號、`GenerationPasses`。

步驟（每步結束 ctest 通過、提交、回報）：

| 步 | 內容 | 估時 |
|---|---|---|
| 1 | `entity_ids` Module（登記檔一列、登記、準備、設定）；`RangeAllotter` 與雜湊搬家；Kernel 測試換成測試用配發器；第 10 節的測試與突變搬家；既有自己登記 `RangeAllotter` 的測試遊戲改用 Module 或替身 | 4～6 h |
| 2 | 生成中、`WorldGeneration`、兩個條件、`Generating`、替換後從生成中開始；行為案例 | 3～5 h |
| 3 | 讀檔與等外部結果的案例（C1～C9）、scenario 第 6 段 | 3～4 h |
| 4 | `gyo_expect_links`（EI-3）；文件（orchestration 頁、kernel 頁、新頁、說明頁、架構 README、dev_log）；相關突變（D184）；設計審查 | 2～3 h |

合計 **12～18 h**。查問題時超過 1.5 倍（27 h）或單一問題累計超過 2 h 就停下回報。

## 10. 守衛與測試的變更（AGENTS §14.4；`git grep -n -E "RangeAllotter|allotter"` 掃 `*mutations*.json` 與 `tests/`，正例 `we-j8-allotter-wraps` 命中）

| 變更 | 守的性質 | 緩的面 | 處置 |
|---|---|---|---|
| `we-j8-allotter-wraps`、`we-j8-allotter-reset`（`docs/architecture/plans/kernel/scripts/mutations.json:2510-2528`）搬到 `entity_ids` 的突變清單，抓的案例搬到 `tests/common/entity_ids/` | DI-KS 4 的「用盡不回繞、沒有歸零」→ EI-2 | 無（同一性質換位置；搬過去照樣在預期的失敗下 killed 才算） | **DI 守衛**：合併前示 1～5 請你核准 |
| `ks-hash-allotter-as-bytes`（同檔 `:7584-7594`，「只有它抓 Kernel 自己的雜湊函式」）搬到 `entity_ids` | `RangeAllotter` 逐欄雜湊、padding 不進摘要 | 無：雜湊函式跟著型別搬；Kernel 搬家後不再有自己的策略型別，Kernel 的逐型別雜湊由 DI-KS 27 既有的守衛守 | **DI 守衛**（DI-KS 27 的一部分）：合併前示 1～5 |
| `WorldTests.cpp:334` D-9「equal allotters give one World summary」［summary-padding-equal］搬到 `tests/common/entity_ids/` | 同上 | 無 | **DI 守衛**：合併前示 1～5 |
| `sc-lower-bound-not-moved`（`docs/architecture/target/v2/scripts/orchestration_mutations.json:1164-1171`，scenario 第一段的受け入れ判定）的 `file` 改成 `entity_ids` 的標頭；`find`、`expect` 不變 | 讀檔移下界 | 無 | 受け入れ判定的突變：事後報告；scenario 第一段改用 `entity_ids` |
| 讀檔宣告判定 2 的四個 `RangeAllotter` 突變（`restore-entry-declaration.zh-Hant.md:270-274`；讀檔還沒實作）改到 `entity_ids` 的標頭，由用 `entity_ids` 的讀檔案例抓 | 讀檔的編號兩種寫法 | 無（觀察位置改） | **改已核准判定的位置**：第 14 節第 4 件 |
| Kernel 測試（約 10 檔：`ExecutorTests`、`ImmediateChangeTests`、`InitialValueTests`、`RegistrationRoundTests`、`RestoreTests`、`SkeletonTests`、`SpawnRejectTests`、`WorldTests`、`HashTests`、`WorldTestSupport.hpp`）的 `RangeAllotter` 換成測試用配發器（同語意） | 機制（`AllotterExhausted`、`IdInAllotterRange` 等），不是配發器本身 | 無 | 事後報告 |
| 編排層、回滾、scenario 的測試遊戲（`SaveGame.hpp`、`InsertTests`、`TraceSpanTests`、`rollback/StrategyTests`、`WalkerGame.hpp`、`ScenarioSegment5Tests`）改用 `entity_ids` 或替身；`gyo_orchestration_tests` 不 link Module（用替身），用 Module 的案例放 `tests/common/entity_ids/` 或 scenario | — | 無 | 事後報告；每個測試 target 新增的 link 寫在 dev_log |
| 每個 World 多一個 Resource（`WorldGeneration`）：寫死 Registry 指紋、Resource 數的測試（掃描結果：orchestration、scenario、world_time、rollback、authority 沒有寫死 Registry 指紋；權威的一致性指紋只收判定資料，不受影響） | — | 無（期待值隨定義變，若實作時找到） | 事後報告，逐案寫 dev_log |

修訂 0 列的 `SkeletonTests.cpp:83`［or-skeleton-only-two］在 B 之下不動（骨架不變）。既有的判定不放寬。

## 11. 判定（先宣告，結果出來之後不放寬）

不是計測的批，判定一次決定。

### 判定 1：行為

編排層的案例在 `tests/common/orchestration/WorldGenTests.cpp`（只 link `GYO::Orchestration`，用替身配發器）；用時間 Module 的放 `tests/common/world_time/` 或 scenario；用 `entity_ids` 的放 `tests/common/entity_ids/`。合成的單機遊戲沿用 `SaveGame.hpp` 的型別。案例名在實作前換成實際的 `TEST_CASE` 名。

| # | 案例 | 檢查 |
|---|---|---|
| C1 | **啟動時讀檔、一幀**：描述的準備放存檔、移下界；讀檔 System 在 `WhileGenerating` 的 set | 第一個 `RunFrame` 之後：編號 12、57、58 在、值相同、`ChaseTarget` 仍是 12、生成已結束、之後的 `Spawn` ≥ 存檔的下一號、沒有被丟的生成；`AfterGenerating` 的 System 第一幀沒跑、第二幀跑 |
| C2 | **遊戲中讀檔、分三幀**：`ReplaceWorld`（`extra` 放存檔、移下界）；讀檔 System 每幀放一個、沒放完就 `Hold()` | 換上之後前兩個 `RunFrame` 後 `Generating` 為 true、`AfterGenerating` 的 System 沒跑；第三個後生成結束；第四個起遊戲邏輯跑；遊戲初始化的計數仍是 1 |
| C3 | **分批數不同、結果相同**：同一份存檔分 1、2、3 幀放回 | 生成結束時三者的 `World::Summary()` 相同 |
| C4 | **等外部結果、不 tick**（`world_time` 測試）：作者的 System 在一個由 World 外設定的旗標（模擬資產到了）為 false 時 `Hold()`，並照 5.6 暫停；另一案照 5.6 用 `fixedStepConditions = {AfterGenerating}`、`pausable=false`；取樣 `TimeBase` 或 `ManualInstant` | 旗標 false 的期間 `Tick` 0、`FixedUpdate` 沒跑；暫停的案 `RealTime` 前進、沒有 `TimeDiscarded`；條件的案每幀一筆 `NotDriven`；設 true 之後生成結束、下一幀起固定步前進 |
| C5 | **遊戲初始化在生成之前**：兩個 World | 遊戲初始化在第一個 World 的生成第一幀之前跑一次；替換後再生成，遊戲初始化不再跑 |
| C6 | **每個 World 各自**：A `Hold()` 兩幀、B 不 `Hold()` | 前兩個 `RunFrame` 後 A 仍在生成、B 已結束 |
| C7 | **生成中出錯**：`WhileGenerating` 的 System 回 Err | Product 結束（O-11）、`WorldFailure.schedule` 是那一張、`entry` 是 `RunFrame` |
| C8 | **`DropOldFirst` 的讀檔、分兩幀** | 舊 World 先銷毀、新 World 生成兩幀；結果同 C2 |
| C9 | **生成中再替換**：A 在生成第 2 幀時 `ReplaceWorld` | 成功：新 World 從生成中開始；`BuildNewFirst` 失敗（`extra` 回 Err）：舊 World 仍在生成中、`Hold()` 的行為照舊 |
| C10 | **`RunSchedule` 不碰生成的狀態**：生成中呼叫 `RunSchedule`，裡面的 System `Hold()` 與不 `Hold()` 各一 | `Generating` 不變；之後的 `RunFrame` 照 5.1 判 |
| C11 | **骨架關掉時 `WorldGeneration` 仍登記**；不 `Hold()` 的 World 第一幀結束就結束生成 | 照寫 |
| C12 | **預設 runner**：`Hold()` 的 World，`Product::Run()` | 回 Success、`Generating` 仍為 true |
| C13 | **不宣告就 `Spawn`**（`entity_ids`）：只選 `entity_ids` | `Spawn` 成功、第一個編號是 1 |
| C14 | **設定與兩個 World**：一個 World 給 `EntityIdsOptions{100, 199}` | 兩個 World 各自從自己的範圍配發 |
| C15 | **不蓋已經在的值**：`CarryOver` 帶過去的配發器 | 準備之後的值是帶過去的那一個 |
| C16 | **預設帶**：產品的登記單位表由真的 `config/modules.csv` 產生（`gyo_product_units`），沒有列任何單位的 World 有 `entity_ids` | `Spawn` 成功（`default=yes` 改成 `no` 時失敗） |

（修訂 0 的 C13 的「`extra` 先放的」那一半、C14 後半的 RE2 拿掉：前者本來就會蓋掉、抓不到；後者是 Kernel 既有的契約。檢查 S7、N6。）

### 判定 2：檢出力（AGENTS §14.2；只跑相關的突變，D184）

新的突變（每個的預期失敗寫成案例裡專屬的 `CHECK_MESSAGE` 標籤）：

| 突變 | 只有它抓的誤り | 預期失敗的案例 |
|---|---|---|
| `wg-held-not-cleared`：每幀開頭不清 `held` | 生成永不結束 | C2 |
| `wg-ends-while-held`：有人 `Hold()` 仍結束生成 | 半成品開跑 | C2、C4 |
| `wg-replace-not-generating`：換上之後不從生成中開始 | 讀檔 System 不跑 | C2、C8 |
| `wg-generating-not-cleared`：生成結束不寫回 `generating=false` | `AfterGenerating` 永遠不跑 | C1、C2 |
| `wg-runschedule-judges`：`RunSchedule` 也判結束 | `RunSchedule` 改了生成的狀態 | C10 |
| `wg-skeleton-off-unregistered`：骨架關掉時不登記 `WorldGeneration` | 骨架關掉的 World 不能用條件 | C11 |
| `ei-prepare-overwrites`：準備函式蓋掉已經在的值 | 帶過去的配發器被蓋 | C15 |
| `ei-not-default`：登記檔 `default=no` | 預設帶 | C16 |
| （搬來）`we-j8-allotter-wraps`、`we-j8-allotter-reset`、`ks-hash-allotter-as-bytes`、`sc-lower-bound-not-moved` | 第 10 節 | 第 10 節 |

不為誤用加突變（`design-review-not-guards`）；預設範圍的逐欄（`first`、`last` 的值）不加突變（逐欄細項，`classify-gaps-before-asking`）。生成期間 tick 前進由 `wg-ends-while-held` 在 C4 抓，scenario 不另立同一個誤り的突變（§14.5）。突變清單由和作者不同的模型做一次敵對式檢查（§14.2）。

### 判定 3：沒有回歸

本機 `ctest --preset test -L "cpu|shader"` 全過；CI L1 三平台通過；第 10 節以外的既有判定不變。

### scenario 第 6 段（`tests/common/scenario/ScenarioSegment6Tests.cpp`）

走路計分的小遊戲：S6-1 開新遊戲，地圖分 3 幀生成（讀取畫面的計數每幀加 1，照 5.6 暫停、`Tick` 0）→ S6-2 生成完開始走、計分 → S6-3 存檔 → S6-4 換 World 讀檔，讀檔分 2 幀 → S6-5 再走 5 幀，位置與分數接得上。突變：讀檔後重跑遊戲初始化——第 2 批已有 `or-` 的同一個誤り時加進它的 `cases`、不另立（§14.5；實作時查）。

## 12. 誰做、模型／檔位

**上段**（navigator：編排層的流程與公開介面、Kernel 公開型別的搬家、Design Invariants、建置圖多一個 Module）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 事前宣告的作者 | Opus／high（本卡） | — |
| 事前宣告的敵對式檢查 | Fable 5.2／high，唯讀 | 和作者換模型（§4）；主會話指定；修訂 0 已做一次 |
| 實作者（步 1～4） | Opus／high | 上段不用小模型 |
| 突變清單的敵對式檢查 | Fable／high | §14.2 換模型 |
| 設計審查（合併前） | Fable／high，唯讀 | 批次表〈每批的設計審查〉 |
| §14.4 的檢證（第 10 節的 DI 守衛搬家） | Fable／high | 和實作者換模型 |

**兩段核准**：這一次請你核准宣告（方向、設計、第 7 節的方向、第 14 節）。實作完、合併前，帶設計審查報告、突變結果與檢證，再請你核准第 7 節的最終文字與第 10 節的 DI 守衛搬家。

## 13. 範圍的限界

- 「生成中」至少一幀：每個 World 的第一幀一律是生成中；`AfterGenerating` 的 set 第一幀不跑（沒用這個條件的作者不受影響）。
- 生成期間要不要 tick 由作者選兩種寫法之一（5.6）；Engine 不預設暫停。
- `WhileGenerating` 的 System 若讀 `Message`，生成結束後它不再跑，它的游標會拉住保留（Kernel 的游標規則對任何不再跑的讀者都一樣，`World.cpp:1042-1049`）；說明頁提醒生成用的 System 不讀 `Message`，或讀完就不再需要。
- 生成用的 System 寫判定資料時進一致性指紋；只在一端生成的要作者標 `OneEndOnly`（5.8）。
- 生成期間出錯 Product 就結束；要「失敗就留舊的」的驗證放在 `beforeSwap` 或發替換之前（5.4、5.5）。
- 「分批數不同、結果相同」是作者寫法的性質，Engine 不保證；C3 只示範一種寫法下成立。
- 只用 `RunSchedule` 推的 World 推不動生成；一個 World 的一輪的入口歸交換點卡（5.3、5.12）。
- 5.12 照兩卡對齊的內容寫（交換點卡修訂 1）；任一邊的節號再改，合併前對一次。

## 14. 要你定與確認的

### 第 1 件（別家分歧、真有取捨）：生成期間主迴圈照跑（B），還是跑另一份清單（A）？

**用遊戲講**：RPG 開新遊戲要花 5 秒生成地圖，期間要有讀取畫面。
- **B（建議；Bevy、Unity、Unreal 的做法）**：這 5 秒裡 `Main` 照跑——輸入、畫面、日誌、資產的套用都照常；生成用的 System 放進「生成中才跑」的 set，遊戲邏輯放進「生成完才跑」的 set；作者讓時間暫停（或用條件擋固定步），`Tick` 停在 0。
- **A（修訂 0 的建議）**：這 5 秒裡 World 只跑生成清單（`Generate`）；`Main` 完全不跑，生成完的那一幀才第一次跑 `Main`。

| | B | A |
|---|---|---|
| 生成期間不 tick | 作者選一種寫法：暫停（`RealTime` 照走、不丟時間）或條件（每幀一筆 `NotDriven`）；時間 Module 不用改 | 結構保證，作者不用寫 |
| 作者要寫的 | 兩個帶條件的 set | 生成的 System 掛 `Generate` |
| 每批都要跑的東西（資產套用、網路收件、輸入、畫面、日誌） | 掛 `Main` 一處 | 每個都要在 `Generate` 再掛一次；單一 World 的讀取畫面一出現就是好幾個 |
| 訊息 | 沒有新的問題 | `Generate` 結束後掛在上面的讀者永久拉住保留；送給 `Main` 讀者的訊息被修剪 |
| 生成期間的真實時間 | 前進（讀取畫面的動畫可用） | 不前進 |
| 骨架（已核准的 O-4） | 不變 | 加一張 `Generate`，DI 守衛要改 |
| 和別家 | 相同 | 載入期間的做法和三家不同 |
| 和之後的狀態機 | 「生成中」是一個兩個值的狀態，要寫清楚分工（5.9） | 不重疊 |

**建議 B**。理由：每批都要跑的東西只掛一處、沒有永不再跑的 schedule、時間 Module 與 asset 都不用改，代價集中在作者的兩個 set 上。

**你 11-16 說過「Startup 才是機制，正常這些邏輯都應該寫在 Startup 裡的」**：修訂 0 拿它當選 A 的理由，那是卡片的讀法——那句指字面的 Startup（只跑一次）還是「建世界的階段」，舊卡問過你、沒答（`inventory-and-premises.md` 末段第 (2) 題）。B 也是「建世界的階段由 Engine 認得」，只是階段期間主迴圈照跑。如果你那句的意思是「階段期間只跑階段自己的東西」，請選 A。

證據：`build/evidence/gyo2-world-gen/other-engines.md` A1～A3、A7、A10；檢查報告 `declaration-r0-adversarial-check.md` B1、B2、S1。

### 第 2 件（新導出、真有取捨）：「生成好了」怎麼判？

- **沒人反對就結束（建議）**：每一幀，還沒做完的 System 呼叫 `Hold()`；哪一幀沒人呼叫就結束。不在意生成的作者什麼都不寫；讀檔、等資產、網路各自 `Hold()`，互不認得。**失敗時放行**：生成用的 System 被別的條件跳過、或忘了 `Hold()`，World 就半成品開跑，沒有訊號。
- **有人說好了才結束（別家的做法）**：作者寫一個總判斷的 System，條件都齊了呼叫 `Finish()`。**失敗時卡住**（永遠在生成中），看得見；不在意生成的作者也得呼叫一次，或由描述宣告「這個 World 不生成」。

**建議「沒人反對就結束」**：GYO 的 Module 之間靠組合（asset 可以給「這組沒好就 `Hold()`」的現成 System），一個總判斷要認得所有參與者。放行的風險由說明頁寫明、設計審查看。

### 第 3 件（確認：你之前的決定的後果）：讀檔失敗的安全範圍收窄

你定 `-replace-order-author-choice` 時說「壞檔是最嚴重的 Bug」。讀檔的放回移到換上之後的生成，**生成期間失敗 Product 就結束**；`BuildNewFirst` 保護的只剩登記、準備、`beforeSwap`。所以驗證（版本、能不能解析、資產 id）要放在發替換之前或 `beforeSwap`。要讓「放回途中失敗也保留舊 World」，得做選項 C（新 World 在旁邊生成完才換上），本頁退掉了。確認收窄，或要求做 C。

### 第 4 件（確認：改已核准判定的位置）：讀檔宣告判定 2 的四個突變

`RangeAllotter` 搬出 Kernel 之後，讀檔宣告（修訂 11，你 11-17 核准）判定 2 裡改 `RangeAllotter` 的四個突變，從「Kernel 的讀檔測試抓」改成「用 `entity_ids` 的讀檔案例抓」（5.10 末）。判定的內容不變，只改位置。確認。

## 15. 卡片照 L17 判的（不是使用者的話；你可以直接更正）

1. 「可以多次」讀成「每次建 World 都生成」；同一個 World 裡重新生成只定形狀；「從主選單進關卡用新 World 還是 World 內」（舊卡第 (3) 題，沒答）交狀態機重做那一批問（5.11）。
2. 每個 World 一律從生成中開始，沒有開關（5.1）。
3. 信號用 Resource、不用 Message；批數不進 World、`Product` 也不記（5.2）。
4. 條件 `WhileGenerating`、`AfterGenerating` 由編排層給（5.2）。
5. `beforeSwap` 留著，和生成分工（5.4）。
6. 生成期間要不要 tick 不預設，作者選暫停或條件（5.6）。
7. 名字：Resource `WorldGeneration`、方法 `Hold()`、條件 `WhileGenerating`／`AfterGenerating`、Module `entity_ids`／`EntityIdsModule`、設定 `EntityIdsOptions`（N11～N22）。
8. 預設範圍 `[1, 2^32−1]`（5.10）。
9. `RangeAllotter` 搬出 Kernel（5.10）。
10. 回滾的配發器登記照舊是作者的；一致性指紋不改（5.8）。
11. 舊卡沒答的五題：(1) 讀取畫面與分批載入仍算點名的 Feature——照統合第 240 行（已核准）；(2) 見第 14 節第 1 件；(3) 見本節第 1 條；(4) 前提表的更正併在本頁第 0、3 節；(5) 已由 `-two-layers-like-bevy` 定。

## 16. 交給其他批／會話的事

- **網路 Module 的宣告**：一致性指紋要不要自動排除生成用的 System（現在靠 `OneEndOnly`）；Client 等初始狀態的寫法（作者的 System `Hold()`）。
- **回滾的後續**：官方回滾策略要不要代為登記 World 的配發器（R-2 前提 6）。
- **狀態機（重做）**：初始狀態和生成的先後；舊卡第 (3) 題；在 World 內換關卡要不要用 `Regenerate`；和「生成中」的分工。
- **讀檔的實作**：讀檔 System 放在 `WhileGenerating` 的 set；判定 2 的四個突變改到 `entity_ids`（第 14 節第 4 件）；要不要用 `ExclusiveWorld::Restore`（5.7）。
- **存檔版本 Module**：放在發替換之前或 `beforeSwap`（5.9）。
- **asset 的實作**：套用 System 照舊掛 `Main/First`；可以給一個「這組沒好就 `Hold()`」的現成 System（選了第 14 節第 2 件的建議時）。
- **交換點卡**：一個 World 的一輪的入口與 `RunFrame` 共用生成的處理（5.3、5.12）。
- **舊清單整理卡**（`v8-arch-gyo2-stale-cleanup-start`，刪 15 份 master 時代的突變清單）：`docs/architecture/plans/kernel/scripts/mutations.json` 是 GYO2 現役的 Kernel 清單（含第 4 批加的 `ks-hash-allotter-as-bytes`），本批要從它搬三個突變，請勿刪（檢查 N3；經主會話轉）。
- **主會話**：v2 之前的 target 文件——runtime-composition DI 3／3a 由本頁取代（(1)「世界內容只經 Loading」拿掉、作者決定；(2)「ready 時 tick 0、Loading 期間不推進」由作者照 5.6 做到；(3) `tick[Loading]` 拿掉；(4) Player 與 Peer 屬 PvP，不在本頁），照「v2 之前的 target 文件由主會話一次改」。

## 17. 修訂 0 檢查的處理（`declaration-r0-adversarial-check.md`）

| 項 | 處理 |
|---|---|
| B1 B 的時間代價寫錯 | 改：5.6 兩種寫法、第 14 節表、8.1 |
| B2 A 的「掛兩處」少算、沒並列共用外框 | 改：建議改 B；8.1 加 A'；RC-O12 改寫 |
| B3 守衛分類與突變盤點 | 改：第 10 節逐筆列（含 `ks-hash-allotter-as-bytes`、D-9、`sc-lower-bound-not-moved`）；`SkeletonTests:83` 在 B 之下不動 |
| B4 讀檔宣告判定 2 的四個突變 | 改：5.10 末、第 10 節、第 14 節第 4 件、第 16 節 |
| S1 訊息游標 | 改：B 之下沒有永不再跑的 schedule；5.5 的例子改用 Resource；第 13 節加讀者的限界；A 的代價列進 8.1、第 14 節 |
| S2 `Main` 放進生成清單跑兩次 | B 之下不適用；列進「選 A 時本頁要改的」 |
| S3 第一次取樣的例外 | B 之下時間照跑，不依賴第一次取樣；C4 指定取樣模式 |
| S4 `Hold()` 的系譜 | 改：標「新導出」、寫失敗時放行；交第 14 節第 2 件 |
| S5 讀檔失敗的安全範圍 | 改：5.4、O-8 條文、選項 C 補好處；第 14 節第 3 件 |
| S6 World 內看不出、多 World 推不動、交界未對齊 | 改：`WorldGeneration` 加 `generating`；5.3 與交換點卡分工（入口歸交換點卡、處理共用）；交界已對齊（5.12） |
| S7 C13 抓不到的一半 | 改：拿掉 `extra` 那一半 |
| S8 缺的案例 | 改：加 C9～C12、C16（RunSchedule、生成中替換、骨架關掉、預設 runner、經真登記檔的預設帶）；生成清單名字的 O-E5 在 B 之下沒有生成清單、不適用 |
| S9 `gyo_expect_links` 太早 | 改：第 4 步、scenario 第 6 段之後 |
| S10 `GenerationPasses` 多餘、突變重複 | 改：拿掉；scenario 不另立同一個誤り的突變 |
| S11 拿沒確認的讀法當使用者的話 | 改：第 14 節第 1 件標明是卡片的讀法；第 15 節第 11 條列五題 |
| S12 測試 target 的 link | 改：第 10、11 節寫明各案例的位置與 link |
| N1～N9 | N1 編號重排；N2 改引 `SaveGame.hpp:71`；N3 文件分支改 c65548d9、第 16 節轉舊清單整理卡；N4 B 之下套用 System 一幀一次、不適用；N5 Module 改名 `entity_ids`、Resource 名 `RangeAllotter`；N6 拿掉；N7 O-4 補字；N8 §6 加架構 README、DI-KS 15 (7) 改字；N9 5.10 寫明 |
