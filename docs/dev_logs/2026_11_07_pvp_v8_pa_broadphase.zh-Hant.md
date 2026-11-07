# PvP v8：v8-product-pa-broadphase（牆的查詢改用 StaticAabbGrid，撤掉 1024 上限）

日期：2026-11-07。分支 `claude/pvp-v8-pa-broadphase`。主會話 Opus 5.5／high；審查者 Opus（engine-grid 會話）；驗證者 Fable／xhigh（general-purpose 子 agent，唯讀）。估 6～10 h。

**狀態：實作與 B1～B6 完成；審查、驗證、B7（L1）未完。** [事前宣告](../object_fps_pvp/plans/v8/v8-product-pa-broadphase-declaration.md)使用者 2026-11-07 照推薦核准（上限的 assert 採 (b)；biome 可進 repo）。

## 為什麼

使用者要在 11/9 的 lag-l2＋K8 場次用大地圖 biome（2～3 人、現場沒有網路）。Match 以 `walls.size() > 1024` 拒收 biome（1,260 盒），錯誤訊息只說 spawns。順序改成 pa-broadphase → 場次 → hitbox-rewind → pm-detect（使用者 2026-11-07；架構師已記）。

## 做了什麼

- `CharacterCollision` 的三個函式改收 `StaticAabbGrid`：
  - `CanPlaceCharacterBody`、`IsCharacterSupported` 用 `OverlapVerticalCapsuleAll`／`SweepVerticalCapsuleAll`（只看有沒有）；
  - `MoveCharacterBody` 的穿透解除與掃描滑動都用 All，依索引遞增套用原本的邏輯（掃描的過濾在比較之前，所以不用 Nearest）。
- `ShotQuery` 的牆用 `RaycastNearest(…, HighestIndex)`，再與地板用 `<=` 比。
- `Arena` 加 `wallGrid`（`shared_ptr<const StaticAabbGrid>`）、`BuildWallGrid()`、`WallGrid()`。`Load` 在 `Validate` 之後建。`walls` 仍是權威（digest、wire、渲染）。digest 的結構化綁定加了第 14 個名字（`wallGrid`，不進 digest）。
- 上限：`MaximumArenaColliders = 2^20`；超過時 `Arena has N colliders; at most 1048576`。暫時的保守上界 `RegistrationsFit`（每軸至多 floor(邊長／cell)＋2 格）擋住 Engine 會 assert 的登記數；架構師的 [#166](https://github.com/yojinn-io/GYO-Engine/pull/166)（`StaticAabbGrid::Build`）合併後換掉。
- Match 啟動 log 加一行 `wall grid colliders=… cell=… given=…`（collision.md 的 Replacement Criteria 5）。
- biome 進 repo（`assets/object_fps_pvp/maps/biome.json`、`biome.voxel_mesh.json`，`arenas.json` 第 3 個）。來源：開源的 MagicaVoxel 地圖，使用者的個人測試用（使用者 2026-11-07），由 voxel 線轉換與烘焙。
- 測試：`WallGridEquivalenceTests.cpp`（B2 交叉、B3 上限、B4 計時，B4 在 CI 跳過）。

## 判定

| 判定 | 結果 |
|---|---|
| B1 既有測試 | 通過：`cpu\|shader` 138／138（含 AuthorityDigest 的 golden） |
| B2 等價 | 通過：40,000 個查詢（200 組 × 200），比較數＝產生數，不一致 0 |
| B3 上限 | 通過：1,025 與 2^20 接受、2^20＋1 拒絕（訊息含 collider 數）、過大的盒子回錯誤而不是 assert |
| B4 tick 成本 | 通過（Intel Mac i9-9980HK、RelWithDebInfo）：biome p99 一步 0.0067 ms、一發 0.0047 ms、一 tick 0.032 ms；合成 10^5 盒 0.0015／0.0057／0.016 ms（閾值 0.4／0.2／2 ms） |
| B5 biome 能玩 | 通過：Match `arena=biome_trial` ready，Client 2 個加入（使用者試玩），45 個 decision，`trace_end` records 45、dropped 0 |
| B6 突變 | 通過：15／15 killed（新 7＋改動範圍 8） |
| B7 L1 | 未跑 |

- B4 跑了兩次，兩次都通過（第一次的 log 標籤印成指標位址，修正標籤後重跑；數字同級：biome tick 0.029 ms、合成 0.018 ms）。
- 合成圖的格子 cell 是 2（給 1，2 倍 < 4 倍，不用回報）。

## 經過

- 測試裡直接組 arena 的地方很多：在 24 個工廠的 `return arena;` 前加 `BuildWallGrid()`，改牆的測試之後也重建。沒有牆的 arena 不必建（`WallGrid()` 回傳空格子）。
- 突變的 `find` 有 5 個因為改寫而 stale（`av2-spawn-floating`、`av2-spawn-own-rule`、`av2-movement-own-rule`、`av2-support-diverges`、`av2-geometry-left-on-failure`），照新程式改寫，重跑都 killed。
- arena 清單的兩個測試（安裝 2 個→3 個、深路徑）改成期待 biome；這是固定內容的測試，判定沒有放寬。
- B2 的隨機盒子一開始出現負的尺寸（格子的 assert 擋下），改成夾到 0。

## 證據

主 checkout 的 `build/evidence/v8-pa-broadphase/`（Intel Mac）：`b2_b3.txt`、`b4.txt`、`b5-20261107-132039/`（Match、Gateway、兩個 Client 的 log 與 trace）、`mutations.txt`／`.json`。

## 範圍的限界

- `WallGrid()` 只以盒子數偵測過期：壁數不變只改內容而忘了重建時偵測不到。正式路徑（`Load`）一定會建，測試要自己注意。
- B2 的 reference throws 0：`PlanarPenetration` 的例外路徑沒有被隨機資料碰到。
- 移動的缺陷（卡在空中、空氣牆）不在這批（`pm-detect`）。
