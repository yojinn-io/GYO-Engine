# 事前宣告：生成世界（重做）＋預設的實體配發器（`v8-arch-gyo2-world-gen`）

更新：2026-11-22（修訂 0）。Owner：架構（GYO2 主會話開的任務卡 `gyo2-world-gen`）。**狀態：等動工前核准。實作要等使用者核准、說開始。**

**性質**：實作段第 5 批的事前宣告。涉及 AGENTS §2 的基盤 Architecture（編排層的流程、Kernel 的公開型別）、§3 的 Architecture Delta（新 Module、Kernel 公開型別搬家、編排層 Design Invariants 的增改）、§4 的系譜與決勝理由、§14.4 的守衛搬家。

**用詞**：照前幾批，設計段／實作段、動工前核准／合併前核准、第 N 步。標「**卡片照 L17 判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。要使用者定的只有第 14 節一件。

**核心一句**（每一項都拿來對照）：**生成世界是機制——編排層認得「這個 World 還在生成」、照作者給的清單一批一批推，作者說「還沒好」就再推一批；生成什麼、分幾批、等什麼才算好，全是作者的。** 預設的實體配發器同理：**Kernel 只認得「有一個配發器」，配發策略是一個可換的 Resource；預設帶一個，作者不必宣告就能 `Spawn`。**

## 0. 根據與讀的版本

- 批次表 `rebuild-batch-plan.zh-Hant.md`（文件分支 `claude/gyo2-time-design` 4030808d）第 5 批「生成世界（重做）」那一列與開頭的〈每批的設計審查〉；HANDOFF `v8-arch-gyo2-batch5-start`。
- [回收清單](recovered-from-stale-cards.zh-Hant.md)第 2 節；主 checkout `build/evidence/gyo2-build-world-phase/inventory-and-premises.md`（使用者 11-11 原話、盤點、前提表、五題）。
- HANDOFF 決定（文件分支 4030808d）：`v8-arch-gyo2-build-world-phase-is-mechanism`、`-save-unaware-means-format-only`、`-game-init-list`、`-init-values-like-bevy`、`-two-layers-like-bevy`、`-restore-entry-new-world-default`、`-restore-entry-ids-author-choice`、`-replace-order-author-choice`、`-save-version-module`、`-default-allotter-module`（主會話照 L17 判）、`-asset-visible-on-arrival`、`-broken-world-ends-product`、`-five-declarations-approved`。
- [統合宣告](kernel-app-boundary-declaration.zh-Hant.md)修訂 38：L4、L14、L16、L17；第 240 行（11-11 的讀取畫面與分批載入→生成世界）。
- [讀檔宣告](restore-entry-declaration.zh-Hant.md)修訂 11：4、5.1、5.8、第 8 節第 198 行（給本批的要求）、第 15 節。
- asset 宣告修訂 3（分支 `claude/gyo2-asset-585196` 1b120a86，已核准、未合併）：5.6、5.7、5.13（G-1～G-3）、5.14；決定 9。
- 第 4 批（已合併）：[權威頁](authority.zh-Hant.md) DI-A 5、第 4 節；[回滾頁](rollback.zh-Hant.md) R-2 前提 6；Kernel DI-KS 12、DI-KS 15 第 (7)、(13) 項。
- 已合併的程式：`origin/gyo2` 45f13757——`engine/orchestration`（`Product.hpp`、`Setup.hpp`、`ScheduleSkeleton.hpp`、`src/Product.cpp`）、`engine/ecs/kernel/include/engine/kernel/detail/Storage.hpp:409-462`（`EntityAllotter`、`RangeAllotter`）、`Registry.hpp:238-244`、`src/Registry.cpp:693-711`（兩個不同的配發器是 RE2）、[orchestration.zh-Hant.md](orchestration.zh-Hant.md) O-1～O-11、[world-time.zh-Hant.md](world-time.zh-Hant.md) WT-2、WT-3。
- 別家原始碼：主 checkout `build/evidence/gyo2-world-gen/other-engines.md`（偵察者 Opus／high；路徑:行號都在裡面，本頁只引用結論與關鍵行）。

---

## 1. 這一批做什麼（先用遊戲講）

**單機 RPG 開新遊戲。** 玩家在主選單按「開始」。產品的 World 建好、準備函式跑完之後，World 進「生成中」。每一幀，編排層不跑平常的 `Main`，改跑這個 World 的**生成清單**（預設只有一張 `Generate`）：作者的 System 這一批生成地圖的前 2000 格、下一批生成場景物件、再下一批生成敵人、最後放玩家（使用者 11-11 原話的順序）。每一批裡，還沒做完的 System 呼叫一次 `Hold()`（「還沒好」）。哪一批跑完沒有人說「還沒好」，生成就結束，同一幀接著跑 `Main`——固定步從這一刻才開始，`Tick` 在生成期間一直是 0。讀取畫面的 System 也掛在 `Generate` 裡，所以生成期間畫面會動。

**遊戲中讀另一個存檔。** 玩家在第 3 關按「讀取存檔 2」。作者先在現在的 World 驗證存檔（版本、資產 id 全部載好；不對就不發替換）；再 `ReplaceWorld`，在 `extra` 準備函式裡放好暫存的存檔資料、把配發器範圍的下界移到存檔記的下一號之後。新 World 換上之後進生成中：作者的讀檔 System 掛在 `Generate`，照存檔編號放回物品、任務、觸發條件（存檔大就分幾批，`Hold()`）；做完，新 World 開始跑 `Main`。遊戲初始化不再跑。

**等資產。** 地圖用的模型還在背景解碼。asset Module 的套用 System 同時掛在 `Main` 與 `Generate` 的 `First`，所以每一批開頭 World 都看到最新的載入結果；作者的 System 用 asset 的就緒查詢看「全部好了沒有」，沒好就 `Hold()`。判定要用的內容在生成期間就載完，之後才開始 tick（asset 決定 9）。

**作者什麼都不宣告就 `Spawn`。** 卡牌遊戲的作者只登記元件、寫一個生成牌的 System。預設帶的 `entity_allotter` Module 已經在每個 World 登記好範圍配發器、放好預設範圍 `[1, 2^32−1]`；`Spawn` 直接拿得到編號。PvP 的作者想要「單調不重用」——預設的就是；想要別的策略，就在那個 World 關掉這個 Module、登記自己的配發器型別。

**什麼都不生成的工具。** 描述關掉骨架、不給生成清單：World 建好就直接跑每幀清單，和現在一樣。

## 2. 範圍

### 2.1 這一批做的（W1～W8）

| # | 內容 | 歸屬 |
|---|---|---|
| W1 | 每個 World 一份**生成清單**（`WorldDescription::generate`、`WorldSetup::Generate()`，和每幀清單同樣的插入規則） | 編排層 |
| W2 | **生成中**：World 建好（啟動、每次替換）且準備函式跑完後進入；`RunFrame` 對生成中的 World 跑一批生成清單；沒有人 `Hold()` 就結束生成、同一個 `RunFrame` 接著跑每幀清單 | 編排層 |
| W3 | 編排層自己的 Resource `WorldGeneration`（`Hold()`），每個 World 都登記；每批開頭由編排層清掉 | 編排層 |
| W4 | 預設骨架多一張 `Generate`（stage 同 `Main` 去掉 `FixedMainLoop`），生成清單沒寫它時放在最前面 | 編排層 |
| W5 | `Product::Generating(world)` 查詢 | 編排層 |
| W6 | **預設的實體配發器** `entity_allotter` Module：每個 World 登記範圍配發器、準備函式放預設範圍（不蓋掉已經在的值）、範圍由 `EntityAllotterOptions` 給；登記檔 `default=yes` | 新 Module |
| W7 | `RangeAllotter` 從 Kernel 搬進 `entity_allotter`（Kernel 只留 `EntityAllotter` 這個概念與配發的機制） | Kernel、新 Module |
| W8 | 讀檔掛在生成世界的案例、等外部結果的案例、多 World 的案例；scenario 第 6 段 | 測試 |

