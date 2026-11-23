# 3D 物理（`engine/modules/physics_3d`）

更新：2026-11-23（新頁，`v8-arch-gyo2-physics-3d`）。由來：[事前宣告](physics-3d-declaration.zh-Hant.md)（GYO2 主會話開的任務卡「3D 物理」，薄起步）。**Design Invariants 是草稿，合併前核准定稿。**

Owner：架構。程式：`engine/modules/physics_3d/`（`GYO::Engine::Physics3d`，target `GYO::Physics3d`，登記單位 `Physics3dModule`）。行為測試：`tests/common/physics_3d/`。

## 一個例子

FPS 的地圖是一個實體：`Collider{BoxSet}`（體素工具烘出的 1,260 個方塊）＋`Transform`。每個玩家是一個實體：`Collider{CapsuleY(halfHeight, radius)}`＋`Transform`。

- 走路：產品的移動規則在固定步裡呼叫 `Physics3d::MoveAndSlide(colliders, CapsuleY(…), pose, velocity, dt, config, filter)`，`filter` 排除玩家（只掃牆），把回來的 `position` 寫回 `Transform`；站不站在地上由產品自己判（往下 `CastMove` 加自己的法線門檻），站著時把地面平面放進 `config.planes`。
- 開槍：`Physics3d::CastRay(colliders, ray, range, filter)` 拿最近的牆；hitbox 不是實體，對每顆骨頭上的球、膠囊直接用 `CastRay(Capsule{a, b, radius}, Pose{}, ray, range)`（世界座標的線段膠囊放在單位姿態；球是 `Ball` 放在球心的姿態）。
- 出生：`Intersections(colliders, Capsule{…}, pose, filter)` 是空的就放得下。
- 回滾：查詢只讀 `Transform` 與 `Collider`，重跑時結果逐位元相同。

## 1. 位置與依賴

```text
GYO::Base、GYO::Math ◄── GYO::Kernel ◄── GYO::Orchestration ◄── GYO::Transform ◄── GYO::Physics3d
```

- 登記檔一列：`library_only=no`、`default=no`、`depends=transform`。
- 標頭：`Shapes.hpp`（`Ball`、`Capsule`、`CapsuleY`、`Cuboid`、`BoxSet`、`Shape`、`Pose`、`IsValid`）、`Queries.hpp`（純查詢）、`Collider.hpp`、`SpatialQuery.hpp`（接作者 Query 的函式）、`MoveAndSlide.hpp`（運動學角色的機制）、`Physics3dModule.hpp`、`Physics3dError.hpp`。
- 純查詢不需要 World，工具與測試也能用。

## 2. 用法與後果

- **形狀**：`Ball{radius}`（照 parry 的名字，和 `Math::Sphere` 分開）、`Capsule{a, b, radius}`（局部線段＋半徑，照 parry；任意方向；兩端重合是球；`CapsuleY(halfHeight, radius)` 建沿局部 Y 的那顆）、`Cuboid{halfExtents}`（≥ 0，厚度 0 可以）、`BoxSet`（`Build(span<const Aabb>)` 回 `Result`；不可變、共用；建立時算內容摘要；階層的佈局在 Module 內部的 `src/BoxSetData.hpp`，不在介面）。
- **姿態** `Pose{translation, rotation}`：`Transform` 去掉縮放。
- **純查詢**：`CastRay`、`FindContact`、`FindDeepestBoxContact`、`FindBoxContacts`、`CastShape`。結果型別照 parry：`RayHit{t, normal}`、`Contact{point1, point2, normal1, normal2, distance}`、`ShapeHit{time, witness1, witness2, normal1, normal2, status}`；**點與法線都是世界座標**（parry 是各形狀的局部座標）。`t` 以 |direction| 為單位（同 `Math::Intersect`）。碰到就算（touching counts）。
- **支援的組合**：

  | 查詢 | 形狀 1 | 形狀 2 | 條件 |
  |---|---|---|---|
  | `CastRay` | — | 四種 | 任意姿態（hitbox：`Capsule{a, b, r}` 放在 `Pose{}`） |
  | `FindContact` | 球、膠囊 | 四種 | 任意姿態 |
  | `CastShape` | 球 | 方塊、`BoxSet` | 任意姿態 |
  | `CastShape` | 膠囊 | 方塊、`BoxSet` | 膠囊的線段（`b - a`）在方塊的局部座標裡恰有兩個分量為 0，或兩端重合（當球） |

  其餘回 `Unsupported`。對球、膠囊的掃掠（角色互推）PvP 沒用到。
