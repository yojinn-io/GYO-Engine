# v8 工具線交接

更新：2026-10-30。Owner：三個工具各自的 owner（見 [README](README.md)）。**狀態：先處理 UI Editor（`v8-tool-scope-ui-first`），等你核准；沒有實作，沒有推送。**
本文件是工具線的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [README](README.md)。v8 產品計畫的同步由產品會話做，本線不改 `docs/object_fps_pvp/plans/v8/`。

## 代號對照

代號規則照 [docs/architecture.md](../../../architecture.md)「Plan records and identifiers」（`v8-docs-id-scheme`）。

| 代號 | 意思 |
|---|---|
| ui-widgets-line | PR 線：文字輸入框元件、輸入法組字、ui_editor |

## 現況（每次停下來時更新）

- **完成**：`v8-tool-plan`（本計畫、本 HANDOFF）；先處理 UI（`v8-tool-scope-ui-first`）。
- **待討論**：`v8-tool-voxel-map-editor`、`v8-tool-assets-editor`（你會逐個給方案）。
- **進行中**：無。
- **等你決定**：下一節。
- **發現（不在本線的範圍，只報告）**：
  - `GYO::Ui` 與 ui_editor 沒有 `Design Invariants` 節（[ui_toolchain.md](../../../ui_toolchain.md)、[tool_projects.md](../../../tool_projects.md)）。本線加元件前要補（`v8-tool-ui-invariants`）。
  - PvP 有一份從舊產品複製的 `RetroFPS/World/GridMap.hpp`，經 `GameData.hpp` 被武器標頭間接包含，但 PvP 沒有它的載入器。是不是遺留物沒有查證；交給架構健檢。

## 等你決定

每項：機制、取捨、證據位置、建議。「會改變哪個決定」寫在最後一欄。

| 代號 | 問題 | 選項與取捨 | 建議 | 會改變 |
|---|---|---|---|---|
| `v8-tool-plan-location` | 計畫文件放哪 | `docs/tools/plans/v8/`（④ 的線計畫；之後你的 voxel 方案也能放這裡；`docs/` 底下，不是新的 Top-level 目錄）／`docs/ui_editor/plans/v8/`（現在只剩 UI Editor，放 owner 的文件夾更直接） | 前者，因為 ④ 之後還有 voxel；UI 的格式與 Design Invariants 仍寫在 [ui_toolchain.md](../../../ui_toolchain.md) | 文件位置；圖書管理員可再調 |
| `v8-tool-start-timing` | 實作何時開始 | 照 v8-product-order 在 ③ 之後／ui-widgets-line 的檢測（`v8-tool-ui-ime-observe`）先做，其他照順序 | 檢測先做：它不改行為，結果決定輸入法要不要修 | 何時排實機 |
| `v8-tool-ui-widget-scope` | 「組件化這些按鈕和輸入框」的範圍 | (a) 只加 `text_field`；按鈕已經是 `gyo.ui` 的 `button`（`pvp_lobby.json`）／(b) 再加按鈕的共用樣式（同一組顏色、字級只寫一次），也是 schema 變更／(c) 可重用的元件範本（prefab），接近 ui_toolchain.md 明說不做的「custom widgets」 | (a)；(b) 等第二個畫面也重複時再做 | `v8-tool-ui-button-style` 要不要排 |
| `v8-tool-ui-schema-version` | `text_field` 怎麼進 schema | v1 的集合寫明是封閉的。升 `version: 2`（讀取端接受 1、2；舊讀取端明確拒絕 v2）／留在 v1 加元素（舊讀取端以「未知元素」拒絕，但版本號沒告訴人為什麼） | 升 v2 | 契約文件與 codec |
| `v8-tool-ui-edit-policy-owner` | 文字編輯政策放哪 | 通用的部分（游標、刪除、全選、貼上、組字顯示）放 `GYO::Ui`（Architecture Delta：Ui 的責任變大）／全部留在產品，Ui 只畫（輸入框就不算元件）。字元過濾：(i) `text_field` 宣告 `max_length`，過濾由產品在提交時檢查／(ii) 資料裡宣告允許的字元集合（簡單、封閉，但開始往設定語言走，AGENTS §9）／(iii) 產品在執行時提供過濾函式 | 通用部分在 Ui；過濾用 (iii)：規則留在產品，資料不帶產品語意 | Ui 的 Design Invariants 怎麼寫 |
| `v8-tool-ui-invariants` | 在 ui_toolchain.md 新增 `Design Invariants` 節 | AGENTS §3：節的新增要使用者同意 | 同意後在 `v8-tool-ui-invariants` 寫，內容另給你看 | 能不能開始加元件 |
| `v8-tool-ui-navigation` | 頁面（canvas）跳轉用 if／else 分派 action 字串 | 現況：`object_fps_v2/src/App/ObjectFpsUi.cpp:463-500` 的 if／else 鏈（使用者留的 `TODO: デザイン、問題がある`）、`CanvasFor` 的 switch；PvP 只有一個 canvas，`PvpApplication.cpp` 的 `Execute` 也是 if／else。選項：(A) 產品內改成表（action id → 處理函式），初始化時雙向檢查文件宣告的 action 都有處理、處理的 action 都有宣告；不改契約／(B) `gyo.ui` 加「純 UI 的頁面切換」（按鈕宣告目標 canvas，Ui 自己切；產品只處理有遊戲效果的 action），ui_editor 預覽能點著走；契約變更，且要劃清「哪一頁」的權威（遊戲階段歸產品、純 UI 子頁歸 Ui，AGENTS §3 狀態的權威）／(C) 資料裡寫完整的畫面狀態機與條件（往設定語言走，AGENTS §9 禁止） | (A) 先做；(B) 在 ⑤ 暫停選單、畫面設定要加頁面時，以實際的頁面數再決定 | 是否改 `gyo.ui` 契約 |
| `v8-tool-ime-input-delta` | 組字事件由誰做 | Engine 的 Input（計畫夾 input-and-present；只加不改，不變式 1）在工具線做／交給 Engine 的會話做 | 工具線做，記到 input-and-present 的 HANDOFF；先等 `v8-tool-ui-ime-observe` 的結果 | 有沒有 Engine 批 |

