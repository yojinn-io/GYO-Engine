# Engine v8：v8-engine-voxel-mesh-contract（gyo.voxel_mesh 契約與讀取器）

日期：2026-11-05。分支 `claude/v8-tool-voxel-map-plan`（PR #152）。上段；實作是主會話（Opus 5.5／high），審查是子 Agent（Opus 5.5／high）。事前宣告在 [voxel-map-editor.md](../tools/plans/v8/voxel-map-editor.md)「v8-engine-voxel-mesh-contract 的事前宣告」，2026-11-05 使用者核准（「核准」）。

**狀態：** 本機判定 V1～V5 通過；V6（L1）待 CI。實作和事前宣告有幾處不同（見「和事前宣告的差異」），**等使用者確認**。

相關文件：[voxel-mesh.md](../architecture/voxel-mesh.md)（契約、Design Invariants、Replacement Criteria）、[voxel-map-editor.md](../tools/plans/v8/voxel-map-editor.md)、[HANDOFF](../tools/plans/v8/HANDOFF.md)。

## 做了什麼

- `engine/voxel_mesh/`（`GYO::VoxelMesh`）：
  - `ParseVoxelMesh`：驗證規則 1～10，傳回整數形式。
  - `ExpandVoxelMesh`：把四邊形展開成世界座標的頂點和索引；產生調色盤貼圖（列＝方向）；把碰撞盒換算成 `Math::Aabb`。
  - 連結只有 Base、Math，和 PRIVATE 的 nlohmann。
- `tests/common/voxel_mesh/`：規則測試（每條一個案例）、winding 的性質測試、兩組黃金值、configure 時的連結守衛 `gyo_expect_voxel_mesh_links`。
- `docs/tools/plans/v8/scripts/voxel_mesh_mutations.json`：20 條突變。
- `docs/architecture/voxel-mesh.md`：契約、驗證規則、Design Invariants 5 條、Replacement Criteria 4 條。`docs/architecture.md` 的模組表加一列。

## 判定（事前宣告的 V1～V6）

| 判定 | 結果 |
|---|---|
| V1 規則測試 | 通過：14 個案例、329 個斷言（審查後） |
| V2 展開的黃金值 | 通過：3 個面的頂點、索引、UV 逐位元相同；winding 和 `MakeUnitCube` 一致 |
| V3 換算的黃金值 | 通過：`cell_size` 1.0 和 0.3、負的 origin、+2^20 和 −2^20 兩端，比對字面的位元（`0x3F666667`、`0xBE99999A`、`0xC699999A`） |
| V4 連結守衛 | 通過：突變 `vm-links-collision` 在 configure 時被擋下 |
| V5 宣告的突變 | 審查前 16／16 killed；審查後 20／20 killed（見下） |
| V6 L1 | 待 CI |

## 經過

1. 照 `gyo.anim_events` 的寫法（`AnimationEvents.cpp`）實作：
   - 解析時的 callback 擋下重複的 key；
   - 用例外把錯誤收成 Runtime Error，細節是出錯值的路徑。
2. 角點的順序直接對照 Render 的 `MakeUnitCube`（`engine/render/src/PrimitiveMesh.cpp`），測試用 `cross(v1−v0, v2−v0)` 朝外檢查 6 個方向。
3. 第一次建置有兩個錯誤，都是測試檔的問題，和實作無關：
   - 自己寫的 `Cross` 和 `Engine::Math::Cross` 因為 ADL 撞名；
   - 少了 `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`。
   修好後 14／14 通過，突變 16／16 killed。
4. 宣告的突變「換算少除以 16」原本被我換成 `vm-origin-not-scaled`。理由是當時另一個候選（先除後乘）是等價突變。這個替換沒有先回報，審查指出後補回了 `vm-no-divide-by-16`。
5. 審查（Opus／high）：沒有 blocker。修正與補強如下。
   - **C1**：`cell_size` 的上限算錯，`1e33` 會在展開時產生 inf。
   - **C2**：`cell_size` 沒有下限，subnormal 時「除以 16 是精確的」不成立。
   - C1、C2 合併成「`cell_size` 在 [2^-16, 2^16] 之內」。
   - **C3**：文件寫明 `cell_size` 的取值方式（十進位 → 最近的 double → 最近的 float）。
   - **C4**：文件寫明，重複 key 和非 JSON 的細節是空路徑。
   - **T1～T6**：補測試。頂層重複 key 改成用完整文件，並比對訊息；補上重複的 collider、min > max、巢狀物件的未知欄位、−2^20 的位元，以及各規則的界線值。
   - 新增 4 條突變：`vm-no-divide-by-16`、`vm-collider-order-not-strict`、`vm-cell-size-no-floor`、`vm-repeated-key-nested-only`。
   - **A1**：拿掉「`origin×16` 本身也要在 ±2^20 之內」。這條會拒絕宣告允許的文件；現在只檢查角點，和宣告一致。
   - **A2**：`ExpandVoxelMesh` 對前提加 `GYO_ASSERT`（Assert＝Programmer Error）。
   - DI 2 的措辭改成「入口是 Parse 和 Expand（另外命名錯誤碼）」。
   - 規則 9 的措辭改成「一種幾何只有一種四邊形和盒子的順序」。

## 和事前宣告的差異（等使用者確認）

| 項目 | 宣告 | 實作 | 方向 |
|---|---|---|---|
| 規則 4 `cell_size` | 有限的正數 | 轉成 float 後在 [2^-16, 2^16] 之內 | 收緊：沒有這條，確定性（DI 5）和「展開不會失敗」都不成立 |
| 規則 8 | 角點在 ±2^20 之內 | 角點在 ±2^20 之內，**加上**每個整數在 ±2^30 之內 | 收緊：防止運算溢位；只會拒絕「原始值極大、但加上 origin 後回到範圍內」的文件 |
| 錯誤的細節 | 出錯值的路徑 | 同上；角點超出範圍時回報整個 quad 或盒子的路徑；重複 key、非 JSON 的路徑是空的 | 文件寫明 |
| `ExpandVoxelMesh` | 不會失敗 | 不會失敗；對非 Parse 產生的輸入 assert | 收緊 |
| 突變 | 至少 10 條，含「換算少除以 16」 | 20 條，含 `vm-no-divide-by-16` | 照宣告；第一輪的替換沒有先回報 |

以上都是收緊，或是補寫文件。沒有放寬任何判定（AGENTS §14.4）。

## 證據

主 checkout（macOS Intel）的 `build/evidence/tool-v8-voxel-map/engine-contract/`：

- `configure.txt`
- `build.txt`、`tests.txt`、`mutations.txt`（審查前）
- `build-after-review.txt`、`tests-after-review.txt`、`mutations-after-review.txt`（審查後）

審查者的探測程式放在會話的暫存區，未存證；判定沒有用到它。

## 用時

約 4 h（估 8～12 h）。
