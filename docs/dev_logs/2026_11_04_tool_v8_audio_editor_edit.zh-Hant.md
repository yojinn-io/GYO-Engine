# 工具 v8：v8-tool-audio-editor-edit

日期：2026-11-04。分支 `claude/v8-tool-audio-editor-edit`，從 master `b9f79cb7` 開出，開工前合入 master `8a713b1e`（GYO::Audio 播放控制層，PR #139），合併提交 `a9bbe1fb`。

- 段：上段（Design Invariants 的守衛放寬；新的資料格式：工程檔）。
- 実装者 Opus 5.5／high（主會話）；審査者 Opus 5.5／high；検証者 Fable 5.1／high（守衛的放寬，§14.4）。
- 估時 8～12 h。

**狀態：**
- 程式、測試、突變、審查都完成。
- 守衛的放寬（§14.4）等你核准。
- GUI 的試用（判定 8，L3 macOS）等你做。

相關文件：
- [事前宣告與結果](../tools/plans/v8/assets.md)「`v8-tool-audio-editor-edit` 的事前宣告」與「實作後的結果」；
- [audio_editor 的架構](../audio_editor/architecture.md)：Design Invariants、「Editing and preview」、「Project file」；
- [GYO::Audio 的契約](../architecture/audio.md)。

## 做了什麼

- **一個轉換函式（`Edit.hpp`）：**
  - 設定是裁剪（原檔的幀）與增益（dB，-60～+24）；
  - `RenderClip` 依序做裁剪、增益、重新取樣、量化，試聽與匯出都用它，抖動的種子都取自 id；
  - 預設設定的結果和舊的 `ConvertToClip` 相同；
  - 裁剪後一幀都不剩（重新取樣前或後）時拒絕，回 `InvalidSettings`。
- **匯出**：`ExportRequest` 多了 `settings`；`ExportSound` 改呼叫 `RenderClip`。
- **試聽（`PreviewPlayer.hpp`）：**
  - 在 GYO::Audio 的混音器上播，一次一個片段，增益固定為 1；預覽音量是混音器的 master gain；
  - 換片時先停舊的聲音並卸載舊片段，再載入新的；槽位還沒還回來（`NoClipSlot`）時新片段等待，`Update` 每幀重試；
  - 混音器設定 `PreviewMixerConfig`：2 個 voice、2 個片段槽、沒有排程延遲；
  - 播放位置只拿來顯示（GYO::Audio DI 4），播放器不保存位置，也不依位置做任何決定。
- **波形（`Waveform.hpp`）**：算出每一欄的最小值與最大值。
- **工程檔（`ProjectFile.hpp`）：**
  - 工具自己的 JSON：`format`、`version` 1、原檔、資產根目錄、catalog、id、path、裁剪、增益；
  - 欄位必須一個不多、一個不少；版本不認識時拒絕；存進自己的資產根目錄裡時拒絕；
  - 讀寫經 engine/io（`FileAccess`，和匯出共用，從 `SoundExport.cpp` 移出）。
- **視窗（`AudioEditorWindow.cpp`）：**
  - framework 的 `RunImGuiShell`；`SdlAudioOutput` 由工具自己開，開不了時仍可編輯與匯出；
  - 系統選擇窗：只認最新一次請求；Failed 不結束等待；Stop waiting 之後收到的回答丟掉；
  - 面板：原檔資訊、波形（左鍵設裁剪起點、右鍵設終點、增益反映在波形上、超出滿格標紅）、裁剪與增益、試聽（Play／Stop／Space、音量）、匯出欄位；
  - 指令 `gyo_audio_editor edit [<project.json> | <source.wav>]`。
- **建置**：`project.json` 加 `gui` 變體並設為預設，元件為 SDL_RENDERER、SDL_INPUT、SDL_AUDIO、IMGUI；`audio_editor:cli` 只建命令列。
- **守衛（放寬）**：目標加 `GYO::ToolFrameworkImGui`、`GYO::AudioBackendSDL`、`imgui::core`、`nlohmann_json::nlohmann_json`，include 前綴加 `nlohmann/`；自我測試加兩條。

## 驗證與經過

