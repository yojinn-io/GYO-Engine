# 工具 v8：v8-tool-audio-editor-convert（音效工具：轉換與登錄）

日期：2026-10-31。分支 `claude/v8-tool-audio-editor`（切自 `claude/v8-tool-handoff-cleanup` 的 `3182ac53`，該分支切自 master `16326f78`）。檔位 high（重新取樣局部 xhigh），引擎 Opus 5.5。估 6～10 h；從核准規劃到測試全過約 50 分鐘（18:05～18:55，牆上時間，含 Design Invariants 的關卡與等待回覆）。沒有用 xhigh 審查：重新取樣是移植，以腳本的參考向量和突變檢查驗證。

**狀態：** 完成（Mac 上的測試與突變）。Windows、Linux 由 CI 的 L1 驗證。

相關文件：[規劃](../tools/plans/v8/assets.md)、[Design Invariants](../audio_editor/architecture.md)、`tools/audio_editor/README.md`、音效轉換腳本 `docs/architecture/plans/audio/scripts/convert_wav.py`。

## 做了什麼

音效從「腳本」走到「工具」（AGENTS §5）：工具重現腳本確立的程序，並把結果登錄到產品的 catalog。這一批只有核心程式庫與 CLI；視窗（播放、波形、裁剪、增益）是 `v8-tool-audio-editor-edit`，要等 GYO::Audio 增強（音效線）與 `v8-tool-framework-shell`。

- **新的工具 `audio_editor`**：`tools/audio_editor/`，`engine/config/tools.csv` 一列（enabled、default、release=false，三個平台），`project.json` 只有 `cli` 變體（packageable=false）。`test` preset 的 `AUTO` 會選它，所以 CI 會跑它的測試。
- **`WavFile`**：讀 RIFF/WAVE（8／16／24／32 位元整數、32 位元浮點、1 或 2 聲道、任意取樣率），取樣的縮放、chunk 的走法和腳本相同；寫 44 位元組標頭的 PCM16 WAV。
- **`Conversion`**：
  - `Resample`：腳本的窗化 sinc（Blackman 窗、兩側各 32 個零交越、截止在較低 Nyquist 的 0.95），逐行移植。
  - `Quantize`：`值 × 32767 + 三角抖動`，四捨五入（半數進位）後飽和並計數。抖動用 `std::mt19937_64`（標準規定了輸出序列），取上 53 位元換成 [0, 1)，不用實作定義的 `uniform_real_distribution`，所以和平台無關。種子是資產 ID 的 FNV-1a。
  - `ConvertToClip`：取樣率取自 `Engine::Audio::kSampleRate`（Design Invariant 2，工具不自留數字）。
  - `Conversion.cpp` 在非 MSVC 編譯器上加 `-ffp-contract=off`，不讓編譯器合併乘加。
- **`CatalogRegistration`**：在 catalog v1 的文字上只新增或改寫同一個 ID 的 `sound` 條目，其他位元組（含空行分組、其他條目的排版）不變。新條目接在最後一個條目後面，沿用它的縮排。改完後重新解析，確認「只有那一個條目變了」，否則報錯。拒絕：ID 已用別的型別登錄、路徑登錄在別的 ID 下、不合內容契約的路徑（絕對路徑、`.`、`..`、空段、反斜線、`:`、`content.json`）、不是 catalog v1 的文字。
- **`SoundExport`**：先檢查登錄，再讀原檔、轉換；片段與 catalog 都以「寫到旁邊的暫存檔、再 `RenameReplacing`」寫入，先片段後 catalog。輸出路徑解析後等於原檔或 catalog 時拒絕（Design Invariant 4）。原檔壞掉時什麼都不寫。條目的路徑改變時，回報舊檔不再登錄，不刪除它（Design Invariant 3：不碰別的檔案）。
- **CLI**：`gyo_audio_editor info <wav>`、`gyo_audio_editor convert <wav> --asset-root … --asset-catalog … --id … --path …`；結束碼 0／2（用法）／3（讀寫）／4（WAV、catalog、登錄無效）。
- **Design Invariants**：[docs/audio_editor/architecture.md](../audio_editor/architecture.md)（使用者核准，第 2 條照使用者的意見放寬為跟著 Engine 公開的格式）。

