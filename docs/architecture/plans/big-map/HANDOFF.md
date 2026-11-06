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
- 2026-11-06 使用者同意對抗檢查者用 Fable／xhigh（比主會話 Opus 5.5 強，照計畫先問）。
- 九項參照：開三個子 Agent（general-purpose、Opus 5.5 繼承、high 繼承）讀主要來源，只回事實與來源；判定由調查者（主會話）做。
  分工：R1 Minecraft／Luanti／Veloren；R2 Godot GridMap／UE5 World Partition／Jolt・Bullet broadphase；R3 塔科夫／標記層／greedy 與 LOD。
- 2026-11-06 三份子 Agent 報告收齊（存於 `build/evidence/big-map/survey/sources/`）。survey 本文寫完：九項參照表、proposal §3 的修正、六個未決事項、量測、RC 候選、範圍限界。
  重要修正：Jolt 明寫 broadphase 查詢非決定性（v0.1 的等價契約是 GYO 自己要立的）；塔科夫「伺服器全圖模擬 AI」未確認，社群資料顯示 bot 依距離睡眠；Veloren 玩家物理預設 Client 權威。
  下一步：對抗檢查（Fable／xhigh）。
- 2026-11-06 對抗檢查（Fable 5.1／xhigh）完成，證據在 `build/evidence/big-map/survey/adversary/`。主要指正：
  - (c) 的 1.1 s 是 nlohmann 回呼解析器對「同一陣列內物件數」的二次方（`json_sax.hpp:592` 起的 `end_object`、`:628` 的迴圈），不是每值成本；「4 MB/s」不是速率、22 s 外推無效。
    調查者讀了原始碼並重跑檢查者的 `nl_scaling`，可重現（2 萬物件 1,096 ms）。
  - (b) 「站地面」是最便宜的路徑；站在地形上一步 2.23 ms，4 人約 53 %。
  - 推論被寫成事實（UE 伺服器預設整張載入、塔科夫依距離睡眠）；隱藏的 Delta（工具 DI 3／4 守衛、`GYO::MapMarkers` 新 Subsystem、平行解析不能進 VoxelMesh）。
  - 全部採納，survey 已修正（第 7 節列出判定與處置）。
- 2026-11-06 04:40 JST **停下，交使用者決定六個未決事項**（survey 第 4 節）。用時約 1.2 h（子 Agent 平行），估時 8～12 h 之內。

## 決定（使用者，2026-11-06）

使用者原話：「照建議定案」。survey 第 4 節的六項全部採用建議。

| 代號 | 決定 |
|---|---|
| `v8-arch-big-map-sim-model` | Match 整張載入碰撞盒，沒有「已載入集合」；AI 先全部模擬。產品批量到 tick（玩家＋AI）超過預算時，才加「遠處睡眠」，睡眠狀態是權威元件（進 digest、依 EntityId 升冪）。 |
| `v8-arch-big-map-chunk-size` | 區塊 16³（cell 0.5 m 時 8 m）；region 8×8 區塊柱 × 全高（64 m）；`region_size` 寫進索引。 |
| `v8-arch-big-map-encoding` | 契約只有 JSON 文字一種編碼。engine-map 批把 Engine 內部解析換成不經回呼的嚴格解析（最小改法：SAX 查重複鍵＋不帶回呼的 `json::parse`，規則不動），並事前宣告載入時間的判定；二進位快取記為 Replacement Criterion；換第三方解析器另案（Engine 新依賴，要使用者決定）；region 平行解析由呼叫端做，不進 `GYO::VoxelMesh`。 |
| `v8-arch-big-map-markers-owner` | 標記層 owner＝Engine 新模組 `GYO::MapMarkers`（只依賴 Base、Math）：點、盒、路徑，1/16 格整數座標，`id`／`kind`／`tags` 名字不透明；產品提供 `marker_kinds.json` 並拒絕不認得的 kind；Engine 文件與範例不出現玩法名詞。新 Subsystem 的 Architecture Delta 由 engine-map 批寫 7 項。 |
| `v8-arch-big-map-greedy` | greedy 進工具的烘焙，掃描順序固定；碰撞盒改為區塊內合併。tool-chunked 批事前宣告量測（`pvp_corners` 與像建築的合成圖：四邊形數、盒子數、每區塊重烘時間、T-junction 目視）；工具 DI 3／DI 4 守衛的變更照 §14.4 取得使用者核准。 |
| `v8-arch-big-map-lod` | v0.1 不做 LOD；Client 以 region 做視錐與距離剔除。product-arena-v3 批量 Client 畫面時間（km² 大小、像建築的合成圖、最壞角度），超過宣告值時才由工具烘 region 代理（RC）。 |

proposal v0.1 與 survey 不一致之處，以 survey 與本表為準（例如 §11.3 的範例名、§11.7 的 4 MB/s 外推、§6a 的「全圖模擬 AI」前提）。

