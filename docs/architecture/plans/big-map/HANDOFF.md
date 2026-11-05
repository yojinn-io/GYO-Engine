# 大地圖計畫：交接

## 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-big-map-survey` | 本批：以來源與量測檢驗方案 v0.1 |
| `v8-arch-big-map-survey-measure` | 本批的三項量測（只量，不改程式）：(a) 只畫不重建、(b) Match 一 tick 線性掃碰撞盒、(c) 解析時間拆解 |
| `v8-arch-big-map-survey-adversary` | 本批的對抗檢查 |

## 現況

- 2026-11-06 開工。卡片由架構師會話開出。工作樹 `hopeful-wescoff-d692d8`，分支 `claude/v8-arch-big-map-survey`，基底 master `2cd57244`（PR #157 合併後）。
  主會話：開工時是 Fable 5.1，之後切成 Opus 5.5（計畫指定的調查者模型）；調查、量測、文件都在 Opus 5.5 下做。
- 讀過：proposal（架構師的 plan 檔，已逐字複製為 [proposal-v0.1](proposal-v0.1.zh-Hant.md)）、AGENTS.md、兩份 agents 規則、
  `docs/architecture/voxel-mesh.md`（DI 1～5、RC 1～4）、`engine/voxel_mesh/src/VoxelMesh.cpp` 的解析段、
  產品的 `Movement.cpp`、`ShotQuery.cpp`、`Arena.cpp`、`CharacterCollision.cpp`、
  voxel 線 HANDOFF（PR #158 分支 `claude/v8-tool-voxel-map-window` 上，用唯讀方式讀）與 explore-p1 的證據。
- 估時 8～12 h；超過 18 h、或單一問題累計 >2 h，停下回報。

## 量測的事前宣告（`v8-arch-big-map-survey-measure`，量之前寫）

**性質：探索與描述用，不是受け入れ判定。** 不設門檻、不判過與不過；數字只作為六個未決事項的證據。
所以不需要 AGENTS §14.1 的事前承認；但量測方法在看結果前寫在這裡，看結果後不改方法（要改就另記一次，兩次都留）。

- **機器**：本機 MacBook Pro（Intel Core i9-9980HK 2.4 GHz、16 執行緒、64 GB、macOS 26.7.1）。GPU 驅動由 SDL GPU 自選（Metal）。
- **程式來源**：PR #158 分支（`claude/v8-tool-voxel-map-window`，`690604f7`）工作樹 `v8-tool-voxel-map-plan` 的既有建置樹
  （`build/target/_build/test`，RelWithDebInfo、`-O2 -g`、`-ffp-contract=off`）裡已建好的靜態庫。不重建該樹、不改它的原始碼；
  量測程式在主 checkout 的 `build/evidence/big-map/survey/` 另行編譯連結。
- **合成圖**：和 explore-p1 相同（`prof.cpp`／`PreviewMeasure.cpp` 的 `SyntheticGrid`：64×16×64、填 30%、`minstd_rand(20261106)`、四種方塊、24 個方向）。
- (a) **只畫不重建**：合成圖只 build 一次、上傳一次；之後每幀只做 `Render`（1280×720）＋讀回＋上傳成 SDL 貼圖＋Present。
  600 幀，記每幀時間；另記一次性的 build、上傳時間。件數：要求幀數／成功幀數／失敗幀數。
- (b) **Match 一 tick 線性掃**：由合成圖的 Engine 展開取得碰撞盒（20,691 個），放進產品的 `Arena.walls`（不經 `Validate`，因為 1024 上限擋住）。
  量 `StepMovement`（一位玩家一個 tick）與 `QueryShot`（一槍，4 位玩家）的單次時間；各情境 2,000 次，記中位數與 p95。
  情境：玩家站在地面（`y = 0`，supported 提前返回）、玩家在空中（supported 掃全部）、水平一槍。
  另把盒子平移複製成 ×5、×25，確認時間與盒子數成線性（外推 10⁵～10⁶ 用）。件數：每情境要求次數／完成次數。
- (c) **解析時間拆解**：同一份 5.3 MB 文字，各 10 次：nlohmann 的 SAX 掃描（不建 DOM）、`json::parse`（建 DOM、無回呼）、
  `json::parse`＋與 Engine 相同的重複鍵回呼、Engine `ParseVoxelMesh` 全部、`ExpandVoxelMesh`。規則檢查≈全部−（DOM＋回呼）。
  件數：每項要求次數／完成次數；`ParseVoxelMesh` 每次都要成功。
- 2026-11-06 (c) 第一輪：`dom_parse` 149 ms，但 `dom_parse_dupkey_cb` 1,241 ms，`ParseVoxelMesh` 1,324 ms。差額幾乎全在回呼那一步。
  **追加量測（看到第一輪之後加的，第一輪保留）**：為了分開「nlohmann 的回呼機制本身」與「重複鍵檢查」，加兩項：
  `dom_parse_noop_cb`（回呼什麼都不做，只回 true）、`sax_dupkey`（SAX 掃描＋每個物件一個鍵集合，不建 DOM）。其他方法不變，各 10 次。
- 2026-11-06 量測完成（件數の突き合わせ：一致。每項要求次數＝完成次數，失敗 0）。結果與解讀寫在 [survey](survey.zh-Hant.md) 的「量測」節；證據在主 checkout `build/evidence/big-map/survey/`。
  - (a) 只畫不重建（600 幀，1280×720，497k 頂點）：每幀 p50 3.7 ms、p95 4.6 ms、p99 6.6 ms、max 13.8 ms；一次性 build＋Engine 讀回 2.77 s、上傳 26 ms。
    機器有兩顆 GPU（Intel UHD 630、AMD Radeon Pro 5500M），SDL GPU 自選，沒有記錄用的是哪一顆。
  - (b) 線性掃（每情境 2,000 次）：20,691 盒時一位玩家一步 地面 1.20 ms／空中 2.28 ms，一槍 0.29 ms；×5、×25 時每盒成本固定（約 60／112／15 ns），確認線性。
  - (c) 解析（各 10 次，中位數）：SAX 掃描 75 ms、建 DOM 149 ms、建 DOM＋重複鍵回呼 1,278 ms、空回呼 1,268 ms、SAX＋重複鍵 77 ms、`ParseVoxelMesh` 1,403 ms、展開 5 ms。
    慢的是 nlohmann 回呼式解析的機制，不是 JSON 也不是重複鍵檢查。
