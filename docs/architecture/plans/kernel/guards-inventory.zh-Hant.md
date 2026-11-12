# 守衛與突變的盤點：`v8-arch-guards-inventory`

日期：2026-11-12。Owner：架構（架構師會話開出的任務卡；執行 Fable／high，逐檔讀碼與 grep low）。性質：**只讀盤點**，不改任何守衛、突變、程式；寫入只有本檔、dev_log、kernel README 與 HANDOFF 各一行。
基準：程式與守衛讀 origin/master `d0d8563f`（PR #197 `-zones` 已併入，(c) 區程式在 `apps/object_fps_pvp/src/Sim/`）；計畫文字讀 `origin/claude/arch-migration-plan`（`migration-plan.zh-Hant.md`、kernel `HANDOFF.md` 決定表；該分支落後 master、缺 #197 的搬檔，所以守衛與突變的**路徑一律照 master**）。證據：主 checkout `build/evidence/guards-inventory/`（`mutations-count.txt`、`pvp-mutations-by-file.txt`、`migration-plan.snapshot.md`、`kernel-HANDOFF.snapshot.md`、`migration-plan.snapshot.rev`；git 管理外）。
依據：使用者決定 `v8-arch-guards-migrate-with-code`（守衛與突變隨程式遷移；四選一；判斷標準是性質不是測試名；只在 legacy 的性質在該部件遷移的同一批退役；突變隨守衛重列並照 AGENTS §14.2 由不同模型敵對檢查），遷移計畫 §3 共通「守衛與突變隨程式一起遷移」、§0 A10、§5.2；起點清單 legacy-impact §4 的 21 列。

---

## 0. 件数の突き合わせ與判定

**件数の突き合わせ：一致——守衛 21／突變清單 26 檔 1841 條／執行器 3 支；去處四類（初判、以各批事前宣告為準）：守衛 21 列拆成 63 子列＝留 31／改寫 21／退役 11，另「新增」3 件（J-9 禁字表、plugins 依賴守衛、trace `:155` 契約測試；不是既有子列的去處，列在對應節）；突變 1841 條＝留 1131／重列 627／退役 83／新增 0（新增的突變由各批的新守衛產生，本卡不預列）；缺口 9 條（§4）。**

計數方法：`git ls-files | grep -i mutation`（正例先命中 `tests/object_fps_pvp/mutations.json`）→ Python 逐檔讀 `mutants` 陣列長度（證據 `mutations-count.txt`）。執行器＝`run_mutations.py` 三支（object_fps_pvp、zone_scan、time-threads-trace；後者以 `--catalog` 跑 Engine 與工具的 22 份清單，plugins 遷移後仍是共用執行器）。

判定（看結果前宣告，數字在開始時補、送 parent 核定；parent 會話以 `send_message` 找不到（`session parent not found`），所以核定改由使用者在本會話做，見 §7）：

| # | 判定 | 結果 |
|---|---|---|
| 1 | 覆蓋：21 個守衛逐列、26 檔 mutations.json 逐檔（第 5 欄以檔為單位；pvp 925 條另按 `file` 欄分組對照遷移批）、3 支執行器各一列；漏一＝不成立 | 成立（§2 的 21 節、§3 的 26＋3 列） |
| 2 | 每列七欄齊；「性質」欄不得只是測試名 | 成立（§2 每節：標題行＝第 1 欄；子表＝第 2～7 欄） |
| 3 | 第 7 欄只用具體事實（規則、正規式、白名單、連結期望）支撐 | 成立（每格引檔名行號或規則名） |
| 4 | 零修改：除本檔、dev_log、kernel README 一行、HANDOFF 現況一行外不改任何檔 | 成立（`git diff --stat` 見 dev_log） |

## 1. 讀法

- **第 1 欄**（節標題）：守衛的檔、ctest 名、owner。
- **性質**：這個 CHECK／規則守的是什麼，一句話；同一守衛裡守不同性質的拆成子列（AGENTS §14.4 以判定為單位）。
- **來源條文**：legacy 側的 DI／RC／章節（`docs/architecture/*.md` 的現況文件沒有 Legacy 標示，但在本表一樣算 legacy 側）；target 或 spec 的對應條號；三選一：**只在 legacy**／**兩邊都有**／**只在 target**。
- **去處**：留／改寫／退役／新增（計畫 §3 共通的定義）。退役的寫性質在 target 由誰接手；接不住標「缺口」並列入 §4。
- **突變**：哪幾條針對會被刪或搬的程式碼、去處（留／重列／退役）、重列後的敵對檢查者模型。模型規則：§3 共通第 3 點「宣告沒另指定時用該批審查者的模型」；計畫 §2.1 各遷移批實作者 Opus、審查者 Fable（h7-scanner 相反），所以本表的檢查者預設 **Fable**；敵對檢查者必須與該守衛／突變一覽的作者不同、且不是小模型（navigator）。
- **批**：對照計畫 §5.2 的「判去處的批」；不一致的標 **≠§5.2**。
- **不動會判誰違規**（第 7 欄，本卡最重要）：這個守衛不動時，哪個遷移批的新程式會被它判違規，或它會變成空轉（掃不到新程式）——都指到規則、正規式、白名單或連結期望。

---

## 2. 守衛 21 列

### 2.1 `tests/common/entity_store/entity_store_guard.py`（ctest `entity_store.word_guard`、`entity_store.word_guard_selftest`；owner Engine entity_store）

| 子列 | 性質 | 來源條文（三選一） | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| W1 | `engine/entity_store` 的程式與註解不含消費者（產品、工具、連線角色）的名字 | entity-store DI 9；target：world DI 1～2 的「World 不配發、不解讀鍵」間接相關，spec J-9 對 `engine/kernel` 守同性質「無產品名」→ **兩邊都有**（各守各的目錄） | 留 | `docs/architecture/plans/entity-store/scripts/mutations.json` 15 條（`es-row*`）針對 `engine/entity_store/include/*.hpp`、CMake：留；若 world-exec 依 Delta-3 改 store API（回傳位置的查找版本或 `Cell<T>`），`find` 文字會過期→該批重列、檢查者 Fable | world-exec（＝§5.2 第 1 列） | 字表 `object_fps\|RetroFPS\|fps::pvp\|\b[Pp]vp\|[Pp]layer\|Gateway\|\bMatch\b`（:42）連註解都掃：World 的欄實作若放進 `engine/entity_store`，註解裡寫「Match host」「Player 欄」就命中。這是正確方向（World 不進 store 目錄，Delta-3 第 4 項已寫明），不是拉回舊形狀 |
| W2 | store 內無 unordered 容器、無 `std::hash` | entity-store DI 8；spec W-10／J-9（kernel 同性質）→ 兩邊都有 | 留 | 同上 | world-exec | 無 |
| W3 | store 內無時鐘、亂數、執行緒、atomic（標頭與符號） | entity-store DI 8；J-9 → 兩邊都有 | 留 | 同上 | world-exec | 無 |
| W4 | store 內無非 constexpr 的 `static`／`thread_local` 變數 | entity-store DI 8；J-9（白名單只有 `tlCurrent` 與 `TypeKey<T>` 的 `static inline char`，A.6）→ 兩邊都有但**白名單不同** | 留 | 同上 | world-exec | `TypeKey<T>` 的 `static inline char` 在 entity_store 會被 W4 攔（:61、:100-115 無白名單）→ kernel 的型別鍵只能在 `engine/kernel`。正確攔截 |
| W5 | store 只 include 標準庫、`engine/base/`、`engine/entity_store/` | entity-store DI 9；plan 3.7 `gyo_expect_links(gyo_kernel GYO::Base GYO::EntityStore)`（方向 kernel→store）→ 兩邊都有 | 留 | 同上 | world-exec | 若 World 的 stamp 陣列或 kernel 型別被放進 store 標頭（include `engine/kernel/…`）→ W5 命中。正確攔截（legacy-impact §2.1 DI 6：stamp 屬 World） |
| 整體 | 掃描根存在（:160「no source to scan」） | — | 留；**條件退役**：若 Delta-3 的另一選項把 store 併入 kernel 目錄，守衛整個失敗→同批退役並由 J-9 接手 | — | world-exec | `SCANNED = "engine/entity_store"`（:39）目錄改名或刪除即失敗，不能留著空轉 |

### 2.2 `gyo_expect_links(gyo_entity_store GYO::Base)`（`tests/common/entity_store/CMakeLists.txt:34-37`，configure 時；owner Engine entity_store）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| — | store 的連結（私有與介面）恰等於 `GYO::Base`，是 Base 之上的葉子 | entity-store DI 9；spec J.1 第 3 點引守衛列 9；plan 3.7 kernel 的 `gyo_expect_links` 以 store 為依賴 → 兩邊都有 | 留 | `es-row9-links-math`：留 | world-exec（＝§5.2 第 2 列） | 若 world-exec 讓 store 認得 kernel 型別（連 `GYO::Kernel`）→ `gyo_expect_links`（`tests/common/time/CMakeLists.txt:22-40`，比對排序後的完整連結清單）失敗。正確方向：依賴只能 kernel→store |