## 拆批與撞期（`v8-arch-big-map-split-engine`）

| 代號 | 白話 |
|---|---|
| `v8-arch-big-map-engine-map` | 拆批前的一批：格式＋MapMarkers＋broadphase＋解析替換（宣告 [engine-map](engine-map.zh-Hant.md)） |
| `v8-arch-big-map-engine-grid` | 第一批（PR 1）：broadphase＋解析替換（宣告 [engine-grid](engine-grid.zh-Hant.md)） |
| `v8-arch-big-map-engine-format` | 第二批（PR 2）：格式＋MapMarkers＋v1 退役 |
| `v8-arch-big-map-split-engine` | 拆批的決定 |

時序（2026-11-06，JST，依各會話紀錄）：

1. 架構師會話開 `-engine-map` 卡給 engine-map 會話（工作樹 `bold-mahavira-68c717`）。engine-map 會話寫宣告（`8464d66f`），使用者核准（原話「照建議核准」：v1 退役 (a)、解析門檻 450 ms、broadphase 多兩個 `…All` 查詢、Delta A～D），之後照宣告實作：`447f9194`（解析替換＋格式）、`c2287961`（MapMarkers）、`49449a12`（broadphase）。
2. 同一時段，使用者在架構師會話決定拆批（原話「拆兩批（建議）：先 broadphase＋解析，再格式＋MapMarkers」），並另開 engine-grid 會話（工作樹 `funny-bouman-3d0818`）。
   該會話寫了自己的 engine-grid 宣告（`25026d7d`，未合併），使用者在該會話核准（原話「核准，用 Fable」）；程式寫了一部分，沒有建置、沒有測試。
3. 架構師把拆批轉達給 engine-map 會話時，engine-map 會話的兩批程式都已提交。兩個會話各有一份使用者核准的宣告，內容不同：

   | 項目 | engine-map 宣告（本 PR 採用） | engine-grid 會話的宣告（`25026d7d`，不採用） |
   |---|---|---|
   | 解析門檻 | J5：中位數 ≤ 450 ms | P-1：≤ 0.50 s；另有 P-2（同一陣列 1～8 萬盒，t(8 萬)÷t(1 萬) ≤ 12） |
   | 嚴格解析放哪 | `GYO::Base` 的 `StrictJson.hpp`（Delta） | `VoxelMesh.cpp` 內部，不抽 |
   | broadphase 的 API | 查詢內含保守候選範圍；多 `…All` 兩個查詢；格子數上限時格長加倍 | 公開 `CapsuleBounds`／`SweptBounds`／`RayBounds`＋`Overlapping`；格座標夾在 ±2³⁰＋大盒子清單 |

4. 使用者在 engine-map 會話選「當第一批基礎」，在架構師會話選「用現有分支，拆成兩個 PR」。處置：
   - PR 1＝engine-map 會話分支中的 broadphase（`49449a12` 的內容）＋`StrictJson.hpp`＋`ParseVoxelMesh` 改用它（從 `447f9194` 拆出，不含任何 v2 格式）；宣告由 engine-map 宣告拆出（[engine-grid](engine-grid.zh-Hant.md)），判定照 engine-map 的核准（450 ms）。
   - engine-grid 會話的宣告與程式不合併；它擔任 PR 1 的審查者（Opus，與實作者不同會話）。它指出的兩點已處理：重複鍵突變的改指向照 §14.4 寫在 dev_log；`IsCharacterSupported` 不能換成「取最近」，對照表用 `SweepVerticalCapsuleAll`＋原條件。
   - P-2 沒有在 engine-map 宣告裡事前宣告，所以不當判定；同樣的量測以「探索」記在 dev_log。
   - PR 2 從 PR 1 的分支分出，PR 1 合併後 rebase 再開；宣告用 [engine-format](engine-format.zh-Hant.md) 草稿（在 PR 2 的分支上）。

## 現況：engine-grid（`v8-arch-big-map-engine-grid`，PR 1）

- 分支 `claude/v8-arch-big-map-engine-grid`（從 master `d54b60ef`），工作樹 `bold-mahavira-68c717`。拆批前的整套實作留在本機分支 `claude/v8-arch-big-map-engine-map`（`7c0786f3`），PR 2 的材料。
- J5 解析時間（受理判定；**件數の突き合わせ：一致**——要求 5／完成 5／成功 5，展開後 quads 124,236、colliders 20,691）：
  拆批前的判定試行（engine-map 分支的程式碼）中位數 **382.2 ms** ≤ 450 ms，成立；替換前（master）1,579.7 ms；展開結果 SHA-256 兩者相同 `678a28c2…e299`。
  拆批後 PR 1 的程式碼再量一次（確認，不是判定）：中位數 364.0 ms，SHA-256 相同。
