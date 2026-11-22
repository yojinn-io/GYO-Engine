# 事件游標的重評：事前宣告（修訂 2）

> 性質：事前宣告（AGENTS §3 的 Architecture Delta；§2 的基盤 Architecture）。**使用者核准、說「開始」之前不改程式、不改條文與守衛。** 核准本宣告不等於 AGENTS §14.4 的判定變更核准；第 7 節列的守衛處置，有判定變更的照 1～5 逐條請使用者核准。
> 批次代號：`v8-arch-gyo2-event-cursor-reeval`。Owner：架構（GYO2 主會話開的任務卡）。更新：2026-11-22。
> 起因：使用者 2026-11-22 在狀態機卡選 (A＋)——狀態機這一批照 (A) 做，另外現在就開這張卡（HANDOFF `v8-arch-gyo2-state-machine-r8-crosscheck`，記在 `claude/gyo2-time-design` 1e4fd1ab）。理由（使用者的原則）：建機制這一輪，保守的選項會被上層蓋住，不等觀測。
> 碰到的已核准決定：「照游標修剪」（`v8-arch-gyo2-event-trim-by-cursor`）、`time-plan-two` (5)、游標宣告（`v8-arch-gyo2-event-cursor-declaration-approved`、`-five-declarations-approved`）。本頁的建議**不改「照游標修剪」，也不改預設的「被跳過下次補讀」**；只加一種由條件自己宣告的例外（第 3 節）。

**修訂紀錄**：修訂 0（338e40a0）建議 (R1)「被條件跳過一律算讀過」＋(R2)「讀者出生就拉住」。敵對式檢查（Fable 5.2／high，`declaration-check-r0-fable.md`）blocker 3、should 7、nit 7：(R1) 推翻的是你選過的方案明寫的賣點（B1）、它自己多加了「條件＝把讀者關掉」的前提（B2）；(R2) 讓讀取畫面期間的輸入在第一個固定步一次讀到（B3）。**修訂 1**（d87698a8）：建議改成 (C)「條件宣告判否時放不放開讀者」、題目二維持現狀；改法對照在第 11 節。窄複查（Fable 5.2／high，`declaration-check-r1-fable.md`）blocker 1、should 4、nit 6。**修訂 2（這一份）**：照窄複查補 (C) 的四條附屬規則，一次寫定——任何一個 release 條件判否就放開（B1）、沒有游標就建在當時最新（S1）、宣告進兩種指紋（S2）、不傳進被驅動的 schedule（S4）；回滾的 `NotResimulating` 改成列事實交回滾 owner（S3）。對照在第 12 節。改完沒有再送檢查。

**寫法**：每件事標出處——【你定的】（附 HANDOFF 代號）、【設計稿】（主會話的建議、你選了那個方案，但這一句不是你的原話）、【統合推出的】、【程式】、【別家】、【我判的】。

**證據**（這台 Mac 的主 checkout，git 不追蹤）：
- `build/evidence/gyo2-state-machine-r8/crosscheck-message-retention.md`：五家對照（狀態機卡的唯讀子代理，Opus）。
- `build/evidence/gyo2-event-cursor-reeval/source-verification.md`：本卡自己重讀的行。
- `build/evidence/gyo2-event-cursor-reeval/declaration-check-r0-fable.md`、`declaration-check-r1-fable.md`：修訂 0 的敵對式檢查、修訂 1 的窄複查。
- 別家原始碼：`~/Code/GitHome`（bevy 6a911653f、lightyear 736e1695、bevy_replicon d80c849；其餘版本見對照報告開頭）。
- 讀的程式：`origin/gyo2` 7fcfac27。

---

## 1. 先用遊戲講：三個後果

規則（現在的，【程式】`World.cpp:373-385`、`:1042-1053`、`Executor.cpp:327-358`）：每個讀者一個游標；被 run 條件跳過、或所在 schedule 沒跑，游標不動，下次補讀；第一次跑才開始拉住，第一次從還留著的最舊一筆讀起；每次最外層 Run 結束修剪到全部游標的最小值；滿了（預設 1024）丟最舊，記一筆 `DroppedMessages`（列出還沒讀到的讀者）。

**後果 1：停跑的讀者一直拉住保留，記錄失去警報作用。**
網路對戰裡，暫停選單不會停模擬（對手還在打你）。「播命中特效」帶 `InState<Playing>`，讀 `Hit`。開了選單，它被條件跳過，可是它跑過，所以拉住全部 `Hit`。選單一直開著，`Hit` 堆到 1024 筆之後，每次有人寫，那次 Run 就記一筆 `DroppedMessages`（「讀者：播命中特效」）。記錄每幀都報，真正的漏讀反而看不出來。
同型：主選單的「處理按鈕」帶 `InState<MainMenu>`，進關卡後不跑卻拉住 `ButtonPressed`；帶 `WhileGenerating()` 的讀者生成完永遠不跑、永遠拉住（world-gen 宣告第 8.1 節「決勝理由」的代價、第 13 節）。

