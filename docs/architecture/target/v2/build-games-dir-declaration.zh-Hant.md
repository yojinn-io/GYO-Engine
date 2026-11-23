# 建置層認 `games/`：事前宣告

- 批次代號：`v8-arch-gyo2-build-games-dir`（批次表〈後續小事〉「建置層仍寫死 master 的 `apps/`」那一列）
- 狀態：**動工前核准（2026-11-23）**：使用者「照建議，選 B，守衛核准」（經主會話；HANDOFF `v8-arch-gyo2-build-games-dir-approved`）。核准的是修訂 2（e182968d）：Q1＝B；第 5.1 節五處守衛的掃描根與 os.md DI 1、DI 2 的改字照 §14.4 核准；第 5.4 節的範囲の限界由主會話判定登記成 LIM-9。
- 基準：`gyo2` 0ed6ea10
- 依據的決定：`v8-arch-gyo2-top-dirs-games-assets-tools`（頂層 `games/<遊戲>/`、`assets/<產品>/`、`tools/<工具>/`）、`v8-arch-gyo2-object-connect-dirs-now`（Object_Connect 的目錄已用 `.gitkeep` 建好）
- 為什麼單獨一批：批次表那一列與 `-object-connect-dirs-now` 原寫「移植 Object_Connect 那一批一起改」；主會話 2026-11-23 依使用者「開 1、2、3」把它開成這張任務卡，先做。好處是 Object_Connect 的程式進來時，建置層與守衛已經認得 `games/`，產品的 CMake 也照新名字寫。

一句話：建置層與掃描它的守衛，把遊戲的原始碼目錄從 master 的 `apps/` 改成 `games/`；工具本來就在 `tools/`，不用改；`assets/<產品>/` 的接合點不變，在第 4 節寫明。

**修訂紀錄**
- 修訂 0（1782ad6b）：建議 B（路徑＋用詞 app→game）。
- 修訂 1（8f43c0cd）：敵對式檢查 r0（B5 S11 N6）全部採用，**建議改成 A**，理由是「B 改名的登記機制在 carry list 上是帶過去並重審、還沒重審」。
- 修訂 2：窄複查 r1（B3 S7 N8）全部採用，對照在第 10 節。r1 B1 指出修訂 1 的理由**引錯了**：carry list 上產品登記、打包、驗收、發佈、CI 腳本是「帶過去」（`rebuild-r0-carry-list` :60），附錄 :272 還否決了「R1 重寫」；標「帶過去並重審」的只有 `GyoBuild.cmake` 與元件對照表（:61、:273），重審的理由是對照表指向被刪的 target。退掉 B 的前提不成立，**建議改回 B**，C（重寫登記機制）拿掉。另外補上：`games` 根在 os 定義與 code page 兩處沒有任何測試抓得到（第 5.3 節）、§14.4 的驗證者與突變清單的檢查（第 7 節）、複製整合的 Red／Green 與範囲の限界（第 5.4 節）。
- 兩次檢查的檢查者都是 Opus／high，和作者同一個模型（Fable 用量不足，使用者經主會話指示「暫時用Opus」）。照導航員規則寫明：同一模型的盲點可能重疊。

---

## 1. 現況（讀過的程式）

### 1.1 寫死 `apps` 的地方（`git grep -n -I -P '\bapps\b'`，正例命中 `GyoBuild.cmake:68`）

| 種類 | 位置 | 做什麼 |
|---|---|---|
| 建置 | `build/cmake/GyoBuild.cmake:68-69`、`:90-91` | 對每個選到的遊戲 `add_subdirectory(apps/<遊戲>)`，以及它的 `tests/<遊戲>/`、`build/acceptance/<遊戲>/` |
| 建置 | `build/cmake/GyoApps.cmake:38` | 讀 `apps/<遊戲>/project.json` |
| 建置 | `build/cmake/GyoAppRegistry.cmake:261` | 檢查選到的遊戲有 `apps/<遊戲>/CMakeLists.txt` |
| 建置 | `build/cmake/GyoCodePage.cmake:27`（註解 :4） | Windows 執行檔套 UTF-8 manifest 的根（os.md Contracts :181 寫的實作，DI 11 的機制） |
| 守衛 | `tests/common/os/ProcessCodePageCheck.cmake:19`（註解 :2） | DI 11 的獨立檢查；檔頭 :6-8 說它刻意不沿用 `GyoCodePage.cmake` 的記錄 |
| 守衛 | `tests/common/naming/naming_guard.py:63` `SCAN_ROOTS`（docstring :38-40、註解 :173；自測 `test_naming_guard.py:274-280`、`:313` 釘住；`tests/common/naming/CMakeLists.txt:3` 註解） | N17 |
| 守衛 | `tests/common/os/os_boundary_guard.py:31-32` `ROOTS`、`RUNTIME_ROOTS`（自測 `test_os_boundary_guard.py:122-124`、`:194-214`、`:223-224`、`:296`、`:340-377`） | os.md DI 1、DI 2 |
| 守衛 | `tests/common/os/CMakeLists.txt:158` `GYO_OS_DEFINITION_ROOTS` | 同上，定義的掃描根；沒有釘值測試 |
| CI 測試 | `tests/common/ci/test_workflow_gates.py:1162`、`:1166-1167`（訊息）、`:1177`（比對式） | 共通 CI 定義不寫產品名（AGENTS §7、§9） |
| 文件條文 | `docs/architecture/os.md:387`（DI 1）、`:393`（DI 2）；DI 節外的 `:178`（「every app and tool」）、`:181`（Contracts 的 code page 根）、`:306`、`:318` | 條文寫了掃描根 |
| 合成樹的測試（必改） | `tests/common/cmake/AppRegistryTests.cmake`、`ProjectCompositionTests.cmake`、`ToolRegistryTests.cmake:140-146`；`tests/common/ci/app_copy_integration.py:32`、`test_app_copy_integration.py:21-22` | 經過 `GyoApps.cmake:38` 或 `GyoAppRegistry.cmake:261` 的假樹，路徑不改就失敗 |
| 合成樹的測試（只是合成資料） | `GoServiceTests.cmake`（自己寫 `add_subdirectory`）、`test_go_checks.py`、`test_release_channels.py`、`test_kernel_guard.py:587` | 路徑只是例子，改不改都通過；方案 B 為了一致改，`test_kernel_guard.py:587` 不改（K5 的例子，和遊戲目錄無關） |
| 突變 | `docs/architecture/target/v2/scripts/naming_mutations.json:356-357`、`tests/common/os/guard_mutations.json:295`、`docs/architecture/plans/os-subsystem/scripts/mutations.json:390`（`what`）、`:392`、`:454`、`:1451-1452` | `find`、`replace` 或說明裡有 `apps` |
| 說明文件 | README 三語版、`docs/creating_apps.md` | 講目錄與複製遊戲的那幾行 |

