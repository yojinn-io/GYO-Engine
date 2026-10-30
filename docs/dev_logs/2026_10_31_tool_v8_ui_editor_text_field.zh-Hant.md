# 工具 v8：v8-tool-ui-editor-text-field（ui_editor 編輯與預覽 text_field）

日期：2026-10-31。分支 `claude/v8-tool-ui-editor-text-field`（切自 master `80d14f22`）。檔位 high，引擎 Opus 5.5。估 8～12 h，實際約 6 分鐘（00:16～00:22，牆上時間）。

**狀態：** 完成（Mac 上的測試與突變）。GUI 上沒有手動操作過。

相關文件：[text_field 的 dev_log](2026_10_30_tool_v8_ui_text_field.zh-Hant.md)、[元素表的 dev_log](2026_10_30_tool_v8_ui_editor_element_schema.zh-Hant.md)、`tools/ui_editor/README.md`。

## 做了什麼

- **元素表（`ElementSchema`）**：
  - `ElementKind` 加 `styledText`（沒有文字來源、但有文字樣式）、`integers`（`ElementIntegerField`：標籤、鍵、上下限）、`strings`（`ElementStringField`）。
  - `text_field` 的整數欄位是 `max_length`（1～1024），文字欄位是 `placeholder`。
- **Inspector**：
  - 「Text style」改成有文字來源**或** `styledText` 時都顯示。`Literal` 只在有文字來源時顯示，所以 `text_field` 也能改字級與字型。
  - 新的「Fields」區塊依表畫出整數與文字欄位。
  - `placeholder_color` 沿用表上的顏色欄位。
- **預覽（`PreviewAdapter`）**：在 Preview interaction 模式，`text_field` 可以用鍵盤編輯：
  - 編輯中，ImGui 收到的字元變成 `Insert`；Backspace、Delete、←→、Home、End 變成對應的編輯動作；Ctrl+A（macOS 是 Cmd）是全選，Ctrl+V 是貼上剪貼簿。
  - 編輯中，←→ 不再調整滑桿，Space 不再觸發 activate。
  - 送出時，動作紀錄顯示文字（`payload:"…"`），預覽的 binding 也更新成送出的文字。
  - 要求文字輸入：寫入 ImGui 的 `PlatformImeData`（`WantTextInput`、`WantVisible`、輸入框位置）。ImGui 的 SDL3 後端看到後會打開 SDL 的文字輸入，並把輸入法的視窗放在輸入框下方。
  - 編輯器沒有打開 `imeComposition`，所以組字由作業系統畫；確定後的字才進輸入框。
  - 編輯中，`io.WantTextInput` 在下一幀變成 true，編輯器的快捷鍵（Ctrl+C／V／D、方向鍵微調）會照既有的判斷停用。

## 用到 ImGui 的內部 API

- ImGui 1.92 沒有公開的「不經過 ImGui 文字元件就要求文字輸入」的呼叫，所以 `PreviewAdapter.cpp` include 了 `imgui_internal.h`，寫入 `GetCurrentContext()->PlatformImeData`。只用在這一處，註解寫明原因。
- 風險：升級 ImGui 時，這個內部結構可能改名或改語意。
- 對照 ui_editor 的 Replacement Criteria（「需要的互動（在畫布上打字、輸入法組字）超出 ImGui 能做的」）：這次能做到，只是要用內部 API；組字顯示交給作業系統，沒有在畫布上畫。目前不算觸發條件，但已記在這裡。如果將來升級 ImGui 後做不到，或者需要在畫布上畫組字，就該停下來，請使用者決定要修補還是重建。

## 驗證

- `TestElementKindsMatchUi` 加三項檢查：每種元素的預設值帶著表列的整數與文字欄位、整數在範圍內、`styledText` 和「有 `text_color` 而沒有 `text`」一致。
- 新的 `TestElementFieldsAtTheirBounds`：整數欄位在上下限、文字欄位為空時，文件仍通過 GYO::Ui 的 codec。
- 新的 `TestPreviewTextFieldTyping`（`gyo_ui_editor.preview`）：用真的 ImGui 幀（無視窗的 SDL 軟體 renderer）依序做：
  - Enter 開始編輯，打 `ab`、Backspace、`c`，Enter 送出：動作的 string payload 是 `ac`。
  - 再編輯一次（加 `d`）送出 `acd`，證明預覽的 binding 已經更新。
- `ctest -R "gyo_ui_editor|^ui\.|gyo_ui_tests|^input|platform_sdl|os\."` 21/21 通過，沒有警告。
- 突變 7 個，全部被抓到：
  - 元素表 3 個：不設 `styledText`、`max_length` 下限改成 0、文字欄位的鍵打錯。
  - 預覽 4 個：不送打的字、不交出 `text`、不更新預覽的 binding、沒有 Backspace。

## 未做

- GUI 上手動操作：Inspector 的欄位、預覽裡打字與輸入法視窗的位置。
- 預覽沒有畫組字（由作業系統畫）。
- 輸入框的寬度不會依 `max_length` 自動調整，作者要自己調大小（契約寫明不捲動）。
