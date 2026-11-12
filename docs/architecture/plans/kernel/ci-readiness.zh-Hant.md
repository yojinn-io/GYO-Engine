# kernel 的 CI 就緒調查（`v8-arch-kernel-ci-readiness`）

日期：2026-11-12。Owner：架構（kernel 研究線；架構師會話開的任務卡）。**性質：調查與方案，不是 Architecture Delta；不改 `build/`、`.github/`、頂層 CMake，不改 spike 程式。**
三個問題：(1) 新 Engine 子系統進 L1 的接法；(2) spike 測試升格為 kernel 正式測試的路徑；(3) Windows 的 VEH（Vectored Exception Handler）在 CI 的行為。
證據：主 checkout `build/evidence/kernel-ci-readiness/`（git 管理外；本機 Intel Mac，工具鏈見 `toolchain.txt`）；問題 3 讀既有的 `build/evidence/kernel-pageguard/l1-43169204148/`（L1 run 43169204148，commit `549a8ca9`，PR #184）。

件数の突き合わせ：一致——假子系統 1 個測試：`ctest -N -L cpu` 列出 1（#26）／建置 1／執行 1 過 0 失敗；反向檢查 1 次（多連一個 target）→ configure 失敗 1 次；問題 3 的 windows-x64 xml：tests 2／failures 0／skipped 0，debug 執行檔 doctest 52 過 0 skip、release 執行檔 51 過 1 skip（Cost 案例依設計只在 debug 跑）。

## 判定（事前宣告，看結果前定；2026-11-12 開始前給架構師）

| # | 判定 | 結果 |
|---|---|---|
| 1 | 問題 1 的清單用假子系統走過一遍：本機 preset `test` 的 `ctest -N -L cpu` 列出它的測試、`ctest -L cpu` 跑過；清單每一項對應一個實際改過的檔；清單外若還要改任何檔才通過，算有漏 | **成立**。改的檔正好是清單 §1.1 的 4 新增＋2 修改；沒有第 7 個檔。範圍限制：Windows 專屬項只讀 CMake 與 workflow 確認接線，未實機驗證（§1.4） |
| 2 | 問題 2 列出每個 spike 測試檔與 `mutations.json` 每一類條目的去處（搬／重寫／刪）與理由，理由引用 spike 程式的行或報告的節 | **成立**（§2.2～§2.4） |
| 3 | 問題 3 以 artifact 的 xml／log 行號為證；runner 無 debugger／ASan 只能間接判定，寫「推斷」 | **成立**（§3.1 直接證據；§3.2 推斷） |
| 4 | 共通層零修改：不改 `build/`、`.github/`、頂層 CMake；結構性改動只寫進報告 | **成立**。本 PR 只有 `docs/`。零風險小修：**沒有找到需要修的**（§2.6） |
| 5 | 時間盒：估 3 h，單一問題超過 2 h 停；總時數超過 4.5 h 停 | 未觸發。實際約 2 h（含 configure 162 s） |

## 1. 新 Engine 子系統進 L1 的接法

### 1.1 精確清單（以 `engine/kernel/`＋`tests/common/kernel/` 為例；假子系統實走過）

| # | 檔 | 動作 | 內容 | 為什麼 |
|---|---|---|---|---|
| 1 | `engine/kernel/CMakeLists.txt` | 新增 | `add_library(gyo_kernel INTERFACE)`（有 `.cpp` 就 `STATIC`）、`add_library(GYO::Kernel ALIAS gyo_kernel)`、`target_include_directories(... include)`、`target_link_libraries(gyo_kernel INTERFACE GYO::Base)`、`target_compile_features(... cxx_std_20)` | 照 `engine/entity_store/CMakeLists.txt` 的葉子形狀；spec J.1「測試執行檔只連 Kernel 加 `gyo_test_support`」 |
| 2 | `engine/kernel/include/engine/kernel/*.hpp`（＋`engine/kernel/src/*.cpp`） | 新增 | 本體 | — |
| 3 | `engine/CMakeLists.txt` | 修改 1 行 | `add_subdirectory(kernel)`，放在 `entity_store` 之後、`add_library(engine STATIC)` 之前（連 `GYO::Os` 時放在 `os` 之後） | kernel 在 `GYO::Engine` 之下，不連 engine 函式庫的模組也要能用（同 time／threads／trace 的註解）；順序只影響 alias 可見性 |
| 4 | `tests/common/kernel/CMakeLists.txt` | 新增 | `add_executable(gyo_kernel_tests ...)`；`target_link_libraries(... PRIVATE GYO::Kernel gyo_test_support doctest::doctest)`；MSVC：`/W4 /permissive-`＋`NOMINMAX`，其他：`-Wall -Wextra -Wpedantic`；`add_test(NAME gyo_kernel_tests ...)`；`set_tests_properties(... LABELS "cpu" TIMEOUT 60)`；`gyo_expect_links(gyo_kernel GYO::Base)` 以 `cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL ...)` 延後 | 照 `tests/common/entity_store/CMakeLists.txt`；`cpu` 標籤讓 L1 的 test preset（filter `cpu|shader`，`CMakePresets.json` testPresets `test`）選到它；`gyo_expect_links` 定義在 `tests/common/time/CMakeLists.txt:22`，所以第 5 項的順序要在 `time` 之後 |
| 5 | `tests/common/CMakeLists.txt` | 修改 1 行 | `add_subdirectory(kernel)`，放在 `entity_store` 之後（註解已寫「After time: its directory defines gyo_expect_links」） | 同上 |
| 6 | `build/cmake/*`、`CMakePresets.json`、`.github/workflows/cross-platform.yml` | **不改** | — | `GyoBuild.cmake:51` 整個加 `engine/`，`:75` 加 `tests/common/`；`GyoComponents.cmake` 的 `GYO_KNOWN_COMPONENTS` 只登記第三方／framework 元件（SDL_*、UFBX、IMGUI、FRAMEWORK），Engine 葉子不需要登記；`gyo_label_tests`（`GyoBuild.cmake:121`）會把沒標籤的測試補成 `cpu`，但照慣例明寫；L1 的 configure／build／ctest 三步都走 preset（workflow 第 109～134 行），不點名任何子系統 |

