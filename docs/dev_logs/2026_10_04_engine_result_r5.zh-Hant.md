# Engine Assert／Result 統一 R5：ui_editor

日期：2026-10-04。Owner：Tool（`tools/ui_editor`），連帶 `tests/ui_editor` 與 `tests/common/runtime_sdl`。
狀態：完成，PR [#35](https://github.com/yojinn-io/GYO-Engine/pull/35) 已合併（`f72bfbd`），L1 四列通過（分支 `claude/result-unification-r5`，基準 `88e9641`）。
計畫與證據見 [Assert／Result 統一](../architecture/plans/result-unification/README.md) 與 [HANDOFF](../architecture/plans/result-unification/HANDOFF.md#r5-ui_editor)，本文只記經過。

## 經過

1. R4（#34）L1 四列通過、windows-x64 警告不變後合併，從 master 建立 R5 分支。
2. 新增 `EditorError.hpp`，集中定義 ui_editor 的錯誤型別。
3. 手寫的結果型別（CommandLine、FileService、DocumentSession）改用 Result；`SessionResult` 改成 `Result<SessionReport, SessionError>`，保留 diagnostics。
4. out-param 改成回傳值（ReadOnlyAssetCatalog、AssetPreviewContext、EditorApp）；API 誤用與內部不變式改成 `GYO_ASSERT`。
5. 會丟例外的 `std::filesystem::absolute` 改成共用的 `AbsolutePath`。
6. 4 份錯誤描述 helper 改用 `Base::Describe`；`PreviewAdapter::Result` 改名為 `FrameOutput` 並保留 `UiError`。
7. 測試改用新 API，新增結束碼 2 的檢查；修正 R4 遺留的 clang 警告。
8. 本機驗收：core 23／23、test 50／50；結束碼 0、2、3、4 不變；ui_editor 的 out-param 與 `assert` 歸零；沒有新警告；29 檔 syntax-only 全部通過。

## 留給下一步

- R6（收尾）接著在同日進行，見 [R6 dev log](2026_10_04_engine_result_r6.zh-Hant.md)。
