# 工具 v8：v8-tool-model-viewer-events

日期：2026-11-04。分支 `claude/v8-tool-model-viewer-77f310`（PR #146），接在 `-preview` 之後。順序由你決定：先做 `-events`，再做 `-event-sounds`。

- 段：下段（用 `-core` 的寫出，不改它）。
- 實作者：Opus 5.5／high（主會話）。
- 審查者：Opus 5.5／high（子 agent，一次）。
- 沒有放寬判定，沒有開檢驗者。
- 估時 3～5 h；實際約 2 h。

**狀態：**
- 程式、測試、突變、審查都完成。
- L1 的結果記在 HANDOFF。
- L3 第 6 項照原文做不到，交你決定（見下文）。

相關文件：
- [事前宣告](../tools/plans/v8/model-viewer.md)：「`-events`」與「`-events` 實作後的結果」；
- [docs/model_viewer/architecture.md](../model_viewer/architecture.md)「Window」。

## 做了什麼

- 核心（`cpu` 可測）：
  - `EventHistory`：每次編輯存一份快照，做 Undo／Redo；記下檔案目前存的是哪個狀態，用來判斷「未存檔」。新建的文件在寫出之前，每個狀態都算未存檔。改名成同一個名稱、移動到同一個秒數，都不算一步。
  - `DragSeconds`：拖動時把秒數夾在片段的兩端之間。
  - `PlaybackClock::Advance` 改成交出 Engine 的 `PlaybackInterval`（沒有折回的秒數）。`CrossedEvents` 只用 `Crossed` 判斷經過了哪些事件。
- 視窗的 Events 面板：
  - 開啟模型既有的事件檔，或新建一個：用 `CreateEvents` 登錄到掛載時讀的 catalog，之後重新掛載內容，讓清單裡出現新檔；
  - 時間軸：
    - 點標記只選取，拖過 ImGui 的門檻才會移動，並保留抓住的位置；
    - 放開時記一步；
    - 點線上的空處換播放位置。
  - 「Add at playhead」、改名、輸入秒數（不夾住，存檔時由 Engine 拒絕）、刪除；
  - Undo／Redo、存檔：快捷鍵在輸入文字或按住標記時不作用；
  - 未存檔的標示；
  - 換模型、換內容資料夾、關閉事件檔前，會問要不要丟掉未存檔的修改；
  - 播放經過時，標記會亮 0.3 秒。

## 驗證與經過

- 本機 macOS，`build/target/_build/ui`：
  - `gyo_model_viewer.core` 25 個 case、474 個 assertion 通過；
  - `gyo_model_viewer.*` 與 `gyo_framework.*` 13／13 通過；
  - 審查前的整套 `cpu|shader` 117／117 通過；之後的修正只動到視窗與核心的編輯部分，所以只重跑了上面兩組。
- 件數の突き合わせ：一致。
  - 宣告 3 條 ＋ 實作中 1 條（`mve-unsaved-after-undo`）＋ 審查後 1 條（`mve-step-not-a-loop`）＝ 5 條，5／5 killed。
  - 第一輪是宣告的 3 條加實作中的 1 條，4／4 killed。可是 `mve-unsaved-after-undo` 的取代式在 `saved_` 為空時，會對空值解參考（未定義行為），所以改寫後重跑（第二輪），預期訊息沒有改。審查後加了 1 條，又跑一輪，5／5。
- 審查的指出與處理：
  1. （高）事件的「Seconds」欄位和播放的「Seconds」滑桿 ID 相同，點欄位時會換播放位置，而且打不了字。修正：Events 面板包 `PushID`，播放滑桿另取 ID。
  2. （高）拖動中按 Undo，會讀到範圍外的事件。修正：按住任何東西時，快捷鍵不作用；Undo／Redo 時清掉所有索引；放開時檢查索引範圍。
  3. （高）點一下標記就會移動它。修正：拖過門檻才移動，並保留抓住的位置。
  4. （中）輸入名稱後直接點別的標記，輸入的名稱會丟掉。修正：輸入中的值先記成待確認，選取換到別的事件前先寫入；`Move` 到同一個秒數不算一步。
  5. （中）未存檔的修改會被悄悄丟掉。修正：換模型、換內容、關閉檔案前會問；關閉視窗攔不下來，見下一節。
  6. （中）換片段後，索引還指著舊片段的事件。修正：換片段時清掉。
  7. （中）新建的檔不會出現在清單裡，預設的 catalog 也沒有照命令列的設定。修正：建立後重新掛載；預設改成掛載時讀的 catalog。
  8. （中）E4 分不出循環與夾住。修正：測試加一段跨過片段結尾的播放；加了突變 `mve-step-not-a-loop`。
  - 低：新建文件的存檔狀態已修；亮燈在每次編輯後清掉已修；每步一份快照不設上限，以這種檔的大小來說可以接受。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-model-viewer/events/`，有：
  - `mutations.txt`（5／5）、`mutations-round1.txt`、`mutations-round2.txt`；
  - `ctest-all.txt`、`ctest-viewer.txt`。

## L3 第 6 項（關閉時的未存檔提示）

- framework 的 `RunImGuiShell` 在視窗關閉時直接結束；`ImGuiShellClient` 只有 `WantsExit`，沒辦法擋下關閉。
- 現在做到的：換模型、換內容、關閉檔案前會問；關閉視窗時，只在 log 寫一行警告。
- 要照原文做到，就得改 framework 的外殼契約，例如讓客戶能擋下關閉。這是 framework 的 Architecture Delta，不在本批範圍，交你決定。

## 和事前宣告不同的地方

- 新建的事件檔，在寫出之前每個狀態都算未存檔。
- L3 第 6 項（見上一節）。

## 發現（不在本批範圍）

- framework 的 ImGui 外殼沒有「關閉前詢問」的機制；ui_editor 也一樣，關閉視窗就丟掉未存檔的修改。
