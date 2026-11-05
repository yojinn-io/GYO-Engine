# 架構 v8：v8-arch-big-map-engine-grid（靜態 broadphase 與解析替換）

日期：2026-11-06。分支 `claude/v8-arch-big-map-engine-grid`（切自 master `d54b60ef`）。卡片由架構師會話開出（原為 `v8-arch-big-map-engine-map`，途中拆批）。
段：上段（Engine 公開介面、DI）。實作者：主會話。開工時是 Fable 5.1，讀前置文件途中由使用者切成 Opus 5.5，實作都在 Opus 5.5／high 下做。
審查者：engine-grid 會話（Opus，與實作者不同會話；架構師指定）。

**狀態：** 實作與本機驗證完成，PR 1 待審查與 L1。宣告：[engine-grid](../architecture/plans/big-map/engine-grid.zh-Hant.md)，由使用者 2026-11-06 核准的 [engine-map 宣告](../architecture/plans/big-map/engine-map.zh-Hant.md)拆出。

相關文件：[計畫 README](../architecture/plans/big-map/README.md)、[HANDOFF](../architecture/plans/big-map/HANDOFF.md)（「拆批與撞期」「現況：engine-grid」）、
[collision 契約](../architecture/collision.md)、[voxel-mesh 契約](../architecture/voxel-mesh.md)。

## 做了什麼

- `GYO::Collision` 加 `StaticAabbGrid`（`engine/collision/StaticAabbGrid.hpp`）。契約是等價：每個查詢的結果，和「依索引順序線性掃描全部盒子」的結果位元相同。平手規則由呼叫端用 `TieBreak` 選。
  查詢有五個：`Overlapping`、`RaycastNearest`、`SweepVerticalCapsuleNearest`、`SweepVerticalCapsuleAll`、`OverlapVerticalCapsuleAll`。
  契約文件 `docs/architecture/collision.md` 是新建的（DI 3 條、RC 3 條）。
- `GYO::Base` 加 `StrictJson.hpp`：只依賴標準函式庫的模板，先用 SAX 掃一次查重複鍵，再用不帶回呼的 `parse` 建 DOM。`ParseVoxelMesh` 改用它，規則程式、錯誤碼與訊息都不變。
- 依賴守衛：新增 `gyo_expect_collision_links`（Collision 只連 Math、Base）。
- 文件：`docs/architecture.md` 的目錄樹、模組表與 Model／collision 段加上 broadphase 的歸屬；`docs/architecture/voxel-mesh.md` 的 Cost 一條加上解析方式與量測結果。
- 突變 catalog `docs/architecture/plans/big-map/scripts/engine_grid_mutations.json`，共 16 個。另外把 `voxel_mesh_mutations.json` 兩個突變改指向新位置（見下面的 §14.4）。
- 產品批的對照表放在宣告 2.1 節：Movement、ShotQuery、CharacterCollision 逐行對到 API。

## 經過

1. 照 engine-map 宣告實作了全部四件：格式、MapMarkers、broadphase、解析替換。提交是 `447f9194`、`c2287961`、`49449a12`，都在本機分支 `claude/v8-arch-big-map-engine-map`。
2. 拆批訊息到達時，四件都已經提交。同一時段，engine-grid 會話也寫了一份 engine-grid 宣告，使用者在那邊核准了（`25026d7d`）。
   使用者在本會話選「當第一批基礎」，在架構師會話選「用現有分支，拆成兩個 PR」。
   所以 PR 1 從 master 重組，只拿 broadphase、StrictJson 與 `ParseVoxelMesh` 那一處；`447f9194` 裡的 v2 格式不帶進來。時序與兩份宣告的差異寫在 HANDOFF。
3. 途中一度打算改用 engine-grid 會話的程式碼，因為它和那份已核准的宣告一致。架構師轉達使用者的選擇後取消，那邊的工作樹沒有複製。
4. 規則測試在實作之後才寫，Red 的證據是突變（J2）。
5. 測試本身錯了兩次，都是測試的錯，改的是測試，實作沒動：
   - 一次是 `insert` 的大括號被當成兩個盒子的 initializer_list。
   - 一次是預期接觸數寫錯：膠囊走到 x = 4.25，碰不到 x = 5 的盒子。
6. 為了回應審查者會拿來對照的 `25026d7d`，加了兩項：座標 ±1e6 的邊界案例（6,300 次查詢，不一致 0），以及突變 `grid-capsule-start-only`。

## 受理判定

件數の突き合わせ：一致。各項如下：

| 判定 | 要求 | 完成 | 失敗或不一致 | 其他 |
|---|---|---|---|---|
| J4 隨機集合 | 70,000 | 70,000 | 0 | |
| J4 邊界案例 | 2,088 | 2,088 | 0 | |
| J4 遠座標 | 6,300 | 6,300 | 0 | |
| J5 解析 | 5 | 5 | 0 | quads 124,236、colliders 20,691 |

- **J4 broadphase 等價：成立。** 隨機 200 組，種子固定，用自寫的 splitmix64，所以每個平台產生的集合相同。每組 1～300 盒，範圍 ±4～±23 m，有密有疏。
  集合裡有重複盒、零厚度、跨全部格子的地板。查詢有四類：射線（平行軸、`maximumDistance` 0 到 1e6）、膠囊、位移（含 0 與站立探測的 −0.002）、AABB。
  每個查詢都和測試內照契約寫的線性掃描比對 optional 的有無、索引、Contact 四個欄位的位元。
