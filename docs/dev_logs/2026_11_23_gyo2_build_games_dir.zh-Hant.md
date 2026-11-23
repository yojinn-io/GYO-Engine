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
  - CI：`app_registry.py`→`product_registry.py`（矩陣的鍵 `game`、`kind="game"`、輸出 `has_games`；宣告的改名表原寫 `game_registry.py`，合併前使用者改定，見下）、`app_copy_integration.py`→`game_copy_integration.py`、workflow 步驟 `game_copy`、ctest `cmake.GameRegistry`。
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

**加入的突變**（§14.5，9 個；一批的上限是「改的 DI 條文數 × 5」＝10，沒超過）。每個只抓一種別的突變抓不到的錯，各有專屬的 expect：

| id（清單） | 抓的錯 | 為什麼只有它抓得到 | `guards` |
|---|---|---|---|
| `gdir-code-page-walk-skips-games`（os-subsystem） | `GyoCodePage.cmake` 的根拿掉 `games` | 只有 `cmake.ProcessCodePage` 的 `games/sample` 合成目錄；`pu-x8` 拿掉的是 `tests` | os DI 11、宣告 5.3 |
| `gdir-code-page-check-skips-games`（os-subsystem） | `ProcessCodePageCheck.cmake` 的根拿掉 `games` | 只有 `unrecorded_game` 一案 | os DI 11、宣告 5.3 |
| `gdir-tool-owner-check-skipped`（r0b3） | configure 時不再拒絕同名的遊戲與工具（遊戲登記的資料沒傳到 `gyo_configure_tools`，改名漏網就是這個樣子） | 只有 `cmake.ToolRegistry` 新加的 project 模式案例；原本的直接呼叫抓不到 | AGENTS 7、宣告 10.3 |
| `gdir-identity-id-prefixed`（r0b3） | 生成的 `GYO_GAME_ID` 多了前綴 | 只有 `cmake.ProjectComposition` 新的括號整行比對；原本的子字串比對抓不到 | 宣告 2.1、10.3 |
| `gdir-package-foreign-owner-python`（r0b3） | 打包契約不再拒絕遊戲套件裡別的產品的執行檔 | 只有 `test_game_package_holds_only_its_own_executables`（以錯誤訊息比對） | AGENTS 7、宣告 10.3 |
| `gdir-package-foreign-owner-cmake`（r0b3） | 建置層不再拒絕遊戲產品裡別的 owner 的執行檔 | 只有 `cmake.ProductPackaging` 新加的 foreign-owner fixture | AGENTS 7、宣告 10.3 |
| `gdir-gates-positive-owner-path`（r0b3） | 實樹陽性對照：共通 CI 定義寫 `games/object_connect` | 守衛不再掃 `games/` 或不再比對 `games/<名>` 時，這個會 survived（README 規則 4） | AGENTS 7、9，宣告 10.3 |
| `gdir-gates-positive-l1-copy`（r0b3） | 實樹陽性對照：L1 的 tests 步驟跑複製整合 | `:701` 的禁止字串留在舊檔名時，這個會 survived | AGENTS 7、宣告 10.3 |
| `gdir-mirror-drops-build-cmake`（r0b3） | 複製整合的 mirror 不再帶 `build/cmake/` | `MirrorTests` 每個 include 分支各放一個檔 | 宣告 5.4 |

`gdir-` 前綴：檢查者指出 `bg-` 已在 os 守衛清單指 boundary guard，本批的改用 `gdir-`。結果：第一輪 2/2、第二輪 7/7 killed（`mutations/round2-*.log`）。
第二輪第一次跑時，`cmake.ProjectComposition` 的基準從 repo 根以 `-P` 執行會失敗（預設產生器下，`#error` 那一案的增量建置沒有失敗），而且在 repo 根留下工作目錄。所以 `ProjectComposition`、`ToolRegistry`、`ProductPackaging` 三個測試照 `ProcessCodePageTests` 加 `WORK_ROOT`（預設照舊是建置目錄），新突變傳 `-DGENERATOR=Ninja -DWORK_ROOT=build/target`；留下的兩個工作目錄已刪。

