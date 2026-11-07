# 架構 v8：v8-arch-collision-grid-build（`StaticAabbGrid::Build` 回 Result）

日期：2026-11-07。Owner：Engine（`GYO::Collision`），架構師會話做的小批。計畫：[big-map HANDOFF](../architecture/plans/big-map/HANDOFF.md)「現況：collision-grid-build」。
契約：[collision.md](../architecture/collision.md)「Validation: `Build`」與 Design Invariant 3。

## 起因

產品線 `pa-broadphase` 的事前宣告 §1「上限」（使用者 2026-11-07 核准做法 (b)）：`StaticAabbGrid` 的前提條件（cell length、箱數、`IsValid`、登記數 < 2^32）
只有 `GYO_ASSERT`，地圖檔能觸發，產品不能讓壞地圖變成 assert 當機；產品自己重算分格規則會重複 Engine 的知識（AGENTS §3 狀態的權威）。
11/9（週一）的 lag-l2＋K8 場次要用 biome 地圖，所以要在週日前合併。

## 做了什麼

- `engine/collision/include/engine/collision/StaticAabbGrid.hpp`：`CollisionErrorCode`（`InvalidCellLength`、`TooManyBoxes`、`InvalidBox`、`TooManyRegistrations`）、
  `CollisionError = Base::Error<CollisionErrorCode>`、`static Base::Result<StaticAabbGrid, CollisionError> Build(boxes, cellLength)`；建構子保留（斷言版）。
- `engine/collision/src/StaticAabbGrid.cpp`：建構子本體移成私有 `Initialize()`，建構子 `GYO_ASSERT(!failure)`、`Build` 回 `Base::Err`；一份實作、兩個入口。
  登記數改成在配置任何東西之前，由每個箱子的 cell range 以 64 bit 加總（原本是數完每格的 counts 才 assert），所以巨大的輸入在讀完箱子的時間內就被拒絕。
- `engine/collision/CMakeLists.txt`：`GYO::Base` 由 PRIVATE 改 PUBLIC（`Result` 在公開標頭）。守衛 `gyo_expect_links` 比的是連結集合，不分 PUBLIC／PRIVATE，不必改。
- 測試 `tests/common/collision/StaticAabbGridTests.cpp`：新 case「grid build」：成功時與建構子的結果相同（Size、CellLength、Overlapping、RaycastNearest 與線性掃）；
  cell length 0／−1／NaN／inf → `InvalidCellLength`；倒置的箱子 → `InvalidBox`，detail 是它的索引、`Describe` 的字串；1,049 個各覆蓋 4,096,000 格的箱子 → `TooManyRegistrations`，detail＝4,296,704,000。
  `TooManyBoxes`（2^32 − 1 個箱子）無法以測試觸發，只有程式與審查。「剛好低於 2^32 被接受」也不測：接受就要配置 16 GiB 的 items。
- 突變（`docs/architecture/plans/big-map/scripts/engine_grid_mutations.json`，16 → 19 條）：`grid-build-cell-length-unchecked`、`grid-build-invalid-box-unchecked`、
  `grid-build-registrations-unchecked`；`grid-links-voxel-mesh` 的 find／replace 改成新的連結行。

## Architecture Delta（AGENTS §3）

1. **哪個 Feature 需要**：產品 `pa-broadphase`（撤 1024 上限、改用 broadphase；11/9 場次用 biome）。
2. **現在的結構有什麼問題**：前提條件只能 assert；內容驗證要嘛在產品重複 Engine 的分格知識，要嘛讓壞地圖當機。
3. **哪個邊界變化**：`GYO::Collision` 的公開介面（追加 `Build`、錯誤碼）；`collision.md` DI 3 的字句（Base 從 private 變 public）。
4. **影響的 Module／Product**：Engine `GYO::Collision`；消費端 object_fps_pvp（pa-broadphase 之後）。
5. **Dependency Direction**：不變（Collision → Math、Base）；Base 從 PRIVATE 變 PUBLIC，Base 是純標頭、只依賴 std。
6. **Ownership**：不變。驗證規則仍只在 Engine（一份實作）。產品的 2^20 預算是產品的規則，在 `Build` 之前自己檢查。
7. **為什麼不能更小**：`std::optional<Rejection>` 可以不改 DI 3，但偏離 error-handling.md 規則 2（資料驗證 API 回 Result）；使用者 2026-11-07 選 Result 形式＋改 DI 3。

## 受理判定（本機 Intel Mac，test preset，`-O2 -g -DNDEBUG -ffp-contract=off`）

- `gyo_collision_tests`：30/30（751 assertions）通過；既有的等價 case 全部不變。
- 相關突變（D184：新加的 3 條＋find 行改動的 `grid-links-voxel-mesh`）：**4/4 被殺**（`mutations_build_rerun.txt`）。
  第一輪 2/4：`grid-build-cell-length-unchecked` 原本把整個檢查拿掉，cell length 0 讓格長加倍的迴圈永不結束，只會 timeout（不算被殺）→ 改成只拿掉 `isfinite`，以 inf 殺；
  `grid-build-registrations-unchecked` 原本的 expect 只接受 doctest 的「is NOT correct」，但不查登記數時 32 bit 的 offsets 回繞、fill 寫出界，測試 **SIGSEGV**（doctest「test case CRASHED」）→
  expect 改成接受「test case CRASHED」。兩者都是突變定義的修正，不是測試或實作的修正；第一輪結果保留在 `mutations_build.txt`。
- `engine_grid_mutations.json` 全目錄（19 條；建構子本體搬進 `Initialize`，舊突變的 find 都在改動的檔案裡）：見下「全目錄重跑」。
- 全量建置與 L1：由 PR 的 L1 三平台給（CI 是主要標準）。

### 全目錄重跑

`engine_grid_mutations.json` 19/19 被殺（**件數の突き合わせ：一致**——要求 19、結果 19：被殺 19、存活 0、過期 0）；`mutations_engine_grid_full.txt`。

## 證據

主 checkout `build/evidence/big-map/collision-build/`（git 不追蹤）：`mutations_build.txt`（第一輪 2/4）、`mutations_build_rerun.txt`（4/4）、
`mutations_engine_grid_full.txt`（全目錄）、`ctest_collision.txt`。
