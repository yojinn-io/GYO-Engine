# 事前宣告：玩家存檔的讀檔——Engine 預設支持的路（`v8-arch-gyo2-restore-entry`）

**修訂紀錄**
- 修訂 0（3dc9b464）起草；修訂 1（ea62f0c4）照第一次換模型檢查（Fable／high，blocker 3、should 17、nit 7）重寫。
- 修訂 2～8（2026-11-16～17，e6466c2d～ed742598）：使用者在本卡與邊界卡陸續定了新 World、編號、「不認得存檔只指格式」、Kernel 是 ECS、初始值在建立當下給、權威不數寫者、回滾是獨立 Module；每次照著縮小與改寫。
- 修訂 9（4ad66708、47123a6c）：工作方式改成卡片照 L17 自己判（使用者 2026-11-17：「什麼都要我來定，乾脆代碼我也自己寫好了。名字你有命名規範。例子你有源碼。」），判了 5.3～5.7。
- **修訂 10（2026-11-17）**：照第二次換模型檢查（Fable／high，blocker 3、should 11、nit 8，全部採用；報告 `build/evidence/gyo2-restore-entry/declaration-check-fable-r9.md`）整份重寫。最大的三處：判定改寫成「實作時的 Engine」（批次表第五段，Kernel 減法、編排層、時間 Module、權威與回滾都已做完）；讀檔放在**編排層的「生成世界」**（使用者 `two-layers-like-bevy`），修訂 6～9 的「建世界的階段整件拿掉」是錯的（拿掉的只有 Kernel 那一層）；5.5 照 `clock-out-of-kernel` 重判。逐條對照在第 16 節。

- Owner：架構（GYO2 主會話開的任務卡）。這一批只做到宣告核准為止，**不實作**。實作是批次表第五段「讀檔的實作」（`rebuild-batch-plan.zh-Hant.md`）。
- 讀的版本：程式 `origin/gyo2` 15ba81ef（K0、測試舊帳已合併）；批次表與 HANDOFF 在 `claude/gyo2-time-design` def2849b；統合（邊界卡）的宣告；回滾份 `claude/gyo2-rollback-module-8bac02`（第 7.4 節）。
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
| `v8-arch-gyo2-build-world-phase-is-mechanism`、`-loading-is-author-choice` | 開局、Loading、讀檔是遊戲流程的名字；Loading 有沒有由作者決定。原話含「讀檔應該是和生成世界是同一級別呀」「正常這些邏輯都應該寫在Startup裡的」（全文見 HANDOFF） | 本卡 |
| `v8-arch-gyo2-two-layers-like-bevy` | 照 Bevy：**編排層認得遊戲初始化與生成世界的階段、排流程；Kernel 不認得**。原話「用Bevy的方式走，」 | 邊界卡 |
| `v8-arch-gyo2-kernel-is-ecs-no-restrictions`、`-init-values-like-bevy`、`-authority-no-writer-count` | Kernel 是 ECS、不疊加限制；初始值在建立當下給；伺服器權威不數寫者，判定資料的讀寫規則拿掉（統合 15.3） | 邊界卡 |
| `v8-arch-gyo2-clock-out-of-kernel` | 時鐘與 tick 移到時間的 Module（作者可選）；「tick 只有一個來源、System 不得寫」歸伺服器權威策略 | 邊界卡 |
| `v8-arch-gyo2-rollback-module-strategy`、`-strategy-interface-impl` | 回滾是獨立的 Module，Kernel 只給原語 | 邊界卡 |
| `v8-arch-gyo2-layer-duty-lists`（L17）、`v8-arch-gyo2-engine-not-rigid` | 每層一份職責清單，清單之外由 agent 照同類型多家實作判；某層要多一項職責或判不出哪一層才停下問。不把規則釘死 | 邊界卡 |

標「卡片照 L17 判」的件都不是使用者的話。

---

## 1. 這一批做什麼

### 1.1 用遊戲講

RPG：玩家在第 3 關存檔，身上有 3 瓶藥水、一把鑰匙、金幣 120，任務「救出村長」做到第 2 步，觸發過「拉桿」「Boss 已打倒」。隔天讀檔（或遊戲中讀另一個存檔）：編排層建一個新的 World，跑遊戲初始化，再進生成世界；在生成世界裡，作者的程式照第 3 關的資產生成地圖、怪物、寶箱，作者的讀檔 System 放回物品、任務進度、觸發條件；關卡裡的東西照觸發條件決定自己的樣子。卡牌遊戲存牌組；另一個作者存了每一隻怪物——走同一條路。Engine 不認得任何一種存檔格式。

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
| 編排層 | 建 World、跑遊戲初始化清單、生成世界（每個 World 各自、可以多次、可以分批）；runner | 批次表第 2 段、第 5 段「生成世界（重做）」；`two-layers-like-bevy` |
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