走一遍的結果（證據 `build/evidence/kernel-ci-readiness/`）：

| 步 | 命令／檔 | 結果 |
|---|---|---|
| configure | `cmake --preset test`（`configure-test.log`） | 成功，162 s（含 FetchContent） |
| 列出 | `ctest --preset test -N -L cpu`（`ctest-N-cpu.txt:535`） | `Test #26: gyo_kernel_tests`，共 139 個 cpu 測試 |
| 建置 | `cmake --build --preset test --target gyo_kernel_tests`（`build-gyo_kernel_tests.log`） | 成功，7 s |
| 執行 | `ctest --preset test -R '^gyo_kernel_tests$'`（`ctest-run-kernel.txt`） | 1/1 Passed，Label `cpu` |
| 反向 | 假子系統多連 `GYO::Math` 再 configure（`configure-negative-expect-links.log:182`） | `gyo_kernel links GYO::Base;GYO::Math; expected exactly GYO::Base`，configure 失敗——第 4 項的 DEFER 檢查確實生效 |
| OS 守衛 | `python3 tests/common/os/os_boundary_guard.py --root .`（`os-boundary-guard-with-fake.txt`） | 通過（假子系統沒有 OS 知識） |

假子系統的檔與接線 patch 在 `fake-subsystem/`（`wiring.patch`、`engine_kernel/`、`tests_common_kernel/`），未 commit。

### 1.2 清單以外、同一批要一起做的（不是 CMake，但不做就不算「進了 L1」）

