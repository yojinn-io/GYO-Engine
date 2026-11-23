# 建置層認 `games/`：事前宣告

- 批次代號：`v8-arch-gyo2-build-games-dir`（批次表〈後續小事〉「建置層仍寫死 master 的 `apps/`」那一列）
- 狀態：**草稿，等動工前核准**。核准前不動程式。
- 基準：`gyo2` 0ed6ea10
- 依據的決定：`v8-arch-gyo2-top-dirs-games-assets-tools`（頂層 `games/<遊戲>/`、`assets/<產品>/`、`tools/<工具>/`）、`v8-arch-gyo2-object-connect-dirs-now`（Object_Connect 的目錄已用 `.gitkeep` 建好）

一句話：建置層與掃描它的守衛，把遊戲的原始碼目錄從 master 的 `apps/` 改成 `games/`；工具本來就在 `tools/`，不用改；`assets/<產品>/` 的接合點不變，在第 4 節寫明。

---

## 1. 現況（讀過的程式）

### 1.1 寫死 `apps/` 的地方

| 種類 | 位置 | 做什麼 |
|---|---|---|
| 建置 | `build/cmake/GyoBuild.cmake:68-69`、`:90-91` | 對每個選到的遊戲 `add_subdirectory(apps/<遊戲>)`，以及它的 `tests/<遊戲>/`、`build/acceptance/<遊戲>/` |
| 建置 | `build/cmake/GyoApps.cmake:38` | 讀 `apps/<遊戲>/project.json`（元件清單） |
| 建置 | `build/cmake/GyoAppRegistry.cmake:261` | 檢查選到的遊戲有 `apps/<遊戲>/CMakeLists.txt` |
| 建置 | `build/cmake/GyoCodePage.cmake:27` | Windows 執行檔套 UTF-8 manifest 的根（`engine apps tools build/acceptance tests`；os.md DI 11 的實作） |
| 守衛 | `tests/common/naming/naming_guard.py:63` `SCAN_ROOTS`（自測 `test_naming_guard.py:313` 釘住） | N17：產品的 C++ 不打開 `GYO::` 的根 |
| 守衛 | `tests/common/os/os_boundary_guard.py:31-32` `ROOTS`、`RUNTIME_ROOTS`（自測 `test_os_boundary_guard.py:223-224` 釘住） | os.md DI 1：平台分支與檔案 I/O 只在 OS 層 |
| 守衛 | `tests/common/os/CMakeLists.txt:158` `GYO_OS_DEFINITION_ROOTS` | 同上，定義的掃描根 |
| CI 測試 | `tests/common/ci/test_workflow_gates.py:1162` | 共通 CI 定義不寫產品名（AGENTS §7、§9）；產品名從 `apps/`、`tools/`、`services/` 底下的目錄取 |
| 合成樹的測試 | `tests/common/cmake/AppRegistryTests.cmake`、`ProjectCompositionTests.cmake`、`GoServiceTests.cmake`；`tests/common/ci/app_copy_integration.py`（L1 的「獨立複製遊戲」步驟）、`test_app_copy_integration.py`、`test_go_checks.py`、`test_release_channels.py` | 假樹裡用 `apps/<名>`（後兩個只是合成的路徑資料） |
| 突變 | `docs/architecture/target/v2/scripts/naming_mutations.json:356`、`tests/common/os/guard_mutations.json:295`、`docs/architecture/plans/os-subsystem/scripts/mutations.json:392`、`:454`、`:1451` | `find` 或 `replace` 字串裡有 `apps` |

`tools/` 已經是 `tools/<工具>/`（`GyoTools.cmake:28`、`GyoBuild.cmake:77`、`GyoToolRegistry.cmake:226`），路徑不用改。

**現在就有的覆蓋缺口**：`games/object_connect/` 已存在，但上面三個守衛與 `test_workflow_gates.py:1162` 都不掃 `games/`；workflow gates 那一條目前因為「`apps/`、`tools/`、`services/` 底下沒有目錄」在 skip。現在目錄裡只有 `.gitkeep`，所以還沒漏掉東西；Object_Connect 的程式一放進來就會漏。

### 1.2 master 的產品登記機制，在 gyo2 的去留（只判本批會碰到的）

