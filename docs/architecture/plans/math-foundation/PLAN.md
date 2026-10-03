# GYO Math 基礎統一整改計劃（核准版）

核准：2026-10-03。Owner：Engine（新增模組 `GYO::Math`）。進度見 [README](README.md)，紀錄見 [HANDOFF](HANDOFF.md)。

## Context

GYO 目前沒有共用的數學基礎。同一個概念在各處各自定義：
- `Render::Float3`、`Model::Vec3`、`Collision::Float3`、`fps::Float3` 四份同構的 float3。
- `Render::Float2`、`Model::Vec2`、`UiFloat2` 三份 float2。
- `ShaderAbi::Matrix4` 是 row-major、預設零矩陣；`Model::Matrix4` 是 column-major、預設單位矩陣。兩者的 `Multiply` 參數順序語義相反。
- `Length`、`IsFinite`、`Normalize`、sRGB、角度換算、yaw wrap 等運算在 engine 和 pvp 中各寫多份，寫法不一（hypot 和 sqrt、有無零長度防護、`d*(pi/180)` 和 `d*pi/180`）。

使用者要求把 Math 固定為 Engine 的基礎。範圍是整個 GYO，但不含未啟用產品。

**已定案的方針（2026-10-03 使用者回答）**
1. 舊名稱完全移除。engine 與 active 程式碼只使用 `Engine::Math::*`，不留別名。
2. 數值實作全面統一：每個運算只保留一種實作，接受浮點結果改變，但每處改變都要量測並記錄。
3. Math 定位為完整基礎庫，Architecture Delta 中逐項說明收錄理由。
4. pvp 那 29 個從未被編譯的 .cpp 一起遷移。

**範圍**
- 包含：`engine/*`、`apps/object_fps_pvp`（含 `tests/object_fps_pvp`、`build/acceptance/object_fps_pvp`）、`tools/ui_editor`（含 `tests/ui_editor`）、`tests/common`、docs。
- 排除，一行都不改：`apps/object_fps/**`、`apps/object_fps_v2/**`、`tests/object_fps/**`、`tests/object_fps_v2/**`、`build/acceptance/object_fps{,_v2}/**`、`docs/object_fps/**`、`docs/object_fps_v2/**`、`tools/object_fps_preview/**`、兩份 registry csv。
- 規劃依據：ultracode 規劃 workflow 的結果（3 個盤點、3 個方案、1 個評審、1 次對抗式檢查）。對抗式檢查提出的 2 個 major 和 10 個 minor 都已納入本計劃。

---

## 1. Math 模組契約

### 1.1 位置與建置

- **位置**：header 依使用者的樹狀分類分子目錄，每個型別或主題一個 header，不做 umbrella header；namespace 統一為 `Engine::Math`，不分子 namespace。
  ```text
  engine/math/include/engine/math/
  ├─ linear/    Vec2.hpp  Vec3.hpp  Vec4.hpp  VecInt.hpp（Vec2i／Vec3i）  Vec3d.hpp
  │             Matrix3.hpp  Matrix4.hpp  Quaternion.hpp
  ├─ geometry/  Ray.hpp  Plane.hpp  Sphere.hpp  Aabb.hpp  Capsule.hpp  Segment.hpp  Triangle.hpp  Rect.hpp
  └─ scalar/    Scalar.hpp（Clamp／Lerp／Min／Max）  Constants.hpp  Angle.hpp  ColorSpace.hpp
  ```
  確切檔名於 B1 實作時定稿，分組不變。
- **target**：`gyo_math`，INTERFACE、header-only，alias 為 `GYO::Math`。只依賴 C++ 標準函式庫，是最底層的 leaf，沒有對外依賴。範本參考 `engine/text/CMakeLists.txt`，但不連結 Engine。
- **註冊**：`engine/CMakeLists.txt` 在第一個 `add_subdirectory(model)`（:73）之前加入 `add_subdirectory(math)`。
- **依賴邊**：

  | 連結方 | 方式 |
  |---|---|
  | Collision、Model、Render、Ui | PUBLIC |
  | pvp `match_domain`（`apps/object_fps_pvp/CMakeLists.txt:27`） | 明確 PUBLIC |
  | ui_editor 的 `gyo_ui_editor_preview`（`tools/ui_editor/CMakeLists.txt:55-60`） | 明確 PRIVATE |
  | `GYO::Engine`、Input、Text | 不連結 |

### 1.2 型別

命名（使用者決定）：向量只保留單一型別 `Vec2/Vec3/Vec4`，純量型別用後綴：`Vec3d`（double）、`Vec2i/Vec3i`（int）。點與方向不分型別，由 `TransformPoint`／`TransformVector` 區分。

