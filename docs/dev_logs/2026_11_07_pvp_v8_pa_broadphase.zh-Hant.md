# PvP v8：v8-product-pa-broadphase（牆的查詢改用 StaticAabbGrid，撤掉 1024 上限）

日期：2026-11-07。分支 `claude/pvp-v8-pa-broadphase`。主會話 Opus 5.5／high；審查者 Opus（engine-grid 會話）；驗證者 Fable／xhigh（general-purpose 子 agent，唯讀）。估 6～10 h。

**狀態：B1～B6、審查、驗證完成；B7（L1）未跑。** [事前宣告](../object_fps_pvp/plans/v8/v8-product-pa-broadphase-declaration.md)使用者 2026-11-07 照推薦核准（上限的 assert 採 (b)；biome 可進 repo）。

## 為什麼

使用者要在 11/9 的 lag-l2＋K8 場次用大地圖 biome（2～3 人、現場沒有網路）。Match 以 `walls.size() > 1024` 拒收 biome（1,260 盒），錯誤訊息只說 spawns。順序改成 pa-broadphase → 場次 → hitbox-rewind → pm-detect（使用者 2026-11-07；架構師已記）。

## 做了什麼

- `CharacterCollision` 的三個函式改收 `StaticAabbGrid`：
  - `CanPlaceCharacterBody`、`IsCharacterSupported` 用 `OverlapVerticalCapsuleAll`／`SweepVerticalCapsuleAll`（只看有沒有）；
  - `MoveCharacterBody` 的穿透解除與掃描滑動都用 All，依索引遞增套用原本的邏輯（掃描的過濾在比較之前，所以不用 Nearest）。