**後果 2：恢復時補讀舊的——玩家看得到的錯。**
同一個「播命中特效」，關掉選單的那一幀，一次讀到暫停期間的全部 `Hit`（上限內最多 1024 筆），一口氣播出幾百個特效。主選單的「處理按鈕」回到主選單那一幀，把關卡裡暫停選單寫的 `ButtonPressed` 全部當成主選單的按鈕處理。

**後果 3：進入那張第一次讀不到，之後每次讀到上次以來的全部。**
`Flow.Enter.InLevel` 裡的 System 讀 `LevelSelected`。第一次進關卡時它從沒跑過、不拉住，選關那一幀的 Run 結束，`LevelSelected` 就被修剪，它讀到空的；之後每次進入，讀到上次進入以來的全部。
同型、和狀態機無關：每幀清單是 `[Main, Render]` 時，`Render` 的讀者第一幀讀不到 `Main` 寫的；日誌 Module 把它記成「不保證」（`logging.zh-Hant.md:33` LG-1；【程式】`Forward.hpp:44-46`）。

---

## 2. 假設 → 查證 → 對比 → 推斷 → 總結

### 2.1 假設

**H1**：三個後果來自 GYO 自己多加的前提（1+1=3），不是「照游標修剪」本身。
**H2**：只改那個前提，就能解掉後果 1、2，不必改 Kernel 公開介面、Kernel 不必知道 schedule 的語意。

### 2.2 查證

**前提表**

| # | 前提 | 誰定的／出處 | 掛在它上面的 |
|---|---|---|---|
| U1 | 照游標修剪：丟掉所有讀者都讀過的、或超過上限的；丟了有記錄；不靠時鐘。理由：「它讓『不漏讀』成為 Kernel 自己的保證，不必依賴編排層排得對。」 | 【你定的】`event-trim-by-cursor`（`HANDOFF.md:129`） | 修剪規則 |
| U2 | 「留到讀過或碰到上限，丟了有記錄」 | 【你定的】`time-plan-two` (5)（`HANDOFF.md:59`） | 同上 |
| U3 | 跨時鐘：「讓事件留到另一側讀過為止」 | 【你定的】`cross-clock-events-open`（`HANDOFF.md:48`） | 固定步 0～N 不漏讀 |
| X | **同一個時鐘也用游標**：被 run 條件跳過那一次不會錯過、下次補讀；代價「長期沒執行的讀者會讀到舊事件、並拉住保留」 | 【設計稿】`rebuild-time-design.zh-Hant.md:421`、`:425`、`:432`（「帶條件的讀者會漏讀是現在作者會踩的坑」）；同頁 `:431` 寫明「『同一個時鐘也改』是我的建議，不是你定過的」。你選了這個方案（`time-plan-two` 方案二）；游標宣告第 3.2 節、第 6 節第 2 點把它列為要解的問題 | 後果 1、2 |
| Y | 跑過一次才拉住；第一次從還留著的最舊一筆讀起 | 【統合推出的】游標宣告第 3.2、4.3 節。設計稿原本是「第一次從 0 讀起」，並把「固定步第一次跑時要不要跳過讀取畫面期間寫的」列為沒定的小題、「傾向跳過」（`rebuild-time-design.zh-Hant.md:440`）——Y 等於照那個傾向答了這題 | 後果 3 的第一次讀不到；`[Main, Render]` 第一幀；LG-1 的「不保證」 |
| Z | **條件判否＝一種意思** | 沒有人寫下來；X 與 Y 都默認它 | 見 2.4 D1 |

**H1 的結果：一半不成立。**
- X 不是統合多加的。它是你選的方案裡明寫的賣點（補掉「帶條件的讀者會漏讀」的舊坑），代價也明寫了。當時心裡的「長期沒執行」是固定步這類少數情形；狀態機與生成世界讓「被狀態條件長期關掉」變成常態——**是前提的使用情境變了，不是多加了前提**。
- Y 是統合推出的，但它擋掉了一個真的坑（讀取畫面期間的輸入在第一個固定步一次讀到）；修訂 0 把 Y 換掉，就把那個坑放回來（第 11 節 B3）。所以 Y 不換。
- 真正多出來的是 Z：**條件判否有兩種意思，GYO 只給了一種**。修訂 0 也犯了同一個錯，只是選了另一種意思（第 11 節 B2）。

### 2.3 對比：別家怎麼做、為什麼

條件判否的兩種意思，用遊戲講：
- **暫緩**（「不是現在」）：每 3 幀送一次封包的 System；`OnMessage<TurnEnded>` 觸發後才讀 `Order` 的 System。沒跑的期間寫的，下次要全部讀到。
- **關閉**（「這段時間我不在」）：帶 `InState<MainMenu>` 的主選單按鈕處理、帶 `WhileGenerating()` 的生成用 System。關掉期間寫的，回來不該讀到。

