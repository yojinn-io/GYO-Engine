# 大地圖 engine-grid：靜態 broadphase 與解析替換（`v8-arch-big-map-engine-grid`）

更新：2026-11-06。Owner：Engine（`GYO::Collision`、`GYO::VoxelMesh` 的內部、`GYO::Base` 的一個標頭）。
計畫：[README](README.md)；記錄：[HANDOFF](HANDOFF.md)；根據：[survey](survey.zh-Hant.md) 與 HANDOFF「決定」的六項。
**狀態：由 [engine-map 宣告](engine-map.zh-Hant.md)拆出，使用者 2026-11-06 核准（engine-map 會話，原話「照建議核准」）。核准後判定不放寬。**
拆批的經過與另一份 engine-grid 宣告（engine-grid 會話，分支 `claude/funny-bouman-3d0818`，`25026d7d`）的關係，見 [HANDOFF](HANDOFF.md)「拆批與撞期」。

## 0. 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-big-map-engine-grid` | 本批（PR 1）：`GYO::Collision` 的靜態 broadphase `StaticAabbGrid`；`ParseVoxelMesh` 改用不經回呼的嚴格解析（`GYO::Base` 的 `StrictJson.hpp`）。不碰格式 |
| `v8-arch-big-map-engine-format` | 第二批（PR 2）：`gyo.voxel_map`／`gyo.voxel_region`、`ValidateVoxelChunk`、`ExpandVoxelChunk`、新模組 `GYO::MapMarkers`、v1 退役 (a)。宣告在 [engine-format](engine-format.zh-Hant.md) |
| J2、J4、J5、J6、J7 | engine-map 宣告的判定代號，沿用（J1、J3 屬第二批） |

## 1. 範圍

### 做

- `StaticAabbGrid`（`engine/collision/`）與等價契約、與線性掃描逐一比對的測試、`docs/architecture/collision.md`（新建）。
- 解析替換：`engine/base/include/engine/base/StrictJson.hpp`（只依賴標準函式庫的模板）；`ParseVoxelMesh` 改用它，規則程式、錯誤碼與訊息不變。
- 依賴守衛：`GYO::Collision` 只連 Math、Base（新增）；`GYO::VoxelMesh` 照舊。
- 文件：`docs/architecture.md` 的模組表與 Model／collision 段加 broadphase 的歸屬；`docs/architecture/voxel-mesh.md` 的 Cost 一條加解析方式。

### 不做

- 不改格式（第二批）；不改產品與工具程式碼；不碰 framework、Gateway、pv10。
- 不換第三方解析器、不做平行解析、二進位、LOD。

### `voxel-mesh.md` 的 RC 3

RC 3「量到載入或碰撞查詢需要切成區塊或空間索引」已由 survey 的量測觸發（§5(b)、§5(c)）。依該節要交使用者決定修補或重建；
使用者 2026-11-06 的決定（HANDOFF「決定」`v8-arch-big-map-encoding`、`-chunk-size`）已回答：本批修補（解析替換，契約不變；空間索引放在 `GYO::Collision`），格式的重建在第二批。
RC 3 的文字本批不改，第二批（格式重建）時改寫成「已處置」。

## 2. 設計概要

### 2.1 `StaticAabbGrid`

```cpp
enum class TieBreak : std::uint8_t { LowestIndex, HighestIndex };
struct RaycastHit { float distance; std::uint32_t index; };
struct ContactHit { Contact contact; std::uint32_t index; };

class StaticAabbGrid final {
public:
    StaticAabbGrid(std::span<const Math::Aabb> boxes, float cellLength); // 複製盒子；cellLength 只是效能提示
    std::size_t Size() const noexcept;
    std::vector<std::uint32_t> Overlapping(const Math::Aabb& bounds) const;
    std::optional<RaycastHit>  RaycastNearest(const Math::Ray& ray, float maximumDistance, TieBreak tie) const;
    std::optional<ContactHit>  SweepVerticalCapsuleNearest(const VerticalCapsule& capsule, Math::Vec3 displacement, TieBreak tie) const;
    std::vector<ContactHit>    SweepVerticalCapsuleAll(const VerticalCapsule& capsule, Math::Vec3 displacement) const;
    std::vector<ContactHit>    OverlapVerticalCapsuleAll(const VerticalCapsule& capsule) const;
};
```

