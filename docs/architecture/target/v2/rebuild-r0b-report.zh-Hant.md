# R0b-2 報告與 R0b-3 的判定變更清單（`v8-arch-rebuild`）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：R0b-2 本機完成，未開 PR。使用者 2026-11-14 指示先停下、把判斷機制的邊界與定義定好再動工；R0b-3（第 3 節的 11 項判定變更）暫不進行。**

**2026-11-14 更新：R0b-2 與 R0b-3 都已併入 `gyo2`（PR #225，6b37078b）。** 使用者指示「fix CI」後做了第 3 節的 11 項與兩處 workflow 修改；三平台 L1 通過。實際做法與表不同的地方記在第 3.1 節。

**2026-11-14 更新（收尾，`v8-arch-gyo2-r0b3-closeout`，PR #230，未合併）：** R0b-3 做了別模型的對抗檢查（檢查者 Fable，作者 Opus）、實跑了沒驗證過的路徑、刪了三筆死碼。結果在第 3.2 節。第 3 節表中第 8 項「緩める面：無」不成立，更正寫在 3.2.1 的 S1。

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

### 3.1 實際做法與表不同的地方（2026-11-14，實作 agent Opus／high；待使用者事後確認）

- **第 11 項做不到「toolchain 的 package 照常產生」**：沒有工具時 toolchain 這個產品不會登記，而 package 契約規定 package 至少要有一個執行檔。改成：沒有工具的 toolchain 那一列跳過打包的步驟（install、package 檢查、installed acceptance、smoke、archive），並用 notice 寫明原因（`build-and-validate.yml` 的 `package_plan`）。放寬的面：零工具時不產生 toolchain 的 package。有工具或是產品的列照舊。這條路徑只在 Quick acceptance 與 Trial packages 跑，PR #225 上沒有實跑。
- **第 11 項的 Python 端**：`build/ci/common/app_registry.py` 的 `export_tools` 也要接受「所有平台都沒有工具」，否則 CMake 的修改不生效。只有部分平台是空的照樣拒絕。
- **第 3 項多刪一段**：`ComponentTests.cmake` 的 explicit_device 整段（表只列其中兩行；同段的另一個斷言用的選項也已不存在）。108-113 行沒有刪，只拿掉 `GYO_BUILD_SDL_GPU_BACKEND`，保留 IMAGE、TTF、UFBX 三個明確選項的組合檢查。
- **第 9 項比表窄**：只有產品類的根（apps、tools、build/acceptance）沒有 C++ 時跳過並列出；engine、tests 是空的仍然報錯。
- **第 10 項**：工具登記那一端的保留名測試改用 `toolchain`（原本用 `framework`），讓工具端仍有保留名的檢查。
- **第 8 項**：draft 案例也改讀測試自己的產品登記。
- **沒做**：`tests/common/time` 的 3 筆死碼；`prepare-release.yml` 與 `app_copy_integration`（不在 PR 的路徑上，沒有走查）。
- **檢查**：第 4、5、8、9、11 項做了反向驗證（暫時放入合成的工具、產品、原始檔，檢查恢復並抓到違規），紀錄在主 checkout `build/evidence/gyo2-mechanism/r0b3-reverse.log`。沒有做別模型的對抗檢查。

### 3.2 對抗檢查與收尾（2026-11-14，`v8-arch-gyo2-r0b3-closeout`，PR #230；待使用者確認）

件数の突き合わせ：一致。反向驗證 48 案例，48 個與報告所述相同；突變 OS 守衛 26／26、CI 政策 14／14；既有 OS 清單裡與守衛有關、檔案仍在的 14 條 14／14。每條突變只在預期的那個案例失敗時算抓到。

檢查者：Fable（主會話自己做，沒有另開子 agent）。被檢查的作者：Opus。這一批沒有放寬任何判定；改動都是收緊或新增檢查。

#### 3.2.1 發現

blocker：沒有。R0b-3 併入 `gyo2` 的內容不需要撤回。