| 家 | 暫緩 | 關閉 | 怎麼做到的 |
|---|---|---|---|
| Bevy | 條件當節奏是常態：`on_timer`（`common_conditions.rs:36`）；replicon `send_buffered.run_if(resource_changed::<ServerTick>)`（`server/message.rs:186-188`）。超過兩次 update 就漏（`messages.rs:66-79`），只靠「讀者被預期每幀讀」（`:34`） | 兩次 update 後自然過期，最多漏一兩幀進新狀態（#15072）；Plugin 層按訊息種類清：`clear_messages_on_exit`／`on_enter`（`state_scoped_events.rs:67-107`） | 不分；時間窗剛好讓「關閉」大致對、「暫緩」超過兩幀就錯。暫停時全域停輪替，所有種類無上限累積（對照報告 1.1 (d)） |
| bevy_replicon | 同 Bevy | 自己帶狀態條件的接收器在 `OnEnter` 多跑一份、`OnExit` 清佇列（`client/message.rs:73-85`、`:292-295`） | 在 library 裡自己寫補償 |
| lightyear | 不支援（每幀 `Last` 清，`receive.rs:50-55`、`:447-456`） | 同左（清掉）；範例把接收器寫成不帶條件、讀到就設 `NextState` | 不分；一律當幀 |
| flecs | 不支援（沒有保留式；停用的 observer 當下錯過，`observer.c:276-278`） | 同左 | 不分；建議自己寫佇列（`ObserversManual.md:106-116`） |
| Unity Entities | event entity 沒人刪就一直留，消費者回來全部處理 | 同左（無上限、無記錄） | 不分；事件是普通資料。System 的 change filter 對被跳過的 System 補看，但看的是現在的值、沒有歷史 |
| Unity Netcode | RPC entity 不刪，4 幀後每筆警告一次（`WarnAboutStaleRpcSystem.cs:32-38`） | 同左 | 不分 |
| **GYO 現在** | 對（下次補讀，上限內，丟了有記錄） | 錯（後果 1、2） | 不分；一律當「暫緩」 |

**取能力、不取技術債**：
- 能力 1（GYO 自己的、設計稿的賣點）：暫緩的讀者不漏讀，丟了有記錄。→ 保留。Bevy、lightyear 都做不到，它們的作者用 `on_timer` 讀訊息會靜默漏。
- 能力 2（Bevy 用時間窗大致做到、replicon 自己補）：關閉的讀者回來不讀舊的。→ 要取。
- 債（不取）：Bevy 的時間窗（暫緩超過兩幀靜默漏、暫停無上限累積）；Bevy／replicon「每個 Module 在轉換時自己清」（機制散在各實作裡，而且是按訊息種類、對所有讀者，GYO 的讀者會再拉住之後的）。
- **沒有一家把兩種意思分開**。分開是 GYO 新導的；它的根據是「GYO 的能力 1 是別家沒有的，所以別家不必分」——Bevy 的時間窗讓「關閉」免費近似成立，GYO 的游標不會。

### 2.4 推斷

**D1（Z 是缺口）**：Kernel 只看得到「條件回了否」，看不到它是暫緩還是關閉；知道的是**定義那個條件的人**——狀態機知道 `InState` 是關閉，編排層知道 `WhileGenerating`／`AfterGenerating` 是關閉，寫 `on_timer` 的人知道它是暫緩。所以讓條件在定義時宣告一次，Kernel 照宣告做：機制只有一份（在 Kernel），宣告跟著條件走，Module 不必寫補償。

**D2（預設要是「暫緩」）**：兩種錯的代價不同——暫緩當成關閉是**靜默漏讀**（沒有記錄，設計稿說的舊坑）；關閉當成暫緩是拉住與補讀（看得到、有記錄）。所以沒宣告的條件照現在（暫緩），只有宣告了的才放開。這也讓現有行為、現有測試的判定都不變。

**D3（Kernel 不必知道 schedule 的語意）**：「誰判否」是 Kernel 自己的條件求值（`Executor.cpp:334-350`，依序、short-circuit）。不碰游標宣告 RC 2。

**D4（Y 留著）**：Y 擋掉讀取畫面的坑（`fixedStepConditions={AfterGenerating()}` 掛在驅動固定步的 System 上，`WorldTimeModule.cpp:114-117`，固定步裡的讀者是「schedule 沒跑」；準備函式設 `TimeControl.paused` 也一樣）。後果 3 的第一次讀不到與 LG-1 照舊，說明頁寫：要帶進「進入」的放 Resource。

**D5（留下的）**：
- 「schedule 沒跑」的讀者照舊拉住、補讀：時間暫停時固定步裡的讀者；進入／離開那兩張、少觸發的 observer 裡的讀者。這是 U3 要的，Kernel 分不出「固定步這幀跑 0 次」和「暫停」。和 Bevy 同型（Bevy 是無上限）。作者的寫法：暫停時擋住寫者。
- 同一個「暫停」意圖有三種寫法、三種語意：讀者上帶宣告為關閉的條件（放開）、`fixedStepConditions` 或 `TimeControl.paused`（拉住、補讀）、System 裡判斷後不 `Read()`（拉住、補讀）。說明頁列這張表。
- 轉換那一幀的殘留：讀者被關閉條件跳過時推到當時最新；之後同一幀、排在它後面的寫者寫的，它下一幀變成要跑時會讀到。例：回主選單的那一下 `ButtonPressed`，若 UI 排在「處理主選單按鈕」之後寫，就會漏進主選單一次。範圍最多一幀，取決於 Plan 的先後；Bevy #15072 同型（Bevy 是兩次 update）。說明頁寫：讀者排在寫者之後（或寫者用狀態條件擋）就沒有殘留。
- 兩套「上次以來」：`Changed<T>` 對被跳過的 System 照舊補看（`Executor.cpp:355` 只在真的跑時更新 `lastRun`），關閉條件只管 `Message`。差別的理由：`Changed<T>` 看現在的值，沒有歷史、不拉住保留；Bevy 的 change detection 也是這樣。

