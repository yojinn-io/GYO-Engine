# 2026-11-21 GYO2：`engine/` 分層（主會話親自做）

批次代號：`v8-arch-gyo2-engine-layers`。分支 `claude/gyo2-engine-layers`（從 `gyo2` d3371355）。

## 起因與使用者的決定

- 使用者看了資料夾：「我看了程序的資料夾，特別是engine的分層很不合理。」主會話整理依賴後問指的是哪一種，使用者：「其他的還好，engine下嗎就完全沒有分層，ecs、編排、module還有plugin都會混在一起。」
- 主會話提分層與別家對照（Godot `core/`→`scene/`→`modules/`、Unreal `Runtime/Core`、flecs core＋addons、Bevy 平鋪），使用者：「不用開卡，你來處理。」
- 基礎層的歸屬：使用者：「可以內化的能力不用放Modules，這些東西很長時間不會改的。」主會話把 json、logging、audio 放進 core，使用者：「json 不是Module嗎？」——更正為 Module（2026-11-16「Module＝我們自己維護、介面是我們的」，JSON 是例子；logging 照 2026-11-21「Log 和 trace是2個Module」）。
- 使用者問 Go 的結構（「我接觸的go微服務都是分開的，都是那種小內核。所以層級沒有現在Engine的project這麼深。」），主會話對照後只分一層（`engine/<層>/<單位>/`），不再巢狀。使用者：「照修正後的分層開始搬」。
- 中途使用者問異常要不要分出來（「我想學go的分法，異常信息定義不要放在代碼裡」）：各模組已有自己的 `<Module>Error.hpp`，只有 `time` 的 `TimeError` 寫在 `Waiter.hpp`，本批拆成 `TimeError.hpp`。

## 做了什麼

| 目錄 | 層 | 單位 |
|---|---|---|
| `engine/core/` | 內化、長時間不改的能力，不認得 ECS | base、math、os、io、time、threads |
| `engine/ecs/` | ECS（Kernel）與它的儲存 | kernel、entity_store |
| `engine/orchestration/` | 編排層（一個單位） | orchestration |
| `engine/modules/` | Module：我們維護、介面是我們的、會往上長 | json、logging、audio、world_time、world_logging |
| `plugins/`（頂層，Engine 外） | Plugin：第三方官方維護 | 還沒有 |

- 登記檔加 `dir` 欄（`name,dir,target,…`，10 欄）：Module 的目錄由資料給、不從名字猜（AGENTS §9）；`dir` 是一或兩段、最後一段等於名字。`engine/CMakeLists.txt` 照 `dir` 加子目錄；`GYO_UNITS_<名>_DIR` 給測試用。
- include 路徑不變（`engine/<單位>/<標頭>`），程式碼的 include 一行都沒改。
- 跟著改：守衛的掃描根（kernel、naming、store_stamp、entity_store、logging、os 邊界、orchestration 的 O6 改掃 `engine/` 下所有單位的 CMakeLists）；守衛自己的測試；`UnitRegistryTests`（標頭、欄數、`dir` 的兩個新負例、「每個單位目錄都在某一列的 dir」）；測試的 CMake 裡寫死的 include 目錄（kernel、json、orchestration、os、audio、world_logging 改從登記檔讀 dir）；突變清單的 `file` 路徑；現行文件的路徑與 `docs/architecture/README.md` 的分層表、O-6 草稿。決定記錄、dev_log、HANDOFF、各批宣告不改寫。
- `time` 的錯誤定義拆到 `engine/core/time/include/engine/time/TimeError.hpp`。

## 驗證

- 本機 macOS（Intel，AppleClang）全新 configure（preset `test`）＋全量建置＋ctest：151/151 通過。
- 守衛在真實樹上全數 clean；守衛的單元測試全過（`tests/common/ci/test_*` 用 `python -m unittest` 直接跑在 gyo2 基準上也失敗——import 路徑，平常經 ctest 跑，與本批無關）。
- 突變清單：對全部清單靜態核對 `file` 存在與 `find` 命中，和 `origin/gyo2` 比，本批新弄壞的只有 4 個守衛突變（`lg-guard-logging-off`、`lg-guard-world-off`、`bg-r4-os-headers`、`bg-r4-render-excluded`，find 指著改過的掃描根），已更正並 4/4 killed。其餘對不上的在 gyo2 基準上本來就對不上（master 時代清單的元件：map_markers、collision、framework 等），不在本批。
- 守衛、登記檔讀取、`engine/CMakeLists.txt`、`Waiter.hpp` 上的突變全部重跑：結果見下一節。
- 「搬檔要查守衛 glob 會不會縮小覆蓋」：kernel、naming、store_stamp、entity_store 的掃描根跟著目錄搬，掃到的檔案相同；os 邊界、json、naming 的反向掃描以 `engine/` 整棵為根，不變；orchestration O6 從 `engine/*/CMakeLists.txt` 改成 `engine/` 下所有單位的 CMakeLists，範圍變大。

