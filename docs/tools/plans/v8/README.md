# v8 工具線計畫（④）

更新：2026-10-30（`v8-tool-plan`）。Owner：三個工具各自的 owner（見各節）；本文件是 v8 ④ 的規劃，產品的 v8 計畫在 [object_fps_pvp v8 README](../../../object_fps_pvp/plans/v8/README.md)。
**狀態：先處理 UI Editor（`v8-tool-scope-ui-first`），等使用者核准；沒有實作。** 記錄器是 [HANDOFF](HANDOFF.md)。

## 定位

- v8 的順序是 ① 基礎功能 → ② 體驗優化 → ③ 手感時序 → ④ 工具 → ⑤ 流程項目（v8-product-order）。使用者稱 ④ 為「Engine 的穩定」。本文件規劃 ④ 的三個工具；先處理 `v8-tool-ui-editor-widgets`，其他兩個等使用者的方案（見「待討論的兩個工具」）；工具線的規劃和 ①～③ 並行（使用者要求），**實作何時開始由使用者決定**（`v8-tool-start-timing`）。
- 三個工具都照 AGENTS §8：`Editor → Data Contract → Application`。Application 只讀 Data Contract，不連結、不初始化、不知道 Editor。Contract 的資料用手寫、Script、別的工具都能產生。
- 三個工具都照 AGENTS §5「手動 → Script → Tool」：先把手動的步驟、輸入、輸出、失敗條件寫清楚，再做工具。
- 工具的登記照 [tool_projects.md](../../../tool_projects.md)：`tools/<id>/`、`project.json`、`engine/config/tools.csv` 的一列、`tests/<id>`、`build/acceptance/<id>/`。新增工具只改這些，不改共通 Build。
- 估時是第一版，誤差可能到 ±50%，每批開始前重估；超過估時 1.5 倍就停下回報。

## 現況盤點（2026-10-30，分支 `claude/v8-tool-line-3a892c`，切自 `claude/pvp-v8-pv8-line`）