**H2 的結果：H2 不成立，改寫成 H2′ 才成立。**H2 說的「不必改公開介面」辦不到；改成 H2′「加條件的一個宣告，就能解掉帶狀態條件的讀者的後果 1、2，Kernel 不必知道 schedule 的語意」才成立。要加的是 `Condition<>` 的一個選項（第 4 節）。

### 2.5 總結

- 後果 1、2 的根源是「條件判否只有一種意思（暫緩）」。設計稿選暫緩是對的（補掉舊坑），但狀態機與生成世界帶來大量「關閉」的條件。
- 建議：條件在定義時宣告「判否時放開讀者」；預設不放開（照現在）。`InState`、`NotInState`、`WhileGenerating`、`AfterGenerating` 宣告放開。題目二（Y）不改。
- 後果 3、時間暫停、轉換那一幀的殘留留給說明頁（D5）。

---

## 3. 選項

### 3.1 題目一：被 run 條件跳過的讀者

| | (A) 不改，說明頁寫明 | **(C) 條件宣告「判否時放開讀者」（建議）** | (R1) 條件判否一律放開（修訂 0） | (E) 讀者的選項「被跳過時放開」 | (F) 訊息種類的選項「固定保留 N 次 Run」 | (B／G) 狀態轉換時清除 |
|---|---|---|---|---|---|---|
| 做法 | 照舊 | 定義條件時宣告；System 被跳過、而且它的條件裡**有任何一個宣告了的條件判否**時，它的所有游標（System 與每個條件 slot、每種訊息）推到當時最新（沒有的建在當時最新）；只有沒宣告的條件判否時，照舊不動（第 3.4 節） | 任何條件判否都推到最新 | 作者對每個讀者選 | 作者對每種訊息選；那種訊息不看讀者 | 狀態機、生成世界等在轉換時呼叫 Kernel 的清除入口 |
| 系譜 | 設計稿 X | 新導（2.3 末：別家不必分，GYO 要分） | 狀態機宣告 2.3 的 (C) 同型 | 新導 | Bevy 雙緩衝 | Bevy `clear_messages_on_exit`／`on_enter`、replicon |
| 帶狀態條件的讀者：後果 1、2 | 不解 | 解（殘留最多一幀，D5） | 解 | 選了才解；作者每個讀者都要記得選 | 解（但那種訊息沒有丟棄記錄） | 不解（GYO 的讀者會再拉住之後的） |
| 暫緩的讀者（`on_timer`、`OnMessage`） | 不漏 | 不漏 | **靜默漏讀**（舊坑回來） | 不漏 | 那種訊息會漏 | 不漏 |
| 進入／離開、時間暫停（schedule 沒跑） | 不解 | 不解（D5） | 不解 | 不解 | 解 | 部分 |
| Kernel 公開介面 | 不變 | `Condition<>` 多一個選項 | 不變（契約改） | `SystemOptions` 多一個選項 | `MessageOptions` 多一個選項 | 多清除入口，每個 Module 各自呼叫——**機制散在各實作裡** |
| 推翻的 | 無 | 無（預設照舊） | 設計稿 X 的賣點 | 無 | `time-plan-two` (5)「留到讀過」對那種訊息不成立 | 無 |

**建議 (C)**：唯一能讓暫緩不漏、關閉不拉住同時成立的選項；機制一份在 Kernel，宣告跟著條件走（定義 `InState` 的狀態機、定義生成條件的編排層各宣告一次），作者用 `InState` 不必記任何事。代價：Kernel 介面多一個選項；第 D5 節的三個情形照舊。

**(C) 與 (E) 的差別**：(E) 把宣告放在每個讀者上，用 `InState` 的作者每次都要記得；(C) 放在條件上，條件的定義者寫一次。(E) 的優點是不必回答「哪個條件判否」、不碰條件的指紋；(C) 用第 3.4 節的四條規則一次回答。作者若要某個讀者在 `InState` 擋住時照樣補讀，寫法是讀者不帶 `InState`、在 System 裡判斷狀態（或累積進 Resource）。

### 3.2 題目二：讀者從什麼時候開始拉住

| | **(K) 不改（建議）** | (R2) 出生就拉住（修訂 0） |
|---|---|---|
| 讀取畫面期間的輸入（`fixedStepConditions`、`TimeControl.paused`） | 第一個固定步讀不到（設計稿「傾向跳過」） | 第一個固定步一次讀到上限內全部（B3） |
| 進入那張第一次、`[Main, Render]` 第一幀 | 讀不到（說明頁；LG-1 照舊） | 讀得到 |
| 從沒跑過的讀者（沒進過的狀態的進入讀者等） | 不拉住 | 從出生永久拉住，後果 1 變大 |
| 讀到什麼取決於別的讀者 | 新讀者第一次讀到「別人還拉住的」以後的 | 否 |

