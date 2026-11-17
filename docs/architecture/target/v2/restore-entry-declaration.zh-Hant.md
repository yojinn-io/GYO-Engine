# 事前宣告：玩家存檔的讀檔——Engine 預設支持的路（`v8-arch-gyo2-restore-entry`）

- 修訂 0（2026-11-16，3dc9b464）：任務卡 gyo2-restore-entry 起草。
- 修訂 1（2026-11-16，ea62f0c4）：照換模型檢查（Fable／high，blocker 3、should 17、nit 7，全部採用）重寫。報告與逐條對照在修訂 1 第 16 節。
- 修訂 2（2026-11-16，e6466c2d）：使用者定了「一律在新的 World 裡讀」，存什麼、存檔格式、讀檔流程由作者決定。範圍縮小。
- 修訂 3（2026-11-16）：使用者定了 5.1（編號兩種寫法由作者選）與「建世界的階段」（開局、Loading、讀檔是遊戲流程的名字，機制是建世界的階段；Loading 有沒有由作者決定）。5.2 改成已定；「Loading」改寫成「建世界的階段」；新增給「建世界的階段」那一批的要求（第 9 節）。**修訂 2、3 沒有再送檢查**（要不要送由使用者定）。
- 修訂 4（2026-11-16）：使用者更正「Engine 不認得存檔」只指格式（`v8-arch-gyo2-save-unaware-means-format-only`）。「建世界的階段長什麼樣」交給另開的 `v8-arch-gyo2-build-world-phase`（主會話 2026-11-16 通知）；本宣告只寫「讀檔在建世界的階段做（寫者不計）」。5.3 依更正重擺。
- 修訂 5（2026-11-16）：使用者在邊界卡定了方向「Kernel 是 ECS，不疊加限制；限制先從 Kernel 移出，再重新評估設計」（HANDOFF `v8-arch-gyo2-kernel-is-ecs-no-restrictions`，主會話通知；範圍還沒定，包含「每欄一個寫者」）。5.2 與依賴寫者規則、區規則、「開局寫者不計」的部分改成依賴兩批還沒定的事：寫者規則重新評估的結果（邊界卡 gyo2-kernel-app-boundary）與建世界的階段的形狀（Loading 卡 gyo2-build-world-phase）。本卡不替它們定；宣告核准時這些部分寫成「待那兩批定了再補」。
- 修訂 7（2026-11-17）：第 7 節照使用者在邊界卡定的回滾方向（Kernel 只給原語、回滾是獨立的 Module、DI-KS 12 改寫）重寫，只留「和回滾不衝突」的要求；5.5 不再引用 DI-KS 12。
- 修訂 6（2026-11-17）：照主會話轉知的兩個使用者決定（邊界卡）改。`v8-arch-gyo2-init-values-like-bevy`：初始值在建立實體的當下給、不算寫者，生成世界就是一般流程裡的 schedule——5.2 由它回答，「建世界的階段」整件拿掉（Loading 卡已歸檔）。`v8-arch-gyo2-engine-not-rigid`：不把規則釘死、提問時附別家的設計——5.3 照這個重擺。
- Owner：架構（GYO2 主會話開的任務卡）。這一批只做到宣告核准為止，**不實作**。
- 基準：`gyo2` 3743eaa8（#235 合併後）。K0（PR #239）已核准、還沒合併。
- 證據：開發機（Intel Mac，使用者的本機）主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-restore-entry/`（git 不追蹤）：`scout-old-design.md`、`scout-mature-practice.md`、`premises-and-scope.md`、`declaration-check-fable-r0.md`。

---

## 0. 使用者已定的

| 代號（建議） | 原話（逐字） | 內容 |
|---|---|---|
| `v8-arch-gyo2-restore-entry-scope-player-save` | 「選第二項，Feature是「玩家存檔」。」 | 範圍＝讀檔的入口（先做單機用得到的）；回滾只定「不能和讀檔衝突」的要求與限界。Feature＝玩家存檔 |
| `v8-arch-gyo2-restore-entry-load-in-game-same-mechanism` | 「> 遊戲進行中直接讀另一個存檔（不關遊戲）算不算在裡面　算在裡面，是一個機制。」 | 遊戲中讀檔和關掉再讀是同一個機制 |
| `v8-arch-gyo2-restore-entry-new-world-default` | 「做法一、其實存檔的核心是保存玩家當前狀態（物品、任務進度、出發點條件等）。不用100%還原當前世界（怪物位置、掉落物品等）」「打錯字，是觸發條件」「這2點於機制無關，是遊戲設計該考慮的是Engine都支持。」「這2個問題有差別嗎？對於機制來說，相機和HUD是可選項。而不是必須的，每個遊戲都不用，例如卡牌遊戲和FPS和RPG存檔條件都是不一樣的。這些都應該給作者來確定，Engine保護的是機制。如果作者想吧每一個物件都存下來。是他要做的掃描，Engine不規定存檔格式，但是提供默認支持就是（方案一）」「對就是這個。」 | 存什麼、存檔格式、玩家出現在哪裡、讀檔的流程都是作者的事；Engine 保護機制、不規定存檔格式；預設支持的路＝建一個新的 World，由作者的讀檔 System 放回存檔的內容 |
| `v8-arch-gyo2-restore-entry-ids-author-choice` | 「照建議」 | 編號的兩種寫法由作者選（5.1）；Engine 保證「組裝可以在第一次 Run 前設配發器的範圍」「範圍外的 `SpawnWithId` 照常生成」 |
| `v8-arch-gyo2-build-world-phase-is-mechanism`、`v8-arch-gyo2-loading-is-author-choice` | 「為何讀檔問題會和漏斗有關係？」「讀檔應該是和生成世界是同一級別呀，為何會下有多寫者的問題？你loading有多寫者的問題嗎？」「loading就是開局呀，讀檔也是，要不然你加載了個啥？」「你先評估下我的說法，我認為，開局、loading、讀檔其實都是遊戲邏輯的叫，並不是機制。startup才是機制。正常這些邏輯都應該寫在Startup裡的。」「方向沒錯。「Domain::Loading之前評估時想的是每個遊戲都有Loading階段就記下來了，但是實際上，有沒有是作者決定的，而不是Engine」」 | 見 5.2 |

各條的比過的選項與理由在 HANDOFF（主會話持有）與下面各節。全部已轉給主會話。

---

## 1. 這一批做什麼

### 1.1 用遊戲講

RPG：玩家在第 3 關存檔，身上有 3 瓶藥水、一把鑰匙、金幣 120，任務「救出村長」做到第 2 步，觸發過「入口的過場動畫」「拉桿」「Boss 已打倒」。隔天讀檔：建新的 World；作者的程式照第 3 關的資產生成地圖、怪物、寶箱（怪物在初始位置、滿血，掉落物不在），讀檔的 System 放回物品、任務進度、觸發條件；關卡裡的東西照觸發條件決定自己的樣子（門是開的、Boss 不生成、過場不再播）；玩家出現在作者決定的地方。遊戲中讀另一個存檔走同一條路。

卡牌遊戲存牌組與對局進度；另一個作者存了每一隻怪物——都走「新 World → 作者的讀檔 System 在自己的 schedule 裡放回」。Engine 不認得任何一種存檔，也不認得「讀檔」「Loading」這些名字。

### 1.2 這一批要確保的

預設的路在機制上走得通，而且行為寫清楚：
- 編號（5.1，已定）；
- 讀檔放回的值是建立時的值，不算寫者（5.2，邊界卡的決定已回答）；
- hook、stamp、tick、診斷紀錄在這條路上的行為（5.3～5.6）；
- 「預設支持」具體包含什麼（5.7）；
- 和回滾的關係（第 7 節）。

### 1.3 Derivation Order（AGENTS §2）

| 層 | 內容 |
|---|---|
| Feature | 玩家存檔（使用者 2026-11-16）。Product：單機遊戲 |
| Contract | 存檔檔案是作者的資料契約（AGENTS §8），Engine 不規定。Engine 的契約只有：新 World、作者的讀檔 schedule 這條路上，各機制的行為 |
| Architecture | 現有的 Kernel 機制夠不夠走這條路；不夠的地方（寫者的數法改成「建立時的值不計」）由邊界卡那一批做 |

### 1.4 層的區別

Engine 這一側要確保的是 Kernel 的行為（配發器、丟棄規則、寫者的數法、hook），每個有存檔的產品都會照它寫讀檔，所以照基盤處理。存檔格式與讀檔流程是產品的（應用），這一批不設計。

### 1.5 予見的根據

| 根據 | 推出的要求 |
|---|---|
| 名指的 Feature：玩家存檔（使用者 2026-11-16） | 新 World 裡，作者可以用自己存的編號放回實體，也可以重新配號；放回的值是建立時的值，不算寫者 |
| 參考實作（偵察報告） | Bevy 讀檔一律重新配號；GYO 的預設路是新 World，兩種都做得到 |
| 規模與預算 | 沒有給；存什麼由作者定。大存檔要分批時，作者在自己的 schedule 裡分幾幀放回（Bevy 的 `SpawnScene` 也是每幀流程裡的一張） |

---

## 2. 前提表

修訂 1 第 2 節的表仍適用；這裡列修訂 3 用到的。

| 前提 | 誰定的 | 掛在它上面的 |
|---|---|---|
| 預設支持的路＝新 World＋作者的讀檔 schedule；存什麼、格式、流程由作者 | 使用者 2026-11-16 | 全部 |
| 開局、Loading、讀檔是遊戲流程的名字；Loading 有沒有由作者決定（「機制是建世界的階段」那半句已被 2026-11-17 的 `init-values-like-bevy` 取代：不另設階段） | 使用者 2026-11-16、11-17 | 5.2 |
| 現在的機制：Startup 只跑一次、比任何 schedule 都先（K0）、寫者不計（`Registry.cpp:1413-1424`） | 使用者 2026-11-11（寫者不計）；2026-11-15（K0） | 5.2：讀檔現在就能寫在 Startup |
| 「Startup 只放不依內容的東西，世界內容經 Loading」（runtime-composition DI 3）；理由「現在物件少就算 Startup，將來東西多就撐不住」；Feature：讀取畫面與分批載入 | 使用者 2026-11-11 | 條文要改（第 9 節）；gyo2 沒有實作它的啟動檢查（`git grep` 在 `engine/kernel` 只找到 `Registry.cpp:916` 的 startup 旗標與 `:1421` 的寫者排除） |
| `Domain::Loading` 是 Kernel 的列舉值，帶一格 tick（`Declarations.hpp:61`、`World.hpp:426`） | 2026-11-11 評估時記下 | 第 9 節 |
| 配發器是產品登記的 Resource，策略由產品定（DI-KS 4）；範圍由組裝在 World 建好後用 `SetResource` 給 | 使用者 2026-11-11；現況（`SkeletonTests.cpp:173`） | 5.1 |
| `SpawnWithId` 的編號落在配發器範圍就丟（F-13；`Owns` 判 `first <= id <= last`） | 使用者 2026-11-15 核准 | 5.1 |
| 世代切換＝重建 World | 2026-11-13 核准（DI-KS 14） | 遊戲中讀檔＝重建 World |
| 狀態機：第一次套用先進初始值；快照蓋值時不自己跑進入離開、由遊戲決定 | 使用者 2026-11-15、11-16 | 讀檔後回到哪個狀態是作者的流程 |

---

## 3. 現在的程式在預設的路上會怎樣（讀程式確認；修訂 1 的檢查核對過）

| 做法 | 結果 | 出處 |
|---|---|---|
| 新 World，組裝在第一次 Run 前把配發器的範圍訂成「從存檔記的下一個號開始」；讀檔 System 用 `SpawnWithId` 照存檔編號放回 | 不被丟；之後的生成拿範圍內的號，不撞存檔的號 | `Storage.hpp:365`、`World.cpp:642` |
| 新 World，配發器的範圍和存檔時相同，照存檔編號 `SpawnWithId` | 整批以 `IdInAllotterRange` 被丟 | `World.cpp:642` |
| 讀檔 System 用 `Spawn` 重新配號 | 做得到；`Spawn::New` 記錄當下回傳編號，作者自己記對照表、改寫參照；這個 System 成為配發器的寫者 | `Params.hpp:541-555`、`Registry.cpp:1021-1024` |
| 讀檔 System 掛在 Startup，bundle 直接放 `Position`（`Move` 也寫 `Position`） | 不報 D-E2（Startup 的寫者不計） | `Registry.cpp:1413-1424` |
| 同上，但掛在一張普通的 schedule（例如名叫 Loading 的） | 啟動錯誤 D-E2。照資產生成敵人也一樣，不是讀檔才有 | 同上 |
| hook | 經同步點的生成照常跑 OnAdd，補出的格子也跑；記號是同步點的 | `World.cpp:752-762`、`Storage.hpp:286` |

---

## 4. 讀檔在哪個 World 裡（已定）

使用者 2026-11-16：預設支持的路是一律在新的 World 裡讀。後果寫進說明頁：
- 遊戲中讀檔＝丟掉現在的 World、建新的（DI-KS 14 的世代切換）；現在一個 Product 一個 World，等於重建 Product。要不要重建主選單與相機、讀檔時顯示什麼，是作者與 runner 的事。
- 兩種讀檔結果相同：沒有上一關留下的東西。

---

## 5. 各件

### 5.1 編號（已定：兩種寫法由作者選）

作者可以照存檔編號放回（存檔記「下一個號」，組裝在第一次 Run 前把配發器範圍訂在它之後，用 `SpawnWithId`），也可以重新配號（`Spawn`＋作者自己的對照表與參照改寫）。Engine 保證兩件事，寫進說明頁並用測試守住：
- 組裝可以在第一次 Run 前設配發器的範圍；
- 範圍外的 `SpawnWithId` 照常生成。

### 5.2 讀檔放回的值算不算寫者（**已由邊界卡的決定回答**）

**決定**（使用者 2026-11-17，邊界卡，`v8-arch-gyo2-init-values-like-bevy`；原話「這個可以參照Bevy。」）：初始值在建立實體的當下給（`Spawn`／`SpawnWithId` 帶的值、`InitialValue`）；生成世界就是一般流程裡的 schedule，不另設「寫入不算」的階段；伺服器權威只數修改既有值的寫者。「對已存在的實體插入元件」算建立還是修改，由權威 Module 那一份定。

**對讀檔的後果**：
- 讀檔放回的值就是「建立時的值」：讀檔的 System 用 `SpawnWithId` 或 `Spawn` 帶完整的 bundle 放回敵人的 `Position`，不算 `Position` 的寫者。不需要漏斗、不需要例外、不需要特別的階段。
- 讀檔的 System 掛在作者自己的 schedule（叫「讀檔」「Loading」或別的，作者決定），就是一般流程裡的一張。修訂 3、4 的「建世界的階段」整件拿掉（Loading 那張卡已因前提改變而歸檔；從它回收的東西在 HANDOFF 分支的 `recovered-from-stale-cards.zh-Hant.md`，這裡不帶它的框架）。
- **依賴權威 Module 那一份的部分**：若作者先生成一個只帶標記的實體、之後再「插入」存檔的元件（例：重新配號時先生成拿號、再插入改寫過參照的元件），這種插入算建立還是修改，由權威 Module 定；在那之前，宣告不保證這種寫法不算寫者。照存檔編號、一次帶完整 bundle 生成的寫法不受影響。
- **Resource**（金幣、任務進度）：讀檔時第一次放進 World 的 Resource 是建立；若 World 裡已經有（例如開局放了預設值）再覆寫，算不算修改同樣由權威 Module 那一份定。讀檔的寫法照「新 World 裡由讀檔第一次放進去」寫，不受影響。

**別家怎麼做**（邊界卡的偵察，主 checkout `build/evidence/gyo2-kernel-app-boundary/init-values-other-engines.md`、`cross-comparison.md`）：四家（Bevy、flecs、Unity Entities、Unreal Mass）都沒有寫者規則，初始值一律在生成或插入的當下由範本、預設值或整批搬移給；Bevy 讀檔與放場景在每幀的 `SpawnScene` schedule 做（`crates/bevy_scene/src/lib.rs:98-104`）；Unity 的串流載入在另一個 World 建好再整批搬進主 World（`SceneSectionStreamingSystem.cs:167、227、343`）。

**Kernel 現況**：寫者的數法（`Registry.cpp:1413-1424`）現在仍把生成的 bundle 算寫者、只排除 Startup。照上面的決定改成「建立時的值不計」是邊界卡那一批的事（J-15 改寫）；讀檔這一批的實作要等它，或在那之前用 Startup 跑測試。

（修訂 2～5 在這一件上的選項與推論——漏斗、讀檔專用的生成、Loading 的生成不計、建世界的階段——都不再需要判，保留在 git 歷史。漏斗「兩個請求補同一個目標時沒有訊息」是漏斗自己的問題，已記給主會話。）

### 5.3 hook

**遊戲**：讀檔把關卡放回來時，有三個 OnAdd hook 會碰到：寶箱出現時播「寶箱出現」的音效；碰撞體出現時加進物理的空間索引（不加的話玩家會穿牆）；敵人出現時生成血條。

（修訂 3 的表把「讀檔時不觸發」的代價寫成「和 Engine 不認得存檔衝突」，是錯的：使用者更正，不認得的只是格式。）

**別家怎麼做**（照 `v8-arch-gyo2-engine-not-rigid`，提問附別家的設計）：

| 誰 | 讀檔／放回時 hook 跑不跑 | 出處 |
|---|---|---|
| Bevy（讀檔 `DynamicWorld`） | 照常跑 hook 與 observer；**只刻意跳過「關係」那一類 hook**（寫入時固定傳 `RelationshipHookMode::Skip`）。hook 在讀檔中再生成實體曾讓對照表 panic（#14300，PR #15405 修），留了回歸測試。沒有「讀檔中」的旗標；全部放完後發一個 `WorldInstanceReady` 事件，作者可以接 | 本機原始碼 `6a911653f`：`dynamic_world.rs:154、198、394-437`，`world_asset.rs:197`，`relationship/mod.rs:403-410`，`world_asset_spawner.rs:520-540`（偵察報告 1.2）。跳過關係 hook 的理由原始碼註解沒有讀到；推論：存檔裡已記了關係的兩端，再跑 hook 會重複登記 |
| bevy_replicon（Client 收 Server 的實體） | 照常跑，而且刻意「同一個實體的元件全部到齊才一次插入」，避免 observer 看到一半的實體；文件建議用 observer 或 required components 在 Client 補上不複製的元件 | 本機原始碼 `d80c849`：`deferred_entity.rs:10-16`，`lib.rs:190-222`（偵察報告 3.2、3.3） |
| lightyear（回滾時還原元件） | 經一般的延後指令插入與移除，hook 與 observer 會跑；沒有讀到抑制的機制 | `rollback.rs:896、1127-1137`（推論，偵察報告第 2 節） |
| flecs | 核心有 `on_add`、`on_set`、`on_remove`、`on_replace`、`on_validate` 的 hook；從範本（prefab）生出實例時跑不跑，**沒有讀到** | `include/flecs.h:1015-1038`（邊界卡的偵察） |
| Unity Entities | 串流載入在另一個 World 建好，再整批搬進主 World；元件加入時的 hook，**沒有讀到原始碼** | `SceneSectionStreamingSystem.cs:167、227、343`（邊界卡的偵察） |
| Unreal Mass | fragment 加入時 observer 會跑、用來初始化 | 邊界卡先前的偵察，**沒有讀原始碼** |

讀到的幾家都沒有「讀檔時整批不跑 hook」的開關；Bevy 只對一種有明確理由的 hook（關係）例外。

| 選項 | 得到什麼 | 代價 | 系譜 |
|---|---|---|---|
| 照常觸發 | 空間索引這類必須在出現時登記的自然成立；Kernel 不變；不禁止作者用別的方式分辨讀檔（作者自己的狀態、讀完時自己發的事件） | 讀檔時要不要播音效，作者自己分辨；hook 生成的衍生實體若也存了，讀檔後有兩份（作者的格式決定） | Bevy、bevy_replicon、lightyear |
| 照常觸發，Engine 另外提供一個「讀完了」的訊號 | 作者有固定的時機接（像 Bevy 的 `WorldInstanceReady`） | Engine 要知道「讀檔」這件事何時完成——但讀檔的流程是作者的（使用者 2026-11-16），讀檔的 System 自己發一個事件就做得到 | Bevy |
| 建世界時一律不觸發 | 沒有副作用 | 空間索引這類 hook 不跑，玩家會穿牆；Kernel 要在同步點認得「建世界」——這個階段已被 `init-values-like-bevy` 取消，沒有東西可以掛 | 舊規格 Restore（S12，給回滾用） |
| 每個 hook 登記時選「讀檔時跑不跑」 | 選擇集中在 hook 的宣告 | 同上，Kernel 沒有「讀檔中」可以判斷；是 Engine 替作者訂規則（`engine-not-rigid`） | 沒有參考實作 |
| hook 看得到這筆變更是誰記的 | 中性的機制；hook 能分辨「讀檔 System 生成的」與「玩法生成的」 | hook 的參數（公開介面）變；讀到的別家都沒有這個 | 沒有參考實作 |

**我的建議**：照常觸發，說明頁寫一句「要分辨讀檔，讀檔的 System 自己記狀態或發事件」。理由：玩家存檔要回到存檔時的樣子，必須在出現時登記的 hook 不跑就回不去；讀到的幾家都是照常觸發（`engine-not-rigid`：很多開源都這樣做就跟著做）；不另訂規則，作者想怎麼分辨都可以。「hook 看得到記錄者」寫進 Replacement Criteria：有作者在兩處以上重複寫「分辨讀檔」時再提。

### 5.4 stamp（確認）

放回的格子取套用那個同步點的 stamp，`Added<T>` 成立。新 World 裡一切都是新的，保留存檔時的 stamp 沒有意義（world §2）。**沒有可選的**。

### 5.5 tick（確認）

新 World 的 tick 從 0 開始。遊戲要的「遊戲內時間」由作者放在自己存的 Resource 裡。Engine 不提供設 tick 的入口（「tick 只有一個來源」，kernel 頁 `:46`；DI-KS 12 正由邊界卡改寫，這裡不再引用）。**沒有並列選項**。

### 5.6 診斷紀錄（確認）

新 World 的 `RejectedSpawn` 總數從 0 開始；讀檔時被丟的生成（例：存檔有兩個實體用同一個編號、或編號落在配發器範圍）照常記明細；`rollbackActive` 是 false。**沒有並列選項**。

### 5.7 「預設支持」具體包含什麼

| 選項 | 包含 | 得到什麼 | 代價 |
|---|---|---|---|
| 說明頁的寫法＋測試樣本 | 說明頁新增「讀檔」一節（遊戲例子、新 World、作者的讀檔 schedule、編號兩種寫法、hook、tick）；`RestoreEntryTests` 用測試樣本把這條路跑一次，當守衛 | 作者有範本；路被測試守著；Kernel 不變 | 依賴的 Kernel 行為（範圍外的 `SpawnWithId`、Run 前可設配發器）已在 DI-KS 4、9；「建立時的值不計」由邊界卡改寫 DI-KS 6，不另加條文 |
| 上面＋DI-KS 4 加一句 | 「組裝可以在第一次 Run 前設配發器的範圍；範圍外的 `SpawnWithId` 照常生成，是讀檔的預設寫法所依賴的」 | 有人改配發器或丟棄規則時，會被當成違反條文 | 條文變更要你同意 |
| 上面＋runner／組裝的「重建 World」零件 | 一個重建 Product 並在 Run 前交給作者設定的零件 | 作者不用自己寫重建 | 屬於 T3／T5（runner、多個 World），還沒有宣告；這一批只能記成給它們的要求 |

**我的建議**：說明頁＋測試樣本＋DI-KS 4 加一句；「重建 World 的零件」記成給 T3／T5 的要求。

---

## 6. （修訂 1 第 6 節「在現在的 World 裡讀」的件）

第 4 節選了新 World，整節拿掉。內容留在修訂 1（`ea62f0c4`）供之後查。

---

## 7. 和回滾的關係

**前提已變**（修訂 7，主會話轉知；使用者在邊界卡定的，HANDOFF `v8-arch-gyo2-rollback-module-strategy`、`v8-arch-gyo2-rollback-module-strategy-interface-impl`）：Kernel 只給原語（讀寫整欄、設 stamp、整批複製或替換之類，清單由回滾 Module 那一份與統合定）；回滾是獨立的 Module（`RollbackStrategy` interface 加官方實作，作者可以從頭實作或繼承改裝）；DI-KS 12 改寫成「Kernel 提供原語」。所以修訂 1～6 這一節拿舊的 DI-KS 12 與舊規格 H.5 的 Restore 做的比較都不再適用，這一節不再用它們推。

讀檔這一批只寫「和回滾不衝突」的要求；具體的原語、回滾怎麼退回與重跑，**由回滾 Module 那一份與統合定**。

**給回滾 Module 那一份的要求**：
- 讀檔一律在新的 World（使用者 2026-11-16）；回滾在同一個 World 裡退回。所以回滾保存的狀態不跨 World：讀檔之前存下的回滾狀態，不能還原到讀檔之後的 World。
- 讀檔放回的值是建立時的值（`init-values-like-bevy`）、hook 照常（5.3 若照建議定）。回滾的官方實作若要用讀檔的寫法、或讀檔要用回滾的原語，先對齊兩邊對 hook 與 stamp 的行為。
- 重看 LIM-7（重放的生成撞上帶 (b) 區元件的活實體）——前提是區規則與 F-13 仍在；它們是否移出 Kernel 由邊界卡定。
- 定 `RejectedSpawn` 的 `rollbackActive` 由誰設（Kernel 的原語還是回滾 Module），以及總數要不要扣除重放的部分。

## 8. 來由的三件

1. **回滾重放撞上只帶 (b) 區元件的活實體（LIM-7）**：這一批不回答，留給回滾那一批（範圍）。要求在第 7 節。
2. **`rollbackActive` 固定 false、重放時重複計**：讀檔的部分——讀檔不是重放，false 是對的（5.6）。重放的部分留給回滾那一批，要求在第 7 節。
3. **單機用 `SpawnWithId` 放回會整批被丟**：這一批回答（5.1，使用者已定）。讀檔在新 World；作者想照存檔編號放回時，組裝在第一次 Run 前把配發器的範圍訂在存檔記的下一個號之後，`SpawnWithId` 就不被丟；不想記編號就用 `Spawn` 重新配號。F-13、DI-KS 9、配發器都不改。K0 第 9 節那條（「外層刪除、裡面用同一個編號生成」）在新 World 的建世界階段不會發生；要不要在 K0 第 9 節註明，由你判。

---

## 9. 和其他批次的關係

| 批次 | 關係 |
|---|---|
| **邊界卡**（gyo2-kernel-app-boundary） | 寫者的數法改成「建立時的值不計」（J-15 改寫；`v8-arch-gyo2-init-values-like-bevy`）；「對已存在的實體插入元件」算建立還是修改由權威 Module 那一份定。讀檔這一批的實作要等數法改完（在那之前案例 4、5 會報 D-E2）。Loading 卡已歸檔 |
| K0（PR #239） | 讀檔掛在作者的一張普通 schedule，不巢狀；和 Startup 無關。不用等 |
| 狀態機 | 讀檔後回到哪個狀態是作者的流程；不用等 |
| T3、T5 | 重建 World 的驅動、組裝在 Run 前設配發器與放存檔資料的時機（5.7 的要求） |
| 回滾 | 第 7 節 |

**建議的批次切法**：
1. `v8-arch-gyo2-restore-entry` 的實作（宣告核准後由你另外說）：說明頁「讀檔」一節、`RestoreEntryTests`、DI-KS 4 加一句（若同意）。Kernel 的程式不變。
2. 寫者的數法（「建立時的值不計」）：邊界卡那一批，讀檔的實作排在它之後。

---

## 10. 既有條文與守衛測試（照建議的組合）

- **DI-KS 4**：加一句（5.7，要你同意）。其餘 Kernel 條文這一批不改。
- **runtime-composition DI 3**（「Startup 只放不依內容的東西，世界內容經 Loading」）：讀檔不掛 Startup、不依賴 Loading，這一批不碰；DI 3 本身要不要改由主會話處理（Loading 卡已歸檔）。
- **DI-KS 14**：不改；說明頁寫明「遊戲中讀檔＝重建 World」。
- **`rebuild-entity-order` 第 5 節**「讀檔時用存檔裡的 id」：加一句「讀檔在新 World；照存檔編號放回時，配發器的範圍從存檔記的下一個號開始；作者也可以重新配號」。那是架構師的分析稿，改要主會話同意；它掛在 E2（待使用者確認）上，E2 不在這一批判。
- **守衛測試**：既有的判定不變；新增 `RestoreEntryTests`。指紋不變（Kernel 的程式不變）。

---

## 11. Architecture Delta（照建議的組合）

1. Feature：玩家存檔（使用者 2026-11-16）。
2. 問題：沒有寫明讀檔的路；照字面（配發器範圍不變、`SpawnWithId`、掛在一般的 schedule）會整批被丟或撞寫者的檢查。
3. 邊界：Kernel 的公開介面不變；契約新增（說明頁＋DI-KS 4 一句）：讀檔＝新 World＋作者的讀檔 schedule（放回的值是建立時的值）；Run 前可設配發器的範圍；範圍外的 `SpawnWithId` 照常生成。
4. 影響：說明頁、kernel 頁 DI-KS 4、測試；將來有存檔的產品；runner／組裝（T3、T5 的要求）；邊界卡（寫者的數法）。
5. 依賴方向：不變。
6. Ownership：存檔格式、存什麼、讀檔流程歸作者；Engine 保護機制。
7. 為什麼不能更小：不寫明的話，作者照 `rebuild-entity-order` 第 5 節的字面在原本的範圍裡用 `SpawnWithId`（整批被丟），或掛在一般的 schedule（撞寫者）；這一批只寫明現有機制的用法並用測試守住。

---

## 12. Replacement Criteria（草案，寫在說明頁的讀檔一節；要你同意）

- 有產品必須在不重建 World 的情形下讀檔（例：重建的時間超過產品的預算，量到）→ 重看第 4 節（修訂 1 第 6 節的件）。
- 作者分辨「讀檔生成的」與「玩法生成的」的寫法，在兩個以上的 hook 重複 → 重看 5.3（hook 看得到記錄者）。

（「兩個」沒有根據，是起草者給的。）

---

## 13. 判定（先宣告，結果出來之後不放寬；照建議的組合）

### 判定 1：行為（新測試 `tests/common/kernel/RestoreEntryTests.cpp`；案例名實作前換成實際的 `TEST_CASE` 名）

讀檔的 System 掛在一張普通的 schedule（測試裡叫 `Load`，作者的名字）；存檔資料由組裝在第一次 Run 前用 `SetResource` 放進一個暫存的 Resource。

**修訂 6**：案例 4、5 與判定 2 的寫者那一個突變，依賴邊界卡把寫者的數法改成「建立時的值不計」（J-15 改寫）；在那之前案例 4、5 會報 D-E2，所以讀檔的實作排在那一批之後。案例 1～3（編號與配發器、F-13）、6～8 不依賴寫者規則；若邊界卡把 F-13 的丟棄規則或配發器也移出 Kernel，案例 1～3 跟著重看。

1. **照存檔編號放回**：存檔有玩家（12）與 2 隻怪物（57、58，57 的 `ChaseTarget` 是 12），記的下一個號 900；新 World 在第一次 Run 前設配發器 `[900, 上限]`；讀檔 System 用 `SpawnWithId` 放回，bundle 直接帶 `Position`；之後一張每幀的 schedule 生成一個選單實體。讀檔後編號是 12、57、58；`ChaseTarget` 仍是 12；選單實體的編號 ≥ 900；沒有 `RejectedSpawn`。
2. **重新配號**：同一份存檔，讀檔 System 用 `Spawn`；讀檔後三個實體的編號 ≥ 900、依存檔編號的大小順序；`ChaseTarget` 由讀檔 System 改寫後指向玩家的新編號。
3. **範圍沒設好**：配發器範圍是 `[1, 上限]`，照存檔編號 `SpawnWithId`，每一筆以 `IdInAllotterRange` 被丟（記錄來由第 3 件）。
4. **寫者**：`Position` 的寫者是固定步裡的 `Move`，讀檔 System（`Load`）用帶 `Position` 的 bundle 生成；Registry 完成時沒有 D-E2（建立時的值不算寫者）。
5. **Resource**：讀檔 System 在 `Load` 第一次把金幣放進 World（之前沒有）；固定步裡的 `Wallet` 也寫金幣；沒有 D-E2；讀檔後是存檔值。（覆寫已存在的 Resource 算不算修改，由權威 Module 那一份定，這個案例不用那種寫法。）
6. **hook**：每個放回的格子 OnAdd 各一次；`Added<Position>` 在讀檔後第一個讀者成立。
7. **新 World**：第一個 World 跑過 Fixed 的 tick；第二個 World 讀檔，Fixed 的 tick 是 0、`RejectedSpawns()` 是 0、沒有第一個 World 的實體。
8. **讀兩次相同**：同一份存檔在兩個新 World 讀，`Summary()` 相同。

案例 1～8 守的是「預設的路」這份契約；用到的 Kernel 行為多半已有別的守衛（DI-KS 4、9 的 J-8、J-55；DI-KS 6 的 J-15）。

### 判定 2：檢出力（AGENTS §14.2）

Kernel 的程式不變，突變的對象是預設的路依賴的既有程式行；每一個寫明預期失敗的案例。

| 突變 | 位置 | 預期失敗 |
|---|---|---|
| `Owns` 改成永遠真 | `Storage.hpp:365` | 案例 1（`IdInAllotterRange`） |
| `Owns` 改成永遠假 | 同上 | 案例 3 |
| 第 4 步不檢查「落在範圍」 | `World.cpp:642` | 案例 3 |
| `RangeAllotter::Of` 忽略 `first`（從 0 配） | `Storage.hpp:350-352` | 案例 1（選單實體的編號） |
| `RangeAllotter::Allot` 不前進 | `Storage.hpp:354-362` | 案例 2（同一個編號生成兩次被丟） |
| 寫者的數法把建立時的值（生成的 bundle、第一次放進的 Resource）算進去 | 邊界卡改寫後的數法 | 案例 4、5 |
| 補出或生成的格子不跑 OnAdd | `World.cpp:758-762` | 案例 6 |

突變清單在實作前交給和實作者不同模型的檢查者做一次對抗檢查。

### 判定 3：沒有回歸

`core` preset 的 kernel 測試全過；L1 三平台全過。

---

## 14. 範圍的限界

- 只寫預設的路與它依賴的 Kernel 行為；存檔格式、存什麼、IO、讀檔的流程、重建 World 的驅動都不在這一批。
- 寫者的數法：讀檔的實作依賴邊界卡把它改成「建立時的值不計」；「對已存在的實體插入元件」「覆寫已存在的 Resource」算建立還是修改由權威 Module 那一份定，讀檔的預設寫法不用這兩種。
- 回滾：原語與做法由回滾 Module 那一份與統合定；這一批只寫和回滾不衝突的要求（第 7 節），LIM-7 與重放時的明細不判。
- 「在現在的 World 裡讀」不是預設的路；作者自己做時會碰到的事（修訂 1 第 6 節）這一批不保證，說明頁提一句並指向修訂 1。
- 漏斗「兩個請求補同一個目標時沒有訊息」不在這一批，記給主會話。
- (c) 區的產品讀檔沒有分析。
- 參考實作的事實來自偵察報告（讀本機原始碼）；bevy_ggrs 本機沒有原始碼，沒有讀到。
- 沒有建置、沒有試做；案例 1、2、4 的寫法實作前用樣本跑一次。
- 修訂 2、3 沒有再送換模型檢查。

---

## 15. 誰做、上段或下段、模型／檔位、時間

**上段**（`agents/navigator.md`）：碰到 Design Invariants（DI-KS 4 加句）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 起草者（本卡） | Opus／high | — |
| 宣告的對抗檢查 | Fable／high（唯讀） | 修訂 0 已檢查一次；修訂 3 要不要再檢查，由你定 |
| 實作者（核准後另開） | Opus／high | 上段 |
| 審查者 | Fable（主會話） | 和實作者不同模型 |
| 突變清單的對抗檢查 | Fable／high | 和實作者不同模型 |

時間（agent 的時間）：本卡到核准約 6 小時（已花約 5.5 小時）。實作照建議的組合約 3 小時（8 個案例 1.5 小時、突變 0.5 小時、說明頁與條文 0.5 小時、L1 0.5 小時）。