**契約＝等價定義**：每個查詢的結果，定義為「對全部盒子依索引 0, 1, …, n−1 線性掃描」的結果，位元相同：

| 查詢 | 等價的線性掃描 |
|---|---|
| `Overlapping(b)` | 收集 `i`，使盒子 i 與 b 相交（閉區間，相切算相交）；遞增 |
| `RaycastNearest(ray, d, tie)` | `h = RaycastAabb(ray, d, box[i])`；有值且（還沒有結果、或 `h < best`（LowestIndex）／`h <= best`（HighestIndex））就換成 (h, i) |
| `SweepVerticalCapsuleNearest(c, δ, tie)` | 同上，比 `SweepVerticalCapsuleAgainstAabb(c, δ, box[i])->fraction` |
| `SweepVerticalCapsuleAll(c, δ)` | 收集所有有值的 `(SweepVerticalCapsuleAgainstAabb(c, δ, box[i]), i)`；遞增 |
| `OverlapVerticalCapsuleAll(c)` | 收集所有有值的 `(OverlapVerticalCapsuleAabb(c, box[i]), i)`；遞增 |

- **為什麼比卡片多兩個 `…All`**：對照消費端，產品有三個迴圈不是「取最近」而是「任一個符合」或「取最深」，而且條件是產品的：
  `IsCharacterSupported`（任一 `normal.y > 0.5`）、`CanPlaceCharacterBody`（任一穿透 > 1e-5）、`MoveCharacterBody` 的穿透解除（取最深、`>` 取先、每面牆再做 `PlanarPenetration`）與 `constrainToFloor` 的掃掠（先改法線、略過、再 `<`）。
  若只給 `Overlapping`，產品要自己算「掃掠或膠囊可能碰到的範圍」，而那個範圍要比浮點的捨入保守，等於把 broadphase 的邊界知識搬到產品。`…All` 讓 Engine 負責保守的候選範圍，產品只換掉迴圈的來源、條件不動。
- **消費端零語意變更的對應**（產品批照這張表換）：

  | 產品現行程式（只讀，本批不改） | 平手 | 換成 |
  |---|---|---|
  | `ShotQuery.cpp:99` 牆：`*distance <= closest.distance` | 取後（`<=`） | `RaycastNearest(…, HighestIndex)`；結果只用距離 |
  | `Movement.cpp:76`→`IsCharacterSupported`：任一 `normal.y > 0.5` | 無（任一） | `SweepVerticalCapsuleAll`＋同條件 |
  | `CanPlaceCharacterBody`：任一穿透 > 1e-5 | 無 | `OverlapVerticalCapsuleAll`＋同條件 |
  | `MoveCharacterBody` 穿透解除：最深、`>` | 取先 | `OverlapVerticalCapsuleAll`（遞增）＋同迴圈；`PlanarPenetration` 用 `walls[index]` |
  | `MoveCharacterBody` 掃掠、`constrainToFloor = false`：`<` | 取先 | `SweepVerticalCapsuleNearest(…, LowestIndex)` |
  | `MoveCharacterBody` 掃掠、`constrainToFloor = true` | 取先（過濾後） | `SweepVerticalCapsuleAll`＋同迴圈 |

  牆之後才看 actor 的順序不變：牆的結果等於線性掃描，actor 迴圈照舊接在後面。
