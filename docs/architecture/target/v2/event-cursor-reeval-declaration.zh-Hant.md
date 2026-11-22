# 事件游標的重評：事前宣告（修訂 3）

> **核准**：使用者 2026-11-22 在本卡對話核准修訂 3（f7aeaa6f），原話「核准，加 RC 24，開始」。

> 性質：事前宣告。結論是**不改 Kernel 的程式與契約**，只改說明頁，另加一條 Replacement Criteria（草案）。**使用者核准、說「開始」之前不改文件。** 加 RC 是 Kernel 的 Design Invariants 節的變更，要使用者同意（AGENTS §3）。
> 批次代號：`v8-arch-gyo2-event-cursor-reeval`。Owner：架構（GYO2 主會話開的任務卡）。更新：2026-11-22。
> 起因：使用者 2026-11-22 在狀態機卡選 (A＋)——狀態機那一批照 (A) 做，另外現在就開這張卡重評（HANDOFF `v8-arch-gyo2-state-machine-r8-crosscheck`，記在 `claude/gyo2-time-design` 1e4fd1ab）。

**修訂紀錄**（各修訂的全文在 git 歷史）：
- 修訂 0（338e40a0）：建議「被條件跳過一律算讀過」＋「讀者出生就拉住」。敵對式檢查（Fable 5.2／high）blocker 3、should 7、nit 7。
- 修訂 1（d87698a8）、修訂 2（f957143b）：建議改成 (C)「run 條件宣告判否時放開讀者」，維持「跑過一次才拉住」。窄複查（Fable 5.2／high）blocker 1、should 4、nit 6，修訂 2 照改。
- **修訂 3（這一份）**：使用者要卡片先確認前提——這是 Engine 要解決的，還是作者要解決的（第 0 節）。查證的結論是作者的問題；使用者同意，改成 (A)＋說明頁。

**證據**（這台 Mac 的主 checkout，git 不追蹤）：`build/evidence/gyo2-state-machine-r8/crosscheck-message-retention.md`（五家對照）；`build/evidence/gyo2-event-cursor-reeval/` 的 `source-verification.md`（本卡重讀的行）、`declaration-check-r0-fable.md`、`declaration-check-r1-fable.md`（兩次檢查）。讀的程式：`origin/gyo2`（本卡合併到 1cf859c8 時的版本）。

---

## 0. 使用者的決定

**原話**（2026-11-22，本卡對話，依序）：
1. 卡片交修訂 2 之後：「在不違反GYO的規範前提下，有沒有解決方案？」
2. 卡片逐條對過規範之後：「另外，你先確認這個問題的前提。是Engine需要解決的，還是作者需要解決的？」
3. 卡片的結論是「作者能處理的問題，Engine 要負責的只有：把契約寫清楚，以及 Engine 自己的 Module 不要把作者帶進坑」，並問同不同意：「同意，改成 (A)＋說明頁」

**比過的選項**：(A) 不改 Kernel，說明頁寫明 ← 選／(C) run 條件宣告「判否時放開讀者」／(E) 讀者自己的選項「被跳過時放開」／(R1) 條件判否一律放開／(F) 訊息種類固定保留 N 次 Run／(B／G) 狀態轉換時清除。題目二：維持「跑過一次才拉住」← 選／讀者出生就拉住。比較表在修訂 2 第 3 節（f957143b）。

**理由**：使用者同意的是第 2 節的前提查證（卡片的話，不是使用者的原話）。

**出處**：本卡對話；已照四樣轉給主會話（2026-11-22）。

---

## 1. 三個後果（照舊，說明頁要講的）

規則（現在的，`World.cpp:373-385`、`:1042-1053`、`Executor.cpp:327-358`，契約在 kernel DI-KS 10）：每個讀者一個游標；被 run 條件跳過、或所在 schedule 沒跑，游標不動，下次補讀；第一次跑才開始拉住，第一次從還留著的最舊一筆讀起；每次最外層 Run 結束修剪到全部游標的最小值；滿了（預設 1024）丟最舊，記一筆 `DroppedMessages`。