- `ShotQuery` 的牆用 `RaycastNearest(…, HighestIndex)`，再與地板用 `<=` 比。
- `Arena` 加 `wallGrid`（`shared_ptr<const StaticAabbGrid>`）、`BuildWallGrid()`、`WallGrid()`。`Load` 在 `Validate` 之後建。`walls` 仍是權威（digest、wire、渲染）。digest 的結構化綁定加了第 14 個名字（`wallGrid`，不進 digest）。
- 上限：`MaximumArenaColliders = 2^20`；超過時 `Arena has N colliders; at most 1048576`。Engine 的上限（登記數 < 2^32 等）用架構師的 [#166](https://github.com/yojinn-io/GYO-Engine/pull/166)（`StaticAabbGrid::Build`，使用者 2026-11-07 合併）檢查，回錯誤 `Arena colliders do not fit the collision grid: …`，不再走建構子的 assert。`Validate` 與 `Load` 共用 `CheckedWallGrid`，`Load` 直接留下檢查時建的格子（一次載入只建一次）。
- Match 啟動 log 加一行 `wall grid colliders=… cell=… given=…`（collision.md 的 Replacement Criteria 5）。
- biome 進 repo（`assets/object_fps_pvp/maps/biome.json`、`biome.voxel_mesh.json`，`arenas.json` 第 3 個）。來源：開源的 MagicaVoxel 地圖，使用者的個人測試用（使用者 2026-11-07），由 voxel 線轉換與烘焙。
- 測試：`WallGridEquivalenceTests.cpp`（B2 交叉、B3 上限、B4 計時，B4 在 CI 跳過）。

## 判定

| 判定 | 結果 |
|---|---|
| B1 既有測試 | 通過：`cpu\|shader` 138／138（含 AuthorityDigest 的 golden） |
| B2 等價 | 通過：40,000 個查詢（200 組 × 200），比較數＝產生數，不一致 0 |
| B3 上限 | 通過：1,025 與 2^20 接受、2^20＋1 拒絕（訊息含 collider 數）、登記數超過 2^32 的地圖回 `TooManyRegistrations` 錯誤而不是 assert |
| B4 tick 成本 | 通過（Intel Mac i9-9980HK、RelWithDebInfo）：biome p99 一步 0.0067 ms、一發 0.0047 ms、一 tick 0.032 ms；合成 10^5 盒 0.0015／0.0057／0.016 ms（閾值 0.4／0.2／2 ms） |
| B5 biome 能玩 | 通過：Match `arena=biome_trial` ready，Client 2 個加入（使用者試玩），45 個 decision，`trace_end` records 45、dropped 0 |
| B6 突變 | 通過：第一輪 15／15；換成 `Build` 後重跑 Arena.cpp 的全部與新突變 20／20 killed |
| B7 L1 | 未跑 |

- B4 跑了兩次，兩次都通過（第一次的 log 標籤印成指標位址，修正標籤後重跑；數字同級：biome tick 0.029 ms、合成 0.018 ms）。
- 合成圖的格子 cell 是 2（給 1，2 倍 < 4 倍，不用回報）。

## 經過

- 測試裡直接組 arena 的地方很多：在 24 個工廠的 `return arena;` 前加 `BuildWallGrid()`，改牆的測試之後也重建。沒有牆的 arena 不必建（`WallGrid()` 回傳空格子）。
- 突變的 `find` 有 5 個因為改寫而 stale（`av2-spawn-floating`、`av2-spawn-own-rule`、`av2-movement-own-rule`、`av2-support-diverges`、`av2-geometry-left-on-failure`），照新程式改寫，重跑都 killed。
- arena 清單的兩個測試（安裝 2 個→3 個、深路徑）改成期待 biome；這是固定內容的測試，判定沒有放寬。
- B2 的隨機盒子一開始出現負的尺寸（格子的 assert 擋下），改成夾到 0。

- 審查（engine-grid 會話，Opus）：無 blocker。should：暫時上界 `RegistrationsFit` 用 float 算邊長，格長不是 2 的冪時每軸可能少算 1 格（實例：cell 0.3，算出 58、實際 59），不是嚴格保守。處理：#166 合併後整個換成 `Build`，上限的計算只留在 Engine。nit：`WallGrid()` 與 ShotQuery 的註解（已改）、B2 加半徑 0.3（已加，重跑 0 不一致）、`Load` 建兩次格子（隨 `Build` 改成一次）。另記：沒有牆的 arena 收到 NaN 等不合法輸入時，格子的查詢會 assert，舊的線性掃不會（有牆時兩者都 assert）。
- 驗證（Fable／xhigh，子 agent）：三項都成立。B2 的種子另跑 3 個，共 160,000 個查詢 0 不一致；資料確實到達 tie（掃描 524、其中法線不同 108；穿透 538、其中 44）與過濾（22 件能分出「先取最近再過濾」）。缺口：「牆的距離＝range 且一開始是 Miss」0 件、`PlanarPenetration` 的例外 0 件，兩者只靠論證。
- B3 的「過大」例子第一次寫成一個大盒子：Engine 會把 cell 加倍到格子數不超過 2^22，所以登記數只有數百萬，`Build` 接受了。改成 161^3 格（不加倍）上 1,100 個蓋滿全圖的盒子（約 4.6e9 登記）。
- `mac.sh` 加 `ARENA_FILE`（預設 `pvp_corners.json`），在本機建置上乾跑：`ARENA_FILE=biome.json` 的 Match 啟動、印出 `wall grid colliders=1260 cell=1 given=1`、Ctrl+C 寫出 `trace_end`。
- lag-l2 宣告修訂 8 與場次手冊照使用者的決定改成 biome、2～3 人。

- 換成 `Build` 後的全量 `cpu|shader`：137／138，`object_fps_pvp.ipc` 的「PvP IPC carries the animation authority of every player」一次失敗（`move_weight_permille` 666，期待 500：讀到的 snapshot 多走了 1 tick）。單獨重跑 5／5 通過。切分（§14.1）：值是移動意圖的轉場步數，與牆的查詢無關；失敗時 6 個測試並行、負載平均約 7。判為測試的時間假設在高負載下的偶發（測試本身），未修改；不是本批造成的證據是單跑通過與等價的 B2，但沒有在 master 上同負載重現。

## 證據

主 checkout 的 `build/evidence/v8-pa-broadphase/`（Intel Mac）：`b2_b3.txt`、`b4.txt`、`b5-20261107-132039/`（Match、Gateway、兩個 Client 的 log 與 trace）、`mutations.txt`／`.json`、`mutations_r2.txt`／`.json`、`b2_b3_r2.txt`、`verify/`（驗證者的 probe 與輸出）、`dryrun-mac/`。審查者的搜尋與實例在主 checkout 的 `build/evidence/object_fps_pvp/pa-broadphase/review/`。

## 範圍的限界

- `WallGrid()` 只以盒子數偵測過期：壁數不變只改內容而忘了重建時偵測不到。正式路徑（`Load`）一定會建，測試要自己注意。
- B2 沒有到達「牆的距離＝range 且一開始是 Miss」與 `PlanarPenetration` 的例外（後者要約 0.25·2^64 m 的盒子），兩者只靠論證。
- 移動的缺陷（卡在空中、空氣牆）不在這批（`pm-detect`）。
