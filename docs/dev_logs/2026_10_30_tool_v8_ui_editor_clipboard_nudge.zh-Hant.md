# 工具 v8：v8-tool-ui-editor-clipboard、v8-tool-ui-editor-nudge（複製貼上、方向鍵微調、Alt／Shift 縮放）

日期：2026-10-30。分支 `claude/v8-tool-ui-editor-small`（切自 master `e31dfdf4`）。檔位 high／medium，引擎 Opus 5.5。估 3～5 h＋2～3 h，兩批合計實際約 1 h。

**狀態：** 完成。使用者：「先做小功能」。GUI 上**還沒手動操作過**。

相關文件：[工具線 README](../tools/plans/v8/README.md)「順序與依賴」（`v8-tool-ui-order`）、`tools/ui_editor/README.md`。

## v8-tool-ui-editor-clipboard

- 新增 `ElementClipboard.hpp`／`.cpp`（`gyo_ui_editor_core`，純 JSON，不依賴 ImGui）：
  - `CopyElement`：複製選中元素的子樹。
  - `PasteElement`：貼在選中元素的後面（同一層）；沒選元素、或選中的不在這個 canvas 時，貼到 canvas 最後。
    - 子樹裡每個 id（含清單的 template）改成文件中沒用過的 `<id>_copy_<n>`，沿用複製 canvas 的規則。
    - 可取得焦點的元素加到 canvas 焦點順序的最後。可否取得焦點由 `ElementSchema` 的表決定。
- 改 id 的幾個函式（`CollectAllNodeIds`、`RegenerateNodeIds`）和 `FindParentChildren` 從 Hierarchy 搬到 `EditorDocumentEdits`；`EditorDocumentEdits.cpp` 從 GUI 的程式庫改放 core。複製 canvas 和貼上用的是同一份。
- 編輯器：
  - Edit 選單加 Copy／Paste／Duplicate Element；快捷鍵 Ctrl+C／V／D，打字時不觸發（`WantTextInput`）。
  - 每次貼上或複製一筆是一步 undo。
  - 剪貼簿是編輯器自己的，不經系統剪貼簿，在同一次執行中換文件仍可貼上。

## v8-tool-ui-editor-nudge

- 方向鍵：Canvas 視窗有焦點、選了元素時，`position` 移動 1 設計像素（Shift：10）。按住連發時算一步 undo（放開才結束交易）。
- Canvas 視窗加 `ImGuiWindowFlags_NoNavInputs`：編輯器開了鍵盤導覽（`NavEnableKeyboard`），否則方向鍵也會在 Canvas 的勾選框和按鈕之間移動焦點。代價是 Canvas 視窗的控制項不能再用鍵盤導覽，它們本來就是滑鼠操作的。
- `GizmoMath::DragGizmo` 加 `GizmoModifiers`：
  - Alt（`fromCenter`）：被拖的那一邊怎麼變，對邊就反向變，以中心為準，最小到 0。
  - Shift（`keepAspect`，只對角）：兩軸用同一個倍率，取變化較大的那一軸，對角不動。
  - Snap 先作用在被拖的那一邊，再套用上面兩個修飾鍵。

## 驗證

- 新測試：
  - `TestElementClipboard`：貼上的位置、id、焦點順序、巢狀、template；每次貼上後文件都要能通過 GYO::Ui 的 codec。
  - `TestGizmoResizeModifiers`：7 種情形。
- `ctest -R "gyo_ui_editor|^ui\.|gyo_ui_tests|os\.header_hygiene|os\.boundary_guard"`：14/14 通過（macOS x86_64），沒有警告。
- 突變 8 個，全部被抓到：
  - 貼在前面
  - 不加焦點順序
  - 所有元素都加焦點順序
  - 改 id 時跳過 template
  - Shift 取較小的變化
  - Alt 只變一邊
  - Shift 時對角不固定
  - Alt 不夾到 0

## 未做與限制

- GUI 上沒有手動操作。方向鍵、快捷鍵、修飾鍵的接線只經過編譯，鍵盤事件沒有自動測試。
- 貼上的元素和原本的元素位置相同、疊在一起（Unity 的做法也一樣），要自己移開。
- 貼到別的文件時，元素參照的 action、binding、顏色如果在那份文件裡沒有宣告，會出現在 Diagnostics，不會自動補上宣告。
