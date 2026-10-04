# Engine Assert／Result 統一 R0：任務校正

日期：2026-10-04。Owner：Engine（`engine/base`），連帶測試與計畫紀錄。
狀態：完成，PR [#30](https://github.com/yojinn-io/GYO-Engine/pull/30) 已合併（`3b9765e`），L1 四列通過（分支 `claude/result-unification-r0`，基準 `eeebc1e`）。
計畫與證據見 [Assert／Result 統一](../architecture/plans/result-unification/README.md) 與 [HANDOFF](../architecture/plans/result-unification/HANDOFF.md#r0-任務校正)，本文只記經過。

## 經過

1. 確認 math-foundation 計劃全部合併（#24–#27，後續 #28、#29），開工條件滿足；從 `eeebc1e` 建立分支。
2. 以 ultracode 重新盤點（3 個盤點 agent 加 1 次對抗檢查），prompt 帶 commit hash，不沿用規劃階段的快取。
3. 在 `eeebc1e` 的原狀取得基線：core 19／19、test 46／46、依賴邊 144 條、29 檔 syntax-only 全部通過。建置期間把新測試暫存，確保基線未受影響。
4. 盤點與對抗檢查發現的計劃校正：
   - PR 的 L1 本來就含 MSVC 與 clang；只有 core preset 限定 Linux。
   - NotFound 改查下一個 mount 的邏輯在 `Vfs.hpp`，characterization 改為 `VfsTests`。
   - #28 在 `Math::Clamp` 用了 `<cassert>`。
   - 盤點漏列「以 Result 回傳的 API 誤用」。
5. 使用者決定：
   - Assert 適用於整個 engine，包括 Math；`GYO::Base` 比 Math 更低。原先「Math 不加 Assert」是規劃 agent 未經查證的推論。
   - `Math::Clamp` 改用永遠生效的 `GYO_ASSERT`。
   - API 誤用改成 Assert，只保留 SdlGpu 的例外。
6. 新增 characterization：Vfs 的 Stat、讀取用 Open、Exists 的 NotFound overlay 規則；AssetManager 非同步與同步的 `KeepOldIfAny` 失敗。
7. 建立計劃目錄（README、PLAN、HANDOFF、`baseline/`、`scripts/result_audit.py`），分類表 173 列、資料驗證型 API 16 個。
8. 本機驗收：core 19／19、test 46／46；`engine_tests` 69 個 case 全部通過；nodiscard 警告 0。

## 留給下一步

- PR #30 的 L1 四列驗收。
- 使用者指示後開始 R1（概念文件與 Assert）。