- **守衛腳本測試**（spec J-9、J-17 的 kernel 端自測）：照 `tests/common/entity_store/CMakeLists.txt:21-29`——`find_package(Python3 REQUIRED COMPONENTS Interpreter)`，`add_test(NAME kernel.word_guard COMMAND ${Python3_EXECUTABLE} ... --root ${GYO_REPOSITORY_ROOT})` 與 `kernel.word_guard_selftest`（`unittest discover`），標籤 `cpu`。
- **編譯失敗測試**（J-4、J-10、J-11、J-13、J-43）：repo 目前**沒有** `try_compile` 的先例（`tests/`、`build/cmake`、`engine/*/cmake` 皆無）。spike 以 trait＋`static_assert` 的形狀表達（`AccessDerivationTests.cpp:199` 用 `FunctionArgs::deducible == false` 當可執行的檢查）。實作批要選一：(a) configure 時 `try_compile` 預期失敗（spec J-10 原文）；(b) 像 spike 那樣把拒絕條件做成可查的 trait，再用 doctest 檢查。兩者都要寫進事前宣告；(a) 是新的測試形狀，第一次做時 Windows 的 `try_compile` 要過 L1 才算。
- **Design Invariants 文件**（AGENTS §3）：`docs/architecture/<kernel>.md` 的 `Design Invariants` 與 `Replacement Criteria` 要在堆功能之前寫（spec K 節已有草案）。
- **突變目錄**：`docs/architecture/plans/kernel/scripts/mutations.json`（spec J.3 原文），用 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --catalog ... --build-dir build/target/_build/test` 跑（entity-store 的先例 `docs/architecture/plans/entity-store/scripts/mutations.json`）；不進 CI（memory mutation-rerun-scope）。

### 1.3 Linux 的 `core` 步驟也會跑它

L1 的 Linux 列另以 `core` preset 再 configure／build／ctest 一次（workflow 第 177～185 行；`GYO_TEST_PROFILE=core`、`GYO_APPS=""`、所有 SDL 元件關）。`tests/common/CMakeLists.txt` 無條件加每個子目錄，所以 kernel 的測試在 `core` 也跑。kernel 測試**不得**需要任何 SDL 元件；`tests/common/Requirements.cmake` 的 `GYO_COMMON_TEST_COMPONENTS` 只在 `GyoApps.cmake:43` 用來補元件，與 kernel 無關。

### 1.4 Windows 專屬項（只讀 CMake／workflow 確認接線；本機 macOS 無法實測）

| 項 | 現況 | kernel 要做的 |
|---|---|---|
| `/Zc:preprocessor` | 頂層 `GyoBuild.cmake:49-50` 對 MSVC 全域加（`GYO_ASSERT` 是變參巨集，`Assert.hpp` 在傳統前處理器下拒絕編譯）；`spikes/kernel/CMakeLists.txt:73` 自己加是因為它是獨立專案 | 不用加 |
| `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT` | `ci-windows` preset 設 `Embedded`（`/Z7`，sccache 需要）；spike 步驟手動傳同一個值（workflow 第 198 行） | 不用加；進 build graph 後由 preset 管 |
| PCH | `ci-windows` preset `CMAKE_DISABLE_PRECOMPILE_HEADERS=ON`；repo 內**沒有** target 用 `target_precompile_headers` | 不加 PCH（加了在 Windows CI 也會被關掉，本機與 CI 行為分叉） |
| `/utf-8` | 只有 tools、產品測試、framework 加；engine 測試（math、entity_store、base）不加 | 原始碼只有 ASCII 就不需要；若測試訊息含非 ASCII 才加（MSVC 以 code page 讀原始碼會出 C4819／C4566） |
| `NOMINMAX` | doctest 在 MSVC 上 include `<windows.h>`（SEH 支援，doctest.h 第 274 行起自動開 `DOCTEST_CONFIG_WINDOWS_SEH`），同一 TU 用 `std::min`／`std::max` 會被巨集吃掉；`tests/common/math` 加、`entity_store` 沒加（它不用 min／max） | 測試執行檔加（spike 的 `PageGuardTests.cpp:20` 用 `<algorithm>` 的 `std::min`）；`engine/kernel` 本體不含 OS 標頭就不用 |
| `far`／`near` | `windef.h` 把兩者定成空巨集（memory windows-far-near-macro：PR #152、#163 各踩一次） | 變數、參數、成員都不叫 `far`／`near`（spike 目前沒有，`grep -w` 零命中）；J-9 的字詞掃描可順手加這兩個詞 |
| 程式 code page 宣告 | `GyoCodePage.cmake` 在 configure 末對 `engine`、`apps`、`tools`、`build/acceptance`、`tests` 下**每個**可執行檔自動套 UTF-8 manifest（os.md DI 11），`tests/common/os` 檢查 | 不用做；若要豁免才設 `GYO_PROCESS_CODE_PAGE_EXEMPT` |
| OS 邊界守衛 `os.boundary_guard` | 掃 `engine/`（R1 OS 標頭、R2 OS 巨集、R3 OS API、R4 檔案操作），allowlist 是只縮不放的 ratchet | **頁保護的平台層不能放 `engine/kernel/`**：把 spike 的 `PageGuard.cpp` 原樣放到 `engine/kernel/src/` 探測，守衛報 **27 筆**（R1 4：`windows.h`、`signal.h`、`sys/mman.h`、`unistd.h`；R2 11：`_WIN32`、`__APPLE__`；R3 12：`sigaction` 等；`os-boundary-guard-pageguard-in-kernel.txt`）。要放 `engine/os`，與 spike 報告建議 10 一致；見 §2.3 |
| 組態 | L1 主圖是 **RelWithDebInfo**（`base` preset）；spike 步驟是 **Debug**（workflow 第 198 行） | kernel 的驗證開關必須是執行期選項（spec K.3 第 17 項、`v8-arch-kernel-verification-not-assert`），不能像 spike 那樣跟 `NDEBUG`；否則進了 build graph 後 L1 只會跑到「關」的那一邊。見 §2.5 |

## 2. spike 測試的升格路徑（方案；**不是現在搬**）

### 2.1 前提

- spike 的標頭（`Access.hpp`、`Params.hpp`、`Descriptor.hpp`、`Registry.hpp`、`System.hpp`、`World.hpp`、`TypeList.hpp`，共 1,558 行）是 kernel 本體的**參考**，不是搬的對象：spec 的參數型別（`Query<Read/Write/With/Without/Changed>`、`Res`／`ResMut`、`EventReader`／`Writer`、`Commands`／`CommandsFor`、`Spawn<Bundle>`、`SetRead`、`Local`、`ExclusiveWorld`、`CurrentTick`）比 spike 的七類多，Registry 的 plugin／finish、Scheduler、Executor 的 `Interrupted` spike 都沒有。測試的「搬」是搬**判定與案例形狀**，呼叫的 API 一律改成 kernel 的。
- 判定形狀的三個系統性改變，所有搬過去的測試都要過一遍：
  1. **斷言→Err**：spike 第一層（World API 存取當下）仍 `KERNEL_SPIKE_ASSERT`（`spike-pageguard-report` 差異 1、對抗 S4）；spec 要求兩層都回 `Err` 並 `Interrupted`（W-5、E.5）。`SPIKE_CHECK_ASSERTS` 的案例改成檢查 `RunResult`。真正的 Programmer Error（J-3 結構變更、J-6 Host 在 Running 寫）才用 `GYO_CHECK_ASSERTS`（`tests/common/support/AssertTestSupport.hpp:49`）。
  2. **兩個執行檔→一個執行檔、兩個 World**：spike 用 debug／release 兩個 target 表達「驗證開／關」（`spikes/kernel/CMakeLists.txt:55-80`）；spec 的驗證是執行期開關（§1.4 最後一列）。改成同一個 `gyo_kernel_tests` 內開著與關著各一個 World（J-5 原文：「測試明確打開驗證開關；關閉時同樣三種繞路 System 回 Ok、不攔；『無違規』的報告帶『驗證開著』欄位」）。
  3. **測試型別改名**：`Position`／`Velocity`／`Health`／`Time`／`Gravity`／`Stats`／`Inventory`／`Hit`／`Spawned`（`SpikeTestSupport.hpp:44-76`）→ spec J.1 的 `TestPos`／`TestVel`／`TestMarker`／`CountingDtor`／`TestEvent`／`TestRes`；仍只存在測試裡。

### 2.2 每個檔的去處

| spike 檔 | 去處 | 理由 |
|---|---|---|
| `include/kernel_spike/Assert.hpp` | **刪** | `Assert.hpp:7-11` 自述是「GYO_ASSERT 的替身，因為 spike 不 include GYO 模組」；kernel 直接用 `engine/base/Assert.hpp` |
| `tests/SpikeTestSupport.hpp` | **重寫**→`tests/common/kernel/KernelTestSupport.hpp` | `ScopedAssertionHandler`／`SPIKE_CHECK_ASSERTS`／`SPIKE_CHECK_NO_ASSERT`（第 24-127 行）由 `gyo_test_support` 的 `AssertTestSupport.hpp` 取代（含第 35-40 行 MSVC `_set_abort_behavior`，`AssertTestSupport.hpp:36-39` 已有）；型別改名（§2.1 第 3 點）；`MakeRegistry`（第 81-93 行，id 由登記順序決定）改成 kernel 的 plugin 登記，「id 依登記順序」的前提仍成立（spec R-1） |
| `tests/Main.cpp` | **搬**→`tests/common/kernel/TestMain.cpp`（2 行） | 多檔測試需要獨立的 main（`tests/common/ui/test_main.cpp` 先例）；`DOCTEST_CONFIG_NO_POSIX_SIGNALS` 不放這裡（§2.5） |
| `tests/RegistryWorldTests.cpp`（6 案例） | 見下表 | — |
| `tests/AccessDerivationTests.cpp`（5 案例） | **搬改**→J-12 的核心 | 判定 1 的形狀（型別層 `static_assert` 加 id 層執行期比對，18 個簽名）正是 J-12 要的；加 spec 的參數型別與「兩種參數順序」案例。第 199 行「泛型 lambda 沒有單一參數列」是 J-10 的一條（§1.2 編譯失敗測試的形狀 (b)） |
| `tests/AccessCheckTests.cpp`（24 案例） | 見下表 | — |
| `tests/PageGuardTests.cpp`（19 案例） | **留在 spike，等 A.6**；採用後搬改到 J-5 第三層 | 使用者 2026-11-12 定判定 1 讀法 (i)：「頁保護暫不進 A.6；下一批要重新宣告、重跑同一組測試才能用」（`spike-pageguard-report` 判定 1 的讀法）。A.6 採用前搬過去沒有規格可對 |
| `src/PageGuard.cpp`、`include/kernel_spike/PageGuard.hpp` | **拆兩半**：平台層（頁配置、保護切換、handler 安裝與串接）→`engine/os`；區間表與「記進 context→解除→返回」的策略→kernel | §1.4 的守衛探測（27 筆）；spike 報告建議 10「平台層放 OS 子系統」；memory os-layer-original-intent（OS 層＝adaptor，行為變更要有 Feature——Feature 就是 J-5 第三層）。這是 OS 層的 Architecture Delta（新責務、os.md DI 要加條），交架構師 |
| `mutations.json`（41 條） | **搬改**→`docs/architecture/plans/kernel/scripts/mutations.json` | §2.4 |
| `CMakeLists.txt` | **刪**（連同 `spikes/kernel/` 整個目錄） | §2.6 的時點；AGENTS §7 不留空目錄 |

`RegistryWorldTests.cpp`（行號為 `TEST_CASE` 所在）：

| 行 | 案例 | 去處 | 理由 |
|---|---|---|---|
| 14 | ids follow registration order | **搬改**→J-18 | 性質相同；J-18 另加「交換 plugin 順序」「不同連結順序 id 相同」「fingerprint 跨平台 golden」 |
| 40 | lookup does not create ids, a type registers once | **搬改**→J-19 | 「查未登記不配發」是 spec C.3；「登記兩次」是 R-2～R-6 的一例 |
| 50 | a System over an unregistered type fails when it is made | **重寫**→J-19／J-24 | spike 在 `MakeSystem` 時斷言；spec 的啟動錯誤在 `finish` 回報且一次收集全部（C.4、D.2） |
| 74 | type-erased columns move, align and destroy | **搬改**→J-7 | `CountingDtor` 記解構順序（W-7）；TypeOps 的 move／align 檢查留著 |
| 94 | events readable in the same tick and cleared at the sync point | **重寫**→J-36 | spec 的事件在**下一 tick 也讀得到、隔兩 tick 讀不到**，順序＝(tick, writerPos, seq)（F-7、F-8）；spike 的「同步點清掉」是不同語意，照搬會固定錯的性質 |
| 114 | deferred commands run in queue order at the sync point | **重寫**→J-31／J-32／J-33 | spec F-2 的套用順序由鍵決定、不依記錄順序（突變「依記錄順序套用」必須被殺）；spike 的「queue order」恰好是那個突變 |

`AccessCheckTests.cpp`：

| 行 | 案例 | 去處 | 理由 |
|---|---|---|---|
| 43 | Build: debug checks, release does not | **重寫**→「驗證開／關」兩個 World 的對照 | §2.1 第 2 點 |
| 53、90、104 | Control ×3 | **搬改**→J-5 控制組 | 宣告的讀寫不誤報（W-5）；第 90 行「host 經 World API 寫」改到 Ready 窗口（W-6b） |
| 120、158、173、189 | Bypass (a1)(b1)(b2)(b3) | **搬改**→J-5 原文 (a)(b) | 斷言→Err（§2.1 第 1 點）；(b3) Resource 保持第二層攔（spike 以邊界比對攔到）；頁保護攔不到 (b3) 的事實屬第三層的 Limit |
| 138 | Bypass (a2) host writes outside any System | **重寫**→J-6 | spec 把 Host 在 Running 期間寫 World 定為 Programmer Error（W-6），不是 System 邊界的差異 |
| 206、226、247、268、288 | Bypass (c1)-(c5) | **搬改**→J-5 原文 (c)＋「只宣告 Read 卻經 World API 取可寫」 | 存取當下攔（W-5）；斷言→Err |
| 312 | Always: structural changes and nested runs | **搬改**→J-3 | `GYO_CHECK_ASSERTS`（W-3） |
| 327 | Limit: restored write | **分兩半**：驗證只有兩層時仍是 Limit（L1）；第三層開著時變成 Bypass（`PageGuardTests.cpp:164` 已有） | spike-report 範圍の限界 L1 |
| 343 | Limit: baseline after host touched | **搬**（Limit，L6） | 第三層也攔不到（`PageGuardTests.cpp:538`） |
| 359 | Limit: another thread | **搬**（Limit，L7） | Scheduler 批前不解 |
| 374 | Limit: kept reference read | **搬**（Limit，L2） | 讀不改位元組 |
| 386 | Limit: kept reference written with Write<T> | **搬**（Limit，L3） | `PageGuardTests.cpp:554` 同形 |
| 401 | Limit: heap memory of a resource | **搬**（Limit，L4） | — |
| 415 | Limit: state outside the World | **搬**（Limit，L5） | — |

`PageGuardTests.cpp`（A.6 採用第三層之後；行號為 `TEST_CASE`）：

| 行 | 案例 | 去處 | 理由 |
|---|---|---|---|
| 137 | Build: whole pages for Exclusive columns | **搬改** | 頁對齊是第三層的前提；「release 用 operator new」改成「開關關時用一般配置」 |
| 164、188、213、230、269 | Bypass L1、L6（宣告形狀）、(b1)、(b2)、after Write<T> | **搬改**→J-5 第三層 | 攔到的形狀；Err 形狀照 spec |
| 251 | Bypass (b3), not caught | **搬**（Limit） | Resource 不在保護頁；A.6 若決定保護 Resource（報告建議 2）再改 Bypass |
| 290、312、333、365 | Control ×4 | **搬改** | 第 365 行「pageGuard off 零保護呼叫」就是「驗證開著」欄位的反面 |
| 393、414、458、500、609 | Continue ×5 | **搬改**→E.5 的 Err／Interrupted | 「Run 回 Err、World 可再 Run」對應 J-35 的 Restore；跨 World（500）與 dropped（609）是 r2 補的行為測試，保留 |
| 538、554 | Limit L6、L3 | **搬**（Limit） | — |
| 650 | Cost（printed, not judged） | **不進 ctest**；改成像 `gyo_time_wake_record` 的記錄用執行檔（`tests/common/time/CMakeLists.txt:7-9`「Deliberately not CTests」） | 共用 runner 上的 µs 計時是雜訊，L1 不該有它；判定 4 的門檻是 L2 事前宣告的事 |

### 2.3 平台層進 `engine/os` 的形狀（給 Delta 用；不在本批決定）

- `engine/os` 新增：`PageSize`／`AllocatePages`／`FreePages`／`SetWritable`（對應 `PageGuard.hpp:26-36`）、`InstallFaultHandler`／`EnsureFaultHandler`（第 44-51 行）、handler 統計（第 65-71 行）。handler 要能回呼 kernel 的「這個位址是不是受保護的欄？」——spike 把區間表放在平台層同一檔（`PageGuard.cpp:43-62`），進 Engine 時區間表與 `RunContext` 留在 kernel，`engine/os` 只提供「安裝一個 `bool(*)(uintptr_t address, bool isWrite)` 回呼」的介面，否則 `engine/os` 會反向依賴 kernel（AGENTS §6）。
- os.md 的 Design Invariants 要加：OS 層可以持有一個行程級的故障處理器；它只在回呼說「是我的」時改保護並返回，其他一律交前任（spike r2 的串接策略，報告建議 4）。
- `gyo_expect_links`：kernel 連 `GYO::Os` 必須 PRIVATE（trace 的先例 `tests/common/trace/CMakeLists.txt:12-19` 檢查 interface 不外露 `GYO::Os`）；kernel 若是 INTERFACE 函式庫做不到 PRIVATE，所以有平台層的那一刻 kernel 要變 STATIC（§1.1 第 1 項的括號）。

### 2.4 `mutations.json` 與 `run_mutations.py` 的歸屬

- **目錄**：`spikes/kernel/mutations.json`（41 條：`World.hpp` 13、`Params.hpp` 9、`AccessCheckTests.cpp` 7、`Access.hpp` 6、`Registry.hpp` 2、`AccessDerivationTests.cpp` 2、`PageGuard.cpp` 2）→ `docs/architecture/plans/kernel/scripts/mutations.json`（spec J.3 原文；entity-store 先例 `docs/architecture/plans/entity-store/scripts/mutations.json`）。每條的 `file`／`find`／`replace` 都指 spike 的程式文字，**全部要重寫**成 kernel 的；能搬的是 `id`／`what`／`expect`／`cases` 的意圖，加上 spec J.2 突變欄與 J.3 十六條最優先突變。`target` 改 `gyo_kernel_tests`。
- **執行器**：不複製。沿用 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --catalog <kernel 目錄> --build-dir build/target/_build/test`（spike 已這樣用：`spike-report` 第 124、146 行；描述見 `run_mutations.py:1-26`）。它支援 `expect_build_failure`、`command`、`platforms`，kernel 要的都有。
- **`expect_build_failure` 的 regex**：spike 第三輪把 16 條 regex 逐條對 AppleClang 的輸出核過（`spike-report` 第 124 行、R-10）。gcc 與 MSVC 的 `static_assert` 訊息格式不同；突變只在本機跑（不進 CI），所以 regex 只要在跑的那台機器對得上，但目錄的 `description` 要寫明「以 AppleClang 核對」，換機器時先跑 baseline。
- **不進 CI**：memory mutation-rerun-scope（每批只跑相關突變，PR 線合併前背景完整跑一次）。

