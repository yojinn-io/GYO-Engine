# 事前宣告：`physics_3d` Module（3D 物理，薄起步，`v8-arch-gyo2-physics-3d`）

更新：2026-11-23（修訂 1：照 PvP 移植準備卡的盤點對齊範圍、記下實作時的差異〔第 11 節〕）。Owner：架構（GYO2 主會話開的任務卡「3D 物理」）。**狀態：實作完成，等合併前核准**。照新 AGENTS §2.6（功能加速階段 `v8-arch-gyo2-speed-phase`）：本宣告不推翻使用者的決定、不動 §1，卡片自己判、直接實作；合併前照批次表〈每批的設計審查〉做一次設計審查。

上層決定：HANDOFF `v8-arch-gyo2-2d-and-3d`（物理 2D、3D 分成兩個 Module，各家都分）、`v8-arch-gyo2-transform-camera-physics-modules`（物理放 Module：型別、演算法、接 World 的部分都在 Module）、`v8-arch-gyo2-hierarchy-bevy-and-primary-reference`（ECS 相關機制以 Bevy 生態為主參考；物理是 avian／rapier）、`v8-arch-gyo2-authority-rollback-no-master-reference`（權威、回滾不參考 master 的算法、常數、數據）。

## 0. 根據與讀的版本、時間估計

- 程式：`origin/gyo2` 3ffb9e54。`engine/core/math`（`Aabb`、`Capsule`〔一般線段＋半徑〕、`Ray`〔t 以 |direction| 為單位〕、`Segment`、`Vec3d`、`Quaternion::Rotate`）；`engine/modules/transform`（`Transform` 逐位元 `==`、`GlobalTransform` 是畫面用的推導值）；`docs/cross-component-rules.md` CCR-12（判定不讀 `GlobalTransform`，要全域姿態就從 `Transform`〔與 `ChildOf`〕算）、CCR-13（回滾登記的型別的 `==` 逐位元）；Kernel `Query::Entities()` 依 EntityId 遞增（`Params.hpp:276`）；`Hash.hpp:84-85`（float、double 逐位元雜湊）；`build/cmake/GyoBuild.cmake:42-43`（`-ffp-contract=off`；MSVC `/fp:precise`）。
- 需求來源（**只當需求**，不參考演算法與常數）：`origin/master` 的 `apps/object_fps_pvp/`、`engine/collision/`。**以「PvP 移植準備」卡的盤點為準**（`local_8e95f98a`，卡片分支 `games/object_fps_pvp/docs/porting-inventory.zh-Hant.md` 第 3 節；主會話 2026-11-23 轉來）：七類查詢 Q1～Q7、沒用到的（移動中角色互推、台階、斜坡、三角網格、OBB、剛體）、靜態幾何全是 AABB（biome 1,260 個）。本卡自己的盤點（共享筆記 `gyo2-physics-3d.md`）與它一致。
- 決定性的需求：`docs/architecture/determinism.md`（legacy，仍在 gyo2）§1、§2、§0 DET-0：移動與碰撞掃掠屬「預測核心」，目標 (c) 三平台逐位元；2026-11-09 實機量到 Windows（MSVC）與 macOS x64（AppleClang）的 `sweep_fraction` 差 1～4 ULP、原因未明（11-01 之後的數據，當比較基準）。
- 別家：見第 5 節（avian、parry、rapier、JoltPhysics；使用者 2026-11-23 同意下載，HANDOFF `v8-arch-gyo2-physics-reference-downloads`）。
- **時間估計**：12～16 小時（核心形狀與查詢 5、靜態盒集合 2、World 那一側 2、測試 3、文件與 dev_log 2、L1 與審查 1）。照時間盒子，超過 1.5 倍或單一問題超過 2 小時就停下回報。

## 1. 先用遊戲講

FPS 的一回合：地圖是體素烘出來的幾萬個靜態方塊；每個玩家是一顆直立的膠囊。

