# 事前宣告：玩家存檔的讀檔——Engine 預設支持的路（`v8-arch-gyo2-restore-entry`）

> **核准**（使用者，2026-11-17，主會話；HANDOFF `v8-arch-gyo2-five-declarations-approved`）：原話「全部核准」——和統合修訂 38、權威、回滾、游標的最終版一起核准。核准的版本是本宣告**修訂 11（ec31e863）**，含第 15 節確認清單的那一件：讀檔放在編排層的「生成世界」；使用者沒有更正卡片對 11-16「正常這些邏輯都應該寫在Startup裡的」的讀法（指當時還沒拆開的「開局」，讀檔落在生成世界那一件）。比過的選項：全部核准／挑出有意見的那一份或那一處。理由：使用者沒有另外說。
> **這個核准不等於 AGENTS §14.4 的判定變更核准**：第 13 節的判定在實作批照 §14.1、§14.2 宣告；既有守衛的退役或搬遷，在各實作批逐案核准。
> **核准後的註記**（不改核准的內容）：同時核准的統合修訂 38 把「配發器含範圍的下界」與「Resource 的增刪」補成 Kernel 的機制（HANDOFF 同一列）。前者就是 5.1 的寫法；後者讓 5.8 說的「System 放不進不存在的 Resource」到實作時可能不再成立——5.8 的預設寫法（編排層先放、`ResMut` 覆寫）照樣可用，實作批若 Kernel 已有放進 Resource 的參數，可以在案例 5 加一案，照 §14 在實作批的判定宣告。

**修訂紀錄**
- 修訂 0（3dc9b464）起草；修訂 1（ea62f0c4）照第一次換模型檢查（Fable／high，blocker 3、should 17、nit 7）重寫。
- 修訂 2～8（2026-11-16～17，e6466c2d～ed742598）：使用者在本卡與邊界卡陸續定了新 World、編號、「不認得存檔只指格式」、Kernel 是 ECS、初始值在建立當下給、權威不數寫者、回滾是獨立 Module；每次照著縮小與改寫。
- 修訂 9（4ad66708、47123a6c）：工作方式改成卡片照 L17 自己判（使用者 2026-11-17：「什麼都要我來定，乾脆代碼我也自己寫好了。名字你有命名規範。例子你有源碼。」），判了 5.3～5.7。
- **修訂 10（2026-11-17）**：照第二次換模型檢查（Fable／high，blocker 3、should 11、nit 8，全部採用；報告 `build/evidence/gyo2-restore-entry/declaration-check-fable-r9.md`）整份重寫。最大的三處：判定改寫成「實作時的 Engine」（批次表第五段，Kernel 減法、編排層、時間 Module、權威與回滾都已做完）；讀檔放在**編排層的「生成世界」**（使用者 `two-layers-like-bevy`），修訂 6～9 的「建世界的階段整件拿掉」是錯的（拿掉的只有 Kernel 那一層）；5.5 照 `clock-out-of-kernel` 重判。逐條對照在第 16 節。