### 2.2 不在這一批

- **在已經跑起來的 World 裡重新進生成**（`Regenerate`）：只定形狀（5.13），不實作。沒有直近的 Feature（讀檔一律在新的 World；從主選單進關卡由狀態機重做那一批決定用替換還是 World 內的狀態）。
- **新 World 生成期間舊 World 照跑、生成完才換上**（無縫轉場）：不做（第 8 節退掉的選項 C；RC-O11）。
- 讀取畫面的畫法、轉場、淡出（作者；asset 決定 9）。
- 讀檔的實作、存檔版本 Module、狀態機重做（各自那一批；本頁只寫交界，5.8～5.11）。
- 交換點、多個 World 之間的資料、Product 層 Resource（交換點卡 `gyo2-exchange`；交界見 5.14）。
- 回滾的代為登記配發器（R-2 前提 6 照舊是作者的責任；5.10）。
- 回收編號的配發策略（Bevy 的 free list＋世代）：沒有點名的 Feature（RC-EA1）。

## 3. 基盤 Architecture 的根據（AGENTS §2）

### 3.1 為什麼算基盤

- 生成世界是每個產品建 World 時都會走的流程；之後的讀檔、asset、狀態機、網路的初始同步都會掛上來。形狀一旦有人掛，改形狀要改所有掛上來的單位。
- `RangeAllotter` 是 Kernel 的公開型別，十幾個測試檔與兩個測試用的遊戲在用；搬家越晚越貴（`rename-early-with-rules`）。

### 3.2 予見的根據 → 要求（Feature → Contract → Architecture）

| 根據（出處） | 種類 | 推出的要求 |
|---|---|---|
| 使用者 11-11：地圖→場景→物件→敵人→玩家、物件太多可以分批、Client 要有讀取頁面、「現在物件少可以算 Startup，將來越來越多呢，先把契約訂好」（`inventory-and-premises.md` 開頭） | 1 名指しの Feature | 生成可以跨多幀分批；分批期間能畫讀取畫面；順序由作者排 |
| 使用者 11-16：「開局、Loading、讀檔都是遊戲邏輯的叫法，不是機制，Startup 才是機制」、「Loading 有沒有是作者決定」、「世界生成階段 Engine 不認識，那 Engine 如何排列流程？」（`-build-world-phase-is-mechanism`、`-save-unaware-means-format-only`） | 1 | Engine（編排層）認得「生成」這個階段並排流程；不認得 Loading、讀檔這些名字 |
| 使用者 11-16：遊戲初始化與生成世界分開；生成世界每個 World 各自、可以多次、可以分批（`-game-init-list`） | 1 | 每個 World 一份；每次建 World 都生成；遊戲初始化在前 |
| 使用者 11-17：照 Bevy 兩層，編排層認得、Kernel 不認得（`-two-layers-like-bevy`）；初始值在建立當下給、生成只是一般的 schedule（`-init-values-like-bevy`） | 1 | Kernel 不加任何階段的概念；生成清單裡是一般的 schedule |
| 讀檔宣告修訂 11：讀檔 System 掛在生成世界；可以分批時，照存檔編號放回的作者要在第一批之前設好配發器；判定要有讀檔的案例 | 1（玩家存檔） | 第一批之前有準備時機（已有：O-7）；讀檔的案例 |
| asset 決定 9：判定要用的內容在生成時就載完、之後才開始 tick；轉場歸作者 | 1 | 生成期間固定步不前進；等什麼由作者的 System 判 |
| asset G-1～G-3 | 1 | 生成的每一批之間 World 看得到新的載入結果；結束條件由作者給 |
| `-default-allotter-module`：作者只要呼叫函式；不放 Kernel、不共用 `IdIssuer`、預設帶可關可換 | 1（主會話照 L17 判） | 預設 Module 登記配發器並放範圍 |
| 別家：Bevy `run_main` 第一次先跑 startup 清單（`main_schedule.rs:290-304`）；讀取畫面每幀輪詢（`examples/showcase/loading_screen.rs:208-221`）；Unity 場景載入一律非同步、幾幀後才好（`streaming-overview.md:3`）；Unreal 每 tick 輪詢 `ReadyToStartMatch`（一般知識） | 2 參考實作 | 怎麼做（第 4 節） |
| 規模：BigMap 約 1 km²、區塊化地圖 242 MB（`-replace-order-author-choice` 引用 master 的計畫）；一幀 16.7 ms | 3 規模 | 一次生成不能塞在一幀；生成要能分到很多幀 |

導不出、也不放進來的：「生成完自動跑某件事」的通知事件（沒有點名的 Feature；狀態機要時由那一批提）、生成的進度百分比（作者自己算）。

## 4. 別家怎麼做（出處：`other-engines.md`）

| 問題 | Bevy | Unity Entities | flecs | Unreal（一般知識） |
|---|---|---|---|---|
| 「開始前跑一次」在哪一層 | `bevy_app` 的 `MainScheduleOrder` 有 `startup_labels` 與 `labels` 兩張清單，`run_main` 第一次先跑前者（`main_schedule.rs:213-233`、`:290-304`）；`bevy_ecs` 沒有（grep 0 筆） | 沒有 World 層級的；`OnStartRunning` 暫停後會再跑 | pipeline addon 的 `OnStart`，第一次 `progress` 用臨時 pipeline 跑完就刪（`pipeline.c:711-736`、`:773-776`） | `BeginPlay` 在世界載好之後 |
| 跨多幀的載入 | 沒有內建；例子用 Resource 或 `States`＋`run_if`，**每幀輪詢**資產（`loading_screen.rs:208-221`、`multi_asset_sync.rs:30-35`） | SubScene 一律非同步，先載進 streaming World 再 `MoveEntitiesFrom` 搬進主 World，每次 update 最多搬一個（`SceneSectionStreamingSystem.cs:227`、`:343`、`:71`） | 沒有 | `ReadyToStartMatch` 每 tick 輪詢；level streaming 非同步 |
| 載入期間主迴圈 | 照跑（遊戲邏輯由作者用狀態擋） | 照跑 | — | 世界 tick，比賽未開始 |
| 「好了」的信號 | `NextState`（Resource，World 內固定位置套用，`resources.rs:175-181`）；`AppExit`（Message，runner 在 update 之後讀，`app.rs:1406-1409`、`:1541`） | 作業每次回報 Completed/Busy/…（`:542-548`） | — | `ReadyToStartMatch()` 回 true |
| 資產還沒到 | 場景生成推回佇列、下一幀再試（`world_asset_spawner.rs:461-463`） | Busy | — | — |
| 實體配發 | 核心內、游標＋free list，不能換、不能設範圍；可繞過（`spawn_at`）（`entity/mod.rs:775-850`、`world/mod.rs:1076`） | 核心內；1.2 起整個行程一份（`EntityComponentStoreEntities.cs:14`、`CHANGELOG.md:558`） | 核心內；`entity_ranges` addon 預設開、可在編譯期關，每個 World 多個範圍，**刪過實體之後不能建範圍**（`entity_ranges.c:151-153`） | — |
| 讀檔的編號 | 重新配號＋對照表（`dynamic_world.rs:95-106`） | 重新配號＋remap（`SerializeUtility.cs:125-176`） | `ecs_make_alive` 可指定 | — |

**共通**：(1) 「開始前」的階段都在 ECS 之上的那一層；(2) 跨多幀的載入都是**每幀輪詢、由作者的程式判斷好了沒有**，沒有一家做「逐項做完才下一項」的內建流程；(3) 配發器三家都在核心內、都不能換策略。

