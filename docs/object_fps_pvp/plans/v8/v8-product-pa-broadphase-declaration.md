# v8-product-pa-broadphase 事前宣告

更新：2026-11-07。Owner：`object_fps_pvp`。來源：big-map 計畫 `v8-arch-big-map-product-arena-v3` 的第一批（[big-map HANDOFF](../../../architecture/plans/big-map/HANDOFF.md)「product-arena-v3（核准版）」、決定 `v8-arch-big-map-product-order`）。
**狀態：使用者 2026-11-07 照推薦核准**（上限的 assert 採 (b)：請架構師在 Engine 加不 assert 的檢查；biome 可進 repo：開源地圖、個人測試用）。判定與閾值在看到任何結果之前寫下，核准後不放寬（AGENTS §14.1）。

## 0. 為什麼現在做（Feature → Contract → Architecture）

- **Feature**：使用者 2026-11-07 決定，週一（11/9）的 lag-l2＋K8 場次用新的大地圖 biome，2～3 人（第 3 台 Windows），現場沒有網路。
- **障礙**：現在的 Match 用 `Arena.cpp:44` 的 `walls.size() > 1024` 拒收 biome（單檔 1,260 個碰撞盒），錯誤訊息只說 spawns。
- **順序**（使用者 2026-11-07）：pa-broadphase → 週一場次 → `v8-product-hitbox-rewind` → `pm-detect`。只撤上限、仍然線性掃，是 AGENTS §4 不允許的 workaround，所以撤上限和接 broadphase 在同一批。
- **退路**：週日晚上前沒合併，週一照已備好的 kit（`54948874`，`pvp_corners`）跑。

## 1. 範圍

把產品裡所有「對全部牆線性掃」的查詢，改成 Engine 的 `GYO::Collision::StaticAabbGrid`（契約與 Design Invariants：`docs/architecture/collision.md`）。Match 與 Client 預測用同一份函式。

| 位置 | 現在的掃法 | 換成 | 等價的理由 |
|---|---|---|---|
| `CanPlaceCharacterBody`（Movement 蹲起、PvpMatch 出生點、Arena 驗證） | 任一牆 overlap depth > 1e-5 就 false | `OverlapVerticalCapsuleAll` | 只看「有沒有」，與順序無關 |
| `IsCharacterSupported` | 任一牆往下掃 2 mm、法線 y > 0.5 | `SweepVerticalCapsuleAll` | 同上 |
| `MoveCharacterBody` 穿透解除 | 依索引順序、`>` 取最深（含 PlanarPenetration） | `OverlapVerticalCapsuleAll`，照索引遞增套用原本的同一段邏輯 | All 回傳的是線性掃中「有值」的項目、索引遞增；其他項目在原迴圈裡本來就不會被選 |
| `MoveCharacterBody` 掃描滑動 | 依索引順序、先過濾（`constrainToFloor` 的水平法線）再 `<` 取最近 | `SweepVerticalCapsuleAll`，照索引遞增套用原本的過濾與比較 | 過濾在比較之前，所以不用 `Nearest`；用 All 保留原本的順序 |
| `ShotQuery` 的牆 | 地板之後依索引 `<=` | `RaycastNearest(…, HighestIndex)`，再與地板用 `<=` 比 | 牆的結果只有 `World, 0, distance`，最小距離相同就結果相同 |

- `Arena` 持有用 `walls` 建出的格子（建一次、不變）。`walls` 仍是權威：digest、wire、渲染照舊讀 `walls`。
- 角色之間（actors）的查詢不變（人數 ≤ `MaxPlayers` 4）。
- 格子的 cell 長度：用 arena 的 `cell_size`。`CellLength()` 寫進 Match 的啟動 log；若是給定值的 4 倍以上，照 collision.md 的 Replacement Criteria 5 回報使用者。

### 上限

- `walls.size() > 1024` 改成宣告的預算：**colliders ≤ 2^20（1,048,576）**。Engine 的 2^22 cells、2^32 登記在這個預算之內。
- 超過時的錯誤訊息說出真因（`Arena has N colliders; at most 1048576`），不再說 spawns。spawns 的錯誤訊息照舊。
- **待決（實作前要定，回報使用者）**：Engine 的建構子用 assert 擋「登記數 ≥ 2^32」。這是內容（地圖檔）可以觸發的，不能讓壞地圖變成 assert 當機。兩個做法：
  - (a) 產品在建格子前自己算登記數。缺點：重複 Engine 的分格規則（AGENTS §3 狀態權威的疑慮）。
  - (b) 請架構師在 Engine 加一個不 assert 的檢查（例如回傳登記數或 `Result`）。缺點：動 Engine，要架構師的會話做。
  - 推薦 (b)。若週日前來不及，暫用「colliders ≤ 2^20 且每個盒子的邊長 ≤ 地圖的寬與深」這種不重複分格規則的保守上界，並在 dev_log 記為待收的債。

## 2. 不做

- arena v3／scene v1、區塊化地圖（`gyo.voxel_map`）的讀取。biome 用單檔的 `gyo.voxel_mesh` v1（1.4 MB，未超過 16 MiB）。
- 移動的缺陷（卡在空中、空氣牆）：`pm-detect` 另一張卡（`movement-fix.md`）。**這批不改任何移動的語意**，等價就是證明。
- 撤 1024 而不接 broadphase。

