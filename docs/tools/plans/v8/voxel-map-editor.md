# v8 工具：voxel 地圖編輯器（`v8-tool-voxel-map-editor`）

更新：2026-11-05（`v8-tool-voxel-map-plan`）。Owner：新工具 `voxel_map_editor`（編輯器、格子格式、build）；Engine（建置產物的格式與讀取器）；framework（3D 預覽元件，owner 是架構會話）；`object_fps_pvp`（arena v2、step-up）。記錄器是 [voxel-map-editor-HANDOFF.md](voxel-map-editor-HANDOFF.md)（2026-11-05 以前在工具線的 [HANDOFF](HANDOFF.md)），工具線的總覽在 [README](README.md)。

**狀態：方案與分批已由使用者確認（2026-11-05）。還沒有實作；每一批等使用者說「開始」，開始前寫事前宣告。**

## 由來

- 照 `v8-tool-scope-ui-first`，地圖編輯器的方案由使用者給，工具線不先提案。
- 使用者給的材料：自己寫的報告《用方塊製作遊戲地圖：Voxel 技術說明》（PDF 與文字版在主 checkout 的 `build/evidence/tool-v8-voxel-map/plan/`）。
- 使用者的方向（2026-11-05）：「格子是用來做地圖編輯的。build之後成為mesh。也就是我文章裡面提到的技術。goxel、MagicaVoxel，是製作工具的UI的交互邏輯。GridMap則可以成為存儲格式，voxel則是 editor格式。」
- 做法：
  - 主會話讀了三個參考專案的原始碼或規格：Goxel（GPL-3.0，只參考設計）、MagicaVoxel 的 `.vox`、Godot 的 GridMap（MIT）。
  - 經兩輪 Fable／high 的統合（唯讀的 Plan agent，只有統合者一人，**沒有**敵對式檢查者，所以不算完整的 ultracode）。
  - 再和使用者逐點定案。
  - 討論經過與兩輪統合的原文在 `build/evidence/tool-v8-voxel-map/plan/plan-discussion.md`。

## 參考專案：借了什麼

| 參考 | 借用 | 不借 |
|---|---|---|
| Godot GridMap（`modules/gridmap/`，master `bf194eb5`） | 儲存結構（整數座標→格子內容、24 種直角朝向）；編輯器的操作（編輯平面、樓層、塗、擦、吸取、框選、貼上、游標旋轉）；編輯器只經公開 API 改資料 | 每格擺預製網格＋MultiMesh 的繪製（GYO 的 Render 沒有 instancing）；宣告了卻沒用到的 `layer` 欄位 |
| Goxel（`src/volume*.c`、`src/tools/`） | 稀疏的 16³ 區塊；頂點 UV 指向調色盤貼圖；筆刷類工具的手感 | 每個 voxel 存 RGBA（地圖需要方塊種類）；程式碼（GPL-3.0） |
| MagicaVoxel `.vox` | 「索引 → 定義」的調色盤 | 單一模型 256³ 的上限、z 朝上 |
| Minecraft 的 block model | 形狀＝一組 1/16 單位的盒子；只剔除蓋滿整個格面的面（cullface）；用半磚和朝向做樓梯 | — |

## Feature → Contract → Architecture（AGENTS §2）

**Feature（玩家和編輯者看到什麼）**

- 用方塊編輯地圖：有二樓、有樓梯、二樓可以出生；方塊有形狀和朝向，方便做地形和裝飾。
- 遊戲載入編輯好的地圖，碰撞、射線、出生判定和畫面都一致。
- 出問題時，可以從檔案反查。

**Contract（約定什麼）**

| 契約 | owner | 讀者 | 內容 |
|---|---|---|---|
| 格子原始檔 `gyo.voxel_grid` | 工具 `voxel_map_editor` | 只有編輯器（和它的 CLI） | JSON，依 16³ 區塊分段，區塊內 RLE；形狀庫（每種形狀是一組 1/16 單位的盒子）；方塊＝{名稱, 形狀, 顏色}；每格 [方塊索引, 朝向（24 種）]；`origin`（y 固定 0）、`cell_size` |
| 建置產物（名稱在 `v8-engine-voxel-mesh-contract` 定） | Engine | Client（繪製）、Match 和 Client（碰撞），工具（寫出後讀回驗證） | JSON，全部是整數（1/16 格）：`schema`、`version`、`source_sha256`、`build_version`、`origin`、`cell_size`、調色盤、四邊形清單 `[x,y,z,方向,寬,高,調色盤]`、`colliders`（AABB）。讀取器把四邊形展開成三角形，展開規則屬於契約 |
| arena v2 | `object_fps_pvp`（之後歸場景編輯器） | Match、Client | 檔案的 schema 是 v2：用相鄰檔名引用產物，取代 `walls`；出生點、`width`／`depth`、移動參數照舊手寫。記憶體裡的 `Arena.version` 維持 1 |

**Architecture（結構怎麼變）**

```text
[工具] voxel_map_editor
  ├─ 讀寫 gyo.voxel_grid（原始檔放 build/art-source/，git 不追蹤）
  ├─ build：形狀展開與旋轉 → 面剔除 → 6 方向亮度的調色盤 UV → 合併碰撞 AABB
  │    ├─▶ 記憶體裡的結果 ─▶ [framework] 3D 預覽元件（畫面外繪製、讀回、ImGui 貼圖、OrbitCamera）
  │    └─▶ 存檔時一定同時寫出產物（附 source_sha256、build_version）─▶ assets/<產品>/…（catalog 登錄）
  └─ 拾取：編輯平面求交＋對格子的 DDA

[Engine] 產物的讀取器（唯一的驗證者）
  ├─▶ Client：四邊形 → MeshData → CreateMesh、MeshSubmission
  └─▶ Match／Client：colliders → Arena::walls（既有的碰撞、ShotQuery、出生判定都不改）
```

- 依賴方向：工具 → framework → Engine；產品 → Engine。Engine 不知道工具和產品，Runtime 不依賴編輯器（AGENTS §6、§8）。
- 分層的依據：`docs/architecture.md`「Layers: admission and format ownership」與 framework 的 Design Invariants 3、5。

## 已定案的決定

代號照 `v8-tool-voxel-…`；使用者原話在 [voxel-map-editor-HANDOFF.md](voxel-map-editor-HANDOFF.md) 的決定紀錄。

