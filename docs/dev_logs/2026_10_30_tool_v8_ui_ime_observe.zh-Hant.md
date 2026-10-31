# 工具 v8：v8-tool-ui-ime-observe（輸入法檢測）

日期：2026-10-30。分支 `claude/v8-tool-ui-ime-observe`（[PR #111](https://github.com/yojinn-io/GYO-Engine/pull/111)）。檔位 medium，引擎 Opus 5.5。估 2～4 h＋實機一次；Mac 端實際約 1.5 h，Windows 場次約 2 分鐘的操作。

**狀態：停下回報。事前宣告的判定不成立**（AGENTS §14 冒頭、§14.1）：場次沒有照宣告做，無法照宣告的分類判定。log 本身顯示了機制，記在「觀測到的事實」，但**不當成判定結果**。

收支：一致。Windows 端回報 `input.text` 169 行、34,990 bytes；Mac 收到的檔案相同（169 行、34,990 bytes，SHA-256 `6c5c0971…ff32`）；檔尾沒有 `[log] dropped_lines`，丟棄 0 行。

## 做了什麼（準備）

- `SdlInput::SetTextDiagnostics`、客戶端 `--text-input-trace`、`scripts/ime_observe_windows.cmd`、`scripts/ime_observe_report.py`。見 PR #111 與 README「v8-tool-ui-ime-observe 的事前宣告」。
- 試用套件：手動執行 package-trial（run 41049404191，commit `7f31a215`），Windows 端用 `gyo-package-object_fps_pvp-windows-x64`，SHA-256 一致。

## 場次（Windows 端回報，原文照錄重點）

- Windows Build 26300.9818（26H2）。輸入法：Microsoft IME（日文）、US 鍵盤。**沒有中文輸入法**。
- 對照組 1 次；日文輸入法預定 3 次；中文依使用者指示不做。
- 每次的 Enter 次數、畫面上的位址：**未記錄**。
- 使用者原文：「日本語の入力法は普通にできません、しかし、半角できます」
- 異常：Windows 端原本沒有 `gh`，經使用者同意用 winget 安裝；`.cmd` 除了啟動那一行還有 3 行 `rem` 註解（README 說「只有一行」的描述不精確，指令本身相同）。

## 判定為什麼不成立

宣告的判定是「對照組正常 → 看 3 次日文、3 次中文的冒號分類」。實際：

- log 裡有 6 個文字輸入的區段（trial），不是宣告的 1＋3＋3。其中 2、5、6 只有清除（Backspace）或 Enter，沒有打位址；trial 4 結束時客戶端去連 Gateway（逾時），表示 Enter 被當成送出。
- 中文輸入法沒做；每次的記錄缺 Enter 次數與畫面結果。
- 腳本的分類（`evidence/ime-observe/report.txt`）：trial 1 ascii（對照組，沒有組字：工具正常）；trial 3 missing；trial 4 ascii（但在 ASCII `:` 之前先確定了全形 `：`，分類規則只看「有沒有 ASCII」，所以標成 ascii）；2、5、6 missing（沒有打字）。
  - （後續，2026-10-31）`report.txt` 已移出 repo（AGENTS §15）；它和原始 log `client-20261030-210824.log`（34,990 bytes，沒進過 repo）現在都在主 checkout 的 `build/evidence/tool-v8-ime-observe/`（使用者的 Intel Mac，git 管理外）。全文：

    ```text
    diagnostics ime_ui="(default)" events=169
    trial 1: colon=ascii committed='127.0.0.1:8080' compositions=0 composed_first=0 keys_until_commit=None
    trial 2: colon=missing committed='' compositions=0 composed_first=0 keys_until_commit=None
    trial 3: colon=missing committed='127.0．0.1。０．１' compositions=0 composed_first=0 keys_until_commit=None
    trial 4: colon=ascii committed='.0.1：:8080' compositions=0 composed_first=0 keys_until_commit=None
    trial 5: colon=missing committed='' compositions=0 composed_first=0 keys_until_commit=None
    trial 6: colon=missing committed='' compositions=0 composed_first=0 keys_until_commit=None
    ```

- 依 §14.1，宣告的判定不在看了結果後改寫，所以這裡不下「重現／未重現」的結論。

## 觀測到的事實（不是判定，供下一個決定用）

1. **組字事件一筆都沒有**：`text_editing`／`text_candidates` 0 行。SDL 的 `SDL_HINT_IME_IMPLEMENTED_UI` 是預設（`ime_ui="(default)"`，由作業系統畫組字），這個設定下 Windows 沒有把組字送給遊戲。所以組字中的字遊戲完全看不到。
2. **字是一批一批出現的**：日文輸入法下，按鍵不產生 `text_key`（輸入法吃掉了），字在確定時一次送來，例如 21:09:15.406 同一刻 9 個字（`127.0．0.1`），21:09:20.522 同一刻 4 個字（`。０．１`）。使用者打字時畫面不動，直到確定才一起出現，這和「卡住」的描述一致。
3. **平假名模式送來全形字元**：`．`（U+FF0E）、`。`（U+3002）、`０`（U+FF10）、`１`（U+FF11）、`：`（U+FF1A）。大廳的 `AppendAddress`（`PvpApplication.cpp:201`）逐 byte 只收 ASCII，所以這些字被**靜靜丟掉**：冒號「打不出來」。
4. **半形可以**：trial 4 在全形 `：` 之後，切換後送來 ASCII `:`，之後的 `8080` 也是 ASCII。和使用者原文「半角できます」一致。
5. 範圍外：log 有 3 次 `PvP slow event processing`（884、1735、1202 ms），是事件處理的停頓，不在這次觀測範圍。

## 用了多少時間、下一次會改變什麼判斷

- Mac 端準備約 1.5 h；Windows 場次與回報由使用者與 Windows 會話完成。
- 如果照宣告重做（3 次日文、記 Enter 次數與畫面結果），可以把上面的事實變成判定；但事實 1～3 已經各自有直接證據（逐行的 log），重做主要補的是「每次的畫面結果」。
- 這會改變的決定：`v8-engine-input-composition` 要做哪些（收組字事件需要把 `SDL_HINT_IME_IMPLEMENTED_UI` 設成 `composition`，並用 `SDL_SetTextInputArea` 指定位置），以及產品的過濾要不要把全形英數與標點換成半形（產品的規則，`v8-tool-ui-edit-policy-owner` 定為產品提供檢查函式）。
