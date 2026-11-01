# framework 整改：交接

## 代號對照

| 代號 | 白話 |
|---|---|
| v8-framework-rework-plan | 定計畫 |
| v8-framework-guard-ratchet | 先把守衛的規則裝好，舊違規列進只減不增的清單 |
| v8-engine-platform-path-dialog | 選擇窗的 OS 半進 Engine |
| v8-engine-io-user-files | 使用者檔案的缺口補進 engine/io |
| v8-framework-shell | 產品外殼與 ImGui 綁定 |
| v8-framework-catalog-structure | catalog 登錄的結構與 JSON 文字分開 |
| v8-framework-invariants | 工具切換後收尾 |
| T1～T3b | 工具線（另一個會話）的切換工作，見 README |

## 現況

- 2026-11-01：v8-framework-rework-plan 完成（使用者核准 PLAN 第 1 版與 D-1～D-9，commit `f8878799`）。
- 2026-11-01：v8-framework-guard-ratchet 完成（使用者核准判定變更與 DI 條文，commit `0a635225`，draft PR #127）。L1：手動觸發的 cross-platform run 41227212440，`L1 / linux-x64`、`windows-x64`、`macos-arm64` 都是 success。
- 2026-11-01：v8-engine-platform-path-dialog 完成（使用者核准，commit `472225fe`）。L1：run 41231279740 的 `L1 / linux-x64`、`windows-x64`、`macos-arm64` 都是 success。
- 2026-11-01：v8-engine-io-user-files 完成（使用者核准[契約、行為變更與條文](io-user-files-judgment-change.md)，io.md 條文已寫入）。估 6～8 h，實際約 4.5 h。commit `2aaa6b52`。L1：run 41266291181 的 `L1 / linux-x64`、`windows-x64`、`macos-arm64` 都是 success。
- 2026-11-01：v8-framework-shell 開工（使用者：「開始外殼批」）。第 0 關：外殼草圖先交核准，依 D-9 用 ultracode（獨立提案 3、審查合成 1、Sonnet 對抗式檢查 1，唯讀，不改程式）。整批估 12～16 h，其中第 0 關估 2～3 h。草圖核准前不寫程式。
- 2026-11-01：第 0 關草圖完成，停在核准關卡（[外殼草圖](shell-sketch.md)，決策 D-shell-1～8）。workflow 5 個 Agent 全部回傳；審查合成以 A（PLAN 第 2.4 節補完）為骨架；Sonnet 對抗式檢查 0 blocker、2 major、9 minor，處理見草圖第 8 節。和審查者不同的一處：D-shell-2 我建議照今天的兩段式（沒有觀測到的壓力）。第 0 關實際約 1 h。
- 2026-11-01：**停止（AGENTS §14：發現回歸）。** v8-framework-shell 的實作與測試已寫好（工作樹，未提交），L2 的「只選 audio_editor」configure 失敗：configure 時的連結檢查（guard-ratchet 批 `0a635225` 加的）把例外清單裡 `target:gyo_tool_framework_gui` 那一行判成「不是 framework 的 target，刪掉這行」，但那個 target 只在有 SDL 平台、SDL renderer 與 ImGui 的組態才建。`0a635225` 的前一個 commit 在同一組態 configure 成功，所以是我在 guard-ratchet 批引入的回歸；只在本分支（PR #127 未合併），master 與其他會話不受影響。L1 沒有只選 audio_editor 的組態，所以沒抓到。證據：`l2_audio_only_configure.txt`（失敗）、`l2_audio_only_configure_before_guard.txt`（`0a635225~1` 成功）、`l2_no_tools_configure.txt`（`GYO_TOOLS` 為空，成功、沒有任何 framework 與 SDL3 target）。修法會改守衛的判定（§14.4），等使用者決定。外殼批至今約 2.5 h（第 0 關 1 h、實作與測試 1.5 h）。
- 2026-11-01：突變第一次執行（事前宣告的判定）：**25/26 killed，判定不成立**（§14.1，先回報、不重跑）。stale 0。未 killed 的是 fw-shell-binding-missing（wrong_failure）：手動重現（`binding_missing_diagnosis.txt`）確認觸發的正是預期的存在檢查，但 CMake 把訊息折成兩行（「…but no」換行「GYO::ToolFrameworkImGui」），預期的正規表示式「no GYO::ToolFrameworkImGui」跨不過換行。是突變定義（預期訊息）的錯，不是實作或測試漏檢。證據 `mutations_first_run.txt`。
- 2026-11-01，使用者：「照建議處理」。回歸修法（§14.3）：Red＝L2 只選 audio_editor 的 configure 失敗（已記錄）＋守衛自測新增 2 條（例外清單 link 行的 target 不在 framework 的 CMake 定義裡、或屬於綁定，都要報錯），修正前 2 條失敗（`regression_red_selftest.txt`）。修正：過時的 link 行改由 Python 守衛在每個組態檢查（target 要由核心的 CMake 以 add_library 定義）；configure 時的連結檢查只核對這次組態有建的 target。Green：自測 38/38（`regression_green_selftest.txt`；既有一條自測的夾具補上定義該 target 的 CMakeLists，預期不變）、只選 audio_editor 的 configure 成功且沒有 SDL3 target（`l2_audio_only_configure_after_fix.txt`）。
- 事前宣告（守衛突變，D184：守衛腳本、連結檢查與 framework/CMakeLists.txt 都改了）：`scripts/framework_mutations.json` 的 fw-guard-* 11 條、fw-link-* 6 條（含新增 fw-link-stale-line-ignored、fw-link-binding-line-allowed、fw-link-stale-real），共 17 條全部要 killed、stale 0。之後再以修正後的預期（`no\s+GYO::ToolFrameworkImGui`）只重跑 fw-shell-binding-missing，要 killed。
- 2026-11-01：守衛突變執行：**15/17 killed，判定不成立**（§14.1，先回報、不重跑；`guard_mutations.txt`）。fw-guard-input-target 是 stale：它錨定的 framework/CMakeLists.txt 註解被我在這批改寫了。fw-link-stale-line-ignored 是 wrong_failure：突變拿掉檢查後，下一個分支讀 `defined[target]`，自測以 KeyError 失敗而不是預期訊息（手動重現確認）。兩條都是突變定義的錯（錨點、替換內容），不是守衛漏檢。加上 fw-shell-binding-missing，這一批已有 3 條突變因定義錯誤而判定不成立。外殼批至今約 3.5 h。
- 2026-11-01，使用者：「照建議重跑」。三條突變的定義修正（判定的標準不變）：fw-shell-binding-missing 的預期改成 `but no\s+GYO::ToolFrameworkImGui`（允許 CMake 換行）；fw-guard-input-target 改錨到這批改寫後的註解；fw-link-stale-line-ignored 只把報錯那一行換成 `pass`，預期改成守住它的自測失敗（`FAIL: test_a_link_line_for_a_target_the_framework_does_not_define_fails`），因為 unittest 的差異輸出會把訊息拆行。預檢：兩個突變目錄的每條 find 都恰好出現一次；新的正規表示式對 CMake 折行後的訊息成立。今後每次宣告前都做這兩項預檢。
- 事前宣告（重跑）：只跑這三條，三條都要 killed、stale 0。前兩次不成立的結果保留在 `mutations_first_run.txt`、`guard_mutations.txt`。
- 重跑結果：fw-guard-input-target、fw-link-stale-line-ignored 2/2，fw-shell-binding-missing 1/1（`mutations_rerun.txt`）。合計外殼 26/26、守衛 17/17。
- Sonnet 審查者核對 §14.4 五項（[判定變更](link-check-judgment-change.md)）：0 blocker／major，4 minor。其中兩個是新規則的文字比對漏洞：註解或 ALIAS 裡的 `add_library` 被當成定義（舊判定會擋）；大寫 `ADD_LIBRARY`、`add_library(x)` 後接 `target_sources` 被誤判成沒定義。依 §14.3 修：自測新增 2 條（讀法、重複行），修正前 2 條失敗（`review_red_selftest.txt`），修正後 40/40（`review_green_selftest.txt`）。死分支與沒被引用的檔案在文字比對下仍算定義，寫成限界。
- 事前宣告（預檢：每條 find 恰好一次）：fw-link-* 8 條（既有 6 條＋fw-link-comment-counts、fw-link-duplicate-allowed）全部要 killed、stale 0。
- 結果：fw-link-* 8/8（`link_mutations_after_review.txt`）。
- 全量 CTest 第一次：118/119，`os.boundary_guard` 失敗：H3 用 `_WIN32` 只在 POSIX 編譯，OS 邊界守衛（R2）不允許 tests/framework 用 OS 巨集（`ctest_full.txt`）。我新寫的測試的錯，守衛不改。改成執行時判斷：Engine 的 `CommandLineUtf8` 接受這組 argv 時（Windows 讀程序自己的命令列），H3 直接返回，由 H1 涵蓋。H3 是 fw-shell-argv-raw、fw-shell-failure-code 的殺手，依 D184 宣告：這 2 條重跑要 killed、stale 0（預檢：find 都恰好一次）；全量 CTest 重跑要全過。
- 結果：2/2（`h3_mutations.txt`）；全量建置成功、CTest 119/119（45.9 s，`-j 6`，`ctest_full_2.txt`；比上一批多 4 條：window_shell、imgui_shell、imgui_shell_no_renderer、shell_headers）。突變合計：外殼 26/26、守衛與連結 19/19（fw-guard-* 11、fw-link-* 8），stale 0。停在連結檢查判定變更的核准關卡（[判定變更](link-check-judgment-change.md)）。外殼批至今約 4.5 h。
- 2026-11-01：v8-framework-shell 完成（使用者核准判定變更）。回歸修補與外殼批分兩個 commit。估 12～16 h（第 0 關後重估 14～17 h），實際約 5 h。commit `15639727`（回歸修補）、`ea5df6f6`（外殼）。L1：run 41274514145 的 `L1 / linux-x64`、`windows-x64`、`macos-arm64` 第一次就都是 success。同一 run 的 `Quick acceptance / object_fps_pvp / windows-x64` 第一次 failure：建置途中複製 MSVC 執行期 DLL（`concrt140.dll`）到 `build/target/object_fps_pvp/bin/` 時「Permission denied」，測試沒有跑到；這一步屬 pvp 的打包，不建 framework、不碰守衛，判定為環境（runner 上的檔案被佔用）。只重跑失敗的 job（attempt 2），全部 success，CI gate success。第一次的失敗保留在這裡（§14.1）。
- 2026-11-01：v8-framework-catalog-structure 開工（使用者：「開始下一批」）。D-1 已核准 (a)＋b-lite。估 8～12 h。
- 2026-11-01：v8-framework-catalog-structure 完成，實際約 2 h。commit `1bd5df20`。L1：run 41276711562 第一次就全部 success（`L1 / linux-x64`、`windows-x64`、`macos-arm64`、Quick acceptance、CI gate）。PLAN 第 2.5 節寫的「core 列」不在這個 workflow 的 L1 裡；Engine 側沒壞由三列的全量 CTest（含 `AssetContentContractTests`）證明。
- 2026-11-01：v8-framework-catalog-extra-fields 開工（使用者：「開始下一批」）。估 2～3 h。
- 2026-11-01：v8-framework-catalog-extra-fields 完成，實際約 1.5 h。PR A（D-7：規劃到 catalog-structure，含 D-6）的批次全部完成。commit `98231ca2`。L1：run 41278439662 第一次就全部 success（三列、Quick acceptance、CI gate）。
- 2026-11-01：PR A 合併前的完整突變（D184）在背景執行：本線的 5 個突變目錄（framework、framework_shell、framework_catalog、io_user_files、platform_path_dialog）。ASan 那 1 條要 sanitizer 樹，這次不跑，記為限界。結果：宣告 97、執行 96、killed 96、Windows 限定 1 條 skipped、stale 0（framework 27、framework_shell 26、framework_catalog 12、io_user_files 14、platform_path_dialog 17）。path dialog 目錄第一次我漏設目錄說明要求的 `SDL_VIDEODRIVER=dummy`，基準線就失敗、沒有執行突變；照說明重跑 17/17。證據 `build/evidence/framework-rework/pr-a-full-mutations/`。PR A 等使用者決定是否轉為 ready 並合併；合併後通知工具線。
- 2026-11-01：圖書管理員通知 gearbox 規則更新（PR #129，master `8a175c0a`）：敵對式檢查與 §14.4 的別的審查者要用和提案者不同、且不比提案者弱的引擎（提案者 Opus → Fable）；較弱的引擎只能當額外意見。已同步 master（merge `7710250e`），全量建置與 CTest 123/123、兩個守衛通過，推送後 L1 run 41280529299。這條線以 Sonnet 當唯一檢查的有：guard-ratchet 的 §14.4 審查者、path-dialog 的對抗式檢查、外殼草圖的對抗式檢查、連結檢查判定變更的 §14.4 審查者（計畫的對抗式檢查已執行完，不重做）。Fable 比主會話強，依 gearbox 要先問使用者；PR A 合併前是否以 Fable 重做，等使用者決定。
- 2026-11-01，使用者：「只從現在起套用」。上面四項檢查不重做，PR A 照現狀合併；之後的對抗式檢查與 §14.4 審查者依新規則（提案者 Opus → Fable，先問使用者）。
- 2026-11-01：PR A（yojinn-io/GYO-Engine#127）合併，merge commit `3954f7e7`（使用者指示由我合併；先前一次自動合併被權限檢查擋下）。已通知工具線（「v8 tool-line：工具線規劃 v8-tool-plan」）：合併 commit、新公開介面、T1～T3b 要改的地方。
- 下一步：等工具線 T1、T2、T3a、T3b 都合併，再開始 v8-framework-invariants（PR B）。整條線完成後通知架構會話。外殼的公開介面在 PR A 合併後通知工具線（T3a：ui_editor 改用 RunHeadlessShell／RunImGuiShell；T3b：audio_editor 改用 RunHeadlessShell，執行檔自己具名 GYO::ToolFramework）。