- **核進到方塊裡的接觸**：沿面法線推出量最小的面（平手依 −X、+X、−Y、+Y、−Z、+Z）；不一定是真正的最小移動量。兩個核重合時法線是 +X（同 parry）。
- **World 查詢**：作者自己的 Query 要帶 `Read<Collider>`、`Read<Transform::Transform>`、`Without<Kernel::ChildOf>`（少了 `Without<ChildOf>` 是編譯錯誤 `GYO_PHYSICS3D_REQUIRE_WITHOUT_CHILD_OF`：子實體的姿態合成還沒做）。依 EntityId 遞增走訪；最近者平手取 EntityId 小的。`filter` 通過的實體裡：
  - `Transform::scale` 不是逐位元的 (1,1,1)：回 `UnsupportedScale`（被 `filter` 排除的不看）；
  - 有一個組合不支援：整個查詢回 `Unsupported`（detail 帶那個實體）。地圖與玩家同在一個 World 時，掃掠要用 `filter` 排除玩家。
- **`Collider` 的 `==`** 逐位元，`BoxSet` 先比摘要與個數、相同再逐位元比內容；雜湊收種類、各欄、`BoxSet` 的個數與摘要（CCR-13）。回滾、權威要它時作者自己登記（同 `Transform`）。
- **數值**：內部 double、每個結果捨入成 float 一次；只用 + − × ÷、`sqrt`、比較、`abs`。精確值不是 float 能表示的就照捨入出來（例：接觸點的分量可能是 −4.4e−16，不是 0）。

## 3. MoveAndSlide（運動學角色的機制）

照 avian 的 `MoveAndSlide`（`src/character_controller/move_and_slide.rs`、`velocity_project.rs`），同樣是接作者 Query 與 `filter` 的函式：

- `ProjectVelocity(v, normals)`：純函式，avian 的暴力版（不是對偶解法）：平面數少（`maxPlanes` 20）、沒有自己的迭代與容差，結果只看法線的順序。
- `CastMove(colliders, shape, pose, movement, skinWidth, filter)` → `Result<optional<MoveHit>>`：沿位移多掃 `skinWidth`，起點的重疊只在往裡走時算（`stopAtPenetration = false`），退回到沿法線離 `skinWidth`（`skinWidth / max(dot, 0.005)`）。`distance` 是可走的距離，`collisionDistance` 是不算皮膚的距離（avian 的程式把它填成掃掠長度，這裡照它文件的意思填）。相機防穿牆也用它。算術（掃掠長度、退回、`MoveHit`）在 `CastMoveWith(movement, skinWidth, cast)`，`cast` 是要用的形狀掃掠（`CastMove` 傳 World 的 `CastShape`）。
- `Depenetrate(colliders, shape, pose, DepenetrationConfig, filter)` → `Result<Vec3>`：收 `skinWidth` 內的接觸（每個 collider 一個、`BoxSet` 每個方塊一個，`SpatialQuery.hpp` 的 `ForEachContact`），Gauss-Seidel 推出；比 `penetrationRejectionThreshold` 深的接觸不推。
- `MoveAndSlide(colliders, shape, pose, velocity, deltaSeconds, config, filter, onHit)` → `Result<MoveAndSlideOutput{position, projectedVelocity}>`：先推出；最多 `moveAndSlideIterations` 次「掃到第一個碰撞、走到那裡、收碰撞平面與 `2 × skinWidth` 內的接觸平面（相近的平面合併、留比較擋路的那個；最多 `maxPlanes`）、把速度投影上去」；最後再推出一次。`onHit` 回 `Accept`／`Ignore`／`Abort`，可以改法線、位置、速度。預設值同 avian（4、16、1e−4、0.5、0.01、空、cos 5°、20）；推出的四個欄位是 `config.depenetration`（一個 `DepenetrationConfig`，avian 是重複的四欄）；沒有長度單位資源（GYO 的單位是作者的公尺）。avian 給接觸平面的 `onHit` 傳的是掃掠命中的實體，這裡傳接觸那個實體；接觸平面的 `point` 是 collider 那一側的點（`Contact::point2`），avian 用兩個表面最近點的中點（`collision/contact_types/mod.rs:613-619`）。
- **作者的規則不在這裡**（§1-1）：站地判定、坡度、台階、貼地、重力。rapier 的 `KinematicCharacterController` 把它們綁在一起，這裡不做；PvP 盤點沒有台階與坡。放置檢查用 `Intersections`。
- 不支援的組合照樣回錯：對球、膠囊 collider 掃掠是 `Unsupported`，角色自己與其他角色用 `filter` 排除。
- 走訪的順序固定（實體依 EntityId、方塊依索引），同樣的輸入給同樣的位元；golden 一份（`physics_3d.move_and_slide`）。
- **`BoxSet` 的內部邊（S1，已用旗標修好）。** 兩個天面同高、相鄰的方塊（`[-2,0]×[-1,0]×[-1,1]` 與 `[0,2]×[-1,0]×[-1,1]`），`config.planes = {(0,1,0)}`，`CapsuleY(0.5, 0.5)` 在皮膚高度（y＝1.01）以速度 (∓3, −1, 0)、1/60 秒走 60 步越過接縫：不開旗標時最大 y 1.01882386（**高出 8.8 mm**）、最小水平速度 2.70010996（**慢約 10%**）；原因是每個方塊各算最近點，相鄰方塊的頂邊在 `2 × skinWidth` 內時給出約 11° 傾斜的接觸平面，超過 cos 5° 不和地面合併。`BoxSet::Build(boxes, BoxSetFlags::FixInternalEdges)`（照 parry 複合形狀的 `FIX_INTERNAL_EDGES`，`src/shape/compound.rs:30-40`、`490-636`，夾法在 `compound_pseudo_normals.rs`）打開後：抬高 0、最小水平速度 3（判定在[事前宣告](physics-3d-declaration.zh-Hant.md)第 12 節，閾值先寫好）。**怎麼選**：體素地圖、拼起來的地板與牆打開；預設關閉（同 parry）。**規則**（逐位元、沒有容差；parry 預設在 4 ULP 內焊接頂點，這裡不焊）：方塊 j 在方塊 i 的面（軸 a、＋側）上的點 p 延續該面，當 j 在 a 的最小值等於 i 在 a 的最大值（float 相等；−側反過來），且 j 在另外兩軸的閉區間包含 p。接觸與掃掠的法線在被延續的面的軸上的分量去掉、重新正規化（全部都要去掉時照原樣）；在方塊裡的形狀從沒被延續的面中推出量最小的那面出去（都被延續時照舊）。只改法線，距離與點不變。建立時跑一次找每個面的鄰居（存在隱藏的 `BoxSetData`）；旗標算進摘要、`==`、`Collider` 的雜湊。**限制**：只認恰好貼齊的鄰居（差 1 ULP 就不算）；射線的法線不夾（射線只給面法線）；鄰居只看同一個 `BoxSet` 內，跨 `BoxSet` 或與 `Cuboid` 之間的接縫不處理。

