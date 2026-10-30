# 工具 v8：v8-tool-ui-editor-file-browser（SDL3 系統選擇窗）

日期：2026-10-30。分支 `claude/v8-tool-line-3a892c`。檔位 high，引擎 Opus 5.5。估 3～5 h。

**狀態：** 完成（macOS 上 CI 等級的測試通過）；Windows／Linux 的實際選擇窗沒有在本機看過。

相關文件：[工具線 README](../tools/plans/v8/README.md)、[HANDOFF](../tools/plans/v8/HANDOFF.md)（`v8-tool-ui-editor-mount-ux`）、[ui_toolchain.md](../ui_toolchain.md)。

## 做了什麼

- `PathDialog.hpp`／`.cpp`（core，不依賴 SDL）：選擇窗的用途、結果、執行緒安全的收件匣 `PathDialogInbox`、host 介面 `PathDialogs`。
- `SdlPathDialogs`（GUI host）：用 `SDL_ShowOpenFileDialog`、`SDL_ShowSaveFileDialog`、`SDL_ShowOpenFolderDialog`。SDL 可能在別的執行緒回呼，所以結果先放進收件匣；收件匣以 `shared_ptr` 讓尚未回呼的窗也安全。
- `EditorApp`：新選單「Open Asset Folder...」（選資料夾就掛載，參考 AssetPack 的「データフォルダを開く」）；Open（Ctrl+O）、Export（Ctrl+Shift+S、沒有輸出路徑時的 Save）改用系統窗；掛載對話框的欄位加 Browse。結果在幀之間套用（`ApplyPathDialogResults`，在 `ApplyPendingAssetChanges` 之前）。一次只開一個窗。平台跳不出窗時顯示原因並退回原本手打的對話框。沒有 host（測試、CLI）時行為不變。
- 不記住任何路徑（`io.IniFilename = nullptr` 的約定不變）；檔案的讀寫仍經 engine/io、engine/os。

## 驗證

- 建置：`cmake --preset dev -DGYO_APPS= -DGYO_TOOLS=ui_editor -DBUILD_TESTING=ON`（macOS x86_64）。
- `ctest -R "gyo_ui_editor|os.header_hygiene|os.boundary_guard"`：9/9 通過。新增：`TestPathDialogInbox`（core，跨執行緒）、`TestPathDialogChoosesAssetFolder`（preview：取消不掛載、選資料夾掛載、填欄位不掛載）。
- 手動突變 3 個，全部被抓到：拿掉取消的判斷、選資料夾後不掛載、填 Asset Root 欄位也掛載。
- 範圍的限度：系統選擇窗本身與回呼的執行緒只能實機看；CI 測的是「收到結果之後」。
