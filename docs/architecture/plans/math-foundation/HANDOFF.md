# Math 基礎統一：交接

更新：2026-10-03。**B0 完成，PR [#17](https://github.com/yojinn-io/GYO-Engine/pull/17) 已合併（`0dd3077`，2026-10-03）。B1 與 B1b 本機驗收完成，合併在 PR [#18](https://github.com/yojinn-io/GYO-Engine/pull/18)，待 CI 四平台。**

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

狀態：**本機驗收完成，PR [#18](https://github.com/yojinn-io/GYO-Engine/pull/18) 待 CI 四平台**（分支 `claude/math-foundation-b1`）。

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
| CI 四平台 | 第一次執行時，linux-x64 與 windows-x64 的 `gyo_math_tests` 失敗：倒數相乘正規化的漂移上限只依 Apple 實測設為 2 ulp，而兩平台 hypot 實作不同，實測 4 ulp。上限改為由誤差組成推導的 `kHypotDriftUlpBound + 2`（5 ulp）後重跑。其餘 88 個 test case 在兩平台都通過，Math 與測試程式碼在 gcc-14、MSVC 下都沒有警告 |

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

## 未結事項


- B2 開始時確認 Collision 的 raycast 介面是否改成接收 `Math::Ray`（公開介面變更）。
- B1、B4b、B6a 是否使用 ultracode 對抗式檢查，屆時逐次徵求同意。