- **實作**：均勻格（密集 CSR，覆蓋所有盒子的範圍）；格長由呼叫端給，**格子數超過 2²² 時把格長加倍到不超過為止**（結果依契約與格長無關，所以這不是錯誤路徑）。
  格子索引用 double 的 `floor((x − 原點) / 格長)`，盒子登記到它的閉區間覆蓋的全部格子；查詢用同一個函式，單調性保證「相交的兩個區間，格子範圍必有交集」。
  掃掠、膠囊、射線的候選範圍在 double 中以邊際 `1e-3 + 1e-5 × 最大座標絕對值` 擴大（比 Collision 的 1e-7 容許值與 double 捨入大很多個數量級；多的候選只花時間，不改結果）。
  候選排序去重後，**用 Collision 的精確查詢依索引順序跑同一個比較**，所以結果等於線性掃描。
  射線把 [0, d] 切成長度＝格長的段，逐段收集候選；已找到的最近距離 `h` 之後、起點超過 `h＋邊際` 的段不再看。
- **不做**：動態物體、非 AABB、多執行緒建構（RC）。查詢是 const，不改內部狀態。
- **前提**（違反是 Programmer Error，`GYO_ASSERT`）：每個盒子 `IsValid`、`cellLength` 有限且 > 0、盒子數 < 2³²；查詢的輸入與 Collision 現有查詢相同的前提。

### 2.2 解析替換（契約不變）

- 現在：`json::parse` 帶回呼查重複鍵 → nlohmann 回呼解析器在 `end_object` 對父陣列的線性掃描，物件數的二次方（survey §5(c)）。
- 換成：(1) `sax_parse` 掃一次，每個開著的物件一個鍵集合，重複就失敗；(2) 不帶回呼的 `parse`。規則程式不動。
- **放哪裡**：`engine/base/include/engine/base/StrictJson.hpp`，一個**只依賴標準函式庫的模板**：`template <class Json> std::optional<Json> ParseStrictJson(std::string_view text, std::string& failure)`（失敗時回 `std::nullopt`，`failure` 是「repeated key '…'」或「not JSON: …」；不丟例外）。
  它的 SAX 處理器只透過模板參數（`Json::number_integer_t`、`Json::sax_parse`、`Json::exception`）碰 nlohmann，標頭本身不 include nlohmann，所以 Base 仍是「header-only、只依賴標準函式庫」；只有 instantiate 它的模組（本批是 VoxelMesh；第二批的 MapMarkers 也會用，都 PRIVATE 連 nlohmann）需要 nlohmann。
  - 為什麼不放 voxel_mesh 內部：MapMarkers 不能依賴 VoxelMesh（兩個並列的格式模組，一個依賴另一個沒有理由）。
  - 為什麼不另開模組：卡片指定不開；一個 60 行的模板不值得一個 target。
  - 為什麼不複製兩份：同一個 Adapter 重複（AGENTS §3 的 evidence）；而且 `GYO::Model` 的 `AnimationEvents.cpp:40` 已經是第三份同樣的回呼（**本批不改**，記為 Code Smell）。

## 3. Architecture Delta（AGENTS §3 的 7 項）

### Delta A（engine-map 的 Delta C）：`GYO::Collision` 的靜態 broadphase

| 項 | 內容 |
|---|---|
| 1 需要它的 Feature | raid 地圖的 Match（4 人＋AI）與 Client 預測；survey §5(b)：2 萬盒時一位玩家一步約 2.2 ms，4 人約用掉 tick 的 53 %，加 AI 超過 |
| 2 現在的問題 | 產品對全部牆線性掃描；`walls ≤ 1024` 是現在不出事的原因，大地圖要撤掉它 |
| 3 變化的邊界 | `GYO::Collision` 的公開介面多一個型別與五個查詢；它的契約新增「結果與線性掃描等價」 |
| 4 影響 | Engine `GYO::Collision`；之後的 object_fps_pvp（`Movement`、`ShotQuery`、`CharacterCollision`、spawn 驗證） |
| 5 依賴方向 | 不變（Collision → Math，Base PRIVATE）；不新增依賴 |
| 6 Ownership | 空間索引與等價契約：Engine；牆是什麼、移動與命中的政策（平手以外的條件）：產品 |
| 7 更小的變更為何不夠 | 產品自己做格子：每個用碰撞的產品各寫一份，而且保守範圍的浮點知識留在產品；只給 `Overlapping`：同上（產品要算保守範圍）；換成樹或 Jolt：動態結構與非決定性（survey §2.6），而 GYO 的盒子是靜態的 |

