# 建置層認 `games/`：事前宣告

- 批次代號：`v8-arch-gyo2-build-games-dir`（批次表〈後續小事〉「建置層仍寫死 master 的 `apps/`」那一列）
- 狀態：**修訂 1，等動工前核准**。核准前不動程式。
- 基準：`gyo2` 0ed6ea10
- 依據的決定：`v8-arch-gyo2-top-dirs-games-assets-tools`（頂層 `games/<遊戲>/`、`assets/<產品>/`、`tools/<工具>/`）、`v8-arch-gyo2-object-connect-dirs-now`（Object_Connect 的目錄已用 `.gitkeep` 建好）
- 為什麼單獨一批：批次表那一列與 `-object-connect-dirs-now` 原寫「移植 Object_Connect 那一批一起改」；主會話 2026-11-23 依使用者「開 1、2、3」把它開成這張任務卡，先做。本宣告把「產品登記機制的重審」留在移植那批（第 2 節 C）。

一句話：建置層與掃描它的守衛，把遊戲的原始碼目錄從 master 的 `apps/` 改成 `games/`；工具本來就在 `tools/`，不用改；`assets/<產品>/` 的接合點不變，在第 4 節寫明。

**修訂紀錄**
- 修訂 0（1782ad6b）：建議 B（路徑＋用詞 app→game）。
- 修訂 1：敵對式檢查 r0（Opus／high；作者也是 Opus，因 Fable 用量不足，使用者經主會話指示「暫時用Opus」；B5 S11 N6，全部採用，對照在第 10 節）。**建議改成 A**：B 要改名的那套登記機制在 carry list 上是「帶過去並重審」、還沒重審，先改名就是在為舊形狀投資（第 2 節）。補上漏列的守衛 `ProcessCodePageCheck.cmake` 與 os.md 的 DI 條文、驗證碰不到 `GyoBuild.cmake` 的問題、§4 的錯誤描述。

---

## 1. 現況（讀過的程式）

### 1.1 寫死 `apps` 的地方（`git grep -P '\bapps\b'`，正例命中 `GyoBuild.cmake:68`）

| 種類 | 位置 | 做什麼 |
|---|---|---|
| 建置 | `build/cmake/GyoBuild.cmake:68-69`、`:90-91` | 對每個選到的遊戲 `add_subdirectory(apps/<遊戲>)`，以及它的 `tests/<遊戲>/`、`build/acceptance/<遊戲>/` |
| 建置 | `build/cmake/GyoApps.cmake:38` | 讀 `apps/<遊戲>/project.json` |
| 建置 | `build/cmake/GyoAppRegistry.cmake:261` | 檢查選到的遊戲有 `apps/<遊戲>/CMakeLists.txt` |
| 建置 | `build/cmake/GyoCodePage.cmake:27`（註解 :4） | Windows 執行檔套 UTF-8 manifest 的根（os.md DI 11 的實作） |
| 守衛 | `tests/common/os/ProcessCodePageCheck.cmake:19`（註解 :2） | DI 11 的獨立檢查；檔頭 :6-8 說它刻意不沿用 `GyoCodePage.cmake` 的記錄，好看見建置層忘了記的目錄 |
| 守衛 | `tests/common/naming/naming_guard.py:63` `SCAN_ROOTS`（docstring :38-40；自測 `test_naming_guard.py:274-280`、`:313` 釘住；`tests/common/naming/CMakeLists.txt:3` 註解） | N17：產品的 C++ 不打開 `GYO::` 的根 |
| 守衛 | `tests/common/os/os_boundary_guard.py:31-32` `ROOTS`、`RUNTIME_ROOTS`（自測 `test_os_boundary_guard.py:122-124`、`:194-214`、`:223-224`、`:296`、`:340-377`） | os.md DI 1、DI 2 |
| 守衛 | `tests/common/os/CMakeLists.txt:158` `GYO_OS_DEFINITION_ROOTS` | 同上，定義的掃描根 |
| CI 測試 | `tests/common/ci/test_workflow_gates.py:1162`、`:1166-1167`（訊息）、`:1177`（比對式 `(?:apps|tools|services)/{name}`） | 共通 CI 定義不寫產品名（AGENTS §7、§9） |
| DI 條文 | `docs/architecture/os.md:387`（DI 1）、`:392-393`（DI 2）、`:181`（Contracts 的 code page 根）、`:306`、`:318` | 條文本身寫了掃描根 |
| 合成樹的測試 | `tests/common/cmake/AppRegistryTests.cmake`、`ProjectCompositionTests.cmake`、`GoServiceTests.cmake`、`ToolRegistryTests.cmake:140-146`；`tests/common/ci/app_copy_integration.py:32`、`test_app_copy_integration.py`、`test_go_checks.py`、`test_release_channels.py`；`tests/common/kernel/test_kernel_guard.py:587` | 假樹裡用 `apps/<名>`（後三個只是合成的路徑資料） |
| 突變 | `docs/architecture/target/v2/scripts/naming_mutations.json:356-357`、`tests/common/os/guard_mutations.json:295`、`docs/architecture/plans/os-subsystem/scripts/mutations.json:392`、`:454`、`:1451-1452` | `find` 或 `replace` 字串裡有 `apps` |
| 說明文件 | README 三語版、`docs/creating_apps.md` | 講目錄與複製遊戲的那幾行 |

