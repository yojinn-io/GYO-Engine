# R0b-2 報告與 R0b-3 的判定變更清單（`v8-arch-rebuild`）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：R0b-2 本機完成；R0b-3 的 8 項判定變更待使用者核准（第 3 節）。**

件数の突き合わせ：一致。分支 `claude/gyo2-r0b-delete`（HEAD 0857762d，未推 PR）追蹤的檔 983 個（gyo2 是 1988 個，另加本線的文件）；頂層目錄從 13 個變成 7 個（`.github`、`agents`、`build`、`docs`、`engine`、`tests`、`third_party`）；`engine/` 留 7 個模組加 config。

實作：Opus／high 的實作 agent 做了 6 個 commit 後，在寫報告前被中斷（行程結束）；工作樹乾淨。本報告由主會話依它的 commit 與證據寫成，並重跑了三個失敗的測試。

## 1. 做了什麼

| commit | 內容 |
|---|---|
| a8387b7a | 刪除產品、framework、assets、spikes；登記表只留表頭 |
| cbc91f16 | 刪除不帶的 engine 模組與第三方（只留 doctest） |
| fae9f79a | 刪除被刪模組的測試與守衛 |
| 6d1c21f9 | 建置修補；io 另立 `GYO::Io` |
| d19f5337 | 共通突變執行器搬到 `tests/common/mutation/` |
| 0857762d | os 的 CMake 註解拿掉對 `GYO::Engine` 的提及 |

- 帶過去模組的原始碼沒有改；`-ffp-contract=off` 保留。
- `GyoAppProject.cmake` 的元件對照表現在是空的：產品宣告任何元件都會以「unavailable」停下。

## 2. 本機驗證（Intel Mac，preset `core` 與 `ci-macos-x64`）

- configure 與 build 都通過（83 個建置步驟）。
- ctest：29 個中 26 個通過。3 個失敗都屬 R0b-3，見第 3 節。
- 三平台的 L1 還沒跑：要等 R0b-3 做完、開 PR 之後。

### io 另立 target（Architecture Delta，AGENTS §3 的 7 項）

1. 目的：R0 清單核准 io 帶過去，asset 與 runtime 不帶。
2. 現在的問題：io、asset、runtime 同在 `GYO::Engine` 一個 target，不拆就不能只帶 io。
3. 變化的邊界：Build Graph——新 target `gyo_io`／`GYO::Io`；`GYO::Engine` 這個 target 不再存在。
4. 影響：`engine/io`、`tests/common/core` 的 io 測試。
5. Dependency Direction：`GYO::Io` → PUBLIC `GYO::Base`，PRIVATE `GYO::Math`、`GYO::Os`，與原始碼的含入相同；沒有新的方向。
6. Ownership：不變，io 仍歸 Engine。
7. 更小的做法：保留 `GYO::Engine` 這個名字只裝 io——名字會指錯概念（naming DI 4），不採。

## 3. R0b-3：要使用者核准的判定變更（AGENTS §14.4）

每項寫：守的性質、緩める面（以前失敗、之後通過）、締める面、其他判定是否仍守。