## 突變重跑

改過的守衛、登記檔讀取、`engine/CMakeLists.txt`、`Waiter.hpp` 上的突變 185 個全部重跑（本機 macOS）：

| 清單 | 結果 |
|---|---|
| `tests/common/os/guard_mutations.json` | 26/26 |
| `tests/common/json/mutations.json` | 15/15 |
| `tests/common/logging/mutations.json`（守衛的 7 個） | 7/7 |
| `docs/architecture/target/v2/scripts/orchestration_mutations.json` | 66/66（`ur-name-charset-unchecked` 改 expect 後） |
| `docs/architecture/plans/os-subsystem/scripts/mutations.json` | 7/9 |
| `docs/architecture/plans/kernel/scripts/mutations.json` | 59/62 |

- `ur-name-charset-unchecked`：加了 `dir` 欄之後，名字不合格的列也會被 dir 的檢查擋下、訊息不同，`bad_name` 案從「被接受」變成「沒報出名字自己的訊息」；expect 改成後者（只有這個突變會讓 `bad_name` 換訊息），killed。
- 沒過的 5 個在 `gyo2` 基準上本來就壞了，不是本批造成：`ks-k6-calls-off`、`ks-k6-every-file-exempt`（stale）、`ks-k8-skips-executor-files`（wrong_failure）找的 `executor_file` 已在異常分級 PR #254 拿掉（D1 拿掉 Executor 的豁免）；`os-g13-definition-name`、`os-g14-definition-platform-id`（stale）在基準上 find 就找不到。要退役或改寫，留給後續（不在本批）。

## 登記檔搬到頂層 `config/`（同一 PR）

- 使用者：「為何config 要放到engine層裡？我的產品登記耶放在裡面喔？請給我一個理由。」主會話查到沒有好理由：`projects.csv` 是 2026-09-21 master 大重整（3fc46857）時集中放進 `engine/config/` 的，GYO2 沿用；四份登記檔描述的是 repo 由什麼組成、由建置一起讀，不是 Engine 的能力，Engine 也不該認得產品（AGENTS §6、§7）。提頂層 `config/`（比照 Go 的 `go.work`、Cargo workspace 的根清單）或拆開放，使用者：「好，搬到頂層 config，併進 #257」。新的頂層目錄是 AGENTS §10 的 Architecture Delta，使用者同意。
- 做了：`git mv engine/config config`；讀它的 CMake（`GyoBuild`、`GyoApps`、`GyoTools`、`ExportAppRegistry`、`ExportToolRegistry`、`GyoUnitRegistry` 的說明）、各單位 CMakeLists 的註解、`kernel_guard` 的 `REGISTRIES`、`orchestration_guard` 的說明、release workflow、`ProjectCompositionTests`／`ToolRegistryTests` 的假樹、README 與現行文件；`docs/architecture/README.md` 加一段說明。
- 驗證：本機全新 configure＋全量建置＋ctest 151/151；kernel／orchestration 守衛測試、`test_workflow_gates` 過。這一步碰到的突變 134 個重跑：orchestration 67/67（`or-guard-real-o6-hand-link` 的 expect 裡寫死舊路徑，上一步的替換沒抓到跳脫的點，更正後 killed）、kernel 48/51、os-subsystem 1/7（1 個只在 Windows 跑）、r0b3 5/5、json 2/2、logging 1/1；沒過的都是上一節說的基準上就壞的那些，加上 os-subsystem 另外 4 個（`os-g8-link`、`ti-x1`、`ti-x2`、`th-x1`，find 在基準上就不中：連結已改由登記檔產生）。