### v8-framework-catalog-extra-fields 的事前宣告（2026-11-01，執行測試前）

- 缺陷（D-6）：JSON 綁定的 `RegisterAsset` 更新既有條目時，把整個條目文字換成只有 id、type、path 的新物件，其他欄位（例如 `author`）消失。內容契約保留未知的選用欄位（cases.json `valid-custom-type-and-metadata`）。
- 範圍：只修 JSON 綁定（`framework/bindings/json/src/CatalogText.cpp`）。舊的 `CatalogRegistration` 有同樣的缺陷，但 audio_editor 在 T3b 前還用它，修它會改變工具的行為，不在這批；它在 v8-framework-invariants 刪除，audio_editor 在 T3b 換到新 API 時得到修正。
- 修法：更新時只改條目最上層的 `type` 與 `path` 兩個值的字串，條目的其他位元組不動；寫完的比對改成「預期的條目＝原條目只換 type 與 path」。
- Red：新增 E1（含巢狀同名鍵 `meta.path`、要跳脫的字元，整份文字逐位元組比對）、E2（鍵的順序不同、冒號前有空白、政策允許換 type）、E3（cases.json 的 metadata 案例更新後仍保留 `author`）。修正前三條都要失敗，存紀錄。
- Green：修正後 E1～E3 與既有的 C1～C7 全過；全量 CTest 全過。
- 突變（`scripts/framework_catalog_mutations.json` 追加，宣告前預檢）：fw-extra-whole-entry（更新時又換掉整個條目）、fw-extra-nested-key（掃描不看深度，改到巢狀的同名鍵）、fw-extra-expected-whole（比對的預期又是整個新條目）。全部要 killed、stale 0。
- 停止條件：超過 4.5 h，或單一問題累計超過 2 h。
- 2026-11-01：Red（`red.txt`）：E1～E3 修正前都失敗，E3 直接顯示 `author` 被丟掉。修正後（`green.txt`）E1～E3 通過，但**宣告的判定不成立**：上一批的 C3 失敗（§14.1，先回報、不改）。原因：C3 照抄舊測試，預期更新後整個條目被改寫成一行 `{ "id": …, "type": …, "path": … }`；修正後只換 path 的值，條目原本的兩行排版保留。C3 的預期是舊行為，不是契約。要改 C3 的預期＝既有測試的判定變更（§14.4），等使用者決定。至今約 1 h。
- 2026-11-01，使用者：「照建議改 C3」。C3 的預期改成「只換 path 的值，條目保留原本的排版」（§14.4：守住的性質＝其他條目的位元組不變、previousPath 正確、條目拿到新值；放寬面＝保留排版只換值的輸出以前算失敗；收緊面＝整個條目被改寫的輸出以前通過；整份文字仍逐位元組比對，額外欄位由 E1～E3 守住）。宣告不變：C1～C7、E1～E3 全過，突變 3 條全 killed，全量 CTest 全過。
- 結果（`green_after_c3.txt`）：C1～C7、E1～E3 全過。突變執行前補 E4（巢狀的同名成員排在條目自己的成員前面）：E1 的巢狀成員排在後面，拿掉深度判斷的突變會先找到最上層的鍵而殺不到。預檢：3 條的 find 都恰好一次；預期訊息是 doctest 的「is NOT correct」。宣告：3 條全 killed、stale 0；D184 另重跑 fw-cat-edit-other（錨在改動的更新分支附近）。

