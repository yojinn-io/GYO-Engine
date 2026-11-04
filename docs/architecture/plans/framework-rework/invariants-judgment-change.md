# framework 收尾：例外清單拿掉、DI 4 定稿的判定變更（已核准）

日期：2026-11-01。批次 v8-framework-invariants（PR B）。依 AGENTS.md §3（Design Invariants 的變更）與 §14.4（Design Invariants 的守衛判定變更）提出。
**狀態：2026-11-02 使用者核准（「核准」）。** 舊判定＝master `654df310`（#133 合併後）的守衛、自測與 configure 時的連結檢查。

## 起因

PLAN 第 2.6 節：工具線切換（#131）後刪舊件，例外清單清空，DI 4 與 Using it 定稿。舊件刪掉後，例外清單的 9 行都失去對象（檔案或 target 不存在），清單本身已經是空的。

## 條文（docs/framework/architecture.md）

**DI 2** 的守衛句改為：

> There are no exceptions: the allowlist of the core's old sites emptied and was removed in `v8-framework-invariants`. Guards: the dependency guard (core rules and one positive list per binding; the removed allowlist coming back fails) and the configure-time link check in `tests/framework/CMakeLists.txt`.

**DI 4** 改為（原文寫「replaces the file atomically」，但寫檔的 FileService 這批刪了，framework 不再寫任何檔案）：

> The framework writes no file of its own. The ImGui binding writes no side file (`imgui.ini`, ImGui's log). The catalog mount is read-only and has no write API. The JSON binding's `RegisterAsset` returns the new catalog text and writes nothing; the caller replaces the file atomically (through `engine/io`). The new text changes only the entry with the caller's id (on an update, only that entry's own `type` and `path` values), leaves every other byte as it was, and is read again and checked as data against catalog v1 before it is returned. The catalog-level rules follow the catalog-level cases of `tests/common/fixtures/asset_contract/cases.json`, with Engine's path rule (`IsRelativeContentPath`); across catalogs and against shader bundles, `ContentManifest` decides at load. Guards: the ImGui shell's side-file test (I2), the catalog structure tests (C1–C7, E1–E4) and the fw-cat-* and fw-extra-* mutants.

組成清單與 Using it 只描述現況（去掉舊件，加 `AbsolutePath`），不是 Design Invariants。

## 守衛的變更

- `tests/framework/framework_dependency_guard.py`：拿掉例外清單與 ratchet 的機制——硬／軟之分、ratchet 前的 include 前綴與 target 名單（`OLD_*`、`NEW_TARGETS`）、核心 input `*Native` 的專用硬規則、SDL_GPU 的原文規則與符號規則裡的略過、例外清單的 link 行檢查與 `add_library` 讀法。剩下：每個核心與綁定的正向清單（include 前綴、Native 標頭、符號、target）、登錄名稱、project.json、執行檔與安裝。新增：例外清單檔再出現就報「the framework has no exceptions; remove the file」。
- `tests/framework/CMakeLists.txt` 的 configure 時連結檢查：不再讀例外清單，核心連到任何禁止的 target 都失敗。
- 例外清單檔 `tests/framework/framework_guard_allowlist.txt` 刪除。

## §14.4 的五項

1. **守住的性質**：舊判定守的是「核心既有的違規位置凍結在例外清單、只減不增；ratchet 前就拒絕的東西（含 SDL_GPU 名稱，以原文掃描）永不列入例外」。新判定：沒有例外——每個核心檔案與綁定只能照自己的正向清單 include、具名、連結；例外清單檔再出現就失敗；configure 時的連結檢查不允許任何例外。
2. **放寬的面**（以前失敗、現在通過）：framework 檔案的**註解、字串或字元字面值、include 路徑**裡出現 SDL_GPU 名稱（`SDL_*GPU*`）。舊判定以原文掃描 SDL_GPU，這些都失敗；新判定的符號規則只看程式碼（和 `os_boundary_guard.py` 同一個詞法：include 行、註解、字串與字元字面值都先去掉），include 規則只看路徑前綴。例：`// not SDL_GPUDevice`、`"SDL_GPU"`、`#error 'SDL_GPUDevice' is banned`（引號內被當成字元字面值）、`#include "engine/render/SDL_GPUBridge.hpp"`、`#include "gyo/framework/SDL_GPUBridge.hpp"`、本地的 `#include "SDL_GPUHelper.hpp"`（這些標頭今天都不存在）。無法編譯的檔案裡，未結束的 `"` 或 `/*` 之後的文字也會被當成字串或註解去掉。初稿漏了 include 路徑（審査者指出）與字元字面值（検証者指出），已補上。訊息的差異：舊的 SDL_GPU 規則每個名稱報一次，新的符號規則每次出現都報。
3. **收緊的面**（以前通過、現在失敗）：
   - (a) 例外清單檔的任何內容（連只有註解也算）都失敗。
   - (b) 例外清單原本能涵蓋的核心位置（include、符號、target）一律失敗；現況沒有這種位置，真實守衛通過。
   - (c) configure 時的連結檢查：核心連到禁止的 target 不能再列例外。
   - 舊自測對新守衛有 12 條失敗（`old_selftest_vs_new_guard.txt`）：都斷言例外清單的行為或訊息。逐次比對舊自測全部 49 次掃描在新舊守衛下的發現數（斷言不中斷，`old_tests_scan_counts_old_vs_new.txt`）：舊失敗→新通過 0 次；舊通過→新失敗 1 次（`test_allowlisted_core_sites_pass`，就是 (b)）；其餘輸入在新判定下仍失敗，只是訊息變成「the framework has no exceptions」或直接報出該位置。