| 機制 | gyo2 還用不用 | 本批 |
|---|---|---|
| 產品登記檔 `config/projects.csv`、`config/tools.csv`（名字、enabled、平台；N8「產品登記檔」） | 用。`kernel_guard.py:105` 也讀 | 方案 B 把 `projects.csv` 改名（第 2 節） |
| `project.json` 的 `components`（`GyoComponents.cmake` 的 SDL_*、UFBX、IMGUI） | 元件對照表「帶過去並重審」（`rebuild-r0-carry-list` :61），屬 SDL 那條線 | 不動 |
| `gyo_app_project` 與 `gyo_app_add_*`、生成的身分標頭 | 用。複製遊戲時只改目錄名與登記列，身分與資產根從目錄名推出（AGENTS §7 Add／Clone） | 方案 B 改用詞 |
| `gyo_product_units`（gyo2 新加，產品帶哪些登記單位） | 用 | 不動；和產品登記、打包的互動在第一個真實產品接上時核（編排骨架宣告 :711），屬 Object_Connect 移植那批 |
| Go service 機制（`gyo_app_add_go_service`、`GyoGo.cmake`） | 「機制留」（carry list :274）；gyo2 目前沒有任何 `go.mod` | 只跟著改用詞（B） |
| shader bundle | gyo2 沒有 pipeline，宣告就報錯（`GyoContent.cmake`） | 不動 |
| 產品專屬測試與驗收放在 `tests/<產品>/`、`build/acceptance/<產品>/` | 使用者沒定；AGENTS §7 允許產品的東西分散在不同目錄，只要認得出 owner | 不搬 |

---

## 2. 方案（交使用者選一個）

| | A：只換路徑 | B：路徑＋用詞（建議） |
|---|---|---|
| 做什麼 | 第 1.1 節的 `apps` 改成 `games`；`GYO_APPS`、`projects.csv`、`gyo_app_*`、`app_registry.py` 等用詞照舊 | A 的全部，加上建置層與 CI 裡指「遊戲」的 `app` 一律改成 `game`（下表） |
| 系譜 | master 的形，只換目錄名 | 同詞異義表已記：建置的 KIND app 與 `apps/` 是 App 的同詞異義之一（`rebuild-vocabulary` :223），建置的改名 game 或 product（:302），「App」在 gyo2 不用（:56）；AGENTS §6：產品分 Game 與 Tool |
| 留下的 | 目錄叫 `games/`，選它的變數叫 `GYO_APPS`、登記檔叫 `projects.csv`，同一個概念兩個名字 | 沒有 |
| 碰到的檔 | 約 20 個（建置 5、守衛 3＋自測、CI 測試、假樹測試、突變） | 約 45 個（再加 CI 的 Python 與兩個 workflow、打包清單、`kernel_guard`、release 腳本與測試） |
| 估時 | 約 3 小時（含 L1） | 約 6 小時（含 L1） |

退掉 A 的理由：留下同詞異義表已點名要改的名字；改名越晚、積上去的產品越多越貴（Object_Connect 就是下一批）。B 的費用多在機械改名與 workflow，不改任何判定的意思。

**支撐的 Feature 與產品**：Object_Connect 移植（SDL 宣告的 Q5：使用者點名 Object_Connect，需求併進 R2b）放進 `games/object_connect/`；之後的工具（SDL 宣告提到的 KeepTool）放進 `tools/<工具>/`。兩者都要建置層認得目錄、守衛掃得到。

### 2.1 B 的改名表