改字串的突變（意思不變）：naming 1、os guard 1、os-subsystem 3（`what`、`find`、`replace`）、r0b3 的 `file` 與測試名。

## 範囲の限界

LIM-9：L1（quick）不經過建置層的遊戲路徑，os 定義守衛的 `games` 根沒有常設的檢出；第一個遊戲登記並在 L1 被選到、或複製整合進 L1 時解除。Object_Connect 移植那批的事前宣告要帶上。

## 檢查

都是 Opus／high，和作者同一個模型（Fable 用量不足，使用者經主會話指示「暫時用Opus」）；同一模型的盲點可能重疊。紀錄在 evidence。

- 事前宣告的敵對式檢查 r0（B5 S11 N6）、窄複查 r1（B3 S7 N8）：全部採用，對照在宣告第 10 節。
- **§14.4 驗證者**（`verifier-14-4.md`）：第 5.1 節五欄的 1、2、3、5 屬實（在 base／HEAD 兩份副本實測緩與緊的面；workflow gates 從 skip 變成實際執行並通過；`:701` 等價）。兩處更正：
  - workflow gates 第 4 欄：除了 `apps/` 底下的目錄名，還有一個窄的緩面——CI 文字寫 `apps/<已登記的遊戲或工具名>` 不再被抓。`apps/` 已不是有效路徑，那種寫法是死參照，由 review 看（合併前核准頁列出）。
  - `kernel_guard` 第 5 欄：`ks-k1-registry-ignored` 的 `find` 和路徑無關，不能當路徑的證據；守住路徑的是 fail-closed（`REGISTRIES` 指向不存在的檔時實樹與自測都失敗，驗證者臨時改回 `projects.csv` 實測）。
  - 另補：probe 的腳本、臨時遊戲的 CMakeLists、對照組帶 `GYO_WINDOWS` 的證據存進 `probe/`；os.md DI 11 的 Guards 列加上兩個 code page 突變。
- **突變清單的敵對式檢查**（`mutation-adversarial.md`）：新加的兩個與改字串的突變合格；找到 6 類壞了沒人抓的地方，本批照「契約要補／誤用不補」分：
  - 補測試與突變：C1 遊戲與工具同名（project 模式）、C2 身分標頭四個巨集與跳脫、C3 遊戲套件的 owner（Python 與 CMake）、C4 workflow gates 的兩個陽性對照、C5 mirror 的每個分支（上表）。
  - 一行斷言、不立突變：C6 game 列的 `kind`（`test_game_rows_are_marked_as_games`）。
  - 已在 LIM-9：C7（`GyoBuild.cmake` 的遊戲路徑、os 定義根）。
  - 不補：C8 讓 KIND 多接受 `app`（schema 4 已擋掉所有舊 manifest，只剩手寫的，算誤用）、`GyoUnitRegistry` 改回 include `GyoGameRegistry`（行為不變，交 review）。
  - 順手修：改名留下的「an game」錯字三處、`GyoRegistryCsv.cmake` 的舊註解、`GameRegistryTests` 的 `*_apps` 區域變數。
- 補完後本機 ctest 196/196（`after/ctest3.log`）。
- **合併前設計審查**（`design-review.md`）：可以合併。機械式的已修：`release_channels.py` 的 `app_pairs`（改名表有、漏做）、`creating_games.md` 的 `--kind app` 與 `GYO_CURRENT_APP`、同詞異義表 Registry 列、`target/README.md`、`GyoRegistryCsv.cmake` 的檔頭與區域變數（修時一度留下 `string(JSON project SET "${entry}"`，建置類測試前就改回）、`docs/architecture/README.md` 寫明比照 Cargo／go.work 取了什麼。幾件後續交使用者，見[合併前核准頁](../architecture/target/v2/build-games-dir-merge-approval.zh-Hant.md)第 5 節。
- **`game_registry.py`→`product_registry.py`**（設計審查 B1）：使用者 2026-11-23（本卡）「照建議，改成 product_registry.py」。檔裡除了遊戲的匯出還有 CI 各平台的表與工具的匯出，和 CMake 側拆出中性的 `GyoRegistryCsv.cmake` 同一個理由。測試跟著改名 `test_product_registry.py`；r0b3 清單的 `file` 與測試名跟著改。

