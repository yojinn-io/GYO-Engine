# IU-3 的事前宣告（產品宣告使用 framework，遊戲改用共用轉換）

更新：2026-11-02。**狀態：完成**（2026-11-02）。執行：v8-input-ui-adapter 推動會話。底：master `a8a48243`（IU-1，PR #135 合併後）。Build Graph 的 Delta 已同意：[IU-3 Delta](input-ui-adapter-iu3-build-delta.md)，採 (a)。

核准前不寫程式。

## 做法

查證後發現：`components` 除了宣告需求，還把每個組件對應到一個 target（`build/cmake/GyoAppProject.cmake:113-147` 的 `gyo_app_link_components`），並檢查產品的 target 只連自己宣告過的組件。所以 `FRAMEWORK` 用同一套機制就夠，不用另寫：

1. `build/cmake/GyoComponents.cmake`：`GYO_KNOWN_COMPONENTS` 加 `FRAMEWORK`。它不需要 SDL，所以不會打開 `GYO_ENABLE_SDL3`。
2. `build/cmake/GyoAppProject.cmake`：組件對應表加 `set(FRAMEWORK GYO::ToolFramework)`。
3. `build/cmake/GyoBuild.cmake`：加入 `framework/` 與 `tests/framework` 的兩處條件，改成 `GYO_ACTIVE_TOOLS OR GYO_ENABLE_FRAMEWORK`，註解跟著改。
4. `apps/object_fps_pvp/project.json` 的 `components` 加 `"FRAMEWORK"`；`apps/object_fps_pvp/CMakeLists.txt` 在 `app_support` 的 `PRIVATE_COMPONENTS` 加 `FRAMEWORK`。
5. `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp`：`TextInput` 的本體換成 `Gyo::Tools::Framework::ToUiTextInput(text, TextShortcutModifier::Ctrl, [this] { return platform->ClipboardText(); })`。
   - 修飾鍵傳 Ctrl，和現在相同，行為不變。
   - `UiInputFrame` 的其他欄位不動，依 `v8-input-ui-adapter-iu1-frame-scope` (a)，那些是產品自己的對照。
   - 註解保留 Ui DI 7 的說法：遊戲選擇用 framework 的對照。
6. 文件：
   - `docs/framework/architecture.md` 的 DI 1（條文見下）；
   - `framework/CMakeLists.txt` 開頭與 `tests/framework/CMakeLists.txt` 開頭的註解（「so far only tools do」「when a tool is selected」）；
   - Using it 加一句「遊戲在 `project.json` 宣告 `FRAMEWORK`」；
   - 卡的進度、dev_log、v8 README 的 dev_log 表。

## DI 1 的條文（Design Invariants 節的變更，要你核准）

現行：

> It is built only while a selected product uses it (so far: when at least one tool is selected), so removing every product that uses it leaves the engine and the other products building.

改為：

> It is built only while a selected product uses it: while a tool is selected, or while a selected game declares the component `FRAMEWORK` in its `project.json` (which links `GYO::ToolFramework`), so removing every product that uses it leaves the engine and the other products building.

其他句子不變。

## 驗收（結果出來前宣告；判定事後不放寬）

1. **組件測試**（`tests/common/cmake/ComponentTests.cmake`）：
   - 新增一個案例：`gyo_resolve_components(FRAMEWORK)` 之後只有 `FRAMEWORK` 是 ON，SDL3 是 OFF。
   - 既有的「未知的組件 configure 失敗」照舊。
2. **L2 組態**（本機 Mac，`BUILD_TESTING=ON`）：
   - (a) 只選 object_fps_pvp、不選工具：configure 與建置成功；有 `gyo_tool_framework`、沒有 ImGui 綁定；`tests/framework` 加入建置，它的測試全過。
   - (b) 不選產品也不選工具：configure 成功，沒有 framework 的 target（IU-1 已有一次證據，這批重做）。
   - (c) 只選 audio_editor：configure 與建置成功，CTest 全過。
3. **Removability 探測**：
   - 在暫存副本裡，把 `project.json` 的 `FRAMEWORK` 拿掉，但保留 `PRIVATE_COMPONENTS FRAMEWORK`：configure 要失敗，訊息是「did not declare component FRAMEWORK」。這證明宣告與連結的對應是由共通層檢查的。
   - 同樣在副本裡，只選另一個不宣告 `FRAMEWORK` 的組態，framework 不加入建置。這由 2(b) 證明。
4. **遊戲不變**：
   - 全量建置成功；全量 CTest 全過，含 object_fps_pvp 既有的測試。
   - 轉換與遊戲舊表逐項相同，已由 IU-1 的 U1 證明。
5. **突變**（D184：這批改的是共通建置與遊戲的呼叫處）。共通建置的突變需要一次完整 configure，所以不放進突變目錄，以探測的方式執行、存證據，每一條都要出現預期的失敗：