**建議 (K)**。(K) 的「新讀者第一次讀到什麼取決於別的讀者」寫進說明頁（第一次跑的讀者不要依賴它）。

### 3.3 (C) 之後，一句話的契約

> 讀者讀到的，是它上一次 `Read()` 之後寫的全部（上限內）——**除非它上一次被跳過是因為一個宣告了「判否時放開讀者」的條件，那它從那次跳過之後讀起**。沒跑過的讀者不拉住，第一次從還留著的最舊一筆讀起（照舊）——除非它被 release 條件跳過過，那它從最後一次跳過讀起。放開掉的訊息不記 `DroppedMessages`：對宣告了的條件，`time-plan-two` (5) 的「丟了有記錄」不適用（它不是被上限擠掉，是讀者被關閉）。

### 3.4 (C) 的四條附屬規則（一次寫定）

1. **任何一個 release 條件判否就放開**（窄複查 B1）。現在的求值是先掛載點自己的條件、再 set 的條件，第一個判否就停（`Executor.cpp:341-348`）。`InState` 放在 set 上、System 自己再帶暫緩條件（例：`Gen` set 裡帶 `OnMessage<AssetArrived>` 的 System）時，暫緩的先判否，set 的 release 條件輪不到。所以：**沒宣告的條件判否之後，仍求值剩下的 release 條件**（只求值那些；沒宣告的照舊不求值），其中任何一個判否就放開。條件是唯讀的，多求值一次唯一的影響是那個條件自己的游標，而它馬上一起被推到最新。不改求值順序與 slot 編號。
2. **沒有游標的建在當時最新**（窄複查 S1）。被 release 條件跳過、這個掛載點某個 slot 還沒有游標時，建一個在當時最新。否則第一次打開「設定」的 `InState<Settings>` 讀者，會一次讀到別的讀者（例：時間暫停中的固定步讀者）拉住的全部按鈕——讀到什麼又取決於別的讀者。這不會放回讀取畫面的坑：固定步裡的讀者是「schedule 沒跑」，照舊沒有游標。建出來的游標拉住到下一次求值：還是判否就再推到最新，所以不會長期拉住。
3. **宣告進兩種指紋**（窄複查 S2）。Registry 指紋（J-18）與權威 Module 的一致性指紋（它收寫判定資料的 System 的條件名，`ServerAuthorityStrategy.cpp:329-350`）都收「條件名＋是否 release」。兩端同名條件宣告不同，指紋不同、連線時就看得出來。同一個 Registry 裡同名、宣告不同的條件：不另加守衛（不為防誤用加守衛）；實作批查現在對同名條件有沒有既有的檢查，有就沿用。
4. **不傳進被驅動的 schedule**（窄複查 S4）。release 條件掛在驅動別張 schedule 的 System 上（例：`fixedStepConditions={AfterGenerating()}` 掛在 `RunFixedSteps`，`WorldTimeModule.cpp:114-117`），被放開的只有那個驅動 System 自己的游標；那張 schedule 裡的讀者是「schedule 沒跑」，照舊拉住、補讀。時間 Module 的說明頁加一句。

---

## 4. 改什麼

### 4.1 程式

| 處 | 現在 | 改成 |
|---|---|---|
| `Condition<>`（`Registry.hpp:54-90`）與 `ConditionRef` | 名字、解析、求值 | 多一個宣告「判否時放開讀者」（名字照命名規範在實作批定，暫稱 release）；預設不放開 |
| `Executor.cpp:334-357` 條件求值 | 判否就 short-circuit、`!runs` 直接 return | 照 3.4 第 1 條：沒宣告的判否後仍求值剩下的 release 條件；有 release 條件判否時，這個掛載點的游標（slot 0 與每個條件 slot、每種訊息）推到該種訊息當時最新，沒有的建在當時最新（3.4 第 2 條） |
| `WorldGeneration.cpp:17-23` | `WhileGenerating`、`AfterGenerating` | 宣告 release |
| 狀態機的 `InState`／`NotInState`（狀態機卡，未合併） | — | 宣告 release（第 9 節，經主會話協調） |
| 唯讀登記表、Registry 指紋（J-18） | 條件的名字 | 條件的宣告也收（3.4 第 3 條）；雜湊在條件名之後加一個位元組（0／1） |
| 權威 Module 的一致性指紋（`engine/modules/authority`，`ServerAuthorityStrategy.cpp:329-350`） | 條件名 | 條件名＋宣告（3.4 第 3 條） |
| `NotResimulating`（回滾 Module，`rollback/src/Registrations.cpp:231-235`） | — | **本批不宣告，交回滾 Module 的 owner 判**。事實（窄複查 S3）：回滾的規則只禁止訊息跨過可回滾的邊界，步的 schedule 裡面寫、裡面讀是允許的（`rollback-module-declaration.zh-Hant.md:256-260`）。不宣告：重跑時重寫的 `Hit` 之後被重讀、特效重播；宣告：重跑才新出現的命中不播。兩種都不完美 |