## 3. biome 地圖進 repo

- 把 biome 的 `biome.json` 與 `biome.voxel_mesh.json` 加到 `assets/object_fps_pvp/maps/`，登記到 `arenas.json`。Client 與 Match 都從套件讀同一份（arena digest 要一致）。
- **待決（使用者）**：biome 是用 MagicaVoxel 的 `.vox` 轉來的。要確認 `.vox` 的出處與授權可以放進 repo。不行就只放進場次的套件，不進 repo。

## 4. 判定（先宣告）

| 代號 | 判定 | 通過條件 |
|---|---|---|
| B1 等價（既有） | 既有的 PvP 測試（含 AuthorityDigest 的 golden、Movement、ShotQuery、Arena 測試）不改判定 | 全部通過 |
| B2 等價（新） | 隨機交叉：200 組隨機牆（含相鄰、重疊、貼齊、零距離的邊界）× 每組 200 個隨機查詢；新函式對照保留在測試裡的舊線性實作 | 全部 bit 一致（float 以位元比較；ShotQuery 比 kind、targetId、distance、part） |
| B3 上限 | 1,025 與 2^20 個 collider 的合成地圖可以載入；2^20＋1 被拒，訊息含 collider 數 | 三個 case 都成立 |
| B4 tick 成本 | Intel Mac、RelWithDebInfo。每次查詢計時，取 p99：(i) biome（1,260 盒）；(ii) 合成 10^5 盒（長走廊與樓層，盒子大小與 biome 同級） | Movement 一步（一個玩家）p99 ≤ 0.4 ms、ShotQuery 一發 p99 ≤ 0.2 ms；4 人同一 tick 合計 ≤ 2 ms（16.67 ms 的 12%） |
| B5 biome 能玩 | 本機：Match 用 biome 啟動到 ready，Client 2 個加入、移動、射擊 | Match ready、兩個 Client 進場、`match-shots.jsonl` 有 decision |
| B6 突變 | 新突變（Movement 的 tie 改 HighestIndex、ShotQuery 改 LowestIndex 且不比地板、上限差一、Validate 在建格子前用空格子、All 換成 Nearest） | 全部 killed |
| B7 L1 | 三平台 | 通過 |

- B4 的量法（2026-11-07 補，跑之前寫下）：測試 `WallGridEquivalenceTests.cpp` 的 B4（CI 跳過，`--no-skip` 執行）。4 名玩家各 20,000 tick：每 tick 每人一步 `StepMovement`（約每秒換一次方向、約 1/30 機率跳）與一發 `QueryShot`（射程 100 m，其他玩家都在場）。「Movement 一步」「ShotQuery 一發」各自計時取 p99；「4 人同一 tick 合計」＝該 tick 4 步＋4 發的總和取 p99。biome 用 `build/evidence/tool-v8-voxel-map/window-1/biome/game/bin/assets/object_fps_pvp/maps/biome.json`（1,260 盒）；合成圖是 600×600 m、高 0～20 m、邊長 0.5～4 m 的 10^5 個盒子（固定亂數）。
- B4 的閾值依據：tick 16.67 ms；Match 的一個 tick 還要做網路與其他處理，牆的查詢給 12%。數字是事前宣告，看到結果後不放寬。
- 件數的突き合わせ：B2 記錄「產生的查詢數＝比較的查詢數」，不一致就不判定。

## 5. 模型與檔位（照 big-map 的核准計畫）

- 上段（Match 判定、Collision 語意）。
- 實作者：主會話 Opus 5.5／high；等價的部分（§1 表格的 4、5 列）局部 xhigh。
- 審查者：Opus。建議請寫了 `StaticAabbGrid` 的 engine-grid 會話（`local_9f46f20d`）審。
- 驗證者：Fable／xhigh（使用者 2026-11-07 隨計畫核准）。驗 B2 的交叉測試是否真的覆蓋到 tie 與過濾的順序。
- 估時：6～10 h（含審查與驗證）。超過 15 h 停下回報。

## 6. 週一場次要跟著改的（另一份修訂，不在這批）

- lag-l2 宣告修訂 8：程式改為 pa-broadphase 合併後的 commit、地圖改 biome、3 人（`win3`）。判定（S1、S2、R1、R2）不變。
- Mac 從那個 commit 本機建置；Windows 用 master 整合建置的套件。
- 已知風險：`pm-detect` 之前的移動缺陷可能讓玩家卡住，射擊數不足時 S1 不成立（宣告：場次只跑一次）。
- **待決（使用者）**：架構師建議（2026-11-07），biome 的 arena JSON 暫時把 `radius` 設 0.3（純資料，不改程式，不是 pm-vertical 的實作），避開「一格縫＝2r」的退化點之一。`radius` 只影響移動與出生點的碰撞；部位判定用 `PlayerHitbox` 的資料，不受影響（`ShotQuery.cpp` 的 `StanceCapsule` 只在合成模型用）。

## 7. 文件

- big-map HANDOFF 由這批的 PR 續記「現況：pa-broadphase」，並把「下一步」改成：pa-broadphase（現在）→ 11/9 場次（biome；週日晚未合併退回 pvp_corners kit）→ hitbox-rewind → pm-detect → scene v1 → pa-measure（架構師 2026-11-07，不另開 docs PR）。
- v1 單檔的退役在 scene v1 做，這批不動。