### 2.3 `tests/common/scripts/neutral_names_guard.py`（ctest `gyo_voxel_mesh.neutral_names`、`gyo_map_markers.neutral_names`；owner Engine voxel_mesh／map_markers 各自登記，腳本在共通 scripts）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| WORDS | `engine/voxel_mesh`、`engine/map_markers`、它們的測試與契約文件（`docs/architecture/voxel-mesh.md`、`map-markers.md`）不含 gameplay 名詞 `spawn\|boss\|loot\|extract\|patrol\|team\|zone`（子字串、不分大小寫、無 `\b`，:20、:24） | voxel-mesh DI 1、map-markers DI 1、architecture.md Layers check 1（legacy）；target：J-9「無產品名」是**另一個性質**（產品名 ≠ gameplay 名詞）→ **只在 legacy**（但兩個模組不遷移、文件是現況文件，性質持續有效） | 留（不套到 kernel；J-9 自寫禁字表＝**新增**，plan 3.3 已寫） | `engine_format_mutations.json` 有 1 條針對 `docs/architecture/voxel-mesh.md`（往契約文件塞 gameplay 詞）：留 | kernel-scaffold（＝§5.2 第 3 列） | (a) 掃描範圍含兩份契約文件：closeout 或 a6-revision 若在這兩份文件加「已被取代（批次）」註或對照 target 的句子，寫到 `zone`、`Extract<Read<T>>`、`Despawn` 就失敗（`despawn`、`ExtractRes`、`timezone` 都命中子字串）。(b) 若 kernel-scaffold 沿用 WORDS 當 J-9：kernel 的 `Spawn`／`Despawn`／`Extract`／`zone ∈ {C, B}` 全部誤判（legacy-impact §4 第 3 列已實測退出碼 1）。(c) 不掃 `docs/architecture/target/`，所以 target 文件不受影響 |

### 2.4 `tests/common/os/os_boundary_guard.py`（ctest `os.boundary_guard`、`os.boundary_guard_selftest`；owner Engine os）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| R1 | `engine/os/src` 以外的 C/C++ 不 include OS 標頭（`OS_HEADERS` :40-50，含 `sys/…`、`signal.h`、`windows.h`） | os.md DI 1；target 無對應條文（spec 假設 kernel 不含 OS 碼；Delta-4 把頁保護平台層放 `engine/os`）→ **只在 legacy**（現況文件，持續有效） | 留 | `os-subsystem/scripts/mutations.json` 149 條：針對守衛本身的 `os-g9-macro-list`、`os-g11-roots-skip-products`、`os-g12-allowlist-scope`、`os-g15-runtime-roots`、`pu-x11`、`pu-x14`（6 條改 `os_boundary_guard.py`）與 `os-g6-stale`（allowlist）：留 | kernel-scaffold；pageguard-os（條件）重判（＝§5.2 第 4 列）；**另加 plugins-foundation（≠§5.2，見第 7 欄）** | pageguard-os：`engine/kernel` 若持 `<sys/mman.h>`、`<signal.h>`、`windows.h`、VEH → R1／R2／R3 命中（ci-readiness「27 筆探測」）。**正確攔截**，它決定頁保護平台層的位置。 |
| R2 | 預處理條件不分支 OS 巨集（`OS_MACROS` :52-56） | os.md DI 1 → 只在 legacy | 留 | 同上 | 同上 | 同 R1 |
| R3 | 不呼叫列出的 OS／CRT API（`OS_APIS` :57-64、`signal(`） | os.md DI 1 → 只在 legacy | 留 | 同上 | 同上 | 同 R1 |
| R4 | runtime 根（`RUNTIME_ROOTS = engine, apps, tools` :27）不直接開原生檔（fstream、fopen、`std::filesystem` 操作） | os.md DI 2；target/assets §1 第 4 點（IO 由服務、loader 收位元組）同向 → 兩邊都有 | **改寫（締める面）**：`ROOTS`／`RUNTIME_ROOTS` 加 `plugins` | 同上 | **plugins-foundation（≠§5.2：第 4～6 列都沒列這批）** | **掃描根 `ROOTS = ("engine", "apps", "tools", "build/acceptance", "tests")`（:26）沒有 `plugins/`，也從來沒有 `framework/`**。plugins-foundation 把 `HeadlessShell` 與 `match_main` 的迴圈、asset-io 服務（IO 執行緒讀檔）搬進 `plugins/`：R1～R4 對它們**一律不掃**，「OS 知識只在 engine/os」「原生檔只經 engine/os」對新 top-level 無人守；asset-io 服務若直接 `std::ifstream` 讀檔，R4 看不到。不是誤判新程式，是**空轉**。改 ROOTS 是 DI 守衛的變更（§14.4 最後一條：DI 守衛不論緩める面有無都交使用者） |
| R5 | `engine/os` 只 include 標準庫、OS 標頭、`engine/base`、自己 | os.md DI 3 → 只在 legacy | 留 | 同上 | pageguard-os | 頁保護平台層若 include `engine/kernel/…` 取策略型別→ R5 命中。正確：kernel 持策略、os 持機制（plan 3.10） |
| R6 | `engine/os` 只走 CP_UTF8 與寬字元 Win32 入口 | os.md DI 5、11 → 只在 legacy | 留 | 同上 | — | 無 |
| ratchet | allowlist 條目的檔必須存在、數量恰等於計數（:213-226、:241-252） | os.md DI 1（運用規則） | **改寫型**（搬檔只改路徑欄、不放寬；與 D2 棘輪同型） | — | match-sim（`tests/object_fps_pvp/os_boundary_allowlist.txt` 的 `ArenaDigestTests.cpp R2 1`）、client-link／match-host（`build/acceptance/object_fps_pvp/…/network_main.cpp R3 1`）；Engine allowlist 的 `WaiterTests`、`RoleThreadTests`、`TraceSinkTests`、`platform_sdl/EntryTests`、`AssetPathResolverTests` 不動 | 搬掉 allowlist 列出的檔→「does not exist; remove the entry」（:220-221）失敗；條目只能縮，不能擴（:245-247）。對產品批是正確的提醒 |

### 2.5 OS 編譯定義檢查（`tests/common/os/CMakeLists.txt:63-137` `gyo_expect_no_os_definitions`，configure 時；owner Engine os）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| — | 掃描根下的目錄與 target 不帶識別 OS 的編譯定義（`GYO_OS_IDENTIFYING_NAME` :133-134、`PLATFORM_ID`） | os.md DI 1 → 只在 legacy | 留；**改寫（締める面）**：`GYO_OS_DEFINITION_ROOTS`（:131）加 `plugins` | `os-g13-definition-name`、`os-g14-definition-platform-id`：留 | kernel-scaffold（＝§5.2 第 5 列）；**plugins-foundation（≠§5.2）** | `engine/kernel` 的 target 自動在根下（engine）。`plugins/` 不在 `GYO_OS_DEFINITION_ROOTS = engine apps tools build/acceptance tests` → plugin target 帶 `-DGYO_WINDOWS` 類定義無人查（空轉） |

### 2.6 `tests/common/os/ProcessCodePageCheck.cmake`（`gyo_expect_process_code_pages`，configure 時；ctest `cmake.ProcessCodePage` 以合成樹驗它；owner Engine os）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| — | 掃描根下每個 EXECUTABLE 帶 UTF-8 code page manifest 或列出豁免理由 | os.md DI 11 → 只在 legacy | 留；plugins-foundation 若在 `plugins/` 下放執行檔（探針）則改寫（regex `^(engine\|apps\|tools\|build/acceptance\|tests)(/\|$)` :19 加 plugins）並照 `GyoBuild.cmake:58` 的做法呼叫 `gyo_record_code_page_directory` | `pu-x16-code-page-check-misses-nothing`：留 | kernel-scaffold（＝§5.2 第 6 列） | kernel 測試執行檔在 `tests/common/kernel` 自動帶 manifest。`plugins/` 不在 regex → 那裡的執行檔不查；今天 framework 也不查（DI 1 不建執行檔所以無害）。合成夾具 `tests/common/plugins/fixtures/`（plan 3.11）在 tests 下，有查 |

### 2.7 `gyo_expect_os_links`（`tests/common/os/CMakeLists.txt:54-61`，configure 時；owner Engine os）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| — | `gyo_os` 的連結恰等於 `GYO::Base`＋系統庫（Windows `shell32`、其他 `Threads::Threads`） | os.md DI 3 → 只在 legacy | 留 | `os-g8-link`、`os-g18-windows-extra-link`、`th-x2-os-without-thread-library`：留 | kernel-scaffold；pageguard-os 重判（＝§5.2 第 7 列） | 頁保護的 `mprotect`／`sigaction` 在 libc、`VirtualProtect`／VEH 在 kernel32（MSVC 預設連結）→ 連結清單不變，不會誤判（ci-readiness §4.1、legacy-impact §2.2 同判） |

### 2.8 `tests/common/os/OsHeaderHygieneTests.cpp`（ctest `os.header_hygiene`；owner Engine os）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| (1)(2) | `engine/os` 公開標頭不帶進 `windows.h`／winsock、不定義 `min`／`max`（:17-27、:50-57） | os.md DI 4 → 只在 legacy | 留 | `os-g17-hygiene-coverage`：留 | pageguard-os（條件；不採時不碰；＝§5.2 第 8 列） | 無 |
| (3) | `engine/os/include` 的每個標頭都被本測試 include（:61-85） | os.md DI 4（覆蓋保證） | **改寫型**（加一行 include） | 同上 | pageguard-os | 頁保護平台層新增公開標頭（例 `engine/os/PageGuard.hpp`）→「not included by OsHeaderHygieneTests.cpp」失敗，直到本檔加 include。正確的提醒，不是拉回 |