- **DI**（Collision 文件新增一節）：1 等價：每個 broadphase 查詢的結果與對全部盒子依索引線性掃描的結果位元相同，平手依 `TieBreak`；格長只影響效能；2 靜態：建好不變，查詢不改內部狀態。
- **RC**：1 動態物體（門、可破壞物）；2 非 AABB 的靜態形狀；3 量到格子的桶極不均勻或盒子大小差很多 → 分層格（Ericson §7.2，survey 未讀，記為候選），再來才是樹。

### Delta B（engine-map 的 Delta D）：解析替換與 `GYO::Base` 的 `StrictJson.hpp`

| 項 | 內容 |
|---|---|
| 1 需要它的 Feature | 同 Delta A（km² 地圖的載入時間）；survey 決定 `v8-arch-big-map-encoding` |
| 2 現在的問題 | 回呼解析對同一陣列的物件數二次方（20,691 個盒子約 1.1 s），格式稍改就可能從 0.1 s 變成數秒，不可預測 |
| 3 變化的邊界 | VoxelMesh 的內部（契約不變）；Base 的公開標頭多一個模板 |
| 4 影響 | `GYO::Base`（一個標頭）、`GYO::VoxelMesh`；第二批的 `GYO::MapMarkers` 也會用；`GYO::Model` 的 `AnimationEvents` 同樣的回呼本批不改 |
| 5 依賴方向 | 不變：Base 仍只依賴標準函式庫（模板不 include nlohmann）；VoxelMesh、MapMarkers 各自 PRIVATE 連 nlohmann |
| 6 Ownership | 「嚴格 JSON（拒絕重複鍵）」這個機制放 Base；各格式的規則仍在各自模組 |
| 7 更小的變更為何不夠 | 不換：km² 地圖載入不可預測；只在 VoxelMesh 內換：MapMarkers 要複製一份（同一 Adapter 重複）；換第三方解析器：Engine 新依賴，使用者決定的另案 |

## 4. 受理判定（engine-map 宣告的對應部分；看結果前宣告）

| 代號 | 判定 | 怎麼量 |
|---|---|---|
| J2 突變 | broadphase：打掉平手規則、格子邊界（第一格、最後一格、格數少一）、候選不排序不去重、射線提前停、膠囊候選漏掉高度或半徑、射線裁切錯邊、邊際歸零（若存活就記錄為等價突變並說明理由，不刪測試）、依賴守衛；解析：重複鍵檢查失效（VoxelMesh 的規則 1 測試）；**每個都要被殺**；catalog `docs/architecture/plans/big-map/scripts/engine_grid_mutations.json`；既有 `voxel_mesh_mutations.json` 全部仍被殺（兩個重複鍵突變的 `find` 因解析替換改指向，§14.4 的 1～5 寫在 dev_log） | 突變腳本 |
| J4 broadphase 等價 | 200 組隨機盒子集合（種子固定，盒數 1～300，範圍 ±4～±23 m，有密有疏；含負座標、零厚度、跨全部格子的大盒子、重複盒子），每組每種查詢 50 次；加邊界案例（共面、相切、零厚度、起點在盒內、距離相同的平手、盒子恰在格線上、查詢在格子範圍外、射線平行軸、`maximumDistance = 0`、極小格長觸發加倍、空格子）；**每個查詢與測試內的線性掃描逐一比對位元相同**；不一致件數＝0 | doctest |
| J5 解析時間 | 主 checkout `build/evidence/big-map/survey/synthetic.voxel_mesh.json`（5,334,270 B，SHA-256 `1557bb3b…162a8b`，explore-p1 同一張合成圖）；本機 Intel Mac、`-O2`；`ParseVoxelMesh` 連續 5 次、每次都成功，**中位數 ≤ 450 ms**；替換前（master `d54b60ef`）與替換後的展開結果 SHA-256 **必須相同** | 量測程式 |
| J6 迴歸 | 本機全量 cpu\|shader 與既有 gpu 測試結果不變；CI L1 三平台通過 | ctest＋L1 |
| J7 依賴守衛 | configure 時：`gyo_voxel_mesh` 只連 Base、Math、nlohmann；`gyo_collision` 只連 Math、Base；各有突變（多連一個庫要讓 configure 失敗） | CMake |

