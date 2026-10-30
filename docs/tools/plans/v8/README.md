# v8 工具線計畫（④）

更新：2026-10-30（`v8-tool-plan`）。Owner：三個工具各自的 owner（見各節）；本文件是 v8 ④ 的規劃，產品的 v8 計畫在 [object_fps_pvp v8 README](../../../object_fps_pvp/plans/v8/README.md)。
**狀態：規劃完成，等使用者核准；沒有實作。** 記錄器是 [HANDOFF](HANDOFF.md)；ultracode 規劃批的 prompt 草稿在 [ultracode-prompts.md](ultracode-prompts.md)，啟動前先給使用者看。

## 定位

- v8 的順序是 ① 基礎功能 → ② 體驗優化 → ③ 手感時序 → ④ 工具 → ⑤ 流程項目（v8-product-order）。使用者稱 ④ 為「Engine 的穩定」。本文件只規劃 ④ 的三個工具；工具線的規劃和 ①～③ 並行（使用者要求），**實作何時開始由使用者決定**（`v8-tool-start-timing`）。
- 三個工具都照 AGENTS §8：`Editor → Data Contract → Application`。Application 只讀 Data Contract，不連結、不初始化、不知道 Editor。Contract 的資料用手寫、Script、別的工具都能產生。
- 三個工具都照 AGENTS §5「手動 → Script → Tool」：先把手動的步驟、輸入、輸出、失敗條件寫清楚，再做工具。
- 工具的登記照 [tool_projects.md](../../../tool_projects.md)：`tools/<id>/`、`project.json`、`engine/config/tools.csv` 的一列、`tests/<id>`、`build/acceptance/<id>/`。新增工具只改這些，不改共通 Build（ImGui 的 SDL_GPU 後端除外，見 `v8-tool-gpu-imgui-component`）。
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
| arena 與協議 | arena 內容有 digest（pv6 §2），Match 與 Client 比對。**`Arena` 新增成員或改正規化＝wire 變更**；只改內容不是 wire 變更（不一致時明確拒絕） | [protocol-v6](../../../object_fps_pvp/protocol-v6.zh-Hant.md) 第 51～52 行、§2 |
| 舊產品的格子地圖 | `object_fps` 的 `GridMap`／`GridMapLoader`（ASCII 文字地圖，`assets/object_fps/maps/*.txt`）、`MapGeometryGenerator`。產品停用 | `apps/object_fps/src/World/`、`apps/object_fps/src/Rendering/MapGeometryGenerator.cpp` |
| PvP 裡的 `GridMap.hpp` | 從舊產品複製的標頭，經 `RetroFPS/Data/GameData.hpp` 被武器標頭間接包含；PvP 沒有它的載入器，地圖用的是 `Arena` 的 AABB 牆 | `apps/object_fps_pvp/include/RetroFPS/World/GridMap.hpp`、`Gameplay/Weapon/WeaponState.hpp:3` |
| hitbox 的先例 | `object_fps_v2` 的 `hurt_regions`：每項 `id`、`start`／`end` 骨骼點（`node`＋可選 `offset`，模型座標）、`radius`、可選 `damage_multiplier` | [v2 enemies](../../../object_fps_v2/enemies.zh-Hant.md) 第 117 行附近 |
| ImGui 後端 | 共通 Build 的 `IMGUI` 元件只對應 `imgui::sdl3_sdlrenderer3`。3D 視窗要 SDL_GPU，需要新的 ImGui 後端元件 | `build/cmake/GyoAppProject.cmake:131`、`build/cmake/GyoComponents.cmake:3` |

## 樹狀總覽

格式：「檔位｜引擎｜簡述」。引擎照 [變速箱規則](../../../../agents/effort-gearbox.md)「エンジン（モデル）」：主會話的引擎與檔位由使用者決定，下面是建議。「Opus 5.5」＝目前主會話的引擎；「Sonnet 5.5」用在唯讀偵察與對抗式檢查。

