# ui_editor 的審查（2026-10-30）

審查對象：`tools/ui_editor`（分支 `claude/v8-tool-line-3a892c`，`d5a97fce`）。使用者要求「給這個 UI Editor 一個評價」「原先 UI Editor 有代碼結構，你一起審查」。照 AGENTS §13，先看 Architecture、Ownership、Dependency，再看程式。只審查，沒有修改。

範圍（使用者 2026-10-30）：UI Editor 只處理遊戲選單等介面，不管 HUD（`v8-tool-ui-editor-scope-menus`）。

## 評價

| 面向 | 評價 | 根據 |
|---|---|---|
| 邊界與依賴方向 | 好 | 不連結遊戲、Input、SDL_GPU（`CMakeLists.txt`）；文件只經 `UiDocumentBridge` 進出 `GYO::Ui` 的 codec（`UiDocumentBridge.hpp`「the editor's only document seam」）；預覽用同一個 evaluator（`PreviewAdapter.hpp`）；資產目錄唯讀 |
| 資料安全 | 好 | canonical JSON、原子替換、外部修改偵測（`DocumentSession::HasExternalModification`）、拒絕輸出到掛載的資產根、快照式 undo 與拖動的交易合併（`UndoStack`） |
| 測試 | 中 | core 與預覽資源的生命週期有測試（`tests/ui_editor`）；但 `EditorApp` 的操作與 gizmo 的數學只能在 ImGui 的一幀裡跑，沒有測試 |
| 內部結構 | 要整理 | 見下面 S-1～S-4 |
| 操作 | 不足 | 只能拖錨點與 pivot，不能直接拖位置與大小（S-5） |

一句話：**外面的邊界畫得很乾淨，裡面的責任太集中。** 不需要重寫；在加 `text_field` 之前，先把會被它碰到的部分整理好。

## 發現

### S-1　`EditorApp` 一個類別做太多事

- `EditorApp.cpp` 約 2,300 行：選單、Hierarchy、Canvas、Inspector、對話框、檔案操作、資產掛載、JSON 樹的工具函式都在這裡。`DrawInspector` 一個函式約 790 行（`EditorApp.cpp:1187-1977`）；`ImGui::` 呼叫 365 處。
- 影響：改一個面板就要讀整個檔；邏輯藏在 ImGui 的一幀裡，測不到。

### S-2　元素的 schema 知識在 editor 裡又寫了一份

- 編輯直接操作 `nlohmann::json` 與字串鍵。「哪種元素有 action、binding、哪些欄位」散在 `EditorApp.cpp` 約 15 處 `type == "button"`、`"horizontal_slider"` 的比對（例如 `:293`、`:397`、`:420`、`:974`、`:1926`），和 `GYO::Ui` 的 codec 是兩份。
- 影響：加 `text_field`（`v8-tool-ui-text-field`）要改很多分散的地方，漏改一處 editor 和 codec 就不一致。這是加元件前最大的壓力。
- 判斷：這是 AGENTS §2 說的「同一個 Special Case 出現在多處」。

### S-3　`PreviewAdapter::Draw` 同時做預覽與編輯操作（2026-10-30 已處理：`v8-tool-ui-editor-gizmo-math`）

- 一個約 340 行的函式（`PreviewAdapter.cpp:220-560`）裡有：解析、啟用 canvas、組輸入、繪製、gizmo 的命中判定與拖動數學。gizmo 的數學直接讀 ImGui 的滑鼠狀態，不能單獨測。
- 影響：拖動的 bug（S-5）沒辦法先寫成失敗的測試（AGENTS §14.3 第 2 步）。

### S-4　預覽每幀把整份文件序列化再比對

- `PreviewAdapter.cpp:236`：`sourceDocument.dump()` 每幀一次，再和上一份字串比對。
- 影響：選單文件小，現在不是問題；優先度低，只記錄。

### S-5　拖動與縮放（使用者回報的 bug；**已重現**）

- 使用者（2026-10-30）：拖藍色方塊，「只能左右，往上拖動無效」。
- 現況：gizmo 只有三個把手：`anchor_min`（深藍方塊）、`anchor_max`（淺藍方塊）、`pivot`（橘色圓點）（`PreviewAdapter.hpp` 的 `GizmoField`）。**沒有拖位置（`position`）或大小（`size_delta`）的把手**；點元素本身只會選取（`EditorApp.cpp:1147`）。
- 重現：用暫時的程式直接驅動真正的 `PreviewAdapter::Draw`（ImGui 的滑鼠事件、1280×720、不開 Snap），從把手往四個方向各拖 50 px，結果如下（程式沒有進 repo）：

  | 元素的錨點 | 把手 | 上 | 下 | 左 | 右 |
  |---|---|---|---|---|---|
  | 新增元素的預設 (0.5, 0.5) | `anchor_min` | 動 | **不動** | 動 | **不動** |
  | 同上 | `anchor_max` | **不動** | 動 | **不動** | 動 |
  | 大廳的按鈕 (0, 0) | `anchor_min` | **不動** | **不動** | **不動** | **不動** |
  | 同上 | `anchor_max` | **不動** | 動 | **不動** | 動 |

- 機制（`PreviewAdapter.cpp:533-540`）：
  1. 拖 `anchor_min` 時新值被限制成 `≤ anchor_max`，拖 `anchor_max` 時限制成 `≥ anchor_min`。新增元素與大廳按鈕的兩個錨點**一開始就相等**，所以每個把手在每個軸上只能往一個方向動：`anchor_max` 往上、往左都被擋住，就是使用者看到的「往上無效」。
  2. 錨點又被夾在 0～1（`:533-534`）。大廳按鈕的錨點在 (0, 0)，`anchor_min` 四個方向都動不了。
  3. 就算動了，拖錨點也只改錨點，不補償 `size_delta` 與 `position`（`EditorApp.cpp:1161-1166`），所以元素的大小會在原本的 `size_delta` 之外再加上錨點的距離。
- 判斷：程式照它寫的規則在動，不是算錯；問題是**把手的設計和使用者想做的事不一致**：使用者想移動、縮放元素，但手上只有「錨點」這種調整版面關係的把手。
- 下一步：先定「拖動應該怎樣」（HANDOFF `v8-tool-ui-editor-gizmo-bug`），再照 AGENTS §14.3 把上表的情形寫成會失敗的測試，然後修。

## 建議的整理順序（每項都對應一個要做的 Feature）

| 代號 | 內容 | 支撐的 Feature |
|---|---|---|
| `v8-tool-ui-editor-gizmo-math` | gizmo 的數學抽成純函式＋測試；修 S-5；加位置與大小的把手，拖錨點時保持元素的矩形不動（Unity 的 RectTransform 也是這樣做） | 拖動與縮放的 bug |
| `v8-tool-ui-editor-element-schema` | 每種元素的欄位與 Inspector 集中在一處（一張表或每種元素一個檔），取代散落的字串比對 | `text_field`、之後的背景動畫 |
| `v8-tool-ui-editor-panels` | Hierarchy、Canvas、Inspector、對話框拆成各自的檔；`EditorApp` 只協調 | 上面兩項的改動範圍 |

照 AGENTS §3，整理前先寫 ui_editor 的 `Design Invariants` 與 `Replacement Criteria`（`v8-tool-ui-invariants`）。S-4 不排。