### v8-framework-catalog-structure 的事前宣告（2026-11-01，寫程式前）

- 介面（PLAN 第 2.5 節＋D-1）：
  - Engine（AD4，b-lite）：`Engine::Asset::IsRelativeContentPath(std::string_view)` 從 `ContentManifest.cpp` 的匿名命名空間移到 `ContentManifest.hpp` 公開，行為不變。
  - 核心（不帶 nlohmann，新標頭 `gyo/framework/CatalogPlan.hpp`）：條目型別用 Engine 的 `RawCatalogEntry`；`RegistrationPolicy { allowTypeChange, allowSharedPath }`（預設都拒絕）；`CheckCatalogEntries`（單一 catalog 內 id 唯一、路徑合 `IsRelativeContentPath` 且不是 content.json）；`PlanRegistration`（新增或更新、previousPath）。`CatalogUpdate` 從舊的 `CatalogRegistration.hpp` 移到這個標頭，舊標頭改為 include 它（定義不變，舊 API 行為不變）。
  - JSON 綁定 `GYO::ToolFrameworkJson`（`framework/bindings/json/`，隨核心建，自己 PRIVATE 連 nlohmann）：`ReadCatalog(text)`（v1 合法性交給 Engine 的 `CatalogParser::Parse`，再做 `CheckCatalogEntries`）、`RegisterAsset(text, entry, policy)`（保留位元組的改寫從舊碼複製；寫完重新解析並比對全文）。
  - 舊的 `CatalogRegistration`、它的測試與突變都不動。已知缺陷（更新條目時丟掉額外欄位）照現行行為搬，下一批修（D-6）。
