# 工具 v8：v8-tool-ui-editor-path-dialog-adopt＋v8-tool-ui-editor-shell-adopt（T1＋T3a）

日期：2026-11-01。分支 `claude/v8-tool-framework-rework`（已併 master `3954f7e7`＝PR #127 與 `d4a79c4a`＝PR #130）。

- 檔位與模型：実装者 Opus 5.5／high（主會話）、審査者 Opus 5.5／high、検証者 Fable 5.1／high。這批屬上段，因為它改 Design Invariants 與守衛，並接新的公開介面。
- 估時 5～7 h。從事前宣告核准到結果寫完約 20 分鐘（17:14～17:31，牆上時間），不含兩個子 agent 的等待時間（各約 3 分鐘）。

**狀態：** 程式、測試、突變、審查、檢驗都完成；守衛的放寬已核准。**L3 實機還沒做**，做完後在本檔補上結果。

相關文件：[事前宣告與結果](../tools/plans/v8/assets.md)「framework-rework-line 的事前宣告」、[架構整改計畫](../architecture/plans/framework-rework/PLAN.md) §2.2 與 §2.4、[platform-sdl.md](../architecture/platform-sdl.md)「Path dialogs」與 Design Invariant 10、[ui_toolchain.md](../ui_toolchain.md) ui_editor 的 Design Invariants。

## 做了什麼

- **選擇窗**改由 Engine 平台層開啟：
  - 呼叫 `SdlPlatform::ShowPathDialog` 與 `TakePathDialogAnswers`；
  - ui_editor 只保留兩件事：用途對應到哪種請求與過濾器（`PathDialogRequestFor`），以及每幀套用答案。
- **答案不來時的恢復規則**（`PathDialogRequests`）：
  - 每次 Show 給一個新的 tag，只接受最新請求的答案；
  - Chosen、Cancelled 結束請求；Failed 不結束，所以 Windows 改開舊式對話窗後回來的答案仍會套用；
  - 「Stop waiting」放棄等待；之後才到的答案丟掉，並記在狀態列與 log 的 `ui_editor.dialogs`。
- **外殼**：`main` 用 `RunHeadlessShell`（命令列錯誤時印 help 並回 2），GUI 用 `RunImGuiShell`；預覽用 `NativeRenderer(context.renderer)` 建貼圖。
- **CMake 與守衛**：拿掉 `GYO::ToolFrameworkGui`，加入 `GYO::ToolFrameworkImGui`、`GYO::PlatformSDL`、`GYO::RenderBackendSDLNative`（§14.4，使用者核准）。
- **Design Invariants**：ui_editor 的 DI 1、2 改寫（事前宣告核准的方向）。

## 驗證與經過

- 本機 macOS（`build/target/_build/framework`，全部工具）：ctest 67／67。
- 件數の突き合わせ：一致。宣告 10 條、執行 10 條、killed 10 條、stale 0（使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T1-T3a/`）。
  - 突變 `ui-cli-usage-exit` 只能改成手動跑，期待不變。原因有兩個：執行器只替換整個參數剛好是 `{exe:…}` 的寫法，而 `gyo_ui_editor` 的輸出不在建置目錄裡。
  - 在找出這個原因之前，我改了兩次 CLI 測試腳本：把 WORK 與 EDITOR 轉成絕對路徑。兩次都沒解決，已經還原。
- 審査者沒有發現嚴重問題，指出兩件，都已修：
  - 介面卡沒有測試：改成可測的自由函式並補測試，另加 3 條突變；
  - 被丟掉的答案在狀態列常被蓋掉：等待時也顯示訊息，並另寫一行 log。
- 検証者：
  - 第 1、2、3、5 項成立。
  - 第 4 項部分成立：G3 只檢查 Engine 的後端，不涵蓋 ui_editor 自己的目標。已照實更正（紀錄在 使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T1-T3a/verifier/run.log`）。
- 和事前宣告不同的地方：
  - 被丟掉的答案記在狀態列與 log，不是 Diagnostics。Diagnostics 每次由文件重建，記上去會消失。L3 第 3 項的寫法因此改了，經使用者同意。
  - `SdlPathDialogs::Abandon` 沒有自動測試，由 L3 第 3 項確認。

## 範圍的限界

以下由 L3 實機觀察（Windows 優先）；Linux 這次不測：

- 真實 OS 對話窗的回呼執行緒；
- Linux portal 要 pump；
- 平台或父視窗銷毀時的行為。
