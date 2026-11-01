# framework 可用任何 Engine 模組：DI 2／DI 5 與守衛的判定變更（已核准）

日期：2026-11-01。批次 v8-framework-engine-access。依 AGENTS.md §3（Design Invariants 的變更）與 §14.4（Design Invariants 的守衛判定變更）提出。
**狀態：2026-11-01 使用者核准（「核准」）。** 以 PR #132（經 #131 合併進 master `b6182b2b`）的 DI 2 與守衛為起點。第一版（在 #132 之前做）已作廢，存於證據資料夾的 `wip_before_pr132.patch` 與 `engine-access-judgment-change.draft.md`。

## 起因

- 架構會話（例行架構掃描）查證：「不連結 `GYO::Input`、不用 SDL_GPU」源自 10-30 的 ui_editor DI 1，沒有記錄理由；`GYO::Input` 只連 `GYO::Base`（`engine/input/CMakeLists.txt:14`），禁止它和 Editor → Engine 的方向相反；已造成 Input→Ui 的轉換寫了兩份（pvp 與 ui_editor 的預覽）。
- 使用者的規則（經架構會話轉述的原話）：「Editor 可以連結 framework 和 Engine，但是 framework 是通用層，共通功能優先 framework。如果有觀察到有多處使用，即使 Engine 有也先實作 framework，再調用。」
- 2026-11-01 使用者：「照提案」（DI 5 的界線）；「你先改了，ui_editor 才能跟進」；「等合併後重做」（以 #132 為起點）。
- #132（工具線的 macOS 日文 IME 修正，使用者核准）已先拿掉 `GYO::Input` 的禁令，但只讓 ImGui 綁定連 `GYO::InputBackendSDL`，並有自測擋核心連它；DI 2 仍寫「no SDL_GPU」。
- 工具線 T4 的兩份工具 DI 1 已用對齊後的措辭核准（`b6182b2b`）。

## 條文（docs/framework/architecture.md）

**DI 2** 前半，從 #132 的版本改為：

> Dependencies point one way: game or tool → framework → engine. The framework includes nothing from `apps/` or `tools/`, links no game and names no product, and the engine never names it. It may use any Engine module, input included. It reaches SDL only as a game does: through Engine's SDL backend targets, which keep SDL private, or, where an Engine contract allows it and only inside a binding, an Engine `*Native` target.

刪去 #132 版的「links no game and no SDL_GPU, so a tool's own Design Invariant 1 (no game, no SDL_GPU) still holds through it. Engine's capabilities are open to the framework as to a game or tool, input included (…)」。核心規則的「Engine's neutral targets」改為「Engine's SDL backend targets」。#132 加的「The ImGui binding reads typed text … through Engine's input backend」保留。新增：

> Adding an Engine target that is not `*Native` to a positive list follows this item, though the guard change still follows AGENTS.md section 14.4; adding an SDL, `*Native` or third-party target changes this item.

（検証者指出：初稿寫「加中立 Engine target 不需另外同意」，會預先免除 §14.4 的核准，而且「中立」沒有定義。改成：加非 `*Native` 的 Engine target 不算改本條，但守衛的變更照 §14.4 交使用者核准。若希望之後加 Engine target 完全不必逐條核准，要另外讓守衛自己從 Engine 的 CMake 讀出可用的 target，這批不做。）

**DI 5**，「Otherwise only what two or more products share enters. The game part enters only on the condition above.」改為：

> What two or more products use (a game and a tool, or two tools) enters the framework first, even where Engine has the capability: the framework calls Engine and the products call the framework. What one product alone needs, it takes from Engine directly. The game part, a game's own composition (its loop, its scenes), enters only on the condition above; a capability that a game and a tool both call is shared and enters by this rule (user, 2026-11-01).

## 守衛的變更（`tests/framework/framework_dependency_guard.py`）