- **走路**：角色要往前移 0.1 m。產品的移動規則（`MoveAndSlide` 一類，作者的）問物理：「這顆膠囊沿這個位移掃過去，最先撞到什麼、在第幾分之幾、法線朝哪？」拿到之後自己決定滑動、爬坡、退一點皮膚寬度。
- **開槍**：從眼睛沿瞄準方向打一條射線，問「最近的牆在多遠」，再對每個目標的 hitbox（骨架上的幾顆膠囊、球，產品每個 tick 算出來）問「射線打到哪一顆、多遠」。
- **出生**：問「這顆膠囊放在出生點，和牆或別的玩家重疊嗎」。
- **回滾**：Client 預測錯了，回到第 T 步重跑；重跑時物理的查詢只讀 `Transform` 與 `Collider`，所以結果和第一次一樣（逐位元）。

這一批**不做**剛體（重力、碰撞反彈、堆疊）：PvP 沒有用到。

## 2. 範圍

### 2.1 這一批做的（PvP 直近要用的）

1. 新 Module `engine/modules/physics_3d`（`GYO::Engine::Physics3d`，target `GYO::Physics3d`，登記單位 `Physics3dModule`；目錄名照 N14、N17 推，`3d` 照 camera 的 `Camera3d` 寫法）。登記檔一列：`library_only=no`、`default=no`（物理預設不帶，主會話的讀法）、`depends=transform`。
2. **形狀**（`Shapes.hpp`，局部空間）：`Ball{radius}`、`Capsule{a, b, radius}`（局部線段＋半徑，照 parry；`CapsuleY(halfHeight, radius)` 建沿局部 Y 的）、`Cuboid{halfExtents}`、`BoxSet`（大量靜態 AABB＋加速結構，給體素地圖；不可變、可共用）。`Shape` 是這四種的 variant。
3. **姿態**：`Pose{translation, rotation}`（`Transform` 去掉縮放）。
4. **純查詢**（不碰 World；`Queries.hpp`）：
   - 射線：`CastRay(shape, pose, ray, maxT) → optional<RayHit{t, normal}>`，四種形狀都支援；起點在實心形狀內回 t＝0。
   - 重疊與接觸：`Contact(shape1, pose1, shape2, pose2, prediction) → optional<Contact>`。
   - 形狀掃掠：`CastShape(shape1, pose1, motion, shape2, pose2, options) → Result<optional<ShapeHit>, Unsupported>`。
   - 支援的組合在第 4.3 節的表；不支援的組合回 `Unsupported`（照 parry 的 `Unsupported`，不偷偷回錯的答案）。
5. **World 那一側**（`Collider.hpp`、`SpatialQuery.hpp`、`Physics3dModule.hpp`）：元件 `Collider{Shape}`（登記名 `Physics3d.Collider`）；空間查詢函式接作者自己的 Query（`Query<Read<Collider>, Read<Transform::Transform>, Without<Kernel::ChildOf>, …>`）與過濾謂詞，回帶實體的結果（最近者平手取 EntityId 小的）。
6. 測試：形狀查詢的幾何案、`BoxSet` 與線性掃描逐位元等價（亂數語料）、三平台逐位元的 golden（字面 binary32 位元樣式）、World 查詢與回滾等價（重跑後查詢結果相同）。

### 2.2 不在這一批（只定方向，第 7 節）

剛體與解算器、重力、碰撞事件、自動的寬相位管線（每步維護的 BVH）、子實體上的 collider（需要從 `ChildOf` 合成姿態）、縮放的 collider、凸包與三角網格、GJK／EPA 的一般組合、角色控制器（`MoveAndSlide`）、2D 物理（另一個 Module，跟 2D 遊戲回來）。

## 3. 需求（從 PvP 推，AGENTS §2）