### 2.9 platform-sdl G1／G2／G3（`tests/common/platform_sdl/HeaderHygieneTests.cpp`、`CMakeLists.txt:66-94`；ctest `platform_sdl_header_tests`、configure 時 `gyo_expect_private_sdl_backends`；owner Engine platform_sdl）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| G1 | SDL 後端的非 native 公開標頭不帶進任何 SDL 標頭（:9-31，include 清單寫死、以 `if(TARGET GYO::InputBackendSDL)` 等條件掛入 CMake :38-53） | platform-sdl DI 1 → 只在 legacy（現況；runtime-composition §4 說第三方對接由 plugin 承擔，但沒寫 plugin 公開標頭可不可以露 SDL） | **改寫**：`SdlInput` 搬進 sdl-window-runner plugin 後，`GYO_HYGIENE_SDL_INPUT` 分支（:12-15）因 `if(TARGET)` 不成立而**靜默消失**；plugin 側要自己決定要不要同性質的守衛（§4 缺口 2、§5 Q2） | `input-and-present/scripts/mutations.json` 的 `m19b`（`engine/input/backend/sdl/CMakeLists.txt` 連 Native PUBLIC）：隨 SdlInput 搬入 plugins-frame **重列**、檢查者 Fable；`m19`、`m19c`、`m19d`、audio 的 `au-sdl-public-sdl`：留 | wrap-render（＝§5.2 第 9 列）；**plugins-frame（≠§5.2：第 9 列只寫 wrap-render，SdlInput 搬入在第 17 列才提）** | 不會誤判 kernel（無 SDL）。風險是**空轉**：`if(TARGET …)` 讓搬走的後端自動退出檢查，沒有任何失敗提醒 |
| G2 | `engine/**/include/**` 非 native 的每個公開標頭不含 `SDL_\w+\|SDLK_\w+\|SDLCALL\|<SDL3/`（:82-109；allowlist 空、只能縮） | platform-sdl DI 1 → 只在 legacy（現況） | 留（自動涵蓋 `engine/kernel`、`engine/asset` 新標頭） | 同上 | wrap-render | 不掃 `plugins/`：sdl-window-runner 的公開標頭露 SDL 型別無人判——若 target 認為 plugin 本來就是 SDL 的對接層，這是設計；若不是，是缺口（Q2） |
| G3 | 列名的 target（`gyo_platform_sdl`、`gyo_platform_sdl_main` 無條件；四個後端 `if(TARGET)`）介面裡 SDL3 與 `*Native` 只以 `$<LINK_ONLY:>` 出現、`native/include` 不公開（:66-94） | platform-sdl DI 2 → 只在 legacy（現況） | **改寫**（target 名單改成遷移後的名字；搬進 plugin 的後端在 plugin 側另判） | 同上 | wrap-render、plugins-frame | wrap-render 若改名 `gyo_platform_sdl`／`gyo_platform_sdl_main`→ `gyo_expect_private_sdl` 對不存在的 target 讀屬性失敗（硬錯誤，好）；四個後端改名或搬走→ `if(TARGET ${backend})`（:89）靜默跳過（空轉）。plugin target（例 sdl-window-runner）連 SDL3 PUBLIC 不在名單、不會被判 |

### 2.10 `tests/framework/framework_dependency_guard.py`、configure 時 `gyo_expect_framework_links`（`tests/framework/CMakeLists.txt:315-350`）、PV／fw-* 突變（ctest `gyo_framework.dependency_guard`、`_selftest`；owner framework）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| DI 1 | framework 不是產品：無 `project.json`、CMake 不 `add_executable`／install、兩份 registry 無 `framework` 列、被移除的 allowlist 不得回來（:42-46、:50、:229-232） | framework DI 1 → **只在 legacy**（runtime-composition §4：framework 概念淘汰、DI 整節廢止） | **退役**（framework-delete）。接手：plugin 的「可選可刪」由 plugins-foundation 的刪除驗證守（plan 3.11 判定 2）；但「plugin 不是產品」（不建執行檔、不 install、不在 registry）**沒有條文、沒有守衛**→ §4 缺口 4 | `framework_mutations.json` 的 `fw-guard-executable`、`fw-guard-allowlist-returns`：退役 | framework-delete（＝§5.2 第 10 列） | framework-delete 前都在跑，對新程式無影響（只掃 `framework/`，:234）。刪掉 `framework/` 後 `rglob` 回空、守衛**空轉通過**（Python `Path.rglob` 對不存在的目錄回 `[]`，本卡實測），只剩 registry 檢查——所以不能「先刪程式、守衛晚一批」 |
| DI 2 核心 | core 只 include `gyo/framework/`、`engine/`（非 native）；程式不名 `SDL_`／`ImGui`／`nlohmann`；CMake 只名正向清單 `CORE["targets"]`（:63-65：ToolFramework×3、Engine、Os、PlatformSDL、Input、InputBackendSDL、Ui、Model、Audio、Render、RenderBackendSDLGPU、RenderBackendSDL）；每個 core 庫只連 `CORE["links"]` 列的（:69-74） | framework DI 2、6（shell 不連 render）；target：runtime-composition §4「每個 plugin 一個 target、自己的依賴正向清單」、DI 2「Engine 不 include plugins」→ **性質兩邊都有、owner 不同** | framework 側**退役**（framework-delete）；plugin 側是**新增**（plugins-foundation 的依賴守衛；§5.2 寫「改寫」——owner 與掃描根都換了，實際是另寫一支，不是改這支） | `fw-guard-*`（framework_mutations 10 條、framework_preview 11 條、event_sounds 1 條 `es-guard-audio-backend-listed`、framework_shell 3 條 CMake）：退役；`fw-link-*`（CMake 連結拼法的 6 條）：退役 | framework-delete；plugins-foundation（新增） | **過渡期（階段 2～4）會判新程式**：assets-core 的「`AssetManager` 轉接（期限 framework-delete）」若讓 framework 的 `ReadOnlyAssetCatalog`／`CatalogPlan` 連新 target（例如 `GYO::Asset`、`GYO::Kernel`）或 include `engine/kernel/`，`names GYO::Asset`／`gyo_tool_framework links GYO::Asset` 立刻失敗（:217-219、:194-195）。今天 `engine/asset` 編進 `engine` target（`engine/CMakeLists.txt` 無 `add_subdirectory(asset)`），所以只要 assets-core 不開新 target 就不會碰到；開了就要改 DI 守衛的正向清單（§14.4 交使用者）→ Q3 |
| DI 2 綁定 | `framework/bindings/<name>/` 各有正向清單（imgui、json；:82-103）；只有 imgui 可 include 兩個 `*Native.hpp`；無綁定清單的目錄失敗 | framework DI 2 → 只在 legacy | 退役；plugin 側新增（imgui plugin、editor-dialogs 各自清單） | 同上 | framework-delete；plugins-editor | 同上；plugins-editor 把 `ImGuiShell` 搬出 `framework/bindings/imgui/` 後，留下的空目錄會被判「has no positive list」？不會——目錄沒了就不掃；但 CMake link check 對 `framework/` 不存在的目錄 `get_property(DIRECTORY …)` 會 configure 失敗（:305-313）→ **tests/framework 必須與 framework/ 同批刪**（它在 `GyoBuild.cmake:91` 只隨 `GYO_ACTIVE_TOOLS OR GYO_ENABLE_FRAMEWORK` 加入） |
| DI 3 | framework 任何檔不名 `engine/config/tools.csv`／`projects.csv` 登記的 id | framework DI 3 → 只在 legacy；target 對「共用 plugin 中性」無條文（產品自己的 (c) plugin 本來就名產品）→ §4 缺口 5 | 退役 | `fw-guard-tool-header`、`fw-guard-tool-name`：退役 | framework-delete | 無 |
| link check | configure 時逐 target 讀真實連結：綁定只能是 imgui／json；core 的 `gyo_tool_framework`、`_window` 不得到達 render target（:315-350） | framework DI 2、6 → 只在 legacy | 退役 | `fw-link-*`、`fw-shell-core-render`、`fw-shell-core-native`：退役 | framework-delete | 同 DI 2 綁定列：framework/ 不存在時硬失敗，逼同批刪 |
| PV1～PV7 | 預覽：建立失敗即停（PV1）、每幀獨立且 `EndFrame` 交出本幀 readback（PV2）、尺寸（PV3）、相機（PV4）、`UploadFrame` 貼圖（PV5）、軌道相機數學（PV6）、只有 preview 連 render（PV7） | preview.md DI 1～5；target runtime-composition §3b（資料只往前流、readback 消失）：**DI 4 被取代**，其餘相容 → PV2／PV5 只在 legacy、PV1／PV3／PV4／PV6 兩邊都有、PV7 併入上面的 link check | PV2、PV5 **退役**（接手：H-6／J-43 的 Extract 測試＋plugins-editor「預覽無 readback」掃描，plan 3.24）；PV1／PV3／PV4／PV6 **改寫**（搬進 offscreen-preview plugin 的測試） | `framework_preview_mutations.json` 25 條：`pv-stale-frame`、`pv-no-render`、`pv-queue-not-reset`（PV2）與 `pv-upload-*` 4 條（PV5）退役 7；`pv-create-ignores-bundle-error`、`pv-size-ignored`、`pv-camera-ignored`、`pv-orbit-*` 4 條重列 7、檢查者 Fable；`fw-*` 11 條退役 | plugins-editor（PV2～PV5）；framework-delete（＝§5.2 第 10 列） | PV2／PV5 的判定（`EndFrame` 回 readback、`UploadFrame` 貼圖）**就是 readback 形狀**：若把 PreviewTests 搬進 plugin 不改判定，就把「每幀 readback 再上傳」帶進渲染 World（§3 衝突 2）。這是本表最明確的「守衛把架構拉回舊形狀」案例 |