| 代號 | 決定 |
|---|---|
| `v8-tool-voxel-contract-split` | 契約拆兩層：格子原始檔（工具持有）＋建置產物（Engine 持有）。之後另外做場景編輯器，接手 arena v2 和玩法標記 |
| `v8-tool-voxel-offline-build` | 離線 build，產物存檔，遊戲只讀產物；碰撞也烘進產物；產物記原始檔的 SHA-256 和 build 版本 |
| `v8-tool-voxel-layers` | 產物格式與讀取器＝Engine；格子格式、build、拾取＝工具；3D 預覽元件＝framework；catalog 登錄沿用 framework 的 `CatalogPlan`／`RegisterAsset`；arena v2＝產品 |
| `v8-tool-voxel-units` | 整數，1/16 格的子單位。讀取器換算世界座標 `(index+origin)×cell_size/16` 和 UV `(c+0.5)/s`，每一步只捨入一次，各平台結果相同 |
| `v8-tool-voxel-artifact-encoding` | 產物用 JSON 四邊形清單（見上表）；不依區塊切、不附空間索引，兩者寫進 Replacement Criteria |
| `v8-tool-voxel-render` | 面剔除＋6 個方向的亮度（Render 只有 unlit、沒有 nearest 取樣，所以 UV 取格子中心；Render 沒有 mipmap）。繪製的 greedy、AO 是候選 |
| `v8-tool-voxel-arena-v2` | 只有檔案的 schema 是 v2，`Arena.version` 維持 1；產物的碰撞填進 `Arena::walls`。wire、digest 演算法、協議文件都不變（pv6 §2：arena 內容的修改不是 wire 變更）。`walls≤1024` 不放寬，build 超過就失敗 |
| `v8-tool-voxel-grid-format` | 格子檔：JSON、依 16³ 區塊分段、區塊內 RLE；形狀用資料定義；24 種直角朝向 |
| `v8-tool-voxel-coords` | origin 的 y 固定為 0，地板維持隱含的 y=0 平面；`width`／`depth` 在 arena v2 手寫（使用者無偏好，採建議） |
| `v8-tool-voxel-spawn` | 出生點手寫在 arena v2，編輯器不知道它們；`Arena::Validate` 的 `y==0` 改成「腳下要有支撐（地板或 AABB 頂面）」，這是 arena 契約變更，wire 不變 |
| `v8-tool-voxel-stairs` | B1：用形狀和朝向做出階梯，碰撞仍然全是 AABB；step-up 是產品批，由產品會話做。B2 斜坡另立 Feature，本線只把形狀做成可以擴充的資料 |
| `v8-tool-voxel-preview-framework` | PR #146 合併後，抽出 framework 的 3D 預覽元件成一批，model_viewer 同一批改用；和架構會話協調 |
| `v8-tool-voxel-first-tools` | 第一版：編輯平面與樓層、塗、擦、吸取、框選填滿／清空、選方塊與朝向、調色盤與形狀庫的編輯、undo；擠出、圖層、形狀筆刷延後；P1 先重新宣告再量 |
| `v8-tool-voxel-source-location` | 原始檔放 `build/art-source/`，位置由編輯器決定；編輯器存檔時一定同時寫出產物；**CI 不重新 build**，只驗證產物本身；本機可以用 CLI 的 `verify` 比對原始檔和產物 |
| `v8-tool-voxel-retire-v1` | v1 退役的證據：先比對合併後的 AABB 集合是否等於 v1 的 `walls`（不相等就先調整合併的掃描順序），再用固定命令序列比對移動軌跡，最後 L3 進遊戲走一圈 |

## 分批

- 檔位照[變速箱規則](../../../../agents/effort-gearbox.md)，模型照[領航員規則](../../../../agents/navigator.md)，格式是「檔位｜模型」。
- 估時是第一版，誤差可能到 ±50%。每批開始前重估；超過估時 1.5 倍，或單一問題累計超過 2 小時，就停下回報。

```
v8-tool-voxel-map-editor
├── v8-tool-voxel-map-plan ✓              「medium｜Opus｜本文件、README、HANDOFF、dev_log」
├── v8-engine-voxel-mesh-contract         「high｜Opus｜Engine：產物的格式、讀取器、驗證規則、Design Invariants ✓（L1 三平台通過）」
├── v8-framework-preview-component        「high｜Opus｜framework：抽出 3D 預覽元件，model_viewer 改用（等 #146）」
├── v8-tool-voxel-map-editor-core         「high｜Opus｜工具：登錄、格子格式、形狀庫、build、CLI ✓（L1 三平台通過）」
├── v8-tool-voxel-map-editor-window       「high｜Opus｜工具：ImGui 視窗、編輯平面、工具、朝向、undo、預覽」
├── v8-product-arena-v2                   「high｜Opus｜產品批：arena v2、出生點支撐規則、兩張地圖轉換、v1 退役、搬進 maps/ ✓」
├── v8-product-step-up                    「high｜Opus｜產品批：走上半格台階（產品會話做）」
└── v8-tool-voxel-map-editor-trial        「medium｜Opus｜P1 量測、L3 試用」
```

