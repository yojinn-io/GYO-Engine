# 工具 v8：v8-tool-voxel-map-editor-window（voxel 地圖編輯器的視窗）

- 日期：2026-11-06。
- 分支：`claude/v8-tool-voxel-map-window`（切自 master `2cd57244`）。
- 段：上段。
- 實作：主會話（Opus 5.5／high）。
- 審查與 §14.4 的驗證者：各自是子 Agent，都是 Opus 5.5／high。
  - 沒有另一種模型可用來做第二輪；驗證的四輪都由同一個模型做。

宣告寫在 [voxel-map-editor.md](../tools/plans/v8/voxel-map-editor.md) 的「v8-tool-voxel-map-editor-window 的事前宣告」，2026-11-06 你核准（「承認する」）。和宣告的差異列在同一節的「實作後的變更」。記錄器是 [voxel-map-editor-HANDOFF.md](../tools/plans/v8/voxel-map-editor-HANDOFF.md)。

**狀態：** 功能、審查修正與測試都已完成。還有兩件事要等你：

- 依賴守衛判定的變更（W9，§14.4）與 DI 文字的修改，等你核准。
- P1（預覽在大地圖上的幀時間）交給架構會話，等它的結論。

## 做了什麼

- **編輯模型**（核心，CPU 測試可以涵蓋；`Editing.hpp`／`Editing.cpp`）：
  - 放置、擦除、塗、框選填滿／清空。框選最多 2^20 格。
  - undo／redo：一次拖曳算一步，同一格放了又擦不留下步驟；拖曳中存檔時，存檔位置算在拖曳之後。
  - 方塊與形狀的增、改、刪。使用中的不能刪，刪除後重新編號；每次修改都要能經 `ParseGrid` 讀回。
- **拾取**：
  - `ViewProjection` 照 Engine `Renderer` 的組法；`PixelRay`／`ProjectToPixel` 由它推出。
  - 用 DDA 走格子。沒打到任何格子時，交到編輯平面（限 maxCells 以內）。
  - 射線不是有限值、或座標遠超出範圍時，回傳「沒打到」。
- **存檔**：
  1. 寫出格子文字，build，經 Engine 讀回；
  2. 檢查兩個檔都沒有被外部改過（`FileChanged`）；
  3. 先寫格子，再寫產物，都是原子替換。
  - 產物寫不出去時，格子的 known 位元組照樣更新，下次存檔能接著寫。
- **視窗**（`src/window/`，gui 版本）：
  - 由 `PreviewScene` 持有 framework 的 `OffscreenPreview`，用 Engine 的 unlit shader 和調色盤貼圖畫產物，和遊戲的畫法相同。
  - ImGui 的側欄：檔案、工具、平面 y、框選高度、朝向（R 轉、Shift+R 換上方軸）、方塊、形狀。
  - 游標和平面格線用 draw list 畫，投影和拾取用同一套算法。
  - 關閉時，如果有未存的修改，會提示。
  - 指令 `edit <grid> <mesh>`。
- **量測**：`measure-preview`（`--grid` 或 `--synthetic`），是 P1 用的。
- **登錄**：`project.json` 加 `gui` 版本，設為預設；執行檔旁邊放 builtin shaders。
- **文件**（`docs/voxel_map_editor/architecture.md`）：
  - DI 1 分成核心和視窗兩部分；
  - DI 5 加上「開啟後被改過的檔不覆寫」，並寫明 `measure-preview` 的 CSV；
  - 新增 DI 7（看到的就是遊戲讀到的）；
  - 新增「The window」一節。

## 判定

| 判定 | 結果 |
|---|---|
| W1 編輯操作 | 通過（含框選上限 2^20） |
| W2 undo／redo | 通過（含拖曳中存檔、放了又擦） |
| W3 拾取（CPU） | 通過（投影來回 45 格以上；平面距離上限；NaN、無限大、遠座標） |
| W4 拾取和畫面一致（GPU） | 通過，只在本機 Metal 跑（見「實作後的變更」）。3 個視角共 20,582 個畫素，不一致 0 |
| W5 存檔 | 通過（含外部修改、Engine 拒絕、格子寫不出、產物寫不出） |
| W6 預覽經過 Engine | 通過 |
| W7 調色盤與形狀 | 通過 |
| W8 CLI | `edit`／`measure-preview` 的參數錯誤都回 2；cli 版本的「沒有視窗」只在 cli 建置時跑，本機沒有跑過 |
| W9 依賴守衛 | 自我測試 12 條通過；§14.4 的 1～5 見下面，**等你核准** |
| W10 突變 | 視窗的突變 22 條全部 killed：宣告的 18 條，加上審查後補的 4 條。相關的既有突變 2／2 也 killed |
| W11 L1 | 待 CI |

