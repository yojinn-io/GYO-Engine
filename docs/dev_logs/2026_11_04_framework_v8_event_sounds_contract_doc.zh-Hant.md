# framework v8：v8-arch-event-sounds-contract-doc（`gyo.event_sounds` 的契約、Design Invariants 與事前宣告）

日期：2026-11-04。分支 `claude/recursing-dijkstra-38a360`（基底 PR #142 `ad348a0c`，#142 尚未合併）。上段（新 Data Contract、Design Invariants）。実装者：主會話 Opus 5.5／high（開工前一度是 Fable 5.1，使用者切換）；本批沒有審査者與検証者（只有文件，第 2 批有）。估 1.5～2.5 h，實際約 0.5 h。

**狀態：** 完成。事前宣告經使用者核准（「照建議核准」）。

相關文件：[契約](../framework/event-sounds.md)、[事前宣告](../architecture/plans/event-sounds/contract-declaration.md)、[計畫](../architecture/plans/event-sounds/README.md)、[HANDOFF](../architecture/plans/event-sounds/HANDOFF.md)。

## 做了什麼

- `docs/framework/event-sounds.md`（英文）：What it is、Validation 1～8、Using it、Contracts、Layers checks（#142 的檢查 2、3、4）、Design Invariants 1～6、Replacement Criteria 1～4。
- `docs/framework/architecture.md`：tool part 的核心與 JSON 綁定各加一句、Using it 加一條。DI 沒改（DI 3 在 #142 已允許組合格式）。
- `docs/architecture.md` Runtime boundaries 的 framework 列：加這個格式、依賴補 Model、Audio。
- 事前宣告：ES0～ES9、突變 28 條、守衛正向清單加 `GYO::Model`、`GYO::Audio` 的 §14.4 1～5。

## 經緯與判斷

- **事件名的字元規則（D-es-1）**：Engine 的 `ValidEventName` 在 `engine/model/src/AnimationEvents.cpp` 的匿名 namespace。卡片要求格式自己檢查 1～64 字元 `[a-z0-9_.]`，照做會讓 framework 複製一條沒有共用 fixture 的 Engine 規則（framework 的 Replacement Criteria）。使用者核准 (b)：解析只檢查非空，綁定時「名字必須等於 Engine 驗證過的名字」間接保證。
- **綁定結果**：卡片沒指定形狀；起初在 README 寫成依 clip 列出事件，寫契約時改成「對應表＋`Find(name)`」，因為兩個呼叫者都已從 Engine 拿到每個 clip 的事件，再列一份是複製 Engine 的資料。
- **DI 6 One schedule（D-es-3）**：卡片的 DI 最低要求是五條；加第六條，讓「預覽與遊戲用同一個偏移函式」可檢查。
- **第二個讀者（D-es-4）**：今天只有 PvP 讀這個對應，Model Viewer 尚未開工。#142 的檢查 3 寫「只有一個產品讀的組合屬於那個產品」。使用者在 2026-11-04 決定由 framework 持有；契約把它寫成使用者接受的預測壓力。
- **錯誤型別**：兩個 code（`InvalidEventSounds` 文件規則、`UnboundEventSounds` 綁定），detail 是路徑；工具要分開「檔案壞了」與「和這個模型對不上」。

## 證據

只有文件，沒有執行結果。判斷所依據的位置都在 repo：`engine/model/src/AnimationEvents.cpp:32`（`ValidEventName`）、`engine/audio/include/engine/audio/AudioFormat.hpp:14`（`kMaxGain`、`IsValidGain`）、`engine/CMakeLists.txt:17,94`（audio、model 無條件建置）、`engine/model/CMakeLists.txt:4`。
