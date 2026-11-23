# 3D 物理（`engine/modules/physics_3d`）

更新：2026-11-23（新頁，`v8-arch-gyo2-physics-3d`）。由來：[事前宣告](physics-3d-declaration.zh-Hant.md)（GYO2 主會話開的任務卡「3D 物理」，薄起步）。**Design Invariants 是草稿，合併前核准定稿。**

Owner：架構。程式：`engine/modules/physics_3d/`（`GYO::Engine::Physics3d`，target `GYO::Physics3d`，登記單位 `Physics3dModule`）。行為測試：`tests/common/physics_3d/`。

## 一個例子

FPS 的地圖是一個實體：`Collider{BoxSet}`（體素工具烘出的 1,260 個方塊）＋`Transform`。每個玩家是一個實體：`Collider{Capsule{halfHeight, radius}}`＋`Transform`。

- 走路：產品的移動規則在固定步裡呼叫 `Physics3d::CastShape(colliders, Capsule{…}, pose, motion, options, filter)`，`filter` 排除玩家（只掃牆），拿第一個接觸的比例與法線，自己決定滑動。
- 開槍：`Physics3d::CastRay(colliders, ray, range, filter)` 拿最近的牆；hitbox 不是實體，對每顆骨頭上的球、膠囊直接用 `CastRay(ray, range, Math::Capsule{…})`。
- 出生：`Intersections(colliders, Capsule{…}, pose, filter)` 是空的就放得下。
- 回滾：查詢只讀 `Transform` 與 `Collider`，重跑時結果逐位元相同。

## 1. 位置與依賴

```text
GYO::Base、GYO::Math ◄── GYO::Kernel ◄── GYO::Orchestration ◄── GYO::Transform ◄── GYO::Physics3d
```

- 登記檔一列：`library_only=no`、`default=no`、`depends=transform`。
- 標頭：`Shapes.hpp`（`Sphere`、`Capsule`、`Cuboid`、`BoxSet`、`Shape`、`Pose`、`IsValid`）、`Queries.hpp`（純查詢）、`Collider.hpp`、`SpatialQuery.hpp`（接作者 Query 的函式）、`Physics3dModule.hpp`、`Physics3dError.hpp`。
- 純查詢不需要 World，工具與測試也能用。

## 2. 用法與後果

- **形狀**：`Sphere{radius}`、`Capsule{halfHeight, radius}`（沿局部 Y；`halfHeight` 為 0 是球）、`Cuboid{halfExtents}`（≥ 0，厚度 0 可以）、`BoxSet`（`Build(span<const Aabb>)` 回 `Result`；不可變、共用；建立時算內容摘要）。
- **姿態** `Pose{translation, rotation}`：`Transform` 去掉縮放。
- **純查詢**：`CastRay`、`FindContact`、`FindDeepestBoxContact`、`FindBoxContacts`、`CastShape`。結果型別照 parry：`RayHit{t, normal}`、`Contact{point1, point2, normal1, normal2, distance}`、`ShapeHit{time, witness1, witness2, normal1, normal2, status}`；**點與法線都是世界座標**（parry 是各形狀的局部座標）。`t` 以 |direction| 為單位（同 `Math::Intersect`）。碰到就算（touching counts）。
- **支援的組合**：

  | 查詢 | 形狀 1 | 形狀 2 | 條件 |
  |---|---|---|---|
  | `CastRay` | — | 四種 | 任意姿態；另有世界座標 `Math::Sphere`、`Math::Capsule` 的多載 |
  | `FindContact` | 球、膠囊 | 四種 | 任意姿態 |
  | `CastShape` | 球 | 方塊、`BoxSet` | 任意姿態 |
  | `CastShape` | 膠囊 | 方塊、`BoxSet` | 膠囊的軸在方塊的局部座標裡恰有兩個分量為 0 |

  其餘回 `Unsupported`。對球、膠囊的掃掠（角色互推）PvP 沒用到。
