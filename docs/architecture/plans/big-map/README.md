# 大地圖（區塊化地圖）計畫（`v8-arch-big-map`）

更新：2026-11-06。Owner：架構（Engine 契約的判斷）；各批的實作 owner 見下表。卡片由架構師會話（「[Job][架構師]例行架構掃描」）開出。
**狀態：survey 完成；六個未決事項 2026-11-06 照建議定案（[HANDOFF](HANDOFF.md)「決定」）。下一批 engine-map 由架構師會話開。** 先讀本頁，再讀 [交接](HANDOFF.md)。

## 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-big-map` | 整條線：讓一張地圖從「一份文件」變成「可用座標定址的區塊集合」，撐得住塔科夫 raid 規模（約 1 km²、cell 0.5 m、4 人房） |
| `v8-arch-big-map-survey` | 本批：用實際讀到的來源與量測，檢驗方案 v0.1；產出「採る／改装／採らない」表、六個未決事項的建議、三項量測 |
| `v8-arch-big-map-engine-map` | 之後：Engine 的區塊地圖格式（索引＋region）、標記層、靜態碰撞 broadphase、解析加速 |
| `v8-arch-big-map-tool-chunked` | 之後：voxel 編輯器改成只重烘有修改的區塊、greedy 合併、region 存檔、標記編輯 |
| `v8-arch-big-map-product-arena-v3` | 之後：產品的 arena v3、broadphase、撤 1024 上限、Client 依 region 畫與剔除、兩張既有地圖遷移 |

## 文件

- [方案 v0.1（假說）](proposal-v0.1.zh-Hant.md)：架構師與使用者 2026-11-06 的討論稿＋方案；survey 以它為假說去檢驗。
- [調查（survey）](survey.zh-Hant.md)：九項參照、六個未決事項、三項量測、對抗檢查。
- [交接（HANDOFF）](HANDOFF.md)：記錄器。

## 批次一覽（照 proposal §11.11）

```text
v8-arch-big-map
├── -survey            high｜調查者 Opus，對抗檢查者 Fable／xhigh（先問）｜以來源與量測檢驗 v0.1；停下交使用者定案
├── -engine-map        high（上段）｜Opus／Opus／Fable｜VoxelMesh v2、MapMarkers、StaticAabbGrid、解析加速
├── -tool-chunked      voxel 線 owner｜髒區塊重烘、greedy、區塊內碰撞合併、region 存檔、markers 編輯；之後接 window 批
└── -product-arena-v3  產品會話 owner｜arena v3、markers 賦義、broadphase、撤 1024、Client region mesh＋剔除、遷移
```

| 批次 | 段 | 模型／ギア | Owner | 狀態 |
|---|---|---|---|---|
| `-survey` | 上段（Architecture 判斷的根據） | 調查者 Opus／high；對抗檢查者 Fable／xhigh（使用者已同意） | 架構 | 完成；六項照建議定案 |
| `-engine-map` | 上段 | 未開；survey 定案後由架構師開卡 | Engine | 未開 |
| `-tool-chunked` | 未定 | 未開 | voxel 地圖編輯器線 | 未開 |
| `-product-arena-v3` | 未定 | 未開 | 產品（object_fps_pvp） | 未開 |

survey 之後的三批，範圍以 HANDOFF「決定」的六項為準（與 proposal v0.1 不一致處以決定為準）；樹狀圖只記錄 v0.1 的路線。

## 本批不做

- 不開 engine-map／tool／product 批；不改 Engine、工具、產品的程式碼；不改 PR #152 的文件；不碰 framework、Gateway、pv10。
- 不把 proposal 的估算當結論；標 ⚠ 的數字沒量到就寫「未量」。

## dev_log

- [2026-11-06 `v8-arch-big-map-survey`](../../../dev_logs/2026_11_06_arch_v8_big_map_survey.zh-Hant.md)
