# framework 整改：v8-framework-guard-ratchet（先把守衛的規則裝好）

日期：2026-11-01。分支 `claude/framework-rework`（基準 master `6e78090b`）。檔位 high，引擎 Opus 5.5；獨立審查 Sonnet（檔位繼承）。估 7～9 h，實際約 3.5 h。

**狀態：** 完成（Mac 上的守衛、自我測試、突變、工具測試）。Windows、Linux 由 CI 的 L1 驗證。

相關文件：[判定變更與 DI 條文](../architecture/plans/framework-rework/guard-ratchet-judgment-change.md)、[framework Design Invariants](../framework/architecture.md)、[PLAN 第 2.1 節](../architecture/plans/framework-rework/PLAN.md)。

## 做了什麼

- **framework DI 2、DI 3、DI 5、Replacement Criteria 改寫**（使用者核准）：核心是產品外殼，不含 SDL、ImGui、nlohmann、Native；綁定是 `framework/bindings/<name>/` 的獨立 target，各有正向清單；登錄政策由呼叫者的選項決定。DI 4 與 Using it 留給收尾批。
- **守衛**（`tests/framework/framework_dependency_guard.py`）：核心與綁定分開判定。補上三個洞：沒有斜線的 include（`<imgui.h>`）、SDL 型別的前置宣告、經 `engine/` 前綴放行的 Native 標頭。舊守衛會擋的東西一律是不能進例外清單的硬規則。符號規則只看程式碼（去掉註解、字串與 include 行）。
- **例外清單**（`tests/framework/framework_guard_allowlist.txt`）：照 `tests/common/os/os_boundary_guard.py` 的「path rule count」，次數必須相等，只減不增；link 行點名禁止的 target。
- **舊件搬出主檔**：舊的 gui target 與核心的 nlohmann 連結搬到 `framework/Legacy.cmake`，`framework/CMakeLists.txt` 零例外。建置結果不變。
- **configure 時的連結檢查**（`tests/framework/CMakeLists.txt`）：核心經直接連結（含產生器運算式）或介面閉包碰到 SDL3、ImGui、nlohmann、Native 或綁定就失敗；JSON 綁定、ImGui 綁定各有規則；沒有規則的綁定失敗。
- **突變目錄**：fw-* 從 `docs/tools/plans/v8/scripts/framework_mutations.json` 搬到 `docs/architecture/plans/framework-rework/scripts/framework_mutations.json`。工具的兩條（ui-guard-native-target、ae-guard-json-target）留在原處，屬工具線。fw-guard-tool-header、fw-guard-tool-name 的錨點改到會留下的 `ReadOnlyAssetCatalog.cpp`，預期訊息不變。

## 驗證與經緯

件數の突き合わせ：一致（宣告 14・執行 14・killed 14、stale 0）

- 舊自我測試（HEAD 版 12 條）對新守衛：11 條通過，只有 `test_clean_tree_passes` 失敗，內容正好是三項收緊（核心 include `SDL3/SDL.h`、`backends/imgui_impl_sdl3.h`，CMake 連 `imgui::core`）。這條在新自我測試裡改寫成「核心乾淨、綁定乾淨」與「SDL 與 ImGui 進核心要失敗」兩條（§14.4 的收緊，已核准）。
- 新自我測試 36 條通過。`gyo_framework.*`、`gyo_ui_editor.*`、`gyo_audio_editor.*` 15/15 通過。
- 第一次跑工具測試時有 3 條失敗或沒跑：只建了兩個測試 target，工具執行檔不存在（環境）。全部建好後 15/15。
- 自己找到一個放寬面：只記次數的清單可以把已列的違規換成舊守衛就擋的東西。改成舊判定一律為硬規則。
- 獨立審查（Sonnet）：新舊守衛差分測試約 2 萬棵合成樹，舊拒絕而新接受的只有三個新 target 名稱（放寬面成立）。連結檢查找出 7 類洞：產生器運算式、核心連綁定、未被使用的 INTERFACE 程式庫、直接寫的 LINK_ONLY、非 GLOBAL 的 IMPORTED、SDL3_image／`-lSDL3`／裸名、次數互換。補洞後重跑審查者的 23 個案例，22 個如預期；IMPORTED 那一個寫成限界。
- 補洞時發現連結檢查的迴圈遇到沒有介面的 target 會提早結束（CMake 把結尾是 `-NOTFOUND` 的佇列當成假）。第一次真實 configure 能通過是碰巧，已修正。
- 突變第一次 12 條 12/12 killed；補洞後追加宣告 2 條（fw-link-trade、fw-link-genex），第二次 14/14 killed。
- 限界：綁定目錄還不存在，綁定規則只由自我測試涵蓋，突變在外殼批與 catalog 批補上。連結檢查只在 tests/framework 被 configure 時存在。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-rework/v8-framework-guard-ratchet/`（git 不追蹤，見該資料夾的 README）：舊自我測試與結果、新自我測試結果、突變結果與件數對帳、工具測試結果、補洞後的連結檢查案例結果。審查者的差分測試腳本與原始輸出在會話暫存區，未保存。