---

## 4. 讀檔在哪個 World、哪一層

- **新的 World**（使用者已定）：遊戲中讀檔＝編排層丟掉現在的 World、建新的。兩種讀檔結果相同。
- **編排層的「生成世界」**（**卡片照 L17 與 `two-layers-like-bevy` 判，不是使用者的話**）：讀檔是生成世界裡作者的一個（或幾個）System，和「照資產生成關卡」同一級別——這是使用者 11-16「讀檔應該是和生成世界是同一級別呀」的字面。Kernel 不認得生成世界，也不認得讀檔。
  - 系譜：Bevy 讀檔與放場景在每幀的 `SpawnScene` schedule（`crates/bevy_scene/src/lib.rs:95-104`），由 `bevy_app` 排位置；Unity 的 `SerializeUtility.DeserializeWorld` 只收完全空的 World，訊息要作者「create a new empty World」（`Unity.Entities/Serialization/SerializeUtility.cs:1046、1054`，第二次檢查讀的）——「一律新 World」的獨立先例。
  - 為什麼不是「遊戲初始化」清單：生成世界每個 World 各自、可以多次、可以分批（批次表第 5 段），大存檔與讀取畫面要分批（使用者 11-11 定 DI 3 的理由）；遊戲初始化不分批。
  - 使用者 11-16 也說過「正常這些邏輯都應該寫在Startup裡的」。這句和「放生成世界」是否一致，列進第 15 節的確認清單。

---

## 5. 各件

### 5.1 編號（使用者已定：兩種寫法由作者選）

照存檔編號放回（存檔記「下一個號」，建 World 後、第一次 Run 前把配發器範圍訂在它之後，用 `SpawnWithId`），或重新配號（`Spawn`＋作者的對照表與參照改寫）。

Engine 的兩點保證分在兩層：
- **Kernel**：World 在第一次 Run 前可以設配發器的範圍；範圍外的 `SpawnWithId` 照常生成、範圍內的被丟並留紀錄。Kernel 的測試守（判定 1 案例 1～3）。
- **編排層**：建 World 之後、第一次 Run 之前，給作者（讀檔的程式）一個設配發器範圍的時機。**這是給「編排層的骨架」與「生成世界（重做）」兩批的要求**；Kernel 的測試守不到，那兩批的判定要有案例。

### 5.2 寫者（使用者的決定已回答）

實作時 Kernel 沒有寫者的數量（`authority-no-writer-count`、批次表第 4 段），讀檔不受任何寫者規則管：一次帶完整 bundle 生成、先生成再插入、放進或覆寫 Resource，都不數寫者；用 `Spawn` 的讀檔 System 和玩法 System 同時寫配發器也不報錯。不需要漏斗、例外或特別的階段。

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

修訂 9 寫「Engine 不提供設 tick 的入口（tick 只有一個來源）」「Bevy 讀檔不帶時間」，兩句都錯：tick 已移到時間的 Module，「只有一個來源」歸伺服器權威策略（作者自選）；Bevy 的 `Time<T>` 是可以存讀的 Resource（`crates/bevy_time/src/time.rs:192-193` 的 `reflect(Resource, Default)`），`DynamicWorldBuilder::extract_resources` 會抽出 Resource（`crates/bevy_world_serialization/src/dynamic_world_builder.rs:373`），`advance_to` 公開（`time.rs:246`）；Unity 的 `World.SetTime` 公開（邊界卡 `clock-other-engines.md`）。

**判定**：時間與 tick 是時間 Module 的 Resource，讀檔這一批不給它特別規則。新 World 的時間從時間 Module 的初始值開始；作者想讓讀檔後接著存檔時的時間，就把那些 Resource 存進存檔、讀檔時放回——Engine 不禁止。選了伺服器權威策略的 World，照那個策略的規則。時間 Module 的 Resource 能不能被讀檔放回（例如要不要公開設值的入口）是時間 Module 那一批的事，記成給它的要求。

### 5.6 診斷紀錄（卡片照 L17 判）