- cases.json 對齊：以 `ContentManifest::Parse` 分類，只取「content 合法、只列 1 個 catalog、沒有 shader bundle」的案例。實作前重算：適用 34 條（合法 4、不合法 30）、排除 20 條；測試斷言這兩個件數，適用的每條 `ReadCatalog` 的結果要和 `valid` 一致。
- Red：先讓 `CheckCatalogEntries` 用舊 framework 規則（`IsValidAssetPath` 的複製）跑對齊測試，`asset-path-control` 要失敗（存紀錄）；換成 Engine 的 `IsRelativeContentPath` 後全過。
- 驗收：L1 三列綠；cases.json 子集 34 條全一致；新 API 對 `convert_taken_path` 情境（路徑已屬另一個 id）以預設政策拒絕；`gyo_audio_editor.cli` 照常通過（含 `convert_taken_path` 回 4）；`AssetContentContractTests` 照常通過；例外清單零新增行；configure 時確認 JSON 綁定有建。
- 突變（`scripts/framework_catalog_mutations.json`，宣告前預檢：find 恰好一次、預期訊息對實際輸出成立）：拿掉控制字元檢查（Engine）、拿掉 content.json 檢查、拿掉重複 id 檢查、不經 CatalogParser 判斷 version、改寫時動到其他條目、政策 allowTypeChange 被忽略、政策 allowSharedPath 被忽略、核心 include nlohmann（守衛）、JSON 綁定沒建（configure 失敗）。全部要 killed、stale 0。
- 範圍的限界：寫完後的全文比對在正確的改寫下殺不到（舊碼相同），不設突變；跨 catalog 重複與 shader 重疊不在這批（載入時由 ContentManifest 判定）。
- 停止條件：超過 18 h，或單一問題累計超過 2 h。
- 2026-11-01，突變執行前：Red（`red_old_path_rule.txt`：核心先用舊 framework 的路徑規則，C1 只有 `asset-path-control` 不一致、C6 的含換行路徑沒被擋；適用 34、排除 20、合法 4 與宣告相同）→ 換成 Engine 的 `IsRelativeContentPath` 後 Green（`green_engine_path_rule.txt`，C1～C7 全過）。突變目錄 9 條定稿（`scripts/framework_catalog_mutations.json`），預檢：find 都恰好一次；守衛那條的預期對守衛的實際輸出格式成立；JSON 綁定沒建那條的預期對 CMake 折行與不折行都成立。全部要 killed、stale 0。
- 結果：9/9 killed、stale 0（`mutations.txt`）。D184：所有突變目錄預檢都沒有 stale；錨在這批改過的兩個 CMakeLists 的既有突變，錨點都不在改動的行。殺 fw-shell-binding-missing 的存在檢查前面多了 JSON 綁定的檢查，宣告重跑這一條，要 killed。

### v8-framework-shell 的事前宣告（2026-11-01，草圖核准後、寫程式前）

