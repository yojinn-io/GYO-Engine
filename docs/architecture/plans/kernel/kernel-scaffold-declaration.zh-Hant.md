# 事前宣告：`v8-arch-kernel-scaffold`（kernel 進 L1 的腳手架）

日期：2026-11-13。Owner：架構（架構師會話開出的任務卡）。實作 Opus／high（CMake 兩行、目錄與空殼標頭是 low 的逐字作業）；審查 Fable／high 只讀；驗證者 Opus／medium（只在改 golden、守衛判定或 DI 守衛時出場；本批全是「新增」，預期不出場）。
基準：origin/master `899b3e38`（#199 遷移計畫、#202 a6-revision 已合併；初稿基準 `8cf52dc1`）。計畫：[migration-plan.zh-Hant.md](migration-plan.zh-Hant.md) §3.3（本批）、§3 共通、§4 Delta-1、§7 U5。條文出處：Kernel 的 DI 全文在 [target/kernel.zh-Hant.md](../../target/kernel.zh-Hant.md)（DI-KS 1、3、15）；命名在 [target/naming.zh-Hant.md](../../target/naming.zh-Hant.md)（DI 1、3、4）；log 在 [target/error-handling.md](../../target/error-handling.md)（DI 2）。spec 的 J-9、J-10、W-10 等代號照舊當定義引用。
**狀態：架構師 2026-11-13 核定（附 6 點修訂，已改入本檔；§9）。**

## 1. Architecture Delta

引遷移計畫 §4 **Delta-1**（Kernel 進 Engine，使用者已同意）。本批的差分只有「目錄與 build 2 行」：

- 新增 Engine 子系統目錄 `engine/kernel/`（target `gyo_kernel`／`GYO::Kernel`，INTERFACE，只連 `GYO::Base`）與測試目錄 `tests/common/kernel/`。
- `engine/CMakeLists.txt`、`tests/common/CMakeLists.txt` 各加 1 行 `add_subdirectory(kernel)`（放在 `entity_store` 之後；ci-readiness §1.1 第 3、5 項）。
- 新的 Dependency：`GYO::Kernel → GYO::Base`（INTERFACE）。沒有其他邊；Ownership、Product Registration、Data Contract 不變。
- 與計畫的差分：無。超出時停下回報（§6）。

## 2. 檔案清單（逐檔；清單外零檔）