全部是 `struct final` aggregate，預設 float32（`Vec3d`、`Vec2i/Vec3i` 例外），不加 `alignas`，並用 static_assert 鎖住大小、alignof、trivially copyable 和 standard-layout。

| 型別 | 定義 | 取代 |
|---|---|---|
| **Linear Algebra** | | |
| `Vec2` | `{x, y}` | `Render::Float2`、`Model::Vec2`、`Ui::UiFloat2` |
| `Vec3` | `{x, y, z}`（12B，維持 `Vertex3D` 20B 且 uv@12） | `Render::Float3`、`Model::Vec3`、`Collision::Float3`、pvp 的 `fps::Float3` |
| `Vec4` | `{x, y, z, w}` | 新增（1.5） |
| `Vec3d` | `{x, y, z}` double | `CapsuleQueries.cpp:13` 匿名的 double `Vec3`（collision 刻意使用的高精度路徑） |
| `Vec2i`、`Vec3i` | int32 分量 | 新增（1.5） |
| `Quaternion` | `{x, y, z, w=1}`，xyzw 順序 | `Model::Quaternion` |
| `Matrix3` | `std::array<float,9>`，預設單位矩陣，與 `Matrix4` 同為 column-major、column vector | 新增（1.5、使用者點名） |
| `Matrix4` | `std::array<float,16> values`，預設單位矩陣，column-major（`values[c*4+r]`），column vector | `Model::Matrix4`、`Renderer.cpp` 的私有矩陣碼 |
| **Geometry** | | |
| `Rect` | `{x, y, width, height}`（2D） | `Render::Rect`、`Ui::UiRect` |
| `Aabb` | `{minimum, maximum}`（`Vec3`） | `Collision::Aabb`（1.6） |
| `Capsule` | `{segmentStart, segmentEnd, radius}`，端點是兩個半球的中心，兩端點重合即為球 | `Collision::Capsule`（1.6） |
| `Ray` | `{origin, direction}`，direction 不要求正規化（與現有 Collision raycast 的約定一致；需要單位長度的運算自行正規化） | 新增（1.5、1.6）；現有 raycast 以分開的參數傳遞 |
| `Plane` | `{normal, distance}`，normal 要求正規化，平面方程為 `dot(n, p) = d` | 新增（1.5、1.6） |
| `Sphere` | `{center, radius}` | 新增（1.5、1.6） |
| `Segment` | `{start, end}` | 新增（1.5）；`Capsule` 的軸、`ClosestPointOnSegment` 的參數 |
| `Triangle` | `{a, b, c}`，只定義頂點順序與 normal 的計算方式：`normal = Cross(b − a, c − a)`（需要單位長度時再正規化）。不定義哪一面是正面；正面與剔除屬於 Render 的管線狀態（`RenderDeviceTypes.hpp` 的 `clockwiseFrontFace`） | 新增（1.5） |

**不放進 Math**，以下仍歸原 owner：
- GPU ABI：`ShaderAbi::Matrix4` 與各 Uniforms、`Vertex3D`。
- 語義型別：`Transform3D`、`PerspectiveCamera3D`、`Model::Transform`、`Render::Color`、`UiColor`。
- Collision 的領域演算法與其結果型別（見 1.6）。
- wire 與資料格式：proto、gyo.ui v1 JSON、各 codec。

### 1.3 慣例（寫進 header 註解與 `docs/architecture/math.md`，並用測試固定）

- **座標系**：左手系，+X 右、+Y 上、+Z 前，角度一律用弧度。
- **矩陣**：`p' = M·p`；`Multiply(a, b)` 先套用 b；平移在 `values[12..14]`。
- **旋轉方向**：
  - `MakeRotationY(θ)·(0,0,1) = (sinθ, 0, cosθ)`
  - `MakeRotationX(θ)·(0,0,1) = (0, −sinθ, cosθ)`
  - `MakeRotationZ(θ)·(1,0,0) = (cosθ, sinθ, 0)`
- **Euler**：GYO 的 Euler 順序為 scale → X → Y → Z → translate。這個順序原本只寫在 Render 註解中，現在升格為 Math 契約，由 `ComposeEulerXYZ` 提供。
- **Clip space**：深度 0..1，由 `MakePerspective`、`MakeOrthographicPixels` 提供。
- **GPU**：HLSL 以 `row_major float4x4` 加 `mul(float4(p,1), M)` 讀取同一組 16 個 float，位元組完全相同，上傳時不轉置。這已經驗證：Render 的 `Multiply(A,B)` 逐位元等於 Model 的 `Multiply(B,A)`。

### 1.4 函式（自由函式、非 template、`[[nodiscard]] noexcept`）