| id | 改法 | 預期 |
|---|---|---|
| iu3-build-condition-dropped | `GyoBuild.cmake` 加入 `framework/` 的條件拿掉 `OR GYO_ENABLE_FRAMEWORK` | 組態 2(a) configure 失敗：「Required component FRAMEWORK target GYO::ToolFramework is unavailable」 |
| iu3-component-unknown | `GYO_KNOWN_COMPONENTS` 拿掉 `FRAMEWORK` | 組態 2(a) configure 失敗：「Unknown GYO component 'FRAMEWORK'」 |
| iu3-component-sdl | `FRAMEWORK` 被算成需要 SDL3 | 組件測試新案例失敗 |

## 範圍的限界

- `PvpApplication` 呼叫處傳的修飾鍵（Ctrl）沒有自動測試：遊戲的 `TextInput` 是私有實作。這一處靠審查與 L3 確認；遊戲在 macOS 上仍然是 Ctrl。
- 工具仍是「選了就加入 framework」，不改成也要宣告（Delta 已記錄）。object_fps_preview 不用 framework，卻也會讓它加入建置，這點維持原狀。
- 不改 `GYO::ToolFramework` 的名字。
- 組字在預覽裡的實機確認，由 IU-2 加上使用者的 macOS L3 完成。

## 模型與檔位（navigator.md）

上段。根據：Build Graph 的 Architecture Delta、framework DI 1 條文。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 実装者 | 主會話 Opus 5.5／high | 卡的分配 |
| 審査者 | Opus 5.5／high 子 Agent（唯讀） | 卡的分配；這批沒有時序邏輯，不用 xhigh |
| 検証者 | 不需要 | 這批不改任何守衛或驗收判定 |

## 時間

- 估 3～4 h：共通建置與組件測試 1 h、遊戲呼叫處 0.5 h、L2 三種組態與探測 1～1.5 h、審查與文件 1 h。
- 停止條件：超過 6 h，或單一問題累計超過 2 h。
- 證據放主 checkout 的 `build/evidence/v8-input-ui-adapter/IU-3/`。

## 執行紀錄

- 2026-11-02，使用者：「核准」（含 DI 1 條文）。開工，底是 master `a8a48243`。
- 實作照宣告。組件測試新案例第一次插在 SDL_GPU 那段連續檢查的中間，弄斷了後面的 GPU 斷言（`expected 'dxil;spirv'`）。移到 IMGUI 案例之後即通過，判定不變。
- 建置與測試（本機 Mac，證據 `build/evidence/v8-input-ui-adapter/IU-3/`）：

| 組態 | 結果 |
|---|---|
| test preset | 建置成功、CTest 123/123 |
| 2(a) 只選 object_fps_pvp | 建置成功、CTest 108/108；有 `gyo_tool_framework`、沒有 ImGui 綁定；`tests/framework` 的 4 個測試都通過 |
| 2(b) 不選產品也不選工具 | configure 成功，沒有 framework 的 target |
| 2(c) 只選 audio_editor | 建置成功、CTest 56/56 |

- **探測與突變第一次：無效（§14.1，記錄保留在 `probes_run1_invalid/`）**。
  - 原因：探測腳本每條跑完用 `git checkout -- <檔>` 還原。這會還原到 HEAD，而當時我的修改還沒提交，所以 `project.json`、`GyoBuild.cmake`、`GyoComponents.cmake` 的改動被抹掉。第 1 條之後的三條是在錯的原始碼上跑的，其中兩條顯示「SURVIVED」。這是我操作的錯，不是實作或判定的結果。
  - 處理：照原文重新套用三個檔的修改，做暫存提交 `da28f611`（之後併入正式提交 `06134fd6` 之前的同一批修改，本機已不存在）。之後每條探測都還原到這個提交，並確認工作樹和它沒有差異。判定與預期訊息都不變。
- **重跑結果**（`probes_summary.txt`）：removability-undeclared、iu3-build-condition-dropped、iu3-component-unknown、iu3-component-sdl 4/4 出現預期的失敗。還原後重新 configure 2(a) 成功。
- 審査者（Opus 5.5／high，唯讀）：0 blocker、0 major、2 minor、2 nit。
  - minor 1：GyoBuild 的新條件 L1 抓不到。L1 的 preset 都選了工具，工具已經讓 framework 加入建置。守住它的只有打包 job（遊戲產品不選工具，`build/ci/common/app_registry.py:121`）和這批的手動探測。記進 dev_log 的限界與之後的整理項目。
  - minor 2：狀態行與收尾文件（卡的進度、dev_log、README）已補上。
  - nit 1：`PvpApplication.cpp` 的 include 移出 engine 群組，自成一組。
  - nit 2（既有問題，不是這批新增的）：遊戲可以不宣告 FRAMEWORK，直接在 `PRIVATE_LIBRARIES` 寫 `GYO::ToolFramework`。有別的使用者讓 framework 加入時它照樣能建，那個使用者被移除才壞。這和 SDL 後端 target 是同一類，只記錄。
- IU-3 完成，[dev_log](../../../dev_logs/2026_11_02_pvp_v8_input_ui_adapter_iu3.zh-Hant.md)。