| 代號 | 段 | 檔位｜模型（實作／審查／驗證） | 估時 | 內容 | 驗收 | 依賴 |
|---|---|---|---|---|---|---|
| `v8-tool-voxel-map-plan` ✓ | 下段（只有文件） | medium｜Opus／—／— | 2～3 h | 本文件、README、HANDOFF、dev_log；參考資料存證 | 使用者確認 | — |
| `v8-engine-voxel-mesh-contract` | 上段（新 Subsystem、Data Contract） | high｜Opus／Opus／Opus | 8～12 h | Engine 新模組：產物的解析、驗證、四邊形展開、1/16 換算；只依賴 Base 和 Math，讓 Match 不帶 Render 也能連結；給 Render 的是 `MeshView`／`ImageView`。寫 `docs/architecture/<模組>.md` 的 Design Invariants 和 Replacement Criteria（比照 [animation-events.md](../../../architecture/animation-events.md)） | CI：每條規則一組測試＋相關突變；展開的黃金值；換算的跨平台一致性 | — |
| `v8-framework-preview-component` | 上段（framework 公開介面、DI） | high｜Opus／Opus／Opus | 6～10 h | 從 model_viewer 的 `PreviewRenderer` 抽出中立的部分（畫面外裝置、`Renderer`、讀回、上傳、`OrbitCamera`）；由客戶端建立（framework 的 Replacement Criteria：外殼不替客戶端建裝置）；model_viewer 同一批改用 | CI：framework 的測試與守衛；model_viewer 的既有測試不變；model_viewer 的 P1 重跑 | PR #146 合併；架構會話同意 |
| `v8-tool-voxel-map-editor-core` | 上段（產物寫出端，牽動 Engine 契約） | high｜Opus／Opus／Opus | 12～18 h | 登錄 `tools.csv`（`release=false`）；`gyo.voxel_grid` 的讀寫；形狀庫；build（形狀旋轉、cullface、方向亮度、AABB 合併、固定掃描順序）；CLI `build`／`verify`／`inspect`／`convert-arena-v1`；寫出後經 Engine 讀回；寫 `docs/voxel_map_editor/architecture.md` 的 DI（含「格子出現第二個讀者時升到 Engine」） | CI：build 的黃金值（1 格立方體 6 個面、內部面 0、半磚和階梯的 AABB）；同輸入同位元組；往返不失真；相關突變 | engine 批 |
| `v8-tool-voxel-map-editor-window` | 上段（使用者操作的手感） | high｜Opus／Opus／Opus | 12～20 h | ImGui 視窗；編輯平面與樓層、DDA 拾取；`v8-tool-voxel-first-tools` 的工具；undo（每次拖曳一筆，記格座標與新舊值）；預覽用 framework 元件 | CI：編輯模型與 undo 的單元測試；P1 事前宣告 | framework 批、core 批 |
| `v8-product-arena-v2` | 上段（Data Contract、Match） | high｜Opus／Opus／Opus | 6～10 h | arena v2 的載入（`Arena.version` 維持 1）；出生點的支撐規則；兩張現行地圖轉成格子並 build；產物登錄 catalog、列入 `RETAINED`；改 [assets.zh-Hant.md](../../../object_fps_pvp/assets.zh-Hant.md)。**產品批**，由誰做在開始前和產品會話定 | CI：`v8-tool-voxel-retire-v1` 的集合與軌跡等價；digest 的既有測試；資產契約測試 | core 批 |
| `v8-product-step-up` | 上段（移動、預測） | high｜Opus／Opus／Opus | 產品會話估 | 角色走上半格高的台階；Match 和 Client 的預測同一份規則。**產品批**，由產品會話做 | 產品會話宣告 | arena-v2 批（要有階梯地圖） |
| `v8-tool-voxel-map-editor-trial` | 下段 | medium｜Opus／—／— | 3～5 h＋L3 | P1 量測；L3：使用者用 GUI 建一張有二樓和階梯的地圖，進遊戲走一圈 | 照事前宣告 | 全部 |

- 總估時（不含 step-up）約 49～78 h。
- 順序：contract → core →（framework）→ window → arena-v2 → trial。framework 那批可以在 #146 合併後和 core 並行。
- 每批都屬於 Architecture Delta：開始前讀該 owner 的 Design Invariants，事前宣告寫明 Delta，並取得使用者同意（AGENTS §3）。Render 和 Collision 沒有 Design Invariants 文件；本線不改它們的介面，如果之後要改，得先補文件。

## v8-engine-voxel-mesh-contract 的事前宣告

2026-11-05 寫，**要使用者核准後才實作**。結果出來之後，判定不放寬（AGENTS §14.1）。

- **分支**：沿用本線的 PR #152（`claude/v8-tool-voxel-map-plan`），一條功能線一個 PR。
- **重估**：8～12 h，和計畫相同。
- **段**：上段（新 Subsystem、Data Contract）。
- **實作**：主會話，Opus 5.5／high。
- **審查**：子 Agent，Opus 5.5／high（審查者）。
- **驗證者**：這批沒有放寬既有判定（§14.4），所以不需要。

### 範圍

- **做**：
  - Engine 新模組：產物的解析、驗證、四邊形展開、1/16 換算、調色盤貼圖、碰撞 AABB；
  - Design Invariants 文件、規則測試、相關突變。
- **不做**：
  - 工具的寫出端（core 批）；
  - 產品的載入與 catalog（arena-v2 批）；
  - 繪製與碰撞的接線。

### Architecture Delta（AGENTS §3）

1. **為什麼需要**：`v8-tool-voxel-offline-build` 定了「遊戲只讀產物」，`v8-tool-voxel-layers` 定了產物格式歸 Engine。Client、Match 和工具（寫出後讀回）都要用同一個驗證者。
2. **現況的問題**：Engine 沒有任何「靜態地圖幾何」的資產格式。arena v1 的 `walls` 是產品格式，沒有繪製資料。
3. **變化的邊界**：Engine 多一個資產格式和它的讀取器。
4. **影響範圍**：Engine（新模組）。之後的工具和產品會依賴它，但這一批不改它們。
5. **依賴方向**：新模組 → `GYO::Base`、`GYO::Math`（PUBLIC）、nlohmann（PRIVATE）。不依賴 Render、Collision、Asset、產品。`Engine::Math::Aabb` 在 Math（`engine/math/include/engine/math/geometry/Aabb.hpp:11`），所以 Match 不帶 Render 也能連結。
6. **Ownership**：格式和驗證歸 Engine，寫出和 build 的決定（剔除、合併、亮度）歸工具。
7. **為什麼沒有更小的做法**：如果放進產品，工具就得持有一份規則的副本（違反 `v8-tool-voxel-layers`）；如果沿用 FBX，repo 裡沒有寫出器（`UfbxModelLoader.cpp:228`）。

### 名稱（請一併核准）

- 格式 `gyo.voxel_mesh`。
- 模組 `engine/voxel_mesh/`，target `GYO::VoxelMesh`，標頭 `engine/voxel_mesh/VoxelMesh.hpp`，命名空間 `Engine::VoxelMesh`。
- 文件 `docs/architecture/voxel-mesh.md`。
- catalog type 由產品批登錄，這一批不定。

### 格式 version 1（草案；實作時如有修改，會在審查前回報）

