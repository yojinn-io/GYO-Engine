# 大地圖線的代辦：功能、歸屬批次、觸發條件

更新：2026-11-06。使用者在 engine-map 批的 side chat（「現在 Map 有分區載入的功能嗎?」）要求把下面三個地圖功能記進 v8 代辦。
這一頁只整理**現況、歸屬與觸發條件**，不是新決定；決定在 [HANDOFF](HANDOFF.md)「決定」，根據在 [survey](survey.zh-Hant.md)。
engine-map 批 2026-11-06 拆成 `-engine-grid`（broadphase＋解析）與 `-engine-format`（格式＋MapMarkers）兩批；下表的歸屬以拆後為準。

| # | 功能 | 現況（master） | 歸屬批次 | 觸發條件／前提 |
|---|---|---|---|---|
| 1 | **分區載入**（依位置只載入地圖的一部分、背景串流） | 沒有。arena v2 整份讀 `gyo.voxel_mesh` v1。`-engine-format` 只做「分檔」（`gyo.voxel_map` 索引＋`gyo.voxel_region`；Engine 只解析、不開檔） | Client 背景串流：`-product-arena-v3` 之後，另立批；Match 活動範圍：產品線，另立 Feature | 已定案 `v8-arch-big-map-sim-model`（HANDOFF）：Match 整張載入、沒有「已載入集合」；Client 全部 region 載入後才 ready。**觸發**＝survey §6 的 Replacement Criteria：Client 載入時間超過宣告值或記憶體不夠 → 背景串流；Match 載入時間或記憶體超過宣告值 → 活動範圍（採用時先寫「未載入區塊的碰撞」規則，且載入集合要由世界決定性地導出）。量測與宣告在 `-product-arena-v3` |
| 2 | **局部重建**（編輯器只重烘有修改的區塊） | 沒有。編輯器每改一格整張重烘＋文字往返（explore-p1 每幀約 2.7 s） | `-tool-chunked`（voxel 地圖編輯器線 owner） | 前提＝`-engine-format` 的 `ValidateVoxelChunk`／`ExpandVoxelChunk` 合併。內容：髒區塊＋鄰接區塊重烘、區塊內碰撞盒合併、只存髒 region＋索引、greedy 合併（`v8-arch-big-map-greedy`）。工具 DI 3 黃金值與 DI 4 守衛的判定變更照 AGENTS §14.4 取得使用者核准。window 批（PR #158）接在其後 |
| 3 | **依視野／距離剔除**（culling） | 烘焙時的隱藏面剔除（工具 `Build.cpp` cullface）與 GPU 背面剔除（Render `CullMode::Back` 預設）已有；產品沒有視錐剔除 | `-product-arena-v3`（產品會話 owner） | 內容：Client 每個 region 合成一個 GPU mesh，以 region AABB 做視錐與距離剔除；Engine 不知道渲染單位（`-engine-format` 的 DI 增補）。LOD 不做（`v8-arch-big-map-lod`）；**觸發**＝最壞角度的畫面 p95 超過 `-product-arena-v3` 宣告值 → 工具烘 region 低精度代理（RC），以另一個資產掛上、不改 region 格式 |

## 怎麼用這一頁

- 開 `-tool-chunked`、`-product-arena-v3` 的卡時，把對應列的內容寫進事前宣告；量到觸發條件成立時，把該列改成「已觸發，見 <批次>」。
- 列的增刪是記錄，不是決定；新的決定仍寫 HANDOFF「決定」。
