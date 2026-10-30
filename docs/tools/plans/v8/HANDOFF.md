# v8 工具線交接

更新：2026-10-30。Owner：三個工具各自的 owner（見 [README](README.md)）。**狀態：先處理 UI Editor（`v8-tool-scope-ui-first`）。[PR #107](https://github.com/yojinn-io/GYO-Engine/pull/107) 已合併；`v8-tool-ui-editor-gizmo-fix` 完成，在分支 `claude/v8-tool-ui-editor-gizmo`。**
本文件是工具線的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [README](README.md)。v8 產品計畫的同步由產品會話做，本線不改 `docs/object_fps_pvp/plans/v8/`。

## 代號對照

代號規則照 [docs/architecture.md](../../../architecture.md)「Plan records and identifiers」（`v8-docs-id-scheme`）。

| 代號 | 意思 |
|---|---|
| ui-widgets-line | PR 線：文字輸入框元件、輸入法組字、ui_editor |

## 現況（每次停下來時更新）

- **完成**：`v8-tool-plan`（本計畫、本 HANDOFF）；先處理 UI（`v8-tool-scope-ui-first`）。
- **待討論**：`v8-tool-voxel-map-editor`、`v8-tool-assets-editor`（你會逐個給方案）。
- **完成並合併**：[PR #107](https://github.com/yojinn-io/GYO-Engine/pull/107)（`v8-tool-plan`、`v8-tool-ui-editor-file-browser`、審查、`v8-tool-ui-editor-gizmo-math`）。
- **完成，未合併**：`v8-tool-ui-editor-gizmo-fix`（[PR #108](https://github.com/yojinn-io/GYO-Engine/pull/108)，分支 `claude/v8-tool-ui-editor-gizmo`）。使用者在 GUI 上試過錨點與 pivot 的把手。
- **完成，未推送**：`v8-tool-ui-invariants`（[ui_toolchain.md](../../../ui_toolchain.md)「Design Invariants」＋三個守衛）、`v8-tool-ui-editor-panels`（面板拆檔，純搬移）。下一批 `v8-tool-ui-editor-element-schema`。
- **等你決定**：下一節。
- **發現（不在本線的範圍，只報告）**：
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
| `v8-tool-ui-navigation` | 頁面（canvas）跳轉用 if／else 分派 action 字串 | 現況：`object_fps_v2/src/App/ObjectFpsUi.cpp:463-500` 的 if／else 鏈（使用者留的 `TODO: デザイン、問題がある`）、`CanvasFor` 的 switch；PvP 只有一個 canvas，`PvpApplication.cpp` 的 `Execute` 也是 if／else。選項：(A) 產品內改成表（action id → 處理函式），初始化時雙向檢查文件宣告的 action 都有處理、處理的 action 都有宣告；不改契約／(B) `gyo.ui` 加「純 UI 的頁面切換」（按鈕宣告目標 canvas，Ui 自己切；產品只處理有遊戲效果的 action），ui_editor 預覽能點著走；契約變更，且要劃清「哪一頁」的權威（遊戲階段歸產品、純 UI 子頁歸 Ui，AGENTS §3 狀態的權威）／(C) 資料裡寫完整的畫面狀態機與條件（往設定語言走，AGENTS §9 禁止） | (A) 先做；(B) 在 ⑤ 暫停選單、畫面設定要加頁面時，以實際的頁面數再決定 | 是否改 `gyo.ui` 契約 |
| `v8-tool-ui-background` | main menu 的背景圖或動畫 | 靜態背景圖：`gyo.ui` v1 已有 `image` 元素（`UiDocument.hpp:150`），錨點 0～1、`size_delta` 0、放在第一個子元素就是全螢幕背景，**不用改契約**。動畫：v1 明寫沒有 animation（ui_toolchain.md「JSON v1」）。(a) 產品在 UI 後面畫動態場景（3D 場景、shader），UI 的底色設透明／(b) `gyo.ui` 加有限的動畫（例如圖片的 UV 捲動、逐格動畫），是契約變更（v2）／(c) 影片：要解碼器，最重 | 靜態圖先做；動畫先決定要哪一種效果 | 是否改 `gyo.ui` 契約 |
| `v8-tool-ui-editor-structure` | 審查的整理項目要不要排 | [審查](ui-editor-review.md)：邊界與資料安全好，內部責任太集中（`EditorApp.cpp` 約 2,300 行、元素 schema 在 editor 又寫一份）。建議在加 `text_field` 前做 `v8-tool-ui-editor-gizmo-math`、`v8-tool-ui-editor-element-schema`、`v8-tool-ui-editor-panels`，不重寫 | 照審查的順序排 | ui-widgets-line 的批次 |
| `v8-tool-ime-input-delta` | 組字事件由誰做 | Engine 的 Input（計畫夾 input-and-present；只加不改，不變式 1）在工具線做／交給 Engine 的會話做 | 工具線做，記到 input-and-present 的 HANDOFF；先等 `v8-tool-ui-ime-observe` 的結果 | 有沒有 Engine 批 |

## 決定紀錄

| 代號 | 決定 | 使用者原話 |
|---|---|---|
| `v8-tool-ui-editor-scope-menus` | （2026-10-30）UI Editor 只處理遊戲選單等介面，不管 HUD。HUD（例如 `hit_feedback.json`）由產品自己畫 | 「UI Editor只處理遊戲菜單等界面，而不管HUD。」 |
| `v8-tool-ui-editor-mount-ux` | （2026-10-30）ui_editor 的路徑選擇先用 SDL3 的系統選擇窗（選項 1）：Mount、Open、Export 與掛載對話框的欄位都用；不記住路徑；平台跳不出窗時退回手打。ImGui 自己畫的版本（像 Godot 的預設）之後要時再做，那時才需要 `GYO::Os` 加列目錄（`v8-engine-os-list-directory`，未開） | 「先做1吧，先把功能實現。」 |
| `v8-tool-ui-invariants` | （2026-10-30）照草稿寫進 [ui_toolchain.md](../../../ui_toolchain.md)「Design Invariants」：GYO::Ui 4 條、ui_editor 5 條、Replacement Criteria；守衛 `ui.dependency_guard`、`gyo_ui_editor.dependency_guard`、`TestGizmoAgreesWithUiEvaluator` | 「同意，寫進去」 |
| `v8-tool-ui-order` | （2026-10-30）UI 的順序：先寫 `v8-tool-ui-invariants`，再整理（`v8-tool-ui-editor-panels` → `v8-tool-ui-editor-element-schema`），再做 `text_field`，編輯器的小功能（複製貼上、方向鍵微調）穿插；對齊參考線、多選等需要時再排。見 [README](README.md)「順序與依賴」 | 「照你建議做吧。」 |
| `v8-tool-ui-editor-gizmo-bug` | （2026-10-30）拖動照 (a) Unity 的 RectTransform：本體＝移動 `position`；邊與角＝縮放（`size_delta`、`position`），錨點不動；拖錨點與 pivot 時補償 `position`、`size_delta`，元素的矩形不動。「相等時一起移動」的實作細節由 Claude 定為：被拖的錨點推著另一個走，所以往對方那側拖時一起動、往反方向拖時分開（否則相等的錨點在 gizmo 上永遠分不開）。Snap 只對齊這次拖動的座標 | 「(a) Unity 式」 |
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
- 2026-10-30：使用者補充：UI Editor 只管選單不管 HUD（`v8-tool-ui-editor-scope-menus`）；拖動與縮放有 bug；main menu 要背景圖或動畫；要求評價與結構審查。審查寫成 [ui-editor-review.md](ui-editor-review.md)（只審查，沒改程式）。拖動的 bug 只從程式推論機制，未重現。
- 2026-10-30：使用者描述現象：拖藍色方塊「只能左右，往上拖動無效」。用暫時的程式驅動真正的 `PreviewAdapter::Draw`，四個方向各拖 50 px，重現並寫進審查 S-5（程式留在會話的 scratchpad，沒有進 repo）。
- 2026-10-30：使用者要求把審查的 S-1～S-3 重構掉，先做 S-3。`v8-tool-ui-editor-gizmo-math` 完成：gizmo 數學抽成 `GizmoMath`，行為不變（重現程式前後輸出相同、9/9 測試、8 個突變全部被抓到）。新發現：開 Snap 時沒拖的軸也可能被對齊。拖動 bug 的修法仍等你決定。使用者問 commit 去哪了：都在本機分支 `claude/v8-tool-line-3a892c`，照主會話的交代沒有推送、沒有開 PR。
- 2026-10-30：使用者要求推送並開 PR，並選擇「只放工具線」。從 origin/master（`d9905dec`）開新分支 `claude/v8-tool-ui-editor`，cherry-pick 工具線的 11 個 commit（無衝突；程式與已測的分支逐位元相同，9/9 測試再跑一次通過），推送並開 [PR #107](https://github.com/yojinn-io/GYO-Engine/pull/107)。主會話的 v8 文件（`claude/pvp-v8-pv8-line` 上 18 個未推送的 commit）不在這個 PR；README 指向 `protocol-v8.zh-Hant.md` 的連結要等那邊合併才有效。舊分支 `claude/v8-tool-line-3a892c` 留在本機，不再使用。
- 2026-10-30：使用者確認 CI 後要求合併，[PR #107](https://github.com/yojinn-io/GYO-Engine/pull/107) 以 merge commit 合併（`2d78b753`）。
- 2026-10-30：使用者選拖動的修法 (a) Unity 式（`v8-tool-ui-editor-gizmo-bug`）。`v8-tool-ui-editor-gizmo-fix` 開始（檔位 high，引擎 Opus 5.5，估 3 h），分支 `claude/v8-tool-ui-editor-gizmo`（切自 master `2d78b753`）。
- 2026-10-30：`v8-tool-ui-editor-gizmo-fix` 完成，實際約 1 h。先在舊規則下跑新測試：23 項失敗（Red，含「錨點相等時往上拖無效」），換成新規則後全過；9/9 測試通過；8 個突變全部被抓到。GUI 上的手動拖動還沒做。
- 2026-10-30：使用者用新版試拖，問錨點與 pivot 的把手有什麼用。查證：`gyo.ui` v1 的根節點固定是設計尺寸（`UiRuntime.cpp:721`），整張畫布等比例縮放、留黑邊（`FitDesignCanvas`，`UiRuntime.cpp:603`），所以每個父層的大小在任何螢幕上都不變，錨點與 pivot 只改變 `position`／`size_delta` 的寫法，不改畫面。提了三個選項：隱藏把手、維持現狀、讓根節點跟著螢幕比例走（契約 v2）。使用者：「先保持這樣。」把手維持現狀，不另開待決。
- 2026-10-30：開 [PR #108](https://github.com/yojinn-io/GYO-Engine/pull/108)，Auto-fix 開啟。使用者問 UI 還要加什麼；建議兩類（遊戲畫面要的：`text_field`、背景、頁面切換、設定元件；編輯器缺的：複製貼上、方向鍵微調、對齊參考線、多選）與順序。使用者：「照你建議做吧。」記成 `v8-tool-ui-order`，README 加 `v8-tool-ui-editor-clipboard`、`v8-tool-ui-editor-nudge` 與兩個候選批。
- 2026-10-30：`v8-tool-ui-invariants` 開始（檔位 medium，引擎 Opus 5.5，估 2～3 h）。起草 `GYO::Ui` 與 ui_editor 的 Design Invariants＋Replacement Criteria，給使用者看。新發現：`GizmoMath` 為了反算 `position`／`size_delta`，又寫了一份 `gyo.ui` 的版面公式（引擎的 `ResolveDesignRect` 在 `UiRuntime.cpp` 的匿名命名空間，不公開）；草稿用守衛測試讓兩份保持一致。
- 2026-10-30：使用者同意 Design Invariants 草稿。寫進 ui_toolchain.md，加三個守衛。整個建置目錄重建後，Ui、ui_editor、OS 守衛共 14 個測試通過；突變與植入違規的檢查見 [dev_log](../../../dev_logs/2026_10_30_tool_v8_ui_invariants.zh-Hant.md)。
- 2026-10-30：`v8-tool-ui-editor-panels` 完成（檔位 medium，估 3～5 h，實際約 0.5 h）。`EditorApp.cpp` 拆成 5 個檔加一個共用的內部標頭；類別與標頭不變。逐行比對沒有遺漏，14/14 測試通過。