| 項目 | 現況 | 證據 |
|---|---|---|
| `tools/ui_editor` | 編輯 `gyo.ui` JSON 的獨立工具，release 工具（每個平台都打包 GUI 版）。ImGui＋SDL_Renderer，不連結遊戲、Input、SDL_GPU。資產目錄唯讀掛載，只輸出一個 JSON，不寫 catalog | `tools/ui_editor/README.md`、`tools/ui_editor/project.json`、[ui_toolchain.md](../../../ui_toolchain.md) |
| `tools/object_fps_preview` | `requires_apps: {"game": "object_fps"}`；`object_fps` 在 `projects.csv` 停用、也已不能對現行 Engine 編譯；工具在 `tools.csv` 也停用 | `tools/object_fps_preview/project.json`、[tool_projects.md](../../../tool_projects.md) 末段 |
| `engine/ui`（`GYO::Ui`） | `gyo.ui` v1 的元素集合是**封閉的**：`container`、`panel`、`image`、`text`、`button`、`horizontal_slider`、`fixed_step_list`。沒有文字輸入框。`UiInputFrame` 只有焦點、確認、取消、指標，沒有文字 | [ui_toolchain.md](../../../ui_toolchain.md)「JSON v1」、`engine/ui/include/engine/ui/UiTypes.hpp:129` |
| `GYO::Ui`、ui_editor 的 Design Invariants | **沒有**。[ui_toolchain.md](../../../ui_toolchain.md) 與 [tool_projects.md](../../../tool_projects.md) 都沒有 `Design Invariants` 節 | `grep -n "Design Invariants" docs/ui_toolchain.md docs/tool_projects.md` 無結果 |
| 大廳的按鈕 | 已經是 `gyo.ui` 的 `button`（`assets/object_fps_pvp/ui/pvp_lobby.json`）。每個按鈕重複寫一樣的 `background`、字級、對齊 | `pvp_lobby.json` |
| 大廳的位址輸入 | 是一顆 `button`，文字綁 `address`；按下後產品自己開文字輸入、逐字過濾（`AppendAddress`）、處理 Backspace／Ctrl+A／Ctrl+V／Enter／Esc（`HandleTextEditing`），游標用「`_`」接在字串後面 | `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:195-260` |
| 輸入法組字 | SDL 後端只收 `SDL_EVENT_TEXT_INPUT`（確定的字），**不收 `SDL_EVENT_TEXT_EDITING`**（組字中的字），也沒有 `SDL_SetTextInputArea`（候選窗位置）。所以組字中畫面上什麼都不顯示；「:」卡住推論是組字中（未重現） | `engine/input/backend/sdl/src/SdlInput.cpp:316-334`；`grep TEXT_EDITING engine tools apps` 無結果 |
| Input 的 Design Invariants | 4：frame 不帶擷取政策、文字編輯政策與產品知識。1：frame 只能用新增來擴充 | [input.md](../../../architecture/input.md)「Design Invariants」 |
| PvP 的地圖 | `arenas.json`（v1）列出 `pvp_arena.json`、`pvp_corners.json`。arena v1 = 尺寸、角色尺寸、**AABB 牆的清單**、出生點。載入在 `fps::pvp::Arena::Load`／`LoadInstalledArenas` | `assets/object_fps_pvp/*.json`、`apps/object_fps_pvp/include/RetroFPS/Pvp/Arena.hpp` |
| arena 與協議 | arena 內容有 digest，Match 與 Client 比對。條文在 pv6 §2：**`Arena` 新增成員或改正規化＝wire 變更**；只改內容不是 wire 變更。pv7、pv8 都寫明「沒有改寫的條款原樣沿用」，兩者都沒改寫這一條；pv8 草稿還明寫蹲下參數「不進 `arena_digest`」。現行程式仍帶 `arena_digest`。pv8 重開中，新範圍有可能改這一條 | [protocol-v6](../../../object_fps_pvp/protocol-v6.zh-Hant.md) 第 51～52 行；[protocol-v7](../../../object_fps_pvp/protocol-v7.zh-Hant.md) 第 5 行；[protocol-v8](../../../object_fps_pvp/protocol-v8.zh-Hant.md) 第 4、54 行；`client_v7.proto:24`、`runtime_v7.proto:18`、`IpcHost.cpp:489` |
| 舊產品的格子地圖 | `object_fps` 的 `GridMap`／`GridMapLoader`（ASCII 文字地圖，`assets/object_fps/maps/*.txt`）、`MapGeometryGenerator`。產品停用 | `apps/object_fps/src/World/`、`apps/object_fps/src/Rendering/MapGeometryGenerator.cpp` |
| PvP 裡的 `GridMap.hpp` | 從舊產品複製的標頭，經 `RetroFPS/Data/GameData.hpp` 被武器標頭間接包含；PvP 沒有它的載入器，地圖用的是 `Arena` 的 AABB 牆 | `apps/object_fps_pvp/include/RetroFPS/World/GridMap.hpp`、`Gameplay/Weapon/WeaponState.hpp:3` |
| hitbox 的先例 | `object_fps_v2` 的 `hurt_regions`：每項 `id`、`start`／`end` 骨骼點（`node`＋可選 `offset`，模型座標）、`radius`、可選 `damage_multiplier` | [v2 enemies](../../../object_fps_v2/enemies.zh-Hant.md) 第 117 行附近 |
| ImGui 後端 | 共通 Build 的 `IMGUI` 元件只對應 `imgui::sdl3_sdlrenderer3`。3D 視窗要 SDL_GPU，需要新的 ImGui 後端元件 | `build/cmake/GyoAppProject.cmake:131`、`build/cmake/GyoComponents.cmake:3` |

## 樹狀總覽

格式：「檔位｜引擎｜簡述」。引擎照 [變速箱規則](../../../../agents/effort-gearbox.md)「エンジン（モデル）」：主會話的引擎與檔位由使用者決定，下面是建議。「Opus 5.5」＝目前主會話的引擎；「Sonnet 5.5」用在唯讀偵察與對抗式檢查。

