# 工具 v8：v8-framework-close-request

日期：2026-11-04。分支 `claude/v8-tool-model-viewer-77f310`（PR #146），接在 `-events` 之後。

起因：`-events` 的 L3 第 6 項「關閉時有未存檔提示」，照原文做不到：framework 的外殼不讓客戶擋下關閉。你選了改 framework 的外殼，核准了事前宣告，並選 (a)：ui_editor 一起接上。

- 段：上段（framework 的公開介面、framework DI 6 的字句）。
- 實作者：Opus 5.5／high（主會話）。
- 審查者：Opus 5.5／high（子 agent，一次）。
- 沒有放寬判定，沒有開檢驗者。
- 估時 3～4 h；實際約 1.5 h。

**狀態：**
- 程式、測試、突變、審查都完成。
- L1 的結果記在 HANDOFF。
- Q4（L3）在 `-trial`。

相關文件：
- [事前宣告](../tools/plans/v8/model-viewer.md)：「`v8-framework-close-request` 的事前宣告」與它的「實作後的結果」；
- [framework architecture.md](../framework/architecture.md)：DI 6；
- [platform-sdl.md](../architecture/platform-sdl.md)：中立層的說明。

## 做了什麼

- Engine：沒有改程式。`SdlPlatform::PumpEvents` 回傳 `Stop` 的條件（收到 quit，或本視窗的關閉請求），以前只寫在程式裡，現在寫進標頭與 platform-sdl.md。
- framework：
  - `WindowShellClient::CloseRequested()`，預設回 true。`PumpEvents` 回傳 `Stop` 時，外殼在那一輪 pump 結束後、Update 之前問客戶一次；客戶回 false，這一輪就不結束。
  - ImGui 綁定把這個問題轉給 `ImGuiShellClient::CloseRequested()`。
  - DI 6 補上一句（你同意的）。
- model_viewer：有未存檔的事件時，回 false，並開出「Discard unsaved events?」的詢問。還在輸入中的欄位，問之前先寫入。
- ui_editor：有未存檔的文件時，回 false，並開出「Discard unsaved changes?」的詢問。File > Exit 也走同一個檢查。
- 測試：
  - `gyo_framework.window_close`（Q1、Q2，`cpu`）；
  - `gyo_framework.imgui_close`（Q3，`cpu`）。

## 驗證與經過

- 本機 macOS，`build/target/_build/ui`：
  - `gyo_framework.*`、`gyo_ui_editor.*`、`gyo_model_viewer.*` 23／23 通過；
  - `cpu|shader` 119／119 通過。
- 件數の突き合わせ：一致。
  - 宣告 3 條（`fw-close-*`），3／3 killed；
  - 既有的 `fw-shell-no-observer` 的 find 落在改動的那行，所以照 D184 重跑。第一輪是 stale（那行程式改了）；改成新的寫法後 killed，突變的內容不變（pump 不帶觀察者）；
  - 審查後改了 Q1 的測試檔，所以 4 條全部重跑，4／4 killed；
  - `fw-close-quit-not-asked` 原本的取代式是 `std::exit`，不合理，在第一次執行前就改成「quit 不讓 pump 停下」。
- 審查的指出與處理：
  1. （中）model_viewer 有欄位還在輸入、還沒寫入時，關閉不會問。修正：`CloseRequested` 與 `Guarded` 先把輸入中的欄位寫入。ui_editor 沒有這個問題（進行中的編輯已經算進 `IsDirty()`）。
  2. （低）詢問開著時再收到關閉請求，會重開詢問。修正：已開著就不重開。
  3. （低）「每輪 pump 只問一次」沒有測試。修正：Q1 加一個 case，同一輪送出關閉請求和 quit。
  4. （低）SDL 把 SIGINT、SIGTERM 轉成 quit，有未存檔時這兩個訊號也會先問。我在 `WindowShell.hpp` 寫明了這一點，但沒有改行為。
- 審查認為：兩個產品都用這個回呼，所以 framework 那條 Replacement Criteria（只有一個產品用的回呼）不成立；DI 6 新增的那句與程式一致。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-model-viewer/close-request/`，有：
  - `mutations.txt`（4／4）、`mutations-round1.txt`、`mutations-shell-observer.txt`；
  - `ctest-all.txt`。

## 和事前宣告不同的地方

- Q1 多一個 case（每輪 pump 只問一次）。
- 寫明了 SIGINT 與 SIGTERM 的行為。

## 發現（不在本批範圍）

- 「每輪 pump 只問一次」要寫成突變並不自然：外殼分辨不出是哪種事件讓它停下。所以只有測試，沒有突變。