| # | 需求 | 這一批的回答 |
|---|---|---|
| Q1 | 大量靜態牆（體素烘出的 AABB，master 上限 2^20） | `BoxSet`：建一次、不變；查詢結果等於「依索引順序線性掃描所有方塊」逐位元相同（第 4.4 節） |
| Q2 | 直立膠囊對牆：重疊（含接觸法線與深度，推出牆）、掃掠（第一個接觸的比例、法線，滑動）；放置檢查另要膠囊×膠囊的重疊。移動中角色互推沒用到（master 一律傳空的 actors） | `Contact`（球、膠囊×{球、膠囊、方塊、`BoxSet`}）、`CastShape`（球、膠囊×{方塊、`BoxSet`}） |
| Q3 | 射線對牆、對 hitbox（骨頭上的球與線段膠囊；頭球 1、膠囊 6） | `CastRay`；hitbox 不是實體，用 `Capsule{a, b, radius}`（世界座標的線段）放在 `Pose{}` |
| Q4 | 著地判定（向下短掃、法線 y > 0.5） | 產品用 `CastShape` 往下短掃、看 `normal`；門檻是作者的 |
| Q4b | 相機防穿牆 | 同 Q2 的掃掠 |
| Q5 | 決定性：預測核心目標三平台逐位元 | 只用 IEEE 的 + − × ÷ 與 `sqrt`（正確捨入）、比較；不呼叫 libm 的超越函式；內部以 double 算、結果捨入成 float 一次；三平台 golden 守 |
| Q6 | 回滾等價（R-2）與 digest | 查詢是純函式、只讀 `Transform` 與 `Collider`；`Collider` 的雜湊逐位元（`BoxSet` 用建立時算好的內容摘要），`==` 與雜湊一致（CCR-13） |
| Q7 | 地板（master 是隱含的 y＝0） | 產品放一個大的 `Cuboid` 或 `BoxSet` 的一個方塊；半空間形狀不在這一批 |

## 4. 設計（Contract）

### 4.1 位置與依賴

```text
GYO::Base、GYO::Math ◄── GYO::Kernel ◄── GYO::Orchestration ◄── GYO::Transform ◄── GYO::Physics3d
```

- 依賴：base、math、kernel、orchestration、transform（讀 `Transform`、`ChildOf` 在 Kernel）。不依賴 world_time、rollback、authority：物理在哪張 schedule、要不要回滾 `Collider`，是作者的事（同 `TransformModule` 不登記回滾）。
- 純查詢與形狀不需要 World：`Shapes.hpp`、`Queries.hpp` 只用 Math；可以在任何地方呼叫（工具、測試、產品的 hitbox）。

### 4.2 數值政策

- 內部用 double（`Vec3d`），每個結果捨入成 float 一次。理由：大地圖（master 的 BigMap 約 1 km²）上 float 的平方項會失去精度；parry 有 f64 版本、Jolt 有 `JPH_DOUBLE_PRECISION`，都是為大世界。
- 只用 + − × ÷、`std::sqrt`（IEEE 754 要求正確捨入，各平台相同）、比較、`std::abs`；不用 `std::sin` 等超越函式、不用 `long double`、不用 FMA 內建函式。
- **沒有容差參數**；需要皮膚寬度的作者用 `ShapeCastOptions::targetDistance`（照 parry 的 `target_distance`）。
- 輸入必須有限、形狀尺寸為正（`IsValid`）；違反是程式的錯（`GYO_ASSERT`），內容來的資料先用 `IsValid` 檢查（`BoxSet::Build` 回 `Result`）。

### 4.3 純查詢

`RayHit{float t; Vec3 normal}`：`t` 以 |direction| 為單位（同 `Math::Intersect`），呼叫者給單位方向就是距離；`normal` 是打到的表面的外法線；起點在內時 t＝0、`normal`＝0 向量（照 parry：`solid` 時在內部回 0）。

`Contact{Vec3 point1, point2; Vec3 normal1, normal2; float distance}`（照 parry 的 `Contact`）：`point1` 在形狀 1 表面、`normal1` 是形狀 1 的外法線（指向形狀 2）；`distance` 小於 0 是穿透深度。`prediction` ≥ 0：距離不超過它就回接觸。