**分歧**：載入期間，三家都讓**主迴圈照跑**、遊戲邏輯由作者擋；本頁讓生成中的 World **跑另一份清單、不跑每幀清單**（Bevy startup 清單的延伸）。這一處是真有取捨的分歧，交第 14 節。

**本頁的位置**：兩層照 Bevy（使用者已定）；生成清單是 Bevy `startup_labels` 的延伸——每個 World 一份、每次建 World 都跑、可以一批一批跑；「好了沒有」照每幀輪詢的先例，由作者的 System 判；信號放 World 外的編排層在每批之後讀（同 `AppExit` 的讀法），用 Resource 而不用 Message（5.3）。配發器：GYO 已由使用者定「策略是產品的 Resource」（K.3-1，2026-11-11），和三家不同；本頁不改這一點，只把預設的那一份放進預設帶的 Module。

## 5. 設計（Contract）

### 5.1 生成清單與「生成中」（W1、W2）

- **兩份清單**：每個 World 有生成清單與每幀清單。生成清單的初值是 `WorldDescription::generate`，登記單位在 `Build` 經 `WorldSetup::Generate()` 插入（`Append`、`InsertBefore`、`InsertAfter`，規則同 `ScheduleList`：錨點不在是 O-E5）。系譜：Bevy `MainScheduleOrder` 的兩張清單（`main_schedule.rs:213-218`）。
- **進入**：World 建好、準備函式全部跑完（啟動時在 `Product::Build` 的第 6 步之後；替換時在換上之後）時，生成清單不空就進生成中、批數從 0 起；空的就直接是平常狀態。
- **一批**：`RunFrame` 遇到生成中的 World：編排層先把它的 `WorldGeneration` 清成「沒有人說還沒好」，依序跑生成清單的每一張，批數加 1；跑完讀 `WorldGeneration`：
  - 有人 `Hold()`：這個 World 這一次 `RunFrame` 到此為止，每幀清單不跑。
  - 沒有人：生成結束，**同一個 `RunFrame` 接著跑每幀清單**。系譜：Bevy 第一次 update 先跑 startup 清單、再跑 `labels`（`main_schedule.rs:291-304`）；只生成一批的 World 因此和現在一樣「第一個 `RunFrame` 就跑到 `Main`」。
- **每個 World 各自**：`RunFrame` 照 World 的順序，每個 World 看自己的狀態——A 在生成、B 照常跑幀，互不等待（`-game-init-list`）。
- **作者決定一切內容**：生成清單裡放幾張 schedule、每批做多少、什麼順序、等什麼，都是作者的。跨多批的先後（「地圖做完才放敵人」）由作者的 System 用自己的游標或 run 條件排；Engine 不提供「逐項做完才下一項」（三家都沒有，第 4 節；RC-O14）。
- **卡片照 L17 判**：預設是「跑完一批沒人反對就結束」，不是「要有人說好了才結束」。理由：(1) 多個獨立的 System（讀檔、等資產、生成地圖）各自投票、互不認得就能組在一起；要有人說「好了」的話，誰來說要協調。(2) 只要一批的作者（讀檔放回一次做完）什麼都不用寫，和 Bevy startup 相同。系譜：Bevy `loading_screen.rs` 的完成條件由輪詢的 System 自己判（`:208-221`）、Unreal `ReadyToStartMatch` 每 tick 輪詢。

### 5.2 預設骨架多一張 `Generate`（W4）

- 骨架開著（`ScheduleSkeleton::Default`）時，編排層在任何登記單位的 `Build` 之前多宣告一張 `Generate`，stage 依序 `First`、`PreUpdate`、`Update`、`PostUpdate`、`Last`（同 `Main`，去掉 `FixedMainLoop`：生成期間不驅動固定步）。名字放 `DefaultSchedules::generate`。生成清單沒寫 `Generate` 時放在最前面（同 `Main` 之於每幀清單）。
- 骨架關掉時不宣告；作者自己給生成清單（或不給，World 不生成）。
- 名字照 N22b（schedule 用階段名；`Simulate` 是先例）：`Generate`。不用 `Loading`、`Startup`：前者是遊戲流程的名字（使用者 11-16），後者在 Bevy 是「只跑一次」，和「每次建 World、可以多批」不同。
- 這是 O-4「生成世界的流程」那一項的預設值，**不算職責清單多一項**（同 `Main` 是「每幀的清單」的預設值，使用者 2026-11-19 核准的先例）。

### 5.3 `WorldGeneration`（W3）

```cpp
// engine/orchestration/include/engine/orchestration/WorldGeneration.hpp
struct WorldGeneration {
    // This pass is not the last one: the generate list runs again next RunFrame.
    void Hold() noexcept { held = true; }
    bool held = false;
};
void HashValue(Kernel::Hasher&, const WorldGeneration&);
```

- 編排層在每個 World（骨架開關都一樣）、任何登記單位的 `Build` 之前登記它（owner `orchestration`），放初始值。每批開頭由編排層在兩次 Run 之間寫回 `held=false`。
- System 用 `ResMut<WorldGeneration>` 呼叫 `Hold()`。多個 System 都寫它時 Kernel 只排先後（不數寫者，`-kernel-is-ecs-no-restrictions`）。
- **不放批數**：放了的話，分 3 批與分 1 批生成出來的 World 摘要會不同（DI-KS 27 每個 Resource 都進摘要），「分批數不同、結果相同」就沒法比。批數由 `Product` 記（5.4）；System 要用批數自己記。生成結束後 `held` 一律是 false，摘要不受批數影響。
- 生成結束後 `Hold()` 沒有作用；在 `Main` 裡呼叫它是寫錯，不加守衛（`no-guards-against-misuse`）。
- **為什麼用 Resource、不用 Message**（卡片照 L17 判）：兩種都有先例（`NextState` 是 Resource、`AppExit` 是 Message，`other-engines.md` A3、A5）。這裡要的是「這一批有沒有人反對」——一個每批重設的旗標；用 Message 的話編排層要在 World 外持有讀者游標並處理修剪，多一套東西而沒有多得到什麼。讀的時機照 `AppExit`：World 外的一方在跑完之後讀。

### 5.4 `Product` 的入口與查詢（W5）

- `bool Generating(std::string_view world) const`：那個 World 是否在生成中（`GYO_ASSERT`：沒有那個 World，同 `World()`）。`std::uint32_t GenerationPasses(world) const`：這一次生成已跑的批數（runner、除錯與測試用）。
- `RunSchedule(world, schedule)`：照舊跑那一張，不看也不改生成的狀態。
- `RunGameInit`：照舊。`RunFrame`：遊戲初始化沒跑時先跑它（O-1），再逐 World 照 5.1。
- `InsertSystems`：生成中照樣可以（O-2 不變）。
- `World()`：照舊回那個 World 的執行器。
- runner 要讓生成快一點（一幀跑好幾批）時，多呼叫幾次 `RunFrame` 即可；另開「只跑一批」的入口沒有點名的 Feature，不加。

### 5.5 和遊戲初始化、替換的先後

- **遊戲初始化在第一個 World 的第一批生成之前**跑完（O-1 的「在所有 World 的第一幀與任何 `RunSchedule` 之前」加上「與任何一批生成之前」）。理由：遊戲初始化是整個遊戲的設定（按鍵、UI 設定），生成是這個 World 的內容（`-game-init-list` 原話「整個遊戲初始化，還是進入遊戲後生成世界」）。
- **替換**：`ReplaceWorld` 的步驟照 O-8 不變（帶過去 → 準備 → 必要時遊戲初始化 → `beforeSwap` → 換上）；**換上之後新 World 進生成中**，從下一個 `RunFrame` 開始一批批跑。
- **`beforeSwap` 留著**（卡片照 L17 判）：它和生成做的是不同的事。`beforeSwap` 是「換上之前、在新 World 上同步跑、失敗就保留舊 World」——`BuildNewFirst` 讀檔失敗安全的那道關（O-8、存檔版本 Module「拒絕走讀檔失敗的路」）；生成是換上之後、跨多幀的內容建造。生成期間出錯時舊 World 已經不在，所以**要驗證的東西放在發替換之前或 `beforeSwap`**，生成裡只放「已經驗證過的資料」的放回。這一句寫進說明頁。

