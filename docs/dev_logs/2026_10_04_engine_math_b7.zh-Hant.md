# Engine Math 基礎統一 B7：收尾

日期：2026-10-04。Owner：Engine（`GYO::Math`），連帶文件與計畫紀錄。
狀態：完成，PR [#27](https://github.com/yojinn-io/GYO-Engine/pull/27) 已合併（`fdc72e9`），CI 四平台通過（分支 `claude/math-foundation-b7`，疊在 B6c 分支上）。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#b7-收尾)，本文只記經過。

## 經過

1. B6b（#25）、B6c（#26）CI 四平台通過。
2. 未啟用產品的破損盤點在 B6c 驗證期間已完成：
   - 在 scratch 建置中啟用 object_fps 與 object_fps_v2。B6c 後 104 個 TU 中有 39 個失敗，基準上沒有失敗。
   - 機械式遷移的 scratch 副本可以完整編譯、link，cpu 測試通過。
   - 清單寫成 `inactive_products.md`。
3. 稽核（ultracode，2 個 agent）：
   - **grep 稽核**：找到早期批次遺漏、可逐位元相同地改用 Math 的寫法，分布在 Collision、Model、Render；也找到應登記為例外的保留項，包括 `TransformNormal` 的餘因子、ModelRenderer 的 `Finite(Color)`，以及 pvp authority 的瞄準方向與平面旋轉。
   - **文件一致性**：architecture.md 的 Collision 描述過時，也缺少 Math 的 Architecture Delta；PLAN 有多處被後續決策取代卻沒有加註；HANDOFF 有幾處事實錯誤。
4. 修正後驗證（ultracode，2 個 agent）：
   - **新舊比對**：engine 清理的可觀測行為全部相同。pvp `CharacterCollision` 的一處在 NaN normal 下 y 會不同，因此改回原寫法。
   - **最終審查**：指出紀錄缺口與收錄理由表的不準確，全部更正。
5. 本機驗收：core 19／19、test 46／46，未編譯 29 檔 syntax-only 全部通過；依賴圖與 B0 相比只多出 9 條指向 `gyo_math` 的邊。
6. HANDOFF 寫入 Architecture Report、Math 收錄理由表與範圍外事項彙整。

## 留給下一步

- B7 的 PR 與 CI 四平台驗收。
- 使用者依序合併 #24 → #25 → #26 → #27。
- 待使用者決定：float 純量的 clamp／min／max 寫法是否統一；範圍外各項是否另開工作。