`ShapeHit{float time; Vec3 witness1, witness2, normal1, normal2; ShapeHitStatus status}`（照 parry 的 `ShapeCastHit`）：形狀 1 沿 `motion` 平移，`time` ∈ [0, `maxTime`] 是 `motion` 的比例；`status`：`Converged`（掃到）、`PenetratingOrWithinTargetDistance`（一開始就重疊或在 `targetDistance` 內）。`ShapeCastOptions{maxTime = 1, targetDistance = 0, stopAtPenetration = true}`：`stopAtPenetration` 為 false 時，一開始重疊而 `motion` 沿接觸法線分離或相切（`Dot(motion, normal1) <= 0` 以形狀 1 的外法線計）就不算撞到、繼續掃。

支援的組合（形狀 1 → 形狀 2；不在表裡的回 `Unsupported`）：

| 查詢 | 形狀 1 | 形狀 2 | 條件 |
|---|---|---|---|
| `CastRay` | — | 球、膠囊、方塊、`BoxSet` | 任意姿態 |
| `Contact` | 球、膠囊 | 球、膠囊、方塊、`BoxSet` | 任意姿態（對 `BoxSet` 回最深的一個與它的索引，另有回全部的版本） |
| `CastShape` | 球 | 方塊、`BoxSet` | 任意姿態 |
| `CastShape` | 膠囊 | 方塊、`BoxSet` | 膠囊的線段（`b - a`）在方塊的局部座標裡與某一軸平行（兩個分量恰為 0），或兩端重合（當球）；PvP 的直立膠囊對軸對齊方塊成立 |

形狀 1 對球、膠囊的掃掠（角色互推）PvP 沒用到，回 `Unsupported`。膠囊×方塊的一般方向（需要 GJK 一類的迭代法）留到有產品要用時（第 7 節）。

骨頭上的 hitbox：`CastRay(Capsule{a, b, radius}, Pose{}, ray, maxT)`（線段膠囊，任意方向；設計審查 S2 拿掉了世界座標 `Math::Sphere`、`Math::Capsule` 的多載）。

### 4.4 `BoxSet`

- 局部空間的 AABB 陣列（float），建立時檢查（有限、min ≤ max；`Build` 回 `Result<BoxSet, Physics3dError>`），建一棵靜態 BVH（節點的範圍＝子節點範圍的聯集，float 的 min／max 是精確的）。
- **契約：加速結構不改結果。** 每個查詢的結果，和「依索引 0、1、…、n−1 對每個方塊做同一個單方塊查詢、保留最近者（平手取索引小的）／依索引遞增收集全部」相同。BVH 只決定要測哪些方塊。這是加速查詢本身的正確性（最近者加上明定的平手規則，答案唯一），**不是**為了 master 的權威 digest（PvP 移植準備卡提醒 master 的這條是為它自己的 digest，不照搬；本卡另有理由：查詢結果不隨加速結構的形狀變，回滾重跑與兩端預測才不必另立規則）。選 BVH 不選 master 的均勻格：體素工具烘出的是合併過、大小不一的方塊，BVH 不需要調格子大小；parry 的複合形狀也是 BVH。成立的理由：射線對節點與葉子用同一個 slab 測試，float 的減法與乘以同一個數是單調的，所以大盒子的進入點不晚於它包住的小盒子；重疊與掃掠用往外捨入、再加邊距的查詢範圍挑候選。邊距（`Queries.cpp` 的 `(scale + 1) × 1e-9`）只決定候選、不改結果，推理（設計審查 N2）：不重疊時，單方塊算出的距離來自近似的最近點，不會小於真實距離減去 double 的捨入；掃掠的面區、邊區、角區都要求真的接近，所以接受的方塊一定落在查詢範圍裡；只有一個葉子的 `BoxSet`（方塊 ≤ 4）不看範圍、全部測，等價測試的參照（只有一個方塊）因此和候選選擇無關。守衛：亂數語料的等價測試。
- 內容摘要：建立時算一次 64-bit 摘要（每個方塊的 6 個 float 逐位元、依索引），`Collider` 的雜湊用它，不必每次雜湊幾萬個方塊。

