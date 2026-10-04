# Engine Assert／Result 統一 R4：Model、ui／render 的轉換與 API 誤用

日期：2026-10-04。Owner：Engine（model、render、ui、`GYO::Base`），連帶 pvp 的被迫修改。
狀態：本機驗收完成，PR [#34](https://github.com/yojinn-io/GYO-Engine/pull/34) 待 CI（分支 `claude/result-unification-r4`，基準 `12abe2e`）。
計畫與證據見 [Assert／Result 統一](../architecture/plans/result-unification/README.md) 與 [HANDOFF](../architecture/plans/result-unification/HANDOFF.md#r4-modeluirender-的轉換與-api-誤用)，本文只記經過。

## 經過

1. R3（#33）L1 四列通過、windows-x64 只多 1 個新 TU 的 doctest 警告後合併，從 master 建立 R4 分支。
2. 新增 `ModelError` 與 `ModelRendererError`，Model 與 ModelRenderer 不再以 `std::string` 當 E；訊息逐字保留。
3. 新增 `Base::CauseDetail` 與 `GYO_UNREACHABLE`；`Result` 改以 `CodedError` 約束 E。
4. 依規則 5 改寫跨模組轉換：UiRenderer、ModelRenderer、ShaderLibrary（帶型別的內部例外）、UfbxModelLoader（不再經由例外）。
5. UiRuntime、UiRenderer、Renderer、ModelRenderer 的 API 誤用改成 Assert；viewport 等外部資料的檢查保留 Result。
6. 依編譯錯誤處理被迫的修改：model 測試、pvp 已編譯檔 13 處與 `Require` helper、29 個未編譯檔 17 處。
7. 新增測試，涵蓋 code 的保留、API 誤用與 `CauseDetail`。
8. 本機驗收：core 23／23、test 50／50；`string_error_type` 為 0；沒有新警告；29 檔 syntax-only 全部通過。

## 留給下一步

- R4 的 PR 與 L1 四列結果。
- 使用者指示後開始 R5（ui_editor）。