### 2.11 工具守衛 ×4（`tests/model_viewer/model_viewer_dependency_guard.py`、`tests/ui_editor/ui_editor_dependency_guard.py`、`tests/audio_editor/audio_editor_dependency_guard.py`、`tests/voxel_map_editor/voxel_map_editor_dependency_guard.py`；各工具 ctest `<tool>.dependency_guard`；owner 各工具）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| (a) 不連遊戲、不名他人 | CMake 的 `Name::Name` 與 `target_link_libraries` 只名正向清單或工具自己的 target；不名 registry 裡其他 id（model_viewer DI 6、voxel DI 6） | 四工具 DI 1 的前半＋DI 6 → 兩邊都有（工具中性不變） | 改寫（名單換成 plugin target；性質不變） | 工具突變清單針對守衛的：`mv-guard-*` 3、`tc-guard-*`／`vw1-guard-*` 5、`ui-guard-*` 3，及針對工具 CMake 的 `*-guard-*-target` 約 12 條：**重列**（守衛改寫後對新清單重做），檢查者 Fable | plugins-foundation 初判 → 工具四批（＝§5.2 第 11 列） | `ALLOWED_TARGETS`（model_viewer :43-47、ui_editor :31-36、audio_editor :31-32、voxel `CORE_TARGETS`／`WINDOW_TARGETS` :50-69）**都沒有 `GYO::Kernel`、沒有任何 plugin target**；`ALLOWED_INCLUDE_PREFIXES` 只有 `gyo/<tool>/`、`gyo/framework/`、`engine/`（voxel 另 `window/`）。工具批第一個 include `engine/kernel/…`（算 `engine/` 前綴，過）但第一個連 `GYO::Kernel` 或 plugin target 的 commit 就失敗。每條新邊都要改 DI 守衛→ 交使用者。**不改就只能留在 framework 上**——這正是「拉回舊形狀」的壓力 |
| (b) SDL 只經 Engine／plugin | 不名 `SDL_`、不 include `SDL3/`（ui_editor 例外 :30）、不 include `*Native.hpp`（ui_editor 的 `NATIVE_FILES` 例外） | 四工具 DI 1 後半 → 兩邊都有 | 留（性質）＋改寫（例外清單） | 同上 | 工具四批 | ui_editor 的 `GYO::RenderBackendSDLNative`、`NATIVE_FILES` 例外是 platform-sdl RC 2 說的「until it moves」；plugins-editor 的 imgui plugin 接手後例外要刪，否則守衛允許工具繼續直連 native |
| (c) framework 只經列名 target | 正向清單含 `GYO::ToolFramework`、`GYO::ToolFrameworkImGui`／`Json`／`Preview`、前綴 `gyo/framework/` | 四工具 DI 1「comes through the framework first」→ **只在 legacy**（legacy-impact §2.3 被取代） | 改寫（刪 framework 項、加 plugin 項；性質改為「組合只經 plugin」） | 同上 | 工具四批 | 四個 `ToolFramework*` 項留著不刪，工具就可以**同時**連 framework 與 plugin（半遷移，違 H-7 精神）。要在該工具批刪掉 |
| voxel 結構 | core／window 兩部分：只有 `gyo_voxel_map_editor_window`（gui 變體）可連 Render／Preview／ImGui；`WINDOW_ONLY_INCLUDES = engine/render/, engine/platform/, gyo/framework/Preview, gyo/framework/WindowShell`（:54）；gui 分支以 `if(GYO_TOOL_VARIANT STREQUAL "gui")` 區段判（:139） | voxel DI 1 → 只在 legacy（結構）；target：文件 World＋渲染 World（runtime-composition §3b）是另一種切法 | 改寫（從 target 的兩個 World 重導規則，不是補清單） | `vw1-guard-*`、`vmw-guard-core-links-render`：重列 | tool-voxel | 新切法裡「文件 World 的編輯 System」與「渲染 World 的 extract」若不照 core／window 兩個 library 落檔，守衛的 `WINDOW_LINKERS`／`gui_spans` 規則就判違規；守衛在逼舊的 library 切法 |

### 2.12 `build/cmake/GyoComponents.cmake:3-5`（`FRAMEWORK` ∈ `GYO_KNOWN_COMPONENTS`）、`build/cmake/GyoBuild.cmake:55-58、91-93`（`GYO_ENABLE_FRAMEWORK`；build 層，owner Build）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| component | framework 是產品以資料選用的共用元件；未知元件名 configure 失敗（`gyo_validate_components` :7-13） | framework DI 1 → 只在 legacy；target Delta-5：plugin 選擇是 `project.json` 的 `plugins` 欄（另一條路） | 留到 framework-delete 再**退役**（＝§5.2 第 12 列） | 無突變針對這兩檔 | plugins-foundation 初判 → framework-delete | `gyo_validate_components` 對未知名 FATAL：plugin 若被當成 component 名寫進 `components` 欄會被拒——計畫用獨立 `plugins` 欄，不衝突。`GYO_ACTIVE_TOOLS OR GYO_ENABLE_FRAMEWORK`（:56、:91）讓 framework 與 `tests/framework` 在**任何**工具啟用時都 configure，與工具是否還用 framework 無關→四工具遷完後 framework 守衛仍跑（無害，但是 Dummy 的形狀，§7） |

### 2.13 `tests/common/cmake/ToolRegistryTests.cmake:100-102`（保留名 `framework` 不可當 tool；ctest `cmake.ToolRegistry`；owner Build）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| — | registry 拒絕名為 `framework` 的 tool | framework DI 1 → 只在 legacy | 退役（§7 不留 Dummy；＝§5.2 第 13 列） | 無 | framework-delete | 無影響；留著只是殭屍規則 |

### 2.14 threads／time（`tests/common/threads/RoleThreadTests.cpp:94-115`、`tests/common/time/WaiterTests.cpp:31-36`、`tests/common/time/CMakeLists.txt:42-53` 連結期望；owner Engine threads／time）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| DI 6 | 角色名回報、截斷仍回報全名（Linux 15 bytes） | threads DI 6 → 只在 legacy（現況；spec E.3 以 `RoleThread` 為 Host 建執行緒的手段） | 留 | `time-threads-trace/scripts/mutations.json` RoleThread 2 條：留 | threading（＝§5.2 第 14 列） | 無 |
| DI 7 | `WaitBackend` 名字是消費者 log 的契約 | time DI 7 → 只在 legacy | 留 | Waiter 6 條：留 | threading | 無 |
| links | `gyo_time` 恰連 Base、Threads::Threads；`gyo_threads` 恰連 Time、Threads::Threads、Os（Os 私有） | time DI 10、threads DI 9 → 只在 legacy | 留 | `ti-x1`、`ti-x2`、`th-x1`：留 | threading | kernel-threading 若把 runner 介面或交換點原語（Mailbox／Link swap）寫進 `engine/threads` 並連 `GYO::Kernel`→失敗。正確方向：原語在 kernel，threads 不認 kernel |

### 2.15 trace（`tests/common/trace/CMakeLists.txt:12-20` 連結期望、`TraceSinkTests.cpp`；owner Engine trace）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| links | `gyo_trace` 恰連 Threads、Os（Os 私有） | trace DI 7 → 只在 legacy | 留 | `tt-x1`、`tt-x2`、`tt-x7`：留 | diagnostics-service（＝§5.2 第 15 列） | diagnostics-service 若把 `DiagnosticsWriter<T>` 的接收端放進 `engine/trace` 而連 kernel→失敗。正確方向（kernel→trace） |
| DI 1～5 | Push 無 IO 不等待、滿即丟並計數、守恆、end line 最後、順序 | trace DI 1～5；target S-8（Diagnostics 只寫不讀回）、B-13（World 端緩衝在交換點交出）→ 兩邊都有 | 留；`:155` 形狀的契約測試**新增**（§5.2 第 15 列） | TraceSink 6＋7 條（兩清單）：留；若服務化改動 `TraceSink.cpp` 文字→重列、檢查者 Fable | diagnostics-service | 無；注意 trace RC 1（重放記錄不得走丟棄佇列）：G-4 的 (c) 輸入記錄若走 TraceSink 會觸發 RC 1（legacy-impact §2.2 已記） |

