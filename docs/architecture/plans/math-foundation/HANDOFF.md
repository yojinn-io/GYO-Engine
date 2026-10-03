# Math 基礎統一：交接

更新：2026-10-03。**B0–B4a 已合併（B4a 為 #21，合併為 `ed7a08a`）。B4b 本機驗收完成（分支 `claude/math-foundation-b4b`），待 commit、PR 與 CI。**

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
| 2026-10-03 | 本計劃全程使用 ultracode（盤點、實作、驗證、審查都以多 agent workflow 進行）：這是底層概念模型的變更，且已有大量實作依賴它 | 使用者（B4b 進行中） |
| 2026-10-03 | Collision 的 raycast 改為接收 `Math::Ray`（Ray 是幾何 primitive，`RaycastAabb`／`RaycastCapsule` 是 Collision 演算法） | 使用者 |
| 2026-10-03 | `SweepSphereAgainstCapsule` 的路徑改為接收 `Math::Segment` | 使用者 |
| 2026-10-03 | 浮點收縮模式全專案統一為不收縮（`-ffp-contract=off`，MSVC 預設 `/fp:precise`）；優先平台為 Linux、Windows、mac x64，arm64 為附帶產物 | 使用者 |
| 2026-10-03 | `Matrix3`／`Matrix4` 提供 `Determinant`、`Inverse`（不可逆時回傳 `std::optional` 空值）、`Transpose`；`Quaternion` 提供軸角建構、`Rotate`、`Inverse` | 使用者 |

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

狀態：**本機驗收完成**（分支 `claude/math-foundation-b4b`，自 master `ed7a08a`）。待 commit、PR 與 CI 四平台。

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

## 未結事項


- B2 開始時確認 Collision 的 raycast 介面是否改成接收 `Math::Ray`（公開介面變更）。
- B1、B4b、B6a 是否使用 ultracode 對抗式檢查，屆時逐次徵求同意。