### 2.5 `DOCTEST_CONFIG_NO_POSIX_SIGNALS` 與 sanitizer

- **為什麼 spike 要關**：doctest 在 POSIX 預設（doctest.h 第 281-283 行自動開 `DOCTEST_CONFIG_POSIX_SIGNALS`）每個案例前裝自己的 SIGSEGV／SIGBUS handler，會搶走頁保護的故障（`spikes/kernel/CMakeLists.txt:62-71`）。Windows 不需要：doctest 用 `SetUnhandledExceptionFilter`（doctest.h 第 4711 行），VEH 在它之前執行（§3.3）。
- **方案**：第三層的測試放**獨立執行檔** `gyo_kernel_pageguard_tests`，只有它定義 `DOCTEST_CONFIG_NO_POSIX_SIGNALS`；`gyo_kernel_tests` 其他案例保留 doctest 的致命信號報告（crash 時有案例名）。代價是兩個 main、兩個 ctest 項，比整個 kernel 測試失去 crash 報告便宜。
- **sanitizer 互斥**：ASan／TSan 自己安裝 SIGSEGV handler（Windows 上 ASan 也裝 VEH），與頁保護不能共存（`PageGuard.hpp:41-43` 自述）。CI 不跑 sanitizer（`docs/architecture/plans/framework-rework/scripts/platform_path_dialog_mutations-asan.json:3` 寫明「CI runs no sanitizer」；本機有 `build/target/_build/tsan`）。做法：`gyo_kernel_pageguard_tests` 在編譯期偵測 `__SANITIZE_ADDRESS__`／`__SANITIZE_THREAD__`／`__has_feature(address_sanitizer|thread_sanitizer)`，命中就 `return 77`，ctest 設 `SKIP_RETURN_CODE 77`（`tests/common/os/CMakeLists.txt:46-54` 的先例）——sanitizer 建置下顯示 skipped 而不是誤判，缺口看得見。不用 `doctest::skip`，因為它不會出現在 ctest 的 skipped 計數。
- **組態**：L1 主圖是 RelWithDebInfo（§1.4 最後一列）。spike 在 CI 只以 Debug 建過 MSVC；用 `volatile` 防 `-O3` 折疊的寫法（`spike-pageguard-report` 差異 7）在 MSVC `/O2` 下**沒有 CI 證據**。搬進 build graph 後 L1 第一次就會跑到這條路，事前宣告要列為風險。