```
v8-tool-plan ✓                              「high｜Opus 5.5｜本計畫、HANDOFF、ultracode prompt 草稿」
│
├── ui-widgets-line                          「PR 線：文字輸入框元件＋輸入法組字＋ui_editor」
│   ├── v8-tool-ui-ime-observe               「medium｜Opus 5.5｜先檢測：把 TEXT_EDITING／TEXT_INPUT 記到 log，Windows 重現「:」卡住」
│   ├── v8-tool-ui-invariants                「medium｜Opus 5.5｜加元件之前，寫 GYO::Ui 與 ui_editor 的 Design Invariants＋Replacement Criteria」
│   ├── v8-engine-input-composition          「high｜Opus 5.5｜Input frame 只加不改：組字事件、候選窗位置」
│   ├── v8-tool-ui-text-field                「high（編輯狀態局部 xhigh）｜Opus 5.5｜gyo.ui v2：text_field 元素、通用編輯狀態、組字顯示」
│   ├── v8-tool-ui-editor-text-field         「high｜Opus 5.5｜ui_editor 編輯、預覽、驗證 v2 文件」
│   ├── v8-tool-ui-button-style（待決）       「medium｜Opus 5.5｜按鈕外觀的共用樣式；看 v8-tool-ui-widget-scope」
│   └── v8-product-lobby-text-field          「high｜Opus 5.5｜大廳改用 text_field，刪掉手寫的編輯（產品批，產品會話做）」
│
├── voxel-line                               「PR 線：3D 格子地圖編輯器」
│   ├── v8-tool-voxel-map-plan               「ultracode（先問）｜Opus 5.5＋審查 Sonnet 5.5｜定契約與執行路徑」
│   ├── v8-tool-voxel-contract               「high｜Opus 5.5｜手動＋Script 階段：gyo.voxel_map v1、手寫樣本、烘焙腳本」
│   ├── v8-tool-gpu-imgui-component          「high｜Opus 5.5｜ImGui 的 SDL_GPU 後端元件（共通 Build，Architecture Delta）」
│   ├── v8-tool-voxel-editor-core            「high｜Opus 5.5｜文件 session、undo、格子操作、驗證 CLI（不需 GUI 就能測）」
│   ├── v8-tool-voxel-editor-view            「high｜Opus 5.5｜3D 視窗、相機、擺放／刪除、標記」
│   └── v8-tool-voxel-bake-check             「medium｜Opus 5.5｜CI 檢查：烘焙結果與來源一致」
│
└── assets-line                              「PR 線：資產編輯器（v8 只做 hitbox）」
    ├── v8-tool-assets-editor-plan           「ultracode（先問）｜Opus 5.5＋審查 Sonnet 5.5｜前提：v8-product-hitbox-data 完成」
    ├── v8-tool-assets-editor-core           「high｜Opus 5.5｜讀 hitbox 契約、session、undo、驗證 CLI」
    ├── v8-tool-assets-editor-view           「high｜Opus 5.5｜3D 視窗：蒙皮模型、骨骼、hitbox 形狀」
    └── v8-tool-assets-editor-hitbox         「high｜Opus 5.5｜挑骨骼、拉形狀、標部位；接手手寫 JSON」
```

## 三個工具的建議順序與依賴

```
ui-widgets-line ──（不依賴其他兩條，也不需要 SDL_GPU）
voxel-line ──── v8-tool-gpu-imgui-component ──→ assets-line 沿用
                                                  ↑
① hitbox-line 的 v8-product-hitbox-data（手寫 JSON 與格式）──┘
```

- **建議順序：ui-widgets-line → voxel-line → assets-line**（`v8-tool-order`）。
  - ui-widgets-line 最小、有使用者實際碰到的問題（「:」卡住、手感），不需要 3D。它的第一批只是檢測，可以先做。
  - voxel-line 不依賴 ①～③；它先做 ImGui 的 SDL_GPU 後端。
  - assets-line 要等 `v8-product-hitbox-data` 把 hitbox 的格式與 owner 定下來（hitbox-line 排在 pv8-line 之後），也沿用 voxel-line 的 SDL_GPU 後端。
