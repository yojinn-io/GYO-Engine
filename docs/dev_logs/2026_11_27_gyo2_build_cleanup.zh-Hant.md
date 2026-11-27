# 2026-11-27 建置層清理（`v8-arch-gyo2-build-cleanup`）

Owner：建置層（GYO2 主會話轉來，任務卡）。作者 Opus 5.5／high。

處理批次表〈後續小事〉98、100、102、109、110 行（apps→games #294 留下的後續）；141 行（naming_guard 的 `SCAN_ROOTS` 沒有 `build/acceptance`）隨 98 行一起解消。事前宣告寫在 PR #368 的說明。

## 重現（動手前）

`ProjectCompositionTests.cmake` 用預設產生器（Unix Makefiles；macOS 的 GNU Make 3.81 以秒比較時刻）從 repo 根以 `-P` 跑 8 次：4 次失敗，都在 tool 的 `#error` 那一案（`:198`，「Invalid fixture unexpectedly succeeded」）——同一秒內改寫的原始碼沒被重建。紀錄在主 checkout `build/evidence/gyo2-build-cleanup/repro-composition/`。

## 改了什麼

### 1. 原始碼的頂層目錄收成一處（98 行、141 行）

- 新增 `build/cmake/GyoSourceRoots.txt`：一行一個根，`engine plugins games tools examples build/acceptance tests`。讀法只有兩份：CMake 的 `gyo_source_roots()`（`build/cmake/GyoSourceRoots.cmake`）、Python 的 `tests/common/support/source_roots.py`。
- 放在 `build/cmake/`：複製整合的 mirror 整個帶 `build/cmake/`，不會再出現 a8065853 那種「configure 讀的檔 mirror 沒帶」。
- 讀者：`GyoCodePage.cmake`（套 manifest）、`ProcessCodePageCheck.cmake`（DI 11 的獨立檢查）、`OsDefinitionCheck.cmake`（DI 1 的建置定義檢查）、`os_boundary_guard.py` 的 `ROOTS`、`naming_guard.py` 的 `SCAN_ROOTS`。`RUNTIME_ROOTS`、`REQUIRED_ROOTS` 是 os 守衛自己的規則，照舊寫在守衛裡。
- 「各守衛把清單釘死」：釘死收成一處——`ProcessCodePageTests.cmake` 最後比對清單內容。各守衛的自測改成確認「掃的就是共用清單」（`test_roots_name_no_product`、`test_n1_scan_roots_are_the_source_roots`），守衛自己從清單拿掉一個根就失敗。`ProcessCodePageCheck` 和套用的 walk 分開寫、從頂層目錄以 source path 再走一次，這兩點不變；共用的只有根。
- `GyoBuild.cmake` 的 `add_subdirectory` 是組裝（什麼條件加哪個目錄），不是清單；註解寫「加的目錄都在清單的根底下，新的頂層目錄先加進清單」。
- `test_workflow_gates.py` 的 `("games", "tools", "services")` 是產品的 owner 目錄，和「原始碼的根」是兩件事：改讀 `build/ci/common/plugin_removal.py` 的 `PRODUCT_KINDS`（CI 的 Python 側本來就有這一份），拿掉 gyo2 不存在的 `services`。
- 範圍只有收緊：`examples/` 進 os 守衛（R1～R3）、建置定義檢查（DI 1）、code page（DI 11）；`build/acceptance/` 進 naming 守衛。os.md DI 1 的字面加 `examples`（收緊；合併前核准時給使用者看）。實樹上 `examples/` 沒有命中。
- 順手：`OsDefinitionCheck.cmake` 檔頭寫「engine/os … may define them」，和程式（只豁免 `tests/common/os`，`:82-84` 的註解）不符，改成照程式寫。

### 2. `examples/` 進 DI 11（100 行）

由 1 的清單解決。Red：用 00738c36 的 `GyoCodePage.cmake`、`ProcessCodePageCheck.cmake`、`OsDefinitionCheck.cmake` 跑新的 `ProcessCodePageTests.cmake`，`plain: expected 'sample_example=UTF-8 none'` 失敗（主 checkout `build/evidence/gyo2-build-cleanup/red-examples-old-roots.log`）；改後通過。