World 摘要的格式不變（游標表本來就在，值會因放開而不同）；版本不升。

### 4.2 文件

- `docs/architecture/target/kernel.zh-Hant.md` DI-KS 10 保留式那一段（第 51 行起）：「被 run 條件跳過（含短路沒評估到的條件）…游標不動、下次補讀」後面加例外（第 6 節的條文）。
- `event-cursor-declaration.zh-Hant.md`：頂端加一行「第 3.2 節『不執行的讀者』列由本頁加例外」；內文不改。順手更正第 3.2 節 lightyear 的行號（現在是 `receive.rs:54`、`:105-110`、`:153-160`、`:447-456`）與「Bevy 超過兩次 update 靜默丟」缺了暫停例外（對照報告 D4）——事實更正，不改判斷。
- `world-gen-declaration.zh-Hant.md` 第 8.1 節與第 13 節、生成世界的說明頁（`orchestration.zh-Hant.md` O-14 若有寫）：「`WhileGenerating()` 的讀者永久拉住」改成「不拉住」；「送給還沒跑過的 `AfterGenerating()` 讀者的被修剪」照舊（Y）。owner：架構（本批改）。
- 日誌 Module LG-1：不改（Y 不改）。
- 狀態機的說明頁：第 9 節。
- 說明頁（Kernel 的 System 契約頁 `system-contract-page.zh-Hant.md`，若無合適處則在 kernel 頁 DI-KS 10 旁）：暫緩與關閉的例子、第 D5 節三種寫法的表、轉換那一幀的殘留與避開法。

---

## 5. Architecture Delta（AGENTS §3 的七項）

1. **Feature**：狀態機的段落（主選單、關卡中、暫停）裡帶狀態條件讀訊息的 System 不出現後果 1、2；生成世界的 `WhileGenerating()` 讀者生成完不拉住；暫緩型條件（節奏、`OnMessage`）照舊不漏讀。
2. **現在的問題**：第 1 節後果 1、2；條件判否只有一種意思（2.4 D1）。
3. **變化的邊界**：Kernel 的 run 條件多一個宣告；保留式訊息的讀者契約多一個例外（DI-KS 10）。
4. **影響**：`engine/ecs/kernel`（`Registry.hpp`、`Executor.cpp`、`World.cpp`／`World.hpp` 的推游標函式、`Fingerprint.cpp`、登記表）；`engine/orchestration`（`WorldGeneration.cpp`）；`engine/modules/authority`（一致性指紋）；時間 Module 的說明頁（3.4 第 4 條）；狀態機（`InState`／`NotInState`）；測試見第 7 節；文件見 4.2。
5. **Dependency Direction**：不變（Module 用 Kernel 的條件選項）。
6. **Ownership**：「條件判否是什麼意思」由定義條件的 owner 宣告；Kernel 只照宣告推游標。訊息留多久仍是 Kernel 照游標與上限。
7. **為什麼不能更小**：(A) 留著後果 2（玩家看得到）與失效的警報，作者的補償會積在上層；(R1) 不加介面但讓暫緩的讀者靜默漏讀；(E) 要每個用 `InState` 的作者記得選。

---

## 6. 基盤的根據（AGENTS §2）

- **為什麼算基盤**：條件與讀訊息的行為在每個 System 裡；狀態機、生成世界正要積上去（狀態機的說明頁要寫三個後果的寫法），越晚改，說明頁與作者的補償越多。
- **予見的根據**：(1) 名指的 Feature——狀態機宣告 2.3 寫明四個點名 Feature「都不需要」在帶狀態條件的 System 讀訊息，所以這裡的根據**不是**「Feature 需要」，而是「Feature 會讓作者這樣寫、寫了就踩到看得見的錯而且沒有任何東西擋」——使用者照「保守的選項會被上層蓋住」選了現在就處理（A＋）；生成世界的 `WhileGenerating()` 是已實作的情形。(3) 規模——上限 1024 筆，每幀一筆的 UI 或命中訊息在 60 fps 下約 17 秒填滿。(2) 參考實作——2.3。

**Design Invariants（DI-KS 10 保留式那一段加一句；核准後在實作批寫進 `target/kernel.zh-Hant.md`）**：
- run 條件可以在定義時宣告「判否時放開讀者」，預設不宣告。System 被跳過時，若它的條件裡有任何一個宣告了的條件判否（沒宣告的條件判否之後，仍求值剩下宣告了的條件），它的游標全部推到當時最新、沒有的建在當時最新（不拉住、回來不補讀，放開的不記 `DroppedMessages`）；只有沒宣告的條件判否、或所在 schedule 沒跑，游標不動、下次補讀（照舊）。放開只作用在被跳過的那個 System 自己的游標，不傳進它驅動的 schedule。條件的宣告進 Registry 指紋與權威 Module 的一致性指紋。
- 其餘照現行 DI-KS 10。

**Replacement Criteria（加進 Kernel 的 RC）**：
- 作者或 Module 開始為「schedule 沒跑的讀者」寫補償（暫停後清掉、進入時推到最新）——表示要讓 schedule 的登記者也能宣告（題目一的 schedule 版本，見第 11 節 S6），停下來問。
- 條件的宣告需要第三種意思（例：「判否時只放開某些訊息種類」）——宣告在長成設定語言，停下來問（AGENTS §9）。