```json
{
  "schema": "gyo.voxel_mesh",
  "version": 1,
  "source_sha256": "<64 個小寫十六進位字元>",
  "build_version": "voxel_map_editor 1",
  "origin": [-1, 0, -1],
  "cell_size": 1.0,
  "palette": [
    { "name": "wall", "faces": [[r,g,b,a], …6 組，順序 +X −X +Y −Y +Z −Z] }
  ],
  "quads": [[x, y, z, dir, w, h, palette], …],
  "colliders": [{ "min": [x, y, z], "max": [x, y, z] }, …]
}
```

- **座標**：`quads` 和 `colliders` 用整數，單位是 1/16 格，相對於 `origin`。`origin` 以格為單位。
- **世界座標**：`float(q + 16×origin) × cell_size / 16`。整數相加是精確的；轉成 float 時，絕對值 ≤ 2^20 也是精確的；乘法捨入一次；除以 16 是精確的（2 的冪）。所以各平台結果逐位元相同。
- **`dir`**：0～5，依序是 +X −X +Y −Y +Z −Z。`(x, y, z)` 是面的最小角。
- **`w`／`h`**：面的兩個邊長，沿面內的兩軸，依序是 X 面 (Z, Y)、Y 面 (X, Z)、Z 面 (X, Y)。
- **面的位置**：正向的面（+X 等）位在 `x` 這個平面；負向的面也位在 `x`，法線朝外。
- **調色盤的亮度**：每個條目有 6 個面方向的顏色（sRGB 8 bit）。亮度由工具決定，Engine 不帶美術判斷。

### 驗證規則（每條在測試裡各自一組，任何違反都是 Runtime Error，細節是出錯值的路徑）

1. 文件是一個 JSON 物件，欄位剛好是上面 9 個；任何物件的 key 都不能重複。
2. `schema` 是 `"gyo.voxel_mesh"`，`version` 是整數 1。
3. `source_sha256` 是 64 個小寫十六進位字元；`build_version` 是 1～64 字元的可印 ASCII。
4. `origin` 是 3 個整數；`cell_size` 是有限的正數。
5. `palette` 有 1～256 個條目；`name` 是 1～64 字元的 `a`–`z`、`0`–`9`、`_`、`.`，且不重複；`faces` 剛好 6 組，每組 4 個 0～255 的整數。
6. `quads` 至少一筆。每筆剛好 7 個整數：`dir` 0～5，`w`、`h` ≥ 1，`palette` 小於條目數。
7. `colliders` 可以是空的。每筆剛好有 `min`、`max`，各 3 個整數，每軸 `min < max`。
8. 範圍：每個頂點與角加上 `16×origin` 之後，絕對值 ≤ 2^20。
9. 正規形：`quads` 依 7 個整數的字典序嚴格遞增，所以不會重複；`colliders` 依 (min, max) 的字典序嚴格遞增。
10. 文字大小 ≤ 16 MiB（解析前檢查）。

### 介面（草案）

- `ParseVoxelMesh(std::string_view text)` → `Base::Result<VoxelMeshDocument, VoxelMeshError>`：整數形式，規則 1～10。
- `ExpandVoxelMesh(const VoxelMeshDocument&)` → `VoxelMeshGeometry`：不會失敗，因為輸入已經驗證過。
  - `vertices`（`Math::Vec3 position`、`Math::Vec2 uv`）、`indices`（`uint32`）：每個面固定 4 個頂點、6 個索引，順序依 `quads`。
  - 從外側看是順時針（`RenderDeviceTypes.hpp:28-29`：GYO 的正面是順時針）。
  - `paletteImage`：寬＝條目數，高＝6（列＝`dir`），RGBA8，sRGB。UV 取該格的中心 `((p+0.5)/W, (dir+0.5)/6)`（Render 只有 Linear 取樣，`RenderTypes.hpp:84-87`）。
  - `colliders`：`std::vector<Math::Aabb>`。
- 不提供 `MeshView`／`ImageView`：Engine 的格式不依賴 Render，由使用端轉換。

### Design Invariants（草案，寫進 `docs/architecture/voxel-mesh.md`）

1. **中立**：格式和模組只知道面、顏色、盒子和不透明的名稱。不知道方塊、形狀、朝向、出生點、產品。連結只有 Base、Math 和 PRIVATE 的 JSON。守衛：configure 時的連結檢查（比照 `gyo_expect_model_links`）。
2. **不做 build**：Engine 只展開、只換算，不剔除、不合併、不決定亮度。這些都是工具的事。守衛：審查；介面上只有 Parse 和 Expand。
3. **一個驗證者**：檔案是否合法，只由 `ParseVoxelMesh` 決定，和檔案是誰寫的無關。工具寫出後要用它讀回。守衛：規則測試加突變。
4. **嚴格**：未知的 schema、version、欄位，重複的 key，非正規的順序，超出範圍的值，一律是錯誤，不忽略、不修正。守衛：同上。
5. **確定**：同一份文件，在每個平台展開出逐位元相同的 float。守衛：黃金值測試，比對 float 的位元。

### Replacement Criteria（草案）

1. 碰撞需要 AABB 以外的形狀（B2 斜坡）。
2. 面需要非軸向，或需要每頂點的屬性（AO、法線）。
3. 載入或碰撞查詢量到壓力，需要依區塊切分或空間索引。
4. 工具每加一個功能，就得重新驗證這些規則。

### 判定（現在固定）

| 判定 | 內容 | 通過條件 |
|---|---|---|
| V1 | 規則 1～10 各一組測試（合法的最小文件也算） | 全部通過 |
| V2 | 展開的黃金值：一個 +Y 面、一個 −X 面、一個 w=3 h=2 的 +Z 面，比對頂點、索引、UV 的 float 位元 | 位元相同 |
| V3 | 換算的黃金值：`cell_size` 1.0 和 0.3、`origin` 為負、範圍邊界 ±2^20，比對 float 位元 | 位元相同，邊界外拒絕 |
| V4 | 連結守衛：configure 時檢查模組只連 Base、Math、nlohmann | 通過；守衛的突變被殺死 |
| V5 | 宣告的突變（`docs/tools/plans/v8/scripts/voxel_mesh_mutations.json`，至少 10 條：重複 key 被接受、未知欄位被忽略、version 2 被接受、sha 長度不檢查、faces 少一組被接受、`dir` 6 被接受、`w`=0 被接受、正規順序不檢查、範圍不檢查、winding 反轉、UV 不取中心、換算少除以 16） | 全部 killed（被預期的測試以預期的訊息擋下） |
| V6 | L1：三個平台的 cpu\|shader | 綠 |