| 級 | 發現 | 證據（主 checkout `build/evidence/gyo2-r0b3-closeout/`） | 處理 |
|---|---|---|---|
| should S1 | **第 8 項有一個沒寫出來的放寬。** 發佈 CLI 的 3 個案例改讀 fixture 之後，PR 當下沒有任何測試匯出 repo 自己的 `tools.csv`。登記了工具、但發佈選擇是壞的（某個平台沒有 release 工具、預設 variant 不能打包）時，以前 `build.ci` 有 3 個案例失敗，R0b-3 之後整套通過，要到合併後的 Quick／Trial／發佈流程才失敗 | `probes.log` P1：同一個壞登記，CI 入口 `app_registry.py` 失敗；R0b-3 的整套測試 OK；換回 3eff2b0f 之前的測試檔則 3 個案例失敗（含子案例共 5 筆） | 已補：`test_repository_registries_export_for_ci`（PR 當下匯出 repo 的兩張登記表；「一個工具都沒選」只在登記是空的時候成立） |
| should S2 | **OS 邊界守衛對「必要的根不存在」不報錯。** 文件說 `REQUIRED_ROOTS`（engine、tests）沒有原始碼就是問題，但實際只查「存在而空」；根不存在時列為跳過並通過。`--root` 指到不是 repo 的目錄，守衛回報「沒有 OS 知識」、exit 0 | `probes.log` P3 | 已收緊：必要的根不存在也報錯（五點見 3.2.6） |
| should S3 | **OS 邊界守衛的自我測試抓不到 16 種把守衛改壞的改法**（26 條突變只抓到 10 條）。包括：命令列永遠回 0、不印跳過的根、少掃一種副檔名（`.h`、`.c`、`.mm`、`.inl`）、多排除一個目錄、Engine 名單可以列產品檔、從規則清單拿掉一個 OS 標頭／巨集／API／檔案操作、不查 `#elif` | `os-guard-mutations-before.log`（10／26）、`os-guard-mutations-after.log`（26／26） | 已補 10 個案例；突變清單放在 `tests/common/os/guard_mutations.json` |
| should S4 | **GPU 檢查步驟自己的失敗傳遞判定被連帶刪掉。** 刪 render 的 GPU 案例時（7c22edba），`gpu_smoke` 這一步的 `set -euo pipefail` 斷言也沒了；這一步現在跑的是產品宣告的 GPU 檢查，與 render 模組無關。實際影響小（`shell: bash` 時 GitHub 本來就帶 `-eo pipefail`，少的是 `-u`），但判定確實消失 | `probes.log` P2；`ci-policy-mutations-before.log` 的 `ci-gpu-no-pipefail: survived` | 已補回（`shell: bash`、腳本以 `set -euo pipefail` 開頭、沒有 `continue-on-error`） |
| should S5 | **零工具時 `Prepare Release` 選 tools 這條線，會跑完整個發佈建置才失敗。** 第一個 job 放行；三平台 release 驗證跑完後，draft 步驟以「缺 6 個檔」失敗。不會建立 tag 或 release（檢查在第一個遠端寫入之前） | `release-walkthrough.log` W2（prepare exit 0）、W3（draft exit 1） | 已改：prepare、draft、snapshot 都在讀 archive 之前以「Train tools has nothing to release: no tool is registered」停下；登記一個 release 工具就恢復 |
| should S6 | **snapshot 流程在零工具時每次都會紅**（使用者已決定用 KeepTool 處理，見 3.2.7；這個 PR 不含）。預設分支的 push 會為 tools 線開 `Snapshot / tools` job，零工具時沒有 package 可發，job 失敗。`gyo2` 不是預設分支，現在碰不到；`gyo2` 取代 master 而工具還沒回來時就會出現。不影響 CI gate（gate 不依賴 snapshot） | `release-walkthrough.log` W1（trains 仍列出 tools）、W4（snapshot exit 1） | 沒改（要改發佈線的契約，而且只有預設分支的 push 能驗證） |
| nit N1 | 第 7 項與 shader 工具 cache key 的雜湊，都是「回來時要有人記得改」，不會自動恢復 | `probes.log` P4、P5 | 已補成自動的：設 C++20 的 CMake 檔改用搜尋找出；一旦有 CMake 檔定義 `gyo_shader_host_tools`，就要求 cache key 帶 `hashFiles`（沒有這個 target 時是註明理由的 skip） |
| nit N2 | 既有的 OS 突變清單（`docs/architecture/plans/os-subsystem/scripts/mutations.json`）149 條裡有 17 條指向已刪的檔；共通執行器遇到不存在的檔會直接例外結束，而不是回報 stale | `os-guard-mutations-existing.log` | 沒改（不在這張卡的範圍；清單要在 R0b-4 或工具回來時重整） |
| nit N3 | `package_plan` 的依據是傳下來的工具清單是不是空的。傳遞鏈由既有案例釘住，但如果它斷了而登記有工具，Quick／Trial 會跳過打包、印出不正確的 notice、結論仍是通過；只有發佈流程會因缺 package 失敗 | 讀碼；3.2.3 | 沒改，只記錄 |
| nit N4 | 守衛掃的副檔名不含 `.ixx`、`.cppm`、`.m`、`.ipp`、`.tpp`；`EXCLUDED` 裡的 `third_party/`、`build/target/` 沒有任何根會走到。兩者都是 R0b-3 之前就有的 | 讀碼 | 沒改，只記錄 |