### 2.6 `cross-platform.yml` 的獨立 spike 步驟何時移除

- **條件**（兩者都要）：(a) `tests/common/kernel/` 的 J-12（簽名推導）與 J-5 兩層（三種繞路）在 L1 三平台綠——spike 判定 1、2 的覆蓋面被正式測試接手；(b) `PageGuardTests.cpp` 已搬（A.6 採用）或 A.6 決定不採用第三層。只滿足 (a) 時 spike 步驟仍是頁保護三平台證據的唯一來源，不能刪。
- **要改的地方**（都在 `.github/`，交使用者）：步驟本體（第 188～202 行）、上傳路徑兩行（第 272～273 行）、摘要表的 `KERNEL_SPIKE_RESULT` env 與列（第 288、303 行）、`spikes/kernel/` 整個目錄（AGENTS §7：不留空目錄、不留無效引用）。報告與 dev_log 裡指向 `spikes/kernel/` 的路徑是歷史紀錄，不改寫，在 README 加一句「已於 PR # 移除」。
- **零風險小修的檢查結果：沒有找到。** xml 檔名（`kernel-spike-tests.xml` 寫到 `$PRESET/logs/`，第 202 行）與上傳路徑（`logs` 整個目錄，第 264 行）一致；`-kernel-spike/Testing/Temporary` 與 `CMakeConfigureLog.yaml`（第 272～273 行）在 artifact 裡都在（§3.1 的檔就是從那裡來）；摘要表有列。步驟註解只提判定 1、沒提頁保護——不影響行為，等移除時一起消失，不單獨改。