### 5.6 錯誤

- 生成清單裡的一張回 Err 或丟例外：和每幀清單一樣，World 中斷、Product 結束（O-11）；`WorldFailure` 的 `schedule` 是那一張（例：`Generate`），`entry` 是 `RunFrame`。生成的 World 是 Product 的 World（換上之後才生成），沒有「新 World 丟掉、舊 World 照跑」的路——那條路屬 `beforeSwap`（5.5）。
- 作者的資料錯（讀檔 System 發現存檔壞了）不該回 Err（那會結束 Product）：作者自己處理，例如寫一個作者的 `Message`、讓 runner 改發替換回主選單。說明頁寫這個例子。
- 新增錯誤碼：沒有。

### 5.7 生成期間的時間（asset 決定 9 的回答）

- 預設的生成清單不含 `Main`，時間 Module 的 `AdvanceTime`（`Main/First`）與 `RunFixedSteps`（`Main/FixedMainLoop`）在生成期間不跑：`Tick` 一直是 0、`FixedTime` 不累加。生成結束那一幀第一次跑 `AdvanceTime`，照 WT-2「建好 World 後第一次取樣 delta＝0」——生成花了多久都不會在第一幀補固定步。**時間 Module 不用改。**
- 生成期間 `RealTime` 不前進。讀取畫面要動畫的時間，由作者自己讀時間基準，或之後由時間 Module 另提（沒有點名的 Feature；第 13 節）。
- 作者想讓遊戲邏輯在生成期間就跑（例：背景先跑物理），把 `Main` 放進生成清單即可——那是作者選的（決定 9「等哪些才開始 tick 歸作者」）。

### 5.8 讀檔掛在生成世界（讀檔宣告的要求）

| 讀檔宣告的要求 | 回答 |
|---|---|
| 讀檔 System 掛在生成世界的方式 | 作者把讀檔 System 掛在 `Generate`（或自己插進生成清單的一張）；存檔資料由準備函式放進 World 的暫存 Resource（描述的 `prepare`、或 `ReplaceRequest::extra`）。存檔大時分批放回、沒做完就 `Hold()` |
| 可以分批時，照存檔編號放回的作者要在第一批之前設好配發器 | 準備函式在第一批之前跑（O-7 的時機不變，生成在準備之後才開始）。順序：帶過去 → 登記單位的準備（`entity_allotter` 放預設範圍，**已經在就不蓋**）→ 描述的準備 → `extra`。讀檔在 `extra` 用 `SetResource(RangeAllotter::Of(存檔的下一號, 上限))` 蓋掉預設值。和 flecs「刪過實體之後不能建範圍」（`entity_ranges.c:151-153`）同理：下界只在 World 還沒有任何實體時移 |
| 判定要有讀檔的案例 | 判定 1 的 C1～C3、C8（第 11 節） |
| 重新配號的寫法 | 照讀檔宣告 5.1：`Spawn`＋作者自己的對照表；三家讀檔都這樣做（`other-engines.md` B11、B13） |
| 用 `ExclusiveWorld::Restore` 放回 | Kernel 不知道放回的用途（DI-KS 12），讀檔也可以用。差別：不補 `InitialValue`、全部放回後才跑 hook；範圍內的 id 會被復活，所以配發器也要一起放回或移下界，否則之後的 `Spawn` 會撞到（`AllottedIdAlive`）。這一段寫進說明頁的讀檔一節，由讀檔的實作那一批決定要不要用 |

### 5.9 asset 的需求（asset 宣告 5.13）

| # | 回答 |
|---|---|
| G-1 每一批之間 world_asset 的套用 System 會跑 | world_asset 把套用 System 同時掛在 `Main/First` 與 `Generate/First`（預設骨架）；骨架關掉時由產品掛（同 O-4）。生成那一批跑完、同一幀接著跑 `Main` 時套用 System 跑兩次，第二次照 asset 5.6 的版本號比對直接跳過 |
| G-2 結束條件由作者的 System 給 | 作者的 System 用 `AssetView::Ready(...)`，沒好就 `Hold()` |
| G-3 `beforeSwap` 期間沒有套用點 | 照舊（5.5 不改 `beforeSwap`）；`beforeSwap` 裡讀得到的只有準備函式拉過一次的視圖 |

### 5.10 權威與回滾（第 4 批）

- **建立時的值不算寫者**（`-init-values-like-bevy`）：生成就是一般的 schedule，Kernel 不為它做任何特別處理；本頁不改。
- **一致性指紋**（DI-A 5）：收「看得到寫判定資料的 System」，`Spawn` 的 bundle 算在延後寫裡（`engine/modules/authority/src/Consistency.cpp:83-85`）。所以掛在 `Generate`、生成判定實體的 System 也進指紋。**兩端生成邏輯相同**（Match 與 Client 都照地圖資產生成）時指紋相同；**只在一端生成**的（Match 生成玩家、Client 等網路送來）要用已有的 `OneEndOnly` 標記。本批不改權威；要不要讓指紋自動排除生成清單上的 System，交網路 Module 的宣告（第 16 節）。
- **回滾 R-2 前提 6**（步的 schedule 上有生成時配發器登記成可回滾）：照舊是作者的責任。`entity_allotter` 不依賴回滾 Module（方向：回滾可選、配發器預設帶）；作者寫 `rollback.Resource<EntityAllotter::RangeAllotter>()`。官方回滾策略要不要代為登記 World 的配發器，交回滾的後續（第 16 節）。
- **digest**：配發器的值不是判定資料，不進 digest（權威 DI-A 5 只收權威登記的型別）。

### 5.11 存檔版本 Module、狀態機（之後的批）

- **存檔版本 Module**（`-save-version-module`）：比對版本標記、拒絕走讀檔失敗的路——放在發替換之前或 `beforeSwap`（5.5），**不放生成清單**（生成時舊 World 已經不在，失敗不再安全）。
- **狀態機（重做）**：初始狀態要在生成之前、之後、還是插進生成清單，由那一批定。本批給它的：生成清單可插入（W1）；`Product::Generating(world)`；生成結束那一幀每幀清單照常跑，狀態機的套用 System 在 `Main` 裡就自然在生成之後。

### 5.12 預設的實體配發器（W6）

- **位置**：`engine/modules/entity_allotter`，namespace `GYO::Engine::EntityAllotter`，target `gyo_entity_allotter`／`GYO::EntityAllotter`，登記單位 `EntityAllotterModule`（N11～N17、N20；照 `world_time` 的先例）。`config/modules.csv` 一列：`library_only=no`、`default=yes`、`depends` 空（core、ecs、orchestration 不登記，link 寫在 CMakeLists，`-core-not-registered`）。
- **`Build`**：`Resource<RangeAllotter>({.name = "EntityAllotter"})`、`Allotter<RangeAllotter>()`。
- **準備函式**：World 裡沒有值時放 `RangeAllotter::Of(options.first, options.last)`；已經有就不蓋（O-7）。
- **設定**：`EntityAllotterOptions{EntityId first = 1; EntityId last = 0xFFFF'FFFF;}`，經 `WorldDescription::options`（`OptionBag`）給；沒給用預設。
  - `first = 1`：0 留給作者當「沒有實體」（Unity 的 `Entity.Null` 是 index 0、flecs 的 0 是無效 id——一般知識；本 repo 的測試遊戲已經這樣用，`SaveGame.hpp` 的 `ChaseTarget{0}`）。
  - `last = 2^32−1`：Kernel DI-KS 4 的鍵格式「低 32 index、高 32 generation」，預設策略世代恆 0。
