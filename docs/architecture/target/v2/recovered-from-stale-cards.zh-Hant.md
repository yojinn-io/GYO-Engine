# 從過時的任務卡回收的資源（2026-11-17）

Owner：架構（GYO2 主會話）。使用者 2026-11-17：「現在把回收哪些任務的可用資源，然後歸檔，注意過時設計不要帶進來」。

2026-11-16～17 使用者定了幾個改變前提的方向：Kernel 是 ECS、不疊加限制（`v8-arch-gyo2-kernel-is-ecs-no-restrictions`）；Kernel 之外另立編排層（`v8-arch-gyo2-orchestration-layer-name`）；「開局」拆成遊戲初始化（編排層的清單）與生成世界（`v8-arch-gyo2-game-init-list`）；權威檢查是策略式的 Module（`v8-arch-gyo2-authority-is-game-design`、`-authority-strategy-mechanism`）；Module／Plugin 改定義（`v8-arch-gyo2-module-plugin-owner-definition`）。這些都還在邊界的統合宣告裡收斂（任務卡 gyo2-kernel-app-boundary，統合 `kernel-app-boundary-declaration.zh-Hant.md`）。

這一頁列出：建立在舊前提上的任務卡裡，**哪些還能用**、**哪些不要帶進新設計**，以及舊材料在哪裡。之後重開這些題目時，從這一頁與 HANDOFF 的決定表開始，不要從舊卡的宣告或舊卡的說明開始。

判準：**遊戲看得到的行為與使用者的需求**（作者寫什麼、玩家看到什麼）照舊有效；**靠 Kernel 舊限制或舊分層才成立的做法與理由**不帶進來，重開時重新問使用者。

---

## 1. 狀態機（任務卡 gyo2-state-machine，已歸檔）

**舊材料**（只當歷史，不照做）：分支 `claude/gyo2-state-machine-520d19` commit 25c3949b 的 `state-machine-declaration.zh-Hant.md`（修訂 7，2026-11-16 使用者核准，但核准時的前提已變）。檢查報告與偵察在主 checkout `build/evidence/gyo2-state-machine/`。

### 1.1 還能用的（遊戲看得到的行為、使用者的需求）

| 內容 | HANDOFF 代號 |
|---|---|
| 點名的 Feature：啟動後進主選單、從主選單進關卡、暫停時不跑遊戲邏輯、遊戲中的全局狀態（天氣、時段、地圖的全局事件） | `-state-machine-premises-confirmed`、`-state-machine-every-contract-level` |
| 狀態機支援各等級的契約：本機的狀態與兩端要一致的狀態都要支援；兩端一致的那一種，「權威端換、Client 採用」與「兩端同一個 tick 自己換」都要 | `-state-machine-every-contract-level`、`-state-machine-zone-c-both-shapes` |
| 一次套用的順序：先跑離開（讀到舊值）、再改值、再跑進入 | `-state-machine-exit-reads-old-value` |
| 暫停是「關卡中」的子狀態；離開由子到父、進入由父到子；同一個父底下的子照登記的順序；子不存在時對它的要求丟掉；子每次出現從初始值開始 | `-state-machine-pause-as-substate`、`-state-machine-remaining-eight` 一、三 |
| 父換成同一個值時默認重設子，作者可以改成保留 | `-state-machine-silent-request-and-child-reset` 二 |
| 第一次套用一律先進初始值；之前提出的要求留到下一次 | `-state-machine-first-apply-enters-initial` |
| 一次只換一次；同一幀後寫蓋前寫，被蓋掉的留計數；換成同一個值照樣跑離開與進入；每個值一張進入／離開的 schedule；條件叫 `InState`／`NotInState` | `-state-machine-bevy-comparison-answers`、`-state-machine-overwritten-request-counted`、`-state-machine-named-state-machine` |
| 名字：State Machine；名字表是作者的列舉加特化，寫在產品端 | `-state-machine-named-state-machine`、`-state-machine-names-by-enum-and-traits` |
| 狀態機不碰時間：暫停要不要停時間由作者決定；兩棵互不相干的狀態之間的先後不當規則承諾 | `-state-machine-remaining-eight` 二 |
| 狀態機只是機制：不帶世代與換的 tick；快照蓋掉值時跑不跑離開與進入由遊戲決定；AI 的狀態機不用這一層 | `-state-machine-remaining-eight` 五、七、八 |
| 加需求（例如按鈕改天氣）不該要改狀態機：作者隨時可以經「要求」換值 | `-state-machine-silent-request-and-child-reset` 一的使用者原話 |
| 放在 Engine 的 Module（新定義下仍是 Module：我們自己維護） | `-state-machine-bevy-comparison-answers`、`-module-plugin-owner-definition` |

### 1.2 不要帶進來的（靠舊前提才成立）

