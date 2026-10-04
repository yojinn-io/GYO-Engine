# Engine Assert／Result 統一 R1：概念文件與 Assert

日期：2026-10-04。Owner：Engine（`GYO::Base`），連帶 Math、Collision、Runtime、測試與文件。
狀態：完成，PR [#31](https://github.com/yojinn-io/GYO-Engine/pull/31) 已合併（`9c51b32`），L1 四列通過（分支 `claude/result-unification-r1`，基準 `3b9765e`）。
計畫與證據見 [Assert／Result 統一](../architecture/plans/result-unification/README.md) 與 [HANDOFF](../architecture/plans/result-unification/HANDOFF.md#r1-概念文件與-assert)，本文只記經過。

## 經過

1. R0（#30）L1 四列通過後合併，從 master 建立 R1 分支。
2. `engine/base` 升格為 INTERFACE target `GYO::Base`，排在 Math 之前；Engine、Math、Collision 連它。
3. 新增 `Assert.hpp`：`GYO_ASSERT` 在所有建置都會求值，handler 可替換，重入以 RAII guard 判斷，`AssertionFailure` 不繼承 `std::exception`。
4. Collision 15 處、FixedTickRuntime 2 處的 `throw std::invalid_argument` 改成 `GYO_ASSERT`；`Math::Clamp` 的 `<cassert>` 改成 `GYO_ASSERT`；Result 讀取未持有的一方改成 Assert。依 Lakos rule，會把呼叫端上下界傳給 `Clamp` 的 Math 函式一併拿掉 `noexcept`。
5. 新增測試支援 `gyo_test_support` 與 `GYO_CHECK_ASSERTS`，原本 17 處 `CHECK_THROWS_AS` 改用它；新增 `gyo_base_tests` 與三種模式的 abort probe。
6. 新增 `docs/architecture/error-handling.md`，更新 architecture、math、creating_apps 文件。
7. 本機驗收：core 23／23、test 50／50；依賴圖只多出計畫列出的邊；29 檔 syntax-only 全部通過；沒有新增警告。
8. 使用者要求在合併前讓 MSVC 全專案改用 `/Zc:preprocessor`：在 `GyoBuild.cmake` 全域加入（shader host 個別加入），`Assert.hpp` 加防呆 `#error`，新增 `PreprocessorTests.cpp` 證明選項生效。
9. CI：L1 四列通過。windows-x64 的警告與改動前逐項相同（138 個，全為既有項目）；MSVC 上的 `PreprocessorTests`、abort probe 都通過。

## 留給下一步

- 合併前的 PR run 在 Windows 上都是冷 compiler cache（旗標改變），建置約 24–28 分鐘；合併後由 master 重建 cache。
- 使用者指示後開始 R2（Result 的寫法）。