- **換策略**：在那個 World 不選這個 Module（`WorldDescription::units`；Product 層由登記檔的產品選單關掉），登記自己的型別。同時選這個 Module 又登記另一種配發器，是 Kernel 的 RE2（`Registry.cpp:703-707`，訊息帶兩個名字），不另加檢查。
- **不和 Base 的 `IdIssuer` 共用**（`-default-allotter-module` (1)）：實體編號每個 World 一套、要可重現、要能設範圍、會進存檔與網路；執行緒序號是行程一套、不需決定性。
- **不做**：回收編號、回滾的登記、網路的編號對映、存檔。
- **為什麼放 Module、不放 Kernel**：K.3-1（使用者 2026-11-11）「配發策略是產品的 Resource」與 `-kernel-is-ecs-no-restrictions`。三家都把配發放核心內（`other-engines.md` B14），但 GYO 的「策略可換」是使用者已定的，本頁不重開；預設值放在預設帶、可關的 Module，和 `-module-defaults`（Bevy `DefaultPlugins`）一致。最接近的先例是 flecs 的 `entity_ranges` 做成可關的 addon（`flecs.h:231`、`addon_defines.h:24-25`）。

### 5.13 `RangeAllotter` 從 Kernel 搬進 Module（W7）

- **現在**：`RangeAllotter` 與它的 `HashValue` 在 `engine/ecs/kernel/include/engine/kernel/detail/Storage.hpp:419-453`，註解寫「The kernel's range allotter」。Kernel 本身的程式不用它（`git grep` 只命中這個標頭與測試）。
- **搬去**：`engine/modules/entity_allotter/include/engine/entity_allotter/RangeAllotter.hpp`，`GYO::Engine::EntityAllotter::RangeAllotter`，行為不變（單調、不重用、用完回 `nullopt`、不回繞、沒有 `Reset`）。Kernel 留下 `EntityAllotter` concept、`Registrar::Allotter<T>()`、`AllotterExhausted`、`NoAllotter` 等機制。
- **理由**（卡片照 L17 判）：
  1. 〈每批的設計審查〉第 1 條「一個公開出口＋各實現注入」：配發的公開出口是 Kernel 的 `EntityAllotter` concept；`RangeAllotter` 是一個實現。預設的實現若型別在 Kernel、登記與範圍在 Module，「預設策略」就分在兩處。
  2. 批次表「Kernel 不內建預設策略」；[全局審計](global-audit.zh-Hant.md)第 55 行已記「`RangeAllotter`、`Owns`：與『策略由產品定』矛盾」。
  3. 搬了之後 Kernel 不再附任何策略型別，DI-KS 4「配發策略由產品宣告」的字面與實際一致。
- **測試的改法**（不是決勝理由，§4）：Kernel 的測試不 link Module（層的方向），在 `tests/common/kernel/WorldTestSupport.hpp` 放一個測試用的範圍配發器（合成資料，AGENTS §7）；編排層、回滾、scenario 的測試改用 Module 的型別或 Module 本身。Kernel 的 J-8 兩個突變（「用盡時回繞」「加 `Reset`」）守的是 `RangeAllotter` 的性質，跟著搬到 Module 的測試（第 10 節）。

### 5.13b 只定形狀、不實作：`Regenerate`

`Product::Regenerate(world)`：兩次 Run 之間，把一個健康、平常狀態的 World 放回生成中（批數歸 0、每幀清單停到生成結束）。實體與 Resource 不動，清掉舊內容是作者的事。用途：在同一個 World 裡換關卡（Bevy 用狀態、Unity 把場景載進同一個 World、Unreal 的 level streaming 都是「同一個 World 裡載入」，`other-engines.md` A2、A7）。有點名的 Feature 要用時（狀態機重做決定在 World 內換關卡、或其他）再實作；在那之前，「可以多次」照「每次建 World 都生成」讀（卡片照 L17 判；使用者可以更正）。

### 5.14 和交換點卡的交界（`gyo2-exchange`，`local_7876c9c8`）

2026-11-22 已用 SendMessage 送交界草案；**對方回覆前，「負責」欄是本卡的提案**。

| 交界 | 本卡 | 交換點卡 |
|---|---|---|
| World 外的結果在生成期間進 World | 規則：帶外部結果進 World 的登記單位，套用 System 同時掛 `Main/First` 與 `Generate/First`（預設骨架） | 交換點／extract 若在 World 外由編排層在兩次 Run 之間做，生成中的 World 照樣參與；若以 System 的形式掛進 World，照本卡的規則掛兩處——**由交換點卡定是哪一種** |
| 別的 World 要知道某個 World 在生成（Client 的畫面 World 畫讀取畫面、模擬 World 在生成） | 提供 `Product::Generating(world)` 與 World 內的 `WorldGeneration` | 要不要、怎麼經交換點交給別的 World |
| 產品把設定交給 Module | `entity_allotter` 用每個 World 的 `OptionBag`，不用 Product 層 Resource | Product 層 Resource 的形狀 |
| 執行緒 | 生成的狀態歸 `Product` 的那個 World 槽，照 CCR-5 在呼叫的執行緒上讀寫 | runner 怎麼分執行緒、World 在自己的執行緒時生成的批由誰推 |
| 網路的初始同步（Client 等伺服器的初始狀態） | 照 G-1 的規則；等不等由作者的 System `Hold()` | 屬網路 Module（之後）；lightyear 的 `CatchUpSnapshotReady`／`HasCaughtUp`（`other-engines.md` A9）是參考 |

---

## 6. Architecture Delta（AGENTS §3 的七項）

1. **Feature**：讀取畫面與分批載入（使用者 11-11）；玩家存檔的讀檔在生成世界（讀檔宣告）；資產在生成期間載完再開始 tick（asset 決定 9）；作者不宣告也能 `Spawn`（`-default-allotter-module`）。
2. **現在的問題**：編排層只有「遊戲初始化（行程一次）」與「每幀」；`beforeSwap` 是一次同步跑完的，不能跨幀、不能畫讀取畫面；作者要 `Spawn` 就得自己登記配發器並給範圍（兩行登記加一行範圍）；預設策略的型別在 Kernel、沒有人替作者裝。
3. **變的邊界**：編排層多一個流程階段（生成中）與一張預設 schedule、一個自己的 Resource；新 Module `entity_allotter`；Kernel 少一個公開型別（`RangeAllotter`）。
4. **影響**：`engine/orchestration`、`engine/ecs/kernel`（一個標頭）、新的 `engine/modules/entity_allotter`、`config/modules.csv`；測試：kernel（約 10 檔換成測試用配發器）、orchestration、rollback、scenario；文件：[orchestration.zh-Hant.md](orchestration.zh-Hant.md)、[kernel.zh-Hant.md](../kernel.zh-Hant.md) DI-KS 4、新頁 `entity-allotter.zh-Hant.md`、[說明頁](system-contract-page.zh-Hant.md)；之後掛上來的 asset、讀檔、狀態機、網路。
5. **依賴方向**：`entity_allotter` → orchestration、kernel、base（同 `world_time`）；Kernel、編排層不認得它。編排層不新增依賴。
6. **Ownership**：生成的流程歸編排層（O-4 已列的職責）；預設策略歸 `entity_allotter`；配發的機制仍歸 Kernel（DI-KS 15 第 (7) 項不變）。
7. **為什麼不能更小**：只加「每個 World 的生成清單、一次跑完」做不出分批與讀取畫面（使用者 11-11 的理由）；只用 `beforeSwap` 不能跨幀，啟動時的 World 也沒有 `beforeSwap`；不搬 `RangeAllotter` 的話預設策略分在兩處（5.13）。

## 7. Design Invariants 的變更（逐條要你核准；最終文字在合併前核准定稿）

### 7.1 編排層（[orchestration.zh-Hant.md](orchestration.zh-Hant.md)）