| 分類 | 函式 |
|---|---|
| 向量 | 運算子 `+ − * /`（含一元負號）；`Dot`、`Cross`、`LengthSquared`、`Length`（統一用 `std::sqrt`）、`Normalize`（不做防護，前提是長度 > 0）、`NormalizeOrZero`、`Lerp`、`Distance`、`IsFinite`（各向量、Quaternion、Matrix3、Matrix4、Rect）、逐分量 `Min`／`Max`／`Clamp`。`Vec3d` 只提供 collision 用到的子集；`Vec2i/Vec3i` 只提供加減與比較 |
| 四元數 | `Multiply`、`Conjugate`、`Dot`、`Normalize`（不做防護）、`NormalizeOrIdentity`（長度 ≤ 1e-12F 時回傳單位四元數；兩種語義用不同名稱並存）、`Slerp`（從 `Animation.cpp` 移入）、`MakeRotation(Quaternion)`（含零四元數時 s=0 的容忍） |
| 矩陣 | `Matrix3`：`Multiply`、`Transform`、`Transpose`、由 `Matrix4` 取左上 3×3。`Matrix4`：`Multiply`、`TransformPoint(const Matrix4&, Vec3)`（必須是非 template，呼叫端有 `TransformPoint(m, {})`）、`TransformVector`、`MakeTranslation/Scale/RotationX/Y/Z`、`ComposeTRS`、`ComposeEulerXYZ`、`MakePerspective`、`MakeOrthographicPixels`、`Zero()` |
| 純量與常數 | `Clamp`、`Lerp`、`Min`、`Max`（純量版）、`Pi`、`TwoPi`、`DegreesToRadians`（統一一種寫法）、`RadiansToDegrees`、`WrapRadians`（`std::remainder`）、`LerpRadiansShortest`（yaw 最短弧插值） |
| 幾何 | `Rect` 的 `Intersection`（回傳重疊區域；為避免與回傳 `optional<float>` 的射線 `Intersect` 混淆而改名，B1 定案）；最近點查詢（1.6）；其餘 primitive 的基本查詢（例如點到平面的距離、AABB 合併與包含）在 B1 依 1.5 確認 |
| 色彩空間 | `DecodeSrgb`、`EncodeSrgb`，取代 Render、Ui、ui_editor 三份 |

**實作規則**
- Epsilon 不放全域常數，容差留給呼叫端。
- 容易被 FMA 收縮的運算（`Dot`、`Lerp`、`Cross`、`Multiply`、`TransformPoint`）寫成 inline，不寫 constexpr，避免編譯期求值和執行期結果不一致。
- engine 公開 header 禁止寫 `using namespace Engine::Math`。
- 往後只在出現第二個使用者時才擴充；這次收錄的每個函式都要對應到一處現有用途，寫在 Architecture Delta 裡。
  例外見 1.5。

### 1.5 追加要求：補齊常用數學結構（2026-10-03）

使用者在核准後追加要求：Math 要補齊常用的數學結構，例如 `Matrix3`。
- 對這些結構，放寬 1.4 的「必須對應現有用途」規則，改以「通用基礎庫應有」為收錄理由，
  並在 Architecture Delta 中標明是依使用者要求預先收錄。
- 使用者以樹狀圖定下 Math 的組成（2026-10-03），1.1 的目錄與 1.2 的型別表依此編排：
  ```text
  Math
  ├─ Linear Algebra：Vec2 / Vec3 / Vec4、Matrix（Matrix3、Matrix4）、Quaternion
  ├─ Geometry：Ray、Plane、Sphere、AABB、Capsule、Segment、Triangle
  └─ Scalar / Utility：Clamp、Lerp、Min / Max、Constants
  ```
  另外保留計劃原有、樹狀圖未列出的項目：`Vec3d`、`Vec2i/Vec3i`、`Rect`、角度工具、色彩空間轉換。
- 已定案（使用者，2026-10-03）：`Ray` 的 direction 不要求正規化；`Plane` 的 normal 要求正規化，方程為 `dot(n, p) = d`。
- 已定案（使用者，2026-10-03）：AABB 的型別名稱為 `Aabb`；`Triangle` 只定義頂點順序與 normal 的計算方式，不定義正面。
- B1 開始時定案（使用者，2026-10-03）：
  - 最近點查詢統一用重載 `ClosestPoint(point, 型別)`。double 路徑用 `Segmentd`、`Aabbd`。
  - 幾何交集測試收進 Math：`Intersect(ray, Plane／Sphere／Aabb／Triangle)` 回傳最小的 t ≥ 0，起點在實心內部時回傳 0，不帶容差；`Overlaps` 用於 `Aabb`、`Sphere`、`Rect`。Collision 是否改用，於 B2 決定。
  - `Matrix3`／`Matrix4` 提供 `Determinant`、`Inverse`、`Transpose`；`Quaternion` 提供軸角建構、`Rotate`、`Inverse`。
  - 各型別的基本查詢依 B1 提出的清單實作（例如 `Plane` 的 `SignedDistance`、`Aabb` 的 `Merge`／`Expand`、`Triangle` 的 `Area`／`Centroid`）。