### 2.16 葉子模組連結期望與 audio 守衛（`gyo_expect_collision_links`、`_model_links`、`_map_markers_links`、`_voxel_mesh_links`、`_audio_links`；`gyo_audio_board_readers`、`gyo_audio_alloc_tests`；owner 各模組）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| collision | 恰連 Math、Base | collision DI 3 → 只在 legacy（現況） | 留 | `grid-links-voxel-mesh`：留 | — | 無 |
| model | 恰連 Engine、Math、json | animation-events DI 1 → 只在 legacy | 留 | `ae-model-links-audio`、`es-link-model-json-public`：留 | assets-core（＝§5.2 第 16 列） | loader 由 plugin 登記（plan 3.11）；`engine/asset` 今天在 `engine` target 內，所以 model 用新的 `Assets<T>`／登記 API 不改連結。**若 assets-core 另開 `GYO::Asset` target**，model 連它就失敗→ DI 變更交使用者（Q3） |
| map_markers、voxel_mesh | 恰連 Base、Math、json | map-markers DI 2、voxel-mesh DI 1 → 只在 legacy | 留 | `mm-links-voxel-mesh`、`vm-links-collision`：留 | — | 無 |
| audio links | `gyo_audio` 恰連 Base、Time | audio DI 2 → 只在 legacy；target spec I.1「`Submitter` 是帶親和的 Resource」 | 留；Submitter 要認 kernel 的 Resource 型別時＝連 `GYO::Kernel`＝DI 2 變更交使用者（§5.2 第 16 列同判） | `au-link-engine`：留 | wrap-render | wrap-render 把 Submitter 包成 Resource 若在 `engine/audio` 內 include kernel 標頭→失敗。正確：包在「包」層（Host 零件），audio 不認 kernel |
| board readers | 只有 `Mixer::PlayheadOf` 讀 playhead board（`CheckBoardReaders.cmake` 掃 `engine/audio/src`） | audio DI 4 → 只在 legacy | 留 | audio 清單 Mixer／PlayheadBoard 的相關條：留 | wrap-render | 無 |
| alloc tests | `Render` 不配置、不鎖、不拋 | audio DI 7 → 只在 legacy | 留 | 留 | — | 無 |

### 2.17 input 與 ui（`tests/common/input/SdlInputTests.cpp` IP-1 判定、`input-and-present/scripts/mutations.json` m7～m9；`tests/common/ui/ui_dependency_guard.py`，ctest `ui.dependency_guard`；owner Engine input／ui）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| IP-1 | 鍵重複不是事件、失焦釋放不進 `events`、`text` 只在文字輸入開著時 | input DI 2、3 → 兩邊都有（frame 成 boundary Resource，內容不變；audit） | 改寫（搬：`input_sdl_tests` 連 `GYO::InputBackendSDLNative`，後端搬進 sdl-window-runner plugin 後測試跟著搬到 plugin 的 tests） | `m7`、`m8`、`m9` 與 SdlInput.cpp 的其餘 12 條、`SdlInput.hpp` 2 條、input backend CMake 1 條＝18 條：**重列**（`file` 路徑變）、檢查者 Fable；SdlPlatform 等 65 條：留 | wrap-render（input core）；plugins-frame（SdlInput 搬入）（＝§5.2 第 17 列） | `tests/common/input/CMakeLists.txt:14` `if(TARGET GYO::InputBackendSDL)`：後端 target 從 engine 消失時測試**靜默不建**（空轉），不會提醒要搬 |
| ui DI 2 | `engine/ui` 不 include `engine/input/`、CMake 不名 `GYO::Input*` | ui_toolchain GYO::Ui DI 2 → 只在 legacy（現況，持續有效） | 留（＝§5.2 第 17 列） | 無 | wrap-assets | wrap-assets 的 `UiRenderer` 局部重寫若讓 `engine/ui` 直接吃 `PhysicalInputFrame` Resource→命中。正確：產品轉成 `UiInputFrame` |

### 2.18 `tests/object_fps_pvp/EntityModelTests.cpp`（G1、G2、G3、G7、G13；ctest `object_fps_pvp.cpu`）、`RequireEntityOrder`（`PvpMatch.cpp:674`，GYO_ASSERT）、`authority_digest`（G11，37 golden；ctest `object_fps_pvp.authority_digest`；owner object_fps_pvp）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| G1-i | 疊代順序只來自 id：降冪 Join（3,2,1）、Leave、再 Join 更小的 id 後，snapshot 三個 vector 與七個私有欄都是 {1,2,3} 升冪 | entity-model E-5 前半 → target world DI 1／DI-KS 3／J-1、J-2 → 兩邊都有 | **退役**（機制部分由 World 的 J-1／J-2 接手，同批） | `entity-split-snapshot-reversed`、`entity-split-respawn-reversed`（PvpMatch.cpp）：退役；`entity-store-adapter-key-not-player-id`（`Sim/EntityColumns.hpp`）：留或隨拆欄重列 | match-sim（＝§5.2 第 18 列） | 測試是 `PvpMatch` 的 **friend**（:19-38 `PvpMatchTestAccess` 直接讀 `bodies_`、`combats_`、`animations_`、`actionLedgers_`、`connections_`、`tickShapes_`、`actionStartCombats_`）。要讓它編譯，`PvpMatch` 就必須保留七個私有 vector——這正是舊形狀。match-sim 把欄搬進 World 的第一個 commit 就編不過；若為了讓它過而留一個 `PvpMatch` 門面包著 World，架構被拉回。**必須同批退役／改寫，不能拖** |
| G1-ii | 欄成員關係恰等於種類表：每個活著的實體在每個欄都有一列（`tickShapes` 除外） | E-5 後半 → target **無條文**（J-1 只保證 keys 升冪、`entities` 是超集合，不保證「每個 Player 都有每個欄」）→ 只在 legacy | **改寫**（成產品對 World 的掃描／測試：種類表 M 的實體在它的所有欄都存在） | 同上 | match-sim | 不寫就消失（§4 缺口 6） |
| G2-i | Leave 後實體從每個欄與 tick 快取消失、再 Join 不撞重複插入 | E-7 → world DI 5（despawn 同窗刪所有欄）、J-32 → 兩邊都有 | 退役（機制由 F.2 despawn 階段接手） | `entity-split-erase-skips-animations`、`evict-animation-orphan-erase-skips-animation`（PvpMatch.cpp）：退役 | match-sim | 同 G1-i（friend 存取） |
| G2-ii | Leave 後產品 API：`GetActionResults`／`GetMovementQuality`／`GetActionTiming` 空、`AdmitInput`＝`UnknownPlayer`、snapshot 只剩 2 | 產品語義（E-7 的結果面）→ 只在 legacy | 改寫（對 ingress／結果 System 的新入口重寫） | — | match-sim | 判定綁 `PvpMatch` 的方法名；留著就要求新路徑保留同名方法 |
| G3 | host 逐出（連線品質）後 publication 的 players／combat／animation id 序列一致 | E-7、E-8；target：snapshot 由 World 欄組裝（J-1 超集合）＋逐出在 Ingress／runner → 兩邊都有 | 改寫（host 側） | `evict-animation-orphan-host-erases-two`（MatchRuntimeHost.cpp）：重列、檢查者 Fable | **match-host（≠§5.2：第 18 列只寫 match-sim）** | 測試用 `MatchRuntimeHost(arena, clock lambda)`、`QueueJoin`／`SubmitInput`／`Advance`／`TakeSnapshot`／`TakeEvictions`（:160-202）——host 的 API 形狀。match-host 把這些變成 Ingress System 與 runner 後，不改測試就得保留 host 門面 |
| G7 | 同 tick 重生依 id 升冪放置（平手鍵） | E-6 → target J-2（疊代序）＋產品規則 → 兩邊都有（規則面只在產品） | 改寫（對新 Life System 重寫；用 World 查詢而非 `PvpMatchTestAccess::Bodies` 改私有欄 :261-265） | `entity-split-respawn-reversed`：重列 | match-sim | 直接改 `bodies_`（friend）：同 G1-i |
| G13 | 命中形狀用的是本 tick 開頭的 combat（第一槍的紀錄），不是預設也不是第二槍的結果 | E-8 衍生的產品語義（entity-model §7 G13）→ target：可寫成 Plan 謂詞（`ActionStartCombat` 欄的寫者在 `ResolveActions` 之前、`TickShapes` 讀者在其後；G1 閘門 (iii) 的 Life 謂詞同形）→ 只在 legacy（尚無條文） | 改寫（優先改成 Plan 謂詞＋一個 System 測試） | `entity-split-action-start-stale`：重列 | match-sim（plan-dryrun 可先列謂詞） | 測試用 `PvpMatch(arena, rules…, recording)` 建構與 `SubmitActions`／`Snapshot`（:204-247）→ 綁 PvpMatch API |
| RequireEntityOrder | 發布前 snapshot 各 vector 升冪且 animation id 先行（GYO_ASSERT；37 golden 每 tick 經過） | E-5／E-7 → J-1 → 兩邊都有 | **退役**（J-1 由 World 守；§5.2 寫「改寫」——函式在 `PvpMatch.cpp:674` 隨 PvpMatch 刪，沒有可改寫的本體） | pvp 清單 :7802 的 anim 條（after match-entity-split RequireEntityOrder asserts…）：重列 | match-sim | 無（刪即可） |
| G11 golden | 37 個情境的 authority digest 一位元不變；加種類的批不改 golden | E-9 → spec J.4（遷移後 golden 不變是驗收）→ 兩邊都有 | 留；LIM-5 的差由 match-sim 事前宣告列名、§14.4 1～5、驗證者複核、使用者核准（U9） | `AuthorityDigest.cpp` 1 條、`compare_authority_trees.py` 1 條：留 | match-sim | 無；它是遷移的驗收尺，不是拉回 |

