# Engine Math 基礎統一 B1：GYO::Math 模組

日期：2026-10-03。Owner：Engine（`GYO::Math`）。
狀態：本機驗收完成，PR [#18](https://github.com/yojinn-io/GYO-Engine/pull/18) 待 CI 四平台（分支 `claude/math-foundation-b1`）。既有消費者未修改。
後續：PR #18 已合併為 `6cc2829`。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#b1-gyomath)，本文只記經過。

## 經過

1. 啟動時確認查詢清單，使用者決定三件事：
   - 最近點查詢用統一重載 `ClosestPoint`。
   - 幾何交集測試收進 Math。
   - 提供逆矩陣、行列式等通用運算。

   為了讓重載能以型別區分 double 路徑，新增 `Segmentd`、`Aabbd`。
2. 依 PLAN 1.1–1.6 寫成 19 個 header。函式本體以 Model、Collision、Renderer、ColorTransform、UiRuntime 的現有實作為準；Renderer 的 row-major 矩陣依對照表換算成 column-major。
3. 以 ultracode workflow 平行撰寫兩份測試，再做一輪對抗式審查：
   - 規格測試從契約推導期望值，不照抄實作。
   - characterization 把 engine 舊 helper 逐字凍結，和 Math 逐位元比對。
4. characterization：除了 PrimitiveMesh 的 hypot 和倒數相乘正規化（已量測漂移），engine 內所有被取代的 helper 都和 Math 逐位元相同，包括 Renderer 的整條 WVP 鏈。
   審查另外發現：WVP 必須維持舊的結合方式，改成另一種結合就會改變位元。
5. 審查提出 2 個 blocker、2 個 major、9 個 minor，全部處理：
   - `Triangle` 最近點遇到退化三角形會出現 NaN，在 FMA 下近退化三角形更會出現巨大誤差。
     只加分母防護不夠，改用數值條件判定，內部分支改為沿法線投影，並以 double 參考實作驗證。
   - 「不寫 constexpr 就能避免常數摺疊差異」的前提錯誤：clang 最佳化時，在 x86_64 上也會用 fused 捨入做常數摺疊。
     已更正文件，B4b、B6a 的位元比對改成兩邊都用執行期資料。
   - 射線對球改用 Lagrange 恆等式求判別式，避免遠距離時嚴重相消。
6. 本機驗收：core 19／19、test 42／42。依賴圖只多出測試 target 的兩條邊。`gyo_math_tests` 在四種收縮模式下都通過。

## 留給下一步

- PR 與 CI 四平台驗收，特別是 macos-arm64 的 FMA。
- 浮點收縮模式是否全專案統一，交由使用者決定（見 HANDOFF 未結事項）。
- B2 開始時決定 Collision 的 raycast 是否改用 `Math::Ray` 與 Math 的交集函式。