| # | 檔 | 動作 | 內容 |
|---|---|---|---|
| 1 | `engine/kernel/CMakeLists.txt` | 新增 | `add_library(gyo_kernel INTERFACE)`、`GYO::Kernel` alias、include 目錄、`INTERFACE GYO::Base`、`cxx_std_20`（照 `engine/entity_store/CMakeLists.txt`；本批沒有 `.cpp`，所以 INTERFACE，有 `.cpp` 的批改 STATIC） |
| 2 | `engine/kernel/include/engine/kernel/Kernel.hpp` | 新增 | 空殼標頭：`namespace GYO::Kernel {}` 與一段指向 spec 的註解，不含任何型別 |
| 3 | `engine/CMakeLists.txt` | 修改 1 行 | `add_subdirectory(kernel)` |
| 4 | `tests/common/CMakeLists.txt` | 修改 1 行 | `add_subdirectory(kernel)` |
| 5 | `tests/common/kernel/CMakeLists.txt` | 新增 | `gyo_kernel_tests`（smoke＋樣本 (b)；連 `GYO::Kernel gyo_test_support doctest::doctest`；MSVC `/W4 /permissive-`＋`NOMINMAX`，其他 `-Wall -Wextra -Wpedantic`；標籤 `cpu`）；兩支 Python 守衛與自測的 ctest（標籤 `cpu`）；樣本 (a) 的 `try_compile`；`gyo_expect_links(gyo_kernel GYO::Base)`（DEFER） |
| 6 | `tests/common/kernel/KernelSmokeTests.cpp` | 新增 | smoke：include 空殼標頭並建置；樣本 (b) 的 doctest |
| 7 | `tests/common/kernel/compile_fail/SampleContract.hpp` | 新增 | 兩種樣本共用的**測試用**拒絕條件（見 §3.3）；只在測試目錄，不進 `engine/kernel` |
| 8 | `tests/common/kernel/compile_fail/RejectsCapturingCallable.cpp` | 新增 | 樣本 (a) 的來源：定義 `GYO_KERNEL_SAMPLE_ACCEPTED` 時登記無捕獲的可呼叫物（應編過），否則登記有捕獲的（應被 `static_assert` 拒絕） |
| 9 | `tests/common/kernel/kernel_guard.py` | 新增 | J-9 掃描（§3.1） |
| 10 | `tests/common/kernel/test_kernel_guard.py` | 新增 | J-9 自測（合成目錄樹，不讀 repo 檔） |
| 11 | `tests/common/kernel/naming_guard.py` | 新增 | naming DI 1（namespace↔目錄）與 DI 3（禁止後綴）掃描（§3.2） |
| 12 | `tests/common/kernel/test_naming_guard.py` | 新增 | naming 掃描自測 |
| 13 | `docs/architecture/plans/kernel/scripts/mutations.json` | 新增 | 突變目錄骨架（§5），用 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --catalog` 跑 |
| 14 | `docs/architecture/plans/kernel/kernel-scaffold-declaration.zh-Hant.md` | 新增 | 本檔（結果與核定記錄回寫到本檔 §9） |
| 15 | `docs/dev_logs/2026_11_13_arch_kernel_scaffold.zh-Hant.md` | 新增 | dev_log（AGENTS §15） |
| 16 | `docs/architecture/plans/kernel/README.md` | 修改 | 批次樹該列與 dev_log 一覽各一行 |
| 17 | `docs/architecture/plans/kernel/HANDOFF.md` | 修改 | 現況一行（開始、停止、完成時各更新） |

不動：`build/`、`.github/`、`CMakePresets.json`、`spikes/`、任何既有守衛與突變清單。

## 3. 本批新增的守衛（全部是「新增」）

### 3.1 J-9：`kernel_guard.py`（target/kernel DI-KS 3、DI-KS 15；target/naming DI 4；target/error-handling DI 2；spec J-9 定義；guards-inventory 缺口 3）

掃 `engine/kernel/` 下的 C/C++ 檔與 `CMakeLists.txt`。註解與字串先清空（K1 例外：也讀註解）。禁字表自寫，**不沿用** `neutral_names_guard.py` 的 WORDS（legacy-impact §4 第 3 列：`Spawn`／`Despawn`／`Extract`／`zone` 會誤判）。

| 規則 | 禁止 | 來源 |
|---|---|---|
| K1 | 產品、工具的名字與連線角色詞，分三組：(i) 從 `engine/config/projects.csv`、`tools.csv` 讀登記的 id（原樣與 CamelCase，例 `object_fps_pvp`／`ObjectFpsPvp`、`voxel_map_editor`／`VoxelMapEditor`）；(ii) 登記表推不出的產品別名 `RetroFPS`、`fps::pvp`、`\b[Pp]vp`、`Gyo::Tools`，腳本裡每個各一行註解寫出處（哪個產品的哪種名字、為什麼推不出）；(iii) 連線角色詞 `\bMatch\b`、`[Pp]layer`、`Gateway`，另一組常數，註明來源是 entity_store W1。登記表讀不到＝失敗（fail closed）。新增產品不必改腳本；刪除產品後它的 id 自動不再禁 | DI-KS 15、naming DI 4（R-7、J-9「無產品名」） |
| K2 | `unordered_*`、`std::hash` | DI-KS 3（W-10） |
| K3 | 時鐘、亂數、執行緒、atomic：`<chrono>`、`<random>`、`<thread>`、`<atomic>`、`<mutex>` 等標頭與 `std::chrono`、`rand(` 等名字（照 entity_store W3）。**Parallel 模組的例外本批不設**：v1 只有 Serial（LIM-2），加 Parallel 時改腳本（審查可見） | DI-KS 3、DI-KS 15（W-10、X-8） |
| K4 | 非 constexpr 的 `static`／`thread_local` 變數；白名單**恰好兩個符號名**：`tlCurrent`（`thread_local`）與 `TypeKey<T>` 的 `static inline char`（符號名在 registry-schedule 定案時寫進腳本；本批先登記為 `typeKeyTag`，型別必須是 `char`）。自測斷言白名單集合恰等於這兩個，加第三個就失敗 | DI-KS 3（spec A.6、J-9） |
| K5 | include 白名單外的標頭：標準庫 C++ 標頭、`engine/base/`、`engine/kernel/`（`engine/entity_store/` 等 Delta-3 的批再加） | DI-KS 15（不含能力的名字）、Delta-1 第 5 項 |
| K6 | log 呼叫（Executor 以外）：`Log(`、`SetLogHandler`、`engine/base/Log.hpp`、`<iostream>`、`<cstdio>`、`std::cout`／`cerr`／`clog`、`printf`／`fprintf`／`puts`／`fputs`、`SDL_Log`。例外：檔名以 `Executor` 開頭的檔（Executor 的 handler 是唯一記錄 kernel error 的地方） | target/error-handling DI 2（guards-inventory 缺口 3、Q9） |
| K7 | 識別字 `far`／`near`（windef.h 的空巨集） | memory windows-far-near-macro、ci-readiness §1.4 |
| — | 掃描根 `engine/kernel` 不存在或無檔＝失敗（不空轉；這條只適用於必須存在的根，不適用於 §3.2 N1 反向的掃描根） | guards-inventory 2.1「整體」的教訓 |

### 3.2 naming：`naming_guard.py`（target/naming DI 1「對應以目錄↔namespace 的一張表給定，不要求拼字相同」、DI 3；guards-inventory 缺口 1）

| 規則 | 判定 | 範圍 |
|---|---|---|
| N1 正向 | `engine/kernel/` 下 C/C++ 檔在檔案頂層開的具名 namespace 必須是 `GYO::Kernel` 或 `GYO::Kernel::…`（巢狀的 `Detail` 等不限）；頂層寫 `namespace GYO {` 也算違規（要求限定寫法，fail closed） | 根表本批只有一列：`engine/kernel` ↔ `GYO::Kernel` |
| N1 反向 | `engine/`、`apps/`、`tools/`、`framework/`、`plugins/`、`tests/` 下、`engine/kernel/` 以外的 C/C++ 檔不得開 `GYO::Kernel` namespace（一一對應的另一半）。反向掃描根**不存在就跳過**（`plugins/` 今天不存在），不是失敗；與 K 規則「`engine/kernel` 不存在＝失敗」相反，兩種行為在自測各放一例。`spikes/` 不掃（獨立專案、用 `KernelSpike`，U10 刪除） | 同上 |
| N3 | `engine/kernel/` 下宣告的型別名（`class`／`struct`／`union`／`enum`／`using X =`／`concept`）不得以 `Manager|Renderer|ViewModel|Host|Shell|Runtime|Controller` 結尾（大小寫相符；`Ghost`、`Hostname`、`RuntimeStats` 不算）。白名單空 | naming DI 3、第 5 條 |

範圍：根表只有 `engine/kernel`。`Engine::<能力>`、`Plugins::` 的列由碰到它們的批加；產品的列見 LIM-4。現有 Engine 模組用 `Engine::<模組>`（`Engine::Base`、`Engine::EntityStore`），本批照 target/naming 第 1 條用 `GYO::Kernel`；兩種並存到 wrap 批（target/README：新程式受 target 約束）。

### 3.3 J-10 的兩種編譯失敗樣本（U5）

樣本的拒絕條件是測試用的最小版（S-1 的「可呼叫物有狀態」：`std::is_empty_v<F>` 為假即拒絕），放在 `tests/common/kernel/compile_fail/SampleContract.hpp`；真正的 System 契約與 J-10 全部條目在 registry-schedule。本批只證明兩種**形狀**在三平台能跑、能辨識。

- **(a) `try_compile` 預期失敗**：`tests/common/kernel/CMakeLists.txt` 在 configure 時對同一來源編兩次（`CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY`，include 目錄傳 kernel 與 base）：(a+) 定義 `GYO_KERNEL_SAMPLE_ACCEPTED` 必須成功（對照組：證明失敗不是因為 include 或旗標錯）；(a−) 不定義必須失敗**且**輸出含 `GYO_KERNEL_SAMPLE_REJECTED`（`static_assert` 的訊息；可識別的預期失敗，AGENTS §14.2）。任一不符→`FATAL_ERROR`，configure 失敗。結果以 `message(STATUS "kernel.compile_fail_sample ...")` 印在 configure log。
- 對應條文：DI-KS 1（System 是無狀態的可呼叫物；守衛 J-10～J-12）。
- **(b) trait＋doctest**：`KernelSmokeTests.cpp` 以 `CHECK(Sample::kAccepted<無捕獲>)`、`CHECK_FALSE(Sample::kAccepted<有捕獲>)` 判同一條件。

### 3.4 `gyo_expect_links(gyo_kernel GYO::Base)`

照 entity_store 以 `cmake_language(DEFER …)` 在整個專案 configure 完才檢查。

## 4. 判定與數字門檻（看結果前宣告）

| # | 判定 | 門檻 |
|---|---|---|
| 1 | 清單外零檔 | `git diff --name-only origin/master...HEAD` 與 §2 的 17 檔逐一比對（Python），清單外 0 檔；多一個＝不成立→停（Delta） |
| 2 | L1 三平台綠 | linux-x64（含 `core` preset 那一步）、windows-x64、macos-arm64 的 L1 job 結論全 success；`gyo_kernel_tests`、`kernel.guard`、`kernel.guard_selftest`、`kernel.naming_guard`、`kernel.naming_guard_selftest` 5 個 ctest 在三份 `tests.xml` 各出現且 pass（3×5＝15；Linux `core` 那一步的 5 個另從 log 確認）；件数の突き合わせ寫在報告第一行 |
| 3 | 兩種編譯失敗樣本先過 windows-x64 L1 | windows-x64 的 configure log 有 (a+) 成功、(a−) 被拒且訊息命中；`gyo_kernel_tests` 的 (b) case 過。之後照 U5 選分工（核心三條 (a)、其餘 (b)），寫進 registry-schedule 的宣告輸入；記錄 (a)／(b) 在三平台的 configure 時間與穩定度（一次 L1，失敗次數）。(a) 在 Windows 不穩（失敗或訊息不命中）→不重跑湊綠，報告使用者 |
| 4 | J-9 自測 | K1～K7 每條至少 1 個正例被報（K1 的正例是「加一個產品名」，三組各一）；反例 `Despawn`、`ExtractRes`、`zone`、`Spawn`、`timezone`、`matches`、`Matcher`、`rematch`、`static_assert`、`static constexpr`、註解裡的 `static int x;`、`nearest`／`farther` 不誤報；白名單集合恰等於 2 個符號 |
| 5 | naming 自測 | N1 正向（`namespace Engine::Kernel`、`namespace GYO {`）、N1 反向（`engine/time` 下開 `GYO::Kernel`；反向根不存在＝跳過、`engine/kernel` 不存在＝失敗，各一例）、N3（`struct WorldHost`、`class SceneRenderer`、`using Runtime = …`）各被報；反例 `Ghost`、`Hostname`、`RuntimeStats`、巢狀 `namespace Detail` 不誤報 |
| 6 | `gyo_expect_links` 反例 | 本機在 `engine/kernel/CMakeLists.txt` 多連 `GYO::Math` 再 configure：失敗且訊息 `gyo_kernel links …; expected exactly GYO::Base`。做成突變 `ks-links-extra` |
| 7 | 突變 | §5 目錄每條在本機（Intel Mac、preset `test`）killed（預期訊息命中才算）：n/n。**Fable／high 只讀一次**（作者 Opus，不同模型、不是小模型）同時做 code review 與「刪改實作而測試不失敗」的搜尋，報告分兩節；找到的處加入目錄後再跑；報告寫找到的處數與加入後的檢出數／總數 |
| 8 | 既有守衛不因本批變紅 | `os.boundary_guard`、OS 編譯定義檢查、`ProcessCodePageCheck`、`gyo_expect_os_links`、`neutral_names` 在 L1 照舊 pass（由判定 2 的 L1 涵蓋） |

掃描類判定（「0 檔」「零命中」）照 AGENTS §14.2：用 `git grep`／Python、不丟 stderr、以退出碼區分「無」與「失敗」，先用已知正例命中一次。

## 5. 守衛與突變的去處（計畫 §3 共通第 1 點；§5.2 第 3～7 列）

| 守衛／突變 | 性質 | 去處 | 理由 |
|---|---|---|---|
| `neutral_names_guard`（WORDS） | voxel_mesh／map_markers 不含 gameplay 名詞 | **留**（不套到 kernel；**腳本與掛載都不碰**） | 性質不同（gameplay 名詞≠產品名）；kernel 用 J-9 自寫表。Q10 使用者 2026-11-13 定不放寬、維持現狀（kernel HANDOFF `v8-arch-neutral-names-guard-keep`）：腳本、WORDS、兩個模組的掛載都不改 |
| `os_boundary_guard` | `engine/` 無 OS 標頭／巨集／API | **留** | kernel 無 OS 碼；新檔在掃描根下，自動涵蓋 |
| OS 編譯定義檢查 | 同上（CMake 側） | **留** | 同上 |
| `ProcessCodePageCheck` | 每個可執行檔有 UTF-8 manifest | **留** | `gyo_kernel_tests` 由 `GyoCodePage.cmake` 自動套 |
| `gyo_expect_os_links` | `gyo_os` 只連 Base＋系統庫 | **留** | 本批不連 `GYO::Os`（pageguard-os 重判） |
| J-9 `kernel_guard.py`＋自測 | §3.1 | **新增** | spec J-9；error-handling DI 2 |
| `naming_guard.py`＋自測 | §3.2 | **新增** | naming DI 1、DI 3 |
| 編譯失敗樣本 (a)(b) | §3.3 | **新增** | U5 |
| `gyo_expect_links(gyo_kernel GYO::Base)` | kernel 只連 Base | **新增** | Delta-1 第 5 項 |
| 既有突變清單 26 檔 | — | **不碰** | 本批不改它們的 `find` 對象 |
| 新突變 `mutations.json` | 每條規則至少 1 條：K1～K7、N1 正反、N3、白名單第三符號、樣本 (a) 拿掉 `static_assert`、樣本 (b) 改 trait、`ks-links-extra`、掃描根不存在 | **新增** | 敵對檢查 Fable／high |

突變一覽的作者：本會話（Opus 5.5）；審查與敵對檢查合成一次：Fable／high 只讀（使用者要求節制 Fable 用量）。

## 6. 範囲の限界

- **LIM（本批）-1**：STATIC＋PRIVATE 連 `GYO::Os` 的形狀不在本批（pageguard-os 才需要；U1 延到 G2a 後）。
- **LIM（本批）-2**：MSVC `/W4` 零警告在本機 macOS 無法先驗；只由 windows-x64 L1 判。
- **LIM（本批）-3**：樣本的拒絕條件是測試用最小版，不是 J-10 的條目；J-10 的「帶捕獲 lambda、泛型 lambda、成員函式、`World&` 參數、回傳型別」由 registry-schedule 做。
- **LIM（本批）-4**：naming DI 1 只有 `engine/kernel` 一列根表，不預做讀取機制；DI 2（詞性）不做（「登記時的弱檢查＋審查」，屬 Registry）。產品的列屬產品 owner，之後應由產品側的資料給（與 zone-c-sources 清單同一種做法），由第一個需要它的產品批（match-sim）在宣告裡提，不在共通腳本裡加產品列。
- **LIM（本批）-5**：K4 白名單的 `TypeKey<T>` 符號名是暫定名 `typeKeyTag`；registry-schedule 定案時改腳本（審查可見）。
- **LIM（本批）-6**：突變只在本機 Intel Mac 跑（mutation-rerun-scope；不進 CI）。
- **LIM（本批）-7**（架構師 2026-11-13 定，審查 A6 前半）：J-9 只看帶 `static`／`thread_local` 關鍵字的宣告，看不到 namespace 範圍的 `inline int g = 0;`、匿名 namespace 變數與 `extern`。本批不擴大：文字掃描要判 namespace 範圍的變數定義得做語法近似，誤報與漏報都多，而且本批 kernel 是 INTERFACE、沒有目的檔可查。去處：`v8-arch-kernel-registry-schedule`（第一個有 `.cpp` 的批）的事前宣告新增**符號層**的檢查——對 kernel 靜態庫看可寫資料段（.data／.bss）的符號，白名單恰好是 `tlCurrent` 與 `TypeKey` 的那個符號；Linux 與 macOS 用 `nm`，Windows 先記為限界。
- 共通 LIM-2／3／6：本批不寫 System，不適用。

## 7. 給架構師的問題（已答，2026-11-13）

1. naming 根表的產品列：照預設，本批只做 kernel 一列；方向寫進 LIM-4。
2. K1 的固定別名：可以，條件是每個別名一行出處註解、連線角色詞另成一組（已改入 §3.1）。

## 8. 估時與停止

- 估時：**4～8 h**＋守衛去處判斷 **1～2 h**（推定；計畫 §3.3）。總時數超過估時上限 10 h×1.5＝15 h，或單一問題累計 >2 h→停下回報。
- 停止：清單外還要改檔才綠（Delta）；AGENTS §14 冒頭三條；(a) 在 Windows 不穩（報告，不湊綠）。

## 9. 核定與結果

- 2026-11-13 架構師核定（訊息來自架構師會話；初稿 `a7f6f899`）。6 點修訂：(1) DI 出處改引 target/kernel 頁與 naming DI 4；(2) N1 反向加 `plugins/`、不存在的反向根跳過，自測兩種各一例；(3) K1 別名逐個註出處、角色詞另一組、反例加 `matches`／`Matcher`／`rematch`；(4) LIM-4 寫產品列的方向；(5) Fable 只用一次；(6) `neutral_names_guard` 不碰。全部已改入本檔。

- 2026-11-13 結果（細節與證據見 [dev_log](../../../dev_logs/2026_11_13_arch_kernel_scaffold.zh-Hant.md)）：判定 1～8 成立。L1 第一輪 `b31bae5a`（run 43352865028）與最終程式 `0a2b2f7b` 的第二輪（run 43355513501）三平台綠；樣本 (a)(b) 在 windows-x64 各一次無失敗，照 U5 分工（核心三條 (a)、其餘 (b)）。突變 79/79（初版 37 條＋審查後 41 條＋A8 修訂 1 條）。Fable 審查與敵對檢查（一次）找到程式碼 11 項、存活 22＋1 處；本批修 A1～A5、A6 後半、A7、A9 與全部存活處，A6 前半與 A8 交架構師判斷，結果見下兩條。
- 與宣告的差異：§3.1 K0（無法掃描的檔案類型）與 K5 的「無法讀取的 include 目標」是審查後新增的判定（締める面）；K4 的函式判定改寫以消除對合法函式形狀的誤報；naming 的 SUFFIXES 與 J-9 共用並加 `.m`／`.mm`。§2 檔案清單不變。
- 2026-11-13 核定後的修訂（架構師，審查 A8）：K1 對登記 id 改為**詞邊界**比對。snake 形不分大小寫、以非字母數字（含 `_`）或首尾為界；CamelCase 形分大小寫、從詞頭（前面不是大寫字母）到詞尾（後面不是小寫字母）。id `arena` 時 `Marenal`、`parenage` 不命中，`FrameArena`、`ArenaHost` 仍命中（刻意：產品名當前綴的 `ObjectFpsPvpHost` 不能漏）。命中訊息寫兩種讀法與處置：Kernel 用了產品名＝改 Kernel；新登記的產品 id 是 Engine 通用詞＝改產品的登記 id（代價落在新產品命名，不落在共通層；AGENTS §7、§12）。締める面：無。緩める面：`Marenal` 類（登記 id 出現在另一個詞的中間）不再報；「Kernel 不含產品名」的性質不變。自測加 3 個（4 個詞）、突變加 `ks-k1-word-boundary-dropped`，本機 79/79。
- 2026-11-13 A6 前半：本批不擴大，記為 LIM（本批）-7（§6）。