#### 3.2.2 第 3 節的表逐項核對

「核對」欄的代號是 `reverse_checks.py` 的案例；全部紀錄在 `reverse-checks.log`。做法：暫時放入合成的工具、產品、原始檔，或把一個檔改壞，跑那一項檢查，看結果是否與表相同；每個案例做完還原，最後 `git status` 是乾淨的。

| # | 表的敘述對不對 | 核對 |
|---|---|---|
| 1 | 對。沒有放寬：過期的名單項目仍被拒絕；被刪的檔帶著 OS 分支回來會被報出來 | 1a、1b |
| 2 | 對。render shader 工具路徑下的直接讀檔現在會被 R4 報出來 | 2a |
| 3 | 對。被刪的選項與 FRAMEWORK 元件在 build、engine、tests、preset、workflow 裡 0 筆引用（`absence-scans.log`，附正例）。留下的案例仍然有檢出力：改壞元件解析的 4 種改法都由留下的案例抓到 | 3a–3d |
| 4 | 對。跳過的條件是「`tools.csv` 一列都沒有」；有一列（即使 disabled）就恢復，工具自己的建置開關會被抓到 | 4a–4c |
| 5 | 對。跳過的條件是「apps、tools、services 下沒有 owner 目錄」；有目錄就恢復，workflow 或共通腳本寫了 owner 名會被抓到 | 5a–5d |
| 6 | 對。Engine 名義的 Go module 登記 0 筆（附正例）；留下的建置紀錄欄位判定仍有檢出力。零 module 時 Go 的 plan 正常結束並寫出空的服務紀錄（`release-walkthrough.log` W5） | 6a |
| 7 | 對，但不會自動恢復（N1，已補） | 7a |
| 8 | **「緩める面：無」不對**，見 S1。3 個案例改讀 fixture 這件事本身成立：把第 11 項還原後它們仍通過，repo 自己的匯出則失敗 | 8a、8b |
| 9 | 表與 3.1 都沒寫到的一點：必要的根不存在時也被跳過（S2，已收緊）。其餘對：apps、tools、build/acceptance 有 C++ 就恢復掃描，R1、R2、R4 都報得出來 | 9a–9g |
| 10 | 對。放寬的面就是「產品或工具可以取名 framework」；common、toolchain 兩端都仍被拒絕 | 10a–10d |
| 11 | 對。見 3.2.3 | 11a–11j |
| workflow：kernel spike | 對。`spikes/` 不存在；剩下的 4 筆是註解 | `absence-scans.log` |
| workflow：GPU 測試與 shader 雜湊 | 對。gyo2 上沒有標 `gpu` 的測試、沒有 `render.` 開頭的測試（`release-walkthrough.log` W6：0 個；標籤只有 build、cpu），舊步驟帶 `--no-tests=error` 一定失敗。連帶的兩個問題是 S4 與 N1 | — |
| 3.1：零工具時 package 做不出來 | 對。`build/acceptance/common/package_contract.py:86` 要求 package 的執行檔清單不是空的 | 讀碼 |
| 3.1：`package_plan` | 對。只有「toolchain 列而且沒有工具」跳過打包並印 notice；改壞的 4 種改法都由它的案例抓到。實跑見 3.2.5 | W-plan-a–d |