---

## 7. 守衛與測試（AGENTS §14.4）

(C) 的預設照舊，**訊息的現有判定都不變**；指紋的期望值可能變（下表 J-18），其餘只有新增。

| 判定 | 處置 |
|---|---|
| J-36 全部（`MessageCursorTests.cpp`，含 `msg-skipped-*`、`msg-not-run-*`、`msg-never-run-holds-nothing`、`msg-new-reader-oldest`、`msg-trim-after-rounds`、`msg-summary-cursor-position`）、J-56 `[ns-events]`（`NestedRunTests.cpp:943-963`）、`HookFlushTests.cpp`、`ImmediateChangeTests.cpp`、`ObserverTests.cpp`、`WorldTests.cpp`、`RegistrationRoundTests.cpp` | 不變：它們用的條件（`FlagOn`、`OutSet` 等）沒宣告 release。實作批用 `git grep` 確認沒有測試用 `WhileGenerating`／`AfterGenerating` 擋讀者（修訂 1 時掃過：`tests/common/world_time/WorldGenTimeTests.cpp` 的讀者 `RecordFrame` 不帶這兩個條件） |
| J-18 golden、權威 Module 一致性指紋的期望值 | 若 golden 或期望值裡有條件：雜湊多收宣告，期望值改。這是守衛期望值的變更：實作批查有沒有受影響的，有就照 §14.4 列 1～5 請你核准（預計是收緊：同名而宣告不同的條件以前看不出來、之後指紋不同；沒有放寬的面） |
| 新（Kernel） | release 條件判否：System slot 與每個條件 slot 的游標推到最新、不拉住、回來只讀之後的、不記 `DroppedMessages`；沒宣告的條件判否照舊補讀；掛載點的暫緩條件先判否、set 上的 release 條件判否時仍放開（3.4 第 1 條），暫緩條件判否而 release 條件判真時不放開；判否前同一 Run 已寫的不讀、判否後才寫的下次讀到（D5 的殘留）；沒跑過的讀者被 release 條件跳過時建在最新，之後第一次跑不讀到別的讀者拉住的舊紀錄（3.4 第 2 條）；release 條件掛在驅動巢狀 schedule 的 System 上，裡面的讀者照舊補讀（3.4 第 4 條）；Registry 指紋含宣告 |
| 新（權威 Module） | 兩端同名條件宣告不同時一致性指紋不同 |
| 新（編排層） | 生成結束後，帶 `WhileGenerating()` 的讀者不再出現在 `DroppedMessages` 的讀者裡、不拉住 |

**突變**：新性質各一個只有它能抓的（例：「release 只推 slot 0、不推條件 slot」「release 條件判否卻照舊不推」「沒宣告的條件也推」「暫緩條件判否後不再求值 release 條件」「沒有游標時不建」「指紋不收宣告」），照 AGENTS §14.5 寫明守的條文；只跑相關的突變（`mutation-rerun-scope`）。
**§14.2 的敵對式檢查**：新案例與突變清單，請作者以外的模型（Fable／high）檢查一次。

---

## 8. 批次、模型、檔位、估計

一批（上段：公開介面與 Design Invariants）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 宣告的敵對式檢查 | Fable／high（唯讀） | 卡片指定；修訂 0 已做，修訂 1 沒有再送 |
| 實作者 | Opus／high | 上段不用小模型；本卡 |
| 新案例與突變清單的敵對式檢查 | Fable／high | 和實作者不同模型 |
| 合併前的設計審查 | Fable／high（唯讀） | 卡片指定 |

**估計**（讀程式量估的）：條件的宣告與兩種指紋 2～3 h；Executor（續求值、推與建游標）2～3 h；`WhileGenerating`／`AfterGenerating` 宣告與編排層案例 1 h；Kernel 新案例 2～3 h；突變與檢查 1～2 h；文件（DI-KS 10、4.2）與 dev_log 1～2 h。合計 9～15 h。狀態機的 `InState` 宣告不在本批（第 9 節）。

---

## 9. 和狀態機卡的協調（要先經主會話）

- **會碰到狀態機用的介面**：`InState`／`NotInState` 要宣告 release——在狀態機那邊是一行（建條件時加選項）。本批先合併的話，狀態機那一批補這一行；狀態機先合併的話，本批補。由主會話決定誰補。
- **狀態機說明頁的內容會變**：本批之後，狀態機宣告 2.3 的後果 1、2 對帶 `InState` 的 System 不再發生（剩最多一幀的殘留）；後果 3 照舊。說明頁「要帶進進入的放 Resource」的寫法照舊成立。
- 請主會話轉告狀態機卡：**不要寫固定後果 1、2 會發生的測試**（本批之後會反過來失敗）；後果 3 可以固定（本批不改）。
- 誰後合併，誰用 merge 跟上 gyo2，並對齊兩邊說明頁的文字。

---

## 10. 範圍的限界

