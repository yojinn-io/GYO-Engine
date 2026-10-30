# 工具 v8：v8-tool-ui-editor-gizmo-fix（拖動與縮放改成 Unity 式）

日期：2026-10-30。分支 `claude/v8-tool-ui-editor-gizmo`（切自 master `2d78b753`）。檔位 high，引擎 Opus 5.5。估 3 h，實際約 1 h。

**狀態：** 完成。修好使用者回報的「往上拖動無效」（審查 S-5），並加上移動與縮放的把手。GUI 上的手動拖動**還沒做**。

相關文件：[ui_editor 審查](../tools/plans/v8/ui-editor-review.md) S-5，[工具線 HANDOFF](../tools/plans/v8/HANDOFF.md) `v8-tool-ui-editor-gizmo-bug`。

## 做了什麼

- 使用者選 (a) Unity 式。規則寫在 `GizmoMath.hpp` 的 `DragGizmo` 註解：
  - 本體（`Move`）：只改 `position`。
  - 四邊、四角：改 `size_delta` 與 `position`，錨點不動；一邊不會越過對邊。
  - 錨點：被拖的錨點推著另一個走。相等時往對方那側拖就一起動，往反方向拖就分開。錨點仍限制在 0～1。
  - 拖錨點與 pivot 時，重算 `position` 與 `size_delta`，元素的矩形不動。
  - Snap：只對齊這次拖動的座標。元素的邊與錨點以父層的角為原點、按設計像素對齊；pivot 按元素自己的像素對齊。
- 介面：`GizmoField` 換成 `GizmoHandle`（12 個把手）；新增 `GizmoRect`（gyo.ui 的五個 rect 欄位）。`DragGizmo` 從拖動開始時的狀態與滑鼠位移計算，不再用滑鼠的絕對位置，所以按住時不會跳。
- 本體只在「滑鼠下最上層的元素就是選中的元素」時才抓得到，點選中元素裡的子元素仍然是選取子元素。
- `PreviewAdapter`：記住拖動開始時的狀態與滑鼠位置；畫四個角的小方塊；滑鼠在把手上時換游標（移動、左右、上下、斜向）。`FrameOutput::GizmoEdit` 改成帶整個 rect。
- `EditorApp`：一次寫回五個 rect 欄位；undo 標籤用 `move`、`resize`、`anchor_min`、`anchor_max`、`pivot`。沒有動到的值寫回後不變，所以只點不拖不會留下 undo 紀錄（`UndoStack::CommitTransaction` 比對前後）。
- `tools/ui_editor/README.md` 加一段操作說明。

## 驗證

- Red：新測試先跑在一個暫時的實作上，那個實作用新介面包著舊規則：23 項失敗，包含「相等的錨點從 anchor_max 往上一起移動」，也就是使用者回報的現象。換成新規則後全部通過（Green）。
- 新測試（`EditorCoreTests.cpp`）：`TestGizmoHit`、`TestGizmoAnchorDrag`、`TestGizmoMoveAndResize`、`TestGizmoSnapAndPivot`。期待值從 gyo.ui v1 的版面規則推出（測試裡的 `DesignRect` 與 `UiLayout` 用同一條公式），不是從實作抄來。
- `ctest -R "gyo_ui_editor|os.header_hygiene|os.boundary_guard"`：9/9 通過（macOS x86_64）。建置沒有錯誤，也沒有警告。
- 突變 8 個，全部被抓到：
  - 不推另一個錨點
  - 拖錨點不補償
  - 拖 pivot 不補償
  - 移動時兩個軸都對齊
  - 邊可以越過對邊
  - 縮放不乘 pivot
  - 本體不檢查最上層
  - 位移不除以 design scale

## 改了的測試判定（AGENTS §14.4）

- 移除 `TestGizmoDragCurrentRule`：它是 `v8-tool-ui-editor-gizmo-math` 寫的特徵測試，記錄的就是這個 bug（「anchor_max 往上不動」），註解寫明修 bug 時要改。它不是受理判定，也不是 Design Invariants 的守衛。
  1. 守的性質：重構時不改行為。
  2. 放寬的面：以前算失敗的「錨點相等時往兩個方向都能動」，現在通過。這是使用者選定的修法。
  3. 收緊的面：以前通過的「anchor_max 往上不動」，現在算失敗。
  4. 錨點限制在 0～1 的檢查留著（`anchors stay inside the parent`、`anchor max stops at the parent's right edge`）。
  5. 根據：使用者選擇「(a) Unity 式」；證據是上面的 Red／Green 與突變。
- `TestGizmoDragSnapAndPivot` 中「沒拖的軸也被對齊」（scale 2 時 y 從 0.5 變 0.5111）不再成立，改成只對齊被拖的軸（`snap rounds only the moved axis of the body`）。這修掉上一批記下的新發現。
- 把手的位置、命中半徑、同距離時後者優先：判定照舊保留。角的把手比 pivot 把手近時，角優先（新判定）。

## 未做與限制

- 沒有在 GUI 上手動拖過。規則與預覽的接線已編譯並通過測試，但 `PreviewAdapter` 的滑鼠部分沒有自動測試。
- 沒做 Unity 的「按住 Alt 從中心縮放」「按住 Shift 等比例」。
- 拖動中滾輪縮放或平移畫布時，位移仍以開始時的螢幕座標計算，結果會有偏差。