- J4 broadphase 等價（**件數の突き合わせ：一致**）：200 組隨機集合 70,000 次查詢，比對 70,000，不一致 0；邊界案例 4 種格長 × 172 次＋極小格長 1,400 次，不一致 0。
- 探索（未宣告、不判定）：同一陣列 1／2／4／8 萬個碰撞盒，替換前中位數 345／1,139／4,341／17,081 ms（每加倍約 4 倍），替換後 42.5／85.4／181.1／384.9 ms（每加倍約 2 倍）。
- 補強（對照 `25026d7d` 而加）：座標 ±1e6 的邊界案例 6,300 次查詢，不一致 0；突變 `grid-capsule-start-only`。
- J2 突變（PR 1 分支，最後一輪）：`engine_grid_mutations.json` 15/16 被殺，存活的 `grid-margin-zero` 記為等價突變（理由在 dev_log）；`voxel_mesh_mutations.json` 20/20 被殺（兩個重複鍵突變改指向，§14.4 的 1～5 在 dev_log）。
- J6：本機全量建置成功；`ctest -L "cpu|shader"` 130/130、`-L gpu` 4/4 通過。L1 待 PR。J7：兩個依賴守衛的突變都被殺。
- 證據：主 checkout `build/evidence/big-map/engine-map/`（J5、探索、拆批前的突變）與 `build/evidence/big-map/engine-grid/`（PR 1 的突變、建置、ctest）。

- 2026-11-06 PR #161 開出（head `f7374738`）。架構師的文件審查：沒有 blocker，兩個 nit（collision.md 的產品詞彙、Status 的寫法）已在本機提交 `f113f96e`，未推送。
- **2026-11-06 停下（AGENTS §14 冒頭與 §14.1）：J2 不成立。** 審查者（engine-grid 會話）對 `f7374738` 的結論是 blocker 1、should-fix 2、nit 4、交使用者決定 4。
  審查草稿在主 checkout `build/evidence/big-map/engine-grid/review_pr161/review_ja.md`，尚未送 GitHub。
  - B1：`grid-margin-zero` 不是等價突變。精確掃掠接受終點外 1e-12 × |δ| 內的根，所以邊際歸零會漏掉盒子。反例和 probe 在同一目錄。
    宣告 J2 的「若存活就記錄為等價突變並說明理由」，理由被推翻，所以 J2 未達成。照 §14.1 先報告使用者、再修；架構師會話寫報告。
  - 用時：從開工到停下約 4 h（估時 8～12 h 之內）。
  - 分到的事實：我的隨機集合與邊界案例都沒碰到精確查詢的根接受窗口。原本的論證漏掉窗口，沒有讀窄相位的程式碼。
  - 下一次會改變的判斷：「等價突變」不只靠推理，要讀窄相位的每個容許與窗口，或請不同的人找反例。
- **工作樹裡已準備、未提交、未推送的修法**（等使用者決定）：
  - B1：反例加成 J4 的邊界案例（「a sweep that ends just short of a face on a cell line」）。原程式碼通過；`grid-margin-zero` 單獨重跑後被殺。
    dev_log、catalog、宣告與 collision.md 改寫為「原推定錯誤，反例已加成測試」，判定本身不動。
  - S1：建構時以 64 位元累加登記總數，`GYO_ASSERT` 小於 2³²；前提寫進標頭與 collision.md。
  - S2：`docs/architecture.md` 的 `GYO::Base` 列與 `engine/base/CMakeLists.txt` 的說明補上 StrictJson。
  - N1：宣告 2.2 的簽名改成實作的 `std::optional<Json>`＋`std::string& failure`。
  - N2：已在 `f113f96e`。
  - N3（極小半徑時邊際是否足夠）、N4（離群盒子讓格長加倍）、U1～U4（交使用者決定的四項）：還沒處理，等決定。
- 給使用者的選項（報告由架構師會話整理）：
  1. 照上面的修法：反例加成測試、`grid-margin-zero` 被殺後重跑整個 J2，判定不改。
  2. 回頭看計畫的前提：例如把精確查詢的根接受窗口寫進 Collision 的契約，或另開一批處理窄相位，也就是改變範圍。

- 2026-11-06 CI：L1 windows-x64 編譯失敗（`StaticAabbGrid.cpp` 用 `std::back_inserter` 卻沒 include `<iterator>`，MSVC 的 C2039）。使用者指示修好並推送：`dc3f9dd0` 只補 include，連同 nit 的 `f113f96e` 推送。
- **2026-11-06 使用者對 J2 不成立的決定**（原話「照修法重跑 J2」）：選項 1。判定不改；反例留成測試；推送工作樹的修法（B1、S1、S2、N1），重跑整個 J2 與 L1。

## 下一步

重跑整個 J2 與全量測試，推送，等 L1 與審查者複核；PR 1 合併後開 PR 2。之後 `-tool-chunked`、`-product-arena-v3` 由架構師會話開。
