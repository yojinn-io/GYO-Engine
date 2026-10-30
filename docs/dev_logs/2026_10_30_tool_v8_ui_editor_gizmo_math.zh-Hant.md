# 工具 v8：v8-tool-ui-editor-gizmo-math（S-3：gizmo 數學抽成純函式）

日期：2026-10-30。分支 `claude/v8-tool-line-3a892c`。檔位 high，引擎 Opus 5.5。估 3～5 h。

**狀態：** 完成。行為不變的重構；拖動的 bug（審查 S-5）**沒有修**，修法等使用者決定（HANDOFF `v8-tool-ui-editor-gizmo-bug`）。

相關文件：[ui_editor 審查](../tools/plans/v8/ui-editor-review.md) S-3、S-5，[工具線 HANDOFF](../tools/plans/v8/HANDOFF.md)。

## 做了什麼

- 新增 `GizmoMath.hpp`／`.cpp`（`gyo_ui_editor_core`，不依賴 ImGui）：`GizmoField`、`GizmoFrame`（父層與元素的螢幕矩形、錨點、pivot）、`ComputeGizmoPoints`、`HitGizmoHandle`、`DragGizmoHandle`。
- `PreviewAdapter::Draw` 的 gizmo 部分改呼叫上面三個函式；繪製與 ImGui 的滑鼠狀態（按下、放開、作用中的把手）留在 `PreviewAdapter`。`FrameOutput::GizmoField` 改成指向新列舉的別名，`EditorApp` 不用改。
- 數學逐行搬移，沒有改規則（偏移 ±9 px、命中半徑 10 px、同距離時後者優先、snap、夾在 0～1、`anchor_min ≤ anchor_max`）。

## 驗證

- 新測試（`EditorCoreTests.cpp`）：`TestGizmoHandlesAndHit`、`TestGizmoDragCurrentRule`（把審查 S-5 的重現表寫成目前規則的特徵測試，標明修 bug 時照 AGENTS §14.4 改判定）、`TestGizmoDragSnapAndPivot`。
- `ctest -R "gyo_ui_editor|os.header_hygiene|os.boundary_guard"`：9/9 通過（macOS x86_64）。
- 端到端：重構前後各跑一次暫時的重現程式（驅動真正的 `PreviewAdapter::Draw`，16 種拖法），輸出逐字相同。程式留在會話的 scratchpad，沒有進 repo。
- 突變 8 個，全部被抓到：拿掉 min／max 限制、拿掉夾值、同距離改前者優先、snap 不乘 design scale、pivot 改用父層為基準、忽略把手偏移、命中半徑改 12 px。

## 經過

- 第一次跑測試失敗 1 項：我預期「scale 2 時 y 不動」，實際 y 從 0.5 變 0.5111。原因是 16 px 的格子下 360 是 22.5 格，四捨五入到 368。這是測試預期算錯，不是程式的錯；照實際規則改了預期，並在測試註解寫明「沒拖的軸也可能被 snap 移動」。
- 這是既有行為的一個新發現：開 Snap 時，拖錨點的一個軸，另一個軸也可能被對齊到格子上。記在審查 S-5 之後修法時一併考慮。
