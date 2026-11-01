# 任務卡：Input→Ui 轉換的共用化（`v8-input-ui-adapter`）

更新：2026-11-01。**狀態：已核准**（`v8-input-ui-adapter-approve`，2026-11-01，4 項照建議 (a)：adapter 放 framework 核心；產品在登錄資料宣告使用 framework；IU-1 的 PR 由 framework 整改線決定（它的回答：PR B 之後另開）；模型表照表）。開卡：使用者指示由 v8 規劃會話（本會話）開卡，串起三個 owner、排順序；來源是架構會話 `[Job][架構師]例行架構掃描` 的查證，完成後通知它（健檢追蹤項）。

這張卡只做規劃與對齊。各批由各 owner 的會話執行，不在這裡重複開批。

## 執行分配（2026-11-01 更新）

使用者指出「現在沒人處理」，由 v8 規劃會話開了推動會話「處理 Input→Ui 轉換重複與預覽輸入管線」。推動會話回報，使用者決定的執行分配如下（取代下面「分批與 owner」裡 IU-1、IU-3 的 owner）：

- **IU-1、IU-3 由推動會話做**，IU-2 由工具線做。
- IU-1 等 framework 整改線的 PR B 合併後開工，因為兩邊改同一批檔案；PR B 已做到 B4，停在 §14.1 的關卡。
- IU-3 的 Build Graph Delta（§3 的 7 點）由推動會話先起草交使用者；實作等 IU-1 合併。
- 組字的驗收只做 macOS；Windows 寫成範圍的限界。
- 之後由推動會話在它的分支更新這張卡，隨 IU-3 的 PR 進 master。

## 進度（推動會話記錄）

- 2026-11-01：IU-3 的 Build Graph Delta 已同意（[IU-3 Delta](input-ui-adapter-iu3-build-delta.md)，採 (a)：`components` 加 `FRAMEWORK`）。
- 2026-11-02：IU-1 完成（[事前宣告與執行紀錄](input-ui-adapter-iu1-declaration.md)、[dev_log](../../../dev_logs/2026_11_02_framework_v8_input_ui_adapter_iu1.zh-Hant.md)）。
  - 使用者決定 `UiInputFrame` 只共用文字轉換（`v8-input-ui-adapter-iu1-frame-scope` (a)）。第 1 件事裡的「`UiInputFrame` 也是兩份」改記為：重複的是文字轉換，已合為一份；其他欄位是各產品自己的對照（Ui DI 7）。
  - IU-1 的介面：核心 `ToUiTextInput(events, shortcut, clipboard)`（`gyo/framework/UiTextInput.hpp`）；ImGui 綁定 `ImGuiShellContext::input`（`Frame()`、`RequestTextInput(area, cursorPixels)`，視窗座標，只在 BeginFrame 或 Draw 呼叫）。
  - IU-1 的 PR 合併後通知工具線開 IU-2，推動會話接著做 IU-3。
- 2026-11-02：IU-1 合併（PR #135，`a8a48243`）。工具線開了 IU-2 的 PR #136，等使用者核准守衛收緊、做 macOS L3、合併。
- 2026-11-02：IU-3 完成（[事前宣告與執行紀錄](input-ui-adapter-iu3-declaration.md)、[dev_log](../../../dev_logs/2026_11_02_pvp_v8_input_ui_adapter_iu3.zh-Hant.md)）。
  - 遊戲在 `project.json` 宣告組件 `FRAMEWORK`，共通建置只在有工具或有產品宣告時加入 framework（framework DI 1 條文已改）。
  - `PvpApplication` 改用 `ToUiTextInput`，傳 Ctrl，行為不變。

## 要處理的三件事（使用者給的原文）

1. 同一個轉換寫了兩份。遊戲把 `Input::TextEvent` 轉成 `UiTextInput`（`PvpApplication.cpp:268-289`）；編輯器因為不能用 Input，從 `ImGuiKey` 另外寫一張一樣的表（`PreviewAdapter.cpp:234-256`）。`UiInputFrame` 也是兩份（`PvpApplication.cpp:1149`、`PreviewAdapter.cpp:345`）。這是 AGENTS §2 的「同じ Adapter／Conversion の繰り返し」。
2. 編輯器伸手進 ImGui 內部。`PreviewAdapter.cpp:11-13` 用 `imgui_internal.h` 的 `PlatformImeData` 要求文字輸入，因為 Engine 的 `SdlInput::SetTextInputArea` 不給用。這正是你說的「某天不用 ImGui 就得大改」。
3. 預覽走的不是遊戲的輸入管線。組字事件（`v8-engine-input-composition`）在遊戲有、在編輯器預覽沒有，預覽的保真度打了折。