- 判定 V1～V5 在本機 macOS 跑。
- 證據放主 checkout 的 `build/evidence/tool-v8-voxel-map/engine-contract/`。
- 沒有 L2、L3。

## v8-tool-voxel-map-editor-core 的事前宣告

2026-11-05 寫，**要使用者核准後才實作**。結果出來之後，判定不放寬（AGENTS §14.1）。實作時如果要改宣告，在**審查前**先回報（上一批這點沒做到）。

- **分支**：PR #152，同一條功能線。
- **重估**：14～20 h（原本 12～18 h）。多出來的是 24 種朝向、形狀庫和碰撞盒合併的性質測試。
- **段**：上段：產物的寫出端牽動 Engine 契約；新的工具 owner。
- **模型**：實作是主會話（Opus 5.5／high）；審查是子 Agent（Opus 5.5／high）；這批不放寬既有判定，所以不需要驗證者。

### 範圍

- **做**：
  - 登錄新工具 `voxel_map_editor`：`tools/voxel_map_editor/`、`project.json`、`tools.csv` 的一列（enabled、default 都是 true，release 是 false，三平台）、`tests/voxel_map_editor/`。
  - 核心程式庫：
    - `gyo.voxel_grid` 的讀寫；
    - 形狀庫和 24 種朝向；
    - build（格子 → `gyo.voxel_mesh`）；
    - 寫出前經 Engine 的 `ParseVoxelMesh` 讀回驗證。
  - CLI。
  - `docs/voxel_map_editor/architecture.md` 的 Design Invariants 和 Replacement Criteria。
  - 依賴守衛、規則測試、性質測試、突變。
- **不做**：
  - GUI、預覽（window 批）；
  - catalog 登錄、產品的 arena v2、兩張地圖的正式轉換與等價驗收（arena-v2 批）；
  - framework 的預覽元件。
- **只有 CLI 版本**：這一批 `project.json` 只有 `cli` 一個 variant。window 批再加 `gui`，並改成預設。

### Architecture Delta（AGENTS §3）

1. **為什麼需要**：`v8-tool-voxel-offline-build` 和 `v8-tool-voxel-layers` 定了「格子格式、build 歸工具」。
2. **現況的問題**：沒有任何東西能產生 `gyo.voxel_mesh`。
3. **變化的邊界**：新的工具 owner（依 [tool_projects.md](../../../tool_projects.md) 登錄）；新的、工具持有的格式 `gyo.voxel_grid`。
4. **影響範圍**：只有工具本身。Engine 和產品都不改。
5. **依賴方向**：工具 → Engine（`GYO::VoxelMesh`、`GYO::Engine` 的 io、`GYO::Base` 的 `Sha256`）、framework（`GYO::ToolFramework` 的 headless 外殼）、nlohmann（工具自己連，先例是 audio_editor 的工程檔）。不連遊戲、SDL、ImGui。
6. **Ownership**：格子格式和它的規則都歸工具。Engine 不知道方塊、形狀、朝向（`voxel-mesh.md` 的 DI 1）。
7. **為什麼沒有更小的做法**：照 AGENTS §5，CLI 就是 Script 階段；GUI 留到下一批。

### 格式 `gyo.voxel_grid` version 1（工具持有；草案，實作時要改會在審查前回報）

```json
{
  "schema": "gyo.voxel_grid",
  "version": 1,
  "origin": [-1, 0, -1],
  "cell_size": 1.0,
  "shapes": [
    { "name": "cube", "boxes": [{ "min": [0, 0, 0], "max": [16, 16, 16] }] },
    { "name": "slab", "boxes": [{ "min": [0, 0, 0], "max": [16, 8, 16] }] },
    { "name": "stair", "boxes": [{ "min": [0, 0, 0], "max": [16, 8, 16] }, { "min": [0, 8, 8], "max": [16, 16, 16] }] }
  ],
  "blocks": [{ "name": "wall", "shape": 0, "color": [51, 79, 99, 255] }],
  "chunks": [{ "at": [0, 0, 0], "runs": [[0, 0, 100], [1, 0, 16], …] }]
}
```

- **`origin`**：以格為單位，y 必須是 0（`v8-tool-voxel-coords`）。
- **`cell_size`**：範圍和 `gyo.voxel_mesh` 相同，[2^-16, 2^16]。
- **`shapes`**：每種形狀是一組盒子，單位是 1/16 格，範圍 0～16，每軸 `min < max`。名稱不能重複。
- **`blocks`**：每種方塊是 {名稱, 形狀的索引, sRGB 顏色}。
- **`chunks`**：
  - 每個區塊是 16³ 格，`at` 是區塊座標（格座標除以 16）。
  - `runs` 是 RLE，每段 `[方塊, 朝向, 長度]`：方塊 0 代表空格，1 以後代表 `blocks[方塊−1]`；朝向 0～23；每個區塊的長度加起來剛好是 4096。
  - 格子的順序是 x 最快，其次 z，最後 y。
  - 區塊依 `at` 的字典序嚴格遞增；全空的區塊不寫。
- **規則和 Engine 的格式一樣嚴格**：未知欄位、重複 key、超出範圍、非正規順序、空格卻帶朝向，一律報錯。
- **寫出的格式是固定的**：同一份內容只會寫成一種文字，讀進來再寫回去會逐位元組相同。

### 朝向（24 種）

- 朝向 `o` 的定義：「上方」= 6 個軸方向中的第 `o / 4` 個（+Y −Y +X −X +Z −Z），再繞這個上方軸轉 `(o % 4) × 90°`。
- 旋轉以格子中心 (8, 8, 8) 為軸心，盒子轉完仍然是軸對齊的盒子。
- 24 種朝向的矩陣列成一張表，寫進文件和測試。

### build