- 和既有 owner 重疊的型別，依 1.6 的分界原則決定歸屬。
- 新增結構同樣遵守 1.2 的版面規則、1.3 的慣例和 1.4 的實作規則，並在 `gyo_math_tests` 補上慣例與版面測試。

### 1.6 Math 與 Collision 的分界（使用者決定，2026-10-03）

- **Math**：基礎數學型別，加上純幾何運算。
- **Collision**：以碰撞判定、接觸、穿透、掃掠為目的的領域演算法。

套用到現有程式碼：
- **AABB 歸 Math**：`Collision::Aabb` 移除，Collision 改用 `Math::Aabb`。
- **`Capsule` 歸 Math**：線段加半徑的通用幾何形狀，只描述「空間裡一個膠囊體是什麼」，不帶碰撞系統的語意。
  Render 的除錯繪製、Editor、導航、Collision 都可能使用，所以放在 Math。`Collision::Capsule` 移除，Collision 改用它。
- **`Ray`、`Plane`、`Sphere` 歸 Math**：都是空間幾何 primitive，本身沒有 Collision 語意。現有程式碼中沒有這些型別，屬於新增。
  Collision 的查詢介面是否改成接收 `Ray` 等型別（目前 `RaycastAabb` 等以 origin、direction、maximumDistance 分開傳入），
  會改動 Collision 的公開介面，於 B2 開始時另行確認，不在 B1 預設變更。
- **`VerticalCapsule` 留在 Collision**：它是 Collision／角色碰撞特化後的資料表示（以腳底為基準、直立、高度含兩個半球），
  屬於領域型別；轉成通用 `Capsule` 的 `ToCapsule` 也留在 Collision。
- **留在 Collision**：ray／sweep 查詢、`Contact`、穿透深度、容差 `kEpsilon`、驗證規則，以及 double 精度路徑本身。
- **最近點查詢歸 Math**：不需要「碰撞」語義也成立的純幾何查詢。
  - `CapsuleQueries.cpp:27` 的 `Closest`（點到線段的最近點）。線段退化成一點時回傳端點 a，這個行為要保留。
  - `CapsuleQueries.cpp:32` 的 `Clamp`（點到 AABB 的最近點）。依同一原則一併移入。
  - Math 提供 `Vec3` 與 `Vec3d` 兩個多載，名稱於 B1 確認（例如 `ClosestPointOnSegment`、`ClosestPointOnAabb`）。
  - Collision 的 double 路徑改呼叫 Math 的 `Vec3d` 版本。

---

## 2. 逐項處置要點

**Render**（`Renderer.cpp:17-164`）
- 私有矩陣 helper 刪除。
- `WorldMatrix` 改用 `ComposeEulerXYZ`。
- View、Projection、Sprite 的乘法鏈依下表鏡像改寫。
- `ToShaderMatrix` 只做 16 個 float 的 memcpy。
- 沒人用的 `IsFinite(Float3)`（:164）刪除。
- `PrimitiveMesh` 的 `Length` 從 hypot 改成 sqrt。B1 在 Apple libc++ 上量到正常範圍 0 ulp（libc++ 的三參數 hypot 在該範圍就是同一個 sqrt 式），libstdc++／MSVC 估計最多 3 ulp；溢位與下溢行為不同。
- `PrimitiveMesh.cpp:59,66` 的倒數相乘正規化（`Scale(v, 1.0F / Length(v))`）改用 `Math::Normalize` 時，B1 量到 Apple libc++ 最多 1 ulp（約 53% 的輸入不同），Linux libstdc++ 與 Windows MSVC 最多 4 ulp（約 63%），差異來自各平台的 hypot。B4a 替換時記錄這段漂移。
- `ModelRenderer.cpp:80-81` 改成 `{vertex.position + offset, vertex.uv}`。

**Model**
- `ModelAsset` 中的 `Multiply`、`TransformPoint` 刪除，改由 Math 提供。`ToMatrix(Transform)` 留在 Model，改寫成 `ComposeTRS`。
- `Animation.cpp:170,172` 把 `Lerp` 當成 callable 傳進 template，改傳 lambda。
- `Animation`、`AnimationTransfer` 中的 `Normalize`、`Product`、`Slerp`、`Finite` 改用 Math。
- **ADL 陷阱**：`AnimationTransfer.cpp:16` 的舊 `Inverse(q)` 其實是共軛，必須改寫成 `Math::Conjugate`（呼叫處 :125、:148、:149）。若只刪掉 local helper，未限定的 `Inverse(q)` 會經 ADL 靜默選到真正的 `Math::Inverse`（除以 |q|²），造成位元漂移。

