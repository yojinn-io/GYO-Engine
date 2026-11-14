# R0b-2 報告與 R0b-3 的判定變更清單（`v8-arch-rebuild`）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：R0b-2 本機完成，未開 PR。使用者 2026-11-14 指示先停下、把判斷機制的邊界與定義定好再動工；R0b-3（第 3 節的 11 項判定變更）暫不進行。**

件数の突き合わせ：一致。分支 `claude/gyo2-r0b-delete`（HEAD 0857762d，未開 PR）：刪除前 1992 檔（基底 a56fa94a）→ 刪除後 983 檔；刪 1010、新增 1（`engine/io/CMakeLists.txt`）、改名 1（突變執行器）、修改 16，1992−1010+1＝983。刪除的目錄 59 個。頂層目錄從 13 個變成 7 個；`engine/` 留 7 個模組加 config。正例：刪除前的掃描命中 `engine/CMakeLists.txt:24 GYO::Engine`。

實作：Opus／high 的實作 agent（6 個 commit；中途因行程結束被中斷一次，之後恢復並交了報告，原文在證據 `implementer-report.md`）。主會話重跑了三個失敗的測試核對。

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
- 依賴掃描（刪除前，已保存）：帶過去的 7 個模組之間 11 條含入邊，都在集合內。清單寫的「14 條」含 collision 的 2 條與 kernel 的 1 條，那兩個不帶。
- 突變執行器搬家後，用舊清單實跑兩條帶過去的突變，2／2 killed。

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
| 3 | `tests/common/cmake/ComponentTests.cmake`:76-77、:93-94、:108-113、:126-127、:135 | 測 render 的 shader 格式、GPU driver、`GYO_BUILD_SDL_GPU_BACKEND`、FRAMEWORK 元件；這些選項與元件已刪 | 刪掉這幾組斷言；其餘元件契約的案例保留 | GPU 驅動與 shader 格式的選項政策不再被檢查 | 那個政策屬 render，gyo2 上不存在；render 回來時隨它重建 |
| 4 | `tests/common/ci/test_workflow_gates.py`:361 `assertTrue(tools)` | 工具登記是空的 | 沒有工具時這個案例以「沒有可驗證的對象」結束，並在測試名與訊息寫明；有工具時照舊 | 工具數為零時，「每個平台都打包 toolchain」不再被檢查 | 有工具登記時仍守。工具回來時（R3，要使用者同意）自動恢復 |
| 5 | 同檔 :1121-1128 `assertTrue(owners)` | 沒有任何產品 owner | 同第 4 項的做法 | 產品數為零時，「共通 CI 定義不寫產品名」的字表是空的，等於不查 | 有產品時仍守 |
| 6 | 同檔 :1099-1108 | 斷言「以 Engine 名義登記的 Go module 存在」；gateway 已不帶 | 刪掉這個案例 | 不再要求 Engine 名義的 Go module 存在 | 這條守的是舊安排（gateway 掛在 Engine 名下）；網路的 owner 由 R1 定。Go 機制自己的測試仍在 |
| 7 | 同檔 :684-690 | 讀已刪的 `engine/render/shaders/pipeline/CMakeLists.txt` | 從清單拿掉這個路徑；`GyoBuild.cmake` 那一項保留 | 那個已刪檔案的「C++ 不做 module 掃描」不再被檢查 | 檔案不存在；主建置的檢查仍在 |
| 8 | `test_release_pipeline.py` 的 3 個案例 | 發佈流程的測試以「至少有一個可發佈的工具」為前提；`GyoToolRegistry.cmake:237` 回報「No release tools selected」 | 這 3 個案例改用測試自己的合成工具登記（fixture），不讀 repo 的登記表 | 無：斷言不變，只換輸入的來源 | 發佈流程的三個性質照舊檢查；共通測試本來就不該依賴特定產品（AGENTS §7） |

另有兩處不是本機測試、屬同一批的 CI 修改（判定 4 已核准它們屬 R0b-3）：
- `cross-platform.yml` 的 kernel spike 步驟：刪除（spike 已不存在）。
- 兩支 workflow 的 GPU 測試步驟（`-R '^render\.'` 加 `--no-tests=error`）與 shader 工具快取的 hashFiles：刪除。緩める面：L1 不再跑 Lavapipe 的 render 測試——gyo2 上沒有 render；render 回來時隨它重建。

