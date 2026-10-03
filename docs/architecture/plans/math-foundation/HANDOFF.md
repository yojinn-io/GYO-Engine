# Math 基礎統一：交接

更新：2026-10-04。**B0–B5 已合併（B5 為 #23，合併為 `efe4a30`）。B6a（PR [#24](https://github.com/yojinn-io/GYO-Engine/pull/24)，CI 通過）與 B6b（PR [#25](https://github.com/yojinn-io/GYO-Engine/pull/25)）疊在一起待使用者合併；B6c 進行中。**

## 閱讀入口

1. [進度與執行規則](README.md)。
2. [核准版計畫](PLAN.md)：Math 契約、逐項處置、Render 乘法鏈對照表、批次與驗收、Architecture Delta。
3. 本文件：各批實際做了什麼、決策、證據位置、未結事項。

## 決策紀錄

| 日期 | 決策 | 來源 |
|---|---|---|
| 2026-10-03 | 新增 `GYO::Math`（`engine/math`，INTERFACE、header-only，namespace `Engine::Math`） | 使用者核准計畫 |
| 2026-10-03 | 舊名稱完全移除，不留別名 | 使用者 |
| 2026-10-03 | 數值實作全面統一，接受浮點結果改變，但須量測並記錄 | 使用者 |
| 2026-10-03 | Math 為完整基礎庫 | 使用者 |
| 2026-10-03 | pvp 29 個未編譯 `.cpp` 一起遷移 | 使用者 |
| 2026-10-03 | pvp 的 `fps::Float2{x,z}` 保留為產品語義型別，改名 `GroundPoint` | 計畫建議，核准時未另行指示 |
| 2026-10-03 | 補齊常用數學結構（例如 `Matrix3`），清單於 B1 開始時確認 | 使用者追加要求 |
| 2026-10-03 | 分界：Math 為基礎數學型別加純幾何運算；Collision 為碰撞判定、接觸、穿透、掃掠的領域演算法。AABB 歸 Math，Collision 改用它 | 使用者 |
| 2026-10-03 | `Capsule` 歸 Math；`VerticalCapsule` 留在 Collision | 使用者 |
| 2026-10-03 | 點到線段的最近點 `Closest` 歸 Math（純幾何查詢）；依同一原則，點到 AABB 的最近點 `Clamp` 也移入 | 使用者；`Clamp` 為依原則延伸 |
| 2026-10-03 | `Ray`、`Plane`、`Sphere` 歸 Math（空間幾何 primitive，沒有 Collision 語意；現有程式碼中沒有這些型別，屬於新增） | 使用者 |
| 2026-10-03 | Math 組成依樹狀圖：Linear Algebra（Vec2/3/4、Matrix3/4、Quaternion）、Geometry（Ray、Plane、Sphere、AABB、Capsule、Segment、Triangle）、Scalar／Utility（Clamp、Lerp、Min／Max、Constants） | 使用者 |
| 2026-10-03 | 向量命名：單一型別 `Vec2/Vec3/Vec4`，純量用後綴（`Vec3d`、`Vec2i/Vec3i`）；點與方向不分型別。原計劃的 `Float*`、`Double3` 名稱作廢 | 使用者 |
| 2026-10-03 | header 依樹狀圖分子目錄 `linear/`、`geometry/`、`scalar/`；namespace 仍為單一 `Engine::Math` | 使用者 |
| 2026-10-03 | `Ray`：direction 不要求正規化。`Plane`：normal 要求正規化，方程 `dot(n, p) = d` | 使用者 |
| 2026-10-03 | AABB 型別名稱為 `Aabb`。`Triangle` 只定義頂點順序與 normal 計算（`Cross(b − a, c − a)`），不定義正面；正面與剔除歸 Render 管線狀態 | 使用者（normal 公式為計劃採用的標準式） |
| 2026-10-03 | 最近點查詢統一用重載 `ClosestPoint(point, 型別)`；double 路徑新增 `Segmentd`、`Aabbd`（比照 `Vec3d` 後綴規則） | 使用者（`Segmentd`、`Aabbd` 為依此延伸） |
| 2026-10-03 | 幾何交集測試收進 Math：射線對 `Plane`／`Sphere`／`Aabb`／`Triangle` 的 `Intersect`，以及 `Overlaps`；Collision 是否改用，於 B2 決定 | 使用者 |
| 2026-10-03 | B5：UiRenderer 的 `ClipSprite` 改用 `Math::Intersection`；目標矩形非有限時一律轉交 RenderQueue 回報錯誤。舊版的處理不一致：有時報錯、有時靜默丟棄，寬或高為 +inf 時還會送出 UV 範圍為 0 的 sprite；文字 bounds 接近 `FLT_MAX` 而對齊計算溢位時也會觸發。審查補齊這些情況後，使用者再次確認維持一律報錯 | 使用者 |
| 2026-10-03 | 本計劃全程使用 ultracode（盤點、實作、驗證、審查都以多 agent workflow 進行）：這是底層概念模型的變更，且已有大量實作依賴它 | 使用者（B4b 進行中） |
| 2026-10-03 | Collision 的 raycast 改為接收 `Math::Ray`（Ray 是幾何 primitive，`RaycastAabb`／`RaycastCapsule` 是 Collision 演算法） | 使用者 |
| 2026-10-03 | `SweepSphereAgainstCapsule` 的路徑改為接收 `Math::Segment` | 使用者 |
| 2026-10-03 | 浮點收縮模式全專案統一為不收縮（`-ffp-contract=off`，MSVC 預設 `/fp:precise`）；優先平台為 Linux、Windows、mac x64，arm64 為附帶產物 | 使用者 |
| 2026-10-03 | `Matrix3`／`Matrix4` 提供 `Determinant`、`Inverse`（不可逆時回傳 `std::optional` 空值）、`Transpose`；`Quaternion` 提供軸角建構、`Rotate`、`Inverse` | 使用者 |
| 2026-10-04 | B6a：`Math::DegreesToRadians`／`RadiansToDegrees` 改為 constexpr，讓 pvp 的 `MovementMaximumPitch` 能以 `inline constexpr` 改用它（單次乘法，編譯期與執行期捨入相同） | 使用者 |
| 2026-10-04 | B6a 的跨平台驗證：本機（mac x64）比對 master 與 branch 的全模擬 digest；依賴 libm 的替換（`hypot`）與會漂移的替換（倒數相乘正規化）以 `tests/object_fps_pvp` 的 characterization 測試在 CI 四平台實測；其餘替換只用 IEEE 四則運算與 `sqrt`，與平台無關 | 使用者 |

## B0 基線

狀態：**完成**（2026-10-03，分支 `claude/math-foundation-b0`，基準 commit `0bd5363`；PR #17 合併為 `0dd3077`）。
環境：Intel Mac（x86_64），Apple clang 21.0.0（clang-2100.1.1.101）。cmake／ninja 以絕對路徑呼叫。

| 項目 | 結果 | 保存位置 |
|---|---|---|
| core preset（configure／build／ctest） | 18／18 通過 | [baseline/core_tests.txt](baseline/core_tests.txt) |
| test preset（configure／build／ctest） | 41／41 通過，含 `object_fps_pvp.*` 15 項、`gyo_ui_editor.*` 5 項 | [baseline/test_tests.txt](baseline/test_tests.txt) |
| 依賴圖（test preset） | GYO 相關邊 133 條；`gyo_collision` 沒有對外依賴 | [baseline/gyo_dependency_edges.txt](baseline/gyo_dependency_edges.txt) |
| pvp 未編譯 `.cpp` 的 syntax-only | 29／29 通過（0 錯誤） | [清單](baseline/pvp_uncompiled_sources.txt)、[結果](baseline/pvp_uncompiled_syntax.tsv) |

### 方法

- **依賴圖**：`cmake --preset test --graphviz=<輸出目錄>/deps.dot`，再用
  `python3 docs/architecture/plans/math-foundation/scripts/dot_edges.py <輸出目錄>/deps.dot` 抽出 GYO 相關邊。
  之後每批以同樣方式產生並和基準 diff，新增的邊只能是 PLAN 1.1 列出的幾條。
- **未編譯檔案清單**：`apps/object_fps_pvp/src` 下所有 `.cpp`，扣掉 `PVP_DOMAIN_SOURCES` 與
  `apps/object_fps_pvp/CMakeLists.txt` 明列的來源。`APP_DOMAIN_SOURCES`、`APP_SUPPORT_SOURCES` 沒有被任何 target 使用。
