# IU-3 的 Build Graph Delta（`v8-input-ui-adapter-pvp-build`）

更新：2026-11-01。**狀態：已同意**（`v8-input-ui-adapter-pvp-build-form`，2026-11-01，使用者：「同意 (a)」）。DI 1 改寫的條文全文在 IU-3 的事前宣告交使用者核准。起草：v8-input-ui-adapter 推動會話（使用者 2026-11-01 決定 IU-1、IU-3 由本會話執行）。卡：`docs/object_fps_pvp/plans/v8/input-ui-adapter-card.md`（分支 `claude/pvp-v8-after-pv8`，commit `9725f9d9`）。查證的基準：master `654df310`。

這份只決定「遊戲怎麼讓 framework 加入建置」。`PvpApplication` 改用 adapter 的實作要等 IU-1 合併，另寫事前宣告。

## AGENTS §3 的 7 點

1. **需要它的 Feature**：遊戲與 ui_editor 預覽共用同一個 Input→Ui adapter（卡的第 1 件事）。核准的做法是 adapter 放 framework 核心（`v8-input-ui-adapter-home` (a)），所以遊戲要連 framework。
2. **現在的問題**：`build/cmake/GyoBuild.cmake:53-59` 只在選了工具（`GYO_ACTIVE_TOOLS` 非空）時才加入 `framework/`，`tests/framework` 也一樣（`GyoBuild.cmake:91-94`）。只選遊戲的組態（例如 CI 的遊戲建置、發版）沒有 `GYO::ToolFramework`，遊戲連不到。
3. **變動的邊界**：
   - Build Graph：framework 加入建置的條件，從「有工具」變成「有工具，或有產品宣告使用 framework」。
   - 產品登錄資料（`project.json`）多一種可宣告的東西。
   - framework DI 1（`docs/framework/architecture.md`）的「so far: when at least one tool is selected」要改寫成新條件。這是 Design Invariants 節的變更，需要使用者同意。
4. **影響的 Module／Product／Tool**：
   - `build/cmake/GyoBuild.cmake`（兩處條件）、`build/cmake/GyoComponents.cmake`（若採 (a)）。
   - `apps/object_fps_pvp/project.json`、`apps/object_fps_pvp/CMakeLists.txt`（Client 連 `GYO::ToolFramework`）。
   - `docs/framework/architecture.md` DI 1、`framework/CMakeLists.txt` 開頭的註解（「so far only tools do」）。
   - 測試：`tests/common/cmake/ProjectCompositionTests.cmake`（加組態案例）。
   - 不影響：其他遊戲、audio_editor、object_fps_preview、Engine。
5. **依賴方向**：新增一條 `object_fps_pvp（Client）→ framework`，符合 framework DI 2 的「game or tool → framework → engine」。framework 不知道遊戲；Engine 不變。Match（伺服器）不連 framework。
6. **Ownership**：不變。framework 仍不是產品（DI 1：沒有登錄列、沒有執行檔）；是否使用 framework 由產品自己的登錄資料決定，共通層只解讀這筆資料（AGENTS §9）。
7. **為何不能更小**：
   - 只讓編輯器用 adapter、遊戲保留自己那份（卡上的 (b)）：已被核准的 (a) 排除，而且「同一個轉換兩份」只解決一半。
   - 遊戲直接 include framework 的原始碼、或把 adapter 複製進遊戲：違反 DI 1／DI 2，等於第三份。
   - 永遠加入 framework：只選 Engine 或只選不用 framework 的產品時也建 framework，違反 DI 1「只在有選取的產品使用時才建」。

## 做法的選項（`v8-input-ui-adapter-pvp-build-form`）

### (a) 在 `components` 加一個 `FRAMEWORK`（建議，已採用）

- `GYO_KNOWN_COMPONENTS` 加 `FRAMEWORK`。遊戲在 `project.json` 的 `components` 寫 `"FRAMEWORK"`。
- `GyoBuild.cmake` 的兩處條件改成 `if(GYO_ACTIVE_TOOLS OR GYO_ENABLE_FRAMEWORK)`。`GYO_ENABLE_*` 在 `gyo_resolve_components`（`GyoBuild.cmake:25`）就已算好，早於這兩處。
- 理由：
  - 沿用既有的宣告、驗證（未知的名稱會 configure 失敗）與合併（遊戲、工具、共通測試的需求一起去重）。不用新的解析程式。
  - 和 `IMGUI`、`SDL_GPU` 一樣：產品宣告需求，共通層解讀、決定要建什麼。
- 已查：打包與執行期部署只對 `SDL_*`、`IMGUI` 帶動態函式庫（`GyoPackaging.cmake:91`、`GyoRuntimeDeployment.cmake:70`），`FRAMEWORK` 是靜態函式庫，不受影響。
- 取捨：`components` 原本都是第三方或後端，`FRAMEWORK` 是 repo 自己的一層，語意略寬。

### (b) `project.json` 新增欄位 `"uses_framework": true`

- 語意單純，但要在 `gyo_read_project`（遊戲）加解析與驗證，也要決定工具是否同樣宣告。

### 兩個選項都不做的事

- **工具維持「選了就加入 framework」**，不改成也要宣告。
  - 現在 object_fps_preview 不連 framework，卻也會讓 framework 加入建置。這是既有的不精確，不在本卡範圍；記為之後的整理項目。
  - 要改，就得動工具線的兩個 `project.json`。

## 驗收候選（實作的事前宣告再定）

- 組態測試（ProjectComposition），三種組態：
  - 只選 object_fps_pvp、不選工具：framework 與 `tests/framework` 都要加入；
  - 不選任何產品：都不加入；
  - 只選工具：照舊加入。
- 未知的 component 名稱仍 configure 失敗（既有案例）。
- Product Removability：從登錄資料移除 object_fps_pvp 之後，只選另一個不用 framework 的產品時，framework 不加入建置，也不需要改共通層。

## 範圍的限界

- 不改 `GYO::ToolFramework` 這個名字（名字裡有 Tool，遊戲也會用）。記為之後的整理項目。
- 不做「pvp Client 搬上外殼」。
- 和 PR B 的關係（framework 整改線 2026-11-01 確認）：PR B 不改 `GyoBuild.cmake`、`GyoComponents.cmake` 與任何 `project.json`。它改 `framework/CMakeLists.txt` 的中段（來源清單、Legacy.cmake），以及 `docs/framework/architecture.md` 的 DI 2 守衛句、DI 4、組成清單、Using it。`framework/CMakeLists.txt` 開頭的註解和 DI 1 的「so far」那句它都沒動，所以 PR B 合併後再改這兩處，不會衝突。