- **O-12（新）生成世界。** 每個 World 一份生成清單（初值來自描述、登記單位可插入，規則同每幀清單）。World 建好、準備函式跑完時，生成清單不空就進生成中。`RunFrame` 對生成中的 World 先把 `WorldGeneration` 清成沒人 `Hold()`、跑一批生成清單；有人 `Hold()` 就停在這裡，沒有就結束生成、同一個 `RunFrame` 接著跑每幀清單。每個 World 各自；`RunSchedule` 不看也不改生成的狀態。批數由 `Product` 記，不進 World。生成期間的錯誤同每幀（O-11）。編排層不認得生成清單裡掛的是什麼，也不認得 Loading、讀檔、資產這些名字。守衛：`orchestration.O-12` 各案（第 11 節 C1～C10）、scenario 第 6 段。
- **O-1（改）**：「在所有 World 的第一幀與任何 `RunSchedule` 之前」補「與任何一批生成之前」。
- **O-2（改）**：「每個 World 在任何登記單位的 `Build` 之前，編排層登記 `WorldGeneration`（骨架開關都一樣）」。
- **O-4（改）**：預設骨架加 `Generate`（stage 依序 `First`、`PreUpdate`、`Update`、`PostUpdate`、`Last`），生成清單沒寫它時放最前面；職責清單「生成世界的流程（第 5 條）」去掉「（第 5 條）」。不算職責清單多一項（5.2）。
- **O-8（改）**：「換上之後，新 World 進生成中（O-12）；`beforeSwap` 照舊在換上之前、同步」。補一句「要驗證的外部資料放在發替換之前或 `beforeSwap`；生成期間舊 World 已經不在」。
- **Replacement Criteria（新）**：
  - **RC-O11**：有點名的 Feature 需要「新 World 生成期間舊 World 照跑、生成完才換上」（無縫轉場；第 8 節選項 C）。
  - **RC-O12**：第三個登記單位也要把同一個 System 同時掛在 `Main` 與 `Generate`（掛兩處的規則重複到第三次），改成兩份清單共用一張每批都跑的 schedule。
  - **RC-O13**：生成要做得出來，必須讓 Kernel 認得生成（任何階段的欄位、分支）。
  - **RC-O14**：兩個以上的產品各自寫「逐項做完才下一項」的游標，考慮由編排層提供。

### 7.2 Kernel（[kernel.zh-Hant.md](../kernel.zh-Hant.md)）

- **DI-KS 4（改）**：「配發器是產品的 Resource……配發策略由產品宣告」後補「**Kernel 不附任何配發器型別**；預設的範圍配發器在 `entity_allotter` Module（[entity-allotter.zh-Hant.md](entity-allotter.zh-Hant.md)）」。「〔併自 world DI 2〕……J-8 含突變『用盡時回繞』『加 Reset 歸零』」改成「World 內沒有配發、回收、歸零的操作；預設配發器的單調與不回繞由 EA-2 守」。
- DI-KS 15 第 (7) 項：文字不變（機制仍在 Kernel）。

### 7.3 新頁 `entity-allotter.zh-Hant.md`（見出し `Design Invariants`）

- **EA-1 職責。** 每個選了它的 World：登記 `RangeAllotter` 為那個 World 的配發器；準備函式在沒有值時放 `EntityAllotterOptions` 的範圍（預設 `[1, 2^32−1]`），已經有就不蓋。不做回收、回滾登記、網路對映、存檔。Kernel 與編排層不認得它。守衛：`entity_allotter.module` 各案（C11～C14）。
- **EA-2 範圍配發器。** `RangeAllotter`：一個範圍 `[first, last]`，單調、不重用、用完回 `nullopt`（Kernel 報 `AllotterExhausted`）、不回繞、沒有 `Reset`；`Owns` 是範圍內。逐欄雜湊。守衛：搬來的 J-8 兩個突變與既有的行為案。
- **EA-3 依賴。** 只依賴 orchestration、kernel、base；不依賴回滾、權威、時間。守衛：`gyo_expect_links`（configure 時；同 `world_time` 的先例）。
- **Replacement Criteria**：**RC-EA1** 有點名的產品在一個 World 的生命內用完 2^32 個編號（要回收的策略：Bevy 的 free list＋世代，`entity/mod.rs:775-850`）；**RC-EA2** Kernel 要正確必須知道配發器的型別；**RC-EA3** 兩個以上的產品各自寫同一種自訂配發器；**RC-EA4** 職責清單要多一項。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

### 8.1 生成的形狀

| 選項 | 系譜 | 支撐的 Feature | 結論 |
|---|---|---|---|
| **A：生成中跑另一份清單（本頁）** | 新導出：Bevy `startup_labels`（`main_schedule.rs:213-233`、`:290-304`）延伸成「每個 World、每次建 World、可以多批」；「好了沒有」照每幀輪詢（Bevy `loading_screen.rs`、Unreal `ReadyToStartMatch`） | 分批與讀取畫面、讀檔、資產等完才 tick | **建議**（第 14 節交你定 A 或 B） |
| B：生成期間主迴圈照跑，編排層只立「生成中」的旗標，作者與各 Module 用 run 條件擋（Bevy 的 `States`＋`run_if`、Unity、Unreal 的做法） | 參考實作（三家） | 同上 | 退：(1) 生成用的 System 每個都要掛「生成中」的條件，遊戲邏輯每個都要掛「不在生成中」——條件散在每個實現（設計審查第 1 條）；(2) 時間 Module 要讀編排層的旗標才能「生成期間不 tick」，`AdvanceTime` 也要知道，否則生成的每一幀都記一筆 `TimeDiscarded`；(3) 等於在編排層做一個只有兩個值的狀態機，和之後重做的狀態機重疊；(4) 使用者 11-16「Startup 才是機制，這些邏輯都應該寫在 Startup 裡」指的是 A 這種「開始前的階段」 |
| C：替換時新 World 在旁邊生成、舊 World 照跑，生成完才換上（無縫） | 參考實作：Unity 先載進 streaming World 再搬（`SceneSectionStreamingSystem.cs:227`、`:343`）；Unreal seamless travel（一般知識） | 沒有點名的 Feature（BigMap 用 `DropOldFirst`，兩張放不下） | 退：一個 World 槽同時有兩個 World，`World()`、`InsertSystems`、失敗時的收拾都要重定；RC-O11 |
| D：Engine 提供「逐項做完才下一項」的階段序列 | 新導出（自己發明；三家都沒有，`other-engines.md` 看法 (a)） | 使用者 11-11 的順序 | 退：順序作者用一份清單的先後加自己的游標就排得出；RC-O14 |
| E：把 `beforeSwap` 擴成生成（拿掉 `beforeSwap`） | 舊的形狀 | — | 退：`beforeSwap` 是「失敗就保留舊 World」的關，生成在換上之後，兩者做的事不同（5.5）；啟動時的 World 沒有 `beforeSwap` |

**決勝理由**：A 把「生成期間不跑每幀、不 tick」做成結構的結果，不用在每個 System 與時間 Module 加條件；和使用者「Startup 才是機制」「兩層照 Bevy」同方向。代價：要每批都跑的 Module 掛兩處（RC-O12 監看）；生成期間時間不前進（第 13 節）。

**旧い形に置き場所を探していないか**（〈Batch declaration checks〉）：A 不是把舊的 `Domain::Loading` 或 K0 的「Startup 由 Kernel 跑一次」搬到編排層——Kernel 那邊已拿掉；A 的形狀是從 Bevy 的兩張清單導出，不是從舊的 Loading stage 序（runtime-composition 3a）來。

### 8.2 「好了」的信號

