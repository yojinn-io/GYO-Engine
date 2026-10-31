# v8 工具線交接

更新：2026-10-31。Owner：三個工具各自的 owner（見 [README](README.md)）。**狀態：先處理 UI Editor（`v8-tool-scope-ui-first`）。PR #107～#109、#111 已合併（#111：輸入法檢測、組字事件、gyo.ui v2 的 text_field、大廳改用 text_field，Windows 實機通過）。`v8-tool-ui-editor-text-field` 完成並合併（PR #114）。**
本文件是工具線的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [README](README.md)。v8 產品計畫的同步由產品會話做，本線不改 `docs/object_fps_pvp/plans/v8/`。

## 代號對照

代號規則照 [docs/architecture.md](../../../architecture.md)「Plan records and identifiers」（`v8-docs-id-scheme`）。

| 代號 | 意思 |
|---|---|
| ui-widgets-line | PR 線：文字輸入框元件、輸入法組字、ui_editor |
| anim-events-line | PR 線：動畫事件從無到手工（`gyo.anim_events`），再到工具（模型檢視） |

## 現況（每次停下來時更新）

- **完成**：`v8-tool-plan`（本計畫、本 HANDOFF）；先處理 UI（`v8-tool-scope-ui-first`）。
- **待討論**：`v8-tool-voxel-map-editor`（你會給方案）。
- **anim-events-line（另一個會話）**：`v8-tool-assets-anim-events` 完成，[PR #119](https://github.com/yojinn-io/GYO-Engine/pull/119) 已合併進 master（`45019dff`）。契約在 [animation-events.md](../../../architecture/animation-events.md)。下一批 `v8-tool-model-viewer`。
- **進行中**：`v8-tool-assets-plan` 已核准（[assets.md](assets.md)）。audio-tool-line：`v8-tool-audio-editor-convert` 完成（核心程式庫＋CLI，[dev_log](../../../dev_logs/2026_10_31_tool_v8_audio_editor_convert.zh-Hant.md)），[PR #118](https://github.com/yojinn-io/GYO-Engine/pull/118) 修了 Windows 的編譯錯誤，等 CI；下一批 `v8-tool-framework-shell`；`-edit` 等音效線的 GYO::Audio 增強。
- **完成並合併**：[PR #107](https://github.com/yojinn-io/GYO-Engine/pull/107)（`v8-tool-plan`、`v8-tool-ui-editor-file-browser`、審查、`v8-tool-ui-editor-gizmo-math`）。
- **已合併（[PR #108](https://github.com/yojinn-io/GYO-Engine/pull/108)）**：`v8-tool-ui-editor-gizmo-fix`、`v8-tool-ui-invariants`（[ui_toolchain.md](../../../ui_toolchain.md)「Design Invariants」＋三個守衛）、`v8-tool-ui-editor-panels`（面板拆檔，純搬移）、`v8-tool-ui-editor-element-schema`（元素表）。整理（S-1～S-3）完成。
- **已合併（[PR #109](https://github.com/yojinn-io/GYO-Engine/pull/109)）**：`v8-tool-ui-editor-clipboard`、`v8-tool-ui-editor-nudge`，使用者在 macOS 的 GUI 上試過。`text_field` 的三個決定已定（見決定紀錄）；`v8-tool-ui-ime-observe` **停下回報**：Windows 場次沒有照事前宣告做，判定不成立；log 顯示了機制（見 [dev_log](../../../dev_logs/2026_10_30_tool_v8_ui_ime_observe.zh-Hant.md)）。你選了 (b)，後續規劃已確認。`v8-product-lobby-fullwidth`、`v8-engine-input-composition` 完成（Windows 的 L3 都未做）；Ui 的 Design Invariants 已改（`v8-tool-ui-invariants-text`）；`v8-tool-ui-text-field` 完成（gyo.ui v2）。`v8-product-lobby-text-field` 完成，Windows L3 全部通過（組字顯示、候選窗位置、全形轉半形、Enter／Esc）。ui_editor 的 `v8-tool-ui-editor-text-field` 已合併（[PR #114](https://github.com/yojinn-io/GYO-Engine/pull/114)），使用者在 macOS 的 GUI 上試過。
- **等你決定**：下一節。
- **發現（不在本線的範圍，只報告）**：
  - PvP 有一份從舊產品複製的 `RetroFPS/World/GridMap.hpp`，經 `GameData.hpp` 被武器標頭間接包含，但 PvP 沒有它的載入器。是不是遺留物沒有查證；交給架構健檢。

## 等你決定

每項：機制、取捨、證據位置、建議。「會改變哪個決定」寫在最後一欄。

| 代號 | 問題 | 選項與取捨 | 建議 | 會改變 |
|---|---|---|---|---|
| `v8-tool-ui-navigation` | 頁面（canvas）跳轉用 if／else 分派 action 字串 | 現況：`object_fps_v2/src/App/ObjectFpsUi.cpp:463-500` 的 if／else 鏈（使用者留的 `TODO: デザイン、問題がある`）、`CanvasFor` 的 switch；PvP 只有一個 canvas，`PvpApplication.cpp` 的 `Execute` 也是 if／else。選項：(A) 產品內改成表（action id → 處理函式），初始化時雙向檢查文件宣告的 action 都有處理、處理的 action 都有宣告；不改契約／(B) `gyo.ui` 加「純 UI 的頁面切換」（按鈕宣告目標 canvas，Ui 自己切；產品只處理有遊戲效果的 action），ui_editor 預覽能點著走；契約變更，且要劃清「哪一頁」的權威（遊戲階段歸產品、純 UI 子頁歸 Ui，AGENTS §3 狀態的權威）／(C) 資料裡寫完整的畫面狀態機與條件（往設定語言走，AGENTS §9 禁止） | (A) 先做；(B) 在 ⑤ 暫停選單、畫面設定要加頁面時，以實際的頁面數再決定 | 是否改 `gyo.ui` 契約 |
| `v8-tool-ui-background` | main menu 的背景圖或動畫 | 靜態背景圖：`gyo.ui` v1 已有 `image` 元素（`UiDocument.hpp:150`），錨點 0～1、`size_delta` 0、放在第一個子元素就是全螢幕背景，**不用改契約**。動畫：v1 明寫沒有 animation（ui_toolchain.md「JSON v1」）。(a) 產品在 UI 後面畫動態場景（3D 場景、shader），UI 的底色設透明／(b) `gyo.ui` 加有限的動畫（例如圖片的 UV 捲動、逐格動畫），是契約變更（v2）／(c) 影片：要解碼器，最重 | 靜態圖先做；動畫先決定要哪一種效果 | 是否改 `gyo.ui` 契約 |

## 決定紀錄

| 代號 | 決定 | 使用者原話 |
|---|---|---|
| `v8-tool-assets-anim-events-owner` | （2026-10-31）`gyo.anim_events` 的契約放在 `GYO::Model`（`engine/model/AnimationEvents.hpp`，文件 [animation-events.md](../../../architecture/animation-events.md)）；catalog 型別沿用 `text`，檔頭 `schema` 欄位；照草稿寫 Design Invariants 5 條與 Replacement Criteria 3 條。Architecture Delta：GYO::Model PRIVATE 連結 nlohmann_json | 「照建議核准」 |
| `v8-tool-assets-anim-events-align` | （2026-10-31）對齊方式 (ii)：事件標在畫面上的時刻，音效資料用 `sync_seconds` 指出音檔裡的對齊點，動作開始時一次排好 | 「照建議核准」 |
| `v8-tool-plan-location` | （2026-10-31 補記）照建議：計畫放 `docs/tools/plans/v8/`，UI 的格式與 Design Invariants 仍在 [ui_toolchain.md](../../../ui_toolchain.md)。從 `v8-tool-plan` 起 Claude 照自己的建議放了，使用者沒有另外選過；列出選項後使用者接受現狀 | 「做了就沒事了，我還以為你沒做。」 |
| `v8-tool-start-timing` | （2026-10-31 補記）實際的做法：檢測 `v8-tool-ui-ime-observe` 先做，之後每一批都由使用者說「開始」才動手（ui-widgets-line 已全部合併，#107～#114），不是等 ③ 之後。使用者沒有另外選過；列出選項後使用者接受現狀 | 「做了就沒事了，我還以為你沒做。」 |
| `v8-tool-ui-editor-scope-menus` | （2026-10-30）UI Editor 只處理遊戲選單等介面，不管 HUD。HUD（例如 `hit_feedback.json`）由產品自己畫 | 「UI Editor只處理遊戲菜單等界面，而不管HUD。」 |
| `v8-tool-ui-editor-mount-ux` | （2026-10-30）ui_editor 的路徑選擇先用 SDL3 的系統選擇窗（選項 1）：Mount、Open、Export 與掛載對話框的欄位都用；不記住路徑；平台跳不出窗時退回手打。ImGui 自己畫的版本（像 Godot 的預設）之後要時再做，那時才需要 `GYO::Os` 加列目錄（`v8-engine-os-list-directory`，未開） | 「先做1吧，先把功能實現。」 |
| `v8-tool-ui-invariants-text` | （2026-10-30）[ui_toolchain.md](../../../ui_toolchain.md) 的 GYO::Ui Design Invariants：改第 4 條（編輯中的內容由 Ui 暫時持有，值仍歸產品）、新增 5（字元由產品的檢查函式決定）、6（組字只顯示）、7（文字事件經產品轉成 Ui 的中立型別）；Replacement Criteria 加「超出通用編輯集合就停」。5、6 的守衛隨 `text_field` 加 | 「同意，寫進去」 |
| `v8-tool-ime-input-delta` | （2026-10-30）`v8-engine-input-composition` 由工具線（本會話）做，Architecture Delta 記到 input-and-present 的 HANDOFF | 選「工具線做（建議）」 |
| `v8-tool-ime-plan` | （2026-10-30）照 README「檢測之後：輸入法的規劃」排兩批；產品批 `v8-product-lobby-fullwidth` 也由本會話做 | 選「照這樣排（建議）」「這個會話做」 |
| `v8-tool-ui-ime-observe-next` | （2026-10-30）(b)：判定記為不成立；接受這次的 log 當證據，以觀測到的事實規劃 `v8-engine-input-composition`（README「檢測之後：輸入法的規劃」） | 「(b) 接受證據、往下規劃」 |
| `v8-tool-ui-ime-observe-judgment` | （2026-10-30）照 [README](README.md)「v8-tool-ui-ime-observe 的事前宣告」做 Windows 場次與判定；看了 log 之後不改 | 「同意」 |
| `v8-tool-ui-schema-version` | （2026-10-30）`text_field` 用 `gyo.ui` `version: 2` 加入；讀取端接受 1 與 2，舊讀取端以版本不支援明確拒絕 v2 | 選「升 version 2（建議）」 |
| `v8-tool-ui-edit-policy-owner` | （2026-10-30）文字編輯的通用部分（游標、刪除、全選、貼上、組字顯示）放 `GYO::Ui`；字元過濾留在產品，由產品提供檢查函式。這是 Architecture Delta（Ui 多了文字編輯的責任），開始實作前要改 [ui_toolchain.md](../../../ui_toolchain.md) 的 Design Invariants，內容另給使用者同意 | 選「通用部分放 GYO::Ui（建議）」 |
| `v8-tool-ui-widget-scope` | （2026-10-30）這次只加 `text_field`；按鈕的共用樣式（`v8-tool-ui-button-style`）等第二個畫面也重複時再排 | 選「只加 text_field（建議）」 |
| `v8-tool-ui-editor-element-schema` | （2026-10-30）元素表放在 ui_editor（選項 A），用測試和 GYO::Ui 的 codec／evaluator 對齊；Engine 不為工具提供描述。原則：工具經資料與 Engine 交接，Engine 不承擔工具的職能，工具可以隨時修改或更換 | 「A：放 ui_editor」；「我們可以通過資料來與Engine交接。我的理解是這個，Engine不需要承擔tools的職能，這樣以後tools可以隨時修改和更換。」 |
| `v8-tool-ui-editor-structure` | （2026-10-30）審查的整理項目 S-1～S-3 都做：`v8-tool-ui-editor-gizmo-math`、`v8-tool-ui-editor-panels`、`v8-tool-ui-editor-element-schema`（已合併，PR #107、#108）。S-4 不排 | 「這些問題需要重構掉」 |
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
- 2026-10-30：`v8-tool-ui-editor-element-schema` 完成（檔位 high，估 5～8 h，實際約 1 h）。使用者選 A，並補充「Engine 不承擔工具的職能」。`ElementSchema` 一張表；對齊測試含編譯期的 switch；8 個突變全部被抓到；14/14 通過。
- 2026-10-30：使用者要求把 4 個 commit 併進 [PR #108](https://github.com/yojinn-io/GYO-Engine/pull/108)；推送並更新 PR 的標題與說明（gizmo 修正＋Design Invariants＋S-1、S-2 的整理）。
- 2026-10-30：音效線（v8 audio-line 會話的 `v8-engine-audio-plan`）轉來一項需求：使用者在那邊說「這個你先轉好，需求加到 assets editor 中。」記到 README 的 `v8-tool-assets-editor`，代號 `v8-tool-assets-audio-import`；只記需求，不排實作。轉換腳本在音效線未推送的分支上。
- 2026-10-30：觸發的手動 L1 被取消後，留下 `L1 (dispatch)` failure，使 PR #108 變成 UNSTABLE；重跑後四平台 L1 與 Quick acceptance 全過。使用者要求合併，PR #108 以 merge commit 合併（`e31dfdf4`）。
- 2026-10-30：使用者：「先做小功能」。`v8-tool-ui-editor-clipboard`、`v8-tool-ui-editor-nudge` 開始（high／medium，估 3～5 h＋2～3 h），分支 `claude/v8-tool-ui-editor-small`（切自 master `e31dfdf4`）。
- 2026-10-30：兩批完成，實際約 1 h。新測試與 8 個突變全部被抓到；14/14 通過。GUI 上還沒手動操作。
- 2026-10-30：使用者在 GUI 上試過複製貼上、方向鍵與修飾鍵：「可以了。繼續」。推送並開 PR。
- 2026-10-30：開 [PR #109](https://github.com/yojinn-io/GYO-Engine/pull/109)。`text_field` 的三個決定：升 v2、通用編輯放 `GYO::Ui`（過濾留產品）、只加 `text_field`。
- 2026-10-30：使用者：「開始」。`v8-tool-ui-ime-observe` 開始（medium，估 2～4 h＋實機）。加了 `SdlInput::SetTextDiagnostics`（只加不改，frame 不變）與客戶端 `--text-input-trace`；單元測試＋3 個突變全被抓到；Mac 上實際啟動客戶端，確認 `input.text` 進了客戶端 log。Mac 沒有 PowerShell，Windows 端只用一行 `.cmd`（未在 Windows 上執行過），分析腳本在 Mac 上用合成 log 測過。事前宣告寫進 README，等使用者同意。
- 2026-10-30：使用者同意 `v8-tool-ui-ime-observe` 的事前宣告。
- 2026-10-30：音效線更正 `v8-tool-assets-audio-import` 第 3 點：原檔不能放在資產根目錄下（產品的 catalog 測試要求根目錄下每個檔都登錄），改成放在資產根目錄以外；第 4 點的記錄位置改為音效線的 dev_log。README 已更新。
- 2026-10-30：使用者：「合併 #109、另開 PR」。PR #109 以 merge commit 合併（`5cfa149c`）。剩下的 4 個 commit（text_field 的決定、ime-observe、同意、音效需求更正）rebase 到 master，分支 `claude/v8-tool-ui-ime-observe`，另開 PR。
- 2026-10-30：音效線轉來第二項 assets editor 需求：動畫事件時刻與音效同步資料化（`v8-tool-assets-anim-events`），使用者原話「這個聲音和動畫的管理是資料化的吧，是的話，就給Assets Editor來處理。」記到 README；只記需求。
- 2026-10-30：清理：`v8-tool-ui-editor-structure` 已做完卻還在「等你決定」，移到決定紀錄；README 的 `v8-tool-ui-editor-file-browser` 補上 ✓。
- 2026-10-30：收到 Windows 場次的 log 與回報（日文 IME、無中文 IME、每次的畫面結果未記錄）。收支一致（169 行、34,990 bytes、丟棄 0）。判定不成立，停下回報。觀測到：組字事件 0 筆（SDL 預設由 OS 畫組字）、字在確定時成批送來、平假名模式送全形字元被大廳的 ASCII 過濾丟掉、半形可以。log 與分析輸出存在 `evidence/ime-observe/`。
- 2026-10-30：使用者選 (b)。規劃 `v8-engine-input-composition`（平台選項 `imeComposition`、`TextEvent::Kind::Composition`、`SetTextInputArea`）與產品批 `v8-product-lobby-fullwidth`，寫進 README，等確認。
- 2026-10-30：使用者確認規劃；組字事件由工具線做；產品批由本會話做。`v8-product-lobby-fullwidth` 開始（medium，估 1～2 h）。
- 2026-10-30：`v8-product-lobby-fullwidth` 完成，實際約 0.5 h。`FilterAddressText`；Red（舊規則下 4 個全形斷言失敗）→ Green；5 個突變全被抓到（1 個先存活、補測試後抓到）；產品 CPU 測試全過。Windows L3 未做。
- 2026-10-30：使用者：「開始」。`v8-engine-input-composition` 完成（high，估 6～10 h，實際約 1 h）：`imeComposition`、`Kind::Composition`、`SetTextInputArea`；Red→Green；7 個突變全被抓到；改了上一批診斷測試的判定（§14.4，見 dev_log）。Windows L3 要等有畫組字的消費端。
- 2026-10-30：使用者同意 GYO::Ui Design Invariants 的修改稿，寫進 ui_toolchain.md（`v8-tool-ui-invariants-text`）。
- 2026-10-30：使用者：「開始」。`v8-tool-ui-text-field` 完成（high，估 10～16 h，實際約 15 分鐘）：gyo.ui v2、`text_field`、`UiTextInput`、`SetTextFilter`、renderer 的游標／底線／選取。14 個突變全被抓到（1 個先存活、補測試後抓到）；測試抓到 1 個實作錯（丟棄後同一幀又開始編輯）已修；改了既有版本測試的判定（§14.4）。xhigh 審查沒開（要你同意）。
- 2026-10-30：使用者：「大廳改用 text_field」。`v8-product-lobby-text-field` 完成：大廳 v2、位址欄 `text_field`、產品轉換 `TextInput`、`SyncTextInput`、`SetTextFilter(FilterAddressText)`、`imeComposition`；刪掉手寫的編輯。行為變化：編輯中點其他按鈕會丟掉打到一半的位址（Ui 不變式 4），要按 Enter 套用。Mac 上啟動確認 `ime_ui="composition"`。Windows L3 未做。
- 2026-10-30：Windows L3（package-trial run 41066623724）：A、B1、B2、B3、C、D 全部「是」，符合事前宣告。log 的 `text_editing` 22 行（上次 0 行）。證據在 `evidence/lobby-text-field/`。PR #111 的 L1／macos-arm64 失敗在 `audio_sdl.disk`（音效線的測試，master 上也失敗），已轉給音效線，只重跑失敗的 job。
- 2026-10-30：PR #111 重跑失敗的 job：`L1 / macos-arm64` 這次通過，四列都通過、CLEAN。第一次的失敗（`audio_sdl.disk`，`SdlAudioOutputTests.cpp:158`，音效線的測試）依 §14.1 留在紀錄裡：原因在音效線（master 上同一行也失敗過），不是本 PR 的實作。
- 2026-10-31：PR #111 的 L1／macos-arm64 第二次又失敗在 `audio_sdl.disk`（音效線的測試；原因與修正在音效線的 PR #113：disk 驅動不等待，錨點紀錄溢出）。使用者：「再重跑一次」。只重跑失敗的 job，通過；四列都通過後以 merge commit 合併（`80d14f22`）。三次執行中這個測試失敗 2 次，記錄留著（§14.1）。
- 2026-10-31：使用者：「開始」。`v8-tool-ui-editor-text-field` 完成（high，估 8～12 h，實際約 6 分鐘）：元素表的整數／文字欄位與 `styledText`、Inspector 的「Fields」、預覽中可打字（用 ImGui 的內部 `PlatformImeData` 要求文字輸入，已記在 dev_log，對照 Replacement Criteria 目前未觸發）。7 個突變全被抓到；21/21 通過。GUI 手動操作未做。
- 2026-10-31：使用者試用時發現：在大廳按「+ Canvas」出現 `unknown backdrop color 'transparent'`。原因是新 canvas／新元素的預設值用到大廳沒宣告的顏色（既有缺陷）。Red（7 項失敗）→ 修正：缺的顏色從編輯器預設調色盤補上宣告 → Green；4 個突變全被抓到。
- 2026-10-31：使用者：解析度放在編輯器裡、可編輯但預設鎖定，顯示區域畫框表示。做了 Inspector 的「Design size」（預設鎖定，鎖定狀態不存檔）與畫布上的設計畫面框（依文件的 `design_canvas.size`，標示尺寸）。解析度仍屬於各文件。
- 2026-10-31：使用者：「正常，合併」。PR #114 以 merge commit 合併（`16326f78`）。使用者：「清掉」。「等你決定」裡已照做的 `v8-tool-plan-location`、`v8-tool-start-timing` 移到決定紀錄；README 的 `v8-tool-ui-ime-observe` 補上 ✓。使用者要求列出這兩項的選項；Claude 說明兩項都是自己照建議做的、使用者沒有選過，使用者：「做了就沒事了，我還以為你沒做。」
- 2026-10-31：使用者給了資產工具鏈的方向（專用工具 → Asset 登錄 → Build／打包 → Runtime；五類工具的範圍與共用規則），要求先對照現有架構提出最小可行的邊界、交接與順序。`v8-tool-assets-plan` 開始（high，Opus 5.5，估 1.5～2 h，實際約 40 分鐘）。盤點了 catalog、內容契約與組裝、shader 的離線處理、音效轉換腳本、Runtime loader、熱重載、工具登錄、Os 的行程 API。觀察到的壓力：音效原檔沒有放處、聲音與動畫對不上。草稿寫在 [assets.md](assets.md)，停下等決定。使用者更正：「assets不是管全局的，他也是針對單個產品的。」草稿原本把三個產品各一份的動畫集解析器當成要收成共用的壓力，已撤下；改寫成以單一產品為單位，新增待決 `v8-tool-assets-anim-events-format`。
- 2026-10-31：使用者：「照 (b)」。`v8-tool-assets-anim-events-format` 定為通用格式（契約歸 Engine 的動畫 owner，資料在各產品），記在 [assets.md](assets.md) 的決定紀錄。其餘五個待決未定。
- 2026-10-31：使用者更正：「這個是一條工具鏈而不是一個工具。沒有統一的都去地方和輸出地方。核心是工具加工素材-》assets 登錄，（在遊戲打包是通過assets build 成自定義的二進制包，這個功能可以延後），關鍵是現在的工具。（手工-腳本-工具）這個順序。」草稿重寫：撤下製作設定格式、處理器、Build 階段與四個相關待決；改成各類資產的階段表（音效：腳本 → 工具；動畫事件：無 → 手工；模型：手工 → 之後工具）與兩條 PR 線。
- 2026-10-31：使用者要求列出要做的 Editor 與內容（音效工具、模型檢視工具；動畫事件先手工），之後：「OK，這樣也行，開始。」規劃核准。`v8-tool-audio-editor-convert` 開始（high，重新取樣局部 xhigh，Opus 5.5，估 6～10 h）；第一步是 Design Invariants（新 Component，AGENTS §3），起草後停下等核准。
- 2026-10-31：`v8-tool-audio-editor-convert` 讀了 `convert_wav.py`、ui_editor 的外殼（`ImGuiSdlRendererHost.cpp`、`SdlPathDialogs.cpp`、`FileService.cpp`，約 600 行）、audio.md 的契約與不變式。發現三件要你決定的事：(1) 計畫寫的「和腳本逐位元組相同」要重現 Python 的亂數種子（字串種子經 SHA-512 的 MT19937）、四捨六入五成雙、關閉 FMA，跨平台還受 libm 影響；四個音效的原檔也已找不到，無法重產現有資產；(2) 外殼要複製還是抽成共用（新的共用層是 Architecture Delta）；(3) Audio 不能停止或替換播放中的聲音（Invariant 7、Replacement Criteria 2），預覽只能每次重建輸出。起草了 Design Invariants，停下等核准。分支 `claude/v8-tool-audio-editor`（切自 `claude/v8-tool-handoff-cleanup`）。
- 2026-10-31：使用者對草稿的回覆：輸出格式「這個可以調整，不要弄太死。如果未來Engine 升級，我這個可以不用變。」→ 不變式 2 改成跟著 Engine 公開的格式（取樣率取自 `GYO::Audio` 的 `kSampleRate`，取樣格式以 `SoundLoader` 實際載入來守衛），工具不自留數字。預覽的限制：「這個需要增強GYO::Audio 的功能。」→ 屬於 audio.md 的 Replacement Criteria 1、2（播放中載入片段、停止已送出的聲音），要 Audio 的 owner 規劃；`v8-tool-audio-editor-convert` 不需要播放，可先做，`-edit` 等 Audio 增強。
- 2026-10-31：使用者回答：Audio 增強「音效線做」（需求已用 SendMessage 轉給音效線會話）、驗收「(2) 演算法等價」、不變式「照草稿寫進去」（寫進 `docs/audio_editor/architecture.md`）。外殼的問題使用者反問「現在是否可以建立framework層？」，回答後等決定；`-convert` 先做核心程式庫＋CLI，不需要外殼。
- 2026-10-31：使用者：「照這個範圍」。`v8-tool-framework-shell` 加進 audio-tool-line，排在 `-edit` 之前。`-convert` 開始實作（核心程式庫＋CLI）。
- 2026-10-31：使用者：「Engine保持純粹，framework則提供整合工具。」記為 `v8-tool-framework-principle`；catalog 寫入屬於整合，`-framework-shell` 時從音效工具搬進 framework。
- 2026-10-31：`v8-tool-audio-editor-convert` 完成（約 50 分鐘，估 6～10 h）。事前宣告的判定全部通過；完整 ctest 52／52；突變 17／17。事前宣告的抖動文字寫錯（「≤ 1 LSB（抖動加四捨五入）」合計其實可到 1.5），在看結果之前照使用者選項的「±1 LSB 幅度」改成直接檢查抖動本身，並加一條合計 < 1.5（只締め，§14.4）。
- 2026-10-31：音效線回報（會話訊息，不是使用者原話）：GYO::Audio 控制層重建的計畫已核准，`docs/architecture/plans/audio/CONTROL.md`（分支 `claude/v8-engine-audio-control`，`7fb77575`，未推送），實作未開始。介面：`Submitter::Play` 回傳 `SoundHandle`；`Stop(handle, at)`／`StopAll(at)`；每個提交端的片段槽 `LoadClip`／`UnloadClip`（`MixerConfig::clipSlotsPerSubmitter`，建議 8）；`Mixer::PlayheadOf(handle)` 只供顯示、最多晚一個區塊。工具端要處理：`Stop`／`UnloadClip` 佇列滿回 false 時重試、`LoadClip` 回 `NoClipSlot` 時下一幀重試。音效線說使用者決定 Engine 的 5 批（baseline→stop→clips→playhead→live，約 15～20.5 h）和 `v8-tool-audio-editor-edit` 同一條 PR 線，Engine 先做。哪個分支、哪個會話做 `-edit` 待對齊。
- 2026-10-31：anim-events-line 由另一個會話開始（`v8-tool-assets-anim-events`，high，Opus 5.5，估 6～8 h）。分支 `claude/nifty-lovelace-dd931a`，切自 `v8-tool-handoff-cleanup`（`assets.md` 還沒進 master）。提出契約位置、格式 v1、Design Invariants 草稿與對齊方式，你核准（「照建議核准」）。
- 2026-10-31：`v8-tool-assets-anim-events` 的程式、資料、測試完成；突變 Engine 13／13、PvP 8／8（第一次有 7 個沒被殺：doctest 篩選含逗號、刪行造成編譯失敗、訊息印成指標、CMake 折行，修正後全部 killed）。從骨骼的局部變換推出彈倉插到底在 clip 2.35 s，音檔兩組聲音對應插到底與滑套。等你實聽。
- 2026-10-31：你在 macOS 本機實聽換彈：「It matches」。`v8-tool-assets-anim-events` 完成，推送並開 PR。
- 2026-10-31：PR #119 的 L1（linux-x64、macos-arm64、windows-x64）全部通過，CI gate 通過，CLEAN。等你決定合併。
- 2026-10-31：PR #119 合併後，#118 和 master 在本檔與 README 的 dev_log 一覽衝突（兩條線各自加了行）。合入 master，兩邊的記錄都保留，現況改成 #119 已合併。
- 2026-10-31：你：「CI炸了。」#118 的 L1 windows-x64 編譯失敗：`CatalogRegistration.cpp` 的 json 和 `std::string_view` 比較在 MSVC 上有歧義（C2666）。`9d1e6774` 那次就已失敗，我當時沒有回報。改成先取出字串再比較，突變的 `find` 跟著改，重跑 killed；本機音效工具的測試 4／4 通過。
