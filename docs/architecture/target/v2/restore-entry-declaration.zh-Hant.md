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
- **修訂 12（2026-11-22，草案，等動工前核准）**：實作卡 `gyo2-restore-impl` 開工前照現在的 `origin/gyo2`（879350d4，第 4 批、生成世界 #272 已合併）逐條對照。大部分已被骨架、生成世界兩批做掉；剩下的範圍縮小，判定改成「先用既有測試跑突變，存活的才補案例」。全文在第 18 節；第 2、5.8、5.7、13 節被第 18 節取代的地方以第 18 節為準，原文保留不改（核准的是修訂 11）。
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

讀的版本：`origin/gyo2` 879350d4（第 4 批 #259、#265，生成世界 #272，N13 #271 已合併）；交換點 #270（`claude/gyo2-exchange-81ea9f` 7b4cf2e9，未合併）；asset 宣告（`claude/gyo2-asset-585196`）；狀態機修訂 8 的卡（`gyo2-state-machine-r8`）還沒有 commit。

**一句話**：這份宣告要的東西，骨架與生成世界兩批已經做掉大半（讀檔放在新 World 的生成世界、移下界的時機、遊戲中讀檔、讀兩次相同、新 World 的紀錄從空開始，都有案例）。剩下的是：一個沒有任何測試的寫法（重新配號）、說明頁的讀檔一節、照現在的程式重跑判定 2 的突變。

### 18.1 逐條對照

**A. 仍成立（照做，不改）**

| 節 | 內容 | 現況的根據 |
|---|---|---|
| 4 | 一律新 World；讀檔是生成世界裡作者的 System | O-8、O-14（`orchestration.zh-Hant.md`）；第 15 節已核准 |
| 5.1 | 編號兩種寫法；Kernel 的兩點（範圍外的 `SpawnWithId` 照常、範圍內的被丟並留紀錄） | DI-KS 15 (7)（生成世界改字）；`World.cpp` 的 `IdInAllotterRange` |
| 5.2 | 讀檔不受寫者規則管 | 第 4 批拿掉寫者的數量 |
| 5.3 | hook 照常觸發（Bevy、flecs） | 讀檔走一般的 `Spawn`／`SpawnWithId`，Kernel 不認得讀檔 |
| 5.4 | 放回的格子取套用那個同步點的 stamp | 同上 |
| 5.6 | 新 World 的紀錄從空開始 | `RestoreFlowTests` case 3 `[or-restore-new-records]` |
| 6 | 讀檔不帶回滾的歷史 | 回滾的狀態在 World 的 Resource 與每個 World 一份的 `ModuleBuffer`（`Setup.hpp:142`），替換時隨新的登記重建 |
| 10、11 | Delta、Replacement Criteria | 不變 |

**B. 已被別批做掉（這一批不再做，只在說明頁引用）**

| 節 | 宣告要的 | 誰做的、在哪裡 |
|---|---|---|
| 5.1 編排層半 | 建 World 後、第一次 Run 前設配發器的時機 | 骨架：O-7 準備時機（帶過去 → 登記單位的準備 → 描述的準備 → `extra`）；生成世界：`entity_ids` 只在沒有值時放預設範圍，`extra` 蓋得掉（EI-1～EI-3） |
| 5.7、第 8 節 | 執行中丟掉 World、建新的；遊戲初始化只跑一次；帶過去 | 骨架：`ReplaceWorld`（O-8）、O-1、O-10；`RestoreFlowTests` case 3、6 |
| 5.7、第 8 節 | 讀檔 System 掛在生成世界；分批時第一批前設好配發器 | 生成世界：`WhileGenerating()`、`Hold()`；`WorldGenTests` C1～C3、C8 |
| 5.5 | 時間 Module 的 Resource 能被讀檔放回 | 時間＋事件：`RestoreFixedTime`（`FixedStep.hpp:92`、WT-3：在準備時機放回）；情境測試第二段 `[sc-s2-8-fixed-put-back]` |
| 第 9 節 | 主會話持有的兩句 | 已改（gyo2-time-design c47769b1） |
| DI-KS 15 (7) 的字面 | 「範圍（含下界）」 | 生成世界合併前核准的最終文字（「設定配發器的值」；「範圍的下界」是搬出去的型別的概念） |
| 判定 1 | 案例 1、3、5、9、10 | 編排層與情境測試已有同一件事的案例（18.3 的表） |
| 判定 1 | 案例 4 (a)(b)(c) | Kernel 的 J-55（`SpawnRejectTests.cpp`：「one emitter spawns one id twice」「two emitters spawn one id」「an alive id」）——讀檔就是一般的 `SpawnWithId`，同一條規則 |