1. **拉住保留、一直記丟棄**：主選單的「處理按鈕」帶 `InState<MainMenu>`，讀 `ButtonPressed`。進關卡後它不跑卻拉住；關卡裡寫滿 1024 筆之後，每次 Run 都記一筆 `DroppedMessages`。
2. **恢復時補讀舊的**：回到主選單那一幀，它把關卡期間的 `ButtonPressed`（上限內全部）一次當成主選單的按鈕處理。網路對戰裡暫停選單不停模擬時，帶 `InState<Playing>` 的「播命中特效」關掉選單時一次播出暫停期間的全部命中。
3. **進入那張第一次讀不到，之後讀到上次以來的全部**：`Flow.Enter.InLevel` 裡讀 `LevelSelected` 的 System，第一次進關卡讀到空的；之後每次進入讀到上次進入以來的全部。

---

## 2. 前提查證：Engine 的問題還是作者的問題

**假設**：上面的後果是 Engine 必須解決的問題。

**查證**：
1. **不改 Engine，作者處理得了嗎？**處理得了，不缺能力。寫法：讀者不帶狀態條件、每幀 `Read()`、不在那個狀態就丟掉；要帶進「進入」的資料放 Resource。現在的 Kernel 都做得到，成本是那個 System 每幀多跑一次。
2. **成熟引擎交給誰？**交給作者。Bevy 核心寫明「讀者被預期每幀至少讀一次」（`bevy_ecs/src/message/messages.rs:34`），只有狀態 Plugin 層事後補了清除工具（`bevy_state/src/state_scoped_events.rs:67-107`，#15072）；lightyear 的範例把接收器寫成不帶狀態條件、讀到就設 `NextState`（`examples/lobby/src/client.rs:429-442`）；flecs 用 `yield_existing`、Unity 用現在的值。沒有一家在核心處理。（lightyear 範例、flecs、Unity 的行照五家對照，本卡沒有重讀。）
3. **因產品類型而做不到的？**沒有。單機、PvP 的 Match 與 Client、工具都能用同樣的寫法。
4. **但坑是 Engine 的預設造成的**：同樣把 `InState` 掛在讀者上，Bevy 大致沒事（訊息會過期），GYO 會補讀，因為 GYO 刻意選了「被條件跳過要補讀」（時間方案二設計稿的賣點，`rebuild-time-design.zh-Hant.md:421`、`:432`）。Engine 自己的 Module 也在引導：生成世界要作者把生成用的 System 放進 `WhileGenerating()` 的 set，再提醒它們不要讀 `Message`（`system-contract-page.zh-Hant.md:490`）。
5. **(C) 是 Engine 替作者決定遊戲規則**：被 `InState` 擋住的讀者回來該不該補讀，是遊戲設計的判斷；讓 `InState` 一律放開，就是把它釘死在 Engine 裡。

**總結**：假設不成立。能力上作者處理得了，成熟方案都交給作者。Engine 的責任是：(1) 契約與寫法講清楚，讓作者用直覺寫法之前就知道 GYO 和 Bevy 的差別；(2) Engine 自己的 Module 與說明頁不引導作者把讀者放在會長期跳過的條件下。

---

## 3. 改什麼（只有文件）

| 檔案 | 改法 |
|---|---|
| `docs/architecture/target/v2/system-contract-page.zh-Hant.md` 第 5 節「訊息」那一點 | 加一句「被條件擋住、或不常跑的讀者會拉住、回來補讀，寫法見第 15 節」 |
| 同頁，新第 15 節「帶條件或不常跑的讀者」 | (1) 第 1 節三個後果的遊戲例；(2) 下面的「會不會拉住、補讀」表；(3) 寫法：讀者不帶狀態條件、在 System 裡判斷；要帶進「進入」的放 Resource；暫停時擋住**寫者**；進入／離開那兩張與 observer 不讀和這次無關的訊息；(4) 和 Bevy 的差別一句（Bevy 兩次 update 就過期，GYO 留到讀過或上限）；(5) 連到 kernel DI-KS 10 |
| 同頁第 14 節「生成用的 System 不讀 `Message`」 | 連到第 15 節（「要交給遊戲邏輯的結果放 Resource」照舊） |
| `docs/architecture/target/kernel.zh-Hant.md` Replacement Criteria | 加 RC 24（草案）：**帶條件的讀者要補償**——作者或 Module 為「被 run 條件長期跳過的讀者」寫補償（每幀讀了就丟只為了不拉住、在轉換時清掉、自己記上次讀到哪裡），在產品或 Module 裡反覆出現，或 Engine 的 Module 必須教作者這樣寫才能用——重評 Kernel 是否要提供（候選：本批修訂 2 的 (C)、(E)）。（本批 `v8-arch-gyo2-event-cursor-reeval`）**要你同意** |
| `docs/architecture/target/v2/event-cursor-declaration.zh-Hant.md` | 頂端加一行「第 3.2 節的讀者規則經 `v8-arch-gyo2-event-cursor-reeval` 重評，維持不改；作者的寫法在 System 契約說明頁第 15 節」。事實更正兩處（不改判斷）：第 3.2 節 lightyear 的行號現在是 `receive.rs:54`、`:105-110`、`:153-160`、`:447-456`；「Bevy 超過兩次 update 靜默丟」補上例外——用固定步時虛擬時間暫停，Bevy 全域停輪替、所有種類無上限累積（五家對照 1.1 (d)） |
| `docs/architecture/target/v2/HANDOFF.md` | 主會話記（第 0 節已轉） |
| `docs/dev_logs/` | 本批的 dev_log |