新 World 的紀錄從空的開始；讀檔時被丟的生成（存檔裡同一個編號兩筆、編號落在配發器範圍）照常記。`rollbackActive` 已拿掉（統合修訂 17）。紀錄的形狀（統合 L8）由那一批定。

### 5.7 預設支持包含什麼（卡片照 L17 判）

- **說明頁**：`docs/architecture/target/v2/system-contract-page.zh-Hant.md` 新增一節「讀檔」（遊戲例子、新 World、生成世界、編號兩種寫法、hook、時間、hook 衍生實體不進存檔的提醒）。
- **Kernel 的測試**：`tests/common/kernel/RestoreEntryTests.cpp`（第 13 節），守 Kernel 那一半。
- **給編排層兩批的要求**：建 World 後、第一次 Run 前設配發器的時機（5.1）；生成世界裡掛讀檔 System 的方式；它們的判定要有讀檔的案例。
- **給時間 Module 那一批的要求**：5.5。
- **不改 Kernel 的條文**：修訂 9 寫「已在 DI-KS 4、9」不對——「Run 前可設配發器」沒有條文。這一點是編排層與 Kernel 之間的契約，請主會話在統合或編排層骨架的宣告裡收（統合修訂 31 沒有收修訂 9 的請求，這次再交一次）。Bevy、flecs 都沒有對應的條文（讀檔是應用層的用法）。

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
| 第 2 段 編排層的骨架 | 要求：建 World 後、第一次 Run 前讓作者設配發器的時機；它的判定要有案例 |
| 第 3 段 時間 Module＋事件 | 要求：5.5（時間的 Resource 能被讀檔放回）；讀檔的 System 要發訊息時用 `Message`／`Event` |
| 第 4 段 權威＋回滾 | 拿掉寫者的數量之後，第 3 節那兩列 D-E2 不再出現 |
| 第 5 段 生成世界（重做） | 要求：讀檔 System 掛在生成世界裡；可以分批時，照存檔編號放回的作者要在第一批之前設好配發器 |
| 第 5 段 讀檔的實作 | 本宣告。依賴第 2 段（批次表）與第 4 段（寫者的數量拿掉）、第 5 段的生成世界（掛點） |

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

測試扮演編排層：建 World、在第一次 Run 前用 `SetResource` 設配發器與暫存的存檔資料、跑一張叫 `Load` 的 schedule（作者的名字；實作時照生成世界那一批定的掛法改）。案例名在實作前換成實際的 `TEST_CASE` 名。

### 判定 1：行為（`tests/common/kernel/RestoreEntryTests.cpp`）

1. **照存檔編號放回**：存檔有玩家 12、怪物 57、58（57 的 `ChaseTarget` 是 12），下一個號 900；配發器 `[900, 上限]`；`Load` 用 `SpawnWithId` 帶完整 bundle（含 `Position`）放回；之後另一張 schedule 生成一個選單實體。讀檔後編號是 12、57、58，值與存檔相同；`ChaseTarget` 仍是 12；選單實體的編號 ≥ 900；沒有被丟的生成。
2. **重新配號**：同一份存檔，`Load` 依存檔編號由小到大用 `Spawn`；三個實體的編號 ≥ 900、大小順序和存檔編號相同；`ChaseTarget` 由 `Load` 改寫後指向玩家的新編號。
3. **範圍沒設好**：配發器 `[1, 上限]`，照存檔編號 `SpawnWithId`，每一筆以 `IdInAllotterRange` 被丟並記紀錄。
4. **同一個編號兩筆**：存檔裡編號 57 出現兩次（`Load` 依序記），留第一筆、第二筆以 `IdSpawnedTwice` 被丟；57 的值是第一筆的。
5. **Resource**：`Load` 把金幣放進 World（之前沒有）→ 讀檔後是存檔值；另一案先放預設 0、`Load` 覆寫 → 是存檔值。
6. **hook**：每個放回的格子 OnAdd 各一次；`Monster` 的 OnAdd 在讀檔中生成血條實體，讀檔完成後每隻怪物各有一個血條、沒有錯誤（Bevy #14300 的同型）。
7. **stamp**：讀檔後第一次跑的讀者，`Added<Position>` 對放回的格子成立；同一個讀者再跑一次，不成立。
8. **先生成再插入**：`Load` 先生成只帶標記的實體、同一個 System 再插入 `Position`；讀檔後 `Position` 在、值正確。
9. **新 World 隔離**：第一個 World 跑過、有實體與被丟的紀錄；第二個 World 讀檔，只有讀檔放回的與它自己生成的實體，被丟的紀錄從空的開始。
10. **讀兩次相同**：同一份存檔在兩個新 World 讀，`Summary()` 相同。

