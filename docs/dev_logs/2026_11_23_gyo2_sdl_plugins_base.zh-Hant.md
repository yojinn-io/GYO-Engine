# 2026-11-23 SDL 線子批 P：`plugins/` 的建置基礎（`v8-arch-gyo2-sdl-p`）

Owner：架構（`plugins/sdl*`）。GYO2 主會話開的任務卡（使用者「開 P、I」）。分支 `claude/gyo2-sdl-p-plugins-9ed786`（開向 `gyo2`）。作者 Opus 5.5／high。
根據：[SDL 總宣告](../architecture/target/v2/sdl-declaration.zh-Hant.md)修訂 21 第 9.1、10.2 節的 P 列（使用者 2026-11-23 動工前核准「SDL 照建議核准」）。照新 AGENTS（#296）不另寫事前宣告；取捨寫在 PR 說明與 [sdl.zh-Hant.md](../architecture/target/v2/sdl.zh-Hant.md)。

## 做了什麼

1. **`plugins/` 加進共通的根（五處）**：code page 的套用（`GyoCodePage.cmake`）與它的獨立檢查（`ProcessCodePageCheck.cmake`）、os 守衛的 `ROOTS`、`RUNTIME_ROOTS`（`os_boundary_guard.py`）、建置定義的檢查（`GYO_OS_DEFINITION_ROOTS`）。`plugins` 不進 `REQUIRED_ROOTS`（拿掉 SDL 後可能沒有原始碼，SDL-2）。os.md DI 1、2、11 與本文三處照改（使用者在總宣告核准清單第 5 件同意）；順手把 os.md「每個存在的根都要有原始碼」改成和守衛一致（只有 engine、tests 必須有）。
2. **定義檢查抽成模組** `tests/common/os/OsDefinitionCheck.cmake`（照 `ProcessCodePageCheck.cmake` 的先例），`cmake.ProcessCodePage` 的合成樹也跑它：`plugins/sample` 的 target 帶 `GYO_SAMPLE_LINUX=1` 在任何 OS 都被拒。合成樹裡的包裝 tool 讓走訪無限遞迴（真的樹今天沒有 tool，潛在問題），補上和 code page 走訪一樣的祖先檢查。
3. **只建被選的 Plugin**：`gyo_product_units` 記下產品帶的 Plugin；`GYO_PLUGINS`（名字或 `ALL`）另外點名；`gyo_plugin_selection` 沿依賴補齊（library-only 的 `sdl` 跟著來）；建置層在所有產品之後照登記順序加入。Plugin 加入之後才 configure、又帶 Plugin 的產品是 FATAL_ERROR。`gyo-build.json` 多一欄 `plugins`。`test` preset 設 `ALL`（L1 三平台建 SDL、跑 Plugin 的測試），`core` 設空。
4. **`plugins/sdl`**：登記一列（`library_only`）；FetchContent 抓 SDL 3.4.0 的 release archive（URL＋SHA-256，使用者 2026-11-23「兩個都同意」）；靜態建置；master 的補丁 `gyo-cocoa-ime-composing-keydown` 搬來；`git apply` 以 `GIT_CEILING_DIRECTORIES` 停在原始碼目錄（archive 不是 clone，而且在 GYO 的工作樹之內）；configure 時檢查補丁標記。`GYO::Sdl` 是不帶 SDL 的 INTERFACE target（共用型別由 W、R2a 加）。測試 `sdl.build`（`tests/adapters/sdl/`）：版本是 3.4.0、dummy 視訊驅動能啟動與關閉。
5. **刪 SDL 的建置測試（SDL-2）**：`build/ci/common/plugin_removal.py`。`copy` 建 HEAD 的 git 工作樹，拿掉 `sdl*` 的列、目錄與帶它們的產品（暫存區刪除，經 git 列檔的測試照跑），剩下的 CMake 檔還提到被拿掉的名字就失敗；`check` 確認 `gyo-build.json` 沒有 `sdl*`、`_deps` 沒有抓 SDL。L1 與打包 toolchain 列的 core 步驟改在這份副本上以 `GYO_PLUGINS=ALL` configure、build、ctest，最後 `check`。另加 `cmake.PluginSelection`：整個 repo 什麼都沒選時不加 Plugin（只 configure，沿用父建置抓好的來源、`FETCHCONTENT_FULLY_DISCONNECTED`）。
6. **突變**：os 目錄裡 `find` 對不到的五個（os-g11、os-g15、pu-x8、gdir 兩個）照新字面改；新加 `sdlp-*` 六個（os 目錄）與 `docs/architecture/target/v2/scripts/sdl_mutations.json` 六個。`run_mutations.py` 加 `{build}` 佔位符（`cmake.PluginSelection` 要讀父建置）。

## 查證（本機 macOS Intel，`test` preset，RelWithDebInfo）

- `GYO_PLUGINS` 空：configure／build／ctest 198/198 通過；`gyo-build.json` 的 `plugins` 是 `[]`。
- `GYO_PLUGINS=ALL`：下載 SDL 3.4.0（SHA-256 通過）、補丁上了（`SDL_cocoakeyboard.m` 3 處標記）、SDL 的 revision `SDL-release-3.4.0-0-ga962f40bb`；建置 0 警告；ctest 199/199（多 `sdl.build`）。
- os 守衛的單元測試 48 個、`build.ci` 266 個（含 `test_plugin_removal.py` 10 個）通過。
- 突變（`run_mutations.py`，同一個建置）：
  - 新的與改了 `find` 的：os 目錄 11/11（`sdlp-*` 六個、os-g11、os-g15、pu-x8、gdir 兩個）；`sdl_mutations.json` 6/6。第一輪 `sdl_mutations.json` 3 個是 wrong_failure：CMake 把長訊息換行，expect 改成 `\s+`；`sdlp-late-product-unrefused` 拿掉檢查後探針因別的原因 configure 失敗（`LANGUAGES NONE` 的專案加 `.cpp`），測試照樣失敗、訊息是「did not report」，expect 兩者都收。
  - 殺它的測試被這批改了的（`mutation-rerun-scope`）：os 目錄用 os 守衛單元測試或 `ProcessCodePageTests` 的 22/22（另 4 個只在 Windows，略過）；編排層目錄用 `UnitRegistryTests` 的 56/56。其中 `g8s3-default-prefix` 第一次 wrong_failure：探針多了帶 Plugin 的產品，前綴錯時先在那裡失敗，expect 加上那個訊息。
- 靜態 SDL 仍有 dynamic API：`src/dynapi/SDL_dynapi.h:73-74`，桌面平台預設開，和是不是靜態無關。

## 還沒做、交給後面的

- SDL-1 的原始碼掃描與「SDL 只被 `sdl*` PRIVATE 連結」的設定時檢查：照總宣告第 9.1 節在子批 K 之後立。
- examples 帶 `sdl*` 時怎麼讓 `plugin_removal.py` 拿得掉：W 定（sdl.zh-Hant.md 範囲の限界）。
- 升到 SDL 3.4.20：使用者同意另開小批。