`tools/` 已經是 `tools/<工具>/`（`GyoTools.cmake:25`、`GyoBuild.cmake:76`、`GyoToolRegistry.cmake:232`），路徑不用改。

**現在就有的覆蓋缺口**：`games/object_connect/` 已存在，但上面四個守衛與 workflow gates 都不掃 `games/`；workflow gates 目前在 skip。目錄裡只有 `.gitkeep`，還沒漏掉東西；Object_Connect 的程式一放進來就會漏。

### 1.2 master 的產品登記機制，在 gyo2 的去留

| 機制 | carry list | 本批 |
|---|---|---|
| 產品登記、打包、驗收、發佈、CI 腳本（`projects.csv`、`tools.csv`、`gyo_app_*`、生成的身分標頭、KIND、`app_registry.py`、複製整合） | 「帶過去」（:60），否決「R1 重寫」：職責出自 AGENTS §7、§9，不是舊 Engine 模組的形狀（:272）；登記檔放頂層 `config/`，比照 Cargo workspace 根清單與 `go.work`（`v8-arch-gyo2-config-top-level`） | 方案 B 改目錄與用詞，形不改 |
| `GyoBuild.cmake`、元件對照表（`project.json` 的 `components`、`GyoComponents.cmake`） | 「帶過去並重審」（:61、:273），理由是對照表指向被刪的 target、元件的形狀改成 plugin 選擇 | 元件那一半屬 SDL 那條線，不動；`GyoBuild.cmake` 只改產品目錄 |
| Go service 機制 | 「機制留」（:274）；gyo2 沒有任何 `go.mod` | 只跟著改用詞 |
| `gyo_product_units`（gyo2 新加） | — | 不動；和產品登記、打包的互動在第一個真實產品接上時核（編排骨架宣告 :711） |
| shader bundle | — | 不動（沒有 pipeline，宣告就報錯） |
| 產品專屬測試與驗收在 `tests/<產品>/`、`build/acceptance/<產品>/` | 使用者沒定；AGENTS §7 允許分散，只要認得出 owner | 不搬 |

---

## 2. 方案

**基盤還是應用**：基盤。Product Registration 與建置的共通結構是「再利用可能な層の共通構造」（AGENTS §2）：每個產品都積在它上面，改它要改每個產品的登記與 CMake。

| | A：只改路徑與守衛 | B：A＋用詞 app→game（建議） |
|---|---|---|
| 做什麼 | 第 1.1 節的 `apps` 改成 `games`，補齊守衛與條文；`GYO_APPS`、`projects.csv`、`gyo_app_*` 等名字不動 | A 的全部，加上建置層與 CI 裡指遊戲的 `app` 一律改成 `game`（第 2.1 節） |
| 系譜 | 使用者定的新目錄；機制與名字照 master | 使用者定的新目錄；機制照 master（carry list「帶過去」）；名字照使用者 2026-11-14「這個可以換掉，用 Game 就好，產品實際上有 2 種，一種是 Game，另一種是 Tool」（`rebuild-vocabulary` :25-27） |
| 「是不是在為舊形狀找位置」 | 否：把產品放到使用者定的新位置 | 否：同左；機制本身是 carry list 決定帶過去的，B 只讓名字和使用者定的詞一致 |
| 留下的 | 目錄叫 `games/`，選它的變數叫 `GYO_APPS`、登記檔叫 `projects.csv`，同一概念兩個名字；Object_Connect 的 CMake 會照 `gyo_app_project()`、`Gyo::AppConfig` 寫，之後改名要連產品一起改 | 沒有 |
| 碰到的檔 | 約 32 個 | 約 60 個（再加 CI 的 Python、兩個 workflow、打包清單的讀寫者、`kernel_guard`、release 腳本與測試） |
| 估時 | 約 5 小時（含第 5.3、5.4 節的驗證與 L1） | 約 9 小時 |
| 支撐的 Feature | Object_Connect 移植（使用者在 SDL 宣告 Q5 點名，需求併進 R2b）放進 `games/object_connect/`；之後的工具（KeepTool，HANDOFF :56）放進 `tools/<工具>/` | 同左 |