- **`RequestSilent` 存在的理由**：「直接蓋值做不出來」是因為型別封閉、每欄一個寫者（D-E2）、區規則（Z2）同時成立。限制移出 Kernel 之後，「要不要分兩種要求」要重新問使用者；「按鈕改天氣不用改狀態機」這個需求照舊。
- **(c) 區要等 Kernel 的「(c) 區獨占套用」**（`v8-arch-gyo2-zone-c-exclusive-apply`）：建立在 S-13「獨占的 System 只能是 (b) 區」上，那條限制在重新評估；這一批可能不需要或形狀不同。
- **登記時選 `Level::C` 與 `Zone::C` 的 static_assert**：區（zone）是 Kernel 的限制，要移出；「等級」改由權威策略怎麼表達，等權威 Module 那一份。
- **「開局之後才進初始狀態」的接法**：開局已改成編排層的清單，狀態機自己插進清單或維持在之後，重開時定。
- **套用的 System 由登記函式自己插一個 stage（`InsertStage`）**：stage 與組裝屬編排層還是 Kernel，等統合；需求「不會漏掛」照舊。
- **以 D-E2、R-E2 當守衛的 Design Invariants**（宣告第 8.2 節第 1、7 條）：守衛要換。
- **舊的批次位置「排在 T3 之後」**：T2、T3、T5 要照新分層重排。
- 卡片自己列的 25 處依賴舊限制的段落：主 checkout `build/evidence/gyo2-state-machine/rules-to-recheck.md`（原本放在 app 的 side-session-notes，已搬來）。

---

## 2. 重新評估 Loading（任務卡 gyo2-build-world-phase，已歸檔）

**舊材料**：主 checkout `build/evidence/gyo2-build-world-phase/inventory-and-premises.md`（盤點、前提表、使用者 11-11 的原話、問過還沒答的五題）。這張卡沒有寫宣告、沒有開分支。

### 2.1 還能用的

- **盤點**：GYO2 哪裡把 Loading 當成 Engine 的概念（`Domain::Loading`、寫者檢查只排除 Startup、runtime-composition DI 3 與 3a、舊規格 H.2b）。這是之後要改的清單。
- **使用者 2026-11-11 定 Loading 時的原話與理由**（分批載入、讀取畫面；地圖→場景→物件→敵人→玩家）——需求照舊，由誰提供變了。
- **方向**：開局、Loading、讀檔是遊戲流程的名字，不是機制；Loading 有沒有由作者決定（`v8-arch-gyo2-build-world-phase-is-mechanism`）；「Engine 不認得存檔」只指格式（`v8-arch-gyo2-save-unaware-means-format-only`）。
- **已切出的範圍**：遊戲初始化歸編排層的清單；**生成世界**（每個 World 各自、可以多次、可以分批）是這一題剩下的範圍（`v8-arch-gyo2-game-init-list`）。

### 2.2 不要帶進來的

- 卡片的說明與前提表裡「Startup 由 Kernel 跑一次」「`Domain::Loading` 是 Kernel 的時鐘域」「寫者規則照舊、要豁免」的框架。
- 問過還沒答的五題裡，「Engine 與 ECS 兩層」那一題已由邊界卡處理；其餘重開時照新分層重問。

**重開時**：等邊界的統合核准，照「生成世界」的範圍開一張新卡，說明從這一頁與統合的關聯表寫起。

---

## 3. 已完成、歸檔、沒有要回收的

- **K0（gyo2-nested-schedule）**：#239 已併入 `gyo2`（cb6bccca）。宣告、dev_log、測試、突變都在 `gyo2` 上；證據在 `build/evidence/gyo2-nested-schedule/`。其中「開局由 Kernel 跑」「`ExclusiveWorld<Writes<…>>`」「獨占的 System 只能是 (b) 區」會在 Kernel 拿掉限制的那一批改，統合的關聯表已列。
- **收斂 Module／Plugin 的定義（gyo2-module-plugin-definition）**：#240 已併入 `gyo2`（f00433b0）。三件未確認（作者能不能只選部分 Module、SDL 的 adapter、ufbx）記在 HANDOFF。
- **主會話開過的兩個 side chat**（「這3條一個個來」「來一件件來」）：決定都已轉進 HANDOFF。

## 4. 沒有歸檔的（仍在用）

- **邊界（gyo2-kernel-app-boundary）**：統合，正在等使用者逐件定。
- **還原入口（gyo2-restore-entry）**：部分依賴邊界的結果（5.2、第 7 節），5.3 仍有效；分支 2026-11-17 已推上 `origin/claude/gyo2-restore-entry-a4929d`（a3caf87a）。
- **測試舊帳（gyo2-test-debts）**：#236、#238 等使用者合併。
- **權威 Module、事件游標**：提案卡，照統合寫的。
