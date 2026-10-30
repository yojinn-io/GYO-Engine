# 工具 v8：v8-tool-ui-editor-element-schema（元素的知識集中到一張表）

日期：2026-10-30。分支 `claude/v8-tool-ui-editor-gizmo`。檔位 high，引擎 Opus 5.5。估 5～8 h，實際約 1 h。

**狀態：** 完成（審查 S-2）。使用者選 A：表放在 ui_editor，不由 GYO::Ui 提供。

相關文件：[ui_editor 審查](../tools/plans/v8/ui-editor-review.md) S-2，[ui_toolchain.md](../ui_toolchain.md)「Design Invariants」，[HANDOFF](../tools/plans/v8/HANDOFF.md) `v8-tool-ui-editor-element-schema`。

## 使用者的方向

- 「A：放 ui_editor」
- 實作途中補充：「我們可以通過資料來與Engine交接。我的理解是這個，Engine不需要承擔tools的職能，這樣以後tools可以隨時修改和更換。」

所以表是工具自己的，Engine 不為工具提供描述；兩邊靠 `gyo.ui` 文件（資料契約）和測試對齊。

## 做了什麼

- 新增 `ElementSchema.hpp`／`.cpp`（`gyo_ui_editor_core`，不依賴 ImGui）：
  - `ElementKind` 一種元素一筆：能否取得焦點、action 的 payload、binding 的型別、這種元素專有的顏色欄位、是否有貼圖。
  - `MakeDefaultElement`：新元素的預設欄位，原本的 `MakeElement` 逐字搬過來。
  - 從面板檔搬來 `ContainsNodeId`、`UniqueNodeId`（原 `UniqueId`）、`UniqueDeclarationId`。
- 改用表：
  - Hierarchy 的「Add element」選單與焦點順序。
  - Inspector 的焦點清單、顏色欄位、action 與 binding 的篩選、貼圖按鈕。
- 現在加一種元素，在編輯器這邊只要改 `ElementSchema.cpp`；測試裡的 switch 會強迫一起更新。
- Inspector 的顯示順序不變：「顏色來源」型的欄位（panel 的 color、image 的 tint）在共用的文字樣式之前，「顏色名稱」型的欄位（slider 的 track、fill、thumb）在背景狀態之後。這個規則由欄位的 `source` 旗標決定，不另外加順序欄位。

## 對齊測試（`TestElementKindsMatchUi`）

- 對 `Engine::Ui::UiElementType` 做 switch，沒有 default；這個測試目標開 `-Werror=switch`（MSVC 開 `/we4062`）。GYO::Ui 加了新元素而表沒跟上時，建置就會失敗。
- 每一種元素的預設元素都要：通過 GYO::Ui 的 codec；被 evaluator 排成同一種型別；可否互動和表一致；宣告的 action payload、binding 型別和表一致；帶著表列的顏色欄位；有貼圖欄位與否和表一致。

## 驗證

- `ctest -R "gyo_ui_editor|^ui\.|gyo_ui_tests|os\.header_hygiene|os\.boundary_guard"`：14/14 通過（macOS x86_64），沒有警告。
- 突變 8 個，全部被抓到：
  - slider 的 payload 改成 none
  - button 不能取得焦點
  - 表少了 fixed_step_list
  - list 的 binding 改成 number
  - panel 的顏色鍵打錯
  - image 沒有貼圖
  - slider 預設的 step 改成負數
  - 測試的 switch 少一個 case（建置失敗）

## 留下的問題

- binding 型別（`number`、`enum`、`list<object>` 等）的知識仍散在 Inspector 的 bindings 區塊。它是 schema 的另一部分，不在 S-2「元素」的範圍；等 `text_field` 需要新的 binding 型別時再看。
- GUI 上沒有手動操作；新增各種元素與 Inspector 的顯示由上面的測試和逐字搬移保證。