### 2.19 `tests/object_fps_pvp/PvpMatchTests.cpp:1774`（network §7「連線中斷與重新對時」DI 的註解；OutageRig 四個 TEST_CASE :1947-2076）、`:82`（§7「被替代序號的單一紀錄」S3）、`:2379`（client-interrupted）、`test_diagnostic_isolation.py:52、:155`、`fixtures/ingress_v1/README.zh-Hant.md:59`（owner object_fps_pvp）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| outage | 連線中斷／恢復時 Host 與 Client 預測的行為：第一份副本晚於 N_o 不產生樣本、改發 Resync epoch；held／partial／sparse 三案的值 | network §7 連線中斷 7 條（legacy-impact §2.1：相容）；target spec I.2 Link 是 Resource、G.4 到達時刻進 ring、Resync 由 (c) System 從 ring 算 → 兩邊都有 | **改寫**（OutageRig 以 `LocalPlayerPrediction`＋`MovementTrace`＋連線模型驅動（:1783-1790）→ 預測改走回滾機制後要以新 Runtime 重寫；§5.2 寫「留＋改引用」，但測試本體隨型別刪，不可能原樣留） | `mr-*`（match-resync 10）、`ci-*`（client-interrupted 18）、`csc-*`（8）針對 `ClientConnection.cpp`、`LocalPlayerPrediction.cpp`、`MatchSubstitutionRecords.hpp`：重列、檢查者 Fable | **client-sim（OutageRig 的預測側）＋match-host（帳本側）（≠§5.2：第 19 列只寫 match-host）** | OutageRig 建構 `LocalPlayerPrediction` 並持 `std::shared_ptr<MovementTrace>`、clock（:1783-1790）：固定了預測物件的 API 與「trace 是建構參數」的形狀（pvp-diagnostics 要改成句柄）。留著就逼 client-sim 保留 `LocalPlayerPrediction` 門面 |
| S3 單一紀錄 | 替代紀錄只由 `MatchSubstitutionRecords` 持有、帳本讀它；`late_first == slack_late + late_beyond_no` | network §7 單一紀錄 3 條 → target：Resource 與唯一寫者（DI-KS 6）→ 兩邊都有 | 改寫（成獨占欄的啟動檢查＋一個等式測試） | 同上 | match-host | 無拉回；C++ 守恆檢查搬進 Ingress System 即可 |
| 註解引用 | 測試註解與 fixtures README 指向 network §7 | — | 改引用（closeout 隨 legacy 刪） | — | closeout | 刪檔時懸空 |

### 2.20 `apps/object_fps_pvp/gateway/state_whitelist_test.go`（`go test ./gateway/...`；owner object_fps_pvp Gateway）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| — | Gateway 每個狀態欄位、套件變數、goroutine 本地 map／slice 都屬恰一個允許類別；沒有類別持有 Match 判定狀態的副本（:14-39、:52-） | network §7 Gateway 無狀態邊緣 4 條＋AGENTS §3 State Authority；target spec I.1「Gateway（Go）外部，不變；維持無狀態」→ 兩邊都有 | 留（＝§5.2 第 20 列） | pvp 清單 Go 201 條（`server.go` 71、`runtime_link.go` 53、`adapter.go` 28、`action_delivery.go` 25、`session.go` 11 等）：留 | match-host | 無；match-host 若改 IPC 欄位，新欄位要分類，是正確提醒 |

### 2.21 註解引用 `docs/architecture.md`（`tests/common/scripts/neutral_names_guard.py:3` Layers check 1；`tests/common/collision/CMakeLists.txt:10` module table）

| 子列 | 性質 | 來源條文 | 去處 | 突變 | 批 | 不動會判誰違規 |
|---|---|---|---|---|---|---|
| — | 無判定，只是出處註解 | architecture.md（legacy；Layers 表被取代） | 改引用（指 target 的對應頁：naming／runtime-composition）；§5.2 寫「留」，性質無差、只差註解 | 無 | closeout（＝§5.2 第 21 列） | 無；刪 architecture.md 時懸空 |

### 2.22 補遺：不在 21 列、但同屬「守衛」且會被遷移批碰到（不計入 21；給各批事前宣告對帳）

| 守衛 | 性質 | 去處（初判） | 批 |
|---|---|---|---|
| `tests/object_fps_pvp/test_kernel_prep_sites.py`（ctest `object_fps_pvp.kernel_prep_sites`；D2 棘輪白名單） | 權威的時鐘讀取（W）、全域診斷入口（G）、非模擬執行緒碰權威（A）三類站點的檔／函式／計數，只能縮 | **改寫型**（決定行先例：搬檔只改檔名欄、不放寬）；Match 側 G 6 列→pvp-diagnostics 歸零、A 22 列→match-host 歸零、Client 側 7 列→client-sim | pvp-diagnostics、match-host、client-sim |
| `tests/object_fps_pvp/KernelPrepDetectTests.cpp`（ctest `object_fps_pvp.kernel_prep_detect`；D1 Life 時序、D3 failures 寫點） | 今天的行為特徵（T＝35／R＝215；host 在判定 tick 寫 failures） | 它是「characterization、給遷移批決定對照」（檔頭）。match-sim 以新路徑實測 D1（plan 3.14 判定 (2)）後：時序不變→改寫為新路徑測試；時序改變→退役並記錄（Q6） | match-sim、match-host |
| `tests/object_fps_pvp/test_zone_c_lists.py`（ctest `object_fps_pvp.zone_c_lists`）＋`zone_c_scan_*`（J-17 掃描器） | 位置＝區：`src/Sim/*.cpp` ＝ confirmed＋candidates；Sim 不 include Present；殘留計數固定 | **留**（target 側守衛）；match-sim 在 `src/Sim` 新增 (c) System 檔必須同時列進 `zone-c-sources.json`（新檔未列→失敗，正確）；`fps::pvp::sim` namespace 改名不影響 | match-sim |
| `tests/common/zone_scan/`（`zone_scan.selftest`；ZS1～ZS7） | J-17 的規則本體 | 留（target 側；h7-scanner 可重用形狀） | — |
| `tests/object_fps_pvp/sdl_symbol_guard.py`＋`CMakeLists.txt:199-237` SDL link guard | 產品、探針、測試不直接呼叫 SDL（`DIRECTORIES` 三個產品目錄） | 留（產品中性不變）；client-frame 把 `PvpApplication` 改成 plugin 資料＋Run 後仍成立；`sdl-*` 34 條（PointerCapture 31 等）隨 client-frame 重列 | client-frame |
| `tests/object_fps_pvp/test_diagnostic_isolation.py`（ctest `object_fps_pvp.diagnostic_isolation`） | 診斷不參與判定（network §7 的隔離性質） | 留（性質）＋改寫（引用 pvp-diagnostics 的新輸出路徑） | pvp-diagnostics |

---

## 3. 突變清單 26 檔與執行器 3 支

去處定義：**留**＝目標檔不動、`find` 文字不變、照原清單重跑；**重列**＝目標程式被搬或重寫，清單條目要對新程式重寫（含 `file`／`find`／`expect`），並照 §14.2 由與作者不同、非小模型的檢查者敵對檢查；**退役**＝針對被刪程式或被取代性質，刪條目並在該批 dev_log 記錄。計數是初判，以各批事前宣告為準。檢查者模型預設 Fable（理由見 §1）。