**件數の突き合わせ**：J4＝產生的查詢數／比對數／不一致數；J5＝要求次數／完成次數／成功次數與展開後的 quads、colliders 數（124,236／20,691）。

**範圍的限界**：解析時間只量 Intel Mac 一台；CI 不跑時間判定，所以速度沒有自動守衛（有人改回回呼解析，CI 抓不到；voxel-mesh.md 寫明原因，審查時看）。
broadphase 不設效能判定。「同一陣列 4 萬個物件」的二次方消失，本批沒有宣告成判定（engine-grid 會話的宣告有 P-2，本宣告沒有）；dev_log 記一次未宣告的探索量測。

## 4a. 與 engine-grid 會話宣告（`25026d7d`）的對照

另一份由使用者在 engine-grid 會話核准的宣告（未合併，不採用；經過見 HANDOFF「拆批與撞期」）。PR 1 的審查者會對照它，所以逐項寫出：
**有**＝本 PR 有對應；**等價**＝做法不同、守的性質相同；**無**＝本 PR 沒有（要不要補，交使用者決定）。

| `25026d7d` 的項目 | 本 PR | 說明 |
|---|---|---|
| 等價契約（每個查詢＝依索引順序的線性掃描；`TieBreak` 兩種） | 有 | 2.1 節；`docs/architecture/collision.md` |
| `RaycastNearest(ray, d, tie)`、`SweepVerticalCapsule(c, δ, tie)` | 有 | 後者名為 `SweepVerticalCapsuleNearest` |
| `Overlapping(query, out)`（`size_t`、寫進呼叫端的 vector） | 等價 | 本 PR 回 `std::vector<std::uint32_t>`；盒子數上限 < 2³²（斷言） |
| 公開的保守邊界 `CapsuleBounds`／`SweptBounds`／`RayBounds`＋`Overlapping`＋原迴圈 | 等價 | 本 PR 把保守邊界留在 Engine 內部，改給 `SweepVerticalCapsuleAll`、`OverlapVerticalCapsuleAll`（遞增索引的全部接觸），產品照樣跑原迴圈、原條件；產品不需要知道邊界怎麼放大 |
| 消費端對照表（`IsCharacterSupported` 不能用「最近者」等五處） | 有 | 2.1 節的表；`IsCharacterSupported` 用 `SweepVerticalCapsuleAll`＋原條件 |
| 格座標夾在 ±2³⁰（範圍外不需特例） | 等價 | 本 PR 的格子只覆蓋所有盒子的外包盒，座標的格索引夾在 [−1, 格數]；同樣靠單調性，範圍外的查詢只會多候選、不會少 |
| 大盒子清單（覆蓋太多格的盒子另放，每次查詢都當候選） | 無 | 本 PR 改成「格子總數 ≤ 2²²，超過就把格長加倍」。總格數有上限，但每個大盒子仍登記到它覆蓋的全部格子，所以「很多個巨大盒子」時登記數是盒子數 × 格數；大盒子清單沒有這個問題。產品的地圖（區塊內合併的盒子，最大一個區塊 8 m）不會出現這種輸入；要不要補，交使用者 |
| 隨機 200 組，一半密集（100～2,000 盒）、一半稀疏 | 等價（規模較小） | 本 PR：每組 1～300 盒、範圍 ±4～±23 m，含重複盒、零厚度、跨全部格子的大盒 |
| 座標在 ±1e6 的盒子 | 無 → 補 | 本 PR 原本只有 ±1,000 的射線起點；補一個邊界案例（見 dev_log） |
| 保守邊界的性質測試（窄相位有回傳的盒子都在邊界內） | 等價 | 本 PR 沒有公開邊界；同一性質由「…All 與線性掃描的全部接觸逐一相同」守 |
| 消費端寫法的比對測試（在測試裡重寫 `IsCharacterSupported` 等） | 無 | 本 PR 的 `…All` 就是線性掃描的全部接觸，產品照原迴圈走，所以沒有另寫；要不要補，交使用者 |
| 突變 `cg-tie-ignored`、`cg-tie-inverted` | 有 | `grid-tie-ignored`、`grid-tie-never-replaces` |
| `cg-cell-boundary-open` | 等價 | `grid-first-cell-dropped`、`grid-last-cell-dropped`、`grid-count-short`（被建構時的斷言或比對抓到） |
| `cg-no-dedupe`、`cg-no-sort` | 有 | `grid-candidates-unsorted`（同時打掉排序與去重） |
| `cg-large-list-skipped`、`cg-clamp-removed` | 無 | 沒有大盒子清單；夾限見上 |
| `cg-swept-bounds-start-only` | 無 → 補 | 補 `grid-capsule-start-only`（候選只取起點的膠囊） |
| `cg-ray-bounds-no-margin` | 等價 | `grid-margin-zero`：一度存活並記為等價突變，審查者的反例推翻了這個推定；反例已加成測試，突變被殺（dev_log） |
| `cg-links-threads` | 有 | `grid-links-voxel-mesh` |
| 解析：SAX 查重複鍵＋不帶回呼的 `parse`，規則不動 | 有 | 嚴格解析放在 `GYO::Base` 的 `StrictJson.hpp`（Delta B），對方放在 `VoxelMesh.cpp` 內部 |
| P-1：5.3 MB 合成圖中位數 ≤ 0.50 s | 有（更嚴） | J5：≤ 450 ms |
| P-2：同一陣列 1～8 萬盒，t(8 萬)÷t(1 萬) ≤ 12 | 無（判定）；有（探索） | 本宣告沒有事前宣告，所以不當判定。探索量測：替換後 42.5／85.4／181.1／384.9 ms，比值約 9.1；替換前 345／1,139／4,341／17,081 ms，比值約 49.5 |
| `voxel_mesh_mutations.json` 兩個重複鍵突變改指向（§14.4 的 1～5） | 有 | dev_log |
| 描述用量測（broadphase 各格長的 p50／p95） | 無 | 本宣告沒有 |