## 現況（master `654df310`，本會話查證）

- **原因已經解決**：
  - 兩個工具的 DI 1 改寫，隨 #131 合併（T4，`863f6325`）；
  - framework DI 2 的改寫，隨 #133 合併。
  - 現在工具與 framework 都可以用任何 Engine 模組（含 `GYO::Input`），SDL 只經 Engine 或 framework。守衛已經加上 `GYO::Input`。
- **上面三件事目前沒有 owner。**
- framework DI 2 已寫下約束「ImGui 綁定經 `GYO::InputBackendSDL` 讀文字與組字，不從 SDL 事件讀」，**尚未實作**。framework 整改卡把它標成「另問使用者」；它的 PR B 尚未開工。
- **兩個會改變做法的事實**：
  - **framework 只在選了工具時才加入建置**：`build/cmake/GyoBuild.cmake:53-59`，framework DI 1。所以遊戲要改用 framework 的 adapter，必須先讓產品能宣告「我用 framework」。這是 Build Graph 的 Architecture Delta，和 framework 外殼草圖第 6 節給「pvp Client 搬上外殼」列的第 1 項相同。
  - **Ui 的 Design Invariant 7**：`UiTypes.hpp:132-133` 寫「哪個鍵代表哪種編輯，是產品的對照；每種編輯怎麼改文字，是 Ui 的」。把對照搬進 framework，要確認它仍然符合 DI 7：對照不進 Ui，由產品選擇是否用 framework 的那份。

## 使用者的規則（原話）

「Editor 可以連結 framework 和 Engine，但是 framework 是通用層，共通功能優先 framework。如果有觀察到有多處使用，即使 Engine 有也先實作 framework，再調用。」

## 分批與 owner

```text
v8-input-ui-adapter（本卡：規劃與對齊）
├── IU-1 framework  「上段｜framework 整改線｜核心放 Input→Ui adapter；ImGui 綁定交出 InputBackendSDL 的幀；守衛加 GYO::Input／GYO::Ui」
├── IU-2 tools      「上段｜工具線｜預覽改吃 adapter 的幀、呼叫 SetTextInputArea；刪掉 imgui_internal 與 ImGuiKey 表」
└── IU-3 product    「上段｜本線（pvp）｜產品宣告使用 framework；PvpApplication 改用 adapter」
```

| 代號 | owner（會話） | 估時（推論） | 依賴 | 改的東西 |
|---|---|---|---|---|
| IU-1 | framework 整改線（Rework framework into product shell and sink file service） | 6～10 h | 本卡核准 | `framework/` 核心的 adapter（只依 `GYO::Input`、`GYO::Ui`，不碰 ImGui）；ImGui 綁定經 `InputBackendSDL` 取幀並交出；核心守衛的正面清單 |
| IU-2 | 工具線（v8 tool-line） | 4～6 h | IU-1 | `tools/ui_editor/src/PreviewAdapter.cpp`；ui_editor 守衛禁止 `imgui_internal.h` |
| IU-3 | 本線 | 3～5 h＋建置的 Delta | IU-1、`v8-input-ui-adapter-pvp-build` | `GyoBuild.cmake` 的加入條件（或產品登錄資料）；`PvpApplication.cpp` 的 `TextInput` 與 `UiInputFrame` |

- **IU-1 的介面（工具線的建議，2026-11-01）**：`SetTextInputArea` 在 `SdlInput`（`GYO::InputBackendSDL`）上，IU-1 又讓 ImGui 綁定持有它的幀。所以由 framework 交出兩樣東西：「設定文字輸入區域」與「取 adapter 的幀」，例如放進 `ImGuiShellContext`，ui_editor 只呼叫 framework。
  - ui_editor 因此不必自己連 `InputBackendSDL`，也不會另持一份 `SdlInput`，符合「共通功能優先 framework」。
  - 如果改成 ui_editor 直接連 `InputBackendSDL`，工具守衛要加這個目標（§14.4 的放寬），估時也要加。
- **IU-2 的守衛**：現在的 ui_editor 守衛會略過不含 `/` 的 include（`tests/ui_editor/ui_editor_dependency_guard.py` 的 `if "/" not in include: continue`），抓不到 `#include "imgui_internal.h"`。IU-2 要加一條具名規則，屬收緊，照 §14.4 事後報告。`PreviewAdapter.cpp` 因為用 SDL_Texture 畫預覽，留在守衛的 `NATIVE_FILES`，IU-2 不影響這點。
- 順序：IU-1 先做；IU-2 與 IU-3 改的檔案不重疊，可以並行。
- IU-1 由 framework 整改線決定，要併進 PR B 還是排在它之後（`v8-input-ui-adapter-pr`）。
- 和其他工作的關係：
  - ui_editor 的「日文輸入法確定後文字遺失」（PR #132 的會話）同樣在 ImGui 綁定的文字路徑上。IU-1 的 framework 側要和它對齊，不要兩邊各修一次。
  - 本線的 P2（GUI probe 瞄軀幹）改的是 `build/acceptance`，和本卡不重疊。

