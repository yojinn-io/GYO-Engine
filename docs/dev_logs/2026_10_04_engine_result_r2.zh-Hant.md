# Engine Assert／Result 統一 R2：Result 的寫法

日期：2026-10-04。Owner：Engine（`GYO::Base`），連帶 engine 全模組、tests 與文件。
狀態：本機驗收完成，PR 待開（分支 `claude/result-unification-r2`，從 R1 的 head 建立）。
計畫與證據見 [Assert／Result 統一](../architecture/plans/result-unification/README.md) 與 [HANDOFF](../architecture/plans/result-unification/HANDOFF.md#r2-result-的寫法)，本文只記經過。

## 經過

1. #31 的 CI 重跑中且 repo 不允許 auto-merge，所以從 R1 的 head 建立 R2 分支先行開始。
2. 重寫 `Result.hpp`：新增 `Base::Err` 與 CTAD；成功值隱式轉換；拒絕裸 `E`；非 void 沒有預設建構；讀取未持有的一方走 Assert；class-level `[[nodiscard]]`。
3. 撰寫並執行 codemod（`scripts/r2_result_codemod.py`）：66 個檔案，Ok 233 處、Err 507 處自動改寫，`IoResultVoid` 改成 `IoResult<void>`，刪除 52 個不再使用的區域別名。dry-run 時確認 Ok 與 Err 的總數和起點一致。
4. 手動處理 codemod 列出的 8 處大括號參數；預計要手動處理的 lambda 不需修改。
5. class-level `[[nodiscard]]` 讓 tests 中 5 處丟棄 `Register` 的地方產生警告，改成 `REQUIRE`。
6. 重寫 `ResultTests.cpp`，以 `static_assert` 鎖住建構規則。
7. 本機驗收：core 23／23、test 50／50；稽核的舊寫法全部為 0；Err 數量守恆；沒有新警告；依賴圖不變；29 檔 syntax-only 全部通過。

## 留給下一步

- R2 的 PR 與 L1 四列結果。
- 使用者指示後開始 R3（Error 語意與 asset）。