```
v8-tool-plan ✓                              「high｜Opus 5.5｜本計畫、HANDOFF」
│
├── ui-widgets-line                          「PR 線：文字輸入框元件＋輸入法組字＋ui_editor」
│   ├── v8-tool-ui-ime-observe               「medium｜Opus 5.5｜先檢測：把 TEXT_EDITING／TEXT_INPUT 記到 log，Windows 重現「:」卡住」
│   ├── v8-tool-ui-invariants ✓              「medium｜Opus 5.5｜加元件之前，寫 GYO::Ui 與 ui_editor 的 Design Invariants＋Replacement Criteria」
│   ├── v8-engine-input-composition          「high｜Opus 5.5｜Input frame 只加不改：組字事件、候選窗位置」
│   ├── v8-tool-ui-text-field                「high（編輯狀態局部 xhigh）｜Opus 5.5｜gyo.ui v2：text_field 元素、通用編輯狀態、組字顯示」
│   ├── v8-tool-ui-editor-file-browser       「high｜Opus 5.5｜SDL3 系統選擇窗，取代手打路徑（v8-tool-ui-editor-mount-ux）」
│   ├── v8-tool-ui-editor-gizmo-math ✓       「high｜Opus 5.5｜gizmo 數學抽成純函式＋測試（行為不變）」
│   ├── v8-tool-ui-editor-gizmo-fix ✓        「high｜Opus 5.5｜修拖動與縮放、加位置／大小把手（Unity 式）」
│   ├── v8-tool-ui-editor-panels ✓           「medium｜Opus 5.5｜面板拆檔，EditorApp 只協調」
│   ├── v8-tool-ui-editor-element-schema ✓   「high｜Opus 5.5｜元素的欄位與 Inspector 集中一處（加 text_field 前）」
│   ├── v8-tool-ui-editor-clipboard ✓        「high｜Opus 5.5｜元素的複製、貼上、就地複製（Ctrl+C／V／D）」
│   ├── v8-tool-ui-editor-nudge ✓            「medium｜Opus 5.5｜方向鍵微調；縮放時 Alt 從中心、Shift 等比例」
│   ├── （候選）v8-tool-ui-editor-snap-guides 「high｜Opus 5.5｜拖動時貼齊其他元素的邊與中線；排版覺得累時再排」
│   ├── （候選）v8-tool-ui-editor-multi-select「high｜Opus 5.5｜多選、對齊、平均分布；同上」
│   ├── v8-tool-ui-editor-text-field         「high｜Opus 5.5｜ui_editor 編輯、預覽、驗證 v2 文件」
│   ├── （延後）v8-tool-ui-button-style       「medium｜Opus 5.5｜按鈕外觀的共用樣式；第二個畫面也重複時再排（v8-tool-ui-widget-scope）」
│   └── v8-product-lobby-text-field          「high｜Opus 5.5｜大廳改用 text_field，刪掉手寫的編輯（產品批，產品會話做）」
│
└── （待討論）v8-tool-voxel-map-editor、v8-tool-assets-editor   「待討論：等使用者逐個給方案（v8-tool-scope-ui-first）」
```

## 範圍

UI Editor 只處理遊戲選單等介面，不管 HUD（`v8-tool-ui-editor-scope-menus`）。現況的評價與結構問題見 [ui-editor-review.md](ui-editor-review.md)。

## 順序與依賴

- 本線只做 ui-widgets-line。它不依賴 ①～③ 的其他 PR 線，也不需要 3D 或 SDL_GPU。
- 線內的順序（`v8-tool-ui-order`，2026-10-30 使用者同意）：
  1. `v8-tool-ui-invariants`：整理與加元件之前先寫約束。
  2. 整理：`v8-tool-ui-editor-panels`，再 `v8-tool-ui-editor-element-schema`。審查原本把 element-schema 排前面；改成先拆面板，因為拆檔是純搬移，之後元素表的改動只落在 Inspector 的檔裡，diff 比較好審。
  3. `text_field`：檢測（`v8-tool-ui-ime-observe`，要 Windows 實機）→ 組字事件（`v8-engine-input-composition`）→ `v8-tool-ui-text-field` → `v8-tool-ui-editor-text-field` → 大廳（產品批）。開始前要先定 `v8-tool-ui-schema-version`、`v8-tool-ui-edit-policy-owner`、`v8-tool-ui-widget-scope`。
  4. 編輯器的小功能（`v8-tool-ui-editor-clipboard`、`v8-tool-ui-editor-nudge`）：不依賴 `text_field`，在 2 之後可以穿插做。
  5. 對齊參考線、多選：候選，等畫面變多、排版真的覺得累時再排。
- ui_editor 不依賴其他工具，其他工具也不依賴它（[ui_toolchain.md](../../../ui_toolchain.md) 末段）。

## 工具 1：`v8-tool-ui-editor-widgets`（ui-widgets-line）