1. 每個非空格：把形狀的盒子轉成這格的朝向，換算成以 `origin` 為準的 1/16 單位。
2. **面**：每個盒子有 6 個面。一個面落在格子的邊界上，而且相鄰格在那一側蓋滿整個 16×16 面時，剔除它（Minecraft 的 cullface）。其他的面都保留：不做繪製的 greedy，格子內部盒子之間的面也不剔除。
3. **顏色**：
   - 調色盤的條目＝方塊，6 個方向的顏色＝方塊顏色乘上固定的亮度：+Y 1.0、−Y 0.5、±X 0.8、±Z 0.65（參考 Minecraft 的方向亮度）。
   - 亮度是工具的常數，在 Engine 之外，寫進工具文件。
   - 只列出用到的方塊，依名稱排序。
4. **碰撞**：
   - 所有盒子聯集之後，先把座標壓縮（取所有盒子的邊界值），再在壓縮後的格子上做固定順序的 3D greedy 合併：先沿 x 延伸，再 z，再 y。
   - 合併出的盒子彼此不重疊，聯集和輸入相同。
   - 形狀目前都是實心的（沒有 `solid` 欄位，`v8-tool-voxel-grid-format`）。
5. **輸出**：
   - 依 Engine 的正規順序排序，去掉重複的四邊形，寫成固定格式的 JSON。
   - `source_sha256` 是原始檔位元組的 SHA-256（`Engine::Base::Sha256`）；`build_version` 是 `"voxel_map_editor 1"`。
   - 寫出前用 `ParseVoxelMesh` 讀回；不通過就什麼都不寫。檔案用 `engine/io` 的原子替換寫出。
6. **`walls≤1024` 不在工具裡檢查**：這個數字是產品的規則，放進工具會讓工具知道產品（違反中立）。工具只回報盒子數，由產品的載入和測試擋下（arena-v2 批）。這是我對「build 超過就失敗」（`v8-tool-voxel-arena-v2`）的解讀，**請一併確認**。

### CLI（`gyo_voxel_map_editor`，經 framework 的 headless 外殼）

| 指令 | 內容 | 結束碼 |
|---|---|---|
| `build <grid> <mesh>` | build 並寫出產物 | 0 成功；1 輸入不合法；2 寫入失敗 |
| `verify <grid> <mesh>` | 重新 build，和已有的產物逐位元組比較 | 0 相同；3 不同；1 輸入不合法 |
| `inspect <file>` | 依 `schema` 判斷是格子還是產物，印出摘要：格數、方塊數、四邊形數、盒子數、hash | 0／1 |
| `convert-arena-v1 <arena.json> <grid>` | 把 arena v1 的 `walls`（必須落在整數格上）轉成格子，全部用 `cube`。出生點和移動參數不轉（它們歸 arena v2） | 0／1 |

### Design Invariants（草案，寫進 `docs/voxel_map_editor/architecture.md`）

1. **依賴**：不連遊戲、不提產品的名字。不包含 SDL、ImGui 的標頭，也不連它們的 target。守衛：依賴守衛（比照 model_viewer 的正向清單）和它的自我測試。
2. **一個驗證者**：產物是否合法，只由 Engine 的 `ParseVoxelMesh` 決定。工具寫出前一定讀回，不另外持有一份產物規則。守衛：「讀回失敗就不寫」的測試和突變。
3. **確定**：同一份格子，在每個平台都 build 出逐位元組相同的產物；格子讀進來再寫回也逐位元組相同。守衛：黃金檔比對、兩次 build 的比對。
4. **不失真**：碰撞盒的聯集等於所有盒子的聯集，盒子彼此不重疊；面只在被蓋滿時才剔除。守衛：性質測試。
5. **只寫它產生的檔**：原子替換；不碰 catalog、產品檔、arena。守衛：CLI 測試。
6. **格子歸工具**：`gyo.voxel_grid` 只有這個工具讀；出現第二個讀者時，就該升到 Engine（Replacement Criterion）。

### Replacement Criteria（草案）

1. 格子出現第二個讀者（例如場景編輯器）。
2. 形狀需要盒子以外的幾何（斜坡，B2）。
3. build 時間或產物大小量到壓力，需要繪製的 greedy、依區塊切分或增量 build。
4. 工具每加一個功能，就得重新驗證 Engine 的產物規則。

### 判定（現在固定）

| 判定 | 內容 | 通過條件 |
|---|---|---|
| E1 | `gyo.voxel_grid` 的規則：每條一組測試（含合法的最小文件、讀進來再寫回逐位元組相同） | 全部通過 |
| E2 | 朝向：24 個矩陣兩兩不同、都是正交矩陣、行列式 +1；朝向 0 是恆等；樓梯轉 90° 的盒子和手算的值相同 | 全部通過 |
| E3 | 面剔除的黃金值：1 個立方體 → 6 個面；2 個相鄰的立方體 → 10 個面；半磚旁的立方體 → 立方體朝向半磚的面保留，半磚朝向立方體的側面被剔除 | 四邊形逐筆相同 |
| E4 | 碰撞的性質：多組合成的格子（含隨機的，固定種子，200 組）都滿足「聯集相同」和「互不重疊」，比對在壓縮後的格子上做；2×1×1 個立方體 → 1 個盒子；L 形 → 2 個盒子 | 全部通過 |
| E5 | 確定性：一份含立方體、半磚、樓梯、多種朝向和顏色的格子，build 結果和 repo 裡的黃金產物逐位元組相同，在三平台都比對；同一輸入 build 兩次結果相同 | 逐位元組相同 |
| E6 | 一個驗證者：把寫出端改壞（例如讓 `dir` 變成 6），讀回會失敗，檔案沒有被寫出；所有黃金產物都通過 `ParseVoxelMesh` | 通過 |
| E7 | CLI：四個指令各自的成功與失敗路徑（`cmake -P`，比照 model_viewer）；`verify` 能偵測一個位元組的差異 | 通過 |
| E8 | 依賴守衛＋自我測試 | 通過 |
| E9 | 突變（`docs/tools/plans/v8/scripts/voxel_map_editor_mutations.json`，至少 14 條）：cullface 不檢查蓋滿、剔除方向反了、朝向表錯一格、旋轉軸心錯、RLE 長度不檢查、空格帶朝向被接受、合併後盒子重疊、合併漏掉一塊、亮度用錯方向、排序不正規、讀回失敗仍然寫出、`verify` 永遠成功、sha 算的不是原始檔、依賴守衛放進 SDL | 全部 killed |
| E10 | L1：三平台的 cpu\|shader | 綠 |