- **J5 解析時間：成立。** 判定試行在拆批前，量的是 engine-map 分支的程式碼；`ParseVoxelMesh` 的路徑在兩個分支上只差 v2 新增的部分。判定不放寬。

  | 量測 | 中位數 | 最小～最大 | 備註 |
  |---|---|---|---|
  | 替換前（master `d54b60ef`） | 1,579.7 ms | | |
  | 替換後（判定試行） | 382.2 ms | 363.4～490.5 ms | 門檻 450 ms |
  | PR 1 程式碼（確認，不是判定） | 364.0 ms | | |

  三次的展開結果 SHA-256 都是 `678a28c2…e299`，表示契約沒變。
- **J2 突變：** `engine_grid_mutations.json` 最後一輪 15/16 被殺（`mutations_engine_grid_final.txt`），存活的是 `grid-margin-zero`，見下。`voxel_mesh_mutations.json` 在 PR 1 分支上 20/20 被殺。
  三個格子邊界突變（`grid-count-short`、`grid-first-cell-dropped`、`grid-last-cell-dropped`）是被建構時的斷言 `GYO_ASSERT failed: !ranges[i].empty` 抓到，所以 expect 也接受這段訊息。拆批前那一輪只認比對失敗，所以標成 wrong_failure。
- **J6 迴歸：** 本機全量建置成功；`ctest -L "cpu|shader"` 130/130、`ctest -L gpu` 4/4 通過。L1 三平台待 PR。
- **J7 依賴守衛：** `vm-links-collision` 與 `grid-links-voxel-mesh` 都以 configure 失敗被殺。

### 存活的突變：`grid-margin-zero`（等價突變，推定）

這個突變把候選範圍的放大邊際歸零，結果存活。我推定它是等價突變，理由：

- 候選範圍是膠囊或射線段的外包盒，用 double 精確算出；精確查詢自己也用 double 計算。
- 1e-7 的容許值只會「減少」接觸：淺的初始重疊若分離或切向移動，就不算阻擋。
- 所以精確查詢回報接觸的盒子，幾何上必然和外包盒相交（閉區間）。格子的格索引是單調函式，`Overlapping` 也不需要邊際。

邊際是防禦：以後若改了精確查詢的容許方向，它還能保住等價。這個突變留在 catalog 裡，判定不刪。審查時若找到會讓它被殺的輸入，就把那個輸入加成測試。

### §14.4：兩個突變改指向（DI 3／DI 4 的守衛）

`docs/tools/plans/v8/scripts/voxel_mesh_mutations.json` 的 `vm-repeated-key-kept` 和 `vm-repeated-key-nested-only`，原本的 `find` 指向回呼程式碼那一行。
那一行在解析替換後不存在，所以把 `file`／`find`／`replace` 改指向 `StrictJson.hpp` 裡同一個判斷。`id`、`what`、`target`、`cases`、`expect` 都沒改。

1. **守的性質：** 任何物件裡的重複鍵都是錯誤，而且不能只查巢狀物件（DI 4）。
2. **緩める面：** 沒有。測試、期待訊息、判定都沒變。
3. **締める面：** 沒有。
4. 不適用（沒有緩める面）。
5. **根據：** 改指向後，兩個突變在 PR 1 分支上都以同一個測試（`*rule 1:*`）、同一個期待訊息被殺，證據是 `mutations_voxel_mesh_v1.txt`。
   另外新增的 `sj-repeated-key-kept`、`sj-scan-skipped`、`sj-repeat-message-lost` 也都被殺。

engine-map 宣告的 J2 已寫明「`find` 失效的，改成新實作中的同一個判斷」，這一點使用者在核准宣告時已同意。

## 探索（未宣告、不判定）

同一陣列裡放 N 個碰撞盒的 `gyo.voxel_mesh` 文件，每個 N 量 5 次取中位數：

| N | 替換前 | 替換後 |
|---|---|---|
| 1 萬 | 345 ms | 42.5 ms |
| 2 萬 | 1,139 ms | 85.4 ms |
| 4 萬 | 4,341 ms | 181.1 ms |
| 8 萬 | 17,081 ms | 384.9 ms |

替換前每加倍約 4 倍（二次方），替換後約 2 倍（線性）。8 萬除以 1 萬的比值：替換前約 49.5，替換後約 9.1。
engine-grid 會話宣告的 P-2 門檻是 12；本宣告沒有事前宣告，所以這裡只當描述。

## 與宣告不同之處

- engine-grid 宣告的 J4 寫「盒數 1～300」。engine-map 宣告原寫 1～400；實作一開始就是 300，宣告照實際寫。
- 突變在宣告後新增兩個，都是加嚴：`grid-capsule-start-only`、`sj-scan-skipped`。只記錄，不刪。

## 發現的 Code Smell

- `GYO::Model` 的 `AnimationEvents.cpp:40` 也用回呼查重複鍵，有同樣的二次方。它的檔案小、陣列短，本批不改。之後可以改用 `StrictJson.hpp`。

## 證據

主 checkout，git 不追蹤：

- `build/evidence/big-map/engine-map/`
  - 量測程式：`parse_time.cpp`、`build.sh`。
  - J5：`parse_time_before.txt`、`parse_time_after.txt`，以及 PR 1 確認的 `parse_time_engine_grid.txt`。
  - 探索：`scaling.txt`、`scaling/`。
  - 拆批前的突變結果。
- `build/evidence/big-map/engine-grid/`
  - PR 1 分支的突變：`mutations_engine_grid.txt`、`mutations_engine_grid_final.txt`、`mutations_voxel_mesh_v1.txt`。
  - 建置與測試：`build_all.txt`、`ctest_cpu_shader.txt`、`ctest_gpu.txt`。