### 使用者需求

使用者（2026-10-22，v7.1 `sdl-isolation` 的 ip3f L3）：輸入「:」時忽然卡住（「不知道是不是輸入法的問題」），「操作手感怪怪的」；「v8排一個任務，組件化這些按鈕和輸入框，順便修改uieditor」。

需求拆成三件：

1. 大廳的位址欄要是真正的文字輸入框：看得到游標、看得到組字中的字、候選窗在輸入框旁邊。
2. 輸入框（和按鈕）是 UI 的元件，用 ui_editor 擺放，不是產品手寫。
3. ui_editor 能編輯、預覽新的元件。

不在這條線：手感的另一個來源，IP-2 已知的 drawable 取得停頓（約 66 ms），屬執行緒分離（v8 README 候選表）。

### Data Contract

- 契約是 `gyo.ui` 文件。加入 `text_field` 元素（`v8-tool-ui-schema-version`：建議升到 `version: 2`，讀取端同時接受 1 與 2；v1 的元素集合寫明是封閉的，舊的讀取端遇到新元素會明確拒絕）。
- `text_field` 的資料（草案，規劃時定）：`rect`、字型與字級、顏色、`binding`（初始值與提交後的值，string）、`placeholder`、`max_length`、`submit` action（payload `none`；值從 binding 讀回）、焦點順序沿用 `focus_order`。
- 字元的過濾（現在大廳只收 `a-z A-Z 0-9 . : - [ ]`）是產品的事。選項見 `v8-tool-ui-edit-policy-owner`。
- Application（大廳）讀 v2 文件；不需要 ui_editor。手寫 JSON 也能產生合法的 v2 文件。

### owner 與依賴方向

| 部分 | owner | 依賴 |
|---|---|---|
| 組字事件、候選窗位置 | `GYO::Input`（Engine；計畫夾 [input-and-present](../../../architecture/plans/input-and-present/)） | 只加不改（input.md 不變式 1）；frame 仍不帶編輯政策（不變式 4） |
| `text_field` 的 schema、驗證、版面、通用編輯狀態（游標、刪除、全選、貼上、組字顯示） | `GYO::Ui`（Engine） | Engine、Math；**不依賴 `GYO::Input`**：產品把 Input 的文字事件轉成 Ui 自己的中立型別（和現在的 `UiInputFrame` 相同） |
| 繪製游標、組字底線 | `GYO::UiRenderer` | Ui、Render、Text |
| 編輯與預覽新元素 | `tools/ui_editor` | `GYO::Ui`；不連結遊戲 |
| 位址的字元規則、提交後做什麼 | `object_fps_pvp` | `GYO::Ui`、`GYO::Input` |

依賴方向不變：`Application → Engine`、`Editor → Engine`。

**Architecture Delta**：`GYO::Ui` 多了「文字編輯」的責任（以前在產品）。照 AGENTS §3，加責任前先在 [ui_toolchain.md](../../../ui_toolchain.md) 寫 `Design Invariants` 與 `Replacement Criteria`（`v8-tool-ui-invariants`，要使用者同意）。

### 手動／Script 階段

- `gyo.ui` 的手寫流程已經有了（ui_editor 也有 `--validate` CLI）。新元素先用手寫的 v2 文件與 CLI 驗證，再加到 GUI。
- 輸入法：先檢測，不先修（記憶「Detection before evaluation」、AGENTS §14.3）。`v8-tool-ui-ime-observe` 把 `SDL_EVENT_TEXT_EDITING` 與 `SDL_EVENT_TEXT_INPUT` 的到達順序記到 log，在 Windows（主戰場）用日文／中文輸入法打「127.0.0.1:8080」，看「:」是不是停在組字中。

### 分批