#### 3.2.3 空登記會不會把別的狀態誤當成「沒有工具」

不會。`GyoToolRegistry.cmake` 用的是登記表的列數（含 disabled 的列）；「沒有工具」只在一列都沒有時成立。

| 狀態 | 結果 | 案例 |
|---|---|---|
| 登記表只有表頭 | 匯出成功，每個平台 0 個工具，stderr 有說明 | 11-base、11g |
| 一個工具，enabled、不是 release | 失敗：`No release tools selected for windows` | 11a |
| 一個工具，release、disabled | 失敗（同上） | 11b |
| 所有工具都 disabled（兩列） | 失敗（同上）；CI 入口 `app_registry.py` 也失敗、不產生 matrix | 11c、11h |
| release 工具只支援 linux | 失敗：windows 沒有 | 11d |
| release 工具支援 windows、linux，不支援 macos | 失敗：macos 沒有 | 11e |
| CMake 那端被改成永遠不拒絕 | 既有的 CMake 測試與 Python 測試都失敗 | 11i、`ci-cmake-empty-never-refused` |
| Python 那端收到「只有部分平台是空的」 | 拒絕 | 11j |

Python 那端自己分不出「登記是空的」與「CMake 壞了、全部回空」，它信 CMake。新增的 `test_repository_registries_export_for_ci` 用 repo 的實際資料把兩者綁在一起：每個平台都沒選到工具，只在 `tools.csv` 一列都沒有時允許。

workflow 那端（`package_plan`）用 `GYO_SELECTED_TOOLS` 是否為空來決定。這個值從 matrix 的 `tools` 欄一路傳下來，每一段都有既有的案例釘住（`test_every_platform_packages_toolchain_without_games`、`test_empty_registry_still_builds_every_core_tool_baseline`）。如果這條傳遞鏈斷了而登記有工具，Quick／Trial 會跳過打包、印出不正確的 notice 而通過；發佈流程則會在 draft 步驟因缺 package 失敗（N3）。

#### 3.2.4 突變

| 清單 | 位置 | R0b-3 當時的測試 | 這一批之後 |
|---|---|---|---|
| OS 邊界守衛（把守衛本身改壞） | `tests/common/os/guard_mutations.json` | 10／26 | 26／26 |
| CI 政策（R0b-3 新增、修改的案例，加這一批新增的） | `tests/common/ci/r0b3_mutations.json` | 9／13 | 14／14（多 1 條是 S5 的） |
| 既有 OS 清單裡與守衛有關、檔案仍在的 | `docs/architecture/plans/os-subsystem/scripts/mutations.json` | — | 14／14（另 3 條指向已刪的檔，沒跑） |

- 執行方式：共通執行器 `tests/common/mutation/run_mutations.py --catalog …`，Python 加 `-B`，每次執行前清 `__pycache__`。
- `ci-broken-release-selection` 在知道 KeepTool 要登記之後改過一次：原本是「多登記一個不是 release 的工具」，只在零工具時才會讓匯出失敗；改成「登記一個沒有描述檔的 release 工具」，不論登記表裡還有什麼都會失敗。零工具與有一個 release 工具兩種狀態下都抓到（`ci-policy-mutation-registry-independent.log`）。
- CMake 那幾項（第 3、10、11 項）的突變寫在 `reverse_checks.py`（3a–3d、10a–10c、11i），因為共通執行器沒有 cmake 的占位符；它們存在證據目錄，不在 repo 的清單裡。
- 兩份新清單放在測試旁邊，不在 `docs/` 的計畫目錄（這張卡不能動其他文件）。要不要搬，由圖書管理員決定。