**Collision**
- `Collision.hpp` 改用 `Math::Vec3`；`Collision::Aabb`、`Collision::Capsule` 移除，改用 Math 的 AABB 與 `Capsule`（1.6）。`VerticalCapsule` 保留。
- `Collision.cpp:14,18,51` 和 `CapsuleQueries.cpp:26` 的 helper 刪除，CapsuleQueries 改用 `Math::Vec3d`；`Closest`、`Clamp` 改用 Math 的最近點查詢（1.6）。
- double 路徑本身和 `kEpsilon` 保留。

**Ui 與 ui_editor**
- `UiFloat2`、`UiRect` 改成 Math 型別，`ConvertRect` 刪除。
- `UiColor`、`PreviewAdapter.cpp:27` 的 sRGB 改用 Math。
- `IntersectRect` 改用 `Math::Intersection`（B1 characterization 確認逐位元相同）。

**pvp**
- `fps::Float3` 移除，`Vector.hpp` 中的 Float3 刪除。
- `ToCollision`、`Render::Float3` 與 `Model::Vec3` 之間的互轉全部刪除。
- `Length`、`Lerp`、`Finite`、yaw wrap（7 處）、角度換算、muzzle 的手寫 Euler（`WeaponPresentationDefinition.cpp:64-84`）、yaw 旋轉（`PlayerPresentation.cpp:476`）改用 Math。
- **`fps::Float2{x,z}` 的處理**：它代表地平面座標，語義和 `Math::Vec2{x,y}` 不同，機械式取代會把 z 靜默對應成 y。建議保留為產品自有的語義型別，改名為 `GroundPoint`，並註明它不是數學重複定義。這是本計劃唯一保留的產品型別；核准時未另行指示，採用此方案。

**不得遷移**：`tests/common/render/sdl_gpu/RenderFeatureSmoke.cpp:92-96` 是刻意獨立的 scalar oracle，保持不動。

**範圍外，只回報**
- Collision 內部 float 和 double 兩套演算法並存：這是演算法層的重複，不是數學庫問題，而且會改變 authority 的判定。
- `Render::Color` 和 `UiColor` 同構重複，但不屬於幾何數學。
- 兩份 FNV-1a；include 路徑風格不一致。（CI 快取鍵的無效路徑已依使用者指示在 #18 修正，見 HANDOFF。）
- `object_fps_preview` 的 registration 不一致：工具是 enabled，但依賴的 app 已停用。

**Render 乘法鏈對照表**（B4b 使用）

| 現在的 row-vector 寫法 | 改成 column-vector 寫法 |
|---|---|
| `S·Rx·Ry·Rz·T` | `ComposeEulerXYZ`（內部等於 `Multiply(T, Multiply(Rz, Multiply(Ry, Multiply(Rx, S))))`） |
| View：`T(−p)·Rz·Ry·Rx` | 參數鏡像 |
| `View·Proj` | `Multiply(Proj, View)` |
| `World·VP` | `Multiply(VP, World)` |
| Sprite：`T(pivot)·S·Rz·T(anchor)` | `Multiply(T(anchor), Multiply(Rz, Multiply(S, T(pivot))))` |
| `SpriteWorld·PixelProj` | `Multiply(PixelProj, SpriteWorld)` |
| Projection 的 `[2][3]`、`[3][2]` | flat `values[11]`、`values[14]`，起點必須是 `Zero()` |

---

## 3. 批次

### 共同規則

- **每批**：一個 PR，commit 與 PR 用日語。HANDOFF 在每批開始、里程碑、停止時與該批的工作一起更新。
- **計劃與紀錄文件**：`docs/architecture/plans/math-foundation/{README,HANDOFF}.md`；dev_log 放 `docs/dev_logs/`。
- **舊名稱一次移除**：因為不留別名，每一批都要把該模組的舊名稱和所有 active 使用者（engine、pvp、ui_editor、tests）一起改完，不能留下編譯不過的中間狀態。
- **本機驗收**（cmake、ninja 用絕對路徑）：
  - `cmake --preset core`、`cmake --build --preset core`、`ctest --preset core`
  - `cmake --preset test`、`cmake --build --preset test`、`ctest --preset test`
- **CI 驗收**：`build-and-validate.yml`、`cross-platform.yml` 四個平台全綠；core 只要求在 native 列通過。
- **依賴圖比對**：每批前後用 `cmake --graphviz=<scratchpad>/deps.dot` 比對，新增的邊只能是 1.1 列出的幾條。
- **數值漂移紀錄**：凡是會改變浮點結果的替換，都在 HANDOFF 記錄「操作、輸入網格、最大絕對誤差與最大 ulp」。
  - 量測工具是 B1 建立的 characterization 測試：舊實作凍結成測試內的複本，和 Math 對照。
  - 原本精確比較的測試，因結果改變而需要修改時，要逐一註明理由，不能直接放寬容差。