- 這是宣告，沒有寫程式、沒有量測；估計是讀程式估的。
- 別家的行為從原始碼讀出，沒有執行；Bevy issue #15072 的原文不在本機。flecs、Unity、replicon 的行多數照對照報告與檢查報告引用（`source-verification.md` 末段）；`on_timer`、replicon `server/message.rs:186-188` 是檢查者讀的，本卡沒有重讀。
- 「你當時有沒有讀到設計稿決定 4 那一列」查不到；本頁不再把 X 說成統合多加的。
- 條件宣告的名字、放在 `Condition<>` 的哪個參數，照命名規範在實作批定。
- 修訂 2 沒有再送檢查（修訂 0 全面檢查、修訂 1 窄複查各一次）。
- 窄複查沒讀：狀態機的 `InState` 實作（未合併）、kernel golden 裡有沒有條件、權威 Module 的測試與 Design Invariants 全文、突變清單——實作批補。

---

## 11. 檢查報告的處理（`declaration-check-r0-fable.md`）

| 項 | 處理 |
|---|---|
| B1 X 不是統合多加的 | 接受。前提表改標【設計稿】並引 `rebuild-time-design.zh-Hant.md:421`、`:425`、`:431`、`:432`；U3 改成 `cross-clock-events-open`（`HANDOFF.md:48`）；H1 判「一半不成立」；建議改成不推翻預設的 (C) |
| B2 (R1) 多加了「條件＝關閉」 | 接受。這正是 Z；2.3 改成「暫緩／關閉」兩種意思的對照；(R1) 降為不建議 |
| B3 (R2) 讓讀取畫面的輸入一次讀到 | 接受。題目二改建議 (K)；Y 留著（2.4 D4） |
| S1 「解」寫大了 | 接受。3.1 表分列「帶狀態條件的讀者」與「schedule 沒跑」 |
| S2 殘留正是觸發轉換的那一筆 | 接受。D5 寫明例子、取決於 Plan 先後、避開法 |
| S3 「沒有一家補讀」說過頭 | 接受。2.3 改成「沒有一家把兩種意思分開」，Unity、Netcode、Bevy 暫停列進表 |
| S4 第 7 節不完整 | (C) 預設照舊，這些判定都不變；表裡列出檢查者點名的全部案例與檔案 |
| S5 協調不足 | 接受。第 9 節改寫（`InState` 要加宣告、後果 3 不變）；(R1) 的系譜改「狀態機 2.3 的 (C) 同型」；HANDOFF 位置寫明；生成世界說明頁 owner 寫明；日誌 LG-1 不再改 |
| S6 schedule 沒跑的那一半沒列選項 | 列在第 6 節 RC：「schedule 的登記者宣告這張的讀者不拉住」是 (C) 的 schedule 版本。不做的理由：進入／離開的讀者要的是「觸發這次進入的那一筆」，不拉住就讀不到；固定步要拉住（U3）；沒有點名的 Feature 需要在這些 schedule 讀訊息 |
| S7 (R1) 單獨時沒建游標的讀者 | (C) 寫明：沒有游標的不建 |
| nit：world-gen 章節 | 改成第 8.1、13 節 |
| nit：出生點 | (R2) 不採，不適用 |
| nit：3.3 的契約句 | 改成「上一次 `Read()`」 |
| nit：摘要版本 | 4.1 寫明不升 |
| nit：第 6 節與狀態機 2.3 矛盾 | 第 6 節改寫，明說根據不是「Feature 需要」 |
| nit：估計 | 重估（判定不變，測試只新增） |
| nit：L5 之後 slot 位置會不會動 | 實作批查；(C) 只在判否當下推「這個掛載點已有的」游標，不依賴跨版本的 slot 對應 |

---

## 12. 窄複查的處理（`declaration-check-r1-fable.md`）

| 項 | 處理 |
|---|---|
| B1 先判否的條件決定，set 上的 release 條件輪不到 | 接受。3.4 第 1 條：沒宣告的判否後仍求值剩下的 release 條件；DI、4.1、第 7 節、突變同步 |
| S1 沒有游標的不建，第一次打開時讀到別人拉住的 | 接受。3.4 第 2 條：建在當時最新；第 11 節 S7 的處理由這條取代 |
| S2 漏了權威的一致性指紋 | 接受。3.4 第 3 條；影響加 `engine/modules/authority` |
| S3 `NotResimulating` 不宣告的理由不成立 | 接受。4.1 改成列事實、交回滾 owner；本批不宣告 |
| S4 不傳進被驅動的 schedule | 接受。3.4 第 4 條、DI、第 7 節新案例、時間 Module 說明頁 |
| N1 golden 重產是期望值變更 | 接受。第 7 節改寫、照 §14.4 列 1～5 |
| N2 同名不同宣告 | 3.4 第 3 條：不另加守衛，指紋看得出跨端的；同一 Registry 沿用既有檢查（實作批查） |
| N3 放開的沒有記錄 | 接受。3.3、DI 寫明 |
| N4 用條件的作者不能逐次改 Module 的宣告 | 3.1 末寫別的寫法（不帶 `InState`、在 System 裡判斷） |
| N5 RC 數次數 | 拿掉「兩個以上」 |
| N6 S6 的理由只對再次進入成立 | 不改結論；記在這裡 |