**C. 前提變了，要改**

| # | 宣告寫的 | 現在 | 修訂（卡片照 L17 判，標「交使用者」的除外） |
|---|---|---|---|
| C-1 | 5.8：System 放不進不存在的 Resource，預設寫法是「編排層先放預設值、讀檔 System 用 `ResMut` 覆寫」 | 已經有：`Commands::SetResource`（同步點的 resource 那一步套用，DI-KS 9）與 `ExclusiveWorld::SetResource`（當場，DI-KS 18）；說明頁 11.6 已寫 | 5.8 改成三種都可以：準備函式放預設值＋`ResMut` 覆寫、讀檔 System 用 `Commands::SetResource`、獨占的讀檔 System 用 `ExclusiveWorld::SetResource`。和 Bevy 的 `Commands::insert_resource` 相同。不加介面（本來就不加），只改說明 |
| C-2 | 第 2 節：配發器、F-13 留在 Kernel | `RangeAllotter` 搬到 `entity_ids` Module（預設帶）；Kernel 只留 `EntityAllotter` 的概念與丟棄規則；Kernel 與編排層的測試用替身 `tests/common/support/TestRangeAllotter.hpp` | 說明頁寫 `EntityIds::RangeAllotter`；判定 2 的四個配發器突變照生成世界第 14 節第 4 件（使用者已核准「改替身」）改替身 |
| C-3 | 第 3 節、第 14 節：判定在現在的程式上會被 D-E2、D-E12 擋住 | 第 4 批後都不在 | 刪掉這兩句的效力（歷史保留） |
| C-4 | 生成世界 5.7 留給本批：讀檔要不要用 `ExclusiveWorld::Restore` | `Restore` 是回滾的原語，Kernel 不認得用途（DI-KS 12）；不補 `InitialValue`（#259、#265 的 `Detail::Derive` 分開）、全部放回後才跑 hook、範圍內的 id 也復活 | **不當預設的路，說明頁寫成可以用**、列三個差別：(1) 存檔沒存的衍生元件（`InitialValue` 補的）不會補，存檔要存齊；(2) 不看配發器的範圍，之後的 `Spawn` 要先移下界或放回配發器，否則 `AllottedIdAlive`；(3) hook 看到的是放回完的實體。系譜：Bevy `DynamicWorld`（`dynamic_world.rs:95-106`）與 Unity `SerializeUtility.DeserializeWorld`（`SerializeUtility.cs:125-176`，收空 World、配新實體再 remap 參照）都是重新配號＋對照表，對應 GYO 的第二種寫法；照原編號放回的先例是 flecs（`ecs_make_alive` 可指定編號，生成世界宣告第 4 節的表）。`Restore` 是回滾的原語、整批放回，別家讀檔沒有用回滾原語的先例，所以只寫成「可以用」、不當預設 |
| C-5 | 5.3 只寫 hook | 第 4 批後 `InitialValue` 只在一般的生成與插入補（`SpawnWithId` 帶的 bundle 已有的元件不蓋，`InitialValueTests`「a component the entity already has is left as it is」） | 說明頁補一句：照一般寫法讀檔時，存檔沒存的衍生元件由初始值的宣告補；存了的照存檔的值 |
| C-6 | 沒寫 asset | asset 宣告：資產在 World 外，World 只持有 id | 說明頁補：存檔存資產 id、不存資產本身；id 能不能用在發替換之前驗（生成世界 5.4、5.9 已寫，引用） |
| C-7 | 第 13 節判定 1：新開 `tests/common/kernel/RestoreEntryTests.cpp`、十個案例 | 十個案例裡八個已有同一件事的案例（18.3）。照 AGENTS §14.5（只為既有的抓不到的誤り加）與「不堆屎山」，再寫一份是重複 | **交使用者（改已核准的判定）**：見 18.2 |
| C-8 | 狀態機 | 狀態機修訂 8 的卡在做；舊宣告 3.8「第一次套用一律先進初始值」。讀檔放回狀態時，新 World 會先進初始狀態、再轉到存檔的狀態（初始狀態的 OnEnter 會跑一次） | 不在本批定。經主會話轉狀態機卡：讀檔時狀態怎麼放回（例：在準備函式放好、第一次套用就進存檔的狀態；或照常先進初始狀態）由那一批定，本批說明頁只指路 |

### 18.2 交使用者的一件：判定 1 改成「先跑突變，存活的才補案例」

