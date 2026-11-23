# 2026-11-23 SDL 線子批 P：`plugins/` 的建置基礎（`v8-arch-gyo2-sdl-p`）

Owner：架構（`plugins/sdl*`）。GYO2 主會話開的任務卡（使用者「開 P、I」）。分支 `claude/gyo2-sdl-p-plugins-9ed786`（開向 `gyo2`）。作者 Opus 5.5／high。
根據：[SDL 總宣告](../architecture/target/v2/sdl-declaration.zh-Hant.md)修訂 21 第 9.1、10.2 節的 P 列（使用者 2026-11-23 動工前核准「SDL 照建議核准」）。照新 AGENTS（#296）不另寫事前宣告；取捨寫在 PR 說明與 [sdl.zh-Hant.md](../architecture/target/v2/sdl.zh-Hant.md)。

## 做了什麼

1. **`plugins/` 加進共通的根（五處）**：code page 的套用（`GyoCodePage.cmake`）與它的獨立檢查（`ProcessCodePageCheck.cmake`）、os 守衛的 `ROOTS`、`RUNTIME_ROOTS`（`os_boundary_guard.py`）、建置定義的檢查（`GYO_OS_DEFINITION_ROOTS`）。`plugins` 不進 `REQUIRED_ROOTS`（拿掉 SDL 後可能沒有原始碼，SDL-2）。os.md DI 1、2、11 與本文三處照改（使用者在總宣告核准清單第 5 件同意）；順手把 os.md「每個存在的根都要有原始碼」改成和守衛一致（只有 engine、tests 必須有）。
2. **定義檢查抽成模組** `tests/common/os/OsDefinitionCheck.cmake`（照 `ProcessCodePageCheck.cmake` 的先例），`cmake.ProcessCodePage` 的合成樹也跑它：`plugins/sample` 的 target 帶 `GYO_SAMPLE_LINUX=1` 在任何 OS 都被拒。合成樹裡的包裝 tool 讓走訪無限遞迴（真的樹今天沒有 tool，潛在問題），補上和 code page 走訪一樣的祖先檢查。
3. **只建被選的 Plugin**：`gyo_product_units` 記下產品帶的 Plugin；`GYO_PLUGINS`（名字或 `ALL`）另外點名；`gyo_plugin_selection` 沿依賴補齊（library-only 的 `sdl` 跟著來）；建置層在所有產品之後照登記順序加入。Plugin 加入之後才 configure、又帶 Plugin 的產品是 FATAL_ERROR。`gyo-build.json` 多一欄 `plugins`。`test` preset 設 `ALL`（L1 三平台建 SDL、跑 Plugin 的測試），`core` 設空。
4. **`plugins/sdl`**：登記一列（`library_only`）；FetchContent 抓 SDL 3.4.0 的 release archive（URL＋SHA-256，使用者 2026-11-23「兩個都同意」）；靜態建置；master 的補丁 `gyo-cocoa-ime-composing-keydown` 搬來；`git apply` 以 `GIT_CEILING_DIRECTORIES` 停在原始碼目錄（archive 不是 clone，而且在 GYO 的工作樹之內）；configure 時檢查補丁標記。`GYO::Sdl` 是不帶 SDL 的 INTERFACE target（共用型別由 W、R2a 加）。測試 `sdl.build`（`tests/adapters/sdl/`）：版本是 3.4.0、dummy 視訊驅動能啟動與關閉。
5. **刪 SDL 的建置測試（SDL-2）**：`build/ci/common/plugin_removal.py`。`remove`（CI，在 checkout 本身）或 `copy`（本機，HEAD 的 git 工作樹）拿掉 `sdl*` 的列、目錄與帶它們的產品（暫存區刪除，經 git 列檔的測試照跑），剩下的 CMake 檔還提到被拿掉的名字就失敗；`check` 確認 `gyo-build.json` 沒有 `sdl*`、`_deps` 沒有抓 SDL。L1 與打包 toolchain 列的 core 步驟在拿掉之後以 `GYO_PLUGINS=ALL` configure、build、ctest，`check` 之後 `restore`。另加 `cmake.PluginSelection`：整個 repo 什麼都沒選時不加 Plugin（只 configure，沿用父建置抓好的來源、`FETCHCONTENT_FULLY_DISCONNECTED`）。
6. **突變**：os 目錄裡 `find` 對不到的五個（os-g11、os-g15、pu-x8、gdir 兩個）照新字面改；新加 `sdlp-*` 六個（os 目錄）與 `docs/architecture/target/v2/scripts/sdl_mutations.json` 六個。`run_mutations.py` 加 `{build}` 佔位符（`cmake.PluginSelection` 要讀父建置）。

## 查證（本機 macOS Intel，`test` preset，RelWithDebInfo）