**「會不會拉住、補讀」表**（放進說明頁第 15 節）：

| 讀者的情形 | 拉住保留 | 下次補讀 | 例 |
|---|---|---|---|
| 跑了、有 `Read()` | 讀到的放開 | — | 一般情形 |
| 跑了、沒呼叫 `Read()` | 是 | 是 | System 裡提早 return |
| 被 run 條件跳過（自己的或 set 的） | 是（跑過一次之後） | 是 | 帶 `InState` 的主選單處理 |
| 所在 schedule 沒跑 | 是（跑過一次之後） | 是 | 固定步這幀跑 0 次、時間暫停、`fixedStepConditions` 擋住驅動固定步的 System、進入／離開那兩張 |
| 從沒跑過 | 否 | 第一次從還留著的最舊一筆讀起（讀到什麼取決於別的讀者有沒有拉住） | 第一次進入的那張、帶 `AfterGenerating()` 的讀者在生成期間 |

**不改的**：Kernel 的程式、DI-KS 10、測試、突變；日誌 Module 的 LG-1；狀態機用的介面。

---

## 4. 檢查與 AGENTS 的對照

- **Architecture Delta**：沒有程式、介面、Ownership 的變化。唯一碰到 Design Invariants 節的是加 RC 24（草案），要你同意。
- **守衛**：判定全部不變，沒有 §14.4 的事項。
- **AGENTS §1 的跨部件規則**（「Contract 只保證 X，所以使用者都要做 Y」要查 `docs/cross-component-rules.md`）：說明頁的寫法正是這一型——「游標保證補讀，所以帶長期跳過條件的讀者都要另寫」。實作時先查登記頁有沒有同一條；沒有就照登記頁的手順登記（owner：Kernel 的契約頁，架構）。

---

## 5. 批次、模型、檔位、估計

一批，只改文件。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者（說明頁、RC 草案、事實更正、dev_log） | Opus／medium | 文件是 medium（ギアボックス）；契約頁屬上段，不用小模型 |
| 合併前的設計審查（唯讀） | Fable／high | 卡片指定 |

估計 2～3 h（說明頁第 15 節 1～1.5 h、其餘各處 0.5 h、dev_log 0.5 h）。

---

## 6. 和狀態機卡的協調

- 狀態機那一批照 (A) 做的不用改；`InState`／`NotInState` 不加宣告。
- 狀態機說明頁可以連到 System 契約說明頁第 15 節，不必重寫一遍通用寫法；狀態機專屬的（進入／離開那兩張、要帶進進入的放 Resource）照寫。
- 照現在的行為固定後果 1～3 的測試可以寫（本批不改行為）。
- 已請主會話轉告（2026-11-22）。

---

## 7. 範圍的限界

- 修訂 3 沒有再送敵對式檢查；修訂 0～2 的兩次檢查針對的是改 Kernel 的選項，其中和 (A) 有關的事實（三個後果、`fixedStepConditions` 是「schedule 沒跑」、成熟引擎的寫法）照它們的結論寫。合併前的設計審查（Fable／high）會看說明頁。
- 別家的行為從原始碼讀出，沒有執行；Bevy issue #15072 原文不在本機。
- 前提查證是卡片的判斷，使用者同意；不是使用者另外的話。
