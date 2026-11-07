# v8 工具：預覽的距離上限與視錐剔除（`ve-cull`）事前宣告

更新：2026-11-07。Owner：voxel 地圖編輯器線（本會話）。
- 起因：[ve-scale-measure 的 dev_log](../../../dev_logs/2026_11_07_tool_v8_voxel_scale_measure.zh-Hant.md)
  - km2-ground 全畫 15,625 區塊，p95 388 ms。
  - km2-building 有 46,875 區塊，在 AMD 的 Metal 驅動裡崩潰。
- 架構師轉達的使用者決定（2026-11-07）：視錐的數學放 Engine `GYO::Math`（原話「Engine 本身就應該具有這些能力」）。
- 紀錄：[HANDOFF](voxel-map-editor-HANDOFF.md)。

**狀態：待使用者核准。核准前不動工。**

## 代號對照

| 代號 | 白話 |
|---|---|
| `ve-cull` | 預覽只畫「離相機不遠、而且在畫面裡」的區塊 |
| `math-frustum` | Engine Math 新增視錐型別與「視錐和盒子是否重疊」的查詢 |

## 範圍

### 1. Engine `GYO::Math`：`Frustum` 與 `Intersects`（Engine 公開介面的追加）

- **Architecture Delta（一句）**：在 `engine/math/include/engine/math/geometry/Frustum.hpp` 新增純幾何型別 `Frustum` 與查詢 `Intersects(const Frustum&, const Aabb&)`。
  - 不改既有型別與既有函式。
  - 先例：ParseMarkerKinds。
- **介面（草案，由架構師審）**：
  ```cpp
  // Six planes, normals pointing inward: left, right, bottom, top, near, far.
  struct Frustum final { std::array<Plane, 6> planes{}; };
  // From a clip matrix (projection * view, column vectors, depth 0..1 per math.md).
  [[nodiscard]] inline Frustum MakeFrustum(const Matrix4& viewProjection) noexcept;
  // Conservative: false only when the box is fully outside one plane.
  [[nodiscard]] inline bool Intersects(const Frustum& frustum, const Aabb& box) noexcept;
  ```
- **規則**：
  - 平面從 clip 矩陣的列取出（Gribb–Hartmann 法），再正規化。
  - 深度用 0..1：near 平面是 row 2，不是 row 3 + row 2。
  - `Intersects` 用每個平面的「正頂點」（p-vertex）判斷。
  - 這是保守的判定：盒子在視錐外、但靠近角落時，可能回傳 true；不在任何一個平面外的盒子，一定回傳 true。
  - 這一點寫進 `math.md`：用途是剔除，不是精確的相交。
  - 觸碰算相交（照 `math.md` 的 closed 慣例）。
  - 沒有 epsilon。
  - 純函式、無狀態，所以決定性自然成立。
- **文件**：`docs/architecture/math.md` 新增一節「Frustum」，Layout 與 Types 兩節同時更新。
- **審查**：標頭與文件由架構師審。
- **測試**：放進 `tests/common/math` 的 `gyo_math_tests`（共通測試，不依賴產品）。
  - 用 `MakePerspective` 加上單位 view 組出視錐，對已知的盒子判定：
    - 正前方 → true；
    - 相機背後 → false；
    - 超過 far → false；
    - 剛好貼著 near → true（closed）；
    - 左、右、上、下各在外側 → false；
    - 包住整個視錐的大盒子 → true；
    - 角落外、但不在任何單一平面外的盒子 → true（記下保守的行為）。
  - 加一條：平面法向量是單位長度，而且朝內（視錐內部的一點對六個平面的 `SignedDistance` 都 > 0）。
- **突變**：放進本線的 catalog（`math-frustum-*`），target 是 `gyo_math_tests`。
  - near 平面誤用 OpenGL 的 -1..1 公式。
  - 正頂點取成負頂點。
  - 平面不正規化。
  - 漏掉 far 平面。
  - 比較從 `<` 改成 `<=`，讓觸碰變成不相交。

### 2. 預覽（工具，消費者）

- **距離上限**：
  - 區塊的 AABB 和相機的水平距離超過「繪製距離」就不畫。
  - 預設 256 格，介面可調 64～1024 格。
  - 這個距離和拾取距離（1024 格）是兩個值，不互相綁定。
- **視錐剔除**：用 `Math::MakeFrustum` 與 `Intersects` 判斷區塊的 AABB，不在視錐內就不畫。
- **迴圈與距離判斷都留在工具**（架構師的分工）；遊戲之後的 scene v1 Client 自己寫自己的迴圈。
- **不改**：
  - 不改上傳：所有區塊的 mesh 照舊都建立。
  - 不改烘焙、格式、存檔。
  - DI 7「預覽畫的就是存檔寫的」不受影響：剔除只決定這一幀畫哪些，不改內容。
- **介面**：在狀態列顯示「畫了 N／共 M 區塊」，讓你看得出剔除在動作。