| 舊 | 新 |
|---|---|
| `apps/<遊戲>/` | `games/<遊戲>/` |
| `config/projects.csv`（JSON 裡的集合 `projects`） | `config/games.csv`（集合 `games`）；欄位不變 |
| cache 變數 `GYO_APPS`、`GYO_REGISTRY_FILE` | `GYO_GAMES`、`GYO_GAME_REGISTRY_FILE`（對齊 `GYO_TOOLS`、`GYO_TOOL_REGISTRY_FILE`） |
| `GyoApps.cmake`、`GyoAppRegistry.cmake`、`GyoAppProject.cmake`、`ExportAppRegistry.cmake` | `GyoGames.cmake`、`GyoGameRegistry.cmake`、`GyoGameProject.cmake`、`ExportGameRegistry.cmake` |
| 函式與屬性 `gyo_app_*`、`GYO_APP_*`、`GYO_ACTIVE_APPS`、`GYO_CURRENT_APP` | `gyo_game_*`、`GYO_GAME_*`、`GYO_ACTIVE_GAMES`、`GYO_CURRENT_GAME` |
| target 名 `gyo_<id>`、`gyo_<id>-<role>` | 不變 |
| 產品種類 `KIND app`（安裝、打包清單的 `kind`、`assemble_runtime.py --kind`） | `game`；打包清單 `schema_version` 3→4（列舉值改名，舊讀者讀不了，所以升版；讀者都在 repo 內、gyo2 沒有發過的套件，不留相容分支） |
| 工具描述 `project.json` 的 `requires_apps`（version 1） | `requires_games`（version 2）；gyo2 沒有任何工具描述，只有測試的假樹 |
| `build/ci/common/app_registry.py`、矩陣的鍵 `app`、`tests/common/ci/test_app_registry.py` | `game_registry.py`、`game`、`test_game_registry.py` |
| `tests/common/ci/app_copy_integration.py`、fixture `copy_game/app/`、workflow 步驟 `app_copy` | `game_copy_integration.py`、`copy_game/game/`、`game_copy` |
| ctest `cmake.AppRegistry` | `cmake.GameRegistry` |
| 生成的 `gyo/AppConfig.hpp`，`namespace Gyo::AppConfig { Id, DisplayName, Assets, ContentManifest }` | `gyo/GameConfig.hpp`，巨集 `GYO_GAME_ID`、`GYO_GAME_DISPLAY_NAME`、`GYO_GAME_ASSETS`、`GYO_GAME_CONTENT_MANIFEST`（見下） |

**生成的身分標頭**（卡片照命名規範判）：現在的 `Gyo::AppConfig` 有兩處不合規範——縮寫一大一小（N17 縮寫表：要嘛全大寫、要嘛全小寫），以及用了「App」。可走的路：(i) 改成巨集，照 Cargo 由建置注入套件身分的做法（`env!("CARGO_PKG_NAME")`），不打開任何 namespace ←選；(ii) 新根 `GYO::Game`——要改 N17 的封閉表與守衛 `GYO_ROOTS`，而且 N17 寫「產品不打開 `GYO::` 底下的 namespace」，正相反；(iii) 用產品自己的根——複製遊戲時 namespace 跟著變，複製的程式就編不過（AGENTS §7 Clone）。巨集前綴 `GYO_` 全大寫，同 `GYO_ASSERT`。

---

## 3. Architecture Delta 的 7 項（以 B 寫；A 是其中的路徑部分）

1. **需要它的 Feature**：Object_Connect 移植進 `games/object_connect/`；之後的工具進 `tools/<工具>/`。頂層目錄是使用者 2026-11-23 定的。
2. **現在的問題**：建置只認 `apps/`，`games/<遊戲>/` 的程式不會被建置；三個守衛與 workflow gates 不掃 `games/`（第 1.1 節的缺口）。建置與 CI 用「app」指遊戲，是同詞異義表點名的舊名。
3. **變的邊界**：產品登記（Product Registration）的目錄規則：遊戲＝`games/<登記名>/`、工具＝`tools/<登記名>/`（不變）、資產＝`assets/<登記名>/`（不變）。B 另改遊戲登記檔的檔名、選取的 cache 變數、打包清單的 `kind` 值與版本、工具描述的鍵與版本。
4. **影響的 Module／Product／Tool**：Engine 與 plugins 不受影響（建置層之外）。目前沒有任何登記的遊戲或工具（兩份登記檔只有表頭），所以沒有產品要搬。受影響的是建置層（`build/cmake`）、CI（`build/ci/common`、兩個 workflow）、守衛（naming、os、kernel_guard、workflow gates）與它們的測試。
5. **依賴方向**：不變。仍是 Game／Tool → Engine；建置層讀登記檔、`add_subdirectory` 產品目錄；Engine 不參照產品。
6. **Ownership**：不變。`games/<遊戲>/` 歸該遊戲；登記檔歸建置層的產品登記；守衛的掃描根歸各守衛的 owner。
7. **為什麼不能更小**：A 已是最小的路徑改法；B 多出的部分是用詞，理由在第 2 節。只改 `GyoBuild.cmake` 不改守衛，`games/` 的程式會躲過三個守衛（AGENTS §4：局部不等於責任正確）。

---

## 4. `assets/<產品>/` 的接合點（本批不改）