## Design Invariants（草稿）

守衛欄：行為測試（ctest `physics_3d.queries`、`physics_3d.box_set`、`physics_3d.golden`、`physics_3d.world`、`physics_3d.move_and_slide`）與突變（`docs/architecture/target/v2/scripts/physics_3d_mutations.json`，`p3-`）。

<a id="p3-1"></a>
**P3-1 職責。** 形狀、純查詢、`BoxSet`、`Collider`、空間查詢函式與運動學角色的機制（`MoveAndSlide`、`CastMove`、`Depenetrate`、`ProjectVelocity`）。不做：模擬步與剛體、回滾／權威的登記、角色移動規則（站地、坡度、台階、貼地、重力是作者的）、畫面。`Physics3dModule` 只登記 `Collider`。

<a id="p3-2"></a>
**P3-2 判定只讀 `Transform`。** World 查詢的姿態只從 `Transform` 來、不讀 `GlobalTransform`（CCR-12）；縮放不是 (1,1,1) 回錯、不默默忽略。守衛：`physics_3d.world`；突變 `p3-scale-not-checked`；審查。

<a id="p3-3"></a>
**P3-3 加速結構不改結果。** `BoxSet` 的每個查詢，和依索引 0…n−1 對每個方塊做同一個單方塊查詢（最近者、平手取索引小的；或依索引遞增收集全部）的結果逐位元相同；`FixInternalEdges` 打開時，單方塊查詢帶那個方塊建立時找到的面鄰居（每方塊的資料，和階層無關）。守衛：`physics_3d.box_set` 的亂數語料等價測試（含旗標打開的一組）；突變（平手、剪枝、候選範圍）。

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
- **RC-P3-4**：工具要用純查詢、又不想帶進 Kernel 時（設計審查 N3）：把形狀與純查詢拆成 library-only 的 target，`Collider` 與空間查詢留在 Module。

## 範囲の限界

- 沒有剛體、角色移動規則（只有 move-and-slide 的機制）、子實體 collider、縮放、半空間、凸包、三角網格。
- `BoxSet` 的內部邊只在 `BoxSetFlags::FixInternalEdges` 時處理，且只認同一個 `BoxSet` 內恰好貼齊的鄰居（第 3 節）。
- golden 是在 Intel Mac（macos-x64）產生的；三平台由 L1 確認。
- 和 avian、parry、rapier、Jolt 的對照（檔名行號）在[事前宣告](physics-3d-declaration.zh-Hant.md)第 5 節。
