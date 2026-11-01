# v8-engine-platform-path-dialog：契約與 Design Invariants 條文

日期：2026-11-01。依 AGENTS.md §3（Engine 公開介面與 Design Invariants 的變更，Architecture Delta AD1）、§14.1（事前宣告的判定不成立）與 §14.4（新 DI 的守衛）提出。
**狀態：使用者 2026-11-01 核准**（「照建議核准」）：答案政策 (a)、sanitizer 選項 (1)、AD1、測試縫、條文。條文已寫入 `docs/architecture/platform-sdl.md`；DI 10 的守衛欄依選項 (1) 補上「其中一條只在本機 AddressSanitizer 組態」。

## 1. 要使用者決定的事

1. **Windows 上重複回呼時的答案政策**（xhigh 審查找到、Sonnet 檢查提出政策問題）。
   - 機制：SDL 3.4 在 Windows 上，新式對話窗啟動失敗（例如預設資料夾不存在）時先回呼一次 Failed，接著改開舊式對話窗，使用者選完後再回呼一次（`src/dialog/windows/SDL_windowsdialog.c:692-695,774`）。framework 現有的實作在這種情況會重複釋放記憶體，ui_editor 目前在 Windows 上就有這個風險。
   - (a)（建議，已實作）Failed 之後最多再交出一個答案。工具先收到 Failed，接著收到使用者真正的選擇；ui_editor 的 `ApplyPathDialogResults` 依序處理，不會丟掉選擇。代價：同一個 tag 可能有兩個答案，DI 10 要寫明。
   - (b) 只認第一次回呼。條文簡單，但 Windows 上使用者在舊式對話窗的選擇會被丟掉。
2. **sanitizer 突變的事前宣告不成立**（§14.1，先回報、不自行重跑）。
   - 宣告：兩條生命週期突變只有 AddressSanitizer 抓得到，以 ASan 訊息判定 killed。
   - 結果：0/2 killed，兩條都是「失敗了但訊息不符」。pd-filter-copy 在一般測試就被 CHECK 抓到（字串內容已錯，ASan 沒報）；pd-mailbox-not-shared 是崩潰（鎖已釋放的 mutex，系統函式庫丟例外後終止），沒有 ASan 報告。
   - 探索（不算驗收）：兩條在一般建置上各跑 5 次都被抓到。信箱那條靠 macOS 對已釋放 mutex 的反應，Linux、Windows 不一定崩潰。
   - 選項：
     - (1)（建議）pd-filter-copy 移到一般目錄（CI 就會守），預期改為 CHECK 失敗；pd-mailbox-not-shared 留在 ASan 目錄，預期改為「ASan 報告、mutex lock failed 或崩潰」，在 ASan 樹重跑並存證據。
     - (2) 不再維護 ASan 目錄，把兩條記成範圍的限界。
     - (3) 重新檢討計畫的前提（例如在 CI 加 sanitizer 組態），屬範圍變更。
3. **AD1**：Engine 平台層新增選擇窗能力，以及一張程序級、不解構的待回覆登記表（新的 Engine 狀態，只用來判斷「這個回呼的請求還在不在」）。
4. **測試縫放在 Native 標頭**：`PathDialogLauncher`、`ShowPathDialogWith`、`SdlDialogFunctions`、`ShowWithSdlFunctions`，比照 `ClockReaders`。
5. `docs/architecture/platform-sdl.md` 的條文（第 3 節），以及新 DI 10 的守衛（第 4 節）。

## 2. 介面（已實作，在工作樹）

- 中立標頭：`PathDialogKind`（OpenFile、SaveFile、OpenFolder）、`PathDialogFilter`、`PathDialogRequest`（tag 為 `std::uint64_t`）、`PathDialogOutcome`（Chosen、Cancelled、Failed）、`PathDialogAnswer`；`SdlPlatform::ShowPathDialog`（operation；種類不合法或 pattern 為空時 GYO_ASSERT）、`TakePathDialogAnswers`（主執行緒，GYO_ASSERT；記憶體不足時答案留著）。沒有任何 `SDL_` 符號（G1／G2 通過）。
- Native 標頭：`PathDialogCallback`（帶 noexcept）、`PathDialogLauncher`（noexcept）、`ShowPathDialogWith`、`SdlDialogFunctions` 與 `ShowWithSdlFunctions`（真正的 launcher 經三個 SDL 函式的表呼叫，測試換成假表）。
- 實作：SDL 拿到的是編號，不是請求本身。請求放在程序級登記表；每個請求預先配好兩個答案節點；回呼全程在登記表的鎖內，鎖的順序是登記表、信箱；請求在第一個非 Failed 的答案或第二個答案後離開登記表。信箱由平台與請求共用，平台銷毀後到的答案隨最後一個請求釋放。zenity 用空路徑表示取消，算 Cancelled。
- 和 framework 舊實作的差別：tag 由 32 位元改成 64 位元；重複回呼與 noexcept、`bad_alloc` 都有處理；跨執行緒信箱與生命週期由 Engine 負責（D-3）。種類名稱、答案種類、過濾器格式都相同。

## 3. platform-sdl.md 的條文（英文，照原文件語言）

