# Math 基礎統一：交接

更新：2026-10-03。**B0 基線完成，PR [#17](https://github.com/yojinn-io/GYO-Engine/pull/17)（分支 `claude/math-foundation-b0`）。B1 未開始，需使用者指示啟動。**

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

## B0 基線

狀態：**完成**（2026-10-03，分支 `claude/math-foundation-b0`，基準 commit `0bd5363`）。
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

## 未結事項

- B1 開始時確認最近點查詢函式的命名（結構清單已依樹狀圖定案，見 PLAN 1.5）。
- B1 確認各 primitive 的基本查詢清單。
- B2 開始時確認 Collision 的 raycast 介面是否改成接收 `Math::Ray`（公開介面變更）。
- B1、B4b、B6a 是否使用 ultracode 對抗式檢查，屆時逐次徵求同意。