- **syntax-only**：從 repo 根目錄執行
  `python3 docs/architecture/plans/math-foundation/scripts/syntax_check.py build/target/_build/test/compile_commands.json docs/architecture/plans/math-foundation/baseline/pvp_uncompiled_sources.txt <輸出目錄>`。
  它借用 `app_support`（`PlayerPresentation.cpp`）的編譯旗標逐檔做 `-fsyntax-only`，涵蓋語法與語意檢查，不含連結。
  已用故意寫錯的檔案做反向對照，確認會回報錯誤；以改寫後的腳本重跑，結果與原基線相同。

### 解讀

- 29 個未編譯檔案在 master 上都能通過語意檢查，所以 B6c 的驗收是「29 檔全部仍然通過」，沒有「基線就失敗、只能標為未驗證」的檔案。
- 本機是 x86_64，沒有 FMA 收縮；決定性相關的比較必須包含 CI 的 macos-arm64 列（PLAN B6a）。

## B1 `GYO::Math`

狀態：**完成**。PR [#18](https://github.com/yojinn-io/GYO-Engine/pull/18) 於 2026-10-03 合併為 `6cc2829`；最終 head `9e65b41` 的 CI 四平台全部通過。

### 交付

- `engine/math`：header-only 的 `GYO::Math`，分為 `linear/`、`geometry/`、`scalar/` 三組，共 19 個 header，只依賴標準函式庫。
  每個 header 單獨編譯（且重複 include 兩次）時，`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror` 都沒有警告。
- `engine/CMakeLists.txt` 註冊 `math`，排在所有模組之前。
- `tests/common/math`：`gyo_math_tests`，包含規格測試 `MathTests.cpp` 與 characterization `MathCharacterizationTests.cpp`，共 89 個 test case。
- 文件：新增 `docs/architecture/math.md`（契約）；`docs/architecture.md`、`docs/creating_apps.md` 加入 `GYO::Math`。
- 既有消費者一行都沒改。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 19／19 通過（B0 為 18，新增 `gyo_math_tests`） |
| test preset | 42／42 通過（B0 為 41） |
| 依賴圖 | 與 B0 相比只多 `gyo_math_tests → gyo_math`、`gyo_math_tests → doctest`；`gyo_math` 沒有對外的邊 |
| 收縮模式 | `gyo_math_tests` 在 `-O0`、`-O2`、`-O2 -mfma -ffp-contract=on`、`-O2 -mfma -ffp-contract=fast` 下都通過（本機 clang x86_64） |
| CI 四平台 | 最終 head `9e65b41` 全部通過。第一次執行時，linux-x64 與 windows-x64 的 `gyo_math_tests` 失敗：倒數相乘正規化的漂移上限只依 Apple 實測設為 2 ulp，而兩平台 hypot 實作不同，實測 4 ulp。上限改為由誤差組成推導的 `kHypotDriftUlpBound + 2`（5 ulp）後重跑。其餘 88 個 test case 在兩平台都通過，Math 與測試程式碼在 gcc-14、MSVC 下都沒有警告 |

### Characterization 結果（engine helper → Math）

- **逐位元相同**：
  - Model：`Multiply`、`ToMatrix`（對 `ComposeTRS`）、`TransformPoint`、`Lerp`、兩種四元數 `Normalize`、`Slerp`、`Inverse`（對 `Conjugate`）、`Product`（對 `Normalize(Multiply)`）。
  - Collision：`IsFinite`、`Length`、`Normalize`、double 路徑全部、`Closest`（對 `ClosestPoint(Segmentd)`）、`Clamp`（對 `ClosestPoint(Aabbd)`）。
  - Render：基本矩陣、`Multiply(A,B)`（對 `Multiply(B,A)`）、`WorldMatrix`（對 `ComposeEulerXYZ`）、View／Projection／Sprite／PixelProjection、整條 WVP 鏈、sRGB。
  - Ui：`IntersectRect`（對 `Intersection`）、sRGB（UiColor 的 256 個輸入）、各處 `IsFinite`。
- **漂移**：
  - `PrimitiveMesh` 的 hypot `Length` 對 sqrt：Apple libc++ 正常範圍 0 ulp；溢位與下溢行為不同。
  - 倒數相乘正規化對 `Math::Normalize`：Apple libc++ 最多 1 ulp；Linux libstdc++、Windows MSVC 最多 4 ulp（CI 實測）。
- **刻意的語意差異**：UiColor 的 decode 不 clamp，但在 hex 輸入下結果相同；兩種四元數 `Normalize` 只在長度 ≤ 1e-12 時不同，各自保留為不同名稱。

### 對抗式審查（ultracode）與處理

- 修正 2 個 blocker：
  - `Triangle` 最近點遇到退化或近退化三角形時會出現 NaN；在 FMA 下，近退化甚至出現約 200 的誤差。
    改成以數值條件判定（`|ab×ac|² ≤ FLT_EPSILON·|ab|²|ac|²` 時改取三邊最近點），內部分支改為沿法線投影，並以 double 參考實作驗證。
  - Cross 測試的容差在 FMA 下會誤報，改為隨運算元大小縮放。
- 修正 2 個 major：
  - 「不寫 constexpr 就能避免編譯期與執行期結果不同」的說法錯誤：clang 最佳化時可能用 fused 捨入做常數摺疊，x86_64 也一樣。已更正註解、math.md，以及 PLAN B4b、B6a 的量測方法（兩邊都用執行期資料）。
  - 近退化三角形的測試不足，已補上。
- minor：
  - 射線交集遇到非有限輸入與溢位時改回傳 nullopt；射線對球的判別式改用 Lagrange 恆等式，避免遠距離時嚴重相消。
  - 補齊版面的 static_assert。
  - 修正 `ComposeTRS` 等函式的註解。
  - characterization 補上 `Finite(Matrix4)` 與倒數相乘正規化。
- 寫進 PLAN 的後續注意事項：
  - B3：`AnimationTransfer` 的 `Inverse` 必須改寫成 `Conjugate`，避免 ADL 靜默選到 `Math::Inverse`。
  - B4a：記錄倒數相乘正規化的漂移。
  - B4b：WVP 的結合方式必須照舊。
  - B6a：`PredictionTests` 的 `Distance` 是水平距離，改名為 `HorizontalDistance`。
  - B5、B6a：產品和工具的 helper 比對放在各自的產品測試。

## B1b 浮點收縮模式統一

狀態：**本機驗收完成，依使用者指示併入 PR [#18](https://github.com/yojinn-io/GYO-Engine/pull/18)**（2026-10-03；原分支 `claude/math-foundation-b1b` 的 commit 已 cherry-pick 到 B1 分支，不另開 PR）。

- `build/cmake/GyoBuild.cmake`：在 `third_party` 之後、`engine` 之前加上 `add_compile_options`，對 C／C++ 的 Clang、AppleClang、GCC 加 `-ffp-contract=off`。
  - 作用範圍：engine、apps、tools、tests，以及產品從自己目錄加入的相依套件（pvp 的 protobuf、asio、httplib）。
  - 不受影響：`third_party/` 與以 `ExternalProject` 另外建置的 shader 工具（DXC 等）。
- 預期影響：Linux（gcc-14、ISO 模式）與 Windows（MSVC）原本就不收縮，結果不變；mac x64 只有 Apple clang 對常數輸入做的 fused 常數摺疊會消失；arm64 不再產生 fmadd。
- 文件：`math.md` 數值政策、`Scalar.hpp` 註解、PLAN B6a 的量測方式。
- Architecture Delta：共通建置設定變更（Build Graph／編譯選項）。不新增 target 或依賴邊；產品與工具不需要任何修改。
- 本機驗收（Intel Mac）：core 19／19、test 42／42 通過，包括 pvp 全部 15 項；依賴圖與 B1 相同。
- `compile_commands.json` 確認套用範圍：engine 54／54、apps 18／18、tests 54／54、tools 11／11、build（acceptance）7／7 都帶有 `-ffp-contract=off`；
  SDL、SDL_image、SDL_ttf、imgui、ufbx、doctest 都沒有；pvp 自己目錄加入的 protobuf、absl 也帶有這個選項，與預期相符。

## #18 附帶修正：CI 快取鍵的無效路徑

依使用者指示併入 PR [#18](https://github.com/yojinn-io/GYO-Engine/pull/18)（2026-10-03），不另開 PR。

- B0 盤點時發現 `build-and-validate.yml:129` 與 `cross-platform.yml:120` 的 shader 工具快取鍵引用了已不存在的 `engine/base/src/Sha256.cpp`（`Sha256` 現在是 header-only，`.hpp` 已在鍵中），當時列為範圍外。
- 兩處都刪除該路徑。`hashFiles()` 會略過不存在的路徑，所以刪除前後鍵值相同，不會讓快取失效。
- `tests/common/ci/test_ci_scope.py:59` 只是用該路徑當作「engine 原始碼變更」的範例輸入，不是引用實際檔案，因此保留。本機 `build.ci` 通過。

## B2 Collision

狀態：**完成**。PR [#19](https://github.com/yojinn-io/GYO-Engine/pull/19) 於 2026-10-03 合併為 `5c8fd10`；最終 head `33bf64c` 的 CI 四平台全部通過。

### 變更

- `Collision.hpp`：`Collision::Float3`、`Aabb`、`Capsule` 移除，改用 `Math::Vec3`、`Math::Aabb`、`Math::Capsule`；`VerticalCapsule`、`Contact` 保留，欄位型別改為 `Math::Vec3`。
  - `RaycastAabb`、`RaycastCapsule`（兩個多載）改為接收 `const Math::Ray&`。
  - `SweepSphereAgainstCapsule`（兩個多載）改為接收 `const Math::Segment&`。
  - header 註明：Collision 的 raycast 回傳世界空間距離（內部正規化方向），與 `Math::Intersect` 以 |direction| 為單位的 t 不同。
- `Collision.cpp`：刪除本地 `IsFinite`、`Length`、`Normalize`，改由 ADL 使用 Math 的版本。
- `CapsuleQueries.cpp`：
  - 內部 double `Vec3` 改用 `Math::Vec3d`；`V()`、`F()` 改用 `ToVec3d`、`ToVec3`。
  - `Closest`、`Clamp` 改用 `ClosestPoint(·, Segmentd)` 與 `ClosestPoint(·, Aabbd)`；`Finite` 改用 `IsFinite`。
  - `Roots`、`RayCapsule`、`RayRoundedBox` 與 contact 計算本體不動。
- `engine/collision/CMakeLists.txt`：PUBLIC 連結 `GYO::Math`。
- `tests/common/collision`：呼叫改為傳入 `Ray`／`Segment`，型別改用 Math 的型別。
- pvp：
  - 有編譯的 `Arena`、`Movement`、`PvpMatch`、`ShotQuery`、`CharacterCollision`：只做型別替換與 `Ray` 呼叫。
  - 未編譯 29 檔中引用 Collision 的 8 檔及其 header：同樣處理。
  - `match_domain` 明確 PUBLIC 連結 `GYO::Math`（原計劃在 B6a，因 pvp 程式碼已直接使用 Math 型別而提前）。
  - pvp 自己的 helper（例如 `Arena.cpp` 的 `Finite`）只改參數型別，去重複留到 B6a。
- 文件：`architecture.md` 的 Collision 一列、`math.md` 的邊界表。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 19／19 通過 |
| test preset | 45／45 通過（其中 3 項是 B0 之後由其他 PR 加入的 pvp 驗收測試，見下） |
| Collision digest | master 與 B2 在 25 萬筆查詢（涵蓋全部公開函式、例外行為）上逐位元相同，`-O2` 與 `-O0` 都相同；靈敏度檢查：把 `kEpsilon` 改成 1.1e-6 時出現 737 行差異 |
| pvp 未編譯 29 檔 | syntax-only 29／29 通過 |
| 依賴圖 | 與 B1b 相比只多 `gyo_collision → gyo_math`、`match_domain → gyo_math` |

### master 的變動（B0 之後）

- B0 基線（`0bd5363`）之後，master 先合併了其他 session 的 PR #11–#16（PvP v5 的 batch04、batch05、stable baseline 等），然後才合併 #17、#18。
- 影響：test preset 多出 `object_fps_pvp.action_runner`、`gameplay_soak`、`combat_gui_evidence` 三項；pvp 有 28 個檔案變動（+1381／−444）。
- 29 個未編譯檔案的清單經重新計算後不變。
- PLAN 中 pvp helper 的行號是規劃時量的，**B6 開始時必須重新盤點**。

## B3 Model

狀態：**完成**。PR [#20](https://github.com/yojinn-io/GYO-Engine/pull/20) 於 2026-10-03 合併為 `14a32ac`；最終 head `f116db0` 的 CI 四平台全部通過。

### 變更

- `ModelAsset.hpp`：`Model::Vec2/Vec3/Quaternion/Matrix4` 與 `Multiply`、`TransformPoint` 移除，改用 Math 的型別與函式。
  - `Transform`（資產節點的 TRS 契約）與 `ToMatrix` 保留在 Model；`ToMatrix` 改為呼叫 `Math::ComposeTRS`。
  - `Animation.hpp`、`ModelAsset.hpp` 的欄位型別改為 `Math::` 限定名稱；公開 header 不寫 using。
- `ModelAsset.cpp`：刪除 `Multiply`、`TransformPoint` 的本體。
- `Animation.cpp`：
  - 刪除本地 `Lerp`、guarded `Normalize`、`Slerp`；`BlendPoses` 等未限定的呼叫經 ADL 使用 Math 版本。
  - `Sample()` 以 callable 接收插值函式，改傳包裝 `Math::Lerp`、`Math::Slerp` 的 lambda。
  - **`Finite` 系列保留**：它是 Model 的驗證政策。`Finite(Quaternion)` 另有「長度平方大於 1e-12」的非退化規則；`Finite(Vec3)`、`Finite(Matrix4)` 只是轉呼叫 `Math::IsFinite`，讓 `ValidKeys` 模板能用同一組多載。B7 的 grep 稽核把它們列為已知例外。
- `AnimationTransfer.cpp`：
  - 刪除本地 `Normalize`；舊的 `Inverse`（其實是共軛）三處呼叫全部明確改為 `Math::Conjugate`。
  - `Product` 保留為組合函式，本體為 `Math::Normalize(Math::Multiply(a, b))`。
- ufbx loader、`render/model` 的 ModelRenderer：型別名稱改為 Math 的型別。
- `engine/model/CMakeLists.txt`：PUBLIC 連結 `GYO::Math`。
- 測試：`tests/common/model/ModelTests.cpp`、`tests/object_fps_pvp/PlayerPresentationTests.cpp` 補上需要的 using-declaration。
- pvp：有編譯的 `PlayerPresentation`、`WeaponPresentationDefinition`、`CharacterPresentationDefinition`，以及未編譯的 `EnemyRig`、`EnemyPresentationDefinition`：`Engine::Model::Vec3/Matrix4/Multiply/TransformPoint` 換成 `Engine::Math::`。
- 文件：`architecture.md` 的 Model 列。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 19／19 通過 |
| test preset | 45／45 通過 |
| Model digest | master 與 B3 共 1207 萬筆輸出逐位元相同（`-O2`、`-O0`），涵蓋 Model 全部公開函式與各錯誤分支；9 種刻意改動（例如 `Conjugate` 換回 `Inverse`、Slerp 閾值、乘法順序）都會被偵測到 |
| 漂移 | 0（逐位元相同，沒有需要記錄的漂移） |
| pvp 未編譯 29 檔 | syntax-only 29／29 通過 |
| 依賴圖 | 與 B2 相比只多 `gyo_model → gyo_math` |

### 對抗式審查

3 個 minor，沒有正確性問題：
- 用不到的 using-declaration：`UfbxModelTests.cpp` 還原為 master 版，其餘刪除用不到的宣告。
- `Animation.cpp` 的 `Finite` 系列：保留理由記錄於上方，PLAN 的 B7 稽核加上例外。
- 計劃文件尚未更新：本節與 dev_log、README 一併補上。

## B4a Render 型別與 helper

狀態：**完成**。PR [#21](https://github.com/yojinn-io/GYO-Engine/pull/21) 於 2026-10-03 合併為 `ed7a08a`；最終 head `d78b17c` 的 CI 四平台全部通過。

### 變更

- `RenderTypes.hpp`：`Render::Float2/Float3/Rect` 移除，`Transform3D`、`PerspectiveCamera3D`、`Vertex3D`、`UvTransform`、各 Submission 的欄位改用 `Math::Vec2/Vec3/Rect`；`Transform3D` 註解改指向 `Math::ComposeEulerXYZ` 慣例。
- 重複的 helper：
  - `RenderQueue`、`SdlGpuRenderDevice` 的向量與 `Rect` 版 `IsFinite` 刪除，經 ADL 使用 `Math::IsFinite`；`Renderer.cpp` 中沒人呼叫的 `IsFinite(Float3)` 刪除。
  - `IsFinite(float)`、`IsFinite(Color)` 保留（純量與 Render 語義型別），列為 B7 稽核例外。
  - `PrimitiveMesh`：`Add/Subtract/Scale/Cross` 改用 Math 的運算子與 `Cross`；`hypot` 長度改用 `Math::Length`；倒數相乘的正規化改用 `Math::Normalize`／除法。
- `ColorTransform`：公開的 `Decode/EncodeSrgbComponent` 刪除（與 Math 重複），`DecodeSrgbColor` 等與 `Renderer.cpp` 的 `CaptureComponent` 改用 `Math::DecodeSrgb/EncodeSrgb`；`ClampUnit` 改用 `Math::Clamp`。
- `render/model` 的 ModelRenderer：頂點複製改為 `{vertex.position + offset, vertex.uv}`。
- `Renderer.cpp` 的矩陣碼不動（B4b），只換型別名稱。
- `engine/render/CMakeLists.txt`：PUBLIC 連結 `GYO::Math`。
- UiRenderer：`ConvertRect` 改為回傳 `Math::Rect`（B5 移除 `UiRect` 時一併刪除）。
- pvp（`WeaponPresentationDefinition`、`WeaponViewModel`、`PlayerPresentation`、`PvpApplication`）與測試：型別名稱替換。
- `tests/common/render/sdl_gpu/RenderFeatureSmoke.cpp`：只把 `Float2/Float3` 拼寫換成 Math 型別（不留別名，決策 1），`RotateX/Y/Z`、`Project` 這個獨立 oracle 的計算式沒變；期望值中的 sRGB 改用 `Math::EncodeSrgb`（與被刪除的 Render 函式逐字相同，實際值來自 GPU readback，不經過受測的 `CaptureComponent`）。
- 文件：`architecture.md` 的 Render 列。

### 數值漂移紀錄（PrimitiveMesh，Apple clang 21／libc++，x86_64，`-ffp-contract=off`）

- **長度**：在所有分量的絕對值都在 [2⁻⁶⁴, 2⁶⁴] 的正常範圍，libc++ 的三參數 `hypot` 就是 `sqrt(x*x+y*y+z*z)`，與 `Math::Length` 逐位元相同（200 萬筆隨機向量，0 ulp）。libstdc++、MSVC 的 `hypot` 不同，預期在 Linux、Windows 上另有最多數 ulp 的差異（B1 characterization 實測倒數正規化最多 4 ulp）。
- **正規化**：倒數相乘改成除法，正規化後的分量有 27.5% 差 1 ulp（600 萬樣本，最大 1 ulp）。
- **線框網格**（`MakeWireBox`、`MakeWireCapsule`）：一般輸入（座標 ±100）頂點最大絕對差 3.97e-4（發生在條件很差的極短弧弦），軸對齊輸入最大 3.05e-5，測試用輸入最大 3 ulp。長度接近 1e-6 門檻或 `|axis.y|` 在 0.9 的 1 ulp 內時，少數輸入的頂點數或 `Perpendicular` 參考軸會改變。
- **逐位元相同**：quad、cube、UV 球體；ColorTransform 全部（sRGB 對全部 2³² 個 float 窮舉）；RenderQueue 驗證（20 萬筆）；ModelRenderer 頂點複製；`MakeSpriteUvTransform`。
- 現有測試沒有對 PrimitiveMesh 輸出做精確比較，因此沒有修改任何測試的容差。

### 溢位行為（審查發現，已修正）

- `Math::Length` 不做縮放，邊長或弧弦超過約 1.8e19 時會溢位。master 的 `hypot` 會先縮放。
- 修正前：`MakeWireBox` 在邊長超過約 1.8e19 時回傳 Ok，頂點卻是 NaN；`MakeWireCapsule` 在半徑約 1e20 時也一樣。
- 修正：兩個函式在回傳前檢查所有頂點都是有限值，否則回傳 `InvalidArgument`；header 註明這個上限，並新增測試。
- 與 master 的差異：這種尺度的輸入現在一律回傳錯誤（master 會回傳有限的幾何）。世界座標不會用到這種尺度，視為可接受的行為變更。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 19／19 通過 |
| test preset | 45／45 通過（含 sdl_gpu mesh smoke） |
| digest | 見上方漂移紀錄 |
| pvp 未編譯 29 檔 | syntax-only 29／29 通過 |
| 依賴圖 | 與 B3 相比只多 `gyo_render → gyo_math` |
| 對抗式審查 | 2 個 minor：溢位造成 Ok 加 NaN（已修正），文件未更新（本節） |

## B4b Render 矩陣切換到 Math 慣例

狀態：**完成**。PR [#22](https://github.com/yojinn-io/GYO-Engine/pull/22) 於 2026-10-03 合併為 `43bccad`；最終 head `2cbf2c6` 的 CI 四平台全部通過（含 Linux lavapipe GPU smoke）。

### 變更

- `Renderer.cpp`：
  - 刪除私有的 row-major／row-vector 矩陣 helper（`Identity`、`Multiply`、`Translation`、`Scale`、`RotationX/Y/Z`、`PixelProjection`）。
  - `WorldMatrix` 改用 `Math::ComposeEulerXYZ`；`ViewMatrix`、`SpriteWorldMatrix` 依 PLAN 對照表改為 column-vector 的鏡像鏈；`ProjectionMatrix` 改用 `Math::MakePerspective`；sprite 投影改用 `Math::MakeOrthographicPixels`。
  - mesh 的 WVP 為 `Multiply(Multiply(Proj, View), World)`，對應舊的 `World·(View·Proj)`，結合方式不變；sprite 為 `Multiply(PixelProj, SpriteWorld)`。
  - 新增 `ToShaderMatrix`：把 Math 的 16 個 float 依序 memcpy 到 `ShaderAbi::Matrix4`，不轉置；以 static_assert 鎖住大小與 trivially copyable。
  - `ShaderAbi`、HLSL、shader pipeline 都沒有修改。
- `tests/common/render/RendererTests.cpp`：新增 5 個 memcmp test case（World、WorldOverlay、ViewModel 三種 mesh 圖層，Scene、Overlay 兩種 sprite 圖層，各 40 幀 × 50 筆，執行期隨機輸入、隨機視窗尺寸）。
  - 凍結 master `ed7a08a` 的舊矩陣碼為 `Legacy`，逐筆比對每個 draw 上傳的 64 位元組 uniform。
  - 測試先在未修改的 Renderer 上通過，再於改寫後通過。
  - 依 PLAN D14 永久保留，作為 Renderer 矩陣的回歸測試；重用檔案內既有的 `Library()` 與 `Device` 測試替身。
- 文件：`rendering_architecture.zh-Hant.md`、`.ja.md` 的座標列改寫為 Math 慣例與 GPU 讀法（位元組相同、不轉置）。

### 驗收

| 項目 | 結果 |
|---|---|
| memcmp | 改寫前後都通過；20410 個斷言逐位元相同，數值漂移為 0 |
| 靈敏度檢查 | 8 種刻意改動全部被偵測，包括數學上等價的 WVP 重新結合，以及只差 1 ulp 的 `xScale` 算法；每種改動只讓它碰到的路徑失敗 |
| core preset | 19／19 通過 |
| test preset | 45／45 通過（含 pvp 的 `presentation_evidence` 與本機 metal 的 sdl_gpu smoke） |
| pvp 未編譯 29 檔 | syntax-only 29／29 通過 |
| 對抗式審查 | 3 個 minor：Legacy 來源標註錯誤（已更正）、測試替身重複（併入 `RendererTests.cpp` 重用）、文件未更新（本節）。審查者也以 flat index 對應逐項證明了各鏡像鏈逐位元相同 |

## B5 Ui 與 ui_editor

狀態：**完成**。PR [#23](https://github.com/yojinn-io/GYO-Engine/pull/23) 於 2026-10-04 合併為 `efe4a30`；最終 head `8d0487b` 的 CI 四平台全部通過。

### 進行方式（ultracode）

1. 盤點：3 個 agent 分別盤點 Ui 本體、UiRenderer 與 ui_editor、gyo.ui 資料契約與外部使用者。
2. 前置測試：在 master `43bccad` 的程式碼上新增並通過，再進行遷移。
   - gyo.ui v1 `Serialize` 的 golden：`kDocument`，以及新的全 ASCII 合成文件 `UiGoldenDocument.hpp`（涵蓋 image `source_uv`、panel、顏色 select、value 文字、字型覆寫、float32 數字格式）。
   - sRGB hex 往返（全部 256 個 byte）；decode 結果以凍結的舊版 `SrgbToLinear` 在執行期比對（不寫死依賴 libm 的常數）。
   - JSON 陣列位置對應到欄位的順序。
   - 半開區間點擊判定的右、下邊界。
   - 精確 layout 與 draw list 回歸（`UiLayoutGolden.hpp`，兩份文件 × 3 種 viewport，以 bit pattern 比對）。
   - `ClipSprite` 有限值的精確結果。
   - `tests/ui_editor`：凍結 `PreviewAdapter` 的 `LinearToSrgb` 與 `Math::EncodeSrgb` 比對（新 ctest `gyo_ui_editor.preview_srgb`）。
3. 遷移後全部前置測試照樣通過，再做差異比對與對抗式審查。

### 變更

- `UiTypes.hpp`：`UiFloat2`、`UiRect` 移除；`UiInputFrame`、各 draw command、`UiDesignCanvas`、`UiRectTransform` 欄位、`UiElement::sourceUv/itemStep`、`UiEvaluatedElement`、`HitTestUiLayout` 改用 `Math::Vec2`／`Math::Rect`。`UiColor`、`UiViewport`、`UiRectTransform` 保留。
- `UiRuntime.cpp`：`IntersectRect` 刪除，改用 `Math::Intersection`（參數順序不變）；`ContainsHalfOpen` 保留為 Ui 的半開區間點擊政策（加註說明不用閉區間的 `Math::Contains`）；`MakeFit`、`ResolveDesignRect`、`ToPixels` 只換型別。
- `UiValidation.cpp`：`IsFinite(UiFloat2)` 刪除，經 ADL 使用 `Math::IsFinite`；`source_uv` 的有限性檢查改用 `Math::IsFinite(Rect)`；`IsFinite(UiColor)` 保留（Ui 語義型別，列為 B7 稽核例外）。
- `UiColor.cpp`：`SrgbToLinear`、`LinearToSrgb` 刪除，改用 `Math::DecodeSrgb/EncodeSrgb`；byte 量化的 clamp 改用 `Math::Clamp`。
- codec（`UiDocumentCodec.cpp`、`UiSerialization.cpp`）只換型別，gyo.ui v1 格式與 schema 版本不變。
- `UiRenderer.cpp`：`ConvertRect` 刪除（直接指派）；`ClipSprite` 改用 `Math::Intersection`，目標矩形非有限時一律交給 RenderQueue 報錯（使用者決定，見決策紀錄）。
- `engine/ui/CMakeLists.txt`：`gyo_ui` PUBLIC 連結 `GYO::Math`。
- ui_editor：`PreviewAdapter` 的 `LinearToSrgb` 刪除，改用 `Math::EncodeSrgb`；alpha clamp 改用 `Math::Clamp`；`gyo_ui_editor_preview` PRIVATE 連結 `GYO::Math`。
- pvp：已編譯的 `PvpApplication.cpp` 與未編譯的 `ObjectFpsUi.cpp` 中的 `Engine::Ui::UiRect` 改為 `Engine::Math::Rect`（PLAN 原本沒列，盤點時發現）。
- `Math::Rect` 的註解：改為「查詢預設尺寸非負」，因為 Ui layout 會產生負尺寸的 bounds；`UiEvaluatedElement` 加註。
- 文件：`architecture.md` 的 Ui 與 UI editor 列。

### 驗收

| 項目 | 結果 |
|---|---|
| 前置測試 | 在 master 上通過，遷移後照樣通過（`gyo_ui_tests` 30 個 case） |
| 差異比對（master 對 B5，`-O2`／`-O0`） | 有限輸入全部逐位元相同：codec（含 38400 份突變文件）、layout／compose（4.7 萬個 viewport）、Update 序列（6.6 萬幀）、hit test（104 萬個點）、Renderer（368 萬次 submit）、sRGB 窮舉。唯一差異是決定中的非有限目標矩形一律報錯 |
| core preset | 19／19 通過 |
| test preset | 46／46 通過（多出 `gyo_ui_editor.preview_srgb`） |
| pvp 未編譯 29 檔 | syntax-only 29／29 通過 |
| 依賴圖 | 與 B4a 相比多出 `gyo_ui → gyo_math`、`gyo_ui_editor_preview → gyo_math`、`gyo_ui_editor_preview_srgb_tests → gyo_math` |
| 對抗式審查 | 1 個 major（舊行為描述不完整，已更正並由使用者再次確認）、5 個 minor（全部處理） |
| CI 四平台 | 最終 head `8d0487b` 全部通過。第一次執行時 windows-x64 建置失敗：`UiDocumentCodecTests.cpp`、`UiRuntimeTests.cpp` 把 `std::string_view` 串進 doctest 訊息（`FAIL`、`CAPTURE`），但沒有 include `<ostream>`。doctest 只前置宣告 `std::ostream`，MSVC 實例化 `operator<<` 時需要完整型別；本機 libc++ 可以編譯，沒有重現。兩檔補上 `<ostream>` 後重跑 |

### 範圍外，只回報

- `item_step` 沒有有限性驗證：`{NaN, 1}` 能通過 Validate，序列化後變成 `null` 而無法再讀回。修正會改變資料契約的驗證規則，需另行決定。
- ui_editor 的 letterbox、半開區間點擊判定、文字對齊，與 Ui 本體各有一份；`UiRuntime` 的 Evaluate 與 Compose 也重複了 layout 走訪。屬於 layout 邏輯，不是數學庫範圍。
- `Render::Color` 與 `UiColor` 同構（PLAN 已列）。

## B6a pvp 模擬層 `match_domain`

狀態：**CI 四平台通過，PR [#24](https://github.com/yojinn-io/GYO-Engine/pull/24) 待使用者合併**（分支 `claude/math-foundation-b6a`，自 master `efe4a30`）。

### 進行方式（ultracode）

1. 重新盤點：B0 之後 pvp 有大量變動（見 B2「master 的變動」），PLAN 的 pvp 行號作廢。3 個 agent 分別盤點 `match_domain` 的 helper、`fps::Float3`／`fps::Float2` 的全部使用者、決定性量測方法與 protocol 風險，再由 1 個 agent 做完整性檢查。
   - 行號更正：protocol 版本檢查在 `IpcHost.cpp:155`、`ClientConnection.cpp:274`；`PredictionTests.cpp` 的水平距離在 :39-41；PLAN 所說的 563、584 現在是 565、586。
2. 使用者決定兩項（見決策紀錄 2026-10-04）：Math 的角度換算改為 constexpr；跨平台驗證採「本機全模擬 digest＋CI characterization 測試」。
3. 實作後以 2 個 agent 驗證：全模擬 digest 與靈敏度檢查、對抗式審查。

### 變更

- **型別**：
  - `fps::Float3` 刪除，全部改為 `Engine::Math::Vec3`（133 處、29 個檔案：`match_domain` 22、app_support 的 header 3、測試 6、acceptance 7、未編譯檔與其 header 95）。
  - `fps::Float2` 改名為產品語義型別 `fps::GroundPoint`（XZ 地面上的點或偏移量；95 處、19 個檔案），header 移到 `RetroFPS/World/GroundPoint.hpp`。
  - `RetroFPS/Math/Vector.hpp` 刪除；原本 include 它的 13 個 header 改為各自 include 實際用到的 `GroundPoint.hpp` 或 `engine/math/linear/Vec3.hpp`。
- **`match_domain` 的 helper 改用 Math**：
  - `Movement.cpp`：pitch 上限 `pi/2` 改為 `HalfPi`（與 Go gateway 的 `float32(math.Pi/2)` 同一個 float），yaw 改用 `WrapRadians`，pitch clamp 改用 `Clamp`。
  - `Movement.hpp`：`MovementMaximumPitch` 改為 `DegreesToRadians(89.0F)`。
  - `ShotQuery.cpp`：`IsFinite`、`WrapRadians`、`Clamp`、`Normalize`；地板命中改為 `Intersect(Ray, Plane{})`（`Plane{}` 即 y = 0），半空間政策（起點在地板上或以下時距離為 0）留在產品。
  - `PvpMatch.cpp`：spawn 的 yaw 改用 `WrapRadians`；spawn 距離改為 `LengthSquared(ToVec3d(p - feet))`（先以 float 相減、再以 double 平方，與原式相同）。
  - `LocalPlayerPrediction.cpp`：`Interpolate`、`Difference`、`Length`、`Finite` 刪除，改用 `Lerp`、運算子、`Length`、`IsFinite`；float 的 clamp 與 max 改用 Math。
  - `CharacterCollision.cpp`：向量運算改用 Math 運算子、`Length`、`Dot`、`Min`／`Max`（參數順序不變）；水平長度的 `std::hypot` 改為 `Length(Vec3{x, 0, z})`。
  - `PlanarMovement.cpp`：clamp 改用 `Clamp`；倒數相乘的正規化改為對 `Vec3{x, 0, z}` 使用 `LengthSquared`、`Normalize`。
  - `Arena.cpp`：`Finite` 刪除，改用 `IsFinite(Vec3)`、`IsFinite(Aabb)`。
  - 各處的 identity 拷貝（`{p.x, p.y, p.z}`）改為直接傳值。
- **保留為產品政策**：各種容差與迭代次數；`ShotQuery` 的眼睛位置與 `CharacterCollision` 試探位移的逐分量寫法（改成向量加法會改變 -0 或把 0·inf 帶進 y）；二分法的中點；double 與整數的 clamp／min／max（Math 的 `Min`／`Max`／`Clamp` 只有 float 版本，改用會悄悄縮窄）；`Percentile`。
- **Math**：`DegreesToRadians`、`RadiansToDegrees` 改為 constexpr；`MathTests.cpp` 新增「編譯期求值與執行期逐位元相同」的測試；`math.md` 數值政策加註。
- **未編譯檔的提前處理**：型別換成 `Vec3` 後，以下 helper 會與 Math 經 ADL 歧義，使 syntax-only 檢查失敗，因此在 B6a 先處理：
  - `CombatCollision.cpp` 的 `IsFinite`、`Length`、`Normalize`，以及 `GameSession.cpp` 的 `Length`：與 Math 逐位元相同，刪除後改為明確呼叫 `Engine::Math::`。
  - `ProjectileSystem.cpp` 的 `IsFinite`、`Length` 刪除；`Normalize` 遇到零或非有限值會 throw，語意與 Math 不同，改名為 `NormalizeOrThrow`。
  - 其餘 helper（`AddScaled`、`Subtract`、`ToCollision` 等）照計劃留給 B6c。
- **測試**：
  - `PredictionTests.cpp` 的水平距離 `Distance` 改名為 `HorizontalDistance`（13 個呼叫點），語意不變。
  - `TimelineTests.cpp` 補上 `<numbers>`：它原本經由 `Movement.hpp` 間接取得，而 `Movement.hpp` 已不再需要它。
  - 新增 `tests/object_fps_pvp/MathCharacterizationTests.cpp`（`object_fps_pvp.cpu` 的一部分，8 個 test case）：凍結 master `efe4a30` 的各 helper，在執行期輸入下與 Math 比對。

### 數值

- **逐位元相同**：除了下面兩項，所有替換都逐位元相同，characterization 測試以精確比對鎖定。
- **漂移 1：`ComputePlanarInput` 的正規化**（authority 與 prediction 都會經過）：
  - 由 `x·(1/√s)` 改為 `x/√s`，`s` 不變。推導上限 2 ulp（同一個 binade 內最多 1 ulp）；本機實測最多 1 ulp，需要正規化的輸入中 37214／95023（39%）不同；不需要正規化的輸入完全相同。
- **漂移 2：`CharacterCollision` 的水平長度**（`hypot` 改為 `sqrt`）：
  - 只在 `constrainToFloor = true` 時執行，唯一的呼叫者是未編譯的 `EnemySystem.cpp`；PvP 都傳 false，所以 PvP 模擬不會經過這條路徑。
  - 推導上限：長度 4 ulp、正規化分量 5 ulp（假設各平台 `hypotf` 誤差在 1 ulp 內）；本機實測長度最多 1 ulp、分量最多 2 ulp，`1e-6` 門檻沒有翻轉。
- **全模擬 digest**（本機 mac x64、Apple clang、`-ffp-contract=off`，harness 在 scratch，不進 repo）：
  - 情境：平面輸入、`StepMovement` 連鎖、`MoveCharacterBody`（含 `constrainToFloor` 兩種）、`QueryShot`、PvpMatch 兩人對戰（斷線、射擊、擊殺、復活）、prediction 加虛擬線路（6 種延遲、丟包、stall、斷線設定）、`MatchRuntimeHost`、spawn 近似平局。共 5 個 arena、62 個情境 digest；規模 1 與 10。
  - **isolation 版**（branch 只把上述兩項改回舊寫法）：在 -O2、-O0 以及規模 1、10 下，62 個情境都與 master 相同，trace 逐 byte 相同（規模 10 為 3.86 GB）。其餘替換的逐位元相同因此得到確認。
  - **master 對 branch**：差異全部歸因於這兩項。`hypot` 只影響 `constrainToFloor = true` 的碰撞情境；平面正規化影響移動、authority、prediction、host 情境，不影響射擊情境。
    - authority 位置最大差 2.67e-5 m，prediction 2.15e-5 m，host 1.34e-5 m；沒有任何離散狀態分歧（grounded、生命狀態、命中、擊殺、復活、spawn 選擇都相同）。
    - prediction 的 snap 次數、平滑修正次數、最大修正量、最大 snap 位移都與 master 相同（規模 10：snap 106、平滑修正 28583）。
    - 合成的退化狀態（站在 3 m 高的角落牆頂上，PvP 內容中無法到達）中，1 ulp 的差異會讓接觸判定翻轉，差異最大 0.186 m。這表示邊緣幾何會放大 ulp 級差異，但仍在 1 m 的 snap 門檻以下。
  - -O2 與 -O0 的 digest 相同；連結 CMake 建出的 archive 時，digest 與手動建置相同。
- **Protocol**：不升級，`apps/object_fps_pvp/protocol/` 沒有變動。wire 上的 float 都原樣傳送，沒有狀態 hash；client 每個 snapshot 都以 authority 重建，再 replay 至多 12 步。新舊版本混連時的漂移會被平滑修正吸收。`ValidMovementCommand` 的上限（`HalfPi`、1e6）與 Go gateway 一致，接受條件不變。

### 驗收

| 項目 | 結果 |
|---|---|
| core preset | 19／19 通過（pvp 不在 core preset 中） |
| test preset | 46／46 通過；`object_fps_pvp.cpu` 含新的 characterization 測試 |
| gateway Go 測試 | `object_fps_pvp.go.vet`、`object_fps_pvp.go.test` 通過（`ctest -L go`） |
| pvp 未編譯 29 檔 | syntax-only 29／29 通過 |
| 依賴圖 | 與 B5 相同，沒有新的邊 |
| 全模擬 digest | 見上節 |
| 靈敏度檢查 | 7 種刻意改動中 5 種被抓到。其餘 2 種是等價改動：`Min(Dot, 0)` 參數對調只改變零的正負號，之後被吸收；`NormalizeOrZero` 在 `s > 1` 時與 `Normalize` 同式 |
| 對抗式審查 | 沒有推翻正確性；審查者另寫的 driver 也得到同樣結論。指出 1 個 minor（HANDOFF 與 dev_log 尚未記錄、README 與 PLAN 的 digest 說法未更新）與 2 個 nit（漂移應寫成推導上限、`MathTests` 新 helper 應放進匿名 namespace），都已處理 |
| CI 四平台 | PR #24 的 head `a9a5e85` 全部通過（windows-x64、linux-x64、macos-arm64、macos-x64、CI gate），characterization 測試的推導上限在四平台都成立 |

### Architecture Delta

- **產品內部的檔案移動**：`RetroFPS/Math/Vector.hpp` 刪除，`GroundPoint` 放在 `RetroFPS/World/GroundPoint.hpp`。理由：刪除 `Float3` 後該檔只剩產品語義型別，留在 `Math/` 會讓人誤以為是數學重複定義。不涉及頂層目錄、CMake 或 Product Boundary，依賴方向不變。
- **Math 公開介面**：`DegreesToRadians`、`RadiansToDegrees` 加上 constexpr，相容擴充。
- 沒有新的依賴邊（`match_domain → gyo_math` 在 B2 已加入）。

### 範圍外，只回報

- client 與 server 只比對 arena 的 id 與 version，不比對內容；client 以自己的 arena 檔做 prediction。
- client 與 server 可以跑在不同平台，`sinf`／`cosf` 本來就可能相差 1 ulp；B6a 的漂移屬於同一類。
- 瞄準方向公式在 `ShotQuery.cpp` 與未編譯的 `GameSession.cpp` 各有一份；`AddScaled` 在三個未編譯檔各有一份（B6c）。

## B6b pvp 表現層

狀態：**本機驗收完成，PR [#25](https://github.com/yojinn-io/GYO-Engine/pull/25) 待 CI**（2026-10-04，分支 `claude/math-foundation-b6b`，疊在 B6a 分支 `a9a5e85` 上）。

使用者在 B6a 進行中暫離，指示把剩下的批次照計劃做完：每批 commit、開 PR、跑 CI，有問題就修正後重跑。合併沒有在指示中，所以 B6a 之後的 PR 疊在前一批的分支上，等使用者回來決定合併。

### 進行方式（ultracode）

1. 盤點（B6a 驗證期間進行）：2 個 agent 分別盤點 app_support 等已編譯的非 `match_domain` 程式，以及 acceptance 與測試；再由 1 個 agent 檢查遺漏。
   - runtime_host、ipc、client_network、`main.cpp`、`match_main.cpp` 沒有向量或角度運算，只有 wire 編解碼與 double／整數的 min／max。
   - PLAN 的行號更正：`PvpApplication.cpp:758` 現在是 810-812；`PlayerPresentation.cpp:476` 現在是 670-677；`WeaponPresentationDefinition.cpp:64-84` 現在是 64-85。
2. 實作後以 2 個 agent 驗證：表現層 digest（含 isolation 版）與對抗式審查。

### 變更

- `SnapshotTimeline.hpp`（`match_domain` 的 header，只有表現層使用）：位置與 pitch 插值改用 `Lerp`，yaw 改用 `LerpRadiansShortest`，yaw 差改用 `WrapRadians`，有限性改用 `IsFinite`；水平速度的 `std::hypot` 改為 `Length(Vec3d{dx, 0, dz})`。
- `PvpApplication.cpp`：滑鼠的 yaw／pitch 改用 `WrapRadians`、`Clamp`；世界相機的 FOV 字面值 `1.0471975512F` 改為 `DegreesToRadians(60.0F)` 常數（同一個 float）；牆的中心與尺寸改用 `Center(Aabb)` 與 `max - min`；float 的 min 改用 `Math::Min`；identity 拷貝刪除。
- `PlayerPresentation.cpp`：
  - 第三人稱武器位置 `weaponWorldPosition` 原本手寫 yaw 旋轉，改為 `TransformPoint(ComposeEulerXYZ(transform), mount - anchor)`，`transform` 就是提交給 renderer 的那一個，與 GPU 使用同一個 World 矩陣。
  - 武器 mount 四元數：原本以 double 求倒數再相乘，改為 `Math::Normalize`；驗證改為 float 的 `LengthSquared`（非有限或 `< 1e-12F` 時拒絕）。
  - 移動距離改用 `Length(Vec3d)`，後退判定改用 `Dot(Vec3d)`；float 的 min／max 改用 Math；`Finite` 刪除，改用 `IsFinite`；offset 改為 `-anchor`。
- `WeaponPresentationDefinition.cpp`：muzzle 原本逐步縮放與旋轉，改為 `TransformPoint(ComposeEulerXYZ(transform), point - idleAnchor)`，即 renderer 的提交契約；角度換算改用 `DegreesToRadians`；`Finite` 刪除；identity 拷貝刪除。
- `WeaponViewModel.cpp`（offset 改為 `-idleAnchor`）、`CharacterPresentationDefinition.cpp`（`IsFinite`）、`WeaponShotGeometry.hpp`（上限改用 `Math::Pi`）。
- **保留**：
  - 相機眼睛位置的逐分量寫法（向量加法會改變 -0）、牆頂裝飾條與地板格的配置、射擊的 recoil 衰減、double 與整數的 clamp／min／max。
  - acceptance 與測試中的量測計算（水平距離、yaw 目標、π 常數等）：它們是獨立 oracle，維持原寫法，與 B6a 保留 `HorizontalDistance` 相同。
- **測試**：
  - 新增 `PresentationMathCharacterizationTests.cpp`（presentation 測試 target）與凍結舊式的 `PresentationLegacyMath.hpp`。
  - `PlayerPresentationTests.cpp` 新增正式內容的比對（mount 四元數、mark23 的 muzzle），以及 mount 驗證的拒絕案例（零、過小、float 溢位）。
  - characterization 共用的 helper 移到 `CharacterizationSupport.hpp`，B6a 的測試也改用它。

### 數值

- **逐位元相同**：除了下列四項，所有替換都逐位元相同，由 characterization 測試以精確比對鎖定。
- **muzzle**：
  - 只繞 Y 軸旋轉且 scale 為 1 時，矩陣元素都是精確值，結果與舊式相同。這是正式內容載入時的情況（mark23：旋轉 (0, 180°, 0)、scale 1），`shotGeometry` 不變。
  - 只有 recoil 讓 X 旋轉不為 0 時才漂移：正式內容最多 6.0e-8 m。推導上限是 32 u M（M 為平移量加上 scale 乘以點到 anchor 的 L1 距離，u = 2⁻²⁴）；隨機輸入實測 2.96 u M。
- **`weaponWorldPosition`**：同一上限；只在客戶端顯示與手動 GUI 證據中使用，證據只檢查有限性。
- **mount 四元數**：推導上限 4 ulp，隨機輸入實測 3 ulp；正式內容逐位元相同。
  - 驗證的行為改變（正式內容都碰不到）：
    - 分量大到平方和超出 float 範圍（約 1.85e19 以上）時，原本可以載入，現在拒絕（digest 實例：`[1e20,0,0,1e20]`、`[1.85e19,0,0,0]`、`[1e19]×4`）。
    - `|q|²` 恰在 1e-12 門檻上時，float 與 double 的捨入可能讓判定翻轉（實例：`[0,0,0,1e-6]`、`[5e-7]×4` 原本拒絕，現在接受）。
- **double 水平長度**（`hypot` 改為 `sqrt`）：推導上限 4 ulp。本機（Apple libm）只在兩個位置的指數差很多時不同：距離最多 1 ulp、速度最多 2 ulp；phase 的累加差在 1e-15 以內，沒有改變任何離散狀態或 pose。一般的移動排程幾乎碰不到這類輸入。

- **表現層 digest**（本機 mac x64，harness 在 scratch）：
  - 兩邊各自編譯 `PlayerPresentation`、`WeaponPresentationDefinition`、`CharacterPresentationDefinition`、`AnimationSetDefinition`，連結 repo 建出的 engine 靜態庫，載入正式內容。
  - 涵蓋：player／weapon definition 載入（含 52 組改寫的四元數與 24 組改寫的 viewmodel）、5 個 clip 的 muzzle 加 recoil、`SnapshotTimeline` 的 Push／Sample（8 萬次 Sample，含亂序、hold、死亡、epoch 變更）、locomotion 與 pose 取樣（共約 3400 萬筆 pose 資料）、以 CPU 假裝置跑真正的 `Initialize`／`Submit`（4830 筆 observation），以及 `PvpApplication` 公式的逐字複本。共 13 個部分。
  - **isolation 版**（branch 只把四項漂移改回舊寫法）：-O2 與 -O0 下 13 個部分都與 base 逐位元相同（約 4190 萬筆）。
  - **base 對 branch**：差異只在四項漂移。muzzle 在 recoil 為 0 時完全相同；`weaponWorldPosition` 最多 1 ulp（±60 m 下 3.8e-6 m），y 分量相同；其餘 observation、頂點上傳、queue 的 transform 都相同；locomotion 的離散狀態（reset 原因、後退、jogging、phase、動作）都相同。
  - 靈敏度檢查：2 種非等價的 ulp 級改動被抓到；2 種等價改動沒有誤報。

### 驗收

| 項目 | 結果 |
|---|---|
| test preset | 46／46 通過；`object_fps_pvp.presentation_cpu` 含新的 characterization 與正式內容比對 |
| gateway Go 測試 | 通過 |
| pvp 未編譯 29 檔 | syntax-only 29／29 通過（`WeaponShotGeometry.hpp` 也被它們使用） |
| 依賴圖 | 沒有變化（只新增測試來源） |
| 表現層 digest | 見上節 |
| 對抗式審查 | 沒有推翻正確性。1 個 minor：mount 驗證的接受案例沒有確認載入成功，且離門檻太遠，改為 1.01e-6 並確認載入與正規化結果；1 個 nit：四元數判定翻轉的計數在取樣範圍內不可能失敗，刪除（翻轉由 `PlayerPresentationTests` 的確定性案例涵蓋）。另依 digest agent 的提醒，測試輸入的 `std::pow(10, x)` 改用執行期的底數，避免 clang -O2 換成 `exp10` 使輸入隨最佳化等級改變 |
| CI 四平台 | 待 CI |

### Architecture Delta

- 沒有。只改 `object_fps_pvp` 內部的計算與測試；renderer 的提交契約（`ComposeEulerXYZ` 與 `-anchor` offset）改由產品直接呼叫 Math 表達，不新增依賴邊。

## B6c pvp 未編譯的 29 個檔案

狀態：**本機驗收完成**，PR 待開（2026-10-04，分支 `claude/math-foundation-b6c`，疊在 B6b 分支上）。

### 進行方式（ultracode）

1. 盤點（B6b 驗證期間進行）：2 個 agent 依目錄分擔 29 個檔案與只有它們使用的 header，再由 1 個 agent 檢查遺漏。
2. 這些檔案不在任何 target 中，無法執行。驗證方式：
   - 逐檔 syntax-only；
   - 1 個 agent 把每個改動的新舊寫法抽到 scratch，以執行期輸入比對；
   - 1 個 agent 做對抗式審查。

### 變更

- **GameSession**：
  - `AddScaled`、`Subtract` 刪除，改用 Vec3 運算子；muzzle 的偏移鏈維持 z、x、y 的加法順序。
  - 相機基底 `MakeViewBasis` 改為由 renderer 的方向契約求得：`ComposeEulerXYZ({}, {pitch, yaw, 0}, 1)` 的三個軸。
  - FOV 預設值 `pi/3` 改為 `DegreesToRadians(60.0F)`（同一個 float）；float 的 min／max 改用 Math；距離判斷改用 `Distance`。
- **ProjectileSystem**：`AddScaled` 刪除；向量運算與 min／max 改用 Math；`NormalizeOrThrow` 保留（會 throw 的驗證，Math 沒有對應）。
- **CombatCollision**：
  - `AddScaled` 與 identity 的 `ToCollision(Vec3)` 刪除（產品 `VerticalCapsule` 的轉換保留）。
  - 地板命中改為在產品的條件內呼叫 `Intersect(Ray, Plane{{0,1,0}, sweepRadius})`。
- **EnemyRig**：骨骼的世界座標改為 `TransformPoint(ComposeEulerXYZ({x, 0, z}, {0, yaw, 0}, scale), p - anchor)`，即 `EnemyPresentation` 提交給 renderer 的契約（與 B6b 的第三人稱武器相同）。
- **EnemySystem**：
  - 兩處水平長度的 `std::hypot` 改為 `Length(Vec3{dx, 0, dz})`（B6a 的 CharacterCollision 先例），近戰命中點改用 `Lerp`；float 的 min／max 與 `IsFinite` 改用 Math；identity 拷貝刪除。
  - `GroundPoint` 的 helper（`IsFinite`、`Distance`、`SurfaceDistance`、格子的線段檢查）留在產品：Math 沒有 XZ 型別。
- **EnemyPresentationDefinition**：武器四元數比照 B6b 的 mount（`Math::Normalize` 與 float 的 `LengthSquared` 驗證）；min／max 與 `IsFinite` 改用 Math。
- **其餘**：
  - `Player`（`RadiansToDegrees`）、`PlayerController`（`WrapRadians`、`DegreesToRadians`、`Clamp`）、`PlayerCombatState`、`WeaponController`、`ObjectFpsUi` 的 float clamp／min／max。
  - `CampaignContent`（`IsFinite`）、`MapGeometryGenerator`（`Pi`／`HalfPi`）、`ObjectFpsPresentation`（identity 拷貝與淡出的 `Clamp`）、`EnemyPresentation`（`-anchor` 與 identity 拷貝）。
- **保留**：
  - double 與整數的 min／max／clamp；遊戲規則與容差；淡入淡出的 smoothstep；`GroundPoint` 的 XZ 運算。
  - **B7 稽核例外**：`EnemySystem.cpp` 的 `IsFinite(GroundPoint)`、`Distance(GroundPoint)`（產品語義型別 `GroundPoint` 的 helper），以及 `ProjectileSystem.cpp` 的 `NormalizeOrThrow`（會 throw 的驗證，Math 沒有對應）。
  - `MapGeometryGenerator` 的牆面法線常數：用旋轉求得會帶入 `cos(π/2)` 的捨入雜訊，所以維持字面值。

### 數值（這些檔案不會執行，以 scratch 抽出的新舊寫法實測）

- **逐位元相同**：56 個比對項目中有 51 個逐位元相同，包括 NaN payload、例外判定與回傳值。每項以 100 萬到 43 億筆執行期輸入比對；`WrapRadians`、`DegreesToRadians`、`RadiansToDegrees` 窮舉了全部 2³² 個 float。
- **漂移與邊緣差異**（5 項，都在預期內）：
  - **相機基底**：非零分量完全相同；只在 yaw 或 pitch 恰為 ±0 時，零的正負號可能不同（例如出生時的 `forward.y`）。yaw 或 pitch 非有限時，新寫法九個分量都是 NaN。遊戲中角度一律有限。端到端（射擊、muzzle、tracer）只有零號差異，命中結果相同。
  - **EnemyRig**：y 相同；x、z 在位置量級主導時最多差 2 ulp（結果跨 2 的冪次時），99.9% 在 1 ulp 內；原點附近最多約 4 ulp（相對於位置與縮放後偏移中較大者）；±60 m 內絕對差最多 7.6e-6 m。yaw 非有限時 y 也變成 NaN（原本有限），遊戲中 yaw 來自有限值的 `atan2`。
  - **水平長度**（`hypot` 改為 `Length`）：實際範圍內最多 1 ulp，判定不變；極端值（約 1.8e19 以上或 1e-19 以下）會上溢或下溢；牆面恰好落在 muzzle 距離上時，1 ulp 可能翻轉遮擋判定（極端輸入 100 萬筆中 7 筆，實際範圍 0 筆）。
  - **地板命中**：只有 `sweepRadius` 與 `origin.y` 都是 -0 時，距離與命中點 y 的零號不同；沒有呼叫者傳入 -0。
  - **敵人武器四元數**：最多 3 ulp；正式內容（object_fps_pvp 與 object_fps_v2 的 `ranged.enemy.json`）逐位元相同；平方和超出 float 範圍的內容改為拒絕；門檻上的翻轉只有「原本拒絕、現在接受」一個方向。

### 驗收

| 項目 | 結果 |
|---|---|
| pvp 未編譯 29 檔 | syntax-only 29／29 通過 |
| test preset | 46／46 通過（已編譯的程式沒有變動） |
| 新舊寫法比對 | 見上節（scratch，Apple clang x86_64，-O2 與 -O0 相同） |
| 對抗式審查 | 沒有推翻正確性。1 個 minor：保留的 `IsFinite(GroundPoint)` 未登記為 B7 稽核例外，已在本節與 PLAN 登記；2 個 nit：變更清單的 `CampaignContent` 寫錯、EnemyRig 漂移的描述太樂觀，都已更正 |
| CI 四平台 | 待 PR |

### Architecture Delta

- 沒有。只改 `object_fps_pvp` 未編譯檔案內部的計算；不改 CMake、依賴或 target。

### 範圍外，只回報

- `GroundPoint` 的有限性檢查在 `EnemySystem`、`GridCollision`、`GridMap` 各寫一份；格子線段檢查在 `GameSession` 與 `EnemySystem` 各一份。屬於產品內部的重複，不是 Math 的範圍。
- `EnemyPresentationDefinition.cpp:160` 接受的攻擊 clip 長度上限是 `attackInterval + 1e-5`，`EnemySystem.cpp` 的驗證是 `+ 1e-6`；落在兩者之間的內容能載入，但會在 `EnemySystem` 驗證時失敗。

## 未結事項

- B6c 本機驗收完成；B7（收尾）。B6a（#24）、B6b（#25）待使用者合併。
