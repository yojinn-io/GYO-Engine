# Engine Assert／Result 統一 R3：Error 語意與 asset

日期：2026-10-04。Owner：Engine（`GYO::Base`、asset、io），連帶 render、text、ui、SDL 後端、ui_editor、pvp 的被迫修改。
狀態：完成，PR [#33](https://github.com/yojinn-io/GYO-Engine/pull/33) 已合併（`12abe2e`），L1 四列通過（分支 `claude/result-unification-r3`，基準 `dedeebd`）。
計畫與證據見 [Assert／Result 統一](../architecture/plans/result-unification/README.md) 與 [HANDOFF](../architecture/plans/result-unification/HANDOFF.md#r3-error-語意與-asset)，本文只記經過。

## 經過

1. R2（#32）L1 四列通過、windows-x64 警告與 R1 相同後合併，從 master 建立 R3 分支。
2. 重寫 `Error.hpp`：`Make` 為唯一入口，不可預設建構；移除「沒有錯誤」語意的成員；新增 `CodedError` concept 與 `Describe`。
3. 8 個 code enum 移除 `None`、從 1 開始，補齊 `ToString` 與 `static_assert(CodedError)`；`UiError` 新增 `detail` 與建構子。
4. `AssetError`、`IoError`、`IoResult` 收斂成各 1 個宣告，刪除 43 個重複宣告。
5. `AssetRecord` 的錯誤改成 `std::optional`，`GetError` 以值回傳。
6. `LoaderRegistry::Register` 與 AssetManager 保留欄位的 API 誤用改成 Assert；`Register` 改回傳 `void`，呼叫端隨之簡化。
7. `AssetCatalog` 保留 resolver 的 code，並新增測試；修正多處錯誤的 namespace 結尾註解。
8. 本機驗收：core 23／23、test 50／50；R0 的 characterization 沒有修改就通過；稽核目標全部達成；沒有新警告；依賴圖不變；29 檔 syntax-only 全部通過。

## 留給下一步

- 使用者指示後開始 R4（Model、ui／render 的轉換與 API 誤用）。