`tools/` 已經是 `tools/<工具>/`（`GyoTools.cmake:25`、`GyoBuild.cmake:76`、`GyoToolRegistry.cmake:232`），路徑不用改。

**現在就有的覆蓋缺口**：`games/object_connect/` 已存在，但上面四個守衛與 workflow gates 都不掃 `games/`；workflow gates 目前因為「`apps/`、`tools/`、`services/` 底下沒有目錄」在 skip。現在目錄裡只有 `.gitkeep`，所以還沒漏掉東西；Object_Connect 的程式一放進來就會漏。

### 1.2 master 的產品登記機制，在 gyo2 的去留（只判本批會碰到的）

| 機制 | gyo2 還用不用 | 本批 |
|---|---|---|
| 產品登記檔 `config/projects.csv`、`config/tools.csv`（名字、enabled、平台；N8「產品登記檔」）；放頂層 `config/`，比照 Cargo workspace 根清單與 `go.work`（`v8-arch-gyo2-config-top-level`） | 用；`kernel_guard.py:105` 也讀。兩份都只有表頭 | 不動 |
| `project.json` 的 `components`（`GyoComponents.cmake`） | 元件對照表「帶過去並重審」（`rebuild-r0-carry-list` :61），屬 SDL 那條線 | 不動 |
| `gyo_app_project`、`gyo_app_add_*`、生成的 `Gyo::AppConfig` 標頭、KIND app、`app_registry.py`、複製整合 | `GyoBuild.cmake` 一系「帶過去並重審」（carry list :61），**還沒重審** | 只改路徑；重審與用詞見第 2 節 C |
| `gyo_product_units`（gyo2 新加） | 用 | 不動；和產品登記、打包的互動在第一個真實產品接上時核（編排骨架宣告 :711） |
| Go service 機制 | 「機制留」（carry list :274）；gyo2 沒有任何 `go.mod` | 只改假樹路徑 |
| shader bundle | 沒有 pipeline，宣告就報錯 | 不動 |
| 產品專屬測試與驗收在 `tests/<產品>/`、`build/acceptance/<產品>/` | 使用者沒定；AGENTS §7 允許分散在不同目錄，只要認得出 owner | 不搬 |

---

## 2. 方案

**基盤還是應用**：基盤。Product Registration 與建置的共通結構是「再利用可能な層の共通構造」（AGENTS §2）：每個產品都積在它上面，改它要改每個產品的登記與 CMake。

| | A：路徑與守衛（建議） | B：A＋用詞 app→game | C：A＋重審產品登記機制 |
|---|---|---|---|
| 做什麼 | 第 1.1 節的 `apps` 改成 `games`，補齊守衛與 DI 條文；登記機制的形與名字不動 | A 的全部，加上建置層與 CI 裡指遊戲的 `app` 一律改成 `game`（`projects.csv`→`games.csv`、`GYO_APPS`→`GYO_GAMES`、`gyo_*app*`→`gyo_*game*`、KIND、打包清單版本、`requires_apps`、`app_registry.py`、身分標頭…） | A 的全部，再從 Feature（Object_Connect）與成熟做法（Cargo workspace 的 `members` 與套件身分 `env!("CARGO_PKG_NAME")`、Bevy 的產品組裝）重新推產品登記要什麼、留什麼，用詞在那時一起定 |
| 系譜 | 使用者定的新目錄；機制原樣 | master 的機制原樣，只換名 | 新推導 |
| 「是不是在為舊形狀找位置」 | 否：把產品放到使用者定的新位置，不往舊機制加東西 | **是**：在還沒重審的舊機制上花約 55～60 個檔的改名，重審後被拿掉的部分就白改 | 否 |
| 碰到的檔 | 約 32 個（含 os.md、兩份突變清單） | 約 55～60 個 | 不定 |
| 估時 | 約 4 小時（含 L1） | 約 7 小時 | 要先有 Feature 的需求，屬移植那批 |
| 支撐的 Feature | Object_Connect 移植（使用者在 SDL 宣告 Q5 點名、需求併進 R2b）放進 `games/object_connect/`；之後的工具（KeepTool，HANDOFF :56）放進 `tools/<工具>/` | 同左 | Object_Connect 移植本身 |

