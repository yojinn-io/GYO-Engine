# 工具 v8：v8-tool-ui-text-field（gyo.ui v2、text_field、通用編輯狀態）

日期：2026-10-30。分支 `claude/v8-tool-ui-ime-observe`。檔位 high（計畫上編輯狀態局部 xhigh；這次沒有另外開 xhigh 的審查，見「未做」）。引擎 Opus 5.5。估 10～16 h，實際約 15 分鐘（22:27～22:41，牆上時間）。

**狀態：** 完成（Mac 上的測試與突變）。產品（大廳）還沒改用 `text_field`；Windows 實機（L3）未做。

相關文件：[ui_toolchain.md](../ui_toolchain.md)「Design Invariants」（GYO::Ui 4～7）與「JSON v2」，[工具線 README](../tools/plans/v8/README.md)，[HANDOFF](../tools/plans/v8/HANDOFF.md) 的 `v8-tool-ui-schema-version`、`v8-tool-ui-edit-policy-owner`、`v8-tool-ui-widget-scope`、`v8-tool-ui-invariants-text`。

## 做了什麼

- **契約（gyo.ui v2）**：
  - 讀取端接受版本 1 與 2，其他版本明確拒絕。
  - `UiDocument::version` 照原樣寫回；在程式裡建立的文件預設是 1，所以既有的輸出不變。
  - `string` payload 與 `text_field` 只能用在 v2。
  - `text_field` 的欄位：`binding`（string）、`action`（string payload）、`max_length`（以 code point 計，至少 1）、`placeholder`＋`placeholder_color`、`background`（normal／focused／pressed＝編輯中）、文字樣式（垂直預設置中）。
- **型別（全部只加）**：
  - `UiActionPayloadType::String`、`UiElementType::TextField`
  - `UiActionPayload` 多了 `std::string`
  - `UiTextInput`／`UiTextInputKind`、`UiInputFrame::text`
  - `UiInteractionState::editingElement`
  - `UiTextCaret`／`UiTextDraw::caret`
  - `UiTextFilter`／`UiRuntime::SetTextFilter`
  - UTF-8 工具：`UiCodePointCount`、`UiPreviousCodePoint`、`UiNextCodePoint`、`UiSingleLineText`
- **編輯（`UiRuntime`）**：
  - 開始編輯：focused 時按 activate，或點輸入框，就從 binding 的值複製一份開始編輯。
  - 輸入：依序套用 `UiInputFrame::text`。插入的文字先經產品的檢查函式，再整理成單行、完整的 code point，然後截到 `max_length`。
  - 送出：activate 送出 string 動作。
  - 丟棄編輯內容：cancel（這時不送 canvas 的 cancel 動作）、焦點移走、點到別處、binding 的值被改。
  - 編輯中，滑鼠移動不改焦點。
- **繪製**：
  - runtime 把組字插在游標處顯示，交出 `UiTextDraw::caret`。
  - renderer 用「游標前那段文字」的實際寬度，畫出選取、組字底線與游標。寬度是把那段文字 rasterize 後量出來的，結果也會快取。
  - 空的輸入框也會畫游標。
  - 文字裁到輸入框內，不捲動。
- **連帶**：
  - ui_editor 的元素表加了 `text_field`。對齊測試的 switch 先讓建置失敗，守衛照設計發揮作用。
  - 預設元素會把文件升到 v2，並宣告 string 的動作與 binding。
  - 舊產品 object_fps 的 payload 名稱 switch 補上 `string`。
- **文件**：`ui_toolchain.md` 加「JSON v2」節；Design Invariants 4～6 的「Guard」補上實際的測試名稱（條文內容不變）。

## 設計上的決定（這一批自己定的，供審查）