## 3. Windows 的 VEH 在 CI 的行為

### 3.1 「有跑且過，不是 skip」的直接證據（`build/evidence/kernel-pageguard/l1-43169204148/windows-x64/`）

| 檔 | 行 | 內容 |
|---|---|---|
| `ci-windows/logs/kernel-spike-tests.xml` | 3-6 | `tests="2" failures="0" disabled="0" skipped="0"` |
| 同上 | 11 | `gyo_kernel_spike_tests_debug` `status="run"` `time="0.317196"` |
| 同上 | 13 | `cmake_labels` = `cpu` |
| 同上 | 24 | `kernel spike page guard: System 'Sneak' wrote component 'Position' (id 0) at 0x1ead1310000 without declaring a write`——這一行只會由 VEH 處理器記下的故障產生（`PageGuard.cpp:137-160` `HandleFault`→`RecordFault`，Run 結束時印出），是 handler **確實執行**的證據，不是 skip |
| 同上 | 29 | `gyo_kernel_spike_tests_release` `status="run"` |
| 同上 | 42 | release 執行檔 `51 passed | 0 failed | 1 skipped`——1 skip 是 Cost 案例 `doctest::skip(!kPageGuard)`（`PageGuardTests.cpp:650-651`），release 目標 pageGuard 關，依設計 |
| `ci-windows-kernel-spike/Testing/Temporary/LastTest.log` | 5 | 命令 `gyo_kernel_spike_tests_debug.exe` |
| 同上 | 11-13 | 第二層的訊息（`access check: System 'Tamper' ...`） |
| 同上 | 36 | `page guard: System 'Cheat' wrote component 'Position' (id 0) at 0x1ead1310000 ...`——debug 執行檔第一筆頁保護訊息 |
| 同上 | 48-64 | `page guard: System 'Spray' wrote component 'Lane' (id 0)` … `(id 15)` 十六筆，第 64 行 `System 'Spray' faulted on 4 more columns than it could record`——dropped 路徑（r2 加的 `PageGuardTests.cpp:609`）在 Windows 真的走到 |
| 同上 | 122 | `page guard: System 'Stray' wrote component 'Lane' (id 2) ...` |
| 同上 | 130 | `MESSAGE: page size 4096` |
| 同上 | 138 | `PAGEGUARD_COST protect_us=0.5 switch_us=3.1 full_us=98.1 zone_b_us=98.6 page_bytes=4096`（Windows 的成本數字；比 macOS 快） |
| 同上 | 141-143 | debug 執行檔 `test cases: 52 | 52 passed | 0 failed | 0 skipped`、`assertions: 467 | 467 passed`、`Status: SUCCESS!`——52 = 既有 33 + 頁保護 19，0 skipped，所以 19 個頁保護案例全部執行 |
| 同上 | 147、174 | `Test Passed.` ×2 |
| 同上 | 168 | release 執行檔 `51 passed | 0 failed | 1 skipped` |
| `ci-windows/logs/kernel-spike-tests.log` | 3、5、7 | ctest：兩個測試 Passed，`100% tests passed out of 2` |