| 段 | 現在怎麼接 | owner |
|---|---|---|
| 來源 | `assets/<登記名>/`，和 `games/<登記名>/` 同名；內容根目錄的入口是 `content.json` | 產品擁有內容；`content.json` 與 catalog 的格式、Version、驗證規則歸 asset（`asset.zh-Hant.md` 第 49～99 行），其中 `shader_bundles` 的內容歸 render |
| 建置讀的 | 只讀 `content.json` 的 `version` 與 `shader_bundles` 個數（`GyoContent.cmake`；有 shader bundle 就報錯，因為沒有 pipeline） | 建置層 |
| 組裝 | `build/assemble_runtime.py` 照 `build/content_contract.py`（asset 契約在建置側的鏡像，和 C++ 端用 `tests/common/fixtures/asset_contract` 的共用 fixture 保持一致）複製到 stage 的 `bin/assets/<登記名>/`；`cmake --install` 也跑同一個 | 建置層 |
| 執行時 | 生成的身分標頭給相對路徑 `assets/<登記名>`、`assets/<登記名>/content.json`；產品把它交給 asset 的 `AssetStore` 當內容根目錄 | 產品組裝（setup）；asset 只看內容根目錄 |

本批碰到的只有：路徑規則的說明、B 的 `--kind app`→`game`（組裝器的參數，不是 asset 契約）、身分標頭改成巨集（值不變）。`content.json`、catalog 的格式與 Version、`content_contract.py` 的規則都不改。

**工具的資產**：使用者定的 `assets/<產品>/` 包含工具，但現在只有遊戲的主執行檔會掛內容（`gyo_attach_content` 只在 `gyo_app_add_executable(... MAIN)` 裡），工具沒有 `assets/<工具>/` 的組裝。現在沒有點名要資產的工具，所以本批不做；第一個要資產的工具出現時，把同一個組裝開給工具（不動 asset 契約）。

---

## 5. 影響的守衛、突變、CI 測試

### 5.1 守衛的判定改動（AGENTS §14.4）

下面四處都是「掃描根的清單」把 `apps` 換成 `games`。naming 與 os 兩個是 Design Invariants 的守衛，照 §14.4 不論鬆緊都交使用者核准；workflow gates 一併列出。

| | naming `SCAN_ROOTS` | os `ROOTS`／`RUNTIME_ROOTS`／`GYO_OS_DEFINITION_ROOTS` | workflow gates 的產品目錄 | `kernel_guard` `REGISTRIES`（只 B） |
|---|---|---|---|---|
| 1 守的性質 | 建置讀的每個產品目錄裡的 C++ 不打開 `GYO::` 的根（N17） | 建置讀的每個產品目錄裡，平台分支與檔案 I/O 只在 OS 層（os.md DI 1） | 共通 CI 定義不寫任何產品或工具的名字 | 登記的產品名不進 Kernel（K1） |
| 2 緩的面 | `apps/` 底下的檔不再掃。改完後建置不讀 `apps/`，所以被建置的檔不會漏掉 | 同左 | `apps/` 底下的目錄名不再當產品名 | 只讀 `games.csv`，`projects.csv` 不再讀；改完後它不存在 |
| 3 緊的面 | `games/` 底下開始掃（現在不掃） | 同左 | `games/` 底下開始取名；`games/object_connect/` 已存在，這條從 skip 變成實際執行 | 無 |
| 4 緩的面誰在守 | 同一守衛的新清單：「建置讀哪些目錄」和「守衛掃哪些目錄」改完後再次一致 | 同左 | 同左 | 同左 |
| 5 證據 | 改後的 `GyoBuild.cmake`；自測的釘住值跟著改；突變 `naming_mutations.json:356` 改 `find` 後仍 killed | 改後的 `GyoBuild.cmake`；自測 `:223-224`、`:194-214` 的 `apps` 改 `games`；相關突變仍 killed | `games/object_connect` 存在時這條實際跑過並通過（CI 定義裡沒有 `object_connect`） | `test_kernel_guard.py` 的假登記檔跟著改；`ks-k1-registry-ignored` 仍 killed |

`GyoCodePage.cmake:27` 的根是 DI 11 的實作（不是測試），同樣換成 `games`。

### 5.2 突變（D184：只跑相關的）

- 改 `find`／`replace` 字串（意思不變）：`naming_mutations.json:356`；`os/guard_mutations.json:295`（`bg-engine-allowlist-scope` 的 replace 裡的 `apps/`）；`plans/os-subsystem/scripts/mutations.json` 的 `os-g11-roots-skip-products`（:392）、`RUNTIME_ROOTS` 那一個（:454）、code page 根那一個（:1451）；B 另有 `tests/common/ci/r0b3_mutations.json` 指 `app_registry.py` 的幾個（:198、:217 與測試名）。
- 要跑的：上面這些，加上 `find` 落在本批改到的檔的、殺它的測試被本批改到的。
- 新加的突變：無。本批不加新的判定，只換路徑與名字（§14.5）。