- 驗收：L1 三列（ci-windows、ci-linux、ci-macos）綠；`framework_guard_allowlist.txt` 零新增行；`gyo_framework.dependency_guard`、selftest、configure 連結檢查通過；本機全量 CTest 全過。
- 測試（[草圖](shell-sketch.md)第 5 節）：headless H1～H4（`gyo_framework.core`）；window W1～W10（`gyo_framework.window_shell`，dummy 驅動；W9 在 OnEvent 裡注入本視窗的 live 重繪要求，Update 不得在 OnEvent 裡被呼叫；W10 執行前後 `SetLogHandler` 的值不變）；ImGui I1～I9（`gyo_framework.imgui_shell`，dummy＋software；I3 產品解構時 renderer 仍掛在視窗上且 ImGui context 已關；I8 `SDL_RENDER_DRIVER` 不存在時回 1、不呼叫工廠；I9 options 原樣傳到平台）；公開標頭中立的編譯測試；configure 時的存在檢查。
- 「有沒有真的畫」（I6，第 2 幀後的 BeginFrame 檢查字型貼圖為 OK）：寫好測試後、宣告突變前，先在 v1.92.5 確認正確實作下成立；成立才列入 fw-imgui-no-render，不成立就不列並在這裡寫明。
- 突變（`scripts/framework_shell_mutations.json`，執行前寫好每條的 file、find、預期）：fw-shell-argv-raw、fw-shell-failure-code、fw-shell-body-code、fw-shell-platform-failure、fw-shell-factory-null、fw-shell-exit-code、fw-shell-destroy-order、fw-shell-no-observer、fw-shell-live-frame、fw-shell-log-handler、fw-shell-core-render、fw-shell-core-native、fw-shell-core-symbol、fw-shell-binding-missing（預期 configure 失敗）、fw-imgui-ini、fw-imgui-log、fw-imgui-endframe、fw-imgui-no-event、fw-imgui-sdl-symbol、fw-imgui-init-false、fw-imgui-initialize-early、fw-imgui-product-order、fw-imgui-renderer-order、fw-imgui-renderer-fail、fw-imgui-options，加上條件式的 fw-imgui-no-render。全部要 killed、stale 0；fw-shell-argv-raw 在 macOS 由 H3 殺、在 Windows 由 H1 殺（本機只跑 macOS）。
- 已知限界（不設突變）：主控台 guard 在 POSIX 觀測不到；Clear／Present 拿掉的突變殺不到；live frame 與真實視窗、字型只由 T3a 的 L3 覆蓋；`GYO_TOOLS` 為空與只選 audio_editor 的兩個組態由 L2（本機各 configure 一次）確認，L1 沒有。
- 停止條件：整批超過 25 h，或單一問題累計超過 2 h。
- 2026-11-01，突變執行前：測試寫好後全過（core 3 條 H、window 9 條、ImGui 8 條＋I8 單獨 1 條、標頭 1 條）。I6 在正確實作下成立（第 2、3 幀 BeginFrame 時字型貼圖為 OK），依宣告列入 fw-imgui-no-render。突變目錄 26 條定稿（`scripts/framework_shell_mutations.json`）。window 與 ImGui 的突變經 `scripts/run_shell_test.py` 執行：它照 CTest 的設定給 SDL 的 dummy 驅動、software renderer 與工作目錄（執行器本身不能設環境變數）。fw-imgui-renderer-fail 的預期是「I8 失敗或崩潰」：沒起來的 client 被拿去跑時，在缺 renderer 的地方崩潰。

### v8-engine-io-user-files 的事前宣告（2026-11-01，執行前）

- 突變（`scripts/io_user_files_mutations.json`）9 條：io-within-any-name、io-within-no-resolve、io-within-case（只在 Windows 跑，macOS 上記為 skipped，不算入）、io-within-dot-dot、io-stamp-missing-is-error、io-stamp-hash、io-stamp-not-file、io-stamp-limit-ignored、io-atomic-no-parents。本機（macOS）要 8/8 killed、stale 0。
- 新舊對照：framework `IsPathWithin`／`ProbeFileStamp` 與 engine/io 的新函式在同一組情況（包含判定 14 條、符號連結 6 條、戳記 3 條）答案一致。只在本機有符號連結權限時跑連結那 6 條。
- 第一次執行（2026-11-01）：6/8 killed、1 skipped（Windows 限定）。存活 2 條：io-within-dot-dot（engine 測試沒有「連結後接 `..`」的情況，新舊對照測試裡有）、io-stamp-not-file（macOS 開目錄本來就回 InvalidPath，錯誤碼分辨不出）。處理：補強測試（engine 測試加 `dirlink/../inside`，戳記測試加錯誤訊息檢查），突變的定義與預期不變，重跑。
- 第二次執行：7/8 killed，io-stamp-not-file 仍存活。原因：engine/os 的 FileReader 在 POSIX 與 Windows 都把非一般檔案擋成 InvalidPath「not a regular file」（`engine/os/src/posix/Files.cpp:117`、`windows/Files.cpp:271,280`），ProbeFileStamp 的明確檢查是多餘的，這是等價突變。處理：刪掉多餘的檢查（依靠 engine/os 的契約），突變目錄移除這一條；測試保留。之後一般突變 7 條、Windows 限定 1 條。
- 觀察到的舊行為（照實移植，不在這批改）：根目錄以分隔符結尾時，除了根目錄本身，什麼都判成「不在裡面」。**（已被下面的 xhigh 審查推翻，見下。）**
- xhigh 審查（2026-11-01）：與 framework 版逐步等價，但照實移植把舊缺陷帶進 Engine：F1（新引入）建父目錄選項下磁碟根目錄的檔案寫不進去；F2 根目錄以分隔符結尾（含 ui_editor 預設的 `.`）時什麼都不在裡面，ui_editor 的匯出守衛失效；F3 根目錄是連結或大小寫與磁碟不同時同樣失效；F4 POSIX 上先詞法消去 `..` 再解析連結，與 OS 不一致。minor：F5 契約文字、F6 解析失敗時退回、F7 命名空間前綴、F8 開檔副作用、F9 訊息差異。
- 處理（§14.3）：以「OS 實際把檔案放在哪裡」（父目錄經 `weakly_canonical`，再接最後的名稱）為獨立的對照，先寫 Red 測試（`red_before_fix.txt`：12 條中 6 條失敗，涵蓋 F1～F4、F7），再實作修正版語意（候選路徑：父目錄交 OS 解析、連結先於 `..`、最後一段照寫；根目錄整條解析；去掉結尾空段；拒絕 `//?/` 前綴），轉 Green（`green_after_fix.txt`：12/12）。F1 修正；F8 補回一般檔案檢查。原「大小寫依 SamePathComponent」那條測試的預期是根據我的理解，改以 OS 對照。新舊對照測試改成以 OS 為準，和 framework 不同的 4 個情況逐條印出（`parity_after_fix.txt`）。修正版語意與 framework 不同，交使用者決定（判定變更文件）。
- 追加宣告（修正後）：突變 15 條（io-within-case 只在 Windows、io-within-lexical-dot-dot 只在 macOS／Linux），macOS 上 14 條要全部 killed、stale 0。
- 第四次執行（修正後）：13/14 killed、1 skipped（Windows 限定）、stale 0。存活 io-within-trailing-kept：ResolvedPath 本來就去掉結尾分隔符，所以去結尾空段那一步只在 OS 解析失敗、退回詞法比較時有作用。處理：補一條測試（根目錄的上層去掉執行權限，讓 OS 解析失敗，再給結尾帶分隔符的根目錄），突變的定義與預期不變，重跑這一條 1/1（`mutations_trailing_rerun.txt`）。合計宣告 15・執行 14・killed 14。
- 一般檔案檢查（F8）不設突變：xhigh 審查指出開具名管道或裝置有副作用，所以補回「先看狀態、不是一般檔案就不開檔」。但 engine/os 的 FileReader 也會擋成同一個錯誤，刪掉這個檢查在測試上分辨不出（第二次執行已證明是等價突變），能分辨的只有「有沒有開檔」，本機沒有可靠的觀測方法。理由寫在這裡，不另設突變。