| 選項 | 系譜 | 結論 |
|---|---|---|
| **每批沒人 `Hold()` 就結束（本頁）** | Bevy startup 跑一次就結束；輪詢先例 | 採（5.1 的兩個理由） |
| 要有人呼叫 `Finish()` 才結束 | Bevy `NextState::set` | 退：多個 Module 誰來說好了要協調；只要一批的作者也得寫 |
| 用 `Message` | Bevy `AppExit` | 退：編排層要在 World 外持讀者游標與修剪（5.3） |
| 生成清單每一張回傳「好了沒有」 | — | 退：Kernel 的 schedule 沒有回傳值，要加就碰 Kernel（RC-O13） |

### 8.3 預設配發器

| 選項 | 系譜 | 結論 |
|---|---|---|
| **預設帶、可關的 Module 登記並放範圍；`RangeAllotter` 搬進 Module（本頁）** | `-default-allotter-module`；flecs `entity_ranges` addon（可關） | 採 |
| Module 登記、`RangeAllotter` 留在 Kernel | 現在的位置 | 退：預設策略分在兩處（5.13 理由 1） |
| Kernel 沒有配發器時自己配 | Bevy、flecs、Unity（核心內） | 退：推翻 K.3-1「策略是產品的 Resource」與 DI-KS 4（使用者的決定），而且 Kernel 職責清單第 (7) 項要改 |
| 和 Base 的 `IdIssuer` 共用 | 使用者的提問 | 退：`-default-allotter-module` (1) |

**決勝理由**：使用者已定「策略可換」，預設帶可關的 Module 是在這個前提下讓作者不必宣告的最小做法；型別跟著預設的那一份走，公開出口只留 Kernel 的 concept。既有測試要改不是理由（§4）。

## 9. thin start 與實作的步驟

**直近要用的才實作**：生成清單、生成中、`Hold()`、`Generate` 骨架、查詢；`entity_allotter` 的登記與預設範圍；`RangeAllotter` 搬家。**只定形狀**：`Regenerate`（5.13b）。**不做**：無縫轉場、逐項序列、生成期間的時間、回收編號。

步驟（每步結束 ctest 通過、提交、回報）：

| 步 | 內容 | 估時 |
|---|---|---|
| 1 | `entity_allotter` Module（登記檔一列、登記、準備、設定、`gyo_expect_links`）；`RangeAllotter` 與雜湊搬家；Kernel 測試換成測試用配發器；J-8 兩突變搬家；既有自己登記 `RangeAllotter` 的測試遊戲改用 Module 或在那個 World 關掉它 | 4～6 h |
| 2 | 生成清單、生成中、`WorldGeneration`、`Generate` 骨架、`Generating`／`GenerationPasses`、替換後進生成；行為案例 | 5～7 h |
| 3 | 讀檔與等外部結果的案例（C1～C10）、scenario 第 6 段 | 3～4 h |
| 4 | 文件（orchestration 頁、kernel 頁、新頁、說明頁、dev_log）；相關突變（D184）；設計審查 | 2～3 h |

合計 **14～20 h**。查問題時超過 1.5 倍（30 h）或單一問題累計超過 2 h 就停下回報。

## 10. 守衛與測試的變更（AGENTS §14.4）

| 變更 | 緩的面 | 處置 |
|---|---|---|
| Kernel J-8 的兩個突變（`RangeAllotter` 用盡時回繞、加 `Reset`）搬到 `entity_allotter` 的測試；守的性質（單調、不回繞、沒有歸零）不變 | 無（同一個性質換觀察位置；搬過去照樣在預期的失敗下 killed 才算） | **Design Invariants 的守衛**：照 §14.4 不論緩的面有無，合併前核准時示 1～5 請你核准 |
| Kernel 測試的 `RangeAllotter` 換成測試用配發器（同語意） | 無 | 事後報告 |
| 預設骨架多一張 `Generate`、每個 World 多一個 Resource：寫死 Registry 指紋或 schedule 清單的測試（若有）期待值隨定義變；`WorldFailure::lastSchedule` 在第一幀前失敗的案可能變成 `Generate` | 無（期待值隨定義變） | 事後報告，逐案寫在 dev_log |
| 同時選 `entity_allotter` 又自己登記配發器的測試遊戲（`WalkerGame`、`SaveGame` 等）：改用 Module 或在那個 World 不選它 | 無 | 事後報告 |

既有的判定不放寬。

## 11. 判定（先宣告，結果出來之後不放寬）

不是計測的批，判定一次決定。

### 判定 1：行為（`tests/common/orchestration/WorldGenTests.cpp`、`tests/common/entity_allotter/`）

合成的單機遊戲（產品自己的回呼；沿用 `SaveGame.hpp` 的型別）。案例名在實作前換成實際的 `TEST_CASE` 名。

| # | 案例 | 檢查 |
|---|---|---|
| C1 | **啟動時讀檔、一批**：描述的準備放存檔、移下界；讀檔 System 掛 `Generate` | 第一個 `RunFrame` 之後：編號 12、57、58 在、值相同、`ChaseTarget` 仍是 12、`Main` 已跑一次、之後的 `Spawn` ≥ 存檔的下一號、沒有被丟的生成 |
| C2 | **遊戲中讀檔、分三批**：`ReplaceWorld`（`extra` 放存檔、移下界）；讀檔 System 每批放一個、沒放完就 `Hold()` | 換上之後前兩個 `RunFrame` 只跑生成（`Main` 的計數不變、`Generating` 為 true、`GenerationPasses` 1、2）；第三個跑完生成並跑 `Main`；遊戲初始化的計數仍是 1 |
| C3 | **分批數不同、結果相同**：同一份存檔分 1、2、3 批放回 | 生成結束時三者的 `World::Summary()` 相同 |
| C4 | **等外部結果、不 tick**：帶時間 Module；作者的 System 在一個由 World 外設定的旗標（模擬資產到了）為 false 時 `Hold()` | 旗標 false 的期間 `Tick` 0、`FixedUpdate` 沒跑、`Main` 沒跑；設 true 之後那一幀生成結束、`Main` 跑；第一次 `AdvanceTime` 的 delta 0 |
| C5 | **遊戲初始化在生成之前**：兩個 World | 遊戲初始化在第一個 World 的第一批生成之前跑一次；替換後再生成，遊戲初始化不再跑 |
| C6 | **每個 World 各自**：A 生成 3 批、B 不生成 | 前兩個 `RunFrame`：B 的 `Main` 各跑一次、A 的不跑 |
| C7 | **生成中出錯**：生成清單的一張回 Err | Product 結束（O-11）、`WorldFailure.schedule` 是那一張、`entry` 是 `RunFrame` |
| C8 | **`DropOldFirst` 的讀檔、分兩批** | 舊 World 先銷毀、新 World 生成兩批後跑 `Main`；結果同 C2 |
| C9 | **骨架關掉、不給生成清單** | World 不進生成；第一個 `RunFrame` 直接跑每幀清單（和現在相同） |
| C10 | **骨架**：`Generate` 的 stage 與順序；登記單位往生成清單插入（`InsertBefore` 錨點不在是 O-E5）；開著骨架時作者再宣告 `Generate` 是 R-E2 | 照 O-4 的寫法 |
| C11 | **不宣告就 `Spawn`**：只選 `entity_allotter` | `Spawn` 成功、第一個編號是 1 |
| C12 | **設定與兩個 World**：一個 World 給 `EntityAllotterOptions{100, 199}` | 兩個 World 各自從自己的範圍配發 |
| C13 | **不蓋已經在的值**：`CarryOver` 帶過去或 `extra` 先放的配發器 | 準備之後的值是帶過去或 `extra` 放的那一個 |
| C14 | **換策略**：那個 World 不選 Module、登記自己的型別 → 正常；選了又登記另一種 → RE2，訊息帶兩個名字 | 照寫 |

### 判定 2：檢出力（AGENTS §14.2；只跑相關的突變，D184）

新的突變（每個一行；預期的失敗寫成案例裡專屬的 `CHECK_MESSAGE` 標籤）：