| # | 位置 | 現在為什麼失敗 | 建議的改法 | 緩める面 | 締める面／仍守住的 |
|---|---|---|---|---|---|
| 1 | `tests/common/os/os_boundary_allowlist.txt`:14-17 | 名單列了已刪的檔；守衛要求「不存在的項目要移除」 | 移除這些項目（守衛自己的要求） | 無 | 名單變短。守的性質（OS 的東西只在 `engine/os`）不變 |
| 2 | `tests/common/os/os_boundary_guard.py`:34 | 排除路徑含已刪的 `engine/render/shaders/pipeline/` | 拿掉這個排除路徑 | 無 | 排除變少，掃描範圍只會變大 |
| 3 | `tests/common/cmake/ComponentTests.cmake`:76 | 測「GPU 政策在快取相撞後仍成立」，期待 `dxil;spirv`；GPU 選項已隨 render 刪除 | 刪掉 GPU 政策的案例；其餘元件契約的案例保留 | GPU 驅動與 shader 格式的選項政策不再被檢查 | 那個政策屬 render，gyo2 上不存在；render 回來時隨它重建 |
| 4 | `tests/common/ci/test_workflow_gates.py`:361 `assertTrue(tools)` | 工具登記是空的 | 沒有工具時這個案例以「沒有可驗證的對象」結束，並在測試名與訊息寫明；有工具時照舊 | 工具數為零時，「每個平台都打包 toolchain」不再被檢查 | 有工具登記時仍守。工具回來時（R3，要使用者同意）自動恢復 |
| 5 | 同檔 :1121-1128 `assertTrue(owners)` | 沒有任何產品 owner | 同第 4 項的做法 | 產品數為零時，「共通 CI 定義不寫產品名」的字表是空的，等於不查 | 有產品時仍守 |
| 6 | 同檔 :1099-1108 | 斷言「以 Engine 名義登記的 Go module 存在」；gateway 已不帶 | 刪掉這個案例 | 不再要求 Engine 名義的 Go module 存在 | 這條守的是舊安排（gateway 掛在 Engine 名下）；網路的 owner 由 R1 定。Go 機制自己的測試仍在 |
| 7 | 同檔 :684-690 | 讀已刪的 `engine/render/shaders/pipeline/CMakeLists.txt` | 從清單拿掉這個路徑；`GyoBuild.cmake` 那一項保留 | 那個已刪檔案的「C++ 不做 module 掃描」不再被檢查 | 檔案不存在；主建置的檢查仍在 |
| 8 | `test_release_pipeline.py` 的 3 個案例 | 發佈流程的測試以「至少有一個可發佈的工具」為前提；`GyoToolRegistry.cmake:237` 回報「No release tools selected」 | 這 3 個案例改用測試自己的合成工具登記（fixture），不讀 repo 的登記表 | 無：斷言不變，只換輸入的來源 | 發佈流程的三個性質照舊檢查；共通測試本來就不該依賴特定產品（AGENTS §7） |

另有兩處不是本機測試、屬同一批的 CI 修改（判定 4 已核准它們屬 R0b-3）：
- `cross-platform.yml` 的 kernel spike 步驟：刪除（spike 已不存在）。
- 兩支 workflow 的 GPU 測試步驟（`-R '^render\.'` 加 `--no-tests=error`）與 shader 工具快取的 hashFiles：刪除。緩める面：L1 不再跑 Lavapipe 的 render 測試——gyo2 上沒有 render；render 回來時隨它重建。

第 4、5 項是真的放寬：沒有產品與工具的期間，這兩條檢查是空的。替代做法是留一個假的產品登記讓它們有對象，但 AGENTS §7 禁止為了檢查留 Dummy。

## 4. 隨模組刪除的守衛（它們守的性質，R1、R2 重建時參考）

| 守衛 | 守的性質 |
|---|---|
| kernel_guard、naming_guard、kernel_symbols、compile-fail 樣本 | Kernel 中性、禁雜湊疊代與時鐘、可寫全域符號、簽名契約 |
| entity_store_guard | 欄容器的排序與中性 |
| zone_scan、h7_scan | (c) 區來源檔；host 不手排順序 |
| ui_dependency_guard、platform_sdl 的守衛、sdl_symbol_guard、framework_dependency_guard、四支工具守衛 | 各模組的依賴方向；SDL 只出現在它的 adapter |
| neutral_names_guard | 資料契約文件不出現玩法詞 |

## 5. 判定 5 的掃描（修訂後的判定）

- 範圍：`engine/`、`build/`、`tests/`、`.github/`、頂層檔案。刪除前命中 2,910 行（正例），刪除後 75 行。
- 刪除後的 75 行分三類：
  - 屬 R0b-3：workflow 的 hashFiles、`test_workflow_gates.py:684`、`os_boundary_guard.py:34`。
  - 屬 R0b-4（文件）：README 三語提到 shader 編譯器的位置。
  - 註解或測試用的假路徑：`MathCharacterizationTests.cpp` 的出處註解（約 20 行）、os 守衛自測裡的假檔名。
- 逐筆分類要在 R0b-3 做完後重掃一次，才算判定 5 的結果；目前不判。
- `docs/` 另列清單：1,163 行提到被刪的路徑，R0b-4 處理。

## 6. 範囲の限界

- 只在 Intel Mac 本機驗證；三平台 L1 未跑。
- 實作 agent 沒有寫完報告就中斷；「R0b-3 清單」是主會話依失敗輸出整理的，可能不完整——R0b-3 做完後以 L1 為準。
- 未查證：工具數為零時 toolchain 的打包（L1 的 Package 步驟）會不會失敗。
- 證據：主 checkout `build/evidence/rebuild-r0b/`（configure、build、ctest 的 log，刪除前後的依賴掃描 `deps_pre.json`、`deps_post.json`，判定 5 的掃描 `j5_pre.txt`、`j5_post.txt`、`j5_post_docs.txt`，掃描腳本）。
