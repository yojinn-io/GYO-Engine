# GYO2 重建：R0b-3 的對抗檢查與收尾（`v8-arch-gyo2-r0b3-closeout`）

日期：2026-11-14。Owner：架構（架構師會話開的任務卡）。分支 `claude/gyo2-r0b3-closeout-3d8070`，PR #230 開到 `gyo2`，未合併（合併由使用者決定）。逐項結果：[R0b 報告](../architecture/target/v2/rebuild-r0b-report.zh-Hant.md)第 3.2 節。被檢查的那一批：[R0b-3 的 dev_log](2026_11_14_gyo2_r0b3_ci.zh-Hant.md)。

件数の突き合わせ：一致。反向驗證 48／48 與報告所述相同；突變 OS 守衛 26／26、CI 政策 14／14；本機 ctest 93／93（測試清單前後相同）。

## 做了什麼

R0b-3（刪掉產品與工具之後，為了讓 CI 通過而調整的守衛與 CI 政策）是 Opus 做的，沒有給別的模型檢查過。這一批由 Fable 設法否定它，把找到的洞補上，實跑沒驗證過的路徑，刪掉三筆死碼。沒有放寬任何判定。

- **對抗檢查**：第 3 節的 11 項與兩處 workflow 修改逐項反向驗證（暫時放入合成的工具、產品、原始檔，或把一個檔改壞，看檢查是否恢復、是否抓到）。11 項的敘述有 9 項成立；第 8 項寫「沒有放寬」不成立，第 9 項少寫一個情況。
- **收緊**：
  - OS 邊界守衛：`engine`、`tests` 這兩個根不存在也算問題（以前被跳過而通過）。
  - 新案例：PR 當下匯出 repo 自己的登記表（補第 8 項漏掉的放寬）。
  - 補回：GPU 檢查步驟的 `set -euo pipefail` 斷言（刪 render 案例時連帶消失）。
  - 改成自動：設 C++20 的 CMake 檔用搜尋找出；shader 工具的 target 一出現就要求 cache key 帶原始碼雜湊。
  - 發佈 CLI：零工具時 tools 線在第一步以明確理由停下（以前跑完整個發佈建置才因缺 package 失敗）。
- **OS 守衛的自我測試**：新增 10 個案例；寫了 26 條把守衛改壞的突變，R0b-3 當時的自我測試只抓到 10 條。
- **死碼**：`tests/common/time` 的 window 變體（`GYO::PlatformSDL` 已不在 gyo2）。它不是 CTest，沒有守任何性質。

## 經過

- 檢查者是主會話（Fable），沒有另開子 agent。先讀報告與八個 commit 的 diff，再寫反向驗證腳本（48 個案例），然後做五個「設法否定」的探測，五個都成立（R0b 報告 3.2.1 的 S1、S2、S4、N1 兩項）。
- 走查 PR 路徑以外的流程時發現：零工具時 `Prepare Release` 會放行 tools 線，跑完才失敗；snapshot 在預設分支的 push 上會每次失敗。前者已改，後者要改發佈線的契約，留給使用者決定（R0b 報告 3.2.7）。
- 「零工具時跳過打包」這條路徑第一次實跑：手動觸發 Package trial（與 Quick acceptance 是同一支 `build-and-validate.yml`、同樣的輸入）。`Prepare Release` 與 snapshot 會建立 tag 或發佈，沒有觸發，只走查並在本機以空的 token 跑 CLI。
- 中途修正：反向驗證腳本第一版有 1 個案例名稱寫錯（測試名不存在），而且在 repo 根目錄跑 cmake 測試留下兩個工作目錄；改正後重跑 48／48，`git status` 乾淨。探測腳本用了 bash 才有的變數，`probes.log` 的 `exit=` 是空的，判讀看 unittest 的結果行；同樣五個狀態在突變清單裡有 exit status。
- 既有的 OS 突變清單有 17 條指向已刪的檔，共通執行器遇到會例外結束。沒有改（不在範圍），記在 R0b 報告 3.2.1 的 N2。

## 驗證

- 本機 Intel Mac，preset `core`：ctest 93／93（基線與最終各一次）；`ctest -N` 的清單前後相同。CI 政策測試 253 個通過，3 個是註明理由的 skip（沒有工具、沒有 owner 目錄、沒有 shader 工具的 target）。
- 突變（共通執行器，Python 加 `-B`、每次清 `__pycache__`；只在預期的案例失敗時算抓到）：
  - `tests/common/os/guard_mutations.json`：R0b-3 當時 10／26，之後 26／26。
  - `tests/common/ci/r0b3_mutations.json`：R0b-3 當時 9／13，之後 14／14。
  - 既有 OS 清單中與守衛有關、檔案仍在的 14 條：14／14。
- `app_copy_integration` 在本機實跑通過（零產品的樹上複製兩份 fixture 遊戲、建置、安裝、執行）。
- CI（PR #230）：run [43596915160](https://github.com/yojinn-io/GYO-Engine/actions/runs/43596915160)（commit 4071a9a9）：linux-x64、windows-x64、macos-arm64 的 L1 都通過；Select CI scope 與 CI gate 通過。之後只改 `docs/` 的 commit 不再跑 L1（docs-increment）。
- Package trial（手動觸發）：run [43596922844](https://github.com/yojinn-io/GYO-Engine/actions/runs/43596922844)（Package trial，`workflow_dispatch`，commit 4071a9a9）：結論 success。prepare 的 stderr 有「No tool is registered」。三個 toolchain 列（linux-x64、windows-x64、macos-arm64）：configure、建置、測試（93、96、93 個全過）、core 基線都跑了；`package_plan` 各印一則 notice「No tool is registered for ＜平台＞…」；install、package 檢查、installed acceptance、GPU、archive、上傳 6 步都是 skipped；Linux 列寫出並上傳空的服務紀錄（`gyo-service-toolchain`）；沒有任何 `gyo-package-*` artifact。`CI validation` 通過。步驟清單與 notice 存在證據的 `trial-run-43596922844.json`、`trial-run-43596922844-notices.log`。Quick acceptance 沒有另外觸發（同一支 workflow、同樣的輸入）。

## 沒有驗證到的

- `Prepare Release` 與 snapshot 沒有在 CI 上跑（會建立 tag 或發佈）。
- build-and-validate 的 release profile（Windows 上的 `app_copy_integration` 等）沒有在零工具的狀態下跑過。
- 新增的兩份突變清單由檢查者自己寫，沒有再給別的模型看。

## 證據

這台 Intel Mac 的主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-r0b3-closeout/`：`reverse_checks.py`、`reverse-checks.log`、`probes.log`、`absence-scans.log`、`os-guard-mutations-{before,after,existing}.log`、`ci-policy-mutations-{before,after}.log`、`release-walkthrough.log`、`app-copy.log`、`app-copy-logs/`、`baseline-*.log`、`final-ctest.log`、`final-ci-policy.log`、`ctest-list-{before,after}.txt`、`trial-run-43596922844.json`、`trial-run-43596922844-notices.log`。