## 驗收（事前宣告：assets.md「v8-tool-audio-editor-convert 的事前宣告」）

| 判定 | 結果 | 測試 |
|---|---|---|
| 重新取樣和腳本的 `resample()` 誤差 ≤ 1e-9（5 種合成輸入，含不需重新取樣的 48 kHz） | 通過 | `Resampling matches convert_wav.py's resample() within 1e-9` |
| 同輸入同平台位元組相同 | 通過 | `Quantizing is reproducible…`、`ExportSound writes the clip…`（同一請求兩次） |
| 抖動在 ±1 LSB 內 | 通過 | 輸入 0：輸出只在 -1～1；輸入 0.5 LSB：輸出只有 0 或 1（抖動若超過 ±1 會出現 -1 或 2） |
| 飽和計數等於被截斷的樣本數 | 通過 | `Quantizing counts and clamps saturated samples` |
| 輸出由 `SoundLoader` 載入、`ClipBank` 接受 | 通過 | `A converted clip loads through SoundLoader and ClipBank` |

- 參考向量：`tests/audio_editor/fixtures/make_reference.py` 用腳本自己的 `read_wav`、`resample` 產生（輸入 WAV 與 `resample.json` 一起提交）。
- 事前宣告的抖動判定原文寫成「抖動前後的差 ≤ 1 LSB（三角抖動加四捨五入）」，數學上不對：抖動 (-1, 1) 再加四捨五入的 0.5，合計可到 1.5。照使用者選項的意思（「±1 LSB 幅度」）改成直接檢查抖動本身（0.5 LSB 的輸入），另外加一條「合計 < 1.5 LSB」。判定沒有放寬，是在看到結果之前改正文字（§14.4：締める面のみ）。
- 四個現有音效的原檔已找不到，沒有重產現有資產。

## 測試與突變

- `gyo_audio_editor.core`（doctest，11 個 case）、`gyo_audio_editor.cli`（CMake 腳本：help、未知指令、info、缺參數、轉換並檢查檔案與條目、被占用的路徑、壞的 WAV）、`gyo_audio_editor.dependency_guard`（Design Invariant 1 的允許清單）與它的自我測試。
- `audio` 建置樹（`GYO_APPS=`、`GYO_TOOLS=audio_editor`、`BUILD_TESTING=ON`）的完整 ctest：52／52 通過（含 `cmake.ProjectComposition`、`build.ci` 等工具登錄相關的測試）。
- 突變：`docs/tools/plans/v8/scripts/audio_editor_mutations.json`，用既有的執行器 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --catalog …` 跑，17／17 被抓到（濾波器的截止、窗、取樣點、24 位元的符號、無抖動、抖動過寬、種子被忽略、捨入、飽和不計、取樣率取自原檔、不重新取樣、被占用的路徑、別的型別、`..`、縮排、覆寫原檔、依賴守衛）。

- CI 的 L1：windows-x64 兩次編譯失敗（`9d1e6774` 與合入 master 後的 `9f407514`），linux-x64、macos-arm64 通過。原因：`CatalogRegistration.cpp` 拿 json 直接和 `std::string_view` 比較，MSVC 判為多載歧義（C2666），Clang／GCC 不會。改成先取出字串（`get_ref<const std::string&>()`，`ParseCatalog` 已檢查過是字串）再比較；突變 `ae-catalog-other-type` 的 `find` 跟著改，重跑仍被抓到。第一次失敗當時沒有回報，到使用者說「CI炸了」才處理。

## 已知的限制與之後

- 工具內部的 catalog 寫入屬於「整合」，`v8-tool-framework-shell` 時搬進 `tools/framework/`（`v8-tool-framework-principle`）。
- 讀 48 kHz PCM16 再轉一次會乘上 32767／32768（腳本也是：讀取除以 32768、量化乘以 32767）。照腳本的程序保留。
- 視窗、工程檔（Design Invariant 5）、裁剪與增益在 `-edit`。