- **停止條件**：出現非預期的回歸、範圍擴大，或 PvP authority 的漂移超出 reconciliation 能吸收的程度時，停下來回報並重新規劃，不自行升降檔。
- **ultracode 對抗式檢查**：建議在 B1、B4b、B6a 使用，到時逐次徵求使用者同意。

### B0　基線（medium，不改程式）

- 在 master 上跑 core 和 test，記錄測試清單與結果，保存依賴圖。
- **29 個未編譯 .cpp 的基線**：
  - 從 test preset 的 `compile_commands.json` 取得 `app_support` 的編譯旗標，對每個檔案跑 `-fsyntax-only`，記錄哪些能通過。
  - 輸出放 scratchpad，並加上 `-DGYO_OUTPUT_ROOT=<scratchpad>/out`，避免寫進 repo 的 `build/target`。
- 建立 plan README 與 HANDOFF。
- **驗收**：基線寫入 HANDOFF。

### B1　`GYO::Math` 與 `tests/common/math`（high；慣例與數值規則用 xhigh；建議 ultracode 檢查）

- **範圍**：
  - `engine/math/**`、`engine/CMakeLists.txt`。
  - `tests/common/math/**`、`tests/common/CMakeLists.txt`：測試 target 為 `gyo_math_tests`，警告旗標比照 collision（`-Wall -Wextra -Wpedantic`；MSVC 用 `/W4 /permissive-` 加 `NOMINMAX`）。
  - `docs/architecture/math.md` 初稿。
- **測試內容**：
  1. 版面的 static_assert。
  2. 旋轉方向和組合順序的慣例測試。
  3. Characterization：從各處把舊 helper 逐字凍結成測試內的複本，與 Math 比對。比對結果分兩類：
     - 逐位元相同的，鎖成精確比較。
     - 會改變的，記錄最大誤差。
  4. 輸入涵蓋 ±0、subnormal、inf、NaN、大數值、零四元數。
- **這一批不改任何既有消費者。**
- **驗收**：
  - core、test、CI 四平台全綠，特別是 macos-arm64（有 FMA）。
  - Math header 只 include 標準函式庫。
  - `gyo_math` 沒有對外的依賴邊。

### B2　Collision（high；double 路徑換型別的部分用 xhigh）

- **範圍**：
  - `Collision.hpp`、`Collision.cpp`、`CapsuleQueries.cpp`、`engine/collision/CMakeLists.txt`、collision 測試。
  - pvp 中引用 `Collision::Float3`、`Collision::Aabb`、`Collision::Capsule` 的地方：`ToCollision` 改成轉換到 Math 型別。
  - 依 1.6 在 B1 確認後移入 Math 的形狀與純幾何 helper，在這一批切換。
- **驗收**：
  - `gyo_collision_tests` 通過。
  - `object_fps_pvp.cpu`、`object_fps_pvp.start_phase_record` 通過。
  - 漂移已記錄（這一批預期逐位元不變）。

### B3　Model（high；矩陣與四元數用 xhigh）

- **範圍**：
  - `ModelAsset.hpp/.cpp`、`Animation.cpp`、`AnimationTransfer.cpp`、ufbx loader 中的型別名稱、model 相關測試。
  - pvp 中所有引用 `Model::Vec3/Matrix4/Quaternion` 的程式與測試，例如 `PlayerPresentationTests.cpp` 中不加限定的 `Multiply/TransformPoint`。
- **驗收**：
  - `gyo_model_tests`、`gyo_model_renderer_tests`、`gyo_ufbx_model_tests` 通過。
  - `object_fps_pvp.presentation_cpu` 通過。
  - 漂移已記錄。

### B4a　Render 型別與 helper（high）

- **範圍**：
  - `RenderTypes.hpp` 中的 `Float2`、`Float3`、`Rect` 改成 Math 型別。
  - `RenderQueue`、`SdlGpuRenderDevice`、`PrimitiveMesh`、`ColorTransform`、`render/model`。
  - pvp 和 ui_editor 中所有引用 `Render::Float2/Float3/Rect` 的地方。
  - `Renderer.cpp` 的矩陣碼這一批不動。
- **驗收**：
  - `gyo_render_tests`、shader 標籤的測試、`render.sdl_gpu_mesh_smoke`（CI Linux lavapipe）、pvp 的 cpu 與 presentation_cpu 通過。
  - PrimitiveMesh 的漂移已記錄。

### B4b　Render 矩陣切換到 Math 慣例（xhigh；建議 ultracode 檢查）