#### 3.2.5 PR 路徑以外的流程

| 流程 | 零產品、零工具時會怎樣 | 依據 |
|---|---|---|
| Quick acceptance（`cross-platform.yml` 的 `quick`，push master 或手動觸發時）與 Trial packages（`package-trial.yml`） | 兩者呼叫同一支 `build-and-validate.yml`、同樣的輸入（profile quick）。prepare：匯出 3 個 toolchain 列、工具是空的、stderr 有說明。每一列：configure、建置、測試、core 基線照跑；`package_plan` 印 notice 並輸出 `assemble=false`；install、package 檢查、installed acceptance、archive、上傳都跳過；GPU 步驟因沒有 package 輸出而跳過；Linux 列的 Go plan 寫出空的服務紀錄並上傳。`CI validation` 通過 | **實跑：**run [43596922844](https://github.com/yojinn-io/GYO-Engine/actions/runs/43596922844)（Package trial，`workflow_dispatch`，commit 4071a9a9）：結論 success。prepare 的 stderr 有「No tool is registered」。三個 toolchain 列（linux-x64、windows-x64、macos-arm64）：configure、建置、測試（93、96、93 個全過）、core 基線都跑了；`package_plan` 各印一則 notice「No tool is registered for ＜平台＞…」；install、package 檢查、installed acceptance、GPU、archive、上傳 6 步都是 skipped；Linux 列寫出並上傳空的服務紀錄（`gyo-service-toolchain`）；沒有任何 `gyo-package-*` artifact。`CI validation` 通過。步驟清單與 notice 存在證據的 `trial-run-43596922844.json`、`trial-run-43596922844-notices.log`。Quick acceptance 沒有另外觸發（同一支 workflow、同樣的輸入） |
| `cross-platform.yml` 的 snapshot | 只在預設分支的 push 跑。`snapshot_plan` 仍列出 tools 線；`Snapshot / tools` 下載不到 package，`release_pipeline.py snapshot` 失敗（這一批之後訊息是「no tool is registered」）。不建立 tag、不發佈 | 走查＋本機跑 CLI（W1、W4）。**沒有觸發**（會發佈） |
| `prepare-release.yml`，train＝tools | 這一批之前：prepare 通過 → 三平台 release 驗證（含 Windows 的 `app_copy_integration`）→ draft 步驟因缺 package 失敗，不建立 tag。這一批之後：第一個 job 就以明確理由失敗，後面的 job 不跑 | 走查＋本機跑 CLI（W2、W3）＋新案例 `test_cli_refuses_the_tools_train_while_no_tool_is_registered`。**沒有觸發**（會建立 tag 與 draft release） |
| `prepare-release.yml`，train＝某個遊戲 | 沒有登記任何遊戲，`Unknown release train` 在第一個 job 失敗 | 讀碼（`resolve_train`） |
| `app_copy_integration`（只在 Windows 的 toolchain 列、profile release 跑） | 把 engine、third_party、build/cmake 複製出去，放入兩份 fixture 遊戲，configure、建置、install、執行，兩個遊戲各印出自己的身分。零產品的 gyo2 上通過：新增與複製產品只靠產品內容與登記 | 本機（Intel Mac）實跑：`app-copy.log`、`app-copy-logs/`。**Windows 上沒有跑**（release profile 沒有觸發） |

**合併後的補記**（使用者 2026-11-14「併進下一條線的文件更新」；卡留下的文字，主會話補）：合併後（`gyo2` 15ec34f1，PR #230 的 merge commit）手動觸發 Cross-platform CI：run [43600646685](https://github.com/yojinn-io/GYO-Engine/actions/runs/43600646685) success。L1 三平台通過；Quick acceptance 三個 toolchain 列通過，install、package 檢查、installed acceptance、archive、上傳都是 skipped，`CI validation` 通過（Quick acceptance 這條路徑第一次實跑，之前只跑過 Package trial）；CI gate 通過；`Report dispatched L1` 成功；snapshot 兩個 job skipped，沒有建立 tag 或發佈。合併前 `gyo2` 已前進到 79fad9cb（#227、#228、#232），PR 上的 L1 是對合併前的 base 跑的，這一次才是合併後的樹。步驟清單：主 checkout `build/evidence/gyo2-r0b3-closeout/gyo2-dispatch-run-43600646685.json`；合併前的本機試合檢查：同目錄 `pre-merge-check.log`。

#### 3.2.6 OS 邊界守衛的判定變更（AGENTS.md §14.4 的五點；Design Invariants 的守衛，合併這個 PR 即表示核准）

1. 守的性質：OS 的東西只在 `engine/os`；而且守衛真的掃到了該掃的根。
2. 放寬的面（以前失敗、之後通過）：沒有。
3. 收緊的面（以前通過、之後失敗）：`engine` 或 `tests` 這兩個根不存在。以前被當成「跳過」而通過，現在報 `engine: no C/C++ source scanned (does not exist)`。
4. 放寬的部分誰還守著：沒有放寬，不適用。
5. 根據與證據：`probes.log` P3（空目錄上守衛 exit 0）。舊判定會失敗的輸入新判定都失敗：原有的 37 個自我測試案例一個都沒改判定而全過（只有一個案例多加一條斷言），既有清單的 14 條守衛突變 14／14。新判定的檢出力：`guard_mutations.json` 26／26。

自我測試新增的 10 個案例都只是多查，不改變守衛對任何樹的判定。其中 `test_exclusions_and_required_roots_are_pinned` 把排除目錄、必要的根、Engine 名單的範圍釘成固定值，`test_rule_lists_do_not_shrink` 把四份規則清單的項目數釘成下限（OS 標頭 45、巨集 20、API 32、檔案操作 40）：以後要加排除或拿掉一條規則，就得改這個守衛測試，也就得照 §14.4 給使用者看。

#### 3.2.7 等使用者決定的事

- **要再放寬的判定：沒有。**
- **S6，snapshot 的 tools 線在零工具時怎麼辦。** 當時列的選項：
  - (a) `release_channels.py trains` 在沒有工具時不列 tools 線並印出理由，workflow 對空的 matrix 加條件；工具回來自動恢復。要改「tools 線永遠存在」這個發佈契約，而且只有預設分支的 push 能真的驗證。
  - (b) 不改：`gyo2` 成為預設分支而工具還沒回來的期間，每次 push 的 `Snapshot / tools` 都是紅的（gate 不受影響），訊息現在會寫明原因。
  - (c) 重看前提：`gyo2` 取代 master 之前先讓工具回來（R3），這個狀態就不會出現。
- **S6 的決定（使用者 2026-11-14，在這張卡的會話裡；原話）**：「S6，你做一個KeepTool佔位」，接著補充「但是不是純佔位，需要保障基礎的ImGUI沒問題，輸入法也能用。」
  - 意思：不採 (a)、(b)；登記一個叫 KeepTool 的工具，讓 tools 線一直有東西可打包。它不是空殼：它要能證明基礎的 ImGui 可以跑、輸入法可以用。這等於 (c) 的具體形式——第一個回來的工具就是 KeepTool。
  - **這張卡沒有做 KeepTool，PR #230 不含它。** 原因：`gyo2` 上沒有 SDL、沒有 ImGui、沒有 `plugins/`；platform、input、text、ui、render 都在「不帶、由 R1 重新推導」的名單（[帶過去清單](rebuild-r0-carry-list.zh-Hant.md)第 2 節）。帶 ImGui 與輸入法的 KeepTool 需要下表的東西，等於 R3 的第一個產品加上 R1 要定的幾個基礎位置，超出這張卡的範圍（卡上寫明不改 `engine/`；[重建宣告](rebuild-declaration.zh-Hant.md)第 3 節目前的範圍只到 R2）。照 AGENTS §14 開頭的規定停下回報，由架構師會話排成自己的一批。
  - 不擋 PR #230 合併：`gyo2` 不是預設分支，S6 現在不會發生。

  | KeepTool 需要的東西 | `gyo2` 上的現況 | 參考（master） |
  |---|---|---|
  | 工具的登記、建置、打包、驗收的機制 | 都在，沒有對象而已（`GyoTools.cmake`、`GyoPackaging.cmake`、`build/acceptance/common/`；3.2.5 的實跑證明沒有工具時它們被跳過） | `tests/common/cmake/ProjectCompositionTests.cmake` 裡的合成工具是最小的形狀 |
  | SDL（Module，在 Engine 裡）：視窗、事件、文字輸入事件 | 沒有。`third_party/` 只剩 doctest；adapter 的位置由 R1 定 | `third_party/sdl3`，含 macOS 輸入法的 patch `gyo-cocoa-ime-composing-keydown.patch` |
  | 一個繪圖後端（ImGui 要畫得出來） | 沒有。render 不帶 | SDL_Renderer 的後端最小 |
  | ImGui（Plugin，在 Engine 外） | 沒有；`plugins/` 目錄還不存在。`GyoComponents.cmake` 仍認得 `IMGUI`、`SDL_*` 這些元件名，但沒有對應的 target | `third_party/imgui`；舊 framework 的 ImGui 綁定（`docs/framework/architecture.md` 第 86、194～196 行：輸入法的組字畫在 ImGui 的文字游標處） |
  | 文字輸入的契約（組字、確定、候選視窗的位置、UTF-8） | 沒有。input、text 不帶 | 同上 |
  | 驗收 | — | 見下面第 2 點 |

  （2026-11-16 註：Module／Plugin 的判準改成「誰維護、介面是誰的」，SDL 改為 Plugin、網路改為 Module，見[同詞異義表](rebuild-vocabulary.zh-Hant.md)第 0 節末。本表照當時的定義寫，不改。）

  檢查者的看法（Fable 自己的判斷，不是使用者的決定）：
  1. KeepTool 適合當 R3 的第一個產品，也適合當 R1 的名指 Feature：它小，卻一次用到 Module（SDL）、Plugin（ImGui）、Tool 三層與打包；SDL adapter 放哪、ImGui plugin 怎麼接、文字輸入的契約要保證什麼，都可以從它推出來（AGENTS §2 的 Feature → Contract → Architecture）。
  2. 「輸入法能用」CI 驗不到：runner 沒有輸入法。CI 能驗的是「文字輸入事件的路徑」（合成的組字與確定事件進得了 ImGui 的輸入框、候選視窗位置有設）；真的輸入法要在 Windows、macOS、Linux 實機上按一次。現在的發佈規則是「tools 線不需要實機證據」（`release_channels.py` 的 `train_release_evidence`），要不要為 KeepTool 改，得一起定。
  3. KeepTool 讓 S6 不發生，但沒有修 S6：哪天 KeepTool 被刪而沒有別的工具，S6 會回來。(a) 仍然可以另外做。
  4. `gyo2` 成為預設分支後，每次 push 會發一個只含 KeepTool 的 `tools-snapshot-…` prerelease（保留最新 5 個）。
  5. 登記名要小寫（`[a-z][a-z0-9_]*`），例如 `keep_tool`。
  6. AGENTS §7 禁止「為了引擎的驗證而要求留著某個工具」。KeepTool 不違反它的條件是：刪掉 KeepTool（登記列加它自己的目錄）之後，引擎與 CI 仍然成立——這正是 PR #230 驗過的零工具狀態。KeepTool 的文件要寫明移除條件（AGENTS §3 的 Replacement Criteria）。
- **第 3 節表中第 8 項的更正**（S1）與 **3.1 節各項的事後確認**：3.1 的做法經這次核對都成立；「零工具時跳過打包」已實跑（3.2.5）。

#### 3.2.8 產品、工具、render 回來時的狀況

會自動恢復（對象一出現檢查就跑）：第 1、2、4、5、9、11 項，`package_plan`，這一批補的四個案例（repo 登記表匯出、C++20 的 CMake 檔、shader 工具 cache key、tools 線的發佈）。

要隨模組重建（不會自動回來）：第 3 項的 render 選項政策、第 6 項「Engine 名義的 Go module 無條件登記」、L1 與 toolchain 列的 render GPU 測試步驟、`hashFiles` 的實際內容（這一批的案例只要求「要有」）。

要記進 HANDOFF 的事（這張卡不能改 HANDOFF）：
- R0b-3 已做過別模型的對抗檢查（PR #230）；blocker 0、should 6（5 個已修；S6 由 KeepTool 處理，另開一批）、nit 4。
- OS 邊界守衛收緊一條（必要的根不存在＝問題）；守衛測試新增釘住排除目錄與規則清單的案例。
- 兩份新的突變清單的位置；既有 OS 清單有 17 條過期（N2）。
- S6 的決定（使用者 2026-11-14）：做 KeepTool——不是純佔位，要保障基礎的 ImGui 與輸入法；這張卡沒做，要由架構師排成一批（3.2.7）。原話、比過的選項、理由、出處都在 3.2.7。
- 這一批的 dev_log（`docs/dev_logs/2026_11_14_gyo2_r0b3_closeout.zh-Hant.md`）要列進 `target/v2/README` 的一覽（這張卡不能改 README）。

#### 3.2.9 範囲の限界

- 本機只有 Intel Mac；突變、反向驗證、`app_copy_integration`、發佈 CLI 都只在這台跑。三平台看 PR #230 的 L1：run [43596915160](https://github.com/yojinn-io/GYO-Engine/actions/runs/43596915160)（commit 4071a9a9）：linux-x64、windows-x64、macos-arm64 的 L1 都通過；Select CI scope 與 CI gate 通過。之後只改 `docs/` 的 commit 不再跑 L1（docs-increment）。
- **沒有觸發**：`Prepare Release`、snapshot（會建立 tag 或發佈）。這兩條只有走查與本機 CLI；CLI 以空的 `GH_TOKEN`、`GH_REPO` 執行，碰不到遠端。
- build-and-validate 的 release profile 沒有在 CI 上跑過零工具的狀態：Windows 上的 `app_copy_integration`、app 列的測試、release 才跑的 Go 檢查。
- download-artifact 在沒有符合的 artifact 時的行為沒有實測；draft 步驟不論下載結果如何都會因缺檔而失敗（本機以空目錄確認）。
- 對抗檢查由主會話（Fable）自己做，沒有第二個檢查者；新增的兩份突變清單是檢查者自己寫的，沒有再給別的模型看。
- 沒有檢查 R0b-2 的刪除本身（判定 5 的重掃仍待 R0b-3 完成後另做）。
- 證據：這台 Intel Mac 的主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-r0b3-closeout/`：`reverse_checks.py`、`reverse-checks.log`、`probes.log`、`absence-scans.log`、`os-guard-mutations-{before,after,existing}.log`、`ci-policy-mutations-{before,after}.log`、`release-walkthrough.log`、`app-copy.log`、`app-copy-logs/`、`baseline-*.log`、`final-ctest.log`、`final-ci-policy.log`、`ctest-list-{before,after}.txt`、`trial-run-43596922844.json`、`trial-run-43596922844-notices.log`、`ci-policy-mutation-registry-independent.log`。
- **合併後補記**：三平台 L1 與 Quick acceptance 已在合併後的樹（`gyo2` 15ec34f1）實跑通過，run 43600646685（見 3.2.5 的補記）。上面寫「沒有實跑」的 Quick acceptance 路徑因此已實跑一次。

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
