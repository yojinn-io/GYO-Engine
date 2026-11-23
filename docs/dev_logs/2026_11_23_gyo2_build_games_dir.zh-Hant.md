# 2026-11-23 建置層認 `games/`，建置與 CI 的 app 改叫 game（`v8-arch-gyo2-build-games-dir`）

Owner：建置層的產品登記（`build/cmake`、`build/ci/common`、`config/`）與掃描它的守衛。GYO2 主會話開的任務卡（使用者「開 1、2、3」的第 2 張）。分支 `claude/gyo2-build-games-dir-a85fe4`（開向 `gyo2`）。作者 Opus 5.5。

事前宣告：[`build-games-dir-declaration.zh-Hant.md`](../architecture/target/v2/build-games-dir-declaration.zh-Hant.md)（修訂 2，e182968d）。動工前核准：使用者「照建議，選 B，守衛核准」（經主會話，HANDOFF `v8-arch-gyo2-build-games-dir-approved`）。

## 為什麼

使用者 2026-11-23 定頂層 `games/<遊戲>/`、`assets/<產品>/`、`tools/<工具>/`（`v8-arch-gyo2-top-dirs-games-assets-tools`），`games/object_connect/` 已用 `.gitkeep` 建好。建置層還寫死 master 的 `apps/`，四個守衛與 workflow gates 也不掃 `games/`：Object_Connect 的程式一放進來，就不會被建置、也躲過守衛。用詞照使用者 2026-11-14「用 Game 就好」（同詞異義表 :25-27），趁兩份登記檔都沒有產品時一起改（方案 B）。

## 做了什麼

- **目錄**：建置層讀 `games/<遊戲>/`（`GyoBuild.cmake`、`GyoGames.cmake`、`GyoGameRegistry.cmake`）；工具本來就在 `tools/<工具>/`。
- **用詞 app→game**（宣告第 2.1 節的改名表）：
  - `config/projects.csv`→`config/games.csv`（欄位不變；JSON 集合 `projects`→`games`）；cache 變數 `GYO_APPS`→`GYO_GAMES`、`GYO_REGISTRY_FILE`→`GYO_GAME_REGISTRY_FILE`。
  - `GyoApps`／`GyoAppProject`／`ExportAppRegistry`→`GyoGames`／`GyoGameProject`／`ExportGameRegistry`；`gyo_app_*` 等函式、`GYO_APP_*` 等屬性、錯誤訊息裡的 app 都改 game。
  - `GyoAppRegistry.cmake` **拆成兩個**：modules、plugins、tools、games 共用的 CSV／JSON 解析進中性的 `GyoRegistryCsv.cmake`（`GyoUnitRegistry`、`GyoToolRegistry` 改 include 它），遊戲專屬的進 `GyoGameRegistry.cmake`。
  - 產品種類 KIND `app`→`game`；打包清單 `schema_version` 3→4（讀寫者全在 repo 內，不留相容分支）。
  - 工具描述 `requires_apps`→`requires_games`、version 1→2（version 1 被拒，`ToolRegistryTests` 加 `old_version` 一案）。
  - CI：`app_registry.py`→`game_registry.py`（矩陣的鍵 `game`、`kind="game"`、輸出 `has_games`）、`app_copy_integration.py`→`game_copy_integration.py`、workflow 步驟 `game_copy`、ctest `cmake.GameRegistry`。
  - 生成的身分標頭 `gyo/AppConfig.hpp`（`Gyo::AppConfig::Id` 等）→`gyo/GameConfig.hpp`，巨集 `GYO_GAME_ID`、`GYO_GAME_DISPLAY_NAME`、`GYO_GAME_ASSETS`、`GYO_GAME_CONTENT_MANIFEST`（N15；不打開任何 namespace，複製遊戲不用改原始碼）。
- **守衛的掃描根**（照 §14.4 核准）：naming `SCAN_ROOTS`、os `ROOTS`／`RUNTIME_ROOTS`／`GYO_OS_DEFINITION_ROOTS`、DI 11 的 `ProcessCodePageCheck`（與實作 `GyoCodePage.cmake`）、workflow gates 的產品目錄與比對式、`kernel_guard` 的 `REGISTRIES`：`apps`→`games`。os.md DI 1、DI 2 與 Contracts 節的掃描根改字（意圖不變，字面範圍跟著建置目錄換）。
- **複製整合的 mirror**（§14.3）：在 gyo2 上已壞（a8065853 讓 `GyoBuild.cmake` 讀 `config/modules.csv`，mirror 沒跟上）。mirror 加 `config/modules.csv`、`config/plugins.csv`、`plugins/`；遊戲與工具的登記檔不帶。`test_game_copy_integration.py` 加 `MirrorTests` 當回歸測試（在 `build.ci`，L1 會跑）。
- **新根的檢出**（宣告第 5.3 節 (a)）：`cmake.ProcessCodePage` 的合成樹加 `games/sample` 與 `unrecorded_game` 一案。
- 文件：README 三語版與 `docs/creating_games.md`（原 `creating_apps.md`）的目錄與名字、`docs/tool_projects.md`（工具描述 version 2）、`docs/architecture.md:278-280`（連結）、`docs/architecture/README.md:56`（登記檔名，註明改名）、同詞異義表 App 列 (ii) 一句、limits 頁 LIM-9。