**建議 A，C 排進 Object_Connect 移植那批**（批次表原本就寫「移植那一批一起改」）。退掉 B 的理由：用 AGENTS §4 與 target/README〈每批事前宣告的檢查項〉的問法，B 是在舊形狀上投資。退掉「本批就做 C」的理由：C 要從第一個真實產品推需求（Derivation Order：Feature 先），現在沒有產品的程式。A 留下的代價：目錄叫 `games/`，選它的變數仍叫 `GYO_APPS`、登記檔仍叫 `projects.csv`，到 C 為止同一概念兩個名字；在批次表記一列，C 必須處理。

用詞的依據（給 C 用）：使用者 2026-11-14 回同詞異義表 App 列「這個可以換掉，用 Game 就好，產品實際上有 2 種，一種是 Game，另一種是 Tool」（`rebuild-vocabulary` :25-27）。修訂 0 引的 :302 是底稿的建議欄，v2 欄寫的是「名字保留、不定義內容」，引錯了。

**給 C 的已知事項**（檢查 r0 找到的，本批不改）：`GyoAppRegistry.cmake` 裡有 modules、plugins、tools 共用的 CSV／JSON 解析（:4、:18、:26、:116，`GyoUnitRegistry.cmake:32`、`GyoToolRegistry.cmake:2` 都 include 它），整檔改名成 Game 會讓名字指錯概念，要拆出中性的檔；生成的 `Gyo::AppConfig` 不屬於 N17 表裡任何根、`Id` 等常數不合 N15，可走的路有建置注入巨集（N15：`GYO_` 開頭全大寫）、`target_compile_definitions` 直接注入（最接近 Cargo `env!`）、新根（N17 的根要和原始碼目錄一一對應，生成的標頭在 build 目錄，對不上）。

---

## 3. Architecture Delta 的 7 項（方案 A）

1. **需要它的 Feature**：Object_Connect 移植進 `games/object_connect/`；之後的工具進 `tools/<工具>/`。頂層目錄是使用者 2026-11-23 定的。
2. **現在的問題**：建置只認 `apps/`，`games/<遊戲>/` 的程式不會被建置；四個守衛與 workflow gates 不掃 `games/`（第 1.1 節的缺口）。
3. **變的邊界**：產品登記的目錄規則：遊戲＝`games/<登記名>/`、工具＝`tools/<登記名>/`（不變）、資產＝`assets/<登記名>/`（不變）。os.md DI 1、DI 2、DI 11 相關條文的掃描根文字跟著改。
4. **影響的 Module／Product／Tool**：Engine 與 plugins 的程式不受影響。目前沒有任何登記的遊戲或工具，沒有產品要搬。受影響的是建置層（`build/cmake`）、守衛（naming、os、DI 11 的檢查、workflow gates）、它們的測試、os.md 的條文。
5. **依賴方向**：不變。Game／Tool → Engine；建置層讀登記檔、`add_subdirectory` 產品目錄；Engine 不參照產品。
6. **Ownership**：不變。`games/<遊戲>/` 歸該遊戲；登記檔歸建置層的產品登記；守衛的掃描根歸各守衛的 owner；os.md 的條文歸 OS。
7. **為什麼不能更小**：只改 `GyoBuild.cmake` 不改守衛，`games/` 的程式會躲過四個守衛（AGENTS §4：局部不等於責任正確）；條文寫著掃描根，守衛改了條文不改就不一致。

---

## 4. `assets/<產品>/` 的接合點（本批不改）

