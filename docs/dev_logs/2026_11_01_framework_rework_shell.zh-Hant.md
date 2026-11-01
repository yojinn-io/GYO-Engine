# framework 整改：v8-framework-shell（產品外殼與 ImGui 綁定）

日期：2026-11-01。分支 `claude/framework-rework`。第 0 關草圖 ultracode（獨立提案 3、審查合成 1，引擎與檔位繼承；Sonnet 對抗式檢查 1）；實作 high；守衛判定變更由 Sonnet 審查者核對。主會話引擎 Opus 5.5。估 12～16 h（第 0 關後重估 14～17 h），實際約 5 h。

**狀態：** 完成（Mac 上的測試、突變、全量 CTest、L2 兩次 configure）。Windows、Linux 由 CI 的 L1 驗證。真實視窗與字型由工具線 T3a 的 L3 確認。

相關文件：[外殼草圖](../architecture/plans/framework-rework/shell-sketch.md)、[連結檢查的判定變更](../architecture/plans/framework-rework/link-check-judgment-change.md)、[framework 架構](../framework/architecture.md) Design Invariant 6、[platform-sdl.md](../architecture/platform-sdl.md) Replacement Criterion 2。

## 做了什麼

- **第 0 關草圖**（使用者核准 D-shell-1～8）：三個提案（A：PLAN 第 2.4 節補完；B：SDL3 main callbacks；C：程式庫式組合），審查合成以 A 為骨架。D-shell-2 我和審查者不同：對抗式檢查指出「拿掉 Initialize、反轉銷毀順序」沒有觀測到的壓力，採 (b) 照今天的兩段式。草圖定稿後通知 pvp v8 owner（「[Task]v8 規劃:玩法優化分批與檔位」），附上 pvp 採用前要先改的項目。
- **無視窗外殼** `RunHeadlessShell`（核心 `GYO::ToolFramework`）：主控台輸出 UTF-8、命令列經 Engine 的 `CommandLineUtf8`（程式名在第一個）交給 body、原樣回傳結束碼。三份相同的開場程式（ui_editor、audio_editor、match）由它收掉。
- **有視窗外殼** `RunWindowShell`（核心 `GYO::ToolFrameworkWindow`，只連中立的 `GYO::PlatformSDL`）：建平台、建 client、跑 Engine 的 RuntimeLoop；先銷毀 client 再銷毀平台。不建渲染裝置、不設 live frame、不換 log handler、不 catch 例外。
- **ImGui 綁定** `RunImGuiShell`（`framework/bindings/imgui/`，`GYO::ToolFrameworkImGui`）：從舊宿主搬字型、後端與幀順序；log 改走 Base::Log（`framework.imgui`）；公開標頭只有中立型別，ImGui、SDL 與 `*Native` 都是 PRIVATE。
- **CMake**：核心以列舉加入綁定目錄（D-shell-4），綁定自己決定要不要建；`tests/framework` 在 configure 時檢查該建的外殼都建了（對抗式檢查 major 2）。例外清單零新增行。舊的 `ImGuiSdlHost` 與 `GYO::ToolFrameworkGui` 不動。
- **文件**（使用者核准）：framework DI 6（外殼生命週期）與一條 Replacement Criterion、組成清單與 Using it；platform-sdl Replacement Criterion 2 改寫；`docs/architecture.md` 的 framework 列。
- **測試**：headless H1～H3、window W1～W10（含不設 live frame、不換 log handler）、ImGui I1～I9（含不寫 imgui.ini 與 ImGui log、銷毀順序、事件送達、字型貼圖確實上傳、renderer 起不來回 1）、公開標頭中立的編譯測試。新增 CTest 4 條。
- **突變目錄**：`scripts/framework_shell_mutations.json`（26 條）；window 與 ImGui 的突變經新的 `scripts/run_shell_test.py` 執行，它照 CTest 的設定給 SDL 的 dummy 驅動、software renderer 與工作目錄。

## 回歸（guard-ratchet 批引入，本批修補）

- L2 的「只選 audio_editor」configure 失敗：guard-ratchet 批（`0a635225`）的 configure 時連結檢查，要求例外清單的每一行都是這次組態有建的 target；舊 GUI target 只在有 SDL 與 ImGui 的組態才建。`0a635225~1` 在同一組態成功。只在本分支，L1 沒有這個組態。依 AGENTS §14 停下回報。
- 修補（使用者核准判定變更）：過時的例外行改由 Python 守衛在每個組態擋（target 要由核心的 CMake 以 `add_library` 定義），configure 時只核對這次組態有建的 target。放寬面 (a)～(d) 與性質由誰守住寫在判定變更文件。
- Sonnet 審查者：0 blocker／major，4 minor。我原本寫「有收緊」是錯的（新規則擋的都是舊判定已擋的子集），已更正。文字比對的兩個漏洞（註解、ALIAS 算定義；大寫 `ADD_LIBRARY` 與不帶來源的 `add_library` 被誤判）依 §14.3 修掉；死分支與沒被引用的檔案寫成限界。
- 這段 dev_log 是 guard-ratchet 批的後續；那一批的 dev_log 不改寫。

## 驗證與經緯

件數の突き合わせ：一致（外殼：宣告 26・執行 26・killed 26；守衛與連結：宣告 19・執行 19・killed 19；stale 0）。下列兩次判定不成立都已在 §14.1 先回報，記錄保留。

- 外殼突變第一次 25/26：fw-shell-binding-missing 觸發的正是預期的存在檢查，但 CMake 把訊息折成兩行，預期的正規表示式跨不過。
- 守衛突變第一次 15/17：fw-guard-input-target 的錨點是這批改寫的註解（stale）；fw-link-stale-line-ignored 的替換留下讀 `defined[target]` 的分支，自測以 KeyError 失敗。
- 三條都是突變定義的錯，使用者核准修正定義後只重跑：2/2 與 1/1。之後每次宣告前預檢：每條 find 恰好出現一次、預期的正規表示式對實際（含折行）訊息成立。
- 審查後的連結突變 8/8；H3 改寫後的 2 條 2/2。
- 全量 CTest 第一次 118/119：我寫的 H3 用了 `_WIN32`，OS 邊界守衛（R2）不允許 tests/framework 用 OS 巨集。守衛不改，H3 改成依 Engine 的行為在執行時判斷。
- L2：`GYO_TOOLS` 為空（成功，沒有 framework 與 SDL3 target）、只選 audio_editor（修正後成功，只有 framework 核心，沒有 SDL3 target）。L1 沒有這兩個組態。
- 範圍的限界：live frame 與真實視窗、字型只由 T3a 的 L3 覆蓋；主控台 guard 在 POSIX 觀測不到；拿掉 Clear／Present 的突變殺不到；Windows 上 H3 不執行（由 H1 涵蓋）。

## 全量回歸

`build/target/_build/test`（test preset，GYO_TOOLS=AUTO）全量建置成功；CTest 119/119 通過（45.9 s，`-j 6`）。證據 `full_build_2.txt`、`ctest_full_2.txt`。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-rework/v8-framework-shell/`（git 不追蹤，見該資料夾的 README）。第 0 關 workflow 的輸出與 journal 已保存；Sonnet 判定變更審查者的原始輸出在會話暫存區，未保存。