- 本機的全套 cpu|shader：130／130（審查修正前）。審查修正後的結果記在證據資料夾。
- 編輯器與 GPU 的測試：6／6。

## 經過

1. **實作核心和視窗**：核心的 34 個 test case 第一次就全部通過。
2. **停下（§14）**：用 `measure-preview` 試跑 30 幀（探索用）。
   - 合成地圖（64×16×64、填 30%）每幀約 2.5 s：產物 5.3 MB、124,236 個四邊形，Engine 讀回約 1.2 s，而且讀了兩次。
   - `pvp_corners` 的 p95 是 13.1 ms。
   - 計畫的前提「每次編輯都整張重建」在大地圖上不成立，所以停下回報。
   - 你決定：P1 交給架構會話；縮小地圖或範圍的做法不採用（大地圖正是編輯器的目的）。這一批繼續做編輯器（`v8-tool-voxel-window-p1-to-architect`）。
3. **W4 改成只在本機跑**：CI 的 GPU 步驟只跑 `render.*`。
4. **突變第一輪 18／18**：為了讓 `vmw-redo-kept-after-edit` 和 `vmw-mesh-before-grid` 能被殺死，先補了 W2、W5 的測試。
5. **審查（Opus／high）**：
   - B1：undo「Add shape」之後，`shapes[shape_]` 越界。改為在 Draw 開頭校正索引；草稿只在來源改變時才重新載入（一併修 N3）。
   - S1：框選是 O(n²) 而且沒有上限。改用 map 建索引，框選上限 2^20 格，編輯平面的落點限 maxCells 以內。
   - S2：拖曳中存檔。`MarkSaved` 先結束拖曳。
   - S3：核心可以 include render／platform 的標頭。視窗 library 改成 PRIVATE 連預覽；守衛禁止核心 include 這些標頭。
   - S4：被 `FileChanged` 拒絕之後，視窗沒有出路。**這是設計問題，等你決定。**
   - nit：
     - N1：double 轉 int 的 UB；
     - N2：放了又擦仍留下 undo 步驟；
     - N4：框選沒套用時要有提示；
     - N5：modal 開著時快捷鍵仍有效；
     - N6：從格子內部放置；
     - N7：補產物寫失敗的測試；
     - N8：DI 5 寫明 CSV；
     - N9：缺 `<array>`。
6. **§14.4 驗證者四輪**：守衛有「隱藏的放寬」，每輪修正後再驗，詳見下節。
7. **審查修正後的突變**：新增 4 條和守衛的 4 條共 8／8 killed，之前的 20 條也都是 killed。

## 依賴守衛的 §14.4 報告（給你核准）

守衛：`tests/voxel_map_editor/voxel_map_editor_dependency_guard.py` 與它的自我測試。基準是 master `2cd57244`。

1. **守的性質**：DI 1。
   - 編輯器不連遊戲、不提產品；
   - 不直接用 SDL；
   - 核心沒有 ImGui、render、platform；
   - 只有視窗可以用 framework 的 ImGui 外殼、預覽，以及 Engine 的 render／platform。
2. **放寬的面**（以前失敗，現在通過）：
   - **視窗（`src/window/`）的 include**：可以 include `imgui.h`、`gyo/framework/imgui/` 底下非 native、非 internal、非 backends 的標頭，以及 `window/…`。
   - **核心的 include**：可以 include 剛好一個 `window/EditorWindow.hpp`。
   - **`GYO::Math`**：在任何位置都可以用。
   - **視窗 library 的連結**：`gyo_voxel_map_editor_window` 可以用字面名稱連下列六個 target：`GYO::ToolFrameworkPreview`、`GYO::ToolFrameworkImGui`、`GYO::Render`、`GYO::RenderBackendSDL`、`GYO::PlatformSDL`、`imgui::core`。
   - **這六個名字可以出現的地方**：只有兩處，就是視窗 library 自己的 `target_link_libraries`，以及 gui 版本的存在檢查（`foreach(_target IN ITEMS …)`）。
   - **連視窗 library**：只有執行檔可以連。
   - **逐例核對**：驗證者的 551 個合成案例裡，新通過的 41 個都屬於上面這些。其中也包括幾種合法的寫法：大寫的指令、拆成多行的連結、整行註解。