| 突變 | 預期失敗的案例 |
|---|---|
| `wg-frame-while-held`：有人 `Hold()` 仍跑每幀清單 | C2、C4 |
| `wg-hold-not-cleared`：每批開頭不清 `held` | C2（生成不結束） |
| `wg-no-frame-on-last-pass`：結束那一批不接著跑每幀清單 | C1、C2 |
| `wg-replace-no-generate`：換上之後不進生成 | C2、C8 |
| `wg-generate-before-init`：生成在遊戲初始化之前 | C5 |
| `wg-skeleton-no-generate`：骨架不宣告 `Generate` | C10 |
| `wg-passes-in-world`：批數寫進 `WorldGeneration` | C3 |
| `ea-prepare-overwrites`：準備函式蓋掉已經在的值 | C13 |
| `ea-default-first-zero`：預設 `first` 0 | C11 |
| （搬來）J-8 用盡時回繞、加 `Reset` | EA-2 的案 |

不為誤用加突變（`design-review-not-guards`）。突變清單由和作者不同的模型做一次敵對式檢查（§14.2；Design Invariants 的守衛與受け入れ判定）。

### 判定 3：沒有回歸

本機 `ctest --preset test -L "cpu|shader"` 全過；CI L1 三平台通過；第 10 節以外的既有判定不變。

### scenario 第 6 段（`tests/common/scenario/ScenarioSegment6Tests.cpp`）

走路計分的小遊戲：S6-1 開新遊戲，地圖分 3 批生成（讀取畫面的計數每批加 1，`Tick` 0）→ S6-2 生成完開始走、計分 → S6-3 存檔 → S6-4 換 World 讀檔，讀檔分 2 批 → S6-5 再走 5 幀，位置與分數接得上。突變至少各一：生成期間 tick 前進（S6-1）、讀檔後重跑遊戲初始化（S6-4）。

## 12. 誰做、模型／檔位

**上段**（navigator：編排層的流程與公開介面、Kernel 公開型別的搬家、Design Invariants、建置圖多一個 Module）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 事前宣告的作者 | Opus／high（本卡） | — |
| 事前宣告的敵對式檢查 | Fable 5.2／high，唯讀 | 和作者換模型（§4）；主會話指定 |
| 實作者（步 1～4） | Opus／high | 上段不用小模型 |
| 突變清單的敵對式檢查 | Fable／high | §14.2 換模型 |
| 設計審查（合併前） | Fable／high，唯讀 | 批次表〈每批的設計審查〉 |
| §14.4 的檢證（J-8 搬家） | Fable／high | 和實作者換模型；Design Invariants 的守衛 |

**兩段核准**：這一次請你核准宣告（方向、設計、第 7 節的方向、第 14 節）。實作完、合併前，帶設計審查報告、突變結果與檢證，再請你核准第 7 節的最終文字與 J-8 守衛的搬家。

## 13. 範圍的限界

- 生成期間 `RealTime` 不前進；讀取畫面的動畫時間要作者自己讀時間基準（5.7）。
- 生成清單上寫判定資料的 System 進一致性指紋；只在一端生成的要作者標 `OneEndOnly`（5.10）。
- 生成期間出錯 Product 就結束；要「失敗就留舊的」的驗證放在 `beforeSwap` 或發替換之前（5.5、5.6）。
- 「分批數不同、結果相同」是作者寫法的性質，Engine 不保證；C3 只示範一種寫法下成立。
- 交換點卡的回覆還沒到：5.14 的「交換點卡」欄是提案。
- 同一個 World 裡重新生成（`Regenerate`）沒有實作。

## 14. 要你定的（一件：別家分歧、真有取捨）

### 生成期間，World 跑「另一份清單」，還是主迴圈照跑、靠條件擋？

**用遊戲講**：RPG 開新遊戲要花 5 秒生成地圖。
- **A（建議）**：這 5 秒裡，World 只跑生成清單（地圖生成、讀取畫面、等資產）；平常的 `Main`（輸入、移動、固定步）完全不跑。生成完的那一幀才第一次跑 `Main`，`Tick` 從 0 開始。
- **B（Bevy、Unity、Unreal 的做法）**：這 5 秒裡 `Main` 照跑；編排層立一個「生成中」的旗標，生成用的 System 掛「生成中才跑」，遊戲邏輯掛「生成中不跑」，時間 Module 看旗標決定不 tick。

| | A | B |
|---|---|---|
| 生成期間不 tick | 結構保證（`Main` 沒跑），時間 Module 不用改 | 時間 Module 的兩個 System 都要讀旗標，否則每幀記一筆時間被丟掉 |
| 作者要寫的條件 | 不用；生成的 System 掛 `Generate` 就好 | 每個生成的 System、每個遊戲邏輯都要掛條件 |
| 要每批都跑的 Module（asset 套用、網路收件） | 掛兩處（`Main`、`Generate`） | 掛一處 |
| 生成期間的時間 | 不前進（讀取畫面的動畫要自己讀時鐘） | 前進 |
| 和別家 | 生成的階段照 Bevy 的 startup 清單；載入期間的做法和三家不同 | 和三家相同 |
| 和狀態機 | 不重疊 | 編排層等於多一個兩個值的狀態機，和之後重做的狀態機重疊 |

**建議 A**。理由：你 11-16 說「Startup 才是機制，這些邏輯都應該寫在 Startup 裡」——A 就是「Startup，但每個 World、每次建 World、可以分批」；B 的條件散在每個實現，正是設計審查第 1 條要避免的。A 的代價（掛兩處、時間不前進）有 RC-O12 監看。

證據：`build/evidence/gyo2-world-gen/other-engines.md` A1～A3、A7、A10。

## 15. 卡片照 L17 判的（不是使用者的話；你可以直接更正）

1. 「可以多次」讀成「每次建 World 都生成」；同一個 World 裡重新生成只定形狀（5.13b）。
2. 每批沒人 `Hold()` 就結束（不是要有人說好了）（5.1）。
3. 生成結束那一批同一個 `RunFrame` 接著跑每幀清單（照 Bevy）（5.1）。
4. 信號用 Resource、不用 Message；批數不進 World（5.3）。
5. `beforeSwap` 留著，和生成分工（5.5）。
6. 遊戲初始化在第一批生成之前（5.5）。
7. schedule 名 `Generate`、Resource 名 `WorldGeneration`、方法 `Hold()`、Module 名 `entity_allotter`（N11～N22b）。
8. 預設範圍 `[1, 2^32−1]`（5.12）。
9. `RangeAllotter` 搬出 Kernel（5.13）。
10. 回滾的配發器登記照舊是作者的；一致性指紋不改（5.10）。

## 16. 交給其他批／會話的事

- **網路 Module 的宣告**：一致性指紋要不要自動排除生成清單上的 System（現在靠 `OneEndOnly`）；Client 等初始狀態的寫法（照 G-1 規則掛兩處＋`Hold()`）。
- **回滾的後續**：官方回滾策略要不要代為登記 World 的配發器（R-2 前提 6）。
- **狀態機（重做）**：初始狀態和生成的先後；在 World 內換關卡要不要用 `Regenerate`。
- **讀檔的實作**：讀檔 System 掛 `Generate`；要不要用 `ExclusiveWorld::Restore`（5.8）。
- **存檔版本 Module**：放在發替換之前或 `beforeSwap`（5.11）。
- **asset 的實作**：套用 System 掛 `Main/First` 與 `Generate/First`（5.9）。
- **主會話**：v2 之前的 target 文件——runtime-composition DI 3／3a 由本頁取代（(1)「世界內容只經 Loading」拿掉、作者決定；(2)「ready 時 tick 0、Loading 期間不推進」由 O-12 的結構做到；(3) `tick[Loading]` 拿掉；(4) Player 與 Peer 屬 PvP，不在本頁），照「v2 之前的 target 文件由主會話一次改」。
