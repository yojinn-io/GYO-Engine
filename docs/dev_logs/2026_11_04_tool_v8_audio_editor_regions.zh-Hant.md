# 工具 v8：v8-tool-audio-editor-regions

日期：2026-11-04。分支 `claude/v8-tool-audio-editor-edit`（PR #141，接在 `-edit` 之後，同一條功能線），從 `3fb15495` 開工。

- 段：上段（Design Invariant 3 的措辭；工程檔的格式變更）。
- 実装者 Opus 5.5／high（主會話）；審査者 Opus 5.5／high；検証者 Fable 5.1／high（DI 3 的 §14.4）。
- 估時 8～12 h。

**狀態：**
- 程式、測試、突變、審查、DI 3 的檢驗都完成。
- DI 3 的新措辭（§14.4）等你核准；核准前 `docs/audio_editor/architecture.md` 的 DI 3 不改。
- GUI 的試用（判定 7，L3 macOS）等你做。

相關文件：
- [事前宣告與結果](../tools/plans/v8/assets.md)「`v8-tool-audio-editor-regions` 的事前宣告」；
- [架構評估](../tools/plans/v8/anim-event-sound-request.md)「評估結果」；
- [audio_editor 的架構](../audio_editor/architecture.md)：「Editing and preview」的 Regions、「Project file」。

## 做了什麼

- **淡入淡出（`Edit.hpp`）：**
  - `EditSettings` 加 `fadeIn`、`fadeOut`，單位是原檔的幀；
  - `RenderPlanes` 的順序：裁剪 → 增益 → 淡入淡出 → 重新取樣；直線，第一幀（淡入）與最後一幀（淡出）是 0；
  - 淡入加淡出超過裁剪的長度時拒絕（`InvalidSettings`）。
- **多段匯出（`ExportSounds`）：**
  - 一個原檔、一個目的地、多個區段（`SoundRegion`：id、path、設定）；
  - 先檢查全部、算好全部，再逐段寫片段，最後寫一次 catalog；
  - 拒絕的情形：沒有區段、id 重複、兩個 path 是同一個檔案、path 是原檔或 catalog、catalog 或設定不接受某一段；
  - 是不是同一個檔案，照 OS 層的 `Files::SamePathComponent` 逐段比對（Windows 不分大小寫）；
  - 寫到一半失敗時，錯誤訊息列出已寫、但沒有登錄的片段；
  - 一段讓出的 path 被同一次匯出的另一段登錄時，不報告「已沒有登錄」；
  - `ExportSound`（單段）改成呼叫它。
- **工程檔 version 2**：頂層是 `format`、`version`、`source`、`asset_root`、`asset_catalog`、`regions`；每段剛好 7 個欄位。version 1 讀成一段、沒有淡入淡出；存檔一律寫 version 2；`regions` 是空的時拒絕。
- **命令列**：`gyo_audio_editor export <project.json>`。
- **視窗**：
  - 區段列表（新增、選取、刪除）；欄位編輯選取的那段；
  - 波形上框出其他區段，畫出淡入淡出；
  - 試聽播選取的那段；「Export all and register」；
  - 開 WAV 時保留全部區段，不合的區段點名；讀不了的 WAV 不改變現在的原檔。
- **測試**：輔助函式移到 `tests/audio_editor/AudioEditorTestSupport.hpp`，`AudioEditorEditTests.cpp` 與新的 `AudioEditorRegionsTests.cpp` 共用；命令列測試加 `export`。

## 驗證與經過

- 本機 macOS，框架樹 `build/target/_build/framework`：ctest 69／69，核心測試 27 個案例。
- 判定 1（回歸）：五個 WAV 以 `golden.<檔名>` 轉換，SHA-256 和基準 5／5 相同。
  - 判定 1 的單元測試（version 1 讀成一段）比較的兩邊都經過新的 `ExportSounds`，不是獨立的證據。
