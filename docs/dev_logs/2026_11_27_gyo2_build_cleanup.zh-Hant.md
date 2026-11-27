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

- `games/<名>` 收成 `gyo_game_source_dir(root name output)`（`GyoGameRegistry.cmake`，`-P` 也能用）：`gyo_validate_game_sources`、`GyoGames.cmake` 的 `project.json`、`GyoBuild.cmake` 的第一個迴圈都用它；建置目錄從相對路徑推。第二個迴圈的 `GYO_GAME_SOURCE_DIR`、`GYO_GAME_BINARY_DIR` 沒人讀，拿掉。
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
- 跑的範圍（D184）：檔案在改動範圍的、殺它的測試被改到的（os、naming、`ProcessCodePageTests`、workflow gates、product registry、game copy、`ProjectComposition`、assembly）。只改名的測試檔（release、archive、package checks、release retry）的 expect 都不含改到的名字，不跑。結果見下。

（結果）

## 驗證

（結果）
