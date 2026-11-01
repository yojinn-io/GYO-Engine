# framework 連結檢查的判定變更（回歸修補，已核准）

日期：2026-11-01。依 AGENTS.md §14.3（缺陷的處理）與 §14.4（Design Invariants 的守衛判定變更）提出。
**狀態：2026-11-01 使用者核准（「核准」）。**

## 起因

v8-framework-shell 的 L2 發現：只選 audio_editor 時 configure 失敗。configure 時的連結檢查（guard-ratchet 批 `0a635225` 加的，docs/framework/architecture.md Design Invariant 2 的守衛）要求例外清單 `tests/framework/framework_guard_allowlist.txt` 的每一行 `target:<名稱> link …` 都是這次組態有建的 framework target，否則報「不是 framework 的 target，刪掉這行」。舊的 GUI target `gyo_tool_framework_gui` 只在有 SDL 平台、SDL renderer 與 ImGui 的組態才建，只選 audio_editor 時沒有它，所以失敗。`0a635225~1` 在同一組態 configure 成功：這是我引入的回歸，只在本分支（PR #127 未合併）。L1 沒有這個組態。

## 修正

- 「這一行是不是過時了」改由 Python 守衛（`framework_dependency_guard.py`）判斷：每一行的 target 必須由 framework 核心（不是綁定）的某個 CMake 檔以 `add_library` 定義（照 CMake 的讀法：不分大小寫、可以不帶來源；略過註解；ALIAS 不算定義）。它讀原始碼，和組態無關。重複的 link 行也在這裡擋（configure 時本來就擋）。
- configure 時的連結檢查只核對這次組態有建的 target：那個 target 實際連到的禁止名稱要和那一行完全相同（和以前一樣）。這次組態沒有建的 target，它那一行不在 configure 時檢查。

## §14.4 的五項

1. **守住的性質**：核心既有的禁止連結逐 target 凍結（名稱要和那一行完全相同，所以只能減少、不能交換）；framework 已經沒有的 target，它那一行要刪掉。
2. **放寬的面**（以前失敗、現在通過）：
   - (a) 某個組態沒有建某個有例外行的 target 時（例如只選 audio_editor），configure 不再因為那一行失敗，那一行的名稱在那個組態也不檢查。這正是回歸要修的地方。
   - (b) target 在哪裡都沒有定義的過時行，不再在 configure 時被擋，改由依存守衛的測試擋：時點從 configure 移到 ctest。只 configure、建置而不跑 ctest 的人看不到它。
   - (c) 規則是文字比對：`add_library` 寫在永遠不執行的分支裡（例如 `if(FALSE)`），或寫在沒有任何 `add_subdirectory` 會走到的 framework CMake 檔裡，也算「有定義」。這種行以前在 configure 時會被擋，現在每個組態都通過。它不授權任何東西（例外只對實際建出的 target 生效），但過時的行可能留下來，等那個 target 日後真的建出時預先放行那些名稱。
   - (d) 舊判定還順帶保證「有例外行的 target 一定存在」。現在不保證：舊 GUI target 的條件若悄悄變成不成立，只靠 ui_editor 自己的檢查（`tools/ui_editor/CMakeLists.txt:58`，GUI 版本找不到 `GYO::ToolFrameworkGui` 就失敗）擋下。
3. **收緊的面**：沒有。新規則擋下的（target 沒有由核心定義、或屬於綁定）都是舊判定已經擋下的子集；不同的是現在不必跑 CMake 也會擋，並由自測固定。
4. **放寬後性質由誰守住**：
   - 過時的行：Python 守衛。它和 configure 檢查在完全相同的組態執行（兩者都在 `GYO_ACTIVE_TOOLS AND BUILD_TESTING` 時才加入，`build/cmake/GyoBuild.cmake:91-94`）。自測 `test_a_link_line_for_a_target_the_framework_does_not_define_fails`、`test_a_link_line_for_a_binding_target_fails`、`test_a_link_line_reads_add_library_as_cmake_does`、`test_a_duplicate_link_line_fails`；突變 fw-link-stale-line-ignored、fw-link-binding-line-allowed、fw-link-stale-real、fw-link-comment-counts、fw-link-duplicate-allowed。
   - 每一行的名稱：configure 時的連結檢查，在每個建了那個 target 的組態。CI 的 L1 三列（ci-windows、ci-linux、ci-macos 繼承 test preset，`GYO_TOOLS=AUTO`）會建 `gyo_tool_framework` 與 `gyo_tool_framework_gui`。draft PR 的 L1 要手動觸發；acceptance workflow 以產品挑選工具，不一定建 GUI target，所以閘門是 L1。突變 fw-link-trade、fw-link-closure-sdl、fw-link-genex。
   - 重複的行：configure 檢查與 Python 守衛都擋。
5. **根據與證據**：使用者的 Mac，主 checkout `build/evidence/framework-rework/v8-framework-shell/`：
   - Red：`l2_audio_only_configure.txt`（失敗）、`l2_audio_only_configure_before_guard.txt`（`0a635225~1` 成功）、`regression_red_selftest.txt`（新自測 2 條失敗）、`review_red_selftest.txt`（審查後新增 2 條失敗）。
   - Green：`regression_green_selftest.txt`（38/38）、`review_green_selftest.txt`（40/40）、`l2_audio_only_configure_after_fix.txt`（成功，沒有 SDL3 target）、`l2_no_tools_configure_after_fix.txt`（成功，沒有 framework 與 SDL3 target）。
   - 突變：`guard_mutations.txt`（第一次 15/17，兩條是突變定義的錯）、`mutations_rerun.txt`（修正定義後 2/2 與 1/1）、`link_mutations_after_review.txt`（審查後 8/8）。

既有自測 `test_allowlisted_core_sites_pass` 的夾具補上定義 `gyo_tool_framework` 的 CMakeLists：新規則下沒有定義的 target 本來就該報錯，預期（沒有任何發現）不變。審查者確認拿掉它時正好那一條失敗，所以不是放寬。

## 別的審查者的核對

Sonnet 審查者（檔位繼承，唯讀；實驗都在會話暫存區的 repo 副本與兩個新 configure 的樹裡做）：

- 1、4、5 確認；2 原本少列了 (c)；3 原本寫「收緊」是錯的，已改成「沒有」。5 的「3/3」已改成「2/2 與 1/1」。
- 0 blocker／major。minor 4：
  - 文字比對把註解、ALIAS、死分支、沒被引用的檔案裡的 `add_library` 當成定義：註解與 ALIAS 已修（自測與突變），死分支與沒被引用的檔案寫進 2(c)。
  - 文字比對把大寫 `ADD_LIBRARY`、`add_library(x)` 後接 `target_sources` 誤判成沒定義：已修。用變數當名稱、或名稱含 `-`、`.` 的 target 仍會被誤擋（安全側失敗，訊息會說「不是 framework 的 target」），framework 目前沒有這種寫法，寫成限界。
  - 過時的行要跑 ctest 才看得到：寫進 2(b)。
  - GUI target 消失只靠 ui_editor 自己的檢查：寫進 2(d)。
- 審查者獨立重跑突變 5 條（Python 的 4 條與 fw-guard-allowlist-stale）5/5；在副本裡手動套用 trade、genex、錯名稱 3 條，都讓 configure 失敗。
- 審查者的原始輸出在會話暫存區，未保存；結論在本文件與 HANDOFF。