- **3D 視窗的共用**：voxel 與 assets 兩個工具都要「ImGui＋SDL_GPU 視窗＋相機」。照 AGENTS §1（不以單一用途建通用抽象），第一個工具在自己的 owner 裡寫；第二個工具做的時候，如果真的出現重複，再提 Architecture Delta 把共用的部分抽出來（候選位置在規劃批決定）。ImGui 的後端元件是 Build 層的事，一開始就是共通的。
- **兩個工具之間不互相依賴**（[ui_toolchain.md](../../../ui_toolchain.md) 末段：地圖編輯器不得依賴 UI editor，也不把它變成 plugin host）。

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
| `v8-tool-ui-ime-observe` | medium｜Opus 5.5 | 2～4 h＋實機一次 | 診斷用 log（不改行為）：組字事件的開始、內容、確定。附離線場次腳本（記憶「Offline session scripts」） | 實機：重現或記錄「未重現」與證據。重現不了就停下回報（§14.3），不排修正 |
| `v8-tool-ui-invariants` | medium｜Opus 5.5 | 2～3 h | 在 [ui_toolchain.md](../../../ui_toolchain.md) 寫 `GYO::Ui`、ui_editor 的 `Design Invariants`＋`Replacement Criteria`；能守的寫守衛 Test | 使用者同意節的內容 |
| `v8-engine-input-composition` | high｜Opus 5.5 | 6～10 h | `TextEvent` 加組字的種類（組字字串、游標位置）；`SetTextInputArea`。只加不改 | CI：既有 IP-1 Test 判定不變；新事件的單元 Test（合成的 SDL 事件）；突變只跑相關的（v7.1 D184） |
| `v8-tool-ui-text-field` | high（編輯狀態局部 xhigh）｜Opus 5.5 | 10～16 h | `gyo.ui` v2、`text_field` 的 codec／驗證／版面／編輯狀態／繪製 | CI：codec 往返、v1 文件照常載入、編輯狀態的單元 Test（含組字中按 Enter、Backspace、焦點離開）、突變 |
| `v8-tool-ui-editor-text-field` | high｜Opus 5.5 | 8～12 h | ui_editor 新增、編輯、預覽 `text_field`；輸出 v2 | CI：`tests/ui_editor` 的往返與 CLI 驗證；L3：使用者用 GUI 擺一個輸入框 |
| `v8-tool-ui-button-style`（待決） | medium｜Opus 5.5 | 4～6 h | 按鈕外觀的共用樣式（同一組顏色、字級只寫一次） | 看 `v8-tool-ui-widget-scope` |
| `v8-product-lobby-text-field` | high｜Opus 5.5 | 4～6 h | 大廳的位址欄改成 `text_field`；刪掉 `HandleTextEditing`、`AppendAddress` 的編輯部分，只留字元規則 | CI：大廳的既有 Test；L3：Windows 用輸入法打位址、看得到組字。**產品批**：由產品會話排程 |

## 工具 2：`v8-tool-voxel-map-editor`（voxel-line）

### 使用者需求

- 地圖現在是手寫 JSON 的 AABB 牆清單（v8 README「定位」）。要有一個 3D 格子的地圖編輯器（README「未決事項」b：類似 Godot GridMap，在 3D 格子上擺東西）。
- ⑤ 的「正式的 4 人地圖：出生點分散、出生保護」也會用到它。

### Data Contract（建議，規劃批用 ultracode 定稿）

兩層，各有 owner：

```
voxel editor ──→ gyo.voxel_map v1（工具的原始檔，通用）
                       │
     產品的烘焙 Script（object_fps_pvp）
                       ↓
                 arena v1（既有的產品契約：AABB 牆＋出生點）──→ Client／Match
```

- `gyo.voxel_map` v1（通用，不帶產品知識）：格子大小、範圍、每格的方塊種類（v8 只有「實心／空」或少數幾種方塊）、**標記**（marker：格子座標＋型別字串＋朝向）。標記的型別字串由產品解讀（例如出生點），工具只顯示。
- **烘焙**：產品自己的 Script 把 `gyo.voxel_map` 轉成 arena v1：相鄰的實心格合併成少量的 AABB（避免 README「預計壓力」1、2 的牆數暴增），把產品認得的標記轉成出生點。
- **建議走烘焙，不讓執行端直接讀格子**（`v8-tool-voxel-runtime-path`）：
  - 好處：arena v1 不變，**不是 wire 變更**（pv6 §2：`Arena` 新增成員才算）；Client／Match 不改；碰撞與繪製的壓力不因格子而上升。
  - 代價：一個事實有兩個檔案（原始檔與烘焙結果）。烘焙結果是產生的，不是手動維護的；用 CI 檢查「烘焙結果＝烘焙（原始檔）」（`v8-tool-voxel-bake-check`），維持 AGENTS §9「同一個事實只在一處手動維護」。
  - 另一條路（執行端讀格子＝arena v2）：要新的 pvN、改碰撞與繪製（空間索引、區塊網格）。等真的需要網格模型或大地圖時再開。