- 送出與取消沿用 `UiInputFrame` 既有的 `activatePressed`／`cancelPressed`，不在 `text` 裡另設 Submit、Cancel，免得一次 Enter 被當成兩件事。
- 哪個鍵代表哪個編輯動作，由產品轉換（像現在 Tab 由產品轉成 `focusNextPressed`）；每個動作對編輯內容做什麼，由 Ui 決定。這符合 `v8-tool-ui-edit-policy-owner`。
- 選取只有「全選」（通用集合）。Shift＋方向鍵的範圍選取不在集合內。
- 候選窗的位置（`SetTextInputArea`）由產品用 `EvaluateLayout` 找到輸入框的矩形後設定；Ui 不碰平台（不變式 7）。

## 驗證

- 新測試（`tests/common/ui/UiTextFieldTests.cpp`，7 個 test case）：
  - UTF-8 工具
  - v2 的讀、驗證、寫與 canonical 穩定
  - 拒絕不合契約的輸入框（8 種）
  - 編輯與送出（游標、刪除、全選、`max_length`、單行）
  - 不變式 5 的守衛（檢查函式）
  - 不變式 6 的守衛（組字中按 Enter、cancel、焦點移走）
  - 不變式 4（binding 被改、滑鼠移動、點別處）
  - placeholder 與編輯中的繪製
- renderer：`UiRenderer draws a text field's selection, composition underline and caret`，用每個 code point 8 px 的假 rasterizer 檢查位置。
- `gyo_ui_tests` 3,410 個斷言全過；`ctest -R "gyo_ui_editor|^ui\.|gyo_ui_tests|^input|platform_sdl|os\."` 21/21 通過。wide 建置（含 PvP、ui_editor）沒有錯誤；`ctest -L cpu` 97/97 通過（macOS x86_64）。
- 突變 14 個，全部被抓到：
  - 不經檢查函式
  - 組字寫進編輯內容
  - 送出時帶上組字
  - 不檢查 binding 是否被改
  - 滑鼠移動搶走焦點
  - cancel 繼續往下走
  - 沒有 `max_length`
  - Backspace 只刪一個 byte
  - 全選後插入不取代
  - 控制字元保留下來
  - 丟棄編輯後同一幀又開始編輯
  - 驗證不檢查 v2：第一次存活。原因是「v1 文件」的測試先被 string payload 的版本檢查擋下，補了「action 改成 none 的 v1 文件」這一例後被抓到。
  - 游標畫在組字開頭
  - 底線畫在頂端
- 因為測試是在實作之後寫的，沒有 Red 的步驟；檢出力改由上面的突變證明（AGENTS §14.2）。

## 經過

- 測試抓到實作的錯：binding 被改的同一幀按 Enter，舊的編輯內容被丟掉之後，又因為 activate 立刻開始新的編輯。改成「這一幀丟過編輯內容，就不開始新的」。測試的期待是對的，所以修實作、不動測試。

## 改了的測試判定（AGENTS §14.4）

`UiDocumentCodec rejects unsupported schema and version`（既有）：

1. 守的性質：讀取端明確拒絕不支援的版本。
2. 放寬的面：以前算失敗的「version 2」，現在通過，因為 v2 是這一批核准的契約（`v8-tool-ui-schema-version`）。
3. 收緊的面：新增 version 0 與 3 必須被拒絕（以前只測 2）。
4. 「拒絕不支援的版本」仍由同一個測試守著（0 與 3）；v1 文件拒絕 v2 才有的東西，由新的拒絕測試守著。
5. 根據：使用者的決定 `v8-tool-ui-schema-version`。

不是受理判定，也不是 Design Invariants 的守衛。

## 未做

- 大廳改用 `text_field`（`v8-product-lobby-text-field`，產品批）：
  - 產品要把 Input 的文字事件轉成 `UiTextInput`，打開 `imeComposition`，設定 `SetTextInputArea`，再用 `FilterAddressText` 當檢查函式。
  - Windows L3 在這一批之後做。
- ui_editor 對 `text_field` 的編輯與預覽（`v8-tool-ui-editor-text-field`）。現在只有元素表與預設值。
- 計畫寫著「編輯狀態局部 xhigh」。這次沒有另開 xhigh 的審查 Agent，因為它需要使用者同意，而我用突變代替了。要不要補一次對抗式審查，交給使用者決定。