## 模型與檔位（navigator.md）

| 批 | 段 | 根據 | 実装者 | 審査者 | 検証者 |
|---|---|---|---|---|---|
| IU-1 | 上段 | Design Invariants（framework DI 1、2 的守衛）、公開介面（新的 adapter） | Opus 5.5／high | Opus 5.5／xhigh | Opus 5.5／high（守衛變更的 §14.4；檢查第 4 點時也不用小模型） |
| IU-2 | 上段 | 工具 DI 1 的守衛變更 | Opus 5.5／high | Opus 5.5／high | Opus 5.5／high |
| IU-3 | 上段 | Build Graph 的 Architecture Delta、Ui DI 7 的確認 | Opus 5.5／high | Opus 5.5／high | Opus 5.5／high |

各批開工時，由執行的會話照自己的計畫補上，經使用者確認。

## 驗收候選（各批的宣告再定）

- **同一個 adapter**：同一串 `TextEvent`（含 Composition、Ctrl+A、Ctrl+V、方向鍵、Enter、Escape）進去，遊戲與預覽兩邊產生的 `UiTextInput` 相同。
- **守衛**：
  - `PreviewAdapter` 不再 include `imgui_internal.h`（ui_editor 守衛可守）；
  - framework 核心的 adapter 不 include ImGui。
- **組字**：預覽收得到 Composition。要實機 L3 的日文輸入法，和 #132 的輸入法調查一起看。
- **遊戲不變**：遊戲的文字輸入行為不變（既有的 pvp 測試，加上一個 adapter 與舊表逐項相同的對照測試）。

## 等你決定

### `v8-input-ui-adapter-home`：adapter 放哪

- (a) framework 核心（建議）。
  - 理由：照使用者的規則（多處使用，先實作在 framework）；架構會話也這樣建議。
- (b) Engine 的 Ui 模組。
  - 遊戲不必依賴 framework，但違反「先實作 framework」的規則；也要把「哪個鍵代表哪種編輯」放進 Ui，和 Ui DI 7 衝突。

### `v8-input-ui-adapter-pvp-build`：遊戲怎麼用到 framework

- (a) 產品在登錄資料裡宣告「使用 framework」，`GyoBuild.cmake` 改成「有工具，或有產品宣告」時加入 framework（建議）。
  - 屬 Build Graph 的 Architecture Delta，IU-3 開工前照 §3 的 7 點提出。
  - 理由：外殼草圖第 6 節，pvp Client 搬上外殼，也需要同一個改動；先做這一步，之後搬外殼不必再改。
- (b) 這次只有編輯器用 adapter，遊戲保留自己那份。
  - 不動 Build Graph，但「同一個轉換兩份」只解決一半，還多了一個要對齊的版本。

### `v8-input-ui-adapter-pr`：IU-1 放哪個 PR

- (a) 交給 framework 整改線決定：併進 PR B，或排在它之後（建議）。理由：那條線最清楚 PR B 的範圍。
  - framework 整改線的回覆（2026-11-01）：建議 IU-1 **排在 PR B 之後、另開 PR**，不併入。理由：PR B（`v8-framework-invariants`）只刪舊件、拿掉守衛的例外清單、文件定稿，混入新功能會讓 §14.4 的判定變更和行為變更攪在一起。PR B 和 Input→Ui 沒有衝突。核心清單加 `GYO::Ui` 仍是守衛變更，照 §14.4 交使用者核准。
- (b) 本卡指定獨立的 PR。

### 模型表

- 照上表（三批都是上段，Opus 5.5；IU-1 的審查用 xhigh）（建議）。
- 或 IU-1 的審查改用 Fable 5.1（比主會話強，要你同意）。

## 範圍的限界

- 本卡不做「pvp Client 搬上外殼」：只做 IU-3 需要的「產品能用 framework」，外殼的採用另外決定。
- IU-1 的估時與做法由 framework 整改線在自己的宣告裡定。這裡的估時只是推論。
- 組字在預覽裡的實機確認要人在 macOS／Windows 上用輸入法操作；CI 只跑 `cpu|shader` 的測試。