### 4.5 World 那一側

- 元件 `Collider{Shape shape}`；`==` 逐位元（`BoxSet` 比共用指標相同或內容摘要與方塊數相同）、雜湊一致（CCR-13）。作者要它回滾或進 digest 時自己登記（同 `Transform`）。
- **姿態只讀 `Transform`**（CCR-12；`GlobalTransform` 不讀）：這一批的 collider 只在沒有父的實體上，`Transform` 就是全域姿態；查詢函式要求作者的 Query 帶 `Without<Kernel::ChildOf>`（編譯期檢查）。`Transform::scale` 不是 (1,1,1) 時，那個 collider 回 `Physics3dError`（`UnsupportedScale`），不默默忽略縮放。
- 空間查詢（`SpatialQuery.hpp`，照 avian 的 `SpatialQuery` 系統參數；Kernel 沒有自訂系統參數，所以是接作者 Query 的函式）：`CastRay(colliders, ray, maxT, filter)`、`CastShape(colliders, shape, pose, motion, options, filter)`、`Intersections(colliders, shape, pose, filter)`。回 `Result<optional<EntityRayHit>, Physics3dError>` 等。`filter` 是 `bool(EntityId)` 的可呼叫物（照 avian 的 `*_predicate` 版本），預設全收。
- 走訪是依 EntityId 遞增的線性走訪（Kernel `Query::Entities()` 的順序）；最近者平手取 EntityId 小的。PvP 的實體數（玩家 ≤ 64、地圖 1 個 `BoxSet` 實體）用不到每步維護的寬相位；需要時另加（第 7 節）。
- `Physics3dModule::Build` 只登記 `Collider`。沒有 System：這一批沒有模擬步。

### 4.6 決定性的層級

- 純查詢與 World 查詢：(a) 同一行程、(b) 同一平台都成立（純函式、走訪順序固定）。(c) 三平台逐位元：設計上成立（4.2 的數值政策），由 golden 測試守（L1 三平台：linux-x64、windows-x64、macos-arm64）。master 11-09 的 1～4 ULP 差異原因未明，所以 golden 是這一條唯一的證據；golden 在某平台失敗時照回滾頁的 RC「回滾等價破裂」同樣停下回報，不在物理裡繞過。

## 5. 偏離 Bevy 生態與接合點（批次表〈每批的設計審查〉第 5 項）

讀的版本（本機 `~/Code/GitHome`，2026-11-23 下載，使用者同意 `v8-arch-gyo2-physics-reference-downloads`）：avian e496f4a、parry b3c3584、rapier 475caa3、JoltPhysics 1a14e5e。