**取捨**：
- A 的依據：使用者「放asset的方式，我想先照用master」，主會話記的「使用者照 master 的分法，只改遊戲目錄的名字」（HANDOFF :307）。範圍小、不動 workflow。
- B 的依據：使用者定的用詞（`rebuild-vocabulary` :25-27）；使用者的原則「改名排前面，越晚影響越大」。現在兩份登記檔都只有表頭、沒有任何產品，是改名最便宜的時候；Object_Connect 的程式一進來，改名就要連產品一起改。
- **建議 B**。修訂 1 用來退掉 B 的理由（carry list「帶過去並重審」）引錯了，拿掉後，B 的費用只剩機械改名與 workflow，不改任何判定的意思。

### 2.1 B 的改名表

| 舊 | 新 |
|---|---|
| `apps/<遊戲>/` | `games/<遊戲>/` |
| `config/projects.csv`（JSON 裡的集合 `projects`） | `config/games.csv`（集合 `games`）；欄位不變 |
| cache 變數 `GYO_APPS`、`GYO_REGISTRY_FILE`；`CMakePresets.json` 兩處 | `GYO_GAMES`、`GYO_GAME_REGISTRY_FILE`（對齊 `GYO_TOOLS`、`GYO_TOOL_REGISTRY_FILE`） |
| `GyoAppRegistry.cmake` | **拆兩個**：共用的 CSV／JSON 解析（`_gyo_registry_json_quote`、`_gyo_registry_json_append`、`_gyo_registry_csv_records`、`_gyo_read_registry`、`_gyo_registry_package_platforms`）進中性的 `GyoRegistryCsv.cmake`；遊戲專屬的進 `GyoGameRegistry.cmake`。`GyoUnitRegistry.cmake:32`、`GyoToolRegistry.cmake:2` 改 include 中性的那個（不然 modules、plugins、tools 的登記都要 include 一個叫 Game 的檔） |
| `GyoApps.cmake`、`GyoAppProject.cmake`、`ExportAppRegistry.cmake` | `GyoGames.cmake`、`GyoGameProject.cmake`、`ExportGameRegistry.cmake` |
| 函式：`gyo_app_project`、`gyo_app_add_library`／`_executable`／`_go_service`、`gyo_app_link_components`、`gyo_app_get_target`、`gyo_get_app_target`、`gyo_use_app_config`、`gyo_select_apps`、`gyo_read_app_registry`、`gyo_validate_app_sources`、`gyo_configure_apps`、`gyo_export_app_matrix`、`gyo_app_registry_package_platforms`、`gyo_stage_app_content`、`gyo_tool_app_dependency` | `app` 換 `game`，其餘不變 |
| 屬性與變數：`GYO_APP_*`（含 `GYO_APP_CONTEXT`、`GYO_APP_OWNER`、`GYO_APP_CONFIG_OWNER`、`GYO_APP_REGISTRY_DATA`）、`GYO_ACTIVE_APPS`、`GYO_CURRENT_APP`、`GYO_TOOL_*_REQUIRES_APPS` | `GYO_GAME_*`、`GYO_ACTIVE_GAMES`、`GYO_CURRENT_GAME`、`GYO_TOOL_*_REQUIRES_GAMES` |
| 錯誤訊息：「App/tool owner collision」（`GyoToolRegistry.cmake:27`）、「cannot depend on apps」（:194，`ToolRegistryTests.cmake:65` 比對）等含 app 的訊息 | 換成 game；比對它的測試跟著改 |
| 產品種類 `KIND app`。讀者：`package_contract.py:80`、`:95`，`assemble_runtime.py:55`、`:97`，`GyoRuntimeDeployment.cmake:30`、`:43`，`release_support.py:188`，`archive_package.py:38`，`run_package_checks.py:115` | `game` |
| 打包清單 `schema_version` 3。讀寫者：`GyoPackaging.cmake:65`、`package_contract.py:76`、`test_native_linkage.py:19`、`test_release_pipeline.py:65`、`ProductPackagingTests.cmake:82`、`ProjectCompositionTests.cmake:115` | 4（列舉值改名，舊讀者讀不了；讀者都在 repo 內、gyo2 沒有發過套件，不留相容分支） |
| 工具描述 `project.json` 的 `requires_apps`（version 1） | `requires_games`（version 2）；gyo2 沒有任何工具描述，只有測試的假樹 |
| CI：`build/ci/common/app_registry.py`、矩陣的鍵 `app`、`kind="app"`、輸出 `has_apps`（:130、:138）、`release_channels.py:72` 的 `app_pairs`、`tests/common/ci/test_app_registry.py` | `game_registry.py`、`game`、`kind="game"`、`has_games`、`game_pairs`、`test_game_registry.py` |
| `tests/common/ci/app_copy_integration.py`、`test_app_copy_integration.py`、fixture `copy_game/app/`、workflow 步驟 `app_copy`（`build-and-validate.yml:167-171`、`:352`、`:370`） | `game_copy_integration.py`、`test_game_copy_integration.py`、`copy_game/game/`、`game_copy` |
| `test_workflow_gates.py:388`（`-DGYO_APPS=$apps`）、`:435-441`、`:701`（禁止字串 `app_copy_integration`） | 跟著改；`:701` 的禁止字串改成新檔名（L1 不跑複製整合；不改的話這條會永不命中、悄悄放寬，見第 5.1 節） |
| `prepare-release.yml:8` 的說明字串 | `config/games.csv` |
| `r0b3_mutations.json` 的 `file`：:46、:84 指 `GyoApps.cmake`；:198、:217 指 `app_registry.py`；測試名 :20、:210、:229、:248 | 新檔名；`find` 不含 app，不用改 |
| `kernel_guard.py:105` `REGISTRIES`（docstring :13）、`test_kernel_guard.py` 的假登記檔（:54、:99、:105、:112、:218） | `config/games.csv` |
| `tests/common/json/test_json_guard.py:26` 的一行假資料 | `GYO_GAME_SOURCE_DIR` |
| ctest `cmake.AppRegistry`（`tests/common/cmake/CMakeLists.txt:1`、`AppRegistryTests.cmake`） | `cmake.GameRegistry`、`GameRegistryTests.cmake` |
| 生成的 `gyo/AppConfig.hpp`，`namespace Gyo::AppConfig { Id, DisplayName, Assets, ContentManifest }` | `gyo/GameConfig.hpp`，巨集 `GYO_GAME_ID`、`GYO_GAME_DISPLAY_NAME`、`GYO_GAME_ASSETS`、`GYO_GAME_CONTENT_MANIFEST`（見下） |
| `docs/creating_apps.md` | `docs/creating_games.md`；`docs/architecture/README.md:56` 的登記檔名改 `games.csv`（那一句記的是使用者 2026-11-21 的決定，只改檔名，註明） |
| 不改 | target 名 `gyo_<id>`、`gyo_<id>-<role>`；`GYO_PRODUCTS`、`gyo_install_product_target`、KIND `toolchain`（中性，指產品與工具兩者） |