| 段 | 現在怎麼接 | owner |
|---|---|---|
| 來源 | `assets/<登記名>/`，和 `games/<登記名>/` 同名；內容根目錄的入口是 `content.json` | 產品擁有內容；`content.json` 與 catalog 的格式、Version、驗證規則歸 asset（`asset.zh-Hant.md` 第 49～99 行），`shader_bundles` 的內容歸 render |
| 建置讀的 | `content.json` 的 `version` 與 `shader_bundles` 個數（`GyoContent.cmake:24`；有 shader bundle 就報錯，因為沒有 pipeline） | 建置層 |
| 組裝 | `build/assemble_runtime.py` 照 `build/content_contract.py` 驗證並複製到 stage 的 `bin/assets/<登記名>/`；`cmake --install` 也跑同一個 | 建置層 |
| 執行時 | 生成的身分標頭給 `assets/<登記名>`、`assets/<登記名>/content.json`，是**相對執行檔目錄**的路徑；`AssetStore` 會把相對的內容根對 CWD 轉成絕對路徑（asset 頁第 4 節），所以產品要先接上執行檔目錄（fixture 用 `argv[0]` 的 parent；Engine 有 `OS::Process::ExecutablePath`），再交給 `AssetStore` | 產品組裝（setup）；asset 只看內容根目錄 |

本批只改路徑規則的說明，`content.json`、catalog、`content_contract.py`、組裝器都不碰，不觸及 asset 的 Data Contract。

**工具的資產**：使用者定的 `assets/<產品>/` 包含工具，但現在只有遊戲的主執行檔會掛內容（`gyo_attach_content` 只在 `gyo_app_add_executable(... MAIN)` 裡）。現在沒有點名要資產的工具，本批不做；第一個要資產的工具出現時，把同一個組裝開給工具（不動 asset 契約）。

---

## 5. 影響的守衛、突變、CI 測試

### 5.1 守衛判定與 DI 條文的改動（AGENTS §3、§14.4）

都是「掃描根的清單」把 `apps` 換成 `games`。naming（N17）、os（DI 1、DI 2）、`ProcessCodePageCheck`（DI 11）是 Design Invariants 的守衛，照 §14.4 不論鬆緊都交使用者核准；os.md 的 DI 條文改字是 §3 的 DI 節變更，也要同意。

| | naming `SCAN_ROOTS` | os `ROOTS`／`RUNTIME_ROOTS`／`GYO_OS_DEFINITION_ROOTS` | `ProcessCodePageCheck.cmake:19` | workflow gates `:1162`、`:1177` |
|---|---|---|---|---|
| 1 守的性質 | 建置讀的產品目錄裡的 C++ 不打開 `GYO::` 的根（N17） | 建置讀的產品目錄裡，平台分支與檔案 I/O 只在 OS 層（DI 1、DI 2） | 建置出的每個 repo 自己的 Windows 執行檔都帶 UTF-8 manifest（DI 11） | 共通 CI 定義不寫產品或工具的名字 |
| 2 緩的面 | `apps/` 底下的檔不再掃。改完後建置不讀 `apps/`，被建置的檔不會漏 | 同左 | `apps/` 底下的執行檔不再查。同左 | `apps/` 底下的目錄名不再當產品名；`apps/<名>` 的寫法不再比對 |
| 3 緊的面 | `games/` 底下開始掃 | 同左 | 同左 | `games/` 底下開始取名、`games/<名>` 開始比對；`games/object_connect/` 已存在，這條從 skip 變成實際執行 |
| 4 緩的面誰在守 | 同一守衛的新清單：「建置讀哪些目錄」和「守衛掃哪些目錄」再次一致 | 同左 | 同左 | 同左 |
| 5 證據 | 改後的 `GyoBuild.cmake`；自測釘住值跟著改；突變 `nm-scan-roots-examples` 改 `find` 後仍 killed | 改後的 `GyoBuild.cmake`；自測跟著改；`os-g11-roots-skip-products` 等仍 killed | 同左；code page 根的突變（os-subsystem :1451）仍 killed | 檢查 r0 已確認 `.github` 與 `build/ci/common` 裡 `object_connect` 0 處，實際執行會通過 |

`GyoCodePage.cmake:27` 是 DI 11 的實作（不是測試），同樣換成 `games`。os.md 的改字：`:181`、`:306`、`:318`、`:387`、`:392-393` 的 `apps` 換成 `games`，意思不變。