- **範圍**：`Renderer.cpp` 依上面的對照表改寫並加上 `ToShaderMatrix`；同步修改 `docs/rendering_architecture.{zh-Hant,ja}.md:83`。
- **前置**：先在 B4b 開頭新增測試：把舊 `Renderer.cpp` 的矩陣碼凍結成 Legacy 版本，經 capture device 取得 VertexUniforms 後做 memcmp。輸入要涵蓋 sprite 的旋轉與非中心 pivot。
  - 兩邊的輸入都必須是執行期資料（例如經 opaque 函式或檔案供給）。clang 最佳化時可能用 fused 捨入做常數摺疊，即使在 x86_64 上也一樣。
  - WVP 的結合方式必須維持 `Multiply(Multiply(Proj, View), World)`（對應舊的 `World·(View·Proj)`）。B1 量到改成 `Multiply(Proj, Multiply(View, World))` 會改變位元。
- **驗收**：
  - memcmp 在四個平台逐位元相同。矩陣乘法已證明可以逐位元保持；若某平台不同，就停下來回報。
  - `gyo_render_tests`、`render.sdl_gpu_mesh_smoke`、pvp 的 `presentation_evidence` 通過。

### B5　Ui 與 ui_editor（high；codec 驗收的執行用 medium）

- **範圍**：
  - `UiTypes.hpp`、`UiColor.cpp`、`UiValidation.cpp`、`UiRuntime.cpp`、`UiRenderer.cpp`、`engine/ui/CMakeLists.txt`、ui 測試。
  - ui_editor 中所有用到 `UiFloat2/UiRect` 的地方，以及 `PreviewAdapter.cpp`。
- **前置**：gyo.ui v1 `Serialize` 的輸出加上 golden 字串測試，先在 master 程式碼上通過。
- **驗收**：
  - `gyo_ui_tests` 通過，golden 字串完全一致，schema 版本不變。
  - `gyo_ui_editor.{core,validate,content_cli,acceptance_contract,preview}` 與 `build/acceptance/ui_editor` 通過。

### B6a　pvp 模擬層 `match_domain`（xhigh；建議 ultracode 檢查）

- **範圍**：
  - `Vector.hpp` 中的 `fps::Float3` 移除；`fps::Float2` 依第 2 節的方案處理。
  - `match_domain` 的 CMake 連結 Math。
  - `Movement`、`ShotQuery`、`PvpMatch`、`LocalPlayerPrediction`、`Arena`、`CharacterCollision` 中的 helper 改用 Math。
  - `tests/object_fps_pvp` 中對應的測試。
- **數值**：
  - authority 的結果允許改變（使用者決策 2）。
  - client 和 server 從同一個 commit 建置；protocol 目前只檢查版本號 5（`IpcHost.cpp:145`、`ClientConnection.cpp:269`），沒有模擬版本的檢查。
  - **預設不升 protocol**。如果量測到的漂移會讓新舊版本混連時出現 reconciliation 無法吸收的偏差，就停下來，請使用者決定是否升到 v6。
- **決定性量測**：
  - 固定指令序列的 digest，在 master 和 branch 上分別產生並比較。
  - B1b 起全專案以 `-ffp-contract=off` 建置（MSVC 預設即不收縮），優先平台為 Linux、Windows、mac x64，arm64 為附帶產物。digest 以優先平台為準，各平台結果應只因 libm 不同而有差異。digest 的輸入仍必須是執行期資料。
  - 差異如實記錄。注意 `PredictionTests.cpp:563,584` 和 `PvpMatchTests.cpp:258-318` 都是 Approx 或同一次執行內的自我一致比較，不是 golden。
- **名稱衝突**：`tests/object_fps_pvp/PredictionTests.cpp:37` 的 `Distance(fps::Float3, fps::Float3)` 算的是水平（XZ）距離。型別換成 `Math::Vec3` 後會和 `Math::Distance` 歧義；改名為 `HorizontalDistance` 並保留語意，不可當成重複定義刪除。
- **產品 helper 的 characterization**：pvp 舊 helper 的凍結比對放在 `tests/object_fps_pvp`，不放共通測試（AGENTS §7）。ui_editor 的 `PreviewAdapter` 舊 helper 同理，在 B5 放進 `tests/ui_editor`。
- **驗收**：
  - `object_fps_pvp.cpu`、`object_fps_pvp.start_phase_record`、gateway 的 Go 測試通過。
  - `apps/object_fps_pvp/protocol/` 沒有任何變動，除非使用者決定升 protocol。

### B6b　pvp 表現層與 acceptance（high；執行驗收用 medium）

- **範圍**：
  - `app_support` 的表現層檔案（`PlayerPresentation`、`WeaponPresentationDefinition`，以及 `PvpApplication.cpp:758` 與 `SnapshotTimeline`）。
  - `build/acceptance/object_fps_pvp`。
- **驗收**：
  - `presentation_cpu` 和所有能執行的 `*_evidence` 通過。
  - muzzle 證據的差異已記錄。