- 判定 2～6：`tests/audio_editor/AudioEditorRegionsTests.cpp`；命令列測試加 `export`。
- 件數の突き合わせ：一致。第二輪宣告 18 條、18 條 killed；第三輪 3 條、3 條 killed；stale 0。
- 經過：
  1. 寫測試時發現參考 WAV 只有 300 幀，把測試的區段縮進範圍內。另一個是測試自己的錯：用參考接住暫存 vector 的元素，造成懸空參考，已改成複製。兩個都是測試的錯，不是實作的缺陷。
  2. 突變第一輪 16 條，15 killed。存活的 `ae-gui-preview-first-region`（視窗固定播第一段）和事前預測的一樣：視窗的邏輯沒有自動測試。這條從目錄拿掉，留下的證據在 `mutations-run1.txt`。
  3. 審査者（Opus 5.5／high）指出 3 個缺陷、4 個測試缺口、幾個小問題；検証者（Fable 5.1／high）用 24 個合成的情形跑 `export`，第 3 點附更正（大小寫、測試註解）。缺陷與缺口都已修、補，細節在 assets.md「實作後的結果（2026-11-04）」：
     - 大小寫不同的 path（Windows）；
     - 被別段登錄的 path 仍報告「已沒有登錄」；
     - 開 WAV 會清掉區段。
  4. 修正後新增 3 條突變，`ae-export-duplicate-file` 改了位置。第二輪 18 條，18 killed。
  5. 完整的 ctest 第一次失敗了 1 個：`os.boundary_guard`。測試裡的 `#ifdef _WIN32` 違反 OS 層的規則（只有 OS 層可以分平台），是測試的錯。改成問 `Files::SamePathComponent`，測試多連 `GYO::Os`，69／69。判定 4 的測試因此又改了一次，重跑被它殺的 3 條突變，3 killed（`mutations-run3.txt`）。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-audio-editor-regions/`：
  - `regression/`（`compare.txt`、`sha256.txt`）；
  - `mutations-run1.txt`、`mutations-run2.txt`、`mutations-run3.txt`、`mutations.txt`（第二輪的複本）；
  - `ctest-framework.txt`；
  - `review.diff`（交給審査者的 diff）。
  - 審査者與検証者的報告沒有存檔；結論寫在本文件與 assets.md。

## 和事前宣告不同的地方

- 突變：宣告 12 條，目錄留 14 條：
  - 11 條宣告的；
  - `ae-gui-preview-first-region` 存活後拿掉，只記錄；
  - 修正後加了 3 條。
- 開 WAV 時保留區段：`-edit` 的行為是重設裁剪，現在不重設。
- 兩個 path 是不是同一個檔案，照 OS 層的名稱比對；宣告時只寫了「path 不重複」。
- 寫到一半失敗時，錯誤訊息列出沒登錄的片段。
- `regions` 是空的工程檔，存檔時也拒絕；宣告時只寫了讀取時拒絕。
- DI 3 的新措辭多了兩句：收緊的部分、寫到一半失敗的例外（検証者與審査者的建議）。

## 範圍的限界與之後

- macOS 預設的磁碟不分大小寫，但 OS 層在 macOS 上逐位元組比對（os-subsystem D56），所以只差大小寫的兩個 path 在 macOS 上仍會寫到同一個檔案。
- 寫到一半失敗時，已寫的片段沒有登錄（錯誤訊息會列出）。不回滾，因為刪除可能連帶刪掉被覆蓋的舊版本。
- 同一次匯出裡，一段把 path 讓給另一段時，讓出的那段要排在前面，否則會被 catalog 的規則拒絕。
- 視窗的邏輯（區段的選取、新增、刪除、欄位與工程檔之間）沒有自動測試，只靠判定 7。
- 重新取樣仍在 UI 執行緒上做；每段都從原檔重新算。
- `ae-export-duplicate-file` 只在能建目錄連結的機器上被殺。
- 發現（不是本批造成的）：單段的 `convert` 在不分大小寫的磁碟上，會蓋掉另一個條目已登錄、只差大小寫的檔案（DI 3）。要和 catalog 裡其他條目的檔案比對，屬於登錄規則，可以另排一批。
