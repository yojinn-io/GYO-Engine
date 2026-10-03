# Engine Math 基礎統一 B0：規劃與基線

日期：2026-10-03。Owner：Engine（新增模組 `GYO::Math`）。
狀態：B0 完成（分支 `claude/math-foundation-b0`，基準 `0bd5363`），不改程式；B1 未開始。
計畫與進度見 [Math 基礎統一](../architecture/plans/math-foundation/README.md)，本文只記經過。

## 經過

1. 外部工具掃描回報「Float3／Vec3／Matrix4 在各處重複定義 9 次」。逐一核對後確認數字成立：
   - engine 內有 `Render::Float3`、`ShaderAbi::Matrix4`、`Model::Vec3`、`Model::Matrix4`、`Collision::Float3`、`CapsuleQueries` 內部的 double `Vec3`。
   - 三個 app 各有一份 `fps::Float3`。
   - `Length`、`Normalize`、sRGB 等運算也各有多份，寫法不一。
2. 使用者要求把 Math 固定為 Engine 的基礎，範圍是整個 GYO，但不含未啟用產品。
   以 ultracode 規劃：3 個唯讀盤點、3 個方案（incremental、contract-first、risk-first）、1 個評審、1 次對抗式檢查，共 8 個 agent。
   - 評審選定 contract-first，並嫁接其他方案的優點。
   - 對抗式檢查提出 2 個 major（未啟用產品的探測方法、決定性量測須含 arm64）和 10 個 minor，都已修進計畫。
3. 使用者對四個方向選了最徹底的選項：舊名稱完全移除、數值實作全面統一、完整基礎庫、29 個未編譯檔一起遷移。
   計畫核准。
4. 核准後陸續定案：
   - Math 組成依使用者的樹狀圖（Linear Algebra／Geometry／Scalar）。
   - 向量命名 `Vec2/Vec3/Vec4`，純量用後綴。
   - Math 與 Collision 的分界：`Aabb`、`Capsule`、`Ray`、`Plane`、`Sphere`、最近點查詢歸 Math；`VerticalCapsule` 留在 Collision。
   - `Ray` 的 direction 不要求正規化；`Plane` 的 normal 要求正規化，方程為 `dot(n, p) = d`。
   - `Triangle` 不定義正面，只定義頂點順序與 normal 的計算方式。
5. B0 基線：
   - core 18／18、test 41／41 通過。
   - 依賴圖的 GYO 相關邊共 133 條。
   - pvp 29 個未編譯 `.cpp` 的 syntax-only 全部通過。syntax-only 檢查器先用故意寫錯的檔案做反向對照，確認會報錯。

## 使用者決定

完整清單見 [HANDOFF 決策紀錄](../architecture/plans/math-foundation/HANDOFF.md#決策紀錄)。
本日的主要方針：MVP 階段把基礎債一次修乾淨，不留相容層。代價是未啟用產品重新啟用時必須先遷移。

## 留給 B1

- 確認最近點查詢函式的命名，以及各 primitive 的基本查詢清單。
- 是否使用 ultracode 對抗式檢查，先徵求使用者同意。