### 5.2 突變（D184：只跑相關的）

- 改 `find`／`replace` 字串（意思不變）：`naming_mutations.json:356-357`；`os/guard_mutations.json:295`（`bg-engine-allowlist-scope` 的 replace）；`plans/os-subsystem/scripts/mutations.json` 的 :392、:454、:1451-1452。
- 要跑的：上面這些，加上 `find` 落在本批改到的檔的、殺它的測試被本批改到的。
- 新加的突變：無。本批不加新的判定，只換路徑（§14.5）。

### 5.3 會改的 CI／測試

ctest `build.ci`（`tests/common/ci`）、`cmake.AppRegistry`、`cmake.ToolRegistry`、`cmake.ProjectComposition`、`cmake.GoService`、`naming.guard*`、`os.*` 的守衛自測與 code page 檢查、`kernel.guard_selftest`（一行假資料）。workflow 不改。

### 5.4 驗證怎麼碰到 `GyoBuild.cmake` 的新路徑

`cmake.ProjectComposition` 自己寫 `add_subdirectory("apps/...")`，不經過 `GyoBuild.cmake`；「獨立複製遊戲」步驟只在 release profile 跑（`build-and-validate.yml:168`），L1 是 quick，而且它在 gyo2 上已經壞了：mirror（`app_copy_integration.py:17-18`）沒有複製 `config/`，`GyoBuild.cmake:55-56` 讀不到 `modules.csv` 就 FATAL。所以：

- `app_copy_integration.py` 的 mirror 加上 `config/`（修這個既有的壞處是為了驗證本批）；
- 本機 macOS arm64 實際跑一次複製整合（它把 fixture 複製成 `games/<名>/`、加登記列、從 repo 根 configure 與建置，經過 `GyoBuild.cmake`）；
- 跑不了的話，改用本機臨時的合成遊戲（不提交）從 repo 根 configure＋建置，並把這件寫成**範囲の限界**：L1 不經過 `GyoBuild.cmake` 的產品路徑。

---

## 6. 觀測到的 Pressure（只報告，本批不改）

1. **「我們的原始碼在哪些頂層目錄」手寫在至少 10 處**：`GyoBuild.cmake`（`add_subdirectory`）、`GyoApps.cmake:38`、`GyoAppRegistry.cmake:261`、`GyoCodePage.cmake:27`、`ProcessCodePageCheck.cmake:19`、`os_boundary_guard.py:31-32`、`tests/common/os/CMakeLists.txt:158`、`naming_guard.py:63`、`test_workflow_gates.py:1162`／`:1177`、`app_copy_integration.py:32`，另有 os.md 的條文。AGENTS §9「同一個事實只在一處手動維持」；這次改一個目錄名要逐處改，就是證據。`ProcessCodePageCheck` 刻意和 `GyoCodePage` 分開寫、各守衛把清單釘死，都是為了不讓守衛悄悄縮小，收成一處要保留這一點。本批照現狀逐處改；要不要收，交使用者另排。
2. **`plugins/` 與 `examples/` 不在 code page 的根**（`GyoCodePage.cmake:27`、`ProcessCodePageCheck.cmake:19`），也不在 os 守衛的根。`examples/` 現在就有執行檔（`examples/CMakeLists.txt` 的 `gyo_add_example`，`GYO_BUILD_EXAMPLES` 預設跟著 `BUILD_TESTING`），是 DI 11 現存的缺口；`plugins/` 目前沒有列，SDL 第一個 Plugin 進來前要處理。轉主會話與 SDL 卡。
3. **asset 契約在建置側的鏡像和 C++ 端已經分歧**：asset 頁寫 `shader_bundles` 可省，`content_contract.py:66-69` 要求它存在，`GyoContent.cmake:24` 缺它會失敗；Python 端不查 asset (A) 的規則 4、6、7；C++ 用 `tests/common/asset/contract/`，`tests/common/fixtures/asset_contract` 只有 Python 讀；`content_contract.py:3-5` 的 docstring 是 master 的說法。兩處維持同一事實而且已經不一樣，轉 asset 的 owner（主會話）。
4. **README 三語版、`docs/creating_apps.md`、`docs/architecture.md` 大多是 master 的內容**（`object_fps_pvp`、`services/gyo_gateway`、shader）。本批只改講遊戲目錄的那幾行；其餘交圖書管理員。

