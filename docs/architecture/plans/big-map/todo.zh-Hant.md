# 大地圖線的代辦：功能、歸屬批次、觸發條件

更新：2026-11-07（加 4～8；試玩回饋與 `v8-arch-map-scene-split`）。初版 2026-11-06。使用者在 engine-map 批的 side chat（「現在 Map 有分區載入的功能嗎?」）要求把下面三個地圖功能記進 v8 代辦。
這一頁只整理**現況、歸屬與觸發條件**，不是新決定；決定在 [HANDOFF](HANDOFF.md)「決定」，根據在 [survey](survey.zh-Hant.md)。
engine-map 批 2026-11-06 拆成 `-engine-grid`（broadphase＋解析）與 `-engine-format`（格式＋MapMarkers）兩批；下表的歸屬以拆後為準。

| # | 功能 | 現況（master） | 歸屬批次 | 觸發條件／前提 |
|---|---|---|---|---|
| 1 | **分區載入**（依位置只載入地圖的一部分、背景串流） | 沒有。arena v2 整份讀 `gyo.voxel_mesh` v1。`-engine-format` 只做「分檔」（`gyo.voxel_map` 索引＋`gyo.voxel_region`；Engine 只解析、不開檔） | Client 背景串流：`-product-arena-v3` 之後，另立批；Match 活動範圍：產品線，另立 Feature | 已定案 `v8-arch-big-map-sim-model`（HANDOFF）：Match 整張載入、沒有「已載入集合」；Client 全部 region 載入後才 ready。**觸發**＝survey §6 的 Replacement Criteria：Client 載入時間超過宣告值或記憶體不夠 → 背景串流；Match 載入時間或記憶體超過宣告值 → 活動範圍（採用時先寫「未載入區塊的碰撞」規則，且載入集合要由世界決定性地導出）。量測與宣告在 `-product-arena-v3` |
| 2 | **局部重建**（編輯器只重烘有修改的區塊） | 沒有。編輯器每改一格整張重烘＋文字往返（explore-p1 每幀約 2.7 s） | `-tool-chunked`（voxel 地圖編輯器線 owner） | 前提＝`-engine-format` 的 `ValidateVoxelChunk`／`ExpandVoxelChunk` 合併。內容：髒區塊＋鄰接區塊重烘、區塊內碰撞盒合併、只存髒 region＋索引、greedy 合併（`v8-arch-big-map-greedy`）。工具 DI 3 黃金值與 DI 4 守衛的判定變更照 AGENTS §14.4 取得使用者核准。window 批（PR #158）接在其後 |
| 3 | **依視野／距離剔除**（culling） | 烘焙時的隱藏面剔除（工具 `Build.cpp` cullface）與 GPU 背面剔除（Render `CullMode::Back` 預設）已有；產品沒有視錐剔除 | `-product-arena-v3`（產品會話 owner） | 內容：Client 每個 region 合成一個 GPU mesh，以 region AABB 做視錐與距離剔除；Engine 不知道渲染單位（`-engine-format` 的 DI 增補）。LOD 不做（`v8-arch-big-map-lod`）；**觸發**＝最壞角度的畫面 p95 超過 `-product-arena-v3` 宣告值 → 工具烘 region 低精度代理（RC），以另一個資產掛上、不改 region 格式 |

| 4 | **voxel 外觀的光照／AO／法線** | 沒有。`ArenaMesh.cpp` 只拷位置與 UV；唯一明暗是烘焙時依面向寫死的亮度（頂 100%、側 65～80%、底 50%） | render 線（另立） | 使用者 2026-11-07 決定「先接受現狀」。**觸發**＝使用者定美術方向（保留 voxel 風加光照，或平滑表面＝VoxelMesh RC 2、新契約） |
| 5 | **vox 匯入升成工具命令** | 一次性腳本 `vox_to_grid.py`（證據夾），已第二次使用 | voxel 線 | AGENTS §5 手動→Script→Tool 的壓力已出現。內容：MagicaVoxel `.vox` → `gyo.voxel_grid`，帶比例參數（參照門高 2 m＝4 格，`v8-arch-big-map-scale`）。**觸發**＝下一張用 vox 做的地圖 |
| 6 | **build 寫單行 JSON** | region 檔 38% 是縮排空白（biome：404 KB → 約 252 KB） | voxel 線 | 契約不變（任何空白都合法）。**觸發**＝使用者提出檔案大小（2026-11-07 已提出）；排進 voxel 線的下一批即可 |
| 7 | **水方塊** | 沒有。方塊只有 cube／slab／stair，palette 沒有 alpha，quads 只有面與色 | 外觀與烘焙：voxel 線（block 種類、水面只畫頂面與相鄰非水側面、無碰撞盒）；行為：產品（進水移動、浮力、視覺） | 契約影響先查：「這一格是水」要給產品的話，要嘛 Engine 契約加版本（palette alpha 或 quad 的材質類別），要嘛以場景的 markers box 表示水體；觸發時決定。**觸發**＝第一張需要水的地圖開工；之前不改契約 |
| 8 | **markers 核心搬到場景編輯器** | tool-chunked 把 markers 讀寫核心與 `--marker-kinds` 放進 voxel_map_editor（當時照 proposal） | 場景編輯器線（新工具，未開） | `v8-arch-map-scene-split`：voxel 編輯器只編幾何；不急著拆。**觸發**＝場景編輯器開線時一次搬過去 |

## 怎麼用這一頁

- 開 `-tool-chunked`、`-product-arena-v3` 的卡時，把對應列的內容寫進事前宣告；量到觸發條件成立時，把該列改成「已觸發，見 <批次>」。
- 列的增刪是記錄，不是決定；新的決定仍寫 HANDOFF「決定」。
