# framework 整改：v8-engine-platform-path-dialog（選擇窗的 OS 半進 Engine）

日期：2026-11-01。分支 `claude/framework-rework`。檔位 high；回呼與生命週期一段 xhigh 唯讀審查（引擎繼承）；對抗式檢查 Sonnet（檔位繼承）。主會話引擎 Opus 5.5。估 12～16 h，實際約 6 h。

**狀態：** 完成（Mac 上的測試、突變、ASan、全量 CTest）。Windows、Linux 由 CI 的 L1 驗證。真實 OS 對話窗由工具線 T1 的 L3 實機確認。

相關文件：[契約與 DI 條文](../architecture/plans/framework-rework/path-dialog-judgment-change.md)、[platform-sdl.md](../architecture/platform-sdl.md)「Path dialogs」與 Design Invariant 10、[PLAN 第 2.2 節](../architecture/plans/framework-rework/PLAN.md)。

## 做了什麼

- **Engine 平台層的選擇窗**（AD1，使用者核准）：`SdlPlatform::ShowPathDialog`（operation）與 `TakePathDialogAnswers`（主執行緒），中立型別 `PathDialogKind`／`PathDialogFilter`／`PathDialogRequest`／`PathDialogOutcome`／`PathDialogAnswer`，形狀比照剪貼簿。中立標頭沒有 SDL 符號。
- **實作**：SDL 拿到編號，請求放在程序級、不解構的登記表；每個請求預先配兩個答案節點；回呼 noexcept、全程在登記表的鎖內、不碰平台、不斷言；信箱由平台與請求共用。Impl 改在建立視窗前配置（例外時不洩漏 SDL 資源）。
- **測試縫**（Native 標頭）：`PathDialogLauncher`、`ShowPathDialogWith`、`SdlDialogFunctions`、`ShowWithSdlFunctions`。
- **platform-sdl.md**（使用者核准）：契約「Path dialogs」、DI 10、DI 4 與 DI 7 補句、Replacement Criteria 7～9、Process services 與 Ownership。Replacement Criteria 2 的改寫延到外殼批。
- **突變目錄**：`docs/architecture/plans/framework-rework/scripts/platform_path_dialog_mutations.json`（17 條）與 `-asan.json`（1 條）。input-and-present 的 m6a-g1、m6a-g2 錨點因 `#include <vector>` 改到新位置，替換內容不變。
- framework 的 `SdlPathDialogs`、`PathDialog` 不動，收尾批刪除。

## 驗證與經緯

件數の突き合わせ：一致（一般：宣告 17・執行 17・killed 17、stale 0；ASan：宣告 1・執行 1・killed 1；既有 m6a：2・2・2）。第一次 ASan 宣告（2 條）不成立，見下。

- 選擇窗測試 15 條通過；競爭測試連跑 30 次 0 失敗；platform_sdl 三個 CTest 通過；全量 CTest 結果見下一節。
- 突變的經緯：
  - 第一次 9 條 8/9：pd-g2-neutral 的突變寫錯（命名空間內的前置宣告遮住 SDL 型別，編不過），改成全域前置宣告後 killed（測試側的錯，判定沒變）。
  - xhigh 審查後 13 條 13/13；對抗式檢查後 16 條 16/16；使用者決定後 pd-filter-copy 移入一般目錄 1/1。
  - ASan：第一次宣告 2 條「以 ASan 訊息判定」，0/2（wrong_failure），§14.1 先回報使用者。pd-filter-copy 被一般 CHECK 抓到，pd-mailbox-not-shared 是鎖已釋放的 mutex 而崩潰。使用者選 (1) 後，以定稿程式重建 ASan 樹，基準線 15 條無 ASan 報告，pd-mailbox-not-shared 1/1。
- xhigh 審查：blocker 1——SDL 3.4 在 Windows 上，新式對話窗啟動失敗會先回呼 Failed，再改開舊式對話窗回呼第二次（`src/dialog/windows/SDL_windowsdialog.c:692-695,774`），一次刪除請求的寫法會 use-after-free 與重複釋放。framework 現有的實作有同樣缺陷，ui_editor 目前在 Windows 上有此風險（T1 切換後消失）。另 minor 4（取答案配置失敗遺失、不合法種類、zenity 空路徑、平台銷毀時 SDL 仍用視窗）、nit 6，都已處理或寫成限界。
- Sonnet 對抗式檢查：major 6——答案政策（使用者選 (a)：Failed 之後最多再一個答案，保留 Windows 舊式對話窗的選擇）、真正 launcher 的參數對應沒有測試（改經函式表）、sanitizer 判定、併發沒有守衛、不去重沒有守衛、Replacement Criteria 與 AD1 的登記表沒寫；minor 9。
- 範圍的限界（帶到 T1 的事前宣告）：真實 OS 對話窗的執行緒與行為、Linux portal 需要 pump、平台或父視窗銷毀時 SDL 對視窗的使用、資料競爭（CI 沒有 sanitizer）、記憶體不足的路徑。

## 全量回歸

`build/target/_build/test`（test preset，GYO_TOOLS=AUTO）全量建置成功；CTest 115/115 通過（101.8 s，`-j 6`）。證據 `full_build.txt`、`ctest_full.txt`。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-rework/v8-engine-platform-path-dialog/`（git 不追蹤，見該資料夾的 README）。第一次 ASan 突變的原始輸出被覆蓋，未保存；兩道審查的原始輸出與探針在會話暫存區，未保存。