**Process services**（補一項）：`ClipboardText()`, the system path dialogs (`ShowPathDialog`, `TakePathDialogAnswers`), the opt-in `SdlLogBridge`, and the entry point.

**Ownership**（補一句）：The path dialogs' mailbox is shared by the platform and the requests SDL still holds; the requests wait in a process-wide registry that is never destroyed.

**新契約**

> ### Path dialogs (v8-engine-platform-path-dialog)
>
> `ShowPathDialog` asks the OS for an open-file, save-file or open-folder dialog over this window and returns without waiting for the choice (on Linux the request to the portal itself may wait). The request carries the caller's tag, the filters (ignored for a folder; a pattern follows SDL's rule, and an invalid one comes back Failed) and the default location; the platform reads none of them for a decision (Design Invariant 9). Answers come later from `TakePathDialogAnswers`, on the main thread, with the request's tag: Chosen with the first path (UTF-8), Cancelled (also zenity's empty path), or Failed with SDL's error text. SDL may answer on another thread, before `ShowPathDialog` returns, or twice: SDL 3.4 on Windows answers Failed when its modern dialog cannot start and then again from the legacy dialog, so a Failed answer may be followed by one more. SDL 3.4 cannot cancel a dialog, and on Linux an answer may need the program to keep pumping events; a request the OS never answers gets no answer and stays in the registry until the process ends. Destroying the platform while a dialog is open is safe for the answers; SDL may still refer to the window (not verified on a real OS). Tests replace only the step that hands the request to the OS (native layer); CI never opens a real dialog, and runs the real launcher only with SDL's dialog driver hint set to a driver SDL lacks, which makes SDL answer Failed at once.

**DI 10（新增）**

> 10. A request gets one answer when the OS answers once; a Failed answer may be followed by one more, and no request gets more than two; a request the OS never answers gets none. Answers are handed out only on the main thread by `TakePathDialogAnswers`, in the order they arrived, each once; when it runs out of memory they stay for the next call. SDL gets a key, not the request: requests wait in a process-wide registry that is never destroyed, so a callback after the platform, a repeated one or one during static destruction is safe. The callback is noexcept, never touches the platform and never asserts. Filters and the default location live until the request leaves the registry. The platform keeps no count of open dialogs for any decision and never refuses, merges or deduplicates a request. Guards: `PathDialogTests.cpp` (a fake launcher, fake SDL functions, and the real launcher with SDL's dialog driver hint set); mutants pd-* (docs/architecture/plans/framework-rework/scripts/). Not guarded in CI: data races (no sanitizer), the out-of-memory paths, and real OS dialogs.

**DI 7（補一句）**：`ShowPathDialog` is an operation.

**DI 4（補一句）**：The path dialog launcher seam replaces only the request to the OS; it writes no input frame.

**Replacement Criteria（新增三條）**

> 7. Another asynchronous OS answer (a message box, a permission request) is needed: propose one platform answer queue instead of a second mailbox.
> 8. Path dialog answers must be ordered against input events: reopen the PlatformEvent decision (criterion 3).
> 9. A third workaround for how SDL calls a dialog back is needed: patch or replace SDL's dialog support instead.

Replacement Criteria 2 的改寫（例外改成 ImGui 綁定與 ui_editor 的預覽）移到外殼批：這批新增的是中立 API，沒有產品因此需要 Native；ImGui 綁定也還不存在。

## 4. 驗證

件數の突き合わせ：一般突變一致（宣告 16・執行 16・killed 16、stale 0）；sanitizer 突變不成立（宣告 2・執行 2・killed 0），見第 1 節第 2 項。

- 選擇窗測試 15 條通過；競爭測試連跑 30 次 0 失敗。`platform_sdl_tests`、`platform_sdl_concurrency`、`platform_sdl_header_tests`（G1／G2）通過。
- 一般突變 16/16 killed；被這批改到錨點的既有突變 m6a-g1、m6a-g2 2/2 killed。
- 執行紀錄：第一次 9 條中 8 條 killed，pd-g2-neutral 因突變寫錯（命名空間內的前置宣告遮住 SDL 型別、編不過）建置失敗，改成全域前置宣告後 killed；這是測試側的錯，判定沒變。xhigh 修正後 13/13；對抗式檢查修正後 16/16。
- xhigh 審查：blocker 1（Windows 重複回呼）、minor 4、nit 6，都已處理或寫成限界（平台銷毀時 SDL 仍持有視窗，待 T1 的 L3 確認）。
- Sonnet 對抗式檢查：major 6、minor 9。答案政策與 sanitizer 交使用者決定；真正 launcher 的參數對應、併發、不去重、tag 寬度、多檔、多餘文字的測試缺口已補；Replacement Criteria 與 AD1 的登記表已補進本文件。
- 範圍的限界（帶到 T1 的事前宣告）：真實 OS 對話窗的執行緒與行為、Linux portal 需要 pump、平台或父視窗銷毀時 SDL 對視窗的使用、資料競爭（CI 沒有 sanitizer）、記憶體不足的路徑。
- 證據：使用者 Mac，主 checkout `build/evidence/framework-rework/v8-engine-platform-path-dialog/`。
