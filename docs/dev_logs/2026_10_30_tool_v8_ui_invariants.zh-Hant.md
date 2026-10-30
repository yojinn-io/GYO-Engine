# 工具 v8：v8-tool-ui-invariants（GYO::Ui 與 ui_editor 的 Design Invariants）

日期：2026-10-30。分支 `claude/v8-tool-ui-editor-gizmo`。檔位 medium，引擎 Opus 5.5。估 2～3 h。

**狀態：** 完成。使用者同意草稿（「同意，寫進去」），寫進 [ui_toolchain.md](../ui_toolchain.md)「Design Invariants」，並加三個守衛。

相關文件：[工具線 README](../tools/plans/v8/README.md)「順序與依賴」（`v8-tool-ui-order`）、[HANDOFF](../tools/plans/v8/HANDOFF.md)。

## 做了什麼

- `ui_toolchain.md` 新增 `Design Invariants` 節：
  - `GYO::Ui` 4 條：不懂動作的意思、不依賴 `GYO::Input`、元素集合封閉、不持有遊戲狀態。
  - ui_editor 5 條：不連結遊戲／Input／SDL_GPU、文件只經 bridge 與 codec、資產目錄唯讀、矩形來自 evaluator、預覽不執行動作。
  - 另有 `Replacement Criteria`。
- 守衛：
  - `ui.dependency_guard`（`tests/common/ui/ui_dependency_guard.py`）：`engine/ui` 底下沒有 include `engine/input/`、CMake 沒寫 `GYO::Input`。附自測 `ui.dependency_guard_selftest`。
  - `gyo_ui_editor.dependency_guard`（`tests/ui_editor/ui_editor_dependency_guard.py`）：用正面清單檢查 include 前綴與 CMake 目標，另外禁止 `SDL_*GPU*`。守衛裡不寫任何產品名（AGENTS §7、§9），遊戲的標頭因為不在允許的前綴裡而被擋下。附自測 `gyo_ui_editor.dependency_guard_selftest`。
  - `TestGizmoAgreesWithUiEvaluator`（`gyo_ui_editor.core`）：4 種 rect（父層不是根）在 scale 1 與 2 下各拖 6 種把手，結果交給 `GYO::Ui` 的 evaluator 重新排版。拖錨點與 pivot 時矩形必須不動，移動與縮放時矩形必須照拖動的量變。期待值只用 evaluator 的輸出，不用 `GizmoMath` 的公式。

## 為什麼有第三個守衛

`GizmoMath` 為了反算 `position`／`size_delta`，自己寫了一份 `gyo.ui` 的版面公式。引擎的 `ResolveDesignRect` 在 `UiRuntime.cpp` 的匿名命名空間裡，不對外公開。ui_editor 不變式 4 允許這一份副本只用來反算，條件是由守衛保證它和 evaluator 一致。另一個做法是把引擎的函式公開，但那要改 Engine，範圍比較大，這次沒做。

## 驗證

- 兩支依賴守衛在現在的 repo 上通過；自測各 4、5 項通過。
- 在真實的 repo 裡植入違規，兩支都失敗：在 `engine/ui/src/UiRuntime.cpp`、`tools/ui_editor/src/GizmoMath.cpp` 加 `#include "engine/input/InputFrame.hpp"`；在 `tools/ui_editor/CMakeLists.txt` 加 `GYO::Input`。檢查完已還原。
- `GizmoMath` 的公式突變 3 個，evaluator 守衛全部抓到（括號內：evaluator 守衛的失敗數／全部失敗數）：
  - 版面公式的參考點用中心不用 pivot：6／6。
  - 反算 `size_delta` 時忽略錨點的跨度：6／7。
  - 父層大小不除以 design scale：8／8。
- 第一次跑 evaluator 守衛時，`GYO_ASSERT` 失敗：測試沒有先 `ActivateCanvas`。這是測試寫錯，不是程式的錯，補上後通過。
- 整個建置目錄重建後，`ctest -R "gyo_ui_editor|^ui\.|gyo_ui_tests|os\.header_hygiene|os\.boundary_guard"`：14/14 通過（macOS x86_64）。建置的警告都在別的檔（`ModelTests.cpp`、連結器的重複程式庫），這批的檔沒有警告。

## 未做

- 不變式 3（元素集合封閉）、4（不持有遊戲狀態）、ui_editor 2、3、5 沒有新守衛。2、3 已有既有測試涵蓋（外部修改偵測、拒絕輸出到資產根），其他的由 code review 守。
