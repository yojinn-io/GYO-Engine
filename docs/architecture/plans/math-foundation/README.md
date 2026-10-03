# Math 基礎統一：分批計畫與進度

更新：2026-10-03。Owner：Engine（新增模組 `GYO::Math`）。
**B0 完成；B1 本機驗收完成，待 CI。**

GYO 的向量、矩陣、四元數和常用運算在 Render、Model、Collision、Ui 與 `object_fps_pvp` 各自定義，
兩份 `Matrix4` 的儲存順序與預設值也不同。本計畫新增最底層模組 `GYO::Math`，
把型別、運算與座標慣例收斂成單一實作。

先讀 [核准版計畫](PLAN.md)（契約、逐項處置、批次與驗收）和 [交接](HANDOFF.md)。

## 已定案方針（2026-10-03）

1. 舊名稱完全移除，不留別名；engine 與 active 程式碼只使用 `Engine::Math::*`。
2. 每個運算只保留一種實作；接受浮點結果改變，但每處改變都要量測並記錄。
3. Math 定位為完整基礎庫，Architecture Delta 逐項說明收錄理由。
4. `object_fps_pvp` 中 29 個從未編譯的 `.cpp` 一起遷移，以逐檔 syntax-only 檢查驗證。
5. 追加：依使用者的樹狀圖補齊常用結構：Linear Algebra（`Vec2/3/4`、`Matrix3/4`、`Quaternion`）、
   Geometry（`Ray`、`Plane`、`Sphere`、`Aabb`、`Capsule`、`Segment`、`Triangle`）、Scalar／Utility（見 PLAN 1.1、1.2、1.5）。
6. 向量命名為 `Vec2/Vec3/Vec4`，純量用後綴（`Vec3d`、`Vec2i/Vec3i`）。
7. 分界：Math 為基礎數學型別加純幾何運算；Collision 為碰撞判定、接觸、穿透、掃掠的領域演算法（見 PLAN 1.6）。

範圍不含未啟用產品（`object_fps`、`object_fps_v2`、`tools/object_fps_preview`），這些不做任何修改；
舊名稱移除後它們重新啟用時需要先遷移，B7 會產出破損清單。

## 進度

| 批次 | 建議檔位 | 狀態 | 交付邊界 |
|---|---|---|---|
| B0 基線 | medium | 完成，2026-10-03，PR [#17](https://github.com/yojinn-io/GYO-Engine/pull/17) 已合併 | core／test 基線、依賴圖、29 檔 syntax-only 基線、本文件與 HANDOFF |
| B1 `GYO::Math` | high（慣例與數值 xhigh） | 本機驗收完成，PR [#18](https://github.com/yojinn-io/GYO-Engine/pull/18) 待 CI | 新模組（依樹狀圖的完整型別）、`gyo_math_tests`、characterization、`docs/architecture/math.md` 初稿 |
| B2 Collision | high（double 路徑 xhigh） | 未開始 | `Collision::Float3/Aabb/Capsule` 移除，最近點查詢與 helper 改用 Math；`VerticalCapsule` 保留 |
| B3 Model | high（矩陣與四元數 xhigh） | 未開始 | `Model::Vec2/Vec3/Quaternion/Matrix4` 移除 |
| B4a Render 型別 | high | 未開始 | `Render::Float2/Float3/Rect` 移除，Render helper 改用 Math |
| B4b Render 矩陣 | xhigh | 未開始 | `Renderer.cpp` 改用 Math 慣例，memcmp 逐位元驗證 |
| B5 Ui／ui_editor | high | 未開始 | `UiFloat2/UiRect` 移除，sRGB 統一，gyo.ui golden |
| B6a pvp 模擬層 | xhigh | 未開始 | `fps::Float3` 移除，`match_domain` helper 改用 Math，digest 含 arm64 |
| B6b pvp 表現層 | high | 未開始 | `app_support` 與 acceptance |
| B6c pvp 未編譯檔 | high | 未開始 | 29 檔遷移與 syntax-only 驗證 |
| B7 收尾 | medium | 未開始 | 文件定稿、grep 稽核、未啟用產品破損清單、Architecture Report |

```text
B0 -> B1 -> B2 -> B3 -> B4a -> B4b -> B5 -> B6a -> B6b -> B6c -> B7
```

## 執行規則

- 每次只執行使用者指定的批次；一批一個 PR，commit 與 PR 用日語。
- 每批開始、里程碑、停止時更新本表、HANDOFF 與 dev_log，然後停止，不自動開始下一批。
- 主對話檔位由使用者決定；表中檔位是建議值。ultracode 對抗式檢查（建議 B1、B4b、B6a）每次先徵求同意。
- 出現非預期回歸、範圍擴大，或 PvP authority 漂移超出 reconciliation 能吸收的程度時，停下回報並重新規劃。