### v8-engine-platform-path-dialog 的事前宣告（2026-11-01，執行前）

- 一般突變（`scripts/platform_path_dialog_mutations.json`），全部要 killed、stale 0：pd-drop-answer、pd-tag-lost、pd-order、pd-take-keeps、pd-cancel-as-failed、pd-assert-observer、pd-kind-map、pd-noexcept（預期建置失敗）、pd-g2-neutral。共 9 條。
- 相關的既有突變（D184：錨點被這批改到）：input-and-present 的 m6a-g1、m6a-g2，錨點改到 `#include <vector>`，替換內容不變。共 2 條，要 killed。
- sanitizer 突變（`scripts/platform_path_dialog_mutations-asan.json`，本機 AddressSanitizer 樹）：pd-filter-copy、pd-mailbox-not-shared，共 2 條，要 killed。基準線：ASan 樹上 platform_sdl_tests 的選擇窗案例全過、沒有 ASan 報告。
- 第一次執行（2026-11-01）：一般 9 條中 8 條 killed；pd-g2-neutral 是 build_failed，原因在突變定義（前置宣告寫在 Engine 的命名空間內，產生新型別遮住 SDL 的型別，Engine 本身編不過，沒走到 G2）。改成全域命名空間的前置宣告（可編譯，是真實的洩漏寫法），判定（G2 的訊息）不變，重跑這一條。m6a-g1、m6a-g2：2/2 killed。
- sanitizer 突變第一次執行（ASan 樹，基準線 8/8 通過、無 ASan 報告）：0/2 killed，兩條都是 wrong_failure，**宣告的判定不成立**。pd-filter-copy 被一般 CHECK 抓到（過濾器字串內容已錯，ASan 沒報），pd-mailbox-not-shared 是崩潰（鎖已釋放的 mutex，SIGABRT），沒有 ASan 報告。依 §14.1 不改預期重跑，交使用者決定。探索（不算驗收）：兩條在一般建置上各 5 次都被抓到（CHECK 失敗、SIGABRT），macOS 限定。
- xhigh 審查（2026-11-01）：blocker 1（SDL 3.4 在 Windows 對同一請求回呼兩次，舊碼會 use-after-free 與重複釋放；framework 現有實作同樣有此缺陷）、minor 4、nit 6。處理：待回覆請求改用程序級登記表與編號，第二次回呼找不到就返回；取答案配置失敗時放回；不合法種類與空 pattern 用 GYO_ASSERT；zenity 的空路徑算取消；launcher 型別加 noexcept；取答案只准主執行緒；Impl 先於視窗配置；拿掉觀察不到效果的關閉旗標。測試由 8 條增為 12 條，新增一條用 `SDL_HINT_FILE_DIALOG_DRIVER` 讓真正的 SDL launcher 在 CI 跑。
- 追加宣告（修正後、重跑前）：一般突變 13 條（原 9 條，pd-drop-answer、pd-take-keeps 錨點隨程式改；新增 pd-double-callback、pd-empty-path-chosen、pd-kind-assert、pd-real-launcher-save），全部要 killed、stale 0；m6a-g1、m6a-g2 重跑。
- 第三次執行（對抗式檢查修正後）：一般 16/16 killed，m6a 2/2 killed。
- 第二次執行（xhigh 修正後）：一般 13/13 killed，m6a 2/2 killed。
- Sonnet 對抗式檢查（2026-11-01）：major 6、minor 9。要使用者決定的：Windows 重複回呼的答案政策（第一次 Failed 之後是否交出第二個答案），以及 sanitizer 突變。其餘照做：真正 launcher 改經 `SdlDialogFunctions` 表（測試用假表檢查每種的參數，`ToSdlFileDialogType` 刪除）；答案政策先實作「Failed 之後最多再一個」（每個請求配兩個節點，回呼全程在登記表的鎖內）；補競爭、不去重、64 位元 tag、多檔取第一、無多餘文字、連續兩次 Failed 的測試（共 15 條，競爭測試連跑 30 次 0 失敗）；noexcept 加 static_assert；Linux 上不重設 hint。
- 追加宣告（第三次執行前）：一般突變 16 條（刪 pd-kind-map；pd-drop-answer、pd-order、pd-tag-lost、pd-double-callback、pd-real-launcher-save 錨點或預期隨程式改；新增 pd-third-callback、pd-launcher-many、pd-launcher-folder-window、pd-launcher-drop-filters），全部要 killed、stale 0；m6a-g1、m6a-g2 重跑。sanitizer 兩條等使用者決定，不跑。
- 2026-11-01 使用者核准後的宣告（sanitizer 選項 (1)）：pd-filter-copy 移到一般目錄，預期為 CHECK 失敗（`filters[0].name == "UI layout"`），在一般樹跑；pd-mailbox-not-shared 留在 ASan 目錄，預期為「ASan 的 heap-use-after-free、mutex lock failed 或崩潰」。ASan 樹先用目前的程式重建，基準線（選擇窗 15 條）要全過且沒有 ASan 報告，再跑這一條。兩條都要 killed。
- 使用者決定後的執行：pd-filter-copy 在一般樹 1/1 killed；ASan 樹以定稿程式重建，基準線 15 條全過、無 ASan 報告；pd-mailbox-not-shared 1/1 killed。第一次 ASan 突變的原始輸出被第二次覆蓋，未保存（結果記在本節）。
- 範圍的限界（帶到 T1 的事前宣告）：真實 OS 對話窗的回呼執行緒、Linux portal 需要 pump、平台或父視窗銷毀時的行為、真正的 SDL launcher（`ShowWithSdl`）在 CI 都不執行。