- **修訂 11（2026-11-17）**：照窄檢查（Fable／high；報告 `declaration-check-fable-r10-narrow.md`；舊 blocker：5.5 與 `two-layers-like-bevy` 已改好、判定剩一處；新的 blocker 2、should 5、nit 6，全部採用）改。最大的兩處：Resource 放回改成「編排層先放、讀檔的 System 用 `ResMut` 覆寫」（System 沒有參數能放進不存在的 Resource）；補上使用者的 `game-init-list`（遊戲初始化只在行程啟動時跑一次、跑在第一個 World），修正 1.1、第 2、4、15 節。5.1 改成移配發器範圍的下界。對照在第 17 節。
- **修訂 12（2026-11-22，草案，等動工前核准）**：實作卡 `gyo2-restore-impl` 開工前照現在的 `origin/gyo2`（eacab4d7，第 4 批、生成世界 #272、交換點 #270 已合併）逐條對照。大部分已被骨架、生成世界兩批做掉；剩下的範圍縮小，判定改成「已有帶標記的案例就沿用，沒有的才寫」，並併入存檔版本 Module（使用者 2026-11-22「合併進去。」）。全文在第 18、19 節；第 2、4、5.3、5.7、5.8、13 節被第 18 節取代或補充的地方以第 18 節為準，原文保留不改（核准的是修訂 11）。
- Owner：架構（GYO2 主會話開的任務卡）。這一批只做到宣告核准為止，**不實作**。實作是批次表第五段「讀檔的實作」（`rebuild-batch-plan.zh-Hant.md`）。
- 讀的版本：程式 `origin/gyo2` 15ba81ef（K0、測試舊帳已合併）；批次表與 HANDOFF 在 `claude/gyo2-time-design` f530a1a4；統合修訂 33（1b3f6886，DI-KS 15 第 (7) 項收了「Ready 時設配發器」）；回滾份 `claude/gyo2-rollback-module-8bac02` 9418fcf1（第 7.4 節）。
- 證據：開發機（Intel Mac，使用者的本機）主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-restore-entry/`（git 不追蹤）：`scout-old-design.md`、`scout-mature-practice.md`、`premises-and-scope.md`、`declaration-check-fable-r0.md`、`declaration-check-fable-r9.md`。

---

## 0. 使用者已定的（本宣告必須和它一致）

| 代號 | 內容 | 在哪裡定的 |
|---|---|---|
| `v8-arch-gyo2-restore-entry-scope-player-save` | 範圍＝讀檔的入口；Feature＝玩家存檔。原話「選第二項，Feature是「玩家存檔」。」 | 本卡 |
| `v8-arch-gyo2-restore-entry-load-in-game-same-mechanism` | 遊戲中讀檔和關掉再讀是同一個機制。原話「算在裡面，是一個機制。」 | 本卡 |
| `v8-arch-gyo2-restore-entry-new-world-default` | 預設支持的路＝一律在新的 World 讀；存什麼、存檔格式、讀檔流程由作者決定；Engine 保護機制、不規定存檔格式（原話見 HANDOFF） | 本卡 |
| `v8-arch-gyo2-save-unaware-means-format-only` | 「Engine 不認得存檔」只指格式。原話「首先當時說的不認識存檔是說格式。」 | 本卡 |
| `v8-arch-gyo2-restore-entry-ids-author-choice` | 編號兩種寫法由作者選；Engine 保證「建 World 後、第一次 Run 前可以設配發器的範圍」「範圍外的 `SpawnWithId` 照常生成」。原話「照建議」 | 本卡 |
| `v8-arch-gyo2-build-world-phase-is-mechanism`（HANDOFF 只有這一個代號） | 開局、Loading、讀檔是遊戲流程的名字；Loading 有沒有由作者決定。原話含「讀檔應該是和生成世界是同一級別呀」「正常這些邏輯都應該寫在Startup裡的」（全文見 HANDOFF） | 本卡 |
| `v8-arch-gyo2-game-init-list` | 「開局」切成兩件：**遊戲初始化**——行程啟動時一次、在所有 World 開始每幀之前跑完、跑在第一個 World，是一份可插入的清單；**生成世界**——每個 World 各自、可以多次、可以分批。原話「第一個World，內容有作者自己訂，Engine只提供機制」（全文見 HANDOFF） | 邊界卡的側邊會話 |
| `v8-arch-gyo2-two-layers-like-bevy` | 照 Bevy：**編排層認得遊戲初始化與生成世界的階段、排流程；Kernel 不認得**。原話「用Bevy的方式走，」 | 邊界卡 |
| `v8-arch-gyo2-kernel-is-ecs-no-restrictions`、`-init-values-like-bevy`、`-authority-no-writer-count` | Kernel 是 ECS、不疊加限制；初始值在建立當下給；伺服器權威不數寫者，判定資料的讀寫規則拿掉（統合 15.3） | 邊界卡 |
| `v8-arch-gyo2-clock-out-of-kernel` | 時鐘與 tick 移到時間的 Module（作者可選）；「tick 只有一個來源、System 不得寫」歸伺服器權威策略 | 邊界卡 |
| `v8-arch-gyo2-rollback-module-strategy`、`-strategy-interface-impl` | 回滾是獨立的 Module，Kernel 只給原語 | 邊界卡 |
| `v8-arch-gyo2-layer-duty-lists`（L17）、`v8-arch-gyo2-engine-not-rigid` | 每層一份職責清單，清單之外由 agent 照同類型多家實作判；某層要多一項職責或判不出哪一層才停下問。不把規則釘死 | 邊界卡 |

標「卡片照 L17 判」的件都不是使用者的話。

---

## 1. 這一批做什麼

### 1.1 用遊戲講

RPG：玩家在第 3 關存檔，身上有 3 瓶藥水、一把鑰匙、金幣 120，任務「救出村長」做到第 2 步，觸發過「拉桿」「Boss 已打倒」。隔天讀檔（或遊戲中讀另一個存檔）：編排層建一個新的 World，進生成世界（遊戲初始化只在行程啟動時跑一次，`game-init-list`）；在生成世界裡，作者的程式照第 3 關的資產生成地圖、怪物、寶箱，作者的讀檔 System 放回物品、任務進度、觸發條件；關卡裡的東西照觸發條件決定自己的樣子。卡牌遊戲存牌組；另一個作者存了每一隻怪物——走同一條路。Engine 不認得任何一種存檔格式。

### 1.2 這一批要確保的

預設的路在**實作時的 Engine** 上走得通，而且行為寫清楚：編號（5.1）、寫者（5.2）、hook（5.3）、stamp（5.4）、時間（5.5）、診斷紀錄（5.6）、預設支持包含什麼（5.7）；和回滾（第 6 節）。

### 1.3 Derivation Order

| 層 | 內容 |
|---|---|
| Feature | 玩家存檔（使用者 2026-11-16）。Product：單機遊戲 |
| Contract | 存檔檔案是作者的資料契約（AGENTS §8），Engine 不規定。Engine 的契約：新 World、編排層的生成世界、作者的讀檔 System 這條路上，各機制的行為 |
| Architecture | Kernel（ECS）與編排層現有／計畫中的機制夠不夠；不夠的寫成給那幾批的要求 |

### 1.4 層的區別

Kernel 這一側（配發器、丟棄規則、hook、stamp）每個有存檔的產品都會照它寫讀檔，照基盤處理；編排層的生成世界由它那一批設計，這裡只寫要求；存檔格式與讀檔流程是產品的。

---

## 2. 實作時的 Engine（前提；批次表第五段之前已完成的）

| 東西 | 那時在哪裡、長什麼樣 | 出處 |
|---|---|---|
| Kernel | ECS：World（實體、元件、Resource、hook、延後的指令）、schedule、執行器。**沒有開局排程、沒有 `Domain`／tick、沒有寫者的數量、沒有區規則** | 批次表第 1～4 段；`kernel-is-ecs-no-restrictions`、`clock-out-of-kernel`、`authority-no-writer-count` |
| 配發器、F-13 的丟棄規則 | 留在 Kernel | 回滾份第 7.4 節（還原時存活集合與配發器一起退回）；統合 |
| 編排層 | 建 World；遊戲初始化清單（行程啟動時一次、跑在第一個 World）；runner（批次表第 2 段）。生成世界（每個 World 各自、可以多次、可以分批）是第 5 段的另一批，和讀檔的實作並行，**不是讀檔實作的前提** | 批次表第 2、5 段；`two-layers-like-bevy`、`game-init-list` |
| 時間 | 時間的 Module；tick 是它的 Resource | 批次表第 3 段；`clock-out-of-kernel` |
| 生命週期 | hook 留著；新增時 hook 在 observer 之前，生命週期觸發 observer | 統合第 15.1 節 |
| 診斷紀錄 | 被丟的生成照常記；`rollbackActive` 拿掉 | 統合修訂 17；回滾份第 7.4 節 |

**這一批的判定寫在這個 Engine 上**（第 13 節）。現在的程式（15ba81ef）上的事實列在第 3 節，只用來說明為什麼要這樣寫。

---

## 3. 現在的程式（15ba81ef）碰到讀檔時會怎樣（讀程式確認）

| 做法 | 結果 | 出處 |
|---|---|---|
| 新 World 在第一次 Run 前把配發器的範圍訂成「從存檔記的下一個號開始」，用 `SpawnWithId` 照存檔編號放回 | 不被丟 | `Storage.hpp:365`、`World.cpp:726` |
| 配發器範圍和存檔時相同，照存檔編號 `SpawnWithId` | 整批以 `IdInAllotterRange` 被丟 | `World.cpp:726` |
| 用 `Spawn` 重新配號 | `Spawn::New` 記錄當下回傳編號 | `Params.hpp:543` |
| 讀檔 System 的 bundle 放 `Position`，固定步的 `Move` 也寫 | 現在報 D-E2（`Registry.cpp:1450` 起的寫者數量）；批次表第 4 段拿掉後不報 | `Registry.cpp:1450` |
| 用 `Spawn` 的讀檔 System 與玩法 System | 現在兩者都寫配發器（預設 Exclusive 的 Resource），報 D-E2；同上，第 4 段後不報 | 說明頁第 7 節第 6 點 |
| 生成與第 5a 步補出的格子 | 跑 OnAdd（第 6 步） | `World.cpp:812`、`:840` |
| System 放進一個還不存在的 Resource | 沒有參數做得到：`ResMut` 碰到不存在的 Resource 是斷言（`Params.hpp:393`）；`ExclusiveWorld` 不增刪 Resource（`Params.hpp:875-876`）；同步點第 7 步沒有任何地方記紀錄（`World.cpp:847-863`） | 見 5.8 |

---

## 4. 讀檔在哪個 World、哪一層

- **新的 World**（使用者已定）：遊戲中讀檔＝編排層丟掉現在的 World、建新的。兩種讀檔結果相同。
- **編排層的「生成世界」**（**卡片照 L17 與 `two-layers-like-bevy` 判，不是使用者的話**）：讀檔是生成世界裡作者的一個（或幾個）System，和「照資產生成關卡」同一級別——這是卡片對使用者 11-16「讀檔應該是和生成世界是同一級別呀」的讀法（使用者當時問的是寫者）。Kernel 不認得生成世界，也不認得讀檔。
  - 系譜：Bevy 讀檔在每幀的 `SpawnScene` schedule（`crates/bevy_world_serialization/src/lib.rs:65-69` 的 `world_instance_spawner`），由 `bevy_app` 排位置；Unity 的 `SerializeUtility.DeserializeWorld` 只收完全空的 World，訊息要作者「create a new empty World」（`Unity.Entities/Serialization/SerializeUtility.cs:1046、1054`，第二次檢查讀的）——「一律新 World」的獨立先例。
  - 為什麼不是「遊戲初始化」清單：使用者的 `game-init-list` 定了遊戲初始化在行程啟動時跑一次、跑在第一個 World；遊戲中讀檔（使用者定為同一個機制）建的新 World 不會再跑它，所以讀檔放不進去。另外生成世界可以分批，大存檔與讀取畫面做得到（使用者 11-11 定 DI 3 的理由）。
  - 使用者 11-16 也說過「正常這些邏輯都應該寫在Startup裡的」。這句和「放生成世界」是否一致，列進第 15 節的確認清單。

---

## 5. 各件

### 5.1 編號（使用者已定：兩種寫法由作者選）

照存檔編號放回（存檔記「下一個號」，建 World 後、第一次 Run 前把配發器範圍的**下界**訂成它，用 `SpawnWithId`），或重新配號（`Spawn`＋作者的對照表與參照改寫）。

Engine 的兩點保證分在兩層：
- **Kernel**：World 在第一次 Run 前（Ready）可以設配發器；範圍外的 `SpawnWithId` 照常生成、範圍內的被丟並留紀錄。Kernel 的測試守（判定 1 案例 1～3）。統合修訂 33 收進 DI-KS 15 第 (7) 項「生成」。**要移的是範圍的下界**：`RangeAllotter::Owns` 判的是 `first <= id`（`detail/Storage.hpp:365`），只把「下一個號」往後推、下界不動的話，存檔的編號仍落在範圍內、照樣整批被丟。DI-KS 15 第 (7) 項的字面是「設定配發器的下一個號」，已請主會話改成「範圍（含下界）」。
- **編排層**：建 World 之後、第一次 Run 之前，給作者（讀檔的程式）一個設配發器範圍的時機。**這是給「編排層的骨架」與「生成世界（重做）」兩批的要求**；Kernel 的測試守不到，那兩批的判定要有案例。

### 5.2 寫者（使用者的決定已回答）

實作時 Kernel 沒有寫者的數量（`authority-no-writer-count`、批次表第 4 段），讀檔不受任何寫者規則管：一次帶完整 bundle 生成、先生成再插入、覆寫已有的 Resource，都不數寫者（Resource 怎麼放回見 5.8）；用 `Spawn` 的讀檔 System 和玩法 System 同時寫配發器也不報錯。不需要漏斗、例外或特別的階段。

（修訂 2～8 在這一件上的選項——漏斗、讀檔專用的生成、Loading 的生成不計、建世界的階段——都不再需要判，保留在 git 歷史。漏斗「兩個請求補同一個目標時沒有訊息」是漏斗自己的問題，已記給主會話。）

### 5.3 hook（卡片照 L17 判，不是使用者的話）

**遊戲**：讀檔把關卡放回來時，寶箱的 OnAdd 播音效；碰撞體的 OnAdd 加進空間索引（不加玩家會穿牆）；敵人的 OnAdd 生成血條。

**別家**（同類型的 ECS 與它們的讀檔或還原）：

| 誰 | 讀檔或還原時 hook | 出處 |
|---|---|---|
| Bevy（`DynamicWorld` 讀檔） | 照常；只刻意跳過「關係」的 hook。讀檔中 hook 再生成實體曾 panic（#14300，PR #15405 修），留了回歸測試。沒有「讀檔中」的旗標 | 本機 `6a911653f`：`dynamic_world.rs:154、198、394-437`、`world_asset.rs:197`、`relationship/mod.rs:403-410` |
| bevy_replicon、lightyear | 建在 `bevy_ecs` 上，繼承照常觸發（不是獨立的設計決定）；replicon 刻意讓同一實體的元件到齊才一次插入 | `deferred_entity.rs:10-16`；lightyear `rollback.rs:896`（還原，推論） |
| flecs | 世界的 JSON 還原走一般的 `ecs_add_id`、`ecs_ensure_id` 加 `ecs_modified_id`，沒有抑制 hook 的參數 | `flecs` 09790b5f7：`include/flecs/addons/json.h:88、103`、`src/addons/json/deserialize.c:374、383、387`（第二次檢查讀的） |
| Unity Entities | 沒有元件 hook；讀檔只收空的 World | `SerializeUtility.cs:1046、1054`（第二次檢查讀的） |

**判定**：照常觸發。讀到的有 hook 的兩個獨立實作（Bevy、flecs）都走一般的寫入路、沒有抑制開關；照常觸發也不讓 Kernel 多一項職責（L17）。說明頁寫：要分辨讀檔，讀檔的 System 自己記狀態或發訊息；hook 生成的衍生實體（血條）若也存進存檔，讀檔後會有兩份，由作者的格式決定。Bevy 的「關係」例外：GYO 現在沒有關係的概念，不適用；將來加關係時照 Bevy 再看。

### 5.4 stamp（卡片照 L17 判）

放回的格子取套用那個同步點的 stamp，`Added<T>`、`Changed<T>` 對讀檔之後才跑的讀者成立。系譜：Bevy 讀檔時 added／changed 標成寫入當下（偵察報告 1.2）；Unity 寫入時把版本推到目前（統合 15.4）——多家一致。

### 5.5 時間（卡片照 L17 與 `clock-out-of-kernel` 判；修訂 9 的判定作廢）

修訂 9 寫「Engine 不提供設 tick 的入口（tick 只有一個來源）」「Bevy 讀檔不帶時間」，兩句都錯：tick 已移到時間的 Module，「只有一個來源」歸伺服器權威策略（作者自選）。Bevy 的 `Time` 有登記、抽得到、有公開的設值入口：帶 `ReflectResource`（`crates/bevy_time/src/time.rs:193`），`TimePlugin` 登記四種 `Time`（`crates/bevy_time/src/lib.rs:78-81`），作者呼叫 `DynamicWorldBuilder::extract_resources` 時抽得到（`crates/bevy_world_serialization/src/dynamic_world_builder.rs:373`），`advance_to` 公開（`time.rs:246`；不能往回）。`Time<Real>` 內含 `Instant`，能不能寫進檔案沒有核對。Unity 的 `World.SetTime` 公開（邊界卡 `clock-other-engines.md`）。

**判定**：時間與 tick 是時間 Module 的 Resource，讀檔這一批不給它特別規則。有帶時間 Module 時，新 World 的時間從它的初始值開始；作者想讓讀檔後接著存檔時的時間，就把那些 Resource 存進存檔、讀檔時放回。選了伺服器權威策略的 World，照那個策略的規則。給時間 Module 那一批的要求：它的 Resource 要能被讀檔放回（第 8 節）。

### 5.6 診斷紀錄（卡片照 L17 判）

新 World 的紀錄從空的開始；讀檔時被丟的生成（存檔裡同一個編號兩筆、編號落在配發器範圍）照常記。`rollbackActive` 已拿掉（統合修訂 17）。紀錄的形狀（統合 L8）由那一批定。

### 5.7 預設支持包含什麼（卡片照 L17 判）

- **說明頁**：`docs/architecture/target/v2/system-contract-page.zh-Hant.md` 新增一節「讀檔」（遊戲例子、新 World、生成世界、編號兩種寫法、hook、時間、hook 衍生實體不進存檔的提醒）。
- **Kernel 的測試**：`tests/common/kernel/RestoreEntryTests.cpp`（第 13 節），守 Kernel 那一半。
- **給編排層兩批的要求**：建 World 後、第一次 Run 前設配發器的時機（5.1）；生成世界裡掛讀檔 System 的方式；它們的判定要有讀檔的案例。
- **給時間 Module 那一批的要求**：5.5。
- **不改 Kernel 的條文**：「Ready 時可設配發器」已由統合修訂 33 收進 DI-KS 15 第 (7) 項（下界的字面請主會話改，5.1）；編排層那一半的時機由骨架那一批給、形狀在那一批定。
- **給編排層骨架那一批的另外兩條要求**：「執行中丟掉現在的 World、建新的」（遊戲中讀檔靠它）；遊戲初始化放在第一個 World 的東西，丟掉第一個 World 之後怎麼辦、`new-world-default` 原話裡的「經開局」對新 World 是什麼意思。

### 5.8 Resource 怎麼放回（卡片照 L17 判）

**遊戲**：金幣 120、任務進度存在 Resource 裡。

**事實**：現在的 Kernel 沒有任何 System 的參數能放進一個還不存在的 Resource（第 3 節最後一列）；這些路在批次表第 1～4 段沒有被列進要改的範圍。

**判定**：不加 Kernel 的介面。預設寫法：編排層（或作者的組裝）在建 World 後、第一次 Run 前用 `SetResource` 放好 Resource（預設值），讀檔的 System 用 `ResMut` 覆寫成存檔的值。理由：Kernel 不多一項職責（L17）；讀檔只要「改值」，不需要「讓它出現」。

**別家**：Bevy 有 `Commands::insert_resource`，System 可以延後放進 Resource。GYO 沒有對應的參數、同步點的 Resource 那一步現在沒有東西記（`World.cpp:847-863`，主會話的 `global-audit.zh-Hant.md:68` 也列了）。要不要補，是 Kernel 職責清單（DI-KS 15）的事，照 L17 由統合判或交使用者，不在這一批。已轉主會話。

---

## 6. 和回滾的關係（回滾份已回答）

回滾份（使用者已核准）第 7.4 節逐條回答了修訂 7 的四條要求：
- 讀檔＝新 World，回滾的歷史存在 World 的 Resource，新 World 天然沒有舊歷史（R-5 守）。
- 回滾的還原和讀檔的放回不共用：讀檔是作者的 System 用一般的 `Spawn`／`SpawnWithId`；回滾用 Kernel 的原語。兩者都跑 hook，已經一致。
- LIM-7：區移出 Kernel，LIM-7 的寫法由統合減法批改寫；回滾這一側寫進回滾 Module 的限制。
- `rollbackActive`：Kernel 不加；總數照實計；作者讀「重跑中」。

這一批沒有剩下要回滾那一份做的事。

---

## 7. 來由的三件

1. **回滾重放撞上只帶 (b) 區元件的活實體（LIM-7）**：回滾份第 7.4 節回答（第 6 節）。
2. **`rollbackActive` 固定 false、重放時重複計**：`rollbackActive` 拿掉、總數照實計（第 6 節）。讀檔不是重放。
3. **單機用 `SpawnWithId` 放回會整批被丟**：這一批回答（5.1）。建 World 後、第一次 Run 前把配發器的範圍訂在存檔記的下一個號之後，`SpawnWithId` 就不被丟；不想記編號就用 `Spawn` 重新配號。F-13 與配發器不改。

---

## 8. 和其他批次的關係（批次表 `rebuild-batch-plan.zh-Hant.md`）

| 批 | 關係 |
|---|---|
| 第 1 段 Kernel 減法・結構段 | 判定裡的 `Product` 名字跟著改成 `WorldExecutor` |
| 第 2 段 編排層的骨架 | 要求：建 World 後、第一次 Run 前讓作者設配發器（範圍的下界）與放 Resource 的時機；執行中丟掉 World、建新的；遊戲初始化在第一個 World，丟掉它之後怎麼辦；它的判定要有案例 |
| 第 3 段 時間 Module＋事件 | 要求：5.5（時間的 Resource 能被讀檔放回）；讀檔的 System 要發訊息時用 `Message`／`Event` |
| 第 4 段 權威＋回滾 | 拿掉寫者的數量之後，第 3 節那兩列 D-E2 不再出現 |
| 第 5 段 生成世界（重做） | 要求：讀檔 System 掛在生成世界裡；可以分批時，照存檔編號放回的作者要在第一批之前設好配發器 |
| 第 5 段 讀檔的實作 | 本宣告。依賴第 2 段（批次表）；照先後 1～4 段已完成。不依賴同段的生成世界：測試扮演編排層，掛在一般的 schedule；生成世界做好後，由它那一批加讀檔的案例 |

---

## 9. 既有條文與守衛測試

- **Kernel 條文**：這一批不改。
- **守衛測試**：既有的判定不變；新增 `RestoreEntryTests`。
- **主會話持有的兩個檔**（gyo2-time-design c47769b1 已改）：第二次檢查指出兩句寫過頭，交主會話更正——`kernel-nested-schedule-declaration.zh-Hant.md:380` 的「這個情形不會發生」應是「預設的讀檔寫法碰不到」，不是機制上不會發生；`rebuild-entity-order.zh-Hant.md:68` 的被丟原因應是 `IdInAllotterRange`（落在配發器的範圍），不是 `IdAlive`。

---

## 10. Architecture Delta（AGENTS §3 七項）

1. Feature：玩家存檔（使用者 2026-11-16）。
2. 問題：沒有寫明讀檔的路；照字面（配發器範圍不變、`SpawnWithId`）整批被丟。
3. 邊界：Kernel 的公開介面不變；契約新增在說明頁；編排層兩批多兩條要求（5.1、5.7）。
4. 影響：說明頁、Kernel 測試；編排層的骨架與生成世界、時間 Module 三批的宣告；將來有存檔的產品。
5. 依賴方向：不變（讀檔的 System 用 Kernel 的 `Spawn`／`SpawnWithId`；編排層排它的位置）。
6. Ownership：存檔格式、存什麼、讀檔流程歸作者；讀檔在流程裡的位置歸編排層；配發器與丟棄規則歸 Kernel。
7. 為什麼不能更小：不寫明的話，作者照字面在原本的範圍用 `SpawnWithId`，整批被丟；這一批只寫明用法、用 Kernel 測試守住、把編排層那一半交給它的批次。

---

## 11. Replacement Criteria（L17 的形式；寫在說明頁的讀檔一節）

- 讀檔的預設寫法需要 Kernel 多一項職責（例：有產品必須不重建 World 就讀檔，要 Kernel 提供「在同一個 World 裡清掉再放回」；或 hook 要分辨讀檔，要 Kernel 多一個記錄者的記號）→ 停下來交使用者。
- 讀檔需要的東西判不出屬於 Kernel、編排層、時間 Module 還是作者 → 停下來交使用者。
- 第 4 節是使用者的決定（新 World），任何要改它的情形 → 停下來交使用者，不在實作批裡改。

---

## 12. 誰做、上段或下段、模型／檔位、時間

**上段**（`agents/navigator.md`）：碰到 Kernel 的公開行為的契約（配發器、丟棄規則、hook），而且是基盤。

| 角色 | 模型／檔位 |
|---|---|
| 起草者（本卡） | Opus／high |
| 宣告的對抗檢查 | Fable／high（唯讀）：修訂 0、修訂 9 各一次；修訂 10 要不要再送，由主會話或使用者定 |
| 實作者（第五段） | Opus／high |
| 審查者 | Fable（主會話） |
| 突變清單的對抗檢查 | Fable／high |

時間（agent 的時間）：實作約 3～4 小時（10 個案例 2 小時、突變 1 小時、說明頁 0.5 小時、L1 0.5 小時）。

---

## 13. 判定（先宣告，結果出來之後不放寬；寫在第 2 節的 Engine 上）

測試扮演編排層：建 World、在第一次 Run 前用 `SetResource` 設配發器、暫存的存檔資料與 Resource 的預設值、跑一張叫 `Load` 的一般 schedule（作者的名字）。不依賴生成世界那一批；它做好後由它加掛在生成世界的讀檔案例。案例名在實作前換成實際的 `TEST_CASE` 名。

### 判定 1：行為（`tests/common/kernel/RestoreEntryTests.cpp`）

1. **照存檔編號放回**：存檔有玩家 12、怪物 57、58（57 的 `ChaseTarget` 是 12），下一個號 900；配發器 `[900, 上限]`；`Load` 用 `SpawnWithId` 帶完整 bundle（含 `Position`）放回；之後另一張 schedule 生成一個選單實體。讀檔後編號是 12、57、58，值與存檔相同；`ChaseTarget` 仍是 12；選單實體的編號 ≥ 900；沒有被丟的生成。
2. **重新配號**：同一份存檔，`Load` 依存檔編號由小到大用 `Spawn`；三個實體的編號 ≥ 900、大小順序和存檔編號相同；`ChaseTarget` 由 `Load` 改寫後指向玩家的新編號。
3. **範圍沒設好**：配發器 `[1, 上限]`，照存檔編號 `SpawnWithId`，每一筆以 `IdInAllotterRange` 被丟並記紀錄。
4. **同一個編號兩筆**：(a) 存檔裡編號 57 出現兩次，同一個 `Load` 在同一窗依序記：留第一筆、第二筆以 `IdSpawnedTwice` 被丟；57 的值是第一筆的（兩筆的值不同）。(b) 兩個不同的 System 在同一窗都生成 57：兩筆都以 `IdSpawnedTwice` 被丟。(c) 57 已在前一個同步點生成、還活著，再生成一次：以 `IdAlive` 被丟。說明頁照這三種寫明。
5. **Resource**：測試在第一次 Run 前放金幣的預設值 0，`Load` 用 `ResMut` 覆寫成 120；讀檔後是 120（5.8）。
6. **hook**：每個放回的格子 OnAdd 各一次；`Monster` 的 OnAdd 在讀檔中生成血條實體，讀檔完成後每隻怪物各有一個血條、沒有錯誤（Bevy #14300 的同型）。
7. **stamp**：一個讀 `Added<Position>` 的讀者在 `Load` 之前先跑一次（這時沒有實體）；`Load` 之後它再跑，`Added<Position>` 對放回的格子成立；第三次跑不成立。
8. **先生成再插入**：`Load` 先生成只帶標記的實體、同一個 System 再插入 `Position`；讀檔後 `Position` 在、值正確。
9. **新 World 隔離**：第一個 World 跑過、有實體與被丟的紀錄；第二個 World 讀檔，只有讀檔放回的與它自己生成的實體，被丟的紀錄從空的開始。
10. **讀兩次相同**：同一份存檔在兩個新 World 讀，`Summary()` 相同。

案例 9、10 守的是「新 World」這份契約的後果，靠 World 的建構本身，沒有對應的突變（寫明，不算進檢出力）。 插入的格子（案例 8）與 `InitialValue` 補出的格子的 OnAdd，由既有的 `InitialValueTests` 等守，不算進這一批的檢出力。

### 判定 2：檢出力（AGENTS §14.2；位置照實作時的程式）

| 突變 | 預期失敗 |
|---|---|
| `RangeAllotter::Owns` 永遠真 | 案例 1（12 以 `IdInAllotterRange` 被丟） |
| `RangeAllotter::Owns` 永遠假 | 案例 3 |
| spawn 階段拿掉「落在範圍」的檢查 | 案例 3 |
| `RangeAllotter::Of` 忽略 `first`（從 0 配） | 案例 1（選單實體的編號）、案例 2 |
| `RangeAllotter::Allot` 不前進 | 案例 2（同一個編號生成兩次被丟） |
| 同一個記錄者的同一個編號留最後一筆（不是第一筆） | 案例 4(a)（57 的值） |
| 不同記錄者的同一個編號留其中一筆（不是全丟） | 案例 4(b) |
| spawn 階段 bundle 帶來的格子不跑 OnAdd | 案例 6（OnAdd 次數） |
| 套用中 hook 記的生成被丟掉 | 案例 6（血條） |
| 放回的格子 `added` 記成 1（早於讀者上次跑） | 案例 7（`Load` 之後那一次不成立） |
| 插入排在生成之前套用 | 案例 8（插入落空） |
| `ResMut::Get` 回傳副本（寫入不落到 World） | 案例 5 |

突變清單在實作前交給和實作者不同模型的檢查者做一次對抗檢查（AGENTS §14.2）。

### 判定 3：沒有回歸

`core` preset 的 kernel 測試全過；L1 三平台全過。

---

## 14. 範圍的限界

- 只寫預設的路在 Kernel 這一側的行為與給其他批的要求；存檔格式、存什麼、IO、讀檔流程、生成世界的形狀與分批、重建 World 的驅動都不在這一批。
- 判定寫在第五段的 Engine 上；在現在的程式上案例會先被擋住（寫者的數量 D-E2、沒有 domain 的 schedule D-E12），不試著在現在的程式上跑。
- System 不能放進還不存在的 Resource（5.8）；這一批的預設寫法繞開它，不補。
- 編排層那一半的保證（5.1、5.7）不在 Kernel 的測試裡，交編排層兩批。
- 「在現在的 World 裡讀」不是預設的路；作者自己做時會碰到的事（修訂 1 第 6 節：舊狀態殘留、OnRemove hook 的變更、事件、巢狀）這一批不保證。
- 參考實作：Bevy、replicon、lightyear 讀本機原始碼；flecs 09790b5f7、Unity Entities 4630ff5 由第二次檢查讀本機原始碼；Unreal Mass 沒有讀原始碼；bevy_ggrs 沒有讀到。
- 案例 2 的「順序和存檔編號相同」只在配發器單調時成立（`RangeAllotter` 是；產品自訂的配發器不保證，E2 沒有判）。

---

## 15. 確認清單（一件；修訂 11 改寫）

**讀檔放在編排層的「生成世界」。**
- 照使用者 11-16 的 `game-init-list`：遊戲初始化在行程啟動時跑一次、跑在第一個 World；生成世界每個 World 各自、可以多次、可以分批。遊戲中讀檔（使用者定為同一個機制）建的是新 World，遊戲初始化不會再跑，所以**讀檔本來就放不進遊戲初始化**，只能在生成世界。
- 使用者 11-16 在這張卡也說過「正常這些邏輯都應該寫在Startup裡的」。卡片的讀法：那句是在講「開局、Loading、讀檔是遊戲流程的名字，機制只有 Startup」，之後 `game-init-list` 與 `two-layers-like-bevy` 把「開局」切成遊戲初始化與生成世界兩件、放進編排層；讀檔落在生成世界那一件。
- 例子：RPG 玩家在第 3 關按「讀取第 2 關的存檔」→ 編排層丟掉現在的 World、建新的 → 生成世界（照資產生成第 2 關、讀檔 System 放回物品與任務；可以分批，讀取畫面會動）→ 進遊戲迴圈。行程啟動時跑過的遊戲初始化不再跑。
- 卡片的讀法若不對，請更正；沒有異議就照這樣。

---

## 16. 修訂 10 對第二次檢查的處理（`declaration-check-fable-r9.md`）

| 發現 | 處理 |
|---|---|
| blocker：判定寫在不存在的 Kernel 上、兩種掛法沒定 | 第 2 節列實作時的 Engine；第 13 節改寫在它上面、掛點交生成世界那一批；不再用 Startup |
| blocker：5.5 出處不成立、和 `clock-out-of-kernel` 相反 | 5.5 重判，出處改成 `time.rs:192-193、246`、`dynamic_world_builder.rs:373`、Unity `SetTime` |
| blocker：漏了 `two-layers-like-bevy` | 第 0、4 節；讀檔放編排層的生成世界；第 15 節確認清單；第 8 節加編排層骨架與生成世界兩批，拿掉 T3、T5 |
| should：5.3 的出處 | 改成 Bevy（含建在它上面的兩個）與 flecs 兩個獨立實作，加 Unity |
| should：配發器的 D-E2 | 第 3 節一列；5.2 寫明第 4 段後不報 |
| should：案例 6 的 `Added` 恆真 | 案例 7 改成「第一次成立、第二次不成立」，加突變 |
| should：判定 2 的空白 | 案例 4、5、8 有突變；OnAdd 拆兩列；正文說的「先生成再插入」「覆寫 Resource」「同一編號兩筆」「hook 再生成」都有案例；「分幾幀放回」移給生成世界那一批 |
| should：案例 4、5 前後不一 | 判定重寫，編號重排，沒有殘留 |
| should：已定寫成待定 | 第 6、7 節照回滾份第 7.4 節改成已回答；`rollbackActive` 已拿掉；F-13 與配發器留在 Kernel |
| should：5.7 的依據 | 改正「已在 DI-KS 4、9」；說明頁寫檔名；請求再交主會話 |
| should：Replacement Criteria 第三條 | 改成「動到使用者的決定就停下交使用者」 |
| should：其餘 | 第 2、3 節行號照 15ba81ef；5.1 分兩層 |
| nit 八件 | 修訂紀錄縮短；行號；用詞；第 9 節交主會話的兩句 |

---

## 17. 修訂 11 對窄檢查的處理（`declaration-check-fable-r10-narrow.md`）

| 發現 | 處理 |
|---|---|
| 舊 blocker：判定剩案例 5 前半與一個突變 | 5.8；案例 5 改成「先放預設、`ResMut` 覆寫」；突變改成 `ResMut::Get` 回傳副本 |
| 新 blocker：System 放不進不存在的 Resource | 5.8（不加介面）；第 3 節加一列；第 14 節；轉主會話（Bevy 有 `insert_resource`、第 7 步死碼） |
| 新 blocker：漏了 `game-init-list` | 第 0 節加列；1.1、第 2、4 節改；第 15 節重寫；第 8 節與 5.7 給骨架那一批兩條要求 |
| should：前言與第 2、8 節把生成世界當前提 | 改成並行、不是前提 |
| should：案例 7 只擋得住 0 | 讀者先跑一次；突變改成「記成 1」 |
| should：案例 4 只在同一窗成立 | 案例 4 分三種；加一個突變 |
| should：5.1 要移下界 | 5.1 寫明；DI-KS 15 第 (7) 項的字面請主會話改 |
| should：「執行中丟掉 World、建新的」沒有寄 | 5.7、第 8 節給骨架那一批 |
| nit：代號、Bevy 出處、5.5 措辭、OnAdd 的突變範圍、「字面」、過時字樣 | 第 0 節改代號；第 4 節出處改 `bevy_world_serialization`；5.5 改寫；判定 1 加註；第 4、15 節改「卡片的讀法」；讀的版本與第 14 節更新。批次表的兩處（第一段還寫「等使用者回 5.3」、骨架那一列寫「放回存檔的實體」）轉主會話 |

---

## 18. 修訂 12：實作前對照現況（草案，等動工前核准）

讀的版本：`origin/gyo2` eacab4d7（第 4 批 #259、#265，生成世界 #272，N13 #271，交換點 #270 都已合併；本分支在 f25256c4 併入）；asset 宣告（`claude/gyo2-asset-585196`，**未合併**）；狀態機修訂 8 的卡（`gyo2-state-machine-r8`）還沒有 commit。換模型檢查：Fable 5.2／high 唯讀，兩輪。第一輪檢查 a5e31b9c：blocker 2、should 7、nit 8（`revision12-adversarial-check-fable.md`）；第二輪檢查 479e75ac（第 18 節窄複查＋第 19 節、C-9）：blocker 2、should 7、nit 9（`revision12-r2-check-fable.md`）。全部採用，報告在主 checkout `build/evidence/gyo2-restore-impl/`，處理在 18.6、18.7。

**一句話**：這份宣告要的東西，骨架與生成世界兩批已經做掉大半（讀檔放在新 World、移下界的時機、遊戲中讀檔、讀兩次相同、新 World 的紀錄從空開始，都有案例）。剩下的是：一個沒有任何案例的寫法（重新配號）、三個「讀檔形狀」的組合案（hook、stamp、先生成再插入，補在生成期間的那條路上）、說明頁的讀檔一節、存檔版本 Module（第 19 節）。

### 18.1 逐條對照

**A. 仍成立（照做，不改）**

| 節 | 內容 | 現況的根據 |
|---|---|---|
| 4 | 一律新 World；讀檔是生成世界裡作者的 System | O-8、O-14（`orchestration.zh-Hant.md`）；第 15 節已核准。生成世界把它補成三個位置，見 C-11 |
| 5.1 | 編號兩種寫法；Kernel 的兩點（範圍外的 `SpawnWithId` 照常、範圍內的被丟並留紀錄） | DI-KS 15 (7)（生成世界改字）；`World.cpp` 的 `IdInAllotterRange`。只對 `Spawn`／`SpawnWithId` 成立，準備函式裡直接放的不在此限（C-10） |
| 5.2 | 讀檔不受寫者規則管 | 第 4 批拿掉寫者的數量 |
| 5.3 | hook 照常觸發（Bevy、flecs） | 讀檔走一般的 `Spawn`／`SpawnWithId`，Kernel 不認得讀檔（直接放的不跑 hook，C-10） |
| 5.4 | 放回的格子取套用那個同步點的 stamp | 同上 |
| 5.6 | 新 World 的紀錄從空開始 | `RestoreFlowTests` case 3 `[or-restore-new-records]` |
| 6 | 讀檔不帶回滾的歷史 | 回滾的狀態在 World 的 Resource 與**每次建 World 一份**的 `ModuleBuffer`（`Setup.hpp:195`、`RollbackModule.cpp:34`；#270 之後另有 Product 一份，回滾不用它），替換時隨新的登記重建 |
| 10、11 | Delta、Replacement Criteria | 不變 |

**B. 已被別批做掉（這一批不再做，只在說明頁引用）**

| 節 | 宣告要的 | 誰做的、在哪裡 |
|---|---|---|
| 5.1 編排層半 | 建 World 後、第一次 Run 前設配發器的時機 | 骨架：O-7 準備時機（帶過去 → 登記單位的準備 → 描述的準備 → `extra`）；生成世界：`entity_ids` 只在沒有值時放預設範圍，`extra` 蓋得掉（EI-1～EI-3） |
| 5.7、第 8 節 | 執行中丟掉 World、建新的；遊戲初始化只跑一次；帶過去 | 骨架：`ReplaceWorld`（O-8）、O-1、O-10；`RestoreFlowTests` case 3、6 |
| 5.7、第 8 節 | 讀檔 System 掛在生成世界；分批時第一批前設好配發器 | 生成世界：`WhileGenerating()`、`Hold()`；`WorldGenTests` C1～C3、C8；情境第 6 段 S6-4 |
| 5.5 | 時間 Module 的 Resource 能被讀檔放回 | 時間＋事件：`RestoreFixedTime`（`FixedStep.hpp:92`、WT-3：在準備時機放回）；情境第 2 段 `[sc-s2-8-fixed-put-back]` |
| 第 9 節 | 主會話持有的兩句 | 兩句現在是對的（`kernel-nested-schedule-declaration.zh-Hant.md:380`、`rebuild-entity-order.zh-Hant.md:68`）；c47769b1 是修訂 11 指出寫過頭的那一版，之後由主會話改正 |
| DI-KS 15 (7) 的字面 | 「範圍（含下界）」 | 生成世界合併前核准的最終文字（「設定配發器的值」；「範圍的下界」是搬出去的型別的概念） |

**C. 前提變了，要改**

| # | 宣告寫的 | 現在 | 修訂（卡片照 L17 判；標「交使用者」的除外） |
|---|---|---|---|
| C-1 | 5.8：System 放不進不存在的 Resource，預設寫法是「準備函式先放預設值、讀檔 System 用 `ResMut` 覆寫」 | 已經有 `Commands::SetResource`（同步點第 7 步套用，DI-KS 9）與 `ExclusiveWorld::SetResource`（當場，DI-KS 18）；說明頁 11.6 已寫 | 5.8 改成三種都可以：準備函式放預設值＋`ResMut` 覆寫、讀檔 System 用 `Commands::SetResource`、獨占的讀檔 System 用 `ExclusiveWorld::SetResource`。和 Bevy `Commands::insert_resource` 相同。提醒：`Commands::SetResource` 在同步點第 7 步才套用、晚於同一個同步點的 OnAdd（`World.cpp:890-902`），之前對不存在的 Resource 用 `Res`／`ResMut` 是斷言。三種都有既有守衛（`[sc-s1-7-scores]`、`[sc-s1-3-best-at-sync]`、`ImmediateChangeTests`「Commands::SetResource and RemoveResource are applied at the sync point's resource step」），不加案 |
| C-2 | 第 2 節：配發器、F-13 留在 Kernel | `RangeAllotter` 搬到 `entity_ids` Module（預設帶）；Kernel 只留 `EntityAllotter` 的概念與丟棄規則；Kernel 與編排層的測試用替身 `tests/common/support/TestRangeAllotter.hpp` | 說明頁寫 `EntityIds::RangeAllotter`。判定 2 的四個配發器突變落在哪裡：**交使用者**（18.2 第 1 件，因為「改替身」核准時的條件是案例不動） |
| C-3 | 第 3 節、第 14 節：判定在現在的程式上會被 D-E2、D-E12 擋住 | D-E12 第 3 批（時間＋事件）拿掉、D-E2 第 4 批拿掉 | 這兩句失效（歷史保留） |
| C-4 | 生成世界 5.7 留給本批：讀檔要不要用 `ExclusiveWorld::Restore` | `Restore` 是回滾的原語，整份狀態的語意：`Alive(ids)` 之外的實體全部 despawn、`Column<T>` 沒列到的格移除（`Params.hpp:868-878`）；不補 `InitialValue`；不看配發器的範圍 | **不寫成讀檔的寫法**。理由：照預設流程「先照資產生成關卡、再放回存檔」用它，剛生成的關卡會被整個刪掉；沒有點名的 Feature 需要它；別家讀檔沒有用回滾原語的。說明頁只留一句（照 DI-KS 12 的中立寫法）：「`Restore` 是回滾用的原語，Kernel 不認得放回的用途；它整份換掉 World 的狀態（名單外的實體與格都拿掉），不是預設的讀檔寫法，只適合存了整個 World 的作者」 |
| C-5 | 5.3 只寫 hook | 第 4 批後 `InitialValue` 只在一般的生成與插入補（bundle 已有的元件不蓋，`InitialValueTests`「a component the entity already has is left as it is」） | 說明頁補：照一般寫法讀檔時，存檔沒存的衍生元件由初始值的宣告補；存了的照存檔的值（直接放的不補，C-10） |
| C-6 | 沒寫 asset | asset 宣告 5.14（`claude/gyo2-asset-585196`，**未合併**）：`AssetId` 是 catalog 的字串、持久鍵，存檔裡記 id；資產在 World 外 | 說明頁補一句「存檔記資產 id、不存資產本身；id 能不能用在發替換之前驗（生成世界 5.4、5.9）」，不寫還沒合併的 API 名 |
| C-7 | 第 13 節判定 1：新開 `tests/common/kernel/RestoreEntryTests.cpp`、十個案例 | 六個案例已有同一件事、帶標記的案例（18.3） | **交使用者**：18.2 第 2 件 |
| C-8 | 狀態機 | 狀態機修訂 8 的卡在做；舊宣告 3.8「第一次套用一律先進初始值」。照做的話讀檔時新 World 先進初始狀態（初始狀態的 OnEnter 跑一次）再轉到存檔的狀態 | 不在本批定。已經主會話轉狀態機卡；本批說明頁只指路 |
| C-9 | 沒寫讀檔完成的通知 | 送法原則（`v8-arch-gyo2-event-delivery-rule`，使用者 11-19）的表寫「讀檔完成由新 World 準備後發 `Message`（讀檔批定案）」；同一則 (1)「一直存在的狀態 → 讀 Resource」、(3)「之後各批照原則定送法，不再逐批問」 | 卡片照原則 (1) 判：**Engine 不發「讀檔完成」**。Engine 不認得讀檔（第 4 節），「放回完了」就是生成結束，讀 `WorldGeneration`（`Generating()`）或掛 `AfterGenerating()`；作者要通知別的 System，讀檔 System 自己發 `Message`（生成世界 5.5：沒有讀者拉住時跑完就被修剪）。另一條使用者的決定也撐它：`-world-events-not-via-event`（World 的建立與替換不發事件）。說明頁給「讀檔完成時只做一次」的寫法：`AfterGenerating()` 之後一直為真，作者自己記旗標。和表的字面不同，**交使用者確認**（18.2 第 3 件） |
| C-10 | 沒寫（DI-KS 18 的使用者決定在修訂 11 核准之後：`v8-arch-gyo2-direct-insert-no-allotter-range`，11-18） | 準備函式裡用 `World::Insert` 直接放實體（`World.hpp:215-223`：Host、Ready 才能用）：**不跑 hook、不補 `InitialValue`、不檢查也不移動配發器的範圍**。情境第 1 段 S1-7b 守（忘了移下界時不是被丟，而是之後的 `Spawn` 回 `AllottedIdAlive`） | 說明頁列成第三條放回的路與它的三個差別：「碰撞體要靠 OnAdd 進空間索引的、要靠初始值補衍生元件的，不要走這條；走這條也要移下界」。配發器那一半有守衛（S1-7b）；「不跑 hook、不補初始值」目前是 `World.hpp:215-216` 的註解與設計，說明頁照這樣寫明出處。不加案 |
| C-11 | 第 4 節「讀檔是生成世界裡作者的 System」 | 生成世界第 14 節第 3 件（使用者 11-22 確認）：放回的位置有三個、失敗的後果不同——`beforeSwap`（一次放完，失敗保留舊 World）、生成期間（跨幀，失敗 Product 結束）、準備函式直接放（C-10）；O-8 的安全範圍收窄；`beforeSwap` 期間沒有交換點（拿不到這一幀的外部資料） | 判定與說明頁分開寫三條路各由誰守（18.3）；說明頁照 18.4 的清單搬 |

### 18.2 交使用者的三件

**第 1 件：判定 2 的四個配發器突變落在哪裡（改使用者 11-22 核准的「改替身」的條件）**

使用者核准「改替身」時的原文（`world-gen-declaration.zh-Hant.md:529`）是「突變改 Kernel 測試的替身，**判定的位置與案例都不動**」——前提是讀檔案例 1～3 寫在 Kernel 的測試裡、Kernel 不能 link `entity_ids`，所以只能改替身。第 2 件若照建議不開 Kernel 的讀檔測試檔，這個前提就沒了。

| 選項 | 突變改哪裡 | 預期失敗（先宣告） |
|---|---|---|
| **改真型別（建議）** | `engine/modules/entity_ids/include/engine/entity_ids/RangeAllotter.hpp`（出貨的程式） | `Owns` 恆真：`[sc-s1-7-put-rejected]`（情境第 1 段 S1-7，遊戲自己登記真的 `RangeAllotter`、移下界後照存檔編號放回；英雄被丟。第 6 段 S6-4 會先在 `ScenarioSegment6Tests.cpp:219` 的 `REQUIRE` 中止、走不到 `[sc-s6-4-same-id]`，不用它）；`Owns` 恆假：`[ei-real-in-range-dropped]`；`Of` 忽略 `first`：**不另立**，就是既有的 `sc-lower-bound-not-moved`（同一個改法，expect `[sc-s1-7-put-rejected]`）；`Allot` 不前進：`EntityIdsTests`「allotter: monotonic」的 `allotter.Allot() == std::optional<EntityId>(101)` |
| 照核准改替身 | `tests/common/support/TestRangeAllotter.hpp`（測試的程式；Kernel 與編排層共用） | `Owns` 恆真：`[or-restore-ids]`；恆假：`[or-restore-control]`；`Of` 忽略 `first`、`Allot` 不前進：新增的重新配號案的標記。代價：Kernel 的 J-8、J-55 會先紅一片（算存活與否只看宣告的標記）；守到的是測試的替身，不是出貨的程式 |

理由：守的對象應該是出貨的 `RangeAllotter`；真型別上已有三個案例抓得到，其中一個突變已存在（§14.5 不另立）。代價照實寫：選這一項時，編排層用替身的讀檔案例（`[or-restore-ids]`、`-ref`、`-nothing-dropped`、`-control` 與新的重新配號案）沒有配發器的突變；它們用到的 Engine 部分（`SpawnWithId` 的範圍判定、`Spawn::New` 回傳配到的編號）由 Kernel 既有的突變守（實作第一步查出 id 寫進 dev_log），這些案例是讀檔寫法的示範，不是受け入れ判定的唯一根據。

**第 2 件：判定 1 改成「已有帶標記的案例就沿用，沒有的才寫」**

**遊戲例子**：「讀檔時同一個編號出現兩次，留第一筆」——Kernel 的 J-55 已經守著（`sr-keep-last`），讀檔只是用同一個 `SpawnWithId`。原判定要在 `RestoreEntryTests.cpp` 再寫一次，只是換成讀檔的名字。

| 選項 | 內容 |
|---|---|
| **沿用＋補（建議）** | 不新開 Kernel 的讀檔測試檔。案例 1、3、4、5、9、10 沿用既有、帶標記的案例（18.3）；案例 2（重新配號）新寫；案例 6、7、8 照「讀檔形狀」新寫在**生成期間**那條路上（`RestoreFlowTests.cpp`，讀檔 System 在 `WhileGenerating()` 的 set、延後的 `SpawnWithId`——這條路現有的 `WorldGenTests` C1～C3 沒有標記）。突變照 18.3 的表先宣告預期失敗 |
| 照原判定 | 新開 `RestoreEntryTests.cpp`，十個案例照寫；六個和既有重複，突變被兩處抓到，不增加檢出力（§14.5） |

**緩的面**（照實列，不是「沒有放寬」）：(1) Kernel 這一半不再有不依賴編排層的讀檔測試檔——Kernel 的機制由 J-8、J-30、J-55、`HookFlushTests`、`ImmediateChangeTests` 守，讀檔的組合由編排層的案例守；(2) 案例 9、10 改由編排層的 `RestoreFlowTests` case 3、5 與 `WorldGenTests` C3 守。舊判定從沒實作、沒有結果；宣告 `:4` 已寫明判定在實作批照 §14.1、§14.2 宣告，所以這是動工前重新宣告，不是 §14.4 的改既有測試。

**第 3 件：讀檔完成不發 `Message`（C-9）**

送法原則的表寫「讀檔完成由新 World 準備後發 `Message`（讀檔批定案）」。卡片照同一則的原則 (1) 判成「Engine 不發；讀 `WorldGeneration` 或掛 `AfterGenerating()`；作者要的話自己發」。理由：Engine 不認得讀檔；「放回完了」是一直存在的狀態。和表的字面不同，請確認；若要 Engine 發，請說由誰發（編排層不認得讀檔，只能是「生成結束」的通知）。

### 18.3 判定 1、2（照 18.2 建議；結果出來前宣告，之後不放寬）

**判定 1：案例**

| 案例 | 路 | 由誰守（標記） |
|---|---|---|
| 1 照存檔編號放回 | `beforeSwap`／啟動時讀檔 | `RestoreFlowTests` case 1 `[or-restore-ids]`、`[or-restore-ref]`、`[or-restore-nothing-dropped]`；情境 S1-7 |
| 1' 同上 | 生成期間＋真 `entity_ids` | 情境第 6 段 S6-4 `[sc-s6-4-same-id]`、`[sc-s6-4-above-bound]` |
| 2 重新配號 | 生成期間 | **新增** `RestoreFlowTests`「restore: renumbering while generating」：**不移下界**（重新配號不需要；說明頁寫清楚只有照存檔編號放回才要移），讀檔 System 在 `WhileGenerating()` 的 set，存檔三個實體依編號由小到大用 `Spawn` 放回；**逐個實體**判：新編號是配發器配出的（`[or-renumber-fresh]`）、大小順序和存檔相同（`[or-renumber-order]`）、`ChaseTarget` 改寫後指向玩家的新編號（`[or-renumber-ref]`）；同一個讀檔 System 用 `ResMut<Gold>` 把金幣改成存檔的 120（`[or-gen-resmut-gold]`）、沒有被丟的生成 |
| 3 範圍沒設好 | `beforeSwap` | `RestoreFlowTests` case 2 `[or-restore-control]`；真型別 `[ei-real-in-range-dropped]` |
| 4 同一編號兩筆 | Kernel | J-55 三案（`sr-keep-last` expect `J-55 one-call-twice: 9 keeps the first value`；`sr-two-emitters-keep-first`；`sr-reason-alive-as-twice`） |
| 5 Resource | `beforeSwap`、生成期間 | `RestoreFlowTests` case 1、3 的金幣（那裡是 `ExclusiveWorld::Modify` 寫的）。**兩處同一個標記、期待值不同**（130、120），實作時改名成 `[or-restore-gold-startup]`、`[or-restore-gold-in-game]`（只改標記字樣，判定不變）；`ResMut` 覆寫由案例 2 的 `[or-gen-resmut-gold]` 守 |
| 6 hook | 生成期間 | **新增**「restore: hooks while generating」：每個放回的格子 OnAdd 各一次（`[or-restore-onadd-once]`）；`Monster` 的 OnAdd 生成血條實體，生成結束時每隻怪物各一個血條、沒有錯誤（`[or-restore-hook-spawn]`；Bevy #14300 的同型） |
| 7 stamp | 生成期間 | **新增**「restore: Added after a load」：讀 `Added<Pos>` 的讀者在讀檔那一幀之前先跑過一次（沒有實體）；讀檔之後那一次成立（`[or-restore-added-seen]`）；再下一次不成立（`[or-restore-added-once]`） |
| 8 先生成再插入 | 生成期間 | **新增**「restore: spawn then insert in one System」：讀檔 System 先生成只帶標記的實體、同一個 System 再 `Insert` `Pos`；生成結束時 `Pos` 在、值正確（`[or-restore-spawn-insert]`） |
| 9 新 World 隔離 | 替換 | `RestoreFlowTests` case 3 `[or-restore-new-world]`、`[or-restore-new-records]` |
| 10 讀兩次相同 | 替換、生成期間 | `RestoreFlowTests` case 5（實作時加標記 `[or-restore-twice-same]`）；`WorldGenTests` C3 |

**判定 2：突變→預期失敗（先宣告；「存活」＝宣告的標記沒紅，別的測試紅了也算存活；建置失敗不算抓到）**

| 突變 | 改法 | 預期失敗 |
|---|---|---|
| 配發器四個 | 18.2 第 1 件的表 | 同表 |
| spawn 階段拿掉「落在範圍」的檢查 | — | **不另立**：既有 `sr-range-removed` 已併入 `sr-dropped-made-alive` |
| 同一記錄者留最後一筆 | — | **不另立**：`sr-keep-last` |
| 不同記錄者留其中一筆 | — | **不另立**：`sr-two-emitters-keep-first` |
| spawn 階段 bundle 帶來的格子不跑 OnAdd | `World.cpp` 套用生成時不把 bundle 的格放進 OnAdd 名單 | `[or-restore-onadd-once]` |
| hook 記的生成被丟掉 | hook 佇列裡的生成紀錄在套用時一律丟掉：讀檔 System 在 `Main/Update` 時，hook 的紀錄在下一個同步點第 0 步套用（`World.cpp:935` 一帶），最後一個同步點的才進清空輪；改的是兩條路共用的那一處（實作時找出那一行；只改到清空輪的不算這個突變） | `[or-restore-hook-spawn]` |
| 放回的格子 `added` 記成 1 | 生成的格的 `added` 寫 1 | `[or-restore-added-seen]` |
| 插入排在生成之前套用 | 同步點的套用先做插入、再做生成 | `[or-restore-spawn-insert]` |
| `ResMut` 的寫入不落到 World | `ResMut` 的 `operator->` 改成指向一份每次呼叫都重建的副本（寫到副本後丟掉；編得過） | `[or-gen-resmut-gold]` |
| 重新配號：讀檔 System 的對照表 | 不是 Engine 的程式，不立突變。案例 2 用到的 Engine 部分（單調配發、`Spawn::New` 回傳編號）由既有突變守（選「改真型別」時替身沒有突變，見 18.2 第 1 件的代價） | — |

實作第一步：每一列先對既有清單（Kernel 654 個、v2 的六份）查有沒有同一個 find／replace，**也查已退掉、已併掉的 id**（例：`sr-range-removed` 被併進 `sr-dropped-made-alive`，`what` 欄有寫）；有就照 §14.5 加進既有的、不另立。突變清單在跑之前送 Fable／high 對抗檢查（§14.2）。

**判定 3：沒有回歸**：`core` preset 的 kernel、orchestration、entity_ids、scenario 測試全過；L1 三平台全過。

### 18.4 說明頁

`system-contract-page.zh-Hant.md` 新增第 15 節「讀檔」，一處寫完；第 13 節替換那一條（`:448`）與第 14 節的讀檔一段（`:488`）改成指過去。要有（搬的時候最容易掉的標 ★）：遊戲例子；新 World；★ 啟動時讀檔走描述的準備（沒有 `ReplaceWorld`）、遊戲中讀檔走 `ReplaceWorld`；★ 三條放回的路與失敗後果（`beforeSwap`、生成期間、準備函式直接放，C-10、C-11）；★ `BuildNewFirst`／`DropOldFirst`、替換會重新登記（動態插入的不在）、★ 發替換之前先驗（版本、資產 id）；編號兩種寫法（含重新配號的對照表寫法）；Resource 的三種放法（C-1）；hook 與 hook 衍生實體（若也存進存檔，讀檔後兩份）；初始值（C-5）；stamp；時間（WT-3，在 `extra`）；資產 id（C-6）；`Restore` 不是讀檔的寫法（C-4）；狀態機指路（C-8）；讀檔完成（C-9，照第 3 件的結果）；存檔版本 Module（第 19 節）。

### 18.5 時間、角色

實作約 3.5 小時（突變 0.5、四個新案例 1、說明頁 0.5、存檔版本 Module 1.5）。角色照第 12 節：實作 Opus／high；突變清單與宣告的對抗檢查 Fable／high；合併前設計審查 Fable／high（唯讀）。

### 18.7 對第二輪檢查（479e75ac）的處理

| 發現 | 處理 |
|---|---|
| B1 兩個預期失敗紅不了 | `Owns` 恆真改成 `[sc-s1-7-put-rejected]`；`ResMut` 不落地改由案例 2 的生成期間讀檔用 `ResMut` 放回金幣、新標記 `[or-gen-resmut-gold]` |
| B2 19.4 第 2 件的選項不完整 | 19.4 第 2 件改成三個選項並列、各寫代價與啟用後的效果；「不放在編排裡」的讀法列為要確認 |
| S1 改真型別後替身案例沒有突變 | 18.2 第 1 件寫明代價 |
| S2 也要查退掉、併掉的 id | 判定 2 的實作第一步 |
| S3 hook 生成的突變改法 | 寫成兩條路共用的那一處 |
| S4 新 Module 缺 §3 三樣 | 19.7、19.8 |
| S5 19.5 的判定 | 19.5 重寫：突變→標記表、存活的定義、加「拿掉太舊的拒絕」、V4 不自證、拿掉沒有突變的標記 |
| S6 第 (3) 個位置做不到 | 19.1 改成在 `build` 回呼用 `WorldSetup::ProductBuffer()` 取 `Policy` 再 `Prepare(fn)` |
| S7 Unity 那一列 | 19.2 改：`TryRead` 收的是 `SceneMetaDataSerializeUtility` 的版本；加 `DeserializeWorld` 逐型別比 `StableTypeHash`、對不上丟例外 |
| nit 1～9 | 新案例是四個、時間對齊；C-4 措辭照 DI-KS 12；C-9 加 `-world-events-not-via-event` 與「只做一次」的寫法；V5 比對文字；舊範例字串改名；第一個 `library_only=no`、`default=no` 的單位、啟用寫法；「高於現在也拒」列事後報告；重新配號不移下界；C-10 守衛的範圍 |

### 18.6 對 Fable 檢查（a5e31b9c）的處理

| 發現 | 處理 |
|---|---|
| B1 預期失敗是跑完才寫的 | 18.3 判定 2 改成先宣告的「突變→預期失敗」表；存活的定義寫明；`[or-restore-gold]` 拆成兩個標記 |
| B2 「改替身」的條件被改掉 | 18.2 第 1 件交使用者，列兩個選項 |
| S1 「八個」「不是放寬」說過頭 | 18.2 第 2 件改成六個沿用、一個新增、三個補成讀檔形狀的組合案；緩的面照實列兩項（第三項因為 6、7、8 照寫而消失） |
| S2 四個突變已在清單 | 判定 2 標「不另立」；`sr-range-*` 的錯誤引用改成 `sr-dropped-made-alive` |
| S3 漏 DI-KS 18 直接放 | C-10 |
| S4 漏三個位置 | C-11；18.3 每列寫哪一條路；6、7、8 補在生成期間；18.4 的 ★ |
| S5 `Restore` 少最大的差別、沒有 Feature | C-4 改成不寫成讀檔的寫法 |
| S6 讀的版本過時 | 18 節開頭改 eacab4d7；A 類第 6 列寫明每次建 World 的 `ModuleBuffer`；C-11 寫 `beforeSwap` 沒有交換點 |
| S7 三列引錯守衛 | 案例 6、7、8 改成新寫的組合案，不引 J-30、J-54 |
| nit 1 flecs 是混合型 | 第 19 節之前的系譜改寫（C-4 不再談系譜）；生成世界宣告 `:200`「三家都重新配號」與 flecs「能沿用就沿用、否則重配」（`deserialize.c:129-168`）不一致，轉主會話 |
| nit 2 C-3 的批次 | 改 |
| nit 3 `ResMut::Get` 編不過 | 判定 2 寫成編得過的改法 |
| nit 4 案例 1 引 S6-4 | 18.3 加 1' |
| nit 5 C-1 的提醒 | 加 |
| nit 6 C-6 未合併 | 寫明，不寫 API 名 |
| nit 7 第 9 節出處 | B 類改寫 |
| nit 8 `[or-restore-range]` 偏弱 | 新的重新配號案逐個實體判 |

---

## 19. 存檔版本 Module（`v8-arch-gyo2-save-version-into-restore`；修訂 12 草案）

**來由**：使用者 2026-11-18（`v8-arch-gyo2-save-version-module`）：「需要提供version module，默認不啟用，啟用之後過時檔會直接拒絕。這個補充不放在編排裡，但是得放計劃中。Engine需要支持。」前一句（`-replace-order-author-choice`）：「壞檔問題是最嚴重的Bug，通常是來自於系統更新……通常製作的時候直接上version 管理。太老的檔案不支持。讓玩家自己開新檔。」2026-11-22 併進本卡、同一份宣告、同一個 PR（原話「合併進去。」）。

**用遊戲講**：RPG 1.0 的存檔格式是第 3 版。一年後的 1.5 改了背包，作者把現在的版本訂成 5、最舊支援訂成 4。玩家拿 1.0 的存檔（第 3 版）按「讀取」：選單上直接顯示「這個存檔太舊，請開新遊戲」，現在的 World 照跑，沒有半個讀進來的 World。存檔時寫上 5。

**核心一句**：Engine 只比對一個數字。數字寫在存檔的哪裡、怎麼讀出來，是作者的格式；Module 給的是「現在幾版、最舊收幾版」的一處設定和「收不收」的判斷，拒絕走讀檔失敗的路。

### 19.1 四件

| 件 | 判（卡片照 L17 判，標「使用者」的除外） |
|---|---|
| **存的時候** | 作者從 Module 拿「現在的版本」（`SaveVersion::Policy::Current()`），寫進自己的存檔格式的任何位置。Module 不寫檔頭、不定位置、不碰 io（「不認得存檔只指格式」，使用者 `-save-unaware-means-format-only`） |
| **讀的時候** | 作者先用自己的格式讀出版本數字，交給 `Policy::Check(版本)`；回 `Result`：收、或拒絕（`OrchestrationError`，`reporterCode` 是 `"save-version-too-old"` 或 `"save-version-too-new"`）。**檢查的位置**三處擇一，都在換上之前：(1) 發 `ReplaceWorld` 之前（runner 從 `Product::Buffer()` 拿 `Policy`；選 `DropOldFirst` 時只能這裡，否則被拒＝位置空了、Product 結束）；(2) `ReplaceRequest::extra` 準備函式裡呼叫、把錯誤原樣回傳 → `ReplaceWorld` 回 `ReplacePrepareFailed`、`reporterCode` 照帶（O-8、O-5；`BuildNewFirst` 時舊 World 照跑）；(3) 啟動時讀檔：描述的準備函式在 `Product::Build` 之前就寫好、只拿得到 `World&`（`Product.hpp:50`、`Setup.hpp:80`），碰不到 `Policy`；做法是在描述的 `build` 回呼裡用 `WorldSetup::ProductBuffer()` 取出 `Policy`，再 `Prepare(fn)` 登記一個呼叫它的準備函式（`Setup.hpp:259-261`）→ 被拒時 `Product::Build` 失敗、O-E7（O-E7 沒有 `reporterCode` 欄位，碼接在訊息文字裡，`Product.cpp:274-278`）。**不放在生成期間**（生成期間的錯誤是 Product 結束，生成世界 5.4、5.9） |
| **預設不啟用** | 使用者明說。`config/modules.csv` 一列 `library_only=no`、`default=no`（第一個這樣的單位；現有的進 World 的單位都是 `default=yes`）；產品用 `gyo_product_units(... MODULES save_version)` 啟用（`GyoUnitRegistry.cmake:298`）；產品沒列就不 link、不登記，零成本。啟用時也只在 `Configure` 放一個 `Policy` 進 Product 一份的共用處（交換點 #270 的 `ModuleBuffer`），**不登記任何 World 的元件、Resource、System**：每幀零成本 |
| **作者的關係** | 版本欄位是作者格式的一部分（作者的資料契約，AGENTS §8）。Module 只要一個整數；作者用別的版本表示（字串、語意版本）時自己換成整數。設定由 `ProductDescription::options` 給 `SaveVersionOptions{ std::uint32_t current; std::uint32_t oldestSupported; }`；`oldestSupported > current` 或沒給設定是 `Configure` 的回報（啟動錯誤，O-5） |

### 19.2 和別家對照

| 誰 | 版本怎麼處理 | 出處 |
|---|---|---|
| Unity Entities | 兩層。(1) 檔頭：`BlobAssetReference<T>.TryRead(reader, version, out)` 讀檔頭的一個 int，和呼叫者給的期望值**不等就回 false**（不丟例外、不轉換）；場景的標頭由 `ResolveSceneSectionUtility` 用 `SceneMetaDataSerializeUtility.CurrentFileFormatVersion` 呼叫，失敗記「old version」的錯誤。(2) 自動：`SerializeUtility.DeserializeWorld` 逐型別比對 `StableTypeHash`，找不到型別就丟例外（型別改了舊檔就讀不了） | `Unity.Entities/Blobs.cs:558-580`；`Unity.Scenes/ResolveSceneSectionUtility.cs:133`；`Unity.Entities/Serialization/SerializeUtility.cs:676-690` |
| Bevy | 世界序列化沒有版本欄位；靠 reflect 的型別路徑，型別沒登記就是反序列化錯誤。版本管理交作者 | `crates/bevy_world_serialization/src` 沒有 version（本機 `git grep` 只命中無關的字） |
| flecs | JSON 的 `"version"` 是實體的世代，不是檔案版本；沒有檔案版本 | `src/addons/json/deserialize.c:445` |
| Unreal | 一般知識（沒讀原始碼）：`FArchive` 帶引擎版本與各子系統的 custom version，載入舊版走程式碼轉換 | — |

**和別家不同的地方**（交使用者確認，19.4 第 1 件）：
- Unity 是「不等就拒」，本頁是「低於最舊或高於現在才拒」——中間的版本收，舊檔要轉換的部分由作者的讀檔程式處理（使用者「太老的檔案不支持」讀成「有一條最舊線」）。查到的三家沒有一家用「最舊支援線」（19.4 第 1 件）。
- Unity 的版本寫在檔頭、由 Engine 讀；本頁建議由作者讀出交給 Module（GYO 不認得格式）（19.4 第 2 件列了三條路）。
- Unity 另有自動的一層（型別雜湊對不上就丟例外）；GYO 的讀檔是作者的程式，Engine 沒有存檔裡的型別資訊可比，沒有這一層。
- Bevy、flecs 沒有這個 Module；本頁照使用者的決定提供（不是推翻）。

### 19.3 卡片照 L17 判、不交使用者的

- **高於現在也拒**（玩家降版或拿到別人新版的檔）：碼分兩個，選單可以顯示不同的話。Unity 的「不等就拒」也拒新版。使用者說的只有「過時」，列進合併前的事後報告。
- **碼的名字**：`"save-version-too-old"`、`"save-version-too-new"`。既有的範例字串 `"save-version-unsupported"`（`OrchestrationError.hpp:183` 的註解、`RestoreFlowTests.cpp:24`）是骨架那一批的合成例子，實作時改成新的名字。
- **不轉換**：使用者「或者提供存檔轉換功能」是可能的另一條路，沒有要 Engine 做；轉換是作者讀檔程式的事。
- **不加編排層的步驟**（使用者「不放在編排裡」）：檢查由作者呼叫，走已有的準備函式與 `reporterCode`（骨架 3.9 已確認接得住）。
- **不是事件**：被拒是 `Result`（送法原則的表；時間＋事件宣告第 268 行）。
- **位置與名字**：`engine/modules/save_version`、namespace `GYO::Engine::SaveVersion`、target `GYO::SaveVersion`、登記單位 `SaveVersionModule`（N11～N17、N20；`entity_ids` 的先例）。

### 19.4 交使用者的

1. **「過時」的界線**：(a) **最舊支援線（建議）**——`oldestSupported ≤ 版本 ≤ current` 收，中間舊版由作者的讀檔程式補；(b) **只收現在的版本**（Unity `TryRead` 的做法）——任何不等都拒，作者每改一次格式就讓所有舊檔失效。理由：使用者說的是「太老的檔案不支持」，不是「舊的都不支持」；(b) 可以用 `oldestSupported = current` 做到，所以 (a) 包含 (b)。
2. **「直接拒絕」怎麼做到**（三條路；19.2 的 Unity 檔頭也在裡面）：

| 選項 | 怎麼做 | 啟用之後的效果 | 代價 |
|---|---|---|---|
| **(a) 作者呼叫（建議）** | 作者用自己的格式讀出版本數字，在讀檔入口呼叫 `Policy::Check` 一次（19.1 的三個位置） | **啟用本身不擋任何東西**：作者忘了呼叫就照收。Module 的全部行為是一處設定＋兩個整數比較＋固定的碼 | 最小；Engine 完全不碰格式。「直接」只讀成「不轉換、直接回錯誤」 |
| (b) 作者自選的檔頭小工具 | Module 另給 `WriteHeader(bytes, 版本)`／`ReadHeader(bytes)`：在作者的位元組開頭放一個固定的小檔頭（標記＋版本）；用了它，讀檔入口可以只交位元組給 `Check`（Unity `TryRead` 的做法） | 用了檔頭的作者，版本由 Module 讀出、比對，作者不必自己解析 | Engine 定了作者格式的一小段（作者選用的）；和 `-save-unaware-means-format-only` 是否相容要你判 |
| (c) 自動攔 | 編排層在替換前（`beforeSwap` 之前）問各登記單位「收不收」，這個 Module 回答 | 啟用就擋，作者不會忘 | 要編排層多一個步驟，而且仍需要有人先讀出版本數字（(a) 或 (b)）；和「不放在編排裡」衝突——**這句若只是排程的話**（HANDOFF 的說明是「不放在編排層（第 2 批），列進批次表」，可能指「不排在骨架那一批」），這條路就沒有被排除 |

卡片建議 (a)：Engine 不認得格式，數字一定要作者讀出；(b)、(c) 都多一份 Engine 要守的契約，沒有點名的 Feature 需要。請你判：(1) 「不放在編排裡」是架構的限制還是排程的話；(2) 選哪一條。

### 19.5 判定（照 19.4 建議 (a)；結果出來前宣告，之後不放寬）

`tests/common/save_version/`（link `GYO::Orchestration`＋`GYO::SaveVersion`，合成的單機遊戲沿用 `SaveGame.hpp`）。設定 `{current 5, oldestSupported 4}`。

| # | 案例 | 檢查（標記） |
|---|---|---|
| V1 | 邊界收：版本 4、5 | `Check` 成功（`[sv-accept-oldest]` 對 4、`[sv-accept-current]` 對 5） |
| V2 | 太舊：版本 3，`BuildNewFirst`，`extra` 裡呼叫並回傳錯誤 | `ReplaceWorld` 回 `ReplacePrepareFailed`、`reporterCode == "save-version-too-old"`（`[sv-too-old]`）。舊 World 照跑由骨架既有的 `or-prepare-failure-swaps` 守，不另立標記 |
| V3 | 太新：版本 6 | `reporterCode == "save-version-too-new"`（`[sv-too-new]`） |
| V4 | runner 從 `Product::Buffer()` 拿得到 `Policy`（`DropOldFirst` 前先驗的那條路） | `Product::Buffer()` 裡有 `Policy`、它的 `Check(3)` 回太舊（`[sv-buffer-policy]`） |
| V5 | 啟動時讀檔被拒：`build` 回呼取 `Policy`、`Prepare(fn)` 呼叫 | `Product::Build` 失敗、O-E7、訊息文字含 `save-version-too-old`（`[sv-startup]`） |
| V6 | 設定錯：`oldestSupported 6 > current 5` | `Product::Build` 失敗、單位回報（`[sv-config-order]`）；沒給設定（`[sv-config-missing]`） |
| V7 | 不啟用：產品的單位表由真的 `config/modules.csv` 產生、沒有列 `save_version` | `Product::Buffer()` 裡沒有 `Policy`（`[sv-default-off]`） |
| V8 | 存的時候：`Current()` | 回 5（`[sv-current]`） |

**突變→預期失敗**（「存活」＝宣告的標記沒紅；建置失敗不算抓到）：

| 突變 | 預期失敗 |
|---|---|
| 拿掉「太舊」的拒絕 | `[sv-too-old]` |
| 「太舊」的比較 `<` 改 `<=`（最舊那一版也拒） | `[sv-accept-oldest]` |
| 拿掉「太新」的拒絕 | `[sv-too-new]` |
| 「太新」的比較 `>` 改 `>=`（現在的版本也拒） | `[sv-accept-current]` |
| 兩個碼對調 | `[sv-too-old]` |
| `Configure` 不把 `Policy` 放進 Product 的共用處 | `[sv-buffer-policy]` |
| `Configure` 不檢查 `oldest > current` | `[sv-config-order]` |
| `Configure` 沒給設定時用預設值、不回報 | `[sv-config-missing]` |
| `config/modules.csv` 的 `default=no` 改 `yes` | `[sv-default-off]` |
| `Current()` 回 `oldestSupported` | `[sv-current]` |

`[sv-startup]` 守的是編排層 O-E7 的既有路（`or-` 的既有突變），不另立。突變清單在跑之前送 Fable／high 對抗檢查（§14.2）。時間：約 1.5 小時。說明頁第 15 節「讀檔」加一小節。

### 19.6 對 io 的需求（給 threads／io 重審卡，經主會話）

讀檔與存檔版本 Module 對 io **沒有新需求**：Engine 不做存檔的讀寫。作者用 io 讀寫存檔時用到的是既有的：整檔原子寫入（`engine/io/helpers/WriteFileAtomically.hpp`，防寫到一半的壞檔——使用者說壞檔是最嚴重的 Bug）、整檔讀（`ReadAllBytes.hpp`）；讀檔在 World 執行緒外先讀好、驗好版本再發替換（不在 World 裡做 io）。重審卡回覆兩個都留下（重審宣告 I6）。**存檔目錄**：目前 Engine（io、os）不提供每個使用者的資料目錄（macOS Application Support、Windows `%APPDATA%`、Linux `$XDG_DATA_HOME`），存檔路徑由作者自己決定（`UserFiles.hpp` 只有 `ProbeFileStamp`，讀整檔算戳記判斷有沒有被改過，不是目錄）。要不要補、補在哪一批由重審卡問使用者，本卡不依賴它。

### 19.7 Derivation Order 與 Architecture Delta（AGENTS §2、§3）

| 層 | 內容 |
|---|---|
| Feature | 玩家存檔：遊戲更新後讀舊存檔，太舊的直接拒絕、讓玩家開新遊戲（使用者 11-18）。Product：單機遊戲 |
| Contract | `Policy::Current()`、`Policy::Check(版本) → Result`、兩個碼、`SaveVersionOptions`；存檔裡的版本欄位是作者的資料契約 |
| Architecture | 新 Module `engine/modules/save_version`（預設不帶）；不改 Kernel、不改編排層 |

1. Feature：同上。
2. 問題：沒有一處放「現在幾版、最舊收幾版」，每個產品各寫各的比較與錯誤碼。
3. 邊界：新增一個 Module；Kernel、編排層的公開介面不變。
4. 影響：`config/modules.csv`、新 Module、新測試、說明頁。
5. 依賴方向：`save_version` → `GYO::Orchestration`（登記單位的介面、`OrchestrationError`）；沒有反向。
6. Ownership：版本的策略（兩個數字、比較、碼）歸這個 Module；版本欄位在存檔的位置與格式歸作者；拒絕之後怎麼辦（選單、開新遊戲）歸作者。
7. 為什麼不能更小：使用者要「Engine 需要支持」；一個不登記任何 World 內容的 Module 已是最小的形狀。

### 19.8 Design Invariants（寫進新頁 `docs/architecture/target/v2/save-version.zh-Hant.md`）

- **SV-1 只比數字。** Module 不讀、不寫存檔，不定格式；只收作者交來的整數。
- **SV-2 預設不帶、啟用也零每幀成本。** `default=no`；啟用時只在 `Configure` 放一個 `Policy` 進 Product 的共用處，不登記任何 World 的元件、Resource、System。
- **SV-3 拒絕是 `Result`。** 不發事件、不寫日誌、不中斷 World；碼固定兩個。
- **Replacement Criteria**：
  - RC-SV1：有點名的 Feature 需要 Engine 讀出版本（19.4 第 2 件的 (b)）或自動攔（(c)）。
  - RC-SV2：有點名的 Feature 需要轉換舊存檔（使用者提過「或者提供存檔轉換功能」）。
  - RC-SV3：職責要多一項（例：記住每個存檔的版本）→ 停下來交使用者。