| 代號 | 檔位｜引擎 | 估時 | 內容 | 驗收 |
|---|---|---|---|---|
| `v8-tool-ui-ime-observe` | medium｜Opus 5.5 | 2～4 h＋實機一次 | 診斷用 log（不改行為）：組字事件的開始、內容、確定。附離線場次腳本（記憶「Offline session scripts」） | 實機：重現或記錄「未重現」與證據。重現不了就停下回報（§14.3），不排修正。觀測方法與判定見下面「v8-tool-ui-ime-observe 的事前宣告」 |
| `v8-tool-ui-invariants` | medium｜Opus 5.5 | 2～3 h | 在 [ui_toolchain.md](../../../ui_toolchain.md) 寫 `GYO::Ui`、ui_editor 的 `Design Invariants`＋`Replacement Criteria`；能守的寫守衛 Test | 使用者同意節的內容 |
| `v8-engine-input-composition` | high｜Opus 5.5 | 6～10 h | `TextEvent` 加組字的種類（組字字串、游標位置）；`SetTextInputArea`。只加不改 | CI：既有 IP-1 Test 判定不變；新事件的單元 Test（合成的 SDL 事件）；突變只跑相關的（v7.1 D184） |
| `v8-tool-ui-text-field` | high（編輯狀態局部 xhigh）｜Opus 5.5 | 10～16 h | `gyo.ui` v2、`text_field` 的 codec／驗證／版面／編輯狀態／繪製 | CI：codec 往返、v1 文件照常載入、編輯狀態的單元 Test（含組字中按 Enter、Backspace、焦點離開）、突變 |
| `v8-tool-ui-editor-file-browser` | high｜Opus 5.5 | 3～5 h | SDL3 的系統選擇窗（`SDL_ShowOpenFolderDialog`、`SDL_ShowOpenFileDialog`、`SDL_ShowSaveFileDialog`）：選單「Open Asset Folder...」一步掛載（參考 AssetPack 的「データフォルダを開く」）；Open、Export、掛載對話框的欄位也用。回呼經執行緒安全的收件匣，在幀之間套用。不存狀態；跳不出窗時退回手打 | CI：收件匣跨執行緒、選資料夾後掛載、取消不掛載；L3：Windows 與 macOS 各選一次中文路徑 |
| `v8-tool-ui-editor-panels` | medium｜Opus 5.5 | 3～5 h | Hierarchy、Canvas、Inspector、對話框拆成各自的檔；`EditorApp` 只協調。純搬移，不改行為 | CI：既有測試不變；拆前拆後的行為由既有測試與手動開一次確認 |
| `v8-tool-ui-editor-element-schema` | high｜Opus 5.5 | 5～8 h | 每種元素的欄位與 Inspector 的編輯控制集中在一張表，取代散落約 15 處的字串比對（審查 S-2） | CI：每種元素的欄位表與 `GYO::Ui` codec 一致的測試；突變 |
| `v8-tool-ui-editor-clipboard` | high｜Opus 5.5 | 3～5 h | 元素的複製、貼上、就地複製；貼上時 id 自動改成不重複（沿用 canvas 複製的 `UniqueNodeCopyId`） | CI：複製子樹、id 不重複、undo 一步還原 |
| `v8-tool-ui-editor-nudge` | medium｜Opus 5.5 | 2～3 h | 方向鍵 1 px、Shift 10 px；縮放把手的 Alt（從中心）、Shift（等比例） | CI：`GizmoMath` 的單元測試 |
| `v8-tool-ui-editor-text-field` | high｜Opus 5.5 | 8～12 h | ui_editor 新增、編輯、預覽 `text_field`；輸出 v2 | CI：`tests/ui_editor` 的往返與 CLI 驗證；L3：使用者用 GUI 擺一個輸入框 |
| `v8-tool-ui-button-style`（待決） | medium｜Opus 5.5 | 4～6 h | 按鈕外觀的共用樣式（同一組顏色、字級只寫一次） | 看 `v8-tool-ui-widget-scope` |
| `v8-product-lobby-text-field` | high｜Opus 5.5 | 4～6 h | 大廳的位址欄改成 `text_field`；刪掉 `HandleTextEditing`、`AppendAddress` 的編輯部分，只留字元規則 | CI：大廳的既有 Test；L3：Windows 用輸入法打位址、看得到組字。**產品批**：由產品會話排程 |

### v8-tool-ui-ime-observe 的事前宣告

結果出來之前寫下，要使用者同意（AGENTS §14.1）。看了 log 之後不改。

- **工具**：
  - 客戶端參數 `--text-input-trace` 打開 `SdlInput::SetTextDiagnostics`，log 寫在執行檔旁的 `logs/client-<時間>.log`。
  - Windows 用 [scripts/ime_observe_windows.cmd](scripts/ime_observe_windows.cmd) 啟動（放在 `gyo_object_fps_pvp.exe` 旁邊，雙擊）。
  - 分析用 [scripts/ime_observe_report.py](scripts/ime_observe_report.py)，在 Mac 上跑。