**生成的身分標頭**（卡片照命名規範判）：現在的 `Gyo::AppConfig` 不屬於 N17 封閉表裡任何根、也不是產品自己的根；`Id` 等常數不合 N15（常數 `k` 開頭）；名字用了 App。走的路：
- (i) 生成標頭裡定巨集，N15「巨集 `GYO_` 開頭全大寫」←選。值與現在相同，同一個 target 混用兩個遊戲的設定，照舊由 `GYO_GAME_CONFIG_OWNER` 擋。
- (ii) `target_compile_definitions` 直接注入，不要標頭——最接近 Cargo 的 `env!("CARGO_PKG_NAME")`，但字串值要在 MSVC 與 GCC／Clang 各自跳脫，生成標頭只要跳脫一次（現在 `GyoAppProject.cmake:20-25` 已經做了）。退掉。
- (iii) 新根 `GYO::Game`：N17 的根要和原始碼目錄一一對應，生成的標頭在 build 目錄，對不上；還要改封閉表與守衛。退掉。
- (iv) 產品自己的根：複製遊戲時 namespace 跟著變，複製的程式編不過（AGENTS §7 Clone）。退掉。
- 巨集沒有作用域；名字都以 `GYO_GAME_` 開頭，撞名的機會小。

**改名同時立規則**（使用者「改名排前面、同時立規則」）：在同詞異義表 App 列 (ii)（`rebuild-vocabulary` :223）補一句「建置與 CI 用 game／tool 指兩種產品，不用 app（本批改完）」，之後的批次照著寫。不加守衛（誤用由審查看）。

---

## 3. Architecture Delta 的 7 項（方案 B；A 是其中的路徑部分）

1. **需要它的 Feature**：Object_Connect 移植進 `games/object_connect/`；之後的工具進 `tools/<工具>/`。頂層目錄是使用者 2026-11-23 定的。
2. **現在的問題**：建置只認 `apps/`，`games/<遊戲>/` 的程式不會被建置；四個守衛與 workflow gates 不掃 `games/`。建置與 CI 用 app 指遊戲，和使用者定的詞不同。
3. **變的邊界**：產品登記的目錄規則（遊戲＝`games/<登記名>/`，工具、資產不變）；遊戲登記檔的檔名；選取的 cache 變數；打包清單的 `kind` 值與版本；工具描述的鍵與版本；os.md DI 1、DI 2 條文與 Contracts 節的掃描根文字。
4. **影響的 Module／Product／Tool**：Engine 與 plugins 的程式不受影響。目前沒有任何登記的遊戲或工具，沒有產品要搬。受影響的是建置層、CI（`build/ci/common`、兩個 workflow）、守衛（naming、os、DI 11 的檢查、workflow gates、kernel_guard）、它們的測試、os.md。
5. **依賴方向**：不變。Game／Tool → Engine；建置層讀登記檔、`add_subdirectory` 產品目錄；Engine 不參照產品。新拆的 `GyoRegistryCsv.cmake` 被三份登記的讀取共用，方向和現在 include `GyoAppRegistry.cmake` 一樣。
6. **Ownership**：不變。`games/<遊戲>/` 歸該遊戲；登記檔歸建置層的產品登記；守衛的掃描根歸各守衛的 owner；os.md 的條文歸 OS。
7. **為什麼不能更小**：只改 `GyoBuild.cmake` 不改守衛，`games/` 的程式會躲過四個守衛（AGENTS §4：局部不等於責任正確）；條文寫著掃描根，守衛改了條文不改就不一致。比 A 多出的用詞部分，理由在第 2 節。

