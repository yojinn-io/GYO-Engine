# Engine Math 基礎統一 B6c：pvp 未編譯的 29 個檔案改用 GYO::Math

日期：2026-10-04。Owner：`object_fps_pvp`（沒有被任何 target 編譯的 29 個來源檔）。
狀態：本機驗收完成，PR 待開（分支 `claude/math-foundation-b6c`，疊在 B6b 分支上）。
計畫與證據見 [Math 基礎統一](../architecture/plans/math-foundation/README.md) 與 [HANDOFF](../architecture/plans/math-foundation/HANDOFF.md#b6c-pvp-未編譯的-29-個檔案)，本文只記經過。

## 經過

1. B6a（#24）CI 四平台通過；B6b（#25）已開 PR。盤點在 B6b 驗證期間完成。
2. 盤點（2 個 agent 依目錄分擔，1 個 agent 檢查遺漏）：大多是逐位元相同的替換。會漂移的有相機基底、EnemyRig、水平長度、地板命中、敵人武器四元數；`GroundPoint` 的 helper 留在產品。
3. 實作後 29 檔 syntax-only 全部通過。這些檔案不會執行，所以以 2 個 agent 驗證：
   - **新舊寫法比對**：把每處改動抽到 scratch，以執行期輸入比對。56 個項目中 51 個逐位元相同，其餘 5 個是預期中的漂移，都在說明範圍內。
   - **對抗式審查**：沒有推翻正確性。指出保留的 `IsFinite(GroundPoint)` 未登記為 B7 稽核例外，以及 HANDOFF 的兩處描述問題，都已更正。
4. B7 的未啟用產品破損盤點，已在 B6c 驗證期間先跑完（結果留給 B7 記錄）。

## 留給下一步

- PR 與 CI 四平台驗收。
- B7：文件定稿、grep 稽核、未啟用產品破損清單、依賴圖比對、Architecture Report。