- **場次**（Windows 11，主戰場；不需要 Gateway 與 Match，只用大廳的位址欄）：
  1. 對照組：輸入法關閉（英數），點位址欄、清空、打「127.0.0.1:8080」、Enter。1 次。
  2. 日文輸入法（Microsoft IME，平假名模式），同樣的操作，3 次。
  3. 中文輸入法（使用者平常用的那一個；記下名稱），同樣的操作，3 次。
  - 每次之間點一下別處，讓位址欄結束輸入，每一次會在 log 裡自成一個 trial。
- **判定**（分析腳本的分類，現在固定）：每個 trial 的冒號歸成 ascii／fullwidth／held／missing 之一，並記下是否「先組字才確定」與中間的按鍵數。
  - 對照組不是 ascii、或出現了組字：視為觀測工具有問題，停下回報，不判讀其他 trial。
  - 輸入法的 trial：fullwidth 或 held 或 missing，算「重現了冒號問題」，並以分類說明機制；全部是 ascii，記成「未重現」，附 log。
  - 不論結果，這一批不排修正；修正由 `v8-engine-input-composition`／`v8-tool-ui-text-field` 依結果規劃。
- **範圍的限制**：只看 Windows 上這兩種輸入法；鍵盤事件之外的原因（例如畫面停頓）不在觀測範圍。

## 待討論的兩個工具（`v8-tool-scope-ui-first`）

使用者（2026-10-30）：「我們這次只改UI Editor，voxel Map Editor我後續給妳方案。」之後更正：「我不是要把工具都暫停，而是 voxel assets 都需要討論，我會把方案一個個給你，現在先處理UI。」

- `v8-tool-voxel-map-editor`：沒有暫停，等使用者逐個給方案再一起討論；本線不先提案。上面「現況盤點」裡 arena、舊格子地圖、ImGui 後端的列只是事實，留給之後的規劃參考。
- `v8-tool-assets-editor`：同上，等使用者的方案。它也要和 `v8-product-hitbox-data` 定下的格式與 owner 對齊。
  - 已收到的需求：**音效匯入**（`v8-tool-assets-audio-import`，2026-10-30）。使用者在音效會話說：「這個你先轉好，需求加到 assets editor 中。」由音效線（`v8-engine-audio-plan`）轉來，這裡只記需求，不排實作。
    1. 匯入任意取樣率與位元深度的 WAV（8／16／24／32 位元整數、32 位元浮點；1 或 2 聲道），轉成 `GYO::Audio` 的片段格式：48 kHz、PCM16、原聲道數。理由：混音器執行中不重新取樣（[audio.md](../../../architecture/audio.md) Design Invariant 9），`SoundLoader` 只讀 16 位元。
    2. 轉換要可重現：同一輸入得到同樣的位元組。現行腳本用窗化 sinc 重新取樣（Blackman 窗、兩側各 32 個零交越、截止在較低 Nyquist 的 0.95），用固定種子的三角抖動量化成 16 位元，並報告飽和的樣本數。
    3. 原檔放在資產根目錄**以外**（位置由 assets editor 的規劃決定，例如另一個來源目錄，或不進 repo）；轉好的檔才放進資產根目錄並登錄在 catalog（type `sound`）。理由：產品的內容契約要求資產根目錄下每個檔都登錄在 catalog（`tests/object_fps_pvp/test_asset_catalog.py` 的 `test_every_asset_file_is_in_the_catalog`）。2026-10-30 更正：原本寫「放在 `<asset>/audio/source/`」，音效線照做後這個測試失敗，已移出。
    4. 記錄來源與授權（現在寫在音效線的 dev_log `docs/dev_logs/2026_10_30_pvp_v8_product_sfx.zh-Hant.md`，授權待使用者補）。
    - 現況：AGENTS §5 的 Script 階段，由 `docs/architecture/plans/audio/scripts/convert_wav.py` 手動轉換。腳本、產物與上面的 dev_log 在音效線的分支 `claude/v8-engine-audio-95a496`，尚未推送，之後隨音效的 PR 線合併；合併前本文件裡這些路徑還不存在。
  - 已收到的需求：**動畫與音效的同步**（`v8-tool-assets-anim-events`，2026-10-30）。使用者在音效會話說：「有一些違和感，這個聲音和動畫的管理是資料化的吧，是的話，就給Assets Editor來處理。」由音效線（`v8-engine-audio-plan`）轉來，這裡只記需求，不排實作。
    1. 在動畫片段上標記事件時刻（例如「插入彈倉」在第幾秒、哪一隻腳著地），可以在 Assets Editor 裡預覽與調整。
    2. 音效提示可以掛在這些標記上：執行時動畫到達該時刻就發聲（時刻換成 Engine 時間，交給 `GYO::Audio` 的時間戳排程）。
    3. 格式是 Data Contract（版本與驗證規則），Runtime 不依賴 Editor；owner 與格式位置由 assets editor 的規劃決定（和 `v8-product-hitbox-data` 一樣要對齊格式的 owner）。
    4. 完成後移除產品裡的過渡規則 `ReloadSoundOffsetSeconds`（`apps/object_fps_pvp/src/Pvp/SoundCues.cpp`）。
    - 現況：已資料化的是每種音效提示的片段、音量、空間化與步距（`assets/object_fps_pvp/audio/sfx.json`）；聲音對到動畫哪一刻沒有資料化。換彈最後一聲和插彈倉動畫對不上，目前在 C++ 用「聲音結尾對齊換彈結束」修正，使用者聽了仍覺得違和。動畫片段（例如 `nailer_v2_viewmodel.json`、角色的 animset）沒有事件時刻的標記。上面提到的 `sfx.json`、`SoundCues.cpp` 在音效線的分支 `claude/v8-engine-audio-95a496`，尚未推送。