### 3. os.md 與 os_boundary_guard（102 行）

不必改：SDL-P（c2185934）已把 os.md（現在 `:341-344`）改成「engine、tests 至少要有一個原始碼，其他根沒有就略過並回報」，和守衛（`os_boundary_guard.py:13-16`、`check()`）一致。只改批次表狀態。

### 4. 遊戲目錄規則、KIND、只寫不讀（109 行）

- `games/<名>` 收成 `gyo_game_directory(name output)`（`GyoGameRegistry.cmake`，`-P` 也能用；回傳相對路徑，原始碼與建置目錄各自加前綴）：`gyo_validate_game_sources`、`GyoGames.cmake` 的 `project.json`、`GyoBuild.cmake` 的第一個迴圈都用它。第二個迴圈的 `GYO_GAME_SOURCE_DIR`、`GYO_GAME_BINARY_DIR` 沒人讀，拿掉。
- KIND：`assemble_runtime.py` 的 `--kind`／`kind` 只檢查、不使用（呼叫者 `GyoContent.cmake` 三處都傳 `game`），拿掉。剩下寫的一方（`GyoRuntimeDeployment.cmake`，作者 API 的輸入）與讀 manifest 的一方（`package_contract.py`）；兩種語言、資料格式的兩端，不共用。
- 只寫不讀：`GYO_GAME_ASSET_ROOT`、`GYO_GAME_MAIN_TARGET`、`GYO_GAME_CONTENT_STAGE_TARGET`、CI 輸出 `has_games` 拿掉（`has_games` 的兩行斷言一起拿掉）。`GYO_GAME_GENERATED_INCLUDE_DIR` 有讀者（`ProjectCompositionTests.cmake:65`），留著。

### 5. `MirrorTests`、`ProjectComposition`、`sample_app`（110 行）

- `MirrorTests` 只留 Red 的回歸（帶 `config/modules.csv`、`config/plugins.csv`）與「不帶產品登記、產品、tests、CI」；mirror 拿掉不存在的 `services/`。突變 `gdir-mirror-drops-build-cmake` 退役（`r0b3_mutations.json` 的說明寫了理由）。
- `ProjectComposition` 的遊戲與 tool 的 `#error` 建置改 `--clean-first`：「選到的遊戲／tool 編譯失敗會讓整張圖失敗」和增量無關；增量看不看得到同一秒的改寫取決於產生器的時刻解析度。
- `sample_app`→`sample_game`、`other_app`→`other_game`（`tests/common/ci/` 五個檔）。

## 突變

- find 不見的 9 個改成「讀者從共用清單拿掉一個根」，殺它的測試不變：`os-g11-roots-skip-products`、`sdlp-roots-skip-plugins`（os 守衛）、`pu-x8-code-page-walk-skips-tests`、`gdir-code-page-walk-skips-games`、`sdlp-code-page-walk-skips-plugins`（walk）、`gdir-code-page-check-skips-games`、`sdlp-code-page-check-skips-plugins`（檢查）、`sdlp-definition-roots-skip-plugins`（建置定義）、`nm-scan-roots-examples`（naming）。
- 新增 7 個：`bcl-source-roots-drop-examples`、`bcl-source-roots-drop-build-acceptance`（清單本身；後者只有釘死能抓）、`bcl-code-page-walk-skips-examples`、`bcl-code-page-check-skips-examples`、`bcl-definition-roots-skip-examples`、`bcl-os-guard-skips-examples`、`bcl-naming-skips-build-acceptance`。
- 宣告寫的「`test_workflow_gates` 的 owner」突變沒做：能改的只有測試本身或 `PRODUCT_KINDS`，後者拿掉一個種類只會讓測試看的 owner 變少、照樣通過，不是這個測試守的性質。更正宣告。
- 退役 1 個：`gdir-mirror-drops-build-cmake`。
- 跑的範圍（D184）：檔案在改動範圍的、殺它的測試被改到的（os、naming、`ProcessCodePageTests`、workflow gates、product registry、game copy、`ProjectComposition`、assembly）。只改名的測試檔（release、archive、package checks、release retry）的 expect 都不含改到的名字，不跑。全部清單的 find 都恰好命中一次（靜態掃描）。
- 本機（Intel Mac，主 checkout `build/evidence/gyo2-build-cleanup/mutations/`）：os-subsystem 30/30、os 守衛 26/26、naming 25/25、`pu-x8`（要建置）1/1、r0b3 16/18；設計審查後的修正再跑 GyoBuild／GyoGames 上的 4 個與清單的 2 個，6/6。
- r0b3 沒殺的兩個在基準 00738c36 也一樣（既有）：`ci-shader-target-without-hash` survived、`ci-plan-no-notice` stale（find 已不在 workflow）。
- `sdlp-every-plugin-added`：wrong_failure。突變有被抓到（「Plugins added with none selected: [ "sdl", "sdl_window", "sdl_audio", "sdl_gpu" ]」），但 expect 只寫 `[ "sdl" ]`，之後加了三個 Plugin（既有，和本批無關）。