3. **收緊的面**（以前通過，現在失敗）。真實的樹一個都沒有用到，但以後正當的樹可能需要，到時要改守衛：
   - include 含 `.`、`..`、空的路徑步驟，或含反斜線。
   - 核心 include `engine/render/`、`engine/platform/`、`gyo/framework/Preview*`、`gyo/framework/WindowShell*`、`gyo/framework/imgui/*`。
   - `gyo/framework/imgui/` 底下的 native 標頭（不分大小寫），以及路徑任何位置含 backends 的標頭。
   - `target_link_libraries` 的參數用 `${…}` 或 `$<…>`，例如 `$<LINK_ONLY:…>`、平台條件、`${CMAKE_DL_LIBS}`。
   - 任何 `link_libraries(`、`LINK_LIBRARIES*` 屬性（包括 `LINK_LIBRARIES_ONLY_TARGETS`）、`*link_options(`、`TARGET_OBJECTS`。出現在字串或行尾註解裡也算。
   - 視窗 library 的名字只能出現在三種地方：它自己指令的第一個參數、執行檔的連結、目標清單。其他地方都算失敗，例如 `set_target_properties`、`install`、`if(TARGET …)`、字串裡。
   - 以前「視窗與 render 的 target 一律失敗（暫時）」那條自我測試刪除了，改成「只有視窗 library 可以連」的檢查。
4. **放寬處仍受守護**：
   - 視窗仍然不能用：`SDL3/` 標頭、`SDL_` 名字、`*Native` 標頭與 target、`imgui_internal.h`、`backends/`、遊戲的 include 與 plain target、登錄的 id。
   - 核心仍然不能連那六個 target（以字面名稱、變數、屬性或生成器運算式都不行）。
   - 突變 `vmw-guard-core-imgui`、`vmw-guard-window-native`、`vmw-guard-core-links-render`、`vme-guard-sdl-include` 都 killed。
5. **證據**：
   - 驗證者做了四輪：415 → 464 → 485 → 551 個案例。前三輪都找到隱藏的放寬，我改守衛把它們擋下，沒有放寬文字。
   - 第四輪剩一個罕見寫法：跨行字串裡以 `#` 開頭的行。我修好後，用驗證者的腳本自己重跑了一次：新通過的只剩第 2 點列出的寫法。**最後這次是提案者自己跑的，驗證者沒有再確認。**
   - 驗證者也記錄了兩版都允許的既有弱點：核心可以 include `engine/ui` 等其他 Engine 模組。這是 DI 1「任何 Engine 模組」的允許，連結由 CMake 那一關擋。

## 等你決定

1. W9 的 §14.4 核准（上一節）。DI 1 和 DI 5 這次的文字修改，也一起請你核准。
2. S4：檔案被外部修改之後，視窗現在只能「放棄編輯」。要不要加「從磁碟重新載入」（丟掉未存的修改，重新開檔），或「另存成別的路徑」？DI 5（不覆寫被改過的檔）不受影響。
3. P1：等架構會話的結論。

## 證據

主 checkout（macOS Intel）的 `build/evidence/tool-v8-voxel-map/window/`：

- **P1 的探索量測**：`explore-p1/`（合成地圖與 `pvp_corners` 的 CSV、剖析程式與輸出）。
- **突變**：
  - `mutations-window.txt`：第一輪 18／18；
  - `mutations-core-related.txt`：相關的既有突變 2／2；
  - `mutations-after-review.txt`：20／20；
  - `mutations-final.txt`：8／8。
- **建置與測試**：`build*.txt`、`ctest-*.txt`。
- **§14.4 驗證者**：`guard-verifier/`：
  - `round1`；
  - `round2-proposer`（提案者自己跑的）；
  - `round3-verifier`～`round5-verifier`（每一輪的腳本、新舊守衛的副本、輸出）；
  - `round5-verifier/output5-after.txt`：最後一次修正後，提案者自己重跑的結果。

## 用時

約 9 h（重估 16～22 h）。尚未包括 L1 和你的核准。