---

## 4. `assets/<產品>/` 的接合點（本批不改）

| 段 | 現在怎麼接 | owner |
|---|---|---|
| 來源 | `assets/<登記名>/`，和 `games/<登記名>/` 同名；內容根目錄的入口是 `content.json` | 產品擁有內容；`content.json` 與 catalog 的格式、Version、驗證規則歸 asset（`asset.zh-Hant.md` 第 49～99 行），`shader_bundles` 的內容歸 render |
| 建置讀的 | `content.json` 的 `version` 與 `shader_bundles` 個數（`GyoContent.cmake:24`；有 shader bundle 就報錯） | 建置層 |
| 組裝 | `build/assemble_runtime.py` 照 `build/content_contract.py` 驗證並複製到 stage 的 `bin/assets/<登記名>/`；`cmake --install` 也跑同一個 | 建置層 |
| 執行時 | 生成的身分標頭給 `assets/<登記名>`、`assets/<登記名>/content.json`，是相對執行檔目錄的路徑；`AssetStore` 會把相對的內容根對 CWD 轉成絕對路徑（asset 頁第 4 節），所以產品要先接上執行檔目錄（fixture 用 `argv[0]` 的 parent；Engine 有 `OS::Process::ExecutablePath`），再交給 `AssetStore` | 產品組裝（setup）；asset 只看內容根目錄 |

本批碰到的只有：路徑規則的說明、B 的 `--kind app`→`game`（組裝器的參數，不是 asset 契約）、身分標頭改成巨集（值不變）。`content.json`、catalog、`content_contract.py` 的規則都不改，不觸及 asset 的 Data Contract。

**工具的資產**：使用者定的 `assets/<產品>/` 包含工具，但現在只有遊戲的主執行檔會掛內容（`gyo_attach_content` 只在主執行檔的 `gyo_*_add_executable(... MAIN)` 裡）。現在沒有點名要資產的工具，本批不做；第一個要資產的工具出現時，把同一個組裝開給工具（不動 asset 契約）。

---

## 5. 影響的守衛、突變、CI 測試

### 5.1 守衛判定與條文的改動（AGENTS §3、§14.4）

都是「掃描根的清單」把 `apps` 換成 `games`。naming（N17）、os（DI 1、DI 2）、`ProcessCodePageCheck`（DI 11）、kernel_guard（K1，只 B）是 Design Invariants 的守衛，照 §14.4 不論鬆緊都交使用者核准。條文：照 §3 屬 DI 節變更的是 os.md DI 1（:387）、DI 2（:393）；`:178`、`:181`（Contracts 節）、`:306`、`:318` 在 DI 節外，一併改。改字的意思：**意圖不變（產品的原始碼），字面範圍跟著建置讀的目錄從 `apps` 換成 `games`**。

| | naming `SCAN_ROOTS` | os `ROOTS`／`RUNTIME_ROOTS`／`GYO_OS_DEFINITION_ROOTS` | `ProcessCodePageCheck.cmake:19`（與 `GyoCodePage.cmake:27`） | workflow gates `:1162`、`:1177`（B 另有 `:701`） | `kernel_guard` `REGISTRIES`（只 B） |
|---|---|---|---|---|---|
| 1 守的性質 | 建置讀的產品目錄裡的 C++ 不打開 `GYO::` 的根（N17） | 建置讀的產品目錄裡，平台分支與檔案 I/O 只在 OS 層（DI 1、DI 2） | 建置出的每個 repo 自己的 Windows 執行檔都帶 UTF-8 manifest（DI 11） | 共通 CI 定義不寫產品或工具的名字；`:701` 是 L1（合併關卡）不跑打包與複製整合 | 登記的產品名不進 Kernel（K1） |
| 2 緩的面 | `apps/` 底下的檔不再掃 | 同左 | `apps/` 底下的執行檔不再查 | `apps/` 底下的目錄名不再當產品名；`apps/<名>` 不再比對 | 不再讀 `projects.csv` |
| 3 緊的面 | `games/` 底下開始掃 | 同左 | 同左 | `games/` 底下開始取名、`games/<名>` 開始比對；`games/object_connect/` 已存在，這條從 skip 變成實際執行 | 無 |
| 4 緩的面誰在守 | 緩的面改完後沒有對象：建置不再讀 `apps/`（`GyoBuild.cmake`、`GyoApps.cmake:38`、`GyoAppRegistry.cmake:261` 全改），不需要其他判定 | 同左 | 同左 | 同左 | 同左（`projects.csv` 改名後不存在） |
| 5 證據 | 自測的釘住值跟著改；`nm-scan-roots-examples` 改 `find` 後仍 killed | 自測跟著改；`os-g11-roots-skip-products` 等仍 killed；`GYO_OS_DEFINITION_ROOTS` 見第 5.3 節 | 見第 5.3 節（現有突變 `pu-x8` 拿掉的是 `tests`，和產品根無關，不能當證據） | `.github` 與 `build/ci` 裡 `object_connect` 0 處（r0、r1 都查過），實際執行會通過 | `test_kernel_guard.py` 的假登記檔跟著改；`ks-k1-registry-ignored` 仍 killed |