- 本機用 macOS Intel。
- 證據放主 checkout 的 `build/evidence/tool-v8-voxel-map/core/`。
- 沒有 L2、L3（GUI 在 window 批）。
- **範圍的限制**：兩張現行地圖的轉換只做 smoke（能轉、能 build、能讀回）。和 v1 的等價（`v8-tool-voxel-retire-v1`）留給 arena-v2 批。

## v8-product-arena-v2 的事前宣告

2026-11-05 寫，**要使用者核准後才實作**。結果出來之後，判定不放寬（AGENTS §14.1）。實作時要改宣告，在審查前先回報。

- **誰做**：這個會話（使用者 2026-11-05：「ここでやる」）。這是產品批，觸到的範圍會通知產品會話。
- **分支**：PR #152。
- **重估**：10～14 h（原本 6～10 h）。多出來的是下面的兩個決定點。
- **段**：上段（Data Contract、Match 的碰撞）。
- **模型**：實作是主會話（Opus 5.5／high），審查是子 Agent（Opus 5.5／high）。

### 要先決定的兩件事

**決定 1：Client 這一批要不要畫烘好的 mesh？**

- **(R1) 不畫**：Client 照舊，把 `walls`（產物的碰撞盒）一個個畫成盒子，畫面和現在一樣；mesh 的繪製另外開一批。
- **(R2) 畫（建議）**：Client 用產物的四邊形建一個 mesh、用調色盤建一張貼圖，一次提交；v1 的 arena 照舊畫盒子。理由：
  - 不畫的話，編輯器做出來的樓梯、半磚和顏色在遊戲裡看不到；
  - window 批的 L3（「建一張有二樓和階梯的地圖，進遊戲走一圈」）也需要它。
  - 代價約 +3～4 h。
- **怎麼把 mesh 交給 Client**：`Arena` 不能加成員（digest 會變成 wire 變更，`v8-tool-voxel-arena-v2`），所以 `LoadInstalledArenas` 多一個可選的輸出參數，同時交出每張 arena 的幾何（`Engine::VoxelMesh::VoxelMeshGeometry`，v1 是空的）。Match 和其他呼叫端不變。

**決定 2：碰撞盒的合併方式要改（`v8-tool-voxel-retire-v1` 的「不相等就先調整掃描順序」）**

- **現況**：用現在的合併（沿 x 延伸再沿 z），現行地圖會得到「南北牆含角落、東西牆在中間」；v1 剛好相反，是「東西牆含角落、南北牆在中間」。兩者盒子數相同（5 個），但集合不同。
- **改法（建議）**：每一層裡，先在每一行 x 上取沿 z 的**最長連續段**，再把**完全相同**的相鄰段沿 x 合併；最後和現在一樣，相同的矩形沿 y 往上長。
  - 這是掃描線式的條帶合併。聯集相同、互不重疊的性質不變（工具的 DI 4）。
  - 照手算，現行的兩張地圖會和 v1 的牆集合完全相同。
- **影響**：這是工具 core 的行為變更。黃金產物要重新產生，碰撞盒相關的測試和突變也要更新。工具的 DI 和格式都不變。

### 範圍

- **arena 檔的 schema v2**：
  - `"version": 2`；
  - 用 `"geometry": "<相鄰的檔名>"` 取代 `walls`，其他欄位不變。
  - 檔名的規則比照 `arenas.json`：純檔名，UTF-8。
- **`Arena::Load` 同時接受 v1 和 v2**。v2 會讀相鄰的 `gyo.voxel_mesh`（上限 16 MiB），經 `ParseVoxelMesh` 和 `ExpandVoxelMesh` 之後，把碰撞盒依產物的順序填進 `walls`。
  - 記憶體裡的 `Arena.version` 維持 1。
  - `Arena` 的成員和 digest 演算法都不變。
  - `walls≤1024` 照舊。
- **出生點的規則**：從 `y==0` 改成「`y==0`，或依 `Movement.cpp` 的站立判定有支撐」。
  - 站立判定抽成一個共用函式，`Movement` 和 `Validate` 都呼叫它，所以兩邊一定一致。
  - 這是 arena 契約的變更，不是 wire 變更（出生點的 y 本來就在 digest 裡）。
- **兩張現行地圖轉成 v2**：
  - 用 `gyo_voxel_map_editor import-boxes <v1> /walls` 和 `build` 轉換；
  - 格子原始檔放主 checkout 的 `build/art-source/object_fps_pvp/maps/`（`v8-tool-voxel-source-location`）；
  - 產物 `pvp_arena.voxel_mesh.json`、`pvp_corners.voxel_mesh.json` 放在資產根目錄，和 arena 檔相鄰；
  - 登錄 catalog（type `text`），列入 `RETAINED`（以路徑載入），加上 root-file 的例外（和 arena 檔同一個理由：`v8-product-asset-path-loaded-move` 延後）。
- **(R2) 的話**：Client 的繪製改用 mesh，v1 照舊。
- **文件**：`docs/object_fps_pvp/assets.zh-Hant.md`（地圖產物的登錄與 RETAINED）；arena 的契約說明寫在產品文件裡協議引用的位置，在實作時查出確切的檔案。

### 判定（現在固定）