### v8-framework-guard-ratchet 的事前宣告（2026-11-01，執行前）

- 突變（D184：守衛腳本改了，所以跑全部 fw-guard-* 與新增的；fw-catalog-*、fw-path-dot-dot、fw-inbox-* 的檔案與測試都沒變，不跑）。全部要 killed，stale 為 0：
  fw-guard-input-target、fw-guard-tool-header、fw-guard-tool-name、fw-guard-executable（舊，判定不放寬的證據）；
  fw-guard-core-imgui-noslash、fw-guard-core-sdl-forward、fw-guard-core-native-include、fw-guard-core-sdl-call、
  fw-guard-core-json-link、fw-guard-allowlisted-file-one-more、fw-guard-allowlist-stale、fw-link-closure-sdl（新）。
  共 12 條。第一次執行 12/12 killed。
- 2026-11-01 追加宣告（獨立審查後、第二次執行前）：連結檢查補洞並改成點名，加 fw-link-trade、fw-link-genex，共 14 條，全部要 killed、stale 為 0。
  舊件檔改名為 `framework/Legacy.cmake`（核心 CMakeLists 的 nlohmann 也移進去），fw-guard-allowlisted-file-one-more、fw-guard-core-json-link、fw-link-closure-sdl 的錨點與預期訊息跟著改。
- 範圍的限界：綁定目錄還不存在，綁定規則只由自我測試涵蓋；綁定的突變在外殼批與 catalog 批補上。
- 舊自我測試對新守衛：只允許「乾淨的樹」一條失敗，且失敗內容只能是三項收緊。
- 工具線分支 `claude/v8-tool-audio-editor-edit` 只在本機，停在核准關卡等本計畫。

## 決策紀錄

- 2026-11-01，使用者：「照建議核准」。PLAN 第 1 版與 README 待決事項全部採建議：
  - D-1：catalog 規則採 (a)＋b-lite。framework 核心只做 catalog 層級，用 cases.json 子集對齊；Engine 的 `IsRelativeContentPath` 移到 engine/asset 的公開標頭，framework 直接呼叫（AD4，b-lite）。跨 catalog 與 shader 重疊仍由載入時的 ContentManifest 判定。
  - D-2：`AbsolutePath` 留在 framework 核心，與內容掛載放一起；ui_editor 用核心這一份。
  - D-3：選擇窗的跨執行緒信箱歸 Engine；工具留用途對照與每幀套用。這是原則 6 的釐清。
  - D-4：T1 與 T3a 一起做，不給舊宿主加橋接。
  - D-5：守衛採 ratchet，第一批裝好規則，例外清單逐條記次數、只減不增，收尾批清空。
  - D-6：更新條目時丟掉額外欄位的缺陷由本線修，在 catalog-structure 之後另開一小批，依 §14.3 先 Red 後 Green。
  - D-7：PR 兩條：A＝規劃到 catalog-structure（含 D-6 的小批），B＝收尾。
  - D-8：有視窗外殼目前只有 ui_editor 一個使用者，接受，DI 5 註明。
  - D-9：外殼草圖用 ultracode；選擇窗只做 xhigh 審查加別的引擎做一次對抗式檢查。