- 本機 macOS：
  - `build/target/_build/framework`（ui_editor＋audio_editor）：ctest 69／69；
  - 只選 audio_editor 的 `gui` 與 `audio_editor:cli` 兩棵新樹：設定與建置都通過，audio_editor 的 4 個測試都通過；cli 樹編的是 `AudioEditorGuiUnavailable.cpp`，gui 樹編的是 `AudioEditorWindow.cpp`。
  - 兩棵新樹和框架樹共用輸出目錄 `build/target/_tools`，cli 樹的執行檔蓋掉了框架樹的。驗證後刪掉兩棵樹，並重建框架樹的執行檔（GUI 版）。
- 判定 1（回歸）：五個 WAV 以 `golden.<檔名>` 轉換，SHA-256 和基準 5／5 相同。
- 判定 2～7：`tests/audio_editor/AudioEditorEditTests.cpp`，核心測試共 19 個案例。
- GUI 冒煙：`SDL_AUDIO_DRIVER=dummy` 下開啟 WAV，3 秒內沒有錯誤 log，字型照常載入。
- 件數の突き合わせ：一致。宣告 26 條，最後一輪 26／26 killed，stale 0。
- 經過：
  1. 寫判定 7 的測試時，抓到試聽播放器的缺陷：每幀的 `Update` 都會停掉正在播的聲音。這是在提交前、開發中找到的，不是已提交程式的回歸。修正後列為突變 `ae-preview-update-stops`。
  2. 審査者（Opus 5.5／high）指出 4 個缺陷，都已修：
     - 開工程檔時，設定不合原檔的警告被蓋掉；
     - 原檔打不開時存工程檔，會把裁剪與增益覆寫成欄位裡的舊值；
     - 96 kHz 等高取樣率的原檔裁到不足一個輸出幀時，會匯出空片段；
     - 突變 `ae-waveform-columns-shifted` 的期待寫錯：原本的波形測試資料下，邊界差一個樣本，結果也一樣。
     另外補強了兩個測試：波形改用遞增資料；換片測試改成每幀換兩次，讓第二個片段一定要等槽位。
  3. 突變第一輪 22／26：
     - 3 條試聽突變：測試名稱有逗號，doctest 的 `-tc` 在逗號切開，一個測試都沒跑。這是測試方法的錯，改了名稱。
     - 1 條：`ValidateSettings` 沒有被直接測到，是測試的缺口，補了。
     - `ae-preview-update-stops`：補了在播放中呼叫 `Update` 的檢查。
     第二輪 26／26。
  4. 検証者（Fable 5.1／high）核對守衛放寬的 §14.4 第 1～5 點：
     - 第 1、3、5 點：確認；
     - 第 2、4 點：確認，附更正，已照更正改寫進 assets.md（`nlohmann/` 前綴的範圍；ImGui 綁定的私有連結只靠審查；SDL3 以連結的形式進到 GUI 執行檔）。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-audio-editor-edit/`：
  - `regression/`（`compare.txt`、`sha256.txt`）；
  - `mutations-run1.txt`、`mutations.txt`；
  - `ctest-framework.txt`、`ctest-ae-gui.txt`、`ctest-ae-cli.txt`。
  - 審査者與検証者的報告沒有存檔；結論已寫在本文件與 assets.md。

## 和事前宣告不同的地方

- 裁剪後一幀都不剩時拒絕。舊的 `convert` 遇到 0 幀的 WAV 會寫出空片段，現在改回 4。
- 試聽的混音器沒有排程延遲（`PreviewMixerConfig`）。
- 新增指令 `edit`；不帶指令時照舊回 2。

## 範圍的限界與之後

- GUI 的邏輯（欄位與工程檔之間的轉換、對話框）沒有自動測試，只靠 L3。
- 重新取樣在 UI 執行緒上做，長的非 48 kHz 素材按下 Play 會停頓一下。
- 沒有「未存檔」的提示。
- 命令列給相對路徑時，工程檔照原樣存相對路徑。
- 觀察到的壓力：系統選擇窗的請求規則，ui_editor 與 audio_editor 各寫了一份，可以考慮移進 framework。
- DI 1 的措辭：「no SDL target」要不要改成「no SDL3 target」，等你決定（改 DI 本身是 Architecture Delta）。