### B6c　pvp 的 29 個未編譯 .cpp（high）

- **範圍**：把這 29 個檔案中的舊型別和 helper 全部改用 Math（使用者決策 4）。
- **驗收**：
  - 用 B0 的方法逐檔 `-fsyntax-only`。B0 時能通過的檔案必須仍然通過。
  - B0 時就無法通過的檔案標為「未驗證」並記錄，不算通過。

### B7　收尾（medium）

- **文件定稿**：
  - `docs/architecture.md`：目錄樹、Runtime boundaries 表、慣例章節、Architecture Delta 七點。
  - `docs/creating_apps.md:41`：加入 `GYO::Math`。
  - `docs/architecture/math.md`。
- **grep 稽核 active 範圍**：
  - 除了第 2 節列為保留的型別，已沒有 `struct (Float[23]|Vec[23]|Matrix4|Quaternion)`。
  - 沒有匿名的 `Length/IsFinite/Finite(/Normalize/Multiply/Rotation`。
  - 公共層沒有出現產品名稱。
- **未啟用產品的破損盤點**：
  - 在 scratch registry 中啟用 object_fps 和 object_fps_v2，並設定 `GYO_OUTPUT_ROOT=<scratchpad>`。
  - 用 `ninja -k 0` 建置，逐 TU 收集錯誤，記錄到 HANDOFF，作為將來重新啟用時的遷移清單。repo 不變。
- 依賴圖與 B0 比對；撰寫 Architecture Report；HANDOFF 標為完成。

### 順序理由

- Math 先獨立存在（B1）。
- Collision 最小而且不依賴 Engine，先用它驗證移除舊名的模式。
- Math 的矩陣實作就是 Model 現有的實作，所以 Model 排在 Render 前面。
- Render 拆成兩批，把 xhigh 集中在 B4b。
- pvp 放在最後，分成模擬層、表現層、死檔三批。

---

## 4. Architecture Delta（AGENTS.md §3）

1. **需求來源**：使用者明確要求的 Refactoring：Math 作為 Engine 的基礎。
2. **觀測到的問題**：第 1 節列出的重複定義、語義相反的 Matrix4、多套 helper 寫法、反覆出現的轉換函式（`ToCollision`、`ConvertRect`、型別之間的 brace-copy）。
3. **變化的 boundary**：新增最底層模組 `GYO::Math`。座標系、矩陣、Euler 順序和 clip space 的慣例 ownership，從 Render 和 Model 的註解移到 Math 契約。
4. **影響範圍**：engine 的 collision、model、render、ui；`apps/object_fps_pvp`；`tools/ui_editor`；`tests/common`；docs。
5. **依賴方向**：只新增「模組 → Math」的單向邊，沒有循環。Collision 仍不依賴 `GYO::Engine`，Model 與 Render 仍互不依賴，`match_domain` 不會多出 Render 或 Model 依賴。
6. **Ownership**：值型別、通用運算、純幾何運算和慣例歸 Math；語義型別、ABI、資料契約與領域政策留在原 owner。Math 與 Collision 的分界見 1.6。
7. **為什麼沒有更小的做法**：
   - 放進 `engine/base`：Collision 必須依賴 `GYO::Engine`。
   - 以 Render 的型別為正宗：Model、Collision、Ui 都得依賴 Render，方向錯誤。
   - 只加轉換器：轉換器正是現在觀察到的問題本身。

**對未啟用產品的影響**
- 舊名稱移除後，object_fps、object_fps_v2 和 object_fps_preview 重新啟用時無法直接編譯，需要先遷移。
- 這是使用者選擇「完全移除舊名」的直接後果。依「未啟用產品除外」的範圍，本計劃不修改它們，只在 B7 產出破損清單。
- engine 的 header 和文件中不寫任何產品名稱。

---

## 5. 驗證（總結）

- **每批**：
  - core 與 test 兩個 preset 的 configure、build、ctest。
  - CI 四個平台。
  - graphviz 依賴差分。
  - 數值漂移紀錄。
- **關鍵證據**：
  - B1 的 characterization 測試。
  - B4b 的 Renderer memcmp（四平台）。
  - B5 的 gyo.ui golden 字串。
  - B6a 的 PvP digest（含 arm64）。
  - B6c 的逐檔 syntax-only 檢查。
- **最終**：B7 的 grep 稽核、依賴圖比對、未啟用產品破損清單、Architecture Report。

## 6. 尚未確認之處

- 那 29 個 .cpp 和未啟用產品目前在 master 上能否編譯：由 B0 和 B7 實測。
- MSVC 與 GCC 在內聯後的 FP 收縮行為：以 B1 在 CI 四個平台的結果為準。
- PvP 漂移的實際大小，以及是否需要升 protocol：由 B6a 量測後決定。