案例 9、10 守的是「新 World」這份契約的後果，靠 World 的建構本身，沒有對應的突變（寫明，不算進檢出力）。

### 判定 2：檢出力（AGENTS §14.2；位置照實作時的程式）

| 突變 | 預期失敗 |
|---|---|
| `RangeAllotter::Owns` 永遠真 | 案例 1（12 以 `IdInAllotterRange` 被丟） |
| `RangeAllotter::Owns` 永遠假 | 案例 3 |
| spawn 階段拿掉「落在範圍」的檢查 | 案例 3 |
| `RangeAllotter::Of` 忽略 `first`（從 0 配） | 案例 1（選單實體的編號）、案例 2 |
| `RangeAllotter::Allot` 不前進 | 案例 2（同一個編號生成兩次被丟） |
| 同一個記錄者的同一個編號留最後一筆（不是第一筆） | 案例 4（57 的值） |
| spawn 階段 bundle 帶來的格子不跑 OnAdd | 案例 6（OnAdd 次數） |
| 套用中 hook 記的生成被丟掉 | 案例 6（血條） |
| 放回的格子 `added` 記成 0 | 案例 7（第一次跑不成立） |
| 插入排在生成之前套用 | 案例 8（插入落空） |
| Resource 的放進或覆寫不套用 | 案例 5 |

突變清單在實作前交給和實作者不同模型的檢查者做一次對抗檢查（AGENTS §14.2）。

### 判定 3：沒有回歸

`core` preset 的 kernel 測試全過；L1 三平台全過。

---

## 14. 範圍的限界

- 只寫預設的路在 Kernel 這一側的行為與給其他批的要求；存檔格式、存什麼、IO、讀檔流程、生成世界的形狀與分批、重建 World 的驅動都不在這一批。
- 判定寫在第五段的 Engine 上；在那之前的程式上案例會因寫者的數量（D-E2）而不過，不試著在現在的程式上跑。
- 編排層那一半的保證（5.1、5.7）不在 Kernel 的測試裡，交編排層兩批。
- 「在現在的 World 裡讀」不是預設的路；作者自己做時會碰到的事（修訂 1 第 6 節：舊狀態殘留、OnRemove hook 的變更、事件、巢狀）這一批不保證。
- 參考實作：Bevy、replicon、lightyear 讀本機原始碼；flecs 09790b5f7、Unity Entities 4630ff5 由第二次檢查讀本機原始碼；Unreal Mass 沒有讀原始碼；bevy_ggrs 沒有讀到。
- 案例 2 的「順序和存檔編號相同」只在配發器單調時成立（`RangeAllotter` 是；產品自訂的配發器不保證，E2 沒有判）。

---

## 15. 確認清單（只有一件；會不會改寫使用者較早的話，卡片拿不準）

**讀檔放在編排層的「生成世界」，不是「遊戲初始化」清單。**
- 使用者 11-16 在這張卡說：「讀檔應該是和生成世界是同一級別呀」，也說「正常這些邏輯都應該寫在Startup裡的」。11-17 使用者在邊界卡定了「編排層認得遊戲初始化與生成世界的階段」（`two-layers-like-bevy`）。
- 卡片的判定：放生成世界——合第一句的字面；生成世界每個 World 各自、可以多次、可以分批，大存檔與讀取畫面做得到（使用者 11-11 定 DI 3 的理由）；Bevy 讀檔也在每幀的 `SpawnScene`，不在開局。
- 拿不準的地方：第二句的「Startup」，當時講的是「開局、Loading、讀檔是遊戲流程的名字，機制只有 Startup」；照 11-17 的兩層，那時的「Startup」對應的可能是編排層的遊戲初始化，也可能泛指「遊戲開始跑之前」（生成世界也算）。
- 例子：RPG 玩家按「繼續」→ 編排層建新 World → 遊戲初始化（放固定的設定）→ 生成世界（照資產生成第 3 關、讀檔 System 放回物品與任務）→ 進遊戲迴圈。放生成世界時，讀取畫面可以在放回的過程中動；放遊戲初始化時，讀檔要一次做完。

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
