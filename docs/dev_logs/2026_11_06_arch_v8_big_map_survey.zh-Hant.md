# 架構 v8：v8-arch-big-map-survey（大地圖的參照調查與量測）

日期：2026-11-06。分支 `claude/v8-arch-big-map-survey`（切自 master `2cd57244`）。卡片由架構師會話開出。
段：上段（Architecture 判斷的根據）。調查者：主會話，開工時 Fable 5.1，之後由使用者切成計畫指定的 Opus 5.5／high；
參照的讀取：三個子 Agent（general-purpose、Opus 5.5、high）；對抗檢查者：Fable 5.1／xhigh（使用者同意用比主會話強的模型）。

**狀態：** 完成。六個未決事項 2026-11-06 由使用者照建議定案（「照建議定案」，代號 `v8-arch-big-map-sim-model`、`-chunk-size`、`-encoding`、`-markers-owner`、`-greedy`、`-lod`，內容在 HANDOFF「決定」）。只改文件，沒有程式變更。

相關文件：[計畫 README](../architecture/plans/big-map/README.md)、[survey](../architecture/plans/big-map/survey.zh-Hant.md)、
[proposal v0.1](../architecture/plans/big-map/proposal-v0.1.zh-Hant.md)、[HANDOFF](../architecture/plans/big-map/HANDOFF.md)。

## 做了什麼

- 把架構師會話的討論稿＋方案 v0.1（原檔在 home 的 plan 檔）逐字複製進 repo，開頭標明「假說，由 survey 檢驗」。
- 建計畫 README（批次一覽：survey → engine-map → tool-chunked → product-arena-v3）與 HANDOFF。
- 九項參照（Minecraft、Luanti、Veloren、Godot GridMap、UE5 World Partition、Jolt／Bullet broadphase、逃離塔科夫、標記層、greedy 與 LOD）
  各寫成「機制｜為什麼｜對 GYO 的當てはめ（owner）｜判定｜根據」的表；列出 proposal 第 3 節憑記憶寫的內容與來源不符之處。
- 六個未決事項各一節（業界、建議、理由、不採的代價），不替使用者決定。
- 三項量測（只量，不改程式）；量測的事前宣告寫在 HANDOFF。
- 對抗檢查，指正全部核對後採納，結果附在 survey 第 7 節。
- 「採らない」的項目整理成 Replacement Criteria 候選表。

## 經過

1. 先讀 proposal 與現況程式（Engine 的 voxel mesh 解析、產品的 `Movement.cpp`、`ShotQuery.cpp`、`CharacterCollision.cpp`、`Arena.cpp`）。
2. 量測程式不放進 repo：沿用 PR #158 分支工作樹（`v8-tool-voxel-map-plan`，`690604f7`）既有建置樹的靜態庫，在主 checkout 的 `build/evidence/big-map/survey/` 另行編譯連結
   （用 `ninja -t commands` 取出編譯與連結參數，不重建那棵樹）。
3. (c) 第一輪發現重複鍵回呼那一步占了大部分時間，照事前宣告的規則**另記一輪**追加兩項（空回呼、SAX＋鍵集合），第一輪保留。
   當時的結論是「慢在回呼機制、每個值都付成本」——後來被對抗檢查推翻（見 5）。
4. 三個子 Agent 平行讀主要來源，只回事實與來源；判定由調查者做。報告存進證據目錄。
5. 對抗檢查（Fable／xhigh）指出：
   - (c) 的成本是 nlohmann 回呼解析器對「同一陣列內物件數」的二次方（每結束一個物件就線性掃一次父容器），不是每值成本。
     調查者讀了 nlohmann 3.12.0 的 `json_sax.hpp`（`end_object` 從 `:592` 起、`:628` 的迴圈）並重跑檢查者的程式，可重現。
     「4 MB/s」因此不是速率，原本的 22 s 外推刪除。
   - (b) 原本的「站地面」情境（y = 0、地圖外、無接觸）是最便宜的路徑；站在地形上一步約 2.2 ms，4 人約用掉 tick 的 53 %。
   - 兩個推論被寫成事實（UE 伺服器預設整張載入、塔科夫依距離睡眠），三個隱藏的 Architecture Delta（工具 DI 3／DI 4 的守衛變更、`GYO::MapMarkers` 是新 Subsystem、
     平行解析不能放進 `GYO::VoxelMesh`）。
   - 全部採納並修正 survey。