### 5.3 會改的 CI／測試

ctest `build.ci`（`tests/common/ci` 全部）、`cmake.AppRegistry`（B 改名 `cmake.GameRegistry`）、`cmake.ToolRegistry`、`cmake.ProjectComposition`、`cmake.GoService`、`naming.guard*`、`os.*` 的守衛自測、`kernel.guard*`（B）、`tests/common/assembly`（B 的 `--kind`）、`tests/common/json/test_json_guard.py` 的一行假資料（B）。workflow：`build-and-validate.yml`（`-DGYO_APPS`、`app_registry.py`、`app_copy` 步驟，B）、`prepare-release.yml` 的說明字串（B）。`CMakePresets.json` 的 `GYO_APPS`（B）。

---

## 6. 觀測到的 Pressure（只報告，本批不改）

1. **「我們的原始碼在哪些頂層目錄」手寫在 6 處**：`GyoBuild.cmake`（`add_subdirectory`）、`GyoCodePage.cmake:27`、`os_boundary_guard.py:31-32`、`tests/common/os/CMakeLists.txt:158`、`naming_guard.py:63`、`test_workflow_gates.py:1162`。AGENTS §9「同一個事實只在一處手動維持」；這次改一個目錄名要逐處改，就是證據。各守衛把清單釘死是刻意的（防止守衛悄悄縮小），收成一處要保留這一點。本批照現狀逐處改；要不要收成一處，交使用者另排。
2. **`plugins/` 不在 code page 的根**（`GyoCodePage.cmake:27`），也不在 os 守衛的根：Plugin 裡若有執行檔，不會套 UTF-8 manifest、平台分支不會被查。目前 `plugins.csv` 沒有列；SDL 那批第一個 Plugin 進來前要處理（轉 SDL 卡／主會話）。
3. **README 三語版與 `docs/creating_apps.md` 大多是 master 的內容**（`object_fps_pvp`、`services/gyo_gateway`、shader）。本批只改講目錄、登記檔名、選取變數的那幾行；其餘交圖書管理員。

---

## 7. 步驟、驗證、模型與檔位

| 步 | 內容 | 模型／檔位 |
|---|---|---|
| 1 | 路徑：建置層 5 處、守衛 3 個與自測、workflow gates、假樹測試、突變字串 | 實作 Opus／high |
| 2 | 用詞（只 B）：第 2.1 節的改名表 | 實作 Opus／high（機械改名的部分照表做） |
| 3 | 文件：README 三語版與 `creating_apps.md` 的目錄與名字那幾行、`docs/architecture/README.md:56` 的登記檔名（B）、本宣告的完成紀錄、dev_log | Opus／medium |
| 審查 | 合併前的 code review | Opus／high（和實作者不同的 Agent） |

上段（碰到守衛與 Product Registration 的架構判斷），不用小模型。

驗證：本機 macOS arm64 全新 configure＋全量建置＋`ctest`；第 5.2 節的突變；draft PR 到 `gyo2`，手動 dispatch L1（linux-x64、windows-x64、macos-arm64）。另外確認一件：一個合成的遊戲放在 `games/` 下能被選到、建出、組裝資產（`cmake.ProjectComposition` 與「獨立複製遊戲」步驟涵蓋）。

**本宣告的敵對式檢查**（AGENTS §4：基盤的選項交使用者前，要由和作者不同的模型檢查系譜、退掉的選項與決定理由）：作者是 Opus，檢查者用 Fable／high。Fable 比主會話大，先經主會話問使用者。

## 8. 不碰

`engine/`、`plugins/`；asset 的契約（`content.json`、catalog、`content_contract.py` 的規則）；元件對照表（`GyoComponents.cmake`、`project.json` 的 `components`）；Go 機制的邏輯；shader；`gyo_product_units`；`tests/<產品>/`、`build/acceptance/<產品>/` 的位置；`games/object_connect/`、`assets/object_connect/`、`tools/` 裡的 `.gitkeep`。

## 9. 交使用者決定的

- **Q1**：A（只換路徑）還是 B（路徑＋用詞，建議）。
- **敵對式檢查**：用 Fable／high 檢查本宣告，可以嗎？
- 第 5.1 節四處守衛判定的改動（核准宣告即核准方向；合併前核准頁再逐判定列一次）。