## 決定紀錄

| 代號 | 決定 | 使用者原話 |
|---|---|---|
| `v8-tool-ui-editor-mount-ux` | （2026-10-30）ui_editor 的路徑選擇先用 SDL3 的系統選擇窗（選項 1）：Mount、Open、Export 與掛載對話框的欄位都用；不記住路徑；平台跳不出窗時退回手打。ImGui 自己畫的版本（像 Godot 的預設）之後要時再做，那時才需要 `GYO::Os` 加列目錄（`v8-engine-os-list-directory`，未開） | 「先做1吧，先把功能實現。」 |
| `v8-tool-scope-ui-first` | （2026-10-30）先處理 `v8-tool-ui-editor-widgets`。voxel 與 assets 沒有暫停，等使用者逐個給方案再討論；第一版 Claude 自己寫的地圖建議（烘焙到 arena v1、`gyo.voxel_map`、ImGui 的 SDL_GPU 後端）撤下。取代同日的 `v8-tool-scope-ui-only`（Claude 誤解成「暫停」） | 「我們這次只改UI Editor，voxel Map Editor我後續給妳方案。」→「我不是要把工具都暫停，而是 voxel assets 都需要討論，我會把方案一個個給你，現在先處理UI。」 |

## 紀錄

- 2026-10-30：`v8-tool-plan` 開始（檔位 high，引擎 Opus 5.5，估 3 h）。從主會話開的工具線會話，分支 `claude/v8-tool-line-3a892c`（切自 `claude/pvp-v8-pv8-line` 的 `c7ca2fd7`）。盤點 `tools/`、`engine/ui`、`engine/input` 的 SDL 後端、產品的大廳與 arena、舊產品的格子地圖、v2 的 hurt_regions、共通 Build 的 ImGui 元件。主要發現：SDL 後端不收 `SDL_EVENT_TEXT_EDITING`；`gyo.ui` v1 沒有輸入框；`Arena` 新增成員是 wire 變更（pv6 §2），所以建議地圖編輯器烘焙到 arena v1。
- 2026-10-30：計畫、HANDOFF、ultracode prompt 草稿寫完；實際用時約 1 h（估 3 h 以內）。沒有實作、沒有推送。停下等你核准。
- 2026-10-30：使用者問 pv6 已多處過時，引用它的規定有沒有問題。查證：pv7（`protocol-v7.zh-Hant.md` 第 5 行）與 pv8（`protocol-v8.zh-Hant.md` 第 4 行）都寫「沒有改寫的條款原樣沿用」，兩者都沒改寫 pv6 §2 的 arena digest 條款；pv8 草稿第 54 行明寫蹲下參數「不進 `arena_digest`」；現行程式仍帶 `arena_digest`（`client_v7.proto:24`、`runtime_v7.proto:18`、`IpcHost.cpp:489`）。結論：條文現在仍有效，但第一版只引 pv6、沒有寫出這條沿用鏈，是引用不完整；pv8 重開中，新範圍有可能改它。README 的現況表已補上沿用鏈。
- 2026-10-30：使用者把範圍縮小為 UI Editor（`v8-tool-scope-ui-only`）。README 撤下 voxel 與 assets 的提案，刪除只為這兩個工具寫的 ultracode prompt 草稿，HANDOFF 撤下相關的待決事項。
- 2026-10-30：使用者更正：voxel、assets 不是暫停，是要討論，會逐個給方案（`v8-tool-scope-ui-first`）。另問兩件事：ui_editor 載入配置的地方、頁面跳轉的 if／else。讀了 `EditorApp.cpp:566-616`、`:1999-2030`（掛載對話框）、`ObjectFpsUi.cpp:344-500`（v2 的分派與 `CanvasFor`）、`PvpApplication.cpp` 的 `Execute`，記成 `v8-tool-ui-editor-mount-ux`、`v8-tool-ui-navigation`。
- 2026-10-30：使用者決定載入改用 ImGui 的選擇窗並給了 AssetPack 作參考（`v8-tool-ui-editor-mount-ux`）。讀了 AssetPack 的 README、`src/platform/WinFileDialog.*`、`src/ui/UiProject.cpp:415-470`：可借的是流程（一顆按鈕選資料夾、顯示導出的路徑、未存時先確認），選擇窗本身是 Windows 原生。查到 `GYO::Os::Files` 沒有列目錄（`engine/os/include/engine/os/Files.hpp`），os.md 不變式 2（R4）禁止工具直接用 `std::filesystem`，所以新增待決 `v8-engine-os-list-directory`、`v8-tool-ui-editor-file-browser-scope`，README 加兩批。
- 2026-10-30：使用者：「先討論好，不急Orz。其實我想做跨平台，但是我如果如果不用ImGUI有什麼解法麼。」上一筆把 `v8-tool-ui-editor-mount-ux` 記成決定是 Claude 記早了，改回討論中，並補上不用 ImGui 的選項（SDL3 的系統選擇窗等）。
- 2026-10-30：使用者問 Godot 怎麼做。子 agent（web-fetch）讀了 Godot 4.7 stable 的 FileDialog、EditorFileDialog、EditorSettings、DisplayServer、DirAccess 文件，結果補進 `v8-tool-ui-editor-mount-ux`。DisplayServer 與 EditorSettings 的部分文字來自摘要，不是逐字；編輯器設定的預設值文件沒寫。
- 2026-10-30：`v8-tool-ui-editor-file-browser` 開始（檔位 high，引擎 Opus 5.5，估 3～5 h）。使用者選了 SDL3 的系統選擇窗（`v8-tool-ui-editor-mount-ux`）。
- 2026-10-30：`v8-tool-ui-editor-file-browser` 完成。macOS 上 9 個相關測試通過，手動突變 3 個全部被抓到。Windows、Linux 的實際選擇窗未看過（L3 待排）。