- 2026-10-30 第一版裡兩個工具的建議（烘焙到 arena v1、`gyo.voxel_map`、ImGui 的 SDL_GPU 後端等）已撤下，見 commit `54dac0c3`。

## 驗收的共同規則

- 主要看 CI（記憶「CI is the primary standard」）；實機只做 CI 做不到的：輸入法（需要真的輸入法）、3D 視窗的操作手感。
- 新判定附相關的突變（v7.1 D184 的範圍）；PR 線合併前在背景完整跑一次。
- ui_editor 是 release 工具：每個 release 平台的 `quick`／`release` 驗收都要繼續通過（[tool_projects.md](../../../tool_projects.md) 第 5 步）。

## 要使用者決定的事

完整清單、選項與建議在 [HANDOFF](HANDOFF.md)「等你決定」。

## dev_log

| 批 | dev_log |
|---|---|
| `v8-tool-plan` | [2026_10_30_tool_v8_plan](../../../dev_logs/2026_10_30_tool_v8_plan.zh-Hant.md) |
| `v8-tool-ui-editor-file-browser` | [2026_10_30_tool_v8_ui_editor_file_browser](../../../dev_logs/2026_10_30_tool_v8_ui_editor_file_browser.zh-Hant.md) |
| `v8-tool-ui-editor-gizmo-math` | [2026_10_30_tool_v8_ui_editor_gizmo_math](../../../dev_logs/2026_10_30_tool_v8_ui_editor_gizmo_math.zh-Hant.md) |
| `v8-tool-ui-editor-gizmo-fix` | [2026_10_30_tool_v8_ui_editor_gizmo_fix](../../../dev_logs/2026_10_30_tool_v8_ui_editor_gizmo_fix.zh-Hant.md) |
| `v8-tool-ui-invariants` | [2026_10_30_tool_v8_ui_invariants](../../../dev_logs/2026_10_30_tool_v8_ui_invariants.zh-Hant.md) |
| `v8-tool-ui-editor-panels` | [2026_10_30_tool_v8_ui_editor_panels](../../../dev_logs/2026_10_30_tool_v8_ui_editor_panels.zh-Hant.md) |
| `v8-tool-ui-editor-element-schema` | [2026_10_30_tool_v8_ui_editor_element_schema](../../../dev_logs/2026_10_30_tool_v8_ui_editor_element_schema.zh-Hant.md) |
| `v8-tool-ui-editor-clipboard`、`v8-tool-ui-editor-nudge` | [2026_10_30_tool_v8_ui_editor_clipboard_nudge](../../../dev_logs/2026_10_30_tool_v8_ui_editor_clipboard_nudge.zh-Hant.md) |