| 判定 | 內容 | 通過條件 |
|---|---|---|
| P1 | v2 載入的規則：缺 `geometry`、檔名不合法、找不到檔、產物不合法（例如 Engine 拒絕的檔）、v2 卻還有 `walls`，都要拒絕，並有明確的錯誤 | 全部通過 |
| P2 | **退役證據一**：兩張地圖的 v2 `walls` 和 v1 `walls` 作為集合逐位元相同 | 兩張都相同 |
| P3 | **退役證據二**：v1 和 v2 的 `Arena`，以同一串固定命令（走、跳、撞牆、沿牆滑、蹲；含四個角落和柱子）跑 `StepMovement`，每一步的位置逐位元相同 | 兩張都相同 |
| P4 | 出生點的規則：在方塊頂面上的出生點會被接受、懸空的會被拒絕、地板上的照舊；`Validate` 和 `Movement` 用的是同一個函式 | 通過 |
| P5 | 資產契約測試（catalog、RETAINED、root-file 例外）通過；格子原始檔不在 repo | 通過 |
| P6 | 既有的 PvP 測試全部通過（`ArenaDigestTests`、`PvpMatchTests`、移動相關測試）。digest 會變，這是內容變更；寫死既有 digest 值的測試，如果因此要改，依 AGENTS §14.4 列出並取得核准 | 通過，或改動經核准 |
| P7 | 工具（決定 2）：合併的性質測試照舊通過；新黃金值重新產生；「現行兩張地圖的集合等於 v1」也寫成工具端的測試 | 通過 |
| P8 | 突變（至少 8 條）：v2 讀到 `walls` 卻被接受、`geometry` 檔名含路徑、產物不經 Engine 驗證、出生點懸空卻被接受、`Validate` 和 `Movement` 各寫一份判定、碰撞盒順序被打亂、(R2) 有 mesh 卻退回畫盒子、合併時把不同段沿 x 合在一起 | 全部 killed |
| P9 | L1：三平台的 cpu\|shader | 綠 |
| P10 | L3（R2 才有）：使用者在 macOS 進遊戲，兩張地圖的外觀與碰撞和以前一致 | 使用者確認 |

- **範圍的限制**：
  - P3 的軌跡只比 `StepMovement`（Match 與 Client 的預測共用），不比網路的流程。
  - `walls` 的順序和 v1 不同：v1 照檔案順序，v2 是正規順序。如果 P3 因為順序而不同，就照 AGENTS §14 停下回報。
- 證據放主 checkout 的 `build/evidence/tool-v8-voxel-map/arena-v2/`。

### 實作後的變更（2026-11-05，經使用者決定）

- **搬到 `maps/`**（`v8-product-arena-v2-maps-move`）：原本宣告「產物放資產根目錄、加 root-file 例外」，改成把延後的 `v8-product-asset-path-loaded-move` 併進這一批。
  - arena 一組（`arenas.json`、`pvp_<name>.json`、`pvp_<name>.voxel_mesh.json`）搬進 `maps/`，`movement_rules.json` 搬進 `rules/`。
  - 根目錄的 6 筆例外全部刪除，不加新例外。
  - 資產守衛的 `FOLDER_TYPES` 加 `maps`（DI-2 守衛的判定變更，照 §14.4 走程序：`v8-product-arena-v2-maps-guard`）。
  - `content_deep_path` 的常數沒有改，理由寫在 dev_log。
- **P3 降為回歸檢查**（`v8-product-arena-v2-p3-regression`）：審查實測發現，軌跡對「集合相同、分法不同」的分解沒有檢出力。所以 v1 退役的證據只有 P2（集合逐位元相同）。P3 的腳本加上繞柱、沿柱滑、斜進四角，只收緊。
- **P10 改寫、加 P11**（`v8-product-arena-v2-p10-rewrite`，取代先前「和舊地圖外觀一致」的定義）：烘焙地圖本來就和舊地圖長得不一樣，「一致」無法判定。
  - P10（L3）改成「看起來能玩」：沒有破洞、沒有閃爍（z-fighting）、牆和地板分得出來、看得到對手。
  - 能用數值確認的部分移到 CI，成為 P11：在烘焙檔的 1/16 格上，(1) 碰撞體露在外面的每個面都有畫出的四邊形蓋住（沒有破洞），(2) 每個畫出的四邊形背後都有實心（不會畫在身體穿得過的地方）。夾在實心之間的面可以畫（工具保留同一格裡盒子之間的面）。
  - 「voxel 實心格和碰撞的對照」需要格子原始檔（不在 repo），由工具的 E4 性質測試與本機的 `gyo_voxel_map_editor verify` 擔保；出生點不在實心裡且站在支撐上，已由 P4 檢查。
- **補的測試與突變**（依審查）：
  - 柱子邊緣逐點比較 `Validate` 與 `StepMovement` 的著地；
  - Client 依 id 選烘焙地圖（`InstalledArenaIndex`）；
  - 載入失敗時不交出 geometry；
  - 出生點低於地板；
  - `maps` 資料夾種類被拿掉時守衛會失敗。

## 風險

- **格子出現第二個讀者**（例如場景編輯器要在格子上擺東西）：格子就要升到 Engine，等於再搬一次契約。寫進工具的 Replacement Criteria。
- **改了碰撞盒的分解演算法，digest 就會變**：舊的 Client 全部不相容。`build_version` 要和內容一起管理。
- **v1 退役的等價不是必然成立**：合併出來的盒子不一定和手寫的牆相同，所以先比集合（`v8-tool-voxel-retire-v1`）。
- **CI 不重新 build**：原始檔和產物是否一致，只靠「編輯器存檔時一定同時寫出產物」和 `source_sha256`。原始檔放在 git 不追蹤的位置，遺失就只剩產物。
- **framework 批要等 PR #146 合併**：#146 如果再改 `PreviewRenderer` 的介面，這一批要重估。
- **P1**：編輯時要局部重建預覽，每幀的工作比 model_viewer 多，(B′) 的讀回可能不夠快。

## 候選與延後

| 項目 | 何時再看 |
|---|---|
| 繪製的 greedy | 量到三角形數或產物大小的壓力 |
| AO | Render 有頂點屬性和光照時（Render 的 Delta） |
| 空間索引、產物依區塊切 | 量到載入或碰撞查詢的壓力；`walls≤1024` 不夠時 |
| B2 斜坡 | B1 完成後，用實際地圖確認範圍；最好和下一次已經要改協議的批次一起做（Collision 的新形狀、`Arena` 新成員、新的 pvN） |
| 地板用方塊（坑洞、地下層） | 產品要這個 Feature 時 |
| 擠出、圖層、形狀筆刷 | 第一版用過之後 |
| 地圖搬進 `maps/`、改用 asset id 引用 | `v8-product-asset-path-loaded-move`（[asset-layout.md](../../../object_fps_pvp/plans/v8/asset-layout.md)） |

## 證據

- 主 checkout 的 `build/evidence/tool-v8-voxel-map/plan/`（git 不追蹤）：
  - 使用者的報告（PDF＋文字版）；
  - Goxel、`.vox` 規格、Godot GridMap 的原始檔，出處在 `reference/SOURCES.txt`；
  - 兩輪統合與逐點討論的計畫檔 `plan-discussion.md`。