---

## 7. 步驟、驗證、模型與檔位

| 步 | 內容 | 模型／檔位 |
|---|---|---|
| 1 | 建置層 5 處、守衛 4 個與自測、workflow gates、假樹測試、突變字串、os.md 條文 | 實作 Opus／high |
| 2 | 複製整合的 mirror 加 `config/`，本機跑一次（第 5.4 節） | Opus／high |
| 3 | README 三語版與 `creating_apps.md` 的目錄那幾行、批次表（本列完成＋C 的一列）、dev_log | Opus／medium |
| 審查 | 合併前的 code review | Opus／high（和實作者不同的 Agent） |

上段（碰到守衛、DI 條文與 Product Registration），不用小模型。驗證：本機全新 configure＋全量建置＋`ctest`；第 5.2 節的突變；第 5.4 節；draft PR 到 `gyo2`，手動 dispatch L1（linux-x64、windows-x64、macos-arm64）。

## 8. 不碰

`engine/`、`plugins/`；asset 的契約（`content.json`、catalog、`content_contract.py`、組裝器）；元件對照表；Go 機制的邏輯；shader；`gyo_product_units`；登記機制的名字（`GYO_APPS`、`projects.csv`、`gyo_app_*`、`Gyo::AppConfig`、KIND app、`app_registry.py`）；workflow；`tests/<產品>/`、`build/acceptance/<產品>/` 的位置；三個 `.gitkeep`。

## 9. 交使用者決定的

- **Q1**：A（建議）／B／本批就做 C。選 A 的話，C（重審產品登記機制，含用詞 app→game）排進 Object_Connect 移植那批。
- 第 5.1 節四處守衛判定與 os.md DI 1、DI 2、DI 11 相關條文的改字（核准宣告即核准方向；合併前核准頁再逐判定列一次）。

## 10. 敵對式檢查 r0 的處理

檢查者 Opus／high，作者也是 Opus（同模型，盲點可能重疊；Fable 用量不足，使用者經主會話指示）。紀錄：主 checkout（`/Users/karasu/Code/Source/GYO-Engine`）的 `build/evidence/gyo2-build-games-dir/declaration-r0-adversarial.md`。

| 發現 | 處理 |
|---|---|
| B1 漏列 `ProcessCodePageCheck.cmake` | 列進第 1.1、5.1、6.1 節 |
| B2 os.md 的 DI 條文寫了 `apps` | 列進第 1.1、3、5.1、9 節 |
| B3 驗證碰不到 `GyoBuild.cmake`；複製整合在 gyo2 已壞 | 第 5.4 節 |
| B4 漏列 r0b3 指向 `GyoApps.cmake` 的突變 | 屬 B 的改名；改建議 A 後不改檔名，不受影響。`plans/os-subsystem` :1452 已補 |
| B5 整檔改名會讓共用解析叫 Game | 改建議 A；記進第 2 節「給 C 的已知事項」 |
| S1 同詞異義表引錯 | 第 2 節改引使用者原話 :25-27 |
| S2 workflow gates `:1177`、`:1166-1167` | 列進第 1.1、5.1 節；`:388`、`:701` 屬 B，不改 |
| S3 B 的改名表不完整 | 改建議 A，不做 B；記給 C |
| S4 檔案數低估、§1.1 漏項 | A 改成約 32 檔；第 1.1 節補 |
| S5 §4 鏡像描述錯 | 第 4 節改寫；分歧記進第 6.3 節 |
| S6 §4 執行時太簡化 | 第 4 節改寫 |
| S7 身分標頭的理由要修、漏了 `target_compile_definitions` | 記進第 2 節「給 C 的已知事項」 |
| S8 沒判基盤／應用；沒逐選項回答「為舊形狀找位置」；沒引成熟做法 | 第 2 節補判定與逐選項回答，加 C（Cargo workspace）；據此改建議 A |
| S9 單獨一批沒有出處 | 文首補 |
| S10 §6 要補（10 處、examples） | 第 6.1、6.2 節 |
| S11 DI 守衛不只兩個 | 第 5.1 節改寫（K1 屬 B，A 不改 `kernel_guard` 的登記檔） |
| N（行號、KeepTool 出處、文件、改名規則、workflow 數） | 行號更正；KeepTool 改引 HANDOFF :56；文件歸第 6.4 節；改名規則與 workflow 屬 B／C |