| 9 | `os_boundary_guard.py`:26 的 ROOTS（含 apps、tools、build/acceptance） | 這些根下已沒有 C++，守衛回報「no C/C++ source scanned」 | 根不存在或沒有 C++ 時跳過，並在輸出列出跳過的根 | 這些根回來之前不掃 | 根有檔時照舊掃；產品回來時自動恢復 |
| 10 | 保留名 `framework`：`GyoAppRegistry.cmake:157`，由 `AppRegistryTests.cmake:133`、`ToolRegistryTests.cmake:100-102` 守 | 目前不失敗；framework 已不存在 | 程式與測試一起拿掉這個保留名 | 產品可以取名 `framework` | common、toolchain 兩個保留名仍守 |
| 11 | 「每個平台至少一個 release 工具」：`GyoToolRegistry.cmake:237`，由 `ToolRegistryTests.cmake:103` 守 | 工具登記是空的：`app_registry.py` 的 export_tools 失敗，L1 的打包矩陣與 package-trial 會失敗 | 工具登記為空時，toolchain 的 package 不含工具、照常產生；有登記但某平台沒有 release 工具時仍報錯 | 「零工具」不再是錯誤 | 「有工具卻漏了某個平台」仍報錯 |

第 4、5、11 項是真的放寬：沒有產品與工具的期間，這幾條檢查是空的。替代做法是留一個假的產品登記讓它們有對象，但 AGENTS §7 禁止為了檢查留 Dummy。

## 4. 隨模組刪除的守衛（它們守的性質，R1、R2 重建時參考）

| 守衛 | 守的性質 |
|---|---|
| kernel_guard、kernel_symbols、compile-fail 樣本、zone_c_export_check | Kernel 中性、禁雜湊疊代與時鐘、可寫全域符號、簽名契約 |
| naming_guard（N1、N3） | namespace 對目錄、角色後綴。它放在 `tests/common/kernel/`、只掃 `engine/kernel`、又 import kernel_guard，所以隨 kernel 刪除（清單 §5.2：隨 kernel 在 R2 回來）。在那之前 gyo2 上沒有命名守衛 |
| 被刪目錄裡的 `gyo_expect_links`、`RenderOptionsTests` | 被刪 target 的連結期望；render 選項政策 |
| entity_store_guard | 欄容器的排序與中性 |
| zone_scan、h7_scan | (c) 區來源檔；host 不手排順序 |
| ui_dependency_guard、platform_sdl 的守衛、sdl_symbol_guard、framework_dependency_guard、四支工具守衛 | 各模組的依賴方向；SDL 只出現在它的 adapter |
| neutral_names_guard | 資料契約文件不出現玩法詞 |

## 5. 判定 5 的掃描（修訂後的判定）

- 範圍：`engine/`、`build/`、`tests/`、`.github/`、頂層檔案。刪除前有大量命中（正例），刪除後 58 筆。
- 刪除後的 58 筆：
  - R0b-2 要修的建置引用：0 筆。
  - 由既有條件擋住的死碼 3 筆：`tests/common/time/CMakeLists.txt:11、:14` 的 `if(TARGET GYO::PlatformSDL)` 與 `WakeRecord.cpp:13` 的 `#ifdef`。這段 window 變體現在不會建置，R0b-3 一起刪。
  - 屬 R0b-3：12 筆（os 守衛、allowlist、`test_workflow_gates.py`、workflow）。
  - 註解或字串：43 筆，其中 README 三語 18 筆屬 R0b-4，`MathCharacterizationTests.cpp` 的出處註解 18 筆。
- 逐筆分類要在 R0b-3 做完後重掃一次，才算判定 5 的結果；目前不判。
- `docs/` 另列清單：616 個檔、6,551 筆提到被刪的路徑，多在 dev_logs、產品文件、舊計畫——這些 R0b-4 會刪；另有 52 個文件引用突變執行器的舊路徑。

## 6. 範囲の限界

- 只在 Intel Mac 本機驗證；三平台 L1 未跑。
- R0b-3 清單以本機失敗與實作 agent 的報告為準，L1 上可能再出現新的；出現時照同樣的格式補報。
- 未查證：MSVC 與 gcc 的建置；`gyo-build.json` 仍寫 `shader_bundles`、`shader_host_tools` 欄位（值固定 false，CI 的判定讀它）；突變清單的 find 字串只實跑了兩條。
- 證據：主 checkout `build/evidence/rebuild-r0b/`（configure、build、ctest 的 log，刪除前後的依賴掃描 `deps_pre.json`、`deps_post.json`，判定 5 的掃描 `j5_pre.txt`、`j5_post.txt`、`j5_post_docs.txt`，掃描腳本）。
