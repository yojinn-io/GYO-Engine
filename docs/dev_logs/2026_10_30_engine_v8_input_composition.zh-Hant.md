# Engine v8：v8-engine-input-composition（輸入法的組字事件、輸入區域、平台選項）

日期：2026-10-30。分支 `claude/v8-tool-ui-ime-observe`。檔位 high，引擎 Opus 5.5。估 6～10 h，實際約 1 h。由工具線做（使用者決定，`v8-tool-ime-input-delta`）。

**狀態：** 完成（Mac 上的單元測試與突變）。Windows 實機確認（L3）要等有畫組字的消費端（`text_field` 或大廳）。

相關文件：[input.md](../architecture/input.md)、[platform-sdl.md](../architecture/platform-sdl.md)「Input method composition」、[工具線 README](../tools/plans/v8/README.md)「檢測之後：輸入法的規劃」、[input-and-present HANDOFF](../architecture/plans/input-and-present/HANDOFF.md)「消費端回報」。

## 為什麼

Windows 的檢測 log（`docs/tools/plans/v8/evidence/ime-observe/`）：SDL 預設由作業系統畫組字，遊戲收不到組字事件（0 筆），組字中的字看不到，字在確定時才成批出現。

（後續，2026-10-31）`docs/tools/plans/v8/evidence/ime-observe/` 已移出 repo（AGENTS §15），現在在主 checkout 的 `build/evidence/tool-v8-ime-observe/report.txt`（使用者的 Intel Mac，git 管理外）；腳本的分類全文抄在 [2026_10_30_tool_v8_ui_ime_observe](2026_10_30_tool_v8_ui_ime_observe.zh-Hant.md)「判定為什麼不成立」一節。

## 做了什麼（全部只加不改）

- `SdlPlatformOptions::imeComposition`（預設 false）：在 `Create` 啟動 SDL 視訊之前，把 `SDL_HINT_IME_IMPLEMENTED_UI` 設成 `composition`。這個 hint 作用於整個 process，在視訊啟動時讀取；設為 false 時不動它。
- `TextEvent::Kind::Composition`：`text`（目前整段組字；空字串＝組字結束且沒有字）、`cursor`、`selectionLength`（以 code point 計；平台沒給時為 -1）。和文字、按鍵同樣的規則：只在文字輸入開啟時、只收本視窗、在 `text` 裡依到達順序。
- `SdlInput::SetTextInputArea(area, cursor)`：交給 `SDL_SetTextInputArea`，讓候選窗出現在輸入框旁邊。新的錯誤碼 `TextInputAreaFailed`。
- 診斷 log（`text_editing` 那一行）加上 `taken`。
- 文件：`input.md`（契約）、`platform-sdl.md`（新的契約小節）、input-and-present 的 HANDOFF（Architecture Delta 的摘要）。

## Design Invariants 的檢查

- Input 1（只加不改）：新增 `Kind` 的值與兩個欄位，沒有改變既有欄位、`Kind` 與 `Key` 的意義。
- Input 3（只在文字輸入開啟時、依到達順序）：組字照樣適用，有測試守著。
- Input 4（frame 不帶政策）：組字是作業系統給的事實，不是編輯政策。
- Platform 1（中立標頭沒有 SDL 符號）：`SdlPlatform.hpp` 只在註解裡提到 hint 的名字；G2 掃描會先去掉註解，header 測試通過。
- 既有消費端：大廳的 `HandleTextEditing` 只特別處理 `Kind::Text`，其他的當按鍵看；組字的 `layoutKey` 是 `Key::Count`、沒有修飾鍵，所以什麼也不做。而且大廳沒開 `imeComposition`，收不到組字。

## 驗證

- Red：先加介面與空的實作，新測試失敗：組字 1 項、輸入區域 1 項、平台的 hint 1 項。
- 經過：
  - 平台測試一開始的 `REQUIRE(SDL_ResetHint(...))` 失敗：hint 沒設過時 SDL 回傳 false。是測試寫錯，改成不檢查回傳值。
  - 第一次直接執行測試時，有幾個既有測試失敗（例如期待 `dummy` 驅動）。原因是 ctest 會設 `SDL_VIDEODRIVER=dummy`，而直接執行沒有設。之後一律透過 ctest 或帶這個環境變數執行，全部通過。我當時推測是「組字測試中斷造成連帶失敗」，推測錯了。
- Green：`ctest -R "^input|platform_sdl"` 5/5 通過。wide 建置（含 PvP、ui_editor）沒有錯誤；`ctest -L cpu` 97/97 通過（macOS x86_64）。
- 突變 7 個，全部被抓到：
  - 組字不檢查文字輸入是否開啟
  - 組字不檢查視窗
  - 對調 cursor 與 selection
  - 組字當成 `Kind::Text`
  - 輸入區域的 cursor 丟掉
  - hint 永遠設定
  - hint 永遠不設定

## 改了的測試判定（AGENTS §14.4）

`SDL text diagnostics log what arrives and leave the frame unchanged`（上一批 `v8-tool-ui-ime-observe` 寫的）：

1. 守的性質：診斷不改變 frame；組字不在 frame 裡。
2. 放寬的面：以前算失敗的「組字進了 frame」，現在算通過。這是這一批核准的契約變更。
3. 收緊的面：以前通過的「組字不在 frame」，現在算失敗（期待 4 筆，不是 3 筆）。
4. 「診斷不改變 frame」仍由同一個測試的「開與關逐筆相同」守著。
5. 根據：使用者核准的規劃（`v8-tool-ime-plan`）；組字的規則另有新測試守著。

這不是受理判定，也不是 Design Invariants 的守衛。

## 未做

- L3：Windows 上 `imeComposition` 打開時，`text_editing` 是否真的送到、候選窗是否出現在指定位置。要等有畫組字的消費端；屆時用 `--text-input-trace` 的 log 確認。
- 大廳仍沒有畫組字（等 `text_field`）。