## 查證

全部在這台 macOS Intel 上，test preset（`cpu|shader` 標籤）。證據在主 checkout（`/Users/karasu/Code/Source/GYO-Engine`）的 `build/evidence/gyo2-build-games-dir/`（`README.md` 有各檔說明）。

| 項目 | 結果 |
|---|---|
| 改動前 ctest（gyo2 0ed6ea10） | 196/196 |
| 複製整合 Red（改動前） | configure 失敗 `Registry does not exist: …/config/modules.csv` |
| 改動後 `--fresh` configure＋建置＋ctest，第一次 | 193/196：`cmake.GameRegistry`、`cmake.ToolRegistry`、`build.ci` 失敗。原因是改名漏網：`-DGYO_REGISTRY_FILE` 這類寫法 `D` 與 `G` 之間沒有 `\b`，正規式沒換到；`ci_scope.py`、`test_ci_scope.py` 不在改名清單；兩個測試的 JSON 鍵 `projects`。修正後三個都過 |
| 改動後 ctest，最終（修正後重建） | 196/196（`after/ctest2.log`） |
| `MirrorTests` 的 Red／Green | 把 mirror 改回舊的：失敗（`'config/modules.csv' not found in {'CMakeLists.txt'}`）；新 mirror：通過 |
| 複製整合 Green | 兩個合成遊戲從 `games/` 選到、建出、安裝、執行，身分與資產各自正確 |
| 第 5.3 節 (b) probe 實測（不提交的臨時遊戲 `games/probe`） | 正例：configure 成功、建出、執行印 `probe`；負例：`OS definition guard: gyo_probe COMPILE_DEFINITIONS has 'GYO_WINDOWS'`；對照：拿掉 `games` 根後同一負例通過 |

### 突變（D184：只跑相關的）

選法：python 的守衛清單（naming、os、r0b3、json）測試檔本批有大量改動，全跑；os-subsystem 清單跑 `file` 或 command 落在改到的檔的；kernel、orchestration 清單只跑 `find` 落在改動區塊的（kernel 1 個，orchestration 0 個）。

| 清單 | 結果 |
|---|---|
| `plans/kernel/scripts/mutations.json` | 1/1（`ks-k1-registry-ignored`） |
| `plans/os-subsystem/scripts/mutations.json` | 23/23，5 個只在 win32 跑、這台 skip |
| `target/v2/scripts/naming_mutations.json` | 28/28 |
| `tests/common/ci/r0b3_mutations.json` | 14/14 |
| `tests/common/json/mutations.json` | 15/15 |
| `tests/common/os/guard_mutations.json` | 26/26 |

**加入的突變**（§14.5，2 個，都在 `plans/os-subsystem/scripts/mutations.json`，`guards`：os DI 11、宣告 5.3）：
- `bg-code-page-walk-skips-games`：`GyoCodePage.cmake` 的根拿掉 `games`。只有 `games/sample` 這個合成目錄抓得到；`pu-x8` 拿掉的是 `tests`，不是產品根。
- `bg-code-page-check-skips-games`：`ProcessCodePageCheck.cmake` 的根拿掉 `games`。只有 `unrecorded_game` 一案抓得到。

改字串的突變（意思不變）：naming 1、os guard 1、os-subsystem 3（`what`、`find`、`replace`）、r0b3 的 `file` 與測試名。

## 範囲の限界

LIM-9：L1（quick）不經過建置層的遊戲路徑，os 定義守衛的 `games` 根沒有常設的檢出；第一個遊戲登記並在 L1 被選到、或複製整合進 L1 時解除。Object_Connect 移植那批的事前宣告要帶上。

## 檢查

- 事前宣告的敵對式檢查 r0、窄複查 r1：Opus／high，和作者同一個模型（Fable 用量不足，使用者經主會話指示「暫時用Opus」）。