| # | 清單 | 條 | 針對的程式與去向 | 去處（條） | 批 | 檢查者 |
|---|---|---|---|---|---|---|
| 1 | `docs/architecture/plans/audio/scripts/mutations.json` | 85 | `engine/audio/src/*`（Mixer 43、VoiceMixer 23、PlayheadBoard 5、ClipBank 1）、SDL 後端 10、CMake 2：留；`engine/asset/src/loaders/SoundLoader.cpp` 1：loader 改收位元組（-zones B 5 處、target/assets §1 第 3 點）→重列 | 留 84／重列 1 | assets-core（SoundLoader）；wrap-render（其餘留） | Fable |
| 2 | `audio/scripts/mutations-asan.json` | 1 | Mixer：留 | 留 1 | — | — |
| 3 | `audio/scripts/mutations-tsan.json` | 6 | Mixer SPSC：留 | 留 6 | — | — |
| 4 | `big-map/scripts/engine_format_mutations.json` | 68 | VoxelMesh.cpp 41、MapMarkers.cpp 24、StrictJson 1、CMake 1、`docs/architecture/voxel-mesh.md` 1（neutral 守衛的突變）：留 | 留 68 | — | — |
| 5 | `big-map/scripts/engine_grid_mutations.json` | 19 | StaticAabbGrid、StrictJson、CMake：留 | 留 19 | — | — |
| 6 | `entity-store/scripts/mutations.json` | 15 | `engine/entity_store/include/*.hpp`、CMake：留；**條件**：world-exec 改 store API（Delta-3）時 `find` 過期→重列 | 留 15 | world-exec | Fable |
| 7 | `event-sounds/scripts/event_sounds_mutations.json` | 33 | `framework/src/EventSounds.cpp` 20＋`framework/bindings/json/src/EventSoundsText.cpp` 11→event-sounds plugin：重列；`tests/framework/framework_dependency_guard.py` 1：退役；`engine/model/CMakeLists.txt` 1：留 | 留 1／重列 31／退役 1 | plugins-frame | Fable |
| 8 | `framework-preview/scripts/framework_preview_mutations.json` | 25 | PV2（3）、PV5（4）readback／upload 路徑：退役；PV1／PV3／PV4／PV6（7）→offscreen-preview plugin：重列；`fw-*` CMake 8＋guard 3：退役 | 重列 7／退役 18 | plugins-editor；framework-delete | Fable |
| 9 | `framework-rework/scripts/framework_catalog_mutations.json` | 12 | CatalogText 5＋CatalogPlan 5→asset-catalog plugin：重列；json CMake 1：退役；`engine/asset/src/ContentManifest.cpp` 1：留 | 留 1／重列 10／退役 1 | plugins-frame；framework-delete | Fable |
| 10 | `framework-rework/scripts/framework_mutations.json` | 26 | `fw-guard-*` 針對 `ReadOnlyAssetCatalog.*`（8，往 core 注入 SDL／tool 名給守衛抓）：退役；CMake 6、guard 4：退役；`AbsolutePath.cpp` 2、`UiTextInput.cpp` 6：重列（搬去哪由 plugins-frame／editor 宣告） | 重列 8／退役 18 | plugins-frame、plugins-editor；framework-delete | Fable |
| 11 | `framework-rework/scripts/framework_shell_mutations.json` | 33 | `ImGuiShell.cpp` 17→imgui plugin；`WindowShell.cpp` 8＋`.hpp` 1→sdl-window-runner；`HeadlessShell.cpp` 3→headless-runner：重列 29；CMake 3：退役；`engine/platform/sdl/src/SdlPlatform.cpp` 1：留 | 留 1／重列 29／退役 3 | plugins-foundation（Headless）、plugins-frame、plugins-editor；framework-delete | Fable |
| 12 | `framework-rework/scripts/io_user_files_mutations.json` | 15 | `engine/io`：留 | 留 15 | — | — |
| 13 | `framework-rework/scripts/platform_path_dialog_mutations-asan.json` | 1 | SdlPlatform：留 | 留 1 | — | — |
| 14 | `framework-rework/scripts/platform_path_dialog_mutations.json` | 17 | `engine/platform/sdl`：留（platform 收成 Host 零件但目錄不動，plan 3.18） | 留 17 | wrap-render | — |
| 15 | `input-and-present/scripts/mutations.json` | 83 | `SdlInput.cpp` 15＋`SdlInput.hpp` 2＋input backend CMake 1＝18→sdl-window-runner plugin：重列（含 m7～m9、m19b）；SdlPlatform 50、SdlLogBridge 7、platform CMake 2、SdlPlatform.hpp 2、SdlMain 1、render CMake 1、SceneCaptureFile 2＝65：留 | 留 65／重列 18 | plugins-frame；wrap-render | Fable |
| 16 | `os-subsystem/scripts/mutations.json` | 149 | `engine/os`、`engine/io`、`engine/asset`、守衛本身（6）、allowlist（1）、OsHeaderHygiene（1）、ProcessCodePageCheck（1）：留；pageguard-os 新增 OS 碼時另加條目（新增，不改既有） | 留 149 | pageguard-os（條件） | Fable |
| 17 | `time-threads-trace/scripts/mutations.json` | 14 | RoleThread 2、Waiter 6：留；TraceSink 6：留，服務化若改 `TraceSink.cpp` 文字→重列 | 留 14 | diagnostics-service | Fable |
| 18 | `docs/tools/plans/v8/scripts/anim_events_mutations.json` | 13 | `engine/model/src/AnimationEvents.cpp`、CMake：留 | 留 13 | — | — |
| 19 | `docs/tools/plans/v8/scripts/audio_editor_mutations.json` | 13 | Conversion 10、SoundExport 1、WavFile 1（工具邏輯）：留；CMake 1（`ae-guard-game-target`，守衛改寫後）：重列 | 留 12／重列 1 | tool-audio | Fable |
| 20 | `docs/tools/plans/v8/scripts/framework_mutations.json`（工具對 framework 的守衛與 shell 採用） | 54 | ui_editor guard 3、audio_editor CMake 4、ui_editor CMake 2、視窗／對話框／預覽／shell 相關（AudioEditorWindow 3、FileAccess 2、PreviewPlayer 5、AssetPreviewContext 1、DocumentSession 2、EditorApp 5、EditorAppCanvas 1、EditorWindow 1、PathDialog 4、PreviewAdapter 6、SdlPathDialogs 3）＝42：重列；工具邏輯（Edit 4、ProjectFile 4、SoundExport 2、Waveform 2）＝12：留 | 留 12／重列 42 | tool-audio、tool-ui | Fable |
| 21 | `docs/tools/plans/v8/scripts/model_viewer_mutations.json` | 41 | `engine/render/backend/*` 8：留；`framework/src/Preview.cpp` 1：退役（readback）；guard 3、CMake 1、Main 2、PreviewRenderer 1、FileAccess 1＝8：重列；EventDocument 6、EventFiles 6、EventHistory 2、EventSoundFiles 6、Playback 4＝24：留 | 留 32／重列 8／退役 1 | tool-model-viewer | Fable |
| 22 | `docs/tools/plans/v8/scripts/voxel_map_editor_mutations.json` | 114 | `engine/math` 5、`engine/render` 1、工具邏輯（Editing 29、MapBuild 16、FlyCamera 11、Visibility 10、Tools 10、Build 9、GridDocument 5、Orientation 3、MarkersFile 2、BoxImport 1、JsonRules 1）＝103：留；guard 5、CMake 2、Main 3、`window/PreviewScene.cpp` 1＝11：重列 | 留 103／重列 11 | tool-voxel | Fable |
| 23 | `docs/tools/plans/v8/scripts/voxel_mesh_mutations.json` | 20 | `engine/voxel_mesh`、StrictJson、CMake：留 | 留 20 | — | — |
| 24 | `spikes/kernel/mutations.json` | 41 | spike 原型（World.hpp 13、Params.hpp 9、AccessCheckTests 7、Access.hpp 6、PageGuard 2、Registry 2、AccessDerivationTests 2）：退役——前提是 registry-schedule／world-exec／pageguard-os 已把 spike 測試搬改成 kernel 的 J-n（plan 3.30 兩條件） | 退役 41 | ci-spike-retire | —（退役不需檢查者；搬改後的新條目在各 kernel 批由 Fable 檢查） |
| 25 | `tests/common/zone_scan/mutations.json` | 18 | `zone_scan.py` 13、`zone_scan_rules.json` 5：留（target 側守衛） | 留 18 | — | — |
| 26 | `tests/object_fps_pvp/mutations.json` | 925 | 分組（證據 `pvp-mutations-by-file.txt`）：Gateway Go 201、`src/Sim/*`＋`include/RetroFPS/Sim/*` 135（檔留；`fps::pvp::sim` 改名若動到 `find` 文字則逐條核對）、`build/acceptance/*` 99（分析器讀 log；pvp-diagnostics 判定 (1) 保證欄位不變）、assets 11、`tests/object_fps_pvp/*` 17、CMake 1＝**留 464**；`PvpMatch.cpp` 50→match-sim、`MatchRuntimeHost.cpp` 73＋Match host 側 115（IpcHost 59、match_main 3、Wire／WireCounter／ClockSync／IngressStatistics／MatchIngressLedger／MatchSubstitutionRecords／ConnectionLiveness／SessionSummary／LobbyActions／NetworkStatistics／NativeFiles）→match-host、`ClientConnection.cpp` 65＋InputSendBudget 5→client-link、ClientSimulation*／LocalPlayerPrediction 55→client-sim、PvpApplication／Present／App 61→client-frame、診斷 37（MovementDiag／MovementTrace*／TracePaths／LogFile／MatchShotTrace／MatchIngressTrace／ClogLine）→pvp-diagnostics＝**重列 461**（其中 Match host 側 115 條裡純資料型別的標頭若原檔不動，可改判留——以 match-host 宣告為準） | 留 464／重列 461 | match-sim、match-host、client-link、client-sim、client-frame、pvp-diagnostics | Fable（各批審查者） |

**合計**：留 1131／重列 627／退役 83＝1841（與 §0 一致）。

| 執行器 | 性質 | 去處 | 批 |
|---|---|---|---|
| `tests/object_fps_pvp/run_mutations.py`（＋`test_run_mutations.py`、`run_mutations.zh-Hant.md`） | 產品清單的執行器：`checks` 表（cpu／worker／go／ipc／zone_c_lists…）定義每類檢查怎麼建、怎麼跑 | 留；產品批加新 ctest 名時擴 `checks` 表（例：match-sim 的重放 host 測試） | 各產品批 |
| `tests/common/zone_scan/run_mutations.py` | J-17 掃描器自測的執行器（`-B`、清 `__pycache__`） | 留 | — |
| `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py` | Engine／工具共用執行器（`--catalog`；`command`／`platforms`／`expect_build_failure`） | 留；kernel-scaffold 的 `engine/kernel` 清單沿用它（plan 3.3）；plugins 的清單也沿用 | kernel-scaffold 起 |

---

## 4. 缺口

A. **target 有 DI、尚無守衛、計畫也沒指派批的**：

1. **naming DI 1**（namespace 與目錄一一對應；守衛寫「掃描」）：計畫只把 DI 3 的後綴掃描掛在 kernel-scaffold（plan 3.3），DI 1 的掃描沒有批。
2. **runtime-composition DI 3 的「Peer 元件不掛在 Player 實體」掃描**：DI 條文列了掃描，§3 沒有批接。match-host／client-link 之一要收。
3. **error-handling DI 2**（`Base::Result` 純值；Executor 以外無 log 呼叫；守衛寫「J-9 scan of the kernel directory for log calls outside the Executor」）：kernel-scaffold 的 J-9 禁字表（plan 3.3）列的是 `unordered_`、`std::hash`、`<random>`、`<chrono>`、`<thread>`、static、產品名——**沒有 log 呼叫**。要在 scaffold 的宣告補進去，否則 DI 2 只靠審查。

B. **legacy 守衛退役後無人接的性質**：