- 「網格模型（mesh library）」還是「只有方塊」：建議 v8 只有方塊＋標記（`v8-tool-voxel-cell-model`）。網格模型需要 arena v2 與繪製改動。

### owner 與依賴方向

| 部分 | owner | 依賴 |
|---|---|---|
| `gyo.voxel_map` 的格式文件、讀寫、驗證、編輯器 | 新工具 `tools/voxel_map_editor`（id 待定），文件在 `docs/voxel_map_editor/` | 公開的 Engine API；**不連結遊戲**（`requires_apps: {}`） |
| 烘焙 Script、標記型別的意思 | `object_fps_pvp` | 讀 `gyo.voxel_map`（JSON）、寫 arena v1 |
| ImGui 的 SDL_GPU 後端 | 共通 Build（`build/cmake/`） | Architecture Delta（`v8-tool-gpu-imgui-component`） |
| 格子空間索引、區塊網格 | 現在不做 | 只有在執行端讀格子時才需要（README 未決事項 c） |

- Engine **不新增**模組：執行端不讀 `gyo.voxel_map`，所以讀寫只在工具與產品 Script（`v8-tool-voxel-contract-owner`）。等執行端要讀時，再提 Architecture Delta 把 codec 搬到 Engine（和 `GYO::Ui` 一樣）。
- 產品刪除後：工具與 `gyo.voxel_map` 照常成立；產品的烘焙 Script 跟著產品一起刪（AGENTS §7）。

### 手動／Script 階段

`v8-tool-voxel-contract`：先寫格式文件、手寫一份格子樣本（把 `pvp_arena.json` 用格子重畫）、寫烘焙 Script，產生的 arena 與現在的 `pvp_arena.json` 比對（digest 相同或差異能解釋）。這一步做完，工具只是「更方便地產生同一份原始檔」。

### 分批

| 代號 | 檔位｜引擎 | 估時 | 內容 | 驗收 |
|---|---|---|---|---|
| `v8-tool-voxel-map-plan` | ultracode（先問）｜Opus 5.5，審查 Sonnet 5.5 | 3～5 h | 約 3 個案、審查 1 人、對抗式檢查 1 次。定：格式、烘焙、合併演算法、工具 id、3D 視窗、版本規則 | 使用者核准計畫 |
| `v8-tool-voxel-contract` | high｜Opus 5.5 | 8～12 h | 格式文件、樣本、產品的烘焙 Script（Python，跨平台） | CI：烘焙樣本得到合法 arena（`Arena::Validate`）；合併後的牆數；與 `pvp_arena.json` 的對照 |
| `v8-tool-gpu-imgui-component` | high｜Opus 5.5 | 6～10 h | `IMGUI` 之外加 SDL_GPU 版的元件（名稱規劃時定），部署規則 | CI：三平台 configure／build；ui_editor 不受影響 |
| `v8-tool-voxel-editor-core` | high｜Opus 5.5 | 10～14 h | 文件 session、undo、放／刪格子、標記、`--validate` CLI | CI：`tests/<id>` 的往返、undo、驗證錯誤 |
| `v8-tool-voxel-editor-view` | high｜Opus 5.5 | 16～24 h | 3D 視窗、相機、滑鼠挑格子、擺放 | L3：使用者畫一張小地圖、烘焙、進遊戲走一圈 |
| `v8-tool-voxel-bake-check` | medium｜Opus 5.5 | 3～5 h | CI 檢查烘焙結果和原始檔一致（產品的 Test） | CI：故意改烘焙結果時失敗 |