對照：linux-x64（gcc）與 macos-arm64 的同名 xml 第 3-6、11、24、29、42 行內容相同，頁大小 4096／16384（`spike-pageguard-report` 已記）。

### 3.2 runner 沒有 debugger／ASan（**推斷**，不是直接確認）

- **ASan／其他 sanitizer**：`ci-windows/logs/kernel-spike-configure.log` 第 1 行編譯器 `MSVC 19.51.36260.0`（VS 18 Enterprise，toolset 14.51），其餘 26 行沒有任何 sanitizer 旗標；`kernel-spike-build.log` 34 行只有 doctest.h:539 的 C5285 警告（第三方），無 `/fsanitize`；spike 的 CMake 不設 sanitizer；workflow 第 198 行只傳 `CMAKE_BUILD_TYPE=Debug` 與 debug-info 格式；repo 裡提到 sanitizer 的只有兩個本機突變目錄，且寫明 CI 不跑。Ninja 的 build log **不印命令列**，所以實際旗標是從 CMake 對 MSVC Debug 的預設推的：`/MDd /Z7 /Ob0 /Od /RTC1`（`/Z7` 由 `Embedded` 來）。
- **debugger**：GitHub-hosted runner 由 pwsh 步驟直接執行 ctest，沒有附加 debugger 的機制；doctest 若偵測到 debugger（`IsDebuggerPresent`，doctest.h 第 4591 行）會在失敗時 `DOCTEST_BREAK_INTO_DEBUGGER`——但沒有失敗，所以這條路徑沒有觀測到。**不能直接確認**的原因：spike 沒有印 `IsDebuggerPresent()`。搬進正式測試時建議在「驗證開著」欄位旁印它（spec J-5 的「無違規報告帶欄位」處），一行就把推斷變成證據。

### 3.3 MSVC Debug 組態下 VEH 處理器的已知風險（給 A.6／平台層的事前宣告用）