4. **framework DI 1「不是產品」**（不建執行檔、不 install、不在 registry）：plugins-foundation 的刪除驗證守「可刪」，不守「plugin 不是產品」。runtime-composition §4 沒有條文說 plugin 不得是可啟動的產品。要不要這條，給架構師（Q8）。
5. **framework DI 3「中性」**（不名任何 registry id）：對產品自己的 (c) plugin 不適用（本來就名產品），對**共用** plugin（headless-runner、sdl-window-runner、asset-catalog、imgui、editor-dialogs、offscreen-preview、net-link、trace-diagnostics）沒有條文與守衛→ AGENTS §9「共通層不以產品名分支」在 plugins 層無機械檢查（Q8）。
6. **entity-model E-5 後半「欄成員關係恰等於種類表」**（G1-ii）：World 的 J-1 不保證「同種類的實體在每個欄都有一列」；match-sim 若只退役 G1 不另寫產品掃描，這條就消失（§2.18）。

C. **守衛的掃描根沒跟上新 top-level（空轉）**：

7. **OS 邊界三件**（`os_boundary_guard.py` R1～R4、OS 編譯定義檢查、ProcessCodePageCheck）都不掃 `plugins/`（§2.4～2.6）。今天 `framework/` 也沒被掃，所以搬進 plugins 不會「變差」，但 target 把組合與第三方對接都放進 plugins，plugins 會比 framework 多很多 OS 相鄰的碼（headless-runner 的停止順序、asset-io 服務的 IO 執行緒）。plugins-foundation 要把 `plugins` 加進三個掃描根（DI 守衛變更，交使用者）。
8. **SDL 後端搬進 plugin 後的標頭衛生**（§2.9 G1／G2）：`engine/input/backend/sdl` 搬到 sdl-window-runner 後，G1 的分支靜默消失、G2 不掃 `plugins/`；plugin 的公開標頭能不能露 SDL 沒有條文（Q2）。
9. **`if(TARGET …)` 條件守衛的靜默退出**：platform-sdl G1（CMake :38-53）、G3（:88-92）、`tests/common/input/CMakeLists.txt:14`——target 搬走或改名時測試**不建、不報**。遷移批把這些改成「target 應存在則必須存在」（締める面）或明確移除，不能靜默。

---

## 5. 給架構師的問題

| # | 問題 | 影響的列 |
|---|---|---|
| Q1 | OS 邊界三個掃描根加 `plugins`：由 plugins-foundation 的事前宣告提（締める面，但 DI 守衛→使用者核准）？還是 a6-revision 先在 os.md 寫「掃描根＝engine、plugins、apps、tools、build/acceptance、tests」？ | §2.4～2.6、缺口 7 |
| Q2 | plugin 的**公開標頭**可以露第三方型別（SDL、ImGui）嗎？可以→ G1／G2 對搬走的後端自然退役；不可以→ plugins-frame 要新增 plugin 版的標頭衛生測試。 | §2.9、缺口 8 |
| Q3 | assets-core 會不會另開 CMake target（`GYO::Asset`）？會→ `gyo_expect_model_links`（§2.16）、framework 守衛的正向清單（§2.10 DI 2）、四工具正向清單（§2.11）在 assets-core 就要改，不是等各自的批。 | §2.10、2.11、2.16 |
| Q4 | §5.2 第 18 列只寫 match-sim；G3（host 逐出）綁 `MatchRuntimeHost` API，應歸 match-host。確認拆法。 | §2.18 G3 |
| Q5 | §5.2 第 19 列寫 match-host「留＋改引用」；OutageRig 四個 TEST_CASE 驅動 `LocalPlayerPrediction`（client-sim 要重寫的型別），應是 client-sim 改寫＋match-host 改寫（S3）。確認。 | §2.19 |
| Q6 | `KernelPrepDetectTests`（D1 Life 時序 T＝35／R＝215 的 characterization）在 match-sim 之後：時序照舊→改寫為新路徑測試；時序被 Life 漏斗改變→退役並記錄？plan 3.14 判定 (2) 只說「實測 D1」，沒說測試去處。 | §2.22 |
| Q7 | §5.2 第 1 列「World 不放 entity_store 目錄」是前提；Delta-3 的兩個選項（回傳位置的查找版本／`Cell<T>`）都改 store 的 API——`es-row*` 15 條的 `find` 文字會過期，world-exec 宣告要列「重列」而不是「留」。確認 §5.2 第 1 列改「留（守衛）／重列（突變，條件）」。 | §2.1、§3 第 6 列 |
| Q8 | framework DI 1（不是產品）與 DI 3（中性）退役後，共用 plugin 要不要同性質的條文與守衛（缺口 4、5）？若要，落在 runtime-composition DI 還是 plugins 自己的文件（target 目前沒有 plugins 頁）。 | §2.10、缺口 4、5 |
| Q9 | 缺口 1～3（naming DI 1 掃描、Peer 掃描、J-9 加 log 呼叫）各歸哪一批？建議：J-9 加 log→kernel-scaffold（同一支掃描）；naming DI 1→kernel-scaffold；Peer→match-host。 | §4 A |
| Q10 | `neutral_names_guard` 掃 `docs/architecture/voxel-mesh.md`、`map-markers.md`：a6-revision／closeout 在這兩份加 target 對照或「已被取代」註時，不得出現 `zone`／`Extract`／`spawn` 子字串——寫進那兩批的「不碰」規則，還是把守衛的文件掃描改成只掃程式與測試（緩める面→使用者）？ | §2.3 |
| Q11 | 缺口 9 的 `if(TARGET)` 靜默：改成硬性存在檢查（締める面）由 wrap-render 一次做，還是各批自己處理？ | §2.9、2.17 |
| Q12 | voxel 守衛的 core／window 結構規則（§2.11 第 4 子列）：tool-voxel 是「從 target 的文件 World／渲染 World 重導規則」還是「保留 core／window 兩個 library、只換 target 名」？前者是改寫、後者會把舊切法帶進去。 | §2.11 |

## 6. 與計畫 §5.2 的不一致（彙整）

| §5.2 列 | 計畫寫 | 本卡判 | 差在哪 |
|---|---|---|---|
| 1 | 留 | 留（守衛）；突變 15 條**條件重列** | Delta-3 改 store API 時 `find` 過期（Q7） |
| 1 整體 | — | **條件退役** | store 目錄併入 kernel 時 `SCANNED` 不存在即失敗 |
| 4、5、6 | kernel-scaffold；pageguard-os 重判 | 另加 **plugins-foundation 改寫（加掃描根）** | `plugins/` 不在 ROOTS（缺口 7） |
| 9 | wrap-render 留或改寫 | 另加 **plugins-frame**；G1／G3 的 `if(TARGET)` 會靜默退出 | SdlInput 搬入在第 17 列才提 |
| 10 | 改寫 | framework 側退役＋plugin 側**新增** | owner 與掃描根都換，是另寫一支 |
| 10 | — | fw-* 針對 framework/src 的 29＋31＋10 條在 plugins-foundation／frame／editor **重列**，不是等 framework-delete | 搬檔的批就要重列 |
| 18 | match-sim | G3 → **match-host**；`RequireEntityOrder` → **退役**（非改寫） | host API；函式隨 PvpMatch 刪 |
| 19 | match-host 留＋改引用 | **client-sim 改寫**（OutageRig）＋match-host 改寫（S3） | 測試本體隨型別刪 |
| 21 | 留 | 改引用 | 只差註解 |

## 7. 範囲の限界

- 本卡**沒有執行**任何守衛或突變；去處與第 7 欄都來自讀碼（規則、正規式、白名單、連結期望）與計畫文字，不是跑出來的結果。`find` 文字是否已過期（例如 #197 搬檔後）未逐條核對。
- pvp 925 條的去處按**檔**分組（證據 `pvp-mutations-by-file.txt`），不是逐條；Match host 側 115 條裡原檔不動的資料型別可能改判留。工具四份清單按檔分「工具邏輯（留）／與 framework、視窗、對話框相關（重列）」，是初判。
- 計畫分支 `claude/arch-migration-plan` 落後 master（缺 #197）；本卡路徑照 master，計畫文字照該分支（rev 在 `migration-plan.snapshot.rev`）。
- 事前宣告的數字（26 檔 1841 條、3 執行器）送 parent 核定時，`send_message` 回「session parent not found」（`list_sessions linked:true` 也回空）；數字未經 parent 核定就寫了正文。使用者在本會話核定即可補上。
- §2.18 的子列拆法只讀 `EntityModelTests.cpp` 的 CHECK；沒讀 `AuthorityDigest.cpp` 的 37 個情境內容。
- 補遺（§2.22）只列遷移批會碰到、且本卡讀到的守衛；`tests/object_fps_pvp/CMakeLists.txt` 另有 60 多個 ctest（log_contract、wire_counter_sites、drop_reasons…）未逐個判性質——它們是產品批「測試、守衛與突變的去處」表的對象（計畫 §3 共通第一條），不在 21 列。

## 證據與出處

- 守衛原文：各節引用的檔名與行號（master `d0d8563f`）。
- 突變計數與分組：`build/evidence/guards-inventory/mutations-count.txt`、`pvp-mutations-by-file.txt`（主 checkout，git 管理外）。
- 計畫與決定：`migration-plan.snapshot.md`、`kernel-HANDOFF.snapshot.md`（同目錄；分支 rev 在 `migration-plan.snapshot.rev`）。
- Python `Path.rglob` 對不存在目錄回空：本卡實測（`python3 -c`），未保存。
- `git grep -n 'determinism\.md' -- tests apps build/acceptance engine spikes` 退出碼 1（無守衛以 determinism.md 為判定依據；正例 `entity-store.md` 命中退出碼 0）：未保存，與 legacy-impact §4 結論一致。