**緊的面不會讓現有 CI 失敗**：`games/` 只有 `.gitkeep`；naming、os 守衛的自測都用合成樹；os 守衛遇到空的產品根是跳過；workflow gates 實際執行也通過（上表第 5 列）。

**已知的不一致（不改）**：os.md :318-319 寫「every existing root must yield at least one source」，但 `os_boundary_guard.py` 只要求 `REQUIRED_ROOTS = engine, tests`，產品根空了就跳過（`tools/` 現在就是空的）。本批只改那一句裡的 `apps`，不一致記在這裡，轉 OS 的 owner（主會話）。

### 5.2 突變（D184：只跑相關的）

- 改 `find`／`replace`／`what` 字串（意思不變）：`naming_mutations.json:356-357`；`os/guard_mutations.json:295`；`plans/os-subsystem/scripts/mutations.json` 的 :390、:392、:454、:1451-1452；B 另改 `r0b3_mutations.json` 的 `file` 與測試名（第 2.1 節）。
- 新加（§14.5：只有它能抓的錯）：
  - `games` 根從 `GyoCodePage.cmake:27` 拿掉——守 DI 11；由第 5.3 節 (a) 的新斷言抓。
  - `games` 根從 `ProcessCodePageCheck.cmake:19` 拿掉——守 DI 11；由第 5.3 節 (a) 的新斷言抓。
  - 預期的失敗寫成各自專屬的訊息片段，實作時寫進清單。
- 要跑的：上面這些，加上 `find` 落在本批改到的檔的、殺它的測試被本批改到的。

### 5.3 新根的檢出力（r1 B2）

`games` 根在下面兩處，現在沒有任何測試或突變抓得到：拼錯或漏掉時，本機 ctest、L1、複製整合都會通過。

- **code page 兩處**（`GyoCodePage.cmake:27`、`ProcessCodePageCheck.cmake:19`）：(a) 在 `cmake.ProcessCodePage` 的合成樹（`ProcessCodePageTests.cmake:17`、`:29`）加 `games/sample`，放一個執行檔，斷言它被標上 manifest、也被檢查看到。這是常設的回歸測試，配上第 5.2 節的兩個新突變。
- **os 定義的掃描根**（`tests/common/os/CMakeLists.txt:158`）：它在 configure 時掃實樹，實樹的 `games/` 沒有原始碼，所以突變殺不掉。(b) 本機做一次實測：臨時放一個合成遊戲（不提交），從 repo 根、`BUILD_TESTING=ON` configure。正例：它的執行檔帶 `GYO_PROCESS_CODE_PAGE`；負例：暫時給它 `-DGYO_WINDOWS`，預期 `OS definition guard:` 的 FATAL。log 存主 checkout 的 `build/evidence/gyo2-build-games-dir/`。常設的檢出力要等第一個遊戲的原始碼進來；寫成範囲の限界（第 5.4 節）。

### 5.4 驗證怎麼碰到 `GyoBuild.cmake` 的新路徑，以及範囲の限界

`cmake.ProjectComposition` 自己寫 `add_subdirectory`，不經過 `GyoBuild.cmake`；「獨立複製遊戲」步驟只在 Windows 的 release profile 跑（`build-and-validate.yml:168`；release 只由手動的 `prepare-release.yml` 觸發），L1 是 quick；而且它在 gyo2 上已經壞了。照 §14.3：

1. **Red**：在沒改的 gyo2 上本機跑一次 `app_copy_integration.py`，重現 `Registry does not exist: …/config/modules.csv`。引入的 commit 是 a8065853（編排骨架段 3 讓 `GyoBuild.cmake:55-56` 讀 `config/modules.csv`，mirror `app_copy_integration.py:17-18` 沒跟上）。
2. **修**：mirror 只加 Engine 那一側的 `config/modules.csv`、`config/plugins.csv` 與 `plugins/`；`config/tools.csv` 與遊戲登記檔不複製（腳本自己寫遊戲登記檔；不帶 `tools.csv` 才會走到 `GyoTools.cmake:8-9`「最小複製不必帶工具登記」那一支）。`plugins/` 現在不存在，加上是為了 SDL 第一個 Plugin 登記後不要再壞。`tests/`、`examples/` 不用複製（複製整合不開 `BUILD_TESTING`、`GYO_BUILD_EXAMPLES`）。
3. **Green**：改完（含 `games/`）再跑一次，兩個合成遊戲都從 repo 根 configure、建出、組裝資產。
4. 本機跑的前提：`cmake`、`ninja` 用絕對路徑或補 PATH（Claude 的 shell 不載入 `~/.zshrc`）；yyjson、fast_float 是 FetchContent，用 `--reuse-cache <既有建置的 CMakeCache.txt>` 避免連網；log 用 `--logs` 存 `build/evidence/gyo2-build-games-dir/`。macOS 跑不到 Windows 特有的 `.exe` 與 manifest 嵌入。