| 這一批 | avian／parry／rapier／Jolt（檔名:行） | 偏離與理由 |
|---|---|---|
| 形狀 `Ball`、`Capsule{a, b, radius}`＋`CapsuleY`、`Cuboid{halfExtents}` | parry `Ball`（`src/shape/ball.rs:46`）、`Capsule{segment, radius}`（`src/shape/capsule.rs:71-82`）與 `Capsule::new_y`（`:141`）、`Cuboid`（`src/shape/cuboid.rs:66`）；avian `Collider::capsule`（沿 Y，`src/collision/collider/parry/mod.rs:833`）、`capsule_endpoints`（`:843`） | 照 parry（設計審查 S2、N4 之後）。`Ball` 的名字避開 `Math::Sphere` |
| `BoxSet`（靜態 BVH） | parry `Compound` 帶 `Bvh`（`src/shape/compound.rs:51-53`、`:177` 用 `BvhBuildStrategy::Binned`）；parry `Voxels`（`src/shape/voxels/voxels.rs:499`）、avian `Collider::voxels`（`mod.rs:1050`） | 對應 `Compound`：體素工具烘出的是合併過、大小不一的方塊，不是等大的體素格。不另立子形狀，只放 AABB |
| 查詢結果與選項 | parry `RayIntersection`（`src/query/ray/ray.rs:289`）、`Contact`（`src/query/contact/contact.rs:67`）、`ShapeCastHit`／`ShapeCastStatus`（`src/query/shape_cast/shape_cast.rs:10-80`）、`ShapeCastOptions`（`:129-150`：`max_time_of_impact`、`target_distance`、`stop_at_penetration`、`compute_impact_geometry_on_penetration`）、`Unsupported`（`src/query/error.rs:147`） | 名字與語意照 parry。偏離：(1) 點與法線一律世界座標（parry 是各形狀的局部座標，`shape_cast.rs:33-42`）——呼叫者不必再轉；(2) 一開始重疊時也一律算接觸幾何（parry 要 `compute_impact_geometry_on_penetration`）——形狀簡單、成本低，少一個選項；(3) 狀態只有 `Converged`、`PenetratingOrWithinTargetDistance`：閉式解沒有 `OutOfIterations`、`Failed`；(4) 射線只有 `solid`（parry 的 `solid` 參數，`ray.rs:246-248`）：PvP 只用實心 |
| 空間查詢 | avian `SpatialQuery` 系統參數（`src/spatial_query/system_param.rs:60-65`：讀 `Position`、`Rotation`、`Collider`，加 `ColliderTrees`）、`cast_ray`（`:124`，`direction: Dir` 單位向量＋`max_distance`、`solid`）、`cast_ray_predicate`（`:200`）、`cast_shape`（`:478`）、`shape_intersections`（`:1245`）、`SpatialQueryFilter`（`src/spatial_query/query_filter.rs:35-40`：層的遮罩＋排除的實體）、`RayHitData`／`ShapeHitData`（`ray_caster.rs:403`、`shape_caster.rs:539`）、`ShapeCastConfig`（`shape_caster.rs:370`） | Kernel 沒有自訂系統參數：改成接作者 Query 的函式（膠水：每個函式一個模板參數）。`filter` 只做 avian 的 predicate 版本，層的遮罩不做（PvP 用 predicate 就能排除玩家；`CollisionLayers` 等有第二個用法再加）。射線的 `t` 照 GYO 的 `Math::Ray`（|direction| 為單位）、掃掠用 parry 的「位移的比例」而不是 avian 的單位方向＋距離：不在 Engine 裡正規化方向（正規化會多一次捨入） |
| 物理姿態 | avian 有自己的 `Position`／`Rotation`（`src/physics_transform/transform.rs:45`、`:157`），在物理之前從 `Transform` 同步、之後寫回（`src/physics_transform/mod.rs:31-44`、`:83-102`） | 不另立物理姿態元件：GYO 的 `Transform` 本來就是判定用的值（CCR-12），另立會多一條同步。avian 另立的理由是剛體的解算要自己的姿態，有剛體時重看（RC-P3-3） |
| 寬相位 | avian `ColliderTrees`（`src/collider_tree/mod.rs:123`，每步維護） | 這一批線性走訪實體（PvP：玩家 ≤ 64、地圖 1 個 `BoxSet`）；RC-P3-2 |
| 決定性 | avian／parry 的 `enhanced-determinism`（`crates/avian3d/Cargo.toml:30-35`、`src/lib.rs:67`；用軟體的超越函式）；Jolt `CROSS_PLATFORM_DETERMINISTIC` 時不開 FMA（`Jolt/Jolt.cmake:868-871`、`:968`） | 不用超越函式，不需要軟體 libm；FMA 已由 GYO 的 `-ffp-contract=off` 關掉（同 Jolt 的做法）。內部 double：Jolt 的 `DOUBLE_PRECISION`（`Jolt.cmake:864-866`）、parry 的 f64 版本都是為大世界 |
| 角色控制器（下一步） | rapier `KinematicCharacterController`（`src/control/character_controller.rs:173`）；avian `MoveAndSlide` 系統參數（`src/character_controller/move_and_slide.rs:68`、`MoveAndSlideConfig` `:93`、`move_and_slide` `:483`） | 這一批不做；下一步照這兩家做 |