## 工具 3：`v8-tool-assets-editor`（assets-line）

### 使用者需求

- 資產編輯，包括 hitbox 的形狀與部位標記（v8 README 候選表「PvP 骨骼 hitbox」）。
- ① 的 `v8-product-hitbox-data` 先手寫 JSON（骨骼→形狀＋部位標記是通用格式，部位倍率等語意是產品資料）。Assets Editor 之後接手編輯這份 JSON。

### Data Contract

- 契約是 `v8-product-hitbox-data` 定下來的 hitbox 格式（**格式與 owner 由那一批決定**，本計畫不先定）。先例是 v2 的 `hurt_regions`（骨骼點 `node`＋`offset`、`radius`）。
- 編輯器只寫 hitbox 檔；模型、character 定義、資產目錄**唯讀**掛載（沿用 ui_editor 的做法：`--asset-root`、不寫 catalog）。
- 部位語意（倍率）是產品資料，編輯器不寫，或只當不透明的標籤顯示（看 hitbox-data 的切法）。
- Application 讀 hitbox 檔，不需要編輯器；手寫照常可用。

### owner 與依賴方向

| 部分 | owner | 依賴 |
|---|---|---|
| hitbox 的通用格式與載入 | `v8-product-hitbox-data` 決定（候選：Engine 的 `GYO::Model` 旁，或產品） | — |
| 編輯器 | 新工具 `tools/assets_editor`（id 待定） | `GYO::Model`、`GYO::AssetUfbx`、`GYO::ModelRenderer`、SDL_GPU；**不連結遊戲** |
| 部位語意 | `object_fps_pvp` | — |

如果 hitbox 格式最後放在產品裡，通用的編輯器就不能讀它（工具不得依賴產品）。這時要嘛把通用部分升到 Engine，要嘛把編輯器做成產品專用工具（`requires_apps`，像 `object_fps_preview`）。這是 `v8-tool-assets-format-owner`，要和 hitbox-line 的產品會話雙向對齊。

### 手動／Script 階段

手動階段由 ① 完成（手寫 JSON）。工具線先做驗證 CLI（`--validate`：骨骼名存在、半徑有限且為正、id 不重複），再做 GUI。

### 分批

| 代號 | 檔位｜引擎 | 估時 | 內容 | 驗收 |
|---|---|---|---|---|
| `v8-tool-assets-editor-plan` | ultracode（先問）｜Opus 5.5，審查 Sonnet 5.5 | 3～5 h | 前提：hitbox-data 完成。定：範圍、格式 owner 的對齊、視窗的共用、挑骨骼的方法 | 使用者核准計畫 |
| `v8-tool-assets-editor-core` | high｜Opus 5.5 | 8～12 h | 掛載資產、讀寫 hitbox 檔、session、undo、`--validate` | CI：讀入 ① 手寫的 JSON、原樣寫回（canonical）、驗證錯誤 |
| `v8-tool-assets-editor-view` | high｜Opus 5.5 | 16～24 h | 3D 視窗畫蒙皮模型、骨骼、形狀；選姿勢 | CI：無視窗的部分；L3：看得到角色與現有 hitbox |
| `v8-tool-assets-editor-hitbox` | high｜Opus 5.5 | 10～16 h | 挑骨骼、拉起點／終點／半徑、標部位 | L3：改一個部位、存檔、Match 的命中跟著改 |

## 驗收的共同規則

- 主要看 CI（記憶「CI is the primary standard」）；實機只做 CI 做不到的：輸入法（需要真的輸入法）、3D 視窗的操作手感。
- 新判定附相關的突變（v7.1 D184 的範圍）；PR 線合併前在背景完整跑一次。
- 新工具一開始是本機工具（`release=0`、`packageable=false`），穩定後再決定要不要進 release（`v8-tool-release`）。進 release 要每個平台都有 `quick`／`release` 的驗收。

## 要使用者決定的事

完整清單、選項與建議在 [HANDOFF](HANDOFF.md)「等你決定」。

## dev_log

| 批 | dev_log |
|---|---|
| `v8-tool-plan` | [2026_10_30_tool_v8_plan](../../../dev_logs/2026_10_30_tool_v8_plan.zh-Hant.md) |
