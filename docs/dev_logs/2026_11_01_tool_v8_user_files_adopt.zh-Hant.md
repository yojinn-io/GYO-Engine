# 工具 v8：v8-tool-user-files-adopt（T2）

日期：2026-11-01。分支 `claude/v8-tool-framework-rework`。

- 檔位與模型：実装者 Opus 5.5／high、審査者 Opus 5.5／high、検証者 Fable 5.1／high。這批屬上段，因為它改守衛並修缺陷。
- 估時 2～3 h。從開工到結果寫完約 13 分鐘（17:38～17:51），不含子 agent 的等待時間。

**狀態：** 完成；守衛的放寬已核准。

相關文件：[事前宣告與結果](../tools/plans/v8/assets.md)「T2」、[io.md](../architecture/io.md)「User files」、[架構整改計畫](../architecture/plans/framework-rework/PLAN.md) §2.3。

## 做了什麼

- **ui_editor 的 `DocumentSession`**：讀檔、戳記、存在檢查、原子寫入、包含判定都改走 engine/io。
  - 讀檔上限 64 MB；
  - 寫入時會建出缺少的父目錄；
  - `AbsolutePath` 照舊用 framework 核心的（D-2）。
- **audio_editor**：來源檔與 catalog 的讀取、片段與 catalog 的寫入、`info` 的讀取都改走 engine/io。
  - 上限 64 MB；暫存檔名換成 engine/io 的；
  - CMake 具名 `GYO::Engine`；
  - 守衛加入 `engine/io/` 與 `GYO::Engine`（§14.4，使用者核准）。
- **兩個工具的測試**：準備測試檔改用標準函式庫，不再用 `FileService`。

## 驗證與經過

- 本機 macOS：ctest 67／67。
- §14.3 缺陷：根目錄以分隔符結尾時，匯出到它底下應被拒絕。測試在舊程式上失敗（Red），換成 engine/io 後通過（Green）；紀錄在 使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T2/red.txt` 與 `green.txt`。
- audio_editor 的輸出與改動前的基準相同：5／5 的 WAV 與 catalog（使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T2/bytes/compare.txt`）。
- 件數の突き合わせ：一致。執行 12 條、killed 12 條、stale 0（使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T2/mutations.txt`）。
- os-subsystem 計畫的突變 `ui-x1`、`ui-x2` 現在存活（0／2，`T2/mutations-os-ui-x.txt`）。
  - 原因：它們改的是 `FileService::IsPathWithin`，ui_editor 已不再呼叫。
  - 同樣的性質改由 ui_editor 的連結測試與 engine/io 的 `io-within-no-resolve` 守。
  - 這兩條要在 PR B 退役或改錨。
- 審査者沒有發現嚴重問題。它指出的「和事前宣告不同的地方」記在 assets.md：
  - 包含判定實際改變答案的是 3 種情況，不是 4 種；
  - 64 MB 上限會讓「覆寫」失敗；
  - 帶命名空間前綴的根目錄會讓所有匯出失敗；
  - audio_editor 的錯誤訊息改了。
- 検証者：第 1～5 項都成立（使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T2/verifier/run_log.txt`）。