## 6. 測試、守衛、突變

- ctest `physics_3d.queries`（純查詢）、`physics_3d.box_set`（等價）、`physics_3d.golden`（三平台字面位元）、`physics_3d.world`（World 查詢、`Without<ChildOf>` 的編譯期要求、縮放的錯誤、回滾重跑後結果相同）。標籤 `cpu`。
- 依賴守衛：`gyo_expect_links(gyo_physics_3d GYO::Base GYO::Kernel GYO::Math GYO::Orchestration GYO::Transform)`。
- 突變（D184：只跑相關的）：等價契約（BVH 剪枝用 `<` 換 `<=` 的反向、平手取大索引）、`stopAtPenetration` 的分離判定、`Collider` 的雜湊漏掉 `BoxSet` 摘要、World 查詢讀 `GlobalTransform`。

## 7. 之後（定方向，不寫程式）

- **剛體**：照 avian：`RigidBody{Dynamic, Kinematic, Static}`、速度元件、`PhysicsSchedule` 掛在固定步（world_time 的固定步 schedule）裡；接觸事件用 Kernel 的 `Message`（同一步內寫完讀完，回滾 R-2 前提 4）。要有產品點名才做。
- **子實體 collider**：從 `ChildOf` 與每一層的 `Transform` 合成姿態（CCR-12）；avian 是 `ColliderOf`＋`ColliderTransform`。
- **每步的寬相位**：實體多到線性走訪不夠時，加一個 Resource 的 BVH，在固定步裡重建或增量更新。
- **角色控制器（推出 → 掃掠 → 滑動）**：PvP 移植準備卡判它屬 Engine 的機制（參數：迭代數、皮膚寬度、著地探測、地面法線門檻由作者給；Bevy 生態 rapier 的 `KinematicCharacterController`、avian 的 move-and-slide、Godot `move_and_slide`）。本卡同意方向，排在**下一步**：照 rapier／avian 的原始碼做（等使用者同意下載），不照 master 的 `MoveCharacterBody`。這一批先把它要用的查詢做好。
- **一般組合**：GJK／EPA（parry 的做法）。

## 8. Design Invariants 的草案（寫進 owner 文件 `physics-3d.zh-Hant.md`）

- **P3-1 職責**：形狀、純查詢、`BoxSet`、`Collider` 與空間查詢函式。不做：模擬步、回滾／權威的登記、角色移動規則、畫面。
- **P3-2 判定只讀 `Transform`**：World 查詢的姿態只從 `Transform` 來，不讀 `GlobalTransform`（CCR-12）。
- **P3-3 `BoxSet` 等於線性掃描**：第 4.4 節的契約。
- **P3-4 不支援就說不支援**：沒有實作的組合回 `Unsupported`、縮放回 `UnsupportedScale`，不回近似值。
- **P3-5 數值**：第 4.2 節；三平台 golden。
- **P3-6 依賴**：只依賴 base、math、kernel、orchestration、transform。

## 9. 卡片照 L17 判的（不是使用者的話；可以推翻）

1. 目錄名 `physics_3d`、namespace `Physics3d`（照 camera 的 `Camera3d`，不加縮寫表）。
2. 預設不帶（`default=no`）。
3. 內部 double、結果 float（理由是大地圖的精度，第 4.2 節；不是照 master，PvP 移植準備卡提醒不照搬）。
4. 這一批不做剛體與角色控制器。
5. 子實體 collider 先不支援、用 `Without<ChildOf>` 在編譯期擋（這是契約不是防誤用：姿態的合成還沒做）。

## 10. 範圍の限界

- 三平台逐位元的證據只有 golden；Intel Mac（macos-x64）只本機跑。

## 11. 實作時和本宣告的差異（修訂 1）

