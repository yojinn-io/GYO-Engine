# SDL 的 Plugin（`plugins/sdl*`）

更新：2026-11-23（新頁，子批 P `v8-arch-gyo2-sdl-p`）。由來：[SDL 總宣告](sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 9.1、10.2、12 節。本頁是 `plugins/sdl*` 的 owner 文件：子批 P 開頁、寫目錄的形、取得、選擇與 SDL-1、SDL-2；W（`sdl_window`）、A（`sdl_audio`）、R2a（`sdl_gpu`）各自補自己那一列與 SDL-3～SDL-7（草案在總宣告第 12 節）。

Owner：架構。

## 一個例子

Match（伺服器）不帶任何 `sdl*` 的列：configure 不加 `plugins/sdl`，SDL 不下載、不建，產品裡沒有 SDL。KeepTool 帶 `sdl_window`、`sdl_gpu`：建置層加這兩個目錄，也沿依賴加 `sdl`（函式庫），由 `sdl` 抓 SDL 3.4.0、上 GYO 的補丁。把整組 `sdl*` 從 repo 拿掉，Engine、所有 Module、headless 的測試照樣建得起、測得過（SDL-2）。

## 1. 列與依賴

一列＝一個目錄＝一個 target＝一個單位（`config/plugins.csv`）。依賴只有 `plugins/sdl*` → `engine/modules/*` 與 `sdl` 函式庫；Engine 不 include `plugins/`，Module 不依賴 Plugin（登記檔讀取時的 FATAL_ERROR）。

| 列 | 角色 | 誰建 | 現況 |
|---|---|---|---|
| `sdl` | 函式庫（`library_only`）：抓 SDL3、上補丁；`sdl*` 之間共用的、我們的詞寫的型別（視窗登錄表、時鐘配對） | P | 有（target `GYO::Sdl`；P 只有取得，還沒有共用型別） |
| `sdl_window` | 來源側：Runner、Window、InputEvents、Ime、剪貼簿 | W | 還沒有 |
| `sdl_gpu` | 渲染側：Gpu、GpuSubmit | R2a | 還沒有 |
| `sdl_audio` | `Configure`：AudioOutput | A | 還沒有 |

各列的內容、依賴與兩個 World 的接法見總宣告第 9.1、9.2 節。一列的依賴在登記檔寫：例 `sdl_window` 的 `depends` 是 `sdl;input;window`。

## 2. 目錄的形

```text
plugins/<名>/
  CMakeLists.txt          # target gyo_<名>、alias GYO::<名>；SDL3::SDL3-static 以 PRIVATE 連結
  include/…               # 給依賴者的標頭：我們的詞，不 include SDL3/
  src/…                   # System、單位、runner：不 include SDL3/
  src/adapters/sdl/…      # 只有這裡的 .cpp（與這裡的私有標頭）include SDL3/
  tests/CMakeLists.txt    # BUILD_TESTING 時加入；這個 Plugin 沒被選就不存在
  tests/adapters/sdl/…    # 直接呼叫 SDL 的測試
```

- adapter 給 System 用的介面標頭放在 `src/adapters/sdl/` 外（例：`plugins/sdl_gpu/src/GpuDevice.hpp`），由 adapter 的 `.cpp` 實作（SDL-1）。
- Plugin 的測試放在自己的目錄、跟著列走；名字以 Plugin 名開頭（例：`sdl.build`）。`plugins/` 是執行時的程式：os 守衛的 R1～R4 都管它，它的測試也受 R4（os.md DI 1、2；`tests/` 底下的豁免不延伸到 `plugins/<名>/tests/`）；Windows 上它的執行檔套 UTF-8 的 code page（os.md DI 11）。

## 3. 選擇：只建被選的 Plugin

- 建置層（`build/cmake/GyoBuild.cmake`）在所有產品、測試、examples 之後，加入被選的 Plugin 目錄：產品在 `gyo_product_units(... PLUGINS …)` 列出的，加上快取變數 `GYO_PLUGINS` 點名的（登記檔的名字，或 `ALL`），連同它們依賴的 Plugin，照登記順序（`gyo_plugin_selection`，`GyoUnitRegistry.cmake`）。產品先 configure、以名字 link Plugin 的 target，CMake 在全部目錄 configure 完之後才解析；link 一個沒被選的 Plugin，generate 時 CMake 自己報 target 不存在。Plugin 自己的測試裡也可以用 `gyo_product_units(... PLUGINS <自己>)`。
- 預設 `GYO_PLUGINS` 空：只建產品帶的。`test` 系列的 preset（含 L1 的 `ci-<os>`）設 `ALL`：三個平台都建 SDL、跑 Plugin 自己的測試。`core` preset 設空。
- `gyo-build.json` 的 `plugins` 記這次加入的 Plugin。

## 4. SDL 的取得

| 項目 | 值 |
|---|---|
| 版本 | SDL 3.4.0（tag `release-3.4.0`，commit `a962f40bbba175e9716557a25d5d7965f134a3d3`，2026-01-01 發布） |
| 來源 | 官方 release 的原始碼 archive `https://github.com/libsdl-org/SDL/releases/download/release-3.4.0/SDL3-3.4.0.tar.gz`（15,567,602 bytes） |
| 驗證 | `URL_HASH SHA256=082cbf5f429e0d80820f68dc2b507a94d4cc1b4e70817b119bbb8ec6a69584b8`（GitHub 的 asset digest） |
| 放哪 | FetchContent，建置輸出的 `_deps/sdl3-*`；不放 `plugins/` 底下、不放 `third_party/`（`third_party/` 在登記檔之前無條件加入，放那裡刪不掉 SDL，總宣告第 9.1 節） |
| 建法 | 靜態（`SDL_STATIC=ON`、`SDL_SHARED=OFF`）；不建 SDL 自己的測試、examples、install |

**靜態的理由**：產品旁邊不必再帶一個 SDL3 的動態函式庫，Windows 的測試不必設 DLL 搜尋路徑。master 用動態（`GyoRuntimeDeployment.cmake` 的 `SDL3-shared` 複製）；要改回動態時，打包（子批 K 的 tools 線）一起處理。

**GYO 的編譯選項**：`plugins/` 在 `add_compile_options`（`-ffp-contract=off`、MSVC 的 `/Zc:preprocessor`，`GyoBuild.cmake`）之後加入，FetchContent 的 SDL 也吃到這些選項。L1 三平台（含 MSVC）建得起是 P 的驗收之一。

### SDL 的補丁

`plugins/sdl/patches/*.patch` 照檔名順序上（`plugins/sdl/cmake/ApplyPatches.cmake`；已上的跳過）。原始碼是 release archive 解開的、在建置輸出裡（可能在 GYO 的 git 工作樹之內），所以 `git apply` 用 `GIT_CEILING_DIRECTORIES` 停在原始碼目錄、當一般的 patch 程式用。用 `FETCHCONTENT_SOURCE_DIR_SDL3` 指定的原始碼不經 patch 步驟：configure 時檢查補丁的標記，沒上就是 FATAL_ERROR。

| 補丁 | 缺陷 | 移除條件 |
|---|---|---|
| `gyo-cocoa-ime-composing-keydown` | macOS：組字中，輸入法處理一個鍵卻沒改組字時，SDL 照樣送出按下（日文輸入法的第一個 Return 只確定候選字），程式中途收到 Return。補丁讓組字中按下的鍵不送給程式，除非輸入法把它當命令轉出（`doCommandBySelector:`），同 winit。由來：master `f775acb2`（`v8-tool-ui-editor-ime-enter`） | SDL 上游修好；或升版後補丁上不去（先在新版重現缺陷，再移植補丁） |

### 升版

改 `plugins/sdl/CMakeLists.txt` 的 URL 與 `URL_HASH`、本節的表、`plugins/sdl/tests/adapters/sdl/SdlBuildTests.cpp` 的版本；每個補丁重新上、上不去時照上表的移除條件。下載新版本要使用者同意（版本、來源、大小、SHA-256）。

## Design Invariants

- **SDL-1 SDL 只在 `plugins/sdl*` 的 adapter 目錄**：`SDL3/` 的標頭只被 `plugins/sdl*` 的 `src/adapters/sdl/` 與 `tests/adapters/sdl/` 底下的檔案 include（adapter 目錄裡可以有私有標頭，給 adapter 的幾個 `.cpp` 共用 SDL 的型別）；adapter 目錄外的檔案不 include adapter 目錄裡的任何檔案——adapter 給 System 用的介面標頭（我們的詞，不含 SDL）放在 adapter 目錄外，由 adapter 的 `.cpp` 實作；所以 SDL 不會經遞移的 include 漏到 System。System、runner、單位的 `Configure`／`Build`／`Check` 的程式裡沒有 SDL（建視窗、開裝置、claim 視窗也呼叫 adapter）；`engine/` 底下沒有 SDL。Plugin 之間的交接（視窗登錄表）用不透明的把手，型別定在 `sdl` 函式庫、不露 SDL 的型別（收窄使用者 11-13 的 (iii)，總宣告第 14 節核准清單第 6 件）。SDL3 對 `sdl*` 的 target 都是 PRIVATE 連結；`GYO::Sdl` 本身不帶 SDL。守衛：原始碼掃描在子批 K 的 scenario 通過後立（總宣告第 9.1 節）；在那之前、以及 adapter 目錄外前置宣告 SDL 型別這種掃描判不了的，由每批的設計審查看。
- **SDL-2 沒有 SDL 也成立**：拿掉 `sdl*` 的 Plugin 與登記檔的列（與帶它們的產品），Engine、所有 Module、headless 的測試仍 configure、build、通過，SDL 不被下載、不被建。沒有產品選 `sdl*`、`GYO_PLUGINS` 也沒點名時，同樣不下載、不建。守衛：CI 的 core 步驟（L1 的 Linux 列、打包工作流程的 toolchain 列）——checkout 自己的建置與測試跑完之後，`build/ci/common/plugin_removal.py remove` 在 checkout 裡拿掉 `sdl*` 的列與目錄與帶它們的產品（暫存區刪除，經 git 列檔的測試照樣能跑），以 `core` preset、`GYO_PLUGINS=ALL` configure、build、ctest，`plugin_removal.py check` 確認 `gyo-build.json` 的 `plugins` 沒有 `sdl*`、`_deps` 沒有抓 SDL、CMakeCache 沒有 SDL 自己的選項（`SDL_*`，抓 vendored 或 ExternalProject 加進來的 SDL），最後 `restore` 把 checkout 還原成 HEAD。在 checkout 本身做、不在副本做：編譯快取的命中若跨兩棵原始碼樹，object 裡的 `std::source_location` 會是另一棵樹的路徑（L1 第二輪在 Windows 的 sccache＋MSVC 上碰到，`authority.authority_sources` 失敗）；同一個路徑編譯，快取的 key 也和以前一樣。本機要不動自己的樹時用 `copy`（HEAD 的 git 工作樹）；`cmake.PluginSelection`（整個 repo 什麼都沒選時不加任何 Plugin 目錄；configure 而已，沿用父建置抓好的來源、不連網）；`cmake.UnitRegistry`（選擇：依賴補齊、`ALL`、拒絕非 Plugin 的名字、產品登記）；`build.ci` 的 `test_plugin_removal.py`。突變：`docs/architecture/target/v2/scripts/sdl_mutations.json`。
- SDL-3～SDL-5、SDL-7 的草案在總宣告第 12 節，由 W、A、R2a 寫進本頁。SDL-6 已撤，號碼不重用。

## Replacement Criteria

總宣告第 12 節 RC-SDL 1～4（整條線）；各子批補自己的。

## 範囲の限界

- `remove` 要 checkout 的追蹤檔沒有改動（否則拒絕、請用 `copy`），`restore` 把所有追蹤檔放回 HEAD；中途失敗時 checkout 停在拿掉的狀態（CI 的那個 job 已經失敗）。`copy` 建的是 HEAD 的工作樹：本機還沒 commit 的改動不在裡面。
- 「帶 SDL 的產品」以產品目錄（與它的 `tests/`、`build/acceptance/`、`assets/` 底下同名目錄）的 CMake 檔裡有沒有 `sdl*` 的名字判（`#` 註解不算；多行的 `#[[ ]]` 註解只去掉第一行，後面的名字會多報、不會漏報）；產品以外的 CMake 檔還提到被拿掉的名字時，`remove`／`copy` 失敗並列出檔案。examples 目前是一個 `examples/CMakeLists.txt`：第一個帶 `sdl*` 的 example（W）要能被拿掉（例：每個 example 一個目錄、照目錄加入），由 W 定。
- SDL-2 的 CI 步驟只在 Linux（L1）與打包的 toolchain 列跑；Windows、macOS 的 L1 列建的是帶 SDL 的組態。
- 本機重用的建置目錄若留著以前抓的 `_deps/sdl3-*`，`check` 會報錯：用新的建置目錄。`copy` 會在本機 repo 登記一個 worktree（`git worktree remove` 拿掉）。
- master 留下的舊「component」機制（`GyoComponents.cmake` 的 `SDL_*`、`GYO_ENABLE_SDL3`，`GyoRuntimeDeployment.cmake` 的 `SDL3-shared`，`core` preset 的 `GYO_BUILD_SDL_*`）和 `plugins/sdl` 沒有接上；`GyoRuntimeDeployment` 在產品 configure 時查 `TARGET SDL3-shared`，早於 Plugin 加入，永遠是 false。留給重審產品登記機制的那批（Object_Connect 移植）處理，這批不動。