**遊戲例子**：「讀檔時同一個編號出現兩次，留第一筆」——這條規則 Kernel 已經有測試（J-55）守著，讀檔只是用同一個 `SpawnWithId`。宣告原本要在 `RestoreEntryTests.cpp` 再寫一次，只是換成讀檔的名字。

| 選項 | 內容 | 代價 |
|---|---|---|
| **改（建議）** | 不新開 Kernel 的讀檔測試檔。實作第一步：把判定 2 的十二個突變照現在的位置改寫，用**既有**的測試跑；被抓到的記下是哪個案例抓的（預期失敗寫成那個案例的標記）；存活的才補案例，補在最接近的既有檔（Kernel 的機制補在 Kernel 的檔、讀檔的寫法補在 `RestoreFlowTests.cpp`）。另外一定新增一案：**重新配號**（宣告案例 2；兩種寫法之一，現在沒有任何案例） | 判定的形狀和核准的不同；檢出力的證據改成「哪個既有案例抓到」 |
| 照原判定 | 新開 `RestoreEntryTests.cpp`，十個案例照寫 | 八個和既有案例重複；突變被兩處抓到，不增加檢出力（§14.5） |

這不是放寬：每個突變仍要被抓到（抓不到就補案例），只是不為了換名字重寫已有的案例。結果出來之前宣告，結果出來之後不改（§14.1）。

### 18.3 判定 1 的十個案例對照（實作第一步用突變確認，表是預期）

| 案例 | 預期由誰守 |
|---|---|
| 1 照存檔編號放回 | `RestoreFlowTests` case 1（`[or-restore-ids]`、`[or-restore-ref]`、`[or-restore-range]`、`[or-restore-nothing-dropped]`）；`WorldGenTests` C1；情境第一段 S1-7 |
| 2 重新配號 | **沒有** → 新增一案到 `RestoreFlowTests.cpp`（替身配發器：存檔的三個實體用 `Spawn` 放回、編號 ≥ 存檔的下一號、大小順序和存檔相同、`ChaseTarget` 改寫後指向新編號） |
| 3 範圍沒設好 | `RestoreFlowTests` case 2（`[or-restore-control]`）；`entity_ids` 的「with the default allotter, an id in its range is dropped」 |
| 4 同一編號兩筆 | J-55 三案（18.1 B） |
| 5 Resource | `RestoreFlowTests` case 1、3（`[or-restore-gold]`）；`Commands::SetResource` 由 `ImmediateChangeTests`「Commands::SetResource and RemoveResource are applied at the sync point's resource step」 |
| 6 hook（OnAdd 一次、hook 在讀檔中生成血條） | Kernel 的 hook 與清空輪的案例（`HookFlushTests` J-54、`ImmediateChangeTests`「what the hooks of an immediate change record is applied before it returns」）；突變存活就補 |
| 7 stamp | J-30「Added sees new cells only」；突變存活就補 |
| 8 先生成再插入 | 突變「插入排在生成之前套用」跑既有測試確認；存活就補 |
| 9 新 World 隔離 | `RestoreFlowTests` case 3 |
| 10 讀兩次相同 | `RestoreFlowTests` case 5；`WorldGenTests` C3 |

判定 2 的突變照現在的位置：配發器四個改 `tests/common/support/TestRangeAllotter.hpp`（使用者已核准「改替身」）；「spawn 階段拿掉落在範圍的檢查」已有 `sr-range-*`（`docs/architecture/plans/kernel/scripts/mutations.json`），照 §14.5 不另立、記下沿用哪一個；其餘照原表在 `World.cpp`、`Params.hpp`。

### 18.4 說明頁

`system-contract-page.zh-Hant.md` 第 14 節的「讀檔」一段移成獨立一節（第 15 節「讀檔」），一處寫完、第 14 節與第 13 節的替換那一條改成指過去：遊戲例子、新 World、生成世界、兩種寫法（含重新配號的對照表寫法）、Resource 的三種放法（C-1）、hook 與衍生實體（hook 生成的若也存進存檔，讀檔後兩份）、初始值（C-5）、stamp、時間（引用 WT-3）、asset id（C-6）、`Restore` 的三個差別（C-4）、狀態機指路（C-8）。

### 18.5 時間、角色

實作約 1.5～2 小時（突變 0.5、一案 0.3、說明頁 0.5、L1 0.5）。角色照第 12 節：實作 Opus／high；突變清單與修訂 12 的對抗檢查 Fable／high；合併前設計審查 Fable／high（唯讀）。