## 驗證

- Python：`tests/common/os` 49、`tests/common/naming` 54、`tests/common/ci` 274、`tests/common/assembly` 14，全部 OK；實樹 os 守衛、naming 守衛 clean。
- CMake 合成樹（Ninja）：ProjectComposition、GameRegistry、ToolRegistry、ProductPackaging、GoService、ProcessCodePage 通過。ProjectComposition 用預設產生器跑 8 次：8/8 通過（改前 4/8 失敗）。
- 實樹 `cmake --preset test` configure 通過（code page 與建置定義檢查都在 configure 時跑，含 `examples/`）。
- 複製整合（`game_copy_integration.py`，本機）通過：configure、build、install、兩個遊戲啟動；副本裡有 `build/cmake/GyoSourceRoots.txt`。
- L1：見 PR #368。
- 沒跑：本機全量建置與 ctest（交 L1 三平台）；`cmake.PluginSelection` 在本機的基準就失敗（父建置的依存原始碼借自別的 worktree，`_deps/*-src` 不在；00738c36 一樣），交 L1。

## 設計審查（Opus／high，唯讀；主 checkout `build/evidence/gyo2-build-cleanup/design-review.md`）

沒有 blocker。照改的：`gyo_game_directory` 改回傳相對路徑（省掉 `RELATIVE_PATH`）；`naming_mutations.json` 的舊測試名、os.md 記錄目錄的列舉補 `examples`、`GyoSourceRoots.txt` 檔頭不再逐一列讀者（會過時）；本 dev_log 補結果。

交使用者判斷（未改）：
1. `GyoBuild.cmake` 加了新的頂層目錄卻忘了加進清單時，所有讀者都會安靜地略過（`plugins/`、`examples/` 都發生過）。可選：`ProcessCodePageCheck` 回報「`SOURCE_DIR` 在 repo 內、但不在任何根、不在 `third_party/`、不在建置目錄」的目錄（Cargo 對 workspace 內既不在 `members` 也不在 `exclude` 的套件會拒絕；審查者憑記憶，未對照原始碼）。這是 DI 11「自己的原始碼」的檢查，不是擋誤用；要不要加由使用者定。
2. `test_roots_name_no_product`、`test_n1_scan_roots_are_the_source_roots` 只比對「守衛的根＝清單」，殺的是「讀者自己加過濾」這種突變，接近擋誤用；真正的價值在逐根的行為測試。留或拿掉由使用者定。

## 後續（本批不改）

- `PRODUCT_KINDS` 放在 Plugin 移除腳本（`plugin_removal.py:39`），`kernel_guard.py:105` 的 `REGISTRIES` 另有一份；可收到 `product_registry.py`。
- `tools/<名>` 仍手寫 3 處（`GyoBuild.cmake`、`GyoTools.cmake:25`、`GyoToolRegistry.cmake:250`），和遊戲的 `gyo_game_directory` 不對稱。
- `sdlp-every-plugin-added` 的 expect 過時（上面）；`ci-shader-target-without-hash` survived、`ci-plan-no-notice` stale（既有）。
- `GameRegistryTests.cmake` 以 `-P` 從 repo 根跑時把 `game-registry-tests/` 留在目前目錄（不吃 `WORK_ROOT`）。
- `bcl-source-roots-drop-build-acceptance` 只有釘死能殺（合成樹沒有 `build/acceptance/<x>`）。