- `GYO_PLUGINS` 空：configure／build／ctest 198/198 通過；`gyo-build.json` 的 `plugins` 是 `[]`。
- `GYO_PLUGINS=ALL`：下載 SDL 3.4.0（SHA-256 通過）、補丁上了（`SDL_cocoakeyboard.m` 3 處標記）、SDL 的 revision `SDL-release-3.4.0-0-ga962f40bb`；建置 0 警告；ctest 199/199（多 `sdl.build`）。
- os 守衛的單元測試 48 個、`build.ci` 266 個（含 `test_plugin_removal.py` 10 個）通過。
- 突變（`run_mutations.py`，同一個建置）：
  - 新的與改了 `find` 的：os 目錄 11/11（`sdlp-*` 六個、os-g11、os-g15、pu-x8、gdir 兩個）；`sdl_mutations.json` 5/5（審查後拿掉一個，見下）。第一輪有 wrong_failure：CMake 把長訊息換行，expect 改成 `\s+`。
  - 殺它的測試被這批改了的（`mutation-rerun-scope`）：os 目錄用 os 守衛單元測試或 `ProcessCodePageTests` 的 22/22（另 4 個只在 Windows，略過）；編排層目錄用 `UnitRegistryTests` 的 56/56。其中 `g8s3-default-prefix` 第一次 wrong_failure：探針多了帶 Plugin 的產品，前綴錯時先在那裡失敗，expect 加上那個訊息。
- 靜態 SDL 仍有 dynamic API：`src/dynapi/SDL_dynapi.h:73-74`，桌面平台預設開，和是不是靜態無關。

## 設計審查（Opus／high，作者同為 Opus；Fable 暫停）與 L1 第一輪

- **L1 windows-x64 在 configure 失敗**（run 45149646782，`ApplyPatches.cmake:32`：`patch failed: src/video/cocoa/SDL_cocoakeyboard.m:45`）。原因：Windows 的 checkout（`core.autocrlf`）把 `.patch` 轉成 CRLF，release archive 的原始碼是 LF；master 是 clone SDL，原始碼也被轉成 CRLF，兩邊一致所以沒出事。本機重現：`core.autocrlf=true` 的 clone 裡補丁是 CRLF，`ApplyPatches.cmake` 對 LF 原始碼報同一個錯；LF 的補丁通過。修法：`plugins/sdl/patches/.gitattributes` 寫 `*.patch -text`（跟著 Plugin 走、刪得掉）。
- 拿掉「Plugin 加入之後的產品帶 Plugin 是 FATAL_ERROR」：它擋掉 Plugin 自己測試裡的產品，沒被選的 Plugin 在 generate 時 CMake 本來就報 target 不存在（AGENTS §3：不加防誤用的守衛）。突變 `sdlp-late-product-unrefused` 一起拿掉。
- `check` 多看 CMakeCache 裡家族自己的選項（`SDL_*`）：vendored 或 ExternalProject 的 SDL 不會留下 `_deps`。正例：帶 SDL 的建置命中 93 個；沒有 SDL 的副本照樣通過。
- 帶 SDL 的產品也看它在 `tests/`、`build/acceptance/`、`assets/` 底下同名目錄的 CMake。`cmake.PluginSelection` 的逾時放寬到 600 秒（整個 repo 的 configure，同 `cmake.ProjectComposition`）、註解更正。
- master 留下的 SDL「component」機制（`GyoComponents.cmake`、`GyoRuntimeDeployment.cmake` 的 `SDL3-shared`）沒有接上 `plugins/sdl`，寫進 sdl.zh-Hant.md 範囲の限界，留給重審產品登記機制的那批。

- **L1 第二輪 windows-x64 建置失敗**（run 45151811686，job 137615082067）：補丁已上、configure 通過、`SDL3-static.lib` 在 MSVC 上連結成功（GYO 的編譯選項下 SDL 建得起）；失敗的只有 `SdlBuildTests.cpp:20` 的 `CHECK(std::string_view(...) == "dummy")`——doctest 只前置宣告 `std::basic_ostream`，MSVC 不會間接帶進 `<ostream>`（macOS 的 libc++ 會）。照 repo 其他測試加 `#include <ostream>`。

- **L1 第三輪（run 45155842611）**：L1 三平台通過；Quick acceptance 的 toolchain / windows-x64 在 core 步驟失敗——副本（另一個路徑的 git 工作樹）裡 `authority.authority_sources` 報「System 登記在 repo 根之外」，登記的檔名是原 checkout 的路徑（`std::source_location`），根是副本。同一個 job 裡 checkout 的同一個測試通過，sccache 命中 111 次：推定 sccache＋MSVC 用了 checkout 編出的 object（本機沒有 MSVC 與 sccache，未重現，是推定）。第一個想法「副本關掉 launcher」碰到工作流程的政策測試（launcher 只由 native-setup 設定、工作裡不寫），不採。改成 `remove`（在 checkout 本身拿掉）→ `cmake --preset core`（路徑和以前一樣）→ ctest → `check` → `restore`。`copy` 留給本機。

## SDL-2 的本機實跑

`plugin_removal.py copy` → `cmake -S <副本> --preset core -DGYO_PLUGINS=ALL` → build → ctest 198/198 → `check` 通過（`plugins: []`，`_deps` 只有 doctest、fast_float、yyjson）。合併 gyo2（X1）後再 `copy` 一次，也拿得乾淨。

## 還沒做、交給後面的

- SDL-1 的原始碼掃描與「SDL 只被 `sdl*` PRIVATE 連結」的設定時檢查：照總宣告第 9.1 節在子批 K 之後立。
- examples 帶 `sdl*` 時怎麼讓 `plugin_removal.py` 拿得掉：W 定（sdl.zh-Hant.md 範囲の限界）。
- 升到 SDL 3.4.20：使用者同意另開小批。