**範囲の限界**（不論本機跑不跑得了都成立）：L1（quick）不經過 `GyoBuild.cmake:68-71`、`:89-100` 的遊戲路徑；複製整合在 CI 只在 Windows 的手動 release 跑；os 定義的 `games` 根沒有常設的檢出。本機跑一次只證明本批當下沒問題，沒有常設的回歸測試。建議登記成 LIM-9（一句摘要：「L1 不經過建置層的遊戲路徑，第一個遊戲登記後才有常設檢出」），Object_Connect 移植那批的事前宣告要帶上它；登不登記由主會話決定。

### 5.5 會改的 CI／測試

ctest `build.ci`（`tests/common/ci`）、`cmake.AppRegistry`（B 改名 `cmake.GameRegistry`）、`cmake.ToolRegistry`、`cmake.ProjectComposition`、`cmake.ProductPackaging`（B 的版本）、`cmake.GoService`（B）、`cmake.ProcessCodePage`（第 5.3 節 (a)）、`naming.guard*`、`os.*` 的守衛自測、`kernel.guard*`（B）、`tests/common/assembly`（B 的 `--kind`、`gyo_stage_game_content`）、`tests/common/json` 的一行假資料（B）。workflow（B）：`build-and-validate.yml`、`prepare-release.yml`。

---

## 6. 觀測到的 Pressure（只報告，本批不改）

1. **「我們的原始碼在哪些頂層目錄」手寫在至少 10 處**：`GyoBuild.cmake`、`GyoApps.cmake:38`、`GyoAppRegistry.cmake:261`、`GyoCodePage.cmake:27`、`ProcessCodePageCheck.cmake:19`、`os_boundary_guard.py:31-32`、`tests/common/os/CMakeLists.txt:158`、`naming_guard.py:63`、`test_workflow_gates.py:1162`／`:1177`、`app_copy_integration.py:32`，另有 os.md 的條文。AGENTS §9「同一個事實只在一處手動維持」；這次改一個目錄名要逐處改，就是證據。`ProcessCodePageCheck` 刻意和 `GyoCodePage` 分開寫、各守衛把清單釘死，都是為了不讓守衛悄悄縮小，收成一處要保留這一點。本批逐處改；要不要收，交使用者另排。
2. **`plugins/` 與 `examples/` 不在 code page 的根與 os 守衛的根**：`examples/` 現在就有執行檔，是 DI 11 現存的缺口；`plugins/` 已轉 SDL 卡（子批 P）。主會話已記進批次表的後續小事。
3. **asset 契約在建置側的鏡像和 C++ 端已經分歧**（`content_contract.py` 要求 `shader_bundles`、不查 asset (A) 規則 4、6、7；C++ 用 `tests/common/asset/contract/`，`tests/common/fixtures/asset_contract` 只有 Python 讀）。主會話已記進批次表的後續小事。
4. **os.md :318-319 和 `os_boundary_guard.py` 的行為不一致**（第 5.1 節末）。轉主會話。
5. **README 三語版、`docs/creating_apps.md`、`docs/architecture.md` 大多是 master 的內容**。本批只改講遊戲目錄與名字的那幾行；其餘交圖書管理員。

---

## 7. 步驟、驗證、模型與檔位

| 步 | 內容 | 模型／檔位 |
|---|---|---|
| 1 | 路徑：建置層 5 處、守衛 4 個與自測、workflow gates、合成樹、突變字串、os.md 條文 | 實作 Opus／high |
| 2 | 第 5.3 節 (a) 的合成樹斷言與兩個新突變；(b) 的本機實測 | Opus／high |
| 3 | 第 5.4 節：Red、修 mirror、Green | Opus／high |
| 4 | 用詞（B）：第 2.1 節的改名表，含拆 `GyoRegistryCsv.cmake` | 實作 Opus／high |
| 5 | 文件：README 三語版與 `creating_games.md` 的目錄與名字那幾行、`docs/architecture/README.md:56`、同詞異義表 App 列一句、批次表、dev_log | Opus／medium |
| §14.4 驗證者 | 合併前，用實際資料確認第 5.1 節每欄的 1～5（守衛的新舊清單、改後的建置路徑、第 5.3 節的結果） | Opus／high（和實作者不同的 Agent；Fable 不可用，同模型，報告寫明） |
| 突變清單的敵對式檢查（§14.2） | 第 5.2 節加入與改字串的突變，找改了實作卻不失敗的地方 | Opus／high（§14.2 要和作者不同的模型；Fable 不可用，同模型，報告寫明） |
| 審查 | 合併前的 code review | Opus／high（和實作者不同的 Agent） |

