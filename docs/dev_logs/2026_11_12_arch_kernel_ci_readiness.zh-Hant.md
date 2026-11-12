# 架構 v8：v8-arch-kernel-ci-readiness（kernel 進 L1 的接法、spike 測試升格、Windows VEH 在 CI）

日期：2026-11-12。Owner：架構（kernel 研究線；架構師會話開的任務卡，子會話執行）。報告：[ci-readiness.zh-Hant.md](../architecture/plans/kernel/ci-readiness.zh-Hant.md)。計畫：[kernel README](../architecture/plans/kernel/README.md) 文件清單。
- 執行：子會話（Fable 5.1／high；讀 CI log low），分支 `claude/arch-kernel-ci-readiness`，從 origin/master `2b4618f9` 開。
- 性質：調查與方案。只有 `docs/`；不改 `build/`、`.github/`、頂層 CMake、spike 程式。假子系統只在本機走過，**未 commit**。

## 起因

kernel 本體還沒寫。架構師要在實作批之前知道三件事：新 Engine 子系統要改哪些檔才會被 L1 三平台建置並測試；spike 的測試（簽名推導、存取核對、頁保護）將來怎麼變成 `tests/common/kernel/` 的正式測試；Windows 的 VEH 在 CI runner 上是不是真的跑了。

## 做了什麼

- 讀 `GyoBuild.cmake`／`GyoComponents.cmake`／presets／workflow／`tests/common/*/CMakeLists.txt`，寫出 4 新增＋2 修改的清單；以假子系統 `engine/kernel`＋`tests/common/kernel` 在工作樹實走 preset `test`：configure 162 s、`ctest -N -L cpu` 列出 #26、建置 7 s、執行通過；反向把假子系統多連 `GYO::Math`，`gyo_expect_links` 的 DEFER 檢查讓 configure 失敗（證明接線的守衛有效）。
- 把 spike 的 `PageGuard.cpp` 原樣放進 `engine/kernel/src/` 探測 `os.boundary_guard`：27 筆（R1 4、R2 11、R3 12）→ 平台層必須在 `engine/os`。
- 逐檔、逐案例列 spike 測試的去處（54 個 `TEST_CASE`、41 條突變）；找出兩個 spike 語意與 spec 相反的案例（事件在同步點清掉 vs. F-7 下一 tick 可讀；延後命令依記錄順序 vs. F-2 依鍵），照搬會固定錯的性質。
- 讀 windows-x64 artifact（L1 run 43169204148）的 xml 與 `LastTest.log`，逐行號記；查 doctest 2.4.12 的 Windows SEH 實作，列 VEH 在 MSVC Debug 的十條風險。

## 判定（事前宣告，開始前給架構師）

件数の突き合わせ：一致——假子系統 1 測試：列出 1／建置 1／執行 1 過；反向 1 次失敗 1 次；windows-x64 xml tests 2／failures 0／skipped 0，debug 執行檔 52 過 0 skip、release 51 過 1 skip（Cost 依設計）。

1. 清單用假子系統走過一遍，`ctest -N -L cpu` 看得到、跑得過，清單無漏：**成立**（改的檔正好 6 個）。
2. 每個 spike 測試檔與突變條目的去處與理由：**成立**。
3. 問題 3 以 artifact 行號為證；無 debugger／ASan 寫「推斷」：**成立**。
4. 共通層零修改；零風險小修逐項列出：**成立**（沒有找到需要修的）。
5. 時間盒 3 h／單題 2 h／總 4.5 h：未觸發，約 2 h。

## 經緯

- 第一次用 `GYO_ASSERT(cond, "msg")` 兩個引數寫假標頭；巨集是變參、以 `#__VA_ARGS__` 當訊息，改成一個引數（不影響清單）。
- zsh 把 `====X` 當 `=command` 展開，幾條命令重跑；不影響結果。
- 沒有碰到 AGENTS §14 停止條件。
- 證據：主 checkout `build/evidence/kernel-ci-readiness/`（`configure-test.log`、`ctest-N-cpu.txt`、`build-gyo_kernel_tests.log`、`ctest-run-kernel.txt`、`configure-negative-expect-links.log`、`os-boundary-guard-*.txt`、`fake-subsystem/`（假子系統與 `wiring.patch`）、`toolchain.txt`）。本機 Intel Mac（AppleClang 21，cmake 4.4.3）。

## 架構報告

- Architecture Delta：無（本批 docs-only）。
- 交架構師的結構性項目（報告 §4）：頁保護平台層進 `engine/os`（OS 層 Delta；kernel 要變 STATIC 才能 PRIVATE 連 `GYO::Os`）；編譯失敗測試的形狀（repo 無 `try_compile` 先例）；第三層測試獨立執行檔與 sanitizer 下 `SKIP_RETURN_CODE 77`；spike 步驟移除的兩條件與 `.github/` 範圍；驗證開關必須是執行期選項（L1 主圖 RelWithDebInfo，spike 的 NDEBUG 雙目標不能帶進來）。
- 發現：spike 丟掉了 `AddVectoredExceptionHandler` 的 handle（Windows 無法卸除、無法重排到最前）；spike 的 MSVC 證據只有 Debug 組態。