- **核進到方塊裡的接觸**：沿面法線推出量最小的面（平手依 −X、+X、−Y、+Y、−Z、+Z）；不一定是真正的最小移動量。兩個核重合時法線是 +X（同 parry）。
- **World 查詢**：作者自己的 Query 要帶 `Read<Collider>`、`Read<Transform::Transform>`、`Without<Kernel::ChildOf>`（少了 `Without<ChildOf>` 是編譯錯誤 `GYO_PHYSICS3D_REQUIRE_WITHOUT_CHILD_OF`：子實體的姿態合成還沒做）。依 EntityId 遞增走訪；最近者平手取 EntityId 小的。`filter` 通過的實體裡：
  - `Transform::scale` 不是逐位元的 (1,1,1)：回 `UnsupportedScale`（被 `filter` 排除的不看）；
  - 有一個組合不支援：整個查詢回 `Unsupported`（detail 帶那個實體）。地圖與玩家同在一個 World 時，掃掠要用 `filter` 排除玩家。
- **`Collider` 的 `==`** 逐位元，`BoxSet` 先比摘要與個數、相同再逐位元比內容；雜湊收種類、各欄、`BoxSet` 的個數與摘要（CCR-13）。回滾、權威要它時作者自己登記（同 `Transform`）。
- **數值**：內部 double、每個結果捨入成 float 一次；只用 + − × ÷、`sqrt`、比較、`abs`。精確值不是 float 能表示的就照捨入出來（例：接觸點的分量可能是 −4.4e−16，不是 0）。

## Design Invariants（草稿）

守衛欄：行為測試（ctest `physics_3d.queries`、`physics_3d.box_set`、`physics_3d.golden`、`physics_3d.world`）與突變（`docs/architecture/target/v2/scripts/physics_3d_mutations.json`，`p3-`）。

<a id="p3-1"></a>
**P3-1 職責。** 形狀、純查詢、`BoxSet`、`Collider` 與空間查詢函式。不做：模擬步與剛體、回滾／權威的登記、角色移動規則、畫面。`Physics3dModule` 只登記 `Collider`。

<a id="p3-2"></a>
**P3-2 判定只讀 `Transform`。** World 查詢的姿態只從 `Transform` 來、不讀 `GlobalTransform`（CCR-12）；縮放不是 (1,1,1) 回錯、不默默忽略。守衛：`physics_3d.world`；突變 `p3-scale-not-checked`；審查。

<a id="p3-3"></a>
**P3-3 加速結構不改結果。** `BoxSet` 的每個查詢，和依索引 0…n−1 對每個方塊做同一個單方塊查詢（最近者、平手取索引小的；或依索引遞增收集全部）的結果逐位元相同。守衛：`physics_3d.box_set` 的亂數語料等價測試；突變（平手、剪枝、候選範圍）。

<a id="p3-4"></a>
**P3-4 不支援就說不支援。** 沒實作的組合回 `Unsupported`，不回近似值。守衛：`physics_3d.queries`、`physics_3d.world`。

<a id="p3-5"></a>
**P3-5 數值。** 第 2 節的數值政策；三平台（linux-x64、windows-x64、macos-arm64）同一份 golden（字面 binary32）。守衛：`physics_3d.golden`。golden 在某平台失敗時停下回報，不在物理裡繞過。

<a id="p3-6"></a>
**P3-6 依賴。** 只依賴 base、math、kernel、orchestration、transform。守衛：`gyo_expect_links(gyo_physics_3d GYO::Base GYO::Kernel GYO::Math GYO::Orchestration GYO::Transform)`。

### Replacement Criteria

- **RC-P3-1**：產品要的掃掠組合需要一般方向（GJK／EPA）時，重看形狀與查詢的分派（parry 的 `QueryDispatcher`）。
- **RC-P3-2**：一個 World 的 collider 多到線性走訪實體不夠（計測）：加每步維護的寬相位。
- **RC-P3-3**：要剛體或子實體 collider：要不要另立物理姿態元件（avian 的 `Position`／`Rotation`）在那一批重看。

## 範囲の限界

- 沒有剛體、角色控制器（下一步，照 rapier／avian）、子實體 collider、縮放、半空間、凸包、三角網格。
- golden 是在 Intel Mac（macos-x64）產生的；三平台由 L1 確認。
- avian、parry、rapier 的對照依記憶，未對原始碼（下載等使用者同意）。
