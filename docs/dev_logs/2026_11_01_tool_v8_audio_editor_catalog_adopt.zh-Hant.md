# 工具 v8：v8-tool-audio-editor-catalog-adopt（T3b）

日期：2026-11-01。分支 `claude/v8-tool-framework-rework`。

- 檔位與模型：実装者 Opus 5.5／high、審査者 Opus 5.5／high、検証者 Fable 5.1／high。這批屬上段，因為它改 Design Invariants 與守衛。
- 估時 2～3 h。從開工到結果寫完約 8 分鐘（17:58～18:06），不含子 agent 的等待時間。

**狀態：** 完成；守衛的放寬已核准。

相關文件：[事前宣告與結果](../tools/plans/v8/assets.md)「T3b」、[架構整改計畫](../architecture/plans/framework-rework/PLAN.md) §2.5、[audio_editor 的 Design Invariants](../audio_editor/architecture.md)。

## 做了什麼

- **登錄 catalog**：`SoundExport` 改用 JSON 綁定的 `RegisterAsset`；條目型別是 `RawCatalogEntry`，登錄政策用預設值。
- **`main`**：改用 `RunHeadlessShell`。執行檔具名 `GYO::ToolFramework`，不再需要 `GYO::Os`。
- **守衛**：加入 `GYO::ToolFrameworkJson`（§14.4，使用者核准）。
- **Design Invariants**：DI 3 的「unique ids」改成「catalog 內唯一；跨 catalog 的重複與和 shader 的重疊，在載入時由 ContentManifest 判定」。

## 驗證與經過

- 本機 macOS：ctest 67／67。
- `convert_taken_path` 仍回 4。
- 更新既有條目時，保留它的其他欄位（D-6）：新測試比對整份 catalog 文字。
- 輸出與基準相同：5／5 的 WAV 與 catalog（使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T3b/bytes/compare.txt`）。
- 件數の突き合わせ：一致。執行 5 條、killed 5 條、stale 0。
  - 執行器跑 3 條，手動 2 條；手動的原因同 T1＋T3a（使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T3b/`）。
  - 另外確認：新加的兩個測試換回舊 API 時會失敗。
- 審査者沒有發現嚴重問題。它指出沒列進事前宣告、變嚴格的行為：
  - 已有條目的路徑不是內容路徑時，登錄回 4；
  - `--path` 含控制字元時回 4；
  - 同一個鍵寫兩次的條目無法更新。
  - 前兩項和 Engine 載入時的規則一致；第一項補了測試固定。
- 検証者：第 1～5 項都成立，並在編譯層確認 audio_editor 拿不到 nlohmann 的標頭目錄（使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T3b/verifier/run.log`）。