4. **放寬後，性質由誰守住**：SDL_GPU 的實際使用仍被擋——
   - 程式碼裡的 SDL_GPU 名稱：核心與兩個綁定的 `SDL_` 符號規則（自測 `test_sdl_gpu_is_an_sdl_symbol_in_code_only`；突變 fw-guard-core-sdl-gpu）。
   - 程式碼裡的其他 SDL 名稱（例如前置宣告 `struct SDL_Window;`）：同一條符號規則（突變 fw-guard-core-sdl-forward）。
   - SDL 的標頭：include 規則（核心只能 include `gyo/framework/` 與 Engine 的非 Native 標頭；`SDL3/` 的 include 由自測 `test_sdl_and_imgui_in_the_core_fail`、`test_former_allowlisted_kinds_fail_in_the_core` 守住，沒有對應的突變；Native 標頭由突變 fw-guard-core-native-include、fw-guard-core-input-native 守住）。
   - 連結 SDL3：configure 時的連結檢查（突變 fw-link-closure-sdl、fw-link-genex）。
   - 例外清單不能悄悄回來：突變 fw-guard-allowlist-returns。
   - 只出現在 include 路徑名稱裡的 SDL_GPU：路徑必須在允許的前綴下或是本地標頭。Engine 的標頭由 Engine 自己的 SDL 衛生規則（platform-sdl G1／G2）管，公開標頭不帶 SDL；`gyo/framework/` 底下或本地的標頭本身就在 framework 裡，它的程式碼一樣受符號規則掃描。
5. **根據與證據**：使用者的 Mac，主 checkout `build/evidence/framework-rework/v8-framework-invariants/`：
   - 自測：`selftest_after_b2.txt`（37/37）；交叉：`new_selftest_vs_old_guard.txt`（新自測對舊守衛 3 條失敗：例外清單回來、SDL_GPU 只看程式碼、input Native 加例外清單行）、`old_selftest_vs_new_guard.txt`（舊自測對新守衛 12 條失敗）。
   - 建置與測試：`build_b1_b2.txt`、`ctest_b1_b2.txt`（123/123）；`l2_audio_only_configure.txt`、`l2_audio_only_build.txt`、`l2_no_tools_configure.txt`。
   - 突變：`mutations_framework.txt`（第一次 16/18，兩條預期訊息過時，判定不成立）、`diagnose_fw-link-*.txt`（手動重現）、`mutations_framework_rerun.txt`（使用者「照建議重跑」後 2/2）。合計 18/18、stale 0。

## 審査者的核對

Opus 5.5／high（唯讀子 Agent）。0 blocker、0 major、3 minor、4 nit，都已處理：放寬面漏列 include 路徑（已補）；`docs/architecture.md` 的 framework 列寫「what two or more tools use」但列出的每一項目前只有一個工具使用（改寫並註明 AbsolutePath 依 D-2 留在核心）；`FrameworkError` 的 FileError 註解（framework 已不產生它，改寫）；突變說明的過時字句、fw-abs-not-normal 只在 POSIX 殺得到（加 platforms）；index 混合狀態（整批一起 stage）。確認乾淨：殘留引用、`AbsolutePath` 與舊實作逐行等價、守衛的正向清單沒少規則、刪掉的測試只覆蓋已刪的程式、DI 2／DI 4 與程式一致、可移除性。原始輸出在會話暫存區，未保存。

## 検証者的核對

Fable 5.1／high（使用者在 PR B 開工時同意），唯讀；實驗在會話暫存區的副本做：新舊守衛各跑 243 個合成輸入（核心、兩個綁定、未列的綁定、CMake；include、程式碼／註解／字串／raw 字串／`#error`／`#define`／`#if 0` 裡的符號、target、Native 標頭、SDL_GPU、登錄名稱、project.json、執行檔，各在有與沒有例外清單時），重現舊自測 49 次掃描的比對與兩個交叉執行，以 find／replace 在副本重跑 14 條 Python 檢查的突變（14/14），逐段比對新舊連結檢查。

- 1～5 全部確認。放寬面：舊失敗、新通過的 33 個輸入全是「程式碼以外的 SDL_GPU 名稱」；有例外清單時沒有任何舊失敗、新通過。收緊面 (a)(b)(c) 與 49 次掃描的比對都重現。連結檢查只改了註解、刪了例外清單的讀取、最後的比較改成 `if(found)`；其餘函式逐位元組相同，沒有剩下接受禁止連結的路徑。DI 4 與 `CatalogText.cpp`、framework 沒有寫檔程式都一致。
- 0 blocker、0 major。minor 3：放寬面還包括字元字面值與未結束的引號或註解之後（已補）；fw-guard-core-sdl-forward 守的是符號規則不是 include 規則，`SDL3/` 的 include 沒有突變、只由自測守（已改正歸屬並註明）；include 路徑那一項也涵蓋 `gyo/framework/` 與本地標頭（已改寫）。nit 2：新規則每次出現都報（已補進第 2 項）；未被本文件引用的 `old_failing_inputs_under_new_guard.txt` 只記到每條測試第一個斷言失敗前的掃描（已在檔尾註明，以 `old_tests_scan_counts_old_vs_new.txt` 為準）。
- 検証者的原始輸出在會話暫存區，未保存；結論在本文件與 HANDOFF。