- 各批的 Design Invariants 條文與守衛變更仍在該批開工或定案前逐批核准。
- 2026-11-01，使用者：「核准」。[連結檢查的判定變更](link-check-judgment-change.md)（guard-ratchet 批引入的回歸的修補）：過時的例外行改由 Python 守衛在每個組態擋，configure 時只核對這次組態有建的 target；放寬面 (a)～(d) 照文件。
- 2026-11-01，使用者：「照建議處理」「照建議重跑」：回歸照參考修法修；三條定義錯的突變修正定義後只重跑那三條，宣告前加預檢。
- 2026-11-01，使用者：「照建議核准」。v8-framework-shell 的[外殼草圖](shell-sketch.md)：D-shell-1 外殼採 A（反轉式，外殼擁有 RuntimeLoop）；D-shell-2 採 (b)（ImGui 綁定照今天的兩段式：先工廠、ImGui 建好後 Initialize、ImGui 先關再銷毀產品）；D-shell-3 arguments 含程式名；D-shell-4 綁定目錄以列舉加入；D-shell-5 本批寫入 framework DI 6 與新 Replacement Criterion；D-shell-6 外殼不 catch；D-shell-7 platform-sdl Replacement Criterion 2 照草圖改寫；D-shell-8 最小化節流與 HiDPI 不放進綁定。草圖定稿，通知 pvp v8 owner。
- 2026-11-01，使用者：「照建議核准」。v8-engine-io-user-files 的[契約、行為變更與條文](io-user-files-judgment-change.md)：路徑包含判定採修正版 (a)（OS 解析、和 framework 舊版不同的 4 個情況由 T2 接手）；framework 的 `IsPathWithin` 不在這批修（T2 合併前 ui_editor 只給 `--asset-catalog` 時仍可匯出到資產根目錄）；AD2（engine/io 新增使用者路徑輔助、engine_tests 連結 `GYO::Os`）；io.md 的「User files」與 Replacement Criteria 3 補句。Design Invariants 不變。
- 2026-11-01，使用者：「照建議核准」。v8-engine-platform-path-dialog 的 [契約與 DI 條文](path-dialog-judgment-change.md)：Windows 重複回呼採 (a)「Failed 之後最多再一個答案」；sanitizer 突變採 (1)（pd-filter-copy 移一般目錄，pd-mailbox-not-shared 改預期在 ASan 樹重跑）；AD1（選擇窗能力與程序級登記表）；測試縫放 Native 標頭；platform-sdl.md 的契約、DI 10、DI 4／7 補句、Replacement Criteria 7～9。Replacement Criteria 2 的改寫延到外殼批。
- 2026-11-01，使用者：「核准」。v8-framework-guard-ratchet 的 [判定變更與 DI 條文](guard-ratchet-judgment-change.md)：framework DI 2、DI 3、DI 5、Replacement Criteria 改寫；守衛、例外清單、configure 連結檢查；突變目錄搬到本計畫。

## 研究事實（2026-11-01 核對，基準 `6e78090b`）

- SDL 版本 3.4.0（`third_party/sdl3/CMakeLists.txt:10`）。`SDL_dialog.h`：回呼可能在別的執行緒（:125-126、:154-156）；Linux 可能要 portal，不用 SDL 處理事件的程式要在主迴圈呼叫 `SDL_PumpEvents`（:131-133）；filter 要活到回呼（:145-146）；只能從主執行緒呼叫（:154-156）。文件沒有寫「在呼叫返回前觸發」，`framework/src/SdlPathDialogs.cpp:50-51` 的註解是推測。`engine/render/shaders/pipeline/cmake/Dependencies.cmake:17-19`：Cocoa 對話窗依賴視訊。
- framework 每一塊目前只有一個使用者；兩個工具共用的只有錯誤型別。這與 framework DI 5 衝突。
- audio_editor 完全沒用 `FileService`，自己用 `Engine::Os::Files` 讀寫（`tools/audio_editor/src/SoundExport.cpp:19,38-69`）；它的守衛不允許 `engine/io/` 與 `GYO::Engine`（`tests/audio_editor/audio_editor_dependency_guard.py:26-27`）。
- `FileService` 用 `std::filesystem::path`（`FileService.hpp:24-45`），`AbsolutePath`／`IsPathWithin` 做 `lexically_normal`（`FileService.cpp:180-211`），與 io.md DI 2、DI 3 不合，不能照搬進 engine/io。
- cases.json 已有 54 個案例，含跨 catalog 重複、shader 重疊、控制字元；framework 沒有測試讀它。
- 突變目錄 `docs/tools/plans/v8/scripts/framework_mutations.json` 的 14 條全部會受影響；執行器在 find 對不上時記 stale 並讓整次回 1（`docs/architecture/plans/time-threads-trace/scripts/run_mutations.py:110-112,134`）。
- core preset 的 `GYO_TOOLS` 為空，不建 framework；L1 只有 ci 三列會建 framework 與它的測試。L1 沒有「只選 audio_editor」或「不選工具」的組態。
- CI 沒有 sanitizer（`.github/workflows/` 的四份工作流程、`CMakePresets.json`、`build/cmake/` 都沒有 sanitize）。
- `platform_sdl_tests` 用 dummy 視訊驅動（`tests/common/platform_sdl/CMakeLists.txt:18-22`）；`ClockReaders` 縫在 Native 標頭（`SdlPlatformNative.hpp:29-36`）。

- PR #125（asset-layout 線）新增 CTest `object_fps_pvp.asset_catalog` 的 `test_no_other_owner_names_a_pvp_asset_path`：掃 framework/、engine/、tools/、tests/framework 等共通層，出現 `assets/object_fps_pvp` 或 PvP catalog 的相對路徑就失敗（PvP 素材標準 DI-6，`docs/object_fps_pvp/assets.zh-Hant.md`）。本線的測試資料只用合成資料或 cases.json，不寫 PvP 素材路徑。

證據：使用者的 Mac，主 checkout `build/evidence/framework-rework/`（見該資料夾的 README）。