## 查證過的事實（判斷用到的）

- Jolt 的 broadphase 查詢明寫非決定性，原因是多執行緒與動態樹（`Docs/Architecture.md`，`0f69a6b8…`）；對 GYO 的靜態格不成立，要帶走的是「順序與平手寫進契約、與暴力掃描比對」。
- 產品現在的平手：Movement 取先到的（`<`），ShotQuery 對牆取後到的（`<=`，對結果沒有影響）。
- Godot GridMap 的導航是每格一個區域（`grid_map.cpp:900` 一帶，`8cc3a574…`），不是 proposal 記的每 octant。
- Veloren 的玩家物理預設 Client 權威；卸載區的 NPC 刪除並交給 rtsim。
- Luanti 的 ABM 順序每輪隨機打亂（`serverenvironment.cpp:1009`，`b4c73357…`），沒有決定性。
- 塔科夫的玩家數、AI 上限、距離欄位只有社群資料（Fandom、SPT、tarkov.dev），沒有官方技術說明。

## 驗證

```text
件數の突き合わせ：一致（(a) 600／600；(b) 9 情境 × 2,000／2,000；(c) 5 項 × 10＋7 項 × 10，ParseVoxelMesh 20／20 成功；對抗檢查者另行核對一致）
```

| 量測 | 結果（中位數或 p95） |
|---|---|
| (a) 只畫不重建，497k 頂點、1280×720 | 每幀 p95 4.6 ms；一次性 build＋讀回 2.77 s |
| (b) 線性掃描，20,691 盒 | 一步：地面最便宜 1.20 ms、地形上 2.23 ms（檢查者追加）、空中 2.28 ms；一槍 0.29 ms；每盒成本固定（線性） |
| (c) 解析 5.3 MB | `ParseVoxelMesh` 1.32～1.40 s；無回呼 DOM 149 ms；SAX＋重複鍵 77 ms；規則約 83～124 ms |
| (c) 追加（檢查者） | 同一陣列 2 萬物件的空回呼 1.1 s、4 萬 4.4 s（二次方）；改成整數陣列 143 ms |

- 機器：本機 MacBook Pro（Intel Core i9-9980HK、64 GB、macOS 26.7.1、GPU 兩顆，SDL 自選、未記錄用哪一顆）。Windows、Linux 未量。
- 量測只描述，不是受け入れ判定，沒有門檻。

## 證據

主 checkout 的 `build/evidence/big-map/survey/`（git 不追蹤，本機 MacBook Pro）：

- `build.sh`、`draw_only.cpp`、`match_tick.cpp`、`gen_parse.cpp`、`gen_parse_round1.cpp`，各項的 `*.txt`（摘要）與 `*.csv`（每次的值），合成圖產物 `synthetic.voxel_mesh.json`。
- `sources/R1～R3-*.md`：三個子 Agent 的來源報告（URL、釘選 commit、檔案與行號）。
- `adversary/`：對抗檢查者的追加量測（`nl_scaling.*`、`match_tick_realistic.*`）、重讀的原始碼與設定副本、`README.txt`。
- 子 Agent 的網頁暫存在會話暫存區，未保存，不作證據。

## 用時

開工約 03:30 JST，停下 04:40 JST，約 1.2 h（讀來源與對抗檢查由子 Agent 平行做）。估時 8～12 h 之內。

## 留給之後

- 六項已定案；由架構師會話開 engine-map 批（之後 tool-chunked、product-arena-v3）。
- 未量：greedy 的縮減、碰撞盒區塊內合併的增量、broadphase 後的單步成本、Client 的畫面時間、Windows／Linux 的解析速度。各自寫在 survey 第 4 節的建議裡，排在之後的批次量。
- 未讀：Ericson 第 7 章、simdjson／RapidJSON、Vintage Story、Teardown 作者的新引擎。