## 5. 結果（摘要；細節在 HANDOFF 與 dev_log）

- J4：200 組隨機 70,000 次查詢、邊界案例 2,088 次，與線性掃描不一致 0。成立。
- J5：中位數 382.2 ms（判定試行，拆批前）≤ 450 ms；替換前 1,579.7 ms；展開結果位元相同。成立。PR 1 程式碼的確認量測 364.0 ms。
- J2：**不成立**（宣告的寫法下）：`grid-margin-zero` 存活並記為等價突變，審查者以反例推翻。修法在工作樹，等使用者決定（HANDOFF）。
- J6、J7：見 dev_log（全量、L1）。

## 6. 證據位置

- 主 checkout `build/evidence/big-map/engine-map/`（git 不追蹤；拆批前就存在這裡，沿用）：`parse_time.cpp`、`build.sh`、`parse_time_before.txt`、`parse_time_after.txt`（判定試行）、`parse_time_engine_grid.txt`（PR 1 確認）、`scaling.txt` 與 `scaling/`（探索）、拆批前的突變結果。
- 主 checkout `build/evidence/big-map/engine-grid/`：PR 1 分支的突變（`mutations_engine_grid.txt`、`mutations_voxel_mesh_v1.txt`）、全量建置與 ctest。

## 7. 模型與檔位

上段（Engine 公開介面、DI）。2026-11-06 使用者同意「領航員和檔位照建議」：實作者 Opus／high（主會話），審查者 Opus／high，驗證者 Fable／xhigh。
架構師會話 2026-11-06 指定 PR 1 的審查者由 engine-grid 會話（Opus，與實作者不同會話）擔任。