### 要你決定的一點：view-projection 矩陣從哪裡來

- `Render` 的 `ViewMatrix`／`ProjectionMatrix` 是 `Renderer.cpp` 裡的私有函式。剔除要用的矩陣，必須和實際畫的完全一致，否則邊緣會缺塊。
- **建議 (b)**：把這兩個函式（或一個 `ViewProjectionMatrix(camera, aspect)`）公開到 Render 的標頭。
  - 只是公開既有的計算，行為不變。
  - 但這是第二個 Engine 公開介面的追加（Render owner），也請架構師審。
  - 遊戲的 Client 之後也會需要。
- **(a)**：工具自己用 Math 照同樣的順序組出矩陣。
  - 不動 Render。
  - 但同一個相機慣例會出現兩份，Render 一改，剔除就默默地錯；這正是 AGENTS §2 說的「同じ Conversion の繰り返し」。

## 預算（AGENTS §2；看結果前宣告，看到後不放寬）

| 代號 | 情境 | 預算 |
|---|---|---|
| CB1 | `building`（768 區塊），預設繪製距離，`--no-edit` 的固定相機 | 影格 p95 ≤ 16.7 ms |
| CB2 | `km2-ground`（15,625 區塊），預設繪製距離 256 格，同上 | 影格 p95 ≤ 16.7 ms，而且畫的區塊數 ≤ 2,000 |
| CB3 | 同 CB2，繪製距離 1024 格（最大） | 只記錄，不設預算（用來看上限附近的成本） |

- 方法與 ve-scale-measure 的 SM6 相同：600 幀，丟掉前 60 幀，記 p50、p95、p99、max，加上每幀畫的區塊數。件數の突き合わせ照舊。
- **不測 km2-building 的繪製**：
  - 剔除不減少上傳的 mesh 數（46,875 個）。
  - 驅動的上限是「一幀引用的緩衝區」還是「建立的總數」，目前不知道。
  - 再跑一次可能再當一次機器。這要等 ②（以 region 合併緩衝區），交給架構師一起規劃。
- 不過預算時：照 AGENTS §14 停下回報，不在本批改方法。

## 對機器的影響（跑之前會再問你）

- **CB1**：768 區塊，window-1 的 P1 已經跑過同規模，影響小。
- **CB2、CB3**：要上傳 15,625 個 mesh，上一次全畫時，每幀約 300 ms、整台機器變慢。剔除後預期每幀變輕，但上傳本身仍要數秒到十數秒。
- **做法**：
  1. 先跑 60 幀的試跑，量出單幀成本；
  2. 把「要跑多久、會不會卡」告訴你；
  3. 你同意，或指定時段後，才跑 600 幀。
- 失敗後不立刻重試。

## 判定（完成條件）

| 代號 | 判定 |
|---|---|
| C1 | `gyo_math_tests` 的 Frustum 規則測試通過；`math-frustum-*` 突變全部 killed（期待的訊息） |
| C2 | 預覽的剔除有自我測試：固定相機下，畫的區塊集合等於預期（背後與距離外的不在集合裡）；相關突變 killed |
| C3 | CB1、CB2 在預算內，件數一致；CB3 有記錄 |
| C4 | 架構師審過 Math 的標頭與 `math.md`（選 (b) 時也審 Render 的標頭） |
| C5 | 本機 `cpu|shader` 全部通過；L1 三平台通過 |
| C6 | dev_log 和 HANDOFF 在同一個變更裡更新 |

## 不做

- 以 region 合併緩衝區（②）、LOD、二進位快取（回到候選）。
- 拾取距離、DDA。
- 拖曳開始時的整份複製、undo 的大小、平行烘焙（另批）。
- 遊戲端的剔除迴圈（產品的 scene v1）。

## 分批、模型、ギア、估時

```text
ve-cull（上段：觸及 Engine 公開介面）
├── math-frustum      Opus／high｜2～3 h：Frustum、Intersects、math.md、測試、突變
├── ve-cull-preview   Opus／high｜2～3 h：距離上限、剔除迴圈、狀態列、自我測試（選 (b) 時含 Render 公開）
└── ve-cull-measure   Opus／medium｜1～2 h＋你指定的時段：CB1～CB3
```

- **上段的根據**：觸及公開介面（navigator.md）。
- **模型**：實作者是 Opus 5.5（本會話）；審查者是架構師（Math／Render 的標頭與文件）。
- **估時**：合計 5～8 h（不含等你的時段）。超過 12 h，或單一問題累計超過 2 h，停下回報。
- **順序**：之後接 `ve-feel-survey`（已核准）。
- **分支**：同一條 `claude/v8-tool-voxel-editor-measure`；和量測兩批一起開一個 PR，依賴 PR #169。

## 要使用者決定的事

1. 本宣告：API 草案、預設繪製距離 256 格、預算 CB1～CB3、不測 km2-building。
2. view-projection 矩陣：(b) 公開 Render 既有的計算（建議），或 (a) 工具自己組。
