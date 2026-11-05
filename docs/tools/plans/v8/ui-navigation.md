# PvP 的 UI action 對照表的事前宣告（`v8-product-ui-action-check`）

日期：2026-11-05。提出：v8 工具線。**狀態：2026-11-05 核准，由本線（v8 工具線）實作（`v8-product-ui-action-check-approved`）。**

來源：`v8-tool-ui-navigation` 你選了 (A)「產品內改成表」，先做（`v8-tool-ui-navigation-a-first`，見 [HANDOFF](HANDOFF.md) 決定紀錄）。

## 代號對照

| 代號 | 意思 |
|---|---|
| `v8-product-ui-action-check` | 本批：PvP 大廳的 action 分派改成對照表，啟動時和 UI 檔的 `actions` 宣告雙向比對 |

## 現況（2026-11-05，以 origin/master `3cf63a87` 為準）

- PvP 只有一個 canvas（`lobby`），UI 檔是 `assets/object_fps_pvp/ui/pvp_lobby.json`（`gyo.ui` v2）。
- UI 檔的 `actions` 宣告 6 個：`pvp.submit_address`（string）、`pvp.refresh`、`pvp.create`、`pvp.join`、`pvp.leave`、`pvp.quit`（none）。
- Engine 已經檢查的（`engine/ui/src/UiValidation.cpp`）：元素與 `cancel_action` 用到的 action 都有宣告、payload 型別相符、宣告的 id 不重複。
- 沒有人檢查的：
  - UI 檔宣告了、產品程式沒處理：按下去什麼都不發生，沒有錯誤；
  - 產品程式處理了、UI 檔沒宣告（例如字串打錯）：那段程式永遠不會跑，也沒有錯誤；
  - 產品處理時假設的 payload 型別，和宣告的不一致。
- 分派在 `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:299` 的 `Execute`，是 if／else 比對字串。載入 UI 檔在同檔第 946～958 行。

## Feature → Contract → Architecture（AGENTS §2）

- **Feature**：改 UI 檔或程式時，action 對不上就在啟動時得到明確的錯誤，不會變成「按了沒反應」。行為完全不變。
- **Contract**：不改。`gyo.ui` 已經有 `actions` 宣告；本批只讓產品讀它來比對。
- **Architecture**：不改。對照表與比對都在 PvP 內。沒有新的依賴，Engine 與 framework 不動。

## 改動

1. `Execute` 的 if／else 換成對照表：action id → 處理函式＋期望的 payload 型別。
   - 每個處理函式的內容照搬，包括 `pvp.submit_address` 之外先清 `localStatus`、`Lobby` 階段以外只接受 `leave`／`quit` 等順序。
2. 比對函式（純函式，可單獨測）：輸入 UI 文件的 `actions` 與對照表，任何一邊多出來，或 payload 型別不同，就回傳錯誤，錯誤訊息列出所有對不上的 id。
3. 載入大廳 UI 檔後、`ui.Initialize` 之前呼叫比對。不一致時，啟動失敗，走現有的載入錯誤路徑（和 UI 檔解析失敗同一條）。

## 不做的事（範圍的限界）

- 不做頁面切換，也不改 `gyo.ui`。(B) 等暫停選單、畫面設定要加頁面時，以實際的頁數再決定。
- 不碰 `object_fps_v2`（`ObjectFpsUi.cpp:462` 的 TODO、`CanvasFor`）。v2 有同樣的問題，要做另排一批。
- 不把比對函式放進 framework。目前只有 PvP 用它；v2 也要用時，照「多處使用的共通功能先放 framework」再搬。

## 判定（§14.1，先宣告，結果出來後不放寬）

1. **行為不變**：PvP 現有的測試（`cpu|shader`）全部通過，測試本身不改。
2. **比對抓得到三種不一致**：新的單元測試用合成的 action 清單：
   - 宣告多一個：失敗，訊息含該 id；
   - 處理多一個：失敗，訊息含該 id；
   - payload 型別不同：失敗；
   - 一致：通過；
   - 真的 `pvp_lobby.json` 和真的對照表：通過。
3. **ミューテーションテスト**：以下 4 條都要 killed：
   - 不檢查「宣告多出」；
   - 不檢查「處理多出」；
   - 不比 payload 型別；
   - 啟動時不呼叫比對（由「真的 UI 檔少宣告一個時，啟動失敗」的測試殺）。
4. **L1**：三平台通過。
5. **L3**：不需要。大廳的行為不變，判定 1 涵蓋；你想在 GUI 上點一次也可以，但不算判定。

件數的突き合わせ：突變宣告 4 條，報告寫「宣告數、killed、survived、stale」。

## 段、模型與檔位（navigator.md）

- **段：下段**。不碰 Design Invariants、協議、資料格式、公開介面；只是產品內部的重寫，加一個啟動時的檢查。
- 実装者：Opus 5.5／high（主會話）。
- 審査者：Opus 5.5／high（子 agent，code review）。
- 検証者：不需要（沒有放寬的判定）。
- 估時：2～3 h。超過 4.5 h 就停下回報。

## 要你決定（2026-11-05 已決定：核准，(a) 本線做）

1. 本草稿核准嗎？
2. 誰做：
   - (a) 本會話做，接在 `claude/v8-tool-line-records` 上，和工具線的紀錄同一條 PR（先例：`v8-product-lobby-fullwidth` 在工具線的 #111）；
   - (b) 交給產品會話。
   建議 (a)：範圍小，判斷都在本線。PvP 計畫的同步照舊由產品會話做，本線只通知它。

## 實作後的結果（2026-11-05）

- commit `6764f65f`（實作）、`f38c6e43`（審查後的修正）。dev_log：[2026_11_05_pvp_v8_ui_action_check](../../../dev_logs/2026_11_05_pvp_v8_ui_action_check.zh-Hant.md)。
- 判定 1：PvP `cpu|shader` 50／50，測試未改。判定 2：大廳 10 個 case 全過。判定 3：6／6 killed（宣告 4＋審查後追加 2，只收緊）。判定 4：待 PR 的 L1。判定 5：不需要。
- 和宣告寫法不同之處：突變 4 用「真的 UI 檔多宣告一個 `pvp.unhandled`」殺，不是「少宣告一個」（少宣告會先被 Engine 的驗證拒絕，抓不到比對）。
- `Execute` 每個 case 的內容與啟動時的呼叫點，由審查確認，沒有測試涵蓋（`PvpApplication.cpp` 沒有測試呼叫）。