- 核心的正向清單加 `GYO::Input`、`GYO::InputBackendSDL`；「ratchet 後新增的名稱」（`NEW_TARGETS`）加 `GYO::Input`（`GYO::InputBackendSDL` #132 已加）。兩個綁定的清單不變（ImGui 綁定已有 `GYO::InputBackendSDL`）。
- #132 的「核心 include `engine/input/` 的 `*Native` 標頭是硬規則、永不列入例外清單」保留。
- SDL_GPU 的硬規則保留，理由寫明：ratchet 前就拒絕的東西永不列入例外清單。符號規則不計 SDL_GPU 的名稱（避免重複報），所以在例外清單裡的檔案「新增」一個 SDL_GPU 使用時計數不變，只有這條規則看得到。例外清單在 v8-framework-invariants 清空後，這條規則和符號規則裡的略過一起拿掉，改由 `SDL_` 符號規則報。
- configure 時的連結檢查（`tests/framework/CMakeLists.txt`）不改：它不禁 `GYO::Input`；`GYO::InputBackendSDL` 對 SDL 的連結是 PRIVATE（`$<LINK_ONLY:…>`）。

## §14.4 的五項

以 #132 合併後的判定（master `b6182b2b`）為「舊判定」。

1. **守住的性質**：#132 的判定守的是「`GYO::Input` 的能力開放給 framework，但只有 ImGui 綁定可以連 Engine 的 SDL 輸入後端；核心不連它，也不能 include input 的 `*Native` 標頭」。精確地說：核心具名 `GYO::Input` 是硬規則；核心具名 `GYO::InputBackendSDL` 是可列入例外清單的軟規則（它在 ratchet 後新增的名稱裡，但不在核心的清單）。新判定把「核心不連輸入後端」撤回（DI 2：framework 可用任何 Engine 模組，SDL 經 Engine 的 SDL 後端目標）。仍要守住的是：核心不 include、不具名、不連 SDL／ImGui／nlohmann 與 `*Native`；`*Native` 只在 ImGui 綁定；每個綁定有自己的正向清單；每條依存邊都要列在清單上。
2. **放寬的面**（以前失敗、現在通過）：
   - (a) 核心的 CMake 具名 `GYO::Input`（以前是 ratchet 前就拒絕的名稱，硬規則）。
   - (b) 核心的 CMake 具名 `GYO::InputBackendSDL`（以前由 #132 的自測 `test_input_backend_target_fails_in_the_core` 擋住；這條自測換成 `test_input_targets_in_the_core_pass`）。
   - 因此舊突變 fw-guard-input-target（核心具名 `GYO::Input`）不再被殺，換成 fw-guard-unlisted-engine-target（清單外的 `GYO::RenderBackendSDLGPU` 仍失敗）。
   - include 沒有改變：#132 已讓核心與綁定 include `engine/input/` 的非 Native 標頭。
3. **收緊的面**：
   - (c) 例外清單裡某個核心 CMake 檔的 target 計數若涵蓋了 `GYO::InputBackendSDL`（舊判定下是軟規則，可被計數涵蓋），新判定下它不再算一個發現，計數變多，守衛要求縮小清單而失敗。例：`framework/Old.cmake` 寫 `target_link_libraries(a PRIVATE imgui::core GYO::InputBackendSDL)`、例外清單 `framework/Old.cmake target 2`，舊判定通過、新判定失敗。這是 ratchet 要求縮小，真實的例外清單沒有這種位置（`framework/Legacy.cmake` 不具名任何 input target），真實守衛通過。初稿寫「沒有」，検証者指出後更正。
   - 其他：舊判定失敗的輸入中，除了 (a)(b)，新判定都仍失敗：既有自測的失敗案例全部保留（兩條的範例從 `GYO::Input` 換成 `GYO::RenderBackendSDLGPU`，它在舊判定也失敗），其餘 18 條既有突變照舊被殺。