1. 函式名：宣告的 `Contact(...)` 改叫 `FindContact`（和結構 `Contact` 同名會遮住型別名）；`BoxSet` 用的是 `FindDeepestBoxContact`、`FindBoxContacts`。
2. `BoxSet` 的 `==`：摘要與個數相同之後再逐位元比內容（比宣告嚴，和雜湊一致）。
3. 點與法線一律世界座標（parry 的 `ShapeCastHit` 是各形狀的局部座標）。
4. 兩端重合的膠囊當球，掃掠不看方向。`Cuboid` 的 `halfExtents` ≥ 0（厚度 0 可以，和 `BoxSet` 的 min ≤ max 一致）。
5. World 查詢只對 `filter` 通過的實體查縮放與組合；有一個組合不支援就整個回 `Unsupported`（作者用 `filter` 排除玩家）。
6. 核進到方塊裡的接觸取「沿面法線推出量最小的面」（平手 −X、+X、−Y、+Y、−Z、+Z），不一定是真正的最小移動量；兩核重合時法線 +X（同 parry）。
7. 射線、掃掠對 `BoxSet` 的結果不帶方塊索引（PvP 沒用到；接觸有）。

## 12. 第三個 PR：`BoxSet` 的內部邊（S1）的事前判定（修訂 2，2026-11-23，動工前寫定）

主會話 2026-11-23：照 parry 複合形狀的 `FIX_INTERNAL_EDGES`（parry b3c3584 `src/shape/compound.rs:30-40`），`BoxSet` 加一個作者可選的旗標；把 #311 裡 skip 的量測案例改成正式判定，閾值在看結果之前定好。

**判定（先宣告，結果出來之後不放寬）**——情境同 #311 的量測：方塊 `[-2,0]×[-1,0]×[-1,1]` 與 `[0,2]×[-1,0]×[-1,1]`，`config.planes = {(0,1,0)}`，`CapsuleY(0.5, 0.5)` 從 y＝1.01（皮膚高度）出發，速度 (∓3, −1, 0)，`deltaSeconds` 1/60，60 步，兩個方向各一次：

1. 旗標打開：每一步的 y ≤ 1.01 ＋ 1×10⁻⁵ m（抬高量閾值 0.01 mm；#311 量到 8.8 mm）。
2. 旗標打開：每一步的 |水平速度| ≥ 2.997（掉速閾值 0.1%；#311 量到 10%）。
3. 旗標關閉：照舊（行為和 #311 逐位元相同；量到的抬高仍在，證明旗標是開關而不是改了預設行為之外的東西）。
4. 真的外緣不被抹掉：單獨一個方塊的頂邊（旁邊沒有方塊）在旗標打開時仍給出傾斜的接觸法線（走出平台邊緣時照樣感覺得到邊）。
5. 既有測試（含兩份 golden）在旗標關閉時全部不變。

旗標的預設值照 parry：關閉（parry b3c3584 `src/shape/compound.rs` 的 `Compound::new` 設 `flags: CompoundFlags::empty()`，`CompoundFlags` 的 `Default` 也是空；`FIX_INTERNAL_EDGES` 只由 `with_flags`／`set_flags` 打開），所以 `BoxSet::Build(boxes)` 等於 `Build(boxes, BoxSetFlags::None)`；不因判定結果改。

**修訂 3（2026-11-23，主會話判）：預設改成打開。** 這是預設值的設計選擇，不是放寬判定：判定 1～5 不變，打開與關閉兩種情形都測。理由：`BoxSet` 的用途是體素地圖，parry／avian 的 `Voxels` 形狀沒有開關、永遠以鄰居分類避開內部邊（parry b3c3584 `src/shape/voxels/voxels.rs:14-28`）；`Compound` 預設關閉的理由（任意凸形、焊接容差的誤判、舊行為相容）在 `BoxSet` 大多不成立；AGENTS §1-10「既定値は便利にし」；預設關閉時產品忘了打開，S1 會悄悄回來。§12 原本那句「照 parry 定」是卡片自己寫的，由主會話判（新 AGENTS §2.6）。