1. **與 doctest 的 SEH 共存**（現況可行，但要知道為什麼）：doctest 在 MSVC 自動開 `DOCTEST_CONFIG_WINDOWS_SEH`（doctest.h 第 274-275 行），每個案例裝 `SetUnhandledExceptionFilter` + `SetThreadStackGuarantee(32 KiB)` + `std::set_terminate` + `SIGABRT` handler + `SetErrorMode` + `_set_abort_behavior` + `_CrtSetReportMode`（第 4700-4760 行）。VEH（`AddVectoredExceptionHandler(1, ...)`，`PageGuard.cpp:347`）在所有 SEH frame 與 UEF 之前執行；受保護欄的寫入由 VEH 以 `EXCEPTION_CONTINUE_EXECUTION` 解決，doctest 永遠看不到；其他 AV 由 VEH 回 `CONTINUE_SEARCH`，doctest 的 UEF 照常報告。這個順序是 Windows 的規則，不是 doctest 的配合。
2. **`/EHsc`**：C++ 例外在 Windows 是 SEH code `0xE06D7363`，**每一次 throw 都會先呼叫 VEH**；spike 以 `ExceptionCode != EXCEPTION_ACCESS_VIOLATION` 過濾（`PageGuard.cpp:171-176`），正確但有固定成本（測試用 throw 做 `GYO_CHECK_ASSERTS` 時每次都進一趟）。`/EHsc` 下 `catch(...)` 不接 AV，所以沒有「測試的 catch(...) 吞掉頁故障」的問題；若將來有人改 `/EHa`，VEH 仍先跑，頁保護不受影響，但未受保護的 AV 會被 `catch(...)` 吞掉，doctest 報不出。
3. **Debug CRT**：`/RTC1` 的堆疊檢查與 `0xCC` 填充只碰堆疊；`/MDd` 的 debug heap 在配置／釋放時寫 guard byte（`0xFD`）到區塊**周圍**——spike 的欄用 `VirtualAlloc`（`PageGuard.cpp:308`），不在 debug heap 上，所以沒事。風險在將來：若受保護的欄改用 `operator new`／allocator，debug heap 會在 Ready（沒有 System 執行時）寫 guard byte 到受保護頁 → `faultsWithoutSystem` → `Fatal`→abort（`PageGuard.cpp:144-148`）。欄儲存必須維持獨立頁配置。
4. **debugger 附加時**：first-chance exception 先到 debugger、再到 VEH。Visual Studio 預設對 Win32 Access Violation 的 first-chance 會中斷——每一次受保護寫入都會停在 debugger。開發者本機要在例外設定關掉 `0xC0000005` 的 first-chance，或接受 `IsDebuggerPresent()` 時關第三層；要寫進開發文件。CI 無此問題（§3.2）。
5. **VEH 的順序與移除**：`AddVectoredExceptionHandler(1, ...)` 放最前，但之後任何函式庫以 `1` 再加會排到它前面；spike **丟掉了回傳的 handle**（`PageGuard.cpp:347-348` 只在斷言裡用），所以 `RemoveVectoredExceptionHandler` 不可能、`EnsureFaultHandler` 在 Windows 是 no-op（`PageGuard.hpp:49-51`）。Engine 版要保存 handle，才能在開關關掉時卸除、在 EnterRun 時重排到最前（與 POSIX 的 `reinstalls` 計數對齊）。
6. **ASan on Windows**：ASan 自己裝 VEH 與攔截 `VirtualAlloc`／`VirtualProtect`——與第三層互斥（§2.5 的 77 退出碼就是為此）。
7. **存取種類**：spike 只處理 `ExceptionInformation[0] == 1`（寫）；讀（0）與 DEP 執行（8）回 `CONTINUE_SEARCH`。頁用 `PAGE_READONLY` 不用 `PAGE_GUARD`（`PAGE_GUARD` 是一次性的 `STATUS_GUARD_PAGE_VIOLATION`，語意不同）。正確，寫進 A.6 時保留。
8. **handler 內的工作**：`WriteFile(STD_ERROR_HANDLE)`＋`std::abort`（`PageGuard.cpp:87-93`、`113-119`）；abort 會觸發 doctest 的 SIGABRT handler → `std::exit(EXIT_FAILURE)`（doctest.h 第 4738 行），ctest 判失敗——可觀測，好。VEH 在故障執行緒的堆疊上跑，深遞迴接近堆疊底時 handler 自身可能再溢位；spike 的 handler 只做二分搜尋＋`VirtualProtect`，夠淺。
9. **64 KiB 配置粒度**：每欄 `VirtualAlloc` 保留 64 KiB 位址空間（`spike-pageguard-report` 記憶體一節：1000 欄 ≈ 62.5 MiB 保留、3.9 MiB commit）。L1 runner 沒問題；是 A.6 容量估算的事。
10. **MSVC 最佳化組態未驗**：見 §2.5 最後一點——L1 主圖是 RelWithDebInfo，spike 的 Windows 證據全是 Debug。

## 4. 給架構師的結構性項目（只寫，不做）

1. **OS 層 Architecture Delta**：頁保護平台層進 `engine/os`（§2.3）。影響 os.md Design Invariants、`gyo_expect_links(gyo_os ...)` 的期望（Windows 可能多 `kernel32` 以外的無；POSIX 不變）、kernel 連 `GYO::Os` PRIVATE → kernel 變 STATIC。
2. **編譯失敗測試的形狀**（§1.2）：`try_compile` 預期失敗 vs. trait＋doctest；repo 無先例，第一次做要過 Windows L1。
3. **第三層的獨立測試執行檔**（§2.5）：`gyo_kernel_pageguard_tests`＋`SKIP_RETURN_CODE 77`。
4. **spike 步驟移除的時點與範圍**（§2.6）：兩條件都滿足後，一個 PR 刪步驟、上傳路徑、摘要列與 `spikes/kernel/`；`.github/` 改動交使用者。
5. **驗證開關是執行期選項**（§1.4、§2.1）：進 build graph 後 L1 只有 RelWithDebInfo，spike 的 NDEBUG 雙目標形狀不能帶進來。

## 範圍の限界（本批）

- Windows 專屬項（§1.4）只讀 CMake／workflow，本機 macOS 沒有實測；假子系統在 MSVC 下的 `/W4` 零警告沒有證據。
- §3.2 是推斷：runner 沒有 debugger／ASan 沒有直接的 log 行可引；建議的 `IsDebuggerPresent()` 一行要等正式測試。
- 假子系統只走了 INTERFACE 函式庫的形狀；STATIC（有 `.cpp`、PRIVATE 連 `GYO::Os`）的 `gyo_expect_links` 寫法沒有走，照 trace 的先例即可，但沒有實證。
- 沒有在 Linux `core` preset 實跑假子系統（§1.3 是從 `tests/common/CMakeLists.txt` 無條件加子目錄推的）。