4. **放寬後，其他性質由誰守住**：
   - SDL 只經 Engine 後端目標：核心的 include／符號規則與 configure 時的連結檢查（不變）；突變 fw-guard-core-sdl-call、fw-guard-core-sdl-forward、fw-link-closure-sdl、fw-link-genex。`GYO::InputBackendSDL` 對 SDL 的連結是 PRIVATE，連結檢查不把它算成核心連 SDL：探測確認（下面 5）。
   - 核心不碰 input 的 `*Native`：#132 的硬規則（自測 `test_input_native_header_fails_in_the_core`、`test_input_native_header_in_the_core_is_never_allowlisted`）；新突變 fw-guard-input-native-allowlistable。連 `GYO::InputBackendSDLNative` 由連結檢查的 native 規則與 Python 守衛的清單擋（不在任何清單）。
   - 綁定的清單不因核心放寬而放寬：自測 `test_input_target_in_a_binding_fails`、#132 的 `test_json_binding_input_backend_fails`；突變 fw-guard-binding-input-target、fw-guard-binding-list-widened。
   - SDL_GPU 不能經由例外清單進來：自測 `test_trading_an_allowlisted_site_for_an_old_refusal_fails` 新增的一段（在例外檔案新增一個 SDL_GPU 使用，只報那一條）、#132 改寫的 `test_hard_rules_are_never_allowlisted`；突變 fw-guard-gpu-added、fw-guard-gpu-rule-dropped。
5. **根據與證據**：使用者的 Mac，主 checkout `build/evidence/framework-rework/v8-framework-engine-access/`：
   - 同步 master 後：`after_sync_build.txt`（建置成功）、`after_sync_ctest.txt`（123/123）。
   - Red：`red_selftest_2.txt`（47 條中 `test_input_targets_in_the_core_pass` 失敗）。
   - Green：`green_selftest_2.txt`（47/47）；検証者的修正後 `green_selftest_3.txt`（47/47）；真實守衛通過；`ctest_framework_2.txt`（framework、boundary、guard 相關 16/16）。
   - 突變：`mutations_2.txt`（事前宣告 fw-guard-* 16、fw-link-* 8，24/24 killed、stale 0）。第一次呼叫因 zsh 不切分變數，執行器收到的 `--only` 是一個字串，0 條執行（`mutations_2_empty_argument.txt`），不是判定的結果；改用 xargs 傳入同樣 24 個 id 後執行。
   - 探測：`probe_input_link_configure_2.txt`（暫時讓 `gyo_tool_framework_window` 連 `GYO::Input GYO::InputBackendSDL`，configure 的連結檢查與守衛都通過，之後還原；臨時修改的內容事後補記在檔尾）。
   - 検証者的修正後：`mutations_3.txt`（事前宣告 fw-guard-gpu-added、fw-guard-gpu-rule-dropped，2/2 killed）。

## 検証者的核對

Fable 5.1／high（2026-11-01 使用者：「用 Fable」），唯讀；實驗在會話暫存區的副本做（舊／新守衛各跑 602 個合成輸入、舊自測對新守衛與新自測對舊守衛交叉跑、16 條 fw-guard 突變以 find／replace 重跑、照抄 `tests/framework/CMakeLists.txt` 的連結檢查建小專案驗證）。

- 1 確認（補上 `GYO::InputBackendSDL` 在核心原是軟規則）；2 確認，(a)(b) 是完整的清單（舊失敗、新通過的 11 個輸入都是核心 CMake 具名這兩個 target）；3 不成立：有收緊面 (c)，已補；4 確認（具名的自測與突變都存在且守住所說的事；連結檢查：核心連 `GYO::Input`、`GYO::InputBackendSDL` 通過，連 `GYO::InputBackendSDLNative`、`SDL3::SDL3` 失敗）；5 確認（各計數與證據檔一致）。
- 0 blocker。major 2：收緊面 (c) 漏列（已更正）；DI 2 新句會預先免除 §14.4 且「中立」沒有定義（已改窄，見上）。minor 2：SDL_GPU 硬規則的理由寫錯（已改守衛說明、自測與突變）；探測證據沒記臨時修改（已補記）。
- 検証者的原始輸出在會話暫存區，未保存；結論在本文件與 HANDOFF。