上段（碰到守衛、DI 條文與 Product Registration），不用小模型。驗證：本機全新 configure＋全量建置＋`ctest`；第 5.2 節的突變；第 5.3、5.4 節；draft PR 到 `gyo2`，手動 dispatch L1（linux-x64、windows-x64、macos-arm64）。

## 8. 不碰

`engine/`、`plugins/` 的程式；asset 的契約（`content.json`、catalog、`content_contract.py` 的規則、組裝器的規則）；元件對照表；Go 機制的邏輯；shader；`gyo_product_units`；`tests/<產品>/`、`build/acceptance/<產品>/` 的位置；`test_kernel_guard.py:587`；三個 `.gitkeep`；os.md :318-319 的不一致（只改字）。

## 9. 交使用者決定的

- **Q1**：A（只改路徑與守衛）還是 B（加用詞 app→game，建議）。
- 第 5.1 節的守衛判定改動（B 五處、A 四處），以及 os.md DI 1、DI 2 的改字（核准宣告即核准方向；合併前核准頁再逐判定列一次）。
- 第 5.4 節的範囲の限界要不要登記成 LIM-9（主會話也可以決定）。

## 10. 檢查的處理

### 10.1 敵對式檢查 r0（對修訂 0，1782ad6b）

檢查者 Opus／high（同模型）。紀錄：主 checkout（`/Users/karasu/Code/Source/GYO-Engine`）的 `build/evidence/gyo2-build-games-dir/declaration-r0-adversarial.md`。

| 發現 | 處理（修訂 2 的狀態） |
|---|---|
| B1 漏列 `ProcessCodePageCheck.cmake` | 第 1.1、5.1、5.3、6.1 節 |
| B2 os.md 條文寫了 `apps` | 第 1.1、3、5.1、9 節 |
| B3 驗證碰不到 `GyoBuild.cmake`；複製整合已壞 | 第 5.4 節 |
| B4 漏列 r0b3 指向 `GyoApps.cmake` 的突變 | 第 2.1 節、5.2 節 |
| B5 整檔改名會讓共用解析叫 Game | 第 2.1 節：拆出 `GyoRegistryCsv.cmake` |
| S1 同詞異義表引錯 | 第 2 節改引使用者原話 :25-27 |
| S2 workflow gates 漏列 | 第 1.1、2.1、5.1 節 |
| S3 B 的改名表不完整 | 第 2.1 節補齊 |
| S4 檔案數低估 | 第 2 節 |
| S5、S6 §4 描述錯、太簡化 | 第 4 節；分歧記第 6.3 節 |
| S7 身分標頭的理由 | 第 2.1 節（N15、加 (ii) `target_compile_definitions`） |
| S8 基盤判定、逐選項回答、成熟做法 | 第 2 節 |
| S9 單獨一批的出處 | 文首 |
| S10 §6 要補 | 第 6.1、6.2 節 |
| S11 DI 守衛不只兩個 | 第 5.1 節 |
| N | 行號更正；KeepTool 改引 HANDOFF :56；改名規則見第 2.1 節末 |

### 10.2 窄複查 r1（對修訂 1，8f43c0cd）

檢查者 Opus／high（同模型）。紀錄：同目錄的 `declaration-r1-narrow-check.md`。

| 發現 | 處理 |
|---|---|
| B1 退掉 B 的理由引錯 carry list | 修訂紀錄、第 1.2、2 節：照實改，建議改回 B，C 拿掉；A 的依據改引 HANDOFF :307；「改名排前面」與「先照 master」的取捨寫進第 2 節 |
| B2 `games` 根在 code page 與 os 定義兩處沒有檢出 | 第 5.3 節 (a)(b)、第 5.2 節兩個新突變、範囲の限界 |
| B3 §14.4 的驗證者與突變清單的檢查沒排 | 第 7 節加兩列 |
| S1 給 C 的事項漏記 | C 拿掉；r0 S3、B4、S2 的內容進第 2.1 節 |
| S2 C 放進移植那批會讓產品照舊名寫 | C 拿掉；B 在本批做完，Object_Connect 照新名寫 |
| S3 mirror 只加 `config/` 的兩個問題 | 第 5.4 節第 2 步 |
| S4 沒照 §14.3 | 第 5.4 節 Red／Green、引入的 commit |
| S5 範囲の限界要無條件寫 | 第 5.4 節末、第 9 節 |
| S6 本機跑的前提 | 第 5.4 節第 4 步 |
| S7 §14.4 第 4 項答成套套邏輯 | 第 5.1 節第 4 列改寫 |
| N1 DI 11 的標示不準 | 第 5.1 節：DI 節變更只有 DI 1、DI 2，:181 是 Contracts 節 |
| N2 漏 :178 | 第 1.1、5.1 節 |
| N3 「意思不變」要寫準；:318-319 不一致 | 第 5.1 節 |
| N4 kernel_guard 在 A 下不必動；合成樹分必改／可選 | 第 1.1 節分兩列；`test_kernel_guard.py:587` 不改 |
| N5 兩處文字 | 第 1.1、5.2 節 |
| N6 緊的面不會讓 CI 失敗 | 第 5.1 節 |
| N7 引用查證屬實 | — |
| N8 A 也有一點投資在舊形狀 | C 拿掉後不適用 |
